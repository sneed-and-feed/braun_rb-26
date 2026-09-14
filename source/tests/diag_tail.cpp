#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "DspMath.h"
#include "TailModulator.h"
#include "EarlyReflections.h"

int main() {
    const double fs = 48000.0;

    std::cout << "--- DIAGNOSTIC: TailModulator Excursion Over 3 Seconds ---\n";
    {
        rb26::TailModulator mod;
        mod.prepare(fs);
        mod.setParameters(1.0f, 2.5f, 85.0f); // 1.0 Hz, 2.5 ms, 85 ms bloom

        const size_t totalSamples = static_cast<size_t>(fs * 3.0); // 3 seconds
        std::vector<float> exc0(totalSamples, 0.0f);

        for (size_t n = 0; n < totalSamples; ++n) {
            float transientIn = (n < 480) ? 1.0f : 0.0f; // 10ms transient pulse
            std::array<float, 8> outExc {};
            mod.processSample(transientIn, outExc);
            exc0[n] = outExc[0];
        }

        // Check excursion at various times
        for (double t : { 0.05, 0.10, 0.20, 0.50, 1.0, 1.5, 2.0, 2.5 }) {
            size_t sample = static_cast<size_t>(t * fs);
            std::cout << "t = " << t << "s: exc0 = " << exc0[sample] / fs * 1000.0 << " ms\n";
        }

        // Find min and max after 500 ms
        float minExc = exc0[static_cast<size_t>(0.5 * fs)];
        float maxExc = minExc;
        for (size_t n = static_cast<size_t>(0.5 * fs); n < totalSamples; ++n) {
            minExc = std::min(minExc, exc0[n]);
            maxExc = std::max(maxExc, exc0[n]);
        }
        std::cout << "Peak-to-peak after 500 ms: " << (maxExc - minExc) / fs * 1000.0 << " ms\n";
    }

    std::cout << "\n--- DIAGNOSTIC: Early Reflections Frequency Measurement ---\n";
    {
        rb26::EarlyReflections er;
        er.prepare(fs, 1.0f);
        er.setParameters(1.0f);

        // Feed steady 1000 Hz tone for 500 ms
        const size_t totalSamples = static_cast<size_t>(fs * 0.5);
        std::vector<float> in(totalSamples);
        std::vector<float> outL(totalSamples);
        std::vector<float> outR(totalSamples);

        for (size_t n = 0; n < totalSamples; ++n) {
            in[n] = std::sin(2.0 * 3.14159265358979323846 * 1000.0 * n / fs);
            er.processSample(in[n], in[n], outL[n], outR[n]);
        }

        // In steady state (after all 12 taps have arrived: max tap is 137.3 ms = 6590 samples)
        // at t = 200 ms to 300 ms:
        size_t sStart = static_cast<size_t>(0.200 * fs);
        size_t sEnd   = static_cast<size_t>(0.250 * fs);

        std::vector<double> zc;
        for (size_t n = sStart; n < sEnd - 1; ++n) {
            if ((outL[n] <= 0.0f && outL[n + 1] > 0.0f) || (outL[n] >= 0.0f && outL[n + 1] < 0.0f)) {
                double frac = -outL[n] / (outL[n + 1] - outL[n]);
                zc.push_back(static_cast<double>(n) + frac);
            }
        }

        double maxDev = 0.0;
        for (size_t i = 1; i < zc.size(); ++i) {
            double halfPeriod = zc[i] - zc[i - 1];
            double instFreq = fs / (2.0 * halfPeriod);
            double dev = std::abs(instFreq - 1000.0);
            if (dev > maxDev) maxDev = dev;
        }
        std::cout << "Steady-state Early Reflections max frequency deviation: " << maxDev << " Hz\n";
    }

    return 0;
}
