## What's New in v1.4.1

### 1. Power Standby Default
* Initialized \isPoweredOn = false\ (Standby) across all native C++ plugin formats (VST3, CLAP, Standalone) and the Web Audio showcase.
* In accordance with Dieter Rams functionalist hardware behavior and studio safety practices, the audio graph remains silent on instantiation until engaged by pressing the orange **POWER** switch or the \P\ hotkey, protecting studio monitors and listening environments.

### 2. Native UI Black-Screen & Windowing Remediation
* Removed legacy Win32 child window visibility routines (\setChildHwndsVisible\ / \EnumChildWindows(..., SW_HIDE)\).
* In JUCE 8 on Windows, \SW_HIDE\ on child HWNDs hid Direct2D/DirectWrite rendering canvases inside DAW plugin windows (FL Studio, Ableton Live, Reaper, Bitwig), causing the plugin window to turn completely black when switching to UI: Native.
* Replaced handle manipulation with canonical JUCE component hierarchy management: \emoveChildComponent(webComponent.get())\ when entering Native mode, and \ddAndMakeVisible(*webComponent)\ when restoring Web mode.
* Collapses \webComponent\ bounds to \(0, 0, 0, 0)\ in Native mode, ensuring zero visual occlusion, zero GPU compositor overhead, and uninterrupted native component interaction.

### 3. DAW Host Parameter Context Menu & Automation Parity
* Integrated \juce::AudioProcessorEditor::getHostContext()->getContextMenuForParameter(param)\ into \showKnobContextMenu\.
* Right-clicking any parameter knob in Native UI mode invokes the DAW host's native context menu for direct parameter automation lane creation, MIDI learn assignment, and envelope modulation across Ableton Live, FL Studio, Reaper, Cubase, Studio One, and Bitwig.
* Standalone application falls back cleanly to the Dieter Rams popup menu for numeric parameter entry and default resets.
* Synchronized IPC bridge forwards right-click coordinates from the Web UI to trigger host parameter menus identically.

### 4. Comprehensive Test Certification
* **Headless DSP Stress Suite**: 389 / 389 passed (100% SUCCESS across Tiers 1–4).
* **M4 JUCE Concurrency & Routing Suite**: 8 / 8 passed (100% SUCCESS, 0 data races, 0 real-time allocations).
* **Web Audio & Checklist Harness**: 56 / 56 tests passed (100% SUCCESS).

---

### Distribution Packages (\eleases/\)
* **\BRAUN_RB26-v1.4.1-Windows-x64.zip\** (10.24 MB): Full Windows x64 distribution containing VST3 (\BRAUN_RB26.vst3\), CLAP (\BRAUN_RB26.clap\), Standalone application (\BRAUN_RB26.exe\), \LICENSE\, and \README.md\.
* **\BRAUN_RB26-v1.4.1-VST3-Windows-x64.zip\** (3.40 MB): VST3-only distribution package containing \BRAUN_RB26.vst3\, \LICENSE\, and \README.md\.
