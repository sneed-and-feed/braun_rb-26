#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <array>
#include "DspMath.h"
#include "LowBandModalMatrix.h"

namespace test {

constexpr double kPi = 3.14159265358979323846;

struct OutputMatrix {
    std::string name;
    std::array<float, 4> cL;
    std::array<float, 4> cR;
};

void testOutputMatrix(const OutputMatrix& mat) {
    const double fs = 48000.0;
    rb26::LowBandModalMatrix modal;
    modal.prepare(fs);

    rb26::LowBandModalParams params;
    params.crossoverHz = 180.0f;
    params.bassRt60Mult = 1.0f;
    params.rt60DecaySec = 3.5f;
    params.punchDucking = 0.0f;
    params.subMonoHz = 120.0f;
    modal.setParameters(params);

    std::vector<double> freqs;
    std::vector<double> rmsOutputs;
    for (int f = 40; f <= 200; ++f) freqs.push_back(static_cast<double>(f));

    const size_t testLength = 24000;
    const size_t steadyStart = 9600;

    // Simulate custom output extraction inside modal loop
    rb26::LinkwitzRiley4 crossover;
    crossover.prepare(fs);
    crossover.setCutoff(180.0f);

    rb26::TransientPunchDetector punch;
    punch.prepare(fs);

    rb26::SubBassEllipticalFilter elliptical;
    elliptical.prepare(fs);
    elliptical.setCutoff(120.0f);

    // Primes
    static constexpr std::array<float, 4> kBaseDelayTimes = { 0.0710f, 0.0860f, 0.1040f, 0.1261f };
    std::array<size_t, 4> delayLens {};
    std::array<size_t, 4> writeIdx {};
    std::array<std::vector<float>, 4> bufs;
    std::array<float, 4> decay {};

    auto isPrime = [](size_t n) {
        if (n <= 1) return false;
        if (n <= 3) return true;
        if (n % 2 == 0 || n % 3 == 0) return false;
        for (size_t i = 5; i * i <= n; i += 6) {
            if (n % i == 0 || n % (i + 2) == 0) return false;
        }
        return true;
    };

    auto findClosestPrime = [&](size_t target) {
        if (isPrime(target)) return target;
        size_t offset = 1;
        while (true) {
            if (target >= offset && isPrime(target - offset)) return target - offset;
            if (isPrime(target + offset)) return target + offset;
            ++offset;
        }
    };

    for (size_t i = 0; i < 4; ++i) {
        const size_t nominal = static_cast<size_t>(std::round(kBaseDelayTimes[i] * fs));
        delayLens[i] = findClosestPrime(nominal);
        bufs[i].assign(delayLens[i] + 64, 0.0f);
        const float tSec = static_cast<float>(delayLens[i]) / static_cast<float>(fs);
        decay[i] = std::exp(-6.907755278982137f * tSec / 3.5f);
    }

    double minRms = 1e9, maxRms = -1e9;
    int minF = 0, maxF = 0;
    double rms77 = 0.0, rms143 = 0.0, rms148 = 0.0;

    for (double f : freqs) {
        punch.reset();
        elliptical.reset();
        for (size_t i = 0; i < 4; ++i) {
            std::fill(bufs[i].begin(), bufs[i].end(), 0.0f);
            writeIdx[i] = 0;
        }

        double sumSq = 0.0;
        size_t count = 0;

        for (size_t n = 0; n < testLength; ++n) {
            float in = static_cast<float>(std::sin(2.0 * kPi * f * n / fs));

            // Modal process
            float duckGain = punch.process(in, in, 0.0f);
            float duckL = in * duckGain;
            float duckR = in * duckGain;

            std::array<float, 4> w;
            for (size_t i = 0; i < 4; ++i) w[i] = bufs[i][writeIdx[i]];

            float sum = w[0] + w[1] + w[2] + w[3];
            float halfSum = sum * 0.5f;
            std::array<float, 4> v;
            for (size_t i = 0; i < 4; ++i) v[i] = w[i] - halfSum;

            float midIn = 0.5f * (duckL + duckR);
            float sideIn = 0.5f * (duckL - duckR);
            std::array<float, 4> inj = { duckL, duckR, midIn, sideIn };

            for (size_t i = 0; i < 4; ++i) {
                float fb = v[i] * decay[i];
                float sat = rb26::applySmoothBoundaryKnee(fb + inj[i], 0.72f);
                bufs[i][writeIdx[i]] = rb26::flushDenormal(sat);
                writeIdx[i] = (writeIdx[i] + 1) % delayLens[i];
            }

            // Output extraction
            float rawL = 0.5f * (mat.cL[0]*w[0] + mat.cL[1]*w[1] + mat.cL[2]*w[2] + mat.cL[3]*w[3]);
            float rawR = 0.5f * (mat.cR[0]*w[0] + mat.cR[1]*w[1] + mat.cR[2]*w[2] + mat.cR[3]*w[3]);

            float outL = 0.0f, outR = 0.0f;
            elliptical.process(rawL, rawR, outL, outR);

            if (n >= steadyStart) {
                sumSq += 0.5 * (outL * outL + outR * outR);
                ++count;
            }
        }

        double rms = std::sqrt(sumSq / count);
        rmsOutputs.push_back(rms);

        if (rms < minRms) { minRms = rms; minF = static_cast<int>(f); }
        if (rms > maxRms) { maxRms = rms; maxF = static_cast<int>(f); }
        if (static_cast<int>(f) == 77) rms77 = 20.0 * std::log10(rms);
        if (static_cast<int>(f) == 143) rms143 = 20.0 * std::log10(rms);
        if (static_cast<int>(f) == 148) rms148 = 20.0 * std::log10(rms);
    }

    double maxNotch = 0.0;
    int notchF = 0;
    for (size_t i = 1; i < rmsOutputs.size() - 1; ++i) {
        double localMax = std::max(rmsOutputs[i - 1], rmsOutputs[i + 1]);
        double current = rmsOutputs[i];
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

    std::cout << "MATRIX: " << mat.name << "\n";
    std::cout << "  Row L: [" << mat.cL[0] << ", " << mat.cL[1] << ", " << mat.cL[2] << ", " << mat.cL[3] << "]\n";
    std::cout << "  Row R: [" << mat.cR[0] << ", " << mat.cR[1] << ", " << mat.cR[2] << ", " << mat.cR[3] << "]\n";
    std::cout << "  Max Adjacent Notch : " << std::fixed << std::setprecision(2) << maxNotch << " dB (@ " << notchF << " Hz)\n";
    std::cout << "  Dynamic Ripple     : " << (maxDb - minDb) << " dB (Min: " << minDb << " dB @ " << minF << " Hz, Max: " << maxDb << " dB @ " << maxF << " Hz)\n";
    std::cout << "  RMS @ 77 Hz        : " << rms77 << " dB\n";
    std::cout << "  RMS @ 143 Hz       : " << rms143 << " dB\n";
    std::cout << "  RMS @ 148 Hz       : " << rms148 << " dB\n";
    std::cout << "  143-148 Hz Delta   : " << (rms148 - rms143) << " dB\n\n";
}

} // namespace test

int main() {
    std::cout << "=================================================================\n";
    std::cout << "  DIRECT COMPARISON OF ORTHOGONAL HADAMARD OUTPUT FORMULATIONS\n";
    std::cout << "=================================================================\n\n";

    // 1. Dispatch specification:
    // L = 0.5 * (w0 + w1 - w2 - w3)
    // R = 0.5 * (w0 - w1 + w2 - w3)
    test::testOutputMatrix({
        "Dispatch Specified Formula (Row 2 & Row 1)",
        { 1.0f,  1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f,  1.0f, -1.0f }
    });

    // 2. Symmetric Positive Hadamard (Row 0 & Row 2):
    // L = 0.5 * (w0 + w1 + w2 + w3)
    // R = 0.5 * (w0 + w1 - w2 - w3)
    test::testOutputMatrix({
        "Walsh-Hadamard Row 0 & Row 2 (Constructive Mid w0+w1)",
        { 1.0f,  1.0f,  1.0f,  1.0f },
        { 1.0f,  1.0f, -1.0f, -1.0f }
    });

    // 3. Walsh-Hadamard Row 2 & Row 0 (Symmetric swap):
    test::testOutputMatrix({
        "Walsh-Hadamard Row 2 & Row 0",
        { 1.0f,  1.0f, -1.0f, -1.0f },
        { 1.0f,  1.0f,  1.0f,  1.0f }
    });

    // 4. Candidate [2] from exhaustive search (Row 0 & -Row 2):
    // L = 0.5 * (-w0 - w1 + w2 + w3)
    // R = 0.5 * (w0 + w1 + w2 + w3)
    test::testOutputMatrix({
        "Exhaustive Optimal (Constructive Mid w2+w3)",
        { -1.0f, -1.0f,  1.0f,  1.0f },
        {  1.0f,  1.0f,  1.0f,  1.0f }
    });

    // 5. Symmetric Balanced Pair (Row 2 & Row 3):
    // L = 0.5 * (w0 + w1 - w2 - w3)
    // R = 0.5 * (w0 - w1 - w2 + w3)
    test::testOutputMatrix({
        "Walsh-Hadamard Row 2 & Row 3",
        { 1.0f,  1.0f, -1.0f, -1.0f },
        { 1.0f, -1.0f, -1.0f,  1.0f }
    });

    return 0;
}
