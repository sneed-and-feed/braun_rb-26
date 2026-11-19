#include "ShepardPitchSpiral.h"
#include "DspMath.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

ShepardPitchSpiral::ShepardPitchSpiral() noexcept {
    mDepthSmoother.setTimeConstant(0.025f);
    mRateSmoother.setTimeConstant(0.030f);
    mPartchRatioSmoother.setTimeConstant(0.035f);
    prepare(48000.0);
}

void ShepardPitchSpiral::prepare(double sampleRate) noexcept {
    mSampleRate = static_cast<float>(sampleRate > 100.0 ? sampleRate : 48000.0);
    mWindowSamples = std::clamp(mWindowSec * mSampleRate, 256.0f, static_cast<float>(kMaxCapacity / 2));

    mDelayBufferL.assign(static_cast<size_t>(kMaxCapacity), 0.0f);
    mDelayBufferR.assign(static_cast<size_t>(kMaxCapacity), 0.0f);
    mBufferDirty = false;
    mWriteIndex = 0;
    mSpiralPhase = 0.0f;

    mDepthSmoother.setSampleRate(mSampleRate);
    mRateSmoother.setSampleRate(mSampleRate);
    mPartchRatioSmoother.setSampleRate(mSampleRate);

    mDepthSmoother.reset(mDepth);
    mRateSmoother.reset(mRateHz);
    mPartchRatioSmoother.reset(kPartchRatios[static_cast<size_t>(mPartchIndex)]);

    reset();
}

void ShepardPitchSpiral::reset() noexcept {
    if (mBufferDirty) {
        std::fill(mDelayBufferL.begin(), mDelayBufferL.end(), 0.0f);
        std::fill(mDelayBufferR.begin(), mDelayBufferR.end(), 0.0f);
        mBufferDirty = false;
    }
    mWriteIndex = 0;
    mSpiralPhase = 0.0f;

    for (int m = 0; m < kNumVoices; ++m) {
        mVoicesL[m].grainPhase = static_cast<float>(m) * 0.25f;
        mVoicesL[m].currentRatio = 1.0f;
        mVoicesL[m].currentWeight = 0.0f;

        // Stereo decorrelation: offset right channel grains by quarter cycle
        mVoicesR[m].grainPhase = std::fmod(mVoicesL[m].grainPhase + mStereoSpread, 1.0f);
        mVoicesR[m].currentRatio = 1.0f;
        mVoicesR[m].currentWeight = 0.0f;
    }

    mDepthSmoother.reset(mDepthSmoother.getTarget());
    mRateSmoother.reset(mRateHz);
    mPartchRatioSmoother.reset(mPartchRatioSmoother.getTarget());
}

void ShepardPitchSpiral::setMode(SpiralMode mode) noexcept {
    mMode = mode;
}

void ShepardPitchSpiral::setRateHz(float rateHz) noexcept {
    mRateHz = std::clamp(rateHz, 0.005f, 5.0f);
    mRateSmoother.setTarget(mRateHz);
}

void ShepardPitchSpiral::setDepth(float depth) noexcept {
    mDepth = std::clamp(depth, 0.0f, 1.0f);
    mDepthSmoother.setTarget(mDepth);
}

void ShepardPitchSpiral::setPartchInterval(int intervalIndex) noexcept {
    mPartchIndex = std::clamp(intervalIndex, 0, 5);
    mPartchRatioSmoother.setTarget(kPartchRatios[static_cast<size_t>(mPartchIndex)]);
    mPartchPolyphonic = false; // Selecting an individual interval switches to single-interval mode
}

void ShepardPitchSpiral::setPartchPolyphonic(bool polyphonic) noexcept {
    mPartchPolyphonic = polyphonic;
}

void ShepardPitchSpiral::setWindowSec(float sec) noexcept {
    mWindowSec = std::clamp(sec, 0.020f, 0.200f);
    mWindowSamples = std::clamp(mWindowSec * mSampleRate, 256.0f, static_cast<float>(kMaxCapacity / 2));
}

void ShepardPitchSpiral::setStereoSpread(float spread) noexcept {
    mStereoSpread = std::clamp(spread, 0.0f, 0.5f);
}

float ShepardPitchSpiral::getVoiceRatio(int voiceIdx) const noexcept {
    if (voiceIdx >= 0 && voiceIdx < kNumVoices) {
        return mVoicesL[static_cast<size_t>(voiceIdx)].currentRatio;
    }
    return 1.0f;
}

float ShepardPitchSpiral::getVoiceWeight(int voiceIdx) const noexcept {
    if (voiceIdx >= 0 && voiceIdx < kNumVoices) {
        return mVoicesL[static_cast<size_t>(voiceIdx)].currentWeight;
    }
    return 0.0f;
}

inline float ShepardPitchSpiral::readLinear(const std::vector<float>& buffer, float readPos) const noexcept {
    if (!rb26::isFiniteBitwise(readPos)) [[unlikely]] {
        return 0.0f;
    }
    const int i0 = static_cast<int>(std::floor(readPos));
    const float frac = readPos - static_cast<float>(i0);

    const int i0_m = i0 & kBufferMask;
    const int i1  = (i0 + 1) & kBufferMask;

    const float y0 = buffer[static_cast<size_t>(i0_m)];
    const float y1 = buffer[static_cast<size_t>(i1)];

    return interpolateLinear2P1O(y0, y1, frac);
}

inline float ShepardPitchSpiral::readHermite(const std::vector<float>& buffer, float readPos) const noexcept {
    if (!rb26::isFiniteBitwise(readPos)) [[unlikely]] {
        return 0.0f;
    }
    const int i0 = static_cast<int>(std::floor(readPos));
    const float frac = readPos - static_cast<float>(i0);

    const int im1 = (i0 - 1) & kBufferMask;
    const int i0_m = i0 & kBufferMask;
    const int i1  = (i0 + 1) & kBufferMask;
    const int i2  = (i0 + 2) & kBufferMask;

    const float ym1 = buffer[static_cast<size_t>(im1)];
    const float y0  = buffer[static_cast<size_t>(i0_m)];
    const float y1  = buffer[static_cast<size_t>(i1)];
    const float y2  = buffer[static_cast<size_t>(i2)];

    return interpolateHermite4P3O(ym1, y0, y1, y2, frac);
}

void ShepardPitchSpiral::processSample(float inL, float inR, float& outL, float& outR) noexcept {
    if (mDelayBufferL.empty()) [[unlikely]] {
        outL = flushDenormal(inL);
        outR = flushDenormal(inR);
        return;
    }
    if (mMode == SpiralMode::Bypass) [[unlikely]] {
        outL = flushDenormal(inL);
        outR = flushDenormal(inR);
        return;
    }

    const float depth = mDepthSmoother.next();
    const float rate = mRateSmoother.next();
    const float partchTargetRatio = mPartchRatioSmoother.next();

    // 1. Write incoming samples to circular delay buffers
    mBufferDirty = true;
    mDelayBufferL[static_cast<size_t>(mWriteIndex)] = flushDenormal(inL);
    mDelayBufferR[static_cast<size_t>(mWriteIndex)] = flushDenormal(inR);

    // 2. Advance master spiral phase accumulator
    // One full 4-voice cycle = 4 octaves. Rate in Hz determines cycle duration.
    float deltaSigma = 0.0f;
    if (mMode == SpiralMode::BarberShimmer) {
        deltaSigma = +(rate * 4.0f) / mSampleRate;
    } else if (mMode == SpiralMode::BarberDimmer) {
        deltaSigma = -(rate * 4.0f) / mSampleRate;
    }

    mSpiralPhase += deltaSigma;
    if (mSpiralPhase >= 4.0f) mSpiralPhase -= 4.0f;
    if (mSpiralPhase < 0.0f)  mSpiralPhase += 4.0f;

    // 3. Process 4 voices for Left and Right channels
    float sumL = 0.0f;
    float sumR = 0.0f;
    const bool useLin = shouldUseLinear();

    for (int m = 0; m < kNumVoices; ++m) {
        float rm = 1.0f;
        float Am = 0.0f;

        if (mMode == SpiralMode::BarberDimmer || mMode == SpiralMode::BarberShimmer) {
            // Normalized phase for voice m in [0.0, 4.0)
            float sigma_m = mSpiralPhase + static_cast<float>(m);
            if (sigma_m >= 4.0f) sigma_m -= 4.0f;

            // Frequency ratio r_m(t) = 2^(sigma_m - 2)
            rm = exp2f(sigma_m - 2.0f);

            // Raised-cosine spectral window A_m(t) = 0.5 * (1 - cos(pi/2 * sigma_m))
            Am = 0.5f * (1.0f - FastSinTable::cos(kHalfPi * sigma_m));
        } else if (mMode == SpiralMode::PartchLattice) {
            if (mPartchPolyphonic) {
                // 4-Voice Partch Utonality Cluster: { 1/1, 1/2, 1/3, 1/4 }
                rm = kPartchRatios[static_cast<size_t>(m)];
                // Equal-power weighting: sum A_m^2 == 1.5
                Am = 0.6123724356957945f; // sqrt(1.5 / 4)
            } else {
                // Single Partch Undertone Interval: { 1/(N+1) }
                rm = partchTargetRatio;
                Am = (m == 0) ? std::sqrt(1.5f) : 0.0f;
            }
        }

        mVoicesL[m].currentRatio = rm;
        mVoicesL[m].currentWeight = Am;
        mVoicesR[m].currentRatio = rm;
        mVoicesR[m].currentWeight = Am;

        // Skip computation if voice has zero weight
        if (Am < 1.0e-6f) {
            continue;
        }

        // Signed phase increment for grain delay modulation:
        // readPos(t) = writePos(t) - delay(t), so d(readPos)/dt = 1 - d(delay)/dt == r_m
        // -> d(delay)/dt = 1 - r_m -> signedPhaseInc = (1 - r_m) / windowSamples.
        // Preserves C^1 smooth delay slope across unison (r_m = 1.0) and wraps only at
        // phi = 0 / 1 where Hann window sin^2(pi * phi) and its derivative are strictly zero.
        const float grainPhaseInc = (1.0f - rm) / mWindowSamples;

        // --- Left Channel Voice Processing ---
        {
            auto& vL = mVoicesL[m];
            vL.grainPhase += grainPhaseInc;
            vL.grainPhase -= std::floor(vL.grainPhase);
            if (vL.grainPhase >= 1.0f || vL.grainPhase < 0.0f) {
                vL.grainPhase = 0.0f;
            }

            const float phiA = vL.grainPhase;
            const float phiB = (phiA >= 0.5f) ? (phiA - 0.5f) : (phiA + 0.5f);

            constexpr float kMinDelayMargin = 2.0f;
            const float delayA = kMinDelayMargin + mWindowSamples * phiA;
            const float delayB = kMinDelayMargin + mWindowSamples * phiB;

            const float readPosA = static_cast<float>(mWriteIndex) - delayA;
            const float readPosB = static_cast<float>(mWriteIndex) - delayB;

            const float sA = useLin ? readLinear(mDelayBufferL, readPosA) : readHermite(mDelayBufferL, readPosA);
            const float sB = useLin ? readLinear(mDelayBufferL, readPosB) : readHermite(mDelayBufferL, readPosB);

            // Constant-amplitude Hann crossfade windows: wA + wB == sin^2(pi*phi) + cos^2(pi*phi) == 1.0
            const float sinA = FastSinTable::sin(kPi * phiA);
            const float sinB = FastSinTable::sin(kPi * phiB);
            const float wA = sinA * sinA;
            const float wB = sinB * sinB;

            const float voiceOutL = wA * sA + wB * sB;
            sumL += Am * voiceOutL;
        }

        // --- Right Channel Voice Processing (with stereo phase spread) ---
        {
            auto& vR = mVoicesR[m];
            vR.grainPhase += grainPhaseInc;
            vR.grainPhase -= std::floor(vR.grainPhase);
            if (vR.grainPhase >= 1.0f || vR.grainPhase < 0.0f) {
                vR.grainPhase = 0.0f;
            }

            const float phiA = vR.grainPhase;
            const float phiB = (phiA >= 0.5f) ? (phiA - 0.5f) : (phiA + 0.5f);

            constexpr float kMinDelayMargin = 2.0f;
            const float delayA = kMinDelayMargin + mWindowSamples * phiA;
            const float delayB = kMinDelayMargin + mWindowSamples * phiB;

            const float readPosA = static_cast<float>(mWriteIndex) - delayA;
            const float readPosB = static_cast<float>(mWriteIndex) - delayB;

            const float sA = useLin ? readLinear(mDelayBufferR, readPosA) : readHermite(mDelayBufferR, readPosA);
            const float sB = useLin ? readLinear(mDelayBufferR, readPosB) : readHermite(mDelayBufferR, readPosB);

            const float sinA = FastSinTable::sin(kPi * phiA);
            const float sinB = FastSinTable::sin(kPi * phiB);
            const float wA = sinA * sinA;
            const float wB = sinB * sinB;

            const float voiceOutR = wA * sA + wB * sB;
            sumR += Am * voiceOutR;
        }
    }

    // 4. Power Normalization: sum A_m^2 == 1.5 -> scale by sqrt(2/3) for exact unity gain
    const float normalizedL = sumL * kNormGain;
    const float normalizedR = sumR * kNormGain;

    // 5. Depth crossfade: blend between raw input and pitch spiral
    outL = flushDenormal((1.0f - depth) * inL + depth * normalizedL);
    outR = flushDenormal((1.0f - depth) * inR + depth * normalizedR);

    // 6. Advance circular buffer write index
    mWriteIndex = (mWriteIndex + 1) & kBufferMask;
}

float ShepardPitchSpiral::processMonoSample(float input, int channel) noexcept {
    float outL = 0.0f, outR = 0.0f;
    if (channel == 1) {
        processSample(0.0f, input, outL, outR);
        return outR;
    }
    processSample(input, 0.0f, outL, outR);
    return outL;
}

void ShepardPitchSpiral::process(const float* inL,
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
