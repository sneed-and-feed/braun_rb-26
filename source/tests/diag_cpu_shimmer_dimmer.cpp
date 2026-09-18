#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include "DspMath.h"
#include "Rb26Engine.h"

int main() {
    std::cout << "=========================================================\n";
    std::cout << "DIAGNOSTIC: CPU Benchmark & Stability for Shimmer/Dimmer\n";
    std::cout << "=========================================================\n";

    const double sampleRate = 48000.0;
    const int blockSize = 512;
    const int numBlocks = 2000; // ~21 seconds of audio

    rb26::Rb26ReverbEngine engine;
    engine.prepare(sampleRate, blockSize);

    struct TestCase {
        const char* name;
        float shimmerSend;
        float dimmerSend;
        int shimmerInterval;
        int dimmerInterval;
        float pitchBlend;
        float pitchFeedback;
        float roomSize;
        float decayRt60;
        float highDamping;
    };

    std::vector<TestCase> testCases = {
        { "Anon Exact (0% Shim, 10% Dim, -80% Blend, 11% FB)", 0.0f, 0.10f, 7, -2, -0.80f, 0.11f, 2.0f, 1.26f, 20000.0f },
        { "Both 0% (Bypassed)", 0.0f, 0.0f, 12, -12, 0.0f, 0.0f, 1.0f, 6.5f, 1800.0f },
        { "100% Shimmer, 0% Dimmer", 1.0f, 0.0f, 12, -12, 1.0f, 0.45f, 1.0f, 6.5f, 1800.0f },
        { "0% Shimmer, 100% Dimmer", 0.0f, 1.0f, 12, -12, -1.0f, 0.45f, 1.0f, 6.5f, 1800.0f },
        { "100% Both Shimmer & Dimmer", 1.0f, 1.0f, 12, -12, 0.0f, 0.45f, 1.0f, 6.5f, 1800.0f },
        { "Extreme Max (100% Shim, 100% Dim, 95% FB, 30s RT60)", 1.0f, 1.0f, 24, -12, 0.0f, 0.95f, 4.0f, 30.0f, 20000.0f }
    };

    std::vector<float> inL(blockSize);
    std::vector<float> inR(blockSize);
    std::vector<float> outL(blockSize);
    std::vector<float> outR(blockSize);
    const float* inChannels[2] = { inL.data(), inR.data() };
    float* outChannels[2] = { outL.data(), outR.data() };

    for (const auto& tc : testCases) {
        engine.reset();
        rb26::Rb26Parameters p;
        p.shimmerSend = tc.shimmerSend;
        p.dimmerSend = tc.dimmerSend;
        p.shimmerInterval = tc.shimmerInterval;
        p.dimmerInterval = tc.dimmerInterval;
        p.pitchBlend = tc.pitchBlend;
        p.pitchFeedback = tc.pitchFeedback;
        p.roomSize = tc.roomSize;
        p.decayRt60Sec = tc.decayRt60;
        p.highDampingHz = tc.highDamping;
        p.diffusionDensity = 0.27f;
        p.punchDucking = 0.80f;
        p.lowCrossoverHz = 269.0f;
        p.bassRt60Mult = 2.0f;
        p.subMonoHz = 211.0f;
        p.preDelayMs = 24.0f;
        p.tailModRateHz = 0.05f;
        p.tailModDepthMs = 3.0f;
        p.tailBloomMs = 250.0f;
        p.stereoWidth = 1.0f;
        p.dryWetMix = 0.40f;
        p.earlyLateMix = 0.50f;
        engine.setParameters(p);

        // Fill with realistic audio: sine wave with occasional transients
        for (int i = 0; i < blockSize; ++i) {
            inL[i] = static_cast<float>(0.3 * std::sin(2.0 * 3.141592653589793 * 440.0 * i / sampleRate));
            inR[i] = static_cast<float>(0.3 * std::cos(2.0 * 3.141592653589793 * 440.0 * i / sampleRate));
        }

        // Warm up 100 blocks
        for (int b = 0; b < 100; ++b) {
            engine.process(inChannels, outChannels, 2, blockSize);
        }

        bool hasNaN = false;
        bool hasInf = false;
        bool hasDenormal = false;
        float peak = 0.0f;

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int b = 0; b < numBlocks; ++b) {
            engine.process(inChannels, outChannels, 2, blockSize);
            for (int i = 0; i < blockSize; ++i) {
                float vL = outL[i];
                float vR = outR[i];
                if (std::isnan(vL) || std::isnan(vR)) hasNaN = true;
                if (std::isinf(vL) || std::isinf(vR)) hasInf = true;
                if (std::fpclassify(vL) == FP_SUBNORMAL || std::fpclassify(vR) == FP_SUBNORMAL) hasDenormal = true;
                peak = std::max(peak, std::max(std::abs(vL), std::abs(vR)));
            }
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        double audioTimeUs = (double(numBlocks * blockSize) / sampleRate) * 1e6;
        double cpuPercent = (elapsedUs / audioTimeUs) * 100.0;

        std::cout << "\nTest: " << tc.name << "\n";
        std::cout << "  Elapsed Time: " << std::fixed << std::setprecision(1) << (elapsedUs / 1000.0) << " ms for " 
                  << (audioTimeUs / 1000.0) << " ms audio\n";
        std::cout << "  Core CPU Load: " << std::setprecision(2) << cpuPercent << " % of 1 Core\n";
        std::cout << "  Peak Output: " << std::setprecision(3) << peak << "\n";
        std::cout << "  NaN: " << (hasNaN ? "FAIL" : "NO") 
                  << " | Inf: " << (hasInf ? "FAIL" : "NO")
                  << " | Denormal: " << (hasDenormal ? "FAIL" : "NO") << "\n";
    }

    return 0;
}
