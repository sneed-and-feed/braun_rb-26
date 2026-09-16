#pragma once

#include "DspMath.h"
#include <array>
#include <vector>
#include <cstddef>

namespace rb26 {

class EarlyReflections {
public:
    static constexpr size_t kNumTaps = 12;

    EarlyReflections() noexcept { prepare(48000.0); }
    ~EarlyReflections() noexcept = default;

    void prepare(double sampleRate, float maxRoomSize = 2.0f) noexcept;
    void reset() noexcept;
    void setParameters(float roomSize, float diffusionDensity = 0.0f) noexcept;

    // Guaranteed 100% time-invariant (0% modulation)
    void processSample(float inL, float inR, float& outL, float& outR) noexcept;

    void processBlock(const float* inL, const float* inR,
                      float* outL, float* outR,
                      int numSamples) noexcept;

private:
    struct TapConfig {
        float baseDelayMs;
        float gain;
        float pan; // -1.0 to +1.0
    };

    static constexpr std::array<TapConfig, kNumTaps> kTapConfigs = {{
        {   7.3f,  0.82f, -0.75f },
        {  11.3f, -0.76f,  0.75f },
        {  17.9f,  0.70f, -0.55f },
        {  23.9f, -0.64f,  0.55f },
        {  31.1f,  0.58f, -0.40f },
        {  43.1f, -0.52f,  0.40f },
        {  59.3f,  0.46f, -0.30f },
        {  71.9f, -0.40f,  0.30f },
        {  89.1f,  0.35f, -0.20f },
        { 103.1f, -0.30f,  0.20f },
        { 119.3f,  0.25f, -0.10f },
        { 137.3f, -0.20f,  0.10f }
    }};

    static constexpr size_t kBufferCapacity = 65536;
    static constexpr size_t kBufferMask = kBufferCapacity - 1;

    double mSampleRate { 48000.0 };
    float mRoomSize { 1.0f };

    std::array<size_t, kNumTaps> mTapDelaysSamples {};
    std::array<float, kNumTaps> mTapGainsL {};
    std::array<float, kNumTaps> mTapGainsR {};

    static constexpr int kCrossfadeSamples = 256;
    int mCrossfadeRemaining { 0 };
    std::array<size_t, kNumTaps> mOldTapDelaysSamples {};
    std::array<float, kNumTaps> mOldTapGainsL {};
    std::array<float, kNumTaps> mOldTapGainsR {};

    std::vector<float> mBufferL;
    std::vector<float> mBufferR;
    size_t mWriteIndex { 0 };

    float mDiffusionDensity { 0.0f };
    static constexpr size_t kNumAllpass = 4;
    static constexpr std::array<size_t, kNumAllpass> kBaseAllpassLengths = {{ 149, 211, 163, 223 }};
    std::array<size_t, kNumAllpass> mAllpassLengths {};
    std::array<std::vector<float>, kNumAllpass> mAllpassBuffers {};
    std::array<size_t, kNumAllpass> mAllpassWriteIndices {};

    inline float processAllpass(size_t index, float input, float density) noexcept;
    void updateTaps() noexcept;
};

} // namespace rb26
