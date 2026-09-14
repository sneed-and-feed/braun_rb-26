#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <array>
#include <random>
#include "DspMath.h"
#include "LowBandModalMatrix.h"

namespace test {

constexpr double kPi = 3.14159265358979323846;

double evaluateMatrix(const std::array<float, 4>& cL, const std::array<float, 4>& cR,
                      int& worstFreq, double& worstRipple) {
    const double fs = 48000.0;
    static constexpr std::array<float, 4> kBaseDelayTimes = { 0.0710f, 0.0860f, 0.1040f, 0.1261f };
    std::array<size_t, 4> delayLens;
    std::array<size_t, 4> writeIdx;
    std::array<std::vector<float>, 4> bufs;
    std::array<float, 4> decay;

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

    rb26::TransientPunchDetector punch;
    punch.prepare(fs);

    rb26::SubBassEllipticalFilter elliptical;
    elliptical.prepare(fs);
    elliptical.setCutoff(120.0f);

    std::vector<double> rmsOutputs;
    double minRms = 1e9, maxRms = -1e9;

    for (int f = 40; f <= 200; ++f) {
        punch.reset();
        elliptical.reset();
        for (size_t i = 0; i < 4; ++i) {
            std::fill(bufs[i].begin(), bufs[i].end(), 0.0f);
            writeIdx[i] = 0;
        }

        double sumSq = 0.0;
        size_t count = 0;

        for (size_t n = 0; n < 24000; ++n) {
            float in = static_cast<float>(std::sin(2.0 * kPi * f * n / fs));
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

            float rawL = cL[0]*w[0] + cL[1]*w[1] + cL[2]*w[2] + cL[3]*w[3];
            float rawR = cR[0]*w[0] + cR[1]*w[1] + cR[2]*w[2] + cR[3]*w[3];

            float outL = 0.0f, outR = 0.0f;
            elliptical.process(rawL, rawR, outL, outR);

            if (n >= 9600) {
                sumSq += 0.5 * (outL * outL + outR * outR);
                ++count;
            }
        }

        double rms = std::sqrt(sumSq / count);
        rmsOutputs.push_back(rms);
        if (rms < minRms) minRms = rms;
        if (rms > maxRms) maxRms = rms;
    }

    double maxNotch = 0.0;
    worstFreq = 0;
    for (size_t i = 1; i < rmsOutputs.size() - 1; ++i) {
        double localMax = std::max(rmsOutputs[i - 1], rmsOutputs[i + 1]);
        double current = rmsOutputs[i];
        if (current > 1.0e-8 && localMax > 1.0e-8) {
            double dropDb = 20.0 * std::log10(localMax / current);
            if (dropDb > maxNotch) {
                maxNotch = dropDb;
                worstFreq = 40 + static_cast<int>(i);
            }
        }
    }

    worstRipple = 20.0 * std::log10(maxRms / minRms);
    return maxNotch;
}

} // namespace test

int main() {
    std::cout << "Testing Continuous Stiefel Orthogonal Rotations on Modal Output...\n";

    // Random orthogonal 2-frames in R^4
    std::mt19937 rng(1337);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    double bestNotch = 1e9;
    std::array<float, 4> bestL {}, bestR {};
    int bestFreq = 0;
    double bestRipple = 0;

    for (int iter = 0; iter < 100; ++iter) {
        std::array<float, 4> u {}, v {};
        for (int i = 0; i < 4; ++i) { u[i] = dist(rng); v[i] = dist(rng); }

        // Gram-Schmidt orthonormalization
        float normU = 0.0f;
        for (int i = 0; i < 4; ++i) normU += u[i] * u[i];
        normU = std::sqrt(normU);
        for (int i = 0; i < 4; ++i) u[i] /= normU;

        float dot = 0.0f;
        for (int i = 0; i < 4; ++i) dot += u[i] * v[i];
        for (int i = 0; i < 4; ++i) v[i] -= dot * u[i];

        float normV = 0.0f;
        for (int i = 0; i < 4; ++i) normV += v[i] * v[i];
        normV = std::sqrt(normV);
        for (int i = 0; i < 4; ++i) v[i] /= normV;

        int f = 0;
        double rip = 0.0;
        double notch = test::evaluateMatrix(u, v, f, rip);

        if (notch < bestNotch) {
            bestNotch = notch;
            bestL = u;
            bestR = v;
            bestFreq = f;
            bestRipple = rip;
            std::cout << "Iter " << iter << ": Found new best notch = " << bestNotch << " dB (@ " << bestFreq << " Hz), Ripple = " << bestRipple << " dB\n";
            std::cout << "  cL: [" << u[0] << ", " << u[1] << ", " << u[2] << ", " << u[3] << "]\n";
            std::cout << "  cR: [" << v[0] << ", " << v[1] << ", " << v[2] << ", " << v[3] << "]\n";
        }
    }

    std::cout << "\nGLOBAL BEST ORTHOGONAL NOTCH: " << bestNotch << " dB\n";
    return 0;
}
