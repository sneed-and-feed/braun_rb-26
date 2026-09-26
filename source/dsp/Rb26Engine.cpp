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
    mPitchBoostSmoother.setTimeConstant(0.020f);
    mPitchWarpSmoother.setTimeConstant(0.050f);

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

    // Initial feedback filter bandwidth: HPF 150 Hz -> 40 Hz, LPF 6 kHz -> 16 kHz with pitchWarp
    const float initWarp = mParams.pitchWarp ? 1.0f : 0.0f;
    const float initHp = (1.0f - initWarp) * 150.0f + initWarp * 40.0f;
    const float initLp = (1.0f - initWarp) * 6000.0f + initWarp * 16000.0f;
    mPitchFeedbackHpL.configure(Biquad::Type::Highpass, fs, initHp, 0.70710678f);
    mPitchFeedbackHpR.configure(Biquad::Type::Highpass, fs, initHp, 0.70710678f);
    mPitchFeedbackLpL.configure(Biquad::Type::Lowpass, fs, initLp, 0.70710678f);
    mPitchFeedbackLpR.configure(Biquad::Type::Lowpass, fs, initLp, 0.70710678f);
    mLastFilterWarp = initWarp;
    mFlutterPhase1 = 0.0f;
    mFlutterPhase2 = 0.0f;

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

    mPitchBoostSmoother.setSampleRate(fs);
    mPitchBoostSmoother.setTimeConstant(0.020f);
    mPitchBoostSmoother.reset(mParams.pitchBoostDb);

    mPitchWarpSmoother.setSampleRate(fs);
    mPitchWarpSmoother.setTimeConstant(0.050f);
    mPitchWarpSmoother.reset(initWarp);

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
    mPitchBoostSmoother.reset(mParams.pitchBoostDb);
    mPitchWarpSmoother.reset(mParams.pitchWarp ? 1.0f : 0.0f);
    mLastFilterWarp = -1.0f;
    mFlutterPhase1 = 0.0f;
    mFlutterPhase2 = 0.0f;

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
    mWasFrozen = false;
}

void Rb26ReverbEngine::setParameters(const Rb26Parameters& params) noexcept {
    const bool unfreezeEdge = (mWasFrozen && !params.freezeHold);
    mWasFrozen = params.freezeHold;

    if (unfreezeEdge) {
        std::fill(mPitchDelayBufferL.begin(), mPitchDelayBufferL.end(), 0.0f);
        std::fill(mPitchDelayBufferR.begin(), mPitchDelayBufferR.end(), 0.0f);
        mLastPitchFbL = 0.0f;
        mLastPitchFbR = 0.0f;
    }

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
                           params.tailBloomMs,
                           params.manifold);

    // Pass 0.0f internal feedback to pitch shifter: feedback loop is closed via FDN tank
    mPitchShifter.setParameters(params.shimmerSend,
                                params.dimmerSend,
                                params.shimmerInterval,
                                params.dimmerInterval,
                                params.pitchBlend,
                                0.0f);
    mPitchShifter.setWarpMode(params.pitchWarp, params.pitchWarp ? 1.0f : 0.0f);

    mInputTrimSmoother.setTarget(dbToGain(params.inputTrimDb));
    mPreDelaySmoother.setTarget(params.preDelayMs);
    mDryWetSmoother.setTarget(params.dryWetMix);
    mEarlyLateSmoother.setTarget(params.earlyLateMix);
    mStereoWidthSmoother.setTarget(params.stereoWidth);
    mOutputTrimSmoother.setTarget(dbToGain(params.outputTrimDb));
    mPitchFeedbackSmoother.setTarget(std::clamp(params.pitchFeedback, 0.0f, 0.95f));
    mPitchDelaySmoother.setTarget(std::clamp(params.pitchDelayMs, 20.0f, 500.0f));
    mPitchBlendSmoother.setTarget(std::clamp(params.pitchBlend, -1.0f, 1.0f));
    mPitchBoostSmoother.setTarget(std::clamp(params.pitchBoostDb, 0.0f, 18.0f));
    mPitchWarpSmoother.setTarget(params.pitchWarp ? 1.0f : 0.0f);

    if (params.shimmerSend <= 1.0e-4f && params.dimmerSend <= 1.0e-4f) {
        mPitchFeedbackSmoother.snapTo(0.0f);
        mPitchBlendSmoother.snapTo(std::clamp(params.pitchBlend, -1.0f, 1.0f));
        mPitchBoostSmoother.snapTo(std::clamp(params.pitchBoostDb, 0.0f, 18.0f));
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

    mActiveChannelCount = numChannels;

    const float* inL = inputChannels[0];
    const float* inR = (numChannels > 1 && inputChannels[1]) ? inputChannels[1] : inL;
    const float* auxL = (auxReverbChannels && auxReverbChannels[0]) ? auxReverbChannels[0] : nullptr;
    const float* auxR = (auxReverbChannels && numChannels > 1 && auxReverbChannels[1]) ? auxReverbChannels[1] : auxL;

    float* outL = outputChannels[0];
    float* outR = (numChannels > 1 && outputChannels[1]) ? outputChannels[1] : nullptr;

    // Fast check: does incoming block contain any signal?
    bool blockHasSignal = false;
    for (int ch = 0; ch < numChannels && ch < 12; ++ch) {
        if (!inputChannels[ch]) continue;
        for (int n = 0; n < numSamples; ++n) {
            if (std::abs(inputChannels[ch][n]) > 1.0e-7f) {
                blockHasSignal = true;
                break;
            }
        }
        if (blockHasSignal) break;
    }
    if (!blockHasSignal && auxReverbChannels) {
        for (int ch = 0; ch < 2; ++ch) {
            if (!auxReverbChannels[ch]) continue;
            for (int n = 0; n < numSamples; ++n) {
                if (std::abs(auxReverbChannels[ch][n]) > 1.0e-7f) {
                    blockHasSignal = true;
                    break;
                }
            }
            if (blockHasSignal) break;
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
            for (int ch = 0; ch < numChannels; ++ch) {
                if (outputChannels[ch]) {
                    std::fill(outputChannels[ch], outputChannels[ch] + numSamples, 0.0f);
                }
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
            mPitchBoostSmoother.snapTo(mPitchBoostSmoother.getTarget());
            mPitchWarpSmoother.snapTo(mPitchWarpSmoother.getTarget());

            // Decimate telemetry
            mTelemetryDecimator += numSamples;
            if (mTelemetryDecimator >= 512) {
                mTelemetryDecimator = 0;
                VisualizerFrame zeroFrame {};
                zeroFrame.correlation = 1.0f;
                zeroFrame.channelCount = mActiveChannelCount;
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
        mPitchBoostSmoother.snapTo(mPitchBoostSmoother.getTarget());
        mPitchWarpSmoother.snapTo(mPitchWarpSmoother.getTarget());
    }

    if (numChannels > 2) {
        for (int n = 0; n < numSamples; ++n) {
            const float warpAmount = mPitchWarpSmoother.next();

            // Feedback filter bandwidth dynamically widens with warpAmount:
            // HPF sweeps from 150 Hz down to 40 Hz, LPF sweeps from 6,000 Hz up to 16,000 Hz.
            if (((n & 31) == 0 || n == 0) && std::abs(warpAmount - mLastFilterWarp) > 0.002f) {
                mLastFilterWarp = warpAmount;
                const float hpfCutoff = (1.0f - warpAmount) * 150.0f + warpAmount * 40.0f;
                const float lpfCutoff = (1.0f - warpAmount) * 6000.0f + warpAmount * 16000.0f;
                mPitchFeedbackHpL.configure(Biquad::Type::Highpass, fs, hpfCutoff, 0.70710678f);
                mPitchFeedbackHpR.configure(Biquad::Type::Highpass, fs, hpfCutoff, 0.70710678f);
                mPitchFeedbackLpL.configure(Biquad::Type::Lowpass, fs, lpfCutoff, 0.70710678f);
                mPitchFeedbackLpR.configure(Biquad::Type::Lowpass, fs, lpfCutoff, 0.70710678f);
            }

            const float inTrim = mInputTrimSmoother.next();

            float inSample[12] = {0.0f};
            for (int ch = 0; ch < numChannels && ch < 12; ++ch) {
                inSample[ch] = inputChannels[ch] ? inputChannels[ch][n] : 0.0f;
            }

            // Input staging:
            // If numChannels == 4: Quad FL, FR
            // If numChannels >= 6: L/R from ch 0, 1; Center (ch 2) injected equally (-3 dB); LFE (ch 3) bypassed
            float xL = 0.0f;
            float xR = 0.0f;
            if (numChannels >= 6) {
                xL = inSample[0] + 0.70710678f * inSample[2];
                xR = inSample[1] + 0.70710678f * inSample[2];
            } else {
                xL = inSample[0];
                xR = inSample[1];
            }

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

            // 3. Early Reflections Matrix
            float earlyL = 0.0f, earlyR = 0.0f;
            mEarlyReflections.processSample(highInL, highInR, earlyL, earlyR);

            // 4. Decoupled Pitch Delay & Bidirectional Pitch Shifting Feedback
            const bool pitchActive = blockPitchActive;
            const float elMix = mEarlyLateSmoother.next();
            const float earlyGain = FastSinTable::cos(elMix * kHalfPi);
            const float lateGain  = FastSinTable::sin(elMix * kHalfPi);

            float injPitchL = 0.0f, injPitchR = 0.0f;
            float pitchAddL = 0.0f, pitchAddR = 0.0f;

            if (pitchActive) {
                const float curPitchDelayMs = mPitchDelaySmoother.next();
                const float pBlend = mPitchBlendSmoother.next();
                const float delayMult = 1.0f + 0.35f * std::max(0.0f, -pBlend);
                const float effDelayMs = curPitchDelayMs * delayMult;

                float flutterMsL = 0.0f;
                float flutterMsR = 0.0f;
                if (warpAmount > 0.01f) {
                    mFlutterPhase1 += (kTwoPi * 1.25f) / fs;
                    if (mFlutterPhase1 >= kTwoPi) mFlutterPhase1 -= kTwoPi;
                    mFlutterPhase2 += (kTwoPi * 2.85f) / fs;
                    if (mFlutterPhase2 >= kTwoPi) mFlutterPhase2 -= kTwoPi;

                    const float s1 = FastSinTable::sin(mFlutterPhase1);
                    const float s2 = FastSinTable::sin(mFlutterPhase2);
                    const float c1 = FastSinTable::cos(mFlutterPhase1);

                    const float sharedFlutter = 1.0f * s1 + 0.5f * s2;
                    flutterMsL = std::clamp(warpAmount * sharedFlutter, -1.5f, 1.5f);
                    flutterMsR = std::clamp(warpAmount * (sharedFlutter + 0.35f * c1), -1.5f, 1.5f);
                }

                const float delaySamplesL = std::clamp(((effDelayMs + flutterMsL) * 0.001f) * fs, 2.0f, static_cast<float>(kPitchDelayCapacity - 64));
                const float delaySamplesR = std::clamp(((effDelayMs * 1.07f + flutterMsR) * 0.001f) * fs, 2.0f, static_cast<float>(kPitchDelayCapacity - 64));

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
                const float effFbGain = (1.0f - warpAmount) * (fb * 0.30f) + warpAmount * (fb * 1.30f);

                const float filteredPitchL = mPitchFeedbackLpL.process(mPitchFeedbackHpL.process(delayedPitchL));
                const float filteredPitchR = mPitchFeedbackLpR.process(mPitchFeedbackHpR.process(delayedPitchR));

                const float rawFbL = filteredPitchL * effFbGain;
                const float rawFbR = filteredPitchR * effFbGain;

                const float asym = 0.08f * warpAmount;
                const float drivenL = (rawFbL > 0.0f) ? (rawFbL / (1.0f + asym * rawFbL)) : rawFbL;
                const float drivenR = (rawFbR > 0.0f) ? (rawFbR / (1.0f + asym * rawFbR)) : rawFbR;
                injPitchL = applySmoothBoundaryKnee(drivenL, 0.72f, 1.05f);
                injPitchR = applySmoothBoundaryKnee(drivenR, 0.72f, 1.05f);

                const float boostDb = mPitchBoostSmoother.next();
                const float boostGain = dbToGain(boostDb);
                const float rawPitchL = filteredPitchL * 0.50f * boostGain;
                const float rawPitchR = filteredPitchR * 0.50f * boostGain;
                pitchAddL = applySmoothBoundaryKnee(rawPitchL, 0.72f, 1.05f);
                pitchAddR = applySmoothBoundaryKnee(rawPitchR, 0.72f, 1.05f);
            }

            // Assemble multi-channel tank inputs
            float tankInputs[12] = {0.0f};
            tankInputs[0] = highInL;
            tankInputs[1] = highInR;
            if (numChannels == 4) {
                tankInputs[2] = inSample[2] * inTrim;
                tankInputs[3] = inSample[3] * inTrim;
            } else if (numChannels >= 6) {
                tankInputs[2] = inSample[2] * inTrim; // Center
                tankInputs[3] = 0.0f;                 // LFE bypassed
                tankInputs[4] = inSample[4] * inTrim; // Ls
                tankInputs[5] = inSample[5] * inTrim; // Rs
                if (numChannels >= 8) {
                    tankInputs[6] = inSample[6] * inTrim; // Rls
                    tankInputs[7] = inSample[7] * inTrim; // Rrs
                }
                if (numChannels >= 10) {
                    tankInputs[8] = inSample[8] * inTrim;
                    tankInputs[9] = inSample[9] * inTrim;
                }
                if (numChannels >= 12) {
                    tankInputs[10] = inSample[10] * inTrim;
                    tankInputs[11] = inSample[11] * inTrim;
                }
            }

            const float pitchFbArr[2] = { injPitchL, injPitchR };
            float lateMulti[12] = {0.0f};
            float* latePtrs[12];
            for (int ch = 0; ch < numChannels && ch < 12; ++ch) {
                latePtrs[ch] = &lateMulti[ch];
            }

            mFdnTank.processSampleMultiChannel(tankInputs,
                                               numChannels,
                                               pitchActive ? pitchFbArr : nullptr,
                                               latePtrs,
                                               numChannels);

            if (pitchActive) {
                mPitchShifter.setWarpMode(warpAmount > 0.001f, warpAmount);
                float shiftedL = 0.0f, shiftedR = 0.0f;
                mPitchShifter.processSample(lateMulti[0], lateMulti[1], shiftedL, shiftedR);

                mPitchDelayBufferL[mPitchDelayWriteIndex] = flushDenormal(shiftedL);
                mPitchDelayBufferR[mPitchDelayWriteIndex] = flushDenormal(shiftedR);
                mPitchDelayWriteIndex = (mPitchDelayWriteIndex + 1) & kPitchDelayMask;

                mLastPitchFbL = shiftedL;
                mLastPitchFbR = shiftedR;
            } else {
                mLastPitchFbL = 0.0f;
                mLastPitchFbR = 0.0f;
            }

            // 5. Early / Late combination across all channels
            float wetMulti[12] = {0.0f};
            wetMulti[0] = earlyGain * earlyL + lateGain * (lateMulti[0] + pitchAddL) + lowReverbL;
            wetMulti[1] = earlyGain * earlyR + lateGain * (lateMulti[1] + pitchAddR) + lowReverbR;

            mMasterSubMono.process(wetMulti[0], wetMulti[1], wetMulti[0], wetMulti[1]);

            const float width = mStereoWidthSmoother.next();
            const float wetMid  = 0.5f * (wetMulti[0] + wetMulti[1]);
            const float wetSide = 0.5f * (wetMulti[0] - wetMulti[1]);
            wetMulti[0] = wetMid + width * wetSide;
            wetMulti[1] = wetMid - width * wetSide;

            for (int ch = 2; ch < numChannels && ch < 12; ++ch) {
                if (ch == 3 && numChannels >= 6) {
                    wetMulti[ch] = 0.0f; // LFE has 0 wet reverb
                } else {
                    float earlyCh = 0.0f;
                    if (numChannels == 4) {
                        earlyCh = (ch == 2) ? (earlyL * 0.70710678f) : (earlyR * 0.70710678f);
                    } else if (numChannels >= 6) {
                        if (ch == 2) {
                            earlyCh = 0.50f * (earlyL + earlyR);
                        } else if (ch % 2 == 0) {
                            earlyCh = earlyL * 0.70710678f;
                        } else {
                            earlyCh = earlyR * 0.70710678f;
                        }
                    }
                    wetMulti[ch] = earlyGain * earlyCh + lateGain * lateMulti[ch];
                }
            }

            // 6. Equal-power Dry/Wet crossfade, output trim & C1 Hermite limiter per channel
            const float dwMix = mDryWetSmoother.next();
            const float dryGain = FastSinTable::cos(dwMix * kHalfPi);
            const float wetGain = FastSinTable::sin(dwMix * kHalfPi);
            const float trim = mOutputTrimSmoother.next();

            for (int ch = 0; ch < numChannels && ch < 12; ++ch) {
                if (!outputChannels[ch]) continue;
                float sampleOut = 0.0f;
                if (ch == 3 && numChannels >= 6) {
                    // LFE passed clean/dry
                    sampleOut = inSample[3];
                } else {
                    sampleOut = dryGain * (inSample[ch] * inTrim) + wetGain * wetMulti[ch];
                    sampleOut *= trim;
                    if (mParams.limiterEnable) {
                        sampleOut = softLimit(sampleOut);
                    }
                }
                outputChannels[ch][n] = flushDenormal(sampleOut);
            }

            // Telemetry & silence tracking
            float outPeak = 0.0f;
            for (int ch = 0; ch < numChannels && ch < 12; ++ch) {
                if (outputChannels[ch]) {
                    outPeak = std::max(outPeak, std::abs(outputChannels[ch][n]));
                }
            }
            const float sampleOutL = outputChannels[0] ? outputChannels[0][n] : 0.0f;
            const float sampleOutR = (numChannels > 1 && outputChannels[1]) ? outputChannels[1][n] : sampleOutL;
            mInputRmsSumL += xL * xL;
            mInputRmsSumR += xR * xR;
            mOutputRmsSumL += sampleOutL * sampleOutL;
            mOutputRmsSumR += sampleOutR * sampleOutR;
            mCorrelationSum += sampleOutL * sampleOutR;

            mDecayPeakFollower = std::max(outPeak, mDecayPeakFollower * 0.999f);
            mTailEnergyFollower = flushDenormal(0.999f * mTailEnergyFollower + 0.001f * outPeak);

            float inPeak = 0.0f;
            for (int ch = 0; ch < numChannels && ch < 12; ++ch) {
                inPeak = std::max(inPeak, std::abs(inSample[ch]));
            }
            inPeak += (auxL ? std::abs(auxL[n]) : 0.0f) + (auxR ? std::abs(auxR[n]) : 0.0f);

            if (inPeak < 1.0e-7f && outPeak < 1.0e-7f && mTailEnergyFollower < 1.0e-7f && !mParams.freezeHold) {
                mSilentSamplesCount++;
            } else {
                mSilentSamplesCount = 0;
            }

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
                frame.midEnergy = std::sqrt(0.5f * (lateMulti[0] * lateMulti[0] + lateMulti[1] * lateMulti[1]));
                frame.highEnergy = std::sqrt(0.5f * (earlyL * earlyL + earlyR * earlyR));
                frame.decayEnvelope = mDecayPeakFollower;
                frame.channelCount = mActiveChannelCount;

                pushVisualizerFrame(frame);

                mInputRmsSumL = 0.0f;
                mInputRmsSumR = 0.0f;
                mOutputRmsSumL = 0.0f;
                mOutputRmsSumR = 0.0f;
                mCorrelationSum = 0.0f;
            }
        }
    } else {
        const float monoGain = (numChannels == 1) ? 0.70710678f : 1.0f;
        for (int n = 0; n < numSamples; ++n) {
            const float warpAmount = mPitchWarpSmoother.next();

            // Feedback filter bandwidth dynamically widens with warpAmount:
            // HPF sweeps from 150 Hz down to 40 Hz, LPF sweeps from 6,000 Hz up to 16,000 Hz.
            if (((n & 31) == 0 || n == 0) && std::abs(warpAmount - mLastFilterWarp) > 0.002f) {
                mLastFilterWarp = warpAmount;
                const float hpfCutoff = (1.0f - warpAmount) * 150.0f + warpAmount * 40.0f;
                const float lpfCutoff = (1.0f - warpAmount) * 6000.0f + warpAmount * 16000.0f;
                mPitchFeedbackHpL.configure(Biquad::Type::Highpass, fs, hpfCutoff, 0.70710678f);
                mPitchFeedbackHpR.configure(Biquad::Type::Highpass, fs, hpfCutoff, 0.70710678f);
                mPitchFeedbackLpL.configure(Biquad::Type::Lowpass, fs, lpfCutoff, 0.70710678f);
                mPitchFeedbackLpR.configure(Biquad::Type::Lowpass, fs, lpfCutoff, 0.70710678f);
            }

            const float inTrim = mInputTrimSmoother.next();
            const float xL = inL[n] * monoGain;
            const float xR = inR[n] * monoGain;
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

            // Micro-flutter on delayedPitch in WARP mode:
            // Add subtle tape capstan wow & flutter (+/- 1.5 ms) when warpAmount > 0.01f
            float flutterMsL = 0.0f;
            float flutterMsR = 0.0f;
            if (warpAmount > 0.01f) {
                mFlutterPhase1 += (kTwoPi * 1.25f) / fs;
                if (mFlutterPhase1 >= kTwoPi) mFlutterPhase1 -= kTwoPi;
                mFlutterPhase2 += (kTwoPi * 2.85f) / fs;
                if (mFlutterPhase2 >= kTwoPi) mFlutterPhase2 -= kTwoPi;

                const float s1 = FastSinTable::sin(mFlutterPhase1);
                const float s2 = FastSinTable::sin(mFlutterPhase2);
                const float c1 = FastSinTable::cos(mFlutterPhase1);

                const float sharedFlutter = 1.0f * s1 + 0.5f * s2;
                flutterMsL = std::clamp(warpAmount * sharedFlutter, -1.5f, 1.5f);
                flutterMsR = std::clamp(warpAmount * (sharedFlutter + 0.35f * c1), -1.5f, 1.5f);
            }

            const float delaySamplesL = std::clamp(((effDelayMs + flutterMsL) * 0.001f) * fs, 2.0f, static_cast<float>(kPitchDelayCapacity - 64));
            const float delaySamplesR = std::clamp(((effDelayMs * 1.07f + flutterMsR) * 0.001f) * fs, 2.0f, static_cast<float>(kPitchDelayCapacity - 64));

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
            // Feedback gain: safePitchFb = fb * 0.30f when calibrated, scaling smoothly up to fb * 1.30f in WARP
            const float effFbGain = (1.0f - warpAmount) * (fb * 0.30f) + warpAmount * (fb * 1.30f);

            // Feedback loop bandpass filtering with DC blocking HPF
            const float filteredPitchL = mPitchFeedbackLpL.process(mPitchFeedbackHpL.process(delayedPitchL));
            const float filteredPitchR = mPitchFeedbackLpR.process(mPitchFeedbackHpR.process(delayedPitchR));

            // Scaled feedback audio
            const float rawFbL = filteredPitchL * effFbGain;
            const float rawFbR = filteredPitchR * effFbGain;

            // Asymmetrical / soft-knee saturation: feedback audio passes through an internal soft saturator
            // capping energy at -1 dBFS (<= 1.05) to guarantee strict mathematical boundedness and safety even at 1.30x feedback.
            // DC blocking: HPF in loop guarantees complete DC suppression.
            const float asym = 0.08f * warpAmount;
            const float drivenL = (rawFbL > 0.0f) ? (rawFbL / (1.0f + asym * rawFbL)) : rawFbL;
            const float drivenR = (rawFbR > 0.0f) ? (rawFbR / (1.0f + asym * rawFbR)) : rawFbR;
            const float injPitchL = applySmoothBoundaryKnee(drivenL, 0.72f, 1.05f);
            const float injPitchR = applySmoothBoundaryKnee(drivenR, 0.72f, 1.05f);

            mFdnTank.processSample(highInL, highInR, injPitchL, injPitchR, lateL, lateR);

            // Feed late reverberation into PitchShifter to calculate next shifted sample
            // Propagate dynamic warp amount into PitchShifter
            mPitchShifter.setWarpMode(warpAmount > 0.001f, warpAmount);
            float shiftedL = 0.0f, shiftedR = 0.0f;
            mPitchShifter.processSample(lateL, lateR, shiftedL, shiftedR);

            // Store shifted sample into decoupled delay buffer for distinct temporal spacing
            mPitchDelayBufferL[mPitchDelayWriteIndex] = flushDenormal(shiftedL);
            mPitchDelayBufferR[mPitchDelayWriteIndex] = flushDenormal(shiftedR);
            mPitchDelayWriteIndex = (mPitchDelayWriteIndex + 1) & kPitchDelayMask;

            mLastPitchFbL = shiftedL;
            mLastPitchFbR = shiftedR;

            const float boostDb = mPitchBoostSmoother.next();
            const float boostGain = dbToGain(boostDb);

            // 5. Early / Late Mix (Equal-power trigonometric balance with Hermite bounded booster saturation)
            // Filtered through feedback lowpass/highpass and smoothly scaled to eliminate slapback transient clicks
            const float rawPitchL = filteredPitchL * 0.50f * boostGain;
            const float rawPitchR = filteredPitchR * 0.50f * boostGain;
            const float pitchAddL = applySmoothBoundaryKnee(rawPitchL, 0.72f, 1.05f);
            const float pitchAddR = applySmoothBoundaryKnee(rawPitchR, 0.72f, 1.05f);
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
            frame.channelCount = mActiveChannelCount;

            pushVisualizerFrame(frame);

            mInputRmsSumL = 0.0f;
            mInputRmsSumR = 0.0f;
            mOutputRmsSumL = 0.0f;
            mOutputRmsSumR = 0.0f;
            mCorrelationSum = 0.0f;
        }
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
        p.params.manifold = ManifoldType::PoincareHyperbolic;
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
        p.params.manifold = ManifoldType::WhisperingGallery;
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
        p.params.manifold = ManifoldType::AnharmonicPlate;
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
        p.params.manifold = ManifoldType::PoincareHyperbolic;
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
        p.params.manifold = ManifoldType::AnharmonicPlate;
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
        p.params.manifold = ManifoldType::StockhausenKlangdom;
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
        p.params.manifold = ManifoldType::StockhausenKlangdom;
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
        p.params.manifold = ManifoldType::WhisperingGallery;
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
        p.params.decayRt60Sec = 4.5f;
        p.params.roomSize = 1.20f;
        p.params.highDampingHz = 8000.0f;
        p.params.freezeHold = true;
        p.params.shimmerSend = 0.45f;
        p.params.dimmerSend = 0.30f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.20f;
        p.params.pitchFeedback = 0.25f;
        p.params.tailModRateHz = 0.65f;
        p.params.tailModDepthMs = 2.25f;
        p.params.tailBloomMs = 85.0f;
        p.params.stereoWidth = 1.30f;
        p.params.earlyLateMix = 0.70f;
        p.params.dryWetMix = 0.55f;
        p.params.limiterEnable = true;
        p.params.manifold = ManifoldType::PoincareHyperbolic;
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
        p.params.manifold = ManifoldType::PoincareHyperbolic;
        p.params.pitchWarp = false;
        presets.push_back(p);
    }

    // 11. WARP_CELESTIAL_OVERDRIVE
    {
        PresetDefinition p;
        p.id = "WARP_CELESTIAL_OVERDRIVE";
        p.name = "WARP CELESTIAL OVERDRIVE";
        p.category = "Warp / Unbounded";
        p.description = "Unbounded feedback shimmer cascading beyond unity gain into warm saturated tape harmonic overdrive.";
        p.params.preDelayMs = 28.0f;
        p.params.diffusionDensity = 0.85f;
        p.params.outputTrimDb = -2.0f;
        p.params.lowCrossoverHz = 160.0f;
        p.params.bassRt60Mult = 0.90f;
        p.params.punchDucking = 0.60f;
        p.params.subMonoHz = 120.0f;
        p.params.decayRt60Sec = 16.0f;
        p.params.roomSize = 1.80f;
        p.params.highDampingHz = 12000.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.85f;
        p.params.dimmerSend = 0.20f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = 0.75f;
        p.params.pitchFeedback = 0.85f;
        p.params.pitchDelayMs = 180.0f;
        p.params.pitchBoostDb = 4.5f;
        p.params.pitchWarp = true;
        p.params.tailModRateHz = 0.45f;
        p.params.tailModDepthMs = 3.20f;
        p.params.tailBloomMs = 120.0f;
        p.params.stereoWidth = 1.45f;
        p.params.earlyLateMix = 0.70f;
        p.params.dryWetMix = 0.55f;
        p.params.limiterEnable = true;
        p.params.manifold = ManifoldType::PoincareHyperbolic;
        presets.push_back(p);
    }

    // 12. HAUNTED_TAPE_BEATING
    {
        PresetDefinition p;
        p.id = "HAUNTED_TAPE_BEATING";
        p.name = "HAUNTED TAPE BEATING";
        p.category = "Warp / Unbounded";
        p.description = "Deep micro-fluttered pitch echoes with rich tape wow beating and dense analog chorusing.";
        p.params.preDelayMs = 45.0f;
        p.params.diffusionDensity = 0.78f;
        p.params.outputTrimDb = -1.5f;
        p.params.lowCrossoverHz = 200.0f;
        p.params.bassRt60Mult = 1.10f;
        p.params.punchDucking = 0.50f;
        p.params.subMonoHz = 140.0f;
        p.params.decayRt60Sec = 12.0f;
        p.params.roomSize = 1.40f;
        p.params.highDampingHz = 7500.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.65f;
        p.params.dimmerSend = 0.55f;
        p.params.shimmerInterval = 7;
        p.params.dimmerInterval = -7;
        p.params.pitchBlend = 0.10f;
        p.params.pitchFeedback = 0.75f;
        p.params.pitchDelayMs = 240.0f;
        p.params.pitchBoostDb = 3.0f;
        p.params.pitchWarp = true;
        p.params.tailModRateHz = 1.20f;
        p.params.tailModDepthMs = 4.0f;
        p.params.tailBloomMs = 150.0f;
        p.params.stereoWidth = 1.60f;
        p.params.earlyLateMix = 0.65f;
        p.params.dryWetMix = 0.50f;
        p.params.limiterEnable = true;
        p.params.manifold = ManifoldType::WhisperingGallery;
        presets.push_back(p);
    }

    // 13. SUB_TRITONE_ABYSS
    {
        PresetDefinition p;
        p.id = "SUB_TRITONE_ABYSS";
        p.name = "SUB TRITONE ABYSS";
        p.category = "Warp / Unbounded";
        p.description = "Ominous descending sub-octave dimmer feedback opening into dark subterranean resonating chasms.";
        p.params.preDelayMs = 30.0f;
        p.params.diffusionDensity = 0.82f;
        p.params.outputTrimDb = -2.5f;
        p.params.lowCrossoverHz = 220.0f;
        p.params.bassRt60Mult = 1.40f;
        p.params.punchDucking = 0.70f;
        p.params.subMonoHz = 160.0f;
        p.params.decayRt60Sec = 18.0f;
        p.params.roomSize = 2.20f;
        p.params.highDampingHz = 5500.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.15f;
        p.params.dimmerSend = 0.90f;
        p.params.shimmerInterval = 7;
        p.params.dimmerInterval = -12;
        p.params.pitchBlend = -0.85f;
        p.params.pitchFeedback = 0.80f;
        p.params.pitchDelayMs = 320.0f;
        p.params.pitchBoostDb = 6.0f;
        p.params.pitchWarp = true;
        p.params.tailModRateHz = 0.35f;
        p.params.tailModDepthMs = 2.80f;
        p.params.tailBloomMs = 200.0f;
        p.params.stereoWidth = 1.30f;
        p.params.earlyLateMix = 0.75f;
        p.params.dryWetMix = 0.60f;
        p.params.limiterEnable = true;
        p.params.manifold = ManifoldType::PoincareHyperbolic;
        presets.push_back(p);
    }

    // 14. METALLIC_COMB_DISINTEGRATION
    {
        PresetDefinition p;
        p.id = "METALLIC_COMB_DISINTEGRATION";
        p.name = "METALLIC COMB DISINTEGRATION";
        p.category = "Warp / Unbounded";
        p.description = "Short-grain metallic dispersion with aggressive feedback saturation and disintegrating comb reflections.";
        p.params.preDelayMs = 12.0f;
        p.params.diffusionDensity = 0.92f;
        p.params.outputTrimDb = -3.0f;
        p.params.lowCrossoverHz = 150.0f;
        p.params.bassRt60Mult = 0.75f;
        p.params.punchDucking = 0.55f;
        p.params.subMonoHz = 110.0f;
        p.params.decayRt60Sec = 8.5f;
        p.params.roomSize = 0.65f;
        p.params.highDampingHz = 15000.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.80f;
        p.params.dimmerSend = 0.40f;
        p.params.shimmerInterval = 24;
        p.params.dimmerInterval = -2;
        p.params.pitchBlend = 0.60f;
        p.params.pitchFeedback = 0.90f;
        p.params.pitchDelayMs = 65.0f;
        p.params.pitchBoostDb = 8.0f;
        p.params.pitchWarp = true;
        p.params.tailModRateHz = 2.40f;
        p.params.tailModDepthMs = 4.50f;
        p.params.tailBloomMs = 50.0f;
        p.params.stereoWidth = 1.70f;
        p.params.earlyLateMix = 0.80f;
        p.params.dryWetMix = 0.65f;
        p.params.limiterEnable = true;
        p.params.manifold = ManifoldType::AnharmonicPlate;
        presets.push_back(p);
    }

    // 15. INFINITE_WARP_SINGULARITY
    {
        PresetDefinition p;
        p.id = "INFINITE_WARP_SINGULARITY";
        p.name = "INFINITE WARP SINGULARITY";
        p.category = "Warp / Unbounded";
        p.description = "Supercritical infinite self-oscillating shimmer sphere contained by C1 Hermite boundary saturation.";
        p.params.preDelayMs = 60.0f;
        p.params.diffusionDensity = 0.95f;
        p.params.outputTrimDb = -3.5f;
        p.params.lowCrossoverHz = 170.0f;
        p.params.bassRt60Mult = 1.0f;
        p.params.punchDucking = 0.65f;
        p.params.subMonoHz = 130.0f;
        p.params.decayRt60Sec = 25.0f;
        p.params.roomSize = 2.50f;
        p.params.highDampingHz = 16000.0f;
        p.params.freezeHold = false;
        p.params.shimmerSend = 0.95f;
        p.params.dimmerSend = 0.70f;
        p.params.shimmerInterval = 12;
        p.params.dimmerInterval = -7;
        p.params.pitchBlend = 0.40f;
        p.params.pitchFeedback = 0.95f;
        p.params.pitchDelayMs = 280.0f;
        p.params.pitchBoostDb = 10.0f;
        p.params.pitchWarp = true;
        p.params.tailModRateHz = 0.80f;
        p.params.tailModDepthMs = 3.50f;
        p.params.tailBloomMs = 180.0f;
        p.params.stereoWidth = 1.80f;
        p.params.earlyLateMix = 0.85f;
        p.params.dryWetMix = 0.70f;
        p.params.limiterEnable = true;
        p.params.manifold = ManifoldType::StockhausenKlangdom;
        presets.push_back(p);
    }

    return presets;
}

} // namespace rb26
