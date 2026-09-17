## What's New in v1.3.7

### 🔊 Pitch Shifter & Grain Boundary Smoothing
- **Hann Grain Crossfade Windows**: Replaced half-sine crossfade windows ($w = \sin(\pi \phi)$) in `DualTapDelayPitchShifter` and `ShepardPitchSpiral` with Hann windows ($w = \sin^2(\pi \phi)$). This provides exact constant-amplitude crossfading ($w_1 + w_2 \equiv 1.0$) with $C^1$ zero-derivative boundary suppression ($> 115\text{ dB}$ below peak), eliminating the ~40 Hz periodic grain-wrap step discontinuities and microphonic echoing crackle under sustained inputs (e.g. drone synthesizers).
- **Hermite Interpolator Read Margin Invariant**: Enforced `kMinDelayMargin = 2.0f` on fractional delay lines so 4-point Hermite cubic interpolation never evaluates future unwritten circular buffer indices.
- **Delay Excursion Rate Alignment**: Corrected delay excursion formulas to guarantee exact delay slopes matching $(1 - r)$, maintaining pitch ratio precision ($r = 2.000000$ Shimmer, $r = 0.500000$ Dimmer) without pitch drift.

### 🌊 Tail Bloom Low-Drone Transient Immunity
- **Transient Detector Low-Frequency Smoothing**: Increased the fast envelope follower time constant from 2.0 ms to 7.0 ms and adjusted the onset ratio threshold to 1.80 in `TailModulator.cpp`, preventing low-frequency drone fundamentals (~30–60 Hz) from false-triggering bloom ducking and causing microphonic flutter during steady tones.
- **Hermite Interpolation Fallback in Tail Modulation**: Guarded `TailModulator.h` with linear interpolation fallback for delay excursions $< 1.0$ sample and clamped delay lookups, ensuring zero read-ahead past write heads.

### 📦 Windows Standalone & Multi-Target Build
- **Windows 10 Standalone Rebuilt**: Updated standalone executable build pipeline and verified clean builds for `BRAUN_RB26_Standalone` (`BRAUN_RB26.exe`), `BRAUN_RB26_VST3`, and `BRAUN_RB26_CLAP`.
- **Automated DSP Test Suites**: 100% pass across all 389+ headless DSP tests, challenger stress tests, look-and-feel tests, and Web Audio verification checks.

---

### Included Distribution Binaries:
- **`BRAUN_RB26-v1.3.7-macOS-Universal.zip`** (~23 MB): Universal binary for Apple Silicon (M1/M2/M3/M4) and Intel x86_64, including VST3 (`BRAUN_RB26.vst3`), Audio Unit (`BRAUN_RB26.component`), CLAP (`BRAUN_RB26.clap`), and Standalone (`BRAUN_RB26.app`).
- **`BRAUN_RB26-v1.3.7-Windows-x64.zip`** (~10 MB): Complete bundle (`.vst3`, `.clap`, standalone `.exe`, and web showcase).
- **`BRAUN_RB26-v1.3.7-VST3-Windows-x64.zip`** (~3.4 MB): Lightweight standalone VST3 bundle for DAWs (Ableton Live, FL Studio, Reaper, Cubase, Studio One, Bitwig).
