#pragma once

#include "DspMath.h"
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstddef>

namespace rb26 {

// ============================================================================
// Parameter Structure for LowBandModalMatrix
// ============================================================================
struct LowBandModalParams {
    float crossoverHz    { 180.0f };   // 60.0 - 400.0 Hz (LR4 crossover point)
    float bassRt60Mult   { 1.0f };     // 0.2 - 4.0x (Low-frequency decay scaling)
    float rt60DecaySec   { 3.5f };     // 0.2 - 30.0 s (Base mid RT60 decay)
    float punchDucking   { 0.40f };    // 0.0 - 1.0 (Ducking depth, up to 12 dB)
    float subMonoHz      { 120.0f };   // 20.0 - 250.0 Hz (Elliptical mono maker cutoff)
    bool  freezeHold     { false };    // Infinite sustain toggle
};

// ============================================================================
// LinkwitzRiley4: 4th-Order Linkwitz-Riley Stereo Crossover (Cascaded Butterworth)
// Guarantees |H_sum| = 1.0 and 0 deg relative phase across all frequencies.
// ============================================================================
class LinkwitzRiley4 {
public:
    LinkwitzRiley4() noexcept = default;

    void prepare(double sampleRate) noexcept {
        mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        mCutoffHz = 180.0f;
        updateCoefficients(mCutoffHz);
        reset();
    }

    void reset() noexcept {
        mLp1L.reset(); mLp2L.reset();
        mLp1R.reset(); mLp2R.reset();
        mHp1L.reset(); mHp2L.reset();
        mHp1R.reset(); mHp2R.reset();
    }

    void setCutoff(float cutoffHz) noexcept {
        const float clamped = std::clamp(cutoffHz, 40.0f, std::min(1000.0f, mSampleRate * 0.45f));
        if (std::abs(clamped - mCutoffHz) > 0.02f) {
            mCutoffHz = clamped;
            updateCoefficients(mCutoffHz);
        }
    }

    // Direct Form II Transposed Biquad stage
    struct Stage {
        float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f };
        float a1 { 0.0f }, a2 { 0.0f };
        float s1 { 0.0f }, s2 { 0.0f };

        void reset() noexcept { s1 = 0.0f; s2 = 0.0f; }

        [[nodiscard]] inline float process(float x) noexcept {
            const float y = b0 * x + s1;
            s1 = flushDenormal(b1 * x - a1 * y + s2);
            s2 = flushDenormal(b2 * x - a2 * y);
            return flushDenormal(y);
        }
    };

    // Processes stereo sample: splits into Low-Band (< fx) and High-Band (> fx)
    inline void process(float inL, float inR,
                        float& lowL, float& lowR,
                        float& highL, float& highR) noexcept {
        // Left Channel
        lowL  = mLp2L.process(mLp1L.process(inL));
        highL = mHp2L.process(mHp1L.process(inL));

        // Right Channel
        lowR  = mLp2R.process(mLp1R.process(inR));
        highR = mHp2R.process(mHp1R.process(inR));
    }

private:
    void updateCoefficients(float fc) noexcept {
        const float w0 = kTwoPi * (fc / mSampleRate);
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        const float alpha = sinW0 * 0.7071067811865475f; // Q = 1/sqrt(2) => alpha = sin / sqrt(2)

        const float a0 = 1.0f + alpha;
        const float invA0 = 1.0f / a0;

        const float b0_lp = (1.0f - cosW0) * 0.5f * invA0;
        const float b1_lp = (1.0f - cosW0) * invA0;
        const float b2_lp = b0_lp;

        const float b0_hp = (1.0f + cosW0) * 0.5f * invA0;
        const float b1_hp = -(1.0f + cosW0) * invA0;
        const float b2_hp = b0_hp;

        const float a1 = -2.0f * cosW0 * invA0;
        const float a2 = (1.0f - alpha) * invA0;

        // Apply coefficients to all cascaded biquads
        auto applyCoeffs = [&](Stage& s, float b0, float b1, float b2) {
            s.b0 = b0; s.b1 = b1; s.b2 = b2;
            s.a1 = a1; s.a2 = a2;
        };

        applyCoeffs(mLp1L, b0_lp, b1_lp, b2_lp);
        applyCoeffs(mLp2L, b0_lp, b1_lp, b2_lp);
        applyCoeffs(mLp1R, b0_lp, b1_lp, b2_lp);
        applyCoeffs(mLp2R, b0_lp, b1_lp, b2_lp);

        applyCoeffs(mHp1L, b0_hp, b1_hp, b2_hp);
        applyCoeffs(mHp2L, b0_hp, b1_hp, b2_hp);
        applyCoeffs(mHp1R, b0_hp, b1_hp, b2_hp);
        applyCoeffs(mHp2R, b0_hp, b1_hp, b2_hp);
    }

    float mSampleRate { 48000.0f };
    float mCutoffHz { 180.0f };

    Stage mLp1L, mLp2L, mLp1R, mLp2R;
    Stage mHp1L, mHp2L, mHp1R, mHp2R;
};

// ============================================================================
// TransientPunchDetector: Fast/Slow Envelope Ratio Punch Preserver
// Ducks modal low injection by up to 12 dB on kick/bass attacks (TR > 1.8).
// ============================================================================
class TransientPunchDetector {
public:
    TransientPunchDetector() noexcept = default;

    void prepare(double sampleRate) noexcept {
        mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        mAlphaFast  = std::exp(-1.0f / (mSampleRate * 0.0015f)); // tau = 1.5 ms
        mAlphaSlow  = std::exp(-1.0f / (mSampleRate * 0.0400f)); // tau = 40.0 ms
        mAlphaAtt   = std::exp(-1.0f / (mSampleRate * 0.0010f)); // tau = 1.0 ms
        mAlphaRel   = std::exp(-1.0f / (mSampleRate * 0.0300f)); // tau = 30.0 ms
        mAlphaOnset = std::exp(-1.0f / (mSampleRate * 0.0250f)); // tau = 25.0 ms onset hold
        reset();
    }

    void reset() noexcept {
        mFastEnv = 0.0f;
        mSlowEnv = 0.0f;
        mCurrentDuckGain = 1.0f;
        mPrevX = 0.0f;
        mOnsetEnv = 0.0f;
        mCachedDepth = -1.0f;
        mCachedFloorGain = 1.0f;
    }

    // Process sample: returns ducking gain in [0.2512, 1.0]
    inline float process(float xL, float xR, float punchDepth) noexcept {
        const float absX = std::max(std::abs(xL), std::abs(xR));
        const float normDelta = (absX - mPrevX) * (mSampleRate / 48000.0f);
        mPrevX = absX;

        if (normDelta > 0.010f) {
            mOnsetEnv = 1.0f;
        } else {
            mOnsetEnv = flushDenormal(mOnsetEnv * mAlphaOnset);
        }

        mFastEnv = flushDenormal((1.0f - mAlphaFast) * absX + mAlphaFast * mFastEnv);
        mSlowEnv = flushDenormal((1.0f - mAlphaSlow) * absX + mAlphaSlow * mSlowEnv);

        const float TR = mFastEnv / (mSlowEnv + 1.0e-5f);
        const float depth = std::clamp(punchDepth, 0.0f, 1.0f);

        float targetGain = 1.0f;
        if (TR > 1.8f && depth > 0.001f && mOnsetEnv > 0.01f) {
            if (depth != mCachedDepth) {
                mCachedDepth = depth;
                mCachedFloorGain = std::pow(10.0f, -0.62f * std::pow(depth, 0.35f));
            }
            const float excess = TR - 1.8f;
            const float raw = 1.0f / (1.0f + 0.70f * depth * excess);
            targetGain = std::max(mCachedFloorGain, raw);
        }

        // Asymmetric smoother
        if (targetGain < mCurrentDuckGain) {
            mCurrentDuckGain = mAlphaAtt * mCurrentDuckGain + (1.0f - mAlphaAtt) * targetGain;
        } else {
            mCurrentDuckGain = mAlphaRel * mCurrentDuckGain + (1.0f - mAlphaRel) * targetGain;
        }

        mCurrentDuckGain = flushDenormal(mCurrentDuckGain);
        return mCurrentDuckGain;
    }

    [[nodiscard]] float getLastDuckingGain() const noexcept { return mCurrentDuckGain; }

private:
    float mSampleRate { 48000.0f };
    float mAlphaFast  { 0.0f };
    float mAlphaSlow  { 0.0f };
    float mAlphaAtt   { 0.0f };
    float mAlphaRel   { 0.0f };
    float mAlphaOnset { 0.0f };
    float mFastEnv    { 0.0f };
    float mSlowEnv    { 0.0f };
    float mPrevX      { 0.0f };
    float mOnsetEnv   { 0.0f };
    float mCurrentDuckGain { 1.0f };
    float mCachedDepth { -1.0f };
    float mCachedFloorGain { 1.0f };
};

// ============================================================================
// SubBassEllipticalFilter: 4th-Order Linkwitz-Riley Side Highpass Filter
// Cascaded dual 2nd-order Butterworth stages (24 dB/octave) for assertive mono
// collapse below subMonoHz, eliminating subwoofer phase smearing completely.
// ============================================================================
class SubBassEllipticalFilter {
public:
    SubBassEllipticalFilter() noexcept = default;

    void prepare(double sampleRate) noexcept {
        mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        mCutoffHz = 120.0f;
        updateCoefficients(mCutoffHz);
        reset();
    }

    void reset() noexcept {
        mS1_1 = 0.0f; mS2_1 = 0.0f;
        mS1_2 = 0.0f; mS2_2 = 0.0f;
    }

    void setCutoff(float cutoffHz) noexcept {
        const float clamped = std::clamp(cutoffHz, 20.0f, std::min(400.0f, mSampleRate * 0.45f));
        if (std::abs(clamped - mCutoffHz) > 0.02f) {
            mCutoffHz = clamped;
            updateCoefficients(mCutoffHz);
        }
    }

    inline void process(float inL, float inR, float& outL, float& outR) noexcept {
        const float M = 0.5f * (inL + inR);
        const float S = 0.5f * (inL - inR);

        // Cascaded 4th-Order Linkwitz-Riley Highpass on Side channel (2x 2nd-Order Butterworth, 24 dB/oct)
        const float s1 = mB0 * S + mS1_1;
        mS1_1 = flushDenormal(mB1 * S - mA1 * s1 + mS2_1);
        mS2_1 = flushDenormal(mB2 * S - mA2 * s1);

        const float s2 = mB0 * s1 + mS1_2;
        mS1_2 = flushDenormal(mB1 * s1 - mA1 * s2 + mS2_2);
        mS2_2 = flushDenormal(mB2 * s1 - mA2 * s2);

        outL = flushDenormal(M + s2);
        outR = flushDenormal(M - s2);
    }

private:
    void updateCoefficients(float fc) noexcept {
        const float w0 = kTwoPi * (fc / mSampleRate);
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        const float alpha = sinW0 * 0.7071067811865475f;

        const float a0 = 1.0f + alpha;
        const float invA0 = 1.0f / a0;

        mB0 = (1.0f + cosW0) * 0.5f * invA0;
        mB1 = -(1.0f + cosW0) * invA0;
        mB2 = mB0;
        mA1 = -2.0f * cosW0 * invA0;
        mA2 = (1.0f - alpha) * invA0;
    }

    float mSampleRate { 48000.0f };
    float mCutoffHz   { 120.0f };
    float mB0 { 1.0f }, mB1 { 0.0f }, mB2 { 0.0f };
    float mA1 { 0.0f }, mA2 { 0.0f };
    float mS1_1 { 0.0f }, mS2_1 { 0.0f };
    float mS1_2 { 0.0f }, mS2_2 { 0.0f };
};

// ============================================================================
// LowBandModalMatrix: Master Decoupled Low-Frequency Reverberation Engine
// ============================================================================
class LowBandModalMatrix {
public:
    LowBandModalMatrix() noexcept;
    ~LowBandModalMatrix() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setParameters(const LowBandModalParams& params) noexcept;

    // Single-sample processing
    void processSample(float inL, float inR,
                       float& highOutL, float& highOutR,
                       float& lowReverbOutL, float& lowReverbOutR) noexcept;

    // Block processing (real-time safe: zero dynamic allocations)
    void processBlock(const float* inL, const float* inR,
                      float* highOutL, float* highOutR,
                      float* lowReverbOutL, float* lowReverbOutR,
                      int numSamples) noexcept;

    // Standalone processing accessors for testing
    void processModalOnly(float lowInL, float lowInR, float& lowOutL, float& lowOutR) noexcept;
    void processCrossoverOnly(float inL, float inR, float& lowL, float& lowR, float& highL, float& highR) noexcept;

    // Subsystem accessors
    LinkwitzRiley4& getCrossover() noexcept { return mCrossover; }
    TransientPunchDetector& getPunchDetector() noexcept { return mPunchDetector; }
    SubBassEllipticalFilter& getEllipticalFilter() noexcept { return mEllipticalFilter; }

    // Telemetry
    [[nodiscard]] float getDuckingGain() const noexcept { return mPunchDetector.getLastDuckingGain(); }
    [[nodiscard]] float getLowBandEnergy() const noexcept { return mLastLowEnergy; }

private:
    void updateDecayCoefficients() noexcept;
    [[nodiscard]] size_t findClosestPrime(size_t target) const noexcept;

    float mSampleRate { 48000.0f };

    // Subsystems
    LinkwitzRiley4          mCrossover;
    TransientPunchDetector  mPunchDetector;
    SubBassEllipticalFilter mEllipticalFilter;

    // 4-Line Modal Matrix Delay Lines
    static constexpr size_t kNumModalLines = 4;
    static constexpr std::array<float, kNumModalLines> kBaseDelayTimes = {
        0.0710f, 0.0890f, 0.1070f, 0.1260f // 71ms, 89ms, 107ms, 126ms (Web Audio exact)
    };

    std::array<size_t, kNumModalLines> mDelayLengths { 0, 0, 0, 0 };
    std::array<size_t, kNumModalLines> mWriteIndices { 0, 0, 0, 0 };
    std::array<std::vector<float>, kNumModalLines> mDelayBuffers;
    std::array<float, kNumModalLines> mDecayCoeffs { 0.8f, 0.8f, 0.8f, 0.8f };
    std::array<Biquad, kNumModalLines> mDcBlockFilters;
    std::array<Biquad, kNumModalLines> mDampingFilters;

    // Cached parameters
    LowBandModalParams mParams;
    float mLastLowEnergy { 0.0f };
    int mSubBlockCounter { 0 };
};

} // namespace rb26
