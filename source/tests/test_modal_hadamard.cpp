#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <array>
#include <algorithm>
#include "DspMath.h"
#include "LowBandModalMatrix.h"

namespace test {

constexpr double kPi = 3.14159265358979323846;

class ModalSimulator {
public:
    enum class OutputMode {
        PairwiseOld,
        HadamardNormalized
    };

    void prepare(double sampleRate) {
        mSampleRate = static_cast<float>(sampleRate);
        mCrossover.prepare(sampleRate);
        mPunchDetector.prepare(sampleRate);
        mEllipticalFilter.prepare(sampleRate);

        for (size_t i = 0; i < 4; ++i) {
            const size_t nominal = static_cast<size_t>(std::round(kBaseDelayTimes[i] * mSampleRate));
            mDelayLengths[i] = findClosestPrime(nominal);
            mDelayBuffers[i].assign(mDelayLengths[i] + 64, 0.0f);
            mWriteIndices[i] = 0;
        }

        updateDecayCoefficients(3.5f, 1.0f);
        reset();
    }

    void reset() {
        mCrossover.reset();
        mPunchDetector.reset();
        mEllipticalFilter.reset();
        for (size_t i = 0; i < 4; ++i) {
            std::fill(mDelayBuffers[i].begin(), mDelayBuffers[i].end(), 0.0f);
            mWriteIndices[i] = 0;
        }
    }

    void process(float inL, float inR, float& outL, float& outR, OutputMode mode) {
        // 1. Punch detector
        const float duckGain = mPunchDetector.process(inL, inR, 0.0f);
        const float duckedL = inL * duckGain;
        const float duckedR = inR * duckGain;

        // 2. Read delay lines
        std::array<float, 4> w {};
        for (size_t i = 0; i < 4; ++i) {
            w[i] = mDelayBuffers[i][mWriteIndices[i]];
        }

        // 3. Householder reflection
        const float sum = w[0] + w[1] + w[2] + w[3];
        const float halfSum = sum * 0.5f;
        std::array<float, 4> v {};
        for (size_t i = 0; i < 4; ++i) {
            v[i] = w[i] - halfSum;
        }

        // 4. Input injection
        const float midIn  = 0.5f * (duckedL + duckedR);
        const float sideIn = 0.5f * (duckedL - duckedR);
        std::array<float, 4> injection { duckedL, duckedR, midIn, sideIn };

        // 5. Feedback recirculation
        for (size_t i = 0; i < 4; ++i) {
            const float feedback = v[i] * mDecayCoeffs[i];
            const float nextIn = feedback + injection[i];
            const float saturated = rb26::applySmoothBoundaryKnee(nextIn, 0.72f);
            mDelayBuffers[i][mWriteIndices[i]] = rb26::flushDenormal(saturated);
            mWriteIndices[i] = (mWriteIndices[i] + 1) % mDelayLengths[i];
        }

        // 6. Output Extraction
        float rawL = 0.0f;
        float rawR = 0.0f;
        if (mode == OutputMode::PairwiseOld) {
            rawL = 0.5f * (w[0] + w[2]);
            rawR = 0.5f * (w[1] + w[3]);
        } else {
            // Evaluated Hadamard configuration
            rawL = 0.5f * (mRowL[0]*w[0] + mRowL[1]*w[1] + mRowL[2]*w[2] + mRowL[3]*w[3]);
            rawR = 0.5f * (mRowR[0]*w[0] + mRowR[1]*w[1] + mRowR[2]*w[2] + mRowR[3]*w[3]);
        }

        // 7. Elliptical filter
        mEllipticalFilter.process(rawL, rawR, outL, outR);
    }

    void setRows(std::array<float, 4> rowL, std::array<float, 4> rowR) {
        mRowL = rowL;
        mRowR = rowR;
    }

private:
    std::array<float, 4> mRowL { 1.0f, 1.0f, -1.0f, -1.0f };
    std::array<float, 4> mRowR { 1.0f, -1.0f, 1.0f, -1.0f };

private:
    static bool isPrime(size_t n) noexcept {
        if (n <= 1) return false;
        if (n <= 3) return true;
        if (n % 2 == 0 || n % 3 == 0) return false;
        for (size_t i = 5; i * i <= n; i += 6) {
            if (n % i == 0 || n % (i + 2) == 0) return false;
        }
        return true;
    }

    static size_t findClosestPrime(size_t target) noexcept {
        if (isPrime(target)) return target;
        size_t offset = 1;
        while (true) {
            if (target >= offset && isPrime(target - offset)) return target - offset;
            if (isPrime(target + offset)) return target + offset;
            ++offset;
        }
    }

    void updateDecayCoefficients(float rt60, float mult) {
        const float effRt60 = std::clamp(rt60 * mult, 0.05f, 120.0f);
        for (size_t i = 0; i < 4; ++i) {
            const float tSec = static_cast<float>(mDelayLengths[i]) / mSampleRate;
            mDecayCoeffs[i] = std::exp(-6.907755278982137f * tSec / effRt60);
        }
    }

    float mSampleRate { 48000.0f };
    rb26::LinkwitzRiley4 mCrossover;
    rb26::TransientPunchDetector mPunchDetector;
    rb26::SubBassEllipticalFilter mEllipticalFilter;

    static constexpr std::array<float, 4> kBaseDelayTimes = { 0.0710f, 0.0860f, 0.1040f, 0.1261f };
    std::array<size_t, 4> mDelayLengths {};
    std::array<size_t, 4> mWriteIndices {};
    std::array<std::vector<float>, 4> mDelayBuffers;
    std::array<float, 4> mDecayCoeffs {};
};

struct SweepResult {
    double maxAdjacentNotchDb { 0.0 };
    int maxNotchFreq { 0 };
    double minRmsDb { 0.0 };
    int minRmsFreq { 0 };
    double maxRmsDb { 0.0 };
    int maxRmsFreq { 0 };
    double rmsAt77Hz { 0.0 };
    double rmsAt143Hz { 0.0 };
    double rmsAt148Hz { 0.0 };
};

SweepResult runSweep(ModalSimulator::OutputMode mode) {
    const double fs = 48000.0;
    ModalSimulator sim;
    sim.prepare(fs);

    std::vector<double> freqs;
    std::vector<double> rmsValues;

    for (int f = 40; f <= 200; ++f) {
        freqs.push_back(static_cast<double>(f));
    }

    const size_t testLength = 24000;
    const size_t steadyStart = 9600;

    SweepResult res;
    double minRms = 1e9;
    double maxRms = -1e9;

    for (size_t fi = 0; fi < freqs.size(); ++fi) {
        const double f = freqs[fi];
        sim.reset();
        double sumSq = 0.0;
        size_t count = 0;

        for (size_t n = 0; n < testLength; ++n) {
            float in = static_cast<float>(std::sin(2.0 * kPi * f * n / fs));
            float outL = 0.0f, outR = 0.0f;
            sim.process(in, in, outL, outR, mode);

            if (n >= steadyStart) {
                sumSq += 0.5 * (outL * outL + outR * outR);
                ++count;
            }
        }

        double rms = std::sqrt(sumSq / count);
        rmsValues.push_back(rms);

        if (rms < minRms) {
            minRms = rms;
            res.minRmsFreq = static_cast<int>(f);
        }
        if (rms > maxRms) {
            maxRms = rms;
            res.maxRmsFreq = static_cast<int>(f);
        }

        if (static_cast<int>(f) == 77) res.rmsAt77Hz = 20.0 * std::log10(rms);
        if (static_cast<int>(f) == 143) res.rmsAt143Hz = 20.0 * std::log10(rms);
        if (static_cast<int>(f) == 148) res.rmsAt148Hz = 20.0 * std::log10(rms);
    }

    res.minRmsDb = 20.0 * std::log10(minRms);
    res.maxRmsDb = 20.0 * std::log10(maxRms);

    // Adjacent notch depth measurement (as defined in Challenger 2 test suite)
    for (size_t i = 1; i < rmsValues.size() - 1; ++i) {
        double localMax = std::max(rmsValues[i - 1], rmsValues[i + 1]);
        double current = rmsValues[i];
        if (current > 1.0e-8 && localMax > 1.0e-8) {
            double dropDb = 20.0 * std::log10(localMax / current);
            if (dropDb > res.maxAdjacentNotchDb) {
                res.maxAdjacentNotchDb = dropDb;
                res.maxNotchFreq = static_cast<int>(freqs[i]);
            }
        }
    }

    return res;
}

} // namespace test

int main() {
    std::cout << "=================================================================\n";
    std::cout << "  EXHAUSTIVE SEARCH: ALL 96 ORTHOGONAL HADAMARD PAIRS (40-200 Hz)\n";
    std::cout << "=================================================================\n\n";

    // Generate all 16 vectors of length 4 with entries in {-1, +1}
    std::vector<std::array<float, 4>> allVectors;
    for (int mask = 0; mask < 16; ++mask) {
        std::array<float, 4> v;
        for (int b = 0; b < 4; ++b) {
            v[b] = (mask & (1 << b)) ? 1.0f : -1.0f;
        }
        allVectors.push_back(v);
    }

    // Find all orthogonal pairs (dot product == 0)
    struct Candidate {
        std::array<float, 4> rowL;
        std::array<float, 4> rowR;
        test::SweepResult res;
    };

    std::vector<Candidate> candidates;

    for (size_t i = 0; i < allVectors.size(); ++i) {
        for (size_t j = i + 1; j < allVectors.size(); ++j) {
            float dot = 0.0f;
            for (int k = 0; k < 4; ++k) dot += allVectors[i][k] * allVectors[j][k];
            if (std::abs(dot) < 1e-4f) {
                // Test this pair
                test::ModalSimulator sim;
                sim.prepare(48000.0);
                sim.setRows(allVectors[i], allVectors[j]);

                Candidate cand;
                cand.rowL = allVectors[i];
                cand.rowR = allVectors[j];

                // Run sweep
                std::vector<double> rmsValues;
                double minRms = 1e9, maxRms = -1e9;

                for (int f = 40; f <= 200; ++f) {
                    sim.reset();
                    double sumSq = 0.0;
                    size_t count = 0;
                    for (size_t n = 0; n < 24000; ++n) {
                        float in = static_cast<float>(std::sin(2.0 * test::kPi * f * n / 48000.0));
                        float outL = 0.0f, outR = 0.0f;
                        sim.process(in, in, outL, outR, test::ModalSimulator::OutputMode::HadamardNormalized);
                        if (n >= 9600) {
                            sumSq += 0.5 * (outL * outL + outR * outR);
                            ++count;
                        }
                    }
                    double rms = std::sqrt(sumSq / count);
                    rmsValues.push_back(rms);
                    if (rms < minRms) { minRms = rms; cand.res.minRmsFreq = f; }
                    if (rms > maxRms) { maxRms = rms; cand.res.maxRmsFreq = f; }
                    if (f == 77) cand.res.rmsAt77Hz = 20.0 * std::log10(rms);
                    if (f == 143) cand.res.rmsAt143Hz = 20.0 * std::log10(rms);
                    if (f == 148) cand.res.rmsAt148Hz = 20.0 * std::log10(rms);
                }

                cand.res.minRmsDb = 20.0 * std::log10(minRms);
                cand.res.maxRmsDb = 20.0 * std::log10(maxRms);

                for (size_t k = 1; k < rmsValues.size() - 1; ++k) {
                    double localMax = std::max(rmsValues[k - 1], rmsValues[k + 1]);
                    double current = rmsValues[k];
                    if (current > 1.0e-8 && localMax > 1.0e-8) {
                        double dropDb = 20.0 * std::log10(localMax / current);
                        if (dropDb > cand.res.maxAdjacentNotchDb) {
                            cand.res.maxAdjacentNotchDb = dropDb;
                            cand.res.maxNotchFreq = 40 + static_cast<int>(k);
                        }
                    }
                }

                candidates.push_back(cand);
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.res.maxAdjacentNotchDb < b.res.maxAdjacentNotchDb;
    });

    std::cout << "Top 10 Orthogonal Hadamard Pairs (ranked by lowest Max Adjacent Notch):\n";
    std::cout << "----------------------------------------------------------------------------------------------------\n";
    std::cout << "Rank  RowL               RowR               MaxNotch (dB) @ Freq   Ripple (dB)   77Hz   143Hz  148Hz\n";
    std::cout << "----------------------------------------------------------------------------------------------------\n";
    for (size_t r = 0; r < std::min(size_t{10}, candidates.size()); ++r) {
        const auto& c = candidates[r];
        std::cout << "[" << std::setw(2) << (r + 1) << "]  [";
        for (int k = 0; k < 4; ++k) std::cout << (c.rowL[k] > 0 ? "+1 " : "-1 ");
        std::cout << "]  [";
        for (int k = 0; k < 4; ++k) std::cout << (c.rowR[k] > 0 ? "+1 " : "-1 ");
        std::cout << "]  " << std::setw(6) << c.res.maxAdjacentNotchDb << " dB @" << std::setw(4) << c.res.maxNotchFreq << " Hz  "
                  << std::setw(6) << (c.res.maxRmsDb - c.res.minRmsDb) << " dB  "
                  << std::setw(6) << c.res.rmsAt77Hz << " "
                  << std::setw(6) << c.res.rmsAt143Hz << " "
                  << std::setw(6) << c.res.rmsAt148Hz << "\n";
    }
    std::cout << "----------------------------------------------------------------------------------------------------\n";

    std::cout << "\n--- DIAGNOSTIC: Individual Delay Lines w0..w3 at 53 Hz ---\n";
    {
        test::ModalSimulator sim;
        sim.prepare(48000.0);
        float in = 0.0f;
        // Run to steady state
        for (size_t n = 0; n < 24000; ++n) {
            in = static_cast<float>(std::sin(2.0 * test::kPi * 53.0 * n / 48000.0));
            float outL = 0.0f, outR = 0.0f;
            sim.process(in, in, outL, outR, test::ModalSimulator::OutputMode::HadamardNormalized);
        }
        // Sample next cycle
        double rmsW[4] = {0, 0, 0, 0};
        double sumL = 0, sumR = 0;
        for (size_t n = 0; n < 4800; ++n) {
            in = static_cast<float>(std::sin(2.0 * test::kPi * 53.0 * n / 48000.0));
            float outL = 0.0f, outR = 0.0f;
            sim.process(in, in, outL, outR, test::ModalSimulator::OutputMode::HadamardNormalized);
            sumL += outL * outL;
            sumR += outR * outR;
        }
        std::cout << "outL RMS = " << std::sqrt(sumL / 4800) << " (" << 20.0 * std::log10(std::sqrt(sumL / 4800)) << " dB)\n";
        std::cout << "outR RMS = " << std::sqrt(sumR / 4800) << " (" << 20.0 * std::log10(std::sqrt(sumR / 4800)) << " dB)\n";
    }

    if (!candidates.empty()) {
        const auto& best = candidates[0];
        std::cout << "\nBEST ORTHOGONAL HADAMARD CONFIGURATION:\n";
        std::cout << "RowL: [";
        for (int k = 0; k < 4; ++k) std::cout << (best.rowL[k] > 0 ? "+1 " : "-1 ");
        std::cout << "]\nRowR: [";
        for (int k = 0; k < 4; ++k) std::cout << (best.rowR[k] > 0 ? "+1 " : "-1 ");
        std::cout << "]\nMax Adjacent Notch: " << best.res.maxAdjacentNotchDb << " dB (@ " << best.res.maxNotchFreq << " Hz)\n";
        std::cout << "Dynamic Ripple: " << (best.res.maxRmsDb - best.res.minRmsDb) << " dB\n";
        std::cout << "RMS @ 77 Hz: " << best.res.rmsAt77Hz << " dB\n";
        std::cout << "RMS @ 143 Hz: " << best.res.rmsAt143Hz << " dB\n";
        std::cout << "RMS @ 148 Hz: " << best.res.rmsAt148Hz << " dB\n";

        if (best.res.maxAdjacentNotchDb < 3.0) {
            std::cout << "\nRESULT: PASS! Best configuration achieves " << best.res.maxAdjacentNotchDb << " dB (< 3.0 dB threshold)!\n";
        } else {
            std::cout << "\nRESULT: Best configuration achieves " << best.res.maxAdjacentNotchDb << " dB\n";
        }
    }

    return 0;
}
