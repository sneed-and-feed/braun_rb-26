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

    std::cout << "--- DIAGNOSTIC: DualTapDelayPitchShifter Spectrum ---\n";

    for (int semi : { 12, -12, -24 }) {
        rb26::DualTapDelayPitchShifter shifter;
        shifter.prepare(fs);
        shifter.setInterval(semi);

        double targetFreq = 440.0 * std::pow(2.0, semi / 12.0);

        // Run warmup
        for (int i = 0; i < 9600; ++i) {
            float s = std::sin(2.0 * kPi * 440.0 * i / fs);
            shifter.processSample(s);
        }

        std::vector<std::complex<double>> buf(N);
        for (size_t i = 0; i < N; ++i) {
            float s = std::sin(2.0 * kPi * 440.0 * (i + 9600) / fs);
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

        double binW = fs / N;
        double peakF = maxBin * binW;

        std::cout << "Semitones: " << semi 
                  << " | Target: " << targetFreq << " Hz"
                  << " | Peak: " << peakF << " Hz"
                  << " | Error: " << std::abs(peakF - targetFreq) / targetFreq * 100.0 << "%\n";

        // Print top 5 peaks
        std::vector<std::pair<double, double>> peaks;
        for (size_t k = 2; k < N / 2 - 1; ++k) {
            double m = std::abs(buf[k]);
            if (m > std::abs(buf[k - 1]) && m > std::abs(buf[k + 1]) && m > maxMag * 0.1) {
                peaks.push_back({ m, k * binW });
            }
        }
        std::sort(peaks.rbegin(), peaks.rend());
        std::cout << "  Top peaks:\n";
        for (size_t p = 0; p < std::min(size_t{5}, peaks.size()); ++p) {
            std::cout << "    f = " << peaks[p].second << " Hz (mag = " << peaks[p].first << ")\n";
        }
    }

    return 0;
}
