## What's New in v1.3.9

### 🔊 FDN Physics & Limit-Cycle Hardening
- **Householder -1 Eigenmode Suppression**: Added an 8-channel, 5 Hz one-pole DC blocker ( = 1.0 - \frac{2\pi \cdot 5}{f_s}$) inside the recirculation path to permanently prevent alternating sign accumulation and DC drift from the -1 common-mode eigenvalue during infinite freeze hold (<-110 dBFS).
- **C^1 Smooth Knee Boundary Saturation**: Replaced hard clipping with a continuous C^1 quadratic boundary knee on [0.90, 1.05], preventing derivative discontinuities and scratchy high-frequency spray under heavy overload transients.
- **Poincaré Horocycle Documentation**: Clarified mathematical delay line distribution in hyperbolic geometry (H^2), mapping radial geodesic steps to horocycle wavefront perimeters.

### ❄️ Smooth Freeze Transitions
- **25 ms One-Pole Ramping**: Increased freeze transition time constants (kFreezeInputTimeConstantSec and kFreezeLoopTimeConstantSec) from 8 ms / 5 ms to 25 ms, providing clickless, natural morphing when engaging or disengaging freeze during sustained chords.

### 🎛️ Preset Reset & Web UI Parity
- **Preset-Aware Reset All**: Clicking #btn-reset-all now resets all controls, the Vector Pad reticle, and interval selectors directly to the currently active preset (factory or user patch) instead of kicking the state back to 'DEFAULT'.
- **A/B Comparison Buffer Preservation**: Preserved A/B comparison buffer independence across preset reset and switch actions.

### 🐧 Build System & Linux Decoupling
- **Zero-CURL Linux Builds**: Set JUCE_USE_CURL=0 and removed find_package(CURL REQUIRED) and CURL::libcurl linkages across CMake build targets, enabling clean builds on Linux without external system libcurl packages.
- **Consolidated C++ API**: Unified FdnReverbTank::setParameters into a single signature with a default manifold argument.

### 🔬 Automated Regression Audit
- **Official AcousticDecayAudit Suite**: Added 151 automated acoustic regression tests covering sub-band energy decay curves, freeze DC drift, late-tail silence floors, and spectral crest factor audits (100% passing).

---

### Included Distribution Binaries:
- **BRAUN_RB26-v1.3.9-macOS-Universal.zip** (~23 MB): Universal binary for Apple Silicon (M1/M2/M3/M4) and Intel x86_64, including VST3 (BRAUN_RB26.vst3), Audio Unit (BRAUN_RB26.component), CLAP (BRAUN_RB26.clap), and Standalone (BRAUN_RB26.app).
- **BRAUN_RB26-v1.3.9-Windows-x64.zip** (~10 MB): Complete bundle (.vst3, .clap, standalone .exe, and web showcase).
- **BRAUN_RB26-v1.3.9-VST3-Windows-x64.zip** (~3.4 MB): Lightweight standalone VST3 bundle for DAWs (Ableton Live, FL Studio, Reaper, Cubase, Studio One, Bitwig).
