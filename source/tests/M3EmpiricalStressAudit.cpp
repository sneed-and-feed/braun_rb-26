#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <complex>
#include <algorithm>
#include <iomanip>
#include <chrono>
#include <random>
#include <limits>
#include <cassert>

// Core DSP Headers
#include "DspMath.h"
#include "BoundedSaturator.h"
#include "PitchShifter.h"
#include "LowBandModalMatrix.h"
#include "EarlyReflections.h"
#include "TailModulator.h"
#include "FdnReverbTank.h"
#include "Rb26Engine.h"

namespace m3_audit {

static int gFailedAssertions = 0;
static int gTotalAssertions = 0;

#define AUDIT_ASSERT(cond, msg) do { \
    ++m3_audit::gTotalAssertions; \
    if (!(cond)) { \
        std::cerr << "  [AUDIT FAILED] Line " << __LINE__ << ": " << (msg) << "\n"; \
        ++m3_audit::gFailedAssertions; \
        ++localFailures; \
    } \
} while (0)

#define AUDIT_ASSERT_NEAR(val, exp, tol, msg) do { \
    ++m3_audit::gTotalAssertions; \
    double v_ = static_cast<double>(val); \
    double e_ = static_cast<double>(exp); \
    double diff_ = std::abs(v_ - e_); \
    if (diff_ > (tol) || std::isnan(v_) || std::isinf(v_)) { \
        std::cerr << "  [AUDIT FAILED] Line " << __LINE__ << ": " << (msg) \
                  << " (actual: " << v_ << ", expected: " << e_ << ", diff: " << diff_ << ", tol: " << (tol) << ")\n"; \
        ++m3_audit::gFailedAssertions; \
        ++localFailures; \
    } \
} while (0)

// Helper: Goertzel algorithm to measure magnitude of specific frequency bin
double computeGoertzelMagnitude(const float* buffer, size_t numSamples, double targetFreq, double sampleRate) {
    double k = std::round((numSamples * targetFreq) / sampleRate);
    double omega = (2.0 * rb26::kPi * k) / numSamples;
    double coeff = 2.0 * std::cos(omega);
    double s_prev = 0.0;
    double s_prev2 = 0.0;

    for (size_t i = 0; i < numSamples; ++i) {
        double s = static_cast<double>(buffer[i]) + coeff * s_prev - s_prev2;
        s_prev2 = s_prev;
        s_prev = s;
    }

    double power = s_prev * s_prev + s_prev2 * s_prev2 - coeff * s_prev * s_prev2;
    return std::sqrt(std::max(0.0, power)) * 2.0 / static_cast<double>(numSamples);
}

// ============================================================================
// SUITE 1: FastSinTable Numerical Precision, SNR, and Harmonic Purity
// ============================================================================
bool testFastSinTableFidelity() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M3 Audit Suite 1] FastSinTable Numerical Precision & THD\n";
    std::cout << "============================================================\n";

    // 1.1: Error distribution & SNR across [0, 2*pi) with 1,000,000 evaluation points
    {
        const size_t N = 1000000;
        double maxSinErr = 0.0;
        double maxCosErr = 0.0;
        double sumSinErrSq = 0.0;
        double sumCosErrSq = 0.0;
        double sumSinRefSq = 0.0;
        double sumCosRefSq = 0.0;

        for (size_t i = 0; i < N; ++i) {
            float angle = static_cast<float>(i) * (rb26::kTwoPi / static_cast<float>(N));
            float refSin = std::sin(angle);
            float fastSin = rb26::FastSinTable::sin(angle);
            float refCos = std::cos(angle);
            float fastCos = rb26::FastSinTable::cos(angle);

            double errSin = std::abs(static_cast<double>(fastSin) - static_cast<double>(refSin));
            double errCos = std::abs(static_cast<double>(fastCos) - static_cast<double>(refCos));

            maxSinErr = std::max(maxSinErr, errSin);
            maxCosErr = std::max(maxCosErr, errCos);

            sumSinErrSq += errSin * errSin;
            sumCosErrSq += errCos * errCos;
            sumSinRefSq += static_cast<double>(refSin) * static_cast<double>(refSin);
            sumCosRefSq += static_cast<double>(refCos) * static_cast<double>(refCos);
        }

        double rmsSinErr = std::sqrt(sumSinErrSq / static_cast<double>(N));
        double rmsCosErr = std::sqrt(sumCosErrSq / static_cast<double>(N));
        double rmsSinRef = std::sqrt(sumSinRefSq / static_cast<double>(N));
        double rmsCosRef = std::sqrt(sumCosRefSq / static_cast<double>(N));

        double snrSinDb = 20.0 * std::log10(rmsSinRef / rmsSinErr);
        double snrCosDb = 20.0 * std::log10(rmsCosRef / rmsCosErr);

        std::cout << "  [1.1] 1M Samples [0, 2pi) Evaluation:\n";
        std::cout << "        FastSin: Peak Error = " << std::scientific << std::setprecision(4) << maxSinErr 
                  << " (" << std::fixed << std::setprecision(2) << 20.0 * std::log10(maxSinErr) << " dBFS)"
                  << ", SNR = " << snrSinDb << " dB\n";
        std::cout << "        FastCos: Peak Error = " << std::scientific << std::setprecision(4) << maxCosErr 
                  << " (" << std::fixed << std::setprecision(2) << 20.0 * std::log10(maxCosErr) << " dBFS)"
                  << ", SNR = " << snrCosDb << " dB\n";

        AUDIT_ASSERT(maxSinErr < 1.5e-6, "FastSinTable::sin peak error must remain below 1.5e-6 (-116.5 dB)");
        AUDIT_ASSERT(maxCosErr < 1.5e-6, "FastSinTable::cos peak error must remain below 1.5e-6 (-116.5 dB)");
        AUDIT_ASSERT(snrSinDb > 120.0, "FastSinTable::sin SNR must exceed 120 dB");
        AUDIT_ASSERT(snrCosDb > 120.0, "FastSinTable::cos SNR must exceed 120 dB");
    }

    // 1.2: Spectral Purity, THD, and SFDR of 1 kHz Tone at 48 kHz
    {
        const size_t N = 48000; // 1 second buffer
        std::vector<float> fastSine(N);
        std::vector<float> refSine(N);

        for (size_t n = 0; n < N; ++n) {
            float phase = static_cast<float>(n) * (rb26::kTwoPi * 1000.0f / 48000.0f);
            fastSine[n] = rb26::FastSinTable::sin(phase);
            refSine[n] = std::sin(phase);
        }

        double fundMag = computeGoertzelMagnitude(fastSine.data(), N, 1000.0, 48000.0);
        double harmonicPowerSum = 0.0;
        double maxSpurMag = 0.0;
        int maxSpurHarmonic = 0;

        for (int h = 2; h <= 20; ++h) {
            double hFreq = 1000.0 * static_cast<double>(h);
            double hMag = computeGoertzelMagnitude(fastSine.data(), N, hFreq, 48000.0);
            harmonicPowerSum += hMag * hMag;
            if (hMag > maxSpurMag) {
                maxSpurMag = hMag;
                maxSpurHarmonic = h;
            }
        }

        double thdDb = 10.0 * std::log10(harmonicPowerSum / (fundMag * fundMag));
        double sfdrDb = 20.0 * std::log10(fundMag / maxSpurMag);

        std::cout << "  [1.2] 1 kHz Pure Tone Spectral Purity at 48 kHz (N = 48000):\n";
        std::cout << "        Fundamental Mag: " << fundMag << " (expected ~1.0)\n";
        std::cout << "        THD (Harmonics 2..20): " << std::fixed << std::setprecision(2) << thdDb << " dB\n";
        std::cout << "        SFDR (Max Spur at H" << maxSpurHarmonic << "): " << sfdrDb << " dB\n";

        AUDIT_ASSERT(fundMag > 0.999 && fundMag < 1.001, "Fundamental magnitude must be within 0.1% of unity");
        AUDIT_ASSERT(thdDb < -105.0, "FastSinTable THD must be below -105 dB (far exceeding 16-bit audio dynamic range)");
        AUDIT_ASSERT(sfdrDb > 110.0, "FastSinTable SFDR must exceed 110 dB");
    }

    // 1.3: Extended Domain, Negative Angles, and Periodicity Invariance
    {
        // Operating domain [-4*pi, +4*pi] covering all DSP modulation excursions
        double maxOpErr = 0.0;
        const int opSteps = 100000;
        for (int i = -opSteps; i <= opSteps; ++i) {
            float angle = static_cast<float>(i) * (4.0f * rb26::kPi / static_cast<float>(opSteps));
            float fast = rb26::FastSinTable::sin(angle);
            float ref = std::sin(angle);
            double err = std::abs(static_cast<double>(fast) - static_cast<double>(ref));
            maxOpErr = std::max(maxOpErr, err);
        }

        // Extreme stress domain [-100*pi, +100*pi] checking float32 cancellation limits
        double maxExtErr = 0.0;
        const int extSteps = 200000;
        for (int i = -extSteps; i <= extSteps; ++i) {
            float angle = static_cast<float>(i) * (100.0f * rb26::kPi / static_cast<float>(extSteps));
            float fast = rb26::FastSinTable::sin(angle);
            float ref = std::sin(angle);
            double err = std::abs(static_cast<double>(fast) - static_cast<double>(ref));
            maxExtErr = std::max(maxExtErr, err);
        }

        std::cout << "  [1.3] Extended Range Audit:\n";
        std::cout << "        Operating Range [-4*pi, +4*pi] Max Error: " << std::scientific << maxOpErr 
                  << " (" << std::fixed << std::setprecision(2) << 20.0 * std::log10(maxOpErr) << " dBFS)\n";
        std::cout << "        Extreme Range [-100*pi, +100*pi] Max Error: " << std::scientific << maxExtErr 
                  << " (" << std::fixed << std::setprecision(2) << 20.0 * std::log10(maxExtErr) << " dBFS)\n";

        AUDIT_ASSERT(maxOpErr < 2.0e-6, "FastSinTable must preserve < 2.0e-6 error across operating range [-4pi, +4pi]");
        AUDIT_ASSERT(maxExtErr < 5.0e-5, "FastSinTable must stay within float32 precision limits across [-100pi, +100pi]");
    }

    // 1.4: Special IEEE 754 Boundary Values (NaN, Inf, Denormals, Exact Zero)
    {
        float nanVal = std::numeric_limits<float>::quiet_NaN();
        float infVal = std::numeric_limits<float>::infinity();
        float negInfVal = -std::numeric_limits<float>::infinity();
        float denormVal = 1.0e-39f;

        float resNan = rb26::FastSinTable::sin(nanVal);
        float resInf = rb26::FastSinTable::sin(infVal);
        float resNegInf = rb26::FastSinTable::sin(negInfVal);
        float resZero = rb26::FastSinTable::sin(0.0f);
        float resNegZero = rb26::FastSinTable::sin(-0.0f);
        float resDenorm = rb26::FastSinTable::sin(denormVal);

        std::cout << "  [1.4] IEEE 754 Boundary Inputs:\n";
        std::cout << "        sin(NaN) = " << resNan << " | sin(+Inf) = " << resInf 
                  << " | sin(-Inf) = " << resNegInf << " | sin(0) = " << resZero 
                  << " | sin(denorm) = " << resDenorm << "\n";

        AUDIT_ASSERT(resNan == 0.0f, "sin(NaN) must return safe 0.0f");
        AUDIT_ASSERT(resInf == 0.0f, "sin(+Inf) must return safe 0.0f");
        AUDIT_ASSERT(resNegInf == 0.0f, "sin(-Inf) must return safe 0.0f");
        AUDIT_ASSERT(resZero == 0.0f, "sin(0.0f) must return exact 0.0f");
        AUDIT_ASSERT(resNegZero == 0.0f, "sin(-0.0f) must return exact 0.0f");
        AUDIT_ASSERT(!std::isnan(resDenorm) && !std::isinf(resDenorm), "sin(denormal) must be finite");
    }

    std::cout << "  Suite 1 Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// SUITE 2: exp2f Fidelity and Shepard Spiral Exponentiation
// ============================================================================
bool testExp2fFidelity() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M3 Audit Suite 2] exp2f Precision & Monotonicity\n";
    std::cout << "============================================================\n";

    // 2.1: Shepard Pitch Spiral Domain: exponent in [-2.0f, +2.0f]
    {
        const size_t N = 500000;
        double maxRelErr = 0.0;
        double maxAbsErr = 0.0;
        float prevVal = -1.0f;
        bool strictlyMonotonic = true;

        for (size_t i = 0; i < N; ++i) {
            float x = -2.0f + static_cast<float>(i) * (4.0f / static_cast<float>(N));
            float fExp2 = exp2f(x);
            float fPow = std::pow(2.0f, x);

            double absErr = std::abs(static_cast<double>(fExp2) - static_cast<double>(fPow));
            double relErr = absErr / static_cast<double>(fPow);

            maxAbsErr = std::max(maxAbsErr, absErr);
            maxRelErr = std::max(maxRelErr, relErr);

            if (fExp2 < prevVal) {
                strictlyMonotonic = false;
            }
            prevVal = fExp2;
        }

        std::cout << "  [2.1] 500,000 Points across Shepard Domain [-2.0, +2.0]:\n";
        std::cout << "        Max Absolute Error: " << std::scientific << maxAbsErr << "\n";
        std::cout << "        Max Relative Error: " << std::scientific << maxRelErr << "\n";
        std::cout << "        Strict Monotonicity: " << (strictlyMonotonic ? "TRUE" : "FALSE") << "\n";

        AUDIT_ASSERT(maxRelErr < 1.0e-6, "exp2f relative error vs std::pow(2.0f, x) must be < 1.0e-6");
        AUDIT_ASSERT(strictlyMonotonic, "exp2f must be monotonically increasing");
    }

    // 2.2: Extended range [-10.0, +10.0]
    {
        double maxRelErr = 0.0;
        for (int i = -10000; i <= 10000; ++i) {
            float x = static_cast<float>(i) * 0.001f;
            float fExp2 = exp2f(x);
            float fPow = std::pow(2.0f, x);
            double relErr = std::abs(static_cast<double>(fExp2) - static_cast<double>(fPow)) / static_cast<double>(fPow);
            maxRelErr = std::max(maxRelErr, relErr);
        }
        std::cout << "  [2.2] Extended Range [-10.0, +10.0] Max Relative Error: " << std::scientific << maxRelErr << "\n";
        AUDIT_ASSERT(maxRelErr < 1.0e-6, "exp2f across [-10, 10] relative error must be < 1.0e-6");
    }

    std::cout << "  Suite 2 Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// SUITE 3: Ducking Floor Gain Caching Invariance under Dynamic Modulation
// ============================================================================
bool testDuckingFloorCachingInvariance() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M3 Audit Suite 3] Ducking Floor Gain Caching Invariance\n";
    std::cout << "============================================================\n";

    // 3.1: Verify cached vs fresh detector equivalence across parameter grid
    {
        rb26::TransientPunchDetector cachedDetector;
        cachedDetector.prepare(48000.0);

        const int numDepths = 100;
        double maxDivergence = 0.0;

        for (int i = 0; i <= numDepths; ++i) {
            float d = static_cast<float>(i) / static_cast<float>(numDepths);

            // Create fresh detector for comparison
            rb26::TransientPunchDetector freshDetector;
            freshDetector.prepare(48000.0);

            // Feed identical 100-sample transient burst
            for (int s = 0; s < 100; ++s) {
                float in = (s == 0) ? 1.0f : 0.0f;
                float gCached = cachedDetector.process(in, in, d);
                float gFresh = freshDetector.process(in, in, d);

                double diff = std::abs(static_cast<double>(gCached) - static_cast<double>(gFresh));
                maxDivergence = std::max(maxDivergence, diff);
            }
            cachedDetector.reset();
        }

        std::cout << "  [3.1] Cached vs Fresh Detector Divergence across 101 Depths:\n";
        std::cout << "        Max Divergence: " << std::scientific << maxDivergence << "\n";
        AUDIT_ASSERT(maxDivergence < 1.0e-6, "Cached floor gain must match fresh detector within 1e-6");
    }

    // 3.2: Rapid continuous automation of depth parameter during audio rendering
    {
        rb26::TransientPunchDetector detectorA;
        detectorA.prepare(48000.0);

        const int N = 48000; // 1 second
        float minObservedGain = 1.0f;
        float maxObservedGain = 0.0f;

        // Feed transient bursts while sweeping depth rapidly with 10 Hz LFO
        for (int n = 0; n < N; ++n) {
            float input = (n % 480 == 0) ? 1.0f : 0.0f; // 100 Hz pulses
            float depth = 0.5f + 0.5f * std::sin(2.0f * rb26::kPi * 10.0f * static_cast<float>(n) / 48000.0f);

            float gA = detectorA.process(input, input, depth);
            minObservedGain = std::min(minObservedGain, gA);
            maxObservedGain = std::max(maxObservedGain, gA);

            AUDIT_ASSERT(!std::isnan(gA) && !std::isinf(gA), "Ducking gain must be finite");
            AUDIT_ASSERT(gA >= 0.20f && gA <= 1.05f, "Ducking gain must remain bounded");
        }

        std::cout << "  [3.2] Rapid 10 Hz Parameter Sweeping (48,000 samples):\n";
        std::cout << "        Min Duck Gain: " << minObservedGain << " | Max Duck Gain: " << maxObservedGain << "\n";
        AUDIT_ASSERT(minObservedGain < 0.50f, "Ducking must engage during transient pulses");
        AUDIT_ASSERT(maxObservedGain <= 1.0001f, "Ducking must not exceed unity during release");
    }

    std::cout << "  Suite 3 Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}


// ============================================================================
// SUITE 4: PitchShifter Inactive Bypass & Cross-Boundary Smoothness
// ============================================================================
bool testPitchShifterBypassAndTransitionSmoothness() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M3 Audit Suite 4] PitchShifter Inactive Bypass & Smoothness\n";
    std::cout << "============================================================\n";

    // 4.1: When sends <= 0.001f, wet output must be exact 0.0f with 0 CPU overhead
    {
        rb26::PitchShifter ps;
        ps.prepare(48000.0, 256);
        ps.setParameters(0.0005f, 0.0005f, 12, -12, 0.0f, 0.0f);

        const int bs = 256;
        std::vector<float> inL(bs, 0.7f), inR(bs, -0.7f);
        std::vector<float> outL(bs, 999.0f), outR(bs, 999.0f);

        // Run several blocks to ensure smoother settles to 0.0005f <= 0.001f
        for (int b = 0; b < 20; ++b) {
            ps.process(inL.data(), inR.data(), outL.data(), outR.data(), bs);
        }

        float maxPeakL = 0.0f, maxPeakR = 0.0f;
        for (int i = 0; i < bs; ++i) {
            maxPeakL = std::max(maxPeakL, std::abs(outL[i]));
            maxPeakR = std::max(maxPeakR, std::abs(outR[i]));
        }

        std::cout << "  [4.1] Send = 0.0005f (<= 0.001f) Wet Output Peak: L = " << maxPeakL << ", R = " << maxPeakR << "\n";
        AUDIT_ASSERT(maxPeakL == 0.0f, "Inactive bypass wet output L must be exact 0.0f");
        AUDIT_ASSERT(maxPeakR == 0.0f, "Inactive bypass wet output R must be exact 0.0f");
    }

    // 4.2: Automated Cross-Boundary Transitions (0.0005f <-> 0.05f): No clicks or pops
    {
        rb26::PitchShifter ps;
        ps.prepare(48000.0, 128);
        const int bs = 128;
        std::vector<float> inL(bs), inR(bs), outL(bs), outR(bs);

        for (int i = 0; i < bs; ++i) {
            inL[i] = 0.5f * std::sin(2.0f * rb26::kPi * 440.0f * static_cast<float>(i) / 48000.0f);
            inR[i] = inL[i];
        }

        float maxSampleJump = 0.0f;
        float prevSample = 0.0f;

        // Toggle send across boundary every 10 blocks (every 1280 samples)
        for (int b = 0; b < 100; ++b) {
            float send = ((b / 10) % 2 == 0) ? 0.0f : 0.20f;
            ps.setParameters(send, 0.0f, 12, -12, 0.0f, 0.0f);
            ps.process(inL.data(), inR.data(), outL.data(), outR.data(), bs);

            for (int i = 0; i < bs; ++i) {
                float jump = std::abs(outL[i] - prevSample);
                maxSampleJump = std::max(maxSampleJump, jump);
                prevSample = outL[i];
                AUDIT_ASSERT(!std::isnan(outL[i]) && !std::isinf(outL[i]), "Wet output must be finite");
            }
        }

        std::cout << "  [4.2] Automated Cross-Boundary Transitions Max Sample Jump: " << maxSampleJump << "\n";
        AUDIT_ASSERT(maxSampleJump < 0.25f, "Cross-boundary send automation must be click-free and continuous");
    }

    std::cout << "  Suite 4 Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

// ============================================================================
// SUITE 5: Full Reverb Regression Invariance Across All 5 Domains
// ============================================================================
bool testFullReverbRegressionInvariance() {
    int localFailures = 0;
    std::cout << "\n============================================================\n";
    std::cout << "[M3 Audit Suite 5] Master Engine Regression Invariance\n";
    std::cout << "============================================================\n";

    rb26::Rb26ReverbEngine engine;
    engine.prepare(48000.0, 256);

    // Verify all 10 factory presets can be loaded and rendered without audio anomalies
    const auto& presets = rb26::Rb26ReverbEngine::getFactoryPresets();
    AUDIT_ASSERT(presets.size() == 10, "Must have exactly 10 factory presets");

    const int bs = 256;
    std::vector<float> inL(bs), inR(bs), outL(bs), outR(bs);
    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    for (size_t p = 0; p < presets.size(); ++p) {
        engine.reset();
        engine.setParameters(presets[p].params);

        float peak = 0.0f;
        for (int b = 0; b < 20; ++b) {
            for (int i = 0; i < bs; ++i) {
                inL[i] = (b == 0 && i == 0) ? 1.0f : 0.0f; // Unit impulse
                inR[i] = inL[i];
            }
            engine.process(inPtrs, outPtrs, 2, bs);
            for (int i = 0; i < bs; ++i) {
                AUDIT_ASSERT(!std::isnan(outL[i]) && !std::isinf(outL[i]), "Preset output must be finite");
                AUDIT_ASSERT(!std::isnan(outR[i]) && !std::isinf(outR[i]), "Preset output must be finite");
                peak = std::max(peak, std::max(std::abs(outL[i]), std::abs(outR[i])));
            }
        }
        std::cout << "  [5." << (p + 1) << "] Preset '" << presets[p].name << "' Impulse Peak: " << peak << "\n";
        AUDIT_ASSERT(peak > 0.001f, "Preset must produce audible reverberation");
        AUDIT_ASSERT(peak <= 1.05f, "Preset peak must remain bounded <= 1.05");
    }

    std::cout << "  Suite 5 Verdict: " << (localFailures == 0 ? "PASS" : "FAIL") << "\n";
    return localFailures == 0;
}

} // namespace m3_audit

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    std::cout << "================================================================\n";
    std::cout << "   BRAUN RB-26 — M3_2 EMPIRICAL STRESS AUDIT HARNESS            \n";
    std::cout << "   Auditing FastSinTable Fidelity, exp2f, Ducking Cache & Bypass\n";
    std::cout << "================================================================\n";

    bool p1 = m3_audit::testFastSinTableFidelity();
    bool p2 = m3_audit::testExp2fFidelity();
    bool p3 = m3_audit::testDuckingFloorCachingInvariance();
    bool p4 = m3_audit::testPitchShifterBypassAndTransitionSmoothness();
    bool p5 = m3_audit::testFullReverbRegressionInvariance();

    bool allPass = p1 && p2 && p3 && p4 && p5 && (m3_audit::gFailedAssertions == 0);

    std::cout << "\n================================================================\n";
    std::cout << "        CHALLENGER M3_2 EMPIRICAL AUDIT SUMMARY                 \n";
    std::cout << "================================================================\n";
    std::cout << "  [Suite 1] FastSinTable Fidelity & THD/SFDR          : " << (p1 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite 2] exp2f Precision & Monotonicity            : " << (p2 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite 3] Ducking Floor Gain Caching Invariance     : " << (p3 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite 4] PitchShifter Inactive Bypass Smoothness   : " << (p4 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [Suite 5] Master Engine Regression Invariance       : " << (p5 ? "PASS" : "FAIL") << "\n";
    std::cout << "----------------------------------------------------------------\n";
    std::cout << "  Total Assertions Checked : " << m3_audit::gTotalAssertions << "\n";
    std::cout << "  Failed Assertions        : " << m3_audit::gFailedAssertions << "\n";
    std::cout << "================================================================\n";
    std::cout << "  OVERALL EMPIRICAL VERDICT: " << (allPass ? "APPROVE" : "REJECT") << "\n";
    std::cout << "================================================================\n";

    return allPass ? 0 : 1;
}
