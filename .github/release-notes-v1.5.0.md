# BRAUN RB-26 v1.5.0 — High-Rate 192 kHz Spatial Integrity, Shepard-Risset Shimmer Calibration, Lock-Free Lifecycle & 4-Manifold WebAudio Parity

## Overview

BRAUN RB-26 release **v1.5.0** completes a comprehensive deep audit across the C++20 real-time DSP core, JUCE 8 VST3/CLAP/Standalone lifecycle, lock-free SPSC acoustic exciter bridge, and hybrid WebView2 / WebAudio engine.

In strict adherence to Dieter Rams' ten principles of good design—*"Good design is thorough down to the last detail"* and *"Weniger, aber besser"* (Less, but better)—this release resolves upward Shepard-Risset barber-pole shimmer inversion, eliminates 192 kHz delay-line bitmask wrap-around truncation and premature late-tap idle gating, hardens cross-thread power/reset/auto-wake state transitions with zero audio-thread locks or allocations, and brings full 4-manifold FDN parity and exact parameter scaling to the standalone WebAudio engine and WebView2 control surface.

---

## What's New in v1.5.0

### 1. Shepard-Risset Barber-Pole Shimmer Direction & Linear Crossfade Law
* **Signed Delay-Slope Phase Velocity (`ShepardPitchSpiral`)**:
  * *Root Cause*: `ShepardPitchSpiral::processSample` previously advanced grain phases using unsigned `std::abs(1.0f - rm) / mWindowSamples`, causing upward `BarberShimmer` voices ($r_m > 1$) to sweep delay positively instead of negatively and invert into downward pitch shifts.
  * *Fix*: Replaced the unsigned increment with the exact signed delay-slope phase velocity `(1.0f - rm) / mWindowSamples` and float32-safe `[0, 1)` phase wrapping across all 8 stereo voices (`mVoicesL` and `mVoicesR`). Upward `BarberShimmer` voices now produce true upward harmonic shifts ($f_{\text{out}} = r_m f_{\text{in}}$) with $C^1$ continuity through unison ($r_m = 1$).
* **Linear Spiral Depth Calibration & State Persistence (`PitchShifter`, `ShepardPitchSpiral`)**:
  * *Root Cause*: `PitchShifter` passed `mSpiralDepth` into `mShepardSpiral.setDepth()` and then blended again by `spiralDepth` in `PitchShifter::processSample`, producing quadratic `spiralDepth * spiralDepth` attenuation and dry signal bleed; `ShepardPitchSpiral::prepare()` also zeroed `mDepth` on sample-rate changes.
  * *Fix*: Calibrated `PitchShifter`'s internal `mShepardSpiral.setDepth(1.0f)` in `prepare()` and `setSpiralParameters()` so `(1.0f - spiralDepth) * raw + spiralDepth * spiralOut` obeys an exact linear crossfade law, and persisted `ShepardPitchSpiral::mDepth` across `prepare()`.

### 2. 192 kHz Delay Buffer Capacity Expansion & Idle Silence Gate Hardening
* **Power-of-Two Delay Capacity Expansion (`Rb26ReverbEngine`, `PitchShifter`, `ShepardPitchSpiral`)**:
  * *Root Cause*: At 192 kHz, maximum `pitchDelayMs = 500 ms` with the right-channel `1.05 * 1.375` stereo feedback offset requires `723.75 ms` (`138,960 samples`), exceeding the previous `131,072`-sample (`2^17`) bitmask buffer and wrapping to `41.1 ms`. Similarly, `PitchShifter`'s `211 ms` Dimmer circulation delay (`40,512 samples` at 192 kHz) exceeded `kCircCapacity = 32,768` (`2^15`).
  * *Fix*: Expanded `Rb26ReverbEngine::kPreDelayBufferCapacity` and `kPitchDelayCapacity` from `131,072` to `262,144` (`2^18`), `DualTapDelayPitchShifter::kMaxCapacity` and `ShepardPitchSpiral::kMaxCapacity` from `65,536` to `131,072` (`2^17`), and `PitchShifter::kCircCapacity` from `32,768` to `65,536` (`2^16`).
* **Late Early-Reflection & Pitch-Delay Idle Gate Guard (`Rb26ReverbEngine`)**:
  * *Root Cause*: The fixed `2048`-sample (`10.67 ms` at 192 kHz) base idle threshold accounted only for smoothed pre-delay and could prematurely gate discrete late `EarlyReflections` taps (up to `172.4 ms` at `roomSize = 4.0`) or `723 ms` pitch-delayed echoes when `decayRt60Sec` was short.
  * *Fix*: Extended `idleThresholdSamples` with a sample-rate-scaled `50 ms` guard plus maximum active pre-delay (`current` and `target`) and `max(activePitchDelaySamples, activeErSamples)`.

### 3. Early Reflections Rapid Automation Continuity & Multi-Channel FDN Routing
* **Sample-Rate-Scaled & Queued Dual-Bank Tap Crossfading (`EarlyReflections`)**:
  * *Root Cause*: `EarlyReflections` used a fixed 256-sample tap crossfade (`1.33 ms` at 192 kHz) and restarted tap crossfades immediately if `setParameters()` arrived mid-transition, creating step discontinuities under rapid `roomSize` automation.
  * *Fix*: Scaled crossfade length proportionally with sample rate (`~6 ms`, clamped to `[64, 2048]` samples) and queued incoming tap updates via `mPendingTapUpdate` until active dual-bank crossfades finish cleanly.
* **Mono-In to Stereo/Surround-Out Pitch Feedback (`FdnReverbTank`)**:
  * *Root Cause*: `FdnReverbTank::processSampleMultiChannel` gated right-bank `pitchFb[1]` on `numInChannels > 1`, collapsing stereo pitch-diffusion feedback on Mono-In -> Stereo/Surround-Out bus layouts.
  * *Fix*: Decoupled `fbR = pitchFb[1]` from `numInChannels` and clamped `effectiveOutChannels` to `<= 12` (7.1.4 Atmos ceiling).

### 4. Lock-Free Power/Reset Lifecycle, Standby MIDI Isolation & Auto-Wake
* **Idempotent Power & Deferred Audio-Thread Reset (`PluginProcessor`, `AcousticExciter`)**:
  * *Root Cause*: Calling `setPower(true)` when already active unconditionally triggered an engine reset, wiping active reverb tails on redundant UI/host syncs. `PluginProcessor::reset()` mutated DSP state off-thread, and `AcousticExciterEngine::reset()` wrote producer-owned `mFifoWritePos` from the consumer thread—causing data races and discarding newly queued Standby auto-wake triggers (`Dirac`, `Pink Burst`, `Felt Mallet`).
  * *Fix*: Guarded `setPower(bool)` with `if (prev != powered)`, deferred all `reset()` execution to `processBlock` ahead of block MIDI/FIFO processing, added consumer-only `discardPendingTriggers()`, and preserved queued exciter triggers across Standby auto-wake (`mPendingExciterFifoFlush = !powered`).
* **Standby MIDI Isolation & Deterministic Host/UI Sync (`PluginProcessor`, `PluginEditor`)**:
  * Ignored incoming MIDI Note-On events while in Standby while continuing to honor Note-Off and CC 64 sustain release transitions.
  * Added `mStateSyncDirty` / `consumeStateSyncDirty()` and `hasPendingActivity()` so host `setStateInformation` recalls and Standby exciter auto-wake events synchronize deterministically across the Audio thread, Native JUCE header (`powerButton`), and WebView2 UI (`power = 1.0`).

### 5. WebView2 IPC Echo Prevention, Parameter Bounds & 4-Manifold WebAudio Engine
* **Host-to-Web Power Echo Guard & Parameter Alignment (`web/js/app.js`, `web/js/ui/vector-pad.js`, `PluginEditor.cpp`)**:
  * Added `fromHost` echo suppression to `BraunRb26App.setPower(isPowered, fromHost = false)` so host `paramUpdate` notifications never bounce `power` or `stopRecording` IPC commands back to JUCE.
  * Corrected `tail_mod_depth_ms` conversion between Web UI (`0–100%`) and APVTS (`0.0–5.0 ms`, default `2.25 ms` = `45%`), aligned all rotary knob bounds (`predelay` max `500 ms`, `damping_low` max `4.00x`, `mono_bass` min `20 Hz`, `rt60_decay` min `0.2 s`, `tail_mod_rate` max `5.00 Hz`, `tail_bloom` max `300 ms`) with `Parameters.h`, persisted `manifold_type` in `getSnapshot()`, and synchronized embedded fallback HTML defaults and `dimmer_interval` choices (`-2 st`, `-7 st`, `-12 st`).
* **4-Manifold WebAudio FDN & Crossfading (`web/js/audio/rb26_web_engine.js`)**:
  * Implemented distinct 8-line prime delay tables and dual-bank S-curve crossfading across all four geometric manifolds (`Poincare`, `Whispering Gallery`, `Anharmonic Plate`, `Stockhausen Klangdom`) in `Rb26WebEngine`, and unified `setPower(true)` input gain staging (`1.0x`).

---

## Verification & Diagnostic Summary

| Test Suite | Assertions / Cases | Pass Rate | Status |
|:---|:---:|:---:|:---|
| Headless DSP Test Suite (`Rb26HeadlessDspTests`, Tiers 0–3) | 169 Suites | 100% | UNANIMOUS PASS |
| DSP Challenger Suite (`Rb26DspChallengerTests`, Tests 1–10) | 10 / 10 Suites | 100% | UNANIMOUS PASS |
| Challenger M2-1 Empirical Verification (`Rb26Challenger1M2Tests`) | 960,041 / 960,041 | 100% | UNANIMOUS PASS |
| Challenger M3-2 Stress Audit (`Rb26M3StressAudit`) | 262,455 / 262,455 | 100% | UNANIMOUS PASS |
| Milestone M4 JUCE & Concurrency Suite (`Rb26M4JuceTests`, Tests 1–12) | 8 / 8 Suites | 100% | UNANIMOUS PASS |
| Acoustic Decay Audit (`Rb26AcousticDecayAudit`) | Suites A–H | 100% | UNANIMOUS PASS |
| Multi-Channel Surround & Atmos 7.1.4 Suite (`Rb26MultiChannelTests`) | 7 / 7 Layouts | 100% | UNANIMOUS PASS |
| Felt Hammer Transient & Voice Steal Suite (`Rb26HammerTransientTests`) | 5 / 5 Suites | 100% | UNANIMOUS PASS |
| LookAndFeel & Web Resource Integrity (`Rb26LookAndFeelTests`, `Rb26WebResourceTests`) | Full Suite | 100% | UNANIMOUS PASS |
| Web UI, IPC & Checklist Verification (`web/verify.mjs`, `web/test-checklist.mjs`) | 64 / 64 Assertions | 100% | UNANIMOUS PASS |
| Headless Browser E2E Suite (`web/test-browser.mjs`) | Full DOM + WebAudio | 100% | UNANIMOUS PASS |
| GitHub Math & KaTeX Lint (`scripts/audit_gfm_math.py`) | All Repo Markdown | 100% | ZERO ISSUES |

---

## Downloads & SHA-256 Checksums

| Package Archive | Target Platform & Contents | Size | SHA-256 Checksum |
|:---|:---|:---:|:---|
| **`BRAUN_RB26-v1.5.0-Windows-x64.zip`** | Windows x64 Full (`BRAUN_RB26.vst3`, `BRAUN_RB26.clap`, `BRAUN_RB26.exe`, Docs) | 10,819,561 B (~10.3 MB) | `1942e367637080d5af5b21a6c8a0b42f04a812a397f9531141add2e64998d99b` |
| **`BRAUN_RB26-v1.5.0-VST3-Windows-x64.zip`** | Windows x64 VST3 Only (`BRAUN_RB26.vst3`, Docs) | 3,596,210 B (~3.4 MB) | `a02ce72c39c7be32a468f1834923b373c2b632b17f2f8ad5aeed4bd069ddbf4d` |
| **`BRAUN_RB26-v1.5.0-macOS-Universal.zip`** | macOS Universal (AU, VST3, CLAP, Standalone App) | *CI Artifact* | *Published via macOS CI workflow* |
| **`BRAUN_RB26-v1.5.0-Linux-x64.tar.gz`** | Linux x86_64 (VST3, CLAP, Standalone binary) | *CI Artifact* | *Published via Linux CI workflow* |

All release archives are verified against [`releases/SHA256SUMS.txt`](releases/SHA256SUMS.txt).
