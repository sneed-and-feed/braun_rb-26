#pragma once

#include "DspMath.h"
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace rb26 {

/**
 * ShepardPitchSpiral:
 * 4-Voice Continuous Pitch Spiral (Barber-Pole Dimmer & Shimmer) and
 * Harry Partch Microtonal Sub-Harmonic Undertone Lattice Engine.
 *
 * Guarantees:
 * - 0 dynamic heap allocations in audio processing (pre-allocated for 192 kHz)
 * - Power-of-two bitmask indexing (2^16 = 65,536 samples)
 * - 4-voice circular log-frequency glide with raised-cosine spectral envelope
 * - Strictly constant power sum: sum(A_m^2) == 1.5, normalized by sqrt(2/3) for 0 dB ripple
 * - C^1 boundary continuity: 0 clicks, 0 boundary reset thumps
 * - Full denormal protection
 */
class ShepardPitchSpiral {
public:
    // Operational Modes
    enum class SpiralMode : int {
        BarberDimmer  = 0,   // Continuous downward spiral into sub-bass depths
        BarberShimmer = 1,   // Continuous upward celestial sparkle spiraling into ultra-highs
        PartchLattice = 2,   // Microtonal Harry Partch Utonality sub-harmonic lattice
        Bypass        = 3    // Pass-through
    };

    // Partch Undertone Series Index (1/1 to 1/6)
    enum class PartchInterval : int {
        Unison_1_1          = 0, //   0.00 st (1/1 fundamental)
        SubOctave_1_2       = 1, // -12.00 st (1/2 sub-octave)
        SubFifth_1_3        = 2, // -19.02 st (1/3 perfect fifth below sub-octave)
        DoubleSubOctave_1_4 = 3, // -24.00 st (1/4 double sub-octave)
        SubMajorThird_1_5   = 4, // -27.86 st (1/5 just major third below 2 octaves)
        SubFifth2Oct_1_6    = 5  // -31.02 st (1/6 perfect fifth below 2 octaves)
    };

    static constexpr int kNumVoices = 4;
    static constexpr int kMaxCapacity = 65536; // Sized for 192 kHz (341 ms)
    static constexpr int kBufferMask = kMaxCapacity - 1;

    // Mathematical Constants
    static constexpr float kPowerSum = 1.500000f;
    static constexpr float kNormGain = 0.816496580927726f; // sqrt(2/3)

    // Partch Undertone Lookup Tables
    static constexpr std::array<float, 6> kPartchRatios = {{
        1.0f / 1.0f, // 1.000000f (0 st)
        1.0f / 2.0f, // 0.500000f (-12 st)
        1.0f / 3.0f, // 0.333333f (-19.01955f st)
        1.0f / 4.0f, // 0.250000f (-24 st)
        1.0f / 5.0f, // 0.200000f (-27.86314f st)
        1.0f / 6.0f  // 0.166667f (-31.01955f st)
    }};

    static constexpr std::array<float, 6> kPartchSemitones = {{
         0.000000f,
       -12.000000f,
       -19.019550f,
       -24.000000f,
       -27.863137f,
       -31.019550f
    }};

    ShepardPitchSpiral() noexcept;
    ~ShepardPitchSpiral() noexcept = default;

    // Hard real-time lifecycle
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Parameter setters (thread-safe, smoothed internally)
    void setMode(SpiralMode mode) noexcept;
    void setRateHz(float rateHz) noexcept;
    void setDepth(float depth) noexcept;
    void setPartchInterval(int intervalIndex) noexcept;
    void setPartchPolyphonic(bool polyphonic) noexcept;
    void setWindowSec(float sec) noexcept;
    void setStereoSpread(float spread) noexcept;
    void setQualityMode(PitchQualityMode mode) noexcept { mQualityMode = mode; }
    [[nodiscard]] PitchQualityMode getQualityMode() const noexcept { return mQualityMode; }
    [[nodiscard]] bool shouldUseLinear() const noexcept {
        if (mQualityMode == PitchQualityMode::FastLinear) return true;
        if (mQualityMode == PitchQualityMode::HiQHermite) return false;
        return (mSampleRate >= 88200.0f);
    }

    // Getters for telemetry and verification
    [[nodiscard]] SpiralMode getMode() const noexcept { return mMode; }
    [[nodiscard]] float getRateHz() const noexcept { return mRateHz; }
    [[nodiscard]] float getDepth() const noexcept { return mDepthSmoother.getTarget(); }
    [[nodiscard]] float getSpiralPhase() const noexcept { return mSpiralPhase; }
    [[nodiscard]] float getVoiceRatio(int voiceIdx) const noexcept;
    [[nodiscard]] float getVoiceWeight(int voiceIdx) const noexcept;

    // Real-time audio processing (0 heap allocations, 0 locks)
    void process(const float* inL,
                 const float* inR,
                 float* outL,
                 float* outR,
                 int numSamples) noexcept;

    // Single-sample stereo processing
    void processSample(float inL, float inR, float& outL, float& outR) noexcept;

    // Single-sample mono processing
    [[nodiscard]] float processMonoSample(float input, int channel = 0) noexcept;

private:
    struct ShepardVoice {
        float grainPhase { 0.0f };       // Grain delay phase in [0, 1)
        float currentRatio { 1.0f };     // Instantaneous frequency ratio r_m
        float currentWeight { 0.0f };    // Instantaneous raised-cosine gain A_m
    };

    [[nodiscard]] inline float readHermite(const std::vector<float>& buffer, float readPos) const noexcept;
    [[nodiscard]] inline float readLinear(const std::vector<float>& buffer, float readPos) const noexcept;

    PitchQualityMode mQualityMode { PitchQualityMode::Auto };
    float mSampleRate { 48000.0f };
    SpiralMode mMode { SpiralMode::BarberShimmer };
    float mRateHz { 0.10f };             // Glissando rate (0.01 - 2.0 Hz)
    float mWindowSec { 0.060f };         // Grain window size (60 ms default)
    float mWindowSamples { 2880.0f };
    float mStereoSpread { 0.25f };       // Grain phase offset between L and R
    bool  mPartchPolyphonic { false };   // Default false: focused single undertone; true: 4-voice cluster

    // Spiral master phase accumulator in [0.0, 4.0)
    float mSpiralPhase { 0.0f };

    // 4 Voices per stereo channel
    std::array<ShepardVoice, kNumVoices> mVoicesL;
    std::array<ShepardVoice, kNumVoices> mVoicesR;

    // Circular delay buffers pre-allocated for 192 kHz
    std::vector<float> mDelayBufferL;
    std::vector<float> mDelayBufferR;
    int mWriteIndex { 0 };

    // Parameter smoothers
    OnePoleSmoother mDepthSmoother;
    OnePoleSmoother mRateSmoother;
    OnePoleSmoother mPartchRatioSmoother;

    int mPartchIndex { 1 };              // Default: -12 st (sub-octave)
};

} // namespace rb26
