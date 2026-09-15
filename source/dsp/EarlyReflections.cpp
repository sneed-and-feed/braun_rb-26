#include "EarlyReflections.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

void EarlyReflections::prepare(double sampleRate, float /*maxRoomSize*/) noexcept {
    mSampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    mBufferL.assign(kBufferCapacity, 0.0f);
    mBufferR.assign(kBufferCapacity, 0.0f);
    mWriteIndex = 0;

    for (size_t i = 0; i < kNumAllpass; ++i) {
        const size_t len = static_cast<size_t>(std::round(static_cast<double>(kBaseAllpassLengths[i]) * (mSampleRate / 48000.0)));
        mAllpassLengths[i] = std::max(size_t{16}, len);
        mAllpassBuffers[i].assign(mAllpassLengths[i] + 64, 0.0f);
        mAllpassWriteIndices[i] = 0;
    }

    updateTaps();
}

void EarlyReflections::reset() noexcept {
    std::fill(mBufferL.begin(), mBufferL.end(), 0.0f);
    std::fill(mBufferR.begin(), mBufferR.end(), 0.0f);
    mWriteIndex = 0;

    for (size_t i = 0; i < kNumAllpass; ++i) {
        std::fill(mAllpassBuffers[i].begin(), mAllpassBuffers[i].end(), 0.0f);
        mAllpassWriteIndices[i] = 0;
    }
}

void EarlyReflections::setParameters(float roomSize, float diffusionDensity) noexcept {
    const float clamped = std::clamp(roomSize, 0.1f, 2.0f);
    if (std::abs(clamped - mRoomSize) > 0.005f) {
        mRoomSize = clamped;
        updateTaps();
    }
    mDiffusionDensity = std::clamp(diffusionDensity, 0.0f, 1.0f);
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

inline float EarlyReflections::processAllpass(size_t index, float input, float density) noexcept {
    if (density < 1.0e-4f) return input;
    const size_t len = mAllpassLengths[index];
    const size_t writeIdx = mAllpassWriteIndices[index];
    const float delayed = mAllpassBuffers[index][writeIdx];

    const float g = 0.70f * density;
    const float output = -g * input + delayed;
    mAllpassBuffers[index][writeIdx] = flushDenormal(input + g * output);

    mAllpassWriteIndices[index] = (writeIdx + 1) % len;
    return flushDenormal(output);
}

void EarlyReflections::processSample(float inL, float inR, float& outL, float& outR) noexcept {
    ScopedNoDenormals noDenormals;
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

    const float diffL = processAllpass(1, processAllpass(0, sumL, mDiffusionDensity), mDiffusionDensity);
    const float diffR = processAllpass(3, processAllpass(2, sumR, mDiffusionDensity), mDiffusionDensity);

    outL = flushDenormal(diffL);
    outR = flushDenormal(diffR);
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
