#pragma once

#include "DspMath.h"
#include "BoundedSaturator.h"
#include "ShepardPitchSpiral.h"
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace rb26 {

// ============================================================================
// DualTapDelayPitchShifter: Mono Core Pitch Shifter
// Dual-tap circular delay line, constant-power sine windows, Hermite interpolation
// ============================================================================
class DualTapDelayPitchShifter {
public:
    static constexpr int kMaxCapacity = 65536;
    static constexpr int kBufferMask = kMaxCapacity - 1;

    DualTapDelayPitchShifter() noexcept { prepare(48000.0); }

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setInterval(int semitones) noexcept;
    void setQualityMode(PitchQualityMode mode) noexcept { mQualityMode = mode; }
    [[nodiscard]] PitchQualityMode getQualityMode() const noexcept { return mQualityMode; }
    [[nodiscard]] bool shouldUseLinear() const noexcept {
        if (mQualityMode == PitchQualityMode::FastLinear) return true;
        if (mQualityMode == PitchQualityMode::HiQHermite) return false;
        return (mSampleRate >= 88200.0f);
    }

    [[nodiscard]] float processSample(float input) noexcept;

private:
    [[nodiscard]] inline float readHermite(float readPos) const noexcept;
    [[nodiscard]] inline float readLinear(float readPos) const noexcept;

    PitchQualityMode mQualityMode { PitchQualityMode::Auto };
    float mSampleRate { 48000.0f };
    float mWindowSec { 0.050f };       // 50 ms default window
    float mWindowSamples { 2400.0f };
    float mTargetWindowSamples { 2400.0f };
    int mSemitones { 12 };
    float mRatio { 2.0f };             // 2^(semitones / 12)
    float mPhaseInc { 0.0f };
    float mPhase { 0.0f };             // Phase accumulator in [0, 1)

    std::vector<float> mDelayBuffer;   // Pre-allocated for 192 kHz (65536 samples)
    int mWriteIndex { 0 };
    int mBufferSize { kMaxCapacity };
};

// ============================================================================
// Dedicated Pitch Loop Filters
// ============================================================================
class ShimmerLoopFilter {
public:
    void prepare(double sampleRate) noexcept {
        const float fs = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        mDcBlockHighpass.configure(Biquad::Type::Highpass, fs, 250.0f, 0.7071f);
        mPassHighpass.configure(Biquad::Type::Highpass, fs, 600.0f, 0.7071f);
        mPassLowpass.configure(Biquad::Type::Lowpass, fs, 8000.0f, 0.7071f);
    }

    void reset() noexcept {
        mDcBlockHighpass.reset();
        mPassHighpass.reset();
        mPassLowpass.reset();
    }

    [[nodiscard]] inline float process(float x) noexcept {
        // Cascade: 250Hz HPF -> 600Hz HPF -> 8000Hz LPF
        const float s1 = mDcBlockHighpass.process(x);
        const float s2 = mPassHighpass.process(s1);
        return mPassLowpass.process(s2);
    }

private:
    Biquad mDcBlockHighpass;
    Biquad mPassHighpass;
    Biquad mPassLowpass;
};

class DimmerLoopFilter {
public:
    void prepare(double sampleRate) noexcept {
        const float fs = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        mPassHighpass.configure(Biquad::Type::Highpass, fs, 60.0f, 0.7071f);
        mPassLowpass.configure(Biquad::Type::Lowpass, fs, 1200.0f, 0.7071f);
    }

    void reset() noexcept {
        mPassHighpass.reset();
        mPassLowpass.reset();
    }

    [[nodiscard]] inline float process(float x) noexcept {
        // Cascade: 60Hz HPF -> 1200Hz LPF
        const float s1 = mPassHighpass.process(x);
        return mPassLowpass.process(s1);
    }

private:
    Biquad mPassHighpass;
    Biquad mPassLowpass;
};

// ============================================================================
// PitchShifter: Top-Level Stereo Shimmer + Dimmer Diffusion Engine
// Integrated with Continuous Shepard-Risset Pitch Spiral & Partch Lattice
// ============================================================================
class PitchShifter {
public:
    PitchShifter() noexcept;

    void prepare(double sampleRate, int maxBlockSize = 512) noexcept;
    void reset() noexcept;

    // Classic parameter configuration (thread-safe, smoothed internally)
    void setParameters(float shimmerSend,
                       float dimmerSend,
                       int shimmerInterval,
                       int dimmerInterval,
                       float pitchBlend,
                       float pitchFeedback) noexcept;

    // Continuous Shepard-Risset pitch spiral & Partch lattice configuration
    void setSpiralParameters(int spiralMode,
                             float spiralRateHz,
                             float spiralDepth,
                             int partchInterval = 1) noexcept;

    // Quality mode switch (Auto, HiQHermite, FastLinear)
    void setQualityMode(PitchQualityMode mode) noexcept {
        mQualityMode = mode;
        mShimmerShifterL.setQualityMode(mode);
        mShimmerShifterR.setQualityMode(mode);
        mDimmerShifterL.setQualityMode(mode);
        mDimmerShifterR.setQualityMode(mode);
        mShepardSpiral.setQualityMode(mode);
    }
    [[nodiscard]] PitchQualityMode getQualityMode() const noexcept { return mQualityMode; }

    [[nodiscard]] ShepardPitchSpiral& getShepardSpiral() noexcept { return mShepardSpiral; }
    [[nodiscard]] const ShepardPitchSpiral& getShepardSpiral() const noexcept { return mShepardSpiral; }

    // Hard real-time processing callback (0 dynamic allocations, 0 locks)
    void process(const float* inL,
                 const float* inR,
                 float* outL,
                 float* outR,
                 int numSamples) noexcept;

    // Single-sample processing
    void processSample(float inL, float inR, float& outL, float& outR) noexcept;

    // Checks whether shimmer or dimmer is actively contributing
    [[nodiscard]] inline bool isActive() const noexcept {
        const float sTarget = mShimmerSendSmoother.getTarget();
        const float dTarget = mDimmerSendSmoother.getTarget();
        const float sCurrent = mShimmerSendSmoother.getCurrent();
        const float dCurrent = mDimmerSendSmoother.getCurrent();

        // 1. Both sends are zero -> strictly inactive regardless of blend
        if (sTarget <= 0.001f && dTarget <= 0.001f && sCurrent <= 0.001f && dCurrent <= 0.001f) {
            return false;
        }

        const float curBlend = mPitchBlendSmoother.getCurrent();
        const float targetBlend = mPitchBlendSmoother.getTarget();

        // 2. Shimmer is active if shimmer send > 0 AND blend is not hard-panned to Dimmer (-1.0)
        const bool shimSendActive = (sTarget > 0.001f || sCurrent > 0.001f);
        const bool shimBlendActive = (curBlend > -0.98f || targetBlend > -0.98f);
        const bool shimActive = shimSendActive && shimBlendActive;

        // 3. Dimmer is active if dimmer send > 0 AND blend is not hard-panned to Shimmer (+1.0)
        const bool dimSendActive = (dTarget > 0.001f || dCurrent > 0.001f);
        const bool dimBlendActive = (curBlend < 0.98f || targetBlend < 0.98f);
        const bool dimActive = dimSendActive && dimBlendActive;

        return shimActive || dimActive;
    }

private:
    float mSampleRate { 48000.0f };

    // Shimmer stereo path (+12, +24, +7 st)
    DualTapDelayPitchShifter mShimmerShifterL;
    DualTapDelayPitchShifter mShimmerShifterR;
    ShimmerLoopFilter mShimmerFilterL;
    ShimmerLoopFilter mShimmerFilterR;
    BoundedSaturator mShimmerSaturatorL;
    BoundedSaturator mShimmerSaturatorR;

    // Dimmer stereo path (-12, -24 st)
    DualTapDelayPitchShifter mDimmerShifterL;
    DualTapDelayPitchShifter mDimmerShifterR;
    DimmerLoopFilter mDimmerFilterL;
    DimmerLoopFilter mDimmerFilterR;
    BoundedSaturator mDimmerSaturatorL;
    BoundedSaturator mDimmerSaturatorR;

    // Continuous Shepard-Risset Pitch Spiral & Partch Lattice Engine
    ShepardPitchSpiral mShepardSpiral;

    // Internal recirculation state for pitch feedback with decoupled circulation buffers
    static constexpr size_t kCircCapacity = 32768;
    static constexpr size_t kCircMask = kCircCapacity - 1;
    std::vector<float> mShimmerCircBufferL;
    std::vector<float> mShimmerCircBufferR;
    std::vector<float> mDimmerCircBufferL;
    std::vector<float> mDimmerCircBufferR;
    size_t mCircWriteIndex { 0 };
    float mRecircShimmerL { 0.0f };
    float mRecircShimmerR { 0.0f };
    float mRecircDimmerL { 0.0f };
    float mRecircDimmerR { 0.0f };

    // Parameter smoothers
    OnePoleSmoother mShimmerSendSmoother;
    OnePoleSmoother mDimmerSendSmoother;
    OnePoleSmoother mPitchBlendSmoother;
    OnePoleSmoother mPitchFeedbackSmoother;
    OnePoleSmoother mSpiralDepthSmoother;
    OnePoleSmoother mSpiralRateSmoother;

    int mSpiralMode { 1 };         // 0: BarberDimmer, 1: BarberShimmer, 2: PartchLattice
    float mSpiralRateHz { 0.10f }; // 0.01 - 2.0 Hz
    float mSpiralDepth { 0.0f };   // 0.0 (classic dual-tap) to 1.0 (pure continuous spiral)
    int mPartchInterval { 1 };     // Default: -12 st (sub-octave)

    int mCurrentShimmerInterval { 12 };
    int mCurrentDimmerInterval { -12 };
    PitchQualityMode mQualityMode { PitchQualityMode::Auto };
};

// Type alias for Explorer 1 naming
using BidirectionalPitchShifter = PitchShifter;

} // namespace rb26
