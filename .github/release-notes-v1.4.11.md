# BRAUN RB-26 v1.4.11 — Safe Unity-Gain Standby Bypass, Clean Engine Reactivation, Web Hotkey Parity & Concurrency Routing Suite

## What's New in v1.4.11

### 1. Safe Unity-Gain Standby Bypass
* **Transparent Audio Passthrough**: Rectified overactive standby gating on macOS and digital audio workstations (DAWs) where RB-26 previously cleared buffers until an internal audition sound or MIDI note was triggered.
* **Bit-Accurate Standby**: When unpowered (`power = false` / Standby switch disengaged), the engine operates as a bit-accurate, transparent unity-gain bypass with 0 samples of algorithmic latency.
* **Mono-to-Stereo Safe Handling**: Safely handles 1-in / 2-out bus configurations by replicating clean channel 0 across channel 1, preventing host channel garbage or uninitialized buffer passes.

### 2. Clean Engine Reactivation
* **Dormant DSP State**: Feedback Delay Network (FDN) internal delay lines, pitch shifters, and diffuse matrices remain completely dormant and reset during standby.
* **Zero Transient Artifacts**: Atomic pending-reset signaling (`mPendingEngineReset`) executes cleanly on the audio processing thread prior to reactivation, preventing clicks, pops, dc thumps, or transient spikes when engaging power.

### 3. Hotkey & Web Engine Parity
* **Dieter Rams Standby Hotkey ('P')**: Wired the keyboard shortcut `'P'` in the Web Audio interface (`BraunRb26App`) to toggle system power on/off, matching the physical front-panel power toggle.
* **Web Engine Unity Standby Routing**: Updated `Rb26WebEngine` bus routing to maintain dry input pass-through at unity gain with wet reverb bus muted when powered down, mirroring native VST3/CLAP/AU standby behavior.

### 4. Concurrency & Routing Tests
* **Expanded Verification**: Updated `M4JuceConcurrencyRoutingTests.cpp` to validate standby transparent unity bypass (verifying sample magnitude preservation during standby instead of buffer clearing).
* **Thread Safety & Activation**: Verified atomic power state transitions, thread-safe reset queuing, and smooth audio reactivation without memory allocation or lock contention in real-time processing threads.

---

### Verification & Diagnostic Summary
* **Headless DSP Test Suite**: 391 / 391 passed (100% UNANIMOUS PASS across Tiers 1–4).
* **Concurrency & Routing Suite**: Verified standby bypass, thread safety, and seamless activation.
* **Web Audio & Checklist Suite**: 60 / 60 passed (36 / 36 Web unit tests + 24 / 24 checklist tests).
* **Memory & Numerics**: 0 leaks, 0 denormal stalls, 0 NaNs/Infs, 0 samples algorithmic latency.

---

### Distribution Packages & SHA-256 Checksums
* **`BRAUN_RB26-v1.4.11-Windows-x64.zip`**: Full Windows x64 distribution (VST3, CLAP, Standalone application, `LICENSE`, `README.md`, `ARCHITECTURE.md`).  
  `SHA-256: ef248fc7d6d547a83741480fdaec6442f6958ec521f4f594069e56bc00c033fb`
* **`BRAUN_RB26-v1.4.11-VST3-Windows-x64.zip`**: Streamlined VST3-only package (`BRAUN_RB26.vst3`, `LICENSE`, `README.md`, `ARCHITECTURE.md`).  
  `SHA-256: 1a6e48908a21a49736fdd3bd711bf2fe013492ffb773ab24707c003c49d59cf6`
* **`BRAUN_RB26-v1.4.11-macOS-Universal.zip`**: Universal macOS distribution (AU `.component`, VST3 `.vst3`, CLAP `.clap`, Standalone `.app`, `LICENSE`, `README.md`, `ARCHITECTURE.md`).
* **`BRAUN_RB26-v1.4.11-Linux-x64.tar.gz`**: Linux x86_64 distribution (VST3, CLAP, Standalone binary, `INSTALL.txt`, `LICENSE`, `README.md`, built and verified via Ubuntu CI).
