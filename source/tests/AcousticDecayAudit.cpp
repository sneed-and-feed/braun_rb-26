/**
 * ============================================================================
 * BRAUN RB-26 Comprehensive Acoustic Decay Audit & Artifact Regression Suite
 * ============================================================================
 *
 * ISO C++20 Standards Compliant, Hard Real-Time Allocation Safe
 * Multi-Manifold Geometric Grid Sweep & Numerical Stability Verification
 *
 * Tests:
 *   a) Householder Freeze DC Bleed & Limit Cycle Test (30s hold, < -110 dBFS DC)
 *   b) Sub-Band Energy Decay & High-Frequency Absorption Monotonicity
 *   c) Late-Tail Silence & Limit Cycle Absence (RT60 = 2.0s, 10s-20s < -100 dBFS)
 *   d) Spectral Crest Factor / Ringing Mode Audit (no rogue spikes > 20 dB)
 *   e) C1 Smooth Saturation Boundedness (+40 dBFS impulse, bounded <= 1.05)
 *   f) 64-Point Parameter Grid Sweep (4 Manifolds x 4 Room Sizes x 4 Dampings)
 *   g) Whispering Gallery High-Damping / Long Tail Parametric Stability
 *   h) Mid-Tail Manifold Switching Click-Free Continuity Audit
 * ============================================================================
 */

#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <complex>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <string>
#include <limits>
#include <cstdlib>
#include <cstdint>

// Core DSP Headers
#include "DspMath.h"
#include "BoundedSaturator.h"
#include "FdnReverbTank.h"
#include "ManifoldDelayNetwork.h"

// ============================================================================
// Real-Time Heap Allocation Tracking
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

namespace audit {

static int gFailedAssertions = 0;
static int gTotalAssertions = 0;

#define AUDIT_ASSERT(cond, msg) do { \
    ++audit::gTotalAssertions; \
    if (!(cond)) { \
        std::cerr << "  [AUDIT FAILED] Line " << __LINE__ << ": " << (msg) << "\n"; \
        ++audit::gFailedAssertions; \
        ++localFailures; \
    } \
} while (0)

#define AUDIT_ASSERT_NEAR(val, exp, tol, msg) do { \
    ++audit::gTotalAssertions; \
    double v_ = static_cast<double>(val); \
    double e_ = static_cast<double>(exp); \
    double diff_ = std::abs(v_ - e_); \
    if (diff_ > (tol) || std::isnan(v_) || std::isinf(v_)) { \
        std::cerr << "  [AUDIT FAILED] Line " << __LINE__ << ": " << (msg) \
                  << " (actual: " << v_ << ", expected: " << e_ << ", diff: " << diff_ << ", tol: " << (tol) << ")\n"; \
        ++audit::gFailedAssertions; \
        ++localFailures; \
    } \
} while (0)

// Helper: Radix-2 Cooley-Tukey FFT
inline void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * rb26::kPi / len;
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j) {
                std::complex<double> u = a[i + j];
                std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

// Helper: Calculate RMS of buffer slice
inline double computeRMS(const float* data, size_t count) {
    if (count == 0) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    }
    return std::sqrt(sum / static_cast<double>(count));
}

inline const char* getManifoldName(rb26::ManifoldType m) {
    switch (m) {
        case rb26::ManifoldType::PoincareHyperbolic:  return "PoincareHyperbolic";
        case rb26::ManifoldType::WhisperingGallery:   return "WhisperingGallery";
        case rb26::ManifoldType::AnharmonicPlate:     return "AnharmonicPlate";
        case rb26::ManifoldType::StockhausenKlangdom: return "StockhausenKlangdom";
    }
    return "Unknown";
}

// 4 Manifolds for sweep
static constexpr std::array<rb26::ManifoldType, 4> kManifolds = {{
    rb26::ManifoldType::PoincareHyperbolic,
    rb26::ManifoldType::WhisperingGallery,
    rb26::ManifoldType::AnharmonicPlate,
    rb26::ManifoldType::StockhausenKlangdom
}};

// Room sizes for sweep
static constexpr std::array<float, 4> kRoomSizes = {{ 0.2f, 0.8f, 1.5f, 3.5f }};

// High damping cutoffs for sweep
static constexpr std::array<float, 4> kDampingCutoffs = {{ 1000.0f, 3000.0f, 8000.0f, 18000.0f }};

// ============================================================================
// TEST A: Householder Freeze DC Bleed & Limit Cycle Test
// Energize FdnReverbTank with high-amplitude Dirac pulse and square burst,
// latch freezeHold = true, run for 30 seconds (1.44M samples at 48k).
// Verify:
//   1. Mean DC offset strictly < -110 dBFS.
//   2. Output bounded <= 1.05f with zero NaNs, zero Infs, zero denormals.
//   3. Steady sustain without diverging or decaying to silence.
// ============================================================================
bool testHouseholderFreezeDcBleed() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[Audit Suite A] Householder Freeze DC Bleed & Limit Cycle Test\n";
    std::cout << "============================================================\n";

    constexpr double kFs = 48000.0;
    constexpr size_t kFreezeSamples = 1440000; // 30 seconds at 48 kHz

    for (size_t mIdx = 0; mIdx < kManifolds.size(); ++mIdx) {
        rb26::ManifoldType manifold = kManifolds[mIdx];
        rb26::FdnReverbTank tank;
        tank.prepare(kFs, 4.0f);
        tank.setParameters(1.0f, 3.0f, 8000.0f, 0.75f, false, 0.65f, 2.25f, 85.0f, manifold);
        tank.reset();

        float outL = 0.0f, outR = 0.0f;

        // 1. Energize with high-amplitude Dirac pulse (+0.95f) with balanced anti-phase
        tank.processSample(0.95f, -0.95f, 0.0f, 0.0f, outL, outR);

        // 2. Add square burst (amplitude 0.8f, 1000 Hz: period = 48 samples, 10 cycles = 480 samples, zero DC mean)
        for (int i = 0; i < 480; ++i) {
            float sq = (i % 48 < 24) ? 0.8f : -0.8f;
            tank.processSample(sq, -sq, 0.0f, 0.0f, outL, outR);
        }

        // 3. Allow initial reflections to diffuse and equalize across 8 lines for 4800 samples (100 ms)
        for (int i = 0; i < 4800; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        }

        // 4. Latch freezeHold = true
        tank.setParameters(1.0f, 30.0f, 8000.0f, 0.75f, true, 0.65f, 2.25f, 85.0f, manifold);

        // Run 30 seconds of freeze hold while auditing DC bleed, boundedness, denormals, and stability
        double sumL = 0.0;
        double sumR = 0.0;
        float peakL = 0.0f;
        float peakR = 0.0f;
        size_t nanCount = 0;
        size_t infCount = 0;
        size_t denormalCount = 0;

        double sumSqEarly = 0.0; // 1s - 2s window
        double sumSqMid   = 0.0; // 15s - 16s window
        double sumSqLate  = 0.0; // 29s - 30s window
        constexpr size_t kWinSize = 48000;

        gAllocationCount = 0;
        gTrackAllocations = true;

        for (size_t n = 0; n < kFreezeSamples; ++n) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);

            if (std::isnan(outL) || std::isnan(outR)) ++nanCount;
            if (std::isinf(outL) || std::isinf(outR)) ++infCount;

            // Denormal check
            if ((outL != 0.0f && std::abs(outL) < std::numeric_limits<float>::min()) ||
                (outR != 0.0f && std::abs(outR) < std::numeric_limits<float>::min())) {
                ++denormalCount;
            }

            sumL += static_cast<double>(outL);
            sumR += static_cast<double>(outR);

            peakL = std::max(peakL, std::abs(outL));
            peakR = std::max(peakR, std::abs(outR));

            // Windowed energy checks
            if (n >= 48000 && n < 48000 + kWinSize) {
                sumSqEarly += static_cast<double>(outL * outL + outR * outR);
            } else if (n >= 720000 && n < 720000 + kWinSize) {
                sumSqMid += static_cast<double>(outL * outL + outR * outR);
            } else if (n >= kFreezeSamples - kWinSize) {
                sumSqLate += static_cast<double>(outL * outL + outR * outR);
            }
        }

        gTrackAllocations = false;

        const double meanDcL = sumL / static_cast<double>(kFreezeSamples);
        const double meanDcR = sumR / static_cast<double>(kFreezeSamples);
        const double maxMeanDc = std::max(std::abs(meanDcL), std::abs(meanDcR));
        const double dcDbFS = (maxMeanDc > 1.0e-15) ? (20.0 * std::log10(maxMeanDc)) : -300.0;

        const double rmsEarly = std::sqrt(sumSqEarly / (2.0 * static_cast<double>(kWinSize)));
        const double rmsMid   = std::sqrt(sumSqMid   / (2.0 * static_cast<double>(kWinSize)));
        const double rmsLate  = std::sqrt(sumSqLate  / (2.0 * static_cast<double>(kWinSize)));

        const float maxPeak = std::max(peakL, peakR);

        std::cout << "  [A." << (mIdx + 1) << "] Manifold: " << getManifoldName(manifold) << "\n"
                  << "        DC Offset: " << std::scientific << std::setprecision(3) << maxMeanDc
                  << " (" << std::fixed << std::setprecision(2) << dcDbFS << " dBFS, limit < -110.0 dBFS)\n"
                  << "        Max Peak: " << std::setprecision(4) << maxPeak << " (bound <= 1.05)\n"
                  << "        RMS Energy: Early(1s)=" << std::setprecision(4) << rmsEarly
                  << ", Mid(15s)=" << rmsMid << ", Late(30s)=" << rmsLate << "\n"
                  << "        Artifacts: NaNs=" << nanCount << ", Infs=" << infCount
                  << ", Denormals=" << denormalCount << ", RT Allocs=" << gAllocationCount << "\n";

        AUDIT_ASSERT(dcDbFS < -110.0, "Mean DC offset must be strictly < -110 dBFS");
        AUDIT_ASSERT(maxPeak <= 1.05001f, "Output must remain bounded <= 1.05f");
        AUDIT_ASSERT(nanCount == 0, "Zero NaNs allowed in 30-second freeze hold");
        AUDIT_ASSERT(infCount == 0, "Zero Infs allowed in 30-second freeze hold");
        AUDIT_ASSERT(denormalCount == 0, "Zero denormals allowed in 30-second freeze hold");
        AUDIT_ASSERT(gAllocationCount == 0, "Hard real-time audio thread must have 0 dynamic allocations");
        AUDIT_ASSERT(rmsLate > 0.001, "Loop energy must sustain without decaying to silence");
        AUDIT_ASSERT(rmsLate <= 1.05, "Loop energy must not diverge");
        AUDIT_ASSERT(rmsLate >= rmsEarly * 0.40, "Loop sustain must maintain steady energy over 30s");
    }

    std::cout << "  Suite A Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST B: Sub-Band Energy Decay & High-Frequency Absorption Monotonicity
// Render 10-second impulse response. Compute decay across octave bands (500 Hz, 2 kHz, 8 kHz).
// Verify higher frequency bands decay strictly faster than lower frequency bands when damping < 10 kHz.
// ============================================================================
bool testSubBandEnergyDecayMonotonicity() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[Audit Suite B] Sub-Band Energy Decay & Absorption Monotonicity\n";
    std::cout << "============================================================\n";

    constexpr double kFs = 48000.0;
    constexpr size_t kIrSamples = 480000; // 10 seconds

    // Test across damping cutoffs < 10 kHz: 1000 Hz, 3000 Hz, 8000 Hz
    const std::array<float, 3> activeDampings = {{ 1000.0f, 3000.0f, 8000.0f }};

    int subIdx = 0;
    for (rb26::ManifoldType manifold : kManifolds) {
        for (float dampingHz : activeDampings) {
            ++subIdx;
            rb26::FdnReverbTank tank;
            tank.prepare(kFs, 4.0f);
            tank.setParameters(1.0f, 3.0f, dampingHz, 0.75f, false, 0.65f, 2.25f, 85.0f, manifold);
            tank.reset();

            std::vector<float> irL(kIrSamples);
            std::vector<float> irR(kIrSamples);

            float outL = 0.0f, outR = 0.0f;
            // Impulse excitation
            tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);
            irL[0] = outL;
            irR[0] = outR;

            for (size_t i = 1; i < kIrSamples; ++i) {
                tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
                irL[i] = outL;
                irR[i] = outR;
            }

            // Filter IR into octave bands: 500 Hz, 2000 Hz, 8000 Hz using 2nd order bandpass biquads
            rb26::Biquad bp500, bp2k, bp8k;
            bp500.configure(rb26::Biquad::Type::Bandpass, static_cast<float>(kFs), 500.0f, 1.0f);
            bp2k.configure(rb26::Biquad::Type::Bandpass, static_cast<float>(kFs), 2000.0f, 1.0f);
            bp8k.configure(rb26::Biquad::Type::Bandpass, static_cast<float>(kFs), 8000.0f, 1.0f);

            std::vector<float> f500(kIrSamples);
            std::vector<float> f2k(kIrSamples);
            std::vector<float> f8k(kIrSamples);

            for (size_t i = 0; i < kIrSamples; ++i) {
                const float mono = 0.5f * (irL[i] + irR[i]);
                f500[i] = bp500.process(mono);
                f2k[i]  = bp2k.process(mono);
                f8k[i]  = bp8k.process(mono);
            }

            // Measure early energy (0.05s - 0.15s: samples 2400 to 7200)
            // vs late energy (0.35s - 0.55s: samples 16800 to 26400)
            constexpr size_t kEarlyStart = 2400;
            constexpr size_t kEarlyCount = 4800; // 100 ms
            constexpr size_t kLateStart  = 16800;
            constexpr size_t kLateCount  = 9600; // 200 ms

            const double rmsEarly500 = computeRMS(f500.data() + kEarlyStart, kEarlyCount);
            const double rmsEarly2k  = computeRMS(f2k.data()  + kEarlyStart, kEarlyCount);
            const double rmsEarly8k  = computeRMS(f8k.data()  + kEarlyStart, kEarlyCount);

            const double rmsLate500  = computeRMS(f500.data() + kLateStart, kLateCount);
            const double rmsLate2k   = computeRMS(f2k.data()  + kLateStart, kLateCount);
            const double rmsLate8k   = computeRMS(f8k.data()  + kLateStart, kLateCount);

            // Compute decay amount in dB: Delta_dB = 20 * log10(RMS_early / RMS_late)
            const double decay500Db = 20.0 * std::log10(std::max(1.0e-9, rmsEarly500) / std::max(1.0e-9, rmsLate500));
            const double decay2kDb  = 20.0 * std::log10(std::max(1.0e-9, rmsEarly2k)  / std::max(1.0e-9, rmsLate2k));
            const double decay8kDb  = 20.0 * std::log10(std::max(1.0e-9, rmsEarly8k)  / std::max(1.0e-9, rmsLate8k));

            std::cout << "  [B." << subIdx << "] " << getManifoldName(manifold)
                      << " (Damp=" << static_cast<int>(dampingHz) << " Hz):\n"
                      << "        Decay 500 Hz: " << std::fixed << std::setprecision(2) << decay500Db << " dB\n"
                      << "        Decay 2 kHz : " << std::fixed << std::setprecision(2) << decay2kDb  << " dB\n"
                      << "        Decay 8 kHz : " << std::fixed << std::setprecision(2) << decay8kDb  << " dB\n";

            AUDIT_ASSERT(decay2kDb > decay500Db,
                "2 kHz band must decay strictly faster than 500 Hz band under HF damping");
            AUDIT_ASSERT(decay8kDb > decay2kDb,
                "8 kHz band must decay strictly faster than 2 kHz band under HF damping");
        }
    }

    std::cout << "  Suite B Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST C: Late-Tail Silence & Limit Cycle Absence
// For nominal RT60 = 2.0s and freezeHold = false, render for 20 seconds.
// Verify samples between 10s and 20s have decayed below -100 dBFS and energy
// slope is non-positive (no spontaneous limit cycles or self-oscillation).
// ============================================================================
bool testLateTailSilenceAndLimitCycleAbsence() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[Audit Suite C] Late-Tail Silence & Limit Cycle Absence\n";
    std::cout << "============================================================\n";

    constexpr double kFs = 48000.0;
    constexpr size_t kTotalSamples = 960000;  // 20 seconds
    constexpr size_t kTenSecStart  = 480000;  // 10 seconds

    int subIdx = 0;
    for (rb26::ManifoldType manifold : kManifolds) {
        for (float roomSize : kRoomSizes) {
            ++subIdx;
            rb26::FdnReverbTank tank;
            tank.prepare(kFs, 4.0f);
            tank.setParameters(roomSize, 2.0f, 8000.0f, 0.75f, false, 0.65f, 2.25f, 85.0f, manifold);
            tank.reset();

            float outL = 0.0f, outR = 0.0f;
            // Inject impulse
            tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);

            float peakBetween10And20 = 0.0f;
            std::array<double, 10> windowRms {};

            for (size_t i = 1; i < kTotalSamples; ++i) {
                tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);

                if (i >= kTenSecStart) {
                    peakBetween10And20 = std::max(peakBetween10And20, std::max(std::abs(outL), std::abs(outR)));
                    const size_t secIdx = (i - kTenSecStart) / 48000;
                    if (secIdx < 10) {
                        windowRms[secIdx] += static_cast<double>(outL * outL + outR * outR);
                    }
                }
            }

            for (size_t s = 0; s < 10; ++s) {
                windowRms[s] = std::sqrt(windowRms[s] / (2.0 * 48000.0));
            }

            const double peakDb = (peakBetween10And20 > 1.0e-15f) ? (20.0 * std::log10(peakBetween10And20)) : -300.0;

            bool nonPositiveSlope = true;
            for (size_t s = 0; s < 9; ++s) {
                // RMS must be monotonically non-increasing (within small epsilon for noise floor)
                if (windowRms[s + 1] > windowRms[s] + 1.0e-7) {
                    nonPositiveSlope = false;
                }
            }

            std::cout << "  [C." << subIdx << "] " << getManifoldName(manifold)
                      << " (Room=" << roomSize << "):\n"
                      << "        Peak (10s-20s): " << std::fixed << std::setprecision(2) << peakDb
                      << " dBFS (limit < -100 dBFS)\n"
                      << "        Energy Slope Non-Positive: " << (nonPositiveSlope ? "TRUE" : "FALSE")
                      << " (RMS@10s=" << std::scientific << std::setprecision(2) << windowRms[0]
                      << ", RMS@20s=" << windowRms[9] << ")\n";

            AUDIT_ASSERT(peakDb < -100.0, "Tail samples between 10s and 20s must decay below -100 dBFS");
            AUDIT_ASSERT(nonPositiveSlope, "Tail energy slope must be non-positive with zero limit cycles");
        }
    }

    std::cout << "  Suite C Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST D: Spectral Crest Factor / Ringing Mode Audit
// Compute spectral peak-to-average / crest factor across reverberant tail.
// Verify that no isolated eigenvalue produces a rogue resonant spike (> 20 dB above neighborhood).
// ============================================================================
bool testSpectralCrestFactorAndRingingModes() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[Audit Suite D] Spectral Crest Factor & Ringing Mode Audit\n";
    std::cout << "============================================================\n";

    constexpr double kFs = 48000.0;
    constexpr size_t kN = 16384; // FFT length (16384 points = ~341 ms window, df = 2.93 Hz)

    int subIdx = 0;
    for (rb26::ManifoldType manifold : kManifolds) {
        ++subIdx;
        rb26::FdnReverbTank tank;
        tank.prepare(kFs, 4.0f);
        tank.setParameters(1.0f, 3.0f, 8000.0f, 0.75f, false, 0.65f, 2.25f, 85.0f, manifold);
        tank.reset();

        float outL = 0.0f, outR = 0.0f;
        tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, outL, outR);

        // Pre-roll into reverberant tail (skip initial direct transient: 2400 samples = 50 ms)
        for (size_t i = 1; i < 2400; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
        }

        // Collect tail slice for FFT
        std::vector<std::complex<double>> spectrum(kN);
        for (size_t i = 0; i < kN; ++i) {
            tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
            const float val = 0.5f * (outL + outR);
            // Hann window
            const double win = 0.5 * (1.0 - std::cos(2.0 * rb26::kPi * static_cast<double>(i) / (kN - 1)));
            spectrum[i] = std::complex<double>(static_cast<double>(val) * win, 0.0);
        }

        fft(spectrum);

        // Magnitude spectrum in audio band [200 Hz, 12 kHz]
        const double binWidth = kFs / static_cast<double>(kN);
        const size_t minBin = static_cast<size_t>(200.0 / binWidth);
        const size_t maxBin = static_cast<size_t>(12000.0 / binWidth);

        std::vector<double> mag(maxBin + 1, 0.0);
        double maxMag = 0.0;
        double sumMag = 0.0;

        for (size_t k = minBin; k <= maxBin; ++k) {
            mag[k] = std::abs(spectrum[k]);
            maxMag = std::max(maxMag, mag[k]);
            sumMag += mag[k];
        }

        const double meanMag = sumMag / static_cast<double>(maxBin - minBin + 1);
        const double crestFactorDb = 20.0 * std::log10(maxMag / std::max(1.0e-12, meanMag));

        // Audit local neighborhood peak prominence (check for isolated resonant spikes)
        constexpr int kRadius = 32; // ~94 Hz neighborhood
        double maxSpikeDb = 0.0;
        size_t spikeBin = 0;

        for (size_t k = minBin + kRadius; k <= maxBin - kRadius; ++k) {
            double localSum = 0.0;
            int localCount = 0;
            for (int j = -kRadius; j <= kRadius; ++j) {
                if (std::abs(j) > 3) { // Exclude bin and immediately adjacent spectral leakage bins
                    localSum += mag[k + j];
                    ++localCount;
                }
            }
            const double localAvg = localSum / std::max(1, localCount);
            const double spikeDb = 20.0 * std::log10(mag[k] / std::max(1.0e-12, localAvg));
            if (spikeDb > maxSpikeDb) {
                maxSpikeDb = spikeDb;
                spikeBin = k;
            }
        }

        std::cout << "  [D." << subIdx << "] " << getManifoldName(manifold) << ":\n"
                  << "        Spectral Crest Factor: " << std::fixed << std::setprecision(2) << crestFactorDb << " dB\n"
                  << "        Max Rogue Spike: " << maxSpikeDb << " dB at "
                  << static_cast<int>(spikeBin * binWidth) << " Hz (limit < 20.0 dB)\n";

        AUDIT_ASSERT(maxSpikeDb < 20.0, "No isolated eigenvalue may produce a rogue resonant spike > 20 dB above neighborhood");
        AUDIT_ASSERT(crestFactorDb < 28.0, "Spectral crest factor across reverberant tail must remain bounded");
    }

    std::cout << "  Suite D Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST E: C1 Smooth Saturation Boundedness
// Feed +40 dBFS impulse into tank in both normal and freeze modes.
// Verify zero slope blowups and maximum output strictly bounded <= 1.05.
// ============================================================================
bool testC1SaturationBoundedness() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[Audit Suite E] C1 Smooth Saturation Boundedness (+40 dBFS)\n";
    std::cout << "============================================================\n";

    constexpr double kFs = 48000.0;
    constexpr float kExtremeAmp = 100.0f; // +40 dBFS (10^(40/20) = 100.0)
    constexpr size_t kTestSamples = 96000; // 2 seconds

    // E.1: Mathematical C1 smoothness and derivative audit of BoundedSaturator
    {
        rb26::BoundedSaturator sat(0.72f, 1.05f);
        constexpr size_t kPoints = 1000000;
        constexpr float xMin = -100.0f;
        constexpr float xMax = 100.0f;
        constexpr float dx = (xMax - xMin) / static_cast<float>(kPoints);

        float maxVal = 0.0f;
        float maxSlope = 0.0f;
        float prevY = sat.processSample(xMin);

        for (size_t i = 1; i <= kPoints; ++i) {
            const float x = xMin + static_cast<float>(i) * dx;
            const float y = sat.processSample(x);

            maxVal = std::max(maxVal, std::abs(y));
            const float slope = std::abs(y - prevY) / dx;
            maxSlope = std::max(maxSlope, slope);
            prevY = y;
        }

        std::cout << "  [E.1] BoundedSaturator Transfer Curve Audit [-100, +100]:\n"
                  << "        Max Absolute Output: " << std::fixed << std::setprecision(4) << maxVal
                  << " (bound <= 1.05)\n"
                  << "        Max Derivative dy/dx: " << maxSlope << " (bound <= 1.50, zero blowups)\n";

        AUDIT_ASSERT(maxVal <= 1.05001f, "Saturator output must be strictly bounded <= 1.05f");
        AUDIT_ASSERT(maxSlope <= 1.50f, "Saturator slope must remain strictly bounded (zero slope blowups)");
    }

    // E.2: Extreme +40 dBFS impulse into FdnReverbTank in Normal, Freeze Input, and Freeze Sustain modes
    int subIdx = 0;
    enum class OverloadScenario { Normal, FreezeInputIsolated, FreezeSustainedRecirculation };

    for (rb26::ManifoldType manifold : kManifolds) {
        for (OverloadScenario scen : { OverloadScenario::Normal,
                                       OverloadScenario::FreezeInputIsolated,
                                       OverloadScenario::FreezeSustainedRecirculation }) {
            ++subIdx;
            rb26::FdnReverbTank tank;
            tank.prepare(kFs, 4.0f);

            const bool initialFreeze = (scen == OverloadScenario::FreezeInputIsolated);
            tank.setParameters(1.0f, 3.0f, 8000.0f, 0.75f, initialFreeze, 0.65f, 2.25f, 85.0f, manifold);
            tank.reset();

            float outL = 0.0f, outR = 0.0f;
            float maxOut = 0.0f;
            float maxDeltaSample = 0.0f;
            float prevL = 0.0f, prevR = 0.0f;
            size_t nanCount = 0;
            size_t infCount = 0;

            // Feed +40 dBFS impulse
            tank.processSample(kExtremeAmp, -kExtremeAmp, 0.0f, 0.0f, outL, outR);

            for (size_t n = 0; n < kTestSamples; ++n) {
                if (scen == OverloadScenario::FreezeSustainedRecirculation && n == 100) {
                    // Latch freeze while +40 dBFS blast is recirculating inside the tank
                    tank.setParameters(1.0f, 30.0f, 8000.0f, 0.75f, true, 0.65f, 2.25f, 85.0f, manifold);
                }

                if (n > 0) {
                    tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
                }

                if (std::isnan(outL) || std::isnan(outR)) ++nanCount;
                if (std::isinf(outL) || std::isinf(outR)) ++infCount;

                maxOut = std::max(maxOut, std::max(std::abs(outL), std::abs(outR)));
                const float deltaL = std::abs(outL - prevL);
                const float deltaR = std::abs(outR - prevR);
                maxDeltaSample = std::max(maxDeltaSample, std::max(deltaL, deltaR));

                prevL = outL;
                prevR = outR;
            }

            const char* scenStr = (scen == OverloadScenario::Normal) ? "NORMAL" :
                                  (scen == OverloadScenario::FreezeInputIsolated) ? "FREEZE_ISOLATED" :
                                  "FREEZE_SUSTAINED";

            std::cout << "  [E." << (subIdx + 1) << "] " << getManifoldName(manifold)
                      << " (" << scenStr << "):\n"
                      << "        Max Output: " << std::fixed << std::setprecision(4) << maxOut
                      << " (bound <= 1.05)\n"
                      << "        Max Sample Jump: " << maxDeltaSample << " (bounded, zero slope blowup)\n"
                      << "        Artifacts: NaNs=" << nanCount << ", Infs=" << infCount << "\n";

            AUDIT_ASSERT(maxOut <= 1.05001f, "Tank output under +40 dBFS blast must remain bounded <= 1.05");
            AUDIT_ASSERT(nanCount == 0, "Zero NaNs under +40 dBFS impulse");
            AUDIT_ASSERT(infCount == 0, "Zero Infs under +40 dBFS impulse");
            AUDIT_ASSERT(maxDeltaSample <= 2.1001f, "Sample difference must be strictly bounded");
        }
    }

    std::cout << "  Suite E Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST F: 64-Point Geometric & Acoustic Parameter Grid Sweep
// 4 Manifolds x 4 Room Sizes x 4 High Damping Cutoffs = 64 Configurations
// Verifies full parameter space stability, bounded output, and real-time safety.
// ============================================================================
bool testGeometricGridSweep() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[Audit Suite F] 64-Point Multi-Manifold Geometric Grid Sweep\n";
    std::cout << "============================================================\n";

    constexpr double kFs = 48000.0;
    constexpr size_t kRenderSamples = 24000; // 0.5 sec per point

    int passedConfigs = 0;

    for (rb26::ManifoldType manifold : kManifolds) {
        for (float roomSize : kRoomSizes) {
            for (float dampingHz : kDampingCutoffs) {
                rb26::FdnReverbTank tank;
                tank.prepare(kFs, 4.0f);
                tank.setParameters(roomSize, 2.5f, dampingHz, 0.75f, false, 0.65f, 2.25f, 85.0f, manifold);
                tank.reset();

                float outL = 0.0f, outR = 0.0f;
                float peak = 0.0f;
                size_t nanCount = 0;
                size_t infCount = 0;
                size_t denormalCount = 0;

                // Energize with impulse
                tank.processSample(0.9f, 0.9f, 0.0f, 0.0f, outL, outR);

                for (size_t n = 1; n < kRenderSamples; ++n) {
                    tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);
                    if (std::isnan(outL) || std::isnan(outR)) ++nanCount;
                    if (std::isinf(outL) || std::isinf(outR)) ++infCount;
                    if ((outL != 0.0f && std::abs(outL) < std::numeric_limits<float>::min()) ||
                        (outR != 0.0f && std::abs(outR) < std::numeric_limits<float>::min())) {
                        ++denormalCount;
                    }
                    peak = std::max(peak, std::max(std::abs(outL), std::abs(outR)));
                }

                if (peak <= 1.05001f && peak > 0.0001f && nanCount == 0 && infCount == 0 && denormalCount == 0) {
                    ++passedConfigs;
                } else {
                    std::cerr << "  [GRID CONFIG FAILED] Manifold=" << getManifoldName(manifold)
                              << " Room=" << roomSize << " Damp=" << dampingHz << " Peak=" << peak << "\n";
                    ++localFailures;
                }
            }
        }
    }

    std::cout << "  Passed Configurations: " << passedConfigs << " / 64\n";
    AUDIT_ASSERT(passedConfigs == 64, "All 64 geometric grid configurations must pass 100%");

    std::cout << "  Suite F Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST G: Whispering Gallery High-Damping / Long Tail Parametric Stability
// Test FdnReverbTank with Whispering Gallery manifold, decayRt60Sec = 18.0f and 30.0f,
// highDampingHz = 15000.0f and 20000.0f. Energize with a high-amplitude noise burst.
// Run for 20 seconds. Assert that late tail samples (10s-20s) decay monotonically,
// peak <= -60 dBFS at 20s, and no infinite RT60 or runaway resonant tone at 9.5 kHz exists.
// ============================================================================
bool testWhisperingGalleryParametricStability() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[Audit Suite G] Whispering Gallery High-Damping / Long Tail Stability\n";
    std::cout << "============================================================\n";

    constexpr double kFs = 48000.0;
    constexpr size_t kTotalSamples = 960000;      // 20 seconds
    constexpr size_t kTenSecStart  = 480000;      // 10 seconds
    constexpr size_t kNineteenSecStart = 912000;  // 19 seconds (window at 20s mark)
    constexpr size_t kBurstSamples = 4800;        // 100 ms noise burst

    const std::array<float, 2> testRt60s = {{ 18.0f, 30.0f }};
    const std::array<float, 2> testDampings = {{ 15000.0f, 20000.0f }};

    int subIdx = 0;
    for (float rt60 : testRt60s) {
        for (float dampingHz : testDampings) {
            ++subIdx;
            rb26::FdnReverbTank tank;
            tank.prepare(kFs, 4.0f);
            tank.setParameters(1.0f, rt60, dampingHz, 0.75f, false, 0.65f, 2.25f, 85.0f,
                               rb26::ManifoldType::WhisperingGallery);
            tank.reset();

            // 9.5 kHz Biquad bandpass filter to detect rogue resonant peaking tones
            rb26::Biquad bp9500;
            bp9500.configure(rb26::Biquad::Type::Bandpass, static_cast<float>(kFs), 9500.0f, 2.8f);

            // Deterministic high-amplitude noise burst generator
            uint32_t rng = 0x5EEDCAFE + static_cast<uint32_t>(subIdx * 1013);
            auto getNoise = [&rng]() {
                rng = rng * 1664525u + 1013904223u;
                return (static_cast<float>(rng & 0x00FFFFFF) / static_cast<float>(0x007FFFFF)) - 1.0f;
            };

            float outL = 0.0f, outR = 0.0f;
            float peakAt20s = 0.0f;
            float peak9500At20s = 0.0f;
            size_t nanCount = 0;
            size_t infCount = 0;
            size_t denormalCount = 0;
            std::array<double, 10> windowEnergy {};

            // 1. Energize with 100 ms high-amplitude noise burst
            for (size_t i = 0; i < kBurstSamples; ++i) {
                const float inSampleL = 0.85f * getNoise();
                const float inSampleR = 0.85f * getNoise();
                tank.processSample(inSampleL, inSampleR, 0.0f, 0.0f, outL, outR);
                (void)bp9500.process(0.5f * (outL + outR));
            }

            // 2. Run decay for the remainder of 20 seconds
            for (size_t i = kBurstSamples; i < kTotalSamples; ++i) {
                tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);

                if (std::isnan(outL) || std::isnan(outR)) ++nanCount;
                if (std::isinf(outL) || std::isinf(outR)) ++infCount;
                if ((outL != 0.0f && std::abs(outL) < std::numeric_limits<float>::min()) ||
                    (outR != 0.0f && std::abs(outR) < std::numeric_limits<float>::min())) {
                    ++denormalCount;
                }

                const float bpOut = bp9500.process(0.5f * (outL + outR));

                if (i >= kTenSecStart) {
                    const size_t secIdx = (i - kTenSecStart) / 48000;
                    if (secIdx < 10) {
                        windowEnergy[secIdx] += static_cast<double>(outL * outL + outR * outR);
                    }
                }

                if (i >= kNineteenSecStart) {
                    peakAt20s = std::max(peakAt20s, std::max(std::abs(outL), std::abs(outR)));
                    peak9500At20s = std::max(peak9500At20s, std::abs(bpOut));
                }
            }

            std::array<double, 10> windowRms {};
            for (size_t s = 0; s < 10; ++s) {
                windowRms[s] = std::sqrt(windowEnergy[s] / (2.0 * 48000.0));
            }

            // Monotonic decay audit across 10s-20s
            bool monotonicDecay = true;
            for (size_t s = 0; s < 9; ++s) {
                if (windowRms[s + 1] > windowRms[s] + 1.0e-7) {
                    monotonicDecay = false;
                }
            }

            const double peakDb = (peakAt20s > 1.0e-15f) ? (20.0 * std::log10(peakAt20s)) : -300.0;
            const double peak9500Db = (peak9500At20s > 1.0e-15f) ? (20.0 * std::log10(peak9500At20s)) : -300.0;

            const double maxExpectedPeakDb = (rt60 <= 20.0f) ? -60.0 : (-60.0 * (20.0 / static_cast<double>(rt60)));

            std::cout << "  [G." << subIdx << "] WhisperingGallery (RT60=" << static_cast<int>(rt60)
                      << "s, Damp=" << static_cast<int>(dampingHz) << " Hz):\n"
                      << "        Monotonic Decay (10s-20s): " << (monotonicDecay ? "TRUE" : "FALSE")
                      << " (RMS@10s=" << std::scientific << std::setprecision(2) << windowRms[0]
                      << ", RMS@20s=" << windowRms[9] << ")\n"
                      << "        Peak at 20s: " << std::fixed << std::setprecision(2) << peakDb
                      << " dBFS (bound <= " << maxExpectedPeakDb << " dBFS)\n"
                      << "        Peak 9.5 kHz at 20s: " << peak9500Db << " dBFS (bound <= -60.0 dBFS)\n"
                      << "        Artifacts: NaNs=" << nanCount << ", Infs=" << infCount
                      << ", Denormals=" << denormalCount << "\n";

            AUDIT_ASSERT(monotonicDecay, "Late tail samples (10s-20s) must decay monotonically");
            AUDIT_ASSERT(peakDb <= maxExpectedPeakDb, "Tail peak at 20s must be <= max expected peak (no infinite RT60)");
            AUDIT_ASSERT(peak9500Db <= -60.0, "No runaway resonant tone at 9.5 kHz exists at 20s");
            AUDIT_ASSERT(nanCount == 0, "Zero NaNs in Whispering Gallery high-damping long tail");
            AUDIT_ASSERT(infCount == 0, "Zero Infs in Whispering Gallery high-damping long tail");
        }
    }

    std::cout << "  Suite G Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// TEST H: Mid-Tail Manifold Switching Click-Free Continuity Audit
// Energize FdnReverbTank with an impulse into a 10s tail. Mid-tail, switch
// manifolds through a continuous cycle: Poincare -> WhisperingGallery ->
// AnharmonicPlate -> WhisperingGallery -> StockhausenKlangdom -> WhisperingGallery.
// Inspect maximum sample-to-sample difference across switching points and throughout
// mid-tail to verify smooth C0 continuity (< 0.05, zero discontinuity).
// ============================================================================
bool testManifoldSwitchingContinuity() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[Audit Suite H] Mid-Tail Manifold Switching Click-Free Continuity Audit\n";
    std::cout << "============================================================\n";

    constexpr double kFs = 48000.0;
    rb26::FdnReverbTank tank;
    tank.prepare(kFs, 4.0f);
    tank.setParameters(1.0f, 10.0f, 8000.0f, 0.75f, false, 0.65f, 2.25f, 85.0f,
                       rb26::ManifoldType::PoincareHyperbolic);
    tank.reset();

    // 1. Energize with an impulse into a 10s tail
    float prevOutL = 0.0f, prevOutR = 0.0f;
    tank.processSample(1.0f, 1.0f, 0.0f, 0.0f, prevOutL, prevOutR);

    // Switching schedule: (sampleIndex, newManifold, description)
    struct SwitchEvent {
        size_t sample;
        rb26::ManifoldType manifold;
        const char* desc;
    };

    const std::vector<SwitchEvent> switchEvents = {
        { 10000, rb26::ManifoldType::WhisperingGallery,   "Poincare -> WhisperingGallery" },
        { 15000, rb26::ManifoldType::AnharmonicPlate,     "WhisperingGallery -> AnharmonicPlate" },
        { 20000, rb26::ManifoldType::WhisperingGallery,   "AnharmonicPlate -> WhisperingGallery" },
        { 25000, rb26::ManifoldType::StockhausenKlangdom, "WhisperingGallery -> StockhausenKlangdom" },
        { 30000, rb26::ManifoldType::WhisperingGallery,   "StockhausenKlangdom -> WhisperingGallery" },
        { 35000, rb26::ManifoldType::PoincareHyperbolic,  "WhisperingGallery -> PoincareHyperbolic" }
    };

    constexpr size_t kTotalSamples = 40000;
    size_t currentSwitchIdx = 0;
    float maxDeltaOverall = 0.0f;
    size_t nanCount = 0;
    size_t infCount = 0;

    for (size_t n = 1; n < kTotalSamples; ++n) {
        if (currentSwitchIdx < switchEvents.size() && n == switchEvents[currentSwitchIdx].sample) {
            tank.setManifold(switchEvents[currentSwitchIdx].manifold);
        }

        float outL = 0.0f, outR = 0.0f;
        tank.processSample(0.0f, 0.0f, 0.0f, 0.0f, outL, outR);

        if (std::isnan(outL) || std::isnan(outR)) ++nanCount;
        if (std::isinf(outL) || std::isinf(outR)) ++infCount;

        const float deltaL = std::abs(outL - prevOutL);
        const float deltaR = std::abs(outR - prevOutR);
        const float maxDelta = std::max(deltaL, deltaR);

        if (n >= 1000) {
            maxDeltaOverall = std::max(maxDeltaOverall, maxDelta);
        }

        if (currentSwitchIdx < switchEvents.size() && n == switchEvents[currentSwitchIdx].sample) {
            std::cout << "  [H." << (currentSwitchIdx + 1) << "] Switch at n=" << n
                      << " (" << switchEvents[currentSwitchIdx].desc << "):\n"
                      << "        Sample-to-Sample Jump: " << std::fixed << std::setprecision(5) << maxDelta
                      << " (limit < 0.05)\n";
            AUDIT_ASSERT(maxDelta < 0.05f, "Sample-to-sample difference at manifold switch point must be < 0.05");
            ++currentSwitchIdx;
        }

        prevOutL = outL;
        prevOutR = outR;
    }

    std::cout << "  Max Jump Throughout Switching Suite (n >= 1000): "
              << std::fixed << std::setprecision(5) << maxDeltaOverall << " (limit < 0.05)\n"
              << "  Artifacts: NaNs=" << nanCount << ", Infs=" << infCount << "\n";

    AUDIT_ASSERT(maxDeltaOverall < 0.05f, "Maximum sample difference throughout switching region must be < 0.05");
    AUDIT_ASSERT(nanCount == 0, "Zero NaNs during mid-tail manifold switching");
    AUDIT_ASSERT(infCount == 0, "Zero Infs during mid-tail manifold switching");

    std::cout << "  Suite H Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

} // namespace audit

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    std::cout << "================================================================\n";
    std::cout << "   BRAUN RB-26 — ACOUSTIC DECAY AUDIT & ARTIFACT REGRESSION     \n";
    std::cout << "================================================================\n";

    const auto startTime = std::chrono::high_resolution_clock::now();

    bool pA = audit::testHouseholderFreezeDcBleed();
    bool pB = audit::testSubBandEnergyDecayMonotonicity();
    bool pC = audit::testLateTailSilenceAndLimitCycleAbsence();
    bool pD = audit::testSpectralCrestFactorAndRingingModes();
    bool pE = audit::testC1SaturationBoundedness();
    bool pF = audit::testGeometricGridSweep();
    bool pG = audit::testWhisperingGalleryParametricStability();
    bool pH = audit::testManifoldSwitchingContinuity();

    const auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsedSec = std::chrono::duration<double>(endTime - startTime).count();

    bool allPass = pA && pB && pC && pD && pE && pF && pG && pH && (audit::gFailedAssertions == 0);

    std::cout << "\n================================================================\n";
    std::cout << "           ACOUSTIC DECAY AUDIT SUITE EXECUTION SUMMARY         \n";
    std::cout << "================================================================\n";
    std::cout << "  [Suite A] Householder Freeze DC Bleed & Limit Cycle : " << (pA ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite B] Sub-Band Decay & Absorption Monotonicity   : " << (pB ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite C] Late-Tail Silence & Limit Cycle Absence    : " << (pC ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite D] Spectral Crest Factor & Ringing Mode Audit : " << (pD ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite E] C1 Smooth Saturation Boundedness (+40 dB)  : " << (pE ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite F] 64-Point Multi-Manifold Geometric Sweep    : " << (pF ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite G] Whispering Gallery High-Damp Stability     : " << (pG ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite H] Mid-Tail Switching Continuity Audit        : " << (pH ? "PASS" : "FAIL") << "\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "  Total Assertions Checked : " << audit::gTotalAssertions << "\n";
    std::cout << "  Failed Assertions        : " << audit::gFailedAssertions << "\n";
    std::cout << "  Total Audit Duration     : " << std::fixed << std::setprecision(2) << elapsedSec << " seconds\n";
    std::cout << "================================================================\n";
    std::cout << "  OVERALL ACOUSTIC AUDIT VERDICT: " << (allPass ? "APPROVE (100% PASS)" : "REJECT") << "\n";
    std::cout << "================================================================\n";

    return allPass ? 0 : 1;
}
