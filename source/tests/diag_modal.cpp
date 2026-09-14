#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include "DspMath.h"
#include "LowBandModalMatrix.h"

constexpr double kPi = 3.14159265358979323846;

int main() {
    const double fs = 48000.0;
    rb26::LowBandModalMatrix modal;
    modal.prepare(fs);

    rb26::LowBandModalParams params;
    params.crossoverHz = 180.0f;
    params.bassRt60Mult = 1.0f;
    params.rt60DecaySec = 3.5f;
    params.punchDucking = 0.0f;
    modal.setParameters(params);

    std::cout << "--- DIAGNOSTIC: Modal Response Around 144 Hz ---\n";

    for (int f = 136; f <= 152; f += 1) {
        modal.reset();
        double sumSq = 0.0;
        size_t count = 0;

        for (size_t n = 0; n < 24000; ++n) {
            float in = static_cast<float>(std::sin(2.0 * kPi * f * n / fs));
            float lowOutL = 0.0f, lowOutR = 0.0f;
            modal.processModalOnly(in, in, lowOutL, lowOutR);

            if (n >= 9600) {
                sumSq += 0.5 * (lowOutL * lowOutL + lowOutR * lowOutR);
                ++count;
            }
        }
        double rms = std::sqrt(sumSq / count);
        std::cout << "f = " << f << " Hz : RMS = " << rms << " (" << 20.0 * std::log10(rms) << " dB)\n";
    }

    return 0;
}
