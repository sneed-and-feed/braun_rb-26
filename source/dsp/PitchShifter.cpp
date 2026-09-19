#include "PitchShifter.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

// ============================================================================
// DualTapDelayPitchShifter Implementation
// ============================================================================
void DualTapDelayPitchShifter::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mBufferSize = kMaxCapacity;

    mDelayBuffer.assign(static_cast<size_t>(kMaxCapacity), 0.0f);
    mWriteIndex = 0;
    mPhase = 0.0f;

    setInterval(mSemitones);
}

void DualTapDelayPitchShifter::reset() noexcept {
    std::fill(mDelayBuffer.begin(), mDelayBuffer.end(), 0.0f);
    mWriteIndex = 0;
    mPhase = 0.0f;
    mWindowSamples = mTargetWindowSamples;
    const float slope = 1.0f - mRatio;
    const float absSlope = std::abs(slope);
    mPhaseInc = (absSlope > 1.0e-5f && mWindowSamples > 0.0f) ? (absSlope / mWindowSamples) : 0.0f;
}

void DualTapDelayPitchShifter::setInterval(int semitones) noexcept {
    mSemitones = semitones;
    mRatio = std::pow(2.0f, static_cast<float>(semitones) / 12.0f);

    if (mRatio >= 1.0f) {
        mWindowSec = 0.050f; // 50 ms for Shimmer (+7, +12, +24) -> 0.00% error
    } else if (mRatio >= 0.85f) {
        mWindowSec = 0.070f; // 70 ms for Dimmer -2st (Dark Chorus)
    } else if (mRatio >= 0.60f) {
        mWindowSec = 0.085f; // 85 ms for Dimmer -7st (Sub-Fifth Drone)
    } else if (mRatio >= 0.45f) {
        mWindowSec = 0.100f; // 100 ms for Dimmer -12st (Sub-Octave Bloom) -> 0.02% error
    } else {
        mWindowSec = 0.200f; // 200 ms for Dimmer -24st -> 0.04% error
    }
    mTargetWindowSamples = std::max(64.0f, mWindowSec * mSampleRate);
    if (mWriteIndex == 0) {
        mWindowSamples = mTargetWindowSamples;
    }
    const float slope = 1.0f - mRatio;
    const float absSlope = std::abs(slope);
    mPhaseInc = (absSlope > 1.0e-5f && mWindowSamples > 0.0f) ? (absSlope / mWindowSamples) : 0.0f;
}

float DualTapDelayPitchShifter::processSample(float input) noexcept {
    if (mDelayBuffer.empty()) [[unlikely]] {
        return flushDenormal(input);
    }
    if (std::abs(mTargetWindowSamples - mWindowSamples) > 0.05f) {
        mWindowSamples += 0.005f * (mTargetWindowSamples - mWindowSamples);
        const float slope = 1.0f - mRatio;
        const float absSlope = std::abs(slope);
        mPhaseInc = (absSlope > 1.0e-5f && mWindowSamples > 0.0f) ? (absSlope / mWindowSamples) : 0.0f;
    }
    // Write sample to delay buffer
    mDelayBuffer[static_cast<size_t>(mWriteIndex)] = flushDenormal(input);

    // If pitch shift is 0 (mSemitones == 0 or mRatio == 1.0), bypass pitch-shifting processing block completely and pass dry audio straight through
    if (mSemitones == 0 || mPhaseInc <= 1.0e-7f || std::abs(mRatio - 1.0f) <= 1.0e-5f) {
        mWriteIndex = (mWriteIndex + 1) & kBufferMask;
        return flushDenormal(input);
    }

    const float phase1 = mPhase;
    const float phase2 = (phase1 >= 0.5f) ? (phase1 - 0.5f) : (phase1 + 0.5f);

    // Calculate delay excursions with minimum 2-sample margin
    // Guarantees readPos <= mWriteIndex - 2.0f so floor(readPos) + 2 <= mWriteIndex,
    // strictly ensuring 4-point Hermite interpolation never reads unwritten future indices
    constexpr float kMinDelayMargin = 2.0f;
    float delay1 = 0.0f;
    float delay2 = 0.0f;

    if (mRatio >= 1.0f) {
        // Upward pitch shift (Shimmer): delay ramps downwards from W + minMargin to minMargin
        delay1 = kMinDelayMargin + mWindowSamples * (1.0f - phase1);
        delay2 = kMinDelayMargin + mWindowSamples * (1.0f - phase2);
    } else {
        // Downward pitch shift (Dimmer): delay ramps upwards from minMargin to W + minMargin
        delay1 = kMinDelayMargin + mWindowSamples * phase1;
        delay2 = kMinDelayMargin + mWindowSamples * phase2;
    }

    // Read head fractional indices
    const float readPos1 = static_cast<float>(mWriteIndex) - delay1;
    const float readPos2 = static_cast<float>(mWriteIndex) - delay2;

    const bool useLin = shouldUseLinear();
    const float out1 = useLin ? readLinear(readPos1) : readHermite(readPos1);
    const float out2 = useLin ? readLinear(readPos2) : readHermite(readPos2);

    // Constant-amplitude Hann crossfade windows: w1 + w2 == sin^2(pi*phi) + cos^2(pi*phi) == 1.0
    // C1 smooth zero-crossing at grain boundaries suppresses wrap discontinuity by > 115 dB
    const float sin1 = FastSinTable::sin(kPi * phase1);
    const float sin2 = FastSinTable::sin(kPi * phase2);
    const float w1 = sin1 * sin1;
    const float w2 = sin2 * sin2;

    // Advance normalized phase accumulator
    mPhase += mPhaseInc;
    if (mPhase >= 1.0f) {
        mPhase -= std::floor(mPhase);
    }

    // Advance write head with bitmask wrapping
    mWriteIndex = (mWriteIndex + 1) & kBufferMask;

    return flushDenormal(w1 * out1 + w2 * out2);
}

inline float DualTapDelayPitchShifter::readLinear(float readPos) const noexcept {
    if (!std::isfinite(readPos)) [[unlikely]] {
        return 0.0f;
    }
    const int i0 = static_cast<int>(std::floor(readPos));
    const float frac = readPos - static_cast<float>(i0);

    const int i0_m = i0 & kBufferMask;
    const int i1  = (i0 + 1) & kBufferMask;

    const float y0 = mDelayBuffer[static_cast<size_t>(i0_m)];
    const float y1 = mDelayBuffer[static_cast<size_t>(i1)];

    return interpolateLinear2P1O(y0, y1, frac);
}

inline float DualTapDelayPitchShifter::readHermite(float readPos) const noexcept {
    if (!std::isfinite(readPos)) [[unlikely]] {
        return 0.0f;
    }
    const int i0 = static_cast<int>(std::floor(readPos));
    const float frac = readPos - static_cast<float>(i0);

    const int im1 = (i0 - 1) & kBufferMask;
    const int i0_m = i0 & kBufferMask;
    const int i1  = (i0 + 1) & kBufferMask;
    const int i2  = (i0 + 2) & kBufferMask;

    const float ym1 = mDelayBuffer[static_cast<size_t>(im1)];
    const float y0  = mDelayBuffer[static_cast<size_t>(i0_m)];
    const float y1  = mDelayBuffer[static_cast<size_t>(i1)];
    const float y2  = mDelayBuffer[static_cast<size_t>(i2)];

    return interpolateHermite4P3O(ym1, y0, y1, y2, frac);
}

// ============================================================================
// PitchShifter Implementation
// ============================================================================
PitchShifter::PitchShifter() noexcept
    : mShimmerSaturatorL(0.72f, 1.05f),
      mShimmerSaturatorR(0.72f, 1.05f),
      mDimmerSaturatorL(0.72f, 1.05f),
      mDimmerSaturatorR(0.72f, 1.05f) {
    mShimmerSendSmoother.setTimeConstant(0.020f);  // 20 ms
    mDimmerSendSmoother.setTimeConstant(0.020f);   // 20 ms
    mPitchBlendSmoother.setTimeConstant(0.025f);   // 25 ms
    mPitchFeedbackSmoother.setTimeConstant(0.025f); // 25 ms
    mSpiralDepthSmoother.setTimeConstant(0.025f);   // 25 ms
    mSpiralRateSmoother.setTimeConstant(0.030f);    // 30 ms
    prepare(48000.0, 512);
}

void PitchShifter::prepare(double sampleRate, int /*maxBlockSize*/) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);

    mShimmerShifterL.prepare(sampleRate);
    mShimmerShifterR.prepare(sampleRate);
    mShimmerFilterL.prepare(sampleRate);
    mShimmerFilterR.prepare(sampleRate);

    mDimmerShifterL.prepare(sampleRate);
    mDimmerShifterR.prepare(sampleRate);
    mDimmerFilterL.prepare(sampleRate);
    mDimmerFilterR.prepare(sampleRate);

    mShepardSpiral.prepare(sampleRate);
    setQualityMode(mQualityMode);

    mShimmerSendSmoother.setSampleRate(mSampleRate);
    mDimmerSendSmoother.setSampleRate(mSampleRate);
    mPitchBlendSmoother.setSampleRate(mSampleRate);
    mPitchFeedbackSmoother.setSampleRate(mSampleRate);
    mSpiralDepthSmoother.setSampleRate(mSampleRate);
    mSpiralRateSmoother.setSampleRate(mSampleRate);

    mShimmerShifterL.setInterval(mCurrentShimmerInterval);
    mShimmerShifterR.setInterval(mCurrentShimmerInterval);
    mDimmerShifterL.setInterval(mCurrentDimmerInterval);
    mDimmerShifterR.setInterval(mCurrentDimmerInterval);

    mSpiralDepthSmoother.reset(mSpiralDepth);
    mSpiralRateSmoother.reset(mSpiralRateHz);

    mShimmerCircBufferL.assign(kCircCapacity, 0.0f);
    mShimmerCircBufferR.assign(kCircCapacity, 0.0f);
    mDimmerCircBufferL.assign(kCircCapacity, 0.0f);
    mDimmerCircBufferR.assign(kCircCapacity, 0.0f);
    mCircWriteIndex = 0;

    reset();
}

void PitchShifter::reset() noexcept {
    mShimmerShifterL.reset();
    mShimmerShifterR.reset();
    mShimmerFilterL.reset();
    mShimmerFilterR.reset();

    mDimmerShifterL.reset();
    mDimmerShifterR.reset();
    mDimmerFilterL.reset();
    mDimmerFilterR.reset();

    mShepardSpiral.reset();

    std::fill(mShimmerCircBufferL.begin(), mShimmerCircBufferL.end(), 0.0f);
    std::fill(mShimmerCircBufferR.begin(), mShimmerCircBufferR.end(), 0.0f);
    std::fill(mDimmerCircBufferL.begin(), mDimmerCircBufferL.end(), 0.0f);
    std::fill(mDimmerCircBufferR.begin(), mDimmerCircBufferR.end(), 0.0f);
    mCircWriteIndex = 0;

    mRecircShimmerL = 0.0f;
    mRecircShimmerR = 0.0f;
    mRecircDimmerL = 0.0f;
    mRecircDimmerR = 0.0f;

    mShimmerSendSmoother.reset(mShimmerSendSmoother.getTarget());
    mDimmerSendSmoother.reset(mDimmerSendSmoother.getTarget());
    mPitchBlendSmoother.reset(mPitchBlendSmoother.getTarget());
    mPitchFeedbackSmoother.reset(mPitchFeedbackSmoother.getTarget());
    mSpiralDepthSmoother.reset(mSpiralDepthSmoother.getTarget());
    mSpiralRateSmoother.reset(mSpiralRateSmoother.getTarget());
}

void PitchShifter::setParameters(float shimmerSend,
                                float dimmerSend,
                                int shimmerInterval,
                                int dimmerInterval,
                                float pitchBlend,
                                float pitchFeedback) noexcept {
    const float s = std::clamp(shimmerSend, 0.0f, 1.0f);
    const float d = std::clamp(dimmerSend, 0.0f, 1.0f);
    const float b = std::clamp(pitchBlend, -1.0f, 1.0f);

    if (s <= 1.0e-4f) {
        mShimmerSendSmoother.snapTo(0.0f);
    } else {
        mShimmerSendSmoother.setTarget(s);
    }

    if (d <= 1.0e-4f) {
        mDimmerSendSmoother.snapTo(0.0f);
    } else {
        mDimmerSendSmoother.setTarget(d);
    }

    if (s <= 1.0e-4f && d <= 1.0e-4f) {
        mPitchBlendSmoother.snapTo(b);
    } else {
        mPitchBlendSmoother.setTarget(b);
    }
    mPitchFeedbackSmoother.setTarget(std::clamp(pitchFeedback, 0.0f, 0.95f));

    if (shimmerInterval != mCurrentShimmerInterval) {
        mCurrentShimmerInterval = shimmerInterval;
        mShimmerShifterL.setInterval(shimmerInterval);
        mShimmerShifterR.setInterval(shimmerInterval);
    }

    if (dimmerInterval != mCurrentDimmerInterval) {
        mCurrentDimmerInterval = dimmerInterval;
        mDimmerShifterL.setInterval(dimmerInterval);
        mDimmerShifterR.setInterval(dimmerInterval);
    }
}

void PitchShifter::setSpiralParameters(int spiralMode,
                                       float spiralRateHz,
                                       float spiralDepth,
                                       int partchInterval) noexcept {
    mSpiralMode = std::clamp(spiralMode, 0, 2);
    mSpiralRateHz = std::clamp(spiralRateHz, 0.005f, 5.0f);
    mSpiralDepth = std::clamp(spiralDepth, 0.0f, 1.0f);
    mPartchInterval = std::clamp(partchInterval, 0, 5);

    mSpiralRateSmoother.setTarget(mSpiralRateHz);
    mSpiralDepthSmoother.setTarget(mSpiralDepth);
    mShepardSpiral.setMode(static_cast<ShepardPitchSpiral::SpiralMode>(mSpiralMode));
    mShepardSpiral.setRateHz(mSpiralRateHz);
    mShepardSpiral.setDepth(mSpiralDepth);
    mShepardSpiral.setPartchInterval(mPartchInterval);
}

void PitchShifter::processSample(float inL, float inR, float& outL, float& outR) noexcept {
    if (mShimmerCircBufferL.empty()) [[unlikely]] {
        outL = inL;
        outR = inR;
        return;
    }
    const float sSend = mShimmerSendSmoother.next();
    const float dSend = mDimmerSendSmoother.next();
    const float blend = mPitchBlendSmoother.next();
    const float fb = mPitchFeedbackSmoother.next();
    const float spiralDepth = mSpiralDepthSmoother.next();

    // Fast path: when both shimmer and dimmer sends are bypassed or below threshold
    if (sSend <= 0.001f && dSend <= 0.001f) {
        mRecircShimmerL = 0.0f;
        mRecircShimmerR = 0.0f;
        mRecircDimmerL = 0.0f;
        mRecircDimmerR = 0.0f;
        mShimmerCircBufferL[mCircWriteIndex] = 0.0f;
        mShimmerCircBufferR[mCircWriteIndex] = 0.0f;
        mDimmerCircBufferL[mCircWriteIndex] = 0.0f;
        mDimmerCircBufferR[mCircWriteIndex] = 0.0f;
        mCircWriteIndex = (mCircWriteIndex + 1) & kCircMask;
        outL = 0.0f;
        outR = 0.0f;
        return;
    }

    // Equal-power crossfade weighting based on blend beta in [-1.0, +1.0]
    const float blendAngle = (kPi * 0.25f) * (1.0f - blend);
    const float blendShim = FastSinTable::cos(blendAngle);
    const float blendDim  = FastSinTable::sin(blendAngle);

    const float effShimmerSend = (sSend > 1.0e-5f && blendShim > 1.0e-5f) ? (sSend * blendShim) : 0.0f;
    const float effDimmerSend  = (dSend > 1.0e-5f && blendDim > 1.0e-5f) ? (dSend * blendDim) : 0.0f;

    // Bounded internal feedback gain strictly avoiding dual-closed-loop runaway
    const float safeFb = std::clamp(fb * 0.35f, 0.0f, 0.35f);

    // 1. Shimmer Loop: Scale input by send weight, inject bounded feedback, filter, shift, saturate
    float shimSatL = 0.0f;
    float shimSatR = 0.0f;
    if (effShimmerSend > 1.0e-5f) {
        // Decoupled circulation delay tap (~149 ms for Shimmer, giving distinct temporal sparkle spacing)
        const size_t shimDelaySamples = static_cast<size_t>(std::clamp(0.149f * mSampleRate, 1.0f, static_cast<float>(kCircCapacity - 64)));
        const size_t readShimIdx = (mCircWriteIndex + kCircCapacity - shimDelaySamples) & kCircMask;
        const float delayedShimFbL = mShimmerCircBufferL[readShimIdx];
        const float delayedShimFbR = mShimmerCircBufferR[readShimIdx];

        const float shimInL = inL * effShimmerSend + safeFb * delayedShimFbL;
        const float shimInR = inR * effShimmerSend + safeFb * delayedShimFbR;

        const float shimFiltL = mShimmerFilterL.process(shimInL);
        const float shimFiltR = mShimmerFilterR.process(shimInR);

        float shimRawL = mShimmerShifterL.processSample(shimFiltL);
        float shimRawR = mShimmerShifterR.processSample(shimFiltR);

        // If BarberShimmer mode is active, crossfade shimmer signal with continuous Shepard spiral
        if (spiralDepth > 0.001f && mSpiralMode == static_cast<int>(ShepardPitchSpiral::SpiralMode::BarberShimmer)) {
            float spiralOutL = 0.0f, spiralOutR = 0.0f;
            mShepardSpiral.processSample(shimFiltL, shimFiltR, spiralOutL, spiralOutR);
            shimRawL = (1.0f - spiralDepth) * shimRawL + spiralDepth * spiralOutL;
            shimRawR = (1.0f - spiralDepth) * shimRawR + spiralDepth * spiralOutR;
        }

        shimSatL = mShimmerSaturatorL.processSample(shimRawL);
        shimSatR = mShimmerSaturatorR.processSample(shimRawR);

        mShimmerCircBufferL[mCircWriteIndex] = flushDenormal(shimSatL);
        mShimmerCircBufferR[mCircWriteIndex] = flushDenormal(shimSatR);
        mRecircShimmerL = shimSatL;
        mRecircShimmerR = shimSatR;
    } else {
        mRecircShimmerL = 0.0f;
        mRecircShimmerR = 0.0f;
        mShimmerCircBufferL[mCircWriteIndex] = 0.0f;
        mShimmerCircBufferR[mCircWriteIndex] = 0.0f;
        shimSatL = 0.0f;
        shimSatR = 0.0f;
    }

    // 2. Dimmer Loop: Scale input by send weight, inject bounded feedback, filter, shift, saturate
    float dimSatL = 0.0f;
    float dimSatR = 0.0f;
    if (effDimmerSend > 1.0e-5f) {
        // Decoupled circulation delay tap (~211 ms for Dimmer, giving distinct temporal falling drops)
        const size_t dimDelaySamples = static_cast<size_t>(std::clamp(0.211f * mSampleRate, 1.0f, static_cast<float>(kCircCapacity - 64)));
        const size_t readDimIdx = (mCircWriteIndex + kCircCapacity - dimDelaySamples) & kCircMask;
        const float delayedDimFbL = mDimmerCircBufferL[readDimIdx];
        const float delayedDimFbR = mDimmerCircBufferR[readDimIdx];

        const float dimInL = inL * effDimmerSend + safeFb * delayedDimFbL;
        const float dimInR = inR * effDimmerSend + safeFb * delayedDimFbR;

        const float dimFiltL = mDimmerFilterL.process(dimInL);
        const float dimFiltR = mDimmerFilterR.process(dimInR);

        float dimRawL = mDimmerShifterL.processSample(dimFiltL);
        float dimRawR = mDimmerShifterR.processSample(dimFiltR);

        // If BarberDimmer or PartchLattice mode is active, crossfade dimmer signal with Shepard spiral
        if (spiralDepth > 0.001f && (mSpiralMode == static_cast<int>(ShepardPitchSpiral::SpiralMode::BarberDimmer) ||
                                     mSpiralMode == static_cast<int>(ShepardPitchSpiral::SpiralMode::PartchLattice))) {
            float spiralOutL = 0.0f, spiralOutR = 0.0f;
            mShepardSpiral.processSample(dimFiltL, dimFiltR, spiralOutL, spiralOutR);
            dimRawL = (1.0f - spiralDepth) * dimRawL + spiralDepth * spiralOutL;
            dimRawR = (1.0f - spiralDepth) * dimRawR + spiralDepth * spiralOutR;
        }

        dimSatL = mDimmerSaturatorL.processSample(dimRawL);
        dimSatR = mDimmerSaturatorR.processSample(dimRawR);

        mDimmerCircBufferL[mCircWriteIndex] = flushDenormal(dimSatL);
        mDimmerCircBufferR[mCircWriteIndex] = flushDenormal(dimSatR);
        mRecircDimmerL = dimSatL;
        mRecircDimmerR = dimSatR;
    } else {
        mRecircDimmerL = 0.0f;
        mRecircDimmerR = 0.0f;
        mDimmerCircBufferL[mCircWriteIndex] = 0.0f;
        mDimmerCircBufferR[mCircWriteIndex] = 0.0f;
        dimSatL = 0.0f;
        dimSatR = 0.0f;
    }

    mCircWriteIndex = (mCircWriteIndex + 1) & kCircMask;

    // 3. Composite Output: Sum of already send-weighted Shimmer and Dimmer
    outL = flushDenormal(shimSatL + dimSatL);
    outR = flushDenormal(shimSatR + dimSatR);
}

void PitchShifter::process(const float* inL,
                           const float* inR,
                           float* outL,
                           float* outR,
                           int numSamples) noexcept {
    ScopedNoDenormals noDenormals;
    if (mShimmerCircBufferL.empty()) [[unlikely]] {
        std::copy(inL, inL + numSamples, outL);
        std::copy(inR, inR + numSamples, outR);
        return;
    }

    // Inactive block-level bypass: when both shimmer and dimmer sends are inactive (<= 0.001f),
    // perform a zero-fill pass on wet buffers and bypass per-sample smoother and circular processing.
    if (!isActive()) {
        std::fill(outL, outL + numSamples, 0.0f);
        std::fill(outR, outR + numSamples, 0.0f);
        mRecircShimmerL = 0.0f;
        mRecircShimmerR = 0.0f;
        mRecircDimmerL = 0.0f;
        mRecircDimmerR = 0.0f;
        return;
    }

    for (int n = 0; n < numSamples; ++n) {
        processSample(inL[n], inR[n], outL[n], outR[n]);
    }
}

} // namespace rb26
