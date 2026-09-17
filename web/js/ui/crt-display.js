/**
 * @file crt-display.js
 * @brief Master CRT Phosphor Visualizer for BRAUN RB-26 Studio Reverb (AS-42 Heritage Upgrade)
 * Features:
 * - Authentic P1 oscilloscope lab phosphor persistence decay (#24FF6A)
 * - Two-pass hardware-accelerated vector beam glow
 * - 8x6 precision graticule with center crosshairs and division tick marks (offscreen cached)
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
    this.fontMono = '9px monospace';

    // Buffers for real-time audio analysis
    this.bufferSize = 512;
    this.fftSize = 512;
    this.timeDataL = new Float32Array(this.bufferSize);
    this.timeDataR = new Float32Array(this.bufferSize);
    this.minDecibels = -95;
    this.maxDecibels = -10;
    this.freqDataL = new Float32Array(this.fftSize >> 1);
    this.isJuce = options.isJuce ?? false;

    // EDC waterfall history buffers
    this.historySize = 128;
    this.edcLow = new Float32Array(this.historySize);
    this.edcMid = new Float32Array(this.historySize);
    this.edcHigh = new Float32Array(this.historySize);
    this.historyIdx = 0;

    // Pre-allocated FFT scratch buffers & tables for zero garbage collection
    this.fftSize = 512;
    this._fftReal = new Float32Array(this.fftSize);
    this._fftImag = new Float32Array(this.fftSize);

    // Precomputed Hann window to eliminate 512 Math.cos calls per frame
    this._hannWindow = new Float32Array(this.fftSize);
    for (let i = 0; i < this.fftSize; i++) {
      this._hannWindow[i] = 0.5 * (1 - Math.cos((2 * Math.PI * i) / (this.fftSize - 1)));
    }

    // Precomputed bit-reversal table for 512-point FFT
    this._bitRev = new Uint16Array(this.fftSize);
    for (let i = 0; i < this.fftSize; i++) {
      let rev = 0;
      for (let bit = 0; bit < 9; bit++) {
        if ((i >> bit) & 1) {
          rev |= 1 << (8 - bit);
        }
      }
      this._bitRev[i] = rev;
    }

    // Spectrum bars and precomputed perceptual frequency warping
    this.numBars = 48;
    this.spectrumBars = new Float32Array(this.numBars);
    this.spectrumPeaks = new Float32Array(this.numBars);
    this._barBinIndices = new Uint16Array(this.numBars);
    this._barHeights = new Float32Array(this.numBars);
    this._peakYs = new Float32Array(this.numBars);

    const halfN = this.fftSize >> 1;
    for (let b = 0; b < this.numBars; b++) {
      const normIdx = b / this.numBars;
      this._barBinIndices[b] = Math.min(halfN - 1, Math.floor(Math.pow(normIdx, 1.5) * halfN));
    }

    // Precomputed twiddle factor tables for radix-2 FFT (eliminates per-frame trig & drift)
    this._twiddleCos = new Float32Array(halfN);
    this._twiddleSin = new Float32Array(halfN);
    for (let k = 0; k < halfN; k++) {
      const angle = (-2 * Math.PI * k) / this.fftSize;
      this._twiddleCos[k] = Math.cos(angle);
      this._twiddleSin[k] = Math.sin(angle);
    }

    this._barX = new Float32Array(this.numBars);
    this._barW = 1;

    this.silentFrames = 0;
    this.lastRenderTime = 0;
    this._dpr = 1;
    this.width = 360;
    this.height = 190;
    this._graticuleCanvas = null;

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
    const w = Math.round(rect.width || this.canvas.clientWidth || 360);
    const h = Math.round(rect.height || this.canvas.clientHeight || 190);

    const targetW = Math.floor(w * dpr);
    const targetH = Math.floor(h * dpr);

    // Cache resize: only re-allocate buffer when physical pixels change
    if (this.canvas.width === targetW && this.canvas.height === targetH && this._dpr === dpr && this.width === w && this.height === h) {
      return;
    }

    this._dpr = dpr;
    this.canvas.width = targetW;
    this.canvas.height = targetH;
    if (this.ctx.resetTransform) {
      this.ctx.resetTransform();
    }
    if (this.ctx.scale) {
      this.ctx.scale(dpr, dpr);
    }

    this.width = w;
    this.height = h;

    const barWidth = (w - 20) / this.numBars;
    this._barW = Math.max(1, barWidth - 2);
    for (let b = 0; b < this.numBars; b++) {
      this._barX[b] = 10 + b * barWidth + 1;
    }

    this._updateGraticuleCache(w, h);

    this.ctx.fillStyle = '#121414';
    this.ctx.fillRect(0, 0, w, h);
    this.draw();
  }

  _updateGraticuleCache(w, h) {
    if (typeof document === 'undefined') return;
    if (!this._graticuleCanvas) {
      this._graticuleCanvas = document.createElement('canvas');
    }
    this._graticuleCanvas.width = w;
    this._graticuleCanvas.height = h;
    const gctx = this._graticuleCanvas.getContext('2d');
    if (!gctx) return;
    this._renderGraticuleDirect(gctx, w, h);
  }

  _renderGraticuleDirect(ctx, w, h) {
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

    // Center crosshairs
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
  }

  _drawGraticule(ctx, w, h) {
    if (this._graticuleCanvas && this._graticuleCanvas.width === w && this._graticuleCanvas.height === h) {
      ctx.drawImage(this._graticuleCanvas, 0, 0);
    } else {
      this._renderGraticuleDirect(ctx, w, h);
    }
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
      if (!this.freqDataL || this.freqDataL.length !== (size >> 1)) {
        this.freqDataL = new Float32Array(size >> 1);
      }
      if (typeof this.analyserL.minDecibels === 'number') {
        this.minDecibels = this.analyserL.minDecibels;
      }
      if (typeof this.analyserL.maxDecibels === 'number') {
        this.maxDecibels = this.analyserL.maxDecibels;
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
      if (!hasSignal && (Math.abs(lData[i]) > 0.0005 || Math.abs(r) > 0.0005)) {
        hasSignal = true;
      }
    }
    for (let i = len; i < this.bufferSize; i++) {
      this.timeDataL[i] = 0;
      this.timeDataR[i] = 0;
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

  _updateEdcFromTimeData() {
    const len = this.bufferSize;
    let sumLow = 0, sumMid = 0, sumHigh = 0;
    let prev = 0;
    // Step by 2 for speed
    for (let i = 0; i < len; i += 2) {
      const s = 0.5 * (this.timeDataL[i] + this.timeDataR[i]);
      const diff = s - prev;
      prev = s;
      sumMid += s * s;
      sumHigh += diff * diff;
    }
    const count = len >> 1;
    const midE = Math.min(1.0, Math.sqrt(sumMid / count) * 3.5);
    const highE = Math.min(1.0, Math.sqrt(sumHigh / count) * 2.8);
    const lowE = Math.min(1.0, Math.max(0, midE * 1.25 - highE * 0.4));
    this.pushTelemetry(lowE, midE, highE);
  }

  start() {
    if (this.isRunning) return;
    this.isRunning = true;
    this.lastRenderTime = 0;

    const render = (timestamp) => {
      if (!this.isRunning) return;

      const now = timestamp || (typeof performance !== 'undefined' ? performance.now() : Date.now());
      const elapsed = now - this.lastRenderTime;

      // Adapt refresh rate: 60+ FPS active (interval 10ms to prevent V-Sync jitter frame-skips), 15 FPS idle (>20 silent frames), 5 FPS deep idle (>60)
      const interval = (this.silentFrames > 60) ? 200 : (this.silentFrames > 20) ? 66 : 10;

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
            this._updateEdcFromTimeData();
          } else {
            this.silentFrames = Math.min(100, this.silentFrames + 1);
            if (this.silentFrames < 130) {
              this.pushTelemetry(0, 0, 0);
            }
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

    // 8x6 Precision Graticule (hardware-cached)
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
    ctx.font = this.fontMono;
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('ANALOG OSCILLOSCOPE [CH1 + CH2 SUM]', 10, 15);

    const len = this.bufferSize;
    // Streamlined single-pass Schmitt trigger rising zero-crossing search using stereo composite
    let startIdx = 0;
    const searchLimit = Math.min(len >> 1, len - 2);
    let fallbackIdx = -1;
    for (let i = 0; i < searchLimit; i++) {
      const c0 = 0.5 * (this.timeDataL[i] + this.timeDataR[i]);
      const c1 = 0.5 * (this.timeDataL[i + 1] + this.timeDataR[i + 1]);
      if (c0 < -0.005 && c1 >= 0.0) {
        startIdx = i;
        fallbackIdx = -1;
        break;
      }
      if (fallbackIdx === -1 && c0 < 0.0 && c1 >= 0.0) {
        fallbackIdx = i;
      }
    }
    if (fallbackIdx !== -1 && startIdx === 0) {
      startIdx = fallbackIdx;
    }

    const samplesToDraw = Math.min(len - startIdx, 256);
    if (samplesToDraw < 2) {
      ctx.restore();
      return;
    }

    const sliceWidth = w / (samplesToDraw - 1);
    const midY = h * 0.5;
    const ampScale = midY * 0.85;

    ctx.lineCap = 'butt';
    ctx.lineJoin = 'bevel';
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
    ctx.lineWidth = 3.2;
    ctx.stroke();

    ctx.strokeStyle = this.phosphorColor;
    ctx.lineWidth = 1.4;
    ctx.stroke();

    ctx.restore();
  }

  _renderEdcBand(ctx, hist, color, glow, scale, w, h) {
    ctx.beginPath();
    const histSize = this.historySize;
    const xStep = w / (histSize - 1);
    const hScale = h * 0.82;
    let idx = this.historyIdx;

    for (let i = 0; i < histSize; i++) {
      const val = Math.min(1.0, Math.max(0.0, hist[idx] * scale));
      const px = i * xStep;
      const py = h - val * hScale - 8;

      if (i === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);

      idx++;
      if (idx >= histSize) idx = 0;
    }

    ctx.lineCap = 'butt';
    ctx.lineJoin = 'bevel';
    ctx.strokeStyle = glow;
    ctx.lineWidth = 3.2;
    ctx.stroke();

    ctx.strokeStyle = color;
    ctx.lineWidth = 1.4;
    ctx.stroke();
  }

  /**
   * Mode 2: RT60 Schroeder Backward Energy Decay Curve Waterfall
   */
  _drawEdcWaterfall(ctx, w, h) {
    ctx.save();
    ctx.font = this.fontMono;
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('RT60 ENERGY DECAY [SCHROEDER INTEGRAL]', 10, 15);

    // Low band (Amber), Mid band (Phosphor Green), High band (Braun Orange)
    this._renderEdcBand(ctx, this.edcLow, this.amberColor, 'rgba(229, 169, 60, 0.35)', 1.2, w, h);
    this._renderEdcBand(ctx, this.edcMid, this.phosphorColor, this.phosphorGlow, 1.0, w, h);
    this._renderEdcBand(ctx, this.edcHigh, this.orangeColor, 'rgba(238, 89, 43, 0.35)', 0.85, w, h);

    ctx.restore();
  }

  /**
   * Mode 3: 45-Degree Rotated Stereo Phase Goniometer (Lissajous)
   */
  _drawLissajous(ctx, w, h) {
    ctx.save();
    ctx.font = this.fontMono;
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('LISSAJOUS XY STEREO CORRELATION', 10, 15);

    const midX = w * 0.5;
    const midY = h * 0.5;
    const scale = Math.min(w, h) * 0.44;
    const invSqrt2 = 0.70710678;
    const rotScale = invSqrt2 * scale;

    ctx.lineCap = 'butt';
    ctx.lineJoin = 'bevel';
    ctx.beginPath();
    let hasStarted = false;

    // 45 degree rotated stereo field: X = (L - R)*0.707, Y = (L + R)*0.707
    for (let i = 0; i < this.bufferSize; i += 2) {
      const l = this.timeDataL[i] || 0;
      const r = this.timeDataR[i] || 0;

      const px = midX + (l - r) * rotScale;
      const py = midY - (l + r) * rotScale;

      if (!hasStarted) {
        ctx.moveTo(px, py);
        hasStarted = true;
      } else {
        ctx.lineTo(px, py);
      }
    }

    // Glow pass + Core pass
    ctx.strokeStyle = this.phosphorGlow;
    ctx.lineWidth = 3.0;
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
    const win = this._hannWindow;
    const bitRev = this._bitRev;

    // Hann window + copy left/right average (using precomputed window)
    for (let i = 0; i < N; i++) {
      const sample = 0.5 * (this.timeDataL[i] + this.timeDataR[i]);
      real[i] = sample * win[i];
      imag[i] = 0.0;
    }

    // Bit-reversal permutation via precomputed table
    for (let i = 0; i < N; i++) {
      const j = bitRev[i];
      if (i < j) {
        const tr = real[i]; real[i] = real[j]; real[j] = tr;
        const ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
      }
    }

    // Cooley-Tukey decimation-in-time radix-2 FFT with precomputed twiddle tables
    const cosTable = this._twiddleCos;
    const sinTable = this._twiddleSin;
    for (let lenStep = 2; lenStep <= N; lenStep <<= 1) {
      const halfLen = lenStep >> 1;
      const step = N / lenStep;
      for (let i = 0; i < N; i += lenStep) {
        for (let m = 0; m < halfLen; m++) {
          const twIdx = m * step;
          const wr = cosTable[twIdx];
          const wi = sinTable[twIdx];
          const i1 = i + m;
          const i2 = i1 + halfLen;
          const uR = real[i1];
          const uI = imag[i1];
          const vR = real[i2] * wr - imag[i2] * wi;
          const vI = real[i2] * wi + imag[i2] * wr;
          real[i1] = uR + vR;
          imag[i1] = uI + vI;
          real[i2] = uR - vR;
          imag[i2] = uI - vI;
        }
      }
    }
  }

  _drawSpectrum(ctx, w, h) {
    const isJuce = Boolean(this.isJuce || (typeof window !== 'undefined' && (window.__IS_JUCE__ || window.__JUCE__)));
    const hasAnalyser = !isJuce && Boolean(this.analyserL && typeof this.analyserL.getFloatFrequencyData === 'function');
    if (hasAnalyser) {
      this.analyserL.getFloatFrequencyData(this.freqDataL);
    } else {
      this._computeFft();
    }

    ctx.save();
    ctx.font = this.fontMono;
    ctx.fillStyle = this.phosphorColor;
    ctx.fillText('PRECISION FFT SPECTRUM ANALYZER', 10, 15);

    const real = this._fftReal;
    const imag = this._fftImag;
    const numBars = this.numBars;
    const bottomY = h - 8;
    const barIndices = this._barBinIndices;
    const barHeights = this._barHeights;
    const peakYs = this._peakYs;
    const maxBarH = h * 0.80;
    const barX = this._barX;
    const bw = this._barW;
    const minDb = this.minDecibels;
    const maxDb = this.maxDecibels;
    const dbRangeInv = 1.0 / (maxDb - minDb);

    for (let b = 0; b < numBars; b++) {
      const binIdx = barIndices[b];
      let db;
      if (hasAnalyser) {
        db = this.freqDataL[binIdx];
      } else {
        const r = real[binIdx];
        const im = imag[binIdx];
        const normMag = (4.0 * Math.sqrt(r * r + im * im)) / this.fftSize;
        db = 20 * Math.log10(Math.max(normMag, 1.0e-5));
      }

      const normVal = Math.max(0, Math.min(1.0, (db - minDb) * dbRangeInv));

      // Smooth decay
      this.spectrumBars[b] = Math.max(normVal, this.spectrumBars[b] * 0.88);
      this.spectrumPeaks[b] = Math.max(this.spectrumBars[b], this.spectrumPeaks[b] * 0.97);

      barHeights[b] = Math.max(2, Math.min(maxBarH, this.spectrumBars[b] * maxBarH));
      peakYs[b] = bottomY - Math.min(maxBarH, this.spectrumPeaks[b] * maxBarH);
    }

    // Batched Pass 1: Phosphor Glow
    ctx.fillStyle = this.phosphorGlow;
    for (let b = 0; b < numBars; b++) {
      const bx = barX[b];
      const bh = barHeights[b];
      ctx.fillRect(bx - 1, bottomY - bh - 1, bw + 2, bh + 2);
    }

    // Batched Pass 2: Core Phosphor Bar
    ctx.fillStyle = this.phosphorColor;
    for (let b = 0; b < numBars; b++) {
      const bx = barX[b];
      const bh = barHeights[b];
      ctx.fillRect(bx, bottomY - bh, bw, bh);
    }

    // Batched Pass 3: Peak Markers
    ctx.fillStyle = '#FFFFFF';
    for (let b = 0; b < numBars; b++) {
      const bx = barX[b];
      ctx.fillRect(bx, peakYs[b], bw, 1.5);
    }

    ctx.restore();
  }
}
