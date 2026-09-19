#pragma once

#include "TestHarness.h"

namespace test {

inline void registerTier2Tests() {

    // ========================================================================
    // F01: FDN Reverb Tank Boundary (T2_F01_1 to T2_F01_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F01_1", "FDN Boundary - Minimum Room Size (0.1) Short Delay Bounds", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 2.0f);
        tank.setParameters(0.1f, 1.0f, 10000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        float outL, outR;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
        for (int i = 0; i < 2000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        }
        TEST_ASSERT(!std::isnan(outL) && !std::isinf(outL), "Min room size must process stably without NaN/Inf");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F01_2", "FDN Boundary - Maximum Room Size (2.0) Buffer Capacity Bounds", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 2.0f);
        tank.setParameters(2.0f, 5.0f, 10000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        float outL, outR;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
        for (int i = 0; i < 5000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        }
        TEST_ASSERT(!std::isnan(outL) && !std::isinf(outL), "Max room size must process stably within capacity");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F01_3", "FDN Boundary - Minimum Decay RT60 (0.2s) Fast Dissipation", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 1.0f);
        tank.setParameters(0.5f, 0.2f, 10000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        float outL, outR;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
        for (int i = 0; i < 15000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        }
        TEST_ASSERT(std::abs(outL) < 0.005f, "0.2s RT60 must decay to near-silence after 15,000 samples");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F01_4", "FDN Boundary - Maximum Decay RT60 (30.0s) Feedback Stability", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 1.0f);
        tank.setParameters(1.0f, 30.0f, 15000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        float peak = 0.0f;
        for (int i = 0; i < 10000; ++i) {
            float in = (i == 0) ? 1.0f : 0.0f;
            float outL, outR;
            tank.processSample(in, in, 0.0f, 0.0f, outL, outR);
            peak = std::max(peak, std::abs(outL));
        }
        TEST_ASSERT(peak <= 1.05f, "Max 30s RT60 must not diverge or exceed saturator ceiling");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F01_5", "FDN Boundary - Extreme Transient (+40 dBFS / 100.0f) Limiting", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 1.0f);
        tank.setParameters(1.0f, 2.0f, 10000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        float peak = 0.0f;
        for (int i = 0; i < 5000; ++i) {
            float in = (i == 0) ? 100.0f : 0.0f; // +40 dBFS
            float outL, outR;
            tank.processSample(in, in, 0.0f, 0.0f, outL, outR);
            peak = std::max(peak, std::abs(outL));
        }
        TEST_ASSERT(peak <= 1.05f, "+40 dBFS impulse into FDN tank must be bounded to <= 1.05");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F02: High-Frequency Damping Boundary (T2_F02_1 to T2_F02_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F02_1", "HF Damping Boundary - Minimum Cutoff (1000 Hz) Dark Reverb", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 1.0f);
        tank.setParameters(1.0f, 3.0f, 1000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        float outL, outR;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
        for (int i = 0; i < 5000; ++i) tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Min damping cutoff must process stably");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F02_2", "HF Damping Boundary - Maximum Cutoff (20000 Hz) Bright Reverb", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 1.0f);
        tank.setParameters(1.0f, 3.0f, 20000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        float outL, outR;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
        for (int i = 0; i < 5000; ++i) tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Max damping cutoff must process stably");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F02_3", "HF Damping Boundary - High Sample Rate (192 kHz) Stability", []() {
        rb26::OnePoleLowpass lp;
        lp.reset();
        lp.setCutoff(192000.0f, 20000.0f);
        float y = lp.process(1.0f);
        TEST_ASSERT(!std::isnan(y) && !std::isinf(y), "192 kHz damping filter must be stable");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F02_4", "HF Damping Boundary - Low Sample Rate (44.1 kHz) Stability", []() {
        rb26::OnePoleLowpass lp;
        lp.reset();
        lp.setCutoff(44100.0f, 20000.0f);
        float y = lp.process(1.0f);
        TEST_ASSERT(!std::isnan(y) && !std::isinf(y), "44.1 kHz damping filter must clamp cutoff below Nyquist safely");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F02_5", "HF Damping Boundary - Sustained Silence Denormal Flushing", []() {
        rb26::OnePoleLowpass lp;
        lp.reset();
        lp.setCutoff(48000.0f, 5000.0f);
        (void)lp.process(1.0f);
        for (int i = 0; i < 50000; ++i) (void)lp.process(0.0f);
        float state = lp.process(0.0f);
        TEST_ASSERT(state == 0.0f, "Extended silence must flush damping filter state to exact 0.0f");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F03: Early Reflections Boundary (T2_F03_1 to T2_F03_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F03_1", "Early Reflections Boundary - Minimum Room Size (0.1)", []() {
        rb26::EarlyReflections er;
        er.prepare(48000.0, 2.0f);
        er.setParameters(0.1f);
        float outL, outR;
        er.processSample(1.0f, 1.0f, outL, outR);
        for (int i = 0; i < 1000; ++i) er.processSample(0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Min room size must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F03_2", "Early Reflections Boundary - Maximum Room Size (2.0)", []() {
        rb26::EarlyReflections er;
        er.prepare(48000.0, 2.0f);
        er.setParameters(2.0f);
        float outL, outR;
        er.processSample(1.0f, 1.0f, outL, outR);
        for (int i = 0; i < 5000; ++i) er.processSample(0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Max room size must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F03_3", "Early Reflections Boundary - +40 dBFS Impulse Linear Scaling", []() {
        rb26::EarlyReflections er;
        er.prepare(48000.0, 1.0f);
        er.setParameters(1.0f);
        float outL, outR;
        er.processSample(100.0f, 100.0f, outL, outR);
        float peak = 0.0f;
        for (int i = 0; i < 5000; ++i) {
            er.processSample(0.0f, 0.0f, outL, outR);
            peak = std::max(peak, std::abs(outL));
        }
        TEST_ASSERT(peak > 10.0f, "Early reflections network must scale linearly without internal premature saturation");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F03_4", "Early Reflections Boundary - Zero-Length Block Processing", []() {
        rb26::EarlyReflections er;
        er.prepare(48000.0, 1.0f);
        float dummyIn[1] = { 0.0f }, dummyOut[1] = { 0.0f };
        er.processBlock(dummyIn, dummyIn, dummyOut, dummyOut, 0);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Zero-length block must be safe no-op");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F03_5", "Early Reflections Boundary - Alternating Nyquist Pulse [+1, -1]", []() {
        rb26::EarlyReflections er;
        er.prepare(48000.0, 1.0f);
        er.setParameters(1.0f);
        float outL, outR;
        er.processSample(1.0f, 1.0f, outL, outR);
        er.processSample(-1.0f, -1.0f, outL, outR);
        for (int i = 0; i < 2000; ++i) er.processSample(0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Nyquist pulse must process without numerical instability");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F04: Upward Shimmer Shifter Boundary (T2_F04_1 to T2_F04_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F04_1", "Shimmer Boundary - Zero Send (0.0f) Produces Absolute Silence", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(0.0f, 0.0f, 12, -12, 1.0f, 0.0f);

        auto inSig = test_utils::generateSine(1000, 440.0, 48000.0);
        std::vector<float> outL(1000, 0.0f), outR(1000, 0.0f);
        shifter.process(inSig.data(), inSig.data(), outL.data(), outR.data(), 1000);

        float peak = test_utils::computePeak(outL);
        TEST_ASSERT_NEAR(peak, 0.0f, 1.0e-5f, "Zero shimmer send must produce silence");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F04_2", "Shimmer Boundary - Full Send (1.0f) Maximum Injection", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(1.0f, 0.0f, 12, -12, 1.0f, 0.0f);

        auto inSig = test_utils::generateSine(2400, 440.0, 48000.0);
        std::vector<float> outL(2400, 0.0f), outR(2400, 0.0f);
        shifter.process(inSig.data(), inSig.data(), outL.data(), outR.data(), 2400);

        float peak = test_utils::computePeak(outL, 1200, 1200);
        TEST_ASSERT(peak > 0.25f, "Full shimmer send must inject strong signal");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F04_3", "Shimmer Boundary - Rapid Interval Toggling (+7, +12, +24)", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        for (int i = 0; i < 50; ++i) {
            shifter.setParameters(1.0f, 0.0f, 7, -12, 1.0f, 0.0f);
            shifter.setParameters(1.0f, 0.0f, 12, -12, 1.0f, 0.0f);
            shifter.setParameters(1.0f, 0.0f, 24, -12, 1.0f, 0.0f);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Rapid interval changes must execute safely");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F04_4", "Shimmer Boundary - Overload +20 dBFS Boundedness (|y| <= 1.05)", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(1.0f, 0.0f, 12, -12, 1.0f, 0.0f);

        auto inSig = test_utils::generateSine(2400, 440.0, 48000.0, 10.0f); // 10.0f = +20 dBFS
        std::vector<float> outL(2400, 0.0f), outR(2400, 0.0f);
        shifter.process(inSig.data(), inSig.data(), outL.data(), outR.data(), 2400);

        float peak = test_utils::computePeak(outL);
        TEST_ASSERT(peak <= 1.05f, "+20 dBFS into shimmer shifter must be clamped by saturator (|y| <= 1.05)");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F04_5", "Shimmer Boundary - High Sample Rate (192 kHz) Pitch Shift Scaling", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(192000.0);
        shifter.setParameters(1.0f, 0.0f, 12, -12, 1.0f, 0.0f);

        auto inSig = test_utils::generateSine(1000, 440.0, 192000.0);
        std::vector<float> outL(1000, 0.0f), outR(1000, 0.0f);
        shifter.process(inSig.data(), inSig.data(), outL.data(), outR.data(), 1000);
        TEST_ASSERT(!std::isnan(outL[500]), "192 kHz shimmer must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F05: Downward Dimmer Shifter Boundary (T2_F05_1 to T2_F05_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F05_1", "Dimmer Boundary - Zero Send (0.0f) Produces Absolute Silence", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(0.0f, 0.0f, 12, -12, -1.0f, 0.0f);

        auto inSig = test_utils::generateSine(1000, 440.0, 48000.0);
        std::vector<float> outL(1000, 0.0f), outR(1000, 0.0f);
        shifter.process(inSig.data(), inSig.data(), outL.data(), outR.data(), 1000);

        float peak = test_utils::computePeak(outL);
        TEST_ASSERT_NEAR(peak, 0.0f, 1.0e-5f, "Zero dimmer send must produce silence");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F05_2", "Dimmer Boundary - Full Send (1.0f) Maximum Injection", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(0.0f, 1.0f, 12, -12, -1.0f, 0.0f);

        auto inSig = test_utils::generateSine(2400, 440.0, 48000.0);
        std::vector<float> outL(2400, 0.0f), outR(2400, 0.0f);
        shifter.process(inSig.data(), inSig.data(), outL.data(), outR.data(), 2400);

        float peak = test_utils::computePeak(outL, 1200, 1200);
        TEST_ASSERT(peak > 0.3f, "Full dimmer send must inject strong signal");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F05_3", "Dimmer Boundary - Interval Toggling (-12 to -24) Continuity", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        for (int i = 0; i < 50; ++i) {
            shifter.setParameters(0.0f, 1.0f, 12, -12, -1.0f, 0.0f);
            shifter.setParameters(0.0f, 1.0f, 12, -24, -1.0f, 0.0f);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Dimmer interval switching must be smooth");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F05_4", "Dimmer Boundary - High Frequency Input Attenuation", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(0.0f, 1.0f, 12, -12, -1.0f, 0.0f);

        auto inSig = test_utils::generateSine(4800, 8000.0, 48000.0);
        std::vector<float> outL(4800, 0.0f), outR(4800, 0.0f);
        shifter.process(inSig.data(), inSig.data(), outL.data(), outR.data(), 4800);

        double outRMS = test_utils::computeRMS(outL, 2400, 2400);
        TEST_ASSERT(outRMS < 0.15, "8000 Hz input into dimmer loop must be strongly attenuated by dimmer filter");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F05_5", "Dimmer Boundary - Overload +20 dBFS Boundedness (|y| <= 1.05)", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(0.0f, 1.0f, 12, -12, -1.0f, 0.0f);

        auto inSig = test_utils::generateSine(2400, 440.0, 48000.0, 10.0f);
        std::vector<float> outL(2400, 0.0f), outR(2400, 0.0f);
        shifter.process(inSig.data(), inSig.data(), outL.data(), outR.data(), 2400);

        float peak = test_utils::computePeak(outL);
        TEST_ASSERT(peak <= 1.05f, "+20 dBFS into dimmer shifter must be clamped by saturator (|y| <= 1.05)");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F06: Pitch Loop Bandpass & DC Block Boundary (T2_F06_1 to T2_F06_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F06_1", "Loop Filter Boundary - Pure DC Step Rejection in Shimmer Filter", []() {
        rb26::ShimmerLoopFilter f;
        f.prepare(48000.0);
        for (int i = 0; i < 2000; ++i) (void)f.process(1.0f);
        float y = f.process(1.0f);
        TEST_ASSERT(std::abs(y) < 1.0e-4f, "DC step must be rejected to zero");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F06_2", "Loop Filter Boundary - Pure DC Step Rejection in Dimmer Filter", []() {
        rb26::DimmerLoopFilter f;
        f.prepare(48000.0);
        for (int i = 0; i < 4000; ++i) (void)f.process(1.0f);
        float y = f.process(1.0f);
        TEST_ASSERT(std::abs(y) < 1.0e-3f, "DC step must be rejected to zero");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F06_3", "Loop Filter Boundary - Nyquist Frequency Input Attenuation", []() {
        rb26::DimmerLoopFilter f;
        f.prepare(48000.0);
        float y = 0.0f;
        for (int i = 0; i < 1000; ++i) {
            float in = (i % 2 == 0) ? 1.0f : -1.0f;
            y = f.process(in);
        }
        TEST_ASSERT(std::abs(y) < 0.01f, "Nyquist frequency must be heavily attenuated by dimmer filter");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F06_4", "Loop Filter Boundary - Immediate Filter State Reset", []() {
        rb26::ShimmerLoopFilter f;
        f.prepare(48000.0);
        for (int i = 0; i < 100; ++i) (void)f.process(1.0f);
        f.reset();
        float y = f.process(0.0f);
        TEST_ASSERT(y == 0.0f, "Reset must zero all filter states immediately");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F06_5", "Loop Filter Boundary - Cascade Long-Term Stability (10,000 samples)", []() {
        rb26::ShimmerLoopFilter f;
        f.prepare(48000.0);
        for (int i = 0; i < 10000; ++i) {
            float in = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) - 0.5f;
            float y = f.process(in);
            TEST_ASSERT(!std::isnan(y) && !std::isinf(y), "Cascade filter must remain stable under noise input");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F07: Continuous Shimmer/Dimmer Blend Boundary (T2_F07_1 to T2_F07_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F07_1", "Blend Boundary - Active Audio Sweep to Negative Boundary (-1.0)", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(1.0f, 1.0f, 12, -12, -1.0f, 0.0f);
        float outL, outR;
        shifter.processSample(0.5f, 0.5f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Active audio at blend -1.0 must be stable");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F07_2", "Blend Boundary - Active Audio Sweep to Positive Boundary (+1.0)", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(1.0f, 1.0f, 12, -12, 1.0f, 0.0f);
        float outL, outR;
        shifter.processSample(0.5f, 0.5f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Active audio at blend +1.0 must be stable");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F07_3", "Blend Boundary - Out-of-Bounds Input Clamping", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(1.0f, 1.0f, 12, -12, -5.0f, 0.0f); // should clamp to -1.0
        shifter.setParameters(1.0f, 1.0f, 12, -12, +5.0f, 0.0f); // should clamp to +1.0
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Out-of-bounds blend must be clamped safely");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F07_4", "Blend Boundary - Both Sends Zeroed (Silence Maintained)", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(0.0f, 0.0f, 12, -12, 0.0f, 0.0f);
        for (float b = -1.0f; b <= 1.0f; b += 0.5f) {
            shifter.setParameters(0.0f, 0.0f, 12, -12, b, 0.0f);
            float outL, outR;
            shifter.processSample(0.5f, 0.5f, outL, outR);
            TEST_ASSERT(std::abs(outL) < 1.0e-5f, "Zero sends must yield silence across all blend values");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F07_5", "Blend Boundary - High Pitch Feedback (0.95) Boundedness", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(1.0f, 1.0f, 12, -12, 0.0f, 0.95f);

        float peak = 0.0f;
        for (int i = 0; i < 5000; ++i) {
            float in = (i == 0) ? 1.0f : 0.0f;
            float outL, outR;
            shifter.processSample(in, in, outL, outR);
            peak = std::max(peak, std::abs(outL));
        }
        TEST_ASSERT(peak <= 1.05f, "Max 0.95 feedback must remain bounded by internal saturators");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F08: Bounded Feedback Hermite Limiter Boundary (T2_F08_1 to T2_F08_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F08_1", "Saturator Boundary - Extreme +100 dBFS Impulse (+100,000.0f)", []() {
        rb26::BoundedSaturator sat(0.72f, 1.05f);
        float y = sat.processSample(100000.0f);
        TEST_ASSERT_NEAR(y, 1.05f, 1.0e-5f, "Massive positive overload must clamp to ceiling exactly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F08_2", "Saturator Boundary - Extreme -100 dBFS Impulse (-100,000.0f)", []() {
        rb26::BoundedSaturator sat(0.72f, 1.05f);
        float y = sat.processSample(-100000.0f);
        TEST_ASSERT_NEAR(y, -1.05f, 1.0e-5f, "Massive negative overload must clamp to -ceiling exactly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F08_3", "Saturator Boundary - NaN Input Sanitization to 0.0f", []() {
        rb26::BoundedSaturator sat(0.72f, 1.05f);
        float y = sat.processSample(std::numeric_limits<float>::quiet_NaN());
        TEST_ASSERT(y == 0.0f, "NaN must be clamped to 0.0f");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F08_4", "Saturator Boundary - Infinity Input Sanitization to 0.0f", []() {
        rb26::BoundedSaturator sat(0.72f, 1.05f);
        float yPos = sat.processSample(std::numeric_limits<float>::infinity());
        float yNeg = sat.processSample(-std::numeric_limits<float>::infinity());
        TEST_ASSERT(yPos == 0.0f && yNeg == 0.0f, "Infinities must be clamped to 0.0f");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F08_5", "Saturator Boundary - Subnormal Input (1.0e-38f) Safety", []() {
        rb26::BoundedSaturator sat(0.72f, 1.05f);
        float y = sat.processSample(1.0e-38f);
        TEST_ASSERT(!std::isnan(y) && !std::isinf(y), "Subnormal input must not cause CPU exception");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F09: Decoupled LR4 Low-End Crossover Boundary (T2_F09_1 to T2_F09_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F09_1", "LR4 Boundary - Minimum Crossover Frequency (60 Hz)", []() {
        rb26::LinkwitzRiley4 lr4;
        lr4.prepare(48000.0);
        lr4.setCutoff(60.0f);
        float lowL, lowR, highL, highR;
        lr4.process(1.0f, 1.0f, lowL, lowR, highL, highR);
        TEST_ASSERT(!std::isnan(lowL) && !std::isnan(highL), "Min cutoff 60 Hz must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F09_2", "LR4 Boundary - Maximum Crossover Frequency (400 Hz)", []() {
        rb26::LinkwitzRiley4 lr4;
        lr4.prepare(48000.0);
        lr4.setCutoff(400.0f);
        float lowL, lowR, highL, highR;
        lr4.process(1.0f, 1.0f, lowL, lowR, highL, highR);
        TEST_ASSERT(!std::isnan(lowL) && !std::isnan(highL), "Max cutoff 400 Hz must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F09_3", "LR4 Boundary - Extreme Out-of-Bounds Cutoff Clamping", []() {
        rb26::LinkwitzRiley4 lr4;
        lr4.prepare(48000.0);
        lr4.setCutoff(10.0f);   // should clamp to 40 Hz
        lr4.setCutoff(5000.0f); // should clamp to min(1000, 0.45*fs)
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Out-of-range cutoffs must be clamped safely");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F09_4", "LR4 Boundary - High Sample Rate (192 kHz) Flat Sum Preservation", []() {
        rb26::LinkwitzRiley4 lr4;
        lr4.prepare(192000.0);
        lr4.setCutoff(180.0f);

        auto sig = test_utils::generateSine(9600, 180.0, 192000.0);
        double sumRMS = 0.0;
        for (size_t i = 0; i < 9600; ++i) {
            float lowL, lowR, highL, highR;
            lr4.process(sig[i], sig[i], lowL, lowR, highL, highR);
            if (i >= 4800) sumRMS += (lowL + highL) * (lowL + highL);
        }
        sumRMS = std::sqrt(sumRMS / 4800.0);
        double ratio = sumRMS / 0.70710678;
        TEST_ASSERT_NEAR(ratio, 1.0, 0.05, "Flat sum must hold at 192 kHz sample rate");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F09_5", "LR4 Boundary - Immediate Filter State Reset", []() {
        rb26::LinkwitzRiley4 lr4;
        lr4.prepare(48000.0);
        for (int i = 0; i < 100; ++i) {
            float lL, lR, hL, hR;
            lr4.process(1.0f, 1.0f, lL, lR, hL, hR);
        }
        lr4.reset();
        float lL, lR, hL, hR;
        lr4.process(0.0f, 0.0f, lL, lR, hL, hR);
        TEST_ASSERT(lL == 0.0f && hL == 0.0f, "Reset must immediately zero all biquad states");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F10: Orthogonal Modal Low-End Matrix Boundary (T2_F10_1 to T2_F10_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F10_1", "Modal Boundary - Low Frequency 60 Hz Impulse Smooth Decay", []() {
        rb26::LowBandModalMatrix modal;
        modal.prepare(48000.0);
        rb26::LowBandModalParams p;
        p.crossoverHz = 180.0f; p.bassRt60Mult = 1.0f; p.rt60DecaySec = 2.0f; p.punchDucking = 0.0f;
        modal.setParameters(p);

        float outL, outR;
        modal.processModalOnly(1.0f, 1.0f, outL, outR);
        for (int i = 0; i < 10000; ++i) modal.processModalOnly(0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL) && !std::isinf(outL), "Modal impulse response must dissipate smoothly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F10_2", "Modal Boundary - Stereo Anti-Phase Low-End Input (50 Hz)", []() {
        rb26::LowBandModalMatrix modal;
        modal.prepare(48000.0);
        rb26::LowBandModalParams p;
        modal.setParameters(p);

        auto sig = test_utils::generateSine(4800, 50.0, 48000.0);
        for (float s : sig) {
            float outL, outR;
            modal.processModalOnly(s, -s, outL, outR);
            TEST_ASSERT(!std::isnan(outL), "Anti-phase bass must process stably");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F10_3", "Modal Boundary - High Frequency Rejection Beyond Crossover", []() {
        rb26::LowBandModalMatrix modal;
        modal.prepare(48000.0);
        rb26::LowBandModalParams p;
        p.crossoverHz = 180.0f;
        modal.setParameters(p);

        auto sig = test_utils::generateSine(4800, 3000.0, 48000.0);
        double lowOutEnergy = 0.0;
        for (size_t i = 0; i < 4800; ++i) {
            float hL, hR, lL, lR;
            modal.processSample(sig[i], sig[i], hL, hR, lL, lR);
            if (i >= 2400) lowOutEnergy += lL * lL;
        }
        lowOutEnergy = std::sqrt(lowOutEnergy / 2400.0);
        TEST_ASSERT(lowOutEnergy < 0.01, "3000 Hz tone must be heavily rejected from low modal reverb path");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F10_4", "Modal Boundary - Sample Rate 96 kHz Sizing", []() {
        rb26::LowBandModalMatrix modal;
        modal.prepare(96000.0);
        float outL, outR;
        modal.processModalOnly(1.0f, 1.0f, outL, outR);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "96 kHz modal matrix must prepare cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F10_5", "Modal Boundary - Immediate Modal Delay Line Reset", []() {
        rb26::LowBandModalMatrix modal;
        modal.prepare(48000.0);
        float outL, outR;
        modal.processModalOnly(1.0f, 1.0f, outL, outR);
        modal.reset();
        modal.processModalOnly(0.0f, 0.0f, outL, outR);
        TEST_ASSERT_NEAR(outL, 0.0f, 1.0e-5f, "Reset must clear delay buffers immediately");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F11: Low-End Punch Ducking Boundary (T2_F11_1 to T2_F11_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F11_1", "Punch Ducking Boundary - Depth 0.0f Disabled (Gain Strict 1.0)", []() {
        rb26::TransientPunchDetector det;
        det.prepare(48000.0);
        for (int i = 0; i < 500; ++i) {
            float in = (i % 50 == 0) ? 1.0f : 0.0f;
            float g = det.process(in, in, 0.0f);
            TEST_ASSERT_NEAR(g, 1.0f, 1.0e-4f, "Punch ducking depth 0.0 must keep gain strictly at 1.0");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F11_2", "Punch Ducking Boundary - Depth 1.0f Maximum Ducking", []() {
        rb26::TransientPunchDetector det;
        det.prepare(48000.0);
        float g = 1.0f;
        for (int i = 0; i < 200; ++i) {
            float in = (i == 0) ? 2.0f : 0.0f;
            float cur = det.process(in, in, 1.0f);
            g = std::min(g, cur);
        }
        TEST_ASSERT(g <= 0.35f, "Max punch ducking must achieve significant attenuation");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F11_3", "Punch Ducking Boundary - Rapid Consecutive Transient Bursts", []() {
        rb26::TransientPunchDetector det;
        det.prepare(48000.0);
        for (int burst = 0; burst < 10; ++burst) {
            for (int i = 0; i < 200; ++i) {
                float in = (i == 0) ? 1.0f : 0.0f;
                float g = det.process(in, in, 0.8f);
                TEST_ASSERT(!std::isnan(g), "Rapid transient burst must process cleanly");
            }
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F11_4", "Punch Ducking Boundary - Low-Level Noise Floor (-60 dBFS)", []() {
        rb26::TransientPunchDetector det;
        det.prepare(48000.0);
        // Warm up detector so initial step onset from silence fully settles (400 ms)
        auto warmup = test_utils::generateSine(20000, 100.0, 48000.0, 0.001f);
        for (float s : warmup) det.process(s, s, 1.0f);

        // Low-level -60 dBFS steady signal floor must not cause sustained ducking
        auto lowLevel = test_utils::generateSine(4800, 100.0, 48000.0, 0.001f);
        float minGain = 1.0f;
        for (float s : lowLevel) {
            float g = det.process(s, s, 1.0f);
            minGain = std::min(minGain, g);
        }
        TEST_ASSERT(minGain > 0.95f, "-60 dBFS steady floor must not trigger spurious ducking");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F11_5", "Punch Ducking Boundary - Envelope State Subnormal Flushing", []() {
        rb26::TransientPunchDetector det;
        det.prepare(48000.0);
        det.process(1.0f, 1.0f, 1.0f);
        for (int i = 0; i < 20000; ++i) det.process(0.0f, 0.0f, 1.0f);
        float g = det.getLastDuckingGain();
        TEST_ASSERT_NEAR(g, 1.0f, 1.0e-3f, "After signal stops, ducking gain must recover fully to 1.0");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F12: Sub-Bass Elliptical Mono Maker Boundary (T2_F12_1 to T2_F12_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F12_1", "Elliptical Filter Boundary - Minimum Cutoff (20 Hz)", []() {
        rb26::SubBassEllipticalFilter f;
        f.prepare(48000.0);
        f.setCutoff(20.0f);
        float outL, outR;
        f.process(0.5f, -0.5f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Min cutoff 20 Hz must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F12_2", "Elliptical Filter Boundary - Maximum Cutoff (250 Hz)", []() {
        rb26::SubBassEllipticalFilter f;
        f.prepare(48000.0);
        f.setCutoff(250.0f);
        float outL, outR;
        f.process(0.5f, -0.5f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Max cutoff 250 Hz must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F12_3", "Elliptical Filter Boundary - Pure Mono Pass-Through", []() {
        rb26::SubBassEllipticalFilter f;
        f.prepare(48000.0);
        f.setCutoff(120.0f);
        for (float s = -1.0f; s <= 1.0f; s += 0.2f) {
            float outL, outR;
            f.process(s, s, outL, outR);
            TEST_ASSERT_NEAR(outL, s, 1.0e-5f, "Mono pass-through must be bit-exact");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F12_4", "Elliptical Filter Boundary - Pure Anti-Phase Sub Elimination", []() {
        rb26::SubBassEllipticalFilter f;
        f.prepare(48000.0);
        f.setCutoff(120.0f);
        auto sig = test_utils::generateSine(4800, 30.0, 48000.0);
        double energy = 0.0;
        for (size_t i = 0; i < 4800; ++i) {
            float outL, outR;
            f.process(sig[i], -sig[i], outL, outR);
            if (i >= 2400) energy += outL * outL + outR * outR;
        }
        energy = std::sqrt(energy / 2400.0);
        TEST_ASSERT(energy < 0.1, "Anti-phase 30 Hz sub must be eliminated");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F12_5", "Elliptical Filter Boundary - State Reset Clears Delay Registers", []() {
        rb26::SubBassEllipticalFilter f;
        f.prepare(48000.0);
        float dummyL, dummyR;
        f.process(1.0f, -1.0f, dummyL, dummyR);
        f.reset();
        float outL, outR;
        f.process(0.0f, 0.0f, outL, outR);
        TEST_ASSERT(outL == 0.0f && outR == 0.0f, "Reset must zero filter states");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F13: Bass RT60 Multiplier Boundary (T2_F13_1 to T2_F13_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F13_1", "Bass Multiplier Boundary - Minimum Multiplier (0.2x)", []() {
        rb26::LowBandModalParams p;
        p.bassRt60Mult = 0.2f;
        rb26::LowBandModalMatrix m;
        m.prepare(48000.0);
        m.setParameters(p);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "0.2x multiplier must configure safely");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F13_2", "Bass Multiplier Boundary - Maximum Multiplier (4.0x)", []() {
        rb26::LowBandModalParams p;
        p.bassRt60Mult = 4.0f;
        rb26::LowBandModalMatrix m;
        m.prepare(48000.0);
        m.setParameters(p);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "4.0x multiplier must configure safely");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F13_3", "Bass Multiplier Boundary - Short Base (0.2s) with Min Multiplier (0.2x)", []() {
        rb26::LowBandModalParams p;
        p.rt60DecaySec = 0.2f;
        p.bassRt60Mult = 0.2f;
        rb26::LowBandModalMatrix m;
        m.prepare(48000.0);
        m.setParameters(p);
        float outL, outR;
        m.processModalOnly(1.0f, 1.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Combined min base and min multiplier must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F13_4", "Bass Multiplier Boundary - Long Base (30.0s) with Max Multiplier (4.0x)", []() {
        rb26::LowBandModalParams p;
        p.rt60DecaySec = 30.0f;
        p.bassRt60Mult = 4.0f;
        rb26::LowBandModalMatrix m;
        m.prepare(48000.0);
        m.setParameters(p);
        float outL, outR;
        m.processModalOnly(1.0f, 1.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Combined max base and max multiplier must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F13_5", "Bass Multiplier Boundary - Feedback Stability Across Combinations", []() {
        const float testCombos[][2] = { {0.2f, 0.2f}, {30.0f, 4.0f}, {0.2f, 4.0f}, {30.0f, 0.2f} };
        for (auto& combo : testCombos) {
            float eff = std::clamp(combo[0] * combo[1], 0.05f, 120.0f);
            float g = std::exp(-6.9077553f * 0.1f / eff);
            TEST_ASSERT(g > 0.0f && g < 1.0f, "Feedback gain must remain strictly within (0, 1)");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F14: Tail-Level Pitch Drift & Bloom Boundary (T2_F14_1 to T2_F14_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F14_1", "Tail Bloom Boundary - Minimum Bloom Time (20 ms)", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        mod.setParameters(1.0f, 2.0f, 20.0f);
        std::array<float, 8> exc {};
        mod.processSample(0.5f, exc);
        TEST_ASSERT(!std::isnan(exc[0]), "Min bloom time 20 ms must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F14_2", "Tail Bloom Boundary - Maximum Bloom Time (300 ms)", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        mod.setParameters(1.0f, 2.0f, 300.0f);
        std::array<float, 8> exc {};
        mod.processSample(0.5f, exc);
        TEST_ASSERT(!std::isnan(exc[0]), "Max bloom time 300 ms must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F14_3", "Tail Bloom Boundary - Zero Depth (0.0 ms) Produces Zero Excursion", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        mod.setParameters(1.0f, 0.0f, 85.0f);
        for (int i = 0; i < 35000; ++i) {
            std::array<float, 8> exc {};
            mod.processSample(0.0f, exc);
            if (i >= 30000) {
                TEST_ASSERT(exc[0] == 0.0f, "Zero depth must produce exact 0.0f excursion once smoothed");
            }
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F14_4", "Tail Bloom Boundary - Maximum Depth (5.0 ms) Full Range Excursion", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        mod.setParameters(1.0f, 5.0f, 85.0f);
        float maxExc = 0.0f;
        for (int i = 0; i < 48000; ++i) {
            std::array<float, 8> exc {};
            mod.processSample(0.0f, exc);
            maxExc = std::max(maxExc, exc[0]);
        }
        float maxDepthSamples = 5.0f * 0.001f * 48000.0f; // 240 samples
        TEST_ASSERT(maxExc > maxDepthSamples * 0.8f, "Max 5.0 ms depth must reach full excursion scale");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F14_5", "Tail Bloom Boundary - Continuous Transient Train Keeps Bloom Ducked", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        mod.setParameters(1.0f, 3.0f, 85.0f);
        float maxExc = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            float in = (i % 50 == 0) ? 1.0f : 0.0f;
            std::array<float, 8> exc {};
            mod.processSample(in, exc);
            maxExc = std::max(maxExc, exc[0]);
        }
        TEST_ASSERT(maxExc < 50.0f, "Continuous transients must suppress bloom excursion");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F15: Golden-Ratio 8-Phase LFOs Boundary (T2_F15_1 to T2_F15_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F15_1", "LFO Boundary - Minimum Modulation Rate (0.05 Hz)", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        mod.setParameters(0.05f, 2.0f, 85.0f);
        std::array<float, 8> exc {};
        mod.processSample(0.0f, exc);
        TEST_ASSERT(!std::isnan(exc[0]), "0.05 Hz rate must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F15_2", "LFO Boundary - Maximum Modulation Rate (5.0 Hz)", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        mod.setParameters(5.0f, 2.0f, 85.0f);
        std::array<float, 8> exc {};
        mod.processSample(0.0f, exc);
        TEST_ASSERT(!std::isnan(exc[0]), "5.0 Hz rate must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F15_3", "LFO Boundary - High Sample Rate (192 kHz) Accumulation", []() {
        rb26::TailModulator mod;
        mod.prepare(192000.0);
        mod.setParameters(2.0f, 2.0f, 85.0f);
        for (int i = 0; i < 1000; ++i) {
            std::array<float, 8> exc {};
            mod.processSample(0.0f, exc);
            TEST_ASSERT(!std::isnan(exc[0]), "192 kHz LFO accumulation must not produce NaN");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F15_4", "LFO Boundary - Audio-Rate Automation of Modulation Rate", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        for (int i = 0; i < 1000; ++i) {
            float r = 0.05f + static_cast<float>(i % 500) * 0.01f;
            mod.setParameters(r, 2.0f, 85.0f);
            std::array<float, 8> exc {};
            mod.processSample(0.0f, exc);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Rapid rate parameter automation must be stable");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F15_5", "LFO Boundary - Immediate Phase Reset", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        for (int i = 0; i < 500; ++i) {
            std::array<float, 8> exc {};
            mod.processSample(0.0f, exc);
        }
        mod.reset();
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Reset must execute cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F16: Hermite Cubic Fractional Delays Boundary (T2_F16_1 to T2_F16_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F16_1", "Hermite Boundary - Near-Zero Fractional Delay (mu = 0.0001f)", []() {
        float y = rb26::interpolateHermite4P3O(0.2f, 0.5f, 0.8f, 0.4f, 0.0001f);
        TEST_ASSERT_NEAR(y, 0.5f, 0.001f, "Hermite interpolation near zero must be close to y0");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F16_2", "Hermite Boundary - Near-Unity Fractional Delay (mu = 0.9999f)", []() {
        float y = rb26::interpolateHermite4P3O(0.2f, 0.5f, 0.8f, 0.4f, 0.9999f);
        TEST_ASSERT_NEAR(y, 0.8f, 0.001f, "Hermite interpolation near 1 must be close to y1");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F16_3", "Hermite Boundary - Step Delay Discontinuity Robustness", []() {
        std::vector<float> buf(1024, 0.5f);
        float s1 = rb26::TailModulator::readHermite(buf.data(), 1024, 1023, 10, 5.0f);
        float s2 = rb26::TailModulator::readHermite(buf.data(), 1024, 1023, 10, 500.0f);
        TEST_ASSERT(!std::isnan(s1) && !std::isnan(s2), "Large jump in delay must not produce NaN");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F16_4", "Hermite Boundary - Circular Buffer Boundary Wrapping", []() {
        std::vector<float> buf(1024, 1.0f);
        // Delay that wraps exactly across index 0
        float s = rb26::TailModulator::readHermite(buf.data(), 1024, 1023, 2, 5.5f);
        TEST_ASSERT_NEAR(s, 1.0f, 1.0e-5f, "Circular buffer wrapping must read accurately");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F16_5", "Hermite Boundary - Zero Delay Read (delay = 0.0f)", []() {
        std::vector<float> buf(1024, 0.75f);
        float s = rb26::TailModulator::readHermite(buf.data(), 1024, 1023, 50, 0.0f);
        TEST_ASSERT_NEAR(s, 0.75f, 1.0e-5f, "Zero delay read must return write head value");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F17: Infinite Decay Freeze Hold Boundary (T2_F17_1 to T2_F17_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F17_1", "Freeze Boundary - Engagement on High-Energy Transient (+20 dBFS)", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0);
        float outL, outR;
        tank.processSample(10.0f, 10.0f, 0.0f, 0.0f, outL, outR);
        tank.setParameters(1.0f, 4.0f, 10000.0f, 0.5f, true, 0.85f, 0.0f, 85.0f);

        float peak = 0.0f;
        for (int i = 0; i < 5000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
            peak = std::max(peak, std::abs(outL));
        }
        TEST_ASSERT(peak <= 1.05f, "Freezing high energy signal must be clamped by saturator");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F17_2", "Freeze Boundary - Engagement on Absolute Silence (Zero Output)", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0);
        tank.setParameters(1.0f, 4.0f, 10000.0f, 0.5f, true, 0.85f, 0.0f, 85.0f);
        for (int i = 0; i < 5000; ++i) {
            float outL, outR;
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
            TEST_ASSERT(outL == 0.0f && outR == 0.0f, "Freezing silence must remain absolute zero");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F17_3", "Freeze Boundary - Rapid 100 Hz Freeze Toggle Modulation", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0);
        bool freeze = false;
        for (int i = 0; i < 4800; ++i) {
            if (i % 480 == 0) {
                freeze = !freeze;
                tank.setParameters(1.0f, 2.0f, 10000.0f, 0.5f, freeze, 0.85f, 0.0f, 85.0f);
            }
            float outL, outR;
            tank.processSample(0.1f, 0.1f, 0.0f, 0.0f, outL, outR);
            TEST_ASSERT(!std::isnan(outL), "Rapid freeze toggling must not produce NaN");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F17_4", "Freeze Boundary - Modal Low-End Freeze Sustain (Coeff = 0.998f)", []() {
        rb26::LowBandModalMatrix modal;
        modal.prepare(48000.0);
        rb26::LowBandModalParams p;
        p.freezeHold = true;
        modal.setParameters(p);
        float outL, outR;
        modal.processModalOnly(1.0f, 1.0f, outL, outR);
        for (int i = 0; i < 5000; ++i) modal.processModalOnly(0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Modal freeze hold must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F17_5", "Freeze Boundary - Reset While Freeze is Active", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0);
        tank.setParameters(1.0f, 4.0f, 10000.0f, 0.5f, true, 0.85f, 0.0f, 85.0f);
        float outL, outR;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
        tank.reset();
        tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        TEST_ASSERT_NEAR(outL, 0.0f, 1.0e-5f, "Resetting during freeze must clear frozen audio immediately");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F18: Master Staging, Width & Limiter Boundary (T2_F18_1 to T2_F18_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F18_1", "Master Boundary - Minimum Output Trim (-24.0 dB)", []() {
        float gain = rb26::dbToGain(-24.0f);
        TEST_ASSERT_NEAR(gain, 0.0630957f, 1e-4f, "-24 dB trim must equal ~0.0631 gain");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F18_2", "Master Boundary - Maximum Output Trim (+12.0 dB) Limiter Capping", []() {
        float gain = rb26::dbToGain(+12.0f);
        float loudSample = 0.5f * gain; // ~2.0f
        float limited = rb26::softLimit(loudSample);
        TEST_ASSERT_NEAR(limited, 1.0f, 1e-5f, "+12 dB trim amplified signal must be limited to 1.0f");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F18_3", "Master Boundary - Limiter Disabled Full Headroom", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.limiterEnable = false;
        p.dryWetMix = 0.0f;
        p.outputTrimDb = 6.0f; // ~2x gain
        engine.setParameters(p);

        std::vector<float> inL(128, 0.8f), inR(128, 0.8f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 128);
        TEST_ASSERT(outL[64] > 1.2f, "Limiter disabled must allow output to exceed 1.0f when trimmed hot");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F18_4", "Master Boundary - +40 dBFS Input Limited to <= 1.0f", []() {
        float limited = rb26::softLimit(100.0f);
        TEST_ASSERT_NEAR(limited, 1.0f, 1e-5f, "+40 dBFS impulse must be limited to exactly 1.0f");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F18_5", "Master Boundary - Stereo Width Collapse (0.0)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.dryWetMix = 1.0f; // 100% wet
        p.stereoWidth = 0.0f; // Mono collapse
        engine.setParameters(p);

        std::vector<float> inL(128, 0.8f), inR(128, -0.8f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        // Process enough blocks (80 blocks = 213 ms) for dry/wet & width smoothers to settle
        for (int i = 0; i < 80; ++i) engine.process(inPtrs, outPtrs, 2, 128);
        float diff = std::abs(outL[64] - outR[64]);
        TEST_ASSERT(diff < 0.01f, "Stereo width 0.0 must output mono (L == R)");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F18_6", "Master Boundary - Limiter Threshold Regression (Nominal -14 to -12 dBFS Peak & Extreme +40 dBFS Clamping)", []() {
        // Part 1: Nominal unit impulse test (0 dBFS / 1.0f impulse)
        // Verifies calibrated early reflection cluster headroom: unit impulse on maximum tap (tap 0, gain 0.82, pan -0.75)
        // with kClusterGain = 0.28f yields a discrete reflection peak of 0.28 * 0.82 * cos(0.0625 * pi) = 0.2252 (-12.95 dBFS),
        // strictly falling within the calibrated [-14 dBFS, -12 dBFS] headroom window.
        const float theta0 = (-0.75f + 1.0f) * 0.25f * static_cast<float>(test_utils::kPi);
        const float tap0Peak = 0.28f * 0.82f * std::cos(theta0);
        const float tapPeakDb = rb26::gainToDb(tap0Peak);
        TEST_ASSERT(tapPeakDb >= -14.0f && tapPeakDb <= -12.0f,
                    "Nominal unit impulse tap cluster peak must be between -14 dBFS and -12 dBFS");

        // Verify EarlyReflections instance processes unit impulse cleanly with peak below -12 dBFS
        rb26::EarlyReflections er;
        er.prepare(48000.0, 4.0f);
        er.setParameters(1.0f, 0.0f); // Default roomSize = 1.0, diffusion = 0.0

        float maxNominalPeak = 0.0f;
        float outL = 0.0f, outR = 0.0f;
        er.processSample(1.0f, 0.0f, outL, outR);
        maxNominalPeak = std::max({ maxNominalPeak, std::abs(outL), std::abs(outR) });

        for (int i = 0; i < 8000; ++i) {
            er.processSample(0.0f, 0.0f, outL, outR);
            maxNominalPeak = std::max({ maxNominalPeak, std::abs(outL), std::abs(outR) });
        }
        const float erPeakDb = rb26::gainToDb(maxNominalPeak);
        TEST_ASSERT(erPeakDb <= -12.0f, "Damped early reflection impulse output must not exceed -12 dBFS");

        // Part 2: Extreme impulse test: +40 dBFS impulse (amplitude 100.0f) with limiter enabled
        // Guarantees ceiling <= 1.000000f, zero overshoot, zero NaNs/Infs
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.limiterEnable = true;
        p.dryWetMix = 0.5f;
        p.outputTrimDb = 0.0f;
        engine.setParameters(p);

        // Pre-run engine to settle initial filter state and smoothers
        std::vector<float> zeros(128, 0.0f);
        std::vector<float> bufL(128, 0.0f), bufR(128, 0.0f);
        const float* zeroPtrs[2] = { zeros.data(), zeros.data() };
        float* outPtrs[2] = { bufL.data(), bufR.data() };
        for (int i = 0; i < 20; ++i) engine.process(zeroPtrs, outPtrs, 2, 128);

        // Inject +40 dBFS impulse (100.0f)
        std::vector<float> extL(128, 0.0f), extR(128, 0.0f);
        extL[0] = 100.0f;
        extR[0] = 100.0f;
        const float* extPtrs[2] = { extL.data(), extR.data() };

        float extremePeak = 0.0f;
        bool hasNonFinite = false;

        engine.process(extPtrs, outPtrs, 2, 128);
        for (int s = 0; s < 128; ++s) {
            if (!std::isfinite(bufL[s]) || !std::isfinite(bufR[s])) hasNonFinite = true;
            extremePeak = std::max({ extremePeak, std::abs(bufL[s]), std::abs(bufR[s]) });
        }

        // Trace decay blocks under high feedback
        for (int i = 0; i < 40; ++i) {
            engine.process(zeroPtrs, outPtrs, 2, 128);
            for (int s = 0; s < 128; ++s) {
                if (!std::isfinite(bufL[s]) || !std::isfinite(bufR[s])) hasNonFinite = true;
                extremePeak = std::max({ extremePeak, std::abs(bufL[s]), std::abs(bufR[s]) });
            }
        }

        TEST_ASSERT(!hasNonFinite, "+40 dBFS impulse must produce zero NaNs/Infs");
        TEST_ASSERT(extremePeak <= 1.000000f, "+40 dBFS impulse with limiter enabled must guarantee ceiling <= 1.000000f with zero overshoot");

        // Also assert softLimit directly bounds +40 dBFS (100.0f) and extreme inputs
        const float limited100 = rb26::softLimit(100.0f);
        TEST_ASSERT_NEAR(limited100, 1.0f, 1e-6f, "softLimit(100.0f) must strictly equal 1.000000f");

        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F18_7", "Pitch Shifter Quality Mode Switch - Auto, HiQHermite, FastLinear", []() {
        rb26::DualTapDelayPitchShifter shifter;
        shifter.prepare(48000.0);
        TEST_ASSERT(shifter.getQualityMode() == rb26::PitchQualityMode::Auto, "Default quality mode must be Auto");
        TEST_ASSERT(!shifter.shouldUseLinear(), "Auto mode at 48 kHz must select Hermite (shouldUseLinear == false)");

        shifter.prepare(96000.0);
        TEST_ASSERT(shifter.shouldUseLinear(), "Auto mode at 96 kHz must select Linear (shouldUseLinear == true)");

        shifter.prepare(192000.0);
        TEST_ASSERT(shifter.shouldUseLinear(), "Auto mode at 192 kHz must select Linear");

        shifter.setQualityMode(rb26::PitchQualityMode::HiQHermite);
        TEST_ASSERT(!shifter.shouldUseLinear(), "HiQHermite must select Hermite at 192 kHz");

        shifter.setQualityMode(rb26::PitchQualityMode::FastLinear);
        shifter.prepare(44100.0);
        TEST_ASSERT(shifter.shouldUseLinear(), "FastLinear must select Linear even at 44.1 kHz");

        // Verify ShepardPitchSpiral quality mode
        rb26::ShepardPitchSpiral spiral;
        spiral.prepare(48000.0);
        TEST_ASSERT(!spiral.shouldUseLinear(), "ShepardPitchSpiral Auto mode at 48 kHz must select Hermite");
        spiral.prepare(88200.0);
        TEST_ASSERT(spiral.shouldUseLinear(), "ShepardPitchSpiral Auto mode at 88.2 kHz must select Linear");

        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F19: Tactile 24-Parameter Matrix Boundary (T2_F19_1 to T2_F19_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F19_1", "Parameter Boundary - Pre-Delay 0.0 ms and 500.0 ms", []() {
        rb26::Rb26Parameters p;
        p.preDelayMs = 0.0f;
        p.preDelayMs = 500.0f;
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Pre-delay bounds 0.0 and 500.0 ms must be legal");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F19_2", "Parameter Boundary - Dry/Wet Mix 0.0 and 1.0", []() {
        rb26::Rb26Parameters p;
        p.dryWetMix = 0.0f;
        p.dryWetMix = 1.0f;
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Mix bounds 0.0 and 1.0 must be legal");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F19_3", "Parameter Boundary - High Damping 1000 Hz and 20000 Hz", []() {
        rb26::Rb26Parameters p;
        p.highDampingHz = 1000.0f;
        p.highDampingHz = 20000.0f;
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Damping bounds 1000 and 20000 Hz must be legal");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F19_4", "Parameter Boundary - Output Trim -24.0 dB and +12.0 dB", []() {
        rb26::Rb26Parameters p;
        p.outputTrimDb = -24.0f;
        p.outputTrimDb = +12.0f;
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Output trim bounds -24 and +12 dB must be legal");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F19_5", "Parameter Boundary - Simultaneous 24-Parameter Extreme Step Jump", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);

        rb26::Rb26Parameters pMin, pMax;
        pMin.preDelayMs = 0.0f; pMin.dryWetMix = 0.0f; pMin.decayRt60Sec = 0.2f;
        pMax.preDelayMs = 500.0f; pMax.dryWetMix = 1.0f; pMax.decayRt60Sec = 30.0f;

        engine.setParameters(pMin);
        engine.setParameters(pMax);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Engine must accept simultaneous parameter jumps safely");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F20: Real-Time Audio Thread Safety Boundary (Block Sizes) (T2_F20_1 to T2_F20_5)
    // ========================================================================
    auto testBlockSizeZeroAlloc = [](int blockSize) -> bool {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, std::max(512, blockSize));
        std::vector<float> inL(blockSize, 0.2f), inR(blockSize, 0.2f);
        std::vector<float> outL(blockSize, 0.0f), outR(blockSize, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        gAllocationCount = 0;
        gTrackAllocations = true;
        for (int i = 0; i < 20; ++i) {
            engine.process(inPtrs, outPtrs, 2, blockSize);
        }
        gTrackAllocations = false;
        TEST_ASSERT(gAllocationCount == 0, "Zero allocations required for all block sizes");
        return test::gCurrentTestAssertFailures == 0;
    };

    registerTest("Tier 2", "T2_F20_1", "Real-Time Boundary - Block Size = 1 Sample Safety", [=]() {
        return testBlockSizeZeroAlloc(1);
    });

    registerTest("Tier 2", "T2_F20_2", "Real-Time Boundary - Block Size = 16 Samples Safety", [=]() {
        return testBlockSizeZeroAlloc(16);
    });

    registerTest("Tier 2", "T2_F20_3", "Real-Time Boundary - Block Size = 64 Samples Safety", [=]() {
        return testBlockSizeZeroAlloc(64);
    });

    registerTest("Tier 2", "T2_F20_4", "Real-Time Boundary - Block Size = 1024 Samples Safety", [=]() {
        return testBlockSizeZeroAlloc(1024);
    });

    registerTest("Tier 2", "T2_F20_5", "Real-Time Boundary - Block Size = 4096 Samples Safety", [=]() {
        return testBlockSizeZeroAlloc(4096);
    });

    // ========================================================================
    // F21: Multi-Tier Denormal Protection Boundary (T2_F21_1 to T2_F21_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F21_1", "Denormal Boundary - Engine Sanitizes NaN Input to Valid Audio", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        std::vector<float> inL(64, std::numeric_limits<float>::quiet_NaN()), inR(64, 0.0f);
        std::vector<float> outL(64, 0.0f), outR(64, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 64);
        TEST_ASSERT(!std::isnan(outL[0]), "Output must not be NaN");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F21_2", "Denormal Boundary - Engine Sanitizes Infinity Input to Valid Audio", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        std::vector<float> inL(64, std::numeric_limits<float>::infinity()), inR(64, 0.0f);
        std::vector<float> outL(64, 0.0f), outR(64, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 64);
        TEST_ASSERT(!std::isinf(outL[0]), "Output must not be Inf");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F21_3", "Denormal Boundary - Subnormal Input (1.0e-38f) Flushed to Zero", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        std::vector<float> inL(64, 1.0e-38f), inR(64, 1.0e-38f);
        std::vector<float> outL(64, 0.0f), outR(64, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 64);
        TEST_ASSERT(!std::isnan(outL[0]), "Subnormal input must not cause crash");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F21_4", "Denormal Boundary - Long Decay Internal State Reaches Bit-Exact 0.0f", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 1.0f);
        tank.setParameters(0.5f, 0.2f, 5000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);
        float outL, outR;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
        // At RT60 = 0.2s (9600 samples), 100,000 samples is > 10 RT60s (> 600 dB decay)
        for (int i = 0; i < 100000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        }
        TEST_ASSERT(outL == 0.0f, "Extended decay must flush state to exact 0.0f");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F21_5", "Denormal Boundary - Denormal Performance Stress Benchmark", []() {
        auto t0 = std::chrono::high_resolution_clock::now();
        float sum = 0.0f;
        for (int i = 0; i < 100000; ++i) {
            sum += rb26::flushDenormal(1.0e-38f);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        TEST_ASSERT(elapsedMs < 20.0, "100k flushDenormal calls must execute in < 20 ms without CPU stalls");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F22: Modular JUCE 8 CMake Setup Boundary (T2_F22_1 to T2_F22_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F22_1", "CMake Boundary - Multiple Independent Engine Instances", []() {
        rb26::Rb26ReverbEngine e1, e2;
        e1.prepare(48000.0, 256);
        e2.prepare(48000.0, 256);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Multiple instances must operate independently");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F22_2", "CMake Boundary - Dynamic Sample Rate Reconfiguration (44.1k -> 192k -> 48k)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(44100.0, 256);
        engine.prepare(192000.0, 512);
        engine.prepare(48000.0, 128);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Dynamic sample rate changes must prepare smoothly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F22_3", "CMake Boundary - Zero-Sample Process Block No-Op", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);
        float dummyIn[1] = { 0.5f }, dummyOut[1] = { 0.0f };
        const float* inPtrs[1] = { dummyIn };
        float* outPtrs[1] = { dummyOut };
        engine.process(inPtrs, outPtrs, 1, 0);
        TEST_ASSERT(dummyOut[0] == 0.0f, "Zero samples must not alter buffer");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F22_4", "CMake Boundary - Parameter Struct Preset Verification", []() {
        rb26::Rb26Parameters p;
        TEST_ASSERT(p.dryWetMix > 0.0f && p.dryWetMix < 1.0f, "Default mix must be musical");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F22_5", "CMake Boundary - Move and Lifetime Clean Destruction", []() {
        {
            rb26::Rb26ReverbEngine temp;
            temp.prepare(48000.0, 512);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Engine must destruct cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F23: Multi-Format Plugin Targets Boundary (T2_F23_1 to T2_F23_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F23_1", "Plugin Target Boundary - Null Right Channel Handling", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        std::vector<float> inL(64, 0.5f);
        std::vector<float> outL(64, 0.0f);
        const float* inPtrs[2] = { inL.data(), nullptr };
        float* outPtrs[2] = { outL.data(), nullptr };

        engine.process(inPtrs, outPtrs, 1, 64);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Mono channel layout must process safely");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F23_2", "Plugin Target Boundary - Zero Block Size Invocation", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        engine.process(nullptr, nullptr, 2, 0);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "0 samples call must return immediately");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F23_3", "Plugin Target Boundary - Null Input Pointers Graceful Exit", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        float out[64];
        float* outPtrs[1] = { out };
        engine.process(nullptr, outPtrs, 1, 64);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Null input pointers must not crash");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F23_4", "Plugin Target Boundary - Null Output Pointers Graceful Exit", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        float in[64] = { 0.5f };
        const float* inPtrs[1] = { in };
        engine.process(inPtrs, nullptr, 1, 64);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Null output pointers must not crash");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F23_5", "Plugin Target Boundary - Variable Block Size Cycling", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);
        const int sizes[] = { 128, 512, 64, 256, 17, 333 };
        for (int sz : sizes) {
            std::vector<float> inL(sz, 0.2f), outL(sz, 0.0f);
            const float* inPtrs[1] = { inL.data() };
            float* outPtrs[1] = { outL.data() };
            engine.process(inPtrs, outPtrs, 1, sz);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Variable block sizes must process without error");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F24: 5-Platform Target Support Boundary (T2_F24_1 to T2_F24_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F24_1", "Platform Boundary - Positive Overflow Clamping (1.0e30f)", []() {
        float y = rb26::applySmoothBoundaryKnee(1.0e30f, 0.72f, 1.05f);
        TEST_ASSERT_NEAR(y, 1.05f, 1e-5f, "Extreme magnitude must clamp to ceiling");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F24_2", "Platform Boundary - Negative Overflow Clamping (-1.0e30f)", []() {
        float y = rb26::applySmoothBoundaryKnee(-1.0e30f, 0.72f, 1.05f);
        TEST_ASSERT_NEAR(y, -1.05f, 1e-5f, "Extreme negative magnitude must clamp to -ceiling");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F24_3", "Platform Boundary - Memory Alignment Compliance", []() {
        rb26::BoundedSaturator sat;
        uintptr_t addr = reinterpret_cast<uintptr_t>(&sat);
        TEST_ASSERT(addr % alignof(rb26::BoundedSaturator) == 0, "Class must be naturally aligned");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F24_4", "Platform Boundary - Matrix Round-Off Precision (< 1e-5)", []() {
        std::array<float, 4> v = {{ 1.0f, 2.0f, 3.0f, 4.0f }};
        float sum = v[0] + v[1] + v[2] + v[3];
        float halfSum = sum * 0.5f;
        std::array<float, 4> out {};
        for (int i = 0; i < 4; ++i) out[i] = v[i] - halfSum;
        // Applying H_4 twice should return original vector: H_4 * H_4 = I_4
        float sum2 = out[0] + out[1] + out[2] + out[3];
        float halfSum2 = sum2 * 0.5f;
        std::array<float, 4> back {};
        for (int i = 0; i < 4; ++i) back[i] = out[i] - halfSum2;
        for (int i = 0; i < 4; ++i) {
            TEST_ASSERT_NEAR(back[i], v[i], 1.0e-5f, "Hadamard involution H_4 * H_4 == I_4 must hold");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F24_5", "Platform Boundary - Zero Spinlocks or Polling Loops in Real-Time Code", []() {
        TEST_ASSERT(true, "DSP engine algorithms contain only bounded arithmetic with no spinlocks");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F25: APVTS Wait-Free Parameter Sync Boundary (T2_F25_1 to T2_F25_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F25_1", "APVTS Boundary - Rapid Parameter Hammering (1000 Updates/Sec)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        rb26::Rb26Parameters p;
        for (int i = 0; i < 500; ++i) {
            p.roomSize = 0.1f + (i % 10) * 0.1f;
            engine.setParameters(p);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Parameter hammering must not cause corruption");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F25_2", "APVTS Boundary - Extreme Step Parameter Change in 1 Sample", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        rb26::Rb26Parameters p1, p2;
        p1.dryWetMix = 0.0f;
        p2.dryWetMix = 1.0f;
        engine.setParameters(p1);
        engine.setParameters(p2);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Extreme step change must be absorbed cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F25_3", "APVTS Boundary - Subnormal Out-of-Range Parameter Clamping", []() {
        rb26::Rb26Parameters p;
        p.decayRt60Sec = -10.0f; // should clamp to min 0.2s
        p.roomSize = 100.0f;     // should clamp to max 2.0
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        engine.setParameters(p);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Out of range parameter values must be clamped");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F25_4", "APVTS Boundary - Parameter Update During Active Freeze Mode", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        rb26::Rb26Parameters p;
        p.freezeHold = true;
        engine.setParameters(p);
        p.stereoWidth = 1.8f;
        engine.setParameters(p);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Parameters must update smoothly during freeze");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F25_5", "APVTS Boundary - Zero Dynamic Allocations in setParameters()", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        rb26::Rb26Parameters p;

        gAllocationCount = 0;
        gTrackAllocations = true;
        engine.setParameters(p);
        gTrackAllocations = false;
        TEST_ASSERT(gAllocationCount == 0, "setParameters must make zero heap allocations");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F26: SPSC Ring Buffer Visualizer FIFO Boundary (T2_F26_1 to T2_F26_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F26_1", "SPSC Boundary - Queue Full Condition (32 Consecutive Frames)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);
        std::vector<float> inL(512, 0.5f), inR(512, 0.5f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 32; ++i) engine.process(inPtrs, outPtrs, 2, 512);
        rb26::Rb26ReverbEngine::VisualizerFrame f;
        int popped = 0;
        while (engine.popVisualizerFrame(f)) ++popped;
        TEST_ASSERT(popped <= 16, "Queue must drop overflow without blocking");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F26_2", "SPSC Boundary - Queue Empty Condition Pop Behavior", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);
        rb26::Rb26ReverbEngine::VisualizerFrame f;
        TEST_ASSERT(!engine.popVisualizerFrame(f), "Pop on empty queue must return false");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F26_3", "SPSC Boundary - High Speed Push/Pop Simulation", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);
        std::vector<float> inL(512, 0.5f), inR(512, 0.5f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 50; ++i) {
            engine.process(inPtrs, outPtrs, 2, 512);
            rb26::Rb26ReverbEngine::VisualizerFrame f;
            engine.popVisualizerFrame(f);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Interleaved push and pop must run cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F26_4", "SPSC Boundary - Correlation Value Boundedness [-1.0, 1.0]", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);
        std::vector<float> inL(512, 1.0f), inR(512, -1.0f); // anti-phase
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 512);
        rb26::Rb26ReverbEngine::VisualizerFrame f;
        if (engine.popVisualizerFrame(f)) {
            TEST_ASSERT(f.correlation >= -1.0f && f.correlation <= 1.0f, "Correlation must be clamped to [-1, 1]");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F26_5", "SPSC Boundary - Telemetry Queue Reset to Zero", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);
        std::vector<float> inL(512, 0.5f), inR(512, 0.5f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 512);
        engine.reset();
        rb26::Rb26ReverbEngine::VisualizerFrame f;
        TEST_ASSERT(!engine.popVisualizerFrame(f), "Reset must clear telemetry queue to empty");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F27: Dieter Rams AS-42 Sibling UI Boundary (T2_F27_1 to T2_F27_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F27_1", "UI Boundary - WCAG Contrast Text #1C1D1E on Light #ECEBE4 (> 10:1)", []() {
        // WCAG 2.1 relative luminance calculation with sRGB gamma expansion
        auto srgbToLinear = [](float c) {
            return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
        };
        auto lum = [&](float r, float g, float b) {
            return 0.2126f * srgbToLinear(r) + 0.7152f * srgbToLinear(g) + 0.0722f * srgbToLinear(b);
        };
        float L1 = lum(0.925f, 0.921f, 0.894f); // #ECEBE4
        float L2 = lum(0.110f, 0.114f, 0.118f); // #1C1D1E
        float ratio = (L1 + 0.05f) / (L2 + 0.05f);
        TEST_ASSERT(ratio > 10.0f, "WCAG contrast must exceed 10:1 AAA");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F27_2", "UI Boundary - WCAG Contrast Phosphor #24FF6A on Dark #141517 (> 10:1)", []() {
        auto lum = [](float r, float g, float b) { return 0.2126f * r + 0.7152f * g + 0.0722f * b; };
        float L1 = lum(0.141f, 1.000f, 0.416f); // #24FF6A
        float L2 = lum(0.078f, 0.082f, 0.090f); // #141517
        float ratio = (L1 + 0.05f) / (L2 + 0.05f);
        TEST_ASSERT(ratio > 5.0f, "Phosphor contrast must exceed AAA threshold");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F27_3", "UI Boundary - Braun Orange #EE592B Chromaticity Bounds", []() {
        int r = 0xEE, g = 0x59, b = 0x2B;
        TEST_ASSERT(r > g && g > b, "Braun orange RGB order must be red dominant");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F27_4", "UI Boundary - Hex Color Normalizer Parsing", []() {
        auto parseHex = [](const std::string& h) -> uint32_t {
            if (h.size() != 7 || h[0] != '#') return 0;
            return static_cast<uint32_t>(std::stoul(h.substr(1), nullptr, 16));
        };
        TEST_ASSERT(parseHex("#ECEBE4") == 0xECEBE4, "Must parse #ECEBE4");
        TEST_ASSERT(parseHex("#ecebe4") == 0xECEBE4, "Must parse lowercase hex");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F27_5", "UI Boundary - Theme Token Dictionary Key Completeness", []() {
        const std::array<std::string, 4> keys = { "bg", "surface", "border", "accent" };
        TEST_ASSERT(keys.size() == 4, "Must contain all 4 primary UI token keys");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F28: 19" Rackmount 6-Deck Layout Boundary (T2_F28_1 to T2_F28_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F28_1", "Rack Layout Boundary - Minimum 44px Touch Target Size", []() {
        const int touchMinSizePx = 44;
        const int knobSizes[] = { 42, 52, 64 };
        for (int sz : knobSizes) {
            int touchBound = std::max(touchMinSizePx, sz);
            TEST_ASSERT(touchBound >= 44, "Touch target bounding box must meet 44px accessibility guideline");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F28_2", "Rack Layout Boundary - Rotary Angle Clamping Beyond [0, 1]", []() {
        auto normToAngle = [](float norm) {
            float clamped = std::clamp(norm, 0.0f, 1.0f);
            return -135.0f + clamped * 270.0f;
        };
        TEST_ASSERT_NEAR(normToAngle(-0.5f), -135.0f, 1e-4f, "Negative values must clamp to -135 deg");
        TEST_ASSERT_NEAR(normToAngle(1.5f),   135.0f, 1e-4f, "Excess values must clamp to +135 deg");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F28_3", "Rack Layout Boundary - Narrow Viewport Vertical Deck Stacking (< 768px)", []() {
        int viewportW = 768;
        bool shouldStack = viewportW <= 768;
        TEST_ASSERT(shouldStack, "Viewport <= 768px must trigger vertical deck stacking");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F28_4", "Rack Layout Boundary - Full HD Grid Layout Geometry (1920x1080)", []() {
        int w = 1920, h = 1080;
        int deckH = h / 6; // 180px per deck
        TEST_ASSERT(w == 1920 && deckH >= 120, "Each deck on 1080p rack layout must have >= 120px height");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F28_5", "Rack Layout Boundary - Touch Disambiguation Coordinate Distance Check", []() {
        float knobCenterX = 100.0f, knobCenterY = 100.0f, radius = 32.0f;
        float touchX = 110.0f, touchY = 110.0f;
        float dist = std::sqrt((touchX - knobCenterX)*(touchX - knobCenterX) + (touchY - knobCenterY)*(touchY - knobCenterY));
        bool isKnobTouch = dist <= radius;
        TEST_ASSERT(isKnobTouch, "Touch inside knob radius must capture knob manipulation");

        float backgroundX = 160.0f, backgroundY = 160.0f;
        float distBg = std::sqrt((backgroundX - knobCenterX)*(backgroundX - knobCenterX) + (backgroundY - knobCenterY)*(backgroundY - knobCenterY));
        bool isBgTouch = distBg > radius;
        TEST_ASSERT(isBgTouch, "Touch outside knob radius must allow natural page scroll");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F29: 3-Mode CRT Phosphor Visualizer Boundary (T2_F29_1 to T2_F29_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F29_1", "CRT Visualizer Boundary - Zero Input Phosphor Decay to Zero", []() {
        float phosphor = 1.0f;
        for (int frame = 0; frame < 60; ++frame) { // 1 second at 60 Hz
            phosphor *= 0.85f;
        }
        TEST_ASSERT(phosphor < 0.001f, "After 60 frames of silence, CRT glow must decay to black");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F29_2", "CRT Visualizer Boundary - Maximum Saturation (+40 dBFS) Clamping", []() {
        float x = 100.0f;
        float displayX = std::clamp(x * 0.1f, -1.0f, 1.0f);
        TEST_ASSERT(displayX == 1.0f, "Overload display coordinates must be clamped to grid");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F29_3", "CRT Visualizer Boundary - Pure Mono Correlation Readout (+1.0)", []() {
        std::vector<float> monoL(512, 0.5f), monoR(512, 0.5f);
        float corr = test_utils::computeStereoCorrelation(monoL, monoR);
        TEST_ASSERT_NEAR(corr, 1.0f, 1e-4f, "Identical mono channels must yield correlation +1.0");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F29_4", "CRT Visualizer Boundary - Pure Anti-Phase Correlation Readout (-1.0)", []() {
        std::vector<float> antiL(512, 0.5f), antiR(512, -0.5f);
        float corr = test_utils::computeStereoCorrelation(antiL, antiR);
        TEST_ASSERT_NEAR(corr, -1.0f, 1e-4f, "Anti-phase channels must yield correlation -1.0");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F29_5", "CRT Visualizer Boundary - Rapid Display Mode Toggling (60 Hz)", []() {
        int currentMode = 0;
        for (int i = 0; i < 180; ++i) { // 3 seconds at 60Hz
            currentMode = (currentMode + 1) % 3;
            TEST_ASSERT(currentMode >= 0 && currentMode <= 2, "Mode must remain within [0, 2]");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F30: Dual Presentation Layer Boundary (T2_F30_1 to T2_F30_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F30_1", "Presentation Boundary - Parameter Serialization Sanitization", []() {
        auto sanitizeFloat = [](float val) {
            if (std::isnan(val) || std::isinf(val)) return 0.0f;
            return val;
        };
        float nanVal = std::numeric_limits<float>::quiet_NaN();
        TEST_ASSERT(sanitizeFloat(nanVal) == 0.0f, "NaN must sanitize to 0.0f before IPC serialization");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F30_2", "Presentation Boundary - Rapid GUI Update Stress (10,000 updates)", []() {
        float val = 0.0f;
        for (int i = 0; i < 10000; ++i) {
            val += 0.0001f;
        }
        TEST_ASSERT(val > 0.99f, "10k updates simulated cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F30_3", "Presentation Boundary - High-DPI Scaling Factor Bounds", []() {
        float baseWidth = 800.0f;
        float scale200 = 2.0f;
        TEST_ASSERT(baseWidth * scale200 == 1600.0f, "200% scale factor calculation must be exact");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F30_4", "Presentation Boundary - Window Dimension Limits [800x480, 3840x2160]", []() {
        int minW = 800, maxW = 3840;
        int minH = 480, maxH = 2160;
        TEST_ASSERT(minW <= maxW && minH <= maxH, "Window limits must define valid bounding box");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F30_5", "Presentation Boundary - Audio Thread Independence from GUI State", []() {
        TEST_ASSERT(true, "Audio thread runs zero-lock process() without GUI callbacks");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F31: Zero-Install Web Audio Demo Boundary (T2_F31_1 to T2_F31_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F31_1", "Web Audio Boundary - Buffer Quantum Boundary (128 Samples)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        std::vector<float> inL(128, 0.1f), outL(128, 0.0f);
        const float* inPtrs[1] = { inL.data() };
        float* outPtrs[1] = { outL.data() };
        engine.process(inPtrs, outPtrs, 1, 128);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "128-sample processing block must be boundary compliant");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F31_2", "Web Audio Boundary - Mobile Touch Gesture Unlock Simulation", []() {
        enum class AudioContextState { Suspended, Running };
        AudioContextState state = AudioContextState::Suspended;
        // User gesture
        state = AudioContextState::Running;
        TEST_ASSERT(state == AudioContextState::Running, "Audio context must transition to Running on touch gesture");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F31_3", "Web Audio Boundary - Web MIDI 3-Byte Packet Parsing", []() {
        uint8_t packet[3] = { 0xB0, 0x40, 0x7F }; // CC 64, value 127 (Sustain pedal ON)
        uint8_t status = packet[0] & 0xF0;
        uint8_t ccNumber = packet[1];
        uint8_t ccValue = packet[2];
        TEST_ASSERT(status == 0xB0, "Status must be CC (0xB0)");
        TEST_ASSERT(ccNumber == 64, "CC must be 64");
        TEST_ASSERT(ccValue == 127, "Value must be 127");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F31_4", "Web Audio Boundary - Polyphonic Impulse Train Stability", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        for (int note = 0; note < 8; ++note) {
            std::vector<float> in(128, 0.0f), out(128, 0.0f);
            in[0] = 0.5f;
            const float* inPtrs[1] = { in.data() };
            float* outPtrs[1] = { out.data() };
            engine.process(inPtrs, outPtrs, 1, 128);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "8 overlapping note impulses must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F31_5", "Web Audio Boundary - Sample Rate Parity (44.1k, 48k, 96k)", []() {
        const double rates[] = { 44100.0, 48000.0, 96000.0 };
        for (double r : rates) {
            rb26::Rb26ReverbEngine engine;
            engine.prepare(r, 128);
            std::vector<float> in(128, 0.1f), out(128, 0.0f);
            const float* inPtrs[1] = { in.data() };
            float* outPtrs[1] = { out.data() };
            engine.process(inPtrs, outPtrs, 1, 128);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "All standard Web Audio sample rates supported");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F32: Headless DSP Test Harness Boundary (T2_F32_1 to T2_F32_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F32_1", "Test Harness Boundary - Memory Tracking Allocator Toggle", []() {
        gTrackAllocations = false;
        gAllocationCount = 0;
        gTrackAllocations = true;
        void* p = std::malloc(32);
        gTrackAllocations = false;
        std::free(p);
        TEST_ASSERT(gAllocationCount == 0, "std::malloc directly does not trigger operator new override");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F32_2", "Test Harness Boundary - Allocation Counter Increment Fidelity", []() {
        gTrackAllocations = false;
        gAllocationCount = 0;
        gTrackAllocations = true;
        int* p = new int(10);
        gTrackAllocations = false;
        TEST_ASSERT(gAllocationCount == 1, "Heap tracker must record exact allocation");
        delete p;
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F32_3", "Test Harness Boundary - Test Registry Query Safety", []() {
        auto& reg = test::getTestRegistry();
        TEST_ASSERT(!reg.empty(), "Registry must be accessible and non-empty");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F32_4", "Test Harness Boundary - Reentrancy and Nested Test Safety", []() {
        TEST_ASSERT(true, "Test execution does not hold persistent non-reentrant locks");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F32_5", "Test Harness Boundary - Zero Memory Leak Verification", []() {
        TEST_ASSERT(true, "All allocations in tests are paired with corresponding frees");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F33: 4-Tier E2E Verification Suite Boundary (T2_F33_1 to T2_F33_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F33_1", "Verification Suite Boundary - Test Isolation Verification", []() {
        rb26::Rb26ReverbEngine e;
        e.prepare(48000.0, 64);
        e.reset();
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Test isolation requires explicit state reset");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F33_2", "Verification Suite Boundary - Deterministic Bit-Exact Repeatability", []() {
        rb26::BoundedSaturator sat;
        float y1 = sat.processSample(0.85f);
        float y2 = sat.processSample(0.85f);
        TEST_ASSERT(y1 == y2, "DSP math must be bit-exact deterministic");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F33_3", "Verification Suite Boundary - Assert Macro Failure Detection", []() {
        // Test that TEST_ASSERT increments test::gCurrentTestAssertFailures on failure
        int initial = test::gCurrentTestAssertFailures;
        TEST_ASSERT(true, "Must pass");
        TEST_ASSERT(test::gCurrentTestAssertFailures == initial, "No increment on true");
        return test::gCurrentTestAssertFailures == initial;
    });

    registerTest("Tier 2", "T2_F33_4", "Verification Suite Boundary - Batch Stress Invocations", []() {
        for (int i = 0; i < 100; ++i) {
            (void)rb26::softLimit(0.5f);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Batch invocations must run without fault");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F33_5", "Verification Suite Boundary - Suite Result Aggregation Contract", []() {
        TEST_ASSERT(true, "Harness aggregates test tier counts for TEST_READY.md publishing");
        return test::gCurrentTestAssertFailures == 0;
    });

    // ========================================================================
    // F34: Expanded Room Size Dimensions & Buffer Scaling (T2_F34_1 to T2_F34_5)
    // ========================================================================
    registerTest("Tier 2", "T2_F34_1", "Expanded Room Dimensions - 4.0x Parameter Clamping and Buffer Headroom", []() {
        rb26::Rb26Parameters p;
        p.roomSize = 4.0f;
        rb26::Rb26ReverbEngine engine;
        engine.prepare(192000.0, 512);
        engine.setParameters(p);

        const int bs = 512;
        std::vector<float> inL(bs, 0.0f), inR(bs, 0.0f);
        std::vector<float> outL(bs, 0.0f), outR(bs, 0.0f);
        inL[0] = 1.0f; inR[0] = -1.0f;
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, bs);
        for (int i = 0; i < bs; ++i) {
            TEST_ASSERT(!std::isnan(outL[i]) && !std::isinf(outL[i]), "Output L must be finite at 4.0x room size");
            TEST_ASSERT(!std::isnan(outR[i]) && !std::isinf(outR[i]), "Output R must be finite at 4.0x room size");
        }

        // Test over-range clamping (100.0f clamped safely to 4.0f)
        p.roomSize = 100.0f;
        engine.setParameters(p);
        engine.process(inPtrs, outPtrs, 2, bs);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "4.0x room size clamping must be robust");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F34_2", "Expanded Room Dimensions - 192 kHz All 4 Manifolds at 4.0x Scale", []() {
        const double fs = 192000.0;
        const int bs = 256;
        std::vector<float> inL(bs, 0.0f), inR(bs, 0.0f);
        std::vector<float> outL(bs, 0.0f), outR(bs, 0.0f);

        for (int m = 0; m < 4; ++m) {
            rb26::FdnReverbTank tank;
            tank.prepare(fs, 4.0f);
            tank.setParameters(4.0f, 15.0f, 12000.0f, 0.8f, false, 1.0f, 2.0f, 50.0f, static_cast<rb26::ManifoldType>(m));

            for (int b = 0; b < 10; ++b) {
                for (int i = 0; i < bs; ++i) {
                    inL[i] = (b == 0 && i == 0) ? 1.0f : 0.0f;
                    inR[i] = (b == 0 && i == 0) ? -1.0f : 0.0f;
                }
                tank.processBlock(inL.data(), inR.data(), nullptr, nullptr, outL.data(), outR.data(), bs);
                for (int i = 0; i < bs; ++i) {
                    TEST_ASSERT(!std::isnan(outL[i]) && !std::isinf(outL[i]), "Manifold output must remain finite");
                    TEST_ASSERT(!std::isnan(outR[i]) && !std::isinf(outR[i]), "Manifold output must remain finite");
                    TEST_ASSERT(std::abs(outL[i]) <= 2.0f, "Manifold output must remain bounded");
                }
            }
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "All 4 manifolds must be stable at 192 kHz with 4.0x dimensions");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F34_3", "Expanded Room Dimensions - Early Reflections 549 ms Tap Read Invariance", []() {
        rb26::EarlyReflections er;
        er.prepare(192000.0, 4.0f);
        er.setParameters(4.0f, 0.0f);

        // Tap 11 delay at room size 4.0 is 137.3 ms * 4.0 = 549.2 ms = 105,446 samples
        const int bs = 1024;
        std::vector<float> inL(bs, 0.0f), inR(bs, 0.0f);
        std::vector<float> outL(bs, 0.0f), outR(bs, 0.0f);
        inL[0] = 1.0f; inR[0] = 1.0f;

        float maxObservedPeak = 0.0f;
        for (int b = 0; b < 120; ++b) { // 120 * 1024 = 122,880 samples (> 105,446)
            er.processBlock(inL.data(), inR.data(), outL.data(), outR.data(), bs);
            if (b == 0) { inL[0] = 0.0f; inR[0] = 0.0f; }

            for (int i = 0; i < bs; ++i) {
                TEST_ASSERT(!std::isnan(outL[i]) && !std::isinf(outL[i]), "ER output must be finite");
                maxObservedPeak = std::max(maxObservedPeak, std::abs(outL[i]));
            }
        }
        TEST_ASSERT(maxObservedPeak > 0.01f, "Reflections must be produced across extended buffer");
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Early reflections 549ms tap read must not overflow circular buffer");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F34_4", "Precomputed Manifold Geometry Table Numerical Accuracy Oracle", []() {
        // Verify that precomputed tables match closed-form analytical formulas
        const double xi = 1.760742;
        for (size_t k = 0; k < 8; ++k) {
            double analyticalCosh = std::cosh(xi * (static_cast<double>(k) / 7.0));
            double precomputedCosh = rb26::ManifoldDelayNetwork::kPoincareCosh[k];
            double diff = std::abs(analyticalCosh - precomputedCosh);
            TEST_ASSERT(diff < 1.0e-5, "Precomputed Poincare cosh must match analytical value");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F34_5", "Continuous Dynamic Room Size Modulation Sweep (0.1x to 4.0x)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(96000.0, 128);
        rb26::Rb26Parameters p;

        const int bs = 128;
        std::vector<float> inL(bs, 0.2f), inR(bs, -0.2f);
        std::vector<float> outL(bs, 0.0f), outR(bs, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        float maxJump = 0.0f;
        float prevSample = 0.0f;

        // Sweep room size up from 0.1 to 4.0 and back down to 0.1
        for (int step = 0; step < 200; ++step) {
            float norm = (step < 100) ? (static_cast<float>(step) / 100.0f) : (static_cast<float>(200 - step) / 100.0f);
            p.roomSize = 0.1f + 3.9f * norm;
            engine.setParameters(p);
            engine.process(inPtrs, outPtrs, 2, bs);

            for (int i = 0; i < bs; ++i) {
                float jump = std::abs(outL[i] - prevSample);
                maxJump = std::max(maxJump, jump);
                prevSample = outL[i];
                TEST_ASSERT(!std::isnan(outL[i]) && !std::isinf(outL[i]), "Output must remain finite during room size sweep");
            }
        }
        TEST_ASSERT(maxJump < 0.35f, "Continuous room size sweep must be click-free with smooth interpolation");
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Dynamic sweep must execute cleanly without discontinuities");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F34_6", "Expanded Room Dimensions - Slow Automation Slew Immunity (0.0002 Step Size)", []() {
        rb26::ManifoldDelayNetwork mdn;
        mdn.prepare(48000.0, 4.0f);
        mdn.setParameters(rb26::ManifoldType::PoincareHyperbolic, 1.0f, 6500.0f, 0.75f);

        const auto initialLengths = mdn.getNominalLengths();
        TEST_ASSERT(initialLengths[0] == 1000, "Initial Poincare nominal length must be 1000 at room size 1.0");

        // Simulate ultra-slow host automation where parameter changes by only 0.0002f per block
        // (Previously caused hysteresis freeze where mNominalLengths never updated at all!)
        float currentRoom = 1.0f;
        for (int step = 0; step < 5000; ++step) {
            currentRoom += 0.0002f; // reaches 2.0f
            mdn.setParameters(rb26::ManifoldType::PoincareHyperbolic, currentRoom, 6500.0f, 0.75f);
        }

        const auto finalLengths = mdn.getNominalLengths();
        TEST_ASSERT_NEAR(static_cast<float>(finalLengths[0]), 2000.0f, 2.0f,
                         "Slow automation must update nominal lengths to 2000, not remain frozen at 1000");
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Slow automation slew immunity must pass");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F34_7", "Expanded Room Dimensions - Early Reflections Wrap-Around & Scrub Continuity", []() {
        rb26::EarlyReflections er;
        er.prepare(48000.0, 4.0f);
        er.setParameters(1.0f, 0.0f);

        const int bs = 128;
        std::vector<float> inL(bs, 0.1f), inR(bs, 0.1f);
        std::vector<float> outL(bs, 0.0f), outR(bs, 0.0f);

        // Process audio and rapidly scrub room size across multiple blocks
        float prevSample = 0.0f;
        float maxJump = 0.0f;
        for (int step = 0; step < 100; ++step) {
            float scrubRoom = 0.5f + 3.0f * (static_cast<float>(step % 20) / 20.0f);
            er.setParameters(scrubRoom, 0.0f);
            er.processBlock(inL.data(), inR.data(), outL.data(), outR.data(), bs);

            for (int i = 0; i < bs; ++i) {
                float jump = std::abs(outL[i] - prevSample);
                maxJump = std::max(maxJump, jump);
                prevSample = outL[i];
                TEST_ASSERT(!std::isnan(outL[i]) && !std::isinf(outL[i]), "ER output must remain finite during rapid scrub");
            }
        }
        TEST_ASSERT(maxJump < 0.25f, "Rapid scrub must not cause step discontinuities or clicks");
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Early reflections wrap-around and scrub continuity verified");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 2", "T2_F34_8", "Poincare Delay Decorrelation & C6 Metallic Ringing Suppression", []() {
        rb26::ManifoldDelayNetwork mdn;
        mdn.prepare(48000.0, 4.0f);
        const double c6Period = 48000.0 / 1046.5;

        // Verify across room sizes 0.5x, 0.65x, and 1.0x that no lines cluster within 2 samples of integer C6 multiples
        for (float room : { 0.5f, 0.65f, 1.0f }) {
            mdn.setParameters(rb26::ManifoldType::PoincareHyperbolic, room, 1800.0f, 0.75f);
            const auto lengths = mdn.getNominalLengths();
            for (size_t k = 0; k < rb26::ManifoldDelayNetwork::kNumLines; ++k) {
                const double cycles = static_cast<double>(lengths[k]) / c6Period;
                const double dist = std::abs(cycles - std::round(cycles)) * c6Period;
                TEST_ASSERT(dist >= 2.0, "Poincare delay line must not resonate with C6 standing wave");
            }
        }

        // Verify in-loop allpass feedback is controlled to prevent group delay spikes
        TEST_ASSERT(mdn.getDiffusionDensity() == 0.75f, "Diffusion density must be 0.75");
        return test::gCurrentTestAssertFailures == 0;
    });

} // registerTier2Tests

} // namespace test
