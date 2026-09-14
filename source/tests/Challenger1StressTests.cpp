#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <complex>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>
#include <cassert>
#include <string>

// Core DSP Headers
#include "DspMath.h"
#include "BoundedSaturator.h"
#include "PitchShifter.h"
#include "LowBandModalMatrix.h"
#include "EarlyReflections.h"
#include "TailModulator.h"
#include "FdnReverbTank.h"
#include "Rb26Engine.h"

// ============================================================================
// Global Heap Allocation Tracker for Real-Time Safety Verification
// ============================================================================
static bool gTrackAllocations = false;
static size_t gAllocationCount = 0;
static size_t gBytesAllocated = 0;

void* operator new(size_t size) {
    if (gTrackAllocations) {
        ++gAllocationCount;
        gBytesAllocated += size;
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

namespace test {

static int gFailedAssertions = 0;

#define ADVERSARIAL_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "  [FAIL] Line " << __LINE__ << ": " << (msg) << "\n"; \
        ++test::gFailedAssertions; \
        ++localFailures; \
    } \
} while (0)

#define ADVERSARIAL_ASSERT_BOUND(val, minV, maxV, msg) do { \
    float v_ = static_cast<float>(val); \
    if (std::isnan(v_) || std::isinf(v_) || v_ < (minV) || v_ > (maxV)) { \
        std::cerr << "  [FAIL] Line " << __LINE__ << ": " << (msg) \
                  << " (val: " << v_ << " outside [" << (minV) << ", " << (maxV) << "])\n"; \
        ++test::gFailedAssertions; \
        ++localFailures; \
    } \
} while (0)

// Helper to compute RMS
static double calcRMS(const float* data, size_t count) {
    if (count == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    }
    return std::sqrt(sum / static_cast<double>(count));
}

// Helper to compute peak
static float calcPeak(const float* data, size_t count) {
    float peak = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        peak = std::max(peak, std::abs(data[i]));
    }
    return peak;
}

// ============================================================================
// TEST 1: Variable & Extreme Block Sizes (1 to 8192 Samples)
// ============================================================================
bool testVariableBlockSizes() {
    int localFailures = 0;
    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "[Tier 5.1] Variable & Extreme Block Sizes (1 to 8192 Samples)\n";
    std::cout << "------------------------------------------------------------\n";

    const std::vector<int> testBlocks = {
        1, 2, 3, 5, 7, 13, 31, 64, 127, 128, 255, 256, 511, 512, 1023, 1024, 2047, 2048, 4096, 7777, 8192
    };

    rb26::Rb26ReverbEngine engine;
    engine.prepare(48000.0, 8192);

    rb26::Rb26Parameters p;
    p.roomSize = 0.85f;
    p.decayRt60Sec = 3.0f;
    p.dryWetMix = 0.50f;
    engine.setParameters(p);

    std::vector<float> inBufferL(8192, 0.4f);
    std::vector<float> inBufferR(8192, -0.4f);
    std::vector<float> outBufferL(8192, 0.0f);
    std::vector<float> outBufferR(8192, 0.0f);

    const float* inPtrs[2] = { inBufferL.data(), inBufferR.data() };
    float* outPtrs[2] = { outBufferL.data(), outBufferR.data() };

    gAllocationCount = 0;

    for (int bs : testBlocks) {
        // Generate pseudo-random signal in input buffer
        for (int i = 0; i < bs; ++i) {
            inBufferL[i] = 0.3f * std::sin(static_cast<float>(i * 0.05f));
            inBufferR[i] = 0.3f * std::cos(static_cast<float>(i * 0.07f));
        }

        // Scope allocation tracking strictly to engine.process callback
        gTrackAllocations = true;
        engine.process(inPtrs, outPtrs, 2, bs);
        gTrackAllocations = false;

        float peakL = calcPeak(outBufferL.data(), bs);
        float peakR = calcPeak(outBufferR.data(), bs);

        ADVERSARIAL_ASSERT(!std::isnan(peakL) && !std::isinf(peakL), "Output L must be finite");
        ADVERSARIAL_ASSERT(!std::isnan(peakR) && !std::isinf(peakR), "Output R must be finite");
        ADVERSARIAL_ASSERT_BOUND(peakL, 0.0f, 1.05f, "Peak L must stay bounded <= 1.05");
        ADVERSARIAL_ASSERT_BOUND(peakR, 0.0f, 1.05f, "Peak R must stay bounded <= 1.05");

        std::cout << "  Block size: " << std::setw(4) << bs 
                  << " -> Peak L: " << std::fixed << std::setprecision(4) << peakL 
                  << " | Allocations: " << gAllocationCount << "\n";
    }

    ADVERSARIAL_ASSERT(gAllocationCount == 0, "Variable block processing must produce exactly 0 heap allocations");

    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST 2: Sustained 192 kHz Audiophile Stress
// ============================================================================
bool test192kHzSustainedStress() {
    int localFailures = 0;
    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "[Tier 5.2] Sustained 192 kHz Audiophile Stress\n";
    std::cout << "------------------------------------------------------------\n";

    rb26::Rb26ReverbEngine engine;
    engine.prepare(192000.0, 1024);

    rb26::Rb26Parameters p;
    p.preDelayMs = 500.0f;          // Maximum pre-delay (96,000 samples at 192 kHz)
    p.roomSize = 2.0f;              // Maximum room size (scaled by 4.0 at 192 kHz)
    p.decayRt60Sec = 30.0f;         // Maximum RT60
    p.highDampingHz = 20000.0f;     // Maximum damping cutoff
    p.diffusionDensity = 1.0f;      // Maximum diffusion
    p.shimmerSend = 1.0f;
    p.dimmerSend = 1.0f;
    p.shimmerInterval = 24;         // +24 semitones
    p.dimmerInterval = -24;         // -24 semitones
    p.pitchBlend = 0.0f;            // 50/50 shimmer/dimmer
    p.pitchFeedback = 0.90f;        // High pitch feedback
    p.tailModRateHz = 5.0f;         // Max modulation rate
    p.tailModDepthMs = 5.0f;        // Max modulation depth (960 samples at 192 kHz)
    p.tailBloomMs = 300.0f;
    p.bassRt60Mult = 4.0f;          // Max low-end decay multiplier
    p.lowCrossoverHz = 400.0f;
    p.punchDucking = 1.0f;
    p.subMonoHz = 250.0f;
    p.stereoWidth = 2.0f;
    p.dryWetMix = 1.0f;             // 100% wet
    p.limiterEnable = true;
    engine.setParameters(p);

    const int blockSize = 1024;
    const size_t totalSamples = 500000; // ~2.6 seconds at 192 kHz
    const size_t numBlocks = totalSamples / blockSize;

    std::vector<float> inL(blockSize, 0.0f);
    std::vector<float> inR(blockSize, 0.0f);
    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    // Initial sharp Dirac impulse train
    inL[0] = 1.0f; inR[0] = -1.0f;
    inL[256] = 0.8f; inR[256] = 0.8f;
    inL[512] = -0.9f; inR[512] = 0.9f;

    gAllocationCount = 0;
    float globalMaxPeak = 0.0f;

    for (size_t b = 0; b < numBlocks; ++b) {
        if (b == 100) {
            // Second burst halfway through
            inL[0] = 2.0f; inR[0] = -2.0f;
        }

        gTrackAllocations = true;
        engine.process(inPtrs, outPtrs, 2, blockSize);
        gTrackAllocations = false;

        if (b == 0 || b == 100) {
            inL[0] = 0.0f; inR[0] = 0.0f;
            inL[256] = 0.0f; inR[256] = 0.0f;
            inL[512] = 0.0f; inR[512] = 0.0f;
        }

        float peakL = calcPeak(outL.data(), blockSize);
        float peakR = calcPeak(outR.data(), blockSize);
        globalMaxPeak = std::max(globalMaxPeak, std::max(peakL, peakR));

        ADVERSARIAL_ASSERT(!std::isnan(peakL) && !std::isinf(peakL), "Output L must be finite at 192 kHz");
        ADVERSARIAL_ASSERT(!std::isnan(peakR) && !std::isinf(peakR), "Output R must be finite at 192 kHz");
        ADVERSARIAL_ASSERT_BOUND(peakL, 0.0f, 1.05f, "Peak L must remain <= 1.05 at 192 kHz");
        ADVERSARIAL_ASSERT_BOUND(peakR, 0.0f, 1.05f, "Peak R must remain <= 1.05 at 192 kHz");
    }

    ADVERSARIAL_ASSERT(gAllocationCount == 0, "192 kHz processing must produce 0 heap allocations");

    std::cout << "  Processed " << totalSamples << " samples at 192 kHz.\n";
    std::cout << "  Global Max Peak: " << globalMaxPeak << "\n";
    std::cout << "  Heap Allocations: " << gAllocationCount << "\n";
    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST 3: Infinite Decay Freeze Hold with +40 dBFS Overload Bursts
// ============================================================================
bool testFreezeHoldOverloadStress() {
    int localFailures = 0;
    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "[Tier 5.3] Infinite Decay Freeze Hold with +40 dBFS Overload Bursts\n";
    std::cout << "------------------------------------------------------------\n";

    rb26::Rb26ReverbEngine engine;
    engine.prepare(48000.0, 512);

    rb26::Rb26Parameters p;
    p.roomSize = 1.0f;
    p.decayRt60Sec = 10.0f;
    p.shimmerSend = 0.5f;
    p.dimmerSend = 0.5f;
    p.pitchFeedback = 0.90f;
    p.dryWetMix = 1.0f; // 100% wet
    p.freezeHold = false;
    engine.setParameters(p);

    const int blockSize = 512;
    std::vector<float> inL(blockSize, 0.0f);
    std::vector<float> inR(blockSize, 0.0f);
    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    // Step A: Seed initial tank energy with musical chords
    for (int i = 0; i < blockSize; ++i) {
        inL[i] = 0.5f * std::sin(rb26::kTwoPi * 440.0 * i / 48000.0);
        inR[i] = 0.5f * std::cos(rb26::kTwoPi * 554.37 * i / 48000.0);
    }
    for (int b = 0; b < 10; ++b) {
        engine.process(inPtrs, outPtrs, 2, blockSize);
    }
    std::fill(inL.begin(), inL.end(), 0.0f);
    std::fill(inR.begin(), inR.end(), 0.0f);

    double preFreezeRms = calcRMS(outL.data(), blockSize);
    std::cout << "  Pre-Freeze RMS: " << preFreezeRms << "\n";

    // Step B: Latch Freeze Hold
    p.freezeHold = true;
    engine.setParameters(p);

    // Step C: Bombard with +40 dBFS (amplitude 100.0f) bursts while in freeze
    float maxOverloadPeak = 0.0f;
    for (int burst = 0; burst < 10; ++burst) {
        // Continuous block of +40 dBFS square wave
        for (int i = 0; i < blockSize; ++i) {
            inL[i] = (i % 2 == 0) ? 100.0f : -100.0f;
            inR[i] = (i % 2 == 0) ? -100.0f : 100.0f;
        }
        engine.process(inPtrs, outPtrs, 2, blockSize);
        maxOverloadPeak = std::max(maxOverloadPeak, calcPeak(outL.data(), blockSize));

        // Let freeze circulate for 5 blocks with silence
        std::fill(inL.begin(), inL.end(), 0.0f);
        std::fill(inR.begin(), inR.end(), 0.0f);
        for (int b = 0; b < 5; ++b) {
            engine.process(inPtrs, outPtrs, 2, blockSize);
            maxOverloadPeak = std::max(maxOverloadPeak, calcPeak(outL.data(), blockSize));
        }
    }

    std::cout << "  Max Peak during +40 dBFS bursts: " << maxOverloadPeak << "\n";
    ADVERSARIAL_ASSERT_BOUND(maxOverloadPeak, 0.0f, 1.05f, "Freeze hold must keep output strictly bounded <= 1.05 under +40 dBFS bursts");

    // Step D: Extended freeze circulation for 100,000 samples (~200 blocks)
    double midFreezeRms = 0.0;
    double endFreezeRms = 0.0;
    for (int b = 0; b < 200; ++b) {
        engine.process(inPtrs, outPtrs, 2, blockSize);
        float peak = calcPeak(outL.data(), blockSize);
        ADVERSARIAL_ASSERT_BOUND(peak, 0.0f, 1.05f, "Peak must stay bounded during extended freeze");
        if (b == 100) midFreezeRms = calcRMS(outL.data(), blockSize);
        if (b == 199) endFreezeRms = calcRMS(outL.data(), blockSize);
    }

    std::cout << "  Mid-Freeze RMS: " << midFreezeRms << " | End-Freeze RMS: " << endFreezeRms << "\n";
    ADVERSARIAL_ASSERT(endFreezeRms > 0.001, "Freeze hold must sustain non-zero reverberant energy over 100,000 samples");

    // Step E: Release Freeze and verify natural decay
    p.freezeHold = false;
    p.decayRt60Sec = 0.5f; // Fast decay to verify smooth release
    engine.setParameters(p);

    for (int b = 0; b < 100; ++b) {
        engine.process(inPtrs, outPtrs, 2, blockSize);
    }
    double releasedRms = calcRMS(outL.data(), blockSize);
    std::cout << "  Post-Release RMS after 50,000 samples: " << releasedRms << "\n";
    ADVERSARIAL_ASSERT(releasedRms < 0.001, "Reverb must naturally decay to silence after unfreeze");

    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST 4: Concurrent Rapid Non-Blocking Parameter Updates (Multi-Threaded)
// ============================================================================
bool testConcurrentParameterUpdates() {
    int localFailures = 0;
    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "[Tier 5.4] Concurrent Rapid Parameter Updates (Multi-Threaded Race Stress)\n";
    std::cout << "------------------------------------------------------------\n";

    rb26::Rb26ReverbEngine engine;
    engine.prepare(48000.0, 512);

    std::atomic<bool> keepRunning { true };
    std::atomic<size_t> totalAudioSamplesProcessed { 0 };
    std::atomic<size_t> totalParamUpdatesCount { 0 };
    std::atomic<size_t> totalTelemetryFramesRead { 0 };
    std::atomic<int> audioThreadErrors { 0 };

    // Thread 1: Audio Processing Thread
    std::thread audioThread([&]() {
        const int bs = 128;
        std::vector<float> inL(bs, 0.2f);
        std::vector<float> inR(bs, -0.2f);
        std::vector<float> outL(bs, 0.0f);
        std::vector<float> outR(bs, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        while (keepRunning.load(std::memory_order_relaxed)) {
            engine.process(inPtrs, outPtrs, 2, bs);
            totalAudioSamplesProcessed.fetch_add(bs, std::memory_order_relaxed);

            float peakL = calcPeak(outL.data(), bs);
            if (std::isnan(peakL) || std::isinf(peakL) || peakL > 1.5f) {
                audioThreadErrors.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // Thread 2: Rapid Parameter Automation Thread (1000 updates/sec)
    std::thread paramThread([&]() {
        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> roomDist(0.1f, 2.0f);
        std::uniform_real_distribution<float> decayDist(0.2f, 30.0f);
        std::uniform_real_distribution<float> preDelayDist(0.0f, 500.0f);
        std::uniform_real_distribution<float> blendDist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> fbDist(0.0f, 0.95f);
        std::uniform_real_distribution<float> widthDist(0.0f, 2.0f);

        while (keepRunning.load(std::memory_order_relaxed)) {
            rb26::Rb26Parameters p;
            p.roomSize = roomDist(rng);
            p.decayRt60Sec = decayDist(rng);
            p.preDelayMs = preDelayDist(rng);
            p.pitchBlend = blendDist(rng);
            p.pitchFeedback = fbDist(rng);
            p.stereoWidth = widthDist(rng);
            p.shimmerInterval = (rng() % 2 == 0) ? 12 : 7;
            p.dimmerInterval = (rng() % 2 == 0) ? -12 : -24;

            engine.setParameters(p);
            totalParamUpdatesCount.fetch_add(1, std::memory_order_relaxed);

            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    // Thread 3: Freeze Toggle Thread (latches/unlatches freeze every 2 ms)
    std::thread freezeThread([&]() {
        bool freezeState = false;
        while (keepRunning.load(std::memory_order_relaxed)) {
            freezeState = !freezeState;
            rb26::Rb26Parameters p;
            p.freezeHold = freezeState;
            p.decayRt60Sec = 4.0f;
            engine.setParameters(p);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    // Thread 4: Telemetry Consumer Thread
    std::thread telemetryThread([&]() {
        rb26::Rb26ReverbEngine::VisualizerFrame frame;
        while (keepRunning.load(std::memory_order_relaxed)) {
            while (engine.popVisualizerFrame(frame)) {
                totalTelemetryFramesRead.fetch_add(1, std::memory_order_relaxed);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
    });

    // Run multi-threaded race stress for 1.5 seconds
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    keepRunning.store(false, std::memory_order_relaxed);

    audioThread.join();
    paramThread.join();
    freezeThread.join();
    telemetryThread.join();

    ADVERSARIAL_ASSERT(audioThreadErrors.load() == 0, "Audio thread must encounter 0 numerical anomalies during parameter races");
    ADVERSARIAL_ASSERT(totalAudioSamplesProcessed.load() > 500000, "Audio thread must process >= 500,000 samples during race");
    ADVERSARIAL_ASSERT(totalParamUpdatesCount.load() > 50, "Parameter thread must execute >= 50 updates during race");

    std::cout << "  Audio Samples Processed : " << totalAudioSamplesProcessed.load() << "\n";
    std::cout << "  Parameter Updates Done  : " << totalParamUpdatesCount.load() << "\n";
    std::cout << "  Telemetry Frames Popped : " << totalTelemetryFramesRead.load() << "\n";
    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST 5: Denormal and Subnormal Decay Stress
// ============================================================================
bool testDenormalSubnormalStress() {
    int localFailures = 0;
    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "[Tier 5.5] Denormal and Subnormal Decay Stress\n";
    std::cout << "------------------------------------------------------------\n";

    rb26::Rb26ReverbEngine engine;
    engine.prepare(48000.0, 512);

    rb26::Rb26Parameters p;
    p.decayRt60Sec = 0.5f;
    engine.setParameters(p);

    const int blockSize = 512;
    std::vector<float> inL(blockSize, 0.0f);
    std::vector<float> inR(blockSize, 0.0f);
    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    // Feed subnormal values (1e-38f)
    for (int i = 0; i < blockSize; ++i) {
        inL[i] = 1.0e-38f;
        inR[i] = -1.0e-38f;
    }

    auto startNormal = std::chrono::high_resolution_clock::now();
    for (int b = 0; b < 100; ++b) {
        engine.process(inPtrs, outPtrs, 2, blockSize);
    }
    auto endNormal = std::chrono::high_resolution_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(endNormal - startNormal).count();

    std::cout << "  Processed 51,200 subnormal samples in " << elapsedUs << " us.\n";
    std::cout << "  Final Sample Out: " << outL[0] << "\n";

    ADVERSARIAL_ASSERT(!std::isnan(outL[0]), "Subnormal processing must not produce NaN");
    ADVERSARIAL_ASSERT(std::abs(outL[0]) == 0.0f || std::abs(outL[0]) >= 1.0e-15f, 
                       "Subnormal values must be flushed to exact 0.0f by FTZ/DAZ");

    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST 6: Real-Time Audio Thread Zero-Allocation Audit (2,000,000 Samples)
// ============================================================================
bool testZeroAllocationAudit() {
    int localFailures = 0;
    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "[Tier 5.6] Real-Time Audio Thread Zero-Allocation Audit (2,000,000 Samples)\n";
    std::cout << "------------------------------------------------------------\n";

    const double sampleRates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    const int blockSizes[] = { 1, 16, 64, 128, 256, 512, 1024, 2048, 4096 };

    for (double fs : sampleRates) {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(fs, 4096);

        std::vector<float> inL(4096, 0.15f);
        std::vector<float> inR(4096, -0.15f);
        std::vector<float> outL(4096, 0.0f);
        std::vector<float> outR(4096, 0.0f);

        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        gAllocationCount = 0;

        const size_t targetSamples = 500000;
        size_t samplesProcessed = 0;
        size_t bIdx = 0;

        while (samplesProcessed < targetSamples) {
            int bs = blockSizes[bIdx % 9];
            gTrackAllocations = true;
            engine.process(inPtrs, outPtrs, 2, bs);
            gTrackAllocations = false;
            samplesProcessed += bs;
            ++bIdx;
        }

        std::cout << "  Sample Rate: " << std::setw(6) << static_cast<int>(fs) 
                  << " Hz | Processed: " << samplesProcessed 
                  << " samples | Allocs: " << gAllocationCount << "\n";

        ADVERSARIAL_ASSERT(gAllocationCount == 0, "Zero dynamic allocations permitted in audio thread");
    }

    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

} // namespace test

// ============================================================================
// Main Adversarial Runner Entry Point
// ============================================================================
int main() {
    std::cout << "================================================================\n";
    std::cout << "   BRAUN RB-26 — M5 ADVERSARIAL STRESS TEST RUNNER (TIER 5)     \n";
    std::cout << "   Challenger 1: Stress & White-Box Hardening                   \n";
    std::cout << "================================================================\n";

    bool pass1 = test::testVariableBlockSizes();
    bool pass2 = test::test192kHzSustainedStress();
    bool pass3 = test::testFreezeHoldOverloadStress();
    bool pass4 = test::testConcurrentParameterUpdates();
    bool pass5 = test::testDenormalSubnormalStress();
    bool pass6 = test::testZeroAllocationAudit();

    bool allPass = pass1 && pass2 && pass3 && pass4 && pass5 && pass6 && (test::gFailedAssertions == 0);

    std::cout << "\n================================================================\n";
    std::cout << "                 ADVERSARIAL STRESS TEST SUMMARY                \n";
    std::cout << "================================================================\n";
    std::cout << "  Tier 5.1 (Variable Blocks 1..8192) : " << (pass1 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Tier 5.2 (192 kHz Sustained Stress): " << (pass2 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Tier 5.3 (Freeze +40dBFS Overload) : " << (pass3 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Tier 5.4 (Concurrent Thread Races) : " << (pass4 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Tier 5.5 (Denormal / Subnormal)    : " << (pass5 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Tier 5.6 (Zero Heap Allocations)   : " << (pass6 ? "PASS" : "FAIL") << "\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "  Total Assertion Failures: " << test::gFailedAssertions << "\n";
    std::cout << "================================================================\n";
    std::cout << "  OVERALL ADVERSARIAL VERDICT: " << (allPass ? "APPROVE" : "REQUEST_CHANGES") << "\n";
    std::cout << "================================================================\n";

    return allPass ? 0 : 1;
}
