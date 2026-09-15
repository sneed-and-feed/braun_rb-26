/**
 * @file verify.mjs
 * @brief Independent Verification Suite for BRAUN RB-26 UI & Web Showcase
 */

import { describe, it } from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

describe('BRAUN RB-26 Milestone M4 Verification Suite', () => {

  //----------------------------------------------------------------------------
  describe('1. Dieter Rams Sibling Aesthetic Parity Check', () => {
    it('verifies exact AS-42 color token parity in style.css', () => {
      const cssPath = path.join(__dirname, 'css', 'style.css');
      const css = fs.readFileSync(cssPath, 'utf8');

      // Key AS-42 palette tokens
      assert.ok(css.includes('#ECEBE4'), 'Light chassis #ECEBE4 must be defined');
      assert.ok(css.includes('#E2E0D8'), 'Panel background #E2E0D8 must be defined');
      assert.ok(css.includes('#141517'), 'Dark anthracite chassis #141517 must be defined');
      assert.ok(css.includes('#EE592B'), 'Braun orange #EE592B must be defined');
      assert.ok(css.includes('#24FF6A'), 'CRT phosphor green #24FF6A must be defined');
      assert.ok(css.includes('#E5A93C'), 'Warning amber #E5A93C must be defined');
      assert.ok(css.includes('--font-sans'), 'DIN / Helvetica typography font family must be defined');
      assert.ok(css.includes('--font-mono'), 'Monospace readout font family must be defined');
    });

    it('verifies tablet touch-scrolling rules in html and body', () => {
      const cssPath = path.join(__dirname, 'css', 'style.css');
      const css = fs.readFileSync(cssPath, 'utf8');

      assert.ok(css.includes('overscroll-behavior-y: auto'), 'html/body must allow vertical overscroll');
      assert.ok(css.includes('-webkit-overflow-scrolling: touch'), 'iOS momentum scrolling must be enabled');
      assert.ok(css.includes('touch-action: pan-y'), 'touch-action pan-y must be set on body');
    });

    it('verifies 19-inch 2U rack enclosure styling in rack.css', () => {
      const rackPath = path.join(__dirname, 'css', 'rack.css');
      const rack = fs.readFileSync(rackPath, 'utf8');

      assert.ok(rack.includes('.braun-rack-ear'), 'Rack ears must be styled');
      assert.ok(rack.includes('.braun-rack-screw'), 'Hex countersunk rack screws must be styled');
      assert.ok(rack.includes('.braun-rack-grid'), '6-deck rack grid layout must be defined');
      assert.ok(rack.includes('@media (max-width: 1139px)'), 'Tablet landscape breakpoint must be defined');
      assert.ok(rack.includes('@media (max-width: 820px)'), 'Mobile/portrait breakpoint must be defined');
    });
  });

  //----------------------------------------------------------------------------
  describe('2. 19" Rackmount 6-Deck Chassis & HTML DOM Hierarchy', () => {
    it('verifies all 6 signal-flow decks exist in index.html', () => {
      const htmlPath = path.join(__dirname, 'index.html');
      const html = fs.readFileSync(htmlPath, 'utf8');

      assert.ok(html.includes('01</span>'), 'Deck 1 must be present');
      assert.ok(html.includes('INPUT / PRE-DELAY'), 'Deck 1 title must be present');

      assert.ok(html.includes('02</span>'), 'Deck 2 must be present');
      assert.ok(html.includes('LOW-END MATRIX'), 'Deck 2 title must be present');

      assert.ok(html.includes('03</span>'), 'Deck 3 must be present');
      assert.ok(html.includes('REVERB CORE TANK'), 'Deck 3 title must be present');

      assert.ok(html.includes('04</span>'), 'Deck 4 must be present');
      assert.ok(html.includes('PITCH DIFFUSION'), 'Deck 4 title must be present');

      assert.ok(html.includes('05</span>'), 'Deck 5 must be present');
      assert.ok(html.includes('TAIL MODULATION'), 'Deck 5 title must be present');

      assert.ok(html.includes('06</span>'), 'Deck 6 must be present');
      assert.ok(html.includes('MASTER BUS & MONITOR'), 'Deck 6 title must be present');
    });

    it('verifies 21 rotary knob containers exist in index.html for 24-parameter matrix', () => {
      const htmlPath = path.join(__dirname, 'index.html');
      const html = fs.readFileSync(htmlPath, 'utf8');

      const expectedKnobs = [
        'knob-predelay', 'knob-diffusion', 'knob-input-trim',
        'knob-low-crossover', 'knob-damping-low', 'knob-low-punch', 'knob-mono-bass',
        'knob-rt60-decay', 'knob-room-size', 'knob-damping-high',
        'knob-shimmer-send', 'knob-dimmer-send', 'knob-shim-dim-blend', 'knob-pitch-regen',
        'knob-tail-mod-rate', 'knob-tail-mod-depth', 'knob-tail-bloom',
        'knob-stereo-width', 'knob-early-late-mix', 'knob-dry-wet-mix', 'knob-output-trim'
      ];

      for (const k of expectedKnobs) {
        assert.ok(html.includes(`id="${k}"`), `DOM element id="${k}" must exist`);
      }
    });

    it('verifies CRT canvas and switch controls exist in index.html', () => {
      const htmlPath = path.join(__dirname, 'index.html');
      const html = fs.readFileSync(htmlPath, 'utf8');

      assert.ok(html.includes('id="scope-canvas"'), 'Canvas #scope-canvas must exist');
      assert.ok(html.includes('id="btn-decay-hold"'), 'Freeze hold button must exist');
      assert.ok(html.includes('id="btn-soft-limiter"'), 'Soft limiter button must exist');
      assert.ok(html.includes('id="select-preset"'), 'Preset selector must exist');
      assert.ok(html.includes('id="select-theme"'), 'Theme selector must exist');
      assert.ok(html.includes('id="btn-power"'), 'Power button must exist');
    });

    it('verifies zero-install audition sound buttons in index.html', () => {
      const htmlPath = path.join(__dirname, 'index.html');
      const html = fs.readFileSync(htmlPath, 'utf8');

      assert.ok(html.includes('id="btn-audition-impulse"'), 'Dirac impulse button must exist');
      assert.ok(html.includes('id="btn-audition-kick"'), '808 kick button must exist');
      assert.ok(html.includes('id="btn-audition-snare"'), 'Snare button must exist');
      assert.ok(html.includes('id="btn-audition-piano"'), 'Felt piano button must exist');
      assert.ok(html.includes('id="btn-audition-pad"'), 'Synth pad button must exist');
    });
  });

  //----------------------------------------------------------------------------
  describe('3. Native C++ BraunLookAndFeel Implementation', () => {
    it('verifies BraunLookAndFeel.h declarations and structure', () => {
      const hPath = path.join(__dirname, '..', 'source', 'plugin', 'LookAndFeel', 'BraunLookAndFeel.h');
      const content = fs.readFileSync(hPath, 'utf8');

      assert.ok(content.includes('class BraunLookAndFeel : public juce::LookAndFeel_V4'),
        'Must inherit juce::LookAndFeel_V4');
      assert.ok(content.includes('struct BraunColours'), 'Must define BraunColours');
      assert.ok(content.includes('Light_BgApp          = 0xFFECEBE4'), 'Must declare Light_BgApp');
      assert.ok(content.includes('Dark_BgApp           = 0xFF141517'), 'Must declare Dark_BgApp');
      assert.ok(content.includes('Accent_BraunOrange   = 0xFFEE592B'), 'Must declare Accent_BraunOrange');
      assert.ok(content.includes('PhosphorGreen        = 0xFF24FF6A'), 'Must declare PhosphorGreen');
      assert.ok(content.includes('enum class KnobTier'), 'Must define KnobTier hierarchy');
      assert.ok(content.includes('class CrtVisualizerComponent'), 'Must define CrtVisualizerComponent');
    });

    it('verifies BraunLookAndFeel.cpp implementation details', () => {
      const cppPath = path.join(__dirname, '..', 'source', 'plugin', 'LookAndFeel', 'BraunLookAndFeel.cpp');
      const content = fs.readFileSync(cppPath, 'utf8');

      assert.ok(content.includes('void BraunLookAndFeel::drawRotarySlider'), 'drawRotarySlider must be implemented');
      assert.ok(content.includes('void BraunLookAndFeel::drawToggleButton'), 'drawToggleButton must be implemented');
      assert.ok(content.includes('void BraunLookAndFeel::drawTickBox'), 'drawTickBox must be implemented');
      assert.ok(content.includes('void CrtVisualizerComponent::drawGraticule'), 'drawGraticule must be implemented');
      assert.ok(content.includes('void CrtVisualizerComponent::drawEdcWaterfall'), 'drawEdcWaterfall must be implemented');
      assert.ok(content.includes('void CrtVisualizerComponent::drawLissajous'), 'drawLissajous must be implemented');
      assert.ok(content.includes('void CrtVisualizerComponent::drawSpectrum'), 'drawSpectrum must be implemented');
    });
  });

  //----------------------------------------------------------------------------
  describe('4. Factory Presets and Sound Engine Integrity', () => {
    it('verifies factory presets contain all 12 curated sound patches', () => {
      const presetsPath = path.join(__dirname, 'presets', 'factory_presets.json');
      const json = JSON.parse(fs.readFileSync(presetsPath, 'utf8'));

      assert.strictEqual(json.device, 'BRAUN_RB26');
      assert.strictEqual(json.presets.length, 12, 'Must contain exactly 12 presets (8 classic + 4 Non-Euclidean spaces)');

      const ids = json.presets.map((p) => p.id);
      // 8 Classic Presets
      assert.ok(ids.includes('DEFAULT'));
      assert.ok(ids.includes('AMBIENT_GUITAR_CLOUD'));
      assert.ok(ids.includes('ETHEREAL_SYNTH_PAD'));
      assert.ok(ids.includes('CLUB_KICK_TIGHT'));
      assert.ok(ids.includes('DARK_SUB_DRONE'));
      assert.ok(ids.includes('CATHEDRAL_SHIMMER'));
      assert.ok(ids.includes('INFINITE_FREEZE_DRONE'));
      assert.ok(ids.includes('SUB-BASS_PRESERVER'));

      // 4 Non-Euclidean Spatial Manifolds
      assert.ok(ids.includes('POINCARE_CAVITY'), 'Must include POINCARE_CAVITY');
      assert.ok(ids.includes('WHISPERING_GALLERY'), 'Must include WHISPERING_GALLERY');
      assert.ok(ids.includes('SPRUCE_SOUNDBOARD'), 'Must include SPRUCE_SOUNDBOARD');
      assert.ok(ids.includes('KLANGDOM_SPHERE'), 'Must include KLANGDOM_SPHERE');

      for (const preset of json.presets) {
        assert.ok(typeof preset.params.rt60_decay === 'number');
        assert.ok(typeof preset.params.room_size === 'number');
        assert.ok(typeof preset.params.shimmer_send === 'number');
        assert.ok(typeof preset.params.dimmer_send === 'number');
        assert.ok(typeof preset.params.dry_wet_mix === 'number');
      }
    });

    it('verifies Hermite soft saturation transfer curve in Web Engine', async () => {
      const { Rb26WebEngine } = await import('./js/audio/rb26_web_engine.js');
      const engine = new Rb26WebEngine();
      const curve = engine._generateHermiteCurve();

      assert.strictEqual(curve.length, 1024);
      // Verify odd symmetry: curve[0] approx -curve[1023]
      assert.ok(Math.abs(curve[0] + curve[1023]) < 0.01);
      // Verify bounded output within [-1.05, 1.05]
      for (let i = 0; i < curve.length; i++) {
        assert.ok(Math.abs(curve[i]) <= 1.06, `Curve point ${i} value ${curve[i]} must be bounded`);
      }
      // Center point (x=0) is 0
      assert.ok(Math.abs(curve[512]) < 0.02);
    });
  });

  //----------------------------------------------------------------------------
  describe('5. Knob Math & Touch Disambiguation Mechanics', () => {
    it('verifies logarithmic and linear normalization in BraunKnob', async () => {
      const { BraunKnob } = await import('./js/ui/knob.js');

      // Linear knob
      const linearKnob = new BraunKnob(null);
      linearKnob.min = 0;
      linearKnob.max = 100;
      linearKnob.isLog = false;

      assert.strictEqual(linearKnob.toNormalized(0), 0);
      assert.strictEqual(linearKnob.toNormalized(50), 0.5);
      assert.strictEqual(linearKnob.toNormalized(100), 1.0);
      assert.strictEqual(linearKnob.fromNormalized(0.5), 50);

      // Logarithmic knob (e.g. 1000 to 20000 Hz)
      const logKnob = new BraunKnob(null);
      logKnob.min = 1000;
      logKnob.max = 20000;
      logKnob.isLog = true;

      const midVal = logKnob.fromNormalized(0.5);
      assert.ok(midVal > 4000 && midVal < 5000, `Log midpoint ${midVal} should be ~4472 Hz`);
    });
  });

  //----------------------------------------------------------------------------
  describe('6. Dieter Rams Functionalist Identity: 100% Emoji Elimination Audit', () => {
    it('verifies zero emojis across index.html, test-browser.mjs, style.css, and presets', () => {
      const emojiRegex = /[\u{1F300}-\u{1F9FF}]|[\u{2600}-\u{26FF}]|[\u{2700}-\u{27BF}]/u;

      const filesToCheck = [
        path.join(__dirname, 'index.html'),
        path.join(__dirname, 'test-browser.mjs'),
        path.join(__dirname, 'css', 'style.css'),
        path.join(__dirname, 'presets', 'factory_presets.json')
      ];

      for (const file of filesToCheck) {
        const content = fs.readFileSync(file, 'utf8');
        const matches = content.match(new RegExp(emojiRegex, 'gu'));
        assert.strictEqual(matches, null, `File ${path.basename(file)} must contain 0 emojis, found: ${matches}`);
      }
    });

    it('verifies authentic Braun engineering nomenclature in audition buttons', () => {
      const htmlPath = path.join(__dirname, 'index.html');
      const html = fs.readFileSync(htmlPath, 'utf8');

      assert.ok(html.includes('DIRAC IMPULSE'), 'Must include DIRAC IMPULSE nomenclature');
      assert.ok(html.includes('ACOUSTIC HAMMER'), 'Must include ACOUSTIC HAMMER nomenclature');
      assert.ok(html.includes('PINK BURST'), 'Must include PINK BURST nomenclature');
      assert.ok(html.includes('PIANO CHORD'), 'Must include PIANO CHORD nomenclature');
      assert.ok(html.includes('POISSON CLOCK'), 'Must include POISSON CLOCK nomenclature');
    });
  });

  //----------------------------------------------------------------------------
  describe('7. Web App Lifecycle Guard & Touch Isolation', () => {
    it('verifies DOMContentLoaded race condition check in app.js', () => {
      const appJsPath = path.join(__dirname, 'js', 'app.js');
      const content = fs.readFileSync(appJsPath, 'utf8');

      assert.ok(content.includes('document.readyState === \'interactive\'') ||
                content.includes('document.readyState === "interactive"'),
        'Must check document.readyState for interactive state');
      assert.ok(content.includes('document.readyState === \'complete\'') ||
                content.includes('document.readyState === "complete"'),
        'Must check document.readyState for complete state');
    });

    it('verifies guaranteed audio unlock multi-event listener in app.js', () => {
      const appJsPath = path.join(__dirname, 'js', 'app.js');
      const content = fs.readFileSync(appJsPath, 'utf8');

      assert.ok(content.includes('touchstart'), 'Must listen for touchstart');
      assert.ok(content.includes('touchend'), 'Must listen for touchend');
      assert.ok(content.includes('pointerdown'), 'Must listen for pointerdown');
      assert.ok(content.includes('mousedown'), 'Must listen for mousedown');
      assert.ok(content.includes('keydown'), 'Must listen for keydown');
      assert.ok(content.includes('visibilitychange'), 'Must listen for visibilitychange for auto-recovery');
    });
  });

  //----------------------------------------------------------------------------
  describe('8. Master Utilities: Lossless WAV Recording & A/B Buffer', () => {
    it('verifies MasterWavRecorder implements 16-bit 48kHz RIFF/WAVE encoding', async () => {
      const { MasterWavRecorder } = await import('./js/app.js');
      const recorder = new MasterWavRecorder(null, null);

      // Create test left and right synthetic float buffers (1000 samples each)
      const numSamples = 1000;
      const left = new Float32Array(numSamples);
      const right = new Float32Array(numSamples);
      for (let i = 0; i < numSamples; i++) {
        left[i] = Math.sin((i / numSamples) * Math.PI * 2);
        right[i] = Math.cos((i / numSamples) * Math.PI * 2);
      }

      const blob = recorder.encodeWAV(left, right, 48000);
      assert.strictEqual(blob.type, 'audio/wav');

      const arrayBuf = await blob.arrayBuffer();
      const view = new DataView(arrayBuf);

      // RIFF header
      const riff = String.fromCharCode(view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3));
      assert.strictEqual(riff, 'RIFF');

      // WAVE format
      const wave = String.fromCharCode(view.getUint8(8), view.getUint8(9), view.getUint8(10), view.getUint8(11));
      assert.strictEqual(wave, 'WAVE');

      // 16-bit PCM format
      assert.strictEqual(view.getUint16(20, true), 1, 'AudioFormat must be 1 (PCM)');
      assert.strictEqual(view.getUint16(22, true), 2, 'NumChannels must be 2 (Stereo)');
      assert.strictEqual(view.getUint32(24, true), 48000, 'SampleRate must be 48000');
      assert.strictEqual(view.getUint16(34, true), 16, 'BitsPerSample must be 16');

      // Data chunk length: 44 bytes header + (1000 * 2 channels * 2 bytes = 4000)
      assert.strictEqual(arrayBuf.byteLength, 44 + 4000);
    });

    it('verifies ReverbComparisonBuffer snapshot and toggle behavior', async () => {
      const { ReverbComparisonBuffer } = await import('./js/app.js');

      const mockApp = {
        knobs: {
          predelay: { getValue: () => 24, setValue: (v) => { mockApp.knobs.predelay._val = v; }, _val: 24 },
          rt60_decay: { getValue: () => 6.5, setValue: (v) => { mockApp.knobs.rt60_decay._val = v; }, _val: 6.5 }
        },
        engine: {
          setParam: () => {}
        }
      };

      const buffer = new ReverbComparisonBuffer(mockApp);
      buffer.init();
      assert.strictEqual(buffer.activeBuffer, 'A');

      // Change parameter and toggle to B
      mockApp.knobs.rt60_decay.getValue = () => 16.0;
      const nextBuf = buffer.toggle();
      assert.strictEqual(nextBuf, 'B');
      assert.strictEqual(buffer.activeBuffer, 'B');

      // Toggle back to A
      const prevBuf = buffer.toggle();
      assert.strictEqual(prevBuf, 'A');
      assert.strictEqual(buffer.activeBuffer, 'A');
    });

    it('verifies Master Utilities buttons exist in index.html', () => {
      const htmlPath = path.join(__dirname, 'index.html');
      const html = fs.readFileSync(htmlPath, 'utf8');

      assert.ok(html.includes('id="btn-record-wav"'), 'Record WAV button must exist');
      assert.ok(html.includes('id="btn-ab-toggle"'), 'A/B toggle button must exist');
      assert.ok(html.includes('id="btn-ab-copy"'), 'A/B copy button must exist');
      assert.ok(html.includes('id="btn-save-patch"'), 'Save patch button must exist');
      assert.ok(html.includes('id="btn-export-patch"'), 'Export patch button must exist');
      assert.ok(html.includes('id="btn-load-patch"'), 'Load patch button must exist');
      assert.ok(html.includes('id="btn-reset-all"'), 'Reset button must exist');
      assert.ok(html.includes('id="drop-overlay"'), 'Drag and drop overlay must exist');
    });
  });

  //----------------------------------------------------------------------------
  describe('9. Deck 07 Onboard Exciter Engine & Performance UI', () => {
    it('verifies Deck 07 DOM structure, chime keys, and chords in index.html', () => {
      const htmlPath = path.join(__dirname, 'index.html');
      const html = fs.readFileSync(htmlPath, 'utf8');

      assert.ok(html.includes('07</span>'), 'Deck 7 must be numbered');
      assert.ok(html.includes('HARMONIC STIMULUS & EXCITER'), 'Deck 7 title must exist');
      assert.ok(html.includes('id="chime-strip"'), 'Chime strip container must exist');

      // 11 Chime Keys with hotkeys a through '
      const hotkeys = ['a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\''];
      for (const hk of hotkeys) {
        assert.ok(html.includes(`data-hotkey="${hk}"`), `Chime key data-hotkey="${hk}" must exist`);
      }

      // 12 Signature Chords
      assert.ok(html.includes('id="chord-grid"'), 'Chord grid must exist');
      for (let i = 0; i < 12; i++) {
        assert.ok(html.includes(`data-chord-index="${i}"`), `Chord button data-chord-index="${i}" must exist`);
      }

      // 4 Strum Speeds
      assert.ok(html.includes('data-speed="slow"'), 'Slow strum button must exist');
      assert.ok(html.includes('data-speed="med"'), 'Med strum button must exist');
      assert.ok(html.includes('data-speed="fast"'), 'Fast strum button must exist');
      assert.ok(html.includes('data-speed="instant"'), 'Instant strum button must exist');

      // Laboratory Pulses and Poisson Generator
      assert.ok(html.includes('id="btn-pulse-dirac"'), 'Dirac pulse button must exist');
      assert.ok(html.includes('id="btn-pulse-pink"'), 'Pink burst button must exist');
      assert.ok(html.includes('id="btn-pulse-hammer"'), 'Acoustic hammer button must exist');
      assert.ok(html.includes('id="btn-poisson-toggle"'), 'Poisson toggle button must exist');
      assert.ok(html.includes('id="input-poisson-rate"'), 'Poisson rate slider must exist');
      assert.ok(html.includes('id="input-poisson-humanize"'), 'Poisson humanize slider must exist');
    });

    it('verifies scale quantizer and chord voicings in app.js', async () => {
      const { quantizeMidiToScale, getChimeMidiForDegree, SCALES, CHORD_VOICINGS } = await import('./js/app.js');

      assert.strictEqual(Object.keys(SCALES).length, 8, 'Must provide 8 modal scales');
      assert.strictEqual(CHORD_VOICINGS.length, 12, 'Must provide 12 signature chord voicings');

      // Test quantizer on Budd Pentatonic [0, 2, 4, 7, 9] with C root (0)
      const intervals = SCALES.BUDD_PENTATONIC.intervals;
      assert.strictEqual(quantizeMidiToScale(60, 0, intervals), 60); // C4 -> C4
      assert.strictEqual(quantizeMidiToScale(61, 0, intervals), 60); // C#4 -> C4
      assert.strictEqual(quantizeMidiToScale(62, 0, intervals), 62); // D4 -> D4
      assert.strictEqual(quantizeMidiToScale(65, 0, intervals), 64); // F4 -> E4 (nearest 4)
      assert.strictEqual(quantizeMidiToScale(67, 0, intervals), 67); // G4 -> G4

      // Test negative scale degree wrapping without producing NaN
      assert.strictEqual(getChimeMidiForDegree(-1, 0, intervals), 57, 'Degree -1 in C Pentatonic must be A3 (57)');
      assert.strictEqual(getChimeMidiForDegree(-2, 0, intervals), 55, 'Degree -2 in C Pentatonic must be G3 (55)');
      assert.strictEqual(getChimeMidiForDegree(-5, 0, intervals), 48, 'Degree -5 in C Pentatonic must be C3 (48)');

      // Test empty intervals fallback
      assert.strictEqual(getChimeMidiForDegree(0, 0, []), 60, 'Empty intervals must fall back gracefully to C4');

      // Test chime degree indexing for C Pentatonic across 11 keys: C4, D4, E4, G4, A4, C5, D5, E5, G5, A5, C6
      const expectedPentatonic = [60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84];
      const actualPentatonic = [];
      for (let i = 0; i < 11; i++) {
        actualPentatonic.push(getChimeMidiForDegree(i, 0, intervals));
      }
      assert.deepStrictEqual(actualPentatonic, expectedPentatonic, 'Chime keys must strictly index scale degrees sequentially without duplicate notes');

      // Verify all 8 modal scales produce strictly ascending sequential pitches across 11 keys with zero duplicates
      for (const [scaleKey, scale] of Object.entries(SCALES)) {
        const notes = [];
        for (let i = 0; i < 11; i++) {
          notes.push(getChimeMidiForDegree(i, 0, scale.intervals));
        }
        for (let i = 1; i < notes.length; i++) {
          assert.ok(notes[i] > notes[i - 1], `Scale ${scaleKey} key ${i} (${notes[i]}) must be strictly greater than key ${i - 1} (${notes[i - 1]})`);
        }
      }

      // Verify index.html static DOM contains explicit data-midi attributes matching expectedPentatonic
      const indexPath = path.join(__dirname, 'index.html');
      const indexHtml = fs.readFileSync(indexPath, 'utf8');
      expectedPentatonic.forEach((expectedMidi, idx) => {
        assert.ok(
          indexHtml.includes(`data-key-index="${idx}"`) && indexHtml.includes(`data-midi="${expectedMidi}"`),
          `index.html must specify data-midi="${expectedMidi}" on chime key ${idx}`
        );
      });
    });

    it('verifies Web Reverb Engine Butterworth Q and trigger routing', async () => {
      const { BUTTERWORTH_Q, Rb26WebEngine } = await import('./js/audio/rb26_web_engine.js');
      assert.ok(BUTTERWORTH_Q > 0.70 && BUTTERWORTH_Q < 0.71, 'BUTTERWORTH_Q must be linear 1/sqrt(2) strictly > 0 to ensure filter stability');

      const engine = new Rb26WebEngine();
      assert.strictEqual(typeof engine.triggerDirac, 'function', 'Rb26WebEngine must implement triggerDirac');
      assert.strictEqual(typeof engine.triggerMallet, 'function', 'Rb26WebEngine must implement triggerMallet');
      assert.strictEqual(typeof engine.triggerNoiseBurst, 'function', 'Rb26WebEngine must implement triggerNoiseBurst');
      assert.strictEqual(typeof engine.triggerSynthPad, 'function', 'Rb26WebEngine must implement triggerSynthPad');
      assert.strictEqual(typeof engine.connectInput, 'function', 'Rb26WebEngine must implement connectInput');
    });
  });
});
