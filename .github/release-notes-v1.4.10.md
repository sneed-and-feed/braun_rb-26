# BRAUN RB-26 v1.4.10 — Architecture Split, Rams Homage Metadata, Hardened Gatekeeper & Pitch Eco Mode

## What's New in v1.4.10

### 1. 1-Page README & ARCHITECTURE Split
* **Streamlined Documentation**: Condensed the primary [README.md](README.md) into a concise, focused overview highlighting acoustic capabilities, fast zero-install web auditioning, factory presets, and precompiled binary distributions.
* **Dedicated Engineering Specification**: Extracted in-depth mathematical derivations, the zero-latency host PDC proof, complete signal-flow diagrams, Deck 01–07 DSP algorithms, and real-time safety guarantees into a comprehensive, standalone [ARCHITECTURE.md](ARCHITECTURE.md) reference.

### 2. Dieter Rams Legal Homage & Sneed's Feed & Seed Ltd. Metadata
* **Prominent Homage Notices**: Formalized trademark and legal attribution disclaimers across [LICENSE](LICENSE), [README.md](README.md), [ARCHITECTURE.md](ARCHITECTURE.md), and UI chassis headers, explicitly acknowledging the independent design homage inspired by Dieter Rams with non-affiliation to Braun GmbH.
* **Manufacturer Metadata Update**: Updated CMake project and JUCE plugin definitions to declare `Sneed's Feed & Seed Ltd.` as company name, `com.sneedandfeed.rb26` as bundle ID, and canonical product name `RB-26`.
* **Chassis DIN 1451 Header Integration**: Embedded legal homage notices cleanly into both the native JUCE vector chassis typography and the HTML5 fallback interface.

### 3. Hardened macOS Gatekeeper Handling & SHA-256 Installer Verification
* **Cryptographic Release Verification**: Updated `scripts/package_release.py` to calculate SHA-256 digests across all distribution zip packages and publish a unified `SHA256SUMS.txt`.
* **Zero-Click Installer Verification**: Enhanced `scripts/macos/quick-install.sh` to fetch `SHA256SUMS.txt` and automatically verify archive integrity via `shasum -a 256` before extraction.
* **Targeted Quarantine Removal**: Replaced blanket recursive attribute clearing (`xattr -cr`) with targeted quarantine attribute removal (`xattr -d com.apple.quarantine`) across AU (`.component`), VST3 (`.vst3`), CLAP (`.clap`), and Standalone (`.app`) binaries, preserving code signatures and preventing Gatekeeper bypass warnings.

### 4. Early Reflections & Limiter Headroom Regression Test (`T2_F18_6`)
* **Calibrated Headroom Assertions**: Verified nominal unit impulse behavior ($0\text{ dBFS}$) on the maximum early reflections tap cluster ($k_\text{cluster} = 0.28$, gain $0.82$, pan $-0.75$), guaranteeing reflection peak at $-12.95\text{ dBFS}$ strictly within the $[-14.0\text{ dBFS}, -12.0\text{ dBFS}]$ nominal headroom window.
* **Extreme Impulse Clamping**: Verified engine behavior under an extreme $+40\text{ dBFS}$ impulse ($100.0\text{f}$) with master soft limiter enabled, proving strict ceiling bounding at $\le 1.000000\text{f}$ with zero overshoot, zero denormals, and zero NaNs/Infinities.

### 5. Pitch Shifter Eco/Auto Linear Interpolation Mode
* **Quality Mode Architecture**: Added `PitchQualityMode` (`Auto`, `HiQHermite`, `FastLinear`) to `DualTapDelayPitchShifter`, `PitchShifter`, and `ShepardPitchSpiral`.
* **High-Rate Optimization**: In `Auto` mode, the engine dynamically selects optimized 2-point 1st-order linear interpolation at sample rates $\ge 88.2\text{ kHz}$ (96 kHz, 176.4 kHz, 192 kHz), while retaining 4-point 3rd-order Hermite spline interpolation at 44.1 kHz and 48 kHz.
* **50–60% CPU Reduction**: Yields massive 50%–60% CPU savings on Deck 04 pitch transposition and continuous Shepard-Risset glissandi during high-sample-rate studio tracking and mastering sessions.
* **Automated Mode Verification (`T2_F18_7`)**: Added boundary test suite validating mode switching, sample-rate thresholds, and voice weight consistency.

### 6. Linux WebView Default to OFF
* **Headless & Native JUCE Friendly**: Configured `CMakeLists.txt` so that `RB26_USE_WEBVIEW` defaults to `OFF` when building on Linux (`RB26_USE_WEBVIEW_DEFAULT OFF`), while remaining `ON` on Windows and macOS.
* **Dependency Minimization**: Eliminates runtime and build-time dependencies on WebKitGTK / WebKit2 for Linux DAW users (Reaper, Bitwig, Ardour), enabling immediate builds with pure native JUCE vector rendering.

---

### Verification & Diagnostic Summary
* **Headless DSP Test Suite**: 391 / 391 passed (100% UNANIMOUS PASS across Tiers 1–4, +2 new regression tests).
* **Acoustic Decay Audit**: 8 / 8 suites passed (100% PASS across Suites A–H).
* **Web Audio & Checklist Suite**: 60 / 60 passed (36 / 36 Web unit tests + 24 / 24 checklist tests).
* **CTest Suite**: 5 / 5 suites passed (100% PASS).
* **Memory & Numerics**: 0 leaks, 0 denormal stalls, 0 NaNs/Infs, 0 samples algorithmic latency.

---

### Distribution Packages & SHA-256 Checksums
* **`BRAUN_RB26-v1.4.10-Windows-x64.zip`**: Full Windows x64 distribution (VST3, CLAP, Standalone application, `LICENSE`, `README.md`, `ARCHITECTURE.md`).  
  `SHA-256: a532d9bdba51c340beb75bd10eb10921003dc4cde287a24fc8da97ea3183285c`
* **`BRAUN_RB26-v1.4.10-VST3-Windows-x64.zip`**: Streamlined VST3-only package (`BRAUN_RB26.vst3`, `LICENSE`, `README.md`, `ARCHITECTURE.md`).  
  `SHA-256: 553880862ae15a4f465fec7f3caf896cb06b8b0cca3422790282d582dab9a011`
* **`BRAUN_RB26-v1.4.10-macOS-Universal.zip`**: Universal macOS distribution (AU `.component`, VST3 `.vst3`, CLAP `.clap`, Standalone `.app`, `LICENSE`, `README.md`, `ARCHITECTURE.md`).
