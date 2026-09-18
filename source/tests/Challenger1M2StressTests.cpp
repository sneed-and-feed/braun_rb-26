#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <complex>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <random>
#include <cassert>
#include <string>

// Operator new overrides for real-time heap allocation tracking
bool gTrackAllocations = false;
size_t gAllocationCount = 0;
size_t gBytesAllocated = 0;

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

void* operator new[](size_t size) {
    if (gTrackAllocations) {
        ++gAllocationCount;
        gBytesAllocated += size;
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p, size_t) noexcept {
    std::free(p);
}

// Core DSP Headers
#include "DspMath.h"
#include "BoundedSaturator.h"
#include "PitchShifter.h"
#include "LowBandModalMatrix.h"
#include "EarlyReflections.h"
#include "TailModulator.h"
#include "FdnReverbTank.h"
#include "Rb26Engine.h"
#include "TestHarness.h"

namespace challenger {

static int gFailedAssertions = 0;
static int gTotalAssertions = 0;

#define CH_ASSERT(cond, msg) do { \
    ++challenger::gTotalAssertions; \
    if (!(cond)) { \
        std::cerr << "  [ASSERTION FAILED] Line " << __LINE__ << ": " << (msg) << "\n"; \
        ++challenger::gFailedAssertions; \
        ++localFailures; \
    } \
} while (0)

#define CH_ASSERT_NEAR(val, exp, tol, msg) do { \
    ++challenger::gTotalAssertions; \
    double v_ = static_cast<double>(val); \
    double e_ = static_cast<double>(exp); \
    double diff_ = std::abs(v_ - e_); \
    if (diff_ > (tol) || std::isnan(v_) || std::isinf(v_)) { \
        std::cerr << "  [ASSERTION FAILED] Line " << __LINE__ << ": " << (msg) \
                  << " (actual: " << v_ << ", expected: " << e_ << ", diff: " << diff_ << ", tol: " << (tol) << ")\n"; \
        ++challenger::gFailedAssertions; \
        ++localFailures; \
    } \
} while (0)

// Helper: Calculate RMS of buffer
[[maybe_unused]] static double calcRMS(const float* data, size_t count) {
    if (count == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    }
    return std::sqrt(sum / static_cast<double>(count));
}

// Helper: Calculate Peak
[[maybe_unused]] static float calcPeak(const float* data, size_t count) {
    float peak = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        peak = std::max(peak, std::abs(data[i]));
    }
    return peak;
}

// ============================================================================
// SUITE 1: Freeze Hold Infinite Energy Recirculation & Damping Bypass
// ============================================================================
bool testFreezeEnergyRecirculation() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M2 Challenge 1] Freeze Hold Recirculation & Damping Bypass\n";
    std::cout << "============================================================\n";

    // 1.1: 50,000-sample sustained freeze hold energy maintenance
    {
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0);
        tank.setParameters(1.0f, 10.0f, 18000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        // Seed tank energy with 400 Hz sine wave for 2000 samples
        float outL = 0.0f, outR = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            float in = 0.5f * std::sin(2.0f * 3.14159265f * 400.0f * static_cast<float>(i) / 48000.0f);
            tank.processSample(in, in, 0.0f, 0.0f, outL, outR);
        }
        // Let it diffuse for 2000 samples
        for (int i = 0; i < 2000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        }

        // Latch freeze hold
        tank.setParameters(1.0f, 30.0f, 18000.0f, 0.5f, true, 0.85f, 0.0f, 85.0f);

        double rmsEarly = 0.0;
        double rmsMid = 0.0;
        double rmsLate = 0.0;

        for (int i = 0; i < 10000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
            if (i >= 8000) rmsEarly += outL * outL;
        }
        for (int i = 0; i < 20000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
            if (i >= 18000) rmsMid += outL * outL;
        }
        for (int i = 0; i < 25000; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
            if (i >= 23000) rmsLate += outL * outL;
        }

        rmsEarly = std::sqrt(rmsEarly / 2000.0);
        rmsMid   = std::sqrt(rmsMid / 2000.0);
        rmsLate  = std::sqrt(rmsLate / 2000.0);

        std::cout << "  [1.1] Freeze Hold RMS: Early(10k)=" << rmsEarly 
                  << ", Mid(30k)=" << rmsMid 
                  << ", Late(55k)=" << rmsLate << "\n";

        CH_ASSERT(rmsLate > 0.001, "Frozen reverberation energy must sustain non-zero after 55,000 samples");
        CH_ASSERT(rmsLate > rmsEarly * 0.40, "Frozen energy dissipation must not drop below 40% over extended freeze");
    }

    // 1.2: Damping bypass verification during freeze hold
    // In ManifoldDelayNetwork, when freezeAmount == 1.0f, damping filter is bypassed (s = rawSample).
    // Test with low cutoff (high damping, e.g. 1000 Hz) and high frequency stimulus (4000 Hz).
    {
        rb26::FdnReverbTank tankDamped, tankFreeze;
        tankDamped.prepare(48000.0);
        tankFreeze.prepare(48000.0);

        // Low highDampingHz = 1000 Hz causes strong damping on 4000 Hz
        tankDamped.setParameters(1.0f, 20.0f, 1000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);
        tankFreeze.setParameters(1.0f, 20.0f, 1000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

        // Seed with 4 kHz tone for 1000 samples
        float outL = 0.0f, outR = 0.0f;
        for (int i = 0; i < 1000; ++i) {
            float in = 0.5f * std::sin(2.0f * 3.14159265f * 4000.0f * static_cast<float>(i) / 48000.0f);
            tankDamped.processSample(in, in, 0.0f, 0.0f, outL, outR);
            tankFreeze.processSample(in, in, 0.0f, 0.0f, outL, outR);
        }

        // Engage freeze on tankFreeze
        tankFreeze.setParameters(1.0f, 20.0f, 1000.0f, 0.5f, true, 0.85f, 0.0f, 85.0f);

        // Run 20,000 samples of silence
        double dampedRms = 0.0;
        double freezeRms = 0.0;
        for (int i = 0; i < 20000; ++i) {
            float dL = 0.0f, dR = 0.0f, fL = 0.0f, fR = 0.0f;
            tankDamped.processSample(0.0f, 0.0f, 0.0f, 0.0f, dL, dR);
            tankFreeze.processSample(0.0f, 0.0f, 0.0f, 0.0f, fL, fR);
            if (i >= 18000) {
                dampedRms += dL * dL;
                freezeRms += fL * fL;
            }
        }
        dampedRms = std::sqrt(dampedRms / 2000.0);
        freezeRms = std::sqrt(freezeRms / 2000.0);

        std::cout << "  [1.2] Damped (No Freeze) RMS: " << dampedRms 
                  << " | Damping-Bypassed (Freeze) RMS: " << freezeRms << "\n";
        CH_ASSERT(freezeRms > dampedRms * 10.0, "Freeze hold must bypass damping filter, preserving significantly more high-frequency energy");
    }

    // 1.3: Multi-rate freeze hold stability (44.1k, 48k, 96k, 192k)
    {
        const double sampleRates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
        for (double fs : sampleRates) {
            rb26::FdnReverbTank tank;
            tank.prepare(fs);
            tank.setParameters(1.0f, 15.0f, 16000.0f, 0.5f, false, 0.85f, 0.0f, 85.0f);

            float oL = 0.0f, oR = 0.0f;
            for (int i = 0; i < 2000; ++i) {
                float in = 0.4f * std::sin(2.0f * 3.14159265f * 300.0f * static_cast<float>(i) / static_cast<float>(fs));
                tank.processSample(in, in, 0.0f, 0.0f, oL, oR);
            }

            tank.setParameters(1.0f, 15.0f, 16000.0f, 0.5f, true, 0.85f, 0.0f, 85.0f);
            float peak = 0.0f;
            double lateEnergy = 0.0;
            const int testSamples = static_cast<int>(fs * 0.75); // 0.75s

            for (int i = 0; i < testSamples; ++i) {
                tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, oL, oR);
                peak = std::max(peak, std::max(std::abs(oL), std::abs(oR)));
                if (i >= testSamples - 1000) lateEnergy += oL * oL;
            }
            double rms = std::sqrt(lateEnergy / 1000.0);
            std::cout << "  [1.3] Sample Rate " << std::setw(6) << static_cast<int>(fs) 
                      << " Hz -> Peak: " << peak << ", Late RMS: " << rms << "\n";
            CH_ASSERT(peak <= 1.05f, "Peak during freeze must remain bounded <= 1.05");
            CH_ASSERT(rms > 0.001, "Late RMS during freeze must remain non-zero");
        }
    }

    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// SUITE 2: Tail Bloom Strictly Non-Negative Unipolar Excursions
// ============================================================================
bool testTailBloomUnipolarExcursions() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M2 Challenge 2] Tail Bloom Unipolar Excursions & Modulation\n";
    std::cout << "============================================================\n";

    rb26::TailModulator mod;
    mod.prepare(48000.0);

    const float testRates[] = { 0.1f, 0.5f, 1.0f, 2.5f, 5.0f, 10.0f };
    const float testDepths[] = { 0.1f, 0.5f, 1.0f, 2.5f, 5.0f };
    const float testBlooms[] = { 0.0f, 20.0f, 85.0f, 300.0f };

    float globalMinExcursion = 1.0e9f;
    float globalMaxExcursion = -1.0e9f;
    size_t samplesTested = 0;

    for (float rate : testRates) {
        for (float depth : testDepths) {
            for (float bloom : testBlooms) {
                mod.setParameters(rate, depth, bloom);
                std::array<float, 8> exc {};

                for (int i = 0; i < 500; ++i) {
                    float transient = (i == 0 || i == 100 || i == 250) ? 1.0f : 0.0f;
                    mod.processSample(transient, exc);
                    ++samplesTested;

                    for (size_t k = 0; k < 8; ++k) {
                        float e = exc[k];
                        CH_ASSERT(!std::isnan(e) && !std::isinf(e), "Excursion must be finite");
                        CH_ASSERT(e >= -1.0e-7f, "Excursion must be strictly non-negative (unipolar)");
                        globalMinExcursion = std::min(globalMinExcursion, e);
                        globalMaxExcursion = std::max(globalMaxExcursion, e);
                    }
                }
            }
        }
    }

    std::cout << "  Tested " << samplesTested << " sample frames across parameter grid.\n";
    std::cout << "  Global Min Excursion: " << globalMinExcursion << " samples\n";
    std::cout << "  Global Max Excursion: " << globalMaxExcursion << " samples\n";
    CH_ASSERT(globalMinExcursion >= 0.0f, "Global minimum excursion across all tests must be >= 0.0");

    // Test 2.2: Full scale excursion reach (0.0 to 1.0 unipolar range)
    {
        rb26::TailModulator fullMod;
        fullMod.prepare(48000.0);
        fullMod.setParameters(1.0f, 5.0f, 20.0f); // Max 5.0 ms depth, 20 ms bloom
        fullMod.reset();
        float minObs = 1.0e9f;
        float maxObs = -1.0e9f;
        std::array<float, 8> exc {};

        // Run for 3 seconds (144,000 samples) so lowest golden-ratio LFO (0.485 Hz, T=2.06s) completes full cycle
        for (int i = 0; i < 150000; ++i) {
            fullMod.processSample(0.0f, exc);
            for (size_t k = 0; k < 8; ++k) {
                minObs = std::min(minObs, exc[k]);
                maxObs = std::max(maxObs, exc[k]);
            }
        }

        const float expectedMaxSamples = (5.0f * 0.001f) * 48000.0f; // 240 samples
        std::cout << "  [2.2] Full Range 5.0 ms: MinObs=" << minObs 
                  << ", MaxObs=" << maxObs 
                  << " (ExpectedMax=" << expectedMaxSamples << ")\n";

        CH_ASSERT(minObs >= 0.0f && minObs < 5.0f, "Min excursion must reach near 0.0");
        CH_ASSERT(maxObs > expectedMaxSamples * 0.95f, "Max excursion must reach full scale >= 95% of depth");
    }

    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// SUITE 3: Low-Band Modal Matrix Punch Ducking Dynamics & Bass Transparency
// ============================================================================
bool testPunchDuckingDynamics() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M2 Challenge 3] Punch Ducking Dynamics & Bass Transparency\n";
    std::cout << "============================================================\n";

    // 3.1: Synthesized Kick Transient Attenuation Calibration
    {
        rb26::LowBandModalMatrix modal;
        modal.prepare(48000.0);
        rb26::LowBandModalParams p;
        p.crossoverHz = 200.0f;
        p.bassRt60Mult = 1.0f;
        p.punchDucking = 1.0f; // max depth
        p.subMonoHz = 100.0f;
        modal.setParameters(p);

        // Generate synthesized kick: 150 Hz down to 45 Hz with 50 ms decay envelope
        const int kickLen = 4800; // 100 ms
        std::vector<float> kick(kickLen, 0.0f);
        double phase = 0.0;
        for (int n = 0; n < kickLen; ++n) {
            double t = static_cast<double>(n) / 48000.0;
            double f = 45.0 + (150.0 - 45.0) * std::exp(-t / 0.020);
            phase += 2.0 * 3.14159265 * f / 48000.0;
            double env = std::exp(-t / 0.040);
            kick[n] = static_cast<float>(env * std::sin(phase));
        }

        // Run silence warmup, then feed kick
        float dummyL = 0.0f, dummyR = 0.0f;
        for (int i = 0; i < 1000; ++i) modal.processModalOnly(0.0f, 0.0f, dummyL, dummyR);

        float minGain = 1.0f;
        for (int n = 0; n < kickLen; ++n) {
            modal.processModalOnly(kick[n], kick[n], dummyL, dummyR);
            float g = modal.getDuckingGain();
            if (g < minGain) minGain = g;
        }

        double duckDb = 20.0 * std::log10(minGain);
        std::cout << "  [3.1] Kick Transient Min Gain: " << minGain 
                  << " (" << std::fixed << std::setprecision(2) << duckDb << " dB)\n";

        CH_ASSERT(duckDb <= -9.0 && duckDb >= -14.0, 
                  "Kick transient ducking must be strictly between -9.0 dB and -14.0 dB");
    }

    // 3.2: Sustained Bass Sine Transparency Across Spectrum (30 Hz to 200 Hz)
    {
        const double bassFreqs[] = { 30.0, 40.0, 50.0, 60.0, 80.0, 100.0, 120.0, 150.0, 200.0 };
        for (double freq : bassFreqs) {
            rb26::TransientPunchDetector detector;
            detector.prepare(48000.0);

            // Warm up with 10,000 samples of continuous sine wave
            for (int i = 0; i < 10000; ++i) {
                float s = 0.8f * std::sin(2.0f * 3.14159265f * static_cast<float>(freq) * static_cast<float>(i) / 48000.0f);
                detector.process(s, s, 1.0f);
            }

            // Test over 4,800 samples of steady-state sine
            float minG = 1.0f;
            for (int i = 0; i < 4800; ++i) {
                float s = 0.8f * std::sin(2.0f * 3.14159265f * static_cast<float>(freq) * static_cast<float>(10000 + i) / 48000.0f);
                float g = detector.process(s, s, 1.0f);
                minG = std::min(minG, g);
            }

            std::cout << "  [3.2] Sustained " << std::setw(3) << static_cast<int>(freq) 
                      << " Hz Bass Tone Min Gain: " << minG << "\n";
            CH_ASSERT(minG > 0.95f, "Sustained bass sine tone must NOT trigger ducking (gain must stay > 0.95)");
        }
    }

    // 3.3: Ducking Depth Parameter Scaling Check
    {
        const float depths[] = { 0.0f, 0.25f, 0.50f, 0.65f, 0.80f, 1.0f };
        float lastGain = 1.0f;

        for (float d : depths) {
            rb26::TransientPunchDetector detector;
            detector.prepare(48000.0);

            // Feed sharp step transient
            float minGain = 1.0f;
            for (int i = 0; i < 100; ++i) {
                float in = (i == 0) ? 1.0f : 0.0f;
                float g = detector.process(in, in, d);
                minGain = std::min(minGain, g);
            }

            double db = 20.0 * std::log10(minGain);
            std::cout << "  [3.3] Depth " << std::fixed << std::setprecision(2) << d 
                      << " -> Min Gain: " << minGain << " (" << db << " dB)\n";

            if (d == 0.0f) {
                CH_ASSERT(minGain == 1.0f, "Depth 0.0 must have 1.0 gain (0 dB)");
            } else {
                CH_ASSERT(minGain <= lastGain, "Ducking attenuation must increase monotonically with depth");
            }
            lastGain = minGain;
        }
    }

    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// SUITE 4: State Reset Invariance & Pre-Prepare Memory Safety
// ============================================================================
bool testResetInvarianceAndMemorySafety() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M2 Challenge 4] Reset Invariance & Pre-Prepare Memory Safety\n";
    std::cout << "============================================================\n";

    // 4.1: Pre-prepare memory safety across all sub-modules
    {
        std::cout << "  [4.1] Testing invocation on un-prepared / default-constructed objects...\n";

        // EarlyReflections
        std::cout << "    Checking EarlyReflections..." << std::flush;
        rb26::EarlyReflections er;
        float erL = 0.5f, erR = -0.5f;
        er.processSample(0.5f, -0.5f, erL, erR);
        CH_ASSERT(!std::isnan(erL) && !std::isnan(erR), "EarlyReflections un-prepared processSample must not crash");
        std::cout << " OK\n" << std::flush;

        // DualTapDelayPitchShifter
        std::cout << "    Checking DualTapDelayPitchShifter..." << std::flush;
        rb26::DualTapDelayPitchShifter dt;
        float dtOut = dt.processSample(0.5f);
        CH_ASSERT(!std::isnan(dtOut), "DualTapDelayPitchShifter un-prepared processSample must not crash");
        std::cout << " OK\n" << std::flush;

        // PitchShifter
        std::cout << "    Checking PitchShifter..." << std::flush;
        rb26::PitchShifter ps;
        float psL = 0.5f, psR = -0.5f;
        ps.processSample(0.5f, -0.5f, psL, psR);
        CH_ASSERT(!std::isnan(psL) && !std::isnan(psR), "PitchShifter un-prepared processSample must not crash");
        std::cout << " OK\n" << std::flush;

        // ShepardPitchSpiral
        std::cout << "    Checking ShepardPitchSpiral..." << std::flush;
        rb26::ShepardPitchSpiral sp;
        float spL = 0.5f, spR = -0.5f;
        sp.processSample(0.5f, -0.5f, spL, spR);
        CH_ASSERT(!std::isnan(spL) && !std::isnan(spR), "ShepardPitchSpiral un-prepared processSample must not crash");
        std::cout << " OK\n" << std::flush;

        // LowBandModalMatrix
        std::cout << "    Checking LowBandModalMatrix..." << std::flush;
        rb26::LowBandModalMatrix lmm;
        float lmmL = 0.5f, lmmR = -0.5f;
        lmm.processModalOnly(0.5f, -0.5f, lmmL, lmmR);
        CH_ASSERT(!std::isnan(lmmL) && !std::isnan(lmmR), "LowBandModalMatrix un-prepared processModalOnly must not crash");
        std::cout << " OK\n" << std::flush;

        // TailModulator
        std::cout << "    Checking TailModulator..." << std::flush;
        rb26::TailModulator tm;
        std::array<float, 8> tmExc {};
        tm.processSample(0.5f, tmExc);
        CH_ASSERT(!std::isnan(tmExc[0]), "TailModulator un-prepared processSample must not crash");
        std::cout << " OK\n" << std::flush;

        // Note: FdnReverbTank and Rb26ReverbEngine currently require prepare() call
        // before processSample/process because mAllpassBuffers are allocated during prepare().
        std::cout << "    Checking FdnReverbTank (prepared)..." << std::flush;
        rb26::FdnReverbTank tank;
        tank.prepare(48000.0);
        float tankL = 0.5f, tankR = -0.5f;
        tank.processSample(0.5f, -0.5f, 0.0f, 0.0f, tankL, tankR);
        CH_ASSERT(!std::isnan(tankL) && !std::isnan(tankR), "FdnReverbTank processSample must produce finite values");
        std::cout << " OK\n" << std::flush;

        // Rb26ReverbEngine
        std::cout << "    Checking Rb26ReverbEngine (prepared)..." << std::flush;
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 64);
        float inBufL[64] = { 0.2f }, inBufR[64] = { -0.2f };
        float outBufL[64] = { 0.0f }, outBufR[64] = { 0.0f };
        const float* inPtrs[2] = { inBufL, inBufR };
        float* outPtrs[2] = { outBufL, outBufR };
        engine.process(inPtrs, outPtrs, 2, 64);
        CH_ASSERT(!std::isnan(outBufL[0]), "Rb26ReverbEngine process must produce finite values");
        std::cout << " OK\n" << std::flush;
    }

    // 4.2: Deterministic Reset Invariance Subsystem Isolation
    {
        std::cout << "  [4.2] Isolating Subsystem Deterministic Reset Invariance...\n";

        // Test EarlyReflections
        {
            rb26::EarlyReflections a, b;
            a.prepare(48000.0);
            b.prepare(48000.0);
            a.setParameters(1.0f, 0.5f);
            b.setParameters(1.0f, 0.5f);
            float oL, oR;
            for (int i = 0; i < 5000; ++i) b.processSample(0.5f, -0.5f, oL, oR);
            b.reset();
            float diff = 0.0f;
            for (int i = 0; i < 500; ++i) {
                float aL, aR, bL, bR;
                a.processSample(0.3f, -0.3f, aL, aR);
                b.processSample(0.3f, -0.3f, bL, bR);
                diff = std::max(diff, std::abs(aL - bL));
            }
            std::cout << "    EarlyReflections reset diff: " << diff << "\n";
        }

        // Test PitchShifter
        {
            rb26::PitchShifter a, b;
            a.prepare(48000.0, 256);
            b.prepare(48000.0, 256);
            a.setParameters(0.5f, 0.5f, 12, -12, 0.0f, 0.0f);
            b.setParameters(0.5f, 0.5f, 12, -12, 0.0f, 0.0f);
            float oL, oR;
            for (int i = 0; i < 5000; ++i) b.processSample(0.5f, -0.5f, oL, oR);
            b.reset();
            float diff = 0.0f;
            for (int i = 0; i < 500; ++i) {
                float aL, aR, bL, bR;
                a.processSample(0.3f, -0.3f, aL, aR);
                b.processSample(0.3f, -0.3f, bL, bR);
                diff = std::max(diff, std::abs(aL - bL));
            }
            std::cout << "    PitchShifter reset diff: " << diff << "\n";
        }

        // Test LowBandModalMatrix
        {
            rb26::LowBandModalMatrix a, b;
            a.prepare(48000.0);
            b.prepare(48000.0);
            rb26::LowBandModalParams p;
            a.setParameters(p);
            b.setParameters(p);
            float oL, oR;
            for (int i = 0; i < 5000; ++i) b.processModalOnly(0.5f, -0.5f, oL, oR);
            b.reset();
            float diff = 0.0f;
            for (int i = 0; i < 500; ++i) {
                float aL, aR, bL, bR;
                a.processModalOnly(0.3f, -0.3f, aL, aR);
                b.processModalOnly(0.3f, -0.3f, bL, bR);
                diff = std::max(diff, std::abs(aL - bL));
            }
            std::cout << "    LowBandModalMatrix reset diff: " << diff << "\n";
        }

        // Test TailModulator
        {
            rb26::TailModulator a, b;
            a.prepare(48000.0);
            b.prepare(48000.0);
            a.setParameters(1.0f, 2.0f, 85.0f);
            a.reset(); // align smoother initial state
            b.setParameters(1.0f, 2.0f, 85.0f);
            std::array<float, 8> exc;
            for (int i = 0; i < 5000; ++i) b.processSample(0.5f, exc);
            b.reset();
            float diff = 0.0f;
            for (int i = 0; i < 500; ++i) {
                std::array<float, 8> aExc, bExc;
                a.processSample(0.0f, aExc);
                b.processSample(0.0f, bExc);
                for (int k = 0; k < 8; ++k) diff = std::max(diff, std::abs(aExc[k] - bExc[k]));
            }
            std::cout << "    TailModulator reset diff: " << diff << "\n";
        }

        // Test FdnReverbTank
        {
            rb26::FdnReverbTank a, b;
            a.prepare(48000.0);
            b.prepare(48000.0);
            a.setParameters(1.0f, 2.5f, 8000.0f, 0.5f, false, 0.85f, 1.2f, 85.0f);
            b.setParameters(1.0f, 2.5f, 8000.0f, 0.5f, false, 0.85f, 1.2f, 85.0f);
            float oL, oR;
            for (int i = 0; i < 5000; ++i) {
                a.processSample(0.5f, -0.5f, 0.0f, 0.0f, oL, oR);
                b.processSample(0.5f, -0.5f, 0.0f, 0.0f, oL, oR);
            }
            a.reset();
            b.reset();
            for (int i = 0; i < 5000; ++i) b.processSample(0.5f, -0.5f, 0.0f, 0.0f, oL, oR);
            b.reset();
            float diff = 0.0f;
            for (int i = 0; i < 5000; ++i) {
                float aL, aR, bL, bR;
                a.processSample(0.3f, -0.3f, 0.0f, 0.0f, aL, aR);
                b.processSample(0.3f, -0.3f, 0.0f, 0.0f, bL, bR);
                diff = std::max(diff, std::abs(aL - bL));
            }
            std::cout << "    FdnReverbTank reset diff: " << diff << "\n";
        }

        // Test ManifoldDelayNetwork directly
        {
            rb26::ManifoldDelayNetwork a, b;
            a.prepare(48000.0, 2.0f);
            b.prepare(48000.0, 2.0f);
            a.setParameters(rb26::ManifoldType::PoincareHyperbolic, 1.0f, 8000.0f);
            b.setParameters(rb26::ManifoldType::PoincareHyperbolic, 1.0f, 8000.0f);
            std::array<float, 8> exc {}, out;
            for (int i = 0; i < 5000; ++i) {
                std::array<float, 8> in;
                in.fill(0.5f);
                b.writeFeedback(in);
                b.readAndFilterLines(exc, out, 0.0f);
            }
            b.reset();
            float diff = 0.0f;
            for (int i = 0; i < 500; ++i) {
                std::array<float, 8> in;
                in.fill(0.3f);
                a.writeFeedback(in);
                b.writeFeedback(in);
                std::array<float, 8> aOut, bOut;
                a.readAndFilterLines(exc, aOut, 0.0f);
                b.readAndFilterLines(exc, bOut, 0.0f);
                for (int k = 0; k < 8; ++k) diff = std::max(diff, std::abs(aOut[k] - bOut[k]));
            }
            std::cout << "    ManifoldDelayNetwork reset diff: " << diff << "\n";
        }

        // Test Full Rb26ReverbEngine
        {
            rb26::Rb26ReverbEngine engA, engB;
            engA.prepare(48000.0, 256);
            engB.prepare(48000.0, 256);

            rb26::Rb26Parameters p;
            p.roomSize = 1.0f; // default room size
            p.decayRt60Sec = 2.5f;
            p.dryWetMix = 0.50f;
            p.preDelayMs = 25.0f;
            p.shimmerSend = 0.0f;
            p.dimmerSend = 0.0f;
            engA.setParameters(p);
            engB.setParameters(p);

            std::mt19937 rng(42);
            std::uniform_real_distribution<float> dist(-0.8f, 0.8f);
            const int bs = 256;
            std::vector<float> noiseL(bs), noiseR(bs), dummyL(bs), dummyR(bs);
            const float* noisePtrs[2] = { noiseL.data(), noiseR.data() };
            float* dummyPtrs[2] = { dummyL.data(), dummyR.data() };

            // Let both engines settle initial smoother targets
            for (int i = 0; i < 20; ++i) {
                for (int s = 0; s < bs; ++s) {
                    noiseL[s] = dist(rng);
                    noiseR[s] = dist(rng);
                }
                engA.process(noisePtrs, dummyPtrs, 2, bs);
                engB.process(noisePtrs, dummyPtrs, 2, bs);
            }
            engA.reset();
            engB.reset();

            // Run engB on noise for 50,000 samples

            for (int blk = 0; blk < 200; ++blk) {
                for (int i = 0; i < bs; ++i) {
                    noiseL[i] = dist(rng);
                    noiseR[i] = dist(rng);
                }
                engB.process(noisePtrs, dummyPtrs, 2, bs);
            }

            // Reset engB
            engB.reset();

            // Now feed both identical signal
            std::vector<float> testInL(bs), testInR(bs);
            std::vector<float> outAL(bs), outAR(bs), outBL(bs), outBR(bs);
            const float* inPtrs[2] = { testInL.data(), testInR.data() };
            float* outPtrsA[2] = { outAL.data(), outAR.data() };
            float* outPtrsB[2] = { outBL.data(), outBR.data() };

            float maxDiff = 0.0f;
            for (int blk = 0; blk < 20; ++blk) {
                for (int i = 0; i < bs; ++i) {
                    testInL[i] = 0.3f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(blk * bs + i) / 48000.0f);
                    testInR[i] = 0.3f * std::cos(2.0f * 3.14159265f * 554.0f * static_cast<float>(blk * bs + i) / 48000.0f);
                }
                engA.process(inPtrs, outPtrsA, 2, bs);
                engB.process(inPtrs, outPtrsB, 2, bs);

                for (int i = 0; i < bs; ++i) {
                    float diffL = std::abs(outAL[i] - outBL[i]);
                    float diffR = std::abs(outAR[i] - outBR[i]);
                    if (diffL > 0.0001f || diffR > 0.0001f) {
                        if (maxDiff < 0.0001f) {
                            std::cout << "      First diff at block " << blk << ", sample " << i 
                                      << " -> A_L=" << std::setprecision(8) << outAL[i] 
                                      << ", B_L=" << outBL[i] 
                                      << " | DiffL=" << diffL 
                                      << " | A_R=" << outAR[i] 
                                      << ", B_R=" << outBR[i] 
                                      << " | DiffR=" << diffR << "\n";
                        }
                    }
                    maxDiff = std::max(maxDiff, std::max(diffL, diffR));
                }
            }
            std::cout << "    Rb26ReverbEngine reset diff: " << maxDiff << "\n";
            CH_ASSERT(maxDiff < 1.0e-5f, "Reset engine output must match freshly constructed engine within 1e-5");
        }
    }

    // 4.3: Post-reset immediate silence (zero stale tail)
    {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 128);
        rb26::Rb26Parameters p;
        p.decayRt60Sec = 10.0f;
        p.dryWetMix = 1.0f;
        engine.setParameters(p);

        const int bs = 128;
        std::vector<float> loudL(bs, 0.8f), loudR(bs, -0.8f);
        std::vector<float> outL(bs, 0.0f), outR(bs, 0.0f);
        const float* inPtrs[2] = { loudL.data(), loudR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        for (int b = 0; b < 50; ++b) engine.process(inPtrs, outPtrs, 2, bs);

        // Reset
        engine.reset();

        // Feed silence
        std::fill(loudL.begin(), loudL.end(), 0.0f);
        std::fill(loudR.begin(), loudR.end(), 0.0f);
        engine.process(inPtrs, outPtrs, 2, bs);

        float peakPostReset = calcPeak(outL.data(), bs);
        std::cout << "  [4.3] Post-Reset Silent Block Peak: " << peakPostReset << "\n";
        CH_ASSERT(peakPostReset == 0.0f, "Post-reset silence block output must be exact 0.0f");
    }

    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// SUITE 5: Delay Read Hermite Interpolation Smoothness
// ============================================================================
bool testHermiteDelayInterpolation() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M2 Challenge 5] Hermite Delay Interpolation & Parameter Modulation\n";
    std::cout << "============================================================\n";

    rb26::Rb26ReverbEngine engine;
    engine.prepare(48000.0, 128);

    rb26::Rb26Parameters p;
    p.dryWetMix = 1.0f;
    p.decayRt60Sec = 0.1f; // Short decay so pre-delay dominates
    p.preDelayMs = 5.0f;
    engine.setParameters(p);

    const int bs = 128;
    std::vector<float> inL(bs, 0.0f), inR(bs, 0.0f);
    std::vector<float> outL(bs, 0.0f), outR(bs, 0.0f);
    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    float prevSample = 0.0f;
    float maxDelta = 0.0f;

    // Continuous sweep of preDelayMs from 5.0 ms to 100.0 ms while feeding continuous sine
    for (int b = 0; b < 100; ++b) {
        float sweepMs = 5.0f + (95.0f * static_cast<float>(b) / 100.0f);
        p.preDelayMs = sweepMs;
        engine.setParameters(p);

        for (int i = 0; i < bs; ++i) {
            inL[i] = 0.4f * std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(b * bs + i) / 48000.0f);
            inR[i] = inL[i];
        }

        engine.process(inPtrs, outPtrs, 2, bs);

        for (int i = 0; i < bs; ++i) {
            float delta = std::abs(outL[i] - prevSample);
            maxDelta = std::max(maxDelta, delta);
            prevSample = outL[i];
        }
    }

    std::cout << "  Pre-Delay Sweep Max Sample Delta: " << maxDelta << "\n";
    CH_ASSERT(maxDelta < 0.40f, "Hermite interpolated pre-delay reads must avoid discrete sample pointer jumps");

    std::cout << "  Result: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

} // namespace challenger

// ============================================================================
// Main Runner Entry Point
// ============================================================================
int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    std::cout << "================================================================\n";
    std::cout << "   BRAUN RB-26 — CHALLENGER M2-1 EMPIRICAL VERIFICATION SUITE   \n";
    std::cout << "   Auditing: Freeze Hold, Unipolar Bloom, Punch Ducking, Resets\n";
    std::cout << "================================================================\n" << std::flush;

    bool pass1 = challenger::testFreezeEnergyRecirculation();
    std::cout << std::flush;
    bool pass2 = challenger::testTailBloomUnipolarExcursions();
    std::cout << std::flush;
    bool pass3 = challenger::testPunchDuckingDynamics();
    std::cout << std::flush;
    bool pass4 = challenger::testResetInvarianceAndMemorySafety();
    std::cout << std::flush;
    bool pass5 = challenger::testHermiteDelayInterpolation();
    std::cout << std::flush;

    bool allPass = pass1 && pass2 && pass3 && pass4 && pass5 && (challenger::gFailedAssertions == 0);

    std::cout << "\n================================================================\n";
    std::cout << "           CHALLENGER M2-1 EMPIRICAL VERIFICATION SUMMARY       \n";
    std::cout << "================================================================\n";
    std::cout << "  [Challenge 1] Freeze Hold Recirculation & Damping Bypass : " << (pass1 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Challenge 2] Tail Bloom Unipolar Excursions             : " << (pass2 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Challenge 3] Punch Ducking Dynamics & Bass Transparency: " << (pass3 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Challenge 4] Reset Invariance & Pre-Prepare Safety      : " << (pass4 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Challenge 5] Hermite Delay Interpolation Smoothness     : " << (pass5 ? "PASS" : "FAIL") << "\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "  Total Assertions Checked : " << challenger::gTotalAssertions << "\n";
    std::cout << "  Failed Assertions        : " << challenger::gFailedAssertions << "\n";
    std::cout << "================================================================\n";
    std::cout << "  OVERALL EMPIRICAL VERDICT: " << (allPass ? "APPROVE" : "REJECT") << "\n";
    std::cout << "================================================================\n" << std::flush;

    return allPass ? 0 : 1;
}
