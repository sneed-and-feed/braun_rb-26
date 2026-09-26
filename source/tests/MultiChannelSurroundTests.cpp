#include "dsp/ManifoldDelayNetwork.h"
#include "dsp/FdnReverbTank.h"
#include "dsp/Rb26Engine.h"
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <cassert>
#include <iomanip>

using namespace rb26;

// Global allocation counter for real-time safety verification
static size_t gAllocCount = 0;
#if defined(_WIN32)
// Custom hook or check
#endif

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            return false; \
        } \
    } while (0)

bool testManifoldDelayNetworkMultiChannel() {
    std::cout << "[MDN] Testing ManifoldDelayNetwork multi-channel extraction...\n";
    ManifoldDelayNetwork mdn;
    mdn.prepare(48000.0, 4.0f);

    const std::array<ManifoldType, 4> manifolds = {
        ManifoldType::PoincareHyperbolic,
        ManifoldType::WhisperingGallery,
        ManifoldType::AnharmonicPlate,
        ManifoldType::StockhausenKlangdom
    };

    const std::array<int, 7> channelCounts = { 1, 2, 4, 6, 8, 10, 12 };

    for (auto manifold : manifolds) {
        mdn.setManifold(manifold);

        // Test 1: Baseline extraction bounds across all layouts
        for (int chCount : channelCounts) {
            std::array<float, 8> lines = {{ 0.5f, -0.4f, 0.3f, -0.2f, 0.6f, -0.5f, 0.4f, -0.3f }};
            float outCh[12] = { 0.0f };
            mdn.extractMultiChannel(lines, outCh, chCount);

            for (int c = 0; c < chCount; ++c) {
                TEST_ASSERT(isFiniteBitwise(outCh[c]), "Non-finite output in extractMultiChannel");
                TEST_ASSERT(std::abs(outCh[c]) <= 1.05f, "Output exceeded 1.05 boundary knee bound");
            }

            // In 5.1, 7.1, 7.1.2, 7.1.4: Channel 3 (LFE) must be strictly 0.0f wet reverb
            if (chCount >= 6) {
                TEST_ASSERT(outCh[3] == 0.0f, "LFE wet reverb must be exactly 0.0f");
            }
        }

        // Test 2: Overload & Saturation Bounds (+40 dBFS impulse into lines)
        for (int chCount : channelCounts) {
            std::array<float, 8> overloadLines = {{ 100.0f, -80.0f, 60.0f, -50.0f, 90.0f, -70.0f, 40.0f, -30.0f }};
            float outCh[12] = { 0.0f };
            mdn.extractMultiChannel(overloadLines, outCh, chCount);

            for (int c = 0; c < chCount; ++c) {
                TEST_ASSERT(isFiniteBitwise(outCh[c]), "Non-finite output on overload");
                TEST_ASSERT(std::abs(outCh[c]) <= 1.05f, "Overload failed saturation ceiling (<= 1.05)");
            }
        }

        // Test 3: Center Decorrelation in 5.1 / 7.1 (Poincare)
        if (manifold == ManifoldType::PoincareHyperbolic) {
            // Under orthogonal unit impulses on delay lines, Center must be orthogonal to L and R
            // Line 0 impulse: L = +kNorm, C = +0.25
            // Line 2 impulse: L = +kNorm, C = -0.25
            // Sum of L*C over line 0 and line 2 = kNorm * 0.25 - kNorm * 0.25 = 0.0!
            float dotLC = 0.0f;
            float dotRC = 0.0f;
            for (size_t k = 0; k < 8; ++k) {
                std::array<float, 8> unitImpulse = {};
                unitImpulse[k] = 1.0f;
                float outCh[12] = { 0.0f };
                mdn.extractMultiChannel(unitImpulse, outCh, 6);
                dotLC += outCh[0] * outCh[2];
                dotRC += outCh[1] * outCh[2];
            }
            TEST_ASSERT(std::abs(dotLC) < 1.0e-5f, "Center channel not orthogonal to Left");
            TEST_ASSERT(std::abs(dotRC) < 1.0e-5f, "Center channel not orthogonal to Right");
        }

        // Test 4: 7.1.4 3D Spherical Elevation Mode Orthogonality
        if (manifold == ManifoldType::PoincareHyperbolic) {
            // Top Front Left (ch 8) must be orthogonal to Bed Front Left (ch 0)
            // Top Front Right (ch 9) must be orthogonal to Bed Front Right (ch 1)
            float dotFL_TFL = 0.0f;
            float dotFR_TFR = 0.0f;
            for (size_t k = 0; k < 8; ++k) {
                std::array<float, 8> unitImpulse = {};
                unitImpulse[k] = 1.0f;
                float outCh[12] = { 0.0f };
                mdn.extractMultiChannel(unitImpulse, outCh, 12);
                dotFL_TFL += outCh[0] * outCh[8];
                dotFR_TFR += outCh[1] * outCh[9];
            }
            TEST_ASSERT(std::abs(dotFL_TFL) < 1.0e-5f, "Top Front Left not orthogonal to Bed Front Left");
            TEST_ASSERT(std::abs(dotFR_TFR) < 1.0e-5f, "Top Front Right not orthogonal to Bed Front Right");
        }
    }

    std::cout << "  -> ManifoldDelayNetwork multi-channel PASS\n";
    return true;
}

bool testFdnTankMultiChannel() {
    std::cout << "[FDN] Testing FdnReverbTank multi-channel processing...\n";
    FdnReverbTank tank;
    tank.prepare(48000.0, 4.0f);
    tank.setParameters(1.0f, 2.5f, 5000.0f, 0.75f, false, 0.65f, 2.25f, 85.0f, ManifoldType::PoincareHyperbolic);

    // Test 1: Multi-channel impulse response
    const std::array<int, 7> channelCounts = { 1, 2, 4, 6, 8, 10, 12 };

    for (int chCount : channelCounts) {
        tank.reset();
        float inSample[12] = { 0.0f };
        inSample[0] = 1.0f; // Unit impulse on Left / Main
        if (chCount > 1) inSample[1] = 0.5f;

        float outSample[12] = { 0.0f };
        float* outPtrs[12];
        for (int c = 0; c < chCount; ++c) outPtrs[c] = &outSample[c];

        // Process impulse
        tank.processSampleMultiChannel(inSample, chCount, nullptr, outPtrs, chCount);

        for (int c = 0; c < chCount; ++c) {
            TEST_ASSERT(isFiniteBitwise(outSample[c]), "Non-finite output in FdnTank processSampleMultiChannel");
            TEST_ASSERT(std::abs(outSample[c]) <= 1.05f, "Output exceeded 1.05 bound in FdnTank");
        }
        if (chCount >= 6) {
            TEST_ASSERT(outSample[3] == 0.0f, "LFE wet reverb must be exactly 0.0f in FdnTank");
        }

        // Process 500 decay samples
        std::fill(std::begin(inSample), std::end(inSample), 0.0f);
        for (int i = 0; i < 500; ++i) {
            tank.processSampleMultiChannel(inSample, chCount, nullptr, outPtrs, chCount);
            for (int c = 0; c < chCount; ++c) {
                TEST_ASSERT(isFiniteBitwise(outSample[c]), "Non-finite in decay tail");
                TEST_ASSERT(std::abs(outSample[c]) <= 1.05f, "Decay sample exceeded bound");
            }
        }
    }

    std::cout << "  -> FdnReverbTank multi-channel PASS\n";
    return true;
}

bool testRb26EngineMultiChannel() {
    std::cout << "[ENGINE] Testing Rb26ReverbEngine multi-channel routing & Atmos...\n";
    Rb26ReverbEngine engine;
    engine.prepare(48000.0, 512);

    Rb26Parameters params;
    params.roomSize = 1.0f;
    params.decayRt60Sec = 3.0f;
    params.dryWetMix = 0.5f;
    params.limiterEnable = true;
    engine.setParameters(params);

    const std::array<int, 7> layouts = { 1, 2, 4, 6, 8, 10, 12 };

    for (int numCh : layouts) {
        engine.reset();
        const int blockSize = 256;

        std::vector<std::vector<float>> inBuffers(numCh, std::vector<float>(blockSize, 0.0f));
        std::vector<std::vector<float>> outBuffers(numCh, std::vector<float>(blockSize, 0.0f));

        std::vector<const float*> inPtrs(numCh);
        std::vector<float*> outPtrs(numCh);
        for (int c = 0; c < numCh; ++c) {
            inPtrs[c] = inBuffers[c].data();
            outPtrs[c] = outBuffers[c].data();
        }

        // Feed impulse on channel 0, and sine wave on LFE (channel 3 if available)
        inBuffers[0][0] = 1.0f;
        if (numCh >= 6) {
            for (int i = 0; i < blockSize; ++i) {
                inBuffers[3][i] = 0.5f * std::sin(2.0f * kPi * 60.0f * (float)i / 48000.0f);
            }
        }

        engine.process(inPtrs.data(), outPtrs.data(), numCh, blockSize);

        TEST_ASSERT(engine.getActiveChannelCount() == numCh, "getActiveChannelCount mismatch");
        TEST_ASSERT(engine.getActiveChannelLayout() == getLayoutForChannelCount(numCh), "getActiveChannelLayout mismatch");

        // Verify output boundedness and validity
        for (int c = 0; c < numCh; ++c) {
            for (int i = 0; i < blockSize; ++i) {
                const float s = outBuffers[c][i];
                TEST_ASSERT(isFiniteBitwise(s), "Non-finite sample in engine output");
                TEST_ASSERT(std::abs(s) <= 1.05f, "Sample exceeded 1.05 ceiling in engine output");
            }
        }

        // Verify LFE clean/dry pass-through:
        // LFE (channel 3) must match input exactly (uncolored by wet reverb)
        if (numCh >= 6) {
            for (int i = 0; i < blockSize; ++i) {
                const float diff = std::abs(outBuffers[3][i] - inBuffers[3][i]);
                TEST_ASSERT(diff < 1.0e-5f, "LFE channel was modified or tainted by reverb!");
            }
        }
    }

    // Test Extreme Overload Drive (+30 dBFS) through 7.1.4 Atmos
    {
        engine.reset();
        const int numCh = 12;
        const int blockSize = 128;
        std::vector<std::vector<float>> inBuffers(numCh, std::vector<float>(blockSize, 31.62f)); // +30 dB
        std::vector<std::vector<float>> outBuffers(numCh, std::vector<float>(blockSize, 0.0f));

        std::vector<const float*> inPtrs(numCh);
        std::vector<float*> outPtrs(numCh);
        for (int c = 0; c < numCh; ++c) {
            inPtrs[c] = inBuffers[c].data();
            outPtrs[c] = outBuffers[c].data();
        }

        engine.process(inPtrs.data(), outPtrs.data(), numCh, blockSize);

        for (int c = 0; c < numCh; ++c) {
            if (c == 3) continue; // LFE clean bypass
            for (int i = 0; i < blockSize; ++i) {
                TEST_ASSERT(isFiniteBitwise(outBuffers[c][i]), "Non-finite under +30dB overload");
                TEST_ASSERT(std::abs(outBuffers[c][i]) <= 1.05f, "C1 limiter failed to contain overload <= 1.05");
            }
        }
    }

    std::cout << "  -> Rb26ReverbEngine multi-channel PASS\n";
    return true;
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << "BRAUN RB-26 Multi-Channel / Surround / Atmos Tests\n";
    std::cout << "=======================================================\n";

    if (!testManifoldDelayNetworkMultiChannel()) return 1;
    if (!testFdnTankMultiChannel()) return 1;
    if (!testRb26EngineMultiChannel()) return 1;

    std::cout << "=======================================================\n";
    std::cout << "ALL MULTI-CHANNEL SURROUND & ATMOS TESTS PASSED (100%)\n";
    std::cout << "=======================================================\n";
    return 0;
}
