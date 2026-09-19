# `-ffast-math` NaN/Inf Audit — Remaining Unsafe `std::isnan`/`std::isinf`/`std::isfinite` Calls

> **Status**: Partial fix applied (`de06da6`). Critical DSP-path functions fixed. Many call sites remain.

## Background

The RB-26 CMake build enables `-ffast-math` (`CMakeLists.txt:32`) for performance. Under `-ffast-math`, GCC and Clang set `-ffinite-math-only`, which tells the compiler to **assume NaN and Inf never occur**. This causes:

- `std::isnan(x)` → always returns `false`
- `std::isinf(x)` → always returns `false`
- `std::isfinite(x)` → always returns `true`

This means any guard like `if (!std::isfinite(val)) return 0.0f;` is silently compiled to a no-op.

## What Was Fixed (v1.4.10 CI fix, `de06da6`)

Bitwise IEEE 754 helpers were added to `DspMath.h`:

```cpp
[[nodiscard]] inline bool isFiniteBitwise(float val) noexcept {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(float));
    return (bits & 0x7F800000u) != 0x7F800000u;
}

[[nodiscard]] inline bool isNanOrInfBitwise(float val) noexcept {
    return !isFiniteBitwise(val);
}
```

These were applied to the **critical DSP-path functions** that caused 3 test failures:

| Function | File | Fixed |
|----------|------|-------|
| `flushDenormal()` | `DspMath.h` | ✅ |
| `FastSinTable::sin()` | `DspMath.h` | ✅ |
| `applySmoothBoundaryKnee()` | `DspMath.h` | ✅ |
| `BoundedSaturator::processSample()` | `BoundedSaturator.h` | ✅ |
| Test lambda in `T2_F30_1` | `Tier2_BoundaryTests.h` | ✅ |

## What Still Needs Fixing

The following files still use `std::isnan`/`std::isinf`/`std::isfinite` which are broken under `-ffast-math`. These haven't caused test failures **yet** because the engine produces clean output in practice, so the guards are never actually triggered — but they're silently non-functional.

### DSP Production Code (High Priority)

These are in the hot audio path. If NaN/Inf ever propagates here, the guards won't catch it.

| File | Lines | Call |
|------|-------|------|
| `PitchShifter.cpp` | 122, 138 | `std::isfinite(readPos)` |
| `ShepardPitchSpiral.cpp` | 103, 119 | `std::isfinite(readPos)` |
| `TailModulator.h` | 30 | `std::isfinite(delaySamples)` |
| `FdnReverbTank.cpp` | 61–64, 83–85 | `std::isnan(roomSize)`, etc. |

### Test Assertion Macros (Medium Priority)

These tests verify that engine output is finite. Under `-ffast-math`, the assertions **always pass** regardless of actual output — meaning they can't catch real NaN/Inf regressions.

| File | Approximate Count |
|------|------------------|
| `AcousticDecayAudit.cpp` | ~12 calls |
| `Challenger1StressTests.cpp` | ~8 calls |
| `Challenger1M2StressTests.cpp` | ~10 calls |
| `AdversarialStressTests.cpp` | ~2 calls |
| `M3EmpiricalStressAudit.cpp` | ~8 calls |
| `diag_modal.cpp` | ~1 call |

## Recommended Fix

### Option A: Global replacement (cleanest)

Replace all `std::isnan`/`std::isinf`/`std::isfinite` with `rb26::isNanOrInfBitwise` / `rb26::isFiniteBitwise` across the entire codebase. This is a mechanical find-and-replace.

### Option B: Compile tests without `-ffast-math` (pragmatic)

Keep `-ffast-math` on DSP code for performance, but compile test executables without it so `std::isnan`/`std::isinf` work correctly in assertions. This requires splitting CMake compile flags per target.

### Option C: Hybrid (recommended)

- Fix DSP production code (Option A, ~7 call sites in 4 files)
- Compile test targets without `-ffast-math` (Option B) so assertion macros work naturally

## How to Verify

After applying fixes, all 391 tests should pass on the Linux CI runner (GCC 13, Ubuntu 24.04):

```bash
cmake -B build-ci -DCMAKE_BUILD_TYPE=Release -DRB26_USE_WEBVIEW=OFF -DRB26_BUILD_TESTS=ON
cmake --build build-ci --target rb26_headless_dsp_tests --config Release -j$(nproc)
ctest --test-dir build-ci --output-on-failure -R Rb26HeadlessDspTests
```
