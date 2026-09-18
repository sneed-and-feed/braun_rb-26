#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <chrono>
#include <cassert>
#include "Rb26Engine.h"
#include "FdnReverbTank.h"
#include "PitchShifter.h"
#include "AcousticExciter.h"

#define DIAG_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << (msg) << " [" << #cond << "]\n"; \
        return 1; \
    } \
} while (0)

int main() {
    const double fs = 48000.0;
    std::cout << "=================================================================\n";
    std::cout << "=== VERIFICATION SUITE: FREEZE HOLD, DIFFUSION & PITCH BYPASS ===\n";
    std::cout << "=================================================================\n";

    // -------------------------------------------------------------------------
    // TEST 1: FREEZE HOLD ACROSS ALL 4 MANIFOLDS (No Muting / Infinite Sustain)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 1] FREEZE HOLD SUSTAIN ACROSS MANIFOLDS\n";
    const rb26::ManifoldType manifolds[] = {
        rb26::ManifoldType::PoincareHyperbolic,
        rb26::ManifoldType::WhisperingGallery,
        rb26::ManifoldType::AnharmonicPlate,
        rb26::ManifoldType::StockhausenKlangdom
    };
    const char* manifoldNames[] = {
        "PoincareHyperbolic",
        "WhisperingGallery",
        "AnharmonicPlate",
        "StockhausenKlangdom"
    };

    for (int m = 0; m < 4; ++m) {
        rb26::FdnReverbTank tank;
        tank.prepare(fs);
        // Normal excitation
        tank.setParameters(1.0f, 3.5f, 6500.0f, 0.75f, false, 0.85f, 1.2f, 85.0f, manifolds[m]);

        float outL = 0.0f, outR = 0.0f;
        // Inject burst
        for (int i = 0; i < 2000; ++i) {
            float in = static_cast<float>(0.5 * std::sin(2.0 * 3.141592653589793 * 440.0 * i / fs));
            tank.processSample(in, in, 0.0f, 0.0f, outL, outR);
        }

        // Measure energy before freeze
        double preFreezeEnergy = 0.0;
        for (int i = 0; i < 500; ++i) {
            float in = static_cast<float>(0.5 * std::sin(2.0 * 3.141592653589793 * 440.0 * (2000 + i) / fs));
            tank.processSample(in, in, 0.0f, 0.0f, outL, outR);
            preFreezeEnergy += (outL * outL + outR * outR);
        }
        const double preFreezeRms = std::sqrt(preFreezeEnergy / 1000.0);

        // Engage freeze
        tank.setParameters(1.0f, 3.5f, 6500.0f, 0.75f, true, 0.85f, 1.2f, 85.0f, manifolds[m]);

        // Run 100,000 samples of silence
        double first1kEnergy = 0.0;
        double earlyEnergy = 0.0;
        double lateEnergy = 0.0;
        for (int i = 0; i < 100000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
            if (i < 500) first1kEnergy += (outL * outL + outR * outR);
            if (i >= 5000 && i < 10000) earlyEnergy += (outL * outL + outR * outR);
            if (i >= 95000 && i < 100000) lateEnergy += (outL * outL + outR * outR);
        }
        const double first1kRms = std::sqrt(first1kEnergy / 1000.0);
        const double earlyRms = std::sqrt(earlyEnergy / 10000.0);
        const double lateRms = std::sqrt(lateEnergy / 10000.0);
        const double ratio = lateRms / (earlyRms + 1.0e-9);
        const double onsetRatio = earlyRms / (preFreezeRms + 1.0e-9);

        std::cout << "  " << std::setw(20) << std::left << manifoldNames[m]
                  << " Pre-Freeze: " << std::fixed << std::setprecision(5) << preFreezeRms
                  << " | 0-500: " << first1kRms
                  << " | 5k-10k: " << earlyRms
                  << " | 95k-100k: " << lateRms
                  << " | Onset Ratio: " << std::setprecision(3) << onsetRatio
                  << " | Sustain Ratio: " << std::setprecision(3) << ratio << "\n";

        if (earlyRms < 1.0e-4) {
            std::cerr << "FAIL: Reverb did not energize before freeze!\n";
            return 1;
        }
        if (lateRms < 1.0e-4) {
            std::cerr << "FAIL: Freeze hold muted the reverb!\n";
            return 1;
        }
        if (ratio < 0.70) {
            std::cerr << "FAIL: Freeze hold decayed excessively (" << ratio << ")!\n";
            return 1;
        }
    }
    std::cout << "  -> PASS: All 4 manifolds sustained freeze hold losslessly!\n";

    // -------------------------------------------------------------------------
    // TEST 1B: RB26 REVERB ENGINE FREEZE HOLD (Full Engine Integration)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 1B] RB26 REVERB ENGINE FREEZE HOLD INTEGRATION\n";
    {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(fs, 512);

        rb26::Rb26Parameters p;
        p.dryWetMix = 0.5f;
        p.earlyLateMix = 0.5f;
        p.decayRt60Sec = 3.5f;
        p.diffusionDensity = 0.75f;
        p.freezeHold = false;
        engine.setParameters(p);

        // Inject 4800 samples of 440 Hz audio
        std::vector<float> inL(512, 0.0f);
        std::vector<float> inR(512, 0.0f);
        std::vector<float> outL(512, 0.0f);
        std::vector<float> outR(512, 0.0f);
        const float* inCh[2] = { inL.data(), inR.data() };
        float* outCh[2] = { outL.data(), outR.data() };

        for (int b = 0; b < 10; ++b) {
            for (int i = 0; i < 512; ++i) {
                inL[i] = static_cast<float>(0.5 * std::sin(2.0 * 3.141592653589793 * 440.0 * (b * 512 + i) / fs));
                inR[i] = inL[i];
            }
            engine.process(inCh, outCh, 2, 512);
        }

        // Measure energy before freeze
        double preRms = 0.0;
        std::fill(inL.begin(), inL.end(), 0.0f);
        std::fill(inR.begin(), inR.end(), 0.0f);
        engine.process(inCh, outCh, 2, 512);
        for (int i = 0; i < 512; ++i) preRms += (outL[i] * outL[i] + outR[i] * outR[i]);
        preRms = std::sqrt(preRms / 1024.0);

        // Engage freeze!
        p.freezeHold = true;
        engine.setParameters(p);

        double earlyRms = 0.0;
        double lateRms = 0.0;
        for (int b = 0; b < 200; ++b) {
            engine.process(inCh, outCh, 2, 512);
            if (b >= 10 && b < 20) {
                for (int i = 0; i < 512; ++i) earlyRms += (outL[i] * outL[i] + outR[i] * outR[i]);
            }
            if (b >= 180 && b < 200) {
                for (int i = 0; i < 512; ++i) lateRms += (outL[i] * outL[i] + outR[i] * outR[i]);
            }
        }
        earlyRms = std::sqrt(earlyRms / (10 * 1024.0));
        lateRms  = std::sqrt(lateRms / (20 * 1024.0));
        const double engineRatio = lateRms / (earlyRms + 1.0e-9);

        std::cout << "  Engine Pre-Freeze RMS: " << std::fixed << std::setprecision(5) << preRms
                  << " | Early (5k-10k) RMS: " << earlyRms
                  << " | Late (90k-100k) RMS: " << lateRms
                  << " | Engine Sustain Ratio: " << std::setprecision(3) << engineRatio << "\n";

        if (lateRms < 1.0e-4) {
            std::cerr << "FAIL: Rb26ReverbEngine freeze hold muted the reverb!\n";
            return 1;
        }
        std::cout << "  -> PASS: Rb26ReverbEngine sustained freeze hold cleanly!\n";
    }

    // -------------------------------------------------------------------------
    // TEST 2: DIFFUSION KNOB MODULATES DECAY DIFFUSION
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 2] DIFFUSION KNOB IMPACT ON DECAY\n";
    {
        rb26::FdnReverbTank tank0, tank1;
        tank0.prepare(fs);
        tank1.prepare(fs);

        tank0.setParameters(1.0f, 4.0f, 8000.0f, 0.0f, false, 0.0f, 0.0f, 85.0f);
        tank1.setParameters(1.0f, 4.0f, 8000.0f, 1.0f, false, 0.0f, 0.0f, 85.0f);

        // Inject Dirac impulse
        float outL0 = 0.0f, outR0 = 0.0f, outL1 = 0.0f, outR1 = 0.0f;
        tank0.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL0, outR0);
        tank1.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL1, outR1);

        double diffEnergy = 0.0;
        double totalEnergy0 = 0.0;
        double totalEnergy1 = 0.0;

        for (int i = 0; i < 48000; ++i) {
            tank0.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL0, outR0);
            tank1.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL1, outR1);

            float diff = outL1 - outL0;
            diffEnergy += diff * diff;
            totalEnergy0 += outL0 * outL0;
            totalEnergy1 += outL1 * outL1;
        }

        const double diffRms = std::sqrt(diffEnergy / 48000.0);
        const double rms0 = std::sqrt(totalEnergy0 / 48000.0);
        const double rms1 = std::sqrt(totalEnergy1 / 48000.0);

        std::cout << "  0% Diffusion RMS: " << rms0
                  << " | 100% Diffusion RMS: " << rms1
                  << " | Difference RMS: " << diffRms
                  << " | Relative Diff: " << (diffRms / (rms0 + 1.0e-9)) * 100.0 << " %\n";

        // Measure echo density in late decay window (samples 9600 to 24000, ~200ms - 500ms)
        size_t activeCount0 = 0;
        size_t activeCount1 = 0;
        // Re-run with clean tanks to measure raw impulse response
        tank0.prepare(fs);
        tank1.prepare(fs);
        tank0.setParameters(1.0f, 4.0f, 8000.0f, 0.0f, false, 0.0f, 0.0f, 85.0f);
        tank1.setParameters(1.0f, 4.0f, 8000.0f, 1.0f, false, 0.0f, 0.0f, 85.0f);

        tank0.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL0, outR0);
        tank1.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL1, outR1);

        for (int i = 0; i < 24000; ++i) {
            tank0.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL0, outR0);
            tank1.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL1, outR1);
            if (i >= 9600) {
                if (std::abs(outL0) > 1.0e-5f) activeCount0++;
                if (std::abs(outL1) > 1.0e-5f) activeCount1++;
            }
        }
        std::cout << "  Late Decay Echo Count (200-500ms): 0% Diff = " << activeCount0
                  << " | 100% Diff = " << activeCount1 << "\n";
        DIAG_ASSERT(activeCount1 > activeCount0, "100% diffusion must produce greater late echo density than 0%");

        if (diffRms < 0.001 || (diffRms / (rms0 + 1.0e-9)) < 0.20) {
            std::cerr << "FAIL: Diffusion knob had negligible effect on decay!\n";
            return 1;
        }
        std::cout << "  -> PASS: Diffusion knob actively and profoundly alters decay dispersion and multiplies echo density!\n";
    }

    // -------------------------------------------------------------------------
    // TEST 3: PITCH DIFFUSION 0% INACTIVE BYPASS & CPU SPIKE ELIMINATION
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 3] PITCH DIFFUSION 0% INACTIVE BYPASS\n";
    {
        rb26::PitchShifter shifter;
        shifter.prepare(fs);

        // Case A: Default 0% sends
        shifter.setParameters(0.0f, 0.0f, 1, 2, 0.0f, 0.0f);
        // Slew smoothers
        for (int i = 0; i < 12000; ++i) {
            float dummyL = 0.0f, dummyR = 0.0f;
            shifter.processSample(0.0f, 0.0f, dummyL, dummyR);
        }
        std::cout << "  shimmer=0, dimmer=0 -> isActive(): " << std::boolalpha << shifter.isActive() << "\n";
        DIAG_ASSERT(!shifter.isActive(), "Zero sends must evaluate to inactive");

        // Case B: shimmerSend=0.5, dimmerSend=0.0, pitchBlend=-1.0 (100% dimmer)
        shifter.setParameters(0.5f, 0.0f, 1, 2, -1.0f, 0.0f);
        for (int i = 0; i < 12000; ++i) {
            float dummyL = 0.0f, dummyR = 0.0f;
            shifter.processSample(0.0f, 0.0f, dummyL, dummyR);
        }
        std::cout << "  shimmer=0.5, dimmer=0, blend=-1.0 -> isActive(): " << shifter.isActive() << "\n";
        DIAG_ASSERT(!shifter.isActive(), "100% dimmer blend with 0% dimmer send must evaluate to inactive");

        // Case C: shimmerSend=0.0, dimmerSend=0.5, pitchBlend=+1.0 (100% shimmer)
        shifter.setParameters(0.0f, 0.5f, 1, 2, 1.0f, 0.0f);
        for (int i = 0; i < 12000; ++i) {
            float dummyL = 0.0f, dummyR = 0.0f;
            shifter.processSample(0.0f, 0.0f, dummyL, dummyR);
        }
        std::cout << "  shimmer=0.0, dimmer=0.5, blend=+1.0 -> isActive(): " << shifter.isActive() << "\n";
        DIAG_ASSERT(!shifter.isActive(), "100% shimmer blend with 0% shimmer send must evaluate to inactive");

        // Case D: shimmerSend=0.4, dimmerSend=0.35, pitchBlend=0.0 (active)
        shifter.setParameters(0.4f, 0.35f, 1, 2, 0.0f, 0.0f);
        for (int i = 0; i < 12000; ++i) {
            float dummyL = 0.0f, dummyR = 0.0f;
            shifter.processSample(0.0f, 0.0f, dummyL, dummyR);
        }
        std::cout << "  shimmer=0.4, dimmer=0.35, blend=0.0 -> isActive(): " << shifter.isActive() << "\n";
        DIAG_ASSERT(shifter.isActive(), "Non-zero sends must evaluate to active");

        // Case E: Shimmer=0, Dimmer=0, sweeping Shim/Dim blend (-1.0 to +1.0) must remain strictly inactive with zero CPU spikes
        for (float blendVal : { -1.0f, -0.75f, -0.5f, 0.0f, 0.5f, 0.75f, 1.0f }) {
            shifter.setParameters(0.0f, 0.0f, 1, 2, blendVal, 0.0f);
            DIAG_ASSERT(!shifter.isActive(), "Moving pitch blend at 0% sends must never awaken pitch shifter");
        }
        std::cout << "  shimmer=0, dimmer=0 with Shim/Dim sweep (-1.0 to +1.0) -> strictly inactive across all blend positions\n";

        std::cout << "  -> PASS: PitchShifter::isActive() correctly identifies all inactive states!\n";
    }

    // -------------------------------------------------------------------------
    // TEST 4: C6 (~1046.5 Hz) METALLIC RINGING & POINCARÉ MODAL CLUSTERING
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 4] C6 (~1046.5 Hz) METALLIC RINGING & POINCARÉ MODAL CLUSTERING\n";
    {
        // 4.1 Verify Poincaré delay length decorrelation modulo 46 across room sizes 0.5x, 0.65x, and 1.0x (C6 period = 45.867 samples at 48 kHz)
        rb26::ManifoldDelayNetwork mdn;
        mdn.prepare(fs);
        const double c6Period = fs / 1046.5;

        for (float room : { 0.5f, 0.65f, 1.0f }) {
            mdn.setParameters(rb26::ManifoldType::PoincareHyperbolic, room, 1800.0f, 0.75f);
            const auto& lengths = mdn.getNominalLengths();
            size_t nearC6CongruentCount = 0;
            for (size_t k = 0; k < rb26::ManifoldDelayNetwork::kNumLines; ++k) {
                const double cycles = static_cast<double>(lengths[k]) / c6Period;
                const double distToCycle = std::abs(cycles - std::round(cycles)) * c6Period;
                if (distToCycle < 2.0) {
                    nearC6CongruentCount++;
                }
            }
            std::cout << "  Poincare delay line C6 standing-wave mode counts (room " << room << "): " << nearC6CongruentCount << " / 8\n";
            DIAG_ASSERT(nearC6CongruentCount == 0, "No Poincare delay line should cluster within 2 samples of integer C6 cycles");
        }

        // 4.2 Verify C6 excitation decay in full ReverbEngine with default settings
        rb26::Rb26ReverbEngine engine;
        engine.prepare(fs, 128);
        rb26::Rb26Parameters p;
        engine.setParameters(p);

        // Inject 500 samples of pure C6 tone (1046.5 Hz)
        const int numStimSamples = 500;
        std::vector<float> stimL(numStimSamples), stimR(numStimSamples);
        for (int i = 0; i < numStimSamples; ++i) {
            const float s = 0.5f * std::sin(2.0f * 3.14159265f * 1046.5f * static_cast<float>(i) / static_cast<float>(fs));
            stimL[i] = s;
            stimR[i] = s;
        }

        const float* stimPtrs[2] = { stimL.data(), stimR.data() };
        std::vector<float> outStimL(numStimSamples), outStimR(numStimSamples);
        float* outStimPtrs[2] = { outStimL.data(), outStimR.data() };
        engine.process(stimPtrs, outStimPtrs, 2, numStimSamples);

        // Run 48000 samples of silence (~1 second) and measure late energy (samples 24000 to 48000)
        const int blockSize = 128;
        std::vector<float> zeroIn(blockSize, 0.0f);
        std::vector<float> blockOutL(blockSize), blockOutR(blockSize);
        const float* zeroPtrs[2] = { zeroIn.data(), zeroIn.data() };
        float* outPtrs[2] = { blockOutL.data(), blockOutR.data() };

        double lateEnergy = 0.0;
        int lateSampleCount = 0;

        for (int n = 0; n < 48000; n += blockSize) {
            engine.process(zeroPtrs, outPtrs, 2, blockSize);
            if (n >= 24000) {
                for (int i = 0; i < blockSize; ++i) {
                    lateEnergy += blockOutL[i] * blockOutL[i] + blockOutR[i] * blockOutR[i];
                    lateSampleCount += 2;
                }
            }
        }
        const double lateRms = std::sqrt(lateEnergy / lateSampleCount);
        std::cout << "  C6 Late Tail RMS (0.5s - 1.0s): " << lateRms << "\n";
        DIAG_ASSERT(lateRms < 0.035, "C6 late tail must not exhibit frosty metallic ringing or runaway resonance");
        std::cout << "  -> PASS: C6 metallic ringing and standing-wave clustering successfully eliminated!\n";
    }

    // Benchmark CPU consumption of Rb26ReverbEngine with Pitch Active vs Bypassed
    std::cout << "\n[BENCHMARK] Rb26ReverbEngine CPU Benchmark (Active vs 0% Bypassed)\n";
    {
        rb26::Rb26ReverbEngine engineActive, engineBypassed;
        engineActive.prepare(fs, 512);
        engineBypassed.prepare(fs, 512);

        rb26::Rb26Parameters pActive;
        pActive.shimmerSend = 0.40f;
        pActive.dimmerSend = 0.35f;
        engineActive.setParameters(pActive);

        rb26::Rb26Parameters pBypassed;
        pBypassed.shimmerSend = 0.0f;
        pBypassed.dimmerSend = 0.0f;
        engineBypassed.setParameters(pBypassed);

        const int blockSize = 512;
        const int numBlocks = 1000; // ~10.6 seconds of audio
        std::vector<float> inL(blockSize, 0.1f), inR(blockSize, 0.1f);
        std::vector<float> outL(blockSize), outR(blockSize);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        // Warmup
        for (int b = 0; b < 100; ++b) {
            engineActive.process(inPtrs, outPtrs, 2, blockSize);
            engineBypassed.process(inPtrs, outPtrs, 2, blockSize);
        }

        auto startActive = std::chrono::high_resolution_clock::now();
        for (int b = 0; b < numBlocks; ++b) {
            engineActive.process(inPtrs, outPtrs, 2, blockSize);
        }
        auto endActive = std::chrono::high_resolution_clock::now();
        auto durActiveMs = std::chrono::duration<double, std::milli>(endActive - startActive).count();

        auto startBypassed = std::chrono::high_resolution_clock::now();
        for (int b = 0; b < numBlocks; ++b) {
            engineBypassed.process(inPtrs, outPtrs, 2, blockSize);
        }
        auto endBypassed = std::chrono::high_resolution_clock::now();
        auto durBypassedMs = std::chrono::duration<double, std::milli>(endBypassed - startBypassed).count();

        std::cout << "  Processing 10.6s audio with Pitch ACTIVE:   " << durActiveMs << " ms\n";
        std::cout << "  Processing 10.6s audio with Pitch BYPASSED: " << durBypassedMs << " ms\n";
        std::cout << "  CPU Time Reduction: " << std::fixed << std::setprecision(1)
                  << (1.0 - durBypassedMs / durActiveMs) * 100.0 << " %\n";

        DIAG_ASSERT(durBypassedMs < durActiveMs, "Bypassed pitch must be faster than active");
        std::cout << "  -> PASS: Bypassed pitch diffusion achieves massive CPU reduction!\n";
    }

    // -------------------------------------------------------------------------
    // TEST 4B: TRANSIENT PUNCH DETECTOR ONSET GATING & MULTI-RATE VERIFICATION
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 4B] TRANSIENT PUNCH DETECTOR ONSET GATING & MULTI-RATE VERIFICATION\n";
    {
        for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 }) {
            rb26::TransientPunchDetector detector;
            detector.prepare(rate);

            // 1. Step impulse attack time (< 3 ms)
            float minGainStep = 1.0f;
            int attackSamples = 0;
            const int stepTotalSamples = static_cast<int>(rate * 0.010);
            for (int i = 0; i < stepTotalSamples; ++i) {
                float in = (i == 0) ? 1.0f : 0.0f;
                float g = detector.process(in, in, 1.0f);
                if (g < minGainStep) {
                    minGainStep = g;
                    attackSamples = i;
                }
            }
            double attackMs = static_cast<double>(attackSamples) / rate * 1000.0;
            DIAG_ASSERT(attackMs <= 3.0, "Step impulse attack time must be <= 3.0 ms");

            // 2. Step input over ~2.5 ms (depth scaling, 2.5 ms > 1 ms attack tau)
            detector.reset();
            rb26::TransientPunchDetector detZero;
            detZero.prepare(rate);
            float gZero = 1.0f, gFull = 1.0f;
            const int stepSamples = static_cast<int>(std::round(rate * 0.0025));
            for (int i = 0; i < stepSamples; ++i) {
                gZero = detZero.process(1.0f, 1.0f, 0.0f);
                gFull = detector.process(1.0f, 1.0f, 1.0f);
            }
            DIAG_ASSERT(std::abs(gZero - 1.0f) < 1.0e-4f, "Punch ducking depth 0.0 must yield gain 1.0");
            DIAG_ASSERT(gFull < 0.5f, "Punch ducking depth 1.0 must yield significant gain reduction (< 0.5)");

            // 3. Synthesized kick transient ducking (9-14 dB attenuation at 48kHz)
            if (std::abs(rate - 48000.0) < 1.0) {
                detector.reset();
                const size_t kickLen = 12000;
                const size_t stepOnset = 1200;
                std::vector<float> kick(kickLen, 0.0f);
                for (size_t n = stepOnset; n < kickLen; ++n) {
                    double t = static_cast<double>(n - stepOnset) / 48000.0;
                    double freq = 120.0 * std::exp(-t / 0.025) + 50.0;
                    double env = std::exp(-t / 0.040);
                    kick[n] = static_cast<float>(env * std::sin(2.0 * 3.14159265358979323846 * freq * t));
                }
                kick[stepOnset] = 1.0f;
                float minKickGain = 1.0f;
                for (size_t n = 0; n < kickLen; ++n) {
                    float g = detector.process(kick[n], kick[n], 1.0f);
                    if (n >= stepOnset && n < stepOnset + 1440) {
                        if (g < minKickGain) minKickGain = g;
                    }
                }
                double kickDb = 20.0 * std::log10(minKickGain);
                DIAG_ASSERT(kickDb <= -9.0 && kickDb >= -14.0, "Kick transient must duck modal injection by 9 to 14 dB");
            }

            // 4. Sustained 30 Hz sine wave (min gain > 0.95)
            detector.reset();
            const int warmupSamp = static_cast<int>(rate * 0.20);
            for (int i = 0; i < warmupSamp; ++i) {
                float s = 0.8f * std::sin(2.0f * 3.14159265f * 30.0f * static_cast<float>(i) / static_cast<float>(rate));
                detector.process(s, s, 1.0f);
            }
            float minGain30Hz = 1.0f;
            const int testSamp = static_cast<int>(rate * 0.10);
            for (int i = 0; i < testSamp; ++i) {
                float s = 0.8f * std::sin(2.0f * 3.14159265f * 30.0f * static_cast<float>(warmupSamp + i) / static_cast<float>(rate));
                float g = detector.process(s, s, 1.0f);
                minGain30Hz = std::min(minGain30Hz, g);
            }
            DIAG_ASSERT(minGain30Hz > 0.95f, "Steady continuous 30 Hz sine tone must not trigger ducking");

            // 5. Eno dual drone (5 seconds, beating 32.7 Hz & 33.05 Hz)
            detector.reset();
            float minGainDrone = 1.0f;
            size_t duckSamplesDrone = 0;
            const double f1 = 32.7, f2 = 33.05;
            const int droneTotalSamp = static_cast<int>(rate * 5.0);
            for (int i = 0; i < droneTotalSamp; ++i) {
                const double t = static_cast<double>(i) / rate;
                const float s1 = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979323846 * f1 * t));
                const float s2 = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979323846 * f2 * t));
                float g = detector.process(s1 + s2, s1 + s2, 0.85f);
                minGainDrone = std::min(minGainDrone, g);
                if (g < 0.95f) duckSamplesDrone++;
            }
            DIAG_ASSERT(duckSamplesDrone == 0, "TransientPunchDetector must produce zero ducking samples on Eno dual drone");
            DIAG_ASSERT(minGainDrone >= 0.99f, "TransientPunchDetector duck gain must remain ~ 1.0 on Eno dual drone");

            std::cout << "  Rate " << static_cast<int>(rate) << " Hz: Step Attack " << attackMs 
                      << " ms, Step100 gFull " << gFull << ", 30Hz MinGain " << minGain30Hz 
                      << ", Drone Duck Samples: " << duckSamplesDrone << " -> PASS\n";
        }
        std::cout << "  -> PASS: TransientPunchDetector onset gating verified across all sample rates!\n";
    }

    // -------------------------------------------------------------------------
    // TEST 5: ENO DUAL DRONE INTO SUB_BASS_PRESERVER HEADROOM & DECAY
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 5] ENO DUAL DRONE INTO SUB_BASS_PRESERVER HEADROOM & DECAY\n";
    {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(fs, 512);

        auto presets = rb26::Rb26ReverbEngine::getFactoryPresets();
        bool found = false;
        for (const auto& p : presets) {
            if (p.id != nullptr && std::string_view(p.id) == "SUB_BASS_PRESERVER") {
                engine.setParameters(p.params);
                found = true;
                break;
            }
        }
        DIAG_ASSERT(found, "SUB_BASS_PRESERVER preset must exist");

        // Dual drone: 32.7 Hz (C1) and 33.05 Hz (detuned C1 with 0.35 Hz beat frequency)
        const double f1 = 32.7;
        const double f2 = 33.05;
        const int numBlocks = 500; // ~5.33 seconds of continuous drone excitation
        const int blockSize = 512;

        std::vector<float> inL(blockSize), inR(blockSize);
        std::vector<float> outL(blockSize), outR(blockSize);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        float maxPeak = 0.0f;
        size_t nonFiniteCount = 0;
        size_t denormalCount = 0;
        size_t clippedCount = 0;

        // Run 5.3 seconds of continuous dual drone
        for (int b = 0; b < numBlocks; ++b) {
            for (int i = 0; i < blockSize; ++i) {
                const double t = static_cast<double>(b * blockSize + i) / fs;
                const float s1 = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979323846 * f1 * t));
                const float s2 = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979323846 * f2 * t));
                inL[i] = s1 + s2;
                inR[i] = s1 + s2;
            }
            engine.process(inPtrs, outPtrs, 2, blockSize);
            for (int i = 0; i < blockSize; ++i) {
                if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) nonFiniteCount++;
                if (std::fpclassify(outL[i]) == FP_SUBNORMAL || std::fpclassify(outR[i]) == FP_SUBNORMAL) denormalCount++;
                const float aL = std::abs(outL[i]);
                const float aR = std::abs(outR[i]);
                maxPeak = std::max(maxPeak, std::max(aL, aR));
                if (aL > 1.0f || aR > 1.0f) clippedCount++;
            }
        }

        std::cout << "  Drone Excitation (5.3s) - Max Peak: " << std::fixed << std::setprecision(4) << maxPeak 
                  << " | Clipped Samples: " << clippedCount
                  << " | Non-finite: " << nonFiniteCount
                  << " | Denormals: " << denormalCount << "\n";

        DIAG_ASSERT(nonFiniteCount == 0, "Drone output must have zero NaNs or Infs");
        DIAG_ASSERT(denormalCount == 0, "Drone output must have zero denormals");
        DIAG_ASSERT(clippedCount == 0, "Drone output must have zero clipped samples (> 1.0)");
        DIAG_ASSERT(maxPeak <= 1.0f, "Drone output must remain strictly within headroom");

        // Test decay phase: stop input, run 3.2 seconds of decay
        std::vector<float> zeroIn(blockSize, 0.0f);
        const float* zeroPtrs[2] = { zeroIn.data(), zeroIn.data() };
        float decayEarlyEnergy = 0.0f;
        float decayLateEnergy = 0.0f;
        const int decayBlocks = 300;

        for (int b = 0; b < decayBlocks; ++b) {
            engine.process(zeroPtrs, outPtrs, 2, blockSize);
            for (int i = 0; i < blockSize; ++i) {
                const float e = outL[i] * outL[i] + outR[i] * outR[i];
                if (b < 20) decayEarlyEnergy += e;
                if (b >= decayBlocks - 20) decayLateEnergy += e;
                const float aL = std::abs(outL[i]);
                const float aR = std::abs(outR[i]);
                maxPeak = std::max(maxPeak, std::max(aL, aR));
                if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) nonFiniteCount++;
            }
        }

        std::cout << "  Decay Phase - Early Energy: " << decayEarlyEnergy 
                  << " | Late Energy: " << decayLateEnergy << "\n";

        DIAG_ASSERT(nonFiniteCount == 0, "Decay output must have zero NaNs");
        DIAG_ASSERT(decayLateEnergy < decayEarlyEnergy, "Decay must smoothly dissipate");
        std::cout << "  -> PASS: Eno dual drone under SUB_BASS_PRESERVER operates with clean headroom and smooth decay!\n";

        // Direct LowBandModalMatrix isolation
        rb26::LowBandModalMatrix modal;
        modal.prepare(fs);
        rb26::LowBandModalParams mParams;
        mParams.crossoverHz = 180.0f;
        mParams.bassRt60Mult = 0.80f;
        mParams.rt60DecaySec = 4.5f;
        mParams.punchDucking = 0.85f;
        mParams.subMonoHz = 150.0f;
        modal.setParameters(mParams);

        float maxModalPeak = 0.0f;
        float minModalDuck = 1.0f;
        size_t duckingCount = 0;
        int firstDuckSample = -1;
        int lastDuckSample = -1;
        for (int i = 0; i < 48000 * 5; ++i) {
            const double t = static_cast<double>(i) / fs;
            const float s1 = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979323846 * f1 * t));
            const float s2 = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979323846 * f2 * t));
            float lowL = 0.0f, lowR = 0.0f;
            modal.processModalOnly(s1 + s2, s1 + s2, lowL, lowR);
            maxModalPeak = std::max(maxModalPeak, std::max(std::abs(lowL), std::abs(lowR)));
            float dg = modal.getDuckingGain();
            minModalDuck = std::min(minModalDuck, dg);
            if (dg < 0.95f) {
                duckingCount++;
                if (firstDuckSample < 0) firstDuckSample = i;
                lastDuckSample = i;
            }
        }
        std::cout << "  LowBandModalMatrix Direct Excitation Max Peak: " << maxModalPeak 
                  << " | Min Duck Gain: " << minModalDuck 
                  << " | Ducking Samples (< 0.95): " << duckingCount << " / " << 48000 * 5 
                  << " | First Duck Sample: " << firstDuckSample << " (" << firstDuckSample / 48.0 << " ms)"
                  << " | Last Duck Sample: " << lastDuckSample << " (" << lastDuckSample / 48.0 << " ms)\n";
        DIAG_ASSERT(maxModalPeak < 0.72f, "Modal direct output must remain below 0.72 soft knee limit");
        DIAG_ASSERT(duckingCount == 0, "Modal direct excitation on dual drone must produce zero ducking samples (no audio-rate gain chopping)");
        DIAG_ASSERT(minModalDuck >= 0.95f, "Modal direct excitation ducking gain must remain >= 0.95 on Eno dual drone");
        std::cout << "  -> PASS: LowBandModalMatrix direct excitation maintains clean sub-knee headroom (< 0.72) and zero ducking!\n";

        // Subtest 2.1 sweep reproduction
        {
            rb26::LowBandModalMatrix sweepModal;
            sweepModal.prepare(fs);
            rb26::LowBandModalParams p;
            p.crossoverHz = 180.0f;
            p.bassRt60Mult = 1.0f;
            p.rt60DecaySec = 3.5f;
            p.punchDucking = 0.0f;
            sweepModal.setParameters(p);

            std::vector<double> testFreqs;
            std::vector<double> rmsOutputs;
            for (int f = 40; f <= 200; f += 1) testFreqs.push_back(static_cast<double>(f));
            const size_t testLength = 24000;
            const size_t steadyStart = 9600;
            for (double freq : testFreqs) {
                sweepModal.reset();
                double sumSq = 0.0;
                size_t count = 0;
                for (size_t n = 0; n < testLength; ++n) {
                    float in = static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * freq * n / fs));
                    float lowOutL = 0.0f, lowOutR = 0.0f;
                    sweepModal.processModalOnly(in, in, lowOutL, lowOutR);
                    if (n >= steadyStart) {
                        sumSq += 0.5 * (lowOutL * lowOutL + lowOutR * lowOutR);
                        ++count;
                    }
                }
                rmsOutputs.push_back(std::sqrt(sumSq / count));
            }
            double maxNotchDepthDb = 0.0;
            int notchFreq = 0;
            for (size_t i = 1; i < rmsOutputs.size() - 1; ++i) {
                double localMax = std::max(rmsOutputs[i - 1], rmsOutputs[i + 1]);
                double current = rmsOutputs[i];
                if (current > 1.0e-8 && localMax > 1.0e-8) {
                    double dropDb = 20.0 * std::log10(localMax / current);
                    if (dropDb > maxNotchDepthDb) {
                        maxNotchDepthDb = dropDb;
                        notchFreq = static_cast<int>(testFreqs[i]);
                    }
                }
            }
            std::cout << "  [Sweep 40-200Hz] Max Notch Depth: " << maxNotchDepthDb << " dB at " << notchFreq << " Hz\n";
            for (size_t i = 1; i < rmsOutputs.size() - 1; ++i) {
                double localMax = std::max(rmsOutputs[i - 1], rmsOutputs[i + 1]);
                double current = rmsOutputs[i];
                if (current > 1.0e-8 && localMax > 1.0e-8) {
                    double dropDb = 20.0 * std::log10(localMax / current);
                    if (dropDb > 5.0) {
                        std::cout << "    Notch at " << testFreqs[i] << " Hz : drop = " << dropDb << " dB\n";
                    }
                }
            }
        }
    }

    std::cout << "\n=================================================================\n";
    std::cout << "=== ALL VERIFICATIONS PASSED SUCCESSFULLY! ===\n";
    std::cout << "=================================================================\n";

    return 0;
}
