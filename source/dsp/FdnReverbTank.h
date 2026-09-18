#pragma once

#include "DspMath.h"
#include "TailModulator.h"
#include "ManifoldDelayNetwork.h"
#include <array>
#include <vector>
#include <cmath>
#include <cstddef>
#include <optional>
#include <algorithm>

namespace rb26 {

/**
 * Lightweight, zero-latency 1st-order DC blocker for FDN recirculation.
 * Prevents the Householder -1 eigenvalue common-mode from rectifying into
 * DC offset or low-frequency limit cycles during freeze hold.
 */
struct DcBlocker {
    float x1 { 0.0f };
    float y1 { 0.0f };
    float R { 0.99935f }; // Default ~5 Hz at 48 kHz
    void setCutoff(float fcHz, double sampleRate) noexcept {
        const float fs = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
        R = std::clamp(1.0f - (kTwoPi * fcHz / fs), 0.99f, 0.99995f);
    }
    void reset() noexcept { x1 = 0.0f; y1 = 0.0f; }
    [[nodiscard]] inline float process(float x) noexcept {
        const float y = x - x1 + R * y1;
        x1 = flushDenormal(x);
        y1 = flushDenormal(y);
        return y1;
    }
};

class FdnReverbTank {
public:
    static constexpr size_t kNumLines = 8;
    static constexpr size_t kNumAllpass = 4;
    /**
     * Acoustic Rationale for Freeze Time Constants:
     * A 25 ms time constant (tau = 0.025s) corresponds to an asymptotic ~99% settling
     * time of approximately 75-100 ms, striking an optimal acoustic balance:
     * 1. kFreezeInputTimeConstantSec:
     *    Smoothly isolates the input excitation and pitch feedback without abrupt
     *    waveform truncation clicks, while arresting new acoustic energy rapidly enough
     *    to isolate the exact reverberant bloom the user intended to freeze.
     * 2. kFreezeLoopTimeConstantSec:
     *    Smoothly ramps the recirculating feedback gains to unity (1.0f) and transparently
     *    bypasses HF loop damping absorption, eliminating DC offsets, phase jumps, or
     *    audible spectral thumps across the 8x8 Householder feedback matrix while ensuring
     *    lossless, bounded infinite sustain.
     */
    static constexpr float kFreezeInputTimeConstantSec = 0.025f;
    static constexpr float kFreezeLoopTimeConstantSec  = 0.025f;

    FdnReverbTank() noexcept { prepare(48000.0); }
    ~FdnReverbTank() noexcept = default;

    void prepare(double sampleRate, float maxRoomSize = 4.0f) noexcept;
    void reset() noexcept;

    /**
     * Consolidates all FDN reverb tank and manifold topology parameters into a single
     * real-time safe update.
     *
     * @param roomSize          Geometric room scale factor (clamped [0.1, maxRoomSize]).
     * @param decayRt60Sec      Target reverberation decay time T60 (clamped [0.2, 30.0] sec).
     * @param highDampingHz     High-frequency air absorption cutoff (clamped [500.0, 20000.0] Hz).
     * @param diffusionDensity  Allpass input/loop diffusion density (clamped [0.0, 1.0]).
     * @param freezeHold        Infinite sustain hold mode flag.
     * @param tailModRateHz     LFO tail modulation frequency in Hz.
     * @param tailModDepthMs    LFO excursion depth in milliseconds.
     * @param tailBloomMs       Envelope transient bloom onset time in milliseconds.
     * @param manifoldType      Optional manifold topology selection. When std::nullopt,
     *                          the currently active manifold topology is preserved.
     */
    void setParameters(float roomSize, float decayRt60Sec, float highDampingHz,
                       float diffusionDensity, bool freezeHold,
                       float tailModRateHz, float tailModDepthMs, float tailBloomMs,
                       std::optional<ManifoldType> manifoldType = std::nullopt) noexcept;

    void setManifold(ManifoldType manifoldType) noexcept;
    [[nodiscard]] ManifoldType getActiveManifold() const noexcept { return mManifoldNetwork.getActiveManifold(); }
    [[nodiscard]] ManifoldDelayNetwork& getManifoldNetwork() noexcept { return mManifoldNetwork; }
    [[nodiscard]] const ManifoldDelayNetwork& getManifoldNetwork() const noexcept { return mManifoldNetwork; }
    [[nodiscard]] float getRoomSize() const noexcept { return mRoomSize; }
    [[nodiscard]] float getMaxRoomSize() const noexcept { return mMaxRoomSize; }

    // Process stereo high-band audio + pitch feedback recirculation
    void processSample(float inL, float inR, float pitchFbL, float pitchFbR,
                       float& outLateL, float& outLateR) noexcept;

    void processBlock(const float* inL, const float* inR,
                      const float* pitchFbL, const float* pitchFbR,
                      float* outLateL, float* outLateR,
                      int numSamples) noexcept;

private:
    double mSampleRate { 48000.0 };
    float mRoomSize { 0.65f };
    float mMaxRoomSize { 4.0f };
    float mDecayRt60 { 3.5f };
    float mHighDampingHz { 1800.0f };
    float mDiffusionDensity { 0.75f };
    bool mFreezeHold { false };
    ManifoldType mCurrentManifold { ManifoldType::PoincareHyperbolic };

    TailModulator mTailModulator;
    ManifoldDelayNetwork mManifoldNetwork;

    // Input Allpass Diffusers
    static constexpr std::array<size_t, kNumAllpass> kBaseAllpassLengths = {{ 227, 337, 449, 563 }};
    std::array<size_t, kNumAllpass> mAllpassLengths {};
    std::array<std::vector<float>, kNumAllpass> mAllpassBuffers {};
    std::array<size_t, kNumAllpass> mAllpassWriteIndices {};

    std::array<float, kNumLines> mFeedbackGains {};
    std::array<DcBlocker, kNumLines> mDcBlockers {};
    OnePoleSmoother mFreezeInputSmoother;
    OnePoleSmoother mFreezeLoopSmoother;

    inline float processAllpass(size_t index, float input, float density) noexcept;
    void updateDecayGains() noexcept;
};

} // namespace rb26
