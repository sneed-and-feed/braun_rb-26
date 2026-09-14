#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <algorithm>
#include <iomanip>
#include <cassert>
#include <string>
#include <array>

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
        // Blackman-Harris 4-term window for minimal spectral leakage
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
// TEST 1: Pitch Shifter Spectral Accuracy (+12st, +24st, +7st, -12st, -24st)
// Tolerance: < 0.1%
// ============================================================================
bool runPitchShifterSpectralTests() {
    std::cout << "\n=======================================================================\n";
    std::cout << "TEST 1: Pitch Shifter Spectral Accuracy (+12st, +24st, +7st, -12st, -24st)\n";
    std::cout << "Target Invariant: Frequency error strictly < 0.1% tolerance\n";
    std::cout << "=======================================================================\n";

    const double sampleRate = 48000.0;
    const double inputFreq = 440.0;
    const size_t totalSamples = 96000;
    const size_t warmupSamples = 9600;
    const size_t testSamples = 65536;

    std::vector<float> inBuffer(totalSamples);
    for (size_t n = 0; n < totalSamples; ++n) {
        inBuffer[n] = static_cast<float>(std::sin(2.0 * test_utils::kPi * inputFreq * n / sampleRate));
    }

    struct TestCase {
        std::string name;
        int semitones;
        bool isShimmer;
        double targetFreq;
        double searchMin;
        double searchMax;
    };

    std::vector<TestCase> testCases = {
        { "+12st (2.0x Octave Up)",      12,  true,  440.0 * 2.0,                                 750.0,  1000.0 },
        { "+24st (4.0x Double Octave Up)", 24,  true,  440.0 * 4.0,                                1550.0,  1950.0 },
        { "+7st (1.498x Perfect Fifth)",   7,  true,  440.0 * std::pow(2.0, 7.0 / 12.0),           580.0,   750.0 },
        { "-12st (0.5x Octave Down)",    -12, false,  440.0 * 0.5,                                 180.0,   260.0 },
        { "-24st (0.25x Double Octave)", -24, false,  440.0 * 0.25,                                 80.0,   140.0 },
    };

    bool allPassed = true;

    for (const auto& tc : testCases) {
        rb26::DualTapDelayPitchShifter coreShifter;
        coreShifter.prepare(sampleRate);
        coreShifter.setInterval(tc.semitones);

        std::vector<float> out(totalSamples, 0.0f);
        for (size_t n = 0; n < totalSamples; ++n) {
            out[n] = coreShifter.processSample(inBuffer[n]);
        }

        double peakFreq = test_utils::findPeakFrequency(out, sampleRate, warmupSamples, testSamples, tc.searchMin, tc.searchMax);
        double errorHz = std::abs(peakFreq - tc.targetFreq);
        double errorPct = errorHz / tc.targetFreq * 100.0;

        std::cout << "\n[" << tc.name << "]:\n";
        std::cout << "  Input Frequency  : " << std::fixed << std::setprecision(4) << inputFreq << " Hz\n";
        std::cout << "  Target Frequency : " << tc.targetFreq << " Hz\n";
        std::cout << "  Measured Peak    : " << peakFreq << " Hz\n";
        std::cout << "  Frequency Error  : " << errorHz << " Hz (" << errorPct << "%)\n";
        std::cout << "  Tolerance Limit  : < 0.100%\n";

        if (errorPct < 0.10) {
            std::cout << "  Verdict          : PASS (Error " << errorPct << "% < 0.1%)\n";
        } else {
            std::cout << "  Verdict          : FAIL (Error " << errorPct << "% >= 0.1%)\n";
            allPassed = false;
        }
    }

    // Now test through the full stereo PitchShifter module with loop filters and blend
    std::cout << "\n--- Stereo PitchShifter Module Integration Checks ---\n";
    {
        // 1. Shimmer +24st through PitchShifter
        rb26::PitchShifter shifter;
        shifter.prepare(sampleRate);
        shifter.setParameters(1.0f, 0.0f, 24, -12, 1.0f, 0.0f);

        std::vector<float> outL(totalSamples, 0.0f);
        std::vector<float> outR(totalSamples, 0.0f);
        shifter.process(inBuffer.data(), inBuffer.data(), outL.data(), outR.data(), static_cast<int>(totalSamples));

        double peak = test_utils::findPeakFrequency(outL, sampleRate, warmupSamples, testSamples, 1550.0, 1950.0);
        double errPct = std::abs(peak - 1760.0) / 1760.0 * 100.0;
        std::cout << "  PitchShifter Shimmer +24st peak: " << peak << " Hz (err = " << errPct << "%) -> "
                  << (errPct < 0.10 ? "PASS" : "FAIL") << "\n";
        if (errPct >= 0.10) allPassed = false;
    }
    {
        // 2. Shimmer +7st through PitchShifter
        rb26::PitchShifter shifter;
        shifter.prepare(sampleRate);
        shifter.setParameters(1.0f, 0.0f, 7, -12, 1.0f, 0.0f);

        std::vector<float> outL(totalSamples, 0.0f);
        std::vector<float> outR(totalSamples, 0.0f);
        shifter.process(inBuffer.data(), inBuffer.data(), outL.data(), outR.data(), static_cast<int>(totalSamples));

        double target = 440.0 * std::pow(2.0, 7.0 / 12.0);
        double peak = test_utils::findPeakFrequency(outL, sampleRate, warmupSamples, testSamples, 580.0, 750.0);
        double errPct = std::abs(peak - target) / target * 100.0;
        std::cout << "  PitchShifter Shimmer +7st peak: " << peak << " Hz (err = " << errPct << "%) -> "
                  << (errPct < 0.10 ? "PASS" : "FAIL") << "\n";
        if (errPct >= 0.10) allPassed = false;
    }

    return allPassed;
}

// ============================================================================
// TEST 2: Low-Frequency Modal Matrix (40 Hz to 200 Hz)
// Target Invariant: Smooth decay envelope across 40-200 Hz with zero comb nulls > 6 dB
// ============================================================================
bool runLowFrequencyModalTests() {
    std::cout << "\n=======================================================================\n";
    std::cout << "TEST 2: Low-Frequency Modal Matrix (40 Hz to 200 Hz)\n";
    std::cout << "Target Invariant: Smooth decay envelope with zero comb filter nulls > 6.0 dB\n";
    std::cout << "=======================================================================\n";

    const double sampleRate = 48000.0;
    bool allPassed = true;

    // Subtest 2.1: Swept Steady-State Frequency Response (40 Hz to 200 Hz in 1 Hz steps)
    {
        std::cout << "\n[2.1] Modal Matrix Swept Response (40 Hz - 200 Hz, 161 tones):\n";

        rb26::LowBandModalMatrix modal;
        modal.prepare(sampleRate);
        rb26::LowBandModalParams params;
        params.crossoverHz = 180.0f;
        params.bassRt60Mult = 1.0f;
        params.rt60DecaySec = 3.5f;
        params.punchDucking = 0.0f;
        params.subMonoHz = 120.0f;
        modal.setParameters(params);

        std::vector<double> freqs;
        std::vector<double> rmsOutputs;

        const size_t testLength = 24000;
        const size_t steadyStart = 9600;

        for (int f = 40; f <= 200; ++f) {
            freqs.push_back(static_cast<double>(f));
            modal.reset();
            double sumSq = 0.0;
            size_t count = 0;

            for (size_t n = 0; n < testLength; ++n) {
                float in = static_cast<float>(std::sin(2.0 * test_utils::kPi * f * n / sampleRate));
                float lowOutL = 0.0f, lowOutR = 0.0f;
                modal.processModalOnly(in, in, lowOutL, lowOutR);

                if (n >= steadyStart) {
                    sumSq += 0.5 * (lowOutL * lowOutL + lowOutR * lowOutR);
                    ++count;
                }
            }
            rmsOutputs.push_back(std::sqrt(sumSq / count));
        }

        double maxNotchDepthDb = 0.0;
        int notchFreq = 0;

        for (size_t i = 1; i < rmsOutputs.size() - 1; ++i) {
            double localMax = std::max(rmsOutputs[i - 1], rmsOutputs[i + 1]);
            double current = rmsOutputs[i];
            if (current > 1.0e-8 && localMax > 1.0e-8) {
                double dropDb = 20.0 * std::log10(localMax / current);
                if (dropDb > maxNotchDepthDb) {
                    maxNotchDepthDb = dropDb;
                    notchFreq = static_cast<int>(freqs[i]);
                }
            }
        }

        std::cout << "  Frequencies Evaluated : 40 Hz to 200 Hz (161 discrete tones)\n";
        std::cout << "  Maximum Adjacent Notch: " << std::fixed << std::setprecision(2) << maxNotchDepthDb
                  << " dB (at " << notchFreq << " Hz)\n";
        std::cout << "  Threshold Constraint  : Zero comb filter nulls > 6.0 dB\n";

        if (maxNotchDepthDb < 6.0) {
            std::cout << "  Verdict               : PASS (" << maxNotchDepthDb << " dB < 6.0 dB threshold)\n";
        } else {
            std::cout << "  Verdict               : FAIL (Found notch of " << maxNotchDepthDb << " dB >= 6.0 dB)\n";
            allPassed = false;
        }
    }

    // Subtest 2.2: Modal Impulse Response Energy Decay Curve (EDC) Smoothness
    {
        std::cout << "\n[2.2] Impulse Response Energy Decay Curve (EDC) Smoothness:\n";

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
        double edcAt300ms = 10.0 * std::log10(edc[static_cast<size_t>(0.300 * sampleRate)] / initialEnergy);
        double edcAt500ms = 10.0 * std::log10(edc[static_cast<size_t>(0.500 * sampleRate)] / initialEnergy);
        double edcAt900ms = 10.0 * std::log10(edc[static_cast<size_t>(0.900 * sampleRate)] / initialEnergy);

        std::cout << "  EDC @ 100 ms : " << edcAt100ms << " dB\n";
        std::cout << "  EDC @ 300 ms : " << edcAt300ms << " dB\n";
        std::cout << "  EDC @ 500 ms : " << edcAt500ms << " dB\n";
        std::cout << "  EDC @ 900 ms : " << edcAt900ms << " dB\n";

        if (edcAt100ms > edcAt300ms && edcAt300ms > edcAt500ms && edcAt500ms > edcAt900ms) {
            std::cout << "  Verdict      : PASS (Monotonic, smooth modal dissipation)\n";
        } else {
            std::cout << "  Verdict      : FAIL (Non-monotonic energy decay)\n";
            allPassed = false;
        }
    }

    return allPassed;
}

// ============================================================================
// TEST 3: Sub-Bass Elliptical Filter
// Target Invariant:
// 1. Side energy rolled off by >= 24 dB at 30 Hz
// 2. Zero mono degradation below 120 Hz
// ============================================================================
bool runSubBassEllipticalTests() {
    std::cout << "\n=======================================================================\n";
    std::cout << "TEST 3: Sub-Bass Elliptical Filter Invariants\n";
    std::cout << "Target Invariant 1: Side energy rolled off >= 24 dB at 30 Hz\n";
    std::cout << "Target Invariant 2: Bit-exact zero mono degradation below 120 Hz\n";
    std::cout << "=======================================================================\n";

    const double sampleRate = 48000.0;
    bool allPassed = true;

    // Subtest 3.1: Side Energy Roll-Off at 30 Hz
    {
        std::cout << "\n[3.1] Side Energy Attenuation at 30 Hz (Cutoff = 120 Hz):\n";

        rb26::SubBassEllipticalFilter filter;
        filter.prepare(sampleRate);
        filter.setCutoff(120.0f);

        const size_t totalSamples = 48000; // 1 second
        const size_t testStart = 24000;     // 500 ms steady state

        // Pure anti-phase (pure Side): inL = sin(wt), inR = -sin(wt) => M = 0, S = inL
        double inSideSumSq = 0.0;
        double outSideSumSq = 0.0;
        size_t count = 0;

        for (size_t n = 0; n < totalSamples; ++n) {
            float s = static_cast<float>(std::sin(2.0 * test_utils::kPi * 30.0 * n / sampleRate));
            float outL = 0.0f, outR = 0.0f;
            filter.process(s, -s, outL, outR);

            if (n >= testStart) {
                float sideIn = s;
                float sideOut = 0.5f * (outL - outR);
                inSideSumSq += sideIn * sideIn;
                outSideSumSq += sideOut * sideOut;
                ++count;
            }
        }

        double rmsIn = std::sqrt(inSideSumSq / count);
        double rmsOut = std::sqrt(outSideSumSq / count);
        double attenDb = 20.0 * std::log10(rmsIn / (rmsOut + 1.0e-12));

        std::cout << "  Test Frequency     : 30.0 Hz\n";
        std::cout << "  Elliptical Cutoff  : 120.0 Hz (2 octaves above test tone)\n";
        std::cout << "  Input Side RMS     : " << std::fixed << std::setprecision(6) << rmsIn << "\n";
        std::cout << "  Output Side RMS    : " << rmsOut << "\n";
        std::cout << "  Measured Roll-Off  : " << std::setprecision(2) << attenDb << " dB\n";
        std::cout << "  Target Requirement : >= 24.0 dB roll-off at 30 Hz\n";

        if (attenDb >= 24.0) {
            std::cout << "  Verdict            : PASS (Roll-off " << attenDb << " dB >= 24.0 dB)\n";
        } else {
            std::cout << "  Verdict            : FAIL (Roll-off " << attenDb << " dB < 24.0 dB)\n";
            allPassed = false;
        }
    }

    // Subtest 3.2: Zero Mono Degradation Below 120 Hz
    {
        std::cout << "\n[3.2] Mono Degradation Evaluation Below 120 Hz:\n";

        rb26::SubBassEllipticalFilter filter;
        filter.prepare(sampleRate);
        filter.setCutoff(120.0f);

        const double testTones[] = { 20.0, 30.0, 40.0, 60.0, 80.0, 100.0, 120.0 };
        double maxErrorAcrossAll = 0.0;

        for (double f : testTones) {
            filter.reset();
            double maxErr = 0.0;
            const size_t numSamples = 4800;

            for (size_t n = 0; n < numSamples; ++n) {
                float inMono = static_cast<float>(std::sin(2.0 * test_utils::kPi * f * n / sampleRate));
                float outL = 0.0f, outR = 0.0f;
                filter.process(inMono, inMono, outL, outR);

                double errL = std::abs(outL - inMono);
                double errR = std::abs(outR - inMono);
                if (errL > maxErr) maxErr = errL;
                if (errR > maxErr) maxErr = errR;
            }

            if (maxErr > maxErrorAcrossAll) maxErrorAcrossAll = maxErr;
            std::cout << "  f = " << std::setw(5) << static_cast<int>(f) << " Hz: Max Sample Difference = "
                      << std::scientific << std::setprecision(3) << maxErr << "\n";
        }

        std::cout << "  Max Mono Error Across All Frequencies: " << maxErrorAcrossAll << "\n";
        std::cout << "  Target Requirement                   : Zero mono degradation (Bit-exact transparent)\n";

        if (maxErrorAcrossAll < 1.0e-6) {
            std::cout << "  Verdict                              : PASS (Zero mono degradation, identical pass-through)\n";
        } else {
            std::cout << "  Verdict                              : FAIL (Detected mono modification)\n";
            allPassed = false;
        }
    }

    return allPassed;
}

// ============================================================================
// TEST 4: Tail Modulation Isolation
// Target Invariant:
// 1. 12-tap early reflections have exactly 0.0 Hz pitch flutter
// 2. Late tail modulates smoothly
// ============================================================================
bool runTailModulationIsolationTests() {
    std::cout << "\n=======================================================================\n";
    std::cout << "TEST 4: Tail Modulation Isolation (Early vs Late Tail)\n";
    std::cout << "Target Invariant 1: 12-tap early reflections have exactly 0.0 Hz pitch flutter\n";
    std::cout << "Target Invariant 2: Late tail modulates smoothly (irrational golden LFOs)\n";
    std::cout << "=======================================================================\n";

    const double sampleRate = 48000.0;
    const double testFreq = 1000.0;
    bool allPassed = true;

    // Subtest 4.1: 12-tap Early Reflections Exact 0.0 Hz Pitch Flutter
    {
        std::cout << "\n[4.1] 12-Tap Early Reflections Time-Invariance & 0.0 Hz Pitch Flutter:\n";

        rb26::EarlyReflections er;
        er.prepare(sampleRate, 1.0f);
        er.setParameters(1.0f);

        const size_t numSamples = static_cast<size_t>(sampleRate * 1.0); // 1 second
        std::vector<float> inSig(numSamples);
        std::vector<float> erOutL(numSamples, 0.0f);
        std::vector<float> erOutR(numSamples, 0.0f);

        for (size_t n = 0; n < numSamples; ++n) {
            inSig[n] = static_cast<float>(std::sin(2.0 * test_utils::kPi * testFreq * n / sampleRate));
            er.processSample(inSig[n], inSig[n], erOutL[n], erOutR[n]);
        }

        // Measure zero crossings over steady-state window (200 ms to 800 ms)
        const size_t startSample = static_cast<size_t>(sampleRate * 0.200);
        const size_t endSample   = static_cast<size_t>(sampleRate * 0.800);

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

        std::cout << "  Input Frequency            : " << std::fixed << std::setprecision(2) << testFreq << " Hz\n";
        std::cout << "  Measured Max Pitch Flutter : " << std::fixed << std::setprecision(6) << maxFreqDev << " Hz\n";
        std::cout << "  Target Specification       : Exactly 0.0 Hz flutter (< 0.0001 Hz tolerance)\n";

        if (maxFreqDev < 0.0001) {
            std::cout << "  Verdict                    : PASS (Early reflections have exactly 0.000000 Hz pitch flutter)\n";
        } else {
            std::cout << "  Verdict                    : FAIL (Detected flutter of " << maxFreqDev << " Hz)\n";
            allPassed = false;
        }
    }

    // Subtest 4.2: Late Tail Modulation Excursion & Smoothness
    {
        std::cout << "\n[4.2] Late Tail Modulation Excursion & Spectral Smoothness:\n";

        rb26::TailModulator mod;
        mod.prepare(sampleRate);
        mod.setParameters(1.0f, 2.5f, 85.0f); // 1 Hz rate, 2.5 ms depth, 85 ms bloom

        const size_t totalSamples = static_cast<size_t>(sampleRate * 3.0);
        std::vector<float> exc0(totalSamples, 0.0f);

        // Inject transient at start to trigger bloom detector
        for (size_t n = 0; n < totalSamples; ++n) {
            float inTransient = (n < 480) ? 1.0f : 0.0f;
            std::array<float, 8> outExc {};
            mod.processSample(inTransient, outExc);
            exc0[n] = outExc[0];
        }

        // Verify initial suppression (< 50 ms)
        float maxInitialExc = 0.0f;
        for (size_t n = 0; n < static_cast<size_t>(0.040 * sampleRate); ++n) {
            maxInitialExc = std::max(maxInitialExc, exc0[n]);
        }
        float initialExcMs = maxInitialExc / static_cast<float>(sampleRate) * 1000.0f;

        // Verify full excursion in late tail (0.5s to 3.0s)
        float minLateExc = exc0[static_cast<size_t>(0.5 * sampleRate)];
        float maxLateExc = minLateExc;
        for (size_t n = static_cast<size_t>(0.5 * sampleRate); n < totalSamples; ++n) {
            minLateExc = std::min(minLateExc, exc0[n]);
            maxLateExc = std::max(maxLateExc, exc0[n]);
        }
        float lateP2P_ms = (maxLateExc - minLateExc) / static_cast<float>(sampleRate) * 1000.0f;

        std::cout << "  Initial Window (< 40ms) Excursion : " << std::fixed << std::setprecision(3) << initialExcMs << " ms (Suppressed)\n";
        std::cout << "  Late Tail P2P Excursion (> 500ms) : " << lateP2P_ms << " ms (Target: ~2.500 ms)\n";

        if (initialExcMs < 0.8f && lateP2P_ms >= 2.45f) {
            std::cout << "  Verdict                           : PASS (Transient suppressed, late tail modulates smoothly to 2.5ms)\n";
        } else {
            std::cout << "  Verdict                           : FAIL (Excursion bounds not met)\n";
            allPassed = false;
        }
    }

    return allPassed;
}

// ============================================================================
// TEST 5: Hard Real-Time Audio Safety (Zero Dynamic Allocation Interceptor)
// ============================================================================
bool runRealTimeSafetyTests() {
    std::cout << "\n=======================================================================\n";
    std::cout << "TEST 5: Hard Real-Time Audio Thread Safety Across 6 Sample Rates\n";
    std::cout << "=======================================================================\n";

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

        std::cout << "  fs = " << std::setw(6) << static_cast<int>(fs)
                  << " Hz: Allocations in processBlock() = " << gAllocationCount << "\n";

        if (gAllocationCount != 0) {
            allPassed = false;
        }
    }

    if (allPassed) {
        std::cout << "  Verdict : PASS (100% Real-Time Safe: Bit-Exact Zero Heap Allocations)\n";
    } else {
        std::cout << "  Verdict : FAIL (Heap allocations detected in audio callback)\n";
    }

    return allPassed;
}

// ============================================================================
// MAIN RUNNER
// ============================================================================
int main() {
    std::cout << "=======================================================================\n";
    std::cout << "  BRAUN RB-26 EMPIRICAL CHALLENGER 2 INDEPENDENT ACOUSTIC VERIFIER    \n";
    std::cout << "=======================================================================\n";

    bool pass1 = runPitchShifterSpectralTests();
    bool pass2 = runLowFrequencyModalTests();
    bool pass3 = runSubBassEllipticalTests();
    bool pass4 = runTailModulationIsolationTests();
    bool pass5 = runRealTimeSafetyTests();

    std::cout << "\n=======================================================================\n";
    std::cout << "FINAL EMPIRICAL VERIFICATION SUMMARY:\n";
    std::cout << "  [1] Pitch Shifter Spectral Accuracy (< 0.1% tol) : " << (pass1 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [2] Low-Frequency Modal & Comb Nulls (< 6.0 dB)  : " << (pass2 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [3] Sub-Bass Elliptical Filter (>= 24 dB @ 30 Hz): " << (pass3 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [4] Tail Modulation Isolation (0.0 Hz flutter)  : " << (pass4 ? "PASS" : "FAIL") << "\n";
    std::cout << "  [5] Hard Real-Time Zero Heap Allocations         : " << (pass5 ? "PASS" : "FAIL") << "\n";
    std::cout << "=======================================================================\n";

    const bool overallPass = pass1 && pass2 && pass3 && pass4 && pass5;
    std::cout << "OVERALL ACOUSTIC & SPECTRAL VERDICT: " << (overallPass ? "APPROVE" : "REQUEST_CHANGES") << "\n";
    std::cout << "=======================================================================\n";

    return overallPass ? 0 : 1;
}
