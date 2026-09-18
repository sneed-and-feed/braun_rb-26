## What's New in v1.4.2

### 1. Runtime Native UI Occlusion Remediation
* Resolved pitch-black screen rendering when toggling between Web UI and Native UI at runtime.
* Corrected component hierarchy and movement watcher lifecycle: `webComponent` bounds are now pre-collapsed to `(0, 0, 0, 0)` and hidden before detachment from the peer window.
* Implemented Win32 child window traversal on Windows to explicitly hide underlying Chromium/WebView2 render and intermediate surfaces (`::ShowWindow(child, SW_HIDE)` and `::SetWindowPos(..., SWP_HIDEWINDOW)`).
* Stripped `WS_CLIPCHILDREN` from the peer `HWND` when entering Native UI mode, preventing the Win32 window compositor from clipping out the JUCE vector paint canvas. Restores `WS_CLIPCHILDREN` dynamically upon returning to Web UI mode.

### 2. DAW Host Context Menu & Parameter Automation Parity
* Integrated `juce::AudioProcessorEditor::getHostContext()->getContextMenuForParameter(param)->showNativeMenu(localPos)` into `showKnobContextMenu`.
* Invoking context menus (right-click) on knobs or labels now triggers the host DAW's native parameter automation, modulation envelope, and MIDI learn menus across Ableton Live, FL Studio, Reaper, Cubase, Studio One, and Bitwig.
* Fully maintained standalone fallback to the Dieter Rams popup menu for numeric parameter input and default value resets.
* Synchronized Web UI IPC event routing to forward cursor coordinates for seamless host menu popups from web components.

### 3. Comprehensive Verification & Stability Certification
* **Headless DSP Stress Suite**: 389 / 389 passed (100% SUCCESS across Tiers 1–4).
* **M4 JUCE Concurrency & Routing Suite**: 8 / 8 passed (100% SUCCESS, 0 data races, 0 real-time allocations).
* **Web Audio & Checklist Harness**: 56 / 56 tests passed (100% SUCCESS).

---

### Distribution Packages (`releases/`)
* **`BRAUN_RB26-v1.4.2-Windows-x64.zip`**: Full Windows x64 distribution containing VST3 (`BRAUN_RB26.vst3`), CLAP (`BRAUN_RB26.clap`), Standalone application (`BRAUN_RB26.exe`), `LICENSE`, and `README.md`.
* **`BRAUN_RB26-v1.4.2-VST3-Windows-x64.zip`**: VST3-only distribution package containing `BRAUN_RB26.vst3`, `LICENSE`, and `README.md`.
