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

    DualTapDelayPitchShifter() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setInterval(int semitones) noexcept;

    [[nodiscard]] float processSample(float input) noexcept;

private:
    [[nodiscard]] inline float readHermite(float readPos) const noexcept;

    float mSampleRate { 48000.0f };
    float mWindowSec { 0.050f };       // 50 ms default window
    float mWindowSamples { 2400.0f };
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

    // Internal recirculation state for pitch feedback
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
};

// Type alias for Explorer 1 naming
using BidirectionalPitchShifter = PitchShifter;

} // namespace rb26
