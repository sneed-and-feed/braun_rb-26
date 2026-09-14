#include "EarlyReflections.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

void EarlyReflections::prepare(double sampleRate, float /*maxRoomSize*/) noexcept {
    mSampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    mBufferL.assign(kBufferCapacity, 0.0f);
    mBufferR.assign(kBufferCapacity, 0.0f);
    mWriteIndex = 0;
    updateTaps();
}

void EarlyReflections::reset() noexcept {
    std::fill(mBufferL.begin(), mBufferL.end(), 0.0f);
    std::fill(mBufferR.begin(), mBufferR.end(), 0.0f);
    mWriteIndex = 0;
}

void EarlyReflections::setParameters(float roomSize) noexcept {
    const float clamped = std::clamp(roomSize, 0.1f, 2.0f);
    if (std::abs(clamped - mRoomSize) > 0.005f) {
        mRoomSize = clamped;
        updateTaps();
    }
}

void EarlyReflections::updateTaps() noexcept {
    for (size_t k = 0; k < kNumTaps; ++k) {
        const float delaySec = (kTapConfigs[k].baseDelayMs * 0.001f) * mRoomSize;
        const size_t delaySamples = static_cast<size_t>(std::round(delaySec * mSampleRate));
        mTapDelaysSamples[k] = std::clamp(delaySamples, size_t{1}, kBufferCapacity - 1);

        // Constant power azimuth panning: theta in [0, pi/2]
        const float theta = (kTapConfigs[k].pan + 1.0f) * 0.25f * kPi;
        mTapGainsL[k] = kTapConfigs[k].gain * std::cos(theta);
        mTapGainsR[k] = kTapConfigs[k].gain * std::sin(theta);
    }
}

void EarlyReflections::processSample(float inL, float inR, float& outL, float& outR) noexcept {
    // 20% lateral cross-coupling for room reflection spatial realism
    const float inBufL = inL + 0.20f * inR;
    const float inBufR = inR + 0.20f * inL;

    mBufferL[mWriteIndex] = flushDenormal(inBufL);
    mBufferR[mWriteIndex] = flushDenormal(inBufR);

    float sumL = 0.0f;
    float sumR = 0.0f;

    for (size_t k = 0; k < kNumTaps; ++k) {
        const size_t readIdx = (mWriteIndex + kBufferCapacity - mTapDelaysSamples[k]) & kBufferMask;
        sumL += mBufferL[readIdx] * mTapGainsL[k];
        sumR += mBufferR[readIdx] * mTapGainsR[k];
    }

    mWriteIndex = (mWriteIndex + 1) & kBufferMask;

    outL = flushDenormal(sumL);
    outR = flushDenormal(sumR);
}

void EarlyReflections::processBlock(const float* inL, const float* inR,
                                  float* outL, float* outR,
                                  int numSamples) noexcept {
    ScopedNoDenormals noDenormals;
    for (int i = 0; i < numSamples; ++i) {
        processSample(inL[i], inR[i], outL[i], outR[i]);
    }
}

} // namespace rb26
