#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <iomanip>
#include "DspMath.h"
#include "PitchShifter.h"

constexpr double kPi = 3.14159265358979323846;

void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * kPi / len;
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j) {
                std::complex<double> u = a[i + j];
                std::complex<double> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

int main() {
    const double fs = 48000.0;
    const size_t N = 65536;

    std::cout << "=========================================================\n";
    std::cout << "DIAGNOSTIC: DualTapDelayPitchShifter Dynamic Interval Test\n";
    std::cout << "=========================================================\n";

    rb26::DualTapDelayPitchShifter shifter;
    shifter.prepare(fs);

    // Stream initial sine wave at 440 Hz (warmup while running)
    size_t sampleCounter = 0;
    for (size_t i = 0; i < 9600; ++i) {
        float s = static_cast<float>(std::sin(2.0 * kPi * 440.0 * sampleCounter++ / fs));
        (void)shifter.processSample(s);
    }

    int failures = 0;
    const std::vector<int> testIntervals = { 7, 12, 24, -2, -7, -12 };

    for (int semi : testIntervals) {
        // DYNAMIC SWITCH WHILE STREAMING (mWriteIndex != 0)
        shifter.setInterval(semi);

        const double targetFreq = 440.0 * std::pow(2.0, semi / 12.0);

        // Run post-switch slewing transition
        for (size_t i = 0; i < 4800; ++i) {
            float s = static_cast<float>(std::sin(2.0 * kPi * 440.0 * sampleCounter++ / fs));
            (void)shifter.processSample(s);
        }

        std::vector<std::complex<double>> buf(N);
        for (size_t i = 0; i < N; ++i) {
            float s = static_cast<float>(std::sin(2.0 * kPi * 440.0 * sampleCounter++ / fs));
            float y = shifter.processSample(s);
            double win = 0.5 * (1.0 - std::cos(2.0 * kPi * i / (N - 1)));
            buf[i] = y * win;
        }

        fft(buf);

        double maxMag = 0.0;
        size_t maxBin = 0;
        for (size_t k = 1; k < N / 2; ++k) {
            double mag = std::abs(buf[k]);
            if (mag > maxMag) {
                maxMag = mag;
                maxBin = k;
            }
        }

        const double binW = fs / N;
        const double peakF = maxBin * binW;
        const double errPct = std::abs(peakF - targetFreq) / targetFreq * 100.0;

        std::cout << "Dynamic Switch -> Semitones: " << std::setw(3) << semi 
                  << " | Target: " << std::fixed << std::setprecision(2) << std::setw(7) << targetFreq << " Hz"
                  << " | Peak: " << std::setw(7) << peakF << " Hz"
                  << " | Error: " << std::setprecision(3) << errPct << " %"
                  << (errPct < 2.0 ? " [PASS]" : " [FAIL]") << "\n";

        if (errPct >= 2.0) {
            ++failures;
        }
    }

    std::cout << "---------------------------------------------------------\n";
    if (failures == 0) {
        std::cout << "SUCCESS: All dynamic streaming interval switches verified within < 2% error.\n";
        return 0;
    } else {
        std::cerr << "FAILURE: " << failures << " interval switches failed accuracy threshold.\n";
        return 1;
    }
}
