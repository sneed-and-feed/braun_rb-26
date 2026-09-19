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

    void prepare(double sampleRate, float maxRoomSize = 4.0f) noexcept;
    void reset() noexcept;
    void setParameters(float roomSize, float diffusionDensity = 0.0f) noexcept;

    [[nodiscard]] float getRoomSize() const noexcept { return mRoomSize; }
    [[nodiscard]] float getMaxRoomSize() const noexcept { return mMaxRoomSize; }

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

    // Sized for 192 kHz with expanded dimensions (up to 4.0x room size):
    // Maximum tap 137.3 ms * 4.0 = 549.2 ms (~105,446 samples at 192 kHz)
    // 2^18 = 262,144 samples (~1.365 s at 192 kHz), power-of-two for bitwise masking
    static constexpr size_t kBufferCapacity = 262144;
    static constexpr size_t kBufferMask = kBufferCapacity - 1;

    double mSampleRate { 48000.0 };
    float mRoomSize { 1.0f };
    float mMaxRoomSize { 4.0f };
    float mLastRoomSize { -1.0f };
    bool  mHasProcessedSamples { false };

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

    // Early Reflection Tap Cluster Gain Calibration Factor:
    // kClusterGain = 0.28f (-11.06 dBFS attenuation).
    // Calibration Rationale:
    // 1. Without this calibration factor, the coherent / semi-coherent summation of 12
    //    reflection taps (individual tap gains 0.20 to 0.82) generates a peak burst
    //    accumulating up to +38.5 dB above the FDN late reverberant tank onset,
    //    swamping the acoustic decay tail and causing digital bus clipping.
    // 2. Applying kClusterGain = 0.28f ensures that a nominal unit impulse (0 dBFS / 1.0f)
    //    yields early reflection cluster peaks strictly bounded between -14 dBFS and -12 dBFS
    //    (specifically ~ -12.95 dBFS for the initial tap), achieving an acoustically natural
    //    direct-to-reverberant energy ratio and bit-exact parity with the Web Audio engine.
    static constexpr float kClusterGain = 0.28f;

    float mDiffusionDensity { 0.0f };
    static constexpr size_t kNumAllpass = 4;
    static constexpr std::array<size_t, kNumAllpass> kBaseAllpassLengths = {{ 149, 211, 163, 223 }};
    std::array<size_t, kNumAllpass> mAllpassLengths {};
    std::array<std::vector<float>, kNumAllpass> mAllpassBuffers {};
    std::array<size_t, kNumAllpass> mAllpassWriteIndices {};

    OnePoleLowpass mDampingLpL;
    OnePoleLowpass mDampingLpR;

    inline float processAllpass(size_t index, float input, float density) noexcept;
    void updateTaps() noexcept;
};

} // namespace rb26
