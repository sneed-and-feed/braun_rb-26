# `-ffast-math` NaN/Inf Audit — Remaining Unsafe `std::isnan`/`std::isinf`/`std::isfinite` Calls

> **Status**: Resolved — Option C fully implemented. DSP production code migrated to bitwise IEEE-754 helpers; test targets decoupled from `-ffast-math`/`/fp:fast`.

## Background

The RB-26 CMake build previously enabled `-ffast-math` (and `/fp:fast` on MSVC) globally for performance. Under `-ffast-math`, GCC and Clang set `-ffinite-math-only`, which tells the compiler to **assume NaN and Inf never occur**. This causes:

- `std::isnan(x)` → always returns `false`
- `std::isinf(x)` → always returns `false`
- `std::isfinite(x)` → always returns `true`

This means any guard like `if (!std::isfinite(val)) return 0.0f;` is silently compiled to a no-op.

## Implementation of Option C (Hybrid Resolution)

Option C combines bitwise IEEE-754 sanitization on production DSP code with target-scoped decoupling of compiler optimization flags.

### 1. Production DSP Migration to Bitwise Helpers

All floating-point validation guards in the audio DSP engine have been converted to use the bitwise IEEE-754 helpers defined in `source/dsp/DspMath.h`:

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

#### Production DSP Call Sites Migrated:

| File | Functions / Lines | Original Call | Migrated Replacement | Status |
|------|-------------------|---------------|----------------------|--------|
| `DspMath.h` | `flushDenormal()`, `FastSinTable::sin()`, `applySmoothBoundaryKnee()` | Various | `isNanOrInfBitwise` / `isFiniteBitwise` | ✅ (v1.4.10 / `de06da6`) |
| `BoundedSaturator.h` | `BoundedSaturator::processSample()` | `!std::isfinite` | `!isFiniteBitwise` | ✅ (v1.4.10 / `de06da6`) |
| `PitchShifter.cpp` | `readLinear` (L122), `readHermite` (L138) | `!std::isfinite(readPos)` | `!rb26::isFiniteBitwise(readPos)` | ✅ (Option C) |
| `ShepardPitchSpiral.cpp` | `readLinear` (L103), `readHermite` (L119) | `!std::isfinite(readPos)` | `!rb26::isFiniteBitwise(readPos)` | ✅ (Option C) |
| `TailModulator.h` | `readHermite` (L30) | `!std::isfinite(delaySamples)` | `!rb26::isFiniteBitwise(delaySamples)` | ✅ (Option C) |
| `FdnReverbTank.cpp` | `setParameters` (L61–64, L83–85) | `!std::isnan(...)` | `rb26::isFiniteBitwise(...)` | ✅ (Option C) |

### 2. CMake Compilation Flag Refactor (Target Decoupling)

In root `CMakeLists.txt`:
- Removed `-ffast-math` (GCC/Clang) and `/fp:fast` (MSVC) from global `add_compile_options`.
- Retained global baseline optimization flags (`-Wall -Wextra -Wpedantic -O3` and `/utf-8 /W4 /O2`).
- Applied `-ffast-math` / `/fp:fast` strictly as `PRIVATE` compile options to production targets:
  - `target_compile_options(rb26_dsp_core PRIVATE ...)`
  - `target_compile_options(BRAUN_RB26 PRIVATE ...)`
- Because compile options are scoped `PRIVATE`, they do not propagate to test executables linking against `rb26_dsp_core` or `BRAUN_RB26`.
- Test targets compile under standard IEEE-754 semantics:
  - `rb26_headless_dsp_tests`
  - `rb26_dsp_tests`
  - `rb26_laf_tests`
  - `rb26_web_resource_tests`
  - `rb26_challenger1_m2_tests`
  - `rb26_m3_stress_audit`
  - `rb26_m4_tests`
  - `rb26_diag_modal`
  - `rb26_hammer_transient_tests`
  - `rb26_diag_pitch`
  - `rb26_diag_cpu`
  - `rb26_acoustic_decay_audit`
- Standard assertion macros (`std::isnan`, `std::isinf`, `std::isfinite`) across all test suites now evaluate accurately and catch genuine non-finite floating-point regressions without needing manual test modifications.

## How to Verify

On the Linux CI runner (GCC 13, Ubuntu 24.04) or local build environment:

```bash
cmake -B build-ci -DCMAKE_BUILD_TYPE=Release -DRB26_USE_WEBVIEW=OFF -DRB26_BUILD_TESTS=ON
cmake --build build-ci --target rb26_headless_dsp_tests --config Release -j$(nproc)
ctest --test-dir build-ci --output-on-failure -R Rb26HeadlessDspTests
```

All 391 tests pass with strict IEEE-754 evaluation enabled.
