# BRAUN RB-26 v1.4.12 — WARP Unbounded Shimmer Sound Design, Dolby Atmos 7.1.4 Surround & Architectural Hardening

## Overview

BRAUN RB-26 release v1.4.12 introduces the **WARP / Unbounded Shimmer Sound Design** engine, expansive **Multi-Channel Surround and Dolby Atmos 7.1.4** spatialization, and critical **Architectural & Safety Hardening** across both native C++20 and Web Audio cores. 

In strict adherence to Dieter Rams' ten principles of good design—specifically *"Good design is unobtrusive"*, *"Good design is thorough down to the last detail"*, and *"Weniger, aber besser"* (Less, but better)—this release balances high-gain acoustic experimentation with strict mathematical boundedness, zero allocations on the real-time audio thread, and bit-exact multi-channel routing.

---

## What's New in v1.4.12

### 1. WARP / Unbounded Shimmer Sound Design
* **Overdriven Feedback up to 1.30x**: Pushes the recirculation feedback gain from conservative decay boundaries up to 1.30x unity gain, allowing self-sustaining acoustic clouds, endless pitch spirals, and resonant runaway textures.
* **Hermite Soft Saturation Ceiling (<= 1.05)**: Incorporates smooth cubic Hermite saturation (`applySmoothBoundaryKnee`) on all feedback loops and pitch injection nodes, strictly clamping signal excursion to <= 1.05 and preventing digital clipping or numerical overflow even under supercritical resonance.
* **Dual-LFO Tape Capstan Wow & Flutter**: Models vintage open-reel capstan flutter and mechanical eccentricity using dual asynchronous low-frequency modulators (0.45 Hz wow, 5.8 Hz flutter) coupled through Hermite fractional delay interpolation (+/-1.5 ms excursion).
* **Dynamic Grain Windowing**: Dynamically modulates pitch transposition grain window lengths based on warp intensity, mitigating granular comb artifacts and providing smooth transition between pitch layers.
* **Wideband Feedback Filtering (40 Hz – 16 kHz)**: Dynamically widens the recursive feedback filter envelope from standard bandpass limits (150 Hz – 6 kHz) outward to 40 Hz – 16 kHz, preserving deep sub-bass harmonics and pristine air.
* **5 Curated Factory Presets**:
  * `WARP CELESTIAL OVERDRIVE`: Cascading runaway octave shimmer pushed past unity feedback through Hermite soft saturation.
  * `HAUNTED TAPE BEATING`: Deep analog tape flutter with unstable microtonal pitch beating and dark recursive feedback loops.
  * `SUB TRITONE ABYSS`: Sub-octave gravitational pull sinking into saturated low-end resonance with wide harmonic dispersion.
  * `METALLIC COMB DISINTEGRATION`: Ultra-dense short delay flutter disintegrating into saturated metallic ringing and inharmonic sidebands.
  * `INFINITE WARP SINGULARITY`: Supercritical infinite self-oscillating shimmer sphere contained by C1 Hermite boundary saturation.

### 2. Multi-Channel Surround & Dolby Atmos 7.1.4 Support
* **Comprehensive Multi-Channel Topologies**: Native support for 7 distinct channel configurations negotiated dynamically with host DAWs:
  * **Mono** (1.0)
  * **Stereo** (2.0)
  * **Quadraphonic** (4.0)
  * **5.1 Surround** (6 channels: L, R, C, LFE, Ls, Rs)
  * **7.1 Surround** (8 channels: L, R, C, LFE, Ls, Rs, Rls, Rrs)
  * **7.1.2 Dolby Atmos** (10 channels: 7.1 bed + Top Middle height pair)
  * **7.1.4 Dolby Atmos** (12 channels: 7.1 bed + 4 height speakers: Top Front L/R, Top Rear L/R)
* **Clean Dry LFE Channel Isolation**: In all surround and immersive formats (5.1 through 7.1.4), the Low-Frequency Effects (LFE, channel index 3) bus is strictly isolated from wet reverberation (wet gain = 0.0f). Dry sub energy passes through without acoustic blur, phase smearing, or subwoofer intermodulation.
* **Pairwise Orthogonal 3D Elevation Projection**: Height channels (Top Front L/R, Top Rear L/R) are synthesized via pairwise orthogonal projection from the 8-line Feedback Delay Network, providing genuine vertical spatial depth without phase cancellation against listener-plane bed speakers.
* **Center Channel Injection & Decorrelation**: Center channel input is blended into the spatial manifold with a calibrated -3 dB gain compensation and decorrelated output extraction.
* **12-Channel Filter Banks**: Extended feedforward coloration filter banks to 12 parallel biquad topologies for independent room absorption across each speaker channel.
* **Universal Multi-Platform Binaries**: Native multi-channel support compiled for Apple Silicon Universal (AU, VST3, CLAP on macOS arm64/x86_64), Windows x64, and Linux x86_64.

### 3. Architectural & Safety Hardening
* **Zero Real-Time Heap Allocations**: All internal delay networks, filter arrays, multi-channel buffers, and Hermite interpolators are pre-allocated during `prepareToPlay`. Zero `malloc`, `free`, or locking calls during real-time block processing (`0` heap allocations).
* **Buffer Bounds Hardening in PluginEditor.h**: Corrected atomic parameter coalescing array bound `kNumParams` from 28 to 29 in `PluginEditor.h`, ensuring the new WARP pitch parameter does not cause out-of-bounds memory writes on Win32/macOS GUI message loops.
* **Standby Mode Multi-Channel Passthrough**: Extended the transparent unity-gain standby bypass to all 7 surround bus layouts, passing nominal input channels while strictly clearing extraneous unassigned channels.
* **Adversarial NaN & Denormal Shielding**: Tested against buffer flooding with quiet NaNs, signed infinities, and subnormal denormals. Software and hardware flush-to-zero guarantees clean audio output under all host conditions.

---

## Verification & Diagnostic Summary

| Test Suite | Assertions / Cases | Pass Rate | Status |
|:---|:---:|:---:|:---|
| Headless DSP Test Suite (Tiers 1–4) | 391 / 391 | 100% | UNANIMOUS PASS |
| Milestone M4 JUCE & Concurrency Suite | 11 / 11 | 100% | UNANIMOUS PASS |
| Multi-Channel Surround & Atmos Suite | 7 / 7 Layouts | 100% | UNANIMOUS PASS |
| Web Audio Engine & Parity Verification | 37 / 37 | 100% | UNANIMOUS PASS |
| Automated Production Checklist Harness | 24 / 24 | 100% | UNANIMOUS PASS |
| Memory Leaks, Denormals & NaNs | 0 Leaks / 0 NaNs | 100% | CLEAN |

---

## Downloads & SHA-256 Checksums

| Package Archive | Target Platform & Contents | SHA-256 Checksum |
|:---|:---|:---|
| **`BRAUN_RB26-v1.4.12-Windows-x64.zip`** | Windows x64 Full (VST3, CLAP, Standalone, Docs) | `6e749986f14ba445030038a1380dff97f5320d8187618ad5e6220c9bdcbd32ef` |
| **`BRAUN_RB26-v1.4.12-VST3-Windows-x64.zip`** | Windows x64 VST3 Only (`BRAUN_RB26.vst3`, Docs) | `74f2f8afa7b539a2abad14a0ad23510faf9839d403c82c645c4dcc8559eaf68e` |
| **`BRAUN_RB26-v1.4.12-macOS-Universal.zip`** | macOS Universal (AU, VST3, CLAP, Standalone App) | *Published via macOS CI workflow* |
| **`BRAUN_RB26-v1.4.12-Linux-x64.tar.gz`** | Linux x86_64 (VST3, CLAP, Standalone binary) | *Published via Linux CI workflow* |

All release archives are verified against [`releases/SHA256SUMS.txt`](releases/SHA256SUMS.txt).
