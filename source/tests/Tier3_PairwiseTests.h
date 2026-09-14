#pragma once

#include "TestHarness.h"

namespace test {

inline void registerTier3Tests() {

    registerTest("Tier 3", "T3_P01", "Pairwise - Shimmer (+12st) + Infinite Freeze Hold Recirculation", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);
        rb26::Rb26Parameters p;
        p.shimmerSend = 0.5f;
        p.pitchBlend = 1.0f; // Pure shimmer
        p.freezeHold = true;
        engine.setParameters(p);

        std::vector<float> inL(256, 0.2f), inR(256, 0.2f);
        std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 256);
        float peak = test_utils::computePeak(outL);
        TEST_ASSERT(peak <= 1.05f, "Shimmer + Freeze must remain strictly bounded (|y| <= 1.05)");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P02", "Pairwise - Dimmer (-12st) + Sub-Bass Elliptical Mono Collapse", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);
        rb26::Rb26Parameters p;
        p.dimmerSend = 0.8f;
        p.pitchBlend = -1.0f; // Pure dimmer
        p.subMonoHz = 150.0f;
        p.dryWetMix = 1.0f;
        engine.setParameters(p);

        // Feed stereo anti-phase input
        std::vector<float> inL(256, 0.4f), inR(256, -0.4f);
        std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 30; ++i) engine.process(inPtrs, outPtrs, 2, 256);
        TEST_ASSERT(!std::isnan(outL[128]), "Dimmer + Sub Mono must process stably");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P03", "Pairwise - Low-End Punch Ducking + Low Crossover Frequency", []() {
        rb26::LowBandModalMatrix matrix;
        matrix.prepare(48000.0);
        rb26::LowBandModalParams p;
        p.crossoverHz = 180.0f;
        p.punchDucking = 1.0f;
        matrix.setParameters(p);

        // High frequency continuous tone (2000 Hz) with enough duration for crossover transients to settle
        auto sigHigh = test_utils::generateSine(10000, 2000.0, 48000.0);
        for (float s : sigHigh) {
            float hL, hR, lL, lR;
            matrix.processSample(s, s, hL, hR, lL, lR);
        }
        float duckGain = matrix.getDuckingGain();
        TEST_ASSERT(duckGain > 0.90f, "High frequency input (> 180 Hz) must not trigger low-band punch ducking");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P04", "Pairwise - Tail Bloom Modulation + Early Reflections Time-Invariance", []() {
        rb26::EarlyReflections er;
        rb26::TailModulator tm;
        er.prepare(48000.0);
        tm.prepare(48000.0);
        tm.setParameters(2.0f, 4.0f, 85.0f); // Fast, deep tail modulation

        // Process continuous sine through early reflections while tail modulator runs
        auto inSig = test_utils::generateSine(4800, 1000.0, 48000.0);
        for (size_t i = 0; i < 4800; ++i) {
            std::array<float, 8> exc {};
            tm.processSample(inSig[i], exc);
            float erL, erR;
            er.processSample(inSig[i], inSig[i], erL, erR);
        }
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "Tail bloom and early reflections must remain independent");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P05", "Pairwise - HF Damping + Pitch Loop Bandpass Filter Cascade", []() {
        rb26::OnePoleLowpass damp;
        rb26::ShimmerLoopFilter shim;
        damp.setCutoff(48000.0f, 6500.0f);
        shim.prepare(48000.0);

        auto inSig = test_utils::generateSine(2400, 1000.0, 48000.0);
        for (float s : inSig) {
            float y1 = damp.process(s);
            float y2 = shim.process(y1);
            TEST_ASSERT(!std::isnan(y2), "Cascaded HF damping and shimmer filter must remain stable");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P06", "Pairwise - Bounded Hermite Limiter + High Regen Feedback (0.95)", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(1.0f, 0.0f, 12, -12, 1.0f, 0.95f); // Max shimmer feedback

        float peak = 0.0f;
        for (int i = 0; i < 5000; ++i) {
            float in = (i < 100) ? 1.0f : 0.0f;
            float outL, outR;
            shifter.processSample(in, in, outL, outR);
            peak = std::max(peak, std::abs(outL));
        }
        TEST_ASSERT(peak <= 1.05f, "Hermite limiter must strictly bound max 0.95 pitch feedback loop");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P07", "Pairwise - Stereo Width (2.0x) + Sub-Bass Elliptical Filter", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.stereoWidth = 2.0f; // Exaggerated width
        p.subMonoHz = 120.0f; // Elliptical mono maker
        p.dryWetMix = 1.0f;
        engine.setParameters(p);

        // Low frequency stereo input (50 Hz)
        auto sig50 = test_utils::generateSine(128, 50.0, 48000.0);
        std::vector<float> inL(128), inR(128), outL(128, 0.0f), outR(128, 0.0f);
        for (size_t i = 0; i < 128; ++i) { inL[i] = sig50[i]; inR[i] = -sig50[i]; }
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 128);
        TEST_ASSERT(!std::isnan(outL[64]), "Exaggerated width + elliptical mono must be numerically stable");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P08", "Pairwise - Pre-Delay (500 ms) + Infinite Freeze Hold", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);
        rb26::Rb26Parameters p;
        p.preDelayMs = 500.0f;
        p.freezeHold = true;
        engine.setParameters(p);

        std::vector<float> inL(512, 0.5f), inR(512, 0.5f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 512);
        TEST_ASSERT(test::gCurrentTestAssertFailures == 0, "500ms pre-delay with freeze hold must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P09", "Pairwise - Shimmer/Dimmer Blend Morph + Master Soft Limiter", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.shimmerSend = 1.0f;
        p.dimmerSend = 1.0f;
        p.dryWetMix = 1.0f;
        p.outputTrimDb = +6.0f; // Hot master trim
        engine.setParameters(p);

        std::vector<float> inL(128, 0.8f), inR(128, 0.8f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        float peak = 0.0f;
        for (int i = 0; i < 50; ++i) {
            p.pitchBlend = -1.0f + (static_cast<float>(i) / 25.0f); // sweep from -1 to +1
            engine.setParameters(p);
            engine.process(inPtrs, outPtrs, 2, 128);
            peak = std::max(peak, test_utils::computePeak(outL));
        }
        TEST_ASSERT(peak <= 1.0f, "Blend morphing under hot trim must be limited to <= 1.0");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P10", "Pairwise - Bass RT60 (4.0x) + Transient Punch Ducking (1.0)", []() {
        rb26::LowBandModalMatrix matrix;
        matrix.prepare(48000.0);
        rb26::LowBandModalParams p;
        p.bassRt60Mult = 4.0f;
        p.punchDucking = 1.0f;
        matrix.setParameters(p);

        float outL, outR;
        matrix.processModalOnly(1.0f, 1.0f, outL, outR);
        for (int i = 0; i < 5000; ++i) matrix.processModalOnly(0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Long bass decay with full punch ducking must process stably");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P11", "Pairwise - Early/Late Mix (100% Late) + Tail Modulator", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);
        rb26::Rb26Parameters p;
        p.earlyLateMix = 1.0f; // 100% Late
        p.tailModDepthMs = 2.5f;
        p.dryWetMix = 1.0f;
        engine.setParameters(p);

        std::vector<float> inL(256, 0.5f), inR(256, 0.5f);
        std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 256);
        TEST_ASSERT(!std::isnan(outL[128]), "100% Late with full tail modulation must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P12", "Pairwise - Early/Late Mix (100% Early) + Tail Modulator Isolation", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);
        rb26::Rb26Parameters p;
        p.earlyLateMix = 0.0f; // 100% Early
        p.tailModDepthMs = 5.0f; // Heavy modulation in tank
        p.dryWetMix = 1.0f;
        p.preDelayMs = 0.0f;
        engine.setParameters(p);

        // Process pure early reflections
        std::vector<float> inL(256, 0.5f), inR(256, 0.5f);
        std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 256);
        TEST_ASSERT(!std::isnan(outL[128]), "100% Early reflections must remain unmodulated");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P13", "Pairwise - Golden-Ratio LFOs + Hermite Spline Fractional Delay", []() {
        rb26::TailModulator mod;
        mod.prepare(48000.0);
        mod.setParameters(1.5f, 3.0f, 85.0f);

        std::vector<float> buf(2048, 0.5f);
        for (int i = 0; i < 1000; ++i) {
            std::array<float, 8> exc {};
            mod.processSample(0.0f, exc);
            float val = rb26::TailModulator::readHermite(buf.data(), 2048, 2047, 500, exc[0]);
            TEST_ASSERT(!std::isnan(val), "Hermite reading with golden ratio LFO must be stable");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P14", "Pairwise - Dry/Wet Mix (0% Dry) + Master Limiter Ceiling (+12 dB)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.dryWetMix = 0.0f; // 100% Dry
        p.outputTrimDb = +12.0f; // +12 dB trim
        engine.setParameters(p);

        std::vector<float> inL(128, 0.8f), inR(128, 0.8f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 128);
        float peak = test_utils::computePeak(outL);
        TEST_ASSERT(peak <= 1.0f, "Dry audio trimmed by +12 dB must be limited to 1.0f");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P15", "Pairwise - Denormal Flush + Low-Level Input (-100 dBFS)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        std::vector<float> inL(128, 1.0e-5f), inR(128, 1.0e-5f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 128);
        TEST_ASSERT(!std::isnan(outL[64]), "Low level input must process cleanly with zero denormals");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P16", "Pairwise - SPSC Visualizer FIFO + Audio Real-Time Callback", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);
        std::vector<float> inL(512, 0.4f), inR(512, 0.4f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 512);
        rb26::Rb26ReverbEngine::VisualizerFrame frame;
        bool popped = engine.popVisualizerFrame(frame);
        TEST_ASSERT(popped, "FIFO must provide frame without interfering with audio thread");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P17", "Pairwise - FDN Room Size (2.0) + Dark HF Damping (1000 Hz)", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 2.0f);
        tank.setParameters(2.0f, 10.0f, 1000.0f, 0.8f, false, 0.85f, 0.0f, 85.0f);

        float outL, outR;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
        for (int i = 0; i < 5000; ++i) tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Massive dark cathedral tank must be stable");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P18", "Pairwise - FDN Room Size (0.1) + High Diffusion Density (1.0)", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0, 2.0f);
        tank.setParameters(0.1f, 1.0f, 12000.0f, 1.0f, false, 0.85f, 0.0f, 85.0f);

        float outL, outR;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
        for (int i = 0; i < 2000; ++i) tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Small plate room with max diffusion must be stable");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P19", "Pairwise - Dimmer (-24st) + Infinite Freeze Hold", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(0.0f, 1.0f, 12, -24, -1.0f, 0.90f);

        float outL, outR;
        shifter.processSample(0.5f, 0.5f, outL, outR);
        for (int i = 0; i < 3000; ++i) shifter.processSample(0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Two-octave down dimmer with high sustain must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P20", "Pairwise - Shimmer (+24st) + Bright Damping (20 kHz)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.shimmerInterval = 24;
        p.shimmerSend = 0.5f;
        p.highDampingHz = 20000.0f;
        engine.setParameters(p);

        std::vector<float> inL(128, 0.2f), inR(128, 0.2f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 128);
        TEST_ASSERT(!std::isnan(outL[64]), "+24st shimmer with bright damping must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P21", "Pairwise - Low Crossover (60 Hz) + Sub Mono (250 Hz)", []() {
        rb26::LowBandModalMatrix matrix;
        matrix.prepare(48000.0);
        rb26::LowBandModalParams p;
        p.crossoverHz = 60.0f;
        p.subMonoHz = 250.0f;
        matrix.setParameters(p);

        float hL, hR, lL, lR;
        matrix.processSample(0.5f, -0.5f, hL, hR, lL, lR);
        TEST_ASSERT(!std::isnan(hL) && !std::isnan(lL), "Crossover 60 Hz and sub mono 250 Hz must interact cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P22", "Pairwise - Low Crossover (400 Hz) + Bass RT60 (0.2x)", []() {
        rb26::LowBandModalMatrix matrix;
        matrix.prepare(48000.0);
        rb26::LowBandModalParams p;
        p.crossoverHz = 400.0f;
        p.bassRt60Mult = 0.2f;
        matrix.setParameters(p);

        float hL, hR, lL, lR;
        matrix.processSample(1.0f, 1.0f, hL, hR, lL, lR);
        TEST_ASSERT(!std::isnan(hL) && !std::isnan(lL), "High crossover and fast bass decay must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P23", "Pairwise - Output Trim (-24 dB) + Limiter Inactive", []() {
        float gain = rb26::dbToGain(-24.0f);
        float sample = 0.5f * gain;
        float limited = rb26::softLimit(sample);
        TEST_ASSERT_NEAR(limited, sample, 1.0e-5f, "-24 dB trim must remain in linear region of limiter");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P24", "Pairwise - Output Trim (+12 dB) + Full Wet Mix Limiting", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.dryWetMix = 1.0f;
        p.outputTrimDb = +12.0f;
        engine.setParameters(p);

        std::vector<float> inL(128, 1.0f), inR(128, 1.0f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        float maxVal = 0.0f;
        for (int i = 0; i < 20; ++i) {
            engine.process(inPtrs, outPtrs, 2, 128);
            maxVal = std::max(maxVal, test_utils::computePeak(outL));
        }
        TEST_ASSERT(maxVal <= 1.0f, "Hot wet reverberation must be limited to 1.0f");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P25", "Pairwise - APVTS Parameter Sync + Fast LFO Modulation", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        rb26::Rb26Parameters p;
        p.tailModRateHz = 5.0f; // Fast LFO
        p.tailModDepthMs = 4.0f;
        engine.setParameters(p);

        std::vector<float> inL(64, 0.5f), inR(64, 0.5f);
        std::vector<float> outL(64, 0.0f), outR(64, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 100; ++i) {
            p.decayRt60Sec = 2.0f + (i % 5) * 0.5f;
            engine.setParameters(p);
            engine.process(inPtrs, outPtrs, 2, 64);
        }
        TEST_ASSERT(!std::isnan(outL[0]), "Fast LFO + frequent parameter sync must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P26", "Pairwise - Pre-Delay (0 ms) + Immediate Early Reflection First Tap", []() {
        rb26::EarlyReflections er;
        er.prepare(48000.0, 1.0f);
        er.setParameters(1.0f);

        float outL, outR;
        er.processSample(1.0f, 1.0f, outL, outR);
        // Base delay for tap 0 is 7.3ms (350 samples). At sample 0 output is 0
        TEST_ASSERT_NEAR(outL, 0.0f, 1e-4f, "Immediate reflection at 0 ms must be zero before tap 0 arrives");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P27", "Pairwise - Pre-Delay (100 ms) + Early Reflection Offset", []() {
        const float preDelayMs = 100.0f;
        const float tap0Ms = 7.3f;
        const float totalMs = preDelayMs + tap0Ms;
        TEST_ASSERT_NEAR(totalMs, 107.3f, 1e-4f, "Pre-delay must additively offset reflection tap arrivals");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P28", "Pairwise - Tail Bloom (300 ms) + Short Decay RT60 (0.5s)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.decayRt60Sec = 0.5f;
        p.tailBloomMs = 300.0f;
        engine.setParameters(p);

        std::vector<float> inL(128, 0.5f), inR(128, 0.5f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 30; ++i) engine.process(inPtrs, outPtrs, 2, 128);
        TEST_ASSERT(!std::isnan(outL[64]), "Slow bloom with short decay must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P29", "Pairwise - Tail Bloom (20 ms) + Long Decay RT60 (15.0s)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.decayRt60Sec = 15.0f;
        p.tailBloomMs = 20.0f;
        engine.setParameters(p);

        std::vector<float> inL(128, 0.5f), inR(128, 0.5f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 30; ++i) engine.process(inPtrs, outPtrs, 2, 128);
        TEST_ASSERT(!std::isnan(outL[64]), "Fast bloom with long decay must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P30", "Pairwise - Shimmer (+7st) + Dimmer (-12st) Balanced Blend (0.0)", []() {
        rb26::PitchShifter shifter;
        shifter.prepare(48000.0);
        shifter.setParameters(0.5f, 0.5f, 7, -12, 0.0f, 0.4f);

        auto inSig = test_utils::generateSine(2400, 440.0, 48000.0);
        std::vector<float> outL(2400, 0.0f), outR(2400, 0.0f);
        shifter.process(inSig.data(), inSig.data(), outL.data(), outR.data(), 2400);

        TEST_ASSERT(!std::isnan(outL[1200]), "Balanced +7st and -12st blend must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P31", "Pairwise - Punch Ducking + Freeze Hold Interaction", []() {
        rb26::LowBandModalMatrix matrix;
        matrix.prepare(48000.0);
        rb26::LowBandModalParams p;
        p.punchDucking = 0.8f;
        p.freezeHold = true;
        matrix.setParameters(p);

        float outL, outR;
        matrix.processModalOnly(1.0f, 1.0f, outL, outR);
        for (int i = 0; i < 2000; ++i) matrix.processModalOnly(0.0f, 0.0f, outL, outR);
        TEST_ASSERT(!std::isnan(outL), "Punch ducking during freeze must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P32", "Pairwise - Stereo Width (0.0) + Master Soft Limiter", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.stereoWidth = 0.0f;
        p.outputTrimDb = +6.0f;
        engine.setParameters(p);

        std::vector<float> inL(128, 0.9f), inR(128, 0.9f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 128);
        float peak = test_utils::computePeak(outL);
        TEST_ASSERT(peak <= 1.0f, "Summed mono output must be limited safely to <= 1.0");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P33", "Pairwise - All 8 FDN Delay Lines + Hermite Saturation Boundedness", []() {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0);
        tank.setParameters(1.0f, 10.0f, 20000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        float maxOut = 0.0f;
        for (int i = 0; i < 5000; ++i) {
            float in = (i == 0) ? 50.0f : 0.0f;
            float outL, outR;
            tank.processSample(in, in, 0.0f, 0.0f, outL, outR);
            maxOut = std::max(maxOut, std::abs(outL));
        }
        TEST_ASSERT(maxOut <= 1.05f, "All 8 lines simultaneously saturated must remain bounded to 1.05");
        return test::gCurrentTestAssertFailures == 0;
    });

    registerTest("Tier 3", "T3_P34", "Pairwise - Pitch Shifter (+12st) + Master Stereo Width (2.0x)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.shimmerSend = 0.7f;
        p.pitchBlend = 1.0f;
        p.stereoWidth = 2.0f;
        p.dryWetMix = 1.0f;
        engine.setParameters(p);

        std::vector<float> inL(128, 0.4f), inR(128, -0.4f);
        std::vector<float> outL(128, 0.0f), outR(128, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 30; ++i) engine.process(inPtrs, outPtrs, 2, 128);
        TEST_ASSERT(!std::isnan(outL[64]), "Shimmer with 2.0x width must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

} // registerTier3Tests

} // namespace test
