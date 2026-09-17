#include "Rb26Engine.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

Rb26ReverbEngine::Rb26ReverbEngine() noexcept {
    mInputTrimSmoother.setTimeConstant(0.030f);
    mPreDelaySmoother.setTimeConstant(0.040f);
    mDryWetSmoother.setTimeConstant(0.030f);
    mEarlyLateSmoother.setTimeConstant(0.030f);
    mStereoWidthSmoother.setTimeConstant(0.030f);
    mOutputTrimSmoother.setTimeConstant(0.030f);

    mPitchFeedbackHpL.configure(Biquad::Type::Highpass, 48000.0f, 150.0f, 0.70710678f);
    mPitchFeedbackHpR.configure(Biquad::Type::Highpass, 48000.0f, 150.0f, 0.70710678f);
    mPitchFeedbackLpL.configure(Biquad::Type::Lowpass, 48000.0f, 6000.0f, 0.70710678f);
    mPitchFeedbackLpR.configure(Biquad::Type::Lowpass, 48000.0f, 6000.0f, 0.70710678f);

    mPreDelayBufferL.assign(kPreDelayBufferCapacity, 0.0f);
    mPreDelayBufferR.assign(kPreDelayBufferCapacity, 0.0f);
    mPreDelayWriteIndex = 0;

    mTelemetryWriteIndex.store(0, std::memory_order_relaxed);
    mTelemetryReadIndex.store(0, std::memory_order_relaxed);
}

void Rb26ReverbEngine::prepare(double sampleRate, int maxBlockSize) noexcept {
    mSampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    mMaxBlockSize = std::max(64, maxBlockSize);
    const float fs = static_cast<float>(mSampleRate);

    mLowBandMatrix.prepare(mSampleRate);
    mEarlyReflections.prepare(mSampleRate, 4.0f);
    mFdnTank.prepare(mSampleRate, 4.0f);
    mPitchShifter.prepare(mSampleRate, mMaxBlockSize);
    mMasterSubMono.prepare(mSampleRate);

    // Band-limit pitch feedback path: 150 Hz HPF + 6 kHz LPF Butterworth filters
    mPitchFeedbackHpL.configure(Biquad::Type::Highpass, fs, 150.0f, 0.70710678f);
    mPitchFeedbackHpR.configure(Biquad::Type::Highpass, fs, 150.0f, 0.70710678f);
    mPitchFeedbackLpL.configure(Biquad::Type::Lowpass, fs, 6000.0f, 0.70710678f);
    mPitchFeedbackLpR.configure(Biquad::Type::Lowpass, fs, 6000.0f, 0.70710678f);

    mInputTrimSmoother.setSampleRate(fs);
    mInputTrimSmoother.reset(dbToGain(mParams.inputTrimDb));

    mPreDelaySmoother.setSampleRate(fs);
    mPreDelaySmoother.reset(mParams.preDelayMs);

    mDryWetSmoother.setSampleRate(fs);
    mDryWetSmoother.reset(mParams.dryWetMix);

    mEarlyLateSmoother.setSampleRate(fs);
    mEarlyLateSmoother.reset(mParams.earlyLateMix);

    mStereoWidthSmoother.setSampleRate(fs);
    mStereoWidthSmoother.reset(mParams.stereoWidth);

    mOutputTrimSmoother.setSampleRate(fs);
    mOutputTrimSmoother.reset(dbToGain(mParams.outputTrimDb));

    mPitchFeedbackSmoother.setSampleRate(fs);
    mPitchFeedbackSmoother.reset(mParams.pitchFeedback);

    mPitchDelaySmoother.setSampleRate(fs);
    mPitchDelaySmoother.setTimeConstant(0.040f);
    mPitchDelaySmoother.reset(mParams.pitchDelayMs);

    mPitchBlendSmoother.setSampleRate(fs);
    mPitchBlendSmoother.setTimeConstant(0.020f);
    mPitchBlendSmoother.reset(mParams.pitchBlend);

    mPreDelayBufferL.assign(kPreDelayBufferCapacity, 0.0f);
    mPreDelayBufferR.assign(kPreDelayBufferCapacity, 0.0f);
    mPreDelayWriteIndex = 0;

    mPitchDelayBufferL.assign(kPitchDelayCapacity, 0.0f);
    mPitchDelayBufferR.assign(kPitchDelayCapacity, 0.0f);
    mPitchDelayWriteIndex = 0;

    setParameters(mParams);
    reset();
}

void Rb26ReverbEngine::reset() noexcept {
    mIsIdle = false;
    mSilentSamplesCount = 0;
    mTailEnergyFollower = 0.0f;

    std::fill(mPreDelayBufferL.begin(), mPreDelayBufferL.end(), 0.0f);
    std::fill(mPreDelayBufferR.begin(), mPreDelayBufferR.end(), 0.0f);
    mPreDelayWriteIndex = 0;

    std::fill(mPitchDelayBufferL.begin(), mPitchDelayBufferL.end(), 0.0f);
    std::fill(mPitchDelayBufferR.begin(), mPitchDelayBufferR.end(), 0.0f);
    mPitchDelayWriteIndex = 0;

    mLastPitchFbL = 0.0f;
    mLastPitchFbR = 0.0f;

    mPitchFeedbackHpL.reset();
    mPitchFeedbackHpR.reset();
    mPitchFeedbackLpL.reset();
    mPitchFeedbackLpR.reset();

    mInputTrimSmoother.reset(dbToGain(mParams.inputTrimDb));
    mPreDelaySmoother.reset(mParams.preDelayMs);
    mDryWetSmoother.reset(mParams.dryWetMix);
    mEarlyLateSmoother.reset(mParams.earlyLateMix);
    mStereoWidthSmoother.reset(mParams.stereoWidth);
    mOutputTrimSmoother.reset(dbToGain(mParams.outputTrimDb));

    mPitchFeedbackSmoother.reset(mParams.pitchFeedback);
    mPitchDelaySmoother.reset(mParams.pitchDelayMs);
    mPitchBlendSmoother.reset(mParams.pitchBlend);

    mMasterSubMono.reset();
    mLowBandMatrix.reset();
    mEarlyReflections.reset();
    mFdnTank.reset();
    mPitchShifter.reset();

    mInputRmsSumL = 0.0f;
    mInputRmsSumR = 0.0f;
    mOutputRmsSumL = 0.0f;
    mOutputRmsSumR = 0.0f;
    mCorrelationSum = 0.0f;
    mDecayPeakFollower = 0.0f;
    mTelemetryDecimator = 0;

    mTelemetryWriteIndex.store(0, std::memory_order_relaxed);
    mTelemetryReadIndex.store(0, std::memory_order_relaxed);
}

void Rb26ReverbEngine::setParameters(const Rb26Parameters& params) noexcept {
    if (params.freezeHold && mIsIdle) {
        mIsIdle = false;
        mSilentSamplesCount = 0;
    }

    mParams = params;

    LowBandModalParams lbParams;
    lbParams.crossoverHz = params.lowCrossoverHz;
    lbParams.bassRt60Mult = params.bassRt60Mult;
    lbParams.rt60DecaySec = params.decayRt60Sec;
    lbParams.punchDucking = params.punchDucking;
    lbParams.subMonoHz = params.subMonoHz;
    lbParams.freezeHold = params.freezeHold;
    mLowBandMatrix.setParameters(lbParams);

    mMasterSubMono.setCutoff(params.subMonoHz);
    mEarlyReflections.setParameters(params.roomSize, params.diffusionDensity);

    mFdnTank.setParameters(params.roomSize,
                           params.decayRt60Sec,
                           params.highDampingHz,
                           params.diffusionDensity,
                           params.freezeHold,
                           params.tailModRateHz,
                           params.tailModDepthMs,
                           params.tailBloomMs);

    // Pass 0.0f internal feedback to pitch shifter: feedback loop is closed via FDN tank
    mPitchShifter.setParameters(params.shimmerSend,
                                params.dimmerSend,
                                params.shimmerInterval,
                                params.dimmerInterval,
                                params.pitchBlend,
                                0.0f);

    mInputTrimSmoother.setTarget(dbToGain(params.inputTrimDb));
    mPreDelaySmoother.setTarget(params.preDelayMs);
    mDryWetSmoother.setTarget(params.dryWetMix);
    mEarlyLateSmoother.setTarget(params.earlyLateMix);
    mStereoWidthSmoother.setTarget(params.stereoWidth);
    mOutputTrimSmoother.setTarget(dbToGain(params.outputTrimDb));
    mPitchFeedbackSmoother.setTarget(std::clamp(params.pitchFeedback, 0.0f, 0.95f));
    mPitchDelaySmoother.setTarget(std::clamp(params.pitchDelayMs, 20.0f, 500.0f));
    mPitchBlendSmoother.setTarget(std::clamp(params.pitchBlend, -1.0f, 1.0f));

    if (params.shimmerSend <= 1.0e-4f && params.dimmerSend <= 1.0e-4f) {
        mPitchFeedbackSmoother.snapTo(0.0f);
        mPitchBlendSmoother.snapTo(std::clamp(params.pitchBlend, -1.0f, 1.0f));
    }
}

void Rb26ReverbEngine::process(const float* const* inputChannels,
                              float* const* outputChannels,
                              int numChannels,
                              int numSamples,
                              const float* const* auxReverbChannels) noexcept {
    if (!inputChannels || !outputChannels || numChannels <= 0 || numSamples <= 0) [[unlikely]] {
        return;
    }

    ScopedNoDenormals noDenormals;

    const float* inL = inputChannels[0];
    const float* inR = (numChannels > 1 && inputChannels[1]) ? inputChannels[1] : inL;
    const float* auxL = (auxReverbChannels && auxReverbChannels[0]) ? auxReverbChannels[0] : nullptr;
    const float* auxR = (auxReverbChannels && numChannels > 1 && auxReverbChannels[1]) ? auxReverbChannels[1] : auxL;

    float* outL = outputChannels[0];
    float* outR = (numChannels > 1 && outputChannels[1]) ? outputChannels[1] : nullptr;

    // Fast check: does incoming block contain any signal?
    bool blockHasSignal = false;
    for (int n = 0; n < numSamples; ++n) {
        const float peakL = std::abs(inL[n]);
        const float peakR = std::abs(inR[n]);
        const float auxPeakL = auxL ? std::abs(auxL[n]) : 0.0f;
        const float auxPeakR = auxR ? std::abs(auxR[n]) : 0.0f;
        if (peakL > 1.0e-7f || peakR > 1.0e-7f || auxPeakL > 1.0e-7f || auxPeakR > 1.0e-7f) {
            blockHasSignal = true;
            break;
        }
    }

    // Freeze mode cannot be idle
    if (mParams.freezeHold) {
        mIsIdle = false;
        mSilentSamplesCount = 0;
    }

    // Fast path: Idle Silence Gating
    if (mIsIdle) {
        if (!blockHasSignal && !mParams.freezeHold) {
            std::fill(outL, outL + numSamples, 0.0f);
            if (outR) {
                std::fill(outR, outR + numSamples, 0.0f);
            }

            // Snap smoothers to targets so parameter updates while idle remain synchronized
            mInputTrimSmoother.snapTo(mInputTrimSmoother.getTarget());
            mPreDelaySmoother.snapTo(mPreDelaySmoother.getTarget());
            mDryWetSmoother.snapTo(mDryWetSmoother.getTarget());
            mEarlyLateSmoother.snapTo(mEarlyLateSmoother.getTarget());
            mStereoWidthSmoother.snapTo(mStereoWidthSmoother.getTarget());
            mOutputTrimSmoother.snapTo(mOutputTrimSmoother.getTarget());
            mPitchFeedbackSmoother.snapTo(mPitchFeedbackSmoother.getTarget());
            mPitchDelaySmoother.snapTo(mPitchDelaySmoother.getTarget());
            mPitchBlendSmoother.snapTo(mPitchBlendSmoother.getTarget());

            // Decimate telemetry
            mTelemetryDecimator += numSamples;
            if (mTelemetryDecimator >= 512) {
                mTelemetryDecimator = 0;
                VisualizerFrame zeroFrame {};
                zeroFrame.correlation = 1.0f;
                pushVisualizerFrame(zeroFrame);
            }
            return;
        }

        // Signal detected: instantly wake up from idle state
        mIsIdle = false;
        mSilentSamplesCount = 0;
    }

    const float fs = static_cast<float>(mSampleRate);
    const bool blockPitchActive = mPitchShifter.isActive();

    if (!blockPitchActive) {
        mPitchDelaySmoother.snapTo(mPitchDelaySmoother.getTarget());
        mPitchBlendSmoother.snapTo(mPitchBlendSmoother.getTarget());
        mPitchFeedbackSmoother.snapTo(mPitchFeedbackSmoother.getTarget());
    }

    for (int n = 0; n < numSamples; ++n) {
        const float inTrim = mInputTrimSmoother.next();
        const float xL = inL[n];
        const float xR = inR[n];
        const float tankInL = (xL + (auxL ? auxL[n] : 0.0f)) * inTrim;
        const float tankInR = (xR + (auxR ? auxR[n] : 0.0f)) * inTrim;

        // 1. Pre-Delay
        const float curPreMs = mPreDelaySmoother.next();
        const float preDelaySamples = std::clamp((curPreMs * 0.001f) * fs, 0.0f, static_cast<float>(kPreDelayBufferCapacity - 64));

        mPreDelayBufferL[mPreDelayWriteIndex] = flushDenormal(tankInL);
        mPreDelayBufferR[mPreDelayWriteIndex] = flushDenormal(tankInR);

        const float preL = TailModulator::readHermite(mPreDelayBufferL.data(),
                                                      kPreDelayBufferCapacity,
                                                      kPreDelayBufferMask,
                                                      mPreDelayWriteIndex,
                                                      preDelaySamples);
        const float preR = TailModulator::readHermite(mPreDelayBufferR.data(),
                                                      kPreDelayBufferCapacity,
                                                      kPreDelayBufferMask,
                                                      mPreDelayWriteIndex,
                                                      preDelaySamples);
        mPreDelayWriteIndex = (mPreDelayWriteIndex + 1) & kPreDelayBufferMask;

        // 2. Decoupled LR4 Crossover & Low-Band Modal Matrix
        float highInL = 0.0f, highInR = 0.0f;
        float lowReverbL = 0.0f, lowReverbR = 0.0f;
        mLowBandMatrix.processSample(preL, preR, highInL, highInR, lowReverbL, lowReverbR);

        // 3. Early Reflections Matrix (12 static prime taps, 100% time-invariant)
        float earlyL = 0.0f, earlyR = 0.0f;
        mEarlyReflections.processSample(highInL, highInR, earlyL, earlyR);

        // 4. Decoupled Pitch Delay & Bidirectional Pitch Shifting Feedback
        const bool pitchActive = blockPitchActive;
        const float elMix = mEarlyLateSmoother.next();
        const float earlyGain = FastSinTable::cos(elMix * kHalfPi);
        const float lateGain  = FastSinTable::sin(elMix * kHalfPi);

        float lateL = 0.0f, lateR = 0.0f;
        float highReverbL = 0.0f;
        float highReverbR = 0.0f;

        if (pitchActive) {
            const float curPitchDelayMs = mPitchDelaySmoother.next();
            const float pBlend = mPitchBlendSmoother.next();
            const float delayMult = 1.0f + 0.35f * std::max(0.0f, -pBlend);
            const float effDelayMs = curPitchDelayMs * delayMult;
            const float delaySamplesL = std::clamp((effDelayMs * 0.001f) * fs, 2.0f, static_cast<float>(kPitchDelayCapacity - 64));
            const float delaySamplesR = std::clamp((effDelayMs * 0.001f * 1.07f) * fs, 2.0f, static_cast<float>(kPitchDelayCapacity - 64));

            const float delayedPitchL = TailModulator::readHermite(mPitchDelayBufferL.data(),
                                                                  kPitchDelayCapacity,
                                                                  kPitchDelayMask,
                                                                  mPitchDelayWriteIndex,
                                                                  delaySamplesL);
            const float delayedPitchR = TailModulator::readHermite(mPitchDelayBufferR.data(),
                                                                  kPitchDelayCapacity,
                                                                  kPitchDelayMask,
                                                                  mPitchDelayWriteIndex,
                                                                  delaySamplesR);

            const float fb = mPitchFeedbackSmoother.next();
            const float safePitchFb = fb * 0.30f;
            const float filteredPitchL = mPitchFeedbackLpL.process(mPitchFeedbackHpL.process(delayedPitchL));
            const float filteredPitchR = mPitchFeedbackLpR.process(mPitchFeedbackHpR.process(delayedPitchR));
            const float injPitchL = filteredPitchL * safePitchFb;
            const float injPitchR = filteredPitchR * safePitchFb;

            mFdnTank.processSample(highInL, highInR, injPitchL, injPitchR, lateL, lateR);

            // Feed late reverberation into PitchShifter to calculate next shifted sample
            float shiftedL = 0.0f, shiftedR = 0.0f;
            mPitchShifter.processSample(lateL, lateR, shiftedL, shiftedR);

            // Store shifted sample into decoupled delay buffer for distinct temporal spacing
            mPitchDelayBufferL[mPitchDelayWriteIndex] = flushDenormal(shiftedL);
            mPitchDelayBufferR[mPitchDelayWriteIndex] = flushDenormal(shiftedR);
            mPitchDelayWriteIndex = (mPitchDelayWriteIndex + 1) & kPitchDelayMask;

            mLastPitchFbL = shiftedL;
            mLastPitchFbR = shiftedR;

            // 5. Early / Late Mix (Equal-power trigonometric balance)
            const float pitchAddL = delayedPitchL * 0.85f;
            const float pitchAddR = delayedPitchR * 0.85f;
            highReverbL = earlyGain * earlyL + lateGain * (lateL + pitchAddL);
            highReverbR = earlyGain * earlyR + lateGain * (lateR + pitchAddR);
        } else {
            // Bypass pitch branch: zero pitch injection into FdnTank, pass late reverb straight through
            mFdnTank.processSample(highInL, highInR, 0.0f, 0.0f, lateL, lateR);

            mLastPitchFbL = 0.0f;
            mLastPitchFbR = 0.0f;

            // 5. Early / Late Mix directly with lateL and lateR (bypassing pitch addition)
            highReverbL = earlyGain * earlyL + lateGain * lateL;
            highReverbR = earlyGain * earlyR + lateGain * lateR;
        }

        // 6. Recombine High-Band Reverb with Pristine Low-Band Modal Reverb
        float wetL = highReverbL + lowReverbL;
        float wetR = highReverbR + lowReverbR;

        // Sub-bass elliptical M/S filter on wet bus to eliminate bass phase cancellation below subMonoHz
        mMasterSubMono.process(wetL, wetR, wetL, wetR);

        // 7. Mid/Side (M/S) Stereo Width Stage
        const float width = mStereoWidthSmoother.next();
        const float wetMid  = 0.5f * (wetL + wetR);
        const float wetSide = 0.5f * (wetL - wetR);
        wetL = wetMid + width * wetSide;
        wetR = wetMid - width * wetSide;

        // 8. Dry / Wet Summing (Equal-power trigonometric crossfade)
        const float dwMix = mDryWetSmoother.next();
        const float dryGain = FastSinTable::cos(dwMix * kHalfPi);
        const float wetGain = FastSinTable::sin(dwMix * kHalfPi);
        float sampleOutL = dryGain * (xL * inTrim) + wetGain * wetL;
        float sampleOutR = dryGain * (xR * inTrim) + wetGain * wetR;

        // 9. Master Output Trim
        const float trim = mOutputTrimSmoother.next();
        sampleOutL *= trim;
        sampleOutR *= trim;

        // 10. Master Bus Soft Limiter
        if (mParams.limiterEnable) {
            sampleOutL = softLimit(sampleOutL);
            sampleOutR = softLimit(sampleOutR);
        }

        // Output assignment
        sampleOutL = flushDenormal(sampleOutL);
        sampleOutR = flushDenormal(sampleOutR);
        outL[n] = sampleOutL;
        if (outR) {
            outR[n] = sampleOutR;
        }

        // Telemetry accumulation
        mInputRmsSumL += xL * xL;
        mInputRmsSumR += xR * xR;
        mOutputRmsSumL += sampleOutL * sampleOutL;
        mOutputRmsSumR += sampleOutR * sampleOutR;
        mCorrelationSum += sampleOutL * sampleOutR;

        const float outPeak = std::max(std::abs(sampleOutL), std::abs(sampleOutR));
        mDecayPeakFollower = std::max(outPeak, mDecayPeakFollower * 0.999f);
        mTailEnergyFollower = flushDenormal(0.999f * mTailEnergyFollower + 0.001f * outPeak);

        // Check if signal has completely died down below audible threshold
        const float inPeak = std::max(std::abs(xL), std::abs(xR)) + (auxL ? std::abs(auxL[n]) : 0.0f) + (auxR ? std::abs(auxR[n]) : 0.0f);
        if (inPeak < 1.0e-7f && outPeak < 1.0e-7f && mTailEnergyFollower < 1.0e-7f && !mParams.freezeHold) {
            mSilentSamplesCount++;
        } else {
            mSilentSamplesCount = 0;
        }

        // Periodically emit visualizer frame (~every 512 samples, approx 93 Hz at 48kHz)
        if (++mTelemetryDecimator >= 512) {
            mTelemetryDecimator = 0;
            VisualizerFrame frame;
            const float invCount = 1.0f / 512.0f;
            frame.inputRmsL = std::sqrt(mInputRmsSumL * invCount);
            frame.inputRmsR = std::sqrt(mInputRmsSumR * invCount);
            frame.outputRmsL = std::sqrt(mOutputRmsSumL * invCount);
            frame.outputRmsR = std::sqrt(mOutputRmsSumR * invCount);

            const float denom = (frame.outputRmsL * frame.outputRmsR);
            frame.correlation = (denom > 1.0e-5f) ? std::clamp((mCorrelationSum * invCount) / denom, -1.0f, 1.0f) : 1.0f;

            frame.lowEnergy = mLowBandMatrix.getLowBandEnergy();
            frame.midEnergy = std::sqrt(0.5f * (lateL * lateL + lateR * lateR));
            frame.highEnergy = std::sqrt(0.5f * (earlyL * earlyL + earlyR * earlyR));
            frame.decayEnvelope = mDecayPeakFollower;

            pushVisualizerFrame(frame);

            mInputRmsSumL = 0.0f;
            mInputRmsSumR = 0.0f;
            mOutputRmsSumL = 0.0f;
            mOutputRmsSumR = 0.0f;
            mCorrelationSum = 0.0f;
        }
    }

    // If silence sustained for > 2048 samples (approx 42ms after reaching sub -140dB), transition to idle
    if (mSilentSamplesCount >= 2048 && !mParams.freezeHold) {
        mIsIdle = true;
        // Flush all internal delay buffers, filters, and recursive states to clean zero
        mLowBandMatrix.reset();
        mEarlyReflections.reset();
        mFdnTank.reset();
        mPitchShifter.reset();
        mMasterSubMono.reset();
        std::fill(mPreDelayBufferL.begin(), mPreDelayBufferL.end(), 0.0f);
        std::fill(mPreDelayBufferR.begin(), mPreDelayBufferR.end(), 0.0f);
        std::fill(mPitchDelayBufferL.begin(), mPitchDelayBufferL.end(), 0.0f);
        std::fill(mPitchDelayBufferR.begin(), mPitchDelayBufferR.end(), 0.0f);
        mLastPitchFbL = 0.0f;
        mLastPitchFbR = 0.0f;
        mDecayPeakFollower = 0.0f;
        mTailEnergyFollower = 0.0f;
    }
}

void Rb26ReverbEngine::pushVisualizerFrame(const VisualizerFrame& frame) noexcept {
    const size_t w = mTelemetryWriteIndex.load(std::memory_order_relaxed);
    const size_t r = mTelemetryReadIndex.load(std::memory_order_acquire);

    // Drop frame if queue is full to preserve real-time non-blocking guarantee
    if (((w + 1) & kTelemetryQueueMask) != (r & kTelemetryQueueMask)) {
        mTelemetryQueue[w & kTelemetryQueueMask] = frame;
        mTelemetryWriteIndex.store(w + 1, std::memory_order_release);
    }
}

bool Rb26ReverbEngine::popVisualizerFrame(VisualizerFrame& frame) noexcept {
    const size_t r = mTelemetryReadIndex.load(std::memory_order_relaxed);
    const size_t w = mTelemetryWriteIndex.load(std::memory_order_acquire);

    if (r != w) {
        frame = mTelemetryQueue[r & kTelemetryQueueMask];
        mTelemetryReadIndex.store(r + 1, std::memory_order_release);
        return true;
    }
    return false;
}

std::vector<PresetDefinition> Rb26ReverbEngine::getFactoryPresets() {
    std::vector<PresetDefinition> presets;
    presets.reserve(10);

    // 1. DEFAULT
    {
        PresetDefinition p;
        p.id = "DEFAULT";
        p.name = "CALIBRATED DEFAULT";
        p.category = "Studio General";
        p.description = "Balanced studio reverb with natural 6.5s RT60 decay and gentle shimmer/dimmer harmonic balance.";
        p.params.preDelayMs = 24.0f;
        p.params.diffusionDensity = 0.75f;
        p.params.outputTrimDb = 0.0f;
        p.params.lowCrossoverHz = 180.0f;
        p.params.bassRt60Mult = 1.0f;
        p.params.punchDucking = 0.65f;
        p.params.subMonoHz = 120.0f;
        p.params.decayRt60Sec = 6.5f;
        p.params.roomSize = 1.0f;
        p.params.highDampingHz = 1800.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.40f;
        p.params.dimmerSend = 0.35f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.0f;
        p.params.pitchFeedback = 0.45f;
        p.params.tailModRateHz = 0.65f;
        p.params.tailModDepthMs = 2.25f;
        p.params.tailBloomMs = 85.0f;
        p.params.stereoWidth = 1.0f;
        p.params.earlyLateMix = 0.50f;
        p.params.dryWetMix = 0.40f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    // 2. AMBIENT_GUITAR_CLOUD (Refined musical cloud, eliminates scary drone/feedback runaway)
    {
        PresetDefinition p;
        p.id = "AMBIENT_GUITAR_CLOUD";
        p.name = "AMBIENT GUITAR CLOUD";
        p.category = "Ambient / Guitar";
        p.description = "Lush 9.5-second ambient guitar cloud with +12 semitones shimmer bloom and wide stereo dispersion.";
        p.params.preDelayMs = 45.0f;
        p.params.diffusionDensity = 0.85f;
        p.params.outputTrimDb = 0.0f;
        p.params.lowCrossoverHz = 200.0f;
        p.params.bassRt60Mult = 0.85f;
        p.params.punchDucking = 0.50f;
        p.params.subMonoHz = 140.0f;
        p.params.decayRt60Sec = 9.5f;
        p.params.roomSize = 1.30f;
        p.params.highDampingHz = 9500.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.48f;
        p.params.dimmerSend = 0.05f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 1.0f;
        p.params.pitchFeedback = 0.42f;
        p.params.tailModRateHz = 0.40f;
        p.params.tailModDepthMs = 1.40f;
        p.params.tailBloomMs = 110.0f;
        p.params.stereoWidth = 1.40f;
        p.params.earlyLateMix = 0.70f;
        p.params.dryWetMix = 0.55f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    // 3. AS42_SHIMMER_COMPANION
    {
        PresetDefinition p;
        p.id = "AS42_SHIMMER_COMPANION";
        p.name = "AS-42 TAPE & SHIMMER COMPANION";
        p.category = "Vintage Shimmer";
        p.description = "Warm vintage tape-modulated plate with +12 octave shimmer companion tuned for acoustic instruments.";
        p.params.preDelayMs = 28.0f;
        p.params.diffusionDensity = 0.85f;
        p.params.outputTrimDb = 0.0f;
        p.params.lowCrossoverHz = 180.0f;
        p.params.bassRt60Mult = 1.0f;
        p.params.punchDucking = 0.60f;
        p.params.subMonoHz = 120.0f;
        p.params.decayRt60Sec = 8.5f;
        p.params.roomSize = 1.15f;
        p.params.highDampingHz = 6800.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.45f;
        p.params.dimmerSend = 0.25f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.40f;
        p.params.pitchFeedback = 0.50f;
        p.params.tailModRateHz = 0.65f;
        p.params.tailModDepthMs = 2.0f;
        p.params.tailBloomMs = 85.0f;
        p.params.stereoWidth = 1.20f;
        p.params.earlyLateMix = 0.55f;
        p.params.dryWetMix = 0.45f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    // 4. SOFT_FELT_ACOUSTIC_HALL
    {
        PresetDefinition p;
        p.id = "SOFT_FELT_ACOUSTIC_HALL";
        p.name = "SOFT FELT ACOUSTIC HALL";
        p.category = "Acoustic / Piano";
        p.description = "Warm, intimate wooden hall tuned for felt piano and strings with organic high damping.";
        p.params.preDelayMs = 20.0f;
        p.params.diffusionDensity = 0.78f;
        p.params.outputTrimDb = 0.0f;
        p.params.lowCrossoverHz = 160.0f;
        p.params.bassRt60Mult = 0.95f;
        p.params.punchDucking = 0.55f;
        p.params.subMonoHz = 110.0f;
        p.params.decayRt60Sec = 4.8f;
        p.params.roomSize = 0.90f;
        p.params.highDampingHz = 5600.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.15f;
        p.params.dimmerSend = 0.10f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.20f;
        p.params.pitchFeedback = 0.25f;
        p.params.tailModRateHz = 0.45f;
        p.params.tailModDepthMs = 1.25f;
        p.params.tailBloomMs = 70.0f;
        p.params.stereoWidth = 1.10f;
        p.params.earlyLateMix = 0.45f;
        p.params.dryWetMix = 0.38f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    // 5. GERMAN_PLATE_140
    {
        PresetDefinition p;
        p.id = "GERMAN_PLATE_140";
        p.name = "GERMAN PLATE 140";
        p.category = "Vintage Plate";
        p.description = "High-density EMT-style steel plate emulation with fast onset diffusion and shimmering top-end dispersion.";
        p.params.preDelayMs = 10.0f;
        p.params.diffusionDensity = 0.92f;
        p.params.outputTrimDb = 0.0f;
        p.params.lowCrossoverHz = 220.0f;
        p.params.bassRt60Mult = 0.80f;
        p.params.punchDucking = 0.70f;
        p.params.subMonoHz = 130.0f;
        p.params.decayRt60Sec = 3.8f;
        p.params.roomSize = 0.85f;
        p.params.highDampingHz = 8500.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.20f;
        p.params.dimmerSend = 0.05f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 1.0f;
        p.params.pitchFeedback = 0.20f;
        p.params.tailModRateHz = 0.80f;
        p.params.tailModDepthMs = 1.0f;
        p.params.tailBloomMs = 45.0f;
        p.params.stereoWidth = 1.30f;
        p.params.earlyLateMix = 0.40f;
        p.params.dryWetMix = 0.35f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    // 6. CATHEDRAL_DIFFUSION
    {
        PresetDefinition p;
        p.id = "CATHEDRAL_DIFFUSION";
        p.name = "CATHEDRAL DIFFUSION";
        p.category = "Hall / Cathedral";
        p.description = "Massive 18-second acoustic space with pristine high-frequency shimmer bloom and air damping.";
        p.params.preDelayMs = 45.0f;
        p.params.diffusionDensity = 0.95f;
        p.params.outputTrimDb = -2.0f;
        p.params.lowCrossoverHz = 180.0f;
        p.params.bassRt60Mult = 0.90f;
        p.params.punchDucking = 0.45f;
        p.params.subMonoHz = 120.0f;
        p.params.decayRt60Sec = 18.0f;
        p.params.roomSize = 1.75f;
        p.params.highDampingHz = 10000.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.65f;
        p.params.dimmerSend = 0.10f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.75f;
        p.params.pitchFeedback = 0.55f;
        p.params.tailModRateHz = 0.50f;
        p.params.tailModDepthMs = 2.25f;
        p.params.tailBloomMs = 140.0f;
        p.params.stereoWidth = 1.60f;
        p.params.earlyLateMix = 0.80f;
        p.params.dryWetMix = 0.65f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    // 7. ETHEREAL_SYNTH_PAD
    {
        PresetDefinition p;
        p.id = "ETHEREAL_SYNTH_PAD";
        p.name = "ETHEREAL SYNTH PAD";
        p.category = "Synthesizer";
        p.description = "Lush 10.5-second tail designed for polyphonic pads and brass, featuring balanced shimmer and dimmer.";
        p.params.preDelayMs = 35.0f;
        p.params.diffusionDensity = 0.80f;
        p.params.outputTrimDb = 0.0f;
        p.params.lowCrossoverHz = 160.0f;
        p.params.bassRt60Mult = 0.90f;
        p.params.punchDucking = 0.50f;
        p.params.subMonoHz = 100.0f;
        p.params.decayRt60Sec = 10.5f;
        p.params.roomSize = 1.20f;
        p.params.highDampingHz = 8500.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.55f;
        p.params.dimmerSend = 0.40f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.25f;
        p.params.pitchFeedback = 0.45f;
        p.params.tailModRateHz = 0.70f;
        p.params.tailModDepthMs = 2.25f;
        p.params.tailBloomMs = 90.0f;
        p.params.stereoWidth = 1.40f;
        p.params.earlyLateMix = 0.60f;
        p.params.dryWetMix = 0.50f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    // 8. BLOOM_SHIMMER_VOID
    {
        PresetDefinition p;
        p.id = "BLOOM_SHIMMER_VOID";
        p.name = "BLOOM SHIMMER VOID";
        p.category = "Ambient / Shimmer";
        p.description = "Deep ambient void where cascading octave shimmers bloom slowly behind melodic phrases.";
        p.params.preDelayMs = 50.0f;
        p.params.diffusionDensity = 0.88f;
        p.params.outputTrimDb = -1.0f;
        p.params.lowCrossoverHz = 190.0f;
        p.params.bassRt60Mult = 0.85f;
        p.params.punchDucking = 0.45f;
        p.params.subMonoHz = 130.0f;
        p.params.decayRt60Sec = 14.0f;
        p.params.roomSize = 1.50f;
        p.params.highDampingHz = 9000.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.60f;
        p.params.dimmerSend = 0.15f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.80f;
        p.params.pitchFeedback = 0.50f;
        p.params.tailModRateHz = 0.55f;
        p.params.tailModDepthMs = 2.50f;
        p.params.tailBloomMs = 160.0f;
        p.params.stereoWidth = 1.50f;
        p.params.earlyLateMix = 0.75f;
        p.params.dryWetMix = 0.60f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    // 9. INFINITE_ETHEREAL_FREEZE
    {
        PresetDefinition p;
        p.id = "INFINITE_ETHEREAL_FREEZE";
        p.name = "INFINITE ETHEREAL FREEZE";
        p.category = "Freeze / Sustained";
        p.description = "Lossless infinite recirculating ambient texture with input isolation and gentle tail modulation.";
        p.params.preDelayMs = 20.0f;
        p.params.diffusionDensity = 0.90f;
        p.params.outputTrimDb = -1.0f;
        p.params.lowCrossoverHz = 180.0f;
        p.params.bassRt60Mult = 1.0f;
        p.params.punchDucking = 0.50f;
        p.params.subMonoHz = 120.0f;
        p.params.decayRt60Sec = 30.0f;
        p.params.roomSize = 1.20f;
        p.params.highDampingHz = 8000.0f;
        p.params.freezeHold = true;
        p.params.shimmerSend = 0.45f;
        p.params.dimmerSend = 0.30f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.20f;
        p.params.pitchFeedback = 0.50f;
        p.params.tailModRateHz = 0.65f;
        p.params.tailModDepthMs = 2.25f;
        p.params.tailBloomMs = 85.0f;
        p.params.stereoWidth = 1.30f;
        p.params.earlyLateMix = 0.70f;
        p.params.dryWetMix = 0.55f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    // 10. SUB_BASS_PRESERVER
    {
        PresetDefinition p;
        p.id = "SUB_BASS_PRESERVER";
        p.name = "SUB-BASS PRESERVER";
        p.category = "Studio Bass Mix";
        p.description = "Decoupled low-end preservation isolating kick/sub bass fundamental under 180Hz while adding space.";
        p.params.preDelayMs = 15.0f;
        p.params.diffusionDensity = 0.70f;
        p.params.outputTrimDb = 0.0f;
        p.params.lowCrossoverHz = 180.0f;
        p.params.bassRt60Mult = 0.80f;
        p.params.punchDucking = 0.85f;
        p.params.subMonoHz = 150.0f;
        p.params.decayRt60Sec = 4.5f;
        p.params.roomSize = 0.80f;
        p.params.highDampingHz = 6500.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.25f;
        p.params.dimmerSend = 0.15f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.10f;
        p.params.pitchFeedback = 0.35f;
        p.params.tailModRateHz = 0.40f;
        p.params.tailModDepthMs = 1.50f;
        p.params.tailBloomMs = 60.0f;
        p.params.stereoWidth = 1.0f;
        p.params.earlyLateMix = 0.45f;
        p.params.dryWetMix = 0.35f;
        p.params.limiterEnable = true;
        presets.push_back(p);
    }

    return presets;
}

} // namespace rb26
