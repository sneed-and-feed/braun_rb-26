#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <vector>
#include <limits>

// Hardware denormal control includes
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#include <xmmintrin.h>
#include <pmmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
#include <arm64intr.h>
#endif
#endif

namespace rb26 {

// ============================================================================
// ScopedNoDenormals: Cross-Platform RAII Hardware FTZ/DAZ Guard
// ============================================================================
class ScopedNoDenormals {
public:
    ScopedNoDenormals() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        mOldMxcsr = _mm_getcsr();
        _mm_setcsr(mOldMxcsr | 0x8040); // Bit 15: FTZ (Flush-To-Zero), Bit 6: DAZ (Denormals-Are-Zero)
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
        mOldFpcr = _ReadStatusReg(ARM64_FPCR);
        _WriteStatusReg(ARM64_FPCR, mOldFpcr | (1ULL << 24)); // Bit 24: FZ
#elif defined(__GNUC__) || defined(__clang__)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        mOldFpcr = fpcr;
        asm volatile("msr fpcr, %0" : : "r"(fpcr | (1ULL << 24)));
#endif
#endif
    }

    ~ScopedNoDenormals() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_setcsr(mOldMxcsr);
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
        _WriteStatusReg(ARM64_FPCR, mOldFpcr);
#elif defined(__GNUC__) || defined(__clang__)
        asm volatile("msr fpcr, %0" : : "r"(mOldFpcr));
#endif
#endif
    }

    ScopedNoDenormals(const ScopedNoDenormals&) = delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

private:
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    unsigned int mOldMxcsr { 0 };
#elif defined(__aarch64__) || defined(_M_ARM64)
    uint64_t mOldFpcr { 0 };
#else
    int mDummy { 0 };
#endif
};

// ============================================================================
// Branchless Software Denormal / NaN Flushing
// ============================================================================
[[nodiscard]] inline float flushDenormal(float val) noexcept {
    if (!std::isfinite(val)) [[unlikely]] {
        return 0.0f;
    }
    return (std::abs(val) < 1.0e-15f) ? 0.0f : val;
}

// ============================================================================
// Mathematical Constants
// ============================================================================
inline constexpr float kPi     = 3.14159265358979323846f;
inline constexpr float kTwoPi  = 6.28318530717958647692f;
inline constexpr float kHalfPi = 1.57079632679489661923f;
inline constexpr float kSqrt2  = 1.41421356237309504880f;
inline constexpr float kPhi    = 1.61803398874989484820f; // Golden ratio

// ============================================================================
// Fast Precomputed Sine Lookup Table (2048 points, linear interpolation)
// Peak error < 1.2e-6 (-118.5 dB), 0 dynamic allocations, 0 transcendental calls
// ============================================================================
class FastSinTable {
public:
    static constexpr size_t kTableSize = 2048;
    static constexpr size_t kMask = kTableSize - 1;

    static inline const std::array<float, kTableSize> table = []() {
        std::array<float, kTableSize> t {};
        for (size_t i = 0; i < kTableSize; ++i) {
            t[i] = std::sin(static_cast<float>(i) * (kTwoPi / static_cast<float>(kTableSize)));
        }
        return t;
    }();

    [[nodiscard]] static inline float sin(float angle) noexcept {
        if (!std::isfinite(angle)) [[unlikely]] {
            return 0.0f;
        }
        const float norm = angle * (static_cast<float>(kTableSize) / kTwoPi);
        const int idx = static_cast<int>(std::floor(norm));
        const float frac = norm - static_cast<float>(idx);
        const size_t i0 = static_cast<size_t>(idx) & kMask;
        const size_t i1 = (i0 + 1) & kMask;
        return table[i0] + frac * (table[i1] - table[i0]);
    }

    [[nodiscard]] static inline float cos(float angle) noexcept {
        return sin(angle + kHalfPi);
    }
};

// ============================================================================
// Utility Conversion Functions
// ============================================================================
[[nodiscard]] inline float dbToGain(float db) noexcept {
    return std::pow(10.0f, db * 0.05f);
}

[[nodiscard]] inline float gainToDb(float gain) noexcept {
    return (gain > 1.0e-5f) ? (20.0f * std::log10(gain)) : -100.0f;
}

[[nodiscard]] inline float semitonesToRatio(float semitones) noexcept {
    return std::pow(2.0f, semitones / 12.0f);
}

// ============================================================================
// 4-Point, 3rd-Order Hermite Cubic Spline Interpolation (Horner Form)
// Continuous first derivative (C1), minimal ripple up to 0.45 fs
// ============================================================================
[[nodiscard]] inline float interpolateHermite4P3O(float ym1, float y0, float y1, float y2, float mu) noexcept {
    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return flushDenormal(((c3 * mu + c2) * mu + c1) * mu + c0);
}

// ============================================================================
// C1 Hermite Soft-Knee Boundary Saturation (k = 0.72, ceiling = 1.05)
// ============================================================================
[[nodiscard]] inline float applySmoothBoundaryKnee(float x, float knee = 0.72f, float ceiling = 1.05f) noexcept {
    if (std::isnan(x) || std::isinf(x)) [[unlikely]] {
        return 0.0f;
    }

    const float k = std::clamp(knee, 0.10f, 0.99f);
    const float M = std::max(k + 0.01f, ceiling);
    const float absX = std::abs(x);

    if (absX <= k) [[likely]] {
        return flushDenormal(x);
    }

    const float sign = (x > 0.0f) ? 1.0f : -1.0f;

    if (absX >= M) [[unlikely]] {
        return sign * M;
    }

    const float delta = M - k;
    const float u = (absX - k) / delta;
    // Horner evaluation of u + u^2 - u^3 = u * (1 + u * (1 - u))
    const float poly = u * (1.0f + u * (1.0f - u));
    return flushDenormal(sign * (k + delta * poly));
}

// Master bus soft limiter (knee = 0.85, ceiling = 1.0)
[[nodiscard]] inline float softLimit(float x, float knee = 0.85f) noexcept {
    return applySmoothBoundaryKnee(x, knee, 1.0f);
}

// ============================================================================
// One-Pole Parameter Smoother (Exponential Slewer)
// ============================================================================
class OnePoleSmoother {
public:
    OnePoleSmoother() noexcept = default;

    void reset(float initialValue = 0.0f) noexcept {
        mCurrent = initialValue;
        mTarget = initialValue;
    }

    void setSampleRate(float sampleRate) noexcept {
        mSampleRate = (sampleRate > 100.0f) ? sampleRate : 48000.0f;
        updateCoeff();
    }

    void setTimeConstant(float tauSec) noexcept {
        mTau = (tauSec > 0.0001f) ? tauSec : 0.0001f;
        updateCoeff();
    }

    void setTarget(float target) noexcept { mTarget = target; }
    void snapTo(float value) noexcept { mTarget = value; mCurrent = value; }

    [[nodiscard]] float getTarget() const noexcept { return mTarget; }
    [[nodiscard]] float getCurrent() const noexcept { return mCurrent; }

    [[nodiscard]] inline float next() noexcept {
        mCurrent += mCoeff * (mTarget - mCurrent);
        if (std::abs(mTarget - mCurrent) < 1.0e-6f) {
            mCurrent = mTarget;
        }
        mCurrent = flushDenormal(mCurrent);
        return mCurrent;
    }

private:
    void updateCoeff() noexcept {
        mCoeff = 1.0f - std::exp(-1.0f / (mSampleRate * mTau));
    }

    float mSampleRate { 48000.0f };
    float mTau { 0.025f };
    float mCoeff { 0.05f };
    float mCurrent { 0.0f };
    float mTarget { 0.0f };
};

// ============================================================================
// One-Pole Lowpass Filter
// ============================================================================
class OnePoleLowpass {
public:
    void reset() noexcept { mState = 0.0f; }

    void setCutoff(float sampleRate, float cutoffHz) noexcept {
        const float fs = (sampleRate > 100.0f) ? sampleRate : 48000.0f;
        const float fc = std::clamp(cutoffHz, 1.0f, fs * 0.495f);
        mAlpha = 1.0f - std::exp(-kTwoPi * (fc / fs));
    }

    [[nodiscard]] inline float process(float x) noexcept {
        mState = flushDenormal(mState + mAlpha * (x - mState));
        return mState;
    }

private:
    float mAlpha { 0.1f };
    float mState { 0.0f };
};

// ============================================================================
// One-Pole Highpass Filter
// ============================================================================
class OnePoleHighpass {
public:
    void reset() noexcept {
        mPrevX = 0.0f;
        mState = 0.0f;
    }

    void setCutoff(float sampleRate, float cutoffHz) noexcept {
        const float fs = (sampleRate > 100.0f) ? sampleRate : 48000.0f;
        const float fc = std::clamp(cutoffHz, 1.0f, fs * 0.495f);
        mAlpha = 1.0f / (1.0f + kTwoPi * (fc / fs));
    }

    [[nodiscard]] inline float process(float x) noexcept {
        const float y = mAlpha * (mState + x - mPrevX);
        mPrevX = flushDenormal(x);
        mState = flushDenormal(y);
        return flushDenormal(y);
    }

private:
    float mAlpha { 0.99f };
    float mPrevX { 0.0f };
    float mState { 0.0f };
};

// ============================================================================
// BiquadDirectForm2T: Direct Form II Transposed with Denormal Flushing
// ============================================================================
class BiquadDirectForm2T {
public:
    enum class Type { Lowpass, Highpass, Bandpass, Peaking };

    void reset() noexcept {
        mS1 = 0.0f;
        mS2 = 0.0f;
    }

    void configure(Type type, float sampleRate, float cutoffHz, float Q = 0.70710678f, float gainDb = 0.0f) noexcept {
        const float fs = (sampleRate > 100.0f) ? sampleRate : 48000.0f;
        const float fc = std::clamp(cutoffHz, 5.0f, fs * 0.495f);
        const float q = std::max(0.05f, Q);

        const float omega0 = kTwoPi * (fc / fs);
        const float cosOmega0 = std::cos(omega0);
        const float sinOmega0 = std::sin(omega0);
        const float alpha = sinOmega0 / (2.0f * q);

        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

        switch (type) {
            case Type::Lowpass:
                b0 = (1.0f - cosOmega0) * 0.5f;
                b1 = 1.0f - cosOmega0;
                b2 = (1.0f - cosOmega0) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosOmega0;
                a2 = 1.0f - alpha;
                break;
            case Type::Highpass:
                b0 = (1.0f + cosOmega0) * 0.5f;
                b1 = -(1.0f + cosOmega0);
                b2 = (1.0f + cosOmega0) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosOmega0;
                a2 = 1.0f - alpha;
                break;
            case Type::Bandpass:
                b0 = sinOmega0 * 0.5f;
                b1 = 0.0f;
                b2 = -sinOmega0 * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosOmega0;
                a2 = 1.0f - alpha;
                break;
            case Type::Peaking: {
                const float A = std::pow(10.0f, gainDb / 40.0f);
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cosOmega0;
                b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A;
                a1 = -2.0f * cosOmega0;
                a2 = 1.0f - alpha / A;
                break;
            }
        }

        const float invA0 = 1.0f / a0;
        mB0 = b0 * invA0;
        mB1 = b1 * invA0;
        mB2 = b2 * invA0;
        mA1 = a1 * invA0;
        mA2 = a2 * invA0;
    }

    [[nodiscard]] inline float process(float x) noexcept {
        const float y = mB0 * x + mS1;
        mS1 = flushDenormal(mB1 * x - mA1 * y + mS2);
        mS2 = flushDenormal(mB2 * x - mA2 * y);
        return flushDenormal(y);
    }

private:
    float mB0 { 1.0f }, mB1 { 0.0f }, mB2 { 0.0f };
    float mA1 { 0.0f }, mA2 { 0.0f };
    float mS1 { 0.0f }, mS2 { 0.0f };
};

// Convenient alias
using Biquad = BiquadDirectForm2T;

} // namespace rb26
