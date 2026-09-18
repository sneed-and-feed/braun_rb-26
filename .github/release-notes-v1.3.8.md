## What's New in v1.3.8

### 🎛️ Chassis & Responsive UI Layout
- **Default Window 3-Column View**: Default plugin window opens directly into the full 3-column layout (1280x760, 19" studio rack chassis) across standalone and DAW host windows.
- **Responsive Breakpoint Tuning**: Breakpoint tuned to 1040px to prevent premature 2-column collapsing on standard HD displays and DAW floating windows.
- **Freeze LED Circle Unlit State**: Corrected unlit styling (`:not(.is-active) .braun-led`) ensuring the circular indicator displays an accurate unlit appearance when freeze hold is inactive.

### 🔌 Host Integration & DAW Workflow
- **Persistent Native DAW UI / Web UI Switching**: Seamless switching between the high-fidelity Web UI and Dieter Rams Native DAW UI via the header toggle button, with persistence saved in `%APPDATA%/Braun/RB26_settings.xml`.
- **Native DAW Context Menu Integration**: Parameter sliders and controls integrate native DAW context menus for host parameter automation, MIDI learn, and modulation.
- **Web View Context Menu Suppression**: Disabled default Chromium/Edge context menus in Web view for a seamless, distraction-free studio rack experience.

### 🔊 DSP & Parameter Smoothing Calibration
- **Input Trim Volume Range Calibration**: Calibrated input trim volume range across $[-18, +18]\text{ dB}$ for precision studio gain staging.
- **Host Echo Suppression**: Implemented 250ms host echo suppression preventing volume locking and parameter jumping during rapid host automation.

---

### Included Distribution Binaries:
- **`BRAUN_RB26-v1.3.8-macOS-Universal.zip`** (~23 MB): Universal binary for Apple Silicon (M1/M2/M3/M4) and Intel x86_64, including VST3 (`BRAUN_RB26.vst3`), Audio Unit (`BRAUN_RB26.component`), CLAP (`BRAUN_RB26.clap`), and Standalone (`BRAUN_RB26.app`).
- **`BRAUN_RB26-v1.3.8-Windows-x64.zip`** (~10 MB): Complete bundle (`.vst3`, `.clap`, standalone `.exe`, and web showcase).
- **`BRAUN_RB26-v1.3.8-VST3-Windows-x64.zip`** (~3.4 MB): Lightweight standalone VST3 bundle for DAWs (Ableton Live, FL Studio, Reaper, Cubase, Studio One, Bitwig).
