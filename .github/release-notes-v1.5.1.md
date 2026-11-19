# BRAUN RB-26 v1.5.1 — Windows Standalone VB-Cable Dual-Clock ASRC, S3/S0 Sleep/Wake Audio Recovery & Background Anti-Throttling

## Overview

BRAUN RB-26 release **v1.5.1** resolves long-uptime and post-sleep audio popping in the **Windows Standalone (`BRAUN_RB26.exe`)** when routing live desktop, microphone, or virtual audio cables (such as **VB-Audio Virtual Cable**) into a separate hardware output endpoint—including over **Standby passthrough** and **0% wet mix**.

In strict adherence to Dieter Rams' functionalist principles—*"Good design is thorough down to the last detail"* and *"Weniger, aber besser"* (Less, but better)—this release decouples separate-clock Windows capture and render endpoints onto independent real-time threads bridged by a lock-free 4-point Hermite asynchronous sample-rate converter (ASRC), adds S3 Sleep and Windows 10/11 S0 Modern Standby suspend/resume recovery across `D3 -> D0` codec wake transitions, guards against `AUDCLNT_E_DEVICE_INVALIDATED` callback storms, opts out of Windows background `ProcessPowerThrottling` (EcoQoS and timer coalescing), and suspends occluded WebView2 telemetry IPC while minimized.

---

## What's Fixed in v1.5.1

### 1. Dual-Clock Drift Elimination for VB-Cable & Multi-Endpoint Windows Audio (`StandaloneAudioGuard.h`, `Rb26StandaloneApp.cpp`)
* **Independent Capture/Render Clock Domains (`DriftCompensatedAudioIODeviceType`, `DuplexDriftCompensatedAudioIODevice`)**:
  * *Root Cause*: Stock JUCE `WASAPIAudioIODevice` couples input and output inside a single render-clocked loop with an initially empty (`0`-frame) `SingleThreadedAbstractFifo` reservoir. When pairing a software-clocked virtual cable (`CABLE Output (VB-Audio Virtual Cable)`, driven by QPC) with a hardware-crystal output (`Realtek` / USB DAC, typically $\pm 10\text{–}100\text{ ppm}$ relative clock offset):
    1. Under negative drift (`f_in < f_out`), `copyBuffersFromReservoir` repeatedly hits `0` readable samples and splices raw `0.0f` blocks (`zeromem`) directly into live audio.
    2. Under positive drift (`f_in > f_out`) or brief render stalls, `handleDeviceBuffer` hits `ReleaseBuffer(0)` once the reservoir fills (~10–20 minutes of uptime), overflowing Windows' internal ~20 ms WASAPI capture endpoint buffer and dropping 10 ms packets continuously—even in Standby passthrough (`!isPoweredOn`).
  * *Fix*: Wrapped separate-endpoint Windows device types (`Windows Audio`, `Windows Audio (Exclusive Mode)`, `Windows Audio (Low Latency Mode)`, and `DirectSound`, leaving single-clock `ASIO` untouched) so capture and render run as independent single-direction `AudioIODevice` threads bridged by `ElasticDriftRingBuffer`.
* **Lock-Free 64-Bit SPSC Elastic Ring Buffer & Overdamped 4-Point Hermite ASRC (`ElasticDriftRingBuffer`)**:
  * Pre-primes a symmetric bidirectional cushion (`mTargetFill - P/2`) in a `32,768`-frame circular buffer (`0` real-time heap allocations, `0` locks) and tracks clock drift via an overdamped gain-scheduled 2nd-order PI controller ($\zeta = 1.15, \omega_n = 0.025\text{ rad/s}$) driving 4-point 3rd-order Hermite interpolation (`< 0.5 cents` worst-case packet-beat ripple).
  * Automatically performs asynchronous sample-rate conversion when Low-Latency or Exclusive endpoints are locked to distinct native rates (e.g. $44.1\text{ kHz}$ VB-Cable into $48.0\text{ kHz}$ speakers), applies click-free 128-sample raised-cosine dual-head recentering on severe output stalls, and decays/fades smoothly on capture underruns.

### 2. Laptop Sleep / S0 Modern Standby Wake Recovery & Invalidated Endpoint Storm Guard (`StandaloneAudioGuard.h`)
* **S3 Sleep & S0 Low Power Idle (Modern Standby) Endpoint Recovery (`WindowsPowerSuspendNotifier`, `StandaloneAudioRecoveryController`)**:
  * *Root Cause*: JUCE Standalone lacks `WM_POWERBROADCAST` suspend/resume handling, ignores `IAudioSessionEvents::OnSessionDisconnected` reasons other than format change, and wipes `AudioDeviceManager::currentSetup` via `deleteCurrentDevice()` if an immediate wake-time reopen fails while laptop audio codecs are still transitioning from `D3` to `D0`.
  * *Fix*: Added a hidden top-level `WS_POPUP` `WM_POWERBROADCAST` listener paired with `PowerRegisterSuspendResumeNotification` (`powrprof.dll`, covering Windows 10/11 S0 Modern Standby lid close/reopen) plus a wall-clock sleep-jump detector. Cleanly closes audio handles on `PBT_APMSUSPEND` and restores the preserved `AudioDeviceSetup` across progressive backoff retries on resume (`650 ms` initial settle + up to `8` retries) before resetting DSP buffers.
* **Invalidated WASAPI Spin-Storm Throttling & Hot Input Capture Recovery (`DuplexDriftCompensatedAudioIODevice`)**:
  * *Root Cause*: When `IAudioClient::GetCurrentPadding` fails with `AUDCLNT_E_DEVICE_INVALIDATED` after sleep/wake, JUCE's `WASAPIOutputDevice` falls through returning `actualBufferSize` without waiting on `clientEvent`, spinning `processBlock` unthrottled at 100% CPU; conversely, input-only WASAPI threads exit silently if `WaitForSingleObject(1000 ms)` times out.
  * *Fix*: Added a physically invariant callback-rate detector (`>= 24` blocks and `>= 200 ms` of audio frames completing in `< 25%` of real-time wall-clock duration) that throttles invalidated spin storms and triggers clean stream reopen, alongside non-disruptive message-thread hot recovery of capture if the input-only handle closes mid-session.

### 3. Windows Background Power-Throttling Opt-Out, Occluded WebView2 IPC Guard & Idle Wipe Optimization
* **EcoQoS & Timer Coalescing Opt-Out (`WindowsAudioProcessGuard`)**:
  * Calls `SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, ...)` to disable `PROCESS_POWER_THROTTLING_EXECUTION_SPEED` and `PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION` (with automatic fallback for Windows 10 1709–1909 builds) and requests `1 ms` timer resolution (`timeBeginPeriod(1)`), preventing background CPU frequency capping and `15.625 ms` timer coalescing during unattended passthrough sessions.
* **Minimized/Hidden WebView2 IPC Suspension & Per-Host Profile Isolation (`PluginEditor.cpp`, `Rb26Engine.cpp`)**:
  * Added Win32 child + `GA_ROOT` `IsWindowVisible` / `!IsIconic` checks (`isWebViewActuallyShowing()`) to halt 120 Hz JSON `telemetryFrame` + `scopeFrame` IPC streaming while minimized or hidden (eliminating multi-hour background IPC/V8 queue growth) with automatic full parameter sync on restore, scoped WebView2 `UserDataFolder` per host executable, and added `mPitchBuffersDirty` in `Rb26ReverbEngine` to skip redundant `5.5 MiB` pitch buffer wipes on idle silence transitions when pitch diffusion is inactive.

---

## Verification & Diagnostic Summary

| Test Suite | Assertions / Cases | Pass Rate | Status |
|:---|:---:|:---:|:---|
| Standalone Audio Guard & Live WASAPI/VB-Cable Suite (`Rb26StandaloneAudioGuardTests`) | 6 / 6 Suites (12M+ Drift Frames + Live WASAPI) | 100% | UNANIMOUS PASS |
| Headless DSP Test Suite (`Rb26HeadlessDspTests`, Tiers 0–3) | 169 Suites | 100% | UNANIMOUS PASS |
| DSP Challenger Suite (`Rb26DspChallengerTests`, Tests 1–10) | 10 / 10 Suites | 100% | UNANIMOUS PASS |
| Challenger M2-1 Empirical Verification (`Rb26Challenger1M2Tests`) | 960,041 / 960,041 | 100% | UNANIMOUS PASS |
| Challenger M3-2 Stress Audit (`Rb26M3StressAudit`) | 262,455 / 262,455 | 100% | UNANIMOUS PASS |
| Milestone M4 JUCE & Concurrency Suite (`Rb26M4JuceTests`, Tests 1–12) | 12 / 12 Suites | 100% | UNANIMOUS PASS |
| Acoustic Decay Audit (`Rb26AcousticDecayAudit`) | Suites A–H | 100% | UNANIMOUS PASS |
| Multi-Channel Surround & Atmos 7.1.4 Suite (`Rb26MultiChannelTests`) | 7 / 7 Layouts | 100% | UNANIMOUS PASS |
| Felt Hammer Transient & Voice Steal Suite (`Rb26HammerTransientTests`) | 5 / 5 Suites | 100% | UNANIMOUS PASS |
| LookAndFeel & Web Resource Integrity (`Rb26LookAndFeelTests`, `Rb26WebResourceTests`) | Full Suite | 100% | UNANIMOUS PASS |
| Web UI, IPC & Checklist Verification (`web/verify.mjs`, `web/test-checklist.mjs`) | 65 / 65 Assertions | 100% | UNANIMOUS PASS |
| GitHub Math & KaTeX Lint (`scripts/audit_gfm_math.py`) | 26 / 26 Markdown Files | 100% | ZERO ISSUES |

---

## Downloads & SHA-256 Checksums

| Package Archive | Target Platform & Contents | Size | SHA-256 Checksum |
|:---|:---|:---:|:---|
| **`BRAUN_RB26-v1.5.1-Windows-x64.zip`** | Windows x64 Full (`BRAUN_RB26.vst3`, `BRAUN_RB26.clap`, `BRAUN_RB26.exe`, Docs) | 10,809,568 B (~10.3 MB) | `00c12fb08abc321be8c45e7db4ebcf5c88b94aa774d3963b43978d72f634d968` |
| **`BRAUN_RB26-v1.5.1-VST3-Windows-x64.zip`** | Windows x64 VST3 Only (`BRAUN_RB26.vst3`, Docs) | 3,600,294 B (~3.4 MB) | `db3e2c11245f549c6daf34d694cb97899398d3d11848955d35118b7118e743cd` |
| **`BRAUN_RB26-v1.5.1-macOS-Universal.zip`** | macOS Universal (AU, VST3, CLAP, Standalone App) | *CI Artifact* | *Published via macOS CI workflow* |
| **`BRAUN_RB26-v1.5.1-Linux-x64.tar.gz`** | Linux x86_64 (VST3, CLAP, Standalone binary) | *CI Artifact* | *Published via Linux CI workflow* |

All release archives are verified against [`releases/SHA256SUMS.txt`](releases/SHA256SUMS.txt).
