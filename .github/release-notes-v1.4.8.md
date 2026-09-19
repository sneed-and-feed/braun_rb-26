## What's New in v1.4.8

### 1. Early Reflections Calibration & Air Absorption
* **Acoustic Gain Staging (`kClusterGain = 0.28f`)**: Calibrated early reflection tap gains by applying an acoustic cluster attenuation factor of `0.28f` (`-11.06 dB`). Peak early reflection amplitude dropped from **-0.3 dBFS** to **-16.7 dBFS**, properly seating early room boundaries into the onset of the late FDN reverberant field and matching Web Audio structural parity.
* **1st-Order High-Frequency Damping (6.0 kHz)**: Integrated gentle 6.0 kHz 1st-order lowpass filters (`mDampingLpL/R`) across the early reflections matrix, absorbing harsh high-frequency Dirac delta clicks into warm, natural room boundary reflections.

### 2. Anti-Flutter Allpass Blending & Bounded Room Geometry
* **Equal-Power Dry/Diffused Crossfade**: Replaced 100% series allpass cascading on early reflection taps with equal-power trigonometric crossfading ($\cos / \sin$). This completely eliminates series allpass tap tripling/quadrupling, ensuring that raising diffusion smoothly smears transient energy across the stereo field rather than multiplying discrete delayed echoes.
* **Bounded Physical Room Scaling**: Mapped room scaling in `updateTaps()` via $\text{erScale} = 0.20 + 0.80 \times \frac{\tanh(\text{roomSize})}{\tanh(1.0)}$. At nominal `roomSize = 1.0`, $\text{erScale} = 1.0$ bit-exact, while at large room sizes, early reflections are capped at $\le 171\text{ ms}$ (rather than expanding linearly to $549\text{ ms}$). This prevents early taps from disintegrating into an audible multi-tap slapback delay line, allowing the FDN tank to handle large volumetric expansion.

### 3. Pitch Booster Transient Bleed Quenching
* **Band-Limited Pitch Booster Path**: Filtered `delayedPitchL/R` through the existing 150 Hz highpass and 6.0 kHz lowpass filter cascade before applying `boostGain`. This eliminates delayed transient spikes at $\sim 200\text{ ms}$ when `pitchBoostDb` is turned up, producing a warm harmonic halo under the reverberant tail.

### 4. macOS Universal Easy-Run & Zero-Click Installer Package
* **Double-Click Finder Scripts (`scripts/macos/`)**:
  - **`Install.command`**: Double-click in Finder to automatically copy AU (`.component`), VST3 (`.vst3`), CLAP (`.clap`), and Standalone (`.app`) to user directories, strip macOS Gatekeeper quarantine (`xattr -cr`), and flush `AudioComponentRegistrar`.
  - **`Run_Standalone.command`**: Double-click to instantly unquarantine and launch the standalone engine with zero installation.
  - **`Uninstall.command`**: Safe uninstaller script.
* **One-Line Zero-Click Terminal Installer**:
  ```bash
  curl -fsSL https://raw.githubusercontent.com/sneed-and-feed/braun_rb-26/main/scripts/macos/quick-install.sh | bash
  ```
* **Automated CI Packaging**: Updated `.github/workflows/build-macos.yml` to automatically build Universal binaries (Apple Silicon `arm64` & Intel `x86_64`) on `macos-14` and bundle helper scripts into `BRAUN_RB26-v1.4.8-macOS-Universal.zip`.

### 5. Verification & Diagnostic Metrics
* **First 200 ms Discrete Peak Count**: Reduced from **351 peaks down to 29 peaks** (**-91.7% reduction**, smooth diffuse reverberant tail).
* **Maximum Peak Level**: Reduced from **-8.80 dBFS down to -22.99 dBFS** (**-14.2 dB smoother**).
* **Snare Transient Decay**: Monotonic, smooth energy dissipation across all temporal windows without flutter ripple or delayed spikes.
* **Headless DSP Stress Suite**: 389 / 389 passed (100% SUCCESS across Tiers 1–4).
* **Acoustic Decay Audit**: 180 / 180 assertions passed (100% SUCCESS across Suites A–H).
* **Web Audio & Checklist Harness**: 59 / 59 passed (100% SUCCESS).
* **CTest Suite**: 5 / 5 targets passed (100% SUCCESS).

---

### Distribution Packages (`releases/`)
* **`BRAUN_RB26-v1.4.8-Windows-x64.zip`**: Full Windows x64 distribution containing VST3 (`BRAUN_RB26.vst3`), CLAP (`BRAUN_RB26.clap`), Standalone application (`BRAUN_RB26.exe`), `LICENSE`, and `README.md`.
* **`BRAUN_RB26-v1.4.8-VST3-Windows-x64.zip`**: Lightweight VST3-only distribution package containing `BRAUN_RB26.vst3`, `LICENSE`, and `README.md`.
* **`BRAUN_RB26-v1.4.8-macOS-Universal.zip`**: Universal macOS distribution (Apple Silicon & Intel) containing AU (`.component`), VST3, CLAP, Standalone (`.app`), and easy-run `.command` installer scripts.
