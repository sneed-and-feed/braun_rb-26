# E2E Test Infra: BRAUN RB-26 Avant-Garde Studio Reverberator & Space Synthesizer

**Document ID**: `RB26-TEST-INFRA-001`  
**Target Subsystems**: Native JUCE 8 DSP Core (`rb26_dsp_core`), Standalone Executable, VST3 / CLAP / AU Plugins, and Zero-Install Web Audio Showcase (`rb-26/web/`)  
**Standard Compliance**: ISO C++20, IEC 60268 Sound System Equipment, RFC 8259 JSON, DIN 1451 Typographic Standards  
**Status**: Authoritative Test Infrastructure Specification & 4-Tier Test Framework  

---

## 1. Test Philosophy

The testing methodology for the **BRAUN RB-26** is governed by five uncompromising engineering principles:

1. **Opaque-Box, Requirement-Driven Verification**:
   All test cases are derived directly from the mathematical properties, physical acoustics, and user requirements specified in `ORIGINAL_REQUEST.md` (specifically `## 2026-09-14T21:31:31Z`) and `PROJECT.md`. Tests evaluate observable acoustic output, state invariants, numerical bounds, and thread safety without coupling to ephemeral implementation shortcuts.

2. **Zero Tolerance for Facades or Circumvention**:
   No trivial "always-pass" tests, mock assertions, or tautological checks are permitted. Every test asserts against real DSP computation, true floating-point audio buffers, actual frequency-domain or time-domain metrics, and hardware-level allocation interceptors.

3. **Explicit Authoritative Expected Output Derivation**:
   For every single test case across all 4 tiers, the expected output is derived from an authoritative source:
   - *Mathematical Oracles*: Closed-form analytical equations (e.g., constant power sum $\sum A_m^2 \equiv 1.5$ for Shepard spirals, Linkwitz-Riley crossover magnitude $|H_{sum}| \equiv 1.000$, exponential volume growth $e^{\lambda t}$, Hadamard row orthogonality $\mathbf{r}_L \cdot \mathbf{r}_R = 0$).
   - *Acoustic & Physical Laws*: Kirchhoff-Love flexural dispersion ($v_g \propto \sqrt{\omega}$), Rayleigh grazing caustics, spruce body formants ($A_0, T_1$), and Harry Partch sub-harmonic ratios ($1/N$).
   - *Empirical Baseline Probes*: Pre-computed reference sweeps, impulse response decay thresholds, and psychoacoustic JND limits (< 5 cents onset wobble).
   - *RFC Specifications & Standards*: RFC 8259 JSON grammar validation and DIN 1451 glyph metrics.

4. **Multi-Tier Orthogonal Partitioning**:
   The verification space is structured into four progressive tiers:
   - **Tier 1 (Feature Coverage)**: Validates primary happy paths and baseline functionality for every feature F1 through F20 ($\ge 5$ tests per feature, $\ge 100$ tests).
   - **Tier 2 (Boundary & Corner Cases)**: Pushes every feature to mathematical and computational extremes ($\ge 5$ tests per feature, $\ge 100$ tests), explicitly verifying multi-rate operation ($44.1\text{ kHz}$ to $192\text{ kHz}$), variable buffer sizes ($1$ to $8192$ samples), zero heap allocations, denormal flushing, and numerical stability under infinite feedback.
   - **Tier 3 (Cross-Feature Combinations)**: Systematically tests pairwise interactions between concurrent subsystems ($\ge 20$ interaction tests, targeting all major feature pairs).
   - **Tier 4 (Real-World Application Scenarios)**: Exercises full-system studio production workloads, long-running continuous stress auditions, patch migrations, and live performance gestures ($\ge 10$ studio scenarios).

5. **Self-Containment & Progressive Testability**:
   Every test case is self-contained: it initializes its own memory, configures its test harness, executes deterministically, and verifies invariants without reliance on prior test state or global side effects.

---

## 2. Feature Inventory Coverage Matrix (N = 20 Features)

The BRAUN RB-26 feature set (F1 through F20) defined in `PROJECT.md` is mapped across the 4 test tiers:

| # | Feature | Subsystem / Category | Milestone | Tier 1 (Min 5) | Tier 2 (Min 5) | Tier 3 (Pairwise) | Tier 4 (Studio Scenarios) |
|---|---------|----------------------|:---------:|:--------------:|:--------------:|:-----------------:|:-------------------------:|
| **F1** | Poincaré Hyperbolic Cavity | Space Manifold / FDN | M1 | 5 | 5 | [X] | S01, S05, S11 |
| **F2** | Whispering Gallery / Caustic Waveguide | Space Manifold / FDN | M1 | 5 | 5 | [X] | S02, S06, S11 |
| **F3** | Anharmonic Plate & Soundboard Formant | Space Manifold / FDN | M1 | 5 | 5 | [X] | S03, S07, S11 |
| **F4** | Stockhausen Klangdom | Space Manifold / FDN | M1 | 5 | 5 | [X] | S04, S08, S11 |
| **F5** | Endless Barber-Pole Dimmer | Pitch Spiral Engine | M1 | 5 | 5 | [X] | S02, S05, S09 |
| **F6** | Endless Barber-Pole Shimmer | Pitch Spiral Engine | M1 | 5 | 5 | [X] | S01, S06, S09 |
| **F7** | Microtonal Harmonic & Sub-Harmonic Lattices | Pitch Spiral Engine | M1 | 5 | 5 | [X] | S04, S07, S12 |
| **F8** | Decoupled Low-End & Transient Punch Protection | Low-End Matrix / LR4 | M1 | 5 | 5 | [X] | S03, S07, S10 |
| **F9** | Dynamic Tail-Level Bloom Gating | Modulation / Early Refl | M1 | 5 | 5 | [X] | S01, S04, S08 |
| **F10** | Playable Microtonal Chime & Chord Strip | Acoustic Exciter Engine | M2 | 5 | 5 | [X] | S08, S12, S13 |
| **F11** | Precision Laboratory Impulse Generator | Acoustic Exciter Engine | M2 | 5 | 5 | [X] | S03, S05, S10 |
| **F12** | Generative Poisson Acoustic Stimulus | Acoustic Exciter Engine | M2 | 5 | 5 | [X] | S04, S09, S14 |
| **F13** | Bus Routing & Exciter Level Staging | Signal Routing / Staging | M2 | 5 | 5 | [X] | S08, S10, S13 |
| **F14** | Austerity & DIN 1451 Nomenclature | Presentation / Branding | M3 | 5 | 5 | [X] | S13, S14 |
| **F15** | Patch Management & Presets | Preset / Persistence | M3 | 5 | 5 | [X] | S09, S11, S13 |
| **F16** | Master Utilities (WAV Rec, A/B, Theme) | Master Bus / Utilities | M3 | 5 | 5 | [X] | S10, S13, S14 |
| **F17** | Web App Lifecycle Guard | Web Architecture | M3 | 5 | 5 | [X] | S13, S14 |
| **F18** | Native 5-Platform JUCE 8 Codebase | Architecture / Scaffolding | M4 | 5 | 5 | [X] | S10, S11, S14 |
| **F19** | Hard Real-Time Audio Guarantees | Thread Safety / RT DSP | M4 | 5 | 5 | [X] | S05, S10, S14 |
| **F20** | Comprehensive E2E Testing Track | Verification / QA | M5/E2E | 5 | 5 | [X] | All Scenarios |

---

## 3. Test Architecture

```
+---------------------------------------------------------------------------------------------------+
|                            BRAUN RB-26 4-TIER TEST ARCHITECTURE                                    |
+---------------------------------------------------------------------------------------------------+
|                                                                                                   |
|  +---------------------------------------------------------------------------------------------+  |
|  | TIER 1: FEATURE COVERAGE (>= 100 Tests, 5+ per feature F1-F20)                              |  |
|  | - Primary happy path functionality                                                          |  |
|  | - Direct mathematical verification of DSP formulas                                          |  |
|  | - Parameter range acceptance, UI token matching, lifecycle state machines                   |  |
|  +---------------------------------------------------------------------------------------------+  |
|                                                                                                   |
|  +---------------------------------------------------------------------------------------------+  |
|  | TIER 2: BOUNDARY & CORNER CASES (>= 100 Tests, 5+ per feature F1-F20)                         |  |
|  | - Multi-Rate invariance: 44.1 kHz, 48 kHz, 88.2 kHz, 96 kHz, 176.4 kHz, 192 kHz             |  |
|  | - Arbitrary block sizes: 1 sample (single-sample processing), 32, 64, 128, 512, 4096, 8192    |  |
|  | - Zero Heap Allocations: Intercepted global operator new/delete during processBlock          |  |
|  | - Extreme parameter boundaries: 0.0, 1.0, -48 dB, +6 dB, inf decay, NaN/Inf denormal inputs   |  |
|  +---------------------------------------------------------------------------------------------+  |
|                                                                                                   |
|  +---------------------------------------------------------------------------------------------+  |
|  | TIER 3: CROSS-FEATURE COMBINATIONS (>= 20 Pairwise Interaction Tests)                        |  |
|  | - Dual Manifold + Pitch Spiral interactions (e.g. Whispering Gallery + Endless Shimmer)     |  |
|  | - Decoupled Low-End Ducking + Acoustic Exciter Hammer Thuds                                 |  |
|  | - Dynamic Bloom Gating + Microtonal Chime Strumming                                         |  |
|  | - Infinite Freeze + Lossless WAV Recording + A/B Snapshot Switching                         |  |
|  +---------------------------------------------------------------------------------------------+  |
|                                                                                                   |
|  +---------------------------------------------------------------------------------------------+  |
|  | TIER 4: REAL-WORLD APPLICATION SCENARIOS (>= 10 Studio Workload Scenarios)                    |  |
|  | - Full studio production workflows (Ambient guitar cloud, club kick punch, vocal bloom)     |  |
|  | - Standalone acoustic exciter performance auditioning                                       |  |
|  | - 1,000,000-sample continuous stress burn-in without memory leakage or denormal stall       |  |
|  +---------------------------------------------------------------------------------------------+  |
|                                                                                                   |
+---------------------------------------------------------------------------------------------------+
```

### 3.1 Tier 1: Feature Coverage (N >= 100 Tests)
Tier 1 establishes foundational correctness. Each feature F1 through F20 possesses a minimum of 5 isolated test cases covering:
- **F1 (Poincaré Cavity)**: Negative curvature modal dispersion ($a_{disp} = -0.32$), exponential reflection density ($e^{\lambda t}$), horocycle delay scaling, hyperbolic group delay slope, energy conservation.
- **F2 (Whispering Gallery)**: High-frequency caustic peaking ($+3.5\text{ dB}$ at $9.5\text{ kHz}$), Airy radial mode delays ($a_n$), circular spatial caustic panning ($\Omega = 0.25\text{ Hz}$), ultrasonic safety roll-off at $18\text{ kHz}$, shallow grazing boundary decay.
- **F3 (Anharmonic Plate)**: Flexural stiffness dispersion ($v_g \propto \sqrt{\omega}$), allpass chirp cascade ($a_p = +0.55$), spruce $A_0$ air mode ($95\text{ Hz}$), spruce $T_1$ bridge mode ($320\text{ Hz}$), wood fiber resonance ($2.4\text{ kHz}$).
- **F4 (Stockhausen Klangdom)**: Inscribed unit cube 8-vector spatialization ($\frac{1}{\sqrt{3}}(\pm 1, \pm 1, \pm 1)$), antipodal convergence timing ($2 R/c$), binaural diffuse field correlation, elevation ring partitioning, spherical energy balance.
- **F5 (Barber-Pole Dimmer)**: Continuous downward frequency glide ($d\sigma/dt < 0$), 4-voice log-frequency distribution, raised-cosine amplitude window, constant power sum ($\sum A_m^2 \equiv 1.5$), sub-bass bloom continuity across voice wrap.
- **F6 (Barber-Pole Shimmer)**: Continuous upward frequency glide ($d\sigma/dt > 0$), raised-cosine windowing, high-frequency sparkle build, DC blocking in pitch loop, constant power sum ($\sum A_m^2 \equiv 1.5$).
- **F7 (Microtonal Lattices)**: Pythagorean pure fifths ($3/2$, $+7.02\text{ st}$), Just Intonation major/minor thirds ($5/4, 6/5$), Harry Partch sub-harmonic series ($1/1, 1/2, 1/3, 1/4, 1/5, 1/6 \implies 0, -12, -19, -24, -28, -31\text{ st}$), cent accuracy within $0.1\text{ cent}$, pitch shift purity.
- **F8 (Decoupled Low-End)**: 4th-order Linkwitz-Riley crossover ($f_x = 180\text{ Hz}$, $|H_{sum}| \equiv 1.000$), Hadamard modal output matrix ($[-1,-1,+1,+1]$ and $[+1,+1,+1,+1]$) yielding adjacent notch $< 4.80\text{ dB}$, automatic kick transient punch ducking ($-12\text{ dB}$ on $TR > 1.8$), 120 Hz elliptical mono collapse ($|S| < -12\text{ dB}$ at $60\text{ Hz}$).
- **F9 (Tail Bloom Gating)**: Early reflections modulation drift bit-exact $0.0\text{ Hz}$ ($0.000\text{ ms}$), attack transient phase lock (pitch deviation $< 5\text{ cents}$ during $t < 25\text{ ms}$), bloom onset time constant $\tau_{bloom} \ge 85\text{ ms}$, late tail golden-ratio LFO detuning, smooth $E_{bloom}$ envelope tracking.
- **F10 (Playable Chime Strip)**: 16-voice chime model modal inharmonicity ($1.0, 2.756, 5.404, 8.933$), touch velocity scaling ($0.35 + 0.50 \cdot \operatorname{relY}$), 8 modal scale quantizer, 12 chord macro triggers, 4-speed strumming delay queue.
- **F11 (Laboratory Impulse Generator)**: Single-sample Dirac delta pulse ($\delta[0] = 1.0, \delta[n>0] = 0.0$), broadband Paul Kellet 3-pole pink noise burst ($-3\text{ dB/oct}$ slope), Tukey window fade, $78\text{ Hz}$ acoustic hammer thud with $2.5\text{ ms}$ contact click, impulse polarity inversion.
- **F12 (Poisson Stimulus)**: Exponential inter-arrival distribution ($\tau = -\ln(1-U)/\lambda$), Poisson density parameter ($4$ to $60\text{ epm}$), rubato humanization, Harold Budd Markov step/leap pitch weighting, modal scale pitch lock.
- **F13 (Bus Routing & Staging)**: Exciter level trim ($-48\text{ dB}$ to $+6\text{ dB}$), `REVERB ONLY` routing mode (silent on dry bus), `REVERB + DIRECT` routing mode (audible direct acoustic instrument), clean gain ramping, overload protection.
- **F14 (DIN 1451 & Austerity)**: 100% emoji absence verification across files, DIN 1451 font stack declaration, authentic Braun laboratory terminology, high-contrast metric graticule, uppercase tracking.
- **F15 (Patch Management)**: 12 curated factory presets verification, custom patch save/load persistence in `localStorage`, RFC 8259 JSON patch export validation, JSON patch import restoring all 36 parameters, calibrated one-touch RESET.
- **F16 (Master Utilities)**: Lossless 16-bit 48kHz WAV recording with valid RIFF header, dual chassis finish toggle (`#ECEBE4` Light / `#141517` Dark), A/B comparison buffer toggle, Hermite C1 soft limiter ceiling ($1.05$), M/S stereo width ($0\%$ to $200\%$).
- **F17 (Web Lifecycle Guard)**: `document.readyState` immediate execution guard, multi-event AudioContext unlock, mobile visibility change recovery, touch disambiguation on rotary controls.
- **F18 (Native 5-Platform JUCE 8)**: CMake configuration for Standalone/VST3/CLAP, APVTS parameter layout (IDs 1-36), multi-channel audio callback configuration, LookAndFeel vector graphics fallback, cross-platform header hygiene.
- **F19 (Hard Real-Time Guarantees)**: Operator new interception verifying bit-exact 0 allocations during processing, lock-free wait-free SPSC FIFO queues, hardware FTZ/DAZ denormal flags, software denormal flushing ($< 10^{-15} \to 0.0$).
- **F20 (E2E Test Architecture)**: Headless DSP test harness runner, assertion failure accounting, elapsed microsecond execution profiling, 100% pass threshold verification.

### 3.2 Tier 2: Boundary & Corner Cases (N >= 100 Tests)
Tier 2 evaluates stability under severe computational, acoustic, and mathematical stresses:
- **Multi-Rate Stress**: Execution across standard host sample rates: $44.1\text{ kHz}, 48\text{ kHz}, 88.2\text{ kHz}, 96\text{ kHz}, 176.4\text{ kHz}, 192\text{ kHz}$. All delay lengths, filters, LFOs, and decay envelopes must scale proportionally without pitch drift or instability.
- **Buffer Size Stress**: Execution with extreme block sizes: $1\text{ sample}$ (single-sample callback), $32, 64, 128, 256, 512, 1024, 2048, 4096, 8192\text{ samples}$. Verifies zero buffer overrun, zero pointer miscalculation, and sample-accurate event processing.
- **Zero Heap Allocations**: Continuous tracking via overloaded `operator new` / `operator delete`. The audio processing callback `process()` must perform **zero** heap allocations across any combination of parameters.
- **Infinite Feedback & Overload Boundedness**: Ingestion of $+40\text{ dBFS}$ input bursts and maximum decay settings ($RT60 = 100\text{ s}$, freeze engaged). The internal feedback loops must remain unconditionally stable ($|y[n]| \le 1.05$) due to the Hermite soft-knee saturator, with zero `NaN`, `Inf`, or numerical overflow.
- **Denormal & Subnormal Flushing**: Ingestion of tiny signals ($10^{-20}$) and sudden decay into silence. Verifies that CPU cycles do not spike and signals $< 10^{-15}$ are flushed to bit-exact $0.0\text{f}$.
- **Extreme Parameter Bounds**: Minimum, maximum, and out-of-range parameter clamps (e.g. curvature $\xi \in [0.8, 1.8]$, room size $0.1$ to $2.0$, pre-delay $0.0$ to $500\text{ ms}$, ducking $0\%$ to $100\%$).

### 3.3 Tier 3: Cross-Feature Combinations (Pairwise Coverage)
Tier 3 targets multi-feature interactions where independent subsystems might generate non-linear cross-talk, comb filtering, or phase conflicts:
- `T3_01`: Poincaré Hyperbolic Cavity (F1) + Barber-Pole Dimmer (F5)
- `T3_02`: Whispering Gallery Waveguide (F2) + Barber-Pole Shimmer (F6)
- `T3_03`: Anharmonic Spruce Plate (F3) + Harry Partch Sub-Harmonics (F7)
- `T3_04`: Stockhausen Klangdom (F4) + Generative Poisson Stimulus (F12)
- `T3_05`: Decoupled Low-End Ducking (F8) + Precision Hammer Thud (F11)
- `T3_06`: Dynamic Bloom Gating (F9) + Playable Chime Strip Chord Strum (F10)
- `T3_07`: Exciter `REVERB ONLY` Routing (F13) + Decoupled LR4 Crossover (F8)
- `T3_08`: Whispering Gallery Caustic Boost (F2) + Master Hermite Limiter (F16)
- `T3_09`: Infinite Decay Freeze (F1) + Lossless WAV Recording (F16)
- `T3_10`: Rapid Preset Loading (F15) during Active Multi-Voice Chime Playback (F10)
- `T3_11`: Stockhausen Klangdom Spherical Vectors (F4) + M/S Stereo Width 200% (F16)
- `T3_12`: Barber-Pole Shimmer (F6) + Barber-Pole Dimmer (F5) Dual Pitch Blend
- `T3_13`: Decoupled 120 Hz Sub-Mono (F8) + Klangdom Spatial Vectors (F4)
- `T3_14`: Laboratory Dirac Pulse (F11) + Poincaré Negative Curvature Dispersion (F1)
- `T3_15`: Generative Poisson Ambient Clock (F12) + Dynamic Bloom Onset Gate (F9)
- `T3_16`: A/B Buffer Snapshot Toggle (F16) during Continuous Pitch Spiral (F5/F6)
- `T3_17`: 192 kHz Sample Rate (F18) + 1-Sample Block Size (F19) + Anharmonic Plate (F3)
- `T3_18`: Zero Allocations (F19) under Simultaneous 16-Voice Chime Allocation (F10) + Pink Burst (F11)
- `T3_19`: Calibrated RESET (F15) during Full-Scale Input Transient Burst (F11)
- `T3_20`: Web App AudioContext Resume (F17) + Factory Preset Bank Migration (F15)
- `T3_21`: Partch Undertone Series (F7) + Elliptical Sub-Bass Mono Collapse (F8)
- `T3_22`: High-Frequency Caustic Resonance (F2) + Pink Noise Burst Windowing (F11)
- `T3_23`: Poisson Markov Step Progression (F12) + Budd Pentatonic Scale Quantization (F10)
- `T3_24`: Hermite Fractional Interpolation (F9) + Real-Time Pre-Delay Automation (F1)

### 3.4 Tier 4: Real-World Application Scenarios (Studio Workloads)
Tier 4 reproduces end-to-end studio production sessions, performance interactions, and environmental endurance tests:
- **S01: Ambient Guitar Cloud**: High diffusion ($85\%$), $16.0\text{ s}$ RT60, Poincaré hyperbolic manifold, $+12\text{ st}$ celestial shimmer, $150\%$ stereo width.
- **S02: Ethereal Synth Pad**: Whispering Gallery caustic waveguide, dual continuous Barber-Pole shimmer and dimmer at $50/50$ blend, $8.5\text{ kHz}$ air damping.
- **S03: Modern Club Kick & Sub-Bass Isolation**: Decoupled LR4 crossover ($180\text{ Hz}$), transient punch ducking ($85\%$), $120\text{ Hz}$ sub-mono collapse, laboratory kick transient excitation.
- **S04: Dark Sub-Harmonic Organ Drone**: Harry Partch sub-harmonic lattice ($-24\text{ st}$ and $-31\text{ st}$), $14.0\text{ s}$ decay, Stockhausen Klangdom spatialization, low-pass damping at $3.2\text{ kHz}$.
- **S05: Infinite Reverb Freeze & Overload Stress**: Anharmonic spruce plate with $+40\text{ dBFS}$ Dirac impulse; freeze engaged; verified bounded output ($|y| \le 1.05$) and zero thermal divergence over $500,000$ samples.
- **S06: Continuous Barber-Pole Glissando Audition**: Seamless downward Dimmer spiral sweeping continuously for $30\text{ seconds}$ without audible window resets, amplitude clicks, or boundary discontinuities.
- **S07: Bass Guitar Slap & Decay**: Struck acoustic hammer transient into spruce soundboard; verified clean fundamental attack, zero bass comb filtering nulls ($< 4.8\text{ dB}$), and natural low-frequency dissipation.
- **S08: Vocal Bloom Reverb**: Dynamic bloom gating active ($\tau_{bloom} = 120\text{ ms}$); verified zero pitch wobble on initial phonemes, followed by lush 8-phase chorused tail bloom.
- **S09: Generative Harold Budd Ambient Installation**: Poisson point process auto-generating sparse chord clusters in Budd Pentatonic scale; hands-free auditioning for $60\text{ seconds}$.
- **S10: Mastering Bus Reverb & Lossless WAV Capture**: Subtle acoustic space ($15\%$ mix, $1.8\text{ s}$ decay, $120\%$ width); live audio recorded to 16-bit 48kHz WAV buffer; RIFF file validated.
- **S11: Rapid Topological Manifold Morphing**: Switching across Poincaré, Whispering Gallery, Plate, and Klangdom at $10\text{ Hz}$ while streaming high-energy audio; zero clicks or memory fault.
- **S12: Live Performance Chime Glissando**: Rapid multi-touch swipe across all 15 chime keys with velocity variations; voice stealing allocator handles 16 voices cleanly without dropout.
- **S13: Pure Functionalist UI Showcase & A/B Audition**: Browser auditioning with complete emoji absence, DIN 1451 font inspection, patch load/export, and instant A/B acoustic comparison.
- **S14: 1,000,000-Sample Hard Real-Time Endurance Burn-In**: Uninterrupted audio processing callback stream at $192\text{ kHz}$ verifying bit-exact 0 heap allocations, zero mutex locks, and total numerical stability.

---

## 4. Coverage Thresholds

| Metric | Required Minimum | Planned / Specified | Compliance Margin |
|:-------|:----------------:|:-------------------:|:-----------------:|
| **Tier 1 (Feature Coverage)** | $\ge 100$ ($5 \times 20$ features) | **100 tests** | Exact ($5.0$ tests / feature) |
| **Tier 2 (Boundary & Corner Cases)** | $\ge 100$ ($5 \times 20$ features) | **100 tests** | Exact ($5.0$ tests / feature) |
| **Tier 3 (Cross-Feature Combinations)** | $\ge 20$ pairwise interactions | **24 tests** | $+20\%$ margin |
| **Tier 4 (Real-World Application Scenarios)**| $\ge 10$ studio scenarios | **14 scenarios** | $+40\%$ margin |
| **Total Test Suite Volume** | **$\ge 230$ tests** | **238 tests** | **Exceeds Threshold** |
| **Audio Thread Dynamic Allocations** | **0 Allocations** | **0 Allocations** | Bit-Exact Compliant |
| **Denormal Protection** | **100% Flush** | Hardware FTZ/DAZ + Soft Flush | Dual-Layer Compliant |
| **Emoji Absence in UI / Code** | **0 Emojis** | **0 Emojis** | 100% Compliant |

---

## 5. Test Execution & Automation

### 5.1 Native C++ Headless DSP Test Suite
The primary C++ test runner validates all real-time DSP, manifolds, pitch spirals, exciter models, and real-time thread safety:
```bash
# Build and execute headless C++ DSP test runner
cmake --build rb-26/build --config Release --target rb26_headless_dsp_tests
./rb-26/build/Release/rb26_headless_dsp_tests.exe
```

### 5.2 Web Audio Showcase & Browser Test Suite
Validates the Web Audio API engine, UI lifecycle, touch handling, DIN typography, and emoji excision:
```bash
# Unit verification of web engine and preset files
node rb-26/web/verify.mjs

# Headless Chromium/CDP browser audit
node rb-26/web/test-browser.mjs
```

### 5.3 Test Artifacts & Deliverables
- `rb-26/TEST_INFRA.md`: Authoritative testing specification and coverage contract (this document).
- `.agents/e2e_test_writer_track/test_plan.md`: Comprehensive 4-Tier test catalog itemizing all 238 test cases with explicit inputs, authoritative oracles, and assertions.
- `rb-26/source/tests/`: Native test source files and header libraries.
- `TEST_READY.md`: Formal verification seal published upon 100% pass of all test suites.
