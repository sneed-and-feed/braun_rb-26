#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <iomanip>

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
    double fs = 48000.0;
    double fin = 440.0;
    size_t N = 65536;

    for (int semi : { 12, -12, -24 }) {
        double r = std::pow(2.0, semi / 12.0);
        double fc = fin * r;
        double W = 0.045; // 45 ms
        double W_samples = W * fs;
        double phaseInc = std::abs(1.0 - r) / W_samples;

        std::vector<std::complex<double>> buf(N);
        double phase = 0.0;

        for (size_t n = 0; n < N; ++n) {
            double p1 = phase;
            double p2 = (p1 >= 0.5) ? (p1 - 0.5) : (p1 + 0.5);

            double d1 = (r >= 1.0) ? (W_samples * (1.0 - p1)) : (W_samples * p1);
            double d2 = (r >= 1.0) ? (W_samples * (1.0 - p2)) : (W_samples * p2);

            // True un-interpolated analog delay: s(t - d) = sin(2*pi*fin*(t - d))
            double s1 = std::sin(2.0 * kPi * fin * (n - d1) / fs);
            double s2 = std::sin(2.0 * kPi * fin * (n - d2) / fs);

            double w1 = std::sin(kPi * p1);
            double w2 = std::sin(kPi * p2);

            double y = w1 * s1 + w2 * s2;

            double win = 0.5 * (1.0 - std::cos(2.0 * kPi * n / (N - 1)));
            buf[n] = y * win;

            phase += phaseInc;
            if (phase >= 1.0) phase -= 1.0;
        }

        fft(buf);

        double maxM = 0;
        size_t maxK = 0;
        for (size_t k = 1; k < N / 2; ++k) {
            double m = std::abs(buf[k]);
            if (m > maxM) {
                maxM = m;
                maxK = k;
            }
        }

        std::cout << "Ideal Semi: " << semi << " | fc = " << fc << " Hz | Peak: " << maxK * fs / N << " Hz"
                  << " | Error: " << std::abs(maxK * fs / N - fc) / fc * 100.0 << "%\n";

        // Print top peaks within 50 Hz of fc
        for (size_t k = 1; k < N / 2; ++k) {
            double f = k * fs / N;
            if (std::abs(f - fc) < 40.0) {
                double m = std::abs(buf[k]);
                if (m > maxM * 0.4) {
                    std::cout << "    f = " << f << " Hz : mag = " << m << "\n";
                }
            }
        }
    }
    return 0;
}
