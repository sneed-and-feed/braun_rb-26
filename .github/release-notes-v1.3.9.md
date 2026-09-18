## What's New in v1.3.9

### FDN Physics and Limit-Cycle Hardening

The 8-channel Householder feedback reflection matrix satisfies:

```math
\mathbf{H}_8 = \mathbf{I}_8 - \frac{1}{4}\mathbf{1}\mathbf{1}^T
```

This matrix possesses an eigenvalue of $-1$ corresponding to the uniform common mode:

```math
\sum_{i=1}^8 y_i = -\sum_{i=1}^8 x_i
```

In infinite freeze mode with absorption bypassed, asymmetric saturation or interpolation can rectify this alternating-sign polarity into DC bias. We installed an 8-channel one-pole DC blocker with pole radius:

```math
R = 1 - \frac{2\pi \cdot 5}{f_s}
```

inside the recirculation loop, ensuring DC accumulation remains strictly bounded below $-110\text{ dBFS}$ (measured below $-130\text{ dBFS}$).

The hard clipping boundary was replaced with a $C^1$-continuous boundary saturator:

```math
f(x) = \begin{cases}
x, & \lvert x \rvert \le 0.90 \\
\mathrm{sgn}(x)\left(0.90 + 0.15 \cdot \left[1 - \left(1 - \frac{\lvert x \rvert - 0.90}{0.15}\right)^2\right]\right), & 0.90 < \lvert x \rvert < 1.05 \\
1.05 \cdot \mathrm{sgn}(x), & \lvert x \rvert \ge 1.05
\end{cases}
```

This provides pure linear transparency for $\lvert x \rvert \le 0.90$ while guaranteeing continuous first derivatives up to the $1.05$ ceiling, eliminating derivative discontinuities and scratchy high-frequency harmonic spray under heavy transient bursts.

The mathematical delay line spacing in hyperbolic space $\mathbb H^2$ maps discrete radial geodesic steps:

```math
d_k = \xi \frac{k}{7}, \quad k \in \lbrace 0, 1, \dots, 7 \rbrace
```

to concentric horocycle wavefront perimeters:

```math
\cosh d_k = \frac{1 + r_k^2}{1 - r_k^2}
```

### Smooth Freeze Transitions

Lengthened freeze ramp time constants `kFreezeInputTimeConstantSec` and `kFreezeLoopTimeConstantSec` from 8 ms and 5 ms to 25 ms. Toggling freeze during dense chords or high-energy passages produces clickless, natural morphing.

### Preset Reset and Web UI Parity

- **Preset-Aware Reset All:** Clicking the `#btn-reset-all` button resets all controls, the Vector Pad reticle, and interval switches directly to the currently active preset (factory or user patch) rather than defaulting back to the initial patch.
- **A/B Comparison Buffer Independence:** Preserves A/B comparison buffer separation across preset reset and switch actions.
- **Unified Engine Integration:** Both the browser Web Audio demo and the DAW JUCE 8 WebView plugin share identical reset handling and parameter emission.

### Build System and Linux Decoupling

- **Zero-CURL Linux Builds:** Set `JUCE_USE_CURL=0` and removed `find_package(CURL REQUIRED)` and `CURL::libcurl` linkages across CMake targets, enabling clean builds on Linux without external system `libcurl` packages.
- **Consolidated C++ API:** Unified `FdnReverbTank::setParameters` into a single signature with a default manifold argument (`std::optional<ManifoldType> = std::nullopt`).

### Official Acoustic Decay Audit Suite

Added `source/tests/AcousticDecayAudit.cpp` (`rb26_acoustic_decay_audit.exe`), executing 151 automated acoustic assertions across 6 diagnostic suites:

- **Suite A:** Householder freeze DC drift and common-mode hold: DC offset $\le -110\text{ dBFS}$.
- **Suite B:** Sub-band energy decay curve monotonicity: $(T_{60}(8\text{ kHz}) < T_{60}(2\text{ kHz}) < T_{60}(500\text{ Hz}))$.
- **Suite C:** 60-second late-tail silence and limit-cycle detection: noise floor $\le -206\text{ dBFS}$.
- **Suite D:** Spectral crest factor audit: peak-to-median ratio $< 14\text{ dB}$ (threshold $< 20\text{ dB}$).
- **Suite E:** Quadratic knee overload blast test: $+40\text{ dBFS}$ impulse input bounded to $1.05$.
- **Suite F:** 64-point parametric sweep across Manifolds $\times$ Room Sizes $\times$ Damping factors.

---

### Verification Summary

| Test Harness | Target Scope | Assertions / Cases | Status |
| :--- | :--- | :--- | :--- |
| `rb26_acoustic_decay_audit` | 6 Acoustic Decay Audit Suites (A-F) | 151 assertions | 100% Passed |
| `rb26_headless_dsp_tests` | Tiers 1-4, Stress Tests, Diagnostics | 389 test cases | 100% Passed |
| `npm run verify:all` | Node.js M4 and Verification Checklist | 53 assertions | 100% Passed |
| `npm run test:browser` | Headless Chrome/Edge Browser E2E Tests | Full UI suite | 100% Passed |
| Release Build | C++20 MSVC/Clang (.vst3, .clap, .exe) | All targets | 0 Errors, 0 Warnings |

---

### Included Distribution Binaries:
- **`BRAUN_RB26-v1.3.9-macOS-Universal.zip`** (~23 MB): Universal binary for Apple Silicon (M1/M2/M3/M4) and Intel x86_64, including VST3 (`BRAUN_RB26.vst3`), Audio Unit (`BRAUN_RB26.component`), CLAP (`BRAUN_RB26.clap`), and Standalone (`BRAUN_RB26.app`).
- **`BRAUN_RB26-v1.3.9-Windows-x64.zip`** (~10 MB): Complete bundle (`.vst3`, `.clap`, standalone `.exe`, and web showcase).
- **`BRAUN_RB26-v1.3.9-VST3-Windows-x64.zip`** (~3.4 MB): Lightweight standalone VST3 bundle for DAWs (Ableton Live, FL Studio, Reaper, Cubase, Studio One, Bitwig).
