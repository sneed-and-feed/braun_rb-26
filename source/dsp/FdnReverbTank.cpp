#include "FdnReverbTank.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

void FdnReverbTank::prepare(double sampleRate, float maxRoomSize) noexcept {
    mSampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    const float fs = static_cast<float>(mSampleRate);

    mTailModulator.prepare(mSampleRate);
    mManifoldNetwork.prepare(mSampleRate, maxRoomSize);

    // Prepare allpass diffusers
    for (size_t i = 0; i < kNumAllpass; ++i) {
        const size_t len = static_cast<size_t>(std::round(static_cast<double>(kBaseAllpassLengths[i]) * (mSampleRate / 48000.0)));
        mAllpassLengths[i] = std::max(size_t{16}, len);
        mAllpassBuffers[i].assign(mAllpassLengths[i] + 64, 0.0f);
        mAllpassWriteIndices[i] = 0;
    }

    mFreezeInputSmoother.setSampleRate(fs);
    mFreezeInputSmoother.setTimeConstant(0.060f);
    mFreezeInputSmoother.reset(1.0f);

    mFreezeLoopSmoother.setSampleRate(fs);
    mFreezeLoopSmoother.setTimeConstant(0.060f);
    mFreezeLoopSmoother.reset(0.0f);

    mManifoldNetwork.setParameters(mCurrentManifold, mRoomSize, mHighDampingHz);
    updateDecayGains();
    reset();
}

void FdnReverbTank::reset() noexcept {
    mTailModulator.reset();
    mManifoldNetwork.reset();

    for (size_t i = 0; i < kNumAllpass; ++i) {
        std::fill(mAllpassBuffers[i].begin(), mAllpassBuffers[i].end(), 0.0f);
        mAllpassWriteIndices[i] = 0;
    }

    mFreezeInputSmoother.reset(mFreezeHold ? 0.0f : 1.0f);
    mFreezeLoopSmoother.reset(mFreezeHold ? 1.0f : 0.0f);
}

void FdnReverbTank::setParameters(float roomSize, float decayRt60Sec, float highDampingHz,
                                 float diffusionDensity, bool freezeHold,
                                 float tailModRateHz, float tailModDepthMs, float tailBloomMs) noexcept {
    setParameters(roomSize, decayRt60Sec, highDampingHz, diffusionDensity, freezeHold,
                  tailModRateHz, tailModDepthMs, tailBloomMs, mCurrentManifold);
}

void FdnReverbTank::setParameters(float roomSize, float decayRt60Sec, float highDampingHz,
                                 float diffusionDensity, bool freezeHold,
                                 float tailModRateHz, float tailModDepthMs, float tailBloomMs,
                                 ManifoldType manifoldType) noexcept {
    mRoomSize = std::clamp(roomSize, 0.1f, 2.0f);
    mDecayRt60 = std::clamp(decayRt60Sec, 0.2f, 30.0f);
    mHighDampingHz = std::clamp(highDampingHz, 500.0f, 20000.0f);
    mDiffusionDensity = std::clamp(diffusionDensity, 0.0f, 1.0f);
    mFreezeHold = freezeHold;
    mCurrentManifold = manifoldType;

    if (mFreezeHold) {
        mFreezeInputSmoother.setTarget(0.0f);
        mFreezeLoopSmoother.setTarget(1.0f);
    } else {
        mFreezeInputSmoother.setTarget(1.0f);
        mFreezeLoopSmoother.setTarget(0.0f);
    }

    mTailModulator.setParameters(tailModRateHz, tailModDepthMs, tailBloomMs);
    mManifoldNetwork.setParameters(mCurrentManifold, mRoomSize, mHighDampingHz);
    updateDecayGains();
}

void FdnReverbTank::setManifold(ManifoldType manifoldType) noexcept {
    mCurrentManifold = manifoldType;
    mManifoldNetwork.setManifold(manifoldType);
    updateDecayGains();
}

void FdnReverbTank::updateDecayGains() noexcept {
    const auto& lengths = mManifoldNetwork.getNominalLengths();
    const float safeRt60 = std::max(0.05f, mDecayRt60);
    for (size_t k = 0; k < kNumLines; ++k) {
        const float tSec = static_cast<float>(lengths[k]) / static_cast<float>(mSampleRate);
        mFeedbackGains[k] = std::exp(-6.907755278982137f * tSec / safeRt60);
    }
}

inline float FdnReverbTank::processAllpass(size_t index, float input, float density) noexcept {
    if (index >= kNumAllpass || mAllpassBuffers[index].empty()) [[unlikely]] {
        return 0.0f;
    }
    const size_t len = mAllpassLengths[index];
    const size_t writeIdx = mAllpassWriteIndices[index];
    if (writeIdx >= mAllpassBuffers[index].size()) [[unlikely]] {
        return 0.0f;
    }
    const float delayed = mAllpassBuffers[index][writeIdx];

    const float g = 0.70f * density;
    const float output = -g * input + delayed;
    mAllpassBuffers[index][writeIdx] = flushDenormal(input + g * output);

    const size_t nextIdx = writeIdx + 1;
    mAllpassWriteIndices[index] = (nextIdx >= len) ? 0 : nextIdx;
    return flushDenormal(output);
}

void FdnReverbTank::processSample(float inL, float inR, float pitchFbL, float pitchFbR,
                                 float& outLateL, float& outLateR) noexcept {
    if (mAllpassBuffers[0].empty()) [[unlikely]] {
        outLateL = 0.0f;
        outLateR = 0.0f;
        return;
    }

    const float freezeIn = mFreezeInputSmoother.next();
    const float freezeLoop = mFreezeLoopSmoother.next();

    // 1. Allpass input diffusion (cascaded dual allpasses per channel)
    const float diffL = processAllpass(1, processAllpass(0, inL, mDiffusionDensity), mDiffusionDensity);
    const float diffR = processAllpass(3, processAllpass(2, inR, mDiffusionDensity), mDiffusionDensity);

    // Sum diffused input and pitch feedback with contractive loop gain headroom; isolate both on freeze
    const float dryL = (diffL + pitchFbL) * freezeIn;
    const float dryR = (diffR + pitchFbR) * freezeIn;
    const float mid = 0.70710678f * (dryL + dryR);
    const float side = 0.70710678f * (dryL - dryR);

    // Distribute into 8 FDN lines with balanced spatial phase
    const std::array<float, kNumLines> injection = {{
        dryL,
        dryR,
        mid,
        side,
        dryL - 0.5f * dryR,
        dryR - 0.5f * dryL,
        0.70710678f * (dryL - side),
        0.70710678f * (dryR + side)
    }};

    // 2. Tail modulation excursions
    std::array<float, kNumLines> excursions {};
    mTailModulator.processSample(0.5f * (std::abs(inL) + std::abs(inR)), excursions);

    // 3. Read 8 delay lines with fractional Hermite cubic interpolation, HF damping & manifold filters
    std::array<float, kNumLines> y {};
    mManifoldNetwork.readAndFilterLines(excursions, y, freezeLoop);

    // 4. Orthogonal Householder 8x8 reflection matrix: H_8 = I_8 - 0.25 * 1 * 1^T
    float sum = 0.0f;
    for (size_t k = 0; k < kNumLines; ++k) {
        sum += y[k];
    }
    const float matrixOffset = flushDenormal(sum * 0.25f);

    // 5. Matrix recirculation + saturation bounding
    std::array<float, kNumLines> saturated {};
    for (size_t k = 0; k < kNumLines; ++k) {
        const float reflected = y[k] - matrixOffset;
        const float effGain = (1.0f - freezeLoop) * mFeedbackGains[k] + freezeLoop * 1.0f;
        const float feedback = reflected * effGain;
        const float nextIn = feedback + injection[k];
        saturated[k] = flushDenormal(applySmoothBoundaryKnee(nextIn, 0.72f, 1.05f));
    }
    mManifoldNetwork.writeFeedback(saturated);

    // 6. Balanced stereo output extraction according to active manifold geometry
    mManifoldNetwork.extractStereo(y, outLateL, outLateR);
}

void FdnReverbTank::processBlock(const float* inL, const float* inR,
                                const float* pitchFbL, const float* pitchFbR,
                                float* outLateL, float* outLateR,
                                int numSamples) noexcept {
    ScopedNoDenormals noDenormals;
    for (int i = 0; i < numSamples; ++i) {
        const float pL = pitchFbL ? pitchFbL[i] : 0.0f;
        const float pR = pitchFbR ? pitchFbR[i] : 0.0f;
        processSample(inL[i], inR[i], pL, pR, outLateL[i], outLateR[i]);
    }
}

} // namespace rb26
