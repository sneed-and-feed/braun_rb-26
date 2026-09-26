# BRAUN RB-26 · Master Studio Reverberator

[![Verification: 100% PASS](https://img.shields.io/badge/Verification-100%25%20PASS%20(391%2F391)-24FF6A?style=for-the-badge&logo=checkmarx)](VERIFICATION_CHECKLIST.md)
[![Version: 1.4.11](https://img.shields.io/badge/Version-1.4.11-EE592B?style=for-the-badge)](https://github.com/sneed-and-feed/braun_rb-26/releases/tag/v1.4.11)
[![CI](https://github.com/sneed-and-feed/braun_rb-26/actions/workflows/test.yml/badge.svg)](https://github.com/sneed-and-feed/braun_rb-26/actions)
[![Windows VST3 & CLAP](https://img.shields.io/badge/Windows-VST3%20%7C%20CLAP%20%7C%20Standalone-blue?style=for-the-badge&logo=windows)](https://github.com/sneed-and-feed/braun_rb-26/releases/download/v1.4.11/BRAUN_RB26-v1.4.11-Windows-x64.zip)
[![macOS AU & VST3](https://img.shields.io/badge/macOS-AU%20%7C%20VST3%20%7C%20CLAP%20%7C%20Standalone-white?style=for-the-badge&logo=apple)](https://github.com/sneed-and-feed/braun_rb-26/releases/download/v1.4.11/BRAUN_RB26-v1.4.11-macOS-Universal.zip)
[![Linux VST3 & CLAP](https://img.shields.io/badge/Linux-VST3%20%7C%20CLAP%20%7C%20Standalone-FCC624?style=for-the-badge&logo=linux)](https://github.com/sneed-and-feed/braun_rb-26/releases/download/v1.4.11/BRAUN_RB26-v1.4.11-Linux-x64.tar.gz)
[![Web Audio API](https://img.shields.io/badge/Web%20Audio-100%25%20Client--Side-4A4A4A?style=for-the-badge)](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API)
[![License: MIT](https://img.shields.io/badge/License-MIT-black?style=for-the-badge)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-4A4A4A?style=for-the-badge)](https://isocpp.org/)
[![JUCE 8](https://img.shields.io/badge/JUCE-8.0.6-EE592B?style=for-the-badge)](https://juce.com/)

> *An authentic Dieter Rams functionalist studio reverberator and acoustic space synthesizer.*
> *Direct package-deal hardware sibling companion to the [BRAUN AS-42](https://sneed-and-feed.github.io/).*
> *"Weniger, aber besser" — Less, but better.*

> [!NOTE]
> **Legal Homage Notice**: Not affiliated with Braun GmbH. Dieter Rams inspired design homage. Manufactured by **Sneed's Feed & Seed Ltd.**

---

## Overview

The **BRAUN RB-26** is a research-grade algorithmic reverberator and acoustic space synthesizer built upon Dieter Rams' functionalist design principles. Engineered for pristine clarity, spatial depth, and organic warmth, the RB-26 unites a non-Euclidean 8-line Feedback Delay Network (FDN), bidirectional pitch diffusion (Shimmer + Dimmer), a decoupled 4th-order Linkwitz-Riley low-end matrix, and an interactive 2D Vector Modulation Pad into an intuitive 19" 2U rackmount interface.

Ships as native **VST3**, **CLAP**, **AU**, and **Standalone** plugins for **macOS**, **Windows**, and **Linux**, plus a zero-install **Web Audio** showcase.

---

## Downloads & Packages

Precompiled release packages (verified with SHA-256 in [`releases/SHA256SUMS.txt`](releases/SHA256SUMS.txt)):

* **macOS Universal (Apple Silicon M1–M4 & Intel)**: [**`BRAUN_RB26-v1.4.11-macOS-Universal.zip`**](https://github.com/sneed-and-feed/braun_rb-26/releases/download/v1.4.11/BRAUN_RB26-v1.4.11-macOS-Universal.zip) (~23 MB) — AU (`.component`), VST3 (`.vst3`), CLAP (`.clap`), Standalone (`.app`).
* **Windows x64**: [**`BRAUN_RB26-v1.4.11-Windows-x64.zip`**](https://github.com/sneed-and-feed/braun_rb-26/releases/download/v1.4.11/BRAUN_RB26-v1.4.11-Windows-x64.zip) (~10 MB) or [**VST3 Only (.zip)**](https://github.com/sneed-and-feed/braun_rb-26/releases/download/v1.4.11/BRAUN_RB26-v1.4.11-VST3-Windows-x64.zip) (~3 MB) — VST3, CLAP, Standalone (`.exe`).
* **Linux x64**: [**`BRAUN_RB26-v1.4.11-Linux-x64.tar.gz`**](https://github.com/sneed-and-feed/braun_rb-26/releases/download/v1.4.11/BRAUN_RB26-v1.4.11-Linux-x64.tar.gz) — VST3, CLAP, Standalone binary, and `INSTALL.txt` (packaged via Ubuntu CI).
* View all release notes and checksums on [GitHub Releases](https://github.com/sneed-and-feed/braun_rb-26/releases).

---

## Quickstart

### 1. Web Showcase (Zero Install)

Audition the full 19" 2U rackmount interface, 21 rotary controls, 4-mode CRT vector scope, and acoustic exciter directly in your browser:

```bash
# Windows
start.bat

# macOS / Linux / Cross-Platform
npm start
# Opens http://localhost:3826
```

> [!TIP]
> The RB-26 initializes in **Standby** mode (`power = false`) to protect studio monitors. Click the orange **POWER** switch or press `P` to engage the audio graph.

### 2. DAW Setup (Logic Pro, Ableton Live, FL Studio, Reaper, Bitwig)

* **macOS Verified Two-Step Installation**:
  ```bash
  # Step 1: Download installer script
  curl -fsSL -o quick-install.sh https://raw.githubusercontent.com/sneed-and-feed/braun_rb-26/main/scripts/macos/quick-install.sh
  # Step 2: Review and execute installer (verifies SHA-256 against release catalog)
  bash quick-install.sh
  ```
  *Or extract the Universal `.zip` and double-click `Install.command`.*
* **Windows VST3 / CLAP**: Extract and copy `BRAUN_RB26.vst3` to `C:\Program Files\Common Files\VST3\` and `BRAUN_RB26.clap` to `C:\Program Files\Common Files\CLAP\`.
* **macOS Manual AU / VST3**: Copy `BRAUN_RB26.component` to `~/Library/Audio/Plug-Ins/Components/` and `BRAUN_RB26.vst3` to `~/Library/Audio/Plug-Ins/VST3/`. Clear quarantine:
  ```bash
  xattr -d com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/BRAUN_RB26.component
  ```
* **Linux VST3 / CLAP**: Copy `BRAUN_RB26.vst3` to `~/.vst3/` and `BRAUN_RB26.clap` to `~/.clap/`.

---

## The 7 Control Decks

The RB-26 arranges its spatial acoustic synthesis across 7 tactile physical decks:

1. **Deck 01 · Input & Pre-Delay**: Precision input trim (-18 dB to +18 dB), hard mute, and tempo/millisecond pre-delay (0–500 ms) to keep direct transients clear of the reverb onset.
2. **Deck 02 · Low-End Decoupled Matrix**: 4th-order Linkwitz-Riley crossover isolating bass under 200 Hz into an orthogonal modal matrix with transient punch ducking and sub-bass mono collapse.
3. **Deck 03 · Spatial Geometry & Reverb Tank**: 8-line FDN tank with Householder energy diffusion and 4 non-Euclidean acoustic manifolds (*Poincaré Cavity*, *Whispering Gallery*, *Anharmonic Spruce*, *Klangdom Sphere*).
4. **Deck 04 · Pitch Diffusion Network**: Bidirectional pitch-shifting feedback network with celestial **Shimmer** (+7st, +12st, +24st) and sub-harmonic **Dimmer** (-2st, -7st, -12st) with adaptive grain windows.
5. **Deck 05 · Tail Modulation & Vector Pad**: 8-phase golden-ratio LFO chorus/flutter paired with an interactive 2D Cartesian Vector Modulation Pad (Rate $\times$ Depth) for dynamic gesture control.
6. **Deck 06 · Master Bus & CRT Scope**: Mid/Side stereo width matrix, equal-power dry/wet crossfader, Hermite soft saturation limiter, and hardware-calibrated 4-mode CRT vector scope (*WAVE*, *EDC*, *LISSAJOUS*, *SPECTRUM*).
7. **Deck 07 · Onboard Acoustic Exciter**: Playable physical-model felt piano, unit Dirac impulse, acoustic mallet, pink burst, chime strip, and Poisson ambient clock for standalone auditioning without a DAW host.

*For complete mathematical derivations, filter topologies, and block diagrams, see [`ARCHITECTURE.md`](ARCHITECTURE.md).*

---

## Factory Presets & Sonic Character

Ten factory presets crafted for acoustic instruments, modern studio mixes, and ambient compositions:

1. **Calibrated Default** (RT60: 6.5s) — Reference studio acoustic space with linear decay, natural early reflections, and transparent shimmer/dimmer harmonic balance.
2. **Ambient Guitar Cloud** (RT60: 9.5s) — Expansive, blossoming ambient wash designed for electric guitar swells, featuring +12st octave shimmer and wide stereo diffusion.
3. **AS-42 Tape & Shimmer Companion** (RT60: 8.5s) — Warm, tape-modulated vintage plate calibrated as the direct companion to the BRAUN AS-42 synthesizer.
4. **Soft Felt Acoustic Hall** (RT60: 4.8s) — Intimate wooden concert hall tailored for felt piano and delicate strings, highlighting spruce soundboard resonance and soft HF absorption.
5. **German Plate 140** (RT60: 3.8s) — Dense, classic EMT-style steel plate emulation with rapid diffusion onset and shimmering high-frequency dispersion for snare drums and vocals.
6. **Cathedral Diffusion** (RT60: 18.0s) — Grand sacred stone architecture with soaring decay tails, subtle air damping, and ethereal upper-register bloom.
7. **Ethereal Synth Pad** (RT60: 10.5s) — Cinematic tail designed for polyphonic analog synths and brass, featuring balanced bidirectional shimmer and dimmer harmonics.
8. **Bloom Shimmer Void** (RT60: 14.0s) — Deep ambient void where cascading octave shimmers bloom slowly behind melodic phrases without masking primary transients.
9. **Infinite Ethereal Freeze** (RT60: Infinite) — Lossless infinite recirculating ambient drone with input isolation and gentle tail chorus modulation.
10. **Sub-Bass Preserver** (RT60: 4.5s) — Decoupled low-end preservation isolating kick fundamentals below 180 Hz while maintaining spatial width across midrange and high frequencies.

---

## Verification & Benchmarks

Every release is verified against an uncompromising automated test harness:

* **Automated C++ Suite**: 100% PASS across 391 headless DSP stress tests (Tier 1 Features, Tier 2 Boundaries, Tier 3 Pairwise, Tier 4 Scenarios).
* **Real-Time Safety**: Zero dynamic memory allocations (`malloc`/`new`) in `processBlock()`, hardware FTZ/DAZ denormal immunity, zero NaNs, and zero algorithmic latency.
* **Full Checklist & Reproducers**: Refer to [`VERIFICATION_CHECKLIST.md`](VERIFICATION_CHECKLIST.md) for full benchmark logs, commands, and compliance matrices.

```powershell
# Run headless DSP test suite (391 tests)
.\source\tests\rb26_headless_dsp_tests.exe

# Run Web Audio verification suite (60 tests)
npm run verify:all
```

---

## Building from Source

```bash
# Clone repository
git clone https://github.com/sneed-and-feed/braun_rb-26.git
cd braun_rb-26

# Configure and compile Release build (Windows / macOS / Linux)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

*On Linux, `RB26_USE_WEBVIEW` defaults to `OFF` for a lightweight native build without WebKitGTK dependencies.*

---

## Legal & Homage

**Not affiliated with Braun GmbH. Dieter Rams inspired design homage.**
Manufactured by **Sneed's Feed & Seed Ltd.**
Released under the [MIT License](LICENSE).
