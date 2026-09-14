#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <cassert>
#include <limits>

#include "DspMath.h"
#include "BoundedSaturator.h"
#include "PitchShifter.h"
#include "LowBandModalMatrix.h"
#include "EarlyReflections.h"
#include "TailModulator.h"
#include "FdnReverbTank.h"
#include "Rb26Engine.h"

int main() {
    std::cout << "=======================================================================\n";
    std::cout << "  BRAUN RB-26 ADVERSARIAL STRESS HARNESS (CHALLENGER 2)                 \n";
    std::cout << "=======================================================================\n";

    bool allPassed = true;

    // Stress 1: Extreme Overload Transient (+40 dBFS / 100.0f) under Maximum Feedback (0.95) & Max RT60 (30.0s)
    {
        std::cout << "\n[Stress 1] Overload (+40 dBFS) & Maximum Feedback Recirculation:\n";
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);

        rb26::Rb26Parameters params;
        params.decayRt60Sec = 30.0f;
        params.pitchFeedback = 0.95f;
        params.shimmerSend = 1.0f;
        params.dimmerSend = 1.0f;
        params.dryWetMix = 1.0f;
        params.limiterEnable = true;
        engine.setParameters(params);

        std::vector<float> inL(512, 0.0f);
        std::vector<float> inR(512, 0.0f);
        // Fire massive overload burst of 100.0f (+40 dBFS)
        for (int i = 0; i < 64; ++i) {
            inL[i] = 100.0f;
            inR[i] = -100.0f;
        }

        std::vector<float> outL(512, 0.0f);
        std::vector<float> outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        float maxOutputMag = 0.0f;
        bool hasNaN = false;
        bool hasInf = false;

        // Process 200 blocks (102,400 samples = ~2.1 seconds)
        for (int b = 0; b < 200; ++b) {
            if (b == 1) {
                // Clear input after first block to let feedback ring
                std::fill(inL.begin(), inL.end(), 0.0f);
                std::fill(inR.begin(), inR.end(), 0.0f);
            }
            engine.process(inPtrs, outPtrs, 2, 512);

            for (int i = 0; i < 512; ++i) {
                if (std::isnan(outL[i]) || std::isnan(outR[i])) hasNaN = true;
                if (std::isinf(outL[i]) || std::isinf(outR[i])) hasInf = true;
                maxOutputMag = std::max(maxOutputMag, std::max(std::abs(outL[i]), std::abs(outR[i])));
            }
        }

        std::cout << "  Input Peak Amplitude : 100.00 (+40.0 dBFS)\n";
        std::cout << "  Feedback Setting     : 0.95 (Maximum Feedback), RT60 = 30.0s\n";
        std::cout << "  Max Output Amplitude : " << maxOutputMag << " (Bound: <= 1.05)\n";
        std::cout << "  NaN / Inf Detected   : " << (hasNaN || hasInf ? "YES (BUG!)" : "NO (Clean)") << "\n";

        if (!hasNaN && !hasInf && maxOutputMag <= 1.05f) {
            std::cout << "  Verdict              : PASS (Unconditionally bounded & numerically stable)\n";
        } else {
            std::cout << "  Verdict              : FAIL (Instability or clipping violation)\n";
            allPassed = false;
        }
    }

    // Stress 2: Denormal Input & Sudden Zero Flush
    {
        std::cout << "\n[Stress 2] Denormal Stress (1e-38f Input) & Flush to Zero:\n";
        rb26::Rb26ReverbEngine engine;
        engine.prepare(48000.0, 512);

        std::vector<float> inL(512, 1.0e-38f);
        std::vector<float> inR(512, 1.0e-38f);
        std::vector<float> outL(512, 0.0f);
        std::vector<float> outR(512, 0.0f);
        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        engine.process(inPtrs, outPtrs, 2, 512);

        // Feed exact 0.0f for 5000 samples and check that output flushes completely to 0.0f
        std::fill(inL.begin(), inL.end(), 0.0f);
        std::fill(inR.begin(), inR.end(), 0.0f);

        bool allZeroOrNormal = true;
        for (int b = 0; b < 100; ++b) {
            engine.process(inPtrs, outPtrs, 2, 512);
            for (int i = 0; i < 512; ++i) {
                if (std::fpclassify(outL[i]) == FP_SUBNORMAL || std::fpclassify(outR[i]) == FP_SUBNORMAL) {
                    allZeroOrNormal = false;
                }
            }
        }

        std::cout << "  Subnormal Float Check : " << (allZeroOrNormal ? "ZERO SUBNORMALS (Cleanly flushed)" : "SUBNORMALS DETECTED") << "\n";
        if (allZeroOrNormal) {
            std::cout << "  Verdict               : PASS (Hardware & software denormal immunity)\n";
        } else {
            std::cout << "  Verdict               : FAIL (Denormal leakage)\n";
            allPassed = false;
        }
    }

    // Stress 3: Ultra-High Sample Rate (192 kHz) Pitch Shift Accuracy
    {
        std::cout << "\n[Stress 3] Ultra-High Sample Rate (192 kHz) Pitch Shift Accuracy:\n";
        const double fs = 192000.0;
        rb26::DualTapDelayPitchShifter shifter;
        shifter.prepare(fs);
        shifter.setInterval(12); // +12st

        const size_t N = 131072;
        std::vector<float> inSig(N);
        std::vector<float> outSig(N);
        for (size_t n = 0; n < N; ++n) {
            inSig[n] = std::sin(2.0 * 3.14159265358979323846 * 440.0 * n / fs);
            outSig[n] = shifter.processSample(inSig[n]);
        }

        // Measure zero crossings over steady state (N/2 to N)
        std::vector<double> zc;
        for (size_t n = N / 2; n < N - 1; ++n) {
            if ((outSig[n] <= 0.0f && outSig[n + 1] > 0.0f) || (outSig[n] >= 0.0f && outSig[n + 1] < 0.0f)) {
                double frac = -outSig[n] / (outSig[n + 1] - outSig[n]);
                zc.push_back(static_cast<double>(n) + frac);
            }
        }

        double sumFreq = 0.0;
        size_t count = 0;
        for (size_t i = 1; i < zc.size(); ++i) {
            double halfP = zc[i] - zc[i - 1];
            double instF = fs / (2.0 * halfP);
            sumFreq += instF;
            ++count;
        }
        double avgFreq = sumFreq / count;
        double errorPct = std::abs(avgFreq - 880.0) / 880.0 * 100.0;

        std::cout << "  Sample Rate        : 192,000 Hz\n";
        std::cout << "  Input Frequency    : 440.0 Hz\n";
        std::cout << "  Target Frequency   : 880.0 Hz\n";
        std::cout << "  Measured Avg Freq  : " << std::fixed << std::setprecision(3) << avgFreq << " Hz\n";
        std::cout << "  Frequency Error    : " << errorPct << "%\n";

        if (errorPct < 0.10) {
            std::cout << "  Verdict            : PASS (Error " << errorPct << "% < 0.1% at 192 kHz)\n";
        } else {
            std::cout << "  Verdict            : FAIL (Error exceeds 0.1% at 192 kHz)\n";
            allPassed = false;
        }
    }

    std::cout << "\n=======================================================================\n";
    std::cout << "ADVERSARIAL STRESS OVERALL VERDICT: " << (allPassed ? "PASS" : "FAIL") << "\n";
    std::cout << "=======================================================================\n";

    return allPassed ? 0 : 1;
}
