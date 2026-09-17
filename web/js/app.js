/**
 * @file app.js
 * @brief Application Coordinator, Master Utilities & Exciter Controller for BRAUN RB-26 Web Showcase
 * Features:
 * - Bulletproof Lifecycle Guard & Audio Context Unlock
 * - Curated 10-Preset Factory Bank + User Preset Persistence in LocalStorage
 * - RFC 8259 JSON Patch Export & Drag-and-Drop Chassis Load
 * - Instant A/B Comparison Buffer with Copy Utilities
 * - Lossless 16-bit 48kHz WAV Master Output Bus Recorder
 * - Deck 07 Onboard Exciter: 11-key microtonal chime strip with velocity & glissando,
 *   12 signature chords with 4-speed strumming, Dirac/pink/hammer laboratory pulses,
 *   and autonomous generative Poisson ambient clock.
 */

import { BraunKnob } from './ui/knob.js';
import { BraunCrtDisplay } from './ui/crt-display.js';
import { BraunVectorPad } from './ui/vector-pad.js';
import { Rb26WebEngine } from './audio/rb26_web_engine.js';

// Modal Scale Definitions matching AS-42
export const SCALES = {
  BUDD_PENTATONIC: { name: 'BUDD PENTATONIC', intervals: [0, 2, 4, 7, 9] },
  LYDIAN_DREAM: { name: 'LYDIAN DREAM', intervals: [0, 2, 4, 6, 7, 9, 11] },
  DORIAN_MYSTIC: { name: 'DORIAN MYSTIC', intervals: [0, 2, 3, 5, 7, 9, 10] },
  YOSHIMURA_AMBIENT: { name: 'YOSHIMURA AMBIENT', intervals: [0, 2, 5, 7, 9] },
  AEOLIAN_MIDNIGHT: { name: 'AEOLIAN MIDNIGHT', intervals: [0, 2, 3, 5, 7, 8, 10] },
  BUDD_HEXATONIC: { name: 'BUDD HEXATONIC', intervals: [0, 2, 4, 5, 7, 9] },
  WHOLE_TONE: { name: 'WHOLE TONE', intervals: [0, 2, 4, 6, 8, 10] },
  AVALON_SPIRITED: { name: 'AVALON SPIRITED', intervals: [0, 2, 4, 5, 7, 9, 11] }
};

export const CHIME_KEY_MAP = {
  'KeyA': 0, 'KeyS': 1, 'KeyD': 2, 'KeyF': 3,
  'KeyG': 4, 'KeyH': 5, 'KeyJ': 6, 'KeyK': 7,
  'KeyL': 8, 'Semicolon': 9, 'Quote': 10
};

export const CHIME_CHAR_MAP = {
  'a': 0, 's': 1, 'd': 2, 'f': 3, 'g': 4, 'h': 5, 'j': 6, 'k': 7,
  'l': 8, ';': 9, "'": 10
};

export const CHORD_KEY_MAP = {
  'Digit1': 0, 'Digit2': 1, 'Digit3': 2, 'Digit4': 3, 'Digit5': 4, 'Digit6': 5,
  'Digit7': 6, 'Digit8': 7, 'Digit9': 8, 'Digit0': 9,
  'Minus': 10, 'Equal': 11,
  'Numpad1': 0, 'Numpad2': 1, 'Numpad3': 2, 'Numpad4': 3, 'Numpad5': 4, 'Numpad6': 5,
  'Numpad7': 6, 'Numpad8': 7, 'Numpad9': 8, 'Numpad0': 9,
  'NumpadSubtract': 10, 'NumpadAdd': 11
};

export function getChimeKeyIndex(e) {
  if (!e) return null;
  if (e.code && CHIME_KEY_MAP[e.code] !== undefined) return CHIME_KEY_MAP[e.code];
  const lower = e.key ? e.key.toLowerCase() : '';
  return CHIME_CHAR_MAP[lower] !== undefined ? CHIME_CHAR_MAP[lower] : null;
}

export function getChordKeyIndex(e) {
  if (!e) return null;
  if (e.code && CHORD_KEY_MAP[e.code] !== undefined) return CHORD_KEY_MAP[e.code];
  if (e.key >= '1' && e.key <= '9') return parseInt(e.key, 10) - 1;
  if (e.key === '0') return 9;
  if (e.key === '-' || e.key === '_') return 10;
  if (e.key === '=' || e.key === '+') return 11;
  return null;
}

export const CHORD_SPEEDS = {
  slow: 120,
  med: 50,
  fast: 20,
  instant: 0
};

// 12 Signature Chord Voicings (Hz)
export const CHORD_VOICINGS = [
  { id: 'PAVILION', name: 'PAVILION', freqs: [138.59, 207.65, 277.18, 311.13, 415.30] }, // Harold Budd Sus2 (Db3, Ab3, Db4, Eb4, Ab4)
  { id: 'PLATEAUX', name: 'PLATEAUX', freqs: [130.81, 196.00, 246.94, 293.66, 392.00] }, // Maj9 (C3, G3, B3, D4, G4)
  { id: 'DEEP_5TH', name: 'DEEP 5TH', freqs: [65.41, 98.00, 130.81, 196.00] },          // Sub Drone 5th (C2, G2, C3, G3)
  { id: 'ETHEREAL', name: 'ETHEREAL', freqs: [174.61, 261.63, 349.23, 392.00, 523.25] }, // Suspended Cloud (F3, C4, F4, G4, C5)
  { id: 'LYDIAN', name: 'LYDIAN', freqs: [174.61, 220.00, 261.63, 369.99, 523.25] },     // Lydian #11 (F3, A3, C4, F#4, C5)
  { id: 'SOLAR_BEAT', name: 'SOLAR BEAT', freqs: [146.83, 220.00, 293.66, 295.02, 440.00] }, // Microtonal Beating (D3, A3, D4, D4+8c, A4)
  { id: 'AVALON', name: 'AVALON', freqs: [110.00, 164.81, 220.00, 277.18, 329.63] },     // Avalon Maj9 (A2, E3, A3, C#4, E4)
  { id: 'BLADE_RUNNER', name: 'BLADE RUNNER', freqs: [130.81, 196.00, 261.63, 329.63, 392.00, 493.88] }, // CS-80 Space Poly (C3, G3, C4, E4, G4, B4)
  { id: 'MIN7_9', name: 'MIN7 9', freqs: [146.83, 174.61, 220.00, 261.63, 329.63] },     // Minor 9th (D3, F3, A3, C4, E4)
  { id: 'SUS4_7', name: 'SUS4 7', freqs: [196.00, 261.63, 293.66, 349.23] },             // Suspended 7th (G3, C4, D4, F4)
  { id: 'MAJOR_9', name: 'MAJOR 9', freqs: [130.81, 164.81, 196.00, 246.94, 293.66] },   // Pure Maj9 (C3, E3, G3, B3, D4)
  { id: 'CLUSTER', name: 'CLUSTER', freqs: [220.00, 233.08, 246.94, 261.63, 277.18] }    // Inharmonic Cluster (A3, Bb3, B3, C4, C#4)
];

// Curated 10 Studio Factory Presets
export const FACTORY_PRESETS = {
  DEFAULT: {
    name: 'CALIBRATED DEFAULT',
    params: {
      predelay: 24.0, diffusion: 75, input_trim: 0.0,
      low_crossover: 180, damping_low: 1.0, low_punch: 65, mono_bass: 120,
      rt60_decay: 6.5, room_size: 100, damping_high: 1800, decay_hold: false,
      shimmer_send: 40, dimmer_send: 35, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 0, pitch_regen: 45,
      tail_mod_rate: 0.65, tail_mod_depth: 45, tail_bloom: 85,
      stereo_width: 100, early_late_mix: 50, dry_wet_mix: 40, output_trim: 0.0,
      soft_limiter: true
    }
  },
  AMBIENT_GUITAR_CLOUD: {
    name: 'AMBIENT GUITAR CLOUD',
    params: {
      predelay: 45.0, diffusion: 85, input_trim: 0.0,
      low_crossover: 200, damping_low: 0.85, low_punch: 50, mono_bass: 140,
      rt60_decay: 9.5, room_size: 130, damping_high: 9500, decay_hold: false,
      shimmer_send: 48, dimmer_send: 5, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 100, pitch_regen: 42,
      tail_mod_rate: 0.40, tail_mod_depth: 28, tail_bloom: 110,
      stereo_width: 140, early_late_mix: 70, dry_wet_mix: 55, output_trim: 0.0,
      soft_limiter: true
    }
  },
  AS42_SHIMMER_COMPANION: {
    name: 'AS-42 TAPE & SHIMMER COMPANION',
    params: {
      predelay: 28.0, diffusion: 85, input_trim: 0.0,
      low_crossover: 180, damping_low: 1.0, low_punch: 60, mono_bass: 120,
      rt60_decay: 8.5, room_size: 115, damping_high: 6800, decay_hold: false,
      shimmer_send: 45, dimmer_send: 25, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 40, pitch_regen: 50,
      tail_mod_rate: 0.65, tail_mod_depth: 40, tail_bloom: 85,
      stereo_width: 120, early_late_mix: 55, dry_wet_mix: 45, output_trim: 0.0,
      soft_limiter: true
    }
  },
  SOFT_FELT_ACOUSTIC_HALL: {
    name: 'SOFT FELT ACOUSTIC HALL',
    params: {
      predelay: 20.0, diffusion: 78, input_trim: 0.0,
      low_crossover: 160, damping_low: 0.95, low_punch: 55, mono_bass: 110,
      rt60_decay: 4.8, room_size: 90, damping_high: 5600, decay_hold: false,
      shimmer_send: 15, dimmer_send: 10, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 20, pitch_regen: 25,
      tail_mod_rate: 0.45, tail_mod_depth: 25, tail_bloom: 70,
      stereo_width: 110, early_late_mix: 45, dry_wet_mix: 38, output_trim: 0.0,
      soft_limiter: true
    }
  },
  GERMAN_PLATE_140: {
    name: 'GERMAN PLATE 140',
    params: {
      predelay: 10.0, diffusion: 92, input_trim: 0.0,
      low_crossover: 220, damping_low: 0.8, low_punch: 70, mono_bass: 130,
      rt60_decay: 3.8, room_size: 85, damping_high: 8500, decay_hold: false,
      shimmer_send: 20, dimmer_send: 5, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 100, pitch_regen: 20,
      tail_mod_rate: 0.80, tail_mod_depth: 20, tail_bloom: 45,
      stereo_width: 130, early_late_mix: 40, dry_wet_mix: 35, output_trim: 0.0,
      soft_limiter: true
    }
  },
  CATHEDRAL_DIFFUSION: {
    name: 'CATHEDRAL DIFFUSION',
    params: {
      predelay: 45.0, diffusion: 95, input_trim: -1.0,
      low_crossover: 180, damping_low: 0.9, low_punch: 45, mono_bass: 120,
      rt60_decay: 18.0, room_size: 175, damping_high: 10000, decay_hold: false,
      shimmer_send: 65, dimmer_send: 10, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 75, pitch_regen: 55,
      tail_mod_rate: 0.50, tail_mod_depth: 45, tail_bloom: 140,
      stereo_width: 160, early_late_mix: 80, dry_wet_mix: 65, output_trim: -2.0,
      soft_limiter: true
    }
  },
  ETHEREAL_SYNTH_PAD: {
    name: 'ETHEREAL SYNTH PAD',
    params: {
      predelay: 35.0, diffusion: 80, input_trim: 0.0,
      low_crossover: 160, damping_low: 0.9, low_punch: 50, mono_bass: 100,
      rt60_decay: 10.5, room_size: 120, damping_high: 8500, decay_hold: false,
      shimmer_send: 55, dimmer_send: 40, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 25, pitch_regen: 45,
      tail_mod_rate: 0.70, tail_mod_depth: 45, tail_bloom: 90,
      stereo_width: 140, early_late_mix: 60, dry_wet_mix: 50, output_trim: 0.0,
      soft_limiter: true
    }
  },
  BLOOM_SHIMMER_VOID: {
    name: 'BLOOM SHIMMER VOID',
    params: {
      predelay: 50.0, diffusion: 88, input_trim: -0.5,
      low_crossover: 190, damping_low: 0.85, low_punch: 45, mono_bass: 130,
      rt60_decay: 14.0, room_size: 150, damping_high: 9000, decay_hold: false,
      shimmer_send: 60, dimmer_send: 15, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 80, pitch_regen: 50,
      tail_mod_rate: 0.55, tail_mod_depth: 50, tail_bloom: 160,
      stereo_width: 150, early_late_mix: 75, dry_wet_mix: 60, output_trim: -1.0,
      soft_limiter: true
    }
  },
  INFINITE_ETHEREAL_FREEZE: {
    name: 'INFINITE ETHEREAL FREEZE',
    params: {
      predelay: 20.0, diffusion: 90, input_trim: 0.0,
      low_crossover: 180, damping_low: 1.0, low_punch: 50, mono_bass: 120,
      rt60_decay: 30.0, room_size: 120, damping_high: 8000, decay_hold: true,
      shimmer_send: 45, dimmer_send: 30, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 20, pitch_regen: 50,
      tail_mod_rate: 0.65, tail_mod_depth: 45, tail_bloom: 85,
      stereo_width: 130, early_late_mix: 70, dry_wet_mix: 55, output_trim: -1.0,
      soft_limiter: true
    }
  },
  SUB_BASS_PRESERVER: {
    name: 'SUB-BASS PRESERVER',
    params: {
      predelay: 15.0, diffusion: 70, input_trim: 0.0,
      low_crossover: 180, damping_low: 0.8, low_punch: 85, mono_bass: 150,
      rt60_decay: 4.5, room_size: 80, damping_high: 6500, decay_hold: false,
      shimmer_send: 25, dimmer_send: 15, shimmer_interval: 12, dimmer_interval: -12,
      shimmer_dimmer_blend: 10, pitch_regen: 35,
      tail_mod_rate: 0.40, tail_mod_depth: 30, tail_bloom: 60,
      stereo_width: 100, early_late_mix: 45, dry_wet_mix: 35, output_trim: 0.0,
      soft_limiter: true
    }
  }
};

// Aliases for identifier compatibility
FACTORY_PRESETS['SUB-BASS_PRESERVER'] = FACTORY_PRESETS.SUB_BASS_PRESERVER;
FACTORY_PRESETS['CATHEDRAL_SHIMMER'] = FACTORY_PRESETS.CATHEDRAL_DIFFUSION;
FACTORY_PRESETS['INFINITE_FREEZE_DRONE'] = FACTORY_PRESETS.INFINITE_ETHEREAL_FREEZE;

/**
 * Quantizes an incoming MIDI pitch to the closest degree in the selected scale.
 */
export function quantizeMidiToScale(midi, rootPitchClass = 0, intervals = [0, 2, 4, 7, 9]) {
  const normRoot = ((rootPitchClass % 12) + 12) % 12;
  const baseOctave = Math.floor((midi - normRoot) / 12);
  const relSemitone = (midi - normRoot) - (baseOctave * 12);

  let minDiff = 1e9;
  let bestInterval = intervals[0];

  for (let i = 0; i < intervals.length; i++) {
    const diff = Math.abs(relSemitone - intervals[i]);
    if (diff < minDiff) {
      minDiff = diff;
      bestInterval = intervals[i];
    }
  }

  // Wrap upper octave
  const wrapUp = Math.abs(relSemitone - (intervals[0] + 12));
  if (wrapUp < minDiff) {
    minDiff = wrapUp;
    bestInterval = intervals[0] + 12;
  }

  // Wrap lower octave
  const wrapDown = Math.abs(relSemitone - (intervals[intervals.length - 1] - 12));
  if (wrapDown < minDiff) {
    minDiff = wrapDown;
    bestInterval = intervals[intervals.length - 1] - 12;
  }

  return normRoot + (baseOctave * 12) + bestInterval;
}

/**
 * Computes sequential scale degree MIDI pitch for chime keys (zero duplicate notes).
 * Maps key index (0 to 10) sequentially through scale degrees across octaves.
 */
export function getChimeMidiForDegree(degreeIndex, rootPitchClass = 0, intervals = [0, 2, 4, 7, 9]) {
  const safeIntervals = (Array.isArray(intervals) && intervals.length > 0) ? intervals : [0, 2, 4, 7, 9];
  const normRoot = ((rootPitchClass % 12) + 12) % 12;
  const numDegrees = safeIntervals.length;
  const octave = Math.floor(degreeIndex / numDegrees);
  const degree = ((degreeIndex % numDegrees) + numDegrees) % numDegrees;
  const semitonesFromRoot = octave * 12 + safeIntervals[degree];
  return 60 + normRoot + semitonesFromRoot;
}

// ============================================================================
// Lossless 16-bit 48kHz WAV Output Bus Recorder
// ============================================================================
export class MasterWavRecorder {
  constructor(ctx, sourceNode) {
    this.ctx = ctx;
    this.sourceNode = sourceNode;
    this.isRecording = false;
    this.recordedBuffersL = [];
    this.recordedBuffersR = [];
    this.totalSamples = 0;
    this.processor = null;
    this.sink = null;
  }

  start() {
    if (this.isRecording || !this.ctx || !this.sourceNode) return;
    this.isRecording = true;
    this.recordedBuffersL = [];
    this.recordedBuffersR = [];
    this.totalSamples = 0;

    const bufferSize = 4096;
    this.processor = this.ctx.createScriptProcessor(bufferSize, 2, 2);
    this.sink = this.ctx.createGain();
    this.sink.gain.value = 0.0;

    this.processor.onaudioprocess = (e) => {
      if (!this.isRecording) return;
      const inputL = e.inputBuffer.getChannelData(0);
      const inputR = e.inputBuffer.getChannelData(1);

      this.recordedBuffersL.push(new Float32Array(inputL));
      this.recordedBuffersR.push(new Float32Array(inputR));
      this.totalSamples += inputL.length;
    };

    this.sourceNode.connect(this.processor);
    this.processor.connect(this.sink);
    this.sink.connect(this.ctx.destination);
  }

  stop() {
    if (!this.isRecording) return null;
    this.isRecording = false;

    if (this.processor && this.sourceNode) {
      try {
        this.sourceNode.disconnect(this.processor);
        this.processor.disconnect(this.sink);
        this.sink.disconnect(this.ctx.destination);
      } catch (e) {}
    }

    if (this.totalSamples === 0) return null;

    const leftMerged = new Float32Array(this.totalSamples);
    const rightMerged = new Float32Array(this.totalSamples);
    let offset = 0;
    for (let i = 0; i < this.recordedBuffersL.length; i++) {
      leftMerged.set(this.recordedBuffersL[i], offset);
      rightMerged.set(this.recordedBuffersR[i], offset);
      offset += this.recordedBuffersL[i].length;
    }

    this.recordedBuffersL = [];
    this.recordedBuffersR = [];

    const sampleRate = this.ctx.sampleRate || 48000;
    const wavBlob = this.encodeWAV(leftMerged, rightMerged, sampleRate);
    this.downloadBlob(wavBlob, `BRAUN_RB26_${new Date().toISOString().replace(/[:.]/g, '-')}.wav`);
    return wavBlob;
  }

  encodeWAV(left, right, sampleRate) {
    const numChannels = 2;
    const bytesPerSample = 2; // 16-bit PCM
    const blockAlign = numChannels * bytesPerSample;
    const byteRate = sampleRate * blockAlign;
    const dataSize = left.length * blockAlign;
    const buffer = new ArrayBuffer(44 + dataSize);
    const view = new DataView(buffer);

    const writeString = (offset, str) => {
      for (let i = 0; i < str.length; i++) {
        view.setUint8(offset + i, str.charCodeAt(i));
      }
    };

    // RIFF chunk
    writeString(0, 'RIFF');
    view.setUint32(4, 36 + dataSize, true);
    writeString(8, 'WAVE');

    // fmt sub-chunk
    writeString(12, 'fmt ');
    view.setUint32(16, 16, true);
    view.setUint16(20, 1, true);
    view.setUint16(22, numChannels, true);
    view.setUint32(24, sampleRate, true);
    view.setUint32(28, byteRate, true);
    view.setUint16(32, blockAlign, true);
    view.setUint16(34, 16, true);

    // data sub-chunk
    writeString(36, 'data');
    view.setUint32(40, dataSize, true);

    // Write interleaved 16-bit signed PCM samples
    let offset = 44;
    for (let i = 0; i < left.length; i++) {
      const sL = Math.max(-1.0, Math.min(1.0, left[i]));
      view.setInt16(offset, sL < 0 ? sL * 0x8000 : sL * 0x7FFF, true);
      offset += 2;

      const sR = Math.max(-1.0, Math.min(1.0, right[i]));
      view.setInt16(offset, sR < 0 ? sR * 0x8000 : sR * 0x7FFF, true);
      offset += 2;
    }

    return new Blob([buffer], { type: 'audio/wav' });
  }

  downloadBlob(blob, filename) {
    if (typeof window === 'undefined' || typeof document === 'undefined') return;
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.style.display = 'none';
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    setTimeout(() => {
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }, 1000);
  }
}

// ============================================================================
// Instant A/B Comparison Buffer with Copy Utilities
// ============================================================================
export class ReverbComparisonBuffer {
  constructor(app) {
    this.app = app;
    this.activeBuffer = 'A';
    this.bufferA = null;
    this.bufferB = null;
  }

  init() {
    this.bufferA = this._snapshot();
    this.bufferB = this._snapshot();
  }

  _snapshot() {
    const snap = {};
    for (const [key, knob] of Object.entries(this.app.knobs)) {
      if (knob && typeof knob.getValue === 'function') {
        snap[key] = knob.getValue();
      }
    }
    if (typeof document !== 'undefined') {
      const holdBtn = document.getElementById('btn-decay-hold');
      if (holdBtn) snap.decay_hold = holdBtn.classList.contains('is-active');
      const limBtn = document.getElementById('btn-soft-limiter');
      if (limBtn) snap.soft_limiter = limBtn.classList.contains('is-active');
    }
    return snap;
  }

  toggle() {
    if (!this.bufferA) this.init();
    if (this.activeBuffer === 'A') {
      this.bufferA = this._snapshot();
      this.activeBuffer = 'B';
      this._restore(this.bufferB);
    } else {
      this.bufferB = this._snapshot();
      this.activeBuffer = 'A';
      this._restore(this.bufferA);
    }
    this._updateUi();
    return this.activeBuffer;
  }

  copyAToB() {
    if (!this.bufferA) this.init();
    if (this.activeBuffer === 'A') {
      this.bufferB = this._snapshot();
    } else {
      this.bufferA = this._snapshot();
    }
    this._updateUi();
  }

  _restore(snapshot) {
    if (!snapshot) return;
    for (const [key, val] of Object.entries(snapshot)) {
      if (this.app.knobs[key]) {
        this.app.knobs[key].setValue(val, true);
      }
    }
    if (typeof document !== 'undefined') {
      if (snapshot.decay_hold !== undefined) {
        const holdBtn = document.getElementById('btn-decay-hold');
        if (holdBtn) {
          holdBtn.classList.toggle('is-active', snapshot.decay_hold);
          const statusText = holdBtn.querySelector('.braun-status-text');
          if (statusText) statusText.textContent = snapshot.decay_hold ? 'FREEZE ON' : 'FREEZE OFF';
          if (this.app.engine) this.app.engine.setParam('freezeHold', snapshot.decay_hold);
        }
      }
      if (snapshot.soft_limiter !== undefined) {
        const limBtn = document.getElementById('btn-soft-limiter');
        if (limBtn) {
          limBtn.classList.toggle('is-active', snapshot.soft_limiter);
          if (this.app.engine) this.app.engine.setParam('limiterEnable', snapshot.soft_limiter);
        }
      }
    }
  }

  _updateUi() {
    if (typeof document === 'undefined') return;
    const textEl = document.getElementById('ab-status-text');
    if (textEl) textEl.textContent = this.activeBuffer;

    const toggleBtn = document.getElementById('btn-ab-toggle');
    if (toggleBtn) toggleBtn.classList.toggle('is-buffer-b', this.activeBuffer === 'B');

    const copyBtn = document.getElementById('btn-ab-copy');
    if (copyBtn) copyBtn.textContent = this.activeBuffer === 'A' ? 'COPY A\u2192B' : 'COPY B\u2192A';
  }
}

// ============================================================================
// Main Application Coordinator
// ============================================================================
export class BraunRb26App {
  constructor() {
    this.engine = new Rb26WebEngine();
    this.knobs = {};
    this.display = null;
    this._isJuce = Boolean(
      typeof window !== 'undefined' &&
      (window.__IS_JUCE__ || window.__JUCE__?.backend || window.__JUCE__ || (window.location && (window.location.protocol === 'juce:' || window.location.hostname === 'juce.backend')))
    );
    this.isPowered = this.isJuce;
    this.currentPresetKey = 'DEFAULT';

    // JUCE IPC Coalescing Dispatcher
    this._pendingJuceParams = new Map();
    this._juceRafId = null;

    // Master Utilities
    this.abBuffer = new ReverbComparisonBuffer(this);
    this.wavRecorder = null;
    this._isJuceRecording = false;

    // Deck 07 Exciter State
    this.currentScaleKey = 'BUDD_PENTATONIC';
    this.rootPitchClass = 0; // C
    this.chordSpeed = 'med';
    this.isPoissonRunning = false;
    this.poissonEpm = 18;
    this.poissonHumanize = 0.50;
    this.poissonTimer = null;
    this.lastPoissonMidi = 60;
    this._heldKeys = new Set();
    this._activeVoices = new Map();
    this._activeChordButtons = new Map();
    this._initRoomSizeSmoother();
  }

  get isJuce() {
    return Boolean(
      this._isJuce ||
      (typeof window !== 'undefined' && (window.__IS_JUCE__ || window.__JUCE__?.backend || window.__JUCE__)) ||
      (typeof window !== 'undefined' && window.location && (window.location.protocol === 'juce:' || window.location.hostname === 'juce.backend'))
    );
  }

  set isJuce(val) {
    this._isJuce = Boolean(val);
  }

  _initRoomSizeSmoother() {
    this._targetRoomSize = 1.0;
    this._currentRoomSize = 1.0;
    this._roomSizeRafId = null;
    this._lastRoomSizeUpdateTime = 0;
    this._roomSizeThrottleMs = 85; // Matches equal-power crossfade window (75-100 ms)
  }

  _updateRoomSize(targetNorm, immediate = false) {
    this._targetRoomSize = Math.max(0.1, Math.min(4.0, targetNorm));
    const nowMs = (typeof performance !== 'undefined' ? performance.now() : Date.now());

    if (immediate || !this.engine) {
      if (this._roomSizeRafId) {
        clearTimeout(this._roomSizeRafId);
        this._roomSizeRafId = null;
      }
      this._currentRoomSize = this._targetRoomSize;
      if (this.engine) this.engine.setParam('roomSize', this._currentRoomSize);
      this._lastRoomSizeUpdateTime = nowMs;
      return;
    }

    if (Math.abs(this._targetRoomSize - this._currentRoomSize) < 0.001) return;

    // Dual-bank crossfading in web engine transitions smoothly over 85ms without Doppler artifacts.
    // Rate-limit parameter updates to >= 85ms intervals to prevent conflicting crossfades during rapid scrubs.
    const elapsed = nowMs - this._lastRoomSizeUpdateTime;
    if (elapsed >= this._roomSizeThrottleMs) {
      if (this._roomSizeRafId) {
        clearTimeout(this._roomSizeRafId);
        this._roomSizeRafId = null;
      }
      this._currentRoomSize = this._targetRoomSize;
      this._lastRoomSizeUpdateTime = nowMs;
      if (this.engine) this.engine.setParam('roomSize', this._currentRoomSize);
    } else {
      // Debounce trailing edge to guarantee final knob position is committed after rapid scrub
      if (this._roomSizeRafId) clearTimeout(this._roomSizeRafId);
      const remainingMs = Math.max(10, this._roomSizeThrottleMs - elapsed);
      this._roomSizeRafId = setTimeout(() => {
        this._roomSizeRafId = null;
        this._currentRoomSize = this._targetRoomSize;
        this._lastRoomSizeUpdateTime = (typeof performance !== 'undefined' ? performance.now() : Date.now());
        if (this.engine) this.engine.setParam('roomSize', this._currentRoomSize);
      }, remainingMs);
    }
  }

  async setPower(isPowered) {
    this.isPowered = Boolean(isPowered);
    const powerBtn = document.getElementById('btn-power');
    if (powerBtn) {
      powerBtn.classList.toggle('is-active', this.isPowered);
      const statusText = powerBtn.querySelector('.braun-status-text');
      if (statusText) statusText.textContent = this.isPowered ? 'POWER ON' : 'STANDBY';
    }
    if (this.display) this.display.setPower(this.isPowered);

    if (!this.isPowered) {
      // Cancel active room size debounce timer
      if (this._roomSizeRafId) {
        clearTimeout(this._roomSizeRafId);
        this._roomSizeRafId = null;
      }
      // Release any currently held voices
      if (this._activeVoices) {
        this._activeVoices.forEach((entry) => {
          const v = entry?.voice || (typeof entry?.release === 'function' ? entry : null);
          if (v && typeof v.release === 'function') {
            try { v.release(0.01); } catch (_) {}
          }
        });
        this._activeVoices.clear();
      }
      if (this._activeChordButtons) {
        this._activeChordButtons.forEach((voice, btn) => {
          if (voice && typeof voice.release === 'function') {
            try { voice.release(0.01); } catch (_) {}
          }
          if (btn && btn.classList) btn.classList.remove('is-active');
        });
        this._activeChordButtons.clear();
      }
      if (this._heldKeys) {
        this._heldKeys.clear();
      }
      if (this.isPoissonRunning) this.togglePoisson();

      // Stop recording if active when powering down
      if (this.isJuce && this._isJuceRecording) {
        this._isJuceRecording = false;
        const recordBtn = document.getElementById('btn-record-wav');
        if (recordBtn) {
          recordBtn.classList.remove('is-recording');
          const recText = recordBtn.querySelector('.braun-rec-text');
          if (recText) recText.textContent = 'REC WAV';
        }
        try {
          const backend = window.__JUCE__?.backend;
          if (backend && typeof backend.emitEvent === 'function') {
            backend.emitEvent('stopRecording', {});
          }
        } catch (err) {
          console.warn('JUCE backend emitEvent stopRecording error:', err);
        }
        this._emitJuceParam('stopRecording', 1.0, true);
      } else if (!this.isJuce && this.wavRecorder && this.wavRecorder.isRecording) {
        this.wavRecorder.stop();
        const recordBtn = document.getElementById('btn-record-wav');
        if (recordBtn) {
          recordBtn.classList.remove('is-recording');
          const recText = recordBtn.querySelector('.braun-rec-text');
          if (recText) recText.textContent = 'REC WAV';
        }
      }
    }

    if (this.engine && !this.isJuce) {
      if (this.isPowered) {
        if (!this.engine.isInitialized) await this.engine.init();
        if (this.engine.analyserL && this.display && !this.display.analyserL) {
          this.display.setAnalysers(this.engine.analyserL, this.engine.analyserR);
        }
        if (this.engine.ctx && this.engine.ctx.state === 'suspended') {
          await this.engine.ctx.resume();
        }
        this.engine.setPower(true);
      } else {
        this.engine.setPower(false);
        // Allow audio processing thread 25ms to render silence before suspending context
        await new Promise((r) => setTimeout(r, 25));
        if (this.engine.ctx && this.engine.ctx.state === 'running') {
          await this.engine.ctx.suspend();
        }
      }
    }

    this._emitJuceParam('power', this.isPowered ? 1.0 : 0.0, true);
  }

  async init() {
    this._initTheme();
    this._initDisplay();
    this._initKnobs();
    this._initVectorPad();
    this._initButtons();
    this._initAuditionBar();
    this._initExciterDeck();
    this._initPatchManagement();
    this._initMidi();
    this._initJuceBridge();
    this._initLifecycleAudioUnlock();
  }

  _emitJuceParam(id, value, immediate = false) {
    if (!this.isJuce || typeof window === 'undefined') return;
    this._pendingJuceParams.set(id, value);
    if (immediate) {
      this._flushJuceParams();
      return;
    }
    if (!this._juceRafId) {
      this._juceRafId = (typeof requestAnimationFrame === 'function')
        ? requestAnimationFrame(() => this._flushJuceParams())
        : setTimeout(() => this._flushJuceParams(), 16);
    }
  }

  _flushJuceParams() {
    if (this._juceRafId) {
      if (typeof cancelAnimationFrame === 'function') cancelAnimationFrame(this._juceRafId);
      else clearTimeout(this._juceRafId);
      this._juceRafId = null;
    }
    if (!this._pendingJuceParams || this._pendingJuceParams.size === 0) return;
    const backend = window.__JUCE__?.backend;
    if (!backend || typeof backend.emitEvent !== 'function') {
      this._pendingJuceParams.clear();
      return;
    }
    this._pendingJuceParams.forEach((value, id) => {
      try {
        backend.emitEvent('paramChange', { id, value });
      } catch (e) {}
    });
    this._pendingJuceParams.clear();
  }

  _emitJuceExciter(data) {
    if (this.isJuce && typeof window !== 'undefined' && window.__JUCE__?.backend?.emitEvent) {
      try {
        window.__JUCE__.backend.emitEvent('exciterTrigger', data);
      } catch (e) {}
    }
  }

  _initJuceBridge() {
    if (typeof window === 'undefined') return;

    const setupBackend = () => {
      const backend = window.__JUCE__?.backend;
      if (!backend || this._juceBridgeInitialized) return;
      this._juceBridgeInitialized = true;
      this.isJuce = true;

      // Ensure power status reflects in UI
      if (this.isPowered) {
        const powerBtn = document.getElementById('btn-power');
        if (powerBtn) {
          powerBtn.classList.add('is-active');
          const statusText = powerBtn.querySelector('.braun-status-text');
          if (statusText) statusText.textContent = 'POWER ON';
        }
      }

      if (typeof backend.addEventListener === 'function') {
        backend.addEventListener('paramUpdate', (data) => {
          if (!data || !data.id) return;
          if (data.id === 'power' || data.apvtsId === 'power') {
            const nextPower = data.value > 0.5;
            if (this.isPowered !== nextPower) {
              this.setPower(nextPower);
            }
            return;
          }

          if ((data.id === 'recording' || data.id === 'isRecording' || data.apvtsId === 'isRecording') && typeof data.value === 'number') {
            this._isJuceRecording = (data.value > 0.5);
            const recordBtn = document.getElementById('btn-record-wav');
            if (recordBtn) {
              recordBtn.classList.toggle('is-recording', this._isJuceRecording);
              const recText = recordBtn.querySelector('.braun-rec-text');
              if (recText) recText.textContent = this._isJuceRecording ? 'STOP & SAVE' : 'REC WAV';
            }
            return;
          }

          if (data.id === 'freeze_hold' || data.id === 'freezeHold' || data.apvtsId === 'freeze_hold') {
            const isHeld = data.value > 0.5;
            const holdBtn = document.getElementById('btn-decay-hold');
            if (holdBtn) {
              holdBtn.classList.toggle('is-active', isHeld);
              const statusText = holdBtn.querySelector('.braun-status-text');
              if (statusText) statusText.textContent = isHeld ? 'FREEZE ON' : 'FREEZE OFF';
            }
            if (this.engine) this.engine.setParam('freezeHold', isHeld);
            return;
          }

          if (data.id === 'limiter_enable' || data.id === 'limiterEnable' || data.apvtsId === 'limiter_enable') {
            const isLim = data.value > 0.5;
            const limBtn = document.getElementById('btn-soft-limiter');
            if (limBtn) {
              limBtn.classList.toggle('is-active', isLim);
            }
            if (this.engine) this.engine.setParam('limiterEnable', isLim);
            return;
          }

          if (data.id === 'shimmer_interval' || data.id === 'shimmerInterval' || data.apvtsId === 'shimmer_interval') {
            const sIdx = Math.round(data.value);
            const intervals = [7, 12, 24];
            const st = intervals[sIdx] ?? 12;
            const shimBtns = document.querySelectorAll('.shim-interval-btn');
            shimBtns.forEach((btn) => {
              btn.classList.toggle('is-active', parseInt(btn.dataset.interval, 10) === st);
            });
            if (this.engine) this.engine.setParam('shimmerInterval', st);
            return;
          }

          if (data.id === 'dimmer_interval' || data.id === 'dimmerInterval' || data.apvtsId === 'dimmer_interval') {
            const dIdx = Math.round(data.value);
            const intervals = [-2, -7, -12];
            const st = intervals[dIdx] ?? -12;
            const dimBtns = document.querySelectorAll('.dim-interval-btn');
            dimBtns.forEach((btn) => {
              btn.classList.toggle('is-active', parseInt(btn.dataset.interval, 10) === st);
            });
            if (this.engine) this.engine.setParam('dimmerInterval', st);
            return;
          }

          const apvtsToKnob = {
            input_trim_db: { key: 'input_trim', scale: 1 },
            inputTrimDb: { key: 'input_trim', scale: 1 },
            pre_delay_ms: { key: 'predelay', scale: 1 },
            preDelayMs: { key: 'predelay', scale: 1 },
            diffusion_density: { key: 'diffusion', scale: 100 },
            diffusionDensity: { key: 'diffusion', scale: 100 },
            low_crossover_hz: { key: 'low_crossover', scale: 1 },
            lowCrossoverHz: { key: 'low_crossover', scale: 1 },
            bass_rt60_mult: { key: 'damping_low', scale: 1 },
            bassRt60Mult: { key: 'damping_low', scale: 1 },
            punch_ducking: { key: 'low_punch', scale: 100 },
            punchDucking: { key: 'low_punch', scale: 100 },
            sub_mono_hz: { key: 'mono_bass', scale: 1 },
            subMonoHz: { key: 'mono_bass', scale: 1 },
            decay_rt60_sec: { key: 'rt60_decay', scale: 1 },
            decayRt60Sec: { key: 'rt60_decay', scale: 1 },
            room_size: { key: 'room_size', scale: 100 },
            roomSize: { key: 'room_size', scale: 100 },
            high_damping_hz: { key: 'damping_high', scale: 1 },
            highDampingHz: { key: 'damping_high', scale: 1 },
            shimmer_send: { key: 'shimmer_send', scale: 100 },
            shimmerSend: { key: 'shimmer_send', scale: 100 },
            dimmer_send: { key: 'dimmer_send', scale: 100 },
            dimmerSend: { key: 'dimmer_send', scale: 100 },
            pitch_blend: { key: 'shimmer_dimmer_blend', scale: 100 },
            pitchBlend: { key: 'shimmer_dimmer_blend', scale: 100 },
            pitch_feedback: { key: 'pitch_regen', scale: 100 },
            pitchFeedback: { key: 'pitch_regen', scale: 100 },
            tail_mod_rate_hz: { key: 'tail_mod_rate', scale: 1 },
            tailModRateHz: { key: 'tail_mod_rate', scale: 1 },
            tail_mod_depth_ms: { key: 'tail_mod_depth', scale: 100 / 3.0 },
            tailModDepthMs: { key: 'tail_mod_depth', scale: 100 / 3.0 },
            tail_bloom_ms: { key: 'tail_bloom', scale: 1 },
            tailBloomMs: { key: 'tail_bloom', scale: 1 },
            stereo_width: { key: 'stereo_width', scale: 100 },
            stereoWidth: { key: 'stereo_width', scale: 100 },
            early_late_mix: { key: 'early_late_mix', scale: 100 },
            earlyLateMix: { key: 'early_late_mix', scale: 100 },
            dry_wet_mix: { key: 'dry_wet_mix', scale: 100 },
            dryWetMix: { key: 'dry_wet_mix', scale: 100 },
            output_trim_db: { key: 'output_trim', scale: 1 },
            outputTrimDb: { key: 'output_trim', scale: 1 }
          };

          const entry = apvtsToKnob[data.id] || apvtsToKnob[data.apvtsId];
          if (entry) {
            const knob = this.knobs[entry.key];
            if (knob && typeof knob.setValue === 'function') {
              if (knob.isDragging) return; // Prevent stale host echo from overriding user's active drag
              knob.setValue(data.value * entry.scale, false);
            }
          }
        });

        this._scopeBufferL = new Float32Array(512);
        this._scopeBufferR = new Float32Array(512);
        backend.addEventListener('scopeFrame', (data) => {
          if (data && (data.samplesL || data.samples) && this.display) {
            const arrL = data.samplesL || data.samples;
            const arrR = data.samplesR || arrL;
            const len = Math.min(512, arrL.length);
            for (let i = 0; i < len; i++) {
              this._scopeBufferL[i] = arrL[i] || 0;
              this._scopeBufferR[i] = (arrR && typeof arrR[i] === 'number') ? arrR[i] : (arrL[i] || 0);
            }
            this.display.pushAudio(this._scopeBufferL.subarray(0, len), this._scopeBufferR.subarray(0, len));
          }
        });

        backend.addEventListener('telemetryFrame', (frame) => {
          if (frame && this.display) {
            this.display.pushTelemetry(frame.lowEnergy || 0, frame.midEnergy || 0, frame.highEnergy || 0);
          }
        });

        // Listen for recordingSaved event from JUCE C++ WAV recorder
        backend.addEventListener('recordingSaved', (payload) => {
          this._isJuceRecording = false;
          const recordBtn = document.getElementById('btn-record-wav');
          if (recordBtn) {
            recordBtn.classList.remove('is-recording');
            const recText = recordBtn.querySelector('.braun-rec-text');
            if (recText) recText.textContent = 'REC WAV';
          }
          if (payload && payload.path) {
            console.log('[JUCE] Lossless WAV recording saved to:', payload.path);
          }
        });

        try {
          backend.emitEvent('paramChange', { id: 'requestSync', value: 0 });
        } catch (e) {}
      }
    };

    if (window.__JUCE__?.backend) {
      setupBackend();
    } else {
      let attempts = 0;
      const poll = setInterval(() => {
        attempts++;
        if (window.__JUCE__?.backend) {
          clearInterval(poll);
          setupBackend();
        } else if (attempts >= 100) {
          clearInterval(poll);
        }
      }, 50);
      if (typeof poll.unref === 'function') {
        poll.unref();
      }
    }
  }

  _initLifecycleAudioUnlock() {
    const unlockEvents = ['touchstart', 'touchend', 'pointerdown', 'mousedown', 'keydown'];
    const doUnlock = async () => {
      if (this.isJuce) return;
      if (!this.engine.isInitialized) {
        await this.engine.init();
        if (this.engine.analyserL && this.display) {
          this.display.setAnalysers(this.engine.analyserL, this.engine.analyserR);
        }
        if (this.engine.masterLimiter) {
          this.wavRecorder = new MasterWavRecorder(this.engine.ctx, this.engine.masterLimiter);
        }
      }
      if (this.engine.ctx && this.engine.ctx.state === 'suspended') {
        try {
          await this.engine.ctx.resume();
        } catch (e) {}
      }
      if (this.engine.ctx && this.engine.ctx.state === 'running') {
        unlockEvents.forEach(evt => document.removeEventListener(evt, doUnlock, true));
      }
    };

    unlockEvents.forEach(evt => {
      document.addEventListener(evt, doUnlock, { capture: true, passive: true });
    });

    // Auto-recovery when tab gains focus
    const onVisibilityChange = async () => {
      if (this.isJuce) return;
      if (document.visibilityState === 'visible' && this.engine.ctx) {
        if (this.engine.ctx.state === 'suspended' || this.engine.ctx.state === 'interrupted') {
          try {
            await this.engine.ctx.resume();
          } catch (e) {}
        }
      }
    };

    document.addEventListener('visibilitychange', onVisibilityChange);
    window.addEventListener('pageshow', onVisibilityChange);
  }

  _initTheme() {
    const savedTheme = localStorage.getItem('braun_rb26_theme') || 'light';
    const themeSelect = document.getElementById('select-theme');

    if (savedTheme === 'dark') {
      document.documentElement.setAttribute('data-theme', 'dark');
      if (themeSelect) themeSelect.value = 'dark';
    } else {
      document.documentElement.removeAttribute('data-theme');
      if (themeSelect) themeSelect.value = 'light';
    }

    if (themeSelect) {
      themeSelect.addEventListener('change', (e) => {
        const theme = e.target.value;
        if (theme === 'dark') {
          document.documentElement.setAttribute('data-theme', 'dark');
          localStorage.setItem('braun_rb26_theme', 'dark');
        } else {
          document.documentElement.removeAttribute('data-theme');
          localStorage.setItem('braun_rb26_theme', 'light');
        }
      });
    }
  }

  _initDisplay() {
    const canvas = document.getElementById('scope-canvas');
    if (canvas) {
      this.display = new BraunCrtDisplay(canvas, { mode: 'EDC', isPowered: this.isPowered, isJuce: this.isJuce });
      this.display.start();

      this.engine.onTelemetry = (l, r, lowE, midE, highE) => {
        this.display.pushAudio(l, r);
        this.display.pushTelemetry(lowE, midE, highE);
      };

      const modeButtons = document.querySelectorAll('.braun-mode-btn');
      modeButtons.forEach((btn) => {
        btn.addEventListener('click', () => {
          modeButtons.forEach((b) => b.classList.remove('is-active'));
          btn.classList.add('is-active');
          const mode = btn.getAttribute('data-mode');
          this.display.setMode(mode);
        });
      });
    }
  }

  _initKnobs() {
    const createKnob = (id, options) => {
      const el = document.getElementById(id);
      if (!el) return null;
      return new BraunKnob(el, {
        ...options,
        onDragEnd: (val) => {
          this._flushJuceParams();
          if (typeof options.onDragEnd === 'function') options.onDragEnd(val);
        }
      });
    };

    // Deck 1: INPUT / PRE-DELAY
    this.knobs.predelay = createKnob('knob-predelay', {
      label: 'PRE-DELAY', min: 0, max: 250, step: 1, unit: 'ms', value: 24, size: 'small',
      onChange: (v) => { this.engine.setParam('preDelayMs', v); this._emitJuceParam('preDelayMs', v); }
    });

    this.knobs.diffusion = createKnob('knob-diffusion', {
      label: 'DIFFUSION', min: 0, max: 100, step: 1, unit: '%', value: 75, size: 'small',
      onChange: (v) => { this.engine.setParam('diffusion', v / 100); this._emitJuceParam('diffusionDensity', v / 100); }
    });

    this.knobs.input_trim = createKnob('knob-input-trim', {
      label: 'INPUT TRIM', min: -24, max: 12, step: 0.5, unit: 'dB', value: 0, size: 'small',
      onChange: (v) => { this.engine.setParam('inputTrimDb', v); this._emitJuceParam('inputTrimDb', v); }
    });

    // Deck 2: LOW-END MATRIX
    this.knobs.low_crossover = createKnob('knob-low-crossover', {
      label: 'LOW X-OVER', min: 60, max: 400, step: 1, unit: 'Hz', value: 180, size: 'medium',
      onChange: (v) => { this.engine.setParam('lowCrossoverHz', v); this._emitJuceParam('lowCrossoverHz', v); }
    });

    this.knobs.damping_low = createKnob('knob-damping-low', {
      label: 'BASS MULT', min: 0.20, max: 2.50, step: 0.05, unit: 'x', value: 1.00, size: 'medium',
      onChange: (v) => { this.engine.setParam('bassRt60Mult', v); this._emitJuceParam('bassRt60Mult', v); }
    });

    this.knobs.low_punch = createKnob('knob-low-punch', {
      label: 'PUNCH DUCK', min: 0, max: 100, step: 1, unit: '%', value: 65, size: 'medium',
      onChange: (v) => { this.engine.setParam('punchDucking', v / 100); this._emitJuceParam('punchDucking', v / 100); }
    });

    this.knobs.mono_bass = createKnob('knob-mono-bass', {
      label: 'SUB MONO', min: 40, max: 250, step: 1, unit: 'Hz', value: 120, size: 'small',
      onChange: (v) => { this.engine.setParam('subMonoHz', v); this._emitJuceParam('subMonoHz', v); }
    });

    // Deck 3: REVERB TANK / FDN
    this.knobs.rt60_decay = createKnob('knob-rt60-decay', {
      label: 'DECAY (RT60)', min: 0.3, max: 30.0, step: 0.1, unit: 's', value: 6.5, size: 'large', isLog: true,
      onChange: (v) => { this.engine.setParam('decayRt60Sec', v); this._emitJuceParam('decayRt60Sec', v); }
    });

    this.knobs.room_size = createKnob('knob-room-size', {
      label: 'ROOM SIZE', min: 10, max: 400, step: 1, unit: '%', value: 100, size: 'large',
      onChange: (v) => { this._updateRoomSize(v / 100); this._emitJuceParam('roomSize', v / 100); }
    });

    this.knobs.damping_high = createKnob('knob-damping-high', {
      label: 'HIGH DAMP', min: 1000, max: 20000, step: 50, unit: 'Hz', value: 1800, size: 'medium', isLog: true,
      onChange: (v) => { this.engine.setParam('highDampingHz', v); this._emitJuceParam('highDampingHz', v); }
    });

    // Deck 4: PITCH DIFFUSION
    this.knobs.shimmer_send = createKnob('knob-shimmer-send', {
      label: 'SHIMMER GAIN', min: 0, max: 100, step: 1, unit: '%', value: 40, size: 'medium',
      onChange: (v) => {
        const val = Math.max(0, v);
        this.engine.setParam('shimmerSend', val / 100);
        this._emitJuceParam('shimmerSend', val / 100);
      }
    });

    this.knobs.dimmer_send = createKnob('knob-dimmer-send', {
      label: 'DIMMER GAIN', min: 0, max: 100, step: 1, unit: '%', value: 35, size: 'medium',
      onChange: (v) => {
        const val = Math.max(0, v);
        this.engine.setParam('dimmerSend', val / 100);
        this._emitJuceParam('dimmerSend', val / 100);
      }
    });

    this.knobs.shimmer_dimmer_blend = createKnob('knob-shim-dim-blend', {
      label: 'SHIM / DIM', min: -100, max: 100, step: 1, unit: '%', value: 0, size: 'large',
      onChange: (v) => { this.engine.setParam('pitchBlend', v / 100); this._emitJuceParam('pitchBlend', v / 100); }
    });

    this.knobs.pitch_regen = createKnob('knob-pitch-regen', {
      label: 'PITCH REGEN', min: 0, max: 95, step: 1, unit: '%', value: 45, size: 'medium',
      onChange: (v) => { this.engine.setParam('pitchFeedback', v / 100); this._emitJuceParam('pitchFeedback', v / 100); }
    });

    // Deck 5: TAIL MODULATION
    this.knobs.tail_mod_rate = createKnob('knob-tail-mod-rate', {
      label: 'MOD RATE', min: 0.05, max: 4.00, step: 0.05, unit: 'Hz', value: 0.65, size: 'medium',
      onChange: (v) => {
        this.engine.setParam('tailModRateHz', v);
        this._emitJuceParam('tailModRateHz', v);
        if (this.vectorPad && !this.vectorPad.isEngaged) {
          const normX = Math.max(0, Math.min(1.0, Math.pow(Math.max(0, (v - 0.05) / 3.95), 1 / 1.6)));
          this.vectorPad.setCoordinates(normX, this.vectorPad.y, false);
        }
      }
    });

    this.knobs.tail_mod_depth = createKnob('knob-tail-mod-depth', {
      label: 'MOD DEPTH', min: 0, max: 100, step: 1, unit: '%', value: 45, size: 'medium',
      onChange: (v) => {
        this.engine.setParam('tailModDepthMs', (v / 100) * 3.0);
        this._emitJuceParam('tailModDepthMs', (v / 100) * 3.0);
        if (this.vectorPad && !this.vectorPad.isEngaged) {
          this.vectorPad.setCoordinates(this.vectorPad.x, v / 100, false);
        }
      }
    });

    this.knobs.tail_bloom = createKnob('knob-tail-bloom', {
      label: 'BLOOM ONSET', min: 20, max: 250, step: 5, unit: 'ms', value: 85, size: 'small',
      onChange: (v) => {
        this.engine.setParam('tailBloomMs', v);
        this._emitJuceParam('tailBloomMs', v);
        if (this.vectorPad && !this.vectorPad.isEngaged) {
          const normY = Math.max(0, Math.min(1.0, (v - 20) / 230));
          this.vectorPad.setCoordinates(this.vectorPad.x, normY, false);
        }
      }
    });

    // Deck 6: MASTER BUS
    this.knobs.stereo_width = createKnob('knob-stereo-width', {
      label: 'STEREO WIDTH', min: 0, max: 200, step: 1, unit: '%', value: 100, size: 'medium',
      onChange: (v) => { this.engine.setParam('stereoWidth', v / 100); this._emitJuceParam('stereoWidth', v / 100); }
    });

    this.knobs.early_late_mix = createKnob('knob-early-late-mix', {
      label: 'EARLY / LATE', min: 0, max: 100, step: 1, unit: '%', value: 50, size: 'medium',
      onChange: (v) => { this.engine.setParam('earlyLateMix', v / 100); this._emitJuceParam('earlyLateMix', v / 100); }
    });

    this.knobs.dry_wet_mix = createKnob('knob-dry-wet-mix', {
      label: 'DRY / WET', min: 0, max: 100, step: 1, unit: '%', value: 40, size: 'large',
      onChange: (v) => { this.engine.setParam('dryWetMix', v / 100); this._emitJuceParam('dryWetMix', v / 100); }
    });

    this.knobs.output_trim = createKnob('knob-output-trim', {
      label: 'OUTPUT TRIM', min: -24, max: 12, step: 0.5, unit: 'dB', value: 0, size: 'small',
      onChange: (v) => { this.engine.setParam('outputTrimDb', v); this._emitJuceParam('outputTrimDb', v); }
    });

    // Initialize A/B buffer with initial values
    this.abBuffer.init();
  }

  _initVectorPad() {
    const container = document.getElementById('vector-pad-container');
    if (!container) return;

    const initRate = 0.65;
    const initDepth = 45;
    const initX = Math.max(0, Math.min(1.0, Math.pow(Math.max(0, (initRate - 0.05) / 3.95), 1 / 1.6)));
    const initY = initDepth / 100;

    this.vectorPad = new BraunVectorPad(container, {
      defaultX: initX,
      defaultY: initY,
      onEngage: async () => {
        if (this.isJuce || !this.isPowered) return;
        if (!this.engine.isInitialized) await this.engine.init();
      },
      onChange: ({ rateHz, depthPct, bloomMs }) => {
        if (this.knobs.tail_mod_rate) this.knobs.tail_mod_rate.setValue(rateHz, false);
        if (this.knobs.tail_mod_depth) this.knobs.tail_mod_depth.setValue(depthPct, false);
        if (this.knobs.tail_bloom) this.knobs.tail_bloom.setValue(bloomMs, false);

        this.engine.setParam('tailModRateHz', rateHz);
        this.engine.setParam('tailModDepthMs', (depthPct / 100) * 3.0);
        this.engine.setParam('tailBloomMs', bloomMs);

        this._emitJuceParam('tailModRateHz', rateHz);
        this._emitJuceParam('tailModDepthMs', (depthPct / 100) * 3.0);
        this._emitJuceParam('tailBloomMs', bloomMs);
      }
    });
  }

  _initButtons() {
    // Power button
    const powerBtn = document.getElementById('btn-power');
    if (powerBtn) {
      if (this.isPowered) {
        powerBtn.classList.add('is-active');
        const statusText = powerBtn.querySelector('.braun-status-text');
        if (statusText) statusText.textContent = 'POWER ON';
      }
      powerBtn.addEventListener('click', async () => {
        await this.setPower(!this.isPowered);
      });
    }

    // Freeze / Hold rocker button
    const holdBtn = document.getElementById('btn-decay-hold');
    if (holdBtn) {
      holdBtn.addEventListener('click', () => {
        const isHeld = holdBtn.classList.toggle('is-active');
        const statusText = holdBtn.querySelector('.braun-status-text');
        if (statusText) statusText.textContent = isHeld ? 'FREEZE ON' : 'FREEZE OFF';
        this.engine.setParam('freezeHold', isHeld);
        this._emitJuceParam('freezeHold', isHeld ? 1.0 : 0.0, true);
        this._emitJuceParam('freeze_hold', isHeld ? 1.0 : 0.0, true);
      });
    }

    // Soft limiter rocker button
    const limiterBtn = document.getElementById('btn-soft-limiter');
    if (limiterBtn) {
      limiterBtn.addEventListener('click', () => {
        const isActive = limiterBtn.classList.toggle('is-active');
        this.engine.setParam('limiterEnable', isActive);
        this._emitJuceParam('limiterEnable', isActive ? 1.0 : 0.0);
        this._emitJuceParam('limiter_enable', isActive ? 1.0 : 0.0);
      });
    }

    // Shimmer Interval Segments
    const shimBtns = document.querySelectorAll('.shim-interval-btn');
    shimBtns.forEach((btn) => {
      btn.addEventListener('click', () => {
        shimBtns.forEach((b) => b.classList.remove('is-active'));
        btn.classList.add('is-active');
        const interval = parseInt(btn.getAttribute('data-interval'), 10);
        this.engine.setParam('shimmerInterval', interval);
        const choiceIdx = interval === 7 ? 0 : (interval === 24 ? 2 : 1);
        this._emitJuceParam('shimmerInterval', choiceIdx, true);
      });
    });

    // Dimmer Interval Segments
    const dimBtns = document.querySelectorAll('.dim-interval-btn');
    dimBtns.forEach((btn) => {
      btn.addEventListener('click', () => {
        dimBtns.forEach((b) => b.classList.remove('is-active'));
        btn.classList.add('is-active');
        const interval = parseInt(btn.getAttribute('data-interval'), 10);
        this.engine.setParam('dimmerInterval', interval);
        const choiceIdx = interval === -2 ? 0 : (interval === -7 ? 1 : 2);
        this._emitJuceParam('dimmerInterval', choiceIdx, true);
      });
    });

    // A/B Comparison Toggle and Copy
    const abToggleBtn = document.getElementById('btn-ab-toggle');
    if (abToggleBtn) {
      abToggleBtn.addEventListener('click', () => {
        this.abBuffer.toggle();
      });
    }

    const abCopyBtn = document.getElementById('btn-ab-copy');
    if (abCopyBtn) {
      abCopyBtn.addEventListener('click', () => {
        this.abBuffer.copyAToB();
      });
    }

    // WAV Recorder Button
    const recordBtn = document.getElementById('btn-record-wav');
    if (recordBtn) {
      recordBtn.addEventListener('click', async () => {
        if (!this.isPowered) {
          try {
            await this.setPower(true);
          } catch (err) {
            console.warn('setPower failed on record click:', err);
          }
        }

        if (this.isJuce) {
          this._isJuceRecording = !this._isJuceRecording;
          const recText = recordBtn.querySelector('.braun-rec-text');
          if (this._isJuceRecording) {
            recordBtn.classList.add('is-recording');
            if (recText) recText.textContent = 'STOP & SAVE';
            try {
              const backend = window.__JUCE__?.backend;
              if (backend && typeof backend.emitEvent === 'function') {
                backend.emitEvent('startRecording', {});
              }
            } catch (err) {
              console.warn('JUCE backend emitEvent startRecording error:', err);
            }
            this._emitJuceParam('startRecording', 1.0, true);
          } else {
            recordBtn.classList.remove('is-recording');
            if (recText) recText.textContent = 'REC WAV';
            try {
              const backend = window.__JUCE__?.backend;
              if (backend && typeof backend.emitEvent === 'function') {
                backend.emitEvent('stopRecording', {});
              }
            } catch (err) {
              console.warn('JUCE backend emitEvent stopRecording error:', err);
            }
            this._emitJuceParam('stopRecording', 1.0, true);
          }
          return;
        }

        if (!this.isJuce && !this.engine.isInitialized) {
          await this.engine.init();
          if (this.engine.masterLimiter) {
            this.wavRecorder = new MasterWavRecorder(this.engine.ctx, this.engine.masterLimiter);
          }
        }
        if (!this.wavRecorder && this.engine.masterLimiter) {
          this.wavRecorder = new MasterWavRecorder(this.engine.ctx, this.engine.masterLimiter);
        }

        if (!this.wavRecorder) return;

        const recText = recordBtn.querySelector('.braun-rec-text');
        if (!this.wavRecorder.isRecording) {
          this.wavRecorder.start();
          recordBtn.classList.add('is-recording');
          if (recText) recText.textContent = 'STOP & SAVE';
        } else {
          this.wavRecorder.stop();
          recordBtn.classList.remove('is-recording');
          if (recText) recText.textContent = 'REC WAV';
        }
      });
    }

    // Reset All Button
    const resetBtn = document.getElementById('btn-reset-all');
    if (resetBtn) {
      resetBtn.addEventListener('click', () => {
        this.loadPreset('DEFAULT');
      });
    }
  }

  // ==========================================================================
  // Patch Management & Drag-and-Drop
  // ==========================================================================
  _initPatchManagement() {
    // Preset dropdown listener
    const presetSelect = document.getElementById('select-preset');
    if (presetSelect) {
      presetSelect.addEventListener('change', (e) => {
        this.loadPreset(e.target.value);
        presetSelect.blur();
        if (document.activeElement && typeof document.activeElement.blur === 'function') {
          document.activeElement.blur();
        }
      });
    }

    // Populate user presets in dropdown
    this._populateUserPresetsDropdown();

    // Save Custom Patch
    const saveBtn = document.getElementById('btn-save-patch');
    if (saveBtn) {
      saveBtn.addEventListener('click', () => {
        const patchName = prompt('NAME FOR CUSTOM PATCH:', `USER SPACE ${Date.now().toString().slice(-4)}`);
        if (patchName) {
          this.saveUserPatch(patchName.trim());
        }
      });
    }

    // Export Patch as RFC 8259 JSON
    const exportBtn = document.getElementById('btn-export-patch');
    if (exportBtn) {
      exportBtn.addEventListener('click', () => {
        this.exportCurrentPatch();
      });
    }

    // Load Patch via File Input
    const loadBtn = document.getElementById('btn-load-patch');
    const fileInput = document.getElementById('input-load-patch');
    if (loadBtn && fileInput) {
      loadBtn.addEventListener('click', () => {
        fileInput.value = '';
        fileInput.click();
      });

      fileInput.addEventListener('change', (e) => {
        if (e.target.files && e.target.files[0]) {
          this.loadPatchFromFile(e.target.files[0]);
        }
      });
    }

    // Drag-and-Drop onto Chassis
    const chassis = document.getElementById('braun-chassis');
    if (chassis) {
      chassis.addEventListener('dragover', (e) => {
        e.preventDefault();
        chassis.classList.add('is-drag-over');
      });

      chassis.addEventListener('dragleave', (e) => {
        e.preventDefault();
        chassis.classList.remove('is-drag-over');
      });

      chassis.addEventListener('drop', (e) => {
        e.preventDefault();
        chassis.classList.remove('is-drag-over');
        if (e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0]) {
          this.loadPatchFromFile(e.dataTransfer.files[0]);
        }
      });
    }
  }

  getSnapshot() {
    const snap = {};
    for (const [key, knob] of Object.entries(this.knobs)) {
      if (knob && typeof knob.getValue === 'function') {
        snap[key] = knob.getValue();
      }
    }
    const holdBtn = document.getElementById('btn-decay-hold');
    if (holdBtn) snap.decay_hold = holdBtn.classList.contains('is-active');
    const limBtn = document.getElementById('btn-soft-limiter');
    if (limBtn) snap.soft_limiter = limBtn.classList.contains('is-active');
    return snap;
  }

  getUserPresets() {
    try {
      const stored = localStorage.getItem('BRAUN_RB26_USER_PRESETS');
      return stored ? JSON.parse(stored) : {};
    } catch (e) {
      return {};
    }
  }

  saveUserPatch(name) {
    const snap = this.getSnapshot();
    const userPresets = this.getUserPresets();
    const id = 'USER_' + Date.now();
    userPresets[id] = {
      id,
      name: name.toUpperCase(),
      isUser: true,
      timestamp: new Date().toISOString(),
      params: snap
    };
    try {
      localStorage.setItem('BRAUN_RB26_USER_PRESETS', JSON.stringify(userPresets));
    } catch (e) {}

    this._populateUserPresetsDropdown();
    this.loadPreset(id);
  }

  _populateUserPresetsDropdown() {
    const group = document.getElementById('user-presets-group');
    if (!group) return;
    group.innerHTML = '';

    const userPresets = this.getUserPresets();
    for (const [id, preset] of Object.entries(userPresets)) {
      const opt = document.createElement('option');
      opt.value = id;
      opt.textContent = preset.name || id;
      group.appendChild(opt);
    }
  }

  exportCurrentPatch() {
    const snap = this.getSnapshot();
    const patchData = {
      $schema: 'https://braun-audio.de/schemas/rb26-patch-v1.json',
      format: 'BRAUN_RB26_PATCH',
      version: 1,
      name: `BRAUN RB-26 Preset ${this.currentPresetKey}`,
      device: 'BRAUN_RB26',
      timestamp: new Date().toISOString(),
      theme: document.documentElement.getAttribute('data-theme') || 'light',
      params: snap
    };

    const jsonStr = JSON.stringify(patchData, null, 2);
    const blob = new Blob([jsonStr], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `BRAUN_RB26_${this.currentPresetKey}.json`;
    document.body.appendChild(a);
    a.click();
    setTimeout(() => {
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }, 1000);
  }

  loadPatchFromFile(file) {
    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const data = JSON.parse(e.target.result);
        if (data && data.params) {
          this.applyPatchParams(data.params, data.name || 'IMPORTED PATCH');
        } else if (data && typeof data === 'object') {
          this.applyPatchParams(data, 'IMPORTED PATCH');
        }
      } catch (err) {
        console.error('[BRAUN RB-26] Failed to load JSON patch:', err);
      }
    };
    reader.readAsText(file);
  }

  applyPatchParams(params, name) {
    for (const [key, val] of Object.entries(params)) {
      if (this.knobs[key]) {
        this.knobs[key].setValue(val, true);
      }
    }

    if (params.shimmer_interval !== undefined) {
      this.engine.setParam('shimmerInterval', params.shimmer_interval);
      const sIdx = params.shimmer_interval === 7 ? 0 : (params.shimmer_interval === 24 ? 2 : 1);
      this._emitJuceParam('shimmerInterval', sIdx);
    }

    if (params.dimmer_interval !== undefined) {
      this.engine.setParam('dimmerInterval', params.dimmer_interval);
      const dIdx = params.dimmer_interval === -2 ? 0 : (params.dimmer_interval === -7 ? 1 : 2);
      this._emitJuceParam('dimmerInterval', dIdx);
    }

    if (params.decay_hold !== undefined) {
      const holdBtn = document.getElementById('btn-decay-hold');
      if (holdBtn) {
        const isHold = Boolean(params.decay_hold);
        holdBtn.classList.toggle('is-active', isHold);
        const statusText = holdBtn.querySelector('.braun-status-text');
        if (statusText) statusText.textContent = isHold ? 'FREEZE ON' : 'FREEZE OFF';
        this.engine.setParam('freezeHold', isHold);
        this._emitJuceParam('freezeHold', isHold ? 1 : 0);
      }
    }

    if (params.soft_limiter !== undefined) {
      const limBtn = document.getElementById('btn-soft-limiter');
      if (limBtn) {
        const isLim = Boolean(params.soft_limiter);
        limBtn.classList.toggle('is-active', isLim);
        this.engine.setParam('limiterEnable', isLim);
        this._emitJuceParam('limiterEnable', isLim ? 1 : 0);
      }
    }
  }

  loadPreset(presetKey) {
    let preset = FACTORY_PRESETS[presetKey];
    if (!preset) {
      const userPresets = this.getUserPresets();
      preset = userPresets[presetKey];
    }
    if (!preset) return;

    this.currentPresetKey = presetKey;

    for (const [key, val] of Object.entries(preset.params)) {
      if (this.knobs[key]) {
        this.knobs[key].setValue(val, true);
      }
    }

    const presetSelect = document.getElementById('select-preset');
    if (presetSelect) {
      if (presetSelect.value !== presetKey) {
        presetSelect.value = presetKey;
      }
      presetSelect.blur();
    }

    if (this.vectorPad && preset.params) {
      const pRate = preset.params.tail_mod_rate ?? 0.65;
      const pDepth = preset.params.tail_mod_depth ?? 45;
      const px = Math.max(0, Math.min(1.0, Math.pow(Math.max(0, (pRate - 0.05) / 3.95), 1 / 1.6)));
      const py = Math.max(0, Math.min(1.0, pDepth / 100));
      this.vectorPad.setDefaults(px, py);
      this.vectorPad.setCoordinates(px, py, false);
    }

    if (preset.params.shimmer_interval !== undefined) {
      this.engine.setParam('shimmerInterval', preset.params.shimmer_interval);
      const sIdx = preset.params.shimmer_interval === 7 ? 0 : (preset.params.shimmer_interval === 24 ? 2 : 1);
      this._emitJuceParam('shimmerInterval', sIdx);
    }

    if (preset.params.dimmer_interval !== undefined) {
      this.engine.setParam('dimmerInterval', preset.params.dimmer_interval);
      const dIdx = preset.params.dimmer_interval === -2 ? 0 : (preset.params.dimmer_interval === -7 ? 1 : 2);
      this._emitJuceParam('dimmerInterval', dIdx);
    }

    const holdBtn = document.getElementById('btn-decay-hold');
    if (holdBtn && preset.params.decay_hold !== undefined) {
      const isHold = Boolean(preset.params.decay_hold);
      holdBtn.classList.toggle('is-active', isHold);
      const statusText = holdBtn.querySelector('.braun-status-text');
      if (statusText) statusText.textContent = isHold ? 'FREEZE ON' : 'FREEZE OFF';
      this.engine.setParam('freezeHold', isHold);
      this._emitJuceParam('freezeHold', isHold ? 1 : 0);
    }

    const limBtn = document.getElementById('btn-soft-limiter');
    if (limBtn && preset.params.soft_limiter !== undefined) {
      const isLim = Boolean(preset.params.soft_limiter);
      limBtn.classList.toggle('is-active', isLim);
      this.engine.setParam('limiterEnable', isLim);
      this._emitJuceParam('limiterEnable', isLim ? 1 : 0);
    }
  }

  // ==========================================================================
  // Audition Bar Triggers
  // ==========================================================================
  _initAuditionBar() {
    const attachTrigger = (id, fn) => {
      const el = document.getElementById(id);
      if (el) {
        el.addEventListener('click', async () => {
          if (!this.isPowered) return;
          if (!this.isJuce && !this.engine.isInitialized) {
            await this.engine.init();
          }
          fn.call(this);
        });
      }
    };

    attachTrigger('btn-audition-impulse', () => this.triggerDirac());
    attachTrigger('btn-audition-kick', () => this.triggerHammerThud());
    attachTrigger('btn-audition-snare', () => this.triggerPinkBurst(40));
    attachTrigger('btn-audition-piano', () => this.playChord(0));
    attachTrigger('btn-audition-pad', () => this.togglePoisson());
  }

  // ==========================================================================
  // Deck 07 Onboard Exciter Engine & Performance Controller
  // ==========================================================================
  _initExciterDeck() {
    // Scale & Root pitch class selectors
    const scaleSelect = document.getElementById('select-chime-scale');
    if (scaleSelect) {
      scaleSelect.addEventListener('change', (e) => {
        this.currentScaleKey = e.target.value;
        this._updateChimeKeyLabels();
        scaleSelect.blur();
        if (document.activeElement && typeof document.activeElement.blur === 'function') {
          document.activeElement.blur();
        }
      });
    }

    const rootSelect = document.getElementById('select-chime-root');
    if (rootSelect) {
      rootSelect.addEventListener('change', (e) => {
        this.rootPitchClass = parseInt(e.target.value, 10);
        this._updateChimeKeyLabels();
        rootSelect.blur();
        if (document.activeElement && typeof document.activeElement.blur === 'function') {
          document.activeElement.blur();
        }
      });
    }

    // Laboratory Pulse Buttons
    const diracBtn = document.getElementById('btn-pulse-dirac');
    if (diracBtn) diracBtn.addEventListener('click', () => { if (this.isPowered) this.triggerDirac(); });

    const pinkBtn = document.getElementById('btn-pulse-pink');
    if (pinkBtn) pinkBtn.addEventListener('click', () => { if (this.isPowered) this.triggerPinkBurst(40); });

    const hammerBtn = document.getElementById('btn-pulse-hammer');
    if (hammerBtn) hammerBtn.addEventListener('click', () => { if (this.isPowered) this.triggerHammerThud(); });

    // Poisson Controls
    const poissonBtn = document.getElementById('btn-poisson-toggle');
    if (poissonBtn) poissonBtn.addEventListener('click', () => { if (this.isPowered || this.isPoissonRunning) this.togglePoisson(); });

    const rateSlider = document.getElementById('input-poisson-rate');
    const rateVal = document.getElementById('val-poisson-rate');
    if (rateSlider && rateVal) {
      rateSlider.addEventListener('input', (e) => {
        this.poissonEpm = parseFloat(e.target.value);
        rateVal.textContent = Math.round(this.poissonEpm);
        if (this.isJuce && this.isPoissonRunning) {
          this._emitJuceExciter({ type: 'poisson', enable: true, epm: this.poissonEpm, humanize: this.poissonHumanize });
        }
      });
    }

    const humanSlider = document.getElementById('input-poisson-humanize');
    const humanVal = document.getElementById('val-poisson-humanize');
    if (humanSlider && humanVal) {
      humanSlider.addEventListener('input', (e) => {
        this.poissonHumanize = parseFloat(e.target.value) / 100;
        humanVal.textContent = Math.round(e.target.value);
        if (this.isJuce && this.isPoissonRunning) {
          this._emitJuceExciter({ type: 'poisson', enable: true, epm: this.poissonEpm, humanize: this.poissonHumanize });
        }
      });
    }

    // Strum speed buttons
    const speedBtns = document.querySelectorAll('.chord-speed-btn');
    speedBtns.forEach((btn) => {
      btn.addEventListener('click', () => {
        speedBtns.forEach((b) => b.classList.remove('is-active'));
        btn.classList.add('is-active');
        this.chordSpeed = btn.getAttribute('data-speed') || 'med';
      });
    });

    // 12 Signature Chord Buttons: tactile pointer sustain & release + click fallback
    const chordBtns = document.querySelectorAll('.braun-chord-btn');
    this._activeChordButtons = this._activeChordButtons || new Map();

    chordBtns.forEach((btn) => {
      const chordIndex = parseInt(btn.getAttribute('data-chord-index'), 10);
      const hotkey = btn.getAttribute('data-hotkey') || '';

      const triggerChordDown = async () => {
        if (!this.isPowered) return;
        if (btn._suppressTimer) {
          clearTimeout(btn._suppressTimer);
          btn._suppressTimer = null;
        }
        btn._pointerActive = true;
        btn._suppressNextClick = true;
        btn.classList.add('is-active');

        if (!this.isJuce && !this.engine.isInitialized) {
          await this.engine.init();
        }

        // If pointer was released before initialization resolved, do not start voice
        if (!btn._pointerActive) return;

        // Release any currently active voice on this button before re-triggering
        if (this._activeChordButtons.has(btn)) {
          const oldVoice = this._activeChordButtons.get(btn);
          if (oldVoice && typeof oldVoice.release === 'function') oldVoice.release(0.28);
          this._activeChordButtons.delete(btn);
        }
        if (this._activeVoices && this._activeVoices.has(hotkey)) {
          const oldKeyVoice = this._activeVoices.get(hotkey);
          const oldVoice = oldKeyVoice?.voice || (typeof oldKeyVoice?.release === 'function' ? oldKeyVoice : null);
          if (oldVoice && typeof oldVoice.release === 'function') {
            try { oldVoice.release(0.28); } catch (_) {}
          }
          this._activeVoices.delete(hotkey);
        }

        const voice = this.playChord(chordIndex);
        if (voice) {
          this._activeChordButtons.set(btn, voice);
        }

        // Prevent button from holding spacebar keyboard focus
        if (typeof btn.blur === 'function') btn.blur();
      };

      const triggerChordUp = () => {
        btn._pointerActive = false;
        btn._suppressNextClick = true;
        if (btn._suppressTimer) {
          clearTimeout(btn._suppressTimer);
        }
        btn._suppressTimer = setTimeout(() => {
          btn._suppressNextClick = false;
          btn._suppressTimer = null;
        }, 300);

        if (!this._heldKeys || !this._heldKeys.has(hotkey)) {
          btn.classList.remove('is-active');
        }

        if (this._activeChordButtons.has(btn)) {
          const voice = this._activeChordButtons.get(btn);
          if (voice && typeof voice.release === 'function') {
            voice.release(0.28);
          }
          this._activeChordButtons.delete(btn);
        }
      };

      btn.addEventListener('pointerdown', triggerChordDown);
      btn.addEventListener('pointerup', triggerChordUp);
      btn.addEventListener('pointerleave', () => {
        if (btn._pointerActive) triggerChordUp();
      });
      btn.addEventListener('pointercancel', () => {
        if (btn._pointerActive) triggerChordUp();
      });

      btn.addEventListener('click', (e) => {
        // Suppress duplicate click event dispatched by browser immediately following pointerup
        if (btn._suppressNextClick) {
          btn._suppressNextClick = false;
          if (btn._suppressTimer) {
            clearTimeout(btn._suppressTimer);
            btn._suppressTimer = null;
          }
          e.preventDefault();
          e.stopPropagation();
          return;
        }

        // Programmatic click fallback (e.g. automated test suites, accessibility tools)
        if (!this.isPowered) return;
        btn.classList.add('is-active');
        const voice = this.playChord(chordIndex);
        setTimeout(() => {
          if (!btn._pointerActive && (!this._heldKeys || !this._heldKeys.has(hotkey))) {
            btn.classList.remove('is-active');
          }
          if (voice && typeof voice.release === 'function') {
            voice.release(0.28);
          }
        }, 250);
        if (typeof btn.blur === 'function') btn.blur();
      });
    });

    // 11-Key Chime Strip: Click, Touch Velocity, and Glissando
    this._initChimeStrip();
    this._updateChimeKeyLabels();

    // Track active held keys to prevent machine-gun repeat bursts and support clean sustain
    this._heldKeys = new Set();

    const isPlayableMusicalKey = (e) => {
      if (!e) return false;
      const key = typeof e === 'string' ? e.toLowerCase() : (e.key ? e.key.toLowerCase() : '');
      const code = (typeof e === 'object' && e.code) ? e.code : '';
      return getChimeKeyIndex({ code, key }) !== null ||
             getChordKeyIndex({ code, key }) !== null ||
             key === ' ' || code === 'Space';
    };

    // Keyboard Shortcuts (A-' for chimes, 1-= for chords)
    // Use capture phase to intercept BEFORE focused select/button elements process keystrokes
    window.addEventListener('keydown', (e) => {
      // Don't hijack text entry fields if user is typing custom patch name
      const target = e.target;
      if (target && target.tagName === 'INPUT' && (target.type === 'text' || target.type === 'search' || !target.type)) return;

      if (!isPlayableMusicalKey(e)) return;

      // Immediately blur any select, button, or active element so typing triggers notes cleanly
      const activeEl = document.activeElement;
      if (activeEl && typeof activeEl.blur === 'function' && activeEl !== document.body) {
        activeEl.blur();
      }
      if (target && typeof target.blur === 'function' && target !== document.body) {
        target.blur();
      }

      e.preventDefault();
      e.stopPropagation();

      const keyId = e.code || (e.key ? e.key.toLowerCase() : '');
      // Ignore browser typematic auto-repeat events so sustained notes hold cleanly rather than stuttering at OS repeat speed
      if (e.repeat || (this._heldKeys && (this._heldKeys.has(keyId) || (e.code && this._heldKeys.has(e.code)) || (e.key && this._heldKeys.has(e.key.toLowerCase()))))) {
        return;
      }

      this._heldKeys.add(keyId);
      if (e.code) this._heldKeys.add(e.code);
      if (e.key) this._heldKeys.add(e.key.toLowerCase());
      this._handleKeyboardShortcuts(e);
    }, { capture: true });

    window.addEventListener('keyup', (e) => {
      const target = e.target;
      if (target && target.tagName === 'INPUT' && (target.type === 'text' || target.type === 'search' || !target.type)) return;

      if (isPlayableMusicalKey(e)) {
        e.preventDefault();
        e.stopPropagation();
      }

      const keyId = e.code || (e.key ? e.key.toLowerCase() : '');
      if (this._heldKeys) {
        this._heldKeys.delete(keyId);
        if (e.code) this._heldKeys.delete(e.code);
        if (e.key) this._heldKeys.delete(e.key.toLowerCase());
      }
      this._handleKeyboardKeyUp(e);
    }, { capture: true });

    // Safety release: clean up sustained voices and visual states if window loses focus
    window.addEventListener('blur', () => {
      if (this._activeVoices) {
        this._activeVoices.forEach((entry) => {
          const v = entry?.voice || (typeof entry?.release === 'function' ? entry : null);
          if (v && typeof v.release === 'function') {
            try { v.release(0.28); } catch (_) {}
          }
        });
        this._activeVoices.clear();
      }
      if (this._activeChordButtons) {
        this._activeChordButtons.forEach((voice, btn) => {
          if (voice && typeof voice.release === 'function') {
            try { voice.release(0.28); } catch (_) {}
          }
          if (btn && btn.classList) btn.classList.remove('is-active');
        });
        this._activeChordButtons.clear();
      }
      if (this._heldKeys) {
        this._heldKeys.clear();
      }
      const activeEls = document.querySelectorAll('.braun-chime-key.is-active, .braun-chord-btn.is-active, #btn-audition-impulse.is-active, #btn-pulse-dirac.is-active');
      activeEls.forEach((el) => el.classList.remove('is-active'));
    });
  }

  _updateChimeKeyLabels() {
    const scale = SCALES[this.currentScaleKey] || SCALES.BUDD_PENTATONIC;
    const intervals = scale.intervals;
    const chimeKeys = document.querySelectorAll('.braun-chime-key');
    const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

    chimeKeys.forEach((keyEl, idx) => {
      const midi = getChimeMidiForDegree(idx, this.rootPitchClass, intervals);
      const noteName = noteNames[midi % 12];
      const octave = Math.floor(midi / 12) - 1;
      const noteSpan = keyEl.querySelector('.braun-key-note');
      if (noteSpan) noteSpan.textContent = `${noteName}${octave}`;
      keyEl.setAttribute('data-midi', midi);
      keyEl.setAttribute('data-note-offset', midi - 60);
    });
  }

  _initChimeStrip() {
    const strip = document.getElementById('chime-strip');
    if (!strip) return;

    let isPointerActive = false;
    let activeKeyEl = null;

    const getKeyAtPoint = (x, y) => {
      const el = document.elementFromPoint(x, y);
      return el ? el.closest('.braun-chime-key') : null;
    };

    const triggerKey = async (keyEl, clientY) => {
      if (!this.isPowered) return;
      if (!this.isJuce && !this.engine.isInitialized) await this.engine.init();
      if (!keyEl) return;

      const hotkey = keyEl.getAttribute('data-hotkey') || '';
      if (hotkey && this._activeVoices && this._activeVoices.has(hotkey)) {
        const oldKeyVoice = this._activeVoices.get(hotkey);
        const oldVoice = oldKeyVoice?.voice || (typeof oldKeyVoice?.release === 'function' ? oldKeyVoice : null);
        if (oldVoice && typeof oldVoice.release === 'function') {
          try { oldVoice.release(0.28); } catch (_) {}
        }
        this._activeVoices.delete(hotkey);
      }

      const rect = keyEl.getBoundingClientRect();
      const relY = Math.max(0, Math.min(1, (clientY - rect.top) / rect.height));
      // Continuous velocity: higher on key = softer (0.35), lower = firmer (0.85)
      const velocity = 0.35 + 0.50 * relY;

      const keyIdx = parseInt(keyEl.getAttribute('data-key-index'), 10);
      const scale = SCALES[this.currentScaleKey] || SCALES.BUDD_PENTATONIC;
      const fallbackMidi = !isNaN(keyIdx)
        ? getChimeMidiForDegree(keyIdx, this.rootPitchClass, scale.intervals)
        : (keyEl.hasAttribute('data-note-offset') ? (60 + parseFloat(keyEl.getAttribute('data-note-offset'))) : 60);
      const midi = parseFloat(keyEl.getAttribute('data-midi')) || fallbackMidi;
      const voice = this.playChime(midi, velocity, 3.5, false);
      if (voice && hotkey && this._activeVoices) {
        this._activeVoices.set(hotkey, { voice, key: hotkey });
      }

      keyEl.classList.add('is-active');
      setTimeout(() => {
        if (!this._heldKeys || !this._heldKeys.has(hotkey)) {
          keyEl.classList.remove('is-active');
        }
      }, 140);
    };

    const onPointerDown = (e) => {
      isPointerActive = true;
      const keyEl = getKeyAtPoint(e.clientX, e.clientY);
      if (keyEl) {
        activeKeyEl = keyEl;
        triggerKey(keyEl, e.clientY);
      }
    };

    const onPointerMove = (e) => {
      if (!isPointerActive) return;
      const keyEl = getKeyAtPoint(e.clientX, e.clientY);
      if (keyEl && keyEl !== activeKeyEl) {
        activeKeyEl = keyEl;
        triggerKey(keyEl, e.clientY);
      }
    };

    const onPointerUp = () => {
      isPointerActive = false;
      activeKeyEl = null;
    };

    strip.addEventListener('pointerdown', onPointerDown);
    window.addEventListener('pointermove', onPointerMove);
    window.addEventListener('pointerup', onPointerUp);
    window.addEventListener('pointercancel', onPointerUp);
  }

  _handleKeyboardKeyUp(e) {
    const code = e.code;
    const key = e.key ? e.key.toLowerCase() : '';

    if (key === ' ' || code === 'Space') {
      const impulseBtns = document.querySelectorAll('#btn-audition-impulse, #btn-pulse-dirac');
      impulseBtns.forEach((btn) => btn.classList.remove('is-active'));
      return;
    }

    // Release any active acoustic voice smoothly using piano felt damping (0.35s)
    let foundId = null;
    let foundEntry = null;
    if (this._activeVoices) {
      for (const [id, entry] of this._activeVoices.entries()) {
        const matchId = (id === code || id === key || (typeof id === 'string' && id.toLowerCase() === key));
        const matchCode = (code && entry && entry.code === code);
        const matchKey = (key && entry && (entry.key === key || (typeof entry.key === 'string' && entry.key.toLowerCase() === key)));
        if (matchId || matchCode || matchKey) {
          foundId = id;
          foundEntry = entry;
          break;
        }
      }
    }

    if (foundEntry && foundId !== null) {
      this._activeVoices.delete(foundId);
      const voice = foundEntry.voice || (typeof foundEntry.release === 'function' ? foundEntry : null);
      if (voice && typeof voice.release === 'function') {
        voice.release(0.35);
      }
      if (foundEntry.chimeIdx !== undefined && foundEntry.chimeIdx !== null) {
        const chimeKeys = document.querySelectorAll('.braun-chime-key');
        const keyEl = chimeKeys[foundEntry.chimeIdx];
        if (keyEl) keyEl.classList.remove('is-active');
      }
      if (foundEntry.chordIdx !== undefined && foundEntry.chordIdx !== null) {
        const chordBtns = document.querySelectorAll('.braun-chord-btn');
        const chordBtn = chordBtns[foundEntry.chordIdx];
        if (chordBtn && !chordBtn._pointerActive) chordBtn.classList.remove('is-active');
      }
    }

    const chimeIdx = getChimeKeyIndex(e);
    if (chimeIdx !== null) {
      const chimeKeys = document.querySelectorAll('.braun-chime-key');
      const keyEl = chimeKeys[chimeIdx];
      if (keyEl) {
        keyEl.classList.remove('is-active');
      }
      return;
    }

    const chordIdx = getChordKeyIndex(e);
    if (chordIdx !== null) {
      const chordBtns = document.querySelectorAll('.braun-chord-btn');
      const chordBtn = chordBtns[chordIdx];
      if (chordBtn && !chordBtn._pointerActive) {
        chordBtn.classList.remove('is-active');
      }
    }
  }

  _handleKeyboardShortcuts(e) {
    if (!this.isPowered) return;
    const code = e.code;
    const key = e.key ? e.key.toLowerCase() : '';

    // Spacebar: Dirac Impulse Pulse
    if (key === ' ' || code === 'Space') {
      this.triggerDirac();
      return;
    }

    // Chime hotkeys: A-' via getChimeKeyIndex
    const chimeIdx = getChimeKeyIndex(e);
    if (chimeIdx !== null) {
      const chimeKeys = document.querySelectorAll('.braun-chime-key');
      const keyEl = chimeKeys[chimeIdx];
      if (keyEl) {
        const scale = SCALES[this.currentScaleKey] || SCALES.BUDD_PENTATONIC;
        const midi = parseFloat(keyEl.getAttribute('data-midi')) || getChimeMidiForDegree(chimeIdx, this.rootPitchClass, scale.intervals);
        const keyId = code || key || `chime-${chimeIdx}`;
        if (this._activeVoices && this._activeVoices.has(keyId)) {
          const old = this._activeVoices.get(keyId);
          const oldVoice = old?.voice || (typeof old?.release === 'function' ? old : null);
          if (oldVoice && typeof oldVoice.release === 'function') oldVoice.release(0.28);
          this._activeVoices.delete(keyId);
        }
        const voice = this.playChime(midi, 0.45, 3.5, true);
        if (voice && this._activeVoices) {
          this._activeVoices.set(keyId, { voice, code, key, chimeIdx });
        }
        keyEl.classList.add('is-active');
      }
      return;
    }

    // Chord hotkeys: 1 through 9, 0, -, = via getChordKeyIndex
    const chordIdx = getChordKeyIndex(e);
    if (chordIdx !== null) {
      const chordBtns = document.querySelectorAll('.braun-chord-btn');
      const chordBtn = chordBtns[chordIdx];
      const keyId = code || key || `chord-${chordIdx}`;
      if (this._activeVoices && this._activeVoices.has(keyId)) {
        const old = this._activeVoices.get(keyId);
        const oldVoice = old?.voice || (typeof old?.release === 'function' ? old : null);
        if (oldVoice && typeof oldVoice.release === 'function') oldVoice.release(0.28);
        this._activeVoices.delete(keyId);
      }
      if (chordBtn && this._activeChordButtons && this._activeChordButtons.has(chordBtn)) {
        const oldBtnVoice = this._activeChordButtons.get(chordBtn);
        if (oldBtnVoice && typeof oldBtnVoice.release === 'function') oldBtnVoice.release(0.28);
        this._activeChordButtons.delete(chordBtn);
      }
      const voice = this.playChord(chordIdx, true);
      if (voice && this._activeVoices) {
        this._activeVoices.set(keyId, { voice, code, key, chordIdx });
      }
      if (chordBtn) {
        chordBtn.classList.add('is-active');
      }
    }
  }

  // ==========================================================================
  // Audio Synthesis for Chimes, Pulses, Chords & Poisson
  // ==========================================================================

  /**
   * Play chime key by sequential scale degree index (0 to 10)
   */
  playChimeKey(degreeIndex, velocity = 0.70, durationSec = 3.5) {
    const scale = SCALES[this.currentScaleKey] || SCALES.BUDD_PENTATONIC;
    const midi = getChimeMidiForDegree(degreeIndex, this.rootPitchClass, scale.intervals);
    return this.playChime(midi, velocity, durationSec);
  }

  /**
   * Harold Budd Felt Piano / Acoustic Modeling (AS-42 Heritage)
   * Emulates soft felt hammer impact (warm wooden soundboard thump, Brownian lowpass felt texture, 26ms decay),
   * resonant spruce soundboard formant filter (480–610 Hz peaking filter, Q=1.2, +2dB),
   * steep una corda lowpass damping (dual cascaded 24dB/oct biquads decaying in ~0.18s),
   * dual micro-detuned acoustic string pair, and smooth dynamic decay envelope.
   */
  playChime(midiNote, velocity = 0.70, durationSec = 3.5, isHold = false, isChord = false) {
    if (!this.isPowered) return;

    if (this.isJuce && typeof window !== 'undefined' && window.__JUCE__?.backend?.emitEvent) {
      try {
        window.__JUCE__.backend.emitEvent('exciterTrigger', {
          type: 'note',
          midi: midiNote,
          velocity: velocity,
          duration: durationSec
        });
      } catch (e) {}
    }

    if (!this.engine.ctx || !this.engine.inputGain) return;
    const ctx = this.engine.ctx;
    const now = ctx.currentTime;
    const f0 = 440 * Math.pow(2, (midiNote - 69) / 12);

    // Register modeling matching AS-42 felt-piano.js
    const isBass = midiNote < 48;
    const isTreble = midiNote >= 72;
    const bodyFormantHz = isBass
      ? Math.max(280, Math.min(420, 300 + (midiNote - 24) * 5))
      : (isTreble ? Math.min(950, 680 + (midiNote - 72) * 12) : (480 + (midiNote - 48) * 5.5));
    const hammerCutoff = isBass
      ? Math.min(220, Math.max(110, f0 * 1.3))
      : (isTreble ? Math.min(750, Math.max(550, f0 * 0.70)) : Math.min(420, Math.max(220, f0 * 1.1)));
    const chordScale = isChord ? 0.40 : 1.0;
    const hammerThumpGain = (isBass ? 0.09 : (isTreble ? 0.055 : 0.07)) * velocity * chordScale;
    const thumpDuration = isBass ? 0.032 : (isTreble ? 0.018 : 0.026);
    const filterDecayBase = isBass ? 0.26 : (isTreble ? 0.12 : 0.18);
    const maxCutoff = Math.min(7500, Math.max(f0 * 1.8, 420 + 2600 * velocity));
    const restCutoff = Math.min(2200, Math.max(160, f0 * 1.15));

    const peakGain = velocity * 0.22;
    const attackTime = 0.008;
    const sustainLevel = Math.max(0.002, peakGain * 0.45);
    const sustainTarget = now + attackTime + 0.35;

    // Master Voice Gain with 8ms anti-click attack and smooth exponential decay
    const voiceGain = ctx.createGain();
    voiceGain.gain.setValueAtTime(0.0, now);
    voiceGain.gain.linearRampToValueAtTime(velocity * 0.22, now + 0.008);
    if (isHold) {
      voiceGain.gain.exponentialRampToValueAtTime(sustainLevel, sustainTarget);
    } else {
      voiceGain.gain.exponentialRampToValueAtTime(0.0001, now + durationSec);
      voiceGain.gain.linearRampToValueAtTime(0.0, now + durationSec + 0.05);
    }

    // Spruce Soundboard Resonant Peaking Formant Filter (~480–610 Hz, Q=1.2, +2dB)
    const bodyFilter = ctx.createBiquadFilter();
    bodyFilter.type = 'peaking';
    bodyFilter.frequency.setValueAtTime(bodyFormantHz, now);
    bodyFilter.Q.setValueAtTime(1.2, now);
    bodyFilter.gain.setValueAtTime(2.0, now);

    // Steep Una Corda Damping: Dual cascaded 12dB lowpasses = 24dB/oct steep damping
    const filter1 = ctx.createBiquadFilter();
    const filter2 = ctx.createBiquadFilter();
    filter1.type = 'lowpass';
    filter2.type = 'lowpass';
    filter1.Q.setValueAtTime(Math.SQRT1_2, now); // Butterworth maximally flat response (Q = 1 / sqrt(2))
    filter2.Q.setValueAtTime(Math.SQRT1_2, now);
    filter1.frequency.setValueAtTime(maxCutoff, now);
    filter2.frequency.setValueAtTime(maxCutoff, now);
    filter1.frequency.exponentialRampToValueAtTime(restCutoff, now + filterDecayBase);
    filter2.frequency.exponentialRampToValueAtTime(restCutoff, now + filterDecayBase);

    // Soft Saturation / Warmth Stage (smooth cubic soft clip curve)
    const saturationShaper = ctx.createWaveShaper();
    if (!BraunRb26App._softClipCurve) {
      const n = 512;
      const curve = new Float32Array(n);
      for (let i = 0; i < n; i++) {
        const x = (i / (n - 1)) * 2 - 1;
        curve[i] = Math.tanh(1.2 * x) / Math.tanh(1.2);
      }
      BraunRb26App._softClipCurve = curve;
    }
    saturationShaper.curve = BraunRb26App._softClipCurve;

    // Harmonic Felt String Modes: Fundamental, sympathetic detuned pair, octave, 3rd harmonic
    const stringMixer = ctx.createGain();
    stringMixer.gain.setValueAtTime(1.0, now);

    // Osc 1: Fundamental Sine (-0.8 cents detune)
    const osc1 = ctx.createOscillator();
    osc1.type = 'sine';
    osc1.frequency.setValueAtTime(f0, now);
    osc1.detune.setValueAtTime(-0.8, now);
    const osc1Gain = ctx.createGain();
    osc1Gain.gain.setValueAtTime(0.52, now);
    osc1.connect(osc1Gain);
    osc1Gain.connect(stringMixer);
    osc1.start(now);

    // Osc 2: Sympathetic String Sine (+1.4 cents detune)
    const osc2 = ctx.createOscillator();
    osc2.type = 'sine';
    osc2.frequency.setValueAtTime(f0, now);
    osc2.detune.setValueAtTime(1.4, now);
    const osc2Gain = ctx.createGain();
    osc2Gain.gain.setValueAtTime(0.22, now);
    osc2.connect(osc2Gain);
    osc2Gain.connect(stringMixer);
    osc2.start(now);

    // Osc 3: Octave Harmonic (+2.0 cents detune, faster decay)
    const osc3 = ctx.createOscillator();
    osc3.type = 'sine';
    osc3.frequency.setValueAtTime(f0 * 2, now);
    osc3.detune.setValueAtTime(2.0, now);
    const osc3Gain = ctx.createGain();
    osc3Gain.gain.setValueAtTime(0.08 * velocity, now);
    osc3Gain.gain.exponentialRampToValueAtTime(0.0001, now + Math.min(durationSec, 1.8));
    osc3.connect(osc3Gain);
    osc3Gain.connect(stringMixer);
    osc3.start(now);

    // Acoustic Felt Hammer Transient (Warm wooden soundboard thump + velvety Brownian low-pass felt texture)
    // Precomputed 40ms acoustic buffer eliminates typewriter click and per-note buffer allocation
    const thumpSamples = Math.max(128, Math.floor(ctx.sampleRate * 0.040));
    if (!BraunRb26App._hammerBuffer || BraunRb26App._hammerBufferSampleRate !== ctx.sampleRate) {
      const hammerBuf = ctx.createBuffer(1, thumpSamples, ctx.sampleRate);
      const hd = hammerBuf.getChannelData(0);

      // 1. Generate low-passed velvety felt noise texture (Brownian / 2-pole lowpass)
      let rngState = 1337;
      const nextRand = () => {
        rngState = (rngState * 1664525 + 1013904223) >>> 0;
        return (rngState / 4294967296) * 2 - 1;
      };

      const rawNoise = new Float32Array(thumpSamples);
      let pole1 = 0;
      for (let i = 0; i < thumpSamples; i++) {
        pole1 = pole1 * 0.82 + nextRand() * 0.18;
        rawNoise[i] = pole1;
      }
      let pole2 = 0;
      for (let i = 0; i < thumpSamples; i++) {
        pole2 = pole2 * 0.82 + rawNoise[i] * 0.18;
        rawNoise[i] = pole2;
      }

      // 2. Synthesize warm wooden body modal impulse (damped ~135 Hz soundboard knock)
      // Blended with low-passed felt compression texture
      for (let i = 0; i < thumpSamples; i++) {
        const t = i / ctx.sampleRate;
        const woodThump = Math.sin(2 * Math.PI * 135 * t) * Math.exp(-t / 0.010);
        hd[i] = 0.65 * woodThump + 0.35 * rawNoise[i];
      }

      // 3. Windowed exponential decay with smooth Hann attack & release windows
      const attackSamples = Math.max(2, Math.floor(ctx.sampleRate * 0.0050));
      const releaseSamples = Math.max(2, Math.floor(ctx.sampleRate * 0.0060));
      const releaseStart = thumpSamples - releaseSamples;

      for (let i = 0; i < thumpSamples; i++) {
        let s = hd[i] * Math.exp(-i / (ctx.sampleRate * 0.0090));
        if (i < attackSamples) {
          s *= 0.5 * (1 - Math.cos((Math.PI * i) / attackSamples));
        } else if (i >= releaseStart) {
          const relIdx = i - releaseStart;
          s *= 0.5 * (1 + Math.cos((Math.PI * relIdx) / releaseSamples));
        }
        hd[i] = s;
      }

      // 4. Zero-boundary DC removal using sin^2(pi * i / (N - 1))
      let sumD = 0;
      let sumW = 0;
      const weights = new Float32Array(thumpSamples);
      for (let i = 0; i < thumpSamples; i++) {
        sumD += hd[i];
        const sinVal = Math.sin((Math.PI * i) / (thumpSamples - 1));
        const w = sinVal * sinVal;
        weights[i] = w;
        sumW += w;
      }
      const dcOffset = sumW > 0 ? sumD / sumW : 0;
      for (let i = 0; i < thumpSamples; i++) {
        hd[i] -= dcOffset * weights[i];
      }
      hd[0] = 0.0;
      hd[thumpSamples - 1] = 0.0;

      BraunRb26App._hammerBuffer = hammerBuf;
      BraunRb26App._hammerBufferSampleRate = ctx.sampleRate;
    }

    const noiseSrc = ctx.createBufferSource();
    noiseSrc.buffer = BraunRb26App._hammerBuffer;

    // Filter hammer through warm lowpass filter (Q = 0.85) instead of harsh resonant bandpass
    const hammerFilter = ctx.createBiquadFilter();
    hammerFilter.type = 'lowpass';
    hammerFilter.frequency.setValueAtTime(hammerCutoff, now);
    hammerFilter.Q.setValueAtTime(0.85, now);

    const hammerGainNode = ctx.createGain();
    hammerGainNode.gain.setValueAtTime(0.0, now);
    hammerGainNode.gain.linearRampToValueAtTime(hammerThumpGain, now + 0.002);
    const hammerEndGain = Math.max(0.0001, hammerThumpGain * 0.0183);
    hammerGainNode.gain.exponentialRampToValueAtTime(hammerEndGain, now + thumpDuration);
    hammerGainNode.gain.linearRampToValueAtTime(0.0, now + thumpDuration + 0.004);

    noiseSrc.connect(hammerFilter);
    hammerFilter.connect(hammerGainNode);
    hammerGainNode.connect(bodyFilter);
    noiseSrc.start(now);
    noiseSrc.stop(now + thumpDuration + 0.008);

    // Decoupled physical felt hammer & soundboard routing (AS-42 architecture):
    // 1. Strings: stringMixer -> saturationShaper -> filter1 -> filter2 -> voiceGain -> bodyFilter
    // 2. Felt Hammer: noiseSrc -> hammerFilter -> hammerGainNode -> bodyFilter
    // 3. Resonant Soundboard: bodyFilter -> engine.inputGain
    stringMixer.connect(saturationShaper);
    saturationShaper.connect(filter1);
    filter1.connect(filter2);
    filter2.connect(voiceGain);
    voiceGain.connect(bodyFilter);
    bodyFilter.connect(this.engine.inputGain);

    let isStopped = false;
    const teardownVoice = (tailSec) => {
      if (isStopped) return;
      isStopped = true;
      try {
        const stopTime = ctx.currentTime + Math.max(0.1, tailSec);
        osc1.stop(stopTime);
        osc2.stop(stopTime);
        osc3.stop(stopTime);
        try { noiseSrc.stop(stopTime); } catch (_) {}
        setTimeout(() => {
          try {
            bodyFilter.disconnect();
            voiceGain.disconnect();
            hammerGainNode.disconnect();
            hammerFilter.disconnect();
            filter2.disconnect();
            filter1.disconnect();
            saturationShaper.disconnect();
            stringMixer.disconnect();
            osc1Gain.disconnect();
            osc2Gain.disconnect();
            osc3Gain.disconnect();
          } catch (_) {}
        }, (stopTime - ctx.currentTime + 0.1) * 1000);
      } catch (_) {}
    };

    // Auto-teardown when note naturally expires after durationSec
    const naturalExpireTimer = setTimeout(() => {
      teardownVoice(0.05);
    }, (durationSec + 0.5) * 1000);

    return {
      release: (releaseSec = 0.28) => {
        clearTimeout(naturalExpireTimer);
        try {
          const t = ctx.currentTime;
          const tau = Math.max(0.005, releaseSec * 0.12);
          const tElapsed = Math.max(0, t - now);

          let curGain = sustainLevel;
          if (tElapsed <= attackTime) {
            curGain = peakGain * (tElapsed / Math.max(0.001, attackTime));
          } else if (tElapsed <= (attackTime + 0.35)) {
            const frac = (tElapsed - attackTime) / 0.35;
            curGain = peakGain * Math.pow(sustainLevel / peakGain, frac);
          }

          let heldOk = false;
          if (typeof voiceGain.gain.cancelAndHoldAtTime === 'function') {
            try {
              voiceGain.gain.cancelAndHoldAtTime(t);
              heldOk = true;
            } catch (_) {}
          }
          if (!heldOk) {
            try {
              voiceGain.gain.cancelScheduledValues(t);
              voiceGain.gain.setValueAtTime(curGain, t);
            } catch (_) {}
          }
          voiceGain.gain.setTargetAtTime(0.0, t, Math.max(0.005, releaseSec * 0.25));

          // Fade in-flight hammer smoothly to zero on rapid key release
          if (hammerGainNode && hammerGainNode.gain) {
            try {
              let hHeld = false;
              if (typeof hammerGainNode.gain.cancelAndHoldAtTime === 'function') {
                try {
                  hammerGainNode.gain.cancelAndHoldAtTime(t);
                  hHeld = true;
                } catch (_) {}
              }
              if (!hHeld) {
                hammerGainNode.gain.cancelScheduledValues(t);
                const hammerInFlight = (tElapsed < (thumpDuration + 0.004));
                const safeHGain = hammerInFlight ? Math.max(0.0001, hammerThumpGain * Math.exp(-tElapsed / 0.008)) : 0.0;
                hammerGainNode.gain.setValueAtTime(safeHGain, t);
              }
              hammerGainNode.gain.setTargetAtTime(0.0, t, 0.004);
            } catch (_) {}
          }

          // Fade stringMixer smoothly to zero
          if (stringMixer && stringMixer.gain) {
            try {
              let mHeld = false;
              if (typeof stringMixer.gain.cancelAndHoldAtTime === 'function') {
                try {
                  stringMixer.gain.cancelAndHoldAtTime(t);
                  mHeld = true;
                } catch (_) {}
              }
              if (!mHeld) {
                stringMixer.gain.cancelScheduledValues(t);
              }
              stringMixer.gain.setTargetAtTime(0.0, t, tau);
            } catch (_) {}
          }

          // Harold Budd acoustic felt damping: absorb high-frequency energy smoothly on release
          const dampedCutoff = Math.max(160, f0 * 1.05);
          [filter1, filter2].forEach((f) => {
            if (f && f.frequency) {
              try {
                let fHeld = false;
                if (typeof f.frequency.cancelAndHoldAtTime === 'function') {
                  try {
                    f.frequency.cancelAndHoldAtTime(t);
                    fHeld = true;
                  } catch (_) {}
                }
                if (!fHeld) {
                  f.frequency.cancelScheduledValues(t);
                }
                f.frequency.setTargetAtTime(dampedCutoff, t, tau);
              } catch (_) {}
            }
          });

          // Ensure adequate release tail before stopping oscillators and unhooking nodes
          const tailSec = Math.max(0.5, releaseSec * 3.0);
          teardownVoice(tailSec);
        } catch (_) {}
      }
    };
  }

  /**
   * Strummed Polyphonic Chord Playback (Matching Harold Budd felt piano styling)
   */
  playChord(chordIndex, isHold = false) {
    if (!this.isPowered) return;
    const chord = CHORD_VOICINGS[chordIndex % CHORD_VOICINGS.length];
    if (!chord) return;

    if (this.isJuce && typeof window !== 'undefined' && window.__JUCE__?.backend?.emitEvent) {
      try {
        window.__JUCE__.backend.emitEvent('exciterTrigger', {
          type: 'chord',
          chordIndex: chordIndex,
          speed: this.chordSpeed
        });
      } catch (e) {}
      return;
    }

    const strumMs = CHORD_SPEEDS[this.chordSpeed] ?? 50;
    const voices = [];
    const timerIds = [];
    let isReleased = false;
    let releaseSec = 0.35;

    chord.freqs.forEach((freq, i) => {
      const jitter = strumMs > 0 ? (Math.random() - 0.5) * 6 : 0;
      const delay = Math.max(0, i * strumMs + jitter);
      const tid = setTimeout(() => {
        if (!this.isPowered || isReleased) return;
        const midi = 69 + 12 * Math.log2(freq / 440);
        const v = this.playChime(midi, 0.40, 4.2, isHold, true);
        if (!v) return;
        if (isReleased) {
          if (typeof v.release === 'function') v.release(releaseSec);
          return;
        }
        voices.push(v);
      }, delay);
      timerIds.push(tid);
    });

    return {
      release: (sec = 0.35) => {
        if (isReleased) return;
        isReleased = true;
        releaseSec = sec;
        timerIds.forEach((tid) => clearTimeout(tid));
        timerIds.length = 0;
        voices.forEach((v) => {
          if (v && typeof v.release === 'function') v.release(sec);
        });
        voices.length = 0;
      }
    };
  }

  /**
   * Dirac Delta Band-Limited Impulse Excitation
   */
  triggerDirac() {
    if (!this.isPowered) return;
    if (this.isJuce && typeof window !== 'undefined' && window.__JUCE__?.backend?.emitEvent) {
      try {
        window.__JUCE__.backend.emitEvent('exciterTrigger', { type: 'dirac' });
      } catch (e) {}
    }
    if (this.engine && typeof this.engine.triggerDirac === 'function') {
      this.engine.triggerDirac();
    }
    const impulseBtns = document.querySelectorAll('#btn-audition-impulse, #btn-pulse-dirac');
    impulseBtns.forEach((btn) => {
      btn.classList.add('is-active');
      setTimeout(() => {
        if (!this._heldKeys || !this._heldKeys.has(' ')) {
          btn.classList.remove('is-active');
        }
      }, 160);
    });
  }

  /**
   * Paul Kellet 3-Pole Filtered Pink Noise Burst
   */
  triggerPinkBurst(durationMs = 40) {
    if (!this.isPowered) return;
    if (this.isJuce && typeof window !== 'undefined' && window.__JUCE__?.backend?.emitEvent) {
      try {
        window.__JUCE__.backend.emitEvent('exciterTrigger', { type: 'pink', duration: durationMs });
      } catch (e) {}
    }
    if (!this.engine.ctx || !this.engine.inputGain) return;
    const ctx = this.engine.ctx;
    const length = Math.floor((durationMs / 1000) * ctx.sampleRate);
    const buf = ctx.createBuffer(1, length, ctx.sampleRate);
    const d = buf.getChannelData(0);

    let b0 = 0, b1 = 0, b2 = 0;
    for (let i = 0; i < length; i++) {
      const white = Math.random() * 2 - 1;
      b0 = 0.99886 * b0 + white * 0.0555179;
      b1 = 0.99332 * b1 + white * 0.0750759;
      b2 = 0.96900 * b2 + white * 0.1538520;
      const pink = (b0 + b1 + b2 + white * 0.5362) * 0.25;

      const win = Math.sin((Math.PI * i) / (length - 1));
      d[i] = pink * win * 0.85;
    }

    let dcSum = 0;
    for (let i = 0; i < length; i++) dcSum += d[i];
    const mean = dcSum / length;
    for (let i = 0; i < length; i++) d[i] -= mean;

    const src = ctx.createBufferSource();
    src.buffer = buf;
    src.connect(this.engine.inputGain);
    src.start();
  }

  /**
   * 78Hz Resonant Acoustic Mallet / Hammer Thud
   */
  triggerHammerThud() {
    if (!this.isPowered) return;
    if (this.isJuce && typeof window !== 'undefined' && window.__JUCE__?.backend?.emitEvent) {
      try {
        window.__JUCE__.backend.emitEvent('exciterTrigger', { type: 'hammer' });
      } catch (e) {}
    }
    if (!this.engine.ctx || !this.engine.inputGain) return;
    const ctx = this.engine.ctx;
    const now = ctx.currentTime;

    const osc = ctx.createOscillator();
    const g = ctx.createGain();
    osc.frequency.setValueAtTime(110, now);
    osc.frequency.exponentialRampToValueAtTime(55, now + 0.045);

    g.gain.setValueAtTime(0.0, now);
    g.gain.linearRampToValueAtTime(0.85, now + 0.003);
    g.gain.exponentialRampToValueAtTime(0.0001, now + 0.085);
    g.gain.linearRampToValueAtTime(0.0, now + 0.090);

    osc.connect(g);
    g.connect(this.engine.inputGain);
    osc.start(now);
    osc.stop(now + 0.095);
  }

  triggerMallet() {
    return this.triggerHammerThud();
  }

  triggerNoiseBurst(durationMs = 40) {
    return this.triggerPinkBurst(durationMs);
  }

  triggerSynthPad() {
    if (!this.isPowered) return;
    if (this.isJuce && typeof window !== 'undefined' && window.__JUCE__?.backend?.emitEvent) {
      try {
        window.__JUCE__.backend.emitEvent('exciterTrigger', { type: 'pad' });
      } catch (e) {}
    }
    if (this.engine && typeof this.engine.triggerSynthPad === 'function') {
      this.engine.triggerSynthPad();
    }
  }

  /**
   * Autonomous Generative Poisson Process Clock
   */
  togglePoisson() {
    if (!this.isPowered && !this.isPoissonRunning) return;
    this.isPoissonRunning = !this.isPoissonRunning;
    const poissonBtn = document.getElementById('btn-poisson-toggle');
    if (poissonBtn) {
      poissonBtn.classList.toggle('is-active', this.isPoissonRunning);
    }

    if (this.isJuce && typeof window !== 'undefined' && window.__JUCE__?.backend?.emitEvent) {
      try {
        window.__JUCE__.backend.emitEvent('exciterTrigger', {
          type: 'poisson',
          enable: this.isPoissonRunning,
          epm: this.poissonEpm,
          humanize: this.poissonHumanize
        });
      } catch (e) {}
    }

    if (this.isPoissonRunning) {
      if (!this.isJuce) this._startPoissonLoop();
    } else {
      if (this.poissonTimer) {
        clearTimeout(this.poissonTimer);
        this.poissonTimer = null;
      }
    }
  }

  _startPoissonLoop() {
    const schedule = async () => {
      if (!this.isPoissonRunning || !this.isPowered) {
        this.isPoissonRunning = false;
        const poissonBtn = document.getElementById('btn-poisson-toggle');
        if (poissonBtn) poissonBtn.classList.remove('is-active');
        return;
      }
      if (this.isJuce) return;
      if (!this.engine.isInitialized) await this.engine.init();

      const lambda = Math.max(0.1, this.poissonEpm) / 60.0;
      const u = Math.max(1e-6, Math.min(1 - 1e-6, Math.random()));
      const rawInterval = -Math.log(1 - u) / lambda;
      const mean = 1.0 / lambda;
      const rubato = mean + (rawInterval - mean) * (0.35 + this.poissonHumanize * 0.65);
      const delayMs = Math.round(Math.max(0.25, Math.min(12.0, rubato)) * 1000);

      // Harold Budd step/leap pitch walk
      const isStep = Math.random() < 0.70;
      let targetMidi = this.lastPoissonMidi;
      if (isStep) {
        const steps = [-5, -4, -2, -1, 1, 2, 4, 5];
        targetMidi += steps[Math.floor(Math.random() * steps.length)];
      } else {
        targetMidi = 55 + Math.floor(Math.random() * 24);
      }
      targetMidi = Math.max(48, Math.min(84, targetMidi));

      const scale = SCALES[this.currentScaleKey] || SCALES.BUDD_PENTATONIC;
      const quantized = quantizeMidiToScale(targetMidi, this.rootPitchClass, scale.intervals);
      this.lastPoissonMidi = quantized;

      const vel = 0.35 + ((Math.random() + Math.random()) / 2) * 0.45;
      if (!this.isJuce) {
        this.playChime(quantized, vel);
      }

      // Light up chime key briefly
      const noteEl = document.querySelector(`.braun-chime-key[data-midi="${quantized}"]`);
      if (noteEl) {
        noteEl.classList.add('is-active');
        setTimeout(() => noteEl.classList.remove('is-active'), 120);
      }

      this.poissonTimer = setTimeout(schedule, delayMs);
    };

    this.poissonTimer = setTimeout(schedule, 200);
  }

  // ==========================================================================
  // Web MIDI Handling
  // ==========================================================================
  async _initMidi() {
    if (!navigator.requestMIDIAccess) return;
    try {
      const midi = await navigator.requestMIDIAccess();
      for (const input of midi.inputs.values()) {
        input.onmidimessage = (msg) => this._handleMidiMessage(msg);
      }
      midi.onstatechange = (e) => {
        if (e.port.type === 'input' && e.port.state === 'connected') {
          e.port.onmidimessage = (msg) => this._handleMidiMessage(msg);
        }
      };
    } catch (e) {}
  }

  _handleMidiMessage(msg) {
    if (!this.isPowered) return;
    const data = msg.data;
    if (!data || data.length < 2) return;
    const status = data[0] & 0xF0;

    if (status === 0x90 && data.length >= 3) {
      // Note On
      const note = data[1];
      const vel = data[2] / 127;
      if (vel > 0) {
        this.playChime(note, vel);
      }
    } else if (status === 0xB0 && data.length >= 3) {
      // CC Messages
      const cc = data[1];
      const val = data[2];

      if (cc === 1) {
        // CC 1: Mod Wheel -> Shimmer/Dimmer Blend (-100% to +100%)
        const blendNorm = (val / 127) * 200 - 100;
        if (this.knobs.shimmer_dimmer_blend) {
          this.knobs.shimmer_dimmer_blend.setValue(blendNorm, true);
        }
      } else if (cc === 11) {
        // CC 11: Expression -> Dry / Wet Mix (0 to 100%)
        const wetNorm = (val / 127) * 100;
        if (this.knobs.dry_wet_mix) {
          this.knobs.dry_wet_mix.setValue(wetNorm, true);
        }
      } else if (cc === 64) {
        // CC 64: Sustain Pedal -> Freeze Hold
        const isHeld = val >= 64;
        const holdBtn = document.getElementById('btn-decay-hold');
        if (holdBtn) {
          holdBtn.classList.toggle('is-active', isHeld);
          const statusText = holdBtn.querySelector('.braun-status-text');
          if (statusText) statusText.textContent = isHeld ? 'FREEZE ON' : 'FREEZE OFF';
        }
        this.engine.setParam('freezeHold', isHeld);
        this._emitJuceParam('freezeHold', isHeld ? 1.0 : 0.0, true);
        this._emitJuceParam('freeze_hold', isHeld ? 1.0 : 0.0, true);
      }
    }
  }
}

// ============================================================================
// Web App Lifecycle Guard: Fix DOMContentLoaded Race Condition
// ============================================================================
function bootstrapRb26() {
  if (window.__RB26_INITIALIZED__) return;
  window.__RB26_INITIALIZED__ = true;

  window.__RB26__ = new BraunRb26App();
  window.__RB26__.init().catch((err) => {
    console.error('[BRAUN RB-26] Fatal Initialization Error:', err);
  });
}

if (typeof document !== 'undefined') {
  if (document.readyState === 'interactive' || document.readyState === 'complete') {
    // DOM already parsed — execute immediately without hanging
    bootstrapRb26();
  } else {
    // DOM still loading — bind once to DOMContentLoaded
    document.addEventListener('DOMContentLoaded', bootstrapRb26, { once: true });
  }
}
