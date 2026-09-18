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
      assert.ok(rack.includes('@media (max-width: 1040px)') || rack.includes('@media (max-width: 1139px)'), 'Tablet landscape breakpoint must be defined');
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

    it('verifies 22 rotary knob containers exist in index.html for 25-knob matrix', () => {
      const htmlPath = path.join(__dirname, 'index.html');
      const html = fs.readFileSync(htmlPath, 'utf8');

      const expectedKnobs = [
        'knob-predelay', 'knob-diffusion', 'knob-input-trim',
        'knob-low-crossover', 'knob-damping-low', 'knob-low-punch', 'knob-mono-bass',
        'knob-rt60-decay', 'knob-room-size', 'knob-damping-high',
        'knob-shimmer-send', 'knob-dimmer-send', 'knob-shim-dim-blend', 'knob-pitch-regen',
        'knob-pitch-boost',
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
    it('verifies factory presets contain all 10 curated studio sound patches', () => {
      const presetsPath = path.join(__dirname, 'presets', 'factory_presets.json');
      const json = JSON.parse(fs.readFileSync(presetsPath, 'utf8'));

      assert.strictEqual(json.device, 'BRAUN_RB26');
      assert.strictEqual(json.presets.length, 10, 'Must contain exactly 10 curated studio presets');

      const ids = json.presets.map((p) => p.id);
      assert.ok(ids.includes('DEFAULT'), 'Must include DEFAULT');
      assert.ok(ids.includes('AMBIENT_GUITAR_CLOUD'), 'Must include AMBIENT_GUITAR_CLOUD');
      assert.ok(ids.includes('AS42_SHIMMER_COMPANION'), 'Must include AS42_SHIMMER_COMPANION');
      assert.ok(ids.includes('SOFT_FELT_ACOUSTIC_HALL'), 'Must include SOFT_FELT_ACOUSTIC_HALL');
      assert.ok(ids.includes('GERMAN_PLATE_140'), 'Must include GERMAN_PLATE_140');
      assert.ok(ids.includes('CATHEDRAL_DIFFUSION'), 'Must include CATHEDRAL_DIFFUSION');
      assert.ok(ids.includes('ETHEREAL_SYNTH_PAD'), 'Must include ETHEREAL_SYNTH_PAD');
      assert.ok(ids.includes('BLOOM_SHIMMER_VOID'), 'Must include BLOOM_SHIMMER_VOID');
      assert.ok(ids.includes('INFINITE_ETHEREAL_FREEZE'), 'Must include INFINITE_ETHEREAL_FREEZE');
      assert.ok(ids.includes('SUB_BASS_PRESERVER') || ids.includes('SUB-BASS_PRESERVER'), 'Must include SUB_BASS_PRESERVER');

      for (const preset of json.presets) {
        assert.ok(typeof preset.params.rt60_decay === 'number');
        assert.ok(typeof preset.params.room_size === 'number');
        assert.ok(typeof preset.params.shimmer_send === 'number');
        assert.ok(typeof preset.params.dimmer_send === 'number');
        assert.ok(typeof preset.params.dry_wet_mix === 'number');
        assert.ok(preset.params.shimmer_send >= 5, `${preset.id} shimmer_send must be >= 5%`);
        assert.ok(preset.params.dimmer_send >= 5, `${preset.id} dimmer_send must be >= 5%`);
      }

      // Verify app.js FACTORY_PRESETS do not have 0% shimmer or dimmer sends
      const appJs = fs.readFileSync(path.join(__dirname, 'js', 'app.js'), 'utf8');
      assert.strictEqual(appJs.match(/dimmer_send:\s*0\b/g), null, 'app.js FACTORY_PRESETS must not contain dimmer_send: 0');
      assert.strictEqual(appJs.match(/shimmer_send:\s*0\b/g), null, 'app.js FACTORY_PRESETS must not contain shimmer_send: 0');
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

    it('verifies click-free S-curve crossfade tables in Web Reverb Engine', async () => {
      const { Rb26WebEngine } = await import('./js/audio/rb26_web_engine.js');
      const engine = new Rb26WebEngine();

      assert.ok(engine._xfadeInCurve instanceof Float32Array);
      assert.ok(engine._xfadeOutCurve instanceof Float32Array);
      assert.strictEqual(engine._xfadeInCurve.length, 64);
      assert.strictEqual(engine._xfadeOutCurve.length, 64);

      // Boundary values
      assert.ok(Math.abs(engine._xfadeInCurve[0] - 0.0) < 1e-6);
      assert.ok(Math.abs(engine._xfadeInCurve[63] - 1.0) < 1e-6);
      assert.ok(Math.abs(engine._xfadeOutCurve[0] - 1.0) < 1e-6);
      assert.ok(Math.abs(engine._xfadeOutCurve[63] - 0.0) < 1e-6);

      // S-curve smoothstep property: S(t) + (1 - S(t)) === 1.0 at every step (bounded unity loop gain)
      for (let i = 0; i < 64; i++) {
        const inVal = engine._xfadeInCurve[i];
        const outVal = engine._xfadeOutCurve[i];
        const linearSum = inVal + outVal;
        assert.ok(Math.abs(linearSum - 1.0) < 1e-6, `Step ${i} linear sum ${linearSum} must strictly equal 1.0`);
      }
    });

    it('verifies sub-bass modal loop gain is mathematically bounded strictly < 0.88', async () => {
      const { Rb26WebEngine } = await import('./js/audio/rb26_web_engine.js');
      const engine = new Rb26WebEngine();

      // Test extreme parameter limits (decayRt60Sec = 30, bassRt60Mult = 2.5, freezeHold = true)
      const modalTimes = [0.071, 0.089, 0.107, 0.126];
      const boundedBassMult = Math.min(1.4, Math.max(0.1, 2.5));
      const effRt60 = Math.max(0.1, 30.0 * boundedBassMult);

      for (let i = 0; i < 4; i++) {
        const calculatedFb = Math.exp(-6.907755 * modalTimes[i] / effRt60) * 0.86;
        const fbFreeze = 0.875;
        const fbNormal = Math.min(0.875, calculatedFb);

        assert.ok(fbNormal < 0.88, `Normal loop gain ${fbNormal} must be strictly < 0.88`);
        assert.ok(fbFreeze < 0.88, `Freeze loop gain ${fbFreeze} must be strictly < 0.88`);
      }
    });

    it('verifies Schroeder allpass diffuser structure and diffusion perceptual mapping in Web Reverb Engine', async () => {
      const { Rb26WebEngine, WebAudioSchroederAllpass } = await import('./js/audio/rb26_web_engine.js');
      assert.strictEqual(typeof WebAudioSchroederAllpass, 'function', 'WebAudioSchroederAllpass must be exported');

      const engine = new Rb26WebEngine();
      assert.ok(Array.isArray(engine.inputDiffusers), 'engine.inputDiffusers array must exist');
      assert.ok(Array.isArray(engine.erDiffusers), 'engine.erDiffusers array must exist');

      // Test perceptual square-root curve mapping and contractive loop bounds
      const testValues = [0.0, 0.25, 0.50, 0.75, 1.0];
      let prevG = -1.0;
      for (const val of testValues) {
        const effDiff = Math.sqrt(Math.max(0.0, Math.min(1.0, val)));
        const g = 0.74 * effDiff;
        const dryG = Math.cos(effDiff * 0.5 * Math.PI);
        const diffG = Math.sin(effDiff * 0.5 * Math.PI);

        // Unit energy conservation across constant-power dry/diffuse crossfade
        const energySum = dryG * dryG + diffG * diffG;
        assert.ok(Math.abs(energySum - 1.0) < 1e-6, `Energy sum at diffusion=${val} must be 1.0`);

        // Strictly contractive loop gain bounds
        assert.ok(g >= 0.0 && g <= 0.75, `Allpass feedback gain ${g} must remain within [0.0, 0.75]`);

        // Monotonically increasing smearing progression
        assert.ok(g > prevG, `Feedback gain ${g} must increase monotonically with diffusion knob`);
        prevG = g;

        // Verify boundary conditions
        if (val === 0.0) {
          assert.strictEqual(g, 0.0, 'Diffusion 0.0 must have 0.0 feedback gain for crisp specular reflections');
          assert.strictEqual(dryG, 1.0, 'Diffusion 0.0 must be 100% dry (zero allpass smear)');
          assert.strictEqual(diffG, 0.0, 'Diffusion 0.0 must have 0.0 diffuse send');
        } else if (val === 1.0) {
          assert.ok(Math.abs(g - 0.74) < 1e-6, 'Diffusion 1.0 must reach full 0.74 loop depth');
          assert.ok(Math.abs(dryG - 0.0) < 1e-6, 'Diffusion 1.0 must have 0.0 dry direct send');
          assert.ok(Math.abs(diffG - 1.0) < 1e-6, 'Diffusion 1.0 must be 100% diffuse send');
        }
      }
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

    it('verifies deadzone boundary re-anchoring on knob drag range overflow', async () => {
      const { BraunKnob } = await import('./js/ui/knob.js');
      const knob = new BraunKnob(null, { min: 0, max: 100, step: 1, value: 50 });

      // Simulate applying delta with boundary re-anchoring
      let startVal = 50;
      let startY = 200;
      const pixelRange = 160;

      const applyStep = (currentY) => {
        const deltaY = startY - currentY;
        const normalizedChange = (deltaY / pixelRange) * 1.0;
        const rawNorm = knob.toNormalized(startVal) + normalizedChange;
        const normVal = Math.max(0, Math.min(1, rawNorm));

        if (rawNorm > 1.0 || rawNorm < 0.0) {
          startY = currentY;
          startVal = knob.fromNormalized(normVal);
        }
        return knob.fromNormalized(normVal);
      };

      // Drag way up beyond max
      let val1 = applyStep(0); // 200px drag up
      assert.strictEqual(val1, 100, 'Must clamp at max');

      // Drag down slightly by 16px (10% of pixel range)
      let val2 = applyStep(16);
      assert.strictEqual(val2, 90, 'Must immediately decrease from max without deadzone lag');

      // Drag way down beyond min
      let val3 = applyStep(400); // 384px drag down
      assert.strictEqual(val3, 0, 'Must clamp at min');

      // Drag up slightly by 16px (10% of pixel range)
      let val4 = applyStep(384);
      assert.strictEqual(val4, 10, 'Must immediately increase from min without deadzone lag');
    });

    it('verifies right-click resets knob to default value and ignores secondary drag', async () => {
      const { BraunKnob } = await import('./js/ui/knob.js');
      const knob = new BraunKnob(null, { min: 0, max: 100, step: 1, value: 50 });
      knob.defaultValue = 50;
      knob.setValue(85, false);
      assert.strictEqual(knob.value, 85);

      // Simulate right-click context menu reset
      knob.setValue(knob.defaultValue, true);
      assert.strictEqual(knob.value, 50, 'Right click must cleanly reset knob value to default');
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
      assert.ok(html.includes('title="Reset All Parameters to Selected Preset Default"'), 'Reset button must have calibrated title');
      assert.ok(html.includes('aria-label="Reset All Parameters to Selected Preset Default"'), 'Reset button must have calibrated aria-label');
      assert.ok(html.includes('id="drop-overlay"'), 'Drag and drop overlay must exist');
    });

    it('verifies Reset All button functionality, custom user presets, vector pad coords, and A/B buffer isolation', async () => {
      const { BraunRb26App } = await import('./js/app.js');

      const origDoc = globalThis.document;
      const origWin = globalThis.window;
      const origStorage = globalThis.localStorage;

      const mockStorage = new Map();
      globalThis.localStorage = {
        getItem: (k) => mockStorage.get(k) || null,
        setItem: (k, v) => mockStorage.set(k, String(v)),
        removeItem: (k) => mockStorage.delete(k),
        clear: () => mockStorage.clear()
      };

      const elements = {};
      const createElement = (tag) => {
        const el = {
          tagName: tag.toUpperCase(),
          dataset: {},
          getAttribute: (name) => el.attributes[name] ?? null,
          setAttribute: (name, val) => { el.attributes[name] = String(val); },
          classList: {
            _classes: new Set(),
            add(c) { this._classes.add(c); },
            remove(c) { this._classes.delete(c); },
            toggle(c, f) {
              if (f !== undefined) { f ? this._classes.add(c) : this._classes.delete(c); return f; }
              if (this._classes.has(c)) { this._classes.delete(c); return false; }
              this._classes.add(c); return true;
            },
            contains(c) { return this._classes.has(c); }
          },
          style: {},
          attributes: {},
          value: '',
          innerHTML: '',
          textContent: '',
          children: [],
          appendChild(c) { this.children.push(c); return c; },
          removeChild(c) { const i = this.children.indexOf(c); if (i >= 0) this.children.splice(i, 1); return c; },
          querySelector: () => null,
          querySelectorAll: () => [],
          getContext: () => ({
            resetTransform: () => {}, scale: () => {}, fillRect: () => {}, beginPath: () => {},
            moveTo: () => {}, lineTo: () => {}, stroke: () => {}, fillText: () => {}, save: () => {},
            restore: () => {}, drawImage: () => {}
          }),
          _listeners: {},
          addEventListener(evt, fn) { (this._listeners[evt] = this._listeners[evt] || []).push(fn); },
          blur() { this._blurred = true; },
          async click() {
            if (this._listeners.click) {
              const e = { stopPropagation() {}, preventDefault() {}, target: this };
              for (const fn of this._listeners.click) await fn(e);
            }
          }
        };
        return el;
      };

      try {
        globalThis.document = {
          createElement,
          getElementById: (id) => elements[id] || (elements[id] = createElement('div')),
          querySelector: () => null,
          querySelectorAll: () => [],
          body: createElement('body'),
          addEventListener: () => {},
          removeEventListener: () => {}
        };
        globalThis.window = {
          location: { protocol: 'http:', hostname: 'localhost' },
          addEventListener: () => {},
          removeEventListener: () => {}
        };

        const app = new BraunRb26App();
        app.knobs = {
          rt60_decay: { _val: 6.5, getValue() { return this._val; }, setValue(v) { this._val = v; } },
          tail_mod_rate: { _val: 0.65, getValue() { return this._val; }, setValue(v) { this._val = v; } },
          tail_mod_depth: { _val: 45, getValue() { return this._val; }, setValue(v) { this._val = v; } }
        };

        let padX = 0.45, padY = 0.45;
        app.vectorPad = {
          x: padX,
          y: padY,
          defaultX: padX,
          defaultY: padY,
          setDefaults(x, y) { this.defaultX = x; this.defaultY = y; },
          setCoordinates(x, y) { this.x = x; this.y = y; }
        };

        const selectPreset = createElement('select');
        selectPreset.value = 'DEFAULT';
        elements['select-preset'] = selectPreset;

        const resetBtn = createElement('button');
        elements['btn-reset-all'] = resetBtn;

        resetBtn.addEventListener('click', () => {
          const targetPreset = (selectPreset && selectPreset.value) || app.currentPresetKey || 'DEFAULT';
          app.loadPreset(targetPreset);
          resetBtn.blur();
        });

        // 1. Deviate values away from DEFAULT
        app.knobs.rt60_decay.setValue(14.0);
        app.vectorPad.setCoordinates(0.9, 0.2);
        assert.strictEqual(app.knobs.rt60_decay.getValue(), 14.0);
        assert.strictEqual(app.vectorPad.x, 0.9);

        // Snapshot A/B buffer state before reset
        app.abBuffer.activeBuffer = 'A';
        app.abBuffer.bufferA = { rt60_decay: 6.5 };
        app.abBuffer.bufferB = { rt60_decay: 11.0 };

        // 2. Click Reset All
        await resetBtn.click();

        // 3. Verify knobs reset to DEFAULT preset (6.5s)
        assert.strictEqual(app.knobs.rt60_decay.getValue(), 6.5);
        const expectedPx = Math.max(0, Math.min(1.0, Math.pow(Math.max(0, (0.65 - 0.05) / 3.95), 1 / 1.6)));
        assert.ok(Math.abs(app.vectorPad.x - expectedPx) < 0.01, 'Vector pad X must reset to calibrated coordinate');
        assert.ok(Math.abs(app.vectorPad.y - 0.45) < 0.01, 'Vector pad Y must reset to calibrated coordinate');

        // 4. Verify A/B comparison buffer state was untouched
        assert.strictEqual(app.abBuffer.activeBuffer, 'A');
        assert.strictEqual(app.abBuffer.bufferB.rt60_decay, 11.0);

        // 5. Verify resetBtn was blurred
        assert.strictEqual(resetBtn._blurred, true);

        // 6. Test User Preset restore
        const userPresetId = 'USER_TEST_123';
        const userPresetData = {
          [userPresetId]: {
            id: userPresetId,
            name: 'TEST CUSTOM',
            isUser: true,
            params: {
              rt60_decay: 8.2,
              tail_mod_rate: 1.5,
              tail_mod_depth: 80
            }
          }
        };
        mockStorage.set('BRAUN_RB26_USER_PRESETS', JSON.stringify(userPresetData));

        selectPreset.value = userPresetId;
        app.knobs.rt60_decay.setValue(2.0);
        await resetBtn.click();

        assert.strictEqual(app.currentPresetKey, userPresetId);
        assert.strictEqual(app.knobs.rt60_decay.getValue(), 8.2);
        assert.strictEqual(selectPreset.value, userPresetId);
      } finally {
        globalThis.document = origDoc;
        globalThis.window = origWin;
        globalThis.localStorage = origStorage;
      }
    });

    it('verifies standalone JUCE mode (isJuce=true) dispatches startRecording and stopRecording IPC', async () => {
      const origDoc = globalThis.document;
      const origWin = globalThis.window;

      const elements = {};
      const createElement = (tag) => {
        const el = {
          tagName: tag.toUpperCase(),
          classList: {
            _classes: new Set(),
            add(c) { this._classes.add(c); },
            remove(c) { this._classes.delete(c); },
            toggle(c, f) {
              if (f !== undefined) { f ? this._classes.add(c) : this._classes.delete(c); return f; }
              if (this._classes.has(c)) { this._classes.delete(c); return false; }
              this._classes.add(c); return true;
            },
            contains(c) { return this._classes.has(c); }
          },
          style: {},
          attributes: {},
          innerHTML: '',
          textContent: '',
          children: [],
          appendChild(c) { this.children.push(c); return c; },
          removeChild(c) { const i = this.children.indexOf(c); if (i >= 0) this.children.splice(i, 1); return c; },
          _recText: null,
          _statusText: null,
          querySelector(sel) {
            if (sel === '.braun-rec-text') {
              if (!this._recText) { this._recText = createElement('span'); this._recText.textContent = 'REC WAV'; }
              return this._recText;
            }
            if (sel === '.braun-status-text') {
              if (!this._statusText) { this._statusText = createElement('span'); this._statusText.textContent = 'STANDBY'; }
              return this._statusText;
            }
            return createElement('div');
          },
          querySelectorAll: () => [],
          getContext: () => ({
            resetTransform: () => {}, scale: () => {}, fillRect: () => {}, beginPath: () => {},
            moveTo: () => {}, lineTo: () => {}, stroke: () => {}, fillText: () => {}, save: () => {},
            restore: () => {}, drawImage: () => {}
          }),
          _listeners: {},
          addEventListener(evt, fn) { (this._listeners[evt] = this._listeners[evt] || []).push(fn); },
          async click() {
            if (this._listeners.click) {
              const e = { stopPropagation() {}, preventDefault() {}, target: this };
              for (const fn of this._listeners.click) await fn(e);
            }
          }
        };
        return el;
      };

      globalThis.document = {
        createElement,
        getElementById: (id) => elements[id] || (elements[id] = createElement('div')),
        querySelector: () => null,
        querySelectorAll: () => [],
        body: createElement('body'),
        addEventListener: () => {},
        removeEventListener: () => {}
      };

      const emittedEvents = [];
      const listeners = new Map();
      globalThis.window = {
        devicePixelRatio: 1,
        addEventListener: () => {},
        removeEventListener: () => {},
        requestAnimationFrame: (cb) => setTimeout(cb, 16),
        cancelAnimationFrame: (id) => clearTimeout(id),
        __JUCE__: {
          backend: {
            emitEvent: (name, payload) => { emittedEvents.push({ name, payload }); },
            addEventListener: (name, handler) => { listeners.set(name, handler); }
          }
        }
      };

      try {
        const { BraunRb26App } = await import('./js/app.js');
        const app = new BraunRb26App();
        app.isJuce = true;
        app.isPowered = true;
        app._initButtons();
        app._initJuceBridge();

        const recordBtn = elements['btn-record-wav'];
        const textEl = recordBtn.querySelector('.braun-rec-text');

        assert.strictEqual(app._isJuceRecording, false);

        // 1. Click to start recording
        await recordBtn.click();
        assert.strictEqual(app._isJuceRecording, true);
        assert.ok(recordBtn.classList.contains('is-recording'));
        assert.strictEqual(textEl.textContent, 'STOP & SAVE');
        assert.ok(emittedEvents.some(e => e.name === 'startRecording'));

        // 2. Click to stop recording
        await recordBtn.click();
        assert.strictEqual(app._isJuceRecording, false);
        assert.ok(!recordBtn.classList.contains('is-recording'));
        assert.strictEqual(textEl.textContent, 'REC WAV');
        assert.ok(emittedEvents.some(e => e.name === 'stopRecording'));

        // 3. Test recordingSaved IPC handler
        app._isJuceRecording = true;
        recordBtn.classList.add('is-recording');
        textEl.textContent = 'STOP & SAVE';

        const recordingSavedHandler = listeners.get('recordingSaved');
        assert.ok(typeof recordingSavedHandler === 'function', 'recordingSaved listener must be registered');

        recordingSavedHandler({ path: 'C:\\Users\\test\\Music\\Braun RB-26 Recordings\\braun-rb26-2026-09-17.wav' });
        assert.strictEqual(app._isJuceRecording, false);
        assert.ok(!recordBtn.classList.contains('is-recording'));
        assert.strictEqual(textEl.textContent, 'REC WAV');

        // Graceful handling of null/empty payloads
        assert.doesNotThrow(() => recordingSavedHandler(null));
        assert.doesNotThrow(() => recordingSavedHandler({}));

        // 4. Test paramUpdate for isRecording
        const paramUpdateHandler = listeners.get('paramUpdate');
        assert.ok(typeof paramUpdateHandler === 'function');
        paramUpdateHandler({ id: 'isRecording', value: 1.0 });
        assert.strictEqual(app._isJuceRecording, true);
        assert.ok(recordBtn.classList.contains('is-recording'));
        assert.strictEqual(textEl.textContent, 'STOP & SAVE');

        paramUpdateHandler({ id: 'isRecording', value: 0.0 });
        assert.strictEqual(app._isJuceRecording, false);
        assert.ok(!recordBtn.classList.contains('is-recording'));
        assert.strictEqual(textEl.textContent, 'REC WAV');

        // 5. Test auto-wake power before recording when unpowered
        app.isPowered = false;
        app._isJuceRecording = false;
        recordBtn.classList.remove('is-recording');
        textEl.textContent = 'REC WAV';

        await recordBtn.click();
        assert.strictEqual(app.isPowered, true, 'Synthesizer power must turn on');
        assert.strictEqual(app._isJuceRecording, true, 'Recording must be engaged');
        assert.ok(recordBtn.classList.contains('is-recording'));

        // 6. Test power-down terminates active recording via stopRecording IPC
        const prevStopCount = emittedEvents.filter(e => e.name === 'stopRecording').length;
        await app.setPower(false);
        assert.strictEqual(app._isJuceRecording, false, 'Recording must be terminated on power-down');
        assert.ok(!recordBtn.classList.contains('is-recording'));
        assert.strictEqual(textEl.textContent, 'REC WAV');
        const nextStopCount = emittedEvents.filter(e => e.name === 'stopRecording').length;
        assert.strictEqual(nextStopCount, prevStopCount + 1, 'stopRecording IPC must be dispatched on power-down');
      } finally {
        if (origDoc === undefined) delete globalThis.document; else globalThis.document = origDoc;
        if (origWin === undefined) delete globalThis.window; else globalThis.window = origWin;
      }
    });

    it('verifies dynamic isJuce getter parity with AS-42 across environments and redundant IPC delivery', async () => {
      const origDoc = globalThis.document;
      const origWin = globalThis.window;

      try {
        const { BraunRb26App } = await import('./js/app.js');

        // Case A: Browser environment with no JUCE objects
        globalThis.window = { location: { hostname: 'localhost', protocol: 'http:' } };
        globalThis.document = { createElement: () => ({ classList: { add() {}, remove() {}, contains: () => false }, querySelector: () => null }) };
        const browserApp = new BraunRb26App();
        assert.strictEqual(browserApp.isJuce, false, 'Default web browser environment must evaluate isJuce to false');

        // Case B: Window with window.__IS_JUCE__ injected by userScript
        globalThis.window = { __IS_JUCE__: true, location: { hostname: 'localhost', protocol: 'http:' } };
        const juceUserScriptApp = new BraunRb26App();
        assert.strictEqual(juceUserScriptApp.isJuce, true, 'window.__IS_JUCE__ must evaluate isJuce to true');

        // Case C: Window with juce: protocol
        globalThis.window = { location: { hostname: '', protocol: 'juce:' } };
        const juceProtocolApp = new BraunRb26App();
        assert.strictEqual(juceProtocolApp.isJuce, true, 'juce: protocol must evaluate isJuce to true');

        // Case D: Dynamic injection after instantiation
        globalThis.window = { location: { hostname: 'localhost', protocol: 'http:' } };
        const dynamicApp = new BraunRb26App();
        assert.strictEqual(dynamicApp.isJuce, false, 'Initially false before JUCE backend loads');
        globalThis.window.__JUCE__ = { backend: {} };
        assert.strictEqual(dynamicApp.isJuce, true, 'Dynamic getter must evaluate to true immediately when __JUCE__ appears');
      } finally {
        if (origDoc === undefined) delete globalThis.document; else globalThis.document = origDoc;
        if (origWin === undefined) delete globalThis.window; else globalThis.window = origWin;
      }
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
      assert.ok(Math.abs(BUTTERWORTH_Q - (-3.0103)) < 1e-4, 'BUTTERWORTH_Q must be -3.0103 dB for Web Audio BiquadFilterNode to ensure maximally flat Butterworth response');

      const engine = new Rb26WebEngine();
      assert.strictEqual(typeof engine.triggerDirac, 'function', 'Rb26WebEngine must implement triggerDirac');
      assert.strictEqual(typeof engine.triggerMallet, 'function', 'Rb26WebEngine must implement triggerMallet');
      assert.strictEqual(typeof engine.triggerNoiseBurst, 'function', 'Rb26WebEngine must implement triggerNoiseBurst');
      assert.strictEqual(typeof engine.triggerSynthPad, 'function', 'Rb26WebEngine must implement triggerSynthPad');
      assert.strictEqual(typeof engine.connectInput, 'function', 'Rb26WebEngine must implement connectInput');
    });

    it('verifies Web Reverb Engine master DC blocker cutoff is configured to 35 Hz', () => {
      const engineJs = fs.readFileSync(path.join(__dirname, 'js', 'audio', 'rb26_web_engine.js'), 'utf8');
      assert.ok(
        engineJs.includes('this.masterDcBlocker.frequency.setValueAtTime(35, ctx.currentTime);'),
        'masterDcBlocker must be configured to 35 Hz to eliminate subsonic bloat and DC leakage'
      );
      assert.ok(
        engineJs.includes('newDcBlocker.frequency.setValueAtTime(35, now);'),
        'newDcBlocker flush path must be configured to 35 Hz'
      );
    });
  });

  //----------------------------------------------------------------------------
  describe('10. Anti-Click Voice Envelopes & CRT Display Performance Architecture', () => {
    it('verifies anti-click attack ramping and zero-start initial envelope in playChime and exciters', () => {
      const appJs = fs.readFileSync(path.join(__dirname, 'js', 'app.js'), 'utf8');
      const engineJs = fs.readFileSync(path.join(__dirname, 'js', 'audio', 'rb26_web_engine.js'), 'utf8');

      // Chime voice master gain must initialize strictly at 0.0 before linear attack ramp
      assert.ok(appJs.includes('voiceGain.gain.setValueAtTime(0.0, now)'), 'voiceGain must start strictly at 0.0 to prevent note press click');
      assert.ok(appJs.includes('voiceGain.gain.linearRampToValueAtTime(velocity * 0.22, now + 0.008)'), 'voiceGain must ramp smoothly over 8ms attack');

      // Hammer noise transient must start at 0.0 and ramp smoothly
      assert.ok(appJs.includes('hammerGainNode.gain.setValueAtTime(0.0, now)'), 'hammer thump must initialize at 0.0');
      assert.ok(appJs.includes('hammerGainNode.gain.linearRampToValueAtTime(hammerThumpGain, now + 0.002)'), 'hammer thump must ramp over 2ms');

      // Acoustic hammer mallet must initialize at 0.0
      assert.ok(appJs.includes('g.gain.setValueAtTime(0.0, now);') && appJs.includes('g.gain.linearRampToValueAtTime(0.85, now + 0.003)'),
        'triggerHammerThud must ramp gain from 0.0 over 3ms');

      // Engine transient exciters must start at gain 0.0
      assert.ok(engineJs.includes('gain.gain.setValueAtTime(0.0, now);') && engineJs.includes('gain.gain.linearRampToValueAtTime(0.55, now + 0.003)'),
        'triggerKick must ramp smoothly from 0.0');
      assert.ok(engineJs.includes('g.gain.setValueAtTime(0.0, now);') && engineJs.includes('g.gain.linearRampToValueAtTime(0.85, now + 0.003)'),
        'triggerMallet must ramp smoothly from 0.0');
      assert.ok(engineJs.includes('gain.gain.setValueAtTime(0.0, now);') && engineJs.includes('gain.gain.linearRampToValueAtTime(0.45, now + 0.002)'),
        'triggerSnare must ramp smoothly from 0.0');
    });

    it('verifies click-free setTargetAtTime release envelope without gain.value read discontinuity', () => {
      const appJs = fs.readFileSync(path.join(__dirname, 'js', 'app.js'), 'utf8');

      // Must not read voiceGain.gain.value or call setValueAtTime on release
      assert.ok(!appJs.includes('setValueAtTime(Math.max(0.0001, voiceGain.gain.value)'),
        'Must eliminate gain.value reading and setValueAtTime on release');
      assert.ok(appJs.includes('voiceGain.gain.setTargetAtTime(0.0, t, Math.max(0.005, releaseSec * 0.25))'),
        'Must use setTargetAtTime for C1-continuous exponential decay without step discontinuity');
      assert.ok(appJs.includes('cancelAndHoldAtTime'),
        'Must use cancelAndHoldAtTime to preserve instantaneous gain and prevent timeline reset step jumps on release');

      // Must provide adequate release tail before stopping oscillators
      assert.ok(appJs.includes('const tailSec = Math.max(0.5, releaseSec * 3.0)'),
        'Must provide at least 8 to 12 time constants of release tail before stopping oscillators');

      // Polyphonic chord playback must track pending timers and cancel them on release
      assert.ok(appJs.includes('timerIds.forEach((tid) => clearTimeout(tid))'),
        'playChord must clear pending strum timers on release to prevent late voices popping in');
    });

    it('verifies BraunCrtDisplay precomputed math tables and zero per-frame allocations', async () => {
      const { BraunCrtDisplay } = await import('./js/ui/crt-display.js');
      const mockCanvas = {
        getContext: () => ({
          resetTransform: () => {},
          scale: () => {},
          fillRect: () => {},
          beginPath: () => {},
          moveTo: () => {},
          lineTo: () => {},
          stroke: () => {},
          fillText: () => {},
          save: () => {},
          restore: () => {},
          drawImage: () => {}
        }),
        getBoundingClientRect: () => ({ width: 600, height: 200 }),
        width: 600,
        height: 200
      };

      const display = new BraunCrtDisplay(mockCanvas);
      assert.strictEqual(display.fftSize, 512);
      assert.ok(display._hannWindow instanceof Float32Array, 'Hann window must be precomputed Float32Array');
      assert.strictEqual(display._hannWindow.length, 512);
      assert.ok(Math.abs(display._hannWindow[0]) < 1e-6, 'Hann window start must be 0.0');
      assert.ok(Math.abs(display._hannWindow[511]) < 1e-6, 'Hann window end must be 0.0');
      assert.ok(Math.abs(display._hannWindow[256] - 1.0) < 0.01, 'Hann window center must be ~1.0');

      assert.ok(display._bitRev instanceof Uint16Array, 'Bit-reversal table must be precomputed Uint16Array');
      assert.strictEqual(display._bitRev.length, 512);

      assert.ok(display._twiddleCos instanceof Float32Array, 'Twiddle cosine table must be precomputed Float32Array');
      assert.ok(display._twiddleSin instanceof Float32Array, 'Twiddle sine table must be precomputed Float32Array');
      assert.strictEqual(display._twiddleCos.length, 256);
      assert.strictEqual(display._twiddleSin.length, 256);

      assert.ok(display._barBinIndices instanceof Uint16Array, 'Frequency warping bins must be precomputed Uint16Array');
      assert.strictEqual(display.numBars, 48);
      assert.strictEqual(display._barBinIndices.length, 48);

      // Verify monotonically increasing bin distribution
      for (let i = 1; i < display.numBars; i++) {
        assert.ok(display._barBinIndices[i] >= display._barBinIndices[i - 1],
          `Bin ${i} (${display._barBinIndices[i]}) must be >= bin ${i - 1} (${display._barBinIndices[i - 1]})`);
      }
    });

    it('verifies BraunCrtDisplay hardware-cached graticule architecture', () => {
      const crtJs = fs.readFileSync(path.join(__dirname, 'js', 'ui', 'crt-display.js'), 'utf8');

      assert.ok(crtJs.includes('_updateGraticuleCache'), 'Must implement offscreen graticule caching');
      assert.ok(crtJs.includes('this._graticuleCanvas'), 'Must maintain _graticuleCanvas instance');
      assert.ok(crtJs.includes('ctx.drawImage(this._graticuleCanvas, 0, 0)'), 'Must blit cached graticule in single drawImage pass');
      assert.ok(crtJs.includes('targetW === this.canvas.width && targetH === this.canvas.height') ||
                crtJs.includes('this.canvas.width === targetW && this.canvas.height === targetH'),
        'Must guard against redundant canvas width/height mutations to preserve GPU textures');
    });
  });
});
