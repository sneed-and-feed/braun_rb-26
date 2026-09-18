## What's New in v1.4.4

### 1. Standalone & Plugin Freeze Remediation
* **Acoustic Release Calibration**: Recalibrated preset `INFINITE_ETHEREAL_FREEZE` across C++ DSP engine defaults (`Rb26Engine.cpp`), web factory presets (`factory_presets.json`), and UI models (`app.js`) to `decayRt60Sec = 4.5s` and `pitchFeedback = 0.25` (with `freezeHold = true`). When freeze hold is disengaged, the reverb now audibly and cleanly releases over 4.5 seconds instead of ringing for 30+ seconds with 50% pitch feedback.
* **Freeze Tank Input Excitation**: During freeze hold (`mFreezeHold == true`), the FDN input smoother target is set to `0.08f` (-22 dB, instead of 0.0f). This enables audition exciters, live chords, and chimes to seed and swell into the frozen tank while ducking incoming signal by 22 dB.
* **Pitch Delay Buffer Quenching on Unfreeze**: Added edge detection (`unfreezeEdge = mWasFrozen && !params.freezeHold`) in `Rb26ReverbEngine::setParameters()` to zero out `mPitchDelayBufferL`, `mPitchDelayBufferR`, and feedback registers `mLastPitchFbL`, `mLastPitchFbR`, quenching old recirculating audio from re-pumping into the tank after release.

### 2. UI Synchronization & APVTS Binding
* **Fallback HTML State Sync**: Refactored boolean button generation in `PluginEditor.cpp` fallback HTML to bind strictly to `btn.dataset.state` rather than a desynchronized closure variable. In `paramUpdate`, updated `ctrl.dataset.state`, button text (`ON`/`OFF`), and color (`var(--braun-orange)` / `var(--text-main)`).
* **Immediate Dual-Identifier IPC**: In `web/js/app.js`, ensured both `freezeHold` and `freeze_hold` parameter change events are dispatched with `immediate = true` during preset loading and rocker button toggles, guaranteeing immediate APVTS parameter locking.

### 3. Verification & Diagnostic Test Passes
* **Audited Modal Diagnostic Test (`rb26_diag_modal`)**: Verified unfreeze release decay drops below 0.01 RMS within 2 seconds (1s RMS = 0.017, 2s RMS = 0.004, 4s RMS = 0.000).
* **Headless DSP Stress Suite**: 389 / 389 passed (100% SUCCESS across Tiers 1–4).
* **Web Verification Suite (`web/verify.mjs`)**: 36 / 36 passed (100% SUCCESS).
* **Web Checklist Suite (`web/test-checklist.mjs`)**: 20 / 20 passed (100% SUCCESS).

---

### Distribution Packages (`releases/`)
* **`BRAUN_RB26-v1.4.4-Windows-x64.zip`**: Full Windows x64 distribution containing VST3 (`BRAUN_RB26.vst3`), CLAP (`BRAUN_RB26.clap`), Standalone application (`BRAUN_RB26.exe`), `LICENSE`, and `README.md`.
* **`BRAUN_RB26-v1.4.4-VST3-Windows-x64.zip`**: VST3-only distribution package containing `BRAUN_RB26.vst3`, `LICENSE`, and `README.md`.
