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

class ModalTestBench {
public:
    enum class InjectionMode {
        Original,           // [L, R, Mid, Side]
        BalancedHadamard    // 0.5 * Hadamard input
    };

    enum class OutputMode {
        PairwiseOld,        // 0.5*(w0+w2), 0.5*(w1+w3)
        HadamardDispatch,   // 0.5*(w0+w1-w2-w3), 0.5*(w0-w1+w2-w3)
        HadamardAlternative // custom rows
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

    void setEllipticalCutoff(float fc) {
        mEllipticalFilter.setCutoff(fc);
    }

    void process(float inL, float inR, float& outL, float& outR,
                 InjectionMode inMode, OutputMode outMode, bool bypassElliptical = false) {
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
        std::array<float, 4> injection {};
        if (inMode == InjectionMode::Original) {
            const float midIn  = 0.5f * (duckedL + duckedR);
            const float sideIn = 0.5f * (duckedL - duckedR);
            injection[0] = duckedL;
            injection[1] = duckedR;
            injection[2] = midIn;
            injection[3] = sideIn;
        } else {
            // Balanced input distribution: all 4 delay lines receive energy on mono inputs
            // Injection matrix B = 0.5 * [ [1, 1], [1, -1], [1, 1], [1, -1] ]
            const float midIn  = 0.5f * (duckedL + duckedR);
            const float sideIn = 0.5f * (duckedL - duckedR);
            injection[0] = midIn + 0.5f * sideIn;
            injection[1] = midIn - 0.5f * sideIn;
            injection[2] = midIn + 0.5f * sideIn;
            injection[3] = midIn - 0.5f * sideIn;
        }

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
        if (outMode == OutputMode::PairwiseOld) {
            rawL = 0.5f * (w[0] + w[2]);
            rawR = 0.5f * (w[1] + w[3]);
        } else if (outMode == OutputMode::HadamardDispatch) {
            rawL = 0.5f * (w[0] + w[1] - w[2] - w[3]);
            rawR = 0.5f * (w[0] - w[1] + w[2] - w[3]);
        } else {
            // Balanced Hadamard row pair
            rawL = 0.5f * (w[0] + w[1] + w[2] + w[3]);
            rawR = 0.5f * (w[0] - w[1] + w[2] - w[3]);
        }

        // 7. Elliptical filter
        if (bypassElliptical) {
            outL = rawL;
            outR = rawR;
        } else {
            mEllipticalFilter.process(rawL, rawR, outL, outR);
        }
    }

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

void evaluateConfiguration(const std::string& name,
                           ModalTestBench::InjectionMode inMode,
                           ModalTestBench::OutputMode outMode,
                           bool bypassElliptical,
                           float ellipticalFc = 120.0f) {
    const double fs = 48000.0;
    ModalTestBench bench;
    bench.prepare(fs);
    bench.setEllipticalCutoff(ellipticalFc);

    std::vector<double> freqs;
    std::vector<double> rmsValues;
    for (int f = 40; f <= 200; ++f) freqs.push_back(static_cast<double>(f));

    const size_t testLength = 24000;
    const size_t steadyStart = 9600;

    double minRms = 1e9, maxRms = -1e9;
    int minF = 0, maxF = 0;
    double rms77 = 0.0, rms143 = 0.0, rms148 = 0.0;

    for (size_t fi = 0; fi < freqs.size(); ++fi) {
        const double f = freqs[fi];
        bench.reset();
        double sumSq = 0.0;
        size_t count = 0;

        for (size_t n = 0; n < testLength; ++n) {
            float in = static_cast<float>(std::sin(2.0 * kPi * f * n / fs));
            float outL = 0.0f, outR = 0.0f;
            bench.process(in, in, outL, outR, inMode, outMode, bypassElliptical);
            if (n >= steadyStart) {
                sumSq += 0.5 * (outL * outL + outR * outR);
                ++count;
            }
        }

        double rms = std::sqrt(sumSq / count);
        rmsValues.push_back(rms);
        if (rms < minRms) { minRms = rms; minF = static_cast<int>(f); }
        if (rms > maxRms) { maxRms = rms; maxF = static_cast<int>(f); }
        if (static_cast<int>(f) == 77) rms77 = 20.0 * std::log10(rms);
        if (static_cast<int>(f) == 143) rms143 = 20.0 * std::log10(rms);
        if (static_cast<int>(f) == 148) rms148 = 20.0 * std::log10(rms);
    }

    double maxNotch = 0.0;
    int notchF = 0;
    for (size_t i = 1; i < rmsValues.size() - 1; ++i) {
        double localMax = std::max(rmsValues[i - 1], rmsValues[i + 1]);
        double current = rmsValues[i];
        if (current > 1.0e-8 && localMax > 1.0e-8) {
            double dropDb = 20.0 * std::log10(localMax / current);
            if (dropDb > maxNotch) {
                maxNotch = dropDb;
                notchF = static_cast<int>(freqs[i]);
            }
        }
    }

    double minDb = 20.0 * std::log10(minRms);
    double maxDb = 20.0 * std::log10(maxRms);

    std::cout << "Configuration: " << name << "\n";
    std::cout << "  Max Adjacent Notch : " << std::fixed << std::setprecision(2) << maxNotch << " dB (@ " << notchF << " Hz)\n";
    std::cout << "  Dynamic Range (Rip): " << (maxDb - minDb) << " dB (Min: " << minDb << " dB @ " << minF << " Hz, Max: " << maxDb << " dB @ " << maxF << " Hz)\n";
    std::cout << "  RMS @ 77 Hz        : " << rms77 << " dB\n";
    std::cout << "  RMS @ 143 Hz       : " << rms143 << " dB\n";
    std::cout << "  RMS @ 148 Hz       : " << rms148 << " dB\n";
    std::cout << "  143-148 Hz Delta   : " << (rms148 - rms143) << " dB\n";
    std::cout << "  Verdict (<6dB Pass): " << (maxNotch < 6.0 ? "PASS" : "FAIL") << "\n\n";
}

} // namespace test

int main() {
    std::cout << "=================================================================\n";
    std::cout << "  PARAMETRIC MODAL ANALYSIS & REMEDIATION STRATEGY TEST\n";
    std::cout << "=================================================================\n\n";

    // 1. Original baseline
    test::evaluateConfiguration("1. Baseline Pairwise Old (w0+w2, w1+w3)",
                                test::ModalTestBench::InjectionMode::Original,
                                test::ModalTestBench::OutputMode::PairwiseOld,
                                false, 120.0f);

    // 2. Dispatch Hadamard with original injection & 120 Hz elliptical
    test::evaluateConfiguration("2. Dispatch Hadamard (w0+w1-w2-w3, w0-w1+w2-w3), Elliptical 120Hz",
                                test::ModalTestBench::InjectionMode::Original,
                                test::ModalTestBench::OutputMode::HadamardDispatch,
                                false, 120.0f);

    // 3. Dispatch Hadamard bypassing elliptical filter
    test::evaluateConfiguration("3. Dispatch Hadamard, Bypassed Elliptical Filter",
                                test::ModalTestBench::InjectionMode::Original,
                                test::ModalTestBench::OutputMode::HadamardDispatch,
                                true);

    // 4. Dispatch Hadamard with Elliptical Cutoff tuned to 50 Hz
    test::evaluateConfiguration("4. Dispatch Hadamard, Elliptical Filter @ 50 Hz",
                                test::ModalTestBench::InjectionMode::Original,
                                test::ModalTestBench::OutputMode::HadamardDispatch,
                                false, 50.0f);

    // 5. Balanced Input Injection + Dispatch Hadamard
    test::evaluateConfiguration("5. Balanced Injection + Dispatch Hadamard, Elliptical @ 120 Hz",
                                test::ModalTestBench::InjectionMode::BalancedHadamard,
                                test::ModalTestBench::OutputMode::HadamardDispatch,
                                false, 120.0f);

    // 6. Balanced Input Injection + Dispatch Hadamard, Elliptical @ 50 Hz
    test::evaluateConfiguration("6. Balanced Injection + Dispatch Hadamard, Elliptical @ 50 Hz",
                                test::ModalTestBench::InjectionMode::BalancedHadamard,
                                test::ModalTestBench::OutputMode::HadamardDispatch,
                                false, 50.0f);

    return 0;
}
