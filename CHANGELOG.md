# Changelog

All notable changes to the **BRAUN RB-26** Master Studio Reverberator & Space Synthesizer are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.5.0] - 2026-11-14

### Fixed
- **Shepard-Risset Barber-Pole Shimmer Direction & Continuity (`ShepardPitchSpiral`, `PitchShifter`)**:
  - Replaced unsigned `std::abs(1.0f - rm) / mWindowSamples` with the exact signed delay-slope phase velocity `(1.0f - rm) / mWindowSamples` and float32-safe `[0, 1)` phase wrap on both stereo channels (`mVoicesL` and `mVoicesR`). Upward `BarberShimmer` voices ($r_m > 1$) now produce genuine upward pitch shifts ($f_{\text{out}} = r_m f_{\text{in}}$) with $C^1$ continuity across unison ($r_m = 1$) instead of inverting into downward shifts.
  - Calibrated `PitchShifter`'s internal `mShepardSpiral` depth to `1.0f` in `prepare()` and `setSpiralParameters()` so `PitchShifter::processSample`'s `(1 - spiralDepth) * raw + spiralDepth * spiralOut` blend obeys a linear crossfade law without `depth^2` double-attenuation or dry bleed, and persisted `ShepardPitchSpiral::mDepth` across `prepare()`.
- **192 kHz Delay Buffer Truncation & Premature Idle Silence Gate (`Rb26Engine`, `PitchShifter`, `ShepardPitchSpiral`)**:
  - Expanded `Rb26ReverbEngine::kPreDelayBufferCapacity` and `kPitchDelayCapacity` from $131{,}072$ to $262{,}144$ ($2^{18}$), eliminating $192\text{ kHz}$ bitmask wrap-around truncation when `pitchDelayMs = 500 ms` with the $1.05 \times 1.375$ stereo offset ($723.75\text{ ms} = 138{,}960\text{ samples}$).
  - Expanded `DualTapDelayPitchShifter::kMaxCapacity` and `ShepardPitchSpiral::kMaxCapacity` from $65{,}536$ to $131{,}072$ ($2^{17}$) and `PitchShifter::kCircCapacity` from $32{,}768$ to $65{,}536$ ($2^{16}$), preventing truncation of the $211\text{ ms}$ Dimmer circulation line ($40{,}512\text{ samples}$ at $192\text{ kHz}$) and $200\text{ ms}$ `-24 st` grain windows.
  - Extended `Rb26ReverbEngine`'s idle silence gate threshold with a sample-rate-scaled $50\text{ ms}$ guard plus maximum active pre-delay (`current` and `target`) and `max(activePitchDelaySamples, activeErSamples)`, preventing premature silence clamping of discrete late Early Reflection taps (up to $172.4\text{ ms}$ at `roomSize = 4.0`) and long pitch-delayed echoes when `decayRt60Sec` is short.
- **Early Reflections Rapid Automation Continuity & FDN Multi-Channel Pitch Feedback (`EarlyReflections`, `FdnReverbTank`)**:
  - Scaled `EarlyReflections` dual-bank tap crossfade length proportionally with sample rate (`~6 ms`, clamped `[64, 2048]`) and queued incoming `setParameters()` tap updates via `mPendingTapUpdate` while an active crossfade is in flight, eliminating mid-crossfade tap jump clicks under rapid `roomSize` automation.
  - Fixed `FdnReverbTank::processSampleMultiChannel` so `fbR` reads `pitchFb[1]` independently of `numInChannels`, preserving stereo pitch-diffusion feedback on Mono-In -> Stereo/Surround-Out bus layouts, and clamped `effectiveOutChannels` to `<= 12`.
- **Thread-Safe Power/Reset Lifecycle, Standby MIDI Isolation & Auto-Wake (`PluginProcessor`, `AcousticExciter`, `PluginEditor`)**:
  - Made `BRAUN_RB26AudioProcessor::setPower(bool)` idempotent (`if (prev != powered)`) so redundant `setPower(true)` calls on editor open or state sync never wipe an active reverb tail.
  - Eliminated cross-thread data races in `PluginProcessor::reset()` and `AcousticExciterEngine` by deferring resets to `processBlock` and advancing consumer-owned `mFifoReadPos` (`discardPendingTriggers()`) without mutating producer-owned `mFifoWritePos`.
  - Preserved queued SPSC exciter triggers across Standby auto-wake (`mPendingExciterFifoFlush = !powered`), processed pending engine resets prior to block MIDI/FIFO execution, ignored MIDI Note-On events while in Standby (honoring Note-Off / CC 64), and added `mStateSyncDirty` / `hasPendingActivity()` so host preset/state recalls and Standby Dirac/Pink/Mallet auto-wake triggers synchronize cleanly across Audio, Native UI, and WebView2 threads.
- **WebView2 IPC Echo Loop, Parameter Scaling & 4-Manifold WebAudio Engine (`web/js/app.js`, `web/js/audio/rb26_web_engine.js`, `web/js/ui/vector-pad.js`, `PluginEditor.cpp`)**:
  - Added `fromHost` guard to `BraunRb26App.setPower(isPowered, fromHost = false)` so incoming host `paramUpdate` events do not echo `power` or `stopRecording` back to JUCE.
  - Corrected `tail_mod_depth_ms` scaling between Web UI (`0–100%`) and APVTS (`0.0–5.0 ms`, default `2.25 ms` = `45%`), aligned 6 rotary knob bounds (`predelay` max `500 ms`, `damping_low` max `4.00x`, `mono_bass` min `20 Hz`, `rt60_decay` min `0.2 s`, `tail_mod_rate` max `5.00 Hz`, `tail_bloom` max `300 ms`) with `Parameters.h`, and aligned embedded fallback HTML defaults/choices in `PluginEditor.cpp`.
  - Implemented distinct 8-line FDN delay distributions and dual-bank S-curve crossfading for all 4 geometric manifolds (`Poincare`, `Whispering Gallery`, `Anharmonic Plate`, `Stockhausen Klangdom`) in `Rb26WebEngine`, persisted `manifold_type` in `getSnapshot()`, and unified `setPower(true)` input trim staging (`1.0x`).
- **Documentation, Packaging & GFM Math Compliance**:
  - Registered all 10 C++ verification suites in CTest (`source/tests/CMakeLists.txt`), updated `scripts/package_release.py` to write clean single-version `SHA256SUMS.txt` manifests, synchronized all documentation and `wiki/` guides to `v1.5.0` (29 APVTS parameters, 23 rotary knobs, 15 factory presets), and resolved all GFM/KaTeX lint findings across repository markdown files.

## [1.4.14] - 2026-10-20

### Changed
- Cached FDN allpass feedback coefficients, Early Reflections Schroeder diffuser gains, Low-Band Modal Matrix output scaling, and dirty-checked parameter updates across `FdnReverbTank`, `EarlyReflections`, `LowBandModalMatrix`, `PitchShifter`, `Rb26ReverbEngine`, and `AcousticExciter` to eliminate redundant per-sample/per-block transcendental calls.

### Fixed
- Fixed premature 2048-sample idle gating on long pre-delay settings at 100% wet mix, cleared pitch feedback biquad states on idle sleep, guarded MIDI CC 64 sustain notifications against redundant triggers, clamped `mCurrentProgram` bounds on XML load, and synchronized Native UI record button labels on auto-stop.
