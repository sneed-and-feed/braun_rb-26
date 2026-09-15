#include "LowBandModalMatrix.h"
#include <cmath>
#include <numeric>

namespace rb26 {

static bool isPrime(size_t n) noexcept {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (size_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

size_t LowBandModalMatrix::findClosestPrime(size_t target) const noexcept {
    if (isPrime(target)) return target;
    size_t offset = 1;
    while (true) {
        if (target >= offset && isPrime(target - offset)) return target - offset;
        if (isPrime(target + offset)) return target + offset;
        ++offset;
    }
}

LowBandModalMatrix::LowBandModalMatrix() noexcept {
    for (size_t i = 0; i < kNumModalLines; ++i) {
        mDelayBuffers[i].assign(32768, 0.0f); // Pre-allocate maximum buffer capacity up to 192 kHz
    }
}

void LowBandModalMatrix::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);

    // Prepare child subsystems
    mCrossover.prepare(mSampleRate);
    mCrossover.setCutoff(mParams.crossoverHz);

    mPunchDetector.prepare(mSampleRate);

    mEllipticalFilter.prepare(mSampleRate);
    mEllipticalFilter.setCutoff(mParams.subMonoHz);

    // Size prime delay lines (sized up to 192 kHz max)
    for (size_t i = 0; i < kNumModalLines; ++i) {
        const size_t nominalSamples = static_cast<size_t>(std::round(kBaseDelayTimes[i] * mSampleRate));
        mDelayLengths[i] = findClosestPrime(nominalSamples);
        // Ensure buffer has sufficient capacity without reallocating in process
        if (mDelayBuffers[i].size() < mDelayLengths[i] + 64) {
            mDelayBuffers[i].resize(mDelayLengths[i] + 64, 0.0f);
        }
        std::fill(mDelayBuffers[i].begin(), mDelayBuffers[i].end(), 0.0f);
        mWriteIndices[i] = 0;
    }

    updateDecayCoefficients();
    reset();
}

void LowBandModalMatrix::reset() noexcept {
    mCrossover.reset();
    mPunchDetector.reset();
    mEllipticalFilter.reset();

    for (size_t i = 0; i < kNumModalLines; ++i) {
        std::fill(mDelayBuffers[i].begin(), mDelayBuffers[i].end(), 0.0f);
        mWriteIndices[i] = 0;
    }

    mLastLowEnergy = 0.0f;
    mSubBlockCounter = 0;
}

void LowBandModalMatrix::setParameters(const LowBandModalParams& params) noexcept {
    mParams = params;
    mCrossover.setCutoff(mParams.crossoverHz);
    mEllipticalFilter.setCutoff(mParams.subMonoHz);
    updateDecayCoefficients();
}

void LowBandModalMatrix::updateDecayCoefficients() noexcept {
    if (mParams.freezeHold) {
        for (size_t i = 0; i < kNumModalLines; ++i) {
            mDecayCoeffs[i] = 1.0f;
        }
        return;
    }

    const float effectiveRt60 = std::clamp(mParams.rt60DecaySec * mParams.bassRt60Mult, 0.05f, 120.0f);
    // g_k = exp(-6.9077553 * T_k / RT60_low)
    for (size_t i = 0; i < kNumModalLines; ++i) {
        const float tSec = static_cast<float>(mDelayLengths[i]) / mSampleRate;
        mDecayCoeffs[i] = std::exp(-6.907755278982137f * tSec / effectiveRt60);
    }
}

void LowBandModalMatrix::processCrossoverOnly(float inL, float inR,
                                            float& lowL, float& lowR,
                                            float& highL, float& highR) noexcept {
    mCrossover.process(inL, inR, lowL, lowR, highL, highR);
}

void LowBandModalMatrix::processModalOnly(float lowInL, float lowInR,
                                        float& lowOutL, float& lowOutR) noexcept {
    ScopedNoDenormals noDenormals;

    // 1. Transient punch detector
    const float duckGain = mPunchDetector.process(lowInL, lowInR, mParams.punchDucking);
    const float duckedL = lowInL * duckGain;
    const float duckedR = lowInR * duckGain;

    // 2. Read 4 delay lines
    std::array<float, kNumModalLines> w {};
    for (size_t i = 0; i < kNumModalLines; ++i) {
        w[i] = mDelayBuffers[i][mWriteIndices[i]];
    }

    // 3. Lossless Householder reflection matrix (H_4 = I_4 - 0.5 * 1*1^T)
    const float sum = w[0] + w[1] + w[2] + w[3];
    const float halfSum = flushDenormal(sum * 0.5f);

    std::array<float, kNumModalLines> v {};
    for (size_t i = 0; i < kNumModalLines; ++i) {
        v[i] = w[i] - halfSum;
    }

    // 4. Input distribution (spatial decorrelation)
    const float midIn  = 0.5f * (duckedL + duckedR);
    const float sideIn = 0.5f * (duckedL - duckedR);

    std::array<float, kNumModalLines> injection {};
    injection[0] = duckedL;
    injection[1] = duckedR;
    injection[2] = midIn;
    injection[3] = sideIn;

    // 5. Feedback recirculation with Hermite soft-knee boundary saturation
    for (size_t i = 0; i < kNumModalLines; ++i) {
        const float feedback = v[i] * mDecayCoeffs[i];
        const float nextIn = mParams.freezeHold ? feedback : (feedback + injection[i]);
        const float saturated = applySmoothBoundaryKnee(nextIn, 0.72f);

        mDelayBuffers[i][mWriteIndices[i]] = flushDenormal(saturated);
        mWriteIndices[i] = (mWriteIndices[i] + 1) % mDelayLengths[i];
    }

    // 6. Balanced orthogonal Hadamard output summing with Bass RT60 presence scaling
    // Eliminates pairwise comb cancellations and assertively blooms low frequencies when bassRt60Mult is high
    const float bassPresence = std::clamp(std::sqrt(mParams.bassRt60Mult), 0.707f, 2.0f);
    const float rawLowL = 0.5f * (-w[0] - w[1] + w[2] + w[3]) * bassPresence;
    const float rawLowR = 0.5f * (w[0] + w[1] + w[2] + w[3]) * bassPresence;

    // Apply punch ducking to modal output to eliminate bass smear during transients (bypassed in freeze)
    const float outDuck = mParams.freezeHold ? 1.0f : std::pow(duckGain, 2.0f);
    const float duckedOutL = rawLowL * outDuck;
    const float duckedOutR = rawLowR * outDuck;

    // 7. Sub-bass elliptical M/S filter below cutoff
    mEllipticalFilter.process(duckedOutL, duckedOutR, lowOutL, lowOutR);
    lowOutL = flushDenormal(lowOutL);
    lowOutR = flushDenormal(lowOutR);
}

void LowBandModalMatrix::processSample(float inL, float inR,
                                       float& highOutL, float& highOutR,
                                       float& lowReverbOutL, float& lowReverbOutR) noexcept {
    ScopedNoDenormals noDenormals;

    // 1. LR4 Crossover separates Low and High bands
    float lowInL = 0.0f, lowInR = 0.0f;
    mCrossover.process(inL, inR, lowInL, lowInR, highOutL, highOutR);

    // 2. Process modal reverberation on low band
    processModalOnly(lowInL, lowInR, lowReverbOutL, lowReverbOutR);

    // Telemetry tracking
    const float currentLowAbs = 0.5f * (std::abs(lowReverbOutL) + std::abs(lowReverbOutR));
    mLastLowEnergy = flushDenormal(0.995f * mLastLowEnergy + 0.005f * currentLowAbs);
}

void LowBandModalMatrix::processBlock(const float* inL, const float* inR,
                                     float* highOutL, float* highOutR,
                                     float* lowReverbOutL, float* lowReverbOutR,
                                     int numSamples) noexcept {
    ScopedNoDenormals noDenormals;

    for (int i = 0; i < numSamples; ++i) {
        processSample(inL[i], inR[i],
                      highOutL[i], highOutR[i],
                      lowReverbOutL[i], lowReverbOutR[i]);
    }
}

} // namespace rb26
