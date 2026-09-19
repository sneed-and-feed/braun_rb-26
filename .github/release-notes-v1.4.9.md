## What's New in v1.4.9

### 1. Pitch Delay Host Context Menu Bug Fix
* **Missing Knob-to-Parameter Map Entry**: Resolved an issue where right-clicking the Deck 04 **PITCH DELAY** knob (`knob-pitch-delay`) failed to open the DAW host context menu (MIDI learn, parameter automation, modulation routing).
* **Root Cause**: `knob-pitch-delay` was missing from `knobParamMap` in `web/js/app.js`, causing right-click events to emit raw DOM IDs (`knob-pitch-delay`) rather than canonical APVTS parameter IDs (`pitch_delay_ms`).
* **Web UI Registration**: Added `'knob-pitch-delay': 'pitch_delay_ms'` to `knobParamMap` in `_initKnobs()` and explicitly configured `paramId: 'pitch_delay_ms'` on the `BraunKnob` instantiation for pitch delay.

### 2. Robust Fuzzy Parameter Resolution in PluginEditor
* **Host Parameter Fuzzy Fallback**: Enhanced `BRAUN_RB26AudioProcessorEditor::findKnob()` in `source/plugin/PluginEditor.cpp` with suffix-aware matching (`_ms`, `_hz`, `_db`) after stripping DOM prefixes (`knob-` / `knob_`).
* **Multi-Format Interop Guarantee**: Guarantees that parameter queries from frontend events, WebView IPC, or DAW wrappers resolve deterministically to their respective APVTS knob slots even if unit suffixes (`pitch_delay` vs `pitch_delay_ms`) are omitted.

### 3. Automated Verification & Regression Prevention
* **Automated Knob Mapping Audit (`web/test-checklist.mjs`)**: Added test asserting that all 23 rotary knob containers in `web/index.html` have valid mappings in `knobParamMap` and map directly to APVTS parameters registered in `getParameterMetadataTable()`.
* **C++ Concurrency & Occlusion Suite (`M4JuceConcurrencyRoutingTests.cpp`)**: Added explicit assertions in `runTest10_NativeUIOcclusionAndContextMenu()` verifying `findKnob` resolution for `pitch_delay_ms`, `pitchDelayMs`, `knob-pitch-delay`, and `pitch_delay`.

### 4. Verification & Diagnostic Metrics
* **Headless DSP Stress Suite**: 389 / 389 passed (100% SUCCESS across Tiers 1–4).
* **Acoustic Decay Audit**: 180 / 180 assertions passed (100% SUCCESS across Suites A–H).
* **Web Audio & Checklist Harness**: 60 / 60 passed (36 / 36 Web unit tests + 24 / 24 checklist tests, 100% SUCCESS).
* **M4 Plugin & Concurrency Suite**: 10 / 10 test suites passed (100% SUCCESS).
* **CTest Suite**: 5 / 5 test suites passed (100% SUCCESS).

---

### Distribution Packages (`releases/`)
* **`BRAUN_RB26-v1.4.9-Windows-x64.zip`**: Full Windows x64 distribution containing VST3 (`BRAUN_RB26.vst3`), CLAP (`BRAUN_RB26.clap`), Standalone application (`BRAUN_RB26.exe`), `LICENSE`, and `README.md`.
* **`BRAUN_RB26-v1.4.9-VST3-Windows-x64.zip`**: Lightweight VST3-only distribution package containing `BRAUN_RB26.vst3`, `LICENSE`, and `README.md`.
