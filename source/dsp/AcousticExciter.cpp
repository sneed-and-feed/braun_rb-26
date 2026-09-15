#include "AcousticExciter.h"

#include <cmath>
#include <algorithm>
#include <cstring>

namespace rb26 {

// ============================================================================
// ChimeVoice Implementation
// ============================================================================

void ChimeVoice::prepare(double sampleRate) noexcept {
    mSampleRate = (sampleRate > 1000.0) ? static_cast<float>(sampleRate) : 48000.0f;
    reset();
}

void ChimeVoice::reset() noexcept {
    mActive = false;
    mAge = 0;
    mMidiNote = 60.0f;
    mVelocity = 0.70f;

    for (size_t i = 0; i < kNumChimeModes; ++i) {
        mPhase[i] = 0.0f;
        mPhaseInc[i] = 0.0f;
        mEnv[i] = 0.0f;
        mDecayCoeff[i] = 0.999f;
        mAmp[i] = 0.0f;
    }

    mNoiseEnv = 0.0f;
    mNoiseDecay = 0.99f;
    mPrng.setSeed(0x12345678ULL);
}

void ChimeVoice::trigger(float midiNote, float velocity, float durationSec) noexcept {
    mMidiNote = midiNote;
    mVelocity = std::clamp(velocity, 0.05f, 1.0f);
    mAge = 0;
    mActive = true;

    const float f0 = midiToFrequency(midiNote);
    const float twoPiOverFs = kTwoPi / mSampleRate;
    const float maxFreq = mSampleRate * 0.45f;
    const float baseDecay = std::max(0.5f, durationSec) * (1.0f + 0.5f * mVelocity);

    for (size_t i = 0; i < kNumChimeModes; ++i) {
        const float freq = std::min(f0 * kChimeModeRatios[i], maxFreq);
        mPhaseInc[i] = freq * twoPiOverFs;
        mPhase[i] = 0.0f;

        const float t60 = baseDecay / kChimeModeDecayFactors[i];
        mDecayCoeff[i] = std::exp(-6.907755f / (t60 * mSampleRate));

        // Harold Budd felt piano amplitude weighting with calibrated studio headroom (-18 dBFS to -12 dBFS)
        static constexpr float kVoiceHeadroom = 0.22f;
        static constexpr std::array<float, kNumChimeModes> baseModeAmps = { 0.52f, 0.22f, 0.08f, 0.03f };
        const float velScale = (i == 0) ? 1.0f : std::pow(mVelocity, 0.5f + 0.3f * static_cast<float>(i));
        mAmp[i] = baseModeAmps[i] * velScale * mVelocity * kVoiceHeadroom;
        mEnv[i] = 1.0f;
    }

    // Soft felt hammer noise transient burst (20 ms contact decay)
    mNoiseEnv = mVelocity * 0.20f * 0.22f;
    mNoiseDecay = std::exp(-6.907755f / (0.020f * mSampleRate));
}

void ChimeVoice::release() noexcept {
    // Fast exponential damping on note release (150 ms release time)
    const float fastReleaseCoeff = std::exp(-6.907755f / (0.150f * mSampleRate));
    for (size_t i = 0; i < kNumChimeModes; ++i) {
        mDecayCoeff[i] = std::min(mDecayCoeff[i], fastReleaseCoeff);
    }
}

void ChimeVoice::processSample(float& outL, float& outR) noexcept {
    if (!mActive) return;

    mAge++;

    // Realistic spatial dispersion across modal tines:
    // Mode 0: Center (0.7071, 0.7071)
    // Mode 1: Slanted Left (0.8000, 0.6000)
    // Mode 2: Slanted Right (0.6000, 0.8000)
    // Mode 3: Outer Left (0.8500, 0.5200)
    static constexpr std::array<float, kNumChimeModes> panL = { 0.7071f, 0.8000f, 0.6000f, 0.8500f };
    static constexpr std::array<float, kNumChimeModes> panR = { 0.7071f, 0.6000f, 0.8000f, 0.5200f };

    float voiceL = 0.0f;
    float voiceR = 0.0f;

    for (size_t i = 0; i < kNumChimeModes; ++i) {
        const float modeSig = std::sin(mPhase[i]) * (mAmp[i] * mEnv[i]);
        voiceL += modeSig * panL[i];
        voiceR += modeSig * panR[i];

        mPhase[i] += mPhaseInc[i];
        if (mPhase[i] >= kTwoPi) mPhase[i] -= kTwoPi;

        mEnv[i] *= mDecayCoeff[i];
        mEnv[i] = flushDenormal(mEnv[i]);
    }

    // Attack noise transient injection
    if (mNoiseEnv > 1.0e-5f) {
        const float white = (mPrng.nextFloat01() * 2.0f) - 1.0f;
        const float noiseSig = white * mNoiseEnv;
        voiceL += noiseSig * 0.7071f;
        voiceR += noiseSig * 0.7071f;

        mNoiseEnv *= mNoiseDecay;
        mNoiseEnv = flushDenormal(mNoiseEnv);
    }

    // Voice termination check (fundamental mode and noise have decayed into silence)
    if (mEnv[0] < 1.0e-4f && mNoiseEnv < 1.0e-5f) {
        mActive = false;
    }

    outL += flushDenormal(voiceL);
    outR += flushDenormal(voiceR);
}

// ============================================================================
// LaboratoryImpulseGenerator Implementation
// ============================================================================

void LaboratoryImpulseGenerator::prepare(double sampleRate) noexcept {
    mSampleRate = (sampleRate > 1000.0) ? static_cast<float>(sampleRate) : 48000.0f;
    reset();
}

void LaboratoryImpulseGenerator::reset() noexcept {
    mDiracRemaining = 0;
    mDiracPolarity = 1.0f;

    mBurstRemaining = 0;
    mBurstTotal = 0;
    mB0 = mB1 = mB2 = 0.0f;
    mPinkPrng.setSeed(0x87654321ULL);

    mHammerRemaining = 0;
    mHammerTotal = 0;
    mHammerPhase = 0.0f;
    mHammerFreqInc = 0.0f;
    mHammerEnv = 0.0f;
    mHammerDecay = 0.0f;
}

void LaboratoryImpulseGenerator::triggerDirac(float polarity) noexcept {
    mDiracPolarity = (polarity >= 0.0f) ? 1.0f : -1.0f;
    mDiracRemaining = 1; // Exactly one sample delta pulse
}

void LaboratoryImpulseGenerator::triggerPinkBurst(float durationMs) noexcept {
    const float durMs = std::clamp(durationMs, 10.0f, 1000.0f);
    mBurstTotal = static_cast<int>((durMs * 0.001f) * mSampleRate);
    mBurstTotal = std::max(64, mBurstTotal);
    mBurstRemaining = mBurstTotal;
    mB0 = mB1 = mB2 = 0.0f;
}

void LaboratoryImpulseGenerator::triggerHammerThud(float hardness) noexcept {
    const float h = std::clamp(hardness, 0.0f, 1.0f);
    // Soundboard fundamental resonance centered at 78 Hz
    const float fBody = 78.0f * (0.90f + 0.20f * h);
    mHammerPhase = 0.0f;
    mHammerFreqInc = fBody * (kTwoPi / mSampleRate);
    mHammerEnv = 1.0f;
    mHammerDecay = std::exp(-6.907755f / (0.040f * mSampleRate)); // 40 ms decay

    const float durMs = 45.0f + (1.0f - h) * 35.0f;
    mHammerTotal = static_cast<int>((durMs * 0.001f) * mSampleRate);
    mHammerTotal = std::max(64, mHammerTotal);
    mHammerRemaining = mHammerTotal;
}

void LaboratoryImpulseGenerator::processSample(float& outL, float& outR) noexcept {
    // 1. Single-sample Dirac delta pulse
    if (mDiracRemaining > 0) {
        const float val = mDiracPolarity * 0.95f;
        outL += val;
        outR += val;
        mDiracRemaining--;
    }

    // 2. Broadband Paul Kellet 3-pole pink noise burst + Tukey window
    if (mBurstRemaining > 0) {
        const float white = (mPinkPrng.nextFloat01() * 2.0f) - 1.0f;
        mB0 = 0.99886f * mB0 + white * 0.0555179f;
        mB1 = 0.99332f * mB1 + white * 0.0750759f;
        mB2 = 0.96900f * mB2 + white * 0.1538520f;
        const float pink = (mB0 + mB1 + mB2 + white * 0.5362f) * 0.25f;

        const int elapsed = mBurstTotal - mBurstRemaining;
        const int fadeSamples = std::max(1, std::min(static_cast<int>(0.005f * mSampleRate), mBurstTotal / 4));
        float win = 1.0f;
        if (elapsed < fadeSamples) {
            win = 0.5f * (1.0f - std::cos(kPi * static_cast<float>(elapsed) / static_cast<float>(fadeSamples)));
        } else if (mBurstRemaining < fadeSamples) {
            win = 0.5f * (1.0f + std::cos(kPi * static_cast<float>(fadeSamples - mBurstRemaining) / static_cast<float>(fadeSamples)));
        }

        const float burstVal = pink * win * 0.80f;
        outL += burstVal;
        outR += burstVal;

        mB0 = flushDenormal(mB0);
        mB1 = flushDenormal(mB1);
        mB2 = flushDenormal(mB2);
        mBurstRemaining--;
    }

    // 3. Acoustic soundboard hammer thud (78 Hz body resonance + 2.5 ms contact click)
    if (mHammerRemaining > 0) {
        const float bodySine = std::sin(mHammerPhase) * mHammerEnv;
        mHammerPhase += mHammerFreqInc;
        if (mHammerPhase >= kTwoPi) mHammerPhase -= kTwoPi;
        mHammerEnv *= mHammerDecay;
        mHammerEnv = flushDenormal(mHammerEnv);

        const int elapsed = mHammerTotal - mHammerRemaining;
        const int contactSamples = std::max(1, static_cast<int>(0.0025f * mSampleRate));
        float click = 0.0f;
        if (elapsed < contactSamples) {
            const float tNorm = static_cast<float>(elapsed) / static_cast<float>(contactSamples);
            click = std::pow(1.0f - tNorm, 3.0f) * 0.60f;
        }

        const float thudVal = (bodySine * 0.65f + click) * 0.85f;
        outL += thudVal;
        outR += thudVal;
        mHammerRemaining--;
    }
}

// ============================================================================
// PoissonClock Implementation
// ============================================================================

void PoissonClock::prepare(double sampleRate) noexcept {
    mSampleRate = (sampleRate > 1000.0) ? static_cast<float>(sampleRate) : 48000.0f;
    reset();
}

void PoissonClock::reset() noexcept {
    mSamplesUntilNext = static_cast<int>(mSampleRate * 1.0f); // 1.0s initial graceful prelude
    mLastMidiNote = 60.0f;
    mPrng.setSeed(0x54321987ULL);
}

bool PoissonClock::tick(float epm, float humanize, float& outMidi, float& outVelocity,
                        int rootPitchClass, int scaleIndex,
                        const int* customIntervals, int numCustomIntervals) noexcept {
    if (--mSamplesUntilNext > 0) {
        return false;
    }

    // 1. Continuous stationary Poisson inter-arrival interval: tau = -ln(1 - U) / lambda
    const float lambda = std::max(0.1f, epm) * (1.0f / 60.0f); // events per second
    const float u = std::clamp(mPrng.nextFloat01(), 1.0e-7f, 1.0f - 1.0e-7f);
    const float rawInterval = -std::log(1.0f - u) / lambda;
    const float meanInterval = 1.0f / lambda;
    const float rubato = meanInterval + (rawInterval - meanInterval) * (0.35f + std::clamp(humanize, 0.0f, 1.0f) * 0.65f);
    const float intervalSec = std::clamp(rubato, 0.25f, 12.0f);
    mSamplesUntilNext = static_cast<int>(intervalSec * mSampleRate);

    // 2. Harold Budd Markov step/leap pitch weighting
    const float rChoice = mPrng.nextFloat01();
    float targetMidi = mLastMidiNote;
    if (rChoice < 0.70f) {
        // 70% probability of small melodic step
        static constexpr std::array<int, 8> stepChoices = { -5, -4, -2, -1, 1, 2, 4, 5 };
        const uint32_t stepIdx = mPrng.nextU32() % 8;
        targetMidi += static_cast<float>(stepChoices[stepIdx]);
    } else {
        // 30% probability of expressive leap
        const float rLeap = mPrng.nextFloat01();
        if (rLeap < 0.20f) {
            // Low resonant bass anchor (48 - 55: C3 - G3)
            targetMidi = 48.0f + static_cast<float>(mPrng.nextU32() % 8);
        } else if (rLeap < 0.85f) {
            // Sweet felt piano tenor/alto register (55 - 72: G3 - C5)
            targetMidi = 55.0f + static_cast<float>(mPrng.nextU32() % 18);
        } else {
            // High acoustic chime register (72 - 84: C5 - C6)
            targetMidi = 72.0f + static_cast<float>(mPrng.nextU32() % 13);
        }
    }

    targetMidi = std::clamp(targetMidi, 48.0f, 84.0f);

    // 3. Scale degree quantization
    if (customIntervals != nullptr && numCustomIntervals > 0) {
        outMidi = quantizeMidiToScale(targetMidi, rootPitchClass, customIntervals, numCustomIntervals);
    } else {
        outMidi = quantizeMidiToScaleIndex(targetMidi, rootPitchClass, scaleIndex);
    }
    mLastMidiNote = outMidi;

    // 4. Irwin-Hall Gaussian velocity approximation
    const float uNorm = mPrng.nextGaussian01();
    const float varianceScale = 0.20f + (std::clamp(humanize, 0.0f, 1.0f) * 0.80f);
    const float vel = 0.55f + (uNorm - 0.5f) * 0.54f * varianceScale;
    outVelocity = std::clamp(vel, 0.25f, 0.85f);

    return true;
}

// ============================================================================
// AcousticExciterEngine Implementation
// ============================================================================

AcousticExciterEngine::AcousticExciterEngine() noexcept {
    prepare(48000.0);
}

void AcousticExciterEngine::prepare(double sampleRate) noexcept {
    mSampleRate = (sampleRate > 1000.0) ? sampleRate : 48000.0;

    for (auto& voice : mVoices) {
        voice.prepare(mSampleRate);
    }

    mLabGen.prepare(mSampleRate);
    mPoissonClock.prepare(mSampleRate);

    for (auto& sn : mScheduledNotes) {
        sn.active = false;
        sn.delaySamples = 0;
    }

    mLevelSmoother.setSampleRate(static_cast<float>(mSampleRate));
    mLevelSmoother.setTimeConstant(0.020f); // 20 ms smoothing
    mLevelSmoother.snapTo(dbToGain(mParams.levelDb));
}

void AcousticExciterEngine::reset() noexcept {
    for (auto& voice : mVoices) {
        voice.reset();
    }

    mLabGen.reset();
    mPoissonClock.reset();

    for (auto& sn : mScheduledNotes) {
        sn.active = false;
        sn.delaySamples = 0;
    }

    mFifoWritePos.store(0, std::memory_order_relaxed);
    mFifoReadPos.store(0, std::memory_order_relaxed);
    mLevelSmoother.snapTo(dbToGain(mParams.levelDb));
}

void AcousticExciterEngine::setParameters(const AcousticExciterParameters& params) noexcept {
    mParams = params;
    setLevelDb(params.levelDb);
}

void AcousticExciterEngine::setLevelDb(float db) noexcept {
    mParams.levelDb = std::clamp(db, -48.0f, 6.0f);
    mLevelSmoother.setTarget(dbToGain(mParams.levelDb));
}

bool AcousticExciterEngine::postTriggerEvent(const ExciterTriggerEvent& evt) noexcept {
    const size_t writeIdx = mFifoWritePos.load(std::memory_order_relaxed);
    const size_t readIdx = mFifoReadPos.load(std::memory_order_acquire);
    const size_t nextWrite = (writeIdx + 1) & kFifoMask;

    if (nextWrite == readIdx) {
        return false; // SPSC queue full
    }

    mEventFifo[writeIdx] = evt;
    mFifoWritePos.store(nextWrite, std::memory_order_release);
    return true;
}

bool AcousticExciterEngine::triggerNoteAsync(float midiNote, float velocity, float durationSec) noexcept {
    ExciterTriggerEvent evt;
    evt.type = StimulusType::ChimeTine;
    evt.midiNote = midiNote;
    evt.velocity = velocity;
    evt.durationSec = durationSec;
    evt.chordIndex = -1;
    return postTriggerEvent(evt);
}

bool AcousticExciterEngine::triggerChordAsync(int chordIndex, float rootMidi, float velocity, StrumSpeed speed) noexcept {
    ExciterTriggerEvent evt;
    evt.type = StimulusType::ChimeTine;
    evt.midiNote = rootMidi;
    evt.velocity = velocity;
    evt.durationSec = 3.5f;
    evt.chordIndex = chordIndex;
    evt.strumSpeed = speed;
    return postTriggerEvent(evt);
}

bool AcousticExciterEngine::triggerDiracAsync(float polarity) noexcept {
    ExciterTriggerEvent evt;
    evt.type = StimulusType::DiracImpulse;
    evt.param1 = polarity;
    return postTriggerEvent(evt);
}

bool AcousticExciterEngine::triggerPinkBurstAsync(float durationMs) noexcept {
    ExciterTriggerEvent evt;
    evt.type = StimulusType::PinkBurst;
    evt.param1 = durationMs;
    return postTriggerEvent(evt);
}

bool AcousticExciterEngine::triggerHammerThudAsync(float hardness) noexcept {
    ExciterTriggerEvent evt;
    evt.type = StimulusType::HammerThud;
    evt.param1 = hardness;
    return postTriggerEvent(evt);
}

void AcousticExciterEngine::triggerVoice(float midiNote, float velocity, float durationSec) noexcept {
    // 16-Voice Pool: Look for an inactive voice, or steal the oldest voice (LRU)
    ChimeVoice* targetVoice = nullptr;
    uint32_t maxAge = 0;

    for (auto& voice : mVoices) {
        if (!voice.isActive()) {
            targetVoice = &voice;
            break;
        }
        if (voice.getAge() > maxAge) {
            maxAge = voice.getAge();
            targetVoice = &voice;
        }
    }

    if (targetVoice != nullptr) {
        targetVoice->trigger(midiNote, velocity, durationSec);
    }
}

void AcousticExciterEngine::releaseVoice(float midiNote) noexcept {
    const float roundTarget = std::round(midiNote);
    for (auto& voice : mVoices) {
        if (voice.isActive() && std::abs(std::round(voice.getMidiNote()) - roundTarget) < 0.5f) {
            voice.release();
        }
    }
}

void AcousticExciterEngine::releaseAllVoices() noexcept {
    for (auto& voice : mVoices) {
        if (voice.isActive()) {
            voice.release();
        }
    }
}

void AcousticExciterEngine::triggerChord(int chordIndex, float rootMidi, float velocity, StrumSpeed speed) noexcept {
    const int cIdx = std::clamp(chordIndex, 0, static_cast<int>(kNumChordVoicings - 1));
    const auto& voicing = kChordVoicings[cIdx];
    const int sIdx = std::clamp(static_cast<int>(speed), 0, static_cast<int>(kNumStrumSpeeds - 1));
    const auto& strum = kStrumSpeeds[sIdx];

    for (int k = 0; k < voicing.numNotes; ++k) {
        const float notePitch = rootMidi + voicing.intervals[k];
        float delayMs = 0.0f;
        if (k > 0 && speed != StrumSpeed::Instant && strum.rateMs > 0.0f) {
            const float jitter = (mStrumPrng.nextFloat01() * 2.0f - 1.0f) * strum.jitterMs;
            delayMs = static_cast<float>(k) * strum.rateMs + jitter;
        }

        const int delaySamples = static_cast<int>((delayMs * 0.001f) * static_cast<float>(mSampleRate));
        if (delaySamples <= 0) {
            triggerVoice(notePitch, velocity, 3.5f);
        } else {
            // Find free slot in fixed scheduled notes pool
            for (auto& sn : mScheduledNotes) {
                if (!sn.active) {
                    sn.active = true;
                    sn.delaySamples = delaySamples;
                    sn.midiNote = notePitch;
                    sn.velocity = velocity;
                    sn.durationSec = 3.5f;
                    break;
                }
            }
        }
    }
}

void AcousticExciterEngine::triggerDirac(float polarity) noexcept {
    mLabGen.triggerDirac(polarity);
}

void AcousticExciterEngine::triggerPinkBurst(float durationMs) noexcept {
    mLabGen.triggerPinkBurst(durationMs);
}

void AcousticExciterEngine::triggerHammerThud(float hardness) noexcept {
    mLabGen.triggerHammerThud(hardness);
}

void AcousticExciterEngine::drainTriggerQueue() noexcept {
    size_t readIdx = mFifoReadPos.load(std::memory_order_relaxed);
    const size_t writeIdx = mFifoWritePos.load(std::memory_order_acquire);

    while (readIdx != writeIdx) {
        handleTriggerEvent(mEventFifo[readIdx]);
        readIdx = (readIdx + 1) & kFifoMask;
    }

    mFifoReadPos.store(readIdx, std::memory_order_release);
}

void AcousticExciterEngine::handleTriggerEvent(const ExciterTriggerEvent& evt) noexcept {
    switch (evt.type) {
        case StimulusType::ChimeTine: {
            if (evt.chordIndex >= 0) {
                triggerChord(evt.chordIndex, evt.midiNote, evt.velocity, evt.strumSpeed);
            } else {
                triggerVoice(evt.midiNote, evt.velocity, evt.durationSec);
            }
            break;
        }
        case StimulusType::DiracImpulse:
            mLabGen.triggerDirac(evt.param1);
            break;
        case StimulusType::PinkBurst:
            mLabGen.triggerPinkBurst(evt.param1 > 0.0f ? evt.param1 : 40.0f);
            break;
        case StimulusType::HammerThud:
            mLabGen.triggerHammerThud(evt.param1 > 0.0f ? evt.param1 : 0.7f);
            break;
    }
}

int AcousticExciterEngine::getActiveVoiceCount() const noexcept {
    int count = 0;
    for (const auto& voice : mVoices) {
        if (voice.isActive()) count++;
    }
    return count;
}

void AcousticExciterEngine::process(float* outL, float* outR, int numSamples) noexcept {
    if (numSamples <= 0 || outL == nullptr || outR == nullptr) return;

    ScopedNoDenormals noDenormals;

    if (!mParams.enable) {
        std::fill(outL, outL + numSamples, 0.0f);
        std::fill(outR, outR + numSamples, 0.0f);
        return;
    }

    drainTriggerQueue();

    for (int n = 0; n < numSamples; ++n) {
        // 1. Advance Poisson generative clock if active
        if (mParams.poissonEnable) {
            float pMidi = 60.0f;
            float pVel = 0.60f;
            if (mPoissonClock.tick(mParams.poissonEpm, mParams.poissonHumanize,
                                   pMidi, pVel, mParams.rootPitchClass, mParams.scaleIndex)) {
                triggerVoice(pMidi, pVel, 3.5f);
            }
        }

        // 2. Advance scheduled chord notes
        for (auto& sn : mScheduledNotes) {
            if (sn.active) {
                if (--sn.delaySamples <= 0) {
                    triggerVoice(sn.midiNote, sn.velocity, sn.durationSec);
                    sn.active = false;
                }
            }
        }

        // 3. Synthesize active chime voices
        float sampL = 0.0f;
        float sampR = 0.0f;

        for (auto& voice : mVoices) {
            voice.processSample(sampL, sampR);
        }

        // 4. Synthesize precision laboratory impulse generator
        mLabGen.processSample(sampL, sampR);

        // 5. Apply smooth level gain & soft saturation knee
        const float gain = mLevelSmoother.next();
        sampL *= gain;
        sampR *= gain;
        sampL = applySmoothBoundaryKnee(sampL, 0.75f, 1.0f);
        sampR = applySmoothBoundaryKnee(sampR, 0.75f, 1.0f);

        outL[n] = flushDenormal(sampL);
        outR[n] = flushDenormal(sampR);
    }
}

void AcousticExciterEngine::process(float* const* directBus, float* const* reverbBus, int numSamples) noexcept {
    if (numSamples <= 0) return;

    ScopedNoDenormals noDenormals;

    if (!mParams.enable) return;

    drainTriggerQueue();

    for (int n = 0; n < numSamples; ++n) {
        // 1. Advance Poisson generative clock if active
        if (mParams.poissonEnable) {
            float pMidi = 60.0f;
            float pVel = 0.60f;
            if (mPoissonClock.tick(mParams.poissonEpm, mParams.poissonHumanize,
                                   pMidi, pVel, mParams.rootPitchClass, mParams.scaleIndex)) {
                triggerVoice(pMidi, pVel, 3.5f);
            }
        }

        // 2. Advance scheduled chord notes
        for (auto& sn : mScheduledNotes) {
            if (sn.active) {
                if (--sn.delaySamples <= 0) {
                    triggerVoice(sn.midiNote, sn.velocity, sn.durationSec);
                    sn.active = false;
                }
            }
        }

        // 3. Synthesize active chime voices
        float sampL = 0.0f;
        float sampR = 0.0f;

        for (auto& voice : mVoices) {
            voice.processSample(sampL, sampR);
        }

        // 4. Synthesize precision laboratory impulse generator
        mLabGen.processSample(sampL, sampR);

        // 5. Apply smooth level gain & soft saturation knee
        const float gain = mLevelSmoother.next();
        sampL *= gain;
        sampR *= gain;
        sampL = applySmoothBoundaryKnee(sampL, 0.75f, 1.0f);
        sampR = applySmoothBoundaryKnee(sampR, 0.75f, 1.0f);

        sampL = flushDenormal(sampL);
        sampR = flushDenormal(sampR);

        // 6. Route into Reverb Input Bus
        if (reverbBus != nullptr) {
            if (reverbBus[0] != nullptr) reverbBus[0][n] += sampL;
            if (reverbBus[1] != nullptr) reverbBus[1][n] += sampR;
        }

        // 7. Route into Direct Dry Bus if routing is ReverbAndDirect
        if (mParams.routing == ExciterRouting::ReverbAndDirect && directBus != nullptr) {
            if (directBus[0] != nullptr) directBus[0][n] += sampL;
            if (directBus[1] != nullptr) directBus[1][n] += sampR;
        }
    }
}

void AcousticExciterEngine::process(float* const* directBus, float* const* reverbBus, int numSamples,
                                    bool exciterEnable, float exciterLevelDb, ExciterRouting routing,
                                    bool poissonEnable, float poissonEpm, float poissonHumanize,
                                    int rootPitchClass, const int* scaleIntervals, int numScaleIntervals) noexcept {
    if (numSamples <= 0) return;

    ScopedNoDenormals noDenormals;

    mParams.enable = exciterEnable;
    setLevelDb(exciterLevelDb);
    mParams.routing = routing;
    mParams.poissonEnable = poissonEnable;
    mParams.poissonEpm = poissonEpm;
    mParams.poissonHumanize = poissonHumanize;
    mParams.rootPitchClass = rootPitchClass;

    if (!mParams.enable) return;

    drainTriggerQueue();

    for (int n = 0; n < numSamples; ++n) {
        // 1. Advance Poisson generative clock with custom scale intervals if provided
        if (mParams.poissonEnable) {
            float pMidi = 60.0f;
            float pVel = 0.60f;
            if (mPoissonClock.tick(mParams.poissonEpm, mParams.poissonHumanize,
                                   pMidi, pVel, mParams.rootPitchClass, mParams.scaleIndex,
                                   scaleIntervals, numScaleIntervals)) {
                triggerVoice(pMidi, pVel, 3.5f);
            }
        }

        // 2. Advance scheduled chord notes
        for (auto& sn : mScheduledNotes) {
            if (sn.active) {
                if (--sn.delaySamples <= 0) {
                    triggerVoice(sn.midiNote, sn.velocity, sn.durationSec);
                    sn.active = false;
                }
            }
        }

        // 3. Synthesize active chime voices
        float sampL = 0.0f;
        float sampR = 0.0f;

        for (auto& voice : mVoices) {
            voice.processSample(sampL, sampR);
        }

        // 4. Synthesize precision laboratory impulse generator
        mLabGen.processSample(sampL, sampR);

        // 5. Apply smooth level gain & soft saturation knee
        const float gain = mLevelSmoother.next();
        sampL *= gain;
        sampR *= gain;
        sampL = applySmoothBoundaryKnee(sampL, 0.75f, 1.0f);
        sampR = applySmoothBoundaryKnee(sampR, 0.75f, 1.0f);

        sampL = flushDenormal(sampL);
        sampR = flushDenormal(sampR);

        // 6. Route into Reverb Input Bus
        if (reverbBus != nullptr) {
            if (reverbBus[0] != nullptr) reverbBus[0][n] += sampL;
            if (reverbBus[1] != nullptr) reverbBus[1][n] += sampR;
        }

        // 7. Route into Direct Dry Bus if routing is ReverbAndDirect
        if (mParams.routing == ExciterRouting::ReverbAndDirect && directBus != nullptr) {
            if (directBus[0] != nullptr) directBus[0][n] += sampL;
            if (directBus[1] != nullptr) directBus[1][n] += sampR;
        }
    }
}

} // namespace rb26
