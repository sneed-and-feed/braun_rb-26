#include "ManifoldDelayNetwork.h"
#include "TailModulator.h"
#include <cmath>
#include <algorithm>

namespace rb26 {

// Airy function Ai(-x) zeros a_1 to a_8 for Whispering Gallery radial modes
static constexpr std::array<float, 8> kAiryZeros = {{
    2.33810741f, 4.08794944f, 5.52055983f, 6.78670809f,
    7.94413359f, 9.02265085f, 10.04017434f, 11.00852430f
}};

// Prime decorrelation offsets for Whispering Gallery
static constexpr std::array<size_t, 8> kWhisperingPrimeOffsets = {{
    0, 13, 29, 43, 61, 79, 97, 113
}};

// Biharmonic plate mode indices (m_k, n_k) for Anharmonic Plate
struct PlateMode { float m; float n; };
static constexpr std::array<PlateMode, 8> kPlateModes = {{
    {1.0f, 1.0f}, {1.0f, 2.0f}, {2.0f, 1.0f}, {2.0f, 2.0f},
    {3.0f, 1.0f}, {3.0f, 2.0f}, {3.0f, 3.0f}, {4.0f, 1.0f}
}};

// Prime decorrelation offsets for Anharmonic Plate
static constexpr std::array<size_t, 8> kPlatePrimeOffsets = {{
    0, 11, 23, 37, 53, 67, 79, 97
}};

// Stockhausen Klangdom antipodal perturbation factors
static constexpr std::array<float, 8> kKlangdomDeltas = {{
    -0.079f, -0.053f, -0.023f, 0.000f, +0.023f, +0.053f, +0.079f, +0.107f
}};

ManifoldDelayNetwork::ManifoldDelayNetwork() noexcept {
    for (size_t k = 0; k < kNumLines; ++k) {
        mBuffers[k].assign(kBufferCapacity, 0.0f);
        mWriteIndices[k] = 0;
        mNominalLengths[k] = 1000;
        mLengthSmoothers[k].setTimeConstant(0.040f);
        mSpatialWeightsL[k].setTimeConstant(0.040f);
        mSpatialWeightsR[k].setTimeConstant(0.040f);
    }
}

void ManifoldDelayNetwork::prepare(double sampleRate, float maxRoomSize) noexcept {
    mSampleRate = sampleRate > 100.0 ? sampleRate : 48000.0;
    mMaxRoomSize = std::max(1.0f, maxRoomSize);
    const float fs = static_cast<float>(mSampleRate);

    // 0.25 Hz circular spatial rotation for Whispering Gallery
    mCausticRotationDelta = (kTwoPi * 0.25f) / fs;
    mCausticRotationAngle = 0.0f;
    mIsFirstSet = true;

    for (size_t k = 0; k < kNumLines; ++k) {
        mBuffers[k].assign(kBufferCapacity, 0.0f);
        mWriteIndices[k] = 0;
        mDampingStates[k] = 0.0f;

        mLengthSmoothers[k].setSampleRate(fs);
        mLengthSmoothers[k].setTimeConstant(0.040f);

        mSpatialWeightsL[k].setSampleRate(fs);
        mSpatialWeightsL[k].setTimeConstant(0.040f);

        mSpatialWeightsR[k].setSampleRate(fs);
        mSpatialWeightsR[k].setTimeConstant(0.040f);

        mDispersionStage1[k].reset();
        mDispersionStage2[k].reset();

        mCausticPeaking[k].reset();
        mUltrasonicLowpass[k].reset();

        mSpruceA0[k].reset();
        mSpruceT1[k].reset();
        mSpruceWood[k].reset();
    }

    updateManifoldGeometry();
    updateFilterCoefficients();
    updateSpatialWeights();

    for (size_t k = 0; k < kNumLines; ++k) {
        mLengthSmoothers[k].reset(static_cast<float>(mNominalLengths[k]));
        mSpatialWeightsL[k].reset(mSpatialWeightsL[k].getTarget());
        mSpatialWeightsR[k].reset(mSpatialWeightsR[k].getTarget());
    }
    reset();
}

void ManifoldDelayNetwork::reset() noexcept {
    for (size_t k = 0; k < kNumLines; ++k) {
        std::fill(mBuffers[k].begin(), mBuffers[k].end(), 0.0f);
        mWriteIndices[k] = 0;
        mDampingStates[k] = 0.0f;

        mDispersionStage1[k].reset();
        mDispersionStage2[k].reset();

        mCausticPeaking[k].reset();
        mUltrasonicLowpass[k].reset();

        mSpruceA0[k].reset();
        mSpruceT1[k].reset();
        mSpruceWood[k].reset();

        mLengthSmoothers[k].reset(static_cast<float>(mNominalLengths[k]));
        mSpatialWeightsL[k].reset(mSpatialWeightsL[k].getTarget());
        mSpatialWeightsR[k].reset(mSpatialWeightsR[k].getTarget());
    }
    mCausticRotationAngle = 0.0f;
}

void ManifoldDelayNetwork::setParameters(ManifoldType type, float roomSize, float highDampingHz, float diffusionDensity) noexcept {
    const float clampedRoom = std::clamp(roomSize, 0.05f, mMaxRoomSize);
    const float clampedDiff = std::clamp(diffusionDensity, 0.0f, 1.0f);
    const bool manifoldChanged = (type != mCurrentManifold);
    const bool roomChanged = std::abs(clampedRoom - mRoomSize) > 0.001f;
    const bool dampChanged = std::abs(highDampingHz - mHighDampingHz) > 5.0f;
    const bool diffChanged = std::abs(clampedDiff - mDiffusionDensity) > 0.005f;

    mCurrentManifold = type;
    mRoomSize = clampedRoom;
    mHighDampingHz = highDampingHz;
    mDiffusionDensity = clampedDiff;

    if (manifoldChanged || roomChanged) {
        updateManifoldGeometry();
        updateSpatialWeights();
    }
    if (manifoldChanged || dampChanged || diffChanged) {
        updateFilterCoefficients();
    }

    if (mIsFirstSet) {
        for (size_t k = 0; k < kNumLines; ++k) {
            mLengthSmoothers[k].reset(static_cast<float>(mNominalLengths[k]));
            mSpatialWeightsL[k].reset(mSpatialWeightsL[k].getTarget());
            mSpatialWeightsR[k].reset(mSpatialWeightsR[k].getTarget());
        }
        mIsFirstSet = false;
    }
}

void ManifoldDelayNetwork::setParameters(ManifoldType type, float roomSize, float highDampingHz) noexcept {
    setParameters(type, roomSize, highDampingHz, mDiffusionDensity);
}

void ManifoldDelayNetwork::setManifold(ManifoldType type) noexcept {
    setParameters(type, mRoomSize, mHighDampingHz, mDiffusionDensity);
}

void ManifoldDelayNetwork::computePoincareLengths(std::array<size_t, kNumLines>& lengths) const noexcept {
    // Horocycle delays: L_k = round(L_0 * cosh(xi * k / 7))
    // Nominal L_0 = 1000 at 48 kHz, roomSize = 1.0; xi = 1.760742
    const double rateScale = mSampleRate / 48000.0;
    const double safeRoom = std::clamp(static_cast<double>(mRoomSize), 0.05, static_cast<double>(mMaxRoomSize));
    const double l0 = 1000.0 * rateScale * safeRoom;

    for (size_t k = 0; k < kNumLines; ++k) {
        const double raw = l0 * kPoincareCosh[k];
        const size_t len = static_cast<size_t>(std::round(raw));
        lengths[k] = std::clamp(len, size_t{64}, kBufferCapacity - 4096);
    }
}

void ManifoldDelayNetwork::computeWhisperingLengths(std::array<size_t, kNumLines>& lengths) const noexcept {
    // Airy radial delay modes: L_k = round(L_ring * (1 - a_{k+1} / (2*pi*(k+3)))) + primeOffset
    const double rateScale = mSampleRate / 48000.0;
    const double safeRoom = std::clamp(static_cast<double>(mRoomSize), 0.05, static_cast<double>(mMaxRoomSize));
    const double lRing = 2800.0 * rateScale * safeRoom;

    for (size_t k = 0; k < kNumLines; ++k) {
        const double raw = lRing * kWhisperingRadialFactors[k];
        const size_t len = static_cast<size_t>(std::round(raw)) + kWhisperingPrimeOffsets[k];
        lengths[k] = std::clamp(len, size_t{64}, kBufferCapacity - 4096);
    }
}

void ManifoldDelayNetwork::computePlateLengths(std::array<size_t, kNumLines>& lengths) const noexcept {
    // Biharmonic plate dispersion modes: L_k = round(L_base / sqrt(m^2 + 0.08 * n^2)) + primeOffset
    const double rateScale = mSampleRate / 48000.0;
    const double safeRoom = std::clamp(static_cast<double>(mRoomSize), 0.05, static_cast<double>(mMaxRoomSize));
    const double lBase = 3400.0 * rateScale * safeRoom;

    for (size_t k = 0; k < kNumLines; ++k) {
        const double raw = lBase * kPlateInvLambda[k];
        const size_t len = static_cast<size_t>(std::round(raw)) + kPlatePrimeOffsets[k];
        lengths[k] = std::clamp(len, size_t{64}, kBufferCapacity - 4096);
    }
}

void ManifoldDelayNetwork::computeKlangdomLengths(std::array<size_t, kNumLines>& lengths) const noexcept {
    // Spherical dome antipodal focus: L_k = round(L_dome * (1 + delta_k))
    const double rateScale = mSampleRate / 48000.0;
    const double safeRoom = std::clamp(static_cast<double>(mRoomSize), 0.05, static_cast<double>(mMaxRoomSize));
    const double lDome = 2400.0 * rateScale * safeRoom;

    for (size_t k = 0; k < kNumLines; ++k) {
        const double raw = lDome * kKlangdomScaleFactors[k];
        const size_t len = static_cast<size_t>(std::round(raw));
        lengths[k] = std::clamp(len, size_t{64}, kBufferCapacity - 4096);
    }
}

void ManifoldDelayNetwork::updateManifoldGeometry() noexcept {
    switch (mCurrentManifold) {
        case ManifoldType::PoincareHyperbolic:
            computePoincareLengths(mNominalLengths);
            break;
        case ManifoldType::WhisperingGallery:
            computeWhisperingLengths(mNominalLengths);
            break;
        case ManifoldType::AnharmonicPlate:
            computePlateLengths(mNominalLengths);
            break;
        case ManifoldType::StockhausenKlangdom:
            computeKlangdomLengths(mNominalLengths);
            break;
    }

    for (size_t k = 0; k < kNumLines; ++k) {
        mLengthSmoothers[k].setTarget(static_cast<float>(mNominalLengths[k]));
    }
}

void ManifoldDelayNetwork::updateFilterCoefficients() noexcept {
    const float fs = static_cast<float>(mSampleRate);

    // 1. One-pole HF air damping
    const float fcDamp = std::clamp(mHighDampingHz, 500.0f, fs * 0.49f);
    mDampingAlpha = 1.0f - std::exp(-kTwoPi * (fcDamp / fs));

    // 2. Dispersion Allpass Configuration (Loop Decay Diffusion)
    float baseA1 = 0.0f, baseA2 = 0.0f;
    if (mCurrentManifold == ManifoldType::PoincareHyperbolic) {
        baseA1 = -0.45f; // Negative curvature dispersion: low frequencies lead
        baseA2 = 0.0f;
    } else if (mCurrentManifold == ManifoldType::AnharmonicPlate) {
        baseA1 = +0.55f; // Plate biharmonic dispersion: high frequencies lead
        baseA2 = +0.55f; // 2 cascaded stages
    } else if (mCurrentManifold == ManifoldType::StockhausenKlangdom) {
        baseA1 = +0.40f;
        baseA2 = -0.30f;
    } else if (mCurrentManifold == ManifoldType::WhisperingGallery) {
        baseA1 = +0.35f;
        baseA2 = 0.0f;
    }

    mDispCoeff1 = baseA1 * mDiffusionDensity;
    mDispCoeff2 = baseA2 * mDiffusionDensity;

    for (size_t k = 0; k < kNumLines; ++k) {
        mDispersionStage1[k].setCoeff(mDispCoeff1);
        mDispersionStage2[k].setCoeff(mDispCoeff2);
    }

    // 3. Whispering Gallery Caustic Peaking (+3.5 dB at 9.5 kHz, Q = 2.8) + ultrasonic lowpass
    for (size_t k = 0; k < kNumLines; ++k) {
        mCausticPeaking[k].configure(BiquadDirectForm2T::Type::Peaking, fs, 9500.0f, 2.8f, +3.5f);
        mUltrasonicLowpass[k].setCutoff(fs, std::min(18000.0f, fs * 0.45f));
    }

    // 4. Anharmonic Plate Sitka Spruce Formants (A0: 95 Hz, T1: 320 Hz, Wood: 2400 Hz)
    for (size_t k = 0; k < kNumLines; ++k) {
        mSpruceA0[k].configure(BiquadDirectForm2T::Type::Peaking, fs, 95.0f, 3.2f, +4.0f);
        mSpruceT1[k].configure(BiquadDirectForm2T::Type::Peaking, fs, 320.0f, 2.5f, +5.5f);
        mSpruceWood[k].configure(BiquadDirectForm2T::Type::Peaking, fs, 2400.0f, 1.8f, +3.0f);
    }
}

void ManifoldDelayNetwork::updateSpatialWeights() noexcept {
    switch (mCurrentManifold) {
        case ManifoldType::PoincareHyperbolic:
            // Balanced hyperbolic extraction matching original FDN extraction
            mSpatialWeightsL[0].setTarget(+0.25f); mSpatialWeightsR[0].setTarget( 0.00f);
            mSpatialWeightsL[1].setTarget( 0.00f); mSpatialWeightsR[1].setTarget(+0.25f);
            mSpatialWeightsL[2].setTarget(+0.25f); mSpatialWeightsR[2].setTarget( 0.00f);
            mSpatialWeightsL[3].setTarget( 0.00f); mSpatialWeightsR[3].setTarget(-0.25f);
            mSpatialWeightsL[4].setTarget(-0.25f); mSpatialWeightsR[4].setTarget( 0.00f);
            mSpatialWeightsL[5].setTarget( 0.00f); mSpatialWeightsR[5].setTarget(+0.25f);
            mSpatialWeightsL[6].setTarget(-0.25f); mSpatialWeightsR[6].setTarget( 0.00f);
            mSpatialWeightsL[7].setTarget( 0.00f); mSpatialWeightsR[7].setTarget(-0.25f);
            break;

        case ManifoldType::WhisperingGallery:
            // Default target for Whispering Gallery (runtime uses rotating circular spatial vector)
            for (size_t k = 0; k < kNumLines; ++k) {
                mSpatialWeightsL[k].setTarget(0.35355339f);
                mSpatialWeightsR[k].setTarget(0.35355339f);
            }
            break;

        case ManifoldType::AnharmonicPlate: {
            // Virtual contact pickups at (0.33, 0.42) and (0.67, 0.58)
            static constexpr std::array<float, 8> wL = {{
                +0.208f, +0.105f, +0.211f, +0.107f, +0.023f, +0.012f, -0.016f, -0.204f
            }};
            static constexpr std::array<float, 8> wR = {{
                +0.208f, -0.105f, -0.211f, +0.107f, +0.023f, -0.012f, -0.016f, +0.204f
            }};
            for (size_t k = 0; k < kNumLines; ++k) {
                mSpatialWeightsL[k].setTarget(wL[k]);
                mSpatialWeightsR[k].setTarget(wR[k]);
            }
            break;
        }

        case ManifoldType::StockhausenKlangdom: {
            // 8 spatial diffusion vectors on unit cube vertices (1/sqrt(3))(+/-1, +/-1, +/-1)
            // Normalized projection to stereo with sum(C_L^2 + C_R^2) == 1.0
            static constexpr std::array<float, 8> klangL = {{
                +0.13530006f, -0.13530006f, +0.32663844f, +0.32663844f,
                +0.13530006f, -0.13530006f, +0.32663844f, +0.32663844f
            }};
            static constexpr std::array<float, 8> klangR = {{
                +0.32663844f, +0.32663844f, -0.13530006f, +0.13530006f,
                +0.32663844f, +0.32663844f, -0.13530006f, +0.13530006f
            }};
            for (size_t k = 0; k < kNumLines; ++k) {
                mSpatialWeightsL[k].setTarget(klangL[k]);
                mSpatialWeightsR[k].setTarget(klangR[k]);
            }
            break;
        }
    }
}

void ManifoldDelayNetwork::readAndFilterLines(const std::array<float, kNumLines>& inExcursions,
                                            std::array<float, kNumLines>& outFiltered,
                                            float freezeAmount) noexcept {
    const float freeze = std::clamp(freezeAmount, 0.0f, 1.0f);
    for (size_t k = 0; k < kNumLines; ++k) {
        // 1. Slewed nominal delay length + dynamic tail modulation excursion
        // When freeze is engaged, eliminate modulation excursion and snap to integer sample read
        // to prevent fractional Hermite interpolation lowpass filtering energy dissipation
        const float curLen = mLengthSmoothers[k].next();
        const float effExcursion = inExcursions[k] * (1.0f - freeze);
        const float totalDelay = (freeze >= 0.999f)
            ? std::round(curLen)
            : std::clamp(curLen + effExcursion, 16.0f, static_cast<float>(kBufferCapacity - 128));

        // 2. Fractional Hermite cubic spline read
        const float rawSample = TailModulator::readHermite(mBuffers[k].data(),
                                                           kBufferCapacity,
                                                           kBufferMask,
                                                           mWriteIndices[k],
                                                           totalDelay);

        // 3. One-pole air absorption lowpass filter (bypassed when freezeAmount == 1.0f)
        const float filtered = flushDenormal(mDampingStates[k] + mDampingAlpha * (rawSample - mDampingStates[k]));
        mDampingStates[k] = (freeze >= 0.999f) ? rawSample : filtered;
        float s = (1.0f - freeze) * filtered + freeze * rawSample;

        // 4. Dispersion allpasses (Loop decay diffusion across delay lines)
        if (std::abs(mDispCoeff1) > 1.0e-4f) {
            s = mDispersionStage1[k].process(s);
        }
        if (std::abs(mDispCoeff2) > 1.0e-4f) {
            s = mDispersionStage2[k].process(s);
        }

        // 5. Manifold-specific resonant loop filtering
        if (mCurrentManifold == ManifoldType::WhisperingGallery) {
            // High-frequency caustic peaking filter (+3.5 dB at 9.5 kHz) + ultrasonic lowpass
            // Bypassed during freeze hold to prevent runaway caustic resonance
            if (freeze < 0.999f) {
                const float pf = mUltrasonicLowpass[k].process(mCausticPeaking[k].process(s));
                s = (1.0f - freeze) * pf + freeze * s;
            }
        } else if (mCurrentManifold == ManifoldType::AnharmonicPlate) {
            // Sitka spruce body formants (A0, T1, Wood fiber) with -3 dB loop trim
            // Formants and trim are bypassed during freeze hold so held reverb does not decay/mute
            if (freeze < 0.999f) {
                const float plateFiltered = mSpruceWood[k].process(mSpruceT1[k].process(mSpruceA0[k].process(s))) * 0.70794578f;
                s = (1.0f - freeze) * plateFiltered + freeze * s;
            }
        }

        outFiltered[k] = flushDenormal(s);
    }
}

void ManifoldDelayNetwork::writeFeedback(const std::array<float, kNumLines>& inSaturated) noexcept {
    for (size_t k = 0; k < kNumLines; ++k) {
        mBuffers[k][mWriteIndices[k]] = flushDenormal(inSaturated[k]);
        mWriteIndices[k] = (mWriteIndices[k] + 1) & kBufferMask;
    }
}

void ManifoldDelayNetwork::extractStereo(const std::array<float, kNumLines>& lines,
                                        float& outLateL, float& outLateR) noexcept {
    if (mCurrentManifold == ManifoldType::WhisperingGallery) {
        // Continuous circular spatial rotation at Omega_rot = 0.25 Hz
        mCausticRotationAngle += mCausticRotationDelta;
        if (mCausticRotationAngle >= kTwoPi) {
            mCausticRotationAngle -= kTwoPi;
        }

        float accL = 0.0f;
        float accR = 0.0f;
        static constexpr float kNorm = 0.35355339f; // 1 / sqrt(8)

        for (size_t k = 0; k < kNumLines; ++k) {
            const float theta = (kTwoPi * static_cast<float>(k) / 8.0f) + mCausticRotationAngle;
            const float halfTheta = theta * 0.5f;
            const float panL = kNorm * FastSinTable::cos(halfTheta);
            const float panR = kNorm * FastSinTable::sin(halfTheta);
            accL += panL * lines[k];
            accR += panR * lines[k];
        }

        outLateL = flushDenormal(accL);
        outLateR = flushDenormal(accR);
    } else {
        // Smoothly slewed spatial extraction matrix
        float accL = 0.0f;
        float accR = 0.0f;

        for (size_t k = 0; k < kNumLines; ++k) {
            const float wL = mSpatialWeightsL[k].next();
            const float wR = mSpatialWeightsR[k].next();
            accL += wL * lines[k];
            accR += wR * lines[k];
        }

        outLateL = flushDenormal(accL);
        outLateR = flushDenormal(accR);
    }
}

} // namespace rb26
