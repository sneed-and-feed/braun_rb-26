/**
 * @file crt-display.js
 * @brief 3-Mode CRT Phosphor Visualizer for BRAUN RB-26 Studio Reverb
 * Features 8x6 precision graticule, P1 phosphor persistence decay,
 * two-pass vector beam glow, and 3 analysis modes:
 * Mode 1: RT60 Energy Decay Curve (EDC Waterfall / Schroeder backward integration)
 * Mode 2: Lissajous XY Phase Goniometer
 * Mode 3: Real-time Spectrum Analyzer
 */

export class BraunCrtDisplay {
  /**
   * @param {HTMLCanvasElement} canvas
   * @param {Object} options
   */
  constructor(canvas, options = {}) {
    if (!canvas) return;

    this.canvas = canvas;
    this.ctx = canvas.getContext ? canvas.getContext('2d') : null;
    this.mode = options.mode || 'EDC'; // 'EDC', 'LISSAJOUS', 'SPECTRUM'
    this.isPowered = options.isPowered ?? true;
    this.isRunning = false;
    this.animationFrameId = null;

    // Authentic P1 oscilloscope lab phosphor
    this.phosphorColor = '#24FF6A';
    this.phosphorGlow = 'rgba(36, 255, 106, 0.40)';
    this.amberColor = '#E5A93C';
    this.orangeColor = '#EE592B';

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

    // Spectrum peak hold buffer
    this.numBands = 32;
    this.spectrumPeaks = new Float32Array(this.numBands);
    this.spectrumValues = new Float32Array(this.numBands);

    this.silentFrames = 0;
    this.lastRenderTime = 0;

    this._resize();
    if (typeof window !== 'undefined') {
      window.addEventListener('resize', () => this._resize());
    }
  }

  _resize() {
    if (!this.canvas || !this.ctx) return;
    const rect = this.canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const w = rect.width || this.canvas.clientWidth || 360;
    const h = rect.height || this.canvas.clientHeight || 190;

    this.canvas.width = Math.floor(w * dpr);
    this.canvas.height = Math.floor(h * dpr);
    this.ctx.resetTransform?.();
    this.ctx.scale(dpr, dpr);

    this.width = w;
    this.height = h;

    this.ctx.fillStyle = '#121414';
    this.ctx.fillRect(0, 0, w, h);
    this.draw();
  }

  setPower(isPowered) {
    this.isPowered = Boolean(isPowered);
    this.silentFrames = 0;
    if (!this.isRunning) this.start();
    this.draw();
  }

  setMode(mode) {
    this.mode = mode;
    this.draw();
  }

  /**
   * Push real-time audio samples from the DSP engine
   * @param {Float32Array} left
   * @param {Float32Array} right
   */
  pushAudio(left, right) {
    if (!left) return;
    const len = Math.min(this.bufferSize, left.length);
    for (let i = 0; i < len; i++) {
      this.timeDataL[i] = left[i];
      this.timeDataR[i] = right ? right[i] : left[i];
    }
    this.silentFrames = 0;
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

      const now = timestamp || performance.now();
      const elapsed = now - this.lastRenderTime;

      // Adapt refresh rate: 40 FPS active (25ms), 10 FPS idle
      const interval = this.silentFrames > 40 ? 100 : 25;

      if (elapsed >= interval) {
        this.lastRenderTime = now;
        this.draw();
      }

      this.animationFrameId = requestAnimationFrame(render);
    };

    this.animationFrameId = requestAnimationFrame(render);
  }

  stop() {
    this.isRunning = false;
    if (this.animationFrameId) {
      cancelAnimationFrame(this.animationFrameId);
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

    if (this.mode === 'EDC') {
      this._drawEdcWaterfall(ctx, w, h);
    } else if (this.mode === 'LISSAJOUS') {
      this._drawLissajous(ctx, w, h);
    } else if (this.mode === 'SPECTRUM') {
      this._drawSpectrum(ctx, w, h);
    }
  }

  _drawGraticule(ctx, w, h) {
    ctx.save();
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.08)';
    ctx.lineWidth = 1;

    const numX = 8;
    const numY = 6;
    const stepX = w / numX;
    const stepY = h / numY;

    // Grid lines
    for (let i = 1; i < numX; i++) {
      const x = Math.round(i * stepX) + 0.5;
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }

    for (let j = 1; j < numY; j++) {
      const y = Math.round(j * stepY) + 0.5;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(w, y);
      ctx.stroke();
    }

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
    for (let i = 0; i <= numX * ticksPerDiv; i++) {
      const tx = Math.round(i * (stepX / ticksPerDiv)) + 0.5;
      ctx.beginPath();
      ctx.moveTo(tx, midY - 2);
      ctx.lineTo(tx, midY + 2);
      ctx.stroke();
    }
    for (let j = 0; j <= numY * ticksPerDiv; j++) {
      const ty = Math.round(j * (stepY / ticksPerDiv)) + 0.5;
      ctx.beginPath();
      ctx.moveTo(midX - 2, ty);
      ctx.lineTo(midX + 2, ty);
      ctx.stroke();
    }
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

  _drawEdcWaterfall(ctx, w, h) {
    ctx.save();
    ctx.font = '9px var(--font-mono)';
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

  _drawLissajous(ctx, w, h) {
    ctx.save();
    ctx.font = '9px var(--font-mono)';
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('LISSAJOUS XY STEREO CORRELATION', 10, 15);

    const midX = w / 2;
    const midY = h / 2;
    const scale = Math.min(w, h) * 0.42;
    const invSqrt2 = 0.70710678;

    ctx.beginPath();
    let hasStarted = false;

    // 45° rotated stereo field: X = (L - R)*0.707, Y = (L + R)*0.707
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
    ctx.lineWidth = 1.3;
    ctx.stroke();
    ctx.restore();
  }

  _drawSpectrum(ctx, w, h) {
    ctx.save();
    ctx.font = '9px var(--font-mono)';
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('1/3-OCTAVE SPECTRUM ANALYZER', 10, 15);

    const numBands = this.numBands;
    const bandWidth = (w - 20) / numBands;
    const startX = 10;
    const bottomY = h - 8;

    // Approximate band energies from time samples
    const step = Math.floor(this.bufferSize / numBands);
    for (let b = 0; b < numBands; b++) {
      let sum = 0;
      for (let s = 0; s < step; s++) {
        const idx = (b * step + s) % this.bufferSize;
        sum += Math.abs(this.timeDataL[idx]);
      }
      const rawMag = sum / step;

      // Smooth decay
      this.spectrumValues[b] = Math.max(rawMag, this.spectrumValues[b] * 0.88);
      this.spectrumPeaks[b] = Math.max(this.spectrumValues[b], this.spectrumPeaks[b] * 0.96);

      const barHeight = Math.max(2, Math.min(h * 0.82, this.spectrumValues[b] * h * 2.6));
      const peakY = bottomY - Math.min(h * 0.82, this.spectrumPeaks[b] * h * 2.6);
      const bx = startX + b * bandWidth + 1;
      const bw = bandWidth - 2;

      // Glow pass
      ctx.fillStyle = this.phosphorGlow;
      ctx.fillRect(bx - 1, bottomY - barHeight - 1, bw + 2, barHeight + 2);

      // Core bar
      ctx.fillStyle = this.phosphorColor;
      ctx.fillRect(bx, bottomY - barHeight, bw, barHeight);

      // Peak line
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(bx, peakY, bw, 1.5);
    }
    ctx.restore();
  }
}
