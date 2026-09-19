#pragma once

#include "DspMath.h"
#include <cmath>
#include <algorithm>
#include <cstddef>

namespace rb26 {

/**
 * BoundedSaturator: C1 Hermite soft-knee saturator and feedback limiter.
 *
 * Guarantees:
 * 1. Exact unity gain (0 dBFS, slope = 1.0) below knee (|x| <= 0.72).
 * 2. C1 continuous cubic Hermite curve for 0.72 < |x| < 1.05.
 * 3. Strict saturation bound: |y| <= 1.05 for all real inputs.
 * 4. Zero derivative at |x| = 1.05, ensuring smooth transition to flat ceiling.
 * 5. Branchless NaN / denormal protection.
 */
class BoundedSaturator {
public:
    static constexpr float kDefaultKnee = 0.72f;
    static constexpr float kDefaultCeiling = 1.05f;

    constexpr BoundedSaturator() noexcept
        : mKnee(kDefaultKnee), mCeiling(kDefaultCeiling),
          mDelta(kDefaultCeiling - kDefaultKnee),
          mInvDelta(1.0f / (kDefaultCeiling - kDefaultKnee)) {}

    constexpr BoundedSaturator(float knee, float ceiling) noexcept
        : mKnee(knee), mCeiling(ceiling),
          mDelta(ceiling - knee),
          mInvDelta((ceiling > knee) ? (1.0f / (ceiling - knee)) : 1.0f) {}

    void setKneeAndCeiling(float knee, float ceiling) noexcept {
        mKnee = std::clamp(knee, 0.10f, 0.99f);
        mCeiling = std::max(mKnee + 0.01f, ceiling);
        mDelta = mCeiling - mKnee;
        mInvDelta = 1.0f / mDelta;
    }

    [[nodiscard]] float getKnee() const noexcept { return mKnee; }
    [[nodiscard]] float getCeiling() const noexcept { return mCeiling; }

    /**
     * Process a single audio sample (inlined for real-time performance).
     */
    [[nodiscard]] inline float processSample(float x) const noexcept {
        if (!rb26::isFiniteBitwise(x)) [[unlikely]] {
            return 0.0f;
        }

        const float absX = std::abs(x);
        if (absX < 1.0e-15f) [[unlikely]] {
            return 0.0f;
        }

        // Region 1: Linear unity-gain zone
        if (absX <= mKnee) [[likely]] {
            return x;
        }

        const float sign = (x > 0.0f) ? 1.0f : -1.0f;

        // Region 3: Clamped saturation ceiling
        if (absX >= mCeiling) [[unlikely]] {
            return sign * mCeiling;
        }

        // Region 2: C1 Hermite cubic soft knee (k < |x| < M)
        const float u = (absX - mKnee) * mInvDelta;
        // Horner's evaluation of u + u^2 - u^3 = u * (1.0 + u * (1.0 - u))
        const float poly = u * (1.0f + u * (1.0f - u));
        return flushDenormal(sign * (mKnee + mDelta * poly));
    }

    /**
     * Block processing for SIMD / auto-vectorization.
     */
    void processBlock(const float* input, float* output, int numSamples) const noexcept;

    /**
     * In-place block processing.
     */
    void processBlock(float* buffer, int numSamples) const noexcept;

private:
    float mKnee { kDefaultKnee };
    float mCeiling { kDefaultCeiling };
    float mDelta { kDefaultCeiling - kDefaultKnee };
    float mInvDelta { 1.0f / (kDefaultCeiling - kDefaultKnee) };
};

} // namespace rb26
