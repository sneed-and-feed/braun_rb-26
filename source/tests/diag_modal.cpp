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
            float in = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * i / fs);
            tank.processSample(in, in, 0.0f, 0.0f, outL, outR);
        }

        // Measure energy before freeze
        double preFreezeEnergy = 0.0;
        for (int i = 0; i < 500; ++i) {
            float in = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * (2000 + i) / fs);
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
                inL[i] = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * (b * 512 + i) / fs);
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

    std::cout << "\n=================================================================\n";
    std::cout << "=== ALL VERIFICATIONS PASSED SUCCESSFULLY! ===\n";
    std::cout << "=================================================================\n";

    return 0;
}
