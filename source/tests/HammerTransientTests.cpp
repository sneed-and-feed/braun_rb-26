#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <algorithm>
#include "AcousticExciter.h"

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAILED: " << msg << " (" #cond ")\n"; \
        return 1; \
    } \
} while (0)

int main() {
    const double fs = 48000.0;
    std::cout << "=================================================================\n";
    std::cout << "=== BRAUN RB-26 — HAMMER TRANSIENT & CHIME DE-CLICK TEST SUITE ===\n";
    std::cout << "=================================================================\n";

    // -------------------------------------------------------------------------
    // TEST 1: LaboratoryImpulseGenerator Hammer Thud Zero Initial Jump
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 1] LaboratoryImpulseGenerator Hammer Thud Zero Initial Jump\n";
    {
        rb26::LaboratoryImpulseGenerator gen;
        gen.prepare(fs);

        gen.triggerHammerThud(0.7f);
        float outL = 0.0f, outR = 0.0f;
        gen.processSample(outL, outR);

        std::cout << "  Sample 0 after triggerHammerThud: outL = " << outL << ", outR = " << outR << "\n";
        // Pre-fix: Sample 0 was 0.51 (hard contact click typewriter pop).
        // Post-fix: Must start smoothly near 0.0 (anti-click attack).
        TEST_ASSERT(std::abs(outL) < 0.005f, "Sample 0 must not contain a 0.51 hard click step jump");
        TEST_ASSERT(std::abs(outR) < 0.005f, "Sample 0 must not contain a 0.51 hard click step jump");

        float maxSampleJump = 0.0f;
        float prev = outL;
        float peak = 0.0f;
        for (int i = 1; i < 2000; ++i) {
            float sL = 0.0f, sR = 0.0f;
            gen.processSample(sL, sR);
            float jump = std::abs(sL - prev);
            if (jump > maxSampleJump) maxSampleJump = jump;
            if (std::abs(sL) > peak) peak = std::abs(sL);
            prev = sL;
        }
        std::cout << "  Hammer thud peak = " << peak << ", max consecutive sample jump = " << maxSampleJump << "\n";
        TEST_ASSERT(peak > 0.05f, "Hammer thud must produce audible resonance");
        TEST_ASSERT(maxSampleJump < 0.05f, "Hammer thud must not have discontinuous sample jumps");
        std::cout << "  -> PASS: Hammer thud has smooth anti-click attack without typewriter click!\n";
    }

    // -------------------------------------------------------------------------
    // TEST 2: LaboratoryImpulseGenerator Rapid Hammer Retrigger De-Clicking
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 2] LaboratoryImpulseGenerator Rapid Hammer Retrigger De-Clicking\n";
    {
        rb26::LaboratoryImpulseGenerator gen;
        gen.prepare(fs);

        gen.triggerHammerThud(0.85f);
        float prev = 0.0f;
        for (int i = 0; i < 60; ++i) { // 60 samples into attack (~1.25ms, peak amplitude)
            float sL = 0.0f, sR = 0.0f;
            gen.processSample(sL, sR);
            prev = sL;
        }
        std::cout << "  Amplitude immediately prior to retrigger: " << prev << "\n";
        TEST_ASSERT(std::abs(prev) > 0.02f, "Should be mid-wave at sample 60");

        // Retrigger mid-flight!
        gen.triggerHammerThud(0.85f);
        float nextL = 0.0f, nextR = 0.0f;
        gen.processSample(nextL, nextR);
        float jump = std::abs(nextL - prev);
        std::cout << "  Step jump on retrigger: " << jump << " (prev = " << prev << ", next = " << nextL << ")\n";
        TEST_ASSERT(jump < 0.015f, "Retrigger must seamlessly crossfade without sample drop click");
        std::cout << "  -> PASS: Rapid hammer retriggering is smoothly crossfaded!\n";
    }

    // -------------------------------------------------------------------------
    // TEST 3: ChimeVoice In-Flight Hammer Stealing Continuity
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 3] ChimeVoice In-Flight Hammer Stealing C0 Continuity\n";
    {
        rb26::ChimeVoice voice;
        voice.prepare(fs, 0);

        voice.trigger(60.0f, 0.90f, 3.5f, false);
        float prevL = 0.0f;
        // Run 150 samples (~3.1ms, right in the middle of the felt hammer transient)
        for (int i = 0; i < 150; ++i) {
            float sL = 0.0f, sR = 0.0f;
            voice.processSample(sL, sR);
            prevL = sL;
        }
        std::cout << "  Chime voice output at sample 150 (during hammer strike): " << prevL << "\n";

        // Steal voice at this exact instant
        voice.trigger(72.0f, 0.80f, 3.5f, false);
        float newL = 0.0f, newR = 0.0f;
        voice.processSample(newL, newR);
        float stealJump = std::abs(newL - prevL);
        std::cout << "  Voice stealing sample jump: " << stealJump << " (prev = " << prevL << ", new = " << newL << ")\n";

        // With old code (missing hammer in steal calculation), stealJump was ~0.035.
        // With mLastVoiceL crossfade, stealJump should be < 0.005.
        TEST_ASSERT(stealJump < 0.005f, "Voice stealing must preserve C0 continuity during hammer transient");
        std::cout << "  -> PASS: Voice stealing during hammer transient maintains exact C0 continuity!\n";
    }

    // -------------------------------------------------------------------------
    // TEST 4: Same-Pitch Voice Re-triggering (No Duplicate Voices)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 4] Same-Pitch Voice Conservation & Retriggering\n";
    {
        rb26::AcousticExciterEngine engine;
        engine.prepare(fs);

        // Strike note 60 once
        engine.triggerVoice(60.0f, 0.70f, 3.5f, false);
        TEST_ASSERT(engine.getActiveVoiceCount() == 1, "Must have exactly 1 active voice after first strike");

        // Process 100 samples
        std::vector<float> outL(100, 0.0f), outR(100, 0.0f);
        engine.process(outL.data(), outR.data(), 100);

        // Strike note 60 again (same pitch restrike)
        engine.triggerVoice(60.0f, 0.70f, 3.5f, false);
        std::cout << "  Active voice count after re-striking same note: " << engine.getActiveVoiceCount() << "\n";
        // Must reuse the existing voice instead of spawning voice 2 on the same string!
        TEST_ASSERT(engine.getActiveVoiceCount() == 1, "Must reuse active voice for same pitch restrike");

        // Now strike note 64 (different pitch)
        engine.triggerVoice(64.0f, 0.70f, 3.5f, false);
        std::cout << "  Active voice count after striking different note: " << engine.getActiveVoiceCount() << "\n";
        TEST_ASSERT(engine.getActiveVoiceCount() == 2, "Must allocate new voice for different pitch");
        std::cout << "  -> PASS: Same-pitch restrikes reuse voice and prevent pileup!\n";
    }

    // -------------------------------------------------------------------------
    // TEST 5: Chord Strum Phase Decorrelation
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 5] Chord Strum Phase Decorrelation\n";
    {
        rb26::AcousticExciterEngine engine;
        engine.prepare(fs);

        // Trigger instant strum chord (all voices start together)
        engine.triggerChord(0, 60.0f, 0.80f, rb26::StrumSpeed::Instant);

        std::vector<float> chordL(2048, 0.0f), chordR(2048, 0.0f);
        engine.process(chordL.data(), chordR.data(), 2048);

        float maxJump = 0.0f;
        for (int i = 1; i < 2048; ++i) {
            float jumpL = std::abs(chordL[i] - chordL[i - 1]);
            if (jumpL > maxJump) maxJump = jumpL;
            TEST_ASSERT(std::isfinite(chordL[i]), "Output must be finite");
            TEST_ASSERT(std::isfinite(chordR[i]), "Output must be finite");
        }
        std::cout << "  Instant chord max consecutive sample jump: " << maxJump << "\n";
        TEST_ASSERT(maxJump < 0.05f, "Instant chord must not produce constructive phase step jump");
        std::cout << "  -> PASS: Chord strum voices are decorrelated and free of transient spikes!\n";
    }

    std::cout << "\n=================================================================\n";
    std::cout << "=== ALL HAMMER TRANSIENT & DE-CLICK TESTS PASSED (100%) ===\n";
    std::cout << "=================================================================\n";
    return 0;
}
