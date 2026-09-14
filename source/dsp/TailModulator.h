#pragma once

#include "DspMath.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <cstddef>

namespace rb26 {

class TailModulator {
public:
    static constexpr size_t kNumLines = 8;

    TailModulator() noexcept = default;
    ~TailModulator() noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setParameters(float rateHz, float depthMs, float bloomMs) noexcept;

    // Advances LFOs and calculates 8 fractional delay excursions in samples
    void processSample(float inputTransientLevel,
                       std::array<float, kNumLines>& outExcursionsSamples) noexcept;

    // Branchless 4-point Hermite cubic circular buffer read
    static inline float readHermite(const float* buffer, size_t bufferCapacity,
                                    size_t bufferMask, size_t writeIndex,
                                    float delaySamples) noexcept {
        const float clampedDelay = std::max(0.0f, delaySamples);
        const int intDelay = static_cast<int>(clampedDelay);
        const float mu = clampedDelay - static_cast<float>(intDelay);

        // Buffer write index advances forward; delay looks backward in time
        const size_t idx0 = (writeIndex + bufferCapacity - static_cast<size_t>(intDelay)) & bufferMask;
        const size_t idxM1 = (idx0 + 1) & bufferMask;
        const size_t idx1 = (idx0 + bufferCapacity - 1) & bufferMask;
        const size_t idx2 = (idx0 + bufferCapacity - 2) & bufferMask;

        const float ym1 = buffer[idxM1];
        const float y0  = buffer[idx0];
        const float y1  = buffer[idx1];
        const float y2  = buffer[idx2];

        return interpolateHermite4P3O(ym1, y0, y1, y2, mu);
    }

private:
    double mSampleRate { 48000.0 };
    float mTargetRateHz { 0.85f };
    float mTargetDepthMs { 1.2f };
    float mTargetBloomMs { 85.0f };

    OnePoleSmoother mRateSmoother;
    OnePoleSmoother mDepthSmoother;

    std::array<float, kNumLines> mPhases {};

    // Golden ratio powers phi^((k%4) - 1.5)
    static constexpr std::array<float, 4> kGoldenRatios = {{
        0.48586827f, 0.78615138f, 1.27201965f, 2.05817103f
    }};

    static constexpr std::array<float, kNumLines> kPhaseOffsets = {{
        0.5235988f, 1.3089969f, 2.0943951f, 2.8797933f,
        3.6651914f, 4.4505896f, 5.2359878f, 6.0213859f
    }};

    // Bloom envelope detector & state
    float mBloomEnvelope { 1.0f };
    float mBloomAlpha { 0.001f };
    float mFastEnv { 0.0f };
    float mSlowEnv { 0.0f };
    float mFastAlpha { 0.01f };
    float mSlowAlpha { 0.0005f };

    void updateBloomAlpha() noexcept;
};

} // namespace rb26
