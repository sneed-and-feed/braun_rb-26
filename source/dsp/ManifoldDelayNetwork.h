#pragma once

#include "DspMath.h"
#include "TailModulator.h"
#include <array>
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>

namespace rb26 {

/**
 * ManifoldType: The four selectable non-Euclidean delay geometries.
 */
enum class ManifoldType : int {
    PoincareHyperbolic  = 0,
    WhisperingGallery   = 1,
    AnharmonicPlate     = 2,
    StockhausenKlangdom = 3
};

/**
 * FirstOrderAllpass: 1st-order dispersive allpass filter
 * H(z) = (a + z^-1) / (1 + a * z^-1)
 * Canonical one-multiplier form with denormal flushing.
 */
class FirstOrderAllpass {
public:
    void reset() noexcept { mState = 0.0f; }
    void setCoeff(float a) noexcept { mCoeff = std::clamp(a, -0.99f, 0.99f); }
    [[nodiscard]] inline float process(float x) noexcept {
        const float v = x - mCoeff * mState;
        const float y = mCoeff * v + mState;
        mState = flushDenormal(v);
        return flushDenormal(y);
    }
private:
    float mCoeff { 0.0f };
    float mState { 0.0f };
};

/**
 * ManifoldDelayNetwork:
 * Real-time non-Euclidean spatial delay network for the RB-26 reverb tank.
 * Manages 8 delay lines with geometry-governed delays, per-line dispersion
 * allpasses, manifold-specific resonant loop filters, and spatial stereo projections.
 *
 * Guarantees:
 * - 0 dynamic heap allocations in audio thread (pre-allocated for 192 kHz)
 * - Power-of-two bitmask indexing (2^17 = 131,072 samples)
 * - Click-free manifold and room size parameter switching via OnePoleSmoother
 * - Full denormal protection
 */
class ManifoldDelayNetwork {
public:
    static constexpr size_t kNumLines = 8;
    // Sized for 192 kHz with expanded dimensions (up to 4.0x room size):
    // 131,072 samples (~682.7 ms at 192 kHz, ~2.73 s at 48 kHz), power-of-two for bitwise masking
    static constexpr size_t kBufferCapacity = 131072;
    static constexpr size_t kBufferMask = kBufferCapacity - 1;

    // Precomputed Horocycle cosh(xi * k / 7) values (xi = 1.760742) for ultra-fast room size evaluation
    static constexpr std::array<double, 8> kPoincareCosh = {{
        1.0,
        1.0318019661180760,
        1.1292305945702540,
        1.2984827292384680,
        1.5503234714269785,
        1.9007708826362466,
        2.3721147962611600,
        2.9943345386438420
    }};

    // Precomputed Whispering Gallery Airy radial factors: (1.0 - a_{k+1} / (2 * pi * (k + 3)))
    static constexpr std::array<double, 8> kWhisperingRadialFactors = {{
        0.8759595494062371,
        0.8373456598785636,
        0.8242751228841894,
        0.8199769766924574,
        0.8193788386527336,
        0.8205000646787761,
        0.8224507360316213,
        0.8247938941507753
    }};

    // Precomputed Anharmonic Plate inverse mode factors: 1.0 / sqrt(m^2 + 0.08 * n^2)
    static constexpr std::array<double, 8> kPlateInvLambda = {{
        0.9622504486493761,
        0.8703882797784892,
        0.4950737714883371,
        0.4811252243246881,
        0.3318616557999860,
        0.3275608910402092,
        0.3207501495497921,
        0.2493773340269082
    }};

    // Precomputed Stockhausen Klangdom scale factors: (1.0 + delta_k)
    static constexpr std::array<double, 8> kKlangdomScaleFactors = {{
        0.921, 0.947, 0.977, 1.000, 1.023, 1.053, 1.079, 1.107
    }};

    ManifoldDelayNetwork() noexcept;
    ~ManifoldDelayNetwork() noexcept = default;

    // Hard real-time lifecycle
    void prepare(double sampleRate, float maxRoomSize = 4.0f) noexcept;
    void reset() noexcept;

    // Parameter updates
    void setParameters(ManifoldType type, float roomSize, float highDampingHz, float diffusionDensity) noexcept;
    void setParameters(ManifoldType type, float roomSize, float highDampingHz) noexcept;
    void setManifold(ManifoldType type) noexcept;

    // Read 8 delay lines with Hermite cubic interpolation, apply dispersion allpasses and loop filters
    // inExcursions: 8 modulation excursions in samples from TailModulator
    // outFiltered: 8 filtered and dispersed delay outputs ready for Householder matrix reflection
    // freezeAmount: 0.0f for normal operation, 1.0f to bypass HF damping during freeze hold
    void readAndFilterLines(const std::array<float, kNumLines>& inExcursions,
                            std::array<float, kNumLines>& outFiltered,
                            float freezeAmount = 0.0f) noexcept;

    // Write reflected and saturated feedback samples back to the 8 delay lines
    void writeFeedback(const std::array<float, kNumLines>& inSaturated) noexcept;

    // Extract stereo late reverberation according to active manifold spatial geometry
    void extractStereo(const std::array<float, kNumLines>& lines,
                       float& outLateL, float& outLateR) noexcept;

    [[nodiscard]] ManifoldType getActiveManifold() const noexcept { return mCurrentManifold; }
    [[nodiscard]] const std::array<size_t, kNumLines>& getNominalLengths() const noexcept { return mNominalLengths; }
    [[nodiscard]] float getRoomSize() const noexcept { return mRoomSize; }
    [[nodiscard]] float getMaxRoomSize() const noexcept { return mMaxRoomSize; }
    [[nodiscard]] float getDiffusionDensity() const noexcept { return mDiffusionDensity; }

private:
    double mSampleRate { 48000.0 };
    ManifoldType mCurrentManifold { ManifoldType::PoincareHyperbolic };
    float mRoomSize { 0.65f };
    float mMaxRoomSize { 4.0f };
    float mHighDampingHz { 6500.0f };
    float mDiffusionDensity { 0.75f };
    float mDispCoeff1 { 0.0f };
    float mDispCoeff2 { 0.0f };
    bool  mIsFirstSet { true };

    // 8 Delay line circular buffers pre-allocated for 192 kHz
    std::array<std::vector<float>, kNumLines> mBuffers;
    std::array<size_t, kNumLines> mWriteIndices {};

    // Delay lengths and clickless slewing
    std::array<size_t, kNumLines> mNominalLengths {};
    std::array<OnePoleSmoother, kNumLines> mLengthSmoothers;

    // Per-line Dispersion Allpass Stages (2 stages per line)
    std::array<FirstOrderAllpass, kNumLines> mDispersionStage1;
    std::array<FirstOrderAllpass, kNumLines> mDispersionStage2;

    // Loop Filters
    // 1. One-pole air absorption damping
    std::array<float, kNumLines> mDampingStates {};
    float mDampingAlpha { 0.5f };

    // 2. Whispering Gallery Caustic Peaking Filter (9.5 kHz, +3.5 dB, Q = 2.8) + ultrasonic lowpass
    std::array<BiquadDirectForm2T, kNumLines> mCausticPeaking;
    std::array<OnePoleLowpass, kNumLines> mUltrasonicLowpass;

    // 3. Anharmonic Plate Sitka Spruce Formants (A0: 95 Hz, T1: 320 Hz, Wood: 2400 Hz)
    std::array<BiquadDirectForm2T, kNumLines> mSpruceA0;
    std::array<BiquadDirectForm2T, kNumLines> mSpruceT1;
    std::array<BiquadDirectForm2T, kNumLines> mSpruceWood;

    // Spatial Panning / Extraction
    // Whispering Gallery: Rotating circular spatial vector
    float mCausticRotationAngle { 0.0f };
    float mCausticRotationDelta { 0.0f };

    // Spatial output weight smoothers for click-free manifold transitions
    std::array<OnePoleSmoother, kNumLines> mSpatialWeightsL;
    std::array<OnePoleSmoother, kNumLines> mSpatialWeightsR;

    void updateManifoldGeometry() noexcept;
    void updateFilterCoefficients() noexcept;
    void updateSpatialWeights() noexcept;

    // Manifold-specific delay length calculation routines
    void computePoincareLengths(std::array<size_t, kNumLines>& lengths) const noexcept;
    void computeWhisperingLengths(std::array<size_t, kNumLines>& lengths) const noexcept;
    void computePlateLengths(std::array<size_t, kNumLines>& lengths) const noexcept;
    void computeKlangdomLengths(std::array<size_t, kNumLines>& lengths) const noexcept;
};

} // namespace rb26
