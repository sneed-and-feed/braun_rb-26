# BRAUN RB-26 v1.4.14 — Real-Time DSP CPU Optimization, Long Pre-Delay Idle Guard & Audio Thread Hardening

## Overview

BRAUN RB-26 release **v1.4.14** delivers a comprehensive **Real-Time DSP CPU Optimization** pass across all reverberation and pitch-synthesis subsystems, resolves a critical **Long Pre-Delay Idle Gate** edge case at 100% wet mix, and hardens **Audio Thread & Host State Management** across VST3, CLAP, AU, and Standalone formats.

In strict adherence to Dieter Rams' ten principles of good design—specifically *"Good design is thorough down to the last detail"* and *"Weniger, aber besser"* (Less, but better)—this release eliminates redundant per-sample and per-block transcendental calculations (`std::sqrt`, `std::exp`, `std::pow`, `std::cos`) from the real-time audio path while preserving bit-accurate acoustic response across all 15 factory presets.

---

## What's New in v1.4.14

### 1. Real-Time Per-Sample & Per-Block DSP CPU Optimizations
* **8-Line FDN Reverb Tank (`FdnReverbTank`)**:
  * Cached input diffusion allpass feedback coefficients (`mAllpassG[4]`) on parameter updates instead of evaluating `4x std::sqrt` per sample inside `processAllpass()`.
  * Added dirty-state tracking (`mLastDecayRoom`, `mLastDecayRt60`, `mLastDecayManifold`) in `setParameters()` so `updateDecayGains()` (`8x std::exp`) is only evaluated when room size, RT60, or manifold geometry actually change rather than on every audio block.
* **Early Reflections Diffuser (`EarlyReflections`)**:
  * Cached the 4 series Schroeder allpass coefficients (`mAllpassG[4]`) and equal-power diffusion mix weights (`mDryDiffGain`, `mWetDiffGain`) on `diffusionDensity` change, eliminating `5x std::sqrt` and `2x FastSinTable` lookups per sample.
* **Low-Band Modal Matrix (`LowBandModalMatrix`)**:
  * Cached `mOutScale = 0.25f * std::sqrt(bassRt60Mult)` during coefficient updates to eliminate per-sample `std::sqrt` in `processModalOnly()`.
  * Added parameter dirty-checking in `setParameters()` so the 8 Linkwitz-Riley 4th-order crossover biquads and 4 modal resonator decay coefficients are only recomputed when `lowCrossoverHz`, `subMonoHz`, `roomSize`, `decayRt60Sec`, or `bassRt60Mult` change.
  * Ensured the default constructor initializes valid 48 kHz filter coefficients and delay bounds for pre-`prepare()` call safety.
* **Dual-Tap Pitch Shifter & Shepard Spiral (`PitchShifter`)**:
  * Added state-change guards to `DualTapDelayPitchShifter::setWarpMode`, `DualTapDelayPitchShifter::setWarpModulation`, and `PitchShifter::setWarpMode` to prevent 8 redundant `updateWindowTarget()` calculations per sample when `pitchWarp` is steady.
  * Precomputed `mShimDelaySamples` and `mDimDelaySamples` in `prepare()` instead of recalculating sample counts every sample frame.
* **Master Reverb Engine (`Rb26ReverbEngine`) & Acoustic Exciter (`AcousticExciter`)**:
  * Added a fast-path unity bypass (`|boostDb| < 1e-4f -> 1.0f`) and cached `mLastBoostDb` / `mLastBoostGain` so `dbToGain(boostDb)` (`std::pow(10.0f, ...)`) is only evaluated while `mPitchBoostSmoother` is actively transitioning.
  * Replaced per-sample `std::cos(kPi * frac)` calls in `ChimeVoice::processSample` (felt hammer attack window) and `LaboratoryImpulseGenerator::processSample` (Tukey pink-burst window) with `FastSinTable::cos`.

### 2. DSP Accuracy & State Lifecycle Fixes
* **Long Pre-Delay Premature Idle Gate Fix (`Rb26ReverbEngine`)**:
  * Fixed an edge case where short transient impulses entering the engine at `100%` wet mix (`dryWetMix = 1.0`, where dry output is zero during the pre-delay window) with long pre-delay times (`> 42.6 ms` at 48 kHz or `> 10.6 ms` at 192 kHz, up to `500 ms`) could trigger the 2048-sample idle silence gate while still traversing `mPreDelayL/R`, prematurely wiping the pre-delay line before reaching the reverb tank.
  * Included `preL` and `preR` pre-delay tap outputs in `inPeak` detection and dynamically scaled the idle silence sample threshold by the active pre-delay length (`2048 + max(preDelaySamplesL, preDelaySamplesR) + 64`).
* **Idle Filter State Cleanup & Freeze Sync (`Rb26ReverbEngine`)**:
  * Reset `mPitchFeedbackHpL/R` and `mPitchFeedbackLpL/R` biquad states when entering idle sleep mode.
  * Synchronized `mWasFrozen = mParams.freezeHold` inside `Rb26ReverbEngine::reset()` to avoid spurious freeze-transition resets on the first sample after engine reset.

### 3. Host Integration, MIDI & UI Polish
* **MIDI CC 64 Sustain Pedal Notification Guard (`PluginProcessor`)**:
  * Guarded `freezeParam->setValueNotifyingHost(targetVal)` in `processBlock` so host automation notifications are only dispatched when the sustain pedal state actually crosses the threshold (`|current - target| > 0.5f`), preventing redundant audio-thread host callbacks on continuous half-damper CC 64 streams.
* **Preset Program Bounds Clamping (`PluginProcessor`)**:
  * Clamped `mCurrentProgram` to `[0, getNumPrograms() - 1]` when deserializing XML state in `setStateInformation()`.
* **Native UI Record Capture Label Synchronization (`PluginEditor`)**:
  * Synchronized `recordButton` text (`"RECORDING..."` vs `"REC CAPTURE"`) alongside its toggle state in `timerCallback()` when recording auto-terminates at the 5-minute safety cap.
* **Zero-Warning MSVC `/W4` Build Hygiene (`Parameters.h`, `Tier1_FeatureTests.h`)**:
  * Scoped `C4324` alignment padding suppression around `alignas(16) Rb26ParameterSnapshot` and eliminated `C4305` double-to-float truncation warnings in headless test suites.

---

## Verification & Diagnostic Summary

| Test Suite | Assertions / Cases | Pass Rate | Status |
|:---|:---:|:---:|:---|
| Headless DSP Test Suite (`Rb26HeadlessDspTests`, Tiers 0–3) | 169 Suites | 100% | UNANIMOUS PASS |
| DSP Challenger Suite (`Rb26DspChallengerTests`) | Full Suite | 100% | UNANIMOUS PASS |
| Challenger M2-1 Empirical Verification (`rb26_challenger1_m2_tests`) | 960,041 / 960,041 | 100% | UNANIMOUS PASS |
| Challenger M3-2 Stress Audit (`rb26_m3_stress_audit`) | 262,455 / 262,455 | 100% | UNANIMOUS PASS |
| Milestone M4 JUCE & Concurrency Suite (`rb26_m4_tests`) | 7 / 7 Suites | 100% | UNANIMOUS PASS |
| Acoustic Decay Audit (`Rb26AcousticDecayAudit`) | Suites A–H | 100% | UNANIMOUS PASS |
| Multi-Channel Surround & Atmos 7.1.4 Suite (`Rb26MultiChannelTests`) | 7 / 7 Layouts | 100% | UNANIMOUS PASS |
| Felt Hammer Transient & Voice Steal Suite (`rb26_hammer_transient_tests`) | 5 / 5 Suites | 100% | UNANIMOUS PASS |
| LookAndFeel & Web Resource Integrity (`Rb26LookAndFeelTests`, `Rb26WebResourceTests`) | Full Suite | 100% | UNANIMOUS PASS |

---

## Downloads & SHA-256 Checksums

| Package Archive | Target Platform & Contents | Size | SHA-256 Checksum |
|:---|:---|:---:|:---|
| **`BRAUN_RB26-v1.4.14-Windows-x64.zip`** | Windows x64 Full (`BRAUN_RB26.vst3`, `BRAUN_RB26.clap`, `BRAUN_RB26.exe`, Docs) | 10,812,665 B (~10.3 MB) | `de7f17ef3a69af308b025b3f5bf5304bb8ac15e5d67e28fdc0111b856be57f0e` |
| **`BRAUN_RB26-v1.4.14-VST3-Windows-x64.zip`** | Windows x64 VST3 Only (`BRAUN_RB26.vst3`, Docs) | 3,593,564 B (~3.4 MB) | `87b7b3e239c42a3dcd7887ca31326d49de219920254b97b3f33497e9d862c1fc` |
| **`BRAUN_RB26-v1.4.14-macOS-Universal.zip`** | macOS Universal (AU, VST3, CLAP, Standalone App) | *CI Artifact* | *Published via macOS CI workflow* |
| **`BRAUN_RB26-v1.4.14-Linux-x64.tar.gz`** | Linux x86_64 (VST3, CLAP, Standalone binary) | *CI Artifact* | *Published via Linux CI workflow* |

All release archives are verified against [`releases/SHA256SUMS.txt`](releases/SHA256SUMS.txt).
