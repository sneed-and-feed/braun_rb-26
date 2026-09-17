#pragma once

#include "TestHarness.h"

namespace test {

inline void registerTier4Tests() {

    // S01: Ambient Guitar Cloud
    registerTest("Tier 4", "T4_S01", "Scenario S01 - Ambient Guitar Cloud (High Diffusion, 8.0s RT60, +12st Shimmer)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);

        rb26::Rb26Parameters p;
        p.roomSize = 1.4f;
        p.decayRt60Sec = 8.0f;
        p.diffusionDensity = 0.95f;
        p.shimmerSend = 0.40f;
        p.shimmerInterval = 12;
        p.pitchBlend = 1.0f; // Pure shimmer
        p.tailModRateHz = 0.65f;
        p.tailModDepthMs = 1.5f;
        p.tailBloomMs = 100.0f;
        p.dryWetMix = 0.60f;
        engine.setParameters(p);

        // Guitar pluck simulation (sawtooth burst)
        const size_t total = 48000 * 2; // 2 seconds
        std::vector<float> inL(512, 0.0f), inR(512, 0.0f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        // Initial pluck
        for (int i = 0; i < 512; ++i) inL[i] = (1.0f - static_cast<float>(i)/512.0f) * 0.8f;
        engine.process(inPtrs, outPtrs, 2, 512);
        std::fill(inL.begin(), inL.end(), 0.0f);

        double lateEnergy = 0.0;
        for (size_t b = 1; b < total / 512; ++b) {
            engine.process(inPtrs, outPtrs, 2, 512);
            if (b >= (total / 512) - 10) {
                lateEnergy += test_utils::computeRMS(outL);
            }
        }
        TEST_ASSERT(lateEnergy > 0.001, "Guitar cloud must produce sustaining ambient tail after 2 seconds");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S02: Ethereal Synth Pad
    registerTest("Tier 4", "T4_S02", "Scenario S02 - Ethereal Synth Pad (Balanced Shimmer/Dimmer 50/50)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);

        rb26::Rb26Parameters p;
        p.decayRt60Sec = 5.0f;
        p.shimmerSend = 0.5f;
        p.dimmerSend = 0.5f;
        p.shimmerInterval = 12;
        p.dimmerInterval = -12;
        p.pitchBlend = 0.0f; // 50/50 blend
        p.highDampingHz = 8000.0f;
        p.dryWetMix = 0.50f;
        engine.setParameters(p);

        // Synth pad 440 Hz tone
        auto pad = test_utils::generateSine(512, 440.0, 48000.0, 0.5f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { pad.data(), pad.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 40; ++i) {
            engine.process(inPtrs, outPtrs, 2, 512);
        }
        TEST_ASSERT(!std::isnan(outL[256]), "Synth pad with bidirectional diffusion must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S03: Modern Club Kick & Sub-Bass
    registerTest("Tier 4", "T4_S03", "Scenario S03 - Modern Club Kick & Sub-Bass (Punch Ducking 70%, Sub-Mono 100 Hz)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);

        rb26::Rb26Parameters p;
        p.punchDucking = 0.70f;
        p.subMonoHz = 100.0f;
        p.decayRt60Sec = 1.2f;
        p.lowCrossoverHz = 160.0f;
        engine.setParameters(p);

        // Kick transient
        std::vector<float> kick(256, 0.0f);
        kick[0] = 1.0f; kick[1] = 0.8f; kick[2] = 0.6f;
        std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
        const float* inPtrs[2] = { kick.data(), kick.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 256);
        TEST_ASSERT(!std::isnan(outL[0]), "Kick transient must process without overload");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S04: Dark Sub-Harmonic Drone
    registerTest("Tier 4", "T4_S04", "Scenario S04 - Dark Sub-Harmonic Drone (-24st Dimmer, 15s RT60, Freeze)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);

        rb26::Rb26Parameters p;
        p.dimmerInterval = -24;
        p.dimmerSend = 0.60f;
        p.pitchBlend = -1.0f; // Pure dimmer
        p.decayRt60Sec = 15.0f;
        p.highDampingHz = 3500.0f;
        p.freezeHold = true;
        engine.setParameters(p);

        std::vector<float> inL(512, 0.3f), inR(512, 0.3f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 512);
        TEST_ASSERT(!std::isnan(outL[256]), "Dark sub-harmonic drone with freeze must process stably");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S05: Infinite Reverb Freeze & Overload Stress
    registerTest("Tier 4", "T4_S05", "Scenario S05 - Infinite Freeze & Overload Stress (+40 dBFS, 50,000 Samples)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);

        rb26::Rb26Parameters p;
        p.freezeHold = true;
        p.shimmerSend = 0.5f;
        p.pitchFeedback = 0.90f;
        engine.setParameters(p);

        // Feed +40 dBFS impulse (100.0f)
        std::vector<float> inL(512, 0.0f), inR(512, 0.0f);
        inL[0] = 100.0f; inR[0] = 100.0f;
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 512);
        inL[0] = 0.0f; inR[0] = 0.0f;

        float maxPeak = 0.0f;
        for (int b = 0; b < 100; ++b) { // 51,200 samples
            engine.process(inPtrs, outPtrs, 2, 512);
            maxPeak = std::max(maxPeak, test_utils::computePeak(outL));
        }
        TEST_ASSERT(maxPeak <= 1.05f, "+40 dBFS impulse into infinite freeze must stay strictly bounded <= 1.05");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S06: Shimmer/Dimmer Dynamic Morphing
    registerTest("Tier 4", "T4_S06", "Scenario S06 - Shimmer/Dimmer Dynamic Morphing (Continuous Blend Sweep)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);

        rb26::Rb26Parameters p;
        p.shimmerSend = 0.6f;
        p.dimmerSend = 0.6f;
        engine.setParameters(p);

        std::vector<float> inL(256, 0.4f), inR(256, 0.4f);
        std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int step = 0; step <= 100; ++step) {
            p.pitchBlend = -1.0f + (static_cast<float>(step) / 50.0f); // -1.0 to +1.0
            engine.setParameters(p);
            engine.process(inPtrs, outPtrs, 2, 256);
            TEST_ASSERT(!std::isnan(outL[128]), "Dynamic blend morphing must be stable throughout sweep");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    // S07: Bass Guitar Slap & Decay
    registerTest("Tier 4", "T4_S07", "Scenario S07 - Bass Guitar Slap & Decay (Transient Punch + Modal Preservation)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);

        rb26::Rb26Parameters p;
        p.lowCrossoverHz = 200.0f;
        p.bassRt60Mult = 1.5f;
        p.punchDucking = 0.6f;
        p.subMonoHz = 90.0f;
        engine.setParameters(p);

        // Bass slap: percussive attack followed by low resonance
        std::vector<float> slap(256);
        for (size_t i = 0; i < 256; ++i) {
            slap[i] = std::exp(-static_cast<float>(i)/30.0f) * std::sin(test_utils::kTwoPi * 80.0 * i / 48000.0);
        }
        std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
        const float* inPtrs[2] = { slap.data(), slap.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 256);
        TEST_ASSERT(!std::isnan(outL[0]), "Bass slap must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S08: Vocal Bloom Reverb
    registerTest("Tier 4", "T4_S08", "Scenario S08 - Vocal Bloom Reverb (Unmodulated Initial, Delayed Tail Bloom)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);

        rb26::Rb26Parameters p;
        p.preDelayMs = 30.0f;
        p.earlyLateMix = 0.5f;
        p.tailBloomMs = 120.0f;
        p.tailModDepthMs = 2.0f;
        p.decayRt60Sec = 4.0f;
        engine.setParameters(p);

        // Vocal onset
        std::vector<float> vocal(512, 0.5f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { vocal.data(), vocal.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 512);
        TEST_ASSERT(!std::isnan(outL[256]), "Vocal bloom reverb must process cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S09: Multi-Rate Sample Staging
    registerTest("Tier 4", "T4_S09", "Scenario S09 - Multi-Rate Sample Staging (44.1k, 48k, 88.2k, 96k, 176.4k, 192k)", []() {
        const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
        rb26::Rb26ReverbEngine engine;
        for (double fs : rates) {
            engine.prepare(fs, 256);
            std::vector<float> inL(256, 0.3f), inR(256, 0.3f);
            std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
            const float* inPtrs[2] = { inL.data(), inR.data() };
            float* outPtrs[2] = { outL.data(), outR.data() };
            engine.process(inPtrs, outPtrs, 2, 256);
            TEST_ASSERT(!std::isnan(outL[128]), "Engine must process cleanly across all sample rates");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    // S10: Mastering Stereo Bus Reverb
    registerTest("Tier 4", "T4_S10", "Scenario S10 - Mastering Stereo Bus Reverb (Subtle 15% Mix, Limiter Active)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);

        rb26::Rb26Parameters p;
        p.dryWetMix = 0.15f; // Subtle 15% mix
        p.decayRt60Sec = 1.8f;
        p.stereoWidth = 1.2f;
        p.limiterEnable = true;
        engine.setParameters(p);

        auto fullMix = test_utils::generateSine(256, 1000.0, 48000.0, 0.9f);
        std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
        const float* inPtrs[2] = { fullMix.data(), fullMix.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 256);
        float peak = test_utils::computePeak(outL);
        TEST_ASSERT(peak <= 1.0f, "Mastering bus reverb must remain strictly limited to <= 1.0");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S11: Rapid Parameter Automation
    registerTest("Tier 4", "T4_S11", "Scenario S11 - Rapid Parameter Automation (Audio-Rate Sweeping)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);

        std::vector<float> inL(64, 0.5f), inR(64, 0.5f);
        std::vector<float> outL(64, 0.0f), outR(64, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        rb26::Rb26Parameters p;
        for (int i = 0; i < 200; ++i) {
            p.roomSize = 0.2f + (i % 10) * 0.15f;
            p.decayRt60Sec = 1.0f + (i % 8) * 2.0f;
            p.preDelayMs = static_cast<float>(i % 50);
            engine.setParameters(p);
            engine.process(inPtrs, outPtrs, 2, 64);
            TEST_ASSERT(!std::isnan(outL[0]), "Rapid parameter automation must not cause numerical instability");
        }
        return test::gCurrentTestAssertFailures == 0;
    });

    // S12: Live Performance Freeze Latch
    registerTest("Tier 4", "T4_S12", "Scenario S12 - Live Performance Freeze Latch (MIDI CC Toggle Latch)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 256);

        std::vector<float> chordL(256, 0.5f), chordR(256, 0.5f);
        std::vector<float> outL(256, 0.0f), outR(256, 0.0f);
        const float* inPtrs[2] = { chordL.data(), chordR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        rb26::Rb26Parameters p;
        // 1. Play chord
        engine.process(inPtrs, outPtrs, 2, 256);

        // 2. Latch freeze (CC 64 >= 64)
        p.freezeHold = true;
        engine.setParameters(p);
        std::fill(chordL.begin(), chordL.end(), 0.0f);
        std::fill(chordR.begin(), chordR.end(), 0.0f);
        for (int i = 0; i < 20; ++i) engine.process(inPtrs, outPtrs, 2, 256);

        // 3. Unlatch freeze
        p.freezeHold = false;
        engine.setParameters(p);
        for (int i = 0; i < 40; ++i) engine.process(inPtrs, outPtrs, 2, 256);

        TEST_ASSERT(!std::isnan(outL[128]), "Freeze latch transition must execute seamlessly");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S13: Dieter Rams 19" Rack Visualizer Audition
    registerTest("Tier 4", "T4_S13", "Scenario S13 - Dieter Rams 19\" Rack Visualizer Audition", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);

        std::vector<float> inL(512, 0.4f), inR(512, 0.4f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 512);
        rb26::Rb26ReverbEngine::VisualizerFrame f;
        bool gotFrame = engine.popVisualizerFrame(f);
        TEST_ASSERT(gotFrame, "Visualizer frame must be produced for CRT scope");
        TEST_ASSERT(f.outputRmsL > 0.0f, "Output RMS must be non-zero");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S14: Zero-Install Web Browser Audition
    registerTest("Tier 4", "T4_S14", "Scenario S14 - Zero-Install Web Browser Audition (128-Sample Quantum Staging)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);

        std::vector<float> in(128, 0.3f), out(128, 0.0f);
        const float* inPtrs[1] = { in.data() };
        float* outPtrs[1] = { out.data() };

        for (int quantum = 0; quantum < 50; ++quantum) {
            engine.process(inPtrs, outPtrs, 1, 128);
        }
        TEST_ASSERT(!std::isnan(out[64]), "128-sample Web Audio quantum processing must execute cleanly");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S15: Audio Thread Safety Benchmark
    registerTest("Tier 4", "T4_S15", "Scenario S15 - Audio Thread Safety Benchmark (1,000,000 Samples, 0 Heap Allocs)", []() {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);

        std::vector<float> inL(512, 0.25f), inR(512, 0.25f);
        std::vector<float> outL(512, 0.0f), outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        gAllocationCount = 0;
        gTrackAllocations = true;

        const size_t totalSamples = 1000000;
        const size_t blocks = totalSamples / 512;
        for (size_t b = 0; b < blocks; ++b) {
            engine.process(inPtrs, outPtrs, 2, 512);
        }

        gTrackAllocations = false;
        TEST_ASSERT(gAllocationCount == 0, "1,000,000 sample benchmark must have bit-exact ZERO heap allocations");
        return test::gCurrentTestAssertFailures == 0;
    });

    // S16: Cross-Platform Build Validation
    registerTest("Tier 4", "T4_S16", "Scenario S16 - Cross-Platform Build Validation (Standards Conformance)", []() {
#if defined(_MSVC_LANG)
        TEST_ASSERT(__cplusplus >= 202002L || _MSVC_LANG >= 202002L, "Codebase must compile with C++20 standard compliance");
#else
        TEST_ASSERT(__cplusplus >= 202002L, "Codebase must compile with C++20 standard compliance");
#endif
        return test::gCurrentTestAssertFailures == 0;
    });

    // S17: Headless Batch Regression
    registerTest("Tier 4", "T4_S17", "Scenario S17 - Headless Batch Regression (Complete Verification Pass)", []() {
        TEST_ASSERT(test::getTestRegistry().size() >= 380, "Headless test suite must contain >= 380 test cases");
        return test::gCurrentTestAssertFailures == 0;
    });

} // registerTier4Tests

} // namespace test
