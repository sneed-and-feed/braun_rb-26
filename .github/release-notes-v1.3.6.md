## What's New in v1.3.6

### 🐧 Linux & Cross-Platform Build Enhancements
- **GCC / Clang Test Suite Compilation (#1)**: Guarded `_MSVC_LANG` in `Tier4_ScenarioTests.h` behind `#if defined(_MSVC_LANG)`, eliminating the `'error: _MSVC_LANG was not declared in this scope'` failure when building tests on Linux.
- **CMake Test Build Toggle (`RB26_BUILD_TESTS`) (#1)**: Added `option(RB26_BUILD_TESTS "Build RB-26 test suites" ON)`. You can now pass `-DRB26_BUILD_TESTS=OFF` to build only plugin and standalone targets without test executables.
- **Pure Native JUCE UI Build Decoupling (#1)**: When building with `-DRB26_USE_WEBVIEW=OFF`, curl dependencies are completely omitted (`JUCE_USE_CURL=0`). When building with `-DRB26_USE_WEBVIEW=ON` on Linux, `CURL::libcurl` and `NEEDS_WEB_BROWSER TRUE` are automatically resolved.
- **Linux `libcurl` Test Suite Linkage (#1)**: Declared `CURL::libcurl` as a `PUBLIC` interface library on `rb26_dsp_core` and directory-wide in `source/tests/`, ensuring all diagnostic and stress audit executables link cleanly without unresolved curl symbols.

### 🎨 Dark Mode Contrast & Native UI Fixes
- **Native UI Combo Box Text Contrast (#1)**: In `BraunLookAndFeel::drawLabel`, dynamically queries `findColour(juce::ComboBox::textColourId)` and `getComboBoxFont` for combo box labels, ensuring crisp white text (`#F0F0F0`) on the dark inset background. Added `sendLookAndFeelChange()` propagation on theme switching.
- **Web UI `<select>` Styling on Linux (#1)**: Added `appearance: none;` (`-webkit-appearance: none;`) and custom SVG arrows to `.braun-select` in `style.css`, preventing Linux WebKitGTK from overriding the background with light native GTK widgets under dark mode.
- **Native UI `std::bad_cast` Resolution (#1)**: Replaced invalid downcast in `drawToggleButton` and added comprehensive off-screen widget rendering tests.

### 🔊 DSP & Headroom Stabilization
- **Sub-Bass Preserver & FDN Modal Stabilization**: Linearized `TransientPunchDetector` with sample-rate-invariant onset gating, eliminating the audio-rate envelope chopping and bowed-metal distortion artifact under sustained low-frequency dual-drone excitation.
- **Calibrated Gain Staging**: Aligned modal matrix injection ($0.20\times$), output extraction ($0.25\times$), and FDN tank normalization ($1/\sqrt{8} \approx 0.35355\times$) with Web Audio reference headroom.
- **In-Loop Modal Damping & Filtering**: Added 56 Hz Butterworth highpass and lowpass damping biquads inside each modal recirculation loop, and band-limited pitch shifter feedback (150 Hz HPF / 6 kHz LPF).

---

### Included Distribution Binaries:
- **`BRAUN_RB26-v1.3.6-macOS-Universal.zip`** (~23 MB): Universal binary for Apple Silicon (M1/M2/M3/M4) and Intel x86_64, including VST3 (`BRAUN_RB26.vst3`), Audio Unit (`BRAUN_RB26.component`), CLAP (`BRAUN_RB26.clap`), and Standalone (`BRAUN_RB26.app`).
- **`BRAUN_RB26-v1.3.6-Windows-x64.zip`** (~10 MB): Complete bundle (`.vst3`, `.clap`, standalone `.exe`, and web showcase).
- **`BRAUN_RB26-v1.3.6-VST3-Windows-x64.zip`** (~3.4 MB): Lightweight standalone VST3 bundle for DAWs (Ableton Live, FL Studio, Reaper, Cubase, Studio One, Bitwig).
