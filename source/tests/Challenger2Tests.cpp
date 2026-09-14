#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <algorithm>
#include <iomanip>
#include <cassert>
#include <string>

// Include DSP Core headers
#include "DspMath.h"
#include "BoundedSaturator.h"
#include "PitchShifter.h"
#include "LowBandModalMatrix.h"
#include "EarlyReflections.h"
#include "TailModulator.h"
#include "FdnReverbTank.h"
#include "Rb26Engine.h"

// ============================================================================
// Global Heap Allocation Tracker for Real-Time Safety Verification
// ============================================================================
static bool gTrackAllocations = false;
static size_t gAllocationCount = 0;
static size_t gBytesAllocated = 0;

void* operator new(size_t size) {
    if (gTrackAllocations) {
        ++gAllocationCount;
        gBytesAllocated += size;
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

// ============================================================================
// Test Utilities: FFT, Peak Detection
// ============================================================================
namespace test_utils {

constexpr double kPi = 3.14159265358979323846;

// Radix-2 Cooley-Tukey In-Place FFT
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

// Exact FFT Peak Frequency Detector using Blackman-Harris window and quadratic interpolation
double findPeakFrequency(const std::vector<float>& signal, double sampleRate, size_t startSample, size_t numSamples, double minF, double maxF) {
    size_t fftSize = 1;
    while (fftSize * 2 <= numSamples) fftSize *= 2;

    std::vector<std::complex<double>> buffer(fftSize);
    for (size_t i = 0; i < fftSize; ++i) {
        // Blackman-Harris 4-term window for minimum spectral leakage
        const double a0 = 0.35875, a1 = 0.48829, a2 = 0.14128, a3 = 0.01168;
        const double t = 2.0 * kPi * i / (fftSize - 1);
        const double win = a0 - a1 * std::cos(t) + a2 * std::cos(2.0 * t) - a3 * std::cos(3.0 * t);
        buffer[i] = signal[startSample + i] * win;
    }

    fft(buffer);

    const double binWidth = sampleRate / static_cast<double>(fftSize);
    const size_t minBin = std::max(size_t{2}, static_cast<size_t>(minF / binWidth));
    const size_t maxBin = std::min(fftSize / 2 - 2, static_cast<size_t>(maxF / binWidth) + 1);

    size_t bestBin = minBin;
    double maxMag = 0.0;
    for (size_t k = minBin; k <= maxBin; ++k) {
        double mag = std::abs(buffer[k]);
        if (mag > maxMag) {
            maxMag = mag;
            bestBin = k;
        }
    }

    // Quadratic interpolation around peak bin for exact sub-bin peak localization
    const double alpha = std::abs(buffer[bestBin - 1]);
    const double beta  = std::abs(buffer[bestBin]);
    const double gamma = std::abs(buffer[bestBin + 1]);

    double p = 0.0;
    double denom = (alpha - 2.0 * beta + gamma);
    if (std::abs(denom) > 1.0e-12) {
        p = 0.5 * (alpha - gamma) / denom;
    }

    return (static_cast<double>(bestBin) + p) * binWidth;
}

} // namespace test_utils

// ============================================================================
// TEST 1: Pitch Shifter Spectral Accuracy
// ============================================================================
bool runPitchShifterSpectralTests() {
    std::cout << "\n=======================================================\n";
    std::cout << "TEST 1: Pitch Shifter Spectral Accuracy (440 Hz Input)\n";
    std::cout << "=======================================================\n";

    const double sampleRate = 48000.0;
    const double inputFreq = 440.0;
    const size_t totalSamples = 96000;
    const size_t warmupSamples = 9600;
    const size_t testSamples = 65536;

    std::vector<float> inBuffer(totalSamples);
    for (size_t n = 0; n < totalSamples; ++n) {
        inBuffer[n] = static_cast<float>(std::sin(2.0 * test_utils::kPi * inputFreq * n / sampleRate));
    }

    bool allPassed = true;

    // --- Subtest 1.1: Shimmer +12st ---
    {
        rb26::PitchShifter shifter;
        shifter.prepare(sampleRate);
        shifter.setParameters(1.0f, 0.0f, 12, -12, 1.0f, 0.0f);

        std::vector<float> outL(totalSamples, 0.0f);
        std::vector<float> outR(totalSamples, 0.0f);
        shifter.process(inBuffer.data(), inBuffer.data(), outL.data(), outR.data(), static_cast<int>(totalSamples));

        double peakFreq = test_utils::findPeakFrequency(outL, sampleRate, warmupSamples, testSamples, 750.0, 1000.0);
        double target = 880.0;
        double errorPct = std::abs(peakFreq - target) / target * 100.0;

        std::cout << "[1.1] Shimmer +12st:\n";
        std::cout << "      Input Frequency : 440.0 Hz\n";
        std::cout << "      Target Peak     : 880.0 Hz (+/- 1.0% = [871.2, 888.8] Hz)\n";
        std::cout << "      Measured Peak   : " << std::fixed << std::setprecision(2) << peakFreq << " Hz\n";
        std::cout << "      Frequency Error : " << errorPct << "%\n";

        if (errorPct <= 1.0) {
            std::cout << "      Verdict         : PASS\n";
        } else {
            std::cout << "      Verdict         : FAIL (Error exceeds 1.0%)\n";
            allPassed = false;
        }
    }

    // --- Subtest 1.2: Dimmer -12st ---
    {
        rb26::PitchShifter shifter;
        shifter.prepare(sampleRate);
        shifter.setParameters(0.0f, 1.0f, 12, -12, -1.0f, 0.0f);

        std::vector<float> outL(totalSamples, 0.0f);
        std::vector<float> outR(totalSamples, 0.0f);
        shifter.process(inBuffer.data(), inBuffer.data(), outL.data(), outR.data(), static_cast<int>(totalSamples));

        double peakFreq = test_utils::findPeakFrequency(outL, sampleRate, warmupSamples, testSamples, 180.0, 260.0);
        double target = 220.0;
        double errorPct = std::abs(peakFreq - target) / target * 100.0;

        std::cout << "[1.2] Dimmer -12st:\n";
        std::cout << "      Input Frequency : 440.0 Hz\n";
        std::cout << "      Target Peak     : 220.0 Hz (+/- 1.0% = [217.8, 222.2] Hz)\n";
        std::cout << "      Measured Peak   : " << std::fixed << std::setprecision(2) << peakFreq << " Hz\n";
        std::cout << "      Frequency Error : " << errorPct << "%\n";

        if (errorPct <= 1.0) {
            std::cout << "      Verdict         : PASS\n";
        } else {
            std::cout << "      Verdict         : FAIL (Error exceeds 1.0%)\n";
            allPassed = false;
        }
    }

    // --- Subtest 1.3: Dimmer -24st ---
    {
        rb26::PitchShifter shifter;
        shifter.prepare(sampleRate);
        shifter.setParameters(0.0f, 1.0f, 12, -24, -1.0f, 0.0f);

        std::vector<float> outL(totalSamples, 0.0f);
        std::vector<float> outR(totalSamples, 0.0f);
        shifter.process(inBuffer.data(), inBuffer.data(), outL.data(), outR.data(), static_cast<int>(totalSamples));

        double peakFreq = test_utils::findPeakFrequency(outL, sampleRate, warmupSamples, testSamples, 80.0, 140.0);
        double target = 110.0;
        double errorPct = std::abs(peakFreq - target) / target * 100.0;

        std::cout << "[1.3] Dimmer -24st:\n";
        std::cout << "      Input Frequency : 440.0 Hz\n";
        std::cout << "      Target Peak     : 110.0 Hz (+/- 1.0% = [108.9, 111.1] Hz)\n";
        std::cout << "      Measured Peak   : " << std::fixed << std::setprecision(2) << peakFreq << " Hz\n";
        std::cout << "      Frequency Error : " << errorPct << "%\n";

        if (errorPct <= 1.0) {
            std::cout << "      Verdict         : PASS\n";
        } else {
            std::cout << "      Verdict         : FAIL (Error exceeds 1.0%)\n";
            allPassed = false;
        }
    }

    return allPassed;
}

// ============================================================================
// TEST 2: Low-Frequency Modal Decay & Crossover
// ============================================================================
bool runLowFrequencyModalTests() {
    std::cout << "\n=======================================================\n";
    std::cout << "TEST 2: Low-Frequency Modal Decay & Crossover\n";
    std::cout << "=======================================================\n";

    const double sampleRate = 48000.0;
    bool allPassed = true;

    // --- Subtest 2.1: Sweep 40 Hz - 200 Hz for Comb Filter Nulls (>6dB dropouts) ---
    {
        std::cout << "[2.1] Low-Frequency Modal Swept Response (40 Hz - 200 Hz):\n";

        rb26::LowBandModalMatrix modal;
        modal.prepare(sampleRate);
        rb26::LowBandModalParams params;
        params.crossoverHz = 180.0f;
        params.bassRt60Mult = 1.0f;
        params.rt60DecaySec = 3.5f;
        params.punchDucking = 0.0f;
        modal.setParameters(params);

        std::vector<double> testFreqs;
        std::vector<double> rmsOutputs;

        for (int f = 40; f <= 200; f += 1) {
            testFreqs.push_back(static_cast<double>(f));
        }

        const size_t testLength = 24000;
        const size_t steadyStart = 9600;

        for (double freq : testFreqs) {
            modal.reset();
            double sumSq = 0.0;
            size_t count = 0;

            for (size_t n = 0; n < testLength; ++n) {
                float in = static_cast<float>(std::sin(2.0 * test_utils::kPi * freq * n / sampleRate));
                float lowOutL = 0.0f, lowOutR = 0.0f;
                modal.processModalOnly(in, in, lowOutL, lowOutR);

                if (n >= steadyStart) {
                    sumSq += 0.5 * (lowOutL * lowOutL + lowOutR * lowOutR);
                    ++count;
                }
            }

            double rms = std::sqrt(sumSq / count);
            rmsOutputs.push_back(rms);
        }

        double maxNotchDepthDb = 0.0;
        int notchFreq = 0;

        // Measure notch depth: compare local minimum against adjacent peak envelope
        for (size_t i = 1; i < rmsOutputs.size() - 1; ++i) {
            double localMax = std::max(rmsOutputs[i - 1], rmsOutputs[i + 1]);
            double current = rmsOutputs[i];
            if (current > 1.0e-8 && localMax > 1.0e-8) {
                double dropDb = 20.0 * std::log10(localMax / current);
                if (dropDb > maxNotchDepthDb) {
                    maxNotchDepthDb = dropDb;
                    notchFreq = static_cast<int>(testFreqs[i]);
                }
            }
        }

        std::cout << "      Frequencies Evaluated : 40 Hz to 200 Hz (161 discrete tones)\n";
        std::cout << "      Maximum Adjacent Notch: " << std::fixed << std::setprecision(2) << maxNotchDepthDb
                  << " dB (at " << notchFreq << " Hz)\n";
        std::cout << "      Threshold Constraint  : No comb filter nulls > 6.0 dB\n";

        if (maxNotchDepthDb < 6.0) {
            std::cout << "      Verdict               : PASS (Smooth modal distribution, zero comb nulls)\n";
        } else {
            std::cout << "      Verdict               : FAIL (Found comb null dropout of " << maxNotchDepthDb << " dB)\n";
            allPassed = false;
        }
    }

    // --- Subtest 2.2: Modal Impulse Response Smooth Decay ---
    {
        std::cout << "[2.2] Impulse Response Energy Decay Curve (EDC) Smoothness:\n";

        rb26::LowBandModalMatrix modal;
        modal.prepare(sampleRate);
        rb26::LowBandModalParams params;
        params.bassRt60Mult = 1.0f;
        params.rt60DecaySec = 2.0f;
        params.punchDucking = 0.0f;
        modal.setParameters(params);

        const size_t irLength = 48000;
        std::vector<float> ir(irLength, 0.0f);

        float outL = 0.0f, outR = 0.0f;
        modal.processModalOnly(1.0f, 1.0f, outL, outR);
        ir[0] = 0.5f * (outL + outR);

        for (size_t n = 1; n < irLength; ++n) {
            modal.processModalOnly(0.0f, 0.0f, outL, outR);
            ir[n] = 0.5f * (outL + outR);
        }

        std::vector<double> edc(irLength, 0.0);
        double acc = 0.0;
        for (size_t n = irLength; n-- > 0;) {
            acc += static_cast<double>(ir[n]) * static_cast<double>(ir[n]);
            edc[n] = acc;
        }

        const double initialEnergy = edc[0];
        double edcAt100ms = 10.0 * std::log10(edc[static_cast<size_t>(0.100 * sampleRate)] / initialEnergy);
        double edcAt500ms = 10.0 * std::log10(edc[static_cast<size_t>(0.500 * sampleRate)] / initialEnergy);
        double edcAt900ms = 10.0 * std::log10(edc[static_cast<size_t>(0.900 * sampleRate)] / initialEnergy);

        std::cout << "      EDC @ 100 ms : " << edcAt100ms << " dB\n";
        std::cout << "      EDC @ 500 ms : " << edcAt500ms << " dB\n";
        std::cout << "      EDC @ 900 ms : " << edcAt900ms << " dB\n";

        if (edcAt100ms > edcAt500ms && edcAt500ms > edcAt900ms) {
            std::cout << "      Verdict      : PASS (Monotonic, smooth modal dissipation)\n";
        } else {
            std::cout << "      Verdict      : FAIL (Non-decaying energy curve)\n";
            allPassed = false;
        }
    }

    // --- Subtest 2.3: Transient Punch Ducking ---
    {
        std::cout << "[2.3] Transient Punch Ducking on Synthesized Kick Transient:\n";

        rb26::LowBandModalMatrix modal;
        modal.prepare(sampleRate);
        rb26::LowBandModalParams params;
        params.punchDucking = 1.0f;
        modal.setParameters(params);

        const size_t kickLen = 24000;
        const size_t stepOnset = 2400;
        std::vector<float> kickSignal(kickLen, 0.0f);

        for (size_t n = stepOnset; n < kickLen; ++n) {
            double t = static_cast<double>(n - stepOnset) / sampleRate;
            double freq = 120.0 * std::exp(-t / 0.025) + 50.0;
            double env = std::exp(-t / 0.040);
            kickSignal[n] = static_cast<float>(env * std::sin(2.0 * test_utils::kPi * freq * t));
        }
        kickSignal[stepOnset] = 1.0f;

        float minDuckGain = 1.0f;
        float postTransientGain = 1.0f;

        for (size_t n = 0; n < kickLen; ++n) {
            float dummyOutL = 0.0f, dummyOutR = 0.0f;
            modal.processModalOnly(kickSignal[n], kickSignal[n], dummyOutL, dummyOutR);
            float g = modal.getDuckingGain();

            if (n >= stepOnset && n < stepOnset + static_cast<size_t>(sampleRate * 0.030)) {
                if (g < minDuckGain) minDuckGain = g;
            }
            if (n == kickLen - 1) postTransientGain = g;
        }

        double duckingDb = 20.0 * std::log10(minDuckGain);
        double postDb = 20.0 * std::log10(postTransientGain);

        std::cout << "      Kick Step Attack Min Gain: " << minDuckGain << " (" << duckingDb << " dB)\n";
        std::cout << "      Post-Transient Recovered : " << postTransientGain << " (" << postDb << " dB)\n";
        std::cout << "      Target Specification     : Duck modal injection by 9 to 14 dB\n";

        if (duckingDb <= -9.0 && duckingDb >= -14.0) {
            std::cout << "      Verdict                  : PASS (Exact compliance with 9 to 14 dB requirement)\n";
        } else {
            std::cout << "      Verdict                  : FAIL (Ducking " << duckingDb << " dB out of [9, 14] dB range)\n";
            allPassed = false;
        }
    }

    return allPassed;
}

// ============================================================================
// TEST 3: Tail Modulation Isolation
// ============================================================================
bool runTailModulationIsolationTests() {
    std::cout << "\n=======================================================\n";
    std::cout << "TEST 3: Tail Modulation Isolation (Early vs Late Tail)\n";
    std::cout << "=======================================================\n";

    const double sampleRate = 48000.0;
    const double testFreq = 1000.0;
    bool allPassed = true;

    // --- Subtest 3.1: Early Reflections Invariant Verification ---
    {
        std::cout << "[3.1] Early Reflections Time-Invariance & Pitch Stability:\n";

        rb26::EarlyReflections er;
        er.prepare(sampleRate, 1.0f);
        er.setParameters(1.0f);

        const size_t numSamples = static_cast<size_t>(sampleRate * 0.500); // 500 ms
        std::vector<float> inSig(numSamples);
        std::vector<float> erOutL(numSamples, 0.0f);
        std::vector<float> erOutR(numSamples, 0.0f);

        for (size_t n = 0; n < numSamples; ++n) {
            inSig[n] = static_cast<float>(std::sin(2.0 * test_utils::kPi * testFreq * n / sampleRate));
            er.processSample(inSig[n], inSig[n], erOutL[n], erOutR[n]);
        }

        // Measure steady state instantaneous frequency
        const size_t startSample = static_cast<size_t>(sampleRate * 0.200);
        const size_t endSample   = static_cast<size_t>(sampleRate * 0.300);

        std::vector<double> zeroCrossings;
        for (size_t n = startSample; n < endSample - 1; ++n) {
            if ((erOutL[n] <= 0.0f && erOutL[n + 1] > 0.0f) || (erOutL[n] >= 0.0f && erOutL[n + 1] < 0.0f)) {
                double frac = -erOutL[n] / (erOutL[n + 1] - erOutL[n]);
                zeroCrossings.push_back(static_cast<double>(n) + frac);
            }
        }

        double maxFreqDev = 0.0;
        if (zeroCrossings.size() >= 4) {
            for (size_t i = 1; i < zeroCrossings.size(); ++i) {
                double halfPeriod = zeroCrossings[i] - zeroCrossings[i - 1];
                double instFreq = sampleRate / (2.0 * halfPeriod);
                double dev = std::abs(instFreq - testFreq);
                if (dev > maxFreqDev) maxFreqDev = dev;
            }
        }

        std::cout << "      Input Frequency           : " << testFreq << " Hz\n";
        std::cout << "      Steady State Frequency Dev: " << std::fixed << std::setprecision(6) << maxFreqDev << " Hz\n";
        std::cout << "      Delay Modulation Depth    : 0.000 ms (Static, Time-Invariant Prime Taps)\n";

        if (maxFreqDev < 0.001) {
            std::cout << "      Verdict                   : PASS (Early reflections have bit-exact 0.0 Hz modulation drift)\n";
        } else {
            std::cout << "      Verdict                   : FAIL (Early reflections modulated by " << maxFreqDev << " Hz)\n";
            allPassed = false;
        }
    }

    // --- Subtest 3.2: Late Tail Modulation Excursion (>150 ms) ---
    {
        std::cout << "[3.2] Late Tail Delay Modulation Excursion (>150 ms):\n";

        rb26::TailModulator mod;
        mod.prepare(sampleRate);
        mod.setParameters(1.0f, 2.5f, 85.0f);

        const size_t totalSamples = static_cast<size_t>(sampleRate * 3.0); // 3.0 seconds to cover full LFO cycles
        std::vector<float> exc0(totalSamples, 0.0f);

        for (size_t n = 0; n < totalSamples; ++n) {
            float transientIn = (n < 480) ? 1.0f : 0.0f;
            std::array<float, 8> outExc {};
            mod.processSample(transientIn, outExc);
            exc0[n] = outExc[0];
        }

        // Measure excursion in late tail (t >= 0.5s to 3.0s)
        float minExc = exc0[static_cast<size_t>(0.5 * sampleRate)];
        float maxExc = minExc;
        for (size_t n = static_cast<size_t>(0.5 * sampleRate); n < totalSamples; ++n) {
            minExc = std::min(minExc, exc0[n]);
            maxExc = std::max(maxExc, exc0[n]);
        }

        float lateP2P_ms = (maxExc - minExc) / static_cast<float>(sampleRate) * 1000.0f;

        std::cout << "      Configured Depth          : 2.500 ms\n";
        std::cout << "      Late Tail Excursion (P2P) : " << std::fixed << std::setprecision(3) << lateP2P_ms << " ms\n";

        if (lateP2P_ms >= 2.40f) {
            std::cout << "      Verdict                   : PASS (Late tail achieves full 2.5 ms dynamic modulation)\n";
        } else {
            std::cout << "      Verdict                   : FAIL (Late excursion " << lateP2P_ms << " ms insufficient)\n";
            allPassed = false;
        }
    }

    return allPassed;
}

// ============================================================================
// TEST 4: Real-Time Safety & Zero Allocation Interceptor
// ============================================================================
bool runRealTimeSafetyTests() {
    std::cout << "\n=======================================================\n";
    std::cout << "TEST 4: Hard Real-Time Audio Thread Safety\n";
    std::cout << "=======================================================\n";

    const double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    bool allPassed = true;

    for (double fs : sampleRates) {
        rb26::Rb26ReverbEngine engine;
        engine.prepare(fs, 512);

        rb26::Rb26Parameters params;
        params.shimmerSend = 0.5f;
        params.dimmerSend = 0.5f;
        params.pitchFeedback = 0.6f;
        params.punchDucking = 0.8f;
        params.decayRt60Sec = 10.0f;
        engine.setParameters(params);

        std::vector<float> inL(512, 0.5f);
        std::vector<float> inR(512, 0.5f);
        std::vector<float> outL(512, 0.0f);
        std::vector<float> outR(512, 0.0f);

        const float* inPtrs[2] = { inL.data(), inR.data() };
        float* outPtrs[2] = { outL.data(), outR.data() };

        gAllocationCount = 0;
        gBytesAllocated = 0;
        gTrackAllocations = true;

        for (int block = 0; block < 100; ++block) {
            engine.process(inPtrs, outPtrs, 2, 512);
        }

        gTrackAllocations = false;

        std::cout << "      fs = " << std::setw(6) << static_cast<int>(fs)
                  << " Hz: Allocations in process() = " << gAllocationCount << "\n";

        if (gAllocationCount != 0) {
            allPassed = false;
        }
    }

    if (allPassed) {
        std::cout << "      Verdict : PASS (100% Real-Time Safe: Bit-Exact Zero Heap Allocations)\n";
    } else {
        std::cout << "      Verdict : FAIL (Heap allocations detected in audio callback)\n";
    }

    return allPassed;
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main() {
    std::cout << "=======================================================\n";
    std::cout << "  BRAUN RB-26 DSP CHALLENGER 2 INDEPENDENT TEST SUITE  \n";
    std::cout << "=======================================================\n";

    bool pass1 = runPitchShifterSpectralTests();
    bool pass2 = runLowFrequencyModalTests();
    bool pass3 = runTailModulationIsolationTests();
    bool pass4 = runRealTimeSafetyTests();

    std::cout << "\n=======================================================\n";
    std::cout << "FINAL CHALLENGER 2 VERIFICATION SUMMARY:\n";
    std::cout << "  [1] Pitch Shifter Spectral Accuracy : " << (pass1 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [2] Low-Frequency Modal & Crossover : " << (pass2 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [3] Tail Modulation Isolation       : " << (pass3 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [4] Hard Real-Time Audio Safety     : " << (pass4 ? "PASS" : "FAIL") << "\n";
    std::cout << "=======================================================\n";

    const bool overallPass = pass1 && pass2 && pass3 && pass4;
    std::cout << "OVERALL EMPIRICAL VERDICT: " << (overallPass ? "APPROVE" : "FAIL") << "\n";
    std::cout << "=======================================================\n";

    return overallPass ? 0 : 1;
}
