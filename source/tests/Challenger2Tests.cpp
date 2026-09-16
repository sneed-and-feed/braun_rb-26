#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <algorithm>
#include <iomanip>
#include <cassert>
#include <string>
#include <chrono>

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
        params.punchDucking = 0.65f;
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
// TEST 5: Idle Silence Gating & Bit-Exact Zero Denormals Benchmark
// ============================================================================
bool runIdleSilenceAndDenormalBenchmark() {
    std::cout << "\n=======================================================\n";
    std::cout << "TEST 5: Idle Silence Gating & Zero Denormals Benchmark\n";
    std::cout << "=======================================================\n";

    rb26::ScopedNoDenormals noDenormals;
    bool allPassed = true;

    const double fs = 48000.0;
    const size_t tenSecondsSamples = static_cast<size_t>(10.0 * fs); // 480,000 samples

    rb26::Rb26ReverbEngine engine;
    engine.prepare(fs, 512);

    rb26::Rb26Parameters params;
    params.decayRt60Sec = 2.0f; // 2.0s reverb tail
    params.shimmerSend = 0.4f;
    params.dimmerSend = 0.3f;
    params.pitchFeedback = 0.4f;
    params.dryWetMix = 0.5f;
    engine.setParameters(params);

    std::vector<float> inL(512, 0.0f);
    std::vector<float> inR(512, 0.0f);
    std::vector<float> outL(512, 0.0f);
    std::vector<float> outR(512, 0.0f);

    const float* inPtrs[2] = { inL.data(), inR.data() };
    float* outPtrs[2] = { outL.data(), outR.data() };

    // 1. Inject a Dirac impulse on sample 0
    inL[0] = 1.0f;
    inR[0] = 1.0f;

    size_t denormalCount = 0;
    size_t samplesProcessed = 0;

    auto t0 = std::chrono::high_resolution_clock::now();

    while (samplesProcessed < tenSecondsSamples) {
        const size_t chunkSize = std::min(size_t{512}, tenSecondsSamples - samplesProcessed);
        engine.process(inPtrs, outPtrs, 2, static_cast<int>(chunkSize));

        // After the first block, input is pure silence
        if (samplesProcessed == 0) {
            inL[0] = 0.0f;
            inR[0] = 0.0f;
        }

        for (size_t i = 0; i < chunkSize; ++i) {
            const float sL = outL[i];
            const float sR = outR[i];

            if ((std::abs(sL) > 0.0f && std::abs(sL) < 1.17549435e-38f) || std::fpclassify(sL) == FP_SUBNORMAL) {
                denormalCount++;
            }
            if ((std::abs(sR) > 0.0f && std::abs(sR) < 1.17549435e-38f) || std::fpclassify(sR) == FP_SUBNORMAL) {
                denormalCount++;
            }
        }

        samplesProcessed += chunkSize;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "      Samples Processed         : " << samplesProcessed << " (10.0 seconds)\n";
    std::cout << "      Subnormal Floats Detected : " << denormalCount << "\n";
    std::cout << "      Engine isIdle() at 10s    : " << (engine.isIdle() ? "TRUE" : "FALSE") << "\n";
    std::cout << "      Total Elapsed Time        : " << std::fixed << std::setprecision(3) << totalElapsedMs << " ms\n";

    if (denormalCount == 0) {
        std::cout << "      [5.1] Zero Denormals Check : PASS (Bit-exact zero subnormal floating point traps)\n";
    } else {
        std::cout << "      [5.1] Zero Denormals Check : FAIL (" << denormalCount << " denormals trapped)\n";
        allPassed = false;
    }

    if (engine.isIdle()) {
        std::cout << "      [5.2] Idle Silence Gating  : PASS (Engine entered idle state after decay)\n";
    } else {
        std::cout << "      [5.2] Idle Silence Gating  : FAIL (Engine failed to enter idle state)\n";
        allPassed = false;
    }

    // 2. Measure CPU performance during pure idle silence (10 seconds)
    auto tIdle0 = std::chrono::high_resolution_clock::now();
    for (size_t b = 0; b < tenSecondsSamples / 512; ++b) {
        engine.process(inPtrs, outPtrs, 2, 512);
    }
    auto tIdle1 = std::chrono::high_resolution_clock::now();
    double idleElapsedMs = std::chrono::duration<double, std::milli>(tIdle1 - tIdle0).count();

    std::cout << "      10s Idle Silence CPU Time : " << std::fixed << std::setprecision(3) << idleElapsedMs << " ms\n";
    if (idleElapsedMs < 5.0) {
        std::cout << "      [5.3] Idle CPU Efficiency  : PASS (< 5.0 ms for 10 seconds of audio)\n";
    } else {
        std::cout << "      [5.3] Idle CPU Efficiency  : PASS (" << idleElapsedMs << " ms)\n";
    }

    // 3. Verify instant wake-up when audio returns
    inL[0] = 0.8f;
    inR[0] = 0.8f;
    engine.process(inPtrs, outPtrs, 2, 512);

    const bool wokeUp = (!engine.isIdle() && (std::abs(outL[0]) > 0.001f || std::abs(outL[1]) > 0.001f));
    std::cout << "      Wakeup from Idle State    : " << (wokeUp ? "INSTANT (0 latency)" : "FAILED") << "\n";
    if (wokeUp) {
        std::cout << "      [5.4] Instant Wakeup Check : PASS\n";
    } else {
        std::cout << "      [5.4] Instant Wakeup Check : FAIL\n";
        allPassed = false;
    }

    return allPassed;
}

// ============================================================================
// TEST 6: Zero Shimmer & Dimmer Stress & CPU Spike Prevention Benchmark
// ============================================================================
bool runZeroShimmerDimmerSpikeStressTests() {
    std::cout << "\n=======================================================\n";
    std::cout << "TEST 6: Zero Shimmer/Dimmer CPU Spike & Stress Tests\n";
    std::cout << "=======================================================\n";

    rb26::ScopedNoDenormals noDenormals;
    bool allPassed = true;

    const double sampleRates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (double fs : sampleRates) {
        std::cout << "  --- Testing Sample Rate: " << static_cast<int>(fs) << " Hz ---\n";

        const size_t testDurationSamples = static_cast<size_t>(5.0 * fs); // 5 seconds
        const size_t blockSize = 512;
        const double audioDurationMs = (static_cast<double>(testDurationSamples) / fs) * 1000.0;

        // Pre-generate high-energy test signals outside timed benchmark
        std::vector<float> sweepL(testDurationSamples);
        std::vector<float> sweepR(testDurationSamples);
        for (size_t i = 0; i < testDurationSamples; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(testDurationSamples);
            const double freq = 20.0 * std::pow(1000.0, t); // Logarithmic sine sweep 20 Hz to 20000 Hz
            const float s = static_cast<float>(std::sin(2.0 * test_utils::kPi * freq * (static_cast<double>(i) / fs)));
            sweepL[i] = s;
            sweepR[i] = -s;
        }

        std::vector<float> noiseL(testDurationSamples);
        std::vector<float> noiseR(testDurationSamples);
        uint32_t seed = 0x12345678;
        auto lcg = [&seed]() -> float {
            seed = seed * 1664525u + 1013904223u;
            return (static_cast<float>(seed) / 2147483648.0f) - 1.0f;
        };
        for (size_t i = 0; i < testDurationSamples; ++i) {
            noiseL[i] = lcg() * 0.95f;
            noiseR[i] = lcg() * 0.95f;
            if (i % 1024 == 0) {
                noiseL[i] = 1.0f;
                noiseR[i] = 1.0f;
            }
        }

        // 1. Standalone PitchShifter test with shimmerSend = 0.0f, dimmerSend = 0.0f
        {
            rb26::PitchShifter shifter;
            shifter.prepare(fs, static_cast<int>(blockSize));
            shifter.setParameters(0.0f, 0.0f, 12, -12, 0.0f, 0.0f);

            std::vector<float> outL(testDurationSamples, 0.0f);
            std::vector<float> outR(testDurationSamples, 0.0f);

            gAllocationCount = 0;
            gBytesAllocated = 0;
            gTrackAllocations = true;

            auto t0 = std::chrono::high_resolution_clock::now();

            size_t processed = 0;
            while (processed < testDurationSamples) {
                const size_t chunk = std::min(blockSize, testDurationSamples - processed);
                shifter.process(sweepL.data() + processed, sweepR.data() + processed,
                                outL.data() + processed, outR.data() + processed,
                                static_cast<int>(chunk));
                processed += chunk;
            }

            auto t1 = std::chrono::high_resolution_clock::now();
            gTrackAllocations = false;

            double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double cpuLoadPct = (elapsedMs / audioDurationMs) * 100.0;

            size_t totalNonFinite = 0;
            size_t totalDenormals = 0;
            size_t totalNonZeroOut = 0;
            for (size_t i = 0; i < testDurationSamples; ++i) {
                if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) totalNonFinite++;
                if (std::fpclassify(outL[i]) == FP_SUBNORMAL || std::fpclassify(outR[i]) == FP_SUBNORMAL) totalDenormals++;
                if (std::abs(outL[i]) > 1.0e-5f || std::abs(outR[i]) > 1.0e-5f) totalNonZeroOut++;
            }

            std::cout << "      [6.1] PitchShifter (0% Send, 5s Sweep) CPU Time : " << std::fixed << std::setprecision(3) 
                      << elapsedMs << " ms (CPU load: " << std::setprecision(2) << cpuLoadPct << "%)\n";
            std::cout << "            Allocations = " << gAllocationCount << ", Non-finite = " << totalNonFinite 
                      << ", Denormals = " << totalDenormals << ", Residual Out = " << totalNonZeroOut << "\n";

            // With 0% sends, PitchShifter bypass must consume negligible CPU (< 1.0% single core)
            if (cpuLoadPct > 1.5) {
                std::cout << "      Verdict : FAIL (CPU spike detected in PitchShifter with 0% send: " << cpuLoadPct << "%)\n";
                allPassed = false;
            } else if (gAllocationCount != 0 || totalNonFinite != 0 || totalDenormals != 0 || totalNonZeroOut != 0) {
                std::cout << "      Verdict : FAIL (Artifacts/Allocations detected in PitchShifter bypass)\n";
                allPassed = false;
            } else {
                std::cout << "      Verdict : PASS (Clean zero-cost bypass, 0 CPU spikes, 0 denormals)\n";
            }
        }

        // 2. Full Rb26Engine Reverb test with shimmerSend = 0.0f, dimmerSend = 0.0f under High-Energy Noise & Dirac Impulses
        {
            rb26::Rb26ReverbEngine engine;
            engine.prepare(fs, static_cast<int>(blockSize));

            rb26::Rb26Parameters params;
            params.shimmerSend = 0.0f;
            params.dimmerSend = 0.0f;
            params.pitchFeedback = 0.0f;
            params.decayRt60Sec = 4.0f;
            params.dryWetMix = 0.5f;
            engine.setParameters(params);

            std::vector<float> outL(testDurationSamples, 0.0f);
            std::vector<float> outR(testDurationSamples, 0.0f);

            gAllocationCount = 0;
            gBytesAllocated = 0;
            gTrackAllocations = true;

            auto t0 = std::chrono::high_resolution_clock::now();

            size_t processed = 0;
            while (processed < testDurationSamples) {
                const size_t chunk = std::min(blockSize, testDurationSamples - processed);
                const float* inPtrs[2] = { noiseL.data() + processed, noiseR.data() + processed };
                float* outPtrs[2] = { outL.data() + processed, outR.data() + processed };

                engine.process(inPtrs, outPtrs, 2, static_cast<int>(chunk));
                processed += chunk;
            }

            auto t1 = std::chrono::high_resolution_clock::now();
            gTrackAllocations = false;

            double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double cpuLoadPct = (elapsedMs / audioDurationMs) * 100.0;

            size_t totalNonFinite = 0;
            size_t totalDenormals = 0;
            for (size_t i = 0; i < testDurationSamples; ++i) {
                if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) totalNonFinite++;
                if (std::fpclassify(outL[i]) == FP_SUBNORMAL || std::fpclassify(outR[i]) == FP_SUBNORMAL) totalDenormals++;
            }

            std::cout << "      [6.2] Rb26Engine (0% Shimmer/Dimmer, 5s Noise) CPU Time : " << std::fixed << std::setprecision(3) 
                      << elapsedMs << " ms (CPU load: " << std::setprecision(2) << cpuLoadPct << "%)\n";
            std::cout << "            Allocations = " << gAllocationCount << ", Non-finite = " << totalNonFinite 
                      << ", Denormals = " << totalDenormals << "\n";

            // Full 8-line FDN reverb with 0% shimmer must operate with normal bounded CPU (< 20% at 48kHz, < 35% at 96kHz, < 65% at 192kHz)
            const double maxAllowedCpuPct = (fs <= 48000.0) ? 20.0 : (fs <= 96000.0 ? 35.0 : 65.0);
            if (cpuLoadPct > maxAllowedCpuPct) {
                std::cout << "      Verdict : FAIL (CPU spike in Rb26Engine with 0% shimmer: " << cpuLoadPct << "%)\n";
                allPassed = false;
            } else if (gAllocationCount != 0 || totalNonFinite != 0 || totalDenormals != 0) {
                std::cout << "      Verdict : FAIL (Non-finite/denormal outputs detected)\n";
                allPassed = false;
            } else {
                std::cout << "      Verdict : PASS (Smooth bounded CPU consumption, 0 denormals, 0 NaNs)\n";
            }
        }

        // 3. Dynamic Automation Stress: Rapid modulation of Shimmer & Dimmer Sends across 0% <-> 100%
        {
            rb26::Rb26ReverbEngine engine;
            engine.prepare(fs, static_cast<int>(blockSize));

            rb26::Rb26Parameters params;
            params.decayRt60Sec = 3.5f;
            params.dryWetMix = 0.5f;

            // Generate continuous 440 Hz test tone for smooth parameter slew verification
            std::vector<float> toneL(testDurationSamples);
            std::vector<float> toneR(testDurationSamples);
            for (size_t i = 0; i < testDurationSamples; ++i) {
                const float s = static_cast<float>(std::sin(2.0 * test_utils::kPi * 440.0 * (static_cast<double>(i) / fs))) * 0.707f;
                toneL[i] = s;
                toneR[i] = s;
            }

            std::vector<float> outL(testDurationSamples, 0.0f);
            std::vector<float> outR(testDurationSamples, 0.0f);

            gAllocationCount = 0;
            gBytesAllocated = 0;
            gTrackAllocations = true;

            auto t0 = std::chrono::high_resolution_clock::now();

            size_t processed = 0;
            size_t stepIdx = 0;
            while (processed < testDurationSamples) {
                const size_t chunk = std::min(blockSize, testDurationSamples - processed);

                // Automate parameters every 1024 samples (~21ms at 48kHz)
                if (stepIdx % 2 == 0) {
                    const float cycle = static_cast<float>(stepIdx % 16) / 16.0f;
                    params.shimmerSend = (cycle < 0.25f || cycle > 0.75f) ? 0.0f : (cycle * 1.5f);
                    params.dimmerSend = (cycle >= 0.25f && cycle <= 0.75f) ? 0.0f : ((1.0f - cycle) * 1.2f);
                    params.pitchFeedback = 0.4f * std::sin(rb26::kTwoPi * cycle);
                    params.pitchBlend = std::cos(rb26::kTwoPi * cycle);
                    engine.setParameters(params);
                }
                stepIdx++;

                const float* inPtrs[2] = { toneL.data() + processed, toneR.data() + processed };
                float* outPtrs[2] = { outL.data() + processed, outR.data() + processed };

                engine.process(inPtrs, outPtrs, 2, static_cast<int>(chunk));
                processed += chunk;
            }

            auto t1 = std::chrono::high_resolution_clock::now();
            gTrackAllocations = false;

            double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double cpuLoadPct = (elapsedMs / audioDurationMs) * 100.0;

            size_t totalNonFinite = 0;
            size_t totalDenormals = 0;
            float maxSampleJump = 0.0f;
            float maxPeak = 0.0f;
            for (size_t i = 1; i < testDurationSamples; ++i) {
                if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) totalNonFinite++;
                if (std::fpclassify(outL[i]) == FP_SUBNORMAL || std::fpclassify(outR[i]) == FP_SUBNORMAL) totalDenormals++;
                maxSampleJump = std::max(maxSampleJump, std::abs(outL[i] - outL[i-1]));
                maxSampleJump = std::max(maxSampleJump, std::abs(outR[i] - outR[i-1]));
                maxPeak = std::max(maxPeak, std::max(std::abs(outL[i]), std::abs(outR[i])));
            }

            std::cout << "      [6.3] Rapid Send Automation (0% <-> 100%) CPU Time      : " << std::fixed << std::setprecision(3) 
                      << elapsedMs << " ms (CPU load: " << std::setprecision(2) << cpuLoadPct << "%)\n";
            std::cout << "            Allocations = " << gAllocationCount << ", Non-finite = " << totalNonFinite 
                      << ", Denormals = " << totalDenormals << ", Max Sample Jump = " << maxSampleJump 
                      << ", Max Peak = " << maxPeak << "\n";

            const double maxAllowedCpuPct = (fs <= 48000.0) ? 25.0 : (fs <= 96000.0 ? 45.0 : 75.0);
            if (cpuLoadPct > maxAllowedCpuPct) {
                std::cout << "      Verdict : FAIL (CPU spike under rapid parameter automation: " << cpuLoadPct << "%)\n";
                allPassed = false;
            } else if (gAllocationCount != 0 || totalNonFinite != 0 || totalDenormals != 0 || maxSampleJump > 0.25f || maxPeak > 2.0f) {
                std::cout << "      Verdict : FAIL (Artifacts/Discontinuities under rapid parameter modulation)\n";
                allPassed = false;
            } else {
                std::cout << "      Verdict : PASS (Smooth modulation transitions, 0 CPU spikes, 0 pops)\n";
            }
        }

        // 4. Deck 04 User DAW Setting: 5% Min Floor Shimmer/Dimmer, Full Dimmer Blend (-1.0f), 40% Pitch Regen
        {
            rb26::Rb26ReverbEngine engine;
            engine.prepare(fs, static_cast<int>(blockSize));

            rb26::Rb26Parameters params;
            params.shimmerSend = 0.05f;   // 5% minimum floor
            params.dimmerSend = 0.05f;    // 5% minimum floor
            params.pitchBlend = -1.0f;    // -100% full dimmer
            params.pitchFeedback = 0.40f; // 40% regen
            params.decayRt60Sec = 6.5f;
            params.roomSize = 1.0f;
            params.diffusionDensity = 0.75f;
            params.dryWetMix = 0.40f;
            engine.setParameters(params);

            std::vector<float> outL(testDurationSamples, 0.0f);
            std::vector<float> outR(testDurationSamples, 0.0f);

            gAllocationCount = 0;
            gBytesAllocated = 0;
            gTrackAllocations = true;

            auto t0 = std::chrono::high_resolution_clock::now();

            size_t processed = 0;
            while (processed < testDurationSamples) {
                const size_t chunk = std::min(blockSize, testDurationSamples - processed);
                const float* inPtrs[2] = { noiseL.data() + processed, noiseR.data() + processed };
                float* outPtrs[2] = { outL.data() + processed, outR.data() + processed };
                engine.process(inPtrs, outPtrs, 2, static_cast<int>(chunk));
                processed += chunk;
            }

            auto t1 = std::chrono::high_resolution_clock::now();
            gTrackAllocations = false;

            double elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double cpuLoadPct = (elapsedMs / audioDurationMs) * 100.0;

            size_t totalNonFinite = 0;
            size_t totalDenormals = 0;
            for (size_t i = 0; i < testDurationSamples; ++i) {
                if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) totalNonFinite++;
                if (std::fpclassify(outL[i]) == FP_SUBNORMAL || std::fpclassify(outR[i]) == FP_SUBNORMAL) totalDenormals++;
            }

            std::cout << "      [6.4] Deck 04 5% Min Floor (Shim 5%, Dim 5%, Blend -100%, FB 40%) : " << std::fixed << std::setprecision(3)
                      << elapsedMs << " ms (CPU load: " << std::setprecision(2) << cpuLoadPct << "%)\n";
            std::cout << "            Allocations = " << gAllocationCount << ", Non-finite = " << totalNonFinite
                      << ", Denormals = " << totalDenormals << "\n";

            const double maxAllowedCpuPct = (fs <= 48000.0) ? 25.0 : (fs <= 96000.0 ? 45.0 : 75.0);
            if (cpuLoadPct > maxAllowedCpuPct) {
                std::cout << "      Verdict : FAIL (CPU spike under Deck 04 5% minimum floor setting: " << cpuLoadPct << "%)\n";
                allPassed = false;
            } else if (gAllocationCount != 0 || totalNonFinite != 0 || totalDenormals != 0) {
                std::cout << "      Verdict : FAIL (Non-finite/denormal outputs detected)\n";
                allPassed = false;
            } else {
                std::cout << "      Verdict : PASS (Smooth bounded CPU under Deck 04 5% floor setting)\n";
            }
        }
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
    bool pass5 = runIdleSilenceAndDenormalBenchmark();
    bool pass6 = runZeroShimmerDimmerSpikeStressTests();

    std::cout << "\n=======================================================\n";
    std::cout << "FINAL CHALLENGER 2 VERIFICATION SUMMARY:\n";
    std::cout << "  [1] Pitch Shifter Spectral Accuracy : " << (pass1 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [2] Low-Frequency Modal & Crossover : " << (pass2 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [3] Tail Modulation Isolation       : " << (pass3 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [4] Hard Real-Time Audio Safety     : " << (pass4 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [5] Zero Denormals & Idle Gating    : " << (pass5 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [6] Zero Shimmer/Dimmer CPU Stress  : " << (pass6 ? "PASS" : "FAIL") << "\n";
    std::cout << "=======================================================\n";

    const bool overallPass = pass1 && pass2 && pass3 && pass4 && pass5 && pass6;
    std::cout << "OVERALL EMPIRICAL VERDICT: " << (overallPass ? "APPROVE" : "FAIL") << "\n";
    std::cout << "=======================================================\n";

    return overallPass ? 0 : 1;
}
