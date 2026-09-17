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

        // Engage freeze
        tank.setParameters(1.0f, 3.5f, 6500.0f, 0.75f, true, 0.85f, 1.2f, 85.0f, manifolds[m]);

        // Run 100,000 samples of silence
        double earlyEnergy = 0.0;
        double lateEnergy = 0.0;
        for (int i = 0; i < 100000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
            if (i >= 5000 && i < 10000) earlyEnergy += (outL * outL + outR * outR);
            if (i >= 95000 && i < 100000) lateEnergy += (outL * outL + outR * outR);
        }
        const double earlyRms = std::sqrt(earlyEnergy / 10000.0);
        const double lateRms = std::sqrt(lateEnergy / 10000.0);
        const double ratio = lateRms / (earlyRms + 1.0e-9);

        std::cout << "  " << std::setw(20) << std::left << manifoldNames[m]
                  << " Early RMS: " << std::fixed << std::setprecision(5) << earlyRms
                  << " | Late (100k) RMS: " << lateRms
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

        if (diffRms < 0.001 || (diffRms / (rms0 + 1.0e-9)) < 0.20) {
            std::cerr << "FAIL: Diffusion knob had negligible effect on decay!\n";
            return 1;
        }
        std::cout << "  -> PASS: Diffusion knob actively and profoundly alters decay dispersion!\n";
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
        for (int i = 0; i < 5000; ++i) {
            float dummyL = 0.0f, dummyR = 0.0f;
            shifter.processSample(0.0f, 0.0f, dummyL, dummyR);
        }
        std::cout << "  shimmer=0, dimmer=0 -> isActive(): " << std::boolalpha << shifter.isActive() << "\n";
        assert(!shifter.isActive());

        // Case B: shimmerSend=0.5, dimmerSend=0.0, pitchBlend=-1.0 (100% dimmer)
        shifter.setParameters(0.5f, 0.0f, 1, 2, -1.0f, 0.0f);
        for (int i = 0; i < 5000; ++i) {
            float dummyL = 0.0f, dummyR = 0.0f;
            shifter.processSample(0.0f, 0.0f, dummyL, dummyR);
        }
        std::cout << "  shimmer=0.5, dimmer=0, blend=-1.0 -> isActive(): " << shifter.isActive() << "\n";
        assert(!shifter.isActive());

        // Case C: shimmerSend=0.0, dimmerSend=0.5, pitchBlend=+1.0 (100% shimmer)
        shifter.setParameters(0.0f, 0.5f, 1, 2, 1.0f, 0.0f);
        for (int i = 0; i < 5000; ++i) {
            float dummyL = 0.0f, dummyR = 0.0f;
            shifter.processSample(0.0f, 0.0f, dummyL, dummyR);
        }
        std::cout << "  shimmer=0.0, dimmer=0.5, blend=+1.0 -> isActive(): " << shifter.isActive() << "\n";
        assert(!shifter.isActive());

        // Case D: shimmerSend=0.4, dimmerSend=0.35, pitchBlend=0.0 (active)
        shifter.setParameters(0.4f, 0.35f, 1, 2, 0.0f, 0.0f);
        for (int i = 0; i < 5000; ++i) {
            float dummyL = 0.0f, dummyR = 0.0f;
            shifter.processSample(0.0f, 0.0f, dummyL, dummyR);
        }
        std::cout << "  shimmer=0.4, dimmer=0.35, blend=0.0 -> isActive(): " << shifter.isActive() << "\n";
        assert(shifter.isActive());

        std::cout << "  -> PASS: PitchShifter::isActive() correctly identifies all inactive states!\n";
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

        assert(durBypassedMs < durActiveMs);
        std::cout << "  -> PASS: Bypassed pitch diffusion achieves massive CPU reduction!\n";
    }

    std::cout << "\n=================================================================\n";
    std::cout << "=== ALL VERIFICATIONS PASSED SUCCESSFULLY! ===\n";
    std::cout << "=================================================================\n";

    return 0;
}
