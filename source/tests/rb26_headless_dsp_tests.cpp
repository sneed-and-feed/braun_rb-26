#include <iostream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <string>
#include <cstdlib>

// Operator new overrides must precede other includes
bool gTrackAllocations = false;
size_t gAllocationCount = 0;
size_t gBytesAllocated = 0;

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

void* operator new[](size_t size) {
    if (gTrackAllocations) {
        ++gAllocationCount;
        gBytesAllocated += size;
    }
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    return p;
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p, size_t) noexcept {
    std::free(p);
}

#include "TestHarness.h"
#include "Tier1_FeatureTests.h"
#include "Tier2_BoundaryTests.h"
#include "Tier3_PairwiseTests.h"
#include "Tier4_ScenarioTests.h"

int main() {
    std::cout << "================================================================================\n";
    std::cout << "    BRAUN RB-26 STUDIO REVERB — 4-TIER E2E HEADLESS DSP VERIFICATION RUNNER     \n";
    std::cout << "================================================================================\n\n";

    // 1. Register all test suites
    test::registerTier1Tests();
    test::registerTier2Tests();
    test::registerTier3Tests();
    test::registerTier4Tests();

    auto& registry = test::getTestRegistry();
    const size_t totalTests = registry.size();

    std::cout << "Registered Test Cases: " << totalTests << "\n";
    std::cout << "--------------------------------------------------------------------------------\n\n";

    int tier1Total = 0, tier1Pass = 0;
    int tier2Total = 0, tier2Pass = 0;
    int tier3Total = 0, tier3Pass = 0;
    int tier4Total = 0, tier4Pass = 0;
    int overallPass = 0;
    int overallFail = 0;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < totalTests; ++i) {
        const auto& testCase = registry[i];
        test::gCurrentTestAssertFailures = 0;

        auto t0 = std::chrono::high_resolution_clock::now();
        bool success = false;
        try {
            success = testCase.run();
        } catch (const std::exception& e) {
            std::cerr << "      EXCEPTION: " << e.what() << "\n";
            success = false;
        } catch (...) {
            std::cerr << "      UNKNOWN EXCEPTION\n";
            success = false;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();

        bool passed = success && (test::gCurrentTestAssertFailures == 0);

        if (testCase.tier == "Tier 1") {
            ++tier1Total;
            if (passed) ++tier1Pass;
        } else if (testCase.tier == "Tier 2") {
            ++tier2Total;
            if (passed) ++tier2Pass;
        } else if (testCase.tier == "Tier 3") {
            ++tier3Total;
            if (passed) ++tier3Pass;
        } else if (testCase.tier == "Tier 4") {
            ++tier4Total;
            if (passed) ++tier4Pass;
        }

        if (passed) {
            ++overallPass;
            std::cout << "  [" << std::setw(3) << (i + 1) << "/" << totalTests << "] "
                      << "[PASS] [" << testCase.tier << "] " << testCase.id << ": " << testCase.name
                      << " (" << std::fixed << std::setprecision(1) << elapsedUs << " us)\n";
        } else {
            ++overallFail;
            std::cerr << "  [" << std::setw(3) << (i + 1) << "/" << totalTests << "] "
                      << "[FAIL] [" << testCase.tier << "] " << testCase.id << ": " << testCase.name << "\n";
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    std::cout << "\n================================================================================\n";
    std::cout << "                      E2E VERIFICATION SUITE SUMMARY                            \n";
    std::cout << "================================================================================\n";
    std::cout << "  Tier 1 (Feature Coverage)     : " << tier1Pass << " / " << tier1Total
              << " (" << (tier1Total > 0 ? (tier1Pass * 100 / tier1Total) : 0) << "%)\n";
    std::cout << "  Tier 2 (Boundary & Corners)   : " << tier2Pass << " / " << tier2Total
              << " (" << (tier2Total > 0 ? (tier2Pass * 100 / tier2Total) : 0) << "%)\n";
    std::cout << "  Tier 3 (Pairwise Interactions): " << tier3Pass << " / " << tier3Total
              << " (" << (tier3Total > 0 ? (tier3Pass * 100 / tier3Total) : 0) << "%)\n";
    std::cout << "  Tier 4 (Studio Scenarios)     : " << tier4Pass << " / " << tier4Total
              << " (" << (tier4Total > 0 ? (tier4Pass * 100 / tier4Total) : 0) << "%)\n";
    std::cout << "--------------------------------------------------------------------------------\n";
    std::cout << "  Total Test Cases Executed     : " << totalTests << "\n";
    std::cout << "  Passed                        : " << overallPass << "\n";
    std::cout << "  Failed                        : " << overallFail << "\n";
    std::cout << "  Total Suite Execution Time    : " << std::fixed << std::setprecision(2) << totalElapsedMs << " ms\n";
    std::cout << "================================================================================\n";

    const bool allPassed = (overallFail == 0) && (totalTests >= 380);
    std::cout << "OVERALL E2E VERDICT: " << (allPassed ? "PASS (100% SUCCESS)" : "FAIL") << "\n";
    std::cout << "================================================================================\n";

    return allPassed ? 0 : 1;
}
