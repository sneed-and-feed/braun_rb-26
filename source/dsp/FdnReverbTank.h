#pragma once

#include "DspMath.h"
#include "TailModulator.h"
#include "ManifoldDelayNetwork.h"
#include <array>
#include <vector>
#include <cmath>
#include <cstddef>

namespace rb26 {

class FdnReverbTank {
public:
    static constexpr size_t kNumLines = 8;
    static constexpr size_t kNumAllpass = 4;

    FdnReverbTank() noexcept = default;
    ~FdnReverbTank() noexcept = default;

    void prepare(double sampleRate, float maxRoomSize = 2.0f) noexcept;
    void reset() noexcept;

    // 8-parameter overload maintaining 100% backward compatibility
    void setParameters(float roomSize, float decayRt60Sec, float highDampingHz,
                       float diffusionDensity, bool freezeHold,
                       float tailModRateHz, float tailModDepthMs, float tailBloomMs) noexcept;

    // 9-parameter overload including manifoldType
    void setParameters(float roomSize, float decayRt60Sec, float highDampingHz,
                       float diffusionDensity, bool freezeHold,
                       float tailModRateHz, float tailModDepthMs, float tailBloomMs,
                       ManifoldType manifoldType) noexcept;

    void setManifold(ManifoldType manifoldType) noexcept;
    [[nodiscard]] ManifoldType getActiveManifold() const noexcept { return mManifoldNetwork.getActiveManifold(); }
    [[nodiscard]] ManifoldDelayNetwork& getManifoldNetwork() noexcept { return mManifoldNetwork; }
    [[nodiscard]] const ManifoldDelayNetwork& getManifoldNetwork() const noexcept { return mManifoldNetwork; }

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
    float mDecayRt60 { 3.5f };
    float mHighDampingHz { 6500.0f };
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
    OnePoleSmoother mFreezeInputSmoother;
    OnePoleSmoother mFreezeLoopSmoother;

    inline float processAllpass(size_t index, float input, float density) noexcept;
    void updateDecayGains() noexcept;
};

} // namespace rb26
