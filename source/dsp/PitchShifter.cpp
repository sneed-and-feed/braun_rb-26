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
    mWindowSamples = mWindowSec * mSampleRate;

    const float slope = 1.0f - mRatio;
    const float absSlope = std::abs(slope);

    if (absSlope > 1.0e-5f) {
        // Delta phi = |1 - r| / (W * fs)
        mPhaseInc = absSlope / mWindowSamples;
    } else {
        mPhaseInc = 0.0f;
    }
}

float DualTapDelayPitchShifter::processSample(float input) noexcept {
    // Write sample to delay buffer
    mDelayBuffer[static_cast<size_t>(mWriteIndex)] = flushDenormal(input);

    const float phase1 = mPhase;
    const float phase2 = (phase1 >= 0.5f) ? (phase1 - 0.5f) : (phase1 + 0.5f);

    // Calculate delay excursions
    float delay1 = 0.0f;
    float delay2 = 0.0f;

    if (mRatio >= 1.0f) {
        // Upward pitch shift (Shimmer): delay ramps downwards from W to 0
        delay1 = mWindowSamples * (1.0f - phase1);
        delay2 = mWindowSamples * (1.0f - phase2);
    } else {
        // Downward pitch shift (Dimmer): delay ramps upwards from 0 to W
        delay1 = mWindowSamples * phase1;
        delay2 = mWindowSamples * phase2;
    }

    // Read head fractional indices
    const float readPos1 = static_cast<float>(mWriteIndex) - delay1;
    const float readPos2 = static_cast<float>(mWriteIndex) - delay2;

    const float out1 = readHermite(readPos1);
    const float out2 = readHermite(readPos2);

    // Constant-power sine crossfade windows: w1^2 + w2^2 == 1.0
    const float w1 = std::sin(kPi * phase1);
    const float w2 = std::sin(kPi * phase2);

    // Advance normalized phase accumulator
    mPhase += mPhaseInc;
    if (mPhase >= 1.0f) {
        mPhase -= 1.0f;
    }

    // Advance write head with bitmask wrapping
    mWriteIndex = (mWriteIndex + 1) & kBufferMask;

    return flushDenormal(w1 * out1 + w2 * out2);
}

inline float DualTapDelayPitchShifter::readHermite(float readPos) const noexcept {
    const float bufSz = static_cast<float>(kMaxCapacity);
    while (readPos < 0.0f) readPos += bufSz;
    while (readPos >= bufSz) readPos -= bufSz;

    const int i0 = static_cast<int>(readPos);
    const float frac = readPos - static_cast<float>(i0);

    const int im1 = (i0 - 1 + kMaxCapacity) & kBufferMask;
    const int i1 = (i0 + 1) & kBufferMask;
    const int i2 = (i0 + 2) & kBufferMask;

    const float ym1 = mDelayBuffer[static_cast<size_t>(im1)];
    const float y0  = mDelayBuffer[static_cast<size_t>(i0)];
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
}

void PitchShifter::setParameters(float shimmerSend,
                                float dimmerSend,
                                int shimmerInterval,
                                int dimmerInterval,
                                float pitchBlend,
                                float pitchFeedback) noexcept {
    mShimmerSendSmoother.setTarget(std::clamp(shimmerSend, 0.0f, 1.0f));
    mDimmerSendSmoother.setTarget(std::clamp(dimmerSend, 0.0f, 1.0f));
    mPitchBlendSmoother.setTarget(std::clamp(pitchBlend, -1.0f, 1.0f));
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
    const float sSend = mShimmerSendSmoother.next();
    const float dSend = mDimmerSendSmoother.next();
    const float blend = mPitchBlendSmoother.next();
    const float fb = mPitchFeedbackSmoother.next();
    const float spiralDepth = mSpiralDepthSmoother.next();

    // Equal-power crossfade weighting based on blend beta in [-1.0, +1.0]
    const float blendAngle = (kPi * 0.25f) * (1.0f - blend);
    const float blendShim = std::cos(blendAngle);
    const float blendDim  = std::sin(blendAngle);

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
        std::fill(mShimmerCircBufferL.begin(), mShimmerCircBufferL.end(), 0.0f);
        std::fill(mShimmerCircBufferR.begin(), mShimmerCircBufferR.end(), 0.0f);
        mShimmerFilterL.reset();
        mShimmerFilterR.reset();
        mShimmerShifterL.reset();
        mShimmerShifterR.reset();
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
        std::fill(mDimmerCircBufferL.begin(), mDimmerCircBufferL.end(), 0.0f);
        std::fill(mDimmerCircBufferR.begin(), mDimmerCircBufferR.end(), 0.0f);
        mDimmerFilterL.reset();
        mDimmerFilterR.reset();
        mDimmerShifterL.reset();
        mDimmerShifterR.reset();
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
    for (int n = 0; n < numSamples; ++n) {
        processSample(inL[n], inR[n], outL[n], outR[n]);
    }
}

} // namespace rb26
