/**
 * @file vector-pad.js
 * @brief Braun RB-26 Precision Vector Touchpad for Deck 05 Tail Modulation.
 * Features 2D coordinate modulation (Mod Rate on X, Mod Depth & Bloom Onset on Y),
 * analog hairline crosshair reticle, high-DPI canvas, telemetry readouts,
 * momentary spring-return vs latched operation, and keyboard accessibility.
 * Follows Dieter Rams functionalist industrial design principles (zero emojis).
 */

export class BraunVectorPad {
  /**
   * @param {HTMLElement} container
   * @param {Object} [options={}]
   */
  constructor(container, options = {}) {
    if (!container) return;
    this.container = container;
    this.onEngage = options.onEngage || null;
    this.onChange = options.onChange || null;

    this.defaultX = options.defaultX ?? 0.45;
    this.defaultY = options.defaultY ?? 0.45;
    this.x = this.defaultX;
    this.y = this.defaultY;

    this.mode = options.mode || 'momentary'; // 'momentary' | 'latch'
    this.isEngaged = false;
    this._animId = null;

    this.dpr = (typeof window !== 'undefined' && window.devicePixelRatio) ? window.devicePixelRatio : 1;
    this.width = 340;
    this.height = 120;

    this._render();
    this._attachEvents();
    this._applyModulation(false);
    this.draw();
  }

  _render() {
    if (typeof document === 'undefined' || !this.container) return;
    this.container.innerHTML = '';

    this.wrapper = document.createElement('div');
    this.wrapper.className = 'braun-vector-pad-wrapper';
    this.wrapper.tabIndex = 0;
    this.wrapper.setAttribute('role', 'region');
    this.wrapper.setAttribute('aria-label', 'Braun RB-26 Vector Modulation Touchpad');

    this.wrapper.innerHTML = `
      <div class="braun-vector-header">
        <div class="braun-vector-title-group">
          <span class="braun-panel-title">VECTOR MODULATION</span>
          <span class="braun-vector-axis-meta">X: RATE &middot; Y: DEPTH &amp; BLOOM</span>
        </div>
        <div class="braun-vector-controls">
          <button class="braun-vector-mini-btn" id="btn-vector-mode" title="Toggle Momentary Spring / Latch Lock">
            <span class="braun-led"></span>
            <span id="vector-mode-label">MOMENTARY</span>
          </button>
          <button class="braun-vector-mini-btn" id="btn-vector-reset" title="Reset Coordinates to Center Origin">
            RESET [&middot;]
          </button>
        </div>
      </div>

      <div class="braun-vector-surface-box" id="vector-surface">
        <canvas class="braun-vector-canvas"></canvas>
        <span class="braun-vector-axis-label label-x-min">&#9668; SLOW</span>
        <span class="braun-vector-axis-label label-x-max">FAST &#9658;</span>
        <span class="braun-vector-axis-label label-y-max">&#9650; DEEP BLOOM</span>
        <span class="braun-vector-axis-label label-y-min">SUBTLE &#9660;</span>
      </div>

      <div class="braun-vector-readout">
        <div class="braun-vector-coords">
          <span class="braun-vector-readout-item">X: <span class="braun-vector-readout-val" id="readout-x">0.65 Hz</span></span>
          <span style="color: var(--border-line); margin: 0 5px;">|</span>
          <span class="braun-vector-readout-item">Y: <span class="braun-vector-readout-val" id="readout-y">DEPTH 45% &middot; BLOOM 85ms</span></span>
        </div>
        <div class="braun-vector-readout-indicator">
          <span class="braun-led" id="vector-status-led"></span>
          <span id="vector-status-text" style="font-size: 8px; font-weight: 700; letter-spacing: 0.06em;">STANDBY</span>
        </div>
      </div>
    `;

    this.container.appendChild(this.wrapper);

    this.surfaceBox = this.wrapper.querySelector('#vector-surface');
    this.canvas = this.wrapper.querySelector('.braun-vector-canvas');
    this.ctx = this.canvas && this.canvas.getContext ? this.canvas.getContext('2d') : null;

    this.modeBtn = this.wrapper.querySelector('#btn-vector-mode');
    this.modeLabel = this.wrapper.querySelector('#vector-mode-label');
    this.resetBtn = this.wrapper.querySelector('#btn-vector-reset');
    this.readoutX = this.wrapper.querySelector('#readout-x');
    this.readoutY = this.wrapper.querySelector('#readout-y');
    this.statusLed = this.wrapper.querySelector('#vector-status-led');
    this.statusText = this.wrapper.querySelector('#vector-status-text');

    this._resize();
  }

  _resize() {
    if (!this.canvas || !this.surfaceBox) return;
    const rect = this.surfaceBox.getBoundingClientRect ? this.surfaceBox.getBoundingClientRect() : { width: 340, height: 120 };
    this._cachedRect = rect;
    const dpr = (typeof window !== 'undefined' && window.devicePixelRatio) ? window.devicePixelRatio : 1;
    const w = Math.max(10, Math.round(rect.width || this.surfaceBox.clientWidth || 340));
    const h = Math.max(10, Math.round(rect.height || this.surfaceBox.clientHeight || 120));

    this.dpr = dpr;
    this.width = w;
    this.height = h;

    this.canvas.width = Math.floor(w * dpr);
    this.canvas.height = Math.floor(h * dpr);
  }

  _attachEvents() {
    if (typeof window !== 'undefined') {
      window.addEventListener('resize', () => {
        this._resize();
        this.draw();
      });
    }

    if (typeof ResizeObserver !== 'undefined' && this.surfaceBox) {
      this._resizeObserver = new ResizeObserver(() => {
        this._resize();
        this.draw();
      });
      this._resizeObserver.observe(this.surfaceBox);
    }

    if (this.modeBtn) {
      this.modeBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.toggleMode();
      });
    }

    if (this.resetBtn) {
      this.resetBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.resetToCenter();
      });
    }

    const handlePointerMove = (e) => {
      if (!this.isEngaged) return;
      if (e.cancelable && typeof e.preventDefault === 'function') {
        e.preventDefault();
      }
      const rect = this._cachedRect || this.surfaceBox.getBoundingClientRect();
      if (!rect.width || !rect.height) return;

      const clX = e.clientX ?? (e.touches && e.touches[0].clientX) ?? 0;
      const clY = e.clientY ?? (e.touches && e.touches[0].clientY) ?? 0;

      const normX = Math.max(0, Math.min(1, (clX - rect.left) / rect.width));
      const normY = Math.max(0, Math.min(1, 1.0 - (clY - rect.top) / rect.height));

      this.setCoordinates(normX, normY, true);
    };

    const handlePointerUp = (e) => {
      if (!this.isEngaged) return;
      this.isEngaged = false;
      this._updateStatusUi();

      if (this.surfaceBox.releasePointerCapture && e.pointerId !== undefined) {
        try {
          if (!this.surfaceBox.hasPointerCapture || this.surfaceBox.hasPointerCapture(e.pointerId)) {
            this.surfaceBox.releasePointerCapture(e.pointerId);
          }
        } catch (err) {}
      }

      if (this.mode === 'momentary') {
        this._springReturn();
      } else {
        this.draw();
      }
    };

    this.surfaceBox.addEventListener('pointerdown', async (e) => {
      e.preventDefault();
      this._cachedRect = this.surfaceBox.getBoundingClientRect();
      if (this.surfaceBox.setPointerCapture && e.pointerId !== undefined) {
        try {
          this.surfaceBox.setPointerCapture(e.pointerId);
        } catch (err) {}
      }

      if (this._animId) {
        cancelAnimationFrame(this._animId);
        this._animId = null;
      }

      this.isEngaged = true;
      this._updateStatusUi();

      if (this.onEngage) {
        await this.onEngage();
      }

      const rect = this._cachedRect;
      const clX = e.clientX ?? (e.touches && e.touches[0].clientX) ?? 0;
      const clY = e.clientY ?? (e.touches && e.touches[0].clientY) ?? 0;
      const normX = Math.max(0, Math.min(1, (clX - rect.left) / rect.width));
      const normY = Math.max(0, Math.min(1, 1.0 - (clY - rect.top) / rect.height));

      this.setCoordinates(normX, normY, true);
    });

    this.surfaceBox.addEventListener('pointermove', handlePointerMove);
    this.surfaceBox.addEventListener('pointerup', handlePointerUp);
    this.surfaceBox.addEventListener('pointercancel', handlePointerUp);

    this.surfaceBox.addEventListener('dblclick', () => {
      this.resetToCenter();
    });

    this.wrapper.addEventListener('keydown', (e) => {
      const step = e.shiftKey ? 0.01 : 0.04;
      let handled = false;

      if (e.key === 'ArrowLeft') {
        this.setCoordinates(Math.max(0, this.x - step), this.y, true);
        handled = true;
      } else if (e.key === 'ArrowRight') {
        this.setCoordinates(Math.min(1, this.x + step), this.y, true);
        handled = true;
      } else if (e.key === 'ArrowUp') {
        this.setCoordinates(this.x, Math.min(1, this.y + step), true);
        handled = true;
      } else if (e.key === 'ArrowDown') {
        this.setCoordinates(this.x, Math.max(0, this.y - step), true);
        handled = true;
      } else if (e.key === 'Home' || e.key === 'Escape') {
        this.resetToCenter();
        handled = true;
      }

      if (handled) {
        e.preventDefault();
        if (this.onEngage) this.onEngage();
      }
    });
  }

  toggleMode() {
    this.mode = this.mode === 'momentary' ? 'latch' : 'momentary';
    if (this.modeBtn) {
      this.modeBtn.classList.toggle('is-active', this.mode === 'latch');
    }
    if (this.modeLabel) {
      this.modeLabel.textContent = this.mode.toUpperCase();
    }
    if (this.mode === 'momentary' && !this.isEngaged) {
      this._springReturn();
    }
  }

  setDefaults(defaultX, defaultY) {
    if (this._animId) {
      if (typeof cancelAnimationFrame === 'function') cancelAnimationFrame(this._animId);
      else clearTimeout(this._animId);
      this._animId = null;
    }
    this.isEngaged = false;
    this._updateStatusUi();
    this.defaultX = Math.max(0, Math.min(1.0, defaultX));
    this.defaultY = Math.max(0, Math.min(1.0, defaultY));
  }

  resetToCenter(update = true, animate = false, duration = 250) {
    if (this._animId) {
      if (typeof cancelAnimationFrame === 'function') cancelAnimationFrame(this._animId);
      else clearTimeout(this._animId);
      this._animId = null;
    }
    if (animate && duration > 0) {
      this.animateTo(this.defaultX, this.defaultY, duration, update);
    } else {
      this.setCoordinates(this.defaultX, this.defaultY, update);
    }
  }

  animateTo(targetX, targetY, duration = 250, update = false) {
    if (this._animId) {
      if (typeof cancelAnimationFrame === 'function') cancelAnimationFrame(this._animId);
      else clearTimeout(this._animId);
      this._animId = null;
    }

    const startX = this.x;
    const startY = this.y;
    const clampedTargetX = Math.max(0, Math.min(1.0, targetX));
    const clampedTargetY = Math.max(0, Math.min(1.0, targetY));

    if (duration <= 0 || (Math.abs(startX - clampedTargetX) < 1e-4 && Math.abs(startY - clampedTargetY) < 1e-4)) {
      this.setCoordinates(clampedTargetX, clampedTargetY, update);
      return;
    }

    const startTime = (typeof performance !== 'undefined' && performance.now) ? performance.now() : Date.now();
    const easeOutCubic = (t) => 1 - Math.pow(1 - t, 3);

    const step = (currentTime) => {
      const now = (typeof currentTime === 'number' && currentTime > 0) ? currentTime : ((typeof performance !== 'undefined' && performance.now) ? performance.now() : Date.now());
      const elapsed = Math.max(0, now - startTime);
      const progress = Math.min(1.0, elapsed / duration);
      const eased = easeOutCubic(progress);

      const curX = startX + (clampedTargetX - startX) * eased;
      const curY = startY + (clampedTargetY - startY) * eased;

      this.setCoordinates(curX, curY, update);

      if (progress < 1.0) {
        if (typeof requestAnimationFrame === 'function') {
          this._animId = requestAnimationFrame(step);
        } else {
          this._animId = setTimeout(() => step(Date.now()), 16);
        }
      } else {
        this.setCoordinates(clampedTargetX, clampedTargetY, update);
        this._animId = null;
      }
    };

    if (typeof requestAnimationFrame === 'function') {
      this._animId = requestAnimationFrame(step);
    } else {
      this._animId = setTimeout(() => step(Date.now()), 16);
    }
  }

  setCoordinates(x, y, update = true) {
    this.x = Math.max(0, Math.min(1.0, x));
    this.y = Math.max(0, Math.min(1.0, y));

    if (update) {
      this._applyModulation(true);
    }
    this._updateReadout();
    this.draw();
  }

  _updateStatusUi() {
    if (this.statusLed) {
      this.statusLed.classList.toggle('is-active', this.isEngaged);
    }
    if (this.statusText) {
      this.statusText.textContent = this.isEngaged ? 'ENGAGED' : 'STANDBY';
      this.statusText.style.color = this.isEngaged ? 'var(--braun-orange)' : 'var(--text-secondary)';
    }
    if (this.surfaceBox) {
      this.surfaceBox.classList.toggle('is-active', this.isEngaged);
    }
  }

  _calculateValues() {
    // X: Mod Rate 0.05 Hz to 4.00 Hz (smooth response with fine low-end resolution)
    const rateHz = +(0.05 + Math.pow(this.x, 1.6) * 3.95).toFixed(2);
    // Y: Mod Depth 0% to 100%
    const depthPct = Math.round(this.y * 100);
    // Y: Bloom Onset 20ms to 250ms
    const bloomMs = Math.round(20 + this.y * 230);

    return { rateHz, depthPct, bloomMs };
  }

  _updateReadout() {
    const { rateHz, depthPct, bloomMs } = this._calculateValues();

    if (this.readoutX) {
      this.readoutX.textContent = `${rateHz.toFixed(2)} Hz`;
    }
    if (this.readoutY) {
      this.readoutY.textContent = `DEPTH ${depthPct}% \u00B7 BLOOM ${bloomMs}ms`;
    }
  }

  _applyModulation(notifyChange = true) {
    const { rateHz, depthPct, bloomMs } = this._calculateValues();

    if (notifyChange && this.onChange) {
      this.onChange({
        x: this.x,
        y: this.y,
        rateHz,
        depthPct,
        bloomMs
      });
    }
  }

  _springReturn() {
    if (this._animId) {
      cancelAnimationFrame(this._animId);
      this._animId = null;
    }

    const startX = this.x;
    const startY = this.y;
    const targetX = this.defaultX;
    const targetY = this.defaultY;
    const duration = 220;
    const startTime = (typeof performance !== 'undefined') ? performance.now() : Date.now();

    const step = (currentTime) => {
      const elapsed = currentTime - startTime;
      const progress = Math.min(1.0, elapsed / duration);
      const ease = 1.0 - Math.pow(1.0 - progress, 4);

      const curX = startX + (targetX - startX) * ease;
      const curY = startY + (targetY - startY) * ease;

      this.setCoordinates(curX, curY, true);

      if (progress < 1.0) {
        this._animId = requestAnimationFrame(step);
      } else {
        this.setCoordinates(targetX, targetY, true);
        this._animId = null;
      }
    };

    this._animId = requestAnimationFrame(step);
  }

  draw() {
    if (!this.ctx || !this.width || !this.height) return;
    const ctx = this.ctx;
    const dpr = this.dpr || ((typeof window !== 'undefined' && window.devicePixelRatio) ? window.devicePixelRatio : 1);
    const w = this.width;
    const h = this.height;

    const targetW = Math.floor(w * dpr);
    const targetH = Math.floor(h * dpr);
    if (this.canvas && (this.canvas.width !== targetW || this.canvas.height !== targetH)) {
      this.canvas.width = targetW;
      this.canvas.height = targetH;
    }

    ctx.save();
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    // Dark technical bezel background
    ctx.fillStyle = '#121414';
    ctx.fillRect(0, 0, w, h);

    // Subtle 8x4 technical grid lines
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.07)';
    ctx.lineWidth = 1;
    const cols = 8;
    const rows = 4;
    ctx.beginPath();
    for (let i = 1; i < cols; i++) {
      const gx = Math.floor((w / cols) * i) + 0.5;
      ctx.moveTo(gx, 0);
      ctx.lineTo(gx, h);
    }
    for (let j = 1; j < rows; j++) {
      const gy = Math.floor((h / rows) * j) + 0.5;
      ctx.moveTo(0, gy);
      ctx.lineTo(w, gy);
    }
    ctx.stroke();

    // Center Origin Crosshairs (0.50, 0.50)
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.16)';
    ctx.beginPath();
    const cx = Math.floor(w * 0.5) + 0.5;
    const cy = Math.floor(h * 0.5) + 0.5;
    ctx.moveTo(cx, 0);
    ctx.lineTo(cx, h);
    ctx.moveTo(0, cy);
    ctx.lineTo(w, cy);
    ctx.stroke();

    // Default resting anchor indicator
    const defPx = Math.floor(w * this.defaultX) + 0.5;
    const defPy = Math.floor(h * (1.0 - this.defaultY)) + 0.5;
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.22)';
    ctx.lineWidth = 1;
    ctx.strokeRect(defPx - 3, defPy - 3, 6, 6);

    // Current pointer / vector reticle position
    const px = Math.floor(w * this.x) + 0.5;
    const py = Math.floor(h * (1.0 - this.y)) + 0.5;

    const accentColor = this.isEngaged ? '#EE592B' : '#ECEBE4';
    const glowColor = this.isEngaged ? 'rgba(238, 89, 43, 0.35)' : 'rgba(255, 255, 255, 0.15)';

    // Hairline crosshair through reticle
    ctx.strokeStyle = glowColor;
    ctx.lineWidth = 2.5;
    ctx.beginPath();
    ctx.moveTo(px, 0);
    ctx.lineTo(px, h);
    ctx.moveTo(0, py);
    ctx.lineTo(w, py);
    ctx.stroke();

    ctx.strokeStyle = accentColor;
    ctx.lineWidth = 1.0;
    ctx.stroke();

    // Outer reticle ring
    ctx.strokeStyle = glowColor;
    ctx.lineWidth = 3.0;
    ctx.beginPath();
    ctx.arc(px, py, 10, 0, Math.PI * 2);
    ctx.stroke();

    ctx.strokeStyle = accentColor;
    ctx.lineWidth = 1.4;
    ctx.stroke();

    // Center core reticle dot
    ctx.fillStyle = accentColor;
    ctx.beginPath();
    ctx.arc(px, py, 2.5, 0, Math.PI * 2);
    ctx.fill();

    ctx.restore();
  }
}
