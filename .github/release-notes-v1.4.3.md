## What's New in v1.4.3

### 1. Window Integrity & Non-Destructive UI Switching Remediation
* **Eliminated Win32 Style Hacks**: Removed `EnumChildWindows` and `SetWindowLongPtr` style mutations that altered host window styles (`WS_CLIPCHILDREN`), resolving Win32 painting and window compositing breakage when toggling between Native and Web UI.
* **Non-Destructive Web Browser Lifecycle**: Maintained `webComponent` as an attached child throughout UI transitions, removing destructive `removeChildComponent` calls that previously tore down JUCE 8's internal WebView2 peer connection and corrupted window states upon reattachment.
* **Robust Bounds & Visibility Management**: In `setNativeMode`, the web component is cleanly hidden, collapsed to `(0, 0, 0, 0)`, and sent to the back during Native UI mode, or made visible, expanded to bounds, and brought to front during Web UI mode.
* **Idempotent Mode Switching**: Hardened both C++ IPC handlers (`handleParamChangeFromWeb`) and native button handlers (`viewModeButton.onClick`) to ensure idempotent mode transitions without redundant layout thrashing.
* **Web UI Event Debouncing**: Added event deduplication, pointer debouncing, and event propagation guards on the Web UI `uiModeBtn` to prevent double-firing and race conditions.

### 2. Startup Persistence & Test Isolation
* **Clean Default Startup**: Ensured the plugin cleanly defaults to the modern Web UI upon cold launch when no preference is persisted.
* **Test Harness Setting Isolation**: Updated M4 concurrency and routing test suite (`runTest8_NativeUIOcclusionAndContextMenu`) to reset the persisted mode to Web UI prior to destruction, preventing test runs from corrupting `%APPDATA%\Braun\RB26_settings.xml`.

### 3. Comprehensive Verification & Stability Certification
* **Headless DSP Stress Suite**: 389 / 389 passed (100% SUCCESS across Tiers 1–4).
* **M4 JUCE Concurrency & Routing Suite**: 8 / 8 passed (100% SUCCESS, 0 data races, 0 real-time allocations).
* **Web Audio & Checklist Harness**: 56 / 56 tests passed (100% SUCCESS).

---

### Distribution Packages (`releases/`)
* **`BRAUN_RB26-v1.4.3-Windows-x64.zip`**: Full Windows x64 distribution containing VST3 (`BRAUN_RB26.vst3`), CLAP (`BRAUN_RB26.clap`), Standalone application (`BRAUN_RB26.exe`), `LICENSE`, and `README.md`.
* **`BRAUN_RB26-v1.4.3-VST3-Windows-x64.zip`**: VST3-only distribution package containing `BRAUN_RB26.vst3`, `LICENSE`, and `README.md`.
