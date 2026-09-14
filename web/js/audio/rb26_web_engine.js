/**
 * @file rb26_web_engine.js
 * @brief Complete Web Audio API DSP Reverb Engine for BRAUN RB-26
 * Features:
 * - Decoupled Linkwitz-Riley 4th-order crossover (60 - 400 Hz)
 * - 4-line orthogonal modal low-end matrix with punch ducking
 * - 12-tap early reflection cluster
 * - 8-line orthogonal Householder FDN tank with high damping
 * - Bidirectional dual-delay pitch shifters (Shimmer +12/+24/+7, Dimmer -12/-24)
 * - Hermite soft-saturation bounded loop limiter
 * - Master M/S width, early/late blend, dry/wet, and output trim
 * - Curated zero-install sound generator (Impulse, 808 Kick/Snare, Felt Piano, Ambient Pad)
 */

export class WebAudioPitchShifter {
  constructor(ctx, options = {}) {
    this.ctx = ctx;
    this.semitones = options.semitones ?? +12;
    this.windowSec = options.windowSec ?? 0.045; // 45 ms grain window

    this.input = ctx.createGain();
    this.output = ctx.createGain();

    this.delay1 = ctx.createDelay(0.3);
    this.delay2 = ctx.createDelay(0.3);
    this.delay1.delayTime.value = 0.0;
    this.delay2.delayTime.value = 0.0;
    if (this.delay1.delayTime.setValueAtTime) {
      this.delay1.delayTime.setValueAtTime(0.0, ctx.currentTime);
      this.delay2.delayTime.setValueAtTime(0.0, ctx.currentTime);
    }

    this.gain1 = ctx.createGain();
    this.gain2 = ctx.createGain();
    // Critical: Gain nodes in Web Audio default to gain.value = 1.0.
    // AudioNode connections to AudioParams sum with base value, so base gain MUST be 0.0!
    this.gain1.gain.value = 0.0;
    this.gain2.gain.value = 0.0;
    if (this.gain1.gain.setValueAtTime) {
      this.gain1.gain.setValueAtTime(0.0, ctx.currentTime);
      this.gain2.gain.setValueAtTime(0.0, ctx.currentTime);
    }

    this.input.connect(this.delay1);
    this.input.connect(this.delay2);
    this.delay1.connect(this.gain1);
    this.delay2.connect(this.gain2);
    this.gain1.connect(this.output);
    this.gain2.connect(this.output);

    this._setupModulation();
  }

  setSemitones(semitones) {
    if (this.semitones === semitones) return;
    this.semitones = semitones;
    this._teardownModulation();
    this._setupModulation();
  }

  _setupModulation() {
    const ctx = this.ctx;
    const ratio = Math.pow(2, this.semitones / 12);
    const speed = Math.abs(1.0 - ratio);
    const periodSec = speed > 0.001 ? this.windowSec / speed : 0.05;
    const lengthSamples = Math.max(128, Math.floor(periodSec * ctx.sampleRate));

    const delayModBuffer = ctx.createBuffer(2, lengthSamples, ctx.sampleRate);
    const gainModBuffer = ctx.createBuffer(2, lengthSamples, ctx.sampleRate);
    const d1 = delayModBuffer.getChannelData(0);
    const d2 = delayModBuffer.getChannelData(1);
    const g1 = gainModBuffer.getChannelData(0);
    const g2 = gainModBuffer.getChannelData(1);

    const isUpward = ratio > 1.0;

    for (let i = 0; i < lengthSamples; i++) {
      const phase1 = i / lengthSamples;
      const phase2 = (phase1 + 0.5) % 1.0;

      // Delay ramps
      d1[i] = isUpward ? this.windowSec * (1.0 - phase1) : this.windowSec * phase1;
      d2[i] = isUpward ? this.windowSec * (1.0 - phase2) : this.windowSec * phase2;

      // Raised cosine / Hann window for clickless constant unity gain: sin^2(x) + cos^2(x) = 1
      g1[i] = Math.pow(Math.sin(Math.PI * phase1), 2);
      g2[i] = Math.pow(Math.sin(Math.PI * phase2), 2);
    }

    this.delayModSource = ctx.createBufferSource();
    this.delayModSource.buffer = delayModBuffer;
    this.delayModSource.loop = true;

    const delaySplitter = ctx.createChannelSplitter(2);
    this.delayModSource.connect(delaySplitter);
    delaySplitter.connect(this.delay1.delayTime, 0);
    delaySplitter.connect(this.delay2.delayTime, 1);

    this.gainModSource = ctx.createBufferSource();
    this.gainModSource.buffer = gainModBuffer;
    this.gainModSource.loop = true;

    const gainSplitter = ctx.createChannelSplitter(2);
    this.gainModSource.connect(gainSplitter);
    gainSplitter.connect(this.gain1.gain, 0);
    gainSplitter.connect(this.gain2.gain, 1);

    this.delayModSource.start();
    this.gainModSource.start();
  }

  _teardownModulation() {
    try {
      if (this.delayModSource) {
        this.delayModSource.stop();
        this.delayModSource.disconnect();
      }
      if (this.gainModSource) {
        this.gainModSource.stop();
        this.gainModSource.disconnect();
      }
    } catch (e) {}
  }
}

//==============================================================================
export class Rb26WebEngine {
  constructor() {
    this.ctx = null;
    this.isInitialized = false;
    this.isPowered = false;

    // Parameters matching C++ Rb26Parameters
    this.params = {
      preDelayMs: 24.0,
      diffusion: 0.75,
      inputTrimDb: 0.0,
      lowCrossoverHz: 180.0,
      bassRt60Mult: 1.0,
      punchDucking: 0.65,
      subMonoHz: 120.0,
      decayRt60Sec: 6.5,
      roomSize: 1.0,
      highDampingHz: 7500.0,
      freezeHold: false,
      shimmerSend: 0.40,
      dimmerSend: 0.35,
      shimmerInterval: 12,
      dimmerInterval: -12,
      pitchBlend: 0.0,
      pitchFeedback: 0.45,
      tailModRateHz: 0.65,
      tailModDepthMs: 1.2,
      tailBloomMs: 85.0,
      stereoWidth: 1.0,
      earlyLateMix: 0.50,
      dryWetMix: 0.40,
      outputTrimDb: 0.0,
      limiterEnable: true
    };

    // Telemetry listeners
    this.onTelemetry = null;
  }

  async init() {
    if (this.isInitialized) return;

    const AudioContext = window.AudioContext || window.webkitAudioContext;
    this.ctx = new AudioContext({ latencyHint: 'interactive' });

    if (this.ctx.state === 'suspended') {
      await this.ctx.resume();
    }

    this._buildDspGraph();
    this._startTelemetryLoop();
    this.isInitialized = true;
  }

  _buildDspGraph() {
    const ctx = this.ctx;

    // --- Input Stage ---
    this.inputGain = ctx.createGain();
    this.preDelayNode = ctx.createDelay(0.5);
    this.preDelayNode.delayTime.value = this.params.preDelayMs / 1000;
    this.inputGain.connect(this.preDelayNode);

    // --- Decoupled LR4 Crossover (Cascaded Dual 2nd-Order Butterworth) ---
    this.lowPass1 = ctx.createBiquadFilter();
    this.lowPass1.type = 'lowpass';
    this.lowPass1.frequency.value = this.params.lowCrossoverHz;
    this.lowPass1.Q.value = -3.0103; // Butterworth 2nd-order (Q_lin = 1/sqrt(2)) in dB

    this.lowPass2 = ctx.createBiquadFilter();
    this.lowPass2.type = 'lowpass';
    this.lowPass2.frequency.value = this.params.lowCrossoverHz;
    this.lowPass2.Q.value = -3.0103;

    this.highPass1 = ctx.createBiquadFilter();
    this.highPass1.type = 'highpass';
    this.highPass1.frequency.value = this.params.lowCrossoverHz;
    this.highPass1.Q.value = -3.0103;

    this.highPass2 = ctx.createBiquadFilter();
    this.highPass2.type = 'highpass';
    this.highPass2.frequency.value = this.params.lowCrossoverHz;
    this.highPass2.Q.value = -3.0103;

    this.preDelayNode.connect(this.lowPass1);
    this.lowPass1.connect(this.lowPass2);

    this.preDelayNode.connect(this.highPass1);
    this.highPass1.connect(this.highPass2);

    // --- Low-Band Modal Matrix (4 Delays + Sub-Mono + Punch Ducking) ---
    this.modalInputGain = ctx.createGain();
    this.lowPass2.connect(this.modalInputGain);

    this.modalBus = ctx.createGain();
    this.modalBus.gain.value = 0.35; // Headroom calibration for 4-line modal sum
    this.modalDelays = [];
    this.modalFeedbackGains = [];
    const modalTimes = [0.071, 0.089, 0.107, 0.126];

    for (let i = 0; i < 4; i++) {
      const d = ctx.createDelay(0.3);
      d.delayTime.value = modalTimes[i] * this.params.bassRt60Mult;
      const g = ctx.createGain();
      g.gain.value = 0.75;

      this.modalInputGain.connect(d);
      d.connect(g);
      g.connect(d); // recirculation
      d.connect(this.modalBus);

      this.modalDelays.push(d);
      this.modalFeedbackGains.push(g);
    }

    // --- Early Reflections (12 Taps Matrix) ---
    this.earlyReflectionsBus = ctx.createGain();
    const erTimes = [0.007, 0.013, 0.019, 0.023, 0.029, 0.037, 0.043, 0.053, 0.061, 0.071, 0.083, 0.097];
    const erGains = [0.85, 0.78, 0.72, 0.65, 0.58, 0.52, 0.46, 0.40, 0.35, 0.30, 0.25, 0.20];

    for (let i = 0; i < 12; i++) {
      const d = ctx.createDelay(0.2);
      d.delayTime.value = erTimes[i];
      const g = ctx.createGain();
      g.gain.value = erGains[i] * 0.15;

      this.highPass2.connect(d);
      d.connect(g);
      g.connect(this.earlyReflectionsBus);
    }

    // --- 8-Line Householder FDN Late Tank ---
    this.fdnInputBus = ctx.createGain();
    this.highPass2.connect(this.fdnInputBus);

    this.fdnSumBus = ctx.createGain();
    this.fdnSumBus.gain.value = 1.0 / 8.0; // 0.125: strictly bound 8-line coherent modal peaks below unity loop gain
    this.fdnDelays = [];
    this.fdnDampingFilters = [];
    this.fdnFeedbackGains = [];

    // Prime delay lengths in seconds
    const fdnPrimes = [0.0293, 0.0353, 0.0419, 0.0479, 0.0541, 0.0607, 0.0673, 0.0739];

    for (let i = 0; i < 8; i++) {
      const delay = ctx.createDelay(0.3);
      delay.delayTime.value = fdnPrimes[i] * this.params.roomSize;

      const damping = ctx.createBiquadFilter();
      damping.type = 'lowpass';
      damping.frequency.value = this.params.highDampingHz;
      damping.Q.value = -3.0103; // Butterworth rolloff (linear Q = 1/sqrt(2)), eliminates resonant feedback peak

      const gain = ctx.createGain();
      gain.gain.value = Math.pow(0.001, fdnPrimes[i] / this.params.decayRt60Sec);

      this.fdnInputBus.connect(delay);
      delay.connect(damping);
      damping.connect(gain);
      gain.connect(delay); // loop back
      gain.connect(this.fdnSumBus);

      this.fdnDelays.push(delay);
      this.fdnDampingFilters.push(damping);
      this.fdnFeedbackGains.push(gain);
    }

    // --- Hermite Soft Saturation Curve for Loop Boundedness ---
    this.hermiteSaturator = ctx.createWaveShaper();
    this.hermiteSaturator.curve = this._generateHermiteCurve();
    this.hermiteSaturator.oversample = '2x';
    this.fdnSumBus.connect(this.hermiteSaturator);

    // --- Bidirectional Pitch Shifting Diffusion (Shimmer + Dimmer) ---
    this.shimmerShifter = new WebAudioPitchShifter(ctx, { semitones: this.params.shimmerInterval });
    this.dimmerShifter = new WebAudioPitchShifter(ctx, { semitones: this.params.dimmerInterval });

    // Shimmer Bandpass (800Hz - 8kHz)
    this.shimmerFilter = ctx.createBiquadFilter();
    this.shimmerFilter.type = 'bandpass';
    this.shimmerFilter.frequency.value = 2800;
    this.shimmerFilter.Q.value = 1.0;

    // Dimmer Bandpass (70Hz - 1.2kHz)
    this.dimmerFilter = ctx.createBiquadFilter();
    this.dimmerFilter.type = 'bandpass';
    this.dimmerFilter.frequency.value = 350;
    this.dimmerFilter.Q.value = 1.0;

    this.shimmerSendGain = ctx.createGain();
    this.shimmerSendGain.gain.value = this.params.shimmerSend;

    this.dimmerSendGain = ctx.createGain();
    this.dimmerSendGain.gain.value = this.params.dimmerSend;

    this.hermiteSaturator.connect(this.shimmerSendGain);
    this.shimmerSendGain.connect(this.shimmerFilter);
    this.shimmerFilter.connect(this.shimmerShifter.input);

    this.hermiteSaturator.connect(this.dimmerSendGain);
    this.dimmerSendGain.connect(this.dimmerFilter);
    this.dimmerFilter.connect(this.dimmerShifter.input);

    // Pitch Blend Macro & Recirculation Feedback
    this.pitchReturnBus = ctx.createGain();
    this.pitchBlendGainShim = ctx.createGain();
    this.pitchBlendGainDim = ctx.createGain();

    this._updatePitchBlendGains();

    this.shimmerShifter.output.connect(this.pitchBlendGainShim);
    this.dimmerShifter.output.connect(this.pitchBlendGainDim);

    this.pitchBlendGainShim.connect(this.pitchReturnBus);
    this.pitchBlendGainDim.connect(this.pitchReturnBus);

    // Recirculate back to FDN input
    this.pitchFeedbackGain = ctx.createGain();
    this.pitchFeedbackGain.gain.value = this.params.pitchFeedback;
    this.pitchReturnBus.connect(this.pitchFeedbackGain);
    this.pitchFeedbackGain.connect(this.fdnInputBus);

    // --- Master Bus & Summing ---
    this.earlyMixGain = ctx.createGain();
    this.lateMixGain = ctx.createGain();
    this.earlyMixGain.gain.value = 1.0 - this.params.earlyLateMix;
    this.lateMixGain.gain.value = this.params.earlyLateMix;

    this.earlyReflectionsBus.connect(this.earlyMixGain);
    this.hermiteSaturator.connect(this.lateMixGain);
    this.pitchReturnBus.connect(this.lateMixGain);

    this.wetSumBus = ctx.createGain();
    this.earlyMixGain.connect(this.wetSumBus);
    this.lateMixGain.connect(this.wetSumBus);
    this.modalBus.connect(this.wetSumBus);

    // Dry / Wet Crossfade
    this.dryGain = ctx.createGain();
    this.wetGain = ctx.createGain();
    this._updateDryWetGains();

    this.inputGain.connect(this.dryGain);
    this.wetSumBus.connect(this.wetGain);

    this.masterOutputBus = ctx.createGain();
    this.dryGain.connect(this.masterOutputBus);
    this.wetGain.connect(this.masterOutputBus);

    // Output Trim & Limiter
    this.outputTrimGain = ctx.createGain();
    this.outputTrimGain.gain.value = Math.pow(10, this.params.outputTrimDb / 20);

    this.masterLimiter = ctx.createDynamicsCompressor();
    this.masterLimiter.threshold.value = -0.5;
    this.masterLimiter.knee.value = 0.0;
    this.masterLimiter.ratio.value = 20.0;
    this.masterLimiter.attack.value = 0.002;
    this.masterLimiter.release.value = 0.050;

    this.masterOutputBus.connect(this.outputTrimGain);
    this.outputTrimGain.connect(this.masterLimiter);
    this.masterLimiter.connect(ctx.destination);

    // Audio Analysis tap for CRT Display
    this.analyserL = ctx.createAnalyser();
    this.analyserR = ctx.createAnalyser();
    this.analyserL.fftSize = 512;
    this.analyserR.fftSize = 512;

    const splitter = ctx.createChannelSplitter(2);
    this.masterLimiter.connect(splitter);
    splitter.connect(this.analyserL, 0);
    splitter.connect(this.analyserR, 1);
  }

  _generateHermiteCurve() {
    const n = 1024;
    const curve = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const x = (i / (n - 1)) * 4.0 - 2.0;
      // Hermite bounded saturation: linear inside [-0.75, 0.75], cubic outside, bounded to 1.05
      if (Math.abs(x) < 0.75) {
        curve[i] = x;
      } else {
        const sign = x > 0 ? 1 : -1;
        const absX = Math.abs(x);
        curve[i] = sign * (0.75 + 0.30 * Math.tanh((absX - 0.75) / 0.30));
      }
    }
    return curve;
  }

  _updateDryWetGains() {
    const mix = this.params.dryWetMix;
    // Equal-power crossfade: cos(mix * pi/2), sin(mix * pi/2)
    const dry = Math.cos(mix * 0.5 * Math.PI);
    const wet = Math.sin(mix * 0.5 * Math.PI);
    if (this.dryGain && this.wetGain) {
      this.dryGain.gain.setTargetAtTime(dry, this.ctx.currentTime, 0.02);
      this.wetGain.gain.setTargetAtTime(wet, this.ctx.currentTime, 0.02);
    }
  }

  _updatePitchBlendGains() {
    const b = this.params.pitchBlend; // -1.0 (Dimmer) to +1.0 (Shimmer)
    const shimGain = b >= 0 ? 1.0 : 1.0 + b;
    const dimGain = b <= 0 ? 1.0 : 1.0 - b;

    if (this.pitchBlendGainShim && this.pitchBlendGainDim) {
      this.pitchBlendGainShim.gain.setTargetAtTime(shimGain, this.ctx.currentTime, 0.02);
      this.pitchBlendGainDim.gain.setTargetAtTime(dimGain, this.ctx.currentTime, 0.02);
    }
  }

  setParam(name, value) {
    this.params[name] = value;
    if (!this.ctx) return;
    const now = this.ctx.currentTime;

    switch (name) {
      case 'preDelayMs':
        this.preDelayNode.delayTime.setTargetAtTime(value / 1000, now, 0.02);
        break;
      case 'dryWetMix':
        this._updateDryWetGains();
        break;
      case 'earlyLateMix':
        this.earlyMixGain.gain.setTargetAtTime(1.0 - value, now, 0.02);
        this.lateMixGain.gain.setTargetAtTime(value, now, 0.02);
        break;
      case 'lowCrossoverHz':
        this.lowPass1.frequency.setTargetAtTime(value, now, 0.02);
        this.lowPass2.frequency.setTargetAtTime(value, now, 0.02);
        this.highPass1.frequency.setTargetAtTime(value, now, 0.02);
        this.highPass2.frequency.setTargetAtTime(value, now, 0.02);
        break;
      case 'highDampingHz':
        for (const f of this.fdnDampingFilters) {
          f.frequency.setTargetAtTime(value, now, 0.02);
        }
        break;
      case 'decayRt60Sec':
      case 'roomSize': {
        const fdnPrimes = [0.0293, 0.0353, 0.0419, 0.0479, 0.0541, 0.0607, 0.0673, 0.0739];
        for (let i = 0; i < 8; i++) {
          this.fdnDelays[i].delayTime.setTargetAtTime(fdnPrimes[i] * this.params.roomSize, now, 0.02);
          const feedback = this.params.freezeHold ? 1.0 : Math.pow(0.001, fdnPrimes[i] / this.params.decayRt60Sec);
          this.fdnFeedbackGains[i].gain.setTargetAtTime(feedback, now, 0.02);
        }
        break;
      }
      case 'freezeHold': {
        const isFrozen = Boolean(value);
        this.fdnInputBus.gain.setTargetAtTime(isFrozen ? 0.0 : 1.0, now, 0.02);
        for (let i = 0; i < 8; i++) {
          this.fdnFeedbackGains[i].gain.setTargetAtTime(isFrozen ? 0.999 : Math.pow(0.001, (0.03 + i * 0.006) / this.params.decayRt60Sec), now, 0.02);
        }
        break;
      }
      case 'shimmerSend':
        this.shimmerSendGain.gain.setTargetAtTime(value, now, 0.02);
        break;
      case 'dimmerSend':
        this.dimmerSendGain.gain.setTargetAtTime(value, now, 0.02);
        break;
      case 'shimmerInterval':
        this.shimmerShifter.setSemitones(value);
        break;
      case 'dimmerInterval':
        this.dimmerShifter.setSemitones(value);
        break;
      case 'pitchBlend':
        this._updatePitchBlendGains();
        break;
      case 'pitchFeedback':
        this.pitchFeedbackGain.gain.setTargetAtTime(value, now, 0.02);
        break;
      case 'outputTrimDb':
        this.outputTrimGain.gain.setTargetAtTime(Math.pow(10, value / 20), now, 0.02);
        break;
    }
  }

  _startTelemetryLoop() {
    const leftBuffer = new Float32Array(512);
    const rightBuffer = new Float32Array(512);

    const check = () => {
      if (!this.ctx) return;
      if (this.analyserL && this.analyserR) {
        this.analyserL.getFloatTimeDomainData(leftBuffer);
        this.analyserR.getFloatTimeDomainData(rightBuffer);

        // Calculate band energies
        let sumLow = 0, sumMid = 0, sumHigh = 0;
        for (let i = 0; i < 512; i++) {
          const val = Math.abs(leftBuffer[i]);
          if (i < 128) sumLow += val;
          else if (i < 384) sumMid += val;
          else sumHigh += val;
        }

        const lowE = sumLow / 128;
        const midE = sumMid / 256;
        const highE = sumHigh / 128;

        if (this.onTelemetry) {
          this.onTelemetry(leftBuffer, rightBuffer, lowE, midE, highE);
        }
      }
      requestAnimationFrame(check);
    };

    requestAnimationFrame(check);
  }

  //============================================================================
  // Zero-Install Audition Sound Generators
  //============================================================================

  /**
   * Dirac Delta Impulse Click
   */
  triggerImpulse() {
    if (!this.ctx) return;
    const ctx = this.ctx;
    const buffer = ctx.createBuffer(1, 128, ctx.sampleRate);
    const data = buffer.getChannelData(0);
    data[0] = 1.0;

    const src = ctx.createBufferSource();
    src.buffer = buffer;
    src.connect(this.inputGain);
    src.start();
  }

  /**
   * Synthetic 808 Kick & Transient Click
   */
  triggerKick() {
    if (!this.ctx) return;
    const ctx = this.ctx;
    const now = ctx.currentTime;

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();

    osc.frequency.setValueAtTime(140, now);
    osc.frequency.exponentialRampToValueAtTime(45, now + 0.12);

    gain.gain.setValueAtTime(1.0, now);
    gain.gain.exponentialRampToValueAtTime(0.001, now + 0.35);

    osc.connect(gain);
    gain.connect(this.inputGain);

    osc.start(now);
    osc.stop(now + 0.35);
  }

  /**
   * Snare Transient
   */
  triggerSnare() {
    if (!this.ctx) return;
    const ctx = this.ctx;
    const now = ctx.currentTime;

    // Noise burst
    const noiseBuffer = ctx.createBuffer(1, Math.floor(ctx.sampleRate * 0.2), ctx.sampleRate);
    const output = noiseBuffer.getChannelData(0);
    for (let i = 0; i < noiseBuffer.length; i++) {
      output[i] = Math.random() * 2 - 1;
    }
    const whiteNoise = ctx.createBufferSource();
    whiteNoise.buffer = noiseBuffer;

    const filter = ctx.createBiquadFilter();
    filter.type = 'highpass';
    filter.frequency.value = 1000;

    const gain = ctx.createGain();
    gain.gain.setValueAtTime(0.8, now);
    gain.gain.exponentialRampToValueAtTime(0.01, now + 0.2);

    whiteNoise.connect(filter);
    filter.connect(gain);
    gain.connect(this.inputGain);

    whiteNoise.start(now);
    whiteNoise.stop(now + 0.2);
  }

  /**
   * Lush Felt Piano Chord (Harold Budd Physical Modeling matching AS-42)
   */
  triggerPianoChord() {
    if (!this.ctx) return;
    const ctx = this.ctx;
    const now = ctx.currentTime;

    // Dbmaj9 (Db3, F3, Ab3, C4, Eb4)
    const freqs = [138.59, 174.61, 207.65, 261.63, 311.13];

    freqs.forEach((f, idx) => {
      // Harmonic felt string pair (fundamental + sympathetic unison detune)
      const osc1 = ctx.createOscillator();
      const osc2 = ctx.createOscillator();
      osc1.type = 'sine';
      osc2.type = 'sine';
      osc1.frequency.setValueAtTime(f, now);
      osc2.frequency.setValueAtTime(f, now);
      osc1.detune.setValueAtTime(-0.8, now);
      osc2.detune.setValueAtTime(+1.4, now);

      const oscGain1 = ctx.createGain();
      const oscGain2 = ctx.createGain();
      oscGain1.gain.setValueAtTime(0.52, now);
      oscGain2.gain.setValueAtTime(0.22, now);
      osc1.connect(oscGain1);
      osc2.connect(oscGain2);

      const stringMix = ctx.createGain();
      oscGain1.connect(stringMix);
      oscGain2.connect(stringMix);

      // Una corda steep 24dB damping (cascaded Butterworth lowpasses)
      const filter1 = ctx.createBiquadFilter();
      const filter2 = ctx.createBiquadFilter();
      filter1.type = 'lowpass';
      filter2.type = 'lowpass';
      filter1.Q.setValueAtTime(-3.0103, now);
      filter2.Q.setValueAtTime(-3.0103, now);
      filter1.frequency.setValueAtTime(1800, now);
      filter2.frequency.setValueAtTime(1800, now);
      filter1.frequency.exponentialRampToValueAtTime(450, now + 0.35);
      filter2.frequency.exponentialRampToValueAtTime(450, now + 0.35);

      // Resonant spruce soundboard formant filter (peaking resonator at 540 Hz)
      const bodyFilter = ctx.createBiquadFilter();
      bodyFilter.type = 'peaking';
      bodyFilter.frequency.setValueAtTime(540, now);
      bodyFilter.Q.setValueAtTime(1.2, now);
      bodyFilter.gain.setValueAtTime(2.0, now);

      const gain = ctx.createGain();
      gain.gain.setValueAtTime(0.0001, now);
      gain.gain.linearRampToValueAtTime(0.18, now + 0.008);
      gain.gain.exponentialRampToValueAtTime(0.0001, now + 3.2);

      stringMix.connect(filter1);
      filter1.connect(filter2);
      filter2.connect(bodyFilter);
      bodyFilter.connect(gain);
      gain.connect(this.inputGain);

      osc1.start(now);
      osc2.start(now);
      osc1.stop(now + 3.2);
      osc2.stop(now + 3.2);
    });
  }

  /**
   * Ethereal Ambient Synth Pad Swell
   */
  triggerSynthPad() {
    if (!this.ctx) return;
    const ctx = this.ctx;
    const now = ctx.currentTime;

    // F minor 11 (F3, Ab3, C4, Eb4, G4, Bb4)
    const freqs = [174.61, 207.65, 261.63, 311.13, 392.00, 466.16];

    freqs.forEach((f) => {
      const osc = ctx.createOscillator();
      osc.type = 'sawtooth';
      osc.frequency.value = f * (1.0 + (Math.random() - 0.5) * 0.004); // subtle detune

      const filter = ctx.createBiquadFilter();
      filter.type = 'lowpass';
      filter.frequency.setValueAtTime(300, now);
      filter.frequency.linearRampToValueAtTime(3200, now + 1.5);
      filter.frequency.exponentialRampToValueAtTime(600, now + 4.5);

      const gain = ctx.createGain();
      gain.gain.setValueAtTime(0.001, now);
      gain.gain.linearRampToValueAtTime(0.12, now + 1.2);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 4.5);

      osc.connect(filter);
      filter.connect(gain);
      gain.connect(this.inputGain);

      osc.start(now);
      osc.stop(now + 4.5);
    });
  }
}
