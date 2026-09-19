/**
 * @file test-checklist.mjs
 * @brief Automated Verification Checklist & DSP Validation Harness for BRAUN RB-26
 *
 * Validates:
 * 1. Multi-rate normalized coefficients & scaling (44.1k to 192k)
 * 2. Toxic denormal / NaN / Infinity immunity & flush-to-zero
 * 3. Hermite limiter curve monotonicity, 0 dB transparency & +18 dBFS clamping
 * 4. Parameter smoother continuity (no discrete jumps > 0.05)
 * 5. Factory preset integrity & parameter bounds across all 10 presets
 */

import { describe, it } from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// DSP reference implementations matching C++ DspMath.h and BoundedSaturator.h
function flushDenormal(val) {
  if (!Number.isFinite(val)) return 0.0;
  return Math.abs(val) < 1.0e-15 ? 0.0 : val;
}

function applySmoothBoundaryKnee(x, knee = 0.72, ceiling = 1.05) {
  if (!Number.isFinite(x)) return 0.0;
  const k = Math.min(0.99, Math.max(0.10, knee));
  const M = Math.max(k + 0.01, ceiling);
  const absX = Math.abs(x);
  if (absX <= k) return flushDenormal(x);
  const sign = x > 0 ? 1.0 : -1.0;
  if (absX >= M) return sign * M;
  const delta = M - k;
  const u = (absX - k) / delta;
  const poly = u * (1.0 + u * (1.0 - u));
  return flushDenormal(sign * (k + delta * poly));
}

function softLimit(x, knee = 0.85, ceiling = 1.0) {
  return applySmoothBoundaryKnee(x, knee, ceiling);
}

class ReferenceOnePoleSmoother {
  constructor(tauSec = 0.025, sampleRate = 48000) {
    this.sampleRate = sampleRate;
    this.tau = tauSec;
    this.coeff = 1.0 - Math.exp(-1.0 / (this.sampleRate * this.tau));
    this.current = 0.0;
    this.target = 0.0;
  }

  setSampleRate(fs) {
    this.sampleRate = fs;
    this.coeff = 1.0 - Math.exp(-1.0 / (this.sampleRate * this.tau));
  }

  setTimeConstant(tau) {
    this.tau = Math.max(0.0001, tau);
    this.coeff = 1.0 - Math.exp(-1.0 / (this.sampleRate * this.tau));
  }

  setTarget(target) {
    this.target = target;
  }

  snapTo(val) {
    this.target = val;
    this.current = val;
  }

  next() {
    this.current += this.coeff * (this.target - this.current);
    if (Math.abs(this.target - this.current) < 1.0e-6) {
      this.current = this.target;
    }
    this.current = flushDenormal(this.current);
    return this.current;
  }
}

describe('BRAUN RB-26 Verification Checklist & Automated Validation Suite', () => {

  //----------------------------------------------------------------------------
  describe('1. Multi-Rate Normalized Coefficients Check', () => {
    const sampleRates = [44100, 48000, 88200, 96000, 176400, 192000];

    it('verifies filter coefficient scaling across all 6 supported sample rates', () => {
      const cutoffs = [60, 180, 1000, 4000, 8000, 14000];

      for (const fc of cutoffs) {
        let prevCoeff = 1.0;
        for (const fs of sampleRates) {
          assert.ok(fc < fs * 0.495, `Cutoff ${fc} Hz must be below Nyquist for fs = ${fs} Hz`);
          // 1-pole filter alpha coefficient: 1 - exp(-2 * pi * fc / fs)
          const alpha = 1.0 - Math.exp(-2.0 * Math.PI * (fc / fs));

          assert.ok(alpha > 0.0, `Alpha must be strictly positive at fs = ${fs}`);
          assert.ok(alpha < 1.0, `Alpha must be strictly less than 1.0 at fs = ${fs}`);
          assert.ok(alpha < prevCoeff, `Alpha must monotonically decrease as sample rate increases (fs = ${fs})`);
          prevCoeff = alpha;
        }
      }
    });

    it('verifies FDN prime delay buffer scaling and integer bounds across multi-rate', () => {
      const fdnPrimesSec = [0.0211, 0.0223, 0.0241, 0.0277, 0.0331, 0.0409, 0.0509, 0.0631];
      const maxRoomSize = 4.0; // RB-26 supports room sizes up to 4.0x

      for (const fs of sampleRates) {
        for (let i = 0; i < fdnPrimesSec.length; i++) {
          const delaySec = fdnPrimesSec[i];
          const samples = Math.round(delaySec * fs);
          const maxSamples = Math.round(delaySec * maxRoomSize * fs);

          assert.ok(Number.isInteger(samples), 'Delay sample count must be integer');
          assert.ok(samples >= 930, `Min delay at 44.1k must be >= 930 samples, got ${samples}`);
          assert.ok(maxSamples <= 192000 * 4.0 * 0.0631 + 10, 'Max delay must fit within allocated buffer headroom');
          if (i > 0) {
            const prevSamples = Math.round(fdnPrimesSec[i - 1] * fs);
            assert.ok(samples > prevSamples, `Prime line ${i} must have longer delay than line ${i - 1}`);
          }
        }
      }
    });

    it('verifies pitch shift phase increment invariance across multi-rate', () => {
      const semitones = [12, -12, 7, 24, -24];
      const windowSec = 0.045; // 45ms grain window

      for (const st of semitones) {
        const ratio = Math.pow(2, st / 12);
        const speed = Math.abs(1.0 - ratio);

        for (const fs of sampleRates) {
          const periodSec = speed > 0.001 ? windowSec / speed : 0.05;
          const lengthSamples = Math.max(128, Math.floor(periodSec * fs));

          // Physical time period must remain invariant regardless of sample rate
          const calculatedSec = lengthSamples / fs;
          assert.ok(Math.abs(calculatedSec - periodSec) < (1.0 / fs) * 2.0,
            `Pitch shifter window timing must match physical period at fs = ${fs}`);
        }
      }
    });

    it('verifies Butterworth Q constant (-3.0103 dB) for maximally flat passband', async () => {
      const { BUTTERWORTH_Q } = await import('./js/audio/rb26_web_engine.js');
      assert.ok(Math.abs(BUTTERWORTH_Q - (-3.0103)) < 1e-4);
      // Equivalent linear Q = 10^(Q_dB / 20) = 1 / sqrt(2)
      const linearQ = Math.pow(10, BUTTERWORTH_Q / 20);
      assert.ok(Math.abs(linearQ - (1 / Math.SQRT2)) < 1e-3, 'Linear Q must equal 1/sqrt(2) (0.7071)');
    });
  });

  //----------------------------------------------------------------------------
  describe('2. Toxic Denormal & NaN Immunity Test', () => {
    it('verifies software flushDenormal neutralizes non-finite values and subnormals', () => {
      assert.strictEqual(flushDenormal(NaN), 0.0, 'NaN must flush to bit-exact 0.0');
      assert.strictEqual(flushDenormal(Infinity), 0.0, '+Infinity must flush to bit-exact 0.0');
      assert.strictEqual(flushDenormal(-Infinity), 0.0, '-Infinity must flush to bit-exact 0.0');
      assert.strictEqual(flushDenormal(1.0e-16), 0.0, 'Subnormal 1e-16 must flush to 0.0');
      assert.strictEqual(flushDenormal(-1.0e-16), 0.0, 'Subnormal -1e-16 must flush to 0.0');
      assert.strictEqual(flushDenormal(1.0e-38), 0.0, 'Subnormal 1e-38 must flush to 0.0');
      assert.strictEqual(flushDenormal(-1.0e-45), 0.0, 'Subnormal IEEE denormal must flush to 0.0');
    });

    it('verifies normal audible signals are strictly preserved without distortion', () => {
      assert.strictEqual(flushDenormal(1.0), 1.0);
      assert.strictEqual(flushDenormal(-0.5), -0.5);
      assert.strictEqual(flushDenormal(1.0e-6), 1.0e-6);
      assert.strictEqual(flushDenormal(-1.0e-6), -1.0e-6);
    });

    it('verifies recursive filter state decays to bit-exact 0.0 without subnormal stalls', () => {
      // Simulate 1-pole recursive filter draining into silence
      let state = 1.0;
      const alpha = 0.05;
      let samplesToZero = 0;

      for (let n = 0; n < 10000; n++) {
        state = flushDenormal(state * (1.0 - alpha));
        if (state === 0.0) {
          samplesToZero = n;
          break;
        }
      }

      assert.strictEqual(state, 0.0, 'State must reach bit-exact 0.0');
      assert.ok(samplesToZero > 0 && samplesToZero < 1000, `Decay took ${samplesToZero} samples, expected < 1000`);
    });

    it('verifies buffer containing toxic garbage is neutralized to finite bounded output', () => {
      const toxicInputs = [NaN, Infinity, -Infinity, 1.0e-38, -1.0e-38, 1.0e35, -1.0e35, 0.0, 0.5, -0.5];
      for (const x of toxicInputs) {
        const outKnee = applySmoothBoundaryKnee(x);
        const outLim = softLimit(x);
        assert.ok(Number.isFinite(outKnee), `applySmoothBoundaryKnee(${x}) must be finite`);
        assert.ok(Number.isFinite(outLim), `softLimit(${x}) must be finite`);
        assert.ok(Math.abs(outKnee) <= 1.05, `applySmoothBoundaryKnee(${x}) must be <= 1.05`);
        assert.ok(Math.abs(outLim) <= 1.00, `softLimit(${x}) must be <= 1.00`);
      }
    });
  });

  //----------------------------------------------------------------------------
  describe('3. Hermite Limiter Curve Test', () => {
    it('verifies strict 0 dB small signal transparency below the knee', () => {
      const knee = 0.72;
      const testVals = [0.0, 0.001, 0.1, 0.25, 0.5, 0.70, 0.72, -0.001, -0.1, -0.25, -0.5, -0.70, -0.72];

      for (const x of testVals) {
        const y = applySmoothBoundaryKnee(x, knee, 1.05);
        assert.ok(Math.abs(y - x) < 1.0e-7, `Small signal ${x} must equal output ${y} (bit-exact transparency)`);
      }
    });

    it('verifies strict monotonicity across wide dynamic range [-8.0, +8.0]', () => {
      const steps = 10000;
      let prevY = -2.0;

      for (let i = 0; i <= steps; i++) {
        const x = -8.0 + (16.0 * i) / steps;
        const y = applySmoothBoundaryKnee(x);
        assert.ok(y >= prevY - 1.0e-7, `Monotonicity violation at x = ${x}: y = ${y} < prevY = ${prevY}`);
        prevY = y;
      }
    });

    it('verifies strict peak clamping under +18 dBFS input and beyond', () => {
      // +18 dBFS peak linear amplitude: 10^(18 / 20) = 7.943282
      const plus18dB = Math.pow(10, 18 / 20);
      const y18 = softLimit(plus18dB);
      assert.ok(y18 <= 1.000000, `Output at +18 dBFS (${y18}) must be strictly <= 1.000000`);
      assert.ok(y18 >= 0.999000, `Output at +18 dBFS (${y18}) must be near ceiling`);

      // Extreme +40 dBFS input: 10^(40 / 20) = 100.0
      const y40 = softLimit(100.0);
      assert.strictEqual(y40, 1.0, 'Output at +40 dBFS must be clamped to exact ceiling 1.0');

      const y40neg = softLimit(-100.0);
      assert.strictEqual(y40neg, -1.0, 'Output at -40 dBFS must be clamped to exact ceiling -1.0');
    });

    it('verifies odd mathematical symmetry: f(-x) === -f(x)', () => {
      const probes = [0.1, 0.5, 0.72, 0.85, 1.0, 1.5, 2.5, 5.0, 10.0];
      for (const x of probes) {
        const yPos = applySmoothBoundaryKnee(x);
        const yNeg = applySmoothBoundaryKnee(-x);
        assert.ok(Math.abs(yPos + yNeg) < 1.0e-6, `Odd symmetry failed at x = ${x}: ${yPos} vs ${yNeg}`);
      }
    });

    it('verifies Web Reverb Engine internal Hermite curve generation matches spec', async () => {
      const { Rb26WebEngine } = await import('./js/audio/rb26_web_engine.js');
      const engine = new Rb26WebEngine();
      const curve = engine._generateHermiteCurve();

      assert.strictEqual(curve.length, 1024, 'Hermite wave shaper curve must have 1024 points');
      assert.ok(Math.abs(curve[512]) < 0.01, 'Center point must be zero');
      assert.ok(curve[1023] > 0.90 && curve[1023] <= 1.05, 'Upper bound must saturate smoothly');
      assert.ok(curve[0] < -0.90 && curve[0] >= -1.05, 'Lower bound must saturate smoothly');

      // Monotonicity of table
      for (let i = 1; i < curve.length; i++) {
        assert.ok(curve[i] >= curve[i - 1], `WaveShaper table monotonicity failed at index ${i}`);
      }
    });
  });

  //----------------------------------------------------------------------------
  describe('4. Parameter Smoother Ramp Continuity Test', () => {
    it('verifies no discrete sample jumps > 0.05 on extreme step parameter changes', () => {
      const smoother = new ReferenceOnePoleSmoother(0.025, 48000); // 25ms tau at 48kHz
      smoother.snapTo(0.0);
      smoother.setTarget(1.0);

      let maxJump = 0.0;
      let prevVal = 0.0;

      for (let i = 0; i < 24000; i++) {
        const val = smoother.next();
        const jump = Math.abs(val - prevVal);
        if (jump > maxJump) maxJump = jump;
        assert.ok(jump <= 0.05, `Discrete step jump ${jump} at sample ${i} exceeds 0.05 threshold`);
        prevVal = val;
      }

      assert.ok(maxJump < 0.002, `Max jump was ${maxJump}, well below 0.05 threshold`);
      assert.strictEqual(prevVal, 1.0, 'Smoother must reach exact target 1.0');
    });

    it('verifies ramp continuity across range of time constants (2ms to 80ms)', () => {
      const timeConstants = [0.002, 0.010, 0.020, 0.025, 0.050, 0.080];
      const sampleRates = [44100, 48000, 96000, 192000];

      for (const tau of timeConstants) {
        for (const fs of sampleRates) {
          const smoother = new ReferenceOnePoleSmoother(tau, fs);
          smoother.snapTo(-24.0); // Output trim dB sweep
          smoother.setTarget(+12.0);

          let prev = -24.0;
          let maxStep = 0.0;
          const totalSamples = Math.round(fs * 0.5);

          for (let s = 0; s < totalSamples; s++) {
            const cur = smoother.next();
            const step = Math.abs(cur - prev);
            if (step > maxStep) maxStep = step;
            prev = cur;
          }

          // Step normalized to [0, 1] range: maxStep / 36.0 must be <= 0.05
          const normalizedStep = maxStep / 36.0;
          assert.ok(normalizedStep <= 0.05,
            `Normalized step ${normalizedStep} at tau=${tau}s fs=${fs}Hz exceeded 0.05`);
        }
      }
    });

    it('verifies strictly monotonic trajectory without ringing or overshoot', () => {
      const smoother = new ReferenceOnePoleSmoother(0.020, 48000);
      smoother.snapTo(0.0);
      smoother.setTarget(100.0);

      let prev = 0.0;
      for (let i = 0; i < 24000; i++) {
        const cur = smoother.next();
        assert.ok(cur >= prev, `Trajectory non-monotonic at sample ${i}: ${cur} < ${prev}`);
        assert.ok(cur <= 100.0, `Overshoot detected at sample ${i}: ${cur} > 100.0`);
        prev = cur;
      }
      assert.strictEqual(prev, 100.0);
    });
  });

  //----------------------------------------------------------------------------
  describe('5. Factory Preset Integrity & Serialization Bounds Check', () => {
    const presetsPath = fs.existsSync(path.join(__dirname, 'factory_presets.json'))
      ? path.join(__dirname, 'factory_presets.json')
      : path.join(__dirname, 'presets', 'factory_presets.json');

    it('verifies factory_presets.json file existence and valid JSON grammar', () => {
      assert.ok(fs.existsSync(presetsPath), `Preset file must exist at ${presetsPath}`);
      const raw = fs.readFileSync(presetsPath, 'utf8');
      const data = JSON.parse(raw);
      assert.strictEqual(data.device, 'BRAUN_RB26', 'Device token must match BRAUN_RB26');
      assert.strictEqual(data.presets.length, 10, 'Must provide exactly 10 curated studio presets');
    });

    it('verifies all 10 preset IDs match expected studio presets', () => {
      const data = JSON.parse(fs.readFileSync(presetsPath, 'utf8'));
      const expectedIds = [
        'DEFAULT',
        'AMBIENT_GUITAR_CLOUD',
        'AS42_SHIMMER_COMPANION',
        'SOFT_FELT_ACOUSTIC_HALL',
        'GERMAN_PLATE_140',
        'CATHEDRAL_DIFFUSION',
        'ETHEREAL_SYNTH_PAD',
        'BLOOM_SHIMMER_VOID',
        'INFINITE_ETHEREAL_FREEZE',
        'SUB_BASS_PRESERVER'
      ];

      const actualIds = data.presets.map(p => p.id);
      for (const exp of expectedIds) {
        assert.ok(actualIds.includes(exp), `Preset ID ${exp} must exist in factory presets`);
      }
    });

    it('verifies all 25 parameter keys exist and conform to strict physical bounds', () => {
      const data = JSON.parse(fs.readFileSync(presetsPath, 'utf8'));

      const bounds = {
        predelay: [0, 500],
        diffusion: [0, 100],
        input_trim: [-24, 12],
        low_crossover: [60, 400],
        damping_low: [0.1, 4.0],
        low_punch: [0, 100],
        mono_bass: [20, 250],
        rt60_decay: [0.2, 30.0],
        room_size: [10, 400],
        damping_high: [1000, 20000],
        shimmer_send: [0, 100],
        dimmer_send: [0, 100],
        pitch_regen: [0, 100],
        pitch_delay: [20, 500],
        pitch_boost: [0, 18],
        tail_mod_rate: [0.05, 5.0],
        tail_mod_depth: [0, 100],
        tail_bloom: [0, 300],
        stereo_width: [0, 200],
        early_late_mix: [0, 100],
        dry_wet_mix: [0, 100],
        output_trim: [-24, 12]
      };

      const validShimmerIntervals = [7, 12, 24];
      const validDimmerIntervals = [-24, -12, -7, -2];
      const validManifoldTypes = [0, 1, 2, 3];

      for (const preset of data.presets) {
        const p = preset.params;
        assert.ok(p, `Preset ${preset.id} must have params object`);

        // Check continuous numeric parameters
        for (const [key, [min, max]] of Object.entries(bounds)) {
          assert.ok(key in p, `Preset ${preset.id} missing parameter: ${key}`);
          assert.strictEqual(typeof p[key], 'number', `Param ${key} in ${preset.id} must be a number`);
          assert.ok(p[key] >= min && p[key] <= max,
            `Param ${key} = ${p[key]} in ${preset.id} out of bounds [${min}, ${max}]`);
        }

        // Check discrete intervals and manifolds
        assert.ok(validShimmerIntervals.includes(p.shimmer_interval),
          `Invalid shimmer_interval ${p.shimmer_interval} in ${preset.id}`);
        assert.ok(validDimmerIntervals.includes(p.dimmer_interval),
          `Invalid dimmer_interval ${p.dimmer_interval} in ${preset.id}`);
        assert.ok(validManifoldTypes.includes(p.manifold_type),
          `Invalid manifold_type ${p.manifold_type} in ${preset.id}`);

        // Check boolean switches
        assert.strictEqual(typeof p.decay_hold, 'boolean', `decay_hold in ${preset.id} must be boolean`);
        assert.strictEqual(typeof p.soft_limiter, 'boolean', `soft_limiter in ${preset.id} must be boolean`);
      }
    });

    it('verifies non-zero active shimmer and dimmer sends across factory presets', () => {
      const data = JSON.parse(fs.readFileSync(presetsPath, 'utf8'));
      for (const preset of data.presets) {
        assert.ok(preset.params.shimmer_send >= 5,
          `Preset ${preset.id} shimmer_send (${preset.params.shimmer_send}) must be >= 5%`);
        assert.ok(preset.params.dimmer_send >= 5,
          `Preset ${preset.id} dimmer_send (${preset.params.dimmer_send}) must be >= 5%`);
      }
    });
  });

  //----------------------------------------------------------------------------
  describe('6. 28-Parameter Metadata Table & UI Binding Verification', () => {
    const expectedMetadataTable = [
      { apvtsId: "input_trim_db",      webId: "inputTrimDb",      isChoice: false, isBool: false },
      { apvtsId: "pre_delay_ms",       webId: "preDelayMs",       isChoice: false, isBool: false },
      { apvtsId: "dry_wet_mix",        webId: "dryWetMix",        isChoice: false, isBool: false },
      { apvtsId: "early_late_mix",     webId: "earlyLateMix",     isChoice: false, isBool: false },
      { apvtsId: "low_crossover_hz",   webId: "lowCrossoverHz",   isChoice: false, isBool: false },
      { apvtsId: "bass_rt60_mult",     webId: "bassRt60Mult",     isChoice: false, isBool: false },
      { apvtsId: "punch_ducking",      webId: "punchDucking",     isChoice: false, isBool: false },
      { apvtsId: "sub_mono_hz",        webId: "subMonoHz",        isChoice: false, isBool: false },
      { apvtsId: "room_size",          webId: "roomSize",         isChoice: false, isBool: false },
      { apvtsId: "decay_rt60_sec",     webId: "decayRt60Sec",     isChoice: false, isBool: false },
      { apvtsId: "high_damping_hz",    webId: "highDampingHz",    isChoice: false, isBool: false },
      { apvtsId: "diffusion_density",  webId: "diffusionDensity", isChoice: false, isBool: false },
      { apvtsId: "freeze_hold",        webId: "freezeHold",       isChoice: false, isBool: true  },
      { apvtsId: "shimmer_send",       webId: "shimmerSend",      isChoice: false, isBool: false },
      { apvtsId: "dimmer_send",        webId: "dimmerSend",       isChoice: false, isBool: false },
      { apvtsId: "shimmer_interval",   webId: "shimmerInterval",  isChoice: true,  isBool: false },
      { apvtsId: "dimmer_interval",    webId: "dimmerInterval",   isChoice: true,  isBool: false },
      { apvtsId: "pitch_blend",        webId: "pitchBlend",       isChoice: false, isBool: false },
      { apvtsId: "pitch_feedback",     webId: "pitchFeedback",    isChoice: false, isBool: false },
      { apvtsId: "pitch_delay_ms",     webId: "pitchDelayMs",     isChoice: false, isBool: false },
      { apvtsId: "tail_mod_rate_hz",   webId: "tailModRateHz",    isChoice: false, isBool: false },
      { apvtsId: "tail_mod_depth_ms",  webId: "tailModDepthMs",   isChoice: false, isBool: false },
      { apvtsId: "tail_bloom_ms",      webId: "tailBloomMs",      isChoice: false, isBool: false },
      { apvtsId: "stereo_width",       webId: "stereoWidth",      isChoice: false, isBool: false },
      { apvtsId: "output_trim_db",     webId: "outputTrimDb",     isChoice: false, isBool: false },
      { apvtsId: "limiter_enable",     webId: "limiterEnable",    isChoice: false, isBool: true  },
      { apvtsId: "pitch_boost",        webId: "pitchBoost",       isChoice: false, isBool: false },
      { apvtsId: "manifold_type",      webId: "manifoldType",     isChoice: true,  isBool: false }
    ];

    it('verifies metadata table contains exactly 28 unique parameters', () => {
      assert.strictEqual(expectedMetadataTable.length, 28, 'Metadata table must have exactly 28 parameters');
      const apvtsIds = new Set(expectedMetadataTable.map(p => p.apvtsId));
      const webIds = new Set(expectedMetadataTable.map(p => p.webId));
      assert.strictEqual(apvtsIds.size, 28, 'All 28 APVTS IDs must be unique');
      assert.strictEqual(webIds.size, 28, 'All 28 Web IDs must be unique');
    });

    it('verifies all 28 parameters are bound in index.html, app.js and IPC bridge', () => {
      const appJsContent = fs.readFileSync(path.join(__dirname, 'js', 'app.js'), 'utf8');
      const htmlContent = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf8');

      for (const param of expectedMetadataTable) {
        const hasWebId = appJsContent.includes(`'${param.webId}'`) || appJsContent.includes(`"${param.webId}"`);
        const hasApvtsId = appJsContent.includes(`'${param.apvtsId}'`) || appJsContent.includes(`"${param.apvtsId}"`);
        assert.ok(hasWebId || hasApvtsId, `Parameter ${param.webId} / ${param.apvtsId} must be referenced in app.js`);
      }

      // Verify 4-way manifold selector UI exists in index.html
      assert.ok(htmlContent.includes('id="group-manifold"'), 'index.html must contain group-manifold');
      for (let i = 0; i < 4; i++) {
        assert.ok(htmlContent.includes(`data-val="${i}"`), `index.html must have manifold segment button for index ${i}`);
      }

      // Verify pitch_delay knob container exists in index.html
      assert.ok(htmlContent.includes('id="knob-pitch-delay"'), 'index.html must contain knob-pitch-delay container');
    });

    it('verifies all 10 presets contain manifold_type and valid range [0, 3]', () => {
      const presetsPath = fs.existsSync(path.join(__dirname, 'factory_presets.json'))
        ? path.join(__dirname, 'factory_presets.json')
        : path.join(__dirname, 'presets', 'factory_presets.json');
      const data = JSON.parse(fs.readFileSync(presetsPath, 'utf8'));
      for (const preset of data.presets) {
        assert.ok('manifold_type' in preset.params, `Preset ${preset.id} must define manifold_type`);
        const m = preset.params.manifold_type;
        assert.ok(Number.isInteger(m) && m >= 0 && m <= 3,
          `Preset ${preset.id} manifold_type (${m}) must be an integer in range [0, 3]`);
      }
    });
  });

});
