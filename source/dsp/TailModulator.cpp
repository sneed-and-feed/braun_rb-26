#include "TailModulator.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

void TailModulator::prepare(double sampleRate) noexcept {
    mSampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    const float fs = static_cast<float>(mSampleRate);

    mRateSmoother.setSampleRate(fs);
    mRateSmoother.setTimeConstant(0.040f);
    mRateSmoother.reset(mTargetRateHz);

    mDepthSmoother.setSampleRate(fs);
    mDepthSmoother.setTimeConstant(0.040f);
    mDepthSmoother.reset(mTargetDepthMs);

    mFastAlpha = std::exp(-1.0f / (fs * 0.0020f)); // 2.0 ms
    mSlowAlpha = std::exp(-1.0f / (fs * 0.0450f)); // 45.0 ms

    updateBloomAlpha();
    reset();
}

void TailModulator::reset() noexcept {
    std::fill(mPhases.begin(), mPhases.end(), 0.0f);
    mBloomEnvelope = 1.0f;
    mFastEnv = 0.0f;
    mSlowEnv = 0.0f;
}

void TailModulator::setParameters(float rateHz, float depthMs, float bloomMs) noexcept {
    mTargetRateHz = std::clamp(rateHz, 0.05f, 5.0f);
    mRateSmoother.setTarget(mTargetRateHz);

    mTargetDepthMs = std::clamp(depthMs, 0.0f, 5.0f);
    mDepthSmoother.setTarget(mTargetDepthMs);

    const float clampedBloom = std::clamp(bloomMs, 20.0f, 500.0f);
    if (std::abs(clampedBloom - mTargetBloomMs) > 1.0f) {
        mTargetBloomMs = clampedBloom;
        updateBloomAlpha();
    }
}

void TailModulator::updateBloomAlpha() noexcept {
    const float tauSec = mTargetBloomMs * 0.001f;
    mBloomAlpha = 1.0f - std::exp(-1.0f / (static_cast<float>(mSampleRate) * tauSec));
}

void TailModulator::processSample(float inputTransientLevel,
                                  std::array<float, kNumLines>& outExcursionsSamples) noexcept {
    ScopedNoDenormals noDenormals;

    // 1. Transient detection on input: dual envelope follower
    const float absIn = std::abs(inputTransientLevel);
    mFastEnv = flushDenormal((1.0f - mFastAlpha) * absIn + mFastAlpha * mFastEnv);
    mSlowEnv = flushDenormal((1.0f - mSlowAlpha) * absIn + mSlowAlpha * mSlowEnv);

    const float tr = mFastEnv / (mSlowEnv + 1.0e-5f);
    if (tr > 1.75f) {
        const float excess = std::min(5.0f, tr - 1.75f);
        const float duckFactor = 1.0f / (1.0f + 2.0f * excess);
        mBloomEnvelope = std::min(mBloomEnvelope, duckFactor);
    } else {
        mBloomEnvelope = flushDenormal(mBloomEnvelope + mBloomAlpha * (1.0f - mBloomEnvelope));
    }

    // 2. Smoothed rate and depth
    const float currentRate = mRateSmoother.next();
    const float currentDepthMs = mDepthSmoother.next();
    const float fs = static_cast<float>(mSampleRate);
    const float maxDepthSamples = (currentDepthMs * 0.001f) * fs;
    const float effectiveDepth = maxDepthSamples * mBloomEnvelope;

    if (effectiveDepth < 1.0e-5f) {
        outExcursionsSamples.fill(0.0f);
        return;
    }

    // 3. Update golden-ratio 8-phase LFO network
    for (size_t k = 0; k < kNumLines; ++k) {
        const float freqRatio = kGoldenRatios[k % 4];
        const float freq = currentRate * freqRatio;
        const float phaseInc = kTwoPi * (freq / fs);

        mPhases[k] += phaseInc;
        if (mPhases[k] >= kTwoPi) {
            mPhases[k] -= kTwoPi;
        }

        const float lfoVal = FastSinTable::sin(mPhases[k] + kPhaseOffsets[k]);
        // Zero-mean bipolar excursion
        outExcursionsSamples[k] = flushDenormal((effectiveDepth * 0.5f) * lfoVal);
    }
}

} // namespace rb26
