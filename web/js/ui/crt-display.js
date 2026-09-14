/**
 * @file crt-display.js
 * @brief Master CRT Phosphor Visualizer for BRAUN RB-26 Studio Reverb (AS-42 Heritage Upgrade)
 * Features:
 * - Authentic P1 oscilloscope lab phosphor persistence decay (#24FF6A)
 * - Two-pass hardware-accelerated vector beam glow
 * - 8x6 precision graticule with center crosshairs and division tick marks
 * - 4 Analysis Modes:
 *   Mode 1: WAVE - Triggered analog phosphor waveform trace (zero-crossing sync)
 *   Mode 2: EDC - RT60 Energy Decay Curve waterfall (Schroeder backward integration)
 *   Mode 3: LISSAJOUS - 45-degree stereo phase goniometer
 *   Mode 4: SPECTRUM - Cooley-Tukey Radix-2 FFT spectrum analyzer with peak hold
 * - Adaptive frame-rate throttling during silence to minimize CPU/GPU load
 * - Follows Dieter Rams functionalist industrial design principles (zero emojis)
 */

export class BraunCrtDisplay {
  /**
   * @param {HTMLCanvasElement} canvas
   * @param {Object} [options={}]
   */
  constructor(canvas, options = {}) {
    if (!canvas) return;

    this.canvas = canvas;
    this.ctx = canvas.getContext ? canvas.getContext('2d') : null;
    this.mode = options.mode || 'EDC'; // 'WAVE', 'EDC', 'LISSAJOUS', 'SPECTRUM'
    this.isPowered = options.isPowered ?? true;
    this.isRunning = false;
    this.animationFrameId = null;

    // Authentic P1 oscilloscope lab phosphor
    this.phosphorColor = '#24FF6A'; // Braun lab green
    this.phosphorGlow = 'rgba(36, 255, 106, 0.45)';
    this.amberColor = '#E5A93C';
    this.orangeColor = '#EE592B';
    this.gridColor = 'rgba(255, 255, 255, 0.08)';

    // Buffers for real-time audio analysis
    this.bufferSize = 512;
    this.timeDataL = new Float32Array(this.bufferSize);
    this.timeDataR = new Float32Array(this.bufferSize);

    // EDC waterfall history buffers
    this.historySize = 128;
    this.edcLow = new Float32Array(this.historySize);
    this.edcMid = new Float32Array(this.historySize);
    this.edcHigh = new Float32Array(this.historySize);
    this.historyIdx = 0;

    // Pre-allocated FFT scratch buffers for zero garbage collection
    this.fftSize = 512;
    this._fftReal = new Float32Array(this.fftSize);
    this._fftImag = new Float32Array(this.fftSize);
    this.numBars = 48;
    this.spectrumBars = new Float32Array(this.numBars);
    this.spectrumPeaks = new Float32Array(this.numBars);

    this.silentFrames = 0;
    this.lastRenderTime = 0;

    this._resize();
    if (typeof window !== 'undefined') {
      window.addEventListener('resize', () => this._resize());
    }
    if (typeof ResizeObserver !== 'undefined' && this.canvas && this.canvas.parentElement) {
      this._resizeObserver = new ResizeObserver(() => this._resize());
      this._resizeObserver.observe(this.canvas.parentElement);
    }
  }

  _resize() {
    if (!this.canvas || !this.ctx) return;
    const rect = this.canvas.getBoundingClientRect();
    const dpr = (typeof window !== 'undefined' && window.devicePixelRatio) ? window.devicePixelRatio : 1;
    const w = rect.width || this.canvas.clientWidth || 360;
    const h = rect.height || this.canvas.clientHeight || 190;

    this.canvas.width = Math.floor(w * dpr);
    this.canvas.height = Math.floor(h * dpr);
    if (this.ctx.resetTransform) {
      this.ctx.resetTransform();
    }
    if (this.ctx.scale) {
      this.ctx.scale(dpr, dpr);
    }

    this.width = w;
    this.height = h;

    this.ctx.fillStyle = '#121414';
    this.ctx.fillRect(0, 0, w, h);
    this.draw();
  }

  setAnalysers(analyserL, analyserR = null) {
    this.analyserL = analyserL;
    this.analyserR = analyserR;
    if (this.analyserL) {
      const size = this.analyserL.fftSize || 512;
      if (this.timeDataL.length !== size) {
        this.timeDataL = new Float32Array(size);
        this.timeDataR = new Float32Array(size);
        this.bufferSize = size;
      }
      if (!this.isRunning) this.start();
    }
    this.draw();
  }

  setPower(isPowered) {
    this.isPowered = Boolean(isPowered);
    this.silentFrames = 0;
    if (!this.isRunning) this.start();
    this.draw();
  }

  setMode(mode) {
    // Normalize mode names
    if (mode === 'WAVEFORM') mode = 'WAVE';
    this.mode = mode;
    this.draw();
  }

  /**
   * Push real-time audio samples from the DSP engine
   * @param {Float32Array|Object} left
   * @param {Float32Array} [right]
   */
  pushAudio(left, right) {
    if (!left) return;
    let lData = left;
    let rData = right;
    if (left && left.leftData) {
      lData = left.leftData;
      rData = left.rightData || left.leftData;
    }
    if (!lData || typeof lData.length !== 'number') return;
    const len = Math.min(this.bufferSize, lData.length);
    let hasSignal = false;

    for (let i = 0; i < len; i++) {
      this.timeDataL[i] = lData[i];
      const r = (rData && typeof rData[i] === 'number') ? rData[i] : lData[i];
      this.timeDataR[i] = r;
      if (!hasSignal && (Math.abs(lData[i]) > 0.005 || Math.abs(r) > 0.005)) {
        hasSignal = true;
      }
    }

    if (hasSignal) {
      this.silentFrames = 0;
    } else {
      this.silentFrames = Math.min(100, this.silentFrames + 1);
    }

    if (!this.isRunning) this.start();
  }

  /**
   * Push telemetry metrics (decay envelope, low/mid/high band energy)
   */
  pushTelemetry(lowEnergy, midEnergy, highEnergy) {
    this.edcLow[this.historyIdx] = lowEnergy;
    this.edcMid[this.historyIdx] = midEnergy;
    this.edcHigh[this.historyIdx] = highEnergy;
    this.historyIdx = (this.historyIdx + 1) % this.historySize;
  }

  start() {
    if (this.isRunning) return;
    this.isRunning = true;
    this.lastRenderTime = 0;

    const render = (timestamp) => {
      if (!this.isRunning) return;

      const now = timestamp || (typeof performance !== 'undefined' ? performance.now() : Date.now());
      const elapsed = now - this.lastRenderTime;

      // Adapt refresh rate: 40 FPS active (25ms), 10 FPS idle (>15 silent frames), 5 FPS deep idle (>60)
      const interval = (this.silentFrames > 60) ? 200 : (this.silentFrames > 15) ? 100 : 25;

      if (elapsed >= interval) {
        if (this.analyserL) {
          this.analyserL.getFloatTimeDomainData(this.timeDataL);
          if (this.analyserR) {
            this.analyserR.getFloatTimeDomainData(this.timeDataR);
          } else {
            this.timeDataR.set(this.timeDataL);
          }
          let maxVal = 0;
          for (let i = 0; i < this.bufferSize; i += 8) {
            const absVal = Math.abs(this.timeDataL[i]);
            if (absVal > maxVal) maxVal = absVal;
          }
          if (maxVal > 0.005) {
            this.silentFrames = 0;
          } else {
            this.silentFrames = Math.min(100, this.silentFrames + 1);
          }
        }

        this.lastRenderTime = now;
        this.draw();
      }

      if (typeof requestAnimationFrame !== 'undefined') {
        this.animationFrameId = requestAnimationFrame(render);
      }
    };

    if (typeof requestAnimationFrame !== 'undefined') {
      this.animationFrameId = requestAnimationFrame(render);
    }
  }

  stop() {
    this.isRunning = false;
    if (this.animationFrameId) {
      if (typeof cancelAnimationFrame !== 'undefined') {
        cancelAnimationFrame(this.animationFrameId);
      }
      this.animationFrameId = null;
    }
  }

  draw() {
    if (!this.ctx) return;
    const ctx = this.ctx;
    const w = this.width || 360;
    const h = this.height || 190;

    // Semi-transparent clearing for optical phosphor decay trails
    ctx.fillStyle = 'rgba(18, 20, 20, 0.28)';
    ctx.fillRect(0, 0, w, h);

    // 8x6 Precision Graticule
    this._drawGraticule(ctx, w, h);

    if (!this.isPowered) {
      this._drawStandbyBeam(ctx, w, h);
      return;
    }

    if (this.mode === 'WAVE' || this.mode === 'WAVEFORM') {
      this._drawWaveform(ctx, w, h);
    } else if (this.mode === 'EDC') {
      this._drawEdcWaterfall(ctx, w, h);
    } else if (this.mode === 'LISSAJOUS') {
      this._drawLissajous(ctx, w, h);
    } else if (this.mode === 'SPECTRUM') {
      this._drawSpectrum(ctx, w, h);
    }
  }

  _drawGraticule(ctx, w, h) {
    ctx.save();
    ctx.strokeStyle = this.gridColor;
    ctx.lineWidth = 1;

    const numX = 8;
    const numY = 6;
    const stepX = w / numX;
    const stepY = h / numY;

    // Batch grid lines
    ctx.beginPath();
    for (let i = 1; i < numX; i++) {
      const x = Math.round(i * stepX) + 0.5;
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
    }
    for (let j = 1; j < numY; j++) {
      const y = Math.round(j * stepY) + 0.5;
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
    }
    ctx.stroke();

    // Bold center crosshairs
    const midX = Math.round(w / 2) + 0.5;
    const midY = Math.round(h / 2) + 0.5;
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.16)';
    ctx.beginPath();
    ctx.moveTo(midX, 0);
    ctx.lineTo(midX, h);
    ctx.moveTo(0, midY);
    ctx.lineTo(w, midY);
    ctx.stroke();

    // Calibrated division ticks
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.25)';
    const ticksPerDiv = 5;
    ctx.beginPath();
    for (let i = 0; i <= numX * ticksPerDiv; i++) {
      const tx = Math.round(i * (stepX / ticksPerDiv)) + 0.5;
      ctx.moveTo(tx, midY - 2);
      ctx.lineTo(tx, midY + 2);
    }
    for (let j = 0; j <= numY * ticksPerDiv; j++) {
      const ty = Math.round(j * (stepY / ticksPerDiv)) + 0.5;
      ctx.moveTo(midX - 2, ty);
      ctx.lineTo(midX + 2, ty);
    }
    ctx.stroke();
    ctx.restore();
  }

  _drawStandbyBeam(ctx, w, h) {
    const midY = h / 2;
    ctx.save();
    ctx.strokeStyle = this.phosphorGlow;
    ctx.lineWidth = 3.5;
    ctx.beginPath();
    ctx.moveTo(0, midY);
    ctx.lineTo(w, midY);
    ctx.stroke();

    ctx.strokeStyle = this.phosphorColor;
    ctx.lineWidth = 1.4;
    ctx.beginPath();
    ctx.moveTo(0, midY);
    ctx.lineTo(w, midY);
    ctx.stroke();
    ctx.restore();
  }

  /**
   * Mode 1: Triggered Analog Phosphor Waveform (Zero-Crossing Rising Edge Sync)
   */
  _drawWaveform(ctx, w, h) {
    ctx.save();
    ctx.font = '9px var(--font-mono, monospace)';
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('ANALOG OSCILLOSCOPE [CH1 + CH2 SUM]', 10, 15);

    const len = this.bufferSize;
    // Schmitt trigger rising zero-crossing search with hysteresis for rock-solid stationary trace
    let startIdx = 0;
    const searchLimit = Math.min(Math.floor(len / 2), len - 2);
    for (let i = 0; i < searchLimit; i++) {
      if (this.timeDataL[i] < -0.005 && this.timeDataL[i + 1] >= 0.0) {
        startIdx = i;
        break;
      }
    }
    if (startIdx === 0) {
      for (let i = 0; i < searchLimit; i++) {
        if (this.timeDataL[i] < 0.0 && this.timeDataL[i + 1] >= 0.0) {
          startIdx = i;
          break;
        }
      }
    }

    const samplesToDraw = Math.min(len - startIdx, 256);
    if (samplesToDraw < 2) {
      ctx.restore();
      return;
    }

    const sliceWidth = w / (samplesToDraw - 1);
    const midY = h / 2;
    const ampScale = (h / 2) * 0.85;

    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.beginPath();

    let x = 0;
    for (let i = 0; i < samplesToDraw; i++) {
      // Sum Left + Right for composite studio monitor view
      const sample = 0.5 * (this.timeDataL[startIdx + i] + this.timeDataR[startIdx + i]);
      const clampedSample = Math.max(-1.1, Math.min(1.1, sample));
      const y = midY - clampedSample * ampScale;

      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
      x += sliceWidth;
    }

    // Two-pass hardware-accelerated phosphor vector glow
    ctx.strokeStyle = this.phosphorGlow;
    ctx.lineWidth = 3.6;
    ctx.stroke();

    ctx.strokeStyle = this.phosphorColor;
    ctx.lineWidth = 1.6;
    ctx.stroke();

    ctx.restore();
  }

  /**
   * Mode 2: RT60 Schroeder Backward Energy Decay Curve Waterfall
   */
  _drawEdcWaterfall(ctx, w, h) {
    ctx.save();
    ctx.font = '9px var(--font-mono, monospace)';
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('RT60 ENERGY DECAY [SCHROEDER INTEGRAL]', 10, 15);

    const renderBand = (hist, color, glow, scale) => {
      ctx.beginPath();
      for (let i = 0; i < this.historySize; i++) {
        const idx = (this.historyIdx + i) % this.historySize;
        const val = Math.min(1.0, Math.max(0.0, hist[idx] * scale));
        const px = (i / (this.historySize - 1)) * w;
        const py = h - val * (h * 0.82) - 8;

        if (i === 0) ctx.moveTo(px, py);
        else ctx.lineTo(px, py);
      }

      // Two-pass glow
      ctx.strokeStyle = glow;
      ctx.lineWidth = 3.6;
      ctx.stroke();
      ctx.strokeStyle = color;
      ctx.lineWidth = 1.5;
      ctx.stroke();
    };

    // Low band (Amber), Mid band (Phosphor Green), High band (Braun Orange)
    renderBand(this.edcLow, this.amberColor, 'rgba(229, 169, 60, 0.35)', 1.2);
    renderBand(this.edcMid, this.phosphorColor, this.phosphorGlow, 1.0);
    renderBand(this.edcHigh, this.orangeColor, 'rgba(238, 89, 43, 0.35)', 0.85);

    ctx.restore();
  }

  /**
   * Mode 3: 45-Degree Rotated Stereo Phase Goniometer (Lissajous)
   */
  _drawLissajous(ctx, w, h) {
    ctx.save();
    ctx.font = '9px var(--font-mono, monospace)';
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('LISSAJOUS XY STEREO CORRELATION', 10, 15);

    const midX = w / 2;
    const midY = h / 2;
    const scale = Math.min(w, h) * 0.44;
    const invSqrt2 = 0.70710678;

    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.beginPath();
    let hasStarted = false;

    // 45 degree rotated stereo field: X = (L - R)*0.707, Y = (L + R)*0.707
    for (let i = 0; i < this.bufferSize; i += 2) {
      const l = this.timeDataL[i] || 0;
      const r = this.timeDataR[i] || 0;

      const rotX = (l - r) * invSqrt2;
      const rotY = (l + r) * invSqrt2;

      const px = midX + rotX * scale;
      const py = midY - rotY * scale;

      if (!hasStarted) {
        ctx.moveTo(px, py);
        hasStarted = true;
      } else {
        ctx.lineTo(px, py);
      }
    }

    // Glow pass + Core pass
    ctx.strokeStyle = this.phosphorGlow;
    ctx.lineWidth = 3.2;
    ctx.stroke();

    ctx.strokeStyle = this.phosphorColor;
    ctx.lineWidth = 1.4;
    ctx.stroke();
    ctx.restore();
  }

  /**
   * Mode 4: Cooley-Tukey Radix-2 FFT Spectrum Analyzer with Peak Hold
   */
  _computeFft() {
    const N = this.fftSize;
    const real = this._fftReal;
    const imag = this._fftImag;

    // Hann window + copy left/right average
    for (let i = 0; i < N; i++) {
      const window = 0.5 * (1 - Math.cos((2 * Math.PI * i) / (N - 1)));
      const sample = 0.5 * (this.timeDataL[i] + this.timeDataR[i]);
      real[i] = sample * window;
      imag[i] = 0.0;
    }

    // Bit-reversal permutation
    let j = 0;
    for (let i = 0; i < N - 1; i++) {
      if (i < j) {
        const tr = real[i]; real[i] = real[j]; real[j] = tr;
        const ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
      }
      let k = N >> 1;
      while (k <= j) {
        j -= k;
        k >>= 1;
      }
      j += k;
    }

    // Cooley-Tukey decimation-in-time radix-2 FFT
    for (let lenStep = 2; lenStep <= N; lenStep <<= 1) {
      const halfLen = lenStep >> 1;
      const angle = (-2 * Math.PI) / lenStep;
      const wStepR = Math.cos(angle);
      const wStepI = Math.sin(angle);
      for (let i = 0; i < N; i += lenStep) {
        let wr = 1.0;
        let wi = 0.0;
        for (let m = 0; m < halfLen; m++) {
          const uR = real[i + m];
          const uI = imag[i + m];
          const vR = real[i + m + halfLen] * wr - imag[i + m + halfLen] * wi;
          const vI = real[i + m + halfLen] * wi + imag[i + m + halfLen] * wr;
          real[i + m] = uR + vR;
          imag[i + m] = uI + vI;
          real[i + m + halfLen] = uR - vR;
          imag[i + m + halfLen] = uI - vI;
          const nextWr = wr * wStepR - wi * wStepI;
          wi = wr * wStepI + wi * wStepR;
          wr = nextWr;
        }
      }
    }
  }

  _drawSpectrum(ctx, w, h) {
    this._computeFft();

    ctx.save();
    ctx.font = '9px var(--font-mono, monospace)';
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('PRECISION FFT SPECTRUM ANALYZER', 10, 15);

    const N = this.fftSize;
    const halfN = N >> 1;
    const real = this._fftReal;
    const imag = this._fftImag;
    const numBars = this.numBars;
    const barWidth = (w - 20) / numBars;
    const startX = 10;
    const bottomY = h - 8;

    for (let b = 0; b < numBars; b++) {
      const normIdx = b / numBars;
      // Perceptual frequency warp (t^1.5 gives high bass/mid resolution while reaching 20kHz)
      const binIdx = Math.min(halfN - 1, Math.floor(Math.pow(normIdx, 1.5) * halfN));
      const mag = Math.sqrt(real[binIdx] * real[binIdx] + imag[binIdx] * imag[binIdx]) * 4.0;
      const db = 20 * Math.log10(Math.max(mag, 1e-4));
      const normVal = Math.max(0, Math.min(1.0, (db + 60) / 60));

      // Smooth decay
      this.spectrumBars[b] = Math.max(normVal, this.spectrumBars[b] * 0.88);
      this.spectrumPeaks[b] = Math.max(this.spectrumBars[b], this.spectrumPeaks[b] * 0.97);

      const barHeight = Math.max(2, Math.min(h * 0.80, this.spectrumBars[b] * h * 0.80));
      const peakY = bottomY - Math.min(h * 0.80, this.spectrumPeaks[b] * h * 0.80);
      const bx = startX + b * barWidth + 1;
      const bw = barWidth - 2;

      // Glow pass
      ctx.fillStyle = this.phosphorGlow;
      ctx.fillRect(bx - 1, bottomY - barHeight - 1, bw + 2, barHeight + 2);

      // Core phosphor bar
      ctx.fillStyle = this.phosphorColor;
      ctx.fillRect(bx, bottomY - barHeight, bw, barHeight);

      // Peak line
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(bx, peakY, bw, 1.5);
    }
    ctx.restore();
  }
}
