# Project: BRAUN RB-26 Audio Stabilization, Bug Audit & Real-Time Optimizations

## Architecture
- **Native C++20 DSP Core (`source/dsp/`)**: Header-only math and core algorithms (`Rb26Engine`, `FdnReverbTank`, `ManifoldDelayNetwork`, `LowBandModalMatrix`, `EarlyReflections`, `PitchShifter`, `ShepardPitchSpiral`, `TailModulator`, `AcousticExciter`, `BoundedSaturator`, `DspMath.h`). Compiled into `rb26_dsp_core` static library.
- **JUCE Plugin Layer (`source/plugin/`)**: `BRAUN_RB26AudioProcessor`, `PluginEditor`, parameter APVTS management, LookAndFeel, CRT presentation, and Web UI bridge (WebView2 on Windows).
- **Web Audio Showcase (`web/`)**: Standalone Web Audio API engine (`rb26_web_engine.js`), CRT canvas renderer (`crt-display.js`), controller (`app.js`), factory presets (`factory_presets.json`), and test scripts (`verify.mjs`, `test-browser.mjs`, `server.js`).
- **Test Infrastructure (`source/tests/`)**: `rb26_headless_dsp_tests` (381+ Tier 1-4 tests), `rb26_dsp_tests` (Challenger 2 suite), `rb26_laf_tests`, `rb26_web_resource_tests`.

## Feature Inventory
| # | Feature / Bug | Description | Milestone | Source |
|---|---------------|-------------|-----------|--------|
| 1 | Branching & Git Isolation | Create dedicated branch `audit/sound-quality-optimizations` branched from `main` with clean working tree | M1 | ORIGINAL_REQUEST §R4 |
| 2 | CMake Build Configuration | Register `rb26_headless_dsp_tests` in root `CMakeLists.txt` for clean CLI build & CI parity | M1 | Survey (Explorer 3) |
| 3 | Freeze Hold Damping Bypass | Fix `effAlpha = 0` muting bug in `ManifoldDelayNetwork.cpp:321-337` to sustain infinite reverb hold | M2 | Survey (Explorer 1) |
| 4 | Comprehensive Smoother Resets | Reset all 6 master smoothers in `Rb26Engine` and sub-module smoothers in `FdnTank`, `TailMod`, `PitchShifter`, `ShepardSpiral` | M2 | Survey (Explorer 1) |
| 5 | Pre-Prepare Buffer Safety | Prevent out-of-bounds segfaults on uninitialized vectors in `EarlyReflections`, `PitchShifter`, `ShepardSpiral` | M2 | Survey (Explorer 1) |
| 6 | Fractional Delay Interpolation | Eliminate discrete sample truncation clicks in `preDelay` and `pitchDelay` using Hermite interpolation | M2 | Survey (Explorer 1) |
| 7 | Tap & Window Crossfading | Smooth `roomSize` tap updates in `EarlyReflections` and interval switching in `DualTapDelayPitchShifter` | M2 | Survey (Explorer 2) |
| 8 | Inner-Loop Denormal Optimization | Remove redundant `ScopedNoDenormals` from inner per-sample loops, retaining only at block boundaries | M3 | Survey (Explorer 1) |
| 9 | Fast Transcendental Math | Replace per-sample `std::sin`, `std::cos`, `std::pow` with `FastSinTable` and `exp2f` | M3 | Survey (Explorer 1) |
| 10 | PitchShifter Inactive Bypass | Implement block-level `!isActive()` bypass in `PitchShifter::process` to eliminate 192 kHz CPU spike | M3 | Survey (Explorer 2) |
| 11 | Real-Time Safety Enforcement | Verify zero heap allocations and zero locks on the audio thread across all sample rates | M3 | ORIGINAL_REQUEST §R3 |
| 12 | Audio/UI Thread Concurrency Fix | Remove UI-thread `reverbEngine.reset()` from `setPower()`; make exciter Poisson parameters thread-safe | M4 | Survey (Explorer 2) |
| 13 | Mono-In / Stereo-Out Routing | Check `getTotalNumInputChannels() > 1` in `PluginProcessor.cpp` to prevent reading garbage audio | M4 | Survey (Explorer 2) |
| 14 | Master Limiter Unbypasable Bug | Respect `limiter_enable` parameter in `PluginProcessor.cpp` and eliminate duplicate limiting | M4 | Survey (Explorer 2) |
| 15 | Host DAW Preset Management | Expose the 10 curated factory presets via JUCE program management interface | M4 | Survey (Explorer 2) |
| 16 | Stale Parameters on Prepare | Initialize smoothers from active APVTS snapshot in `prepareToPlay` to prevent playback start ramp | M4 | Survey (Explorer 2) |
| 17 | Web Audio DC Blocker & E2E Test | Raise `masterDcBlocker` cutoff to 35-40 Hz in `rb26_web_engine.js` and fix browser test settling | M5 | Survey (Explorer 3) |
| 18 | Web Showcase Static Serving | Verify static server (`server.js`) launches cleanly and zero console exceptions occur | M5 | ORIGINAL_REQUEST §R1 |
| 19 | Headless 381+ Test Pass Rate | Verify 100% pass rate on `source/tests/rb26_headless_dsp_tests.exe` | M6 | ORIGINAL_REQUEST §R4 |
| 20 | New Regression Test Assertions | Add reproducible test assertions verifying every fixed bug (freeze energy, reset invariance, routing, etc.) | M6 | ORIGINAL_REQUEST §R4 |
| 21 | Adversarial & Forensic Verification | Independent Challenger and Forensic Auditor integrity verification with zero tolerance | M6 | Orchestrator Protocol |
| 22 | Comprehensive Audit Report | Document all findings, bug fixes, benchmarks, and verification results in repository | M6 | ORIGINAL_REQUEST §R1-R5 |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Branch Setup & Build Infra | Git branch `audit/sound-quality-optimizations`, CMake target `rb26_headless_dsp_tests`, clean baseline build | None | DONE |
| M2 | DSP Core Audio & Numerical Stabilization | Fix freeze hold damping, smoother resets, pre-prepare buffer safety, delay interpolation, tap/window clicks | M1 | DONE |
| M3 | Safe Real-Time Performance Optimizations | Inner-loop denormal removal, FastSinTable math, PitchShifter 192kHz bypass, zero-allocation verification | M2 | DONE |
| M4 | JUCE Plugin Interface & Concurrency | UI/audio thread concurrency, Mono-In routing, limiter fix, DAW presets, APVTS prepare snapshot | M2, M3 | DONE |
| M5 | Web Audio Showcase Stabilization | Web engine DC blocker filter, browser E2E test resolution, static server verification | M1 | DONE |
| M6 | Regression Suite, Adversarial Audit & Report | 100% pass on 381+ tests + new regression assertions, challenger audit, forensic audit, AUDIT_REPORT.md | M2, M3, M4, M5 | IN_PROGRESS |

## Interface Contracts
### DSP Core ↔ Plugin Processor
- `Rb26ReverbEngine::prepare(double sampleRate, int maxBlockSize)`: Pre-allocates all buffers; must be real-time safe (called before audio thread starts).
- `Rb26ReverbEngine::reset()`: Resets all internal delay lines, filter states, and smoothers to currently configured target parameter values. Audio thread only.
- `Rb26ReverbEngine::processBlock(const float* const* in, float* const* out, int numSamples, int numInChans, int numOutChans)`: Hard real-time audio thread callback. Zero allocations, zero locks, zero system calls. Denormals masked by `ScopedNoDenormals` at entry.
- `Rb26ReverbEngine::getFactoryPresets()`: Returns `const std::vector<Rb26Preset>&` containing 10 factory presets.

### Plugin Processor ↔ UI Bridge
- Parameter updates: Communicated via atomic APVTS parameters and lock-free SPSC event FIFO.
- Power toggle: Communicated via `std::atomic<bool> isPoweredOn`; silence gating and reset handled on audio thread without blocking UI or audio callback.

### Code Layout & Ownership
- `source/dsp/*`: DSP Core algorithms and math routines (Owned by M2, M3).
- `source/plugin/*`: JUCE audio processor, editor, parameters, and preset bindings (Owned by M4).
- `web/*`: Web Audio showcase, browser tests, static server (Owned by M5).
- `source/tests/*`, `CMakeLists.txt`: Build configurations, test cases, and regression assertions (Owned by M1, M6).
- `AUDIT_REPORT.md`: Audit report documentation (Owned by M6).
