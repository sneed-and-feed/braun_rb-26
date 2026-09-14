#include "Rb26Engine.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

Rb26ReverbEngine::Rb26ReverbEngine() noexcept {
    mPreDelaySmoother.setTimeConstant(0.040f);
    mDryWetSmoother.setTimeConstant(0.030f);
    mEarlyLateSmoother.setTimeConstant(0.030f);
    mStereoWidthSmoother.setTimeConstant(0.030f);
    mOutputTrimSmoother.setTimeConstant(0.030f);

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
    mEarlyReflections.prepare(mSampleRate);
    mFdnTank.prepare(mSampleRate);
    mPitchShifter.prepare(mSampleRate, mMaxBlockSize);

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

    mPreDelayBufferL.assign(kPreDelayBufferCapacity, 0.0f);
    mPreDelayBufferR.assign(kPreDelayBufferCapacity, 0.0f);
    mPreDelayWriteIndex = 0;

    setParameters(mParams);
    reset();
}

void Rb26ReverbEngine::reset() noexcept {
    std::fill(mPreDelayBufferL.begin(), mPreDelayBufferL.end(), 0.0f);
    std::fill(mPreDelayBufferR.begin(), mPreDelayBufferR.end(), 0.0f);
    mPreDelayWriteIndex = 0;

    mLastPitchFbL = 0.0f;
    mLastPitchFbR = 0.0f;

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
    mParams = params;

    LowBandModalParams lbParams;
    lbParams.crossoverHz = params.lowCrossoverHz;
    lbParams.bassRt60Mult = params.bassRt60Mult;
    lbParams.rt60DecaySec = params.decayRt60Sec;
    lbParams.punchDucking = params.punchDucking;
    lbParams.subMonoHz = params.subMonoHz;
    lbParams.freezeHold = params.freezeHold;
    mLowBandMatrix.setParameters(lbParams);

    mEarlyReflections.setParameters(params.roomSize);

    mFdnTank.setParameters(params.roomSize,
                           params.decayRt60Sec,
                           params.highDampingHz,
                           params.diffusionDensity,
                           params.freezeHold,
                           params.tailModRateHz,
                           params.tailModDepthMs,
                           params.tailBloomMs);

    mPitchShifter.setParameters(params.shimmerSend,
                                params.dimmerSend,
                                params.shimmerInterval,
                                params.dimmerInterval,
                                params.pitchBlend,
                                params.pitchFeedback);

    mPreDelaySmoother.setTarget(params.preDelayMs);
    mDryWetSmoother.setTarget(params.dryWetMix);
    mEarlyLateSmoother.setTarget(params.earlyLateMix);
    mStereoWidthSmoother.setTarget(params.stereoWidth);
    mOutputTrimSmoother.setTarget(dbToGain(params.outputTrimDb));
}

void Rb26ReverbEngine::process(const float* const* inputChannels,
                              float* const* outputChannels,
                              int numChannels,
                              int numSamples) noexcept {
    if (!inputChannels || !outputChannels || numChannels <= 0 || numSamples <= 0) [[unlikely]] {
        return;
    }

    ScopedNoDenormals noDenormals;

    const float* inL = inputChannels[0];
    const float* inR = (numChannels > 1 && inputChannels[1]) ? inputChannels[1] : inL;

    float* outL = outputChannels[0];
    float* outR = (numChannels > 1 && outputChannels[1]) ? outputChannels[1] : nullptr;

    const float fs = static_cast<float>(mSampleRate);

    for (int n = 0; n < numSamples; ++n) {
        const float xL = inL[n];
        const float xR = inR[n];

        // 1. Pre-Delay
        const float curPreMs = mPreDelaySmoother.next();
        const size_t preDelaySamples = static_cast<size_t>(
            std::clamp((curPreMs * 0.001f) * fs, 0.0f, static_cast<float>(kPreDelayBufferCapacity - 1))
        );

        mPreDelayBufferL[mPreDelayWriteIndex] = flushDenormal(xL);
        mPreDelayBufferR[mPreDelayWriteIndex] = flushDenormal(xR);

        const size_t preReadIdx = (mPreDelayWriteIndex + kPreDelayBufferCapacity - preDelaySamples) & kPreDelayBufferMask;
        const float preL = mPreDelayBufferL[preReadIdx];
        const float preR = mPreDelayBufferR[preReadIdx];
        mPreDelayWriteIndex = (mPreDelayWriteIndex + 1) & kPreDelayBufferMask;

        // 2. Decoupled LR4 Crossover & Low-Band Modal Matrix
        float highInL = 0.0f, highInR = 0.0f;
        float lowReverbL = 0.0f, lowReverbR = 0.0f;
        mLowBandMatrix.processSample(preL, preR, highInL, highInR, lowReverbL, lowReverbR);

        // 3. Early Reflections Matrix (12 static prime taps, 100% time-invariant)
        float earlyL = 0.0f, earlyR = 0.0f;
        mEarlyReflections.processSample(highInL, highInR, earlyL, earlyR);

        // 4. Late Reverb Tank (8-Line Householder FDN) & Bidirectional Pitch Shifting Feedback
        float lateL = 0.0f, lateR = 0.0f;
        mFdnTank.processSample(highInL, highInR, mLastPitchFbL, mLastPitchFbR, lateL, lateR);

        // Feed late reverberation into PitchShifter to calculate next recirculation sample
        mPitchShifter.processSample(lateL, lateR, mLastPitchFbL, mLastPitchFbR);

        // 5. Early / Late Mix (Equal-power trigonometric balance)
        const float elMix = mEarlyLateSmoother.next();
        const float earlyGain = std::cos(elMix * kHalfPi);
        const float lateGain  = std::sin(elMix * kHalfPi);
        const float highReverbL = earlyGain * earlyL + lateGain * lateL;
        const float highReverbR = earlyGain * earlyR + lateGain * lateR;

        // 6. Recombine High-Band Reverb with Pristine Low-Band Modal Reverb
        float wetL = highReverbL + lowReverbL;
        float wetR = highReverbR + lowReverbR;

        // 7. Mid/Side (M/S) Stereo Width Stage
        const float width = mStereoWidthSmoother.next();
        const float wetMid  = 0.5f * (wetL + wetR);
        const float wetSide = 0.5f * (wetL - wetR);
        wetL = wetMid + width * wetSide;
        wetR = wetMid - width * wetSide;

        // 8. Dry / Wet Summing (Equal-power trigonometric crossfade)
        const float dwMix = mDryWetSmoother.next();
        const float dryGain = std::cos(dwMix * kHalfPi);
        const float wetGain = std::sin(dwMix * kHalfPi);
        float sampleOutL = dryGain * xL + wetGain * wetL;
        float sampleOutR = dryGain * xR + wetGain * wetR;

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
        outL[n] = flushDenormal(sampleOutL);
        if (outR) {
            outR[n] = flushDenormal(sampleOutR);
        }

        // Telemetry accumulation
        mInputRmsSumL += xL * xL;
        mInputRmsSumR += xR * xR;
        mOutputRmsSumL += sampleOutL * sampleOutL;
        mOutputRmsSumR += sampleOutR * sampleOutR;
        mCorrelationSum += sampleOutL * sampleOutR;

        const float outPeak = std::max(std::abs(sampleOutL), std::abs(sampleOutR));
        mDecayPeakFollower = std::max(outPeak, mDecayPeakFollower * 0.999f);

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

} // namespace rb26
