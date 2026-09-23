#include "FdnReverbTank.h"
#include "DspMath.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

void FdnReverbTank::prepare(double sampleRate, float maxRoomSize) noexcept {
    mSampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    mMaxRoomSize = std::max(1.0f, maxRoomSize);
    const float fs = static_cast<float>(mSampleRate);

    mTailModulator.prepare(mSampleRate);
    mManifoldNetwork.prepare(mSampleRate, mMaxRoomSize);

    // Prepare allpass diffusers
    for (size_t i = 0; i < kNumAllpass; ++i) {
        const size_t len = static_cast<size_t>(std::round(static_cast<double>(kBaseAllpassLengths[i]) * (mSampleRate / 48000.0)));
        mAllpassLengths[i] = std::max(size_t{16}, len);
        mAllpassBuffers[i].assign(mAllpassLengths[i] + 64, 0.0f);
        mAllpassWriteIndices[i] = 0;
    }

    mFreezeInputSmoother.setSampleRate(fs);
    mFreezeInputSmoother.setTimeConstant(kFreezeInputTimeConstantSec);
    mFreezeInputSmoother.reset(1.0f);

    mFreezeLoopSmoother.setSampleRate(fs);
    mFreezeLoopSmoother.setTimeConstant(kFreezeLoopTimeConstantSec);
    mFreezeLoopSmoother.reset(0.0f);

    for (size_t k = 0; k < kNumLines; ++k) {
        mDcBlockers[k].setCutoff(5.0f, mSampleRate);
    }

    mManifoldNetwork.setParameters(mCurrentManifold, mRoomSize, mHighDampingHz, mDiffusionDensity);
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

    for (size_t k = 0; k < kNumLines; ++k) {
        mDcBlockers[k].reset();
    }

    mFreezeInputSmoother.reset(mFreezeHold ? 0.08f : 1.0f);
    mFreezeLoopSmoother.reset(mFreezeHold ? 1.0f : 0.0f);
}

void FdnReverbTank::setParameters(float roomSize, float decayRt60Sec, float highDampingHz,
                                 float diffusionDensity, bool freezeHold,
                                 float tailModRateHz, float tailModDepthMs, float tailBloomMs,
                                 std::optional<ManifoldType> manifoldType) noexcept {
    const float safeRoom = (rb26::isFiniteBitwise(roomSize)) ? roomSize : mRoomSize;
    const float safeRt60 = (rb26::isFiniteBitwise(decayRt60Sec)) ? decayRt60Sec : mDecayRt60;
    const float safeDamp = (rb26::isFiniteBitwise(highDampingHz)) ? highDampingHz : mHighDampingHz;
    const float safeDiff = (rb26::isFiniteBitwise(diffusionDensity)) ? diffusionDensity : mDiffusionDensity;

    mRoomSize = std::clamp(safeRoom, 0.1f, mMaxRoomSize);
    mDecayRt60 = std::clamp(safeRt60, 0.2f, 30.0f);
    mHighDampingHz = std::clamp(safeDamp, 500.0f, 20000.0f);
    mDiffusionDensity = std::clamp(safeDiff, 0.0f, 1.0f);
    mFreezeHold = freezeHold;
    if (manifoldType.has_value()) {
        mCurrentManifold = *manifoldType;
    }

    if (mFreezeHold) {
        mFreezeInputSmoother.setTarget(0.08f);
        mFreezeLoopSmoother.setTarget(1.0f);
    } else {
        mFreezeInputSmoother.setTarget(1.0f);
        mFreezeLoopSmoother.setTarget(0.0f);
    }

    const float safeModRate  = (rb26::isFiniteBitwise(tailModRateHz))  ? tailModRateHz  : 0.65f;
    const float safeModDepth = (rb26::isFiniteBitwise(tailModDepthMs)) ? tailModDepthMs : 2.25f;
    const float safeBloom    = (rb26::isFiniteBitwise(tailBloomMs))    ? tailBloomMs    : 85.0f;

    mTailModulator.setParameters(safeModRate, safeModDepth, safeBloom);
    mManifoldNetwork.setParameters(mCurrentManifold, mRoomSize, mHighDampingHz, mDiffusionDensity);
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
        mFeedbackGains[k] = std::clamp(std::exp(-6.907755278982137f * tSec / safeRt60), 0.0f, 0.999f);
    }
}

inline float FdnReverbTank::processAllpass(size_t index, float input, float density) noexcept {
    if (density < 1.0e-4f) {
        return input;
    }
    if (index >= kNumAllpass || mAllpassBuffers[index].empty()) [[unlikely]] {
        return 0.0f;
    }
    const size_t len = mAllpassLengths[index];
    const size_t writeIdx = mAllpassWriteIndices[index];
    if (writeIdx >= mAllpassBuffers[index].size()) [[unlikely]] {
        return 0.0f;
    }
    const float delayed = mAllpassBuffers[index][writeIdx];

    // Perceptual curve mapping and expanded feedback depth (up to 0.74f)
    // Guarantees contractive stability (|g| < 1.0) with audible transient smearing
    const float effDensity = std::sqrt(std::clamp(density, 0.0f, 1.0f));
    const float g = 0.74f * effDensity;
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

    // Distribute into 8 FDN lines with balanced spatial phase and 1/sqrt(8) normalization
    constexpr float kNormFactor = 0.35355339f; // 1 / sqrt(8)
    const std::array<float, kNumLines> injection = {{
        dryL * kNormFactor,
        dryR * kNormFactor,
        mid * kNormFactor,
        side * kNormFactor,
        (dryL - 0.5f * dryR) * kNormFactor,
        (dryR - 0.5f * dryL) * kNormFactor,
        0.70710678f * (dryL - side) * kNormFactor,
        0.70710678f * (dryR + side) * kNormFactor
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
        const float dcBlocked = mDcBlockers[k].process(reflected);
        const float feedback = dcBlocked * effGain;
        const float nextIn = feedback + injection[k];
        const float satNormal = applySmoothBoundaryKnee(nextIn, 0.72f, 1.05f);
        const float satFreeze = applySmoothBoundaryKnee(nextIn, 0.90f, 1.05f);
        saturated[k] = flushDenormal((1.0f - freezeLoop) * satNormal + freezeLoop * satFreeze);
    }
    mManifoldNetwork.writeFeedback(saturated);

    // 6. Balanced stereo output extraction according to active manifold geometry with guaranteed <= 1.05f bounds
    mManifoldNetwork.extractStereo(y, outLateL, outLateR);
    outLateL = applySmoothBoundaryKnee(outLateL, 0.95f, 1.05f);
    outLateR = applySmoothBoundaryKnee(outLateR, 0.95f, 1.05f);
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
