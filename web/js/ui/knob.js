/**
 * @file knob.js
 * @brief Precision Dieter Rams / Braun rotary control component for the RB-26.
 * Features turned aluminum styling, touch disambiguation for tablet scrolling,
 * multi-touch isolation, logarithmic/linear scaling, double-click direct entry,
 * keyboard accessibility, and fine-tuning modifiers.
 */

export class BraunKnob {
  /**
   * @param {HTMLElement} container
   * @param {Object} options
   */
  constructor(container, options = {}) {
    this.container = container;
    this.id = options.id || `knob-${Math.random().toString(36).substr(2, 9)}`;
    this.label = options.label || 'CONTROL';
    this.min = options.min ?? 0;
    this.max = options.max ?? 100;
    this.step = options.step ?? 1;
    this.unit = options.unit || '';
    this.isLog = options.isLog ?? false;
    this.defaultValue = options.value ?? (this.min + (this.max - this.min) / 2);
    this.value = this.defaultValue;
    this.precision = options.precision ?? (this.step < 1 ? (options.precision !== undefined ? options.precision : 2) : 0);
    this.size = options.size || 'medium'; // 'small', 'medium', 'large'
    this.color = options.color || 'var(--knob-fill)';
    this.onChange = options.onChange || null;
    this.onDragEnd = options.onDragEnd || null;

    this.isDragging = false;
    this.lastUserInteractionTime = 0;
    this._lastFormatted = '';
    this._lastAria = null;

    this.startAngle = -140; // degrees
    this.endAngle = 140;   // degrees
    this.angleRange = this.endAngle - this.startAngle; // 280 deg

    if (this.container && typeof document !== 'undefined') {
      this.container.innerHTML = '';
      this._render();
      this._attachEvents();
    }
    this.setValue(this.value, false);
  }

  _render() {
    this.element = document.createElement('div');
    this.element.className = `braun-knob-wrapper braun-knob-${this.size}`;
    this.element.tabIndex = 0;
    this.element.setAttribute('role', 'slider');
    this.element.setAttribute('aria-label', this.label);
    this.element.setAttribute('aria-valuemin', this.min);
    this.element.setAttribute('aria-valuemax', this.max);
    this.element.setAttribute('aria-valuenow', this.value);

    this.element.innerHTML = `
      <div class="braun-knob-label">${this.label}</div>
      <div class="braun-knob-assembly">
        <svg class="braun-knob-scale" viewBox="0 0 100 100">
          <circle class="braun-knob-track" cx="50" cy="50" r="42" />
          <circle class="braun-knob-fill" cx="50" cy="50" r="42" />
        </svg>
        <div class="braun-knob-cap">
          <div class="braun-knob-indicator"></div>
        </div>
      </div>
      <div class="braun-knob-value-display">
        <span class="braun-knob-value-text">${this.formatValue(this.value)}</span><span class="braun-knob-unit">${this.unit}</span>
      </div>
      <input type="text" class="braun-knob-direct-input" style="display:none;" />
    `;

    this.container.appendChild(this.element);

    this.cap = this.element.querySelector('.braun-knob-cap');
    this.fillCircle = this.element.querySelector('.braun-knob-fill');
    this.valueText = this.element.querySelector('.braun-knob-value-text');
    this.directInput = this.element.querySelector('.braun-knob-direct-input');
    this.assembly = this.element.querySelector('.braun-knob-assembly');
    this.labelEl = this.element.querySelector('.braun-knob-label');
    this.valueDisplay = this.element.querySelector('.braun-knob-value-display');

    // Arc length: radius 42 -> circumference ~ 263.89
    // Angle range: 280 deg out of 360 deg = 0.77777 of circle ~ 205.25
    this.circumference = 2 * Math.PI * 42;
    this.arcLength = (this.angleRange / 360) * this.circumference;
    if (this.fillCircle) {
      this.fillCircle.style.strokeDasharray = `${this.arcLength} ${this.circumference}`;
      this.fillCircle.style.strokeDashoffset = `${this.arcLength}`;
      this.fillCircle.setAttribute('stroke-dasharray', `${this.arcLength} ${this.circumference}`);
      this.fillCircle.setAttribute('stroke-dashoffset', `${this.arcLength}`);
    }
  }

  _attachEvents() {
    let startY = 0;
    let startVal = 0;
    let lastShift = false;
    let dragMode = null; // 'pointer' | 'touch' | 'mouse'
    let activeTouchId = null;
    let activePointerId = null;

    const onPointerDown = (e) => {
      if (e.target === this.directInput) return;
      if (this.isDragging) return;

      // Touch Disambiguation: Labels and value readouts allow native vertical momentum scrolling
      const isLabelOrValue = Boolean(
        e.target && (
          (typeof e.target.closest === 'function' && (
            e.target.closest('.braun-knob-label') ||
            e.target.closest('.braun-knob-value-display') ||
            e.target.closest('.braun-knob-direct-input')
          )) ||
          (e.target.classList && (
            e.target.classList.contains('braun-knob-label') ||
            e.target.classList.contains('braun-knob-value-display') ||
            e.target.classList.contains('braun-knob-direct-input')
          ))
        )
      );

      if (isLabelOrValue) {
        return; // Allow native page scroll
      }

      // Only touches specifically on the rotary assembly capture drag
      const isKnobAssembly = Boolean(
        (e.target && typeof e.target.closest === 'function' && e.target.closest('.braun-knob-assembly')) ||
        (e.target && e.target.classList && e.target.classList.contains('braun-knob-assembly')) ||
        (this.assembly && (e.target === this.assembly || (typeof this.assembly.contains === 'function' && this.assembly.contains(e.target))))
      );

      if (!isKnobAssembly) {
        return;
      }

      if (typeof e.preventDefault === 'function') {
        e.preventDefault();
      }

      // Multi-touch isolation
      if (e.pointerId !== undefined && (e.type === 'pointerdown' || e.pointerType)) {
        dragMode = 'pointer';
        activePointerId = e.pointerId;
        startY = e.clientY ?? 0;
        try {
          if (this.element.setPointerCapture && e.pointerId != null) {
            this.element.setPointerCapture(e.pointerId);
          }
        } catch (err) {}
      } else if (e.changedTouches && e.changedTouches.length > 0) {
        dragMode = 'touch';
        activeTouchId = e.changedTouches[0].identifier;
        startY = e.changedTouches[0].clientY;
      } else if (e.touches && e.touches.length > 0) {
        dragMode = 'touch';
        activeTouchId = e.touches[0].identifier;
        startY = e.touches[0].clientY;
      } else {
        dragMode = 'mouse';
        startY = e.clientY || 0;
      }

      this.isDragging = true;
      this.lastUserInteractionTime = (typeof performance !== 'undefined' ? performance.now() : Date.now());
      startVal = this.value;
      lastShift = Boolean(e.shiftKey);
      this.element.classList.add('is-active');

      const applyDeltaY = (currentY, shiftKey) => {
        this.lastUserInteractionTime = (typeof performance !== 'undefined' ? performance.now() : Date.now());
        const currentShift = Boolean(shiftKey);
        if (currentShift !== lastShift) {
          // Re-anchor start values when Shift modifier is pressed/released to eliminate sudden jumps
          startVal = this.value;
          startY = currentY;
          lastShift = currentShift;
        }

        const deltaY = startY - currentY;
        const sensitivity = currentShift ? 0.1 : 1.0;
        const pixelRange = 160;

        const normalizedChange = (deltaY / pixelRange) * sensitivity;
        const rawNorm = this.toNormalized(startVal) + normalizedChange;
        const normVal = Math.max(0, Math.min(1, rawNorm));

        // Re-anchor at bounds to eliminate deadzone lag when reversing drag direction
        if (rawNorm > 1.0 || rawNorm < 0.0) {
          startY = currentY;
          startVal = this.fromNormalized(normVal);
        }

        const newVal = this.fromNormalized(normVal);
        this.setValue(newVal, true);
      };

      const onPointerMove = (ev) => {
        if (!this.isDragging || dragMode !== 'pointer') return;
        if (activePointerId !== null && ev.pointerId !== undefined && ev.pointerId !== activePointerId) {
          return; // Ignore other pointers
        }
        if (ev.cancelable && typeof ev.preventDefault === 'function') {
          ev.preventDefault();
        }
        applyDeltaY(ev.clientY, ev.shiftKey);
      };

      const onMouseMove = (ev) => {
        if (!this.isDragging || dragMode !== 'mouse') return;
        if (ev.cancelable && typeof ev.preventDefault === 'function') {
          ev.preventDefault();
        }
        applyDeltaY(ev.clientY, ev.shiftKey);
      };

      const onTouchMove = (ev) => {
        if (!this.isDragging || dragMode !== 'touch') return;
        if (activeTouchId === null || !ev.touches) return;
        if (ev.cancelable && typeof ev.preventDefault === 'function') {
          ev.preventDefault();
        }
        for (let i = 0; i < ev.touches.length; i++) {
          if (ev.touches[i].identifier === activeTouchId) {
            applyDeltaY(ev.touches[i].clientY, ev.shiftKey);
            break;
          }
        }
      };

      const cleanup = () => {
        if (!this.isDragging) return;
        this.isDragging = false;
        try {
          if (this.element.releasePointerCapture && activePointerId != null) {
            if (!this.element.hasPointerCapture || this.element.hasPointerCapture(activePointerId)) {
              this.element.releasePointerCapture(activePointerId);
            }
          }
        } catch (err) {}

        dragMode = null;
        activePointerId = null;
        activeTouchId = null;
        this.element.classList.remove('is-active');

        window.removeEventListener('pointermove', onPointerMove);
        window.removeEventListener('pointerup', cleanup);
        window.removeEventListener('pointercancel', cleanup);
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', cleanup);
        window.removeEventListener('touchmove', onTouchMove);
        window.removeEventListener('touchend', cleanup);
        window.removeEventListener('touchcancel', cleanup);

        if (typeof this.onDragEnd === 'function') {
          this.onDragEnd(this.value);
        }
      };

      if (dragMode === 'pointer') {
        window.addEventListener('pointermove', onPointerMove, { passive: false });
        window.addEventListener('pointerup', cleanup);
        window.addEventListener('pointercancel', cleanup);
      } else if (dragMode === 'touch') {
        window.addEventListener('touchmove', onTouchMove, { passive: false });
        window.addEventListener('touchend', cleanup);
        window.addEventListener('touchcancel', cleanup);
      } else {
        window.addEventListener('mousemove', onMouseMove);
        window.addEventListener('mouseup', cleanup);
      }
    };

    if (window.PointerEvent) {
      this.element.addEventListener('pointerdown', onPointerDown);
    } else {
      this.element.addEventListener('mousedown', onPointerDown);
      this.element.addEventListener('touchstart', onPointerDown, { passive: false });
    }

    // Mouse Wheel fine adjustment
    this.element.addEventListener('wheel', (e) => {
      e.preventDefault();
      this.lastUserInteractionTime = (typeof performance !== 'undefined' ? performance.now() : Date.now());
      const direction = e.deltaY < 0 ? 1 : -1;
      const stepSize = (e.shiftKey ? this.step * 0.1 : this.step);
      this.setValue(this.value + direction * stepSize, true);
    }, { passive: false });

    // Keyboard Accessibility
    this.element.addEventListener('keydown', (e) => {
      let handled = false;
      const multiplier = e.shiftKey ? 0.1 : 1.0;
      switch (e.key) {
        case 'ArrowUp':
        case 'ArrowRight':
          this.setValue(this.value + this.step * multiplier, true);
          handled = true;
          break;
        case 'ArrowDown':
        case 'ArrowLeft':
          this.setValue(this.value - this.step * multiplier, true);
          handled = true;
          break;
        case 'PageUp':
          this.setValue(this.value + this.step * 10 * multiplier, true);
          handled = true;
          break;
        case 'PageDown':
          this.setValue(this.value - this.step * 10 * multiplier, true);
          handled = true;
          break;
        case 'Home':
          this.setValue(this.min, true);
          handled = true;
          break;
        case 'End':
          this.setValue(this.max, true);
          handled = true;
          break;
      }
      if (handled) {
        this.lastUserInteractionTime = (typeof performance !== 'undefined' ? performance.now() : Date.now());
        e.preventDefault();
      }
    });

    // Double click to direct edit value
    this.element.addEventListener('dblclick', (e) => {
      if (e.target === this.assembly || (this.assembly && this.assembly.contains(e.target))) {
        // Reset to default value on assembly double-click
        this.lastUserInteractionTime = (typeof performance !== 'undefined' ? performance.now() : Date.now());
        this.setValue(this.defaultValue, true);
        return;
      }
      this._startDirectEntry();
    });
  }

  _startDirectEntry() {
    if (!this.directInput || !this.valueDisplay) return;
    this.valueDisplay.style.display = 'none';
    this.directInput.style.display = 'block';
    this.directInput.value = this.value;
    this.directInput.focus();
    this.directInput.select();

    const commit = () => {
      const parsed = parseFloat(this.directInput.value);
      if (!isNaN(parsed)) {
        this.lastUserInteractionTime = (typeof performance !== 'undefined' ? performance.now() : Date.now());
        this.setValue(parsed, true);
      }
      this.directInput.style.display = 'none';
      this.valueDisplay.style.display = 'block';
    };

    const cancel = () => {
      this.directInput.style.display = 'none';
      this.valueDisplay.style.display = 'block';
    };

    const onKey = (ev) => {
      if (ev.key === 'Enter') {
        commit();
        this.directInput.removeEventListener('keydown', onKey);
      } else if (ev.key === 'Escape') {
        cancel();
        this.directInput.removeEventListener('keydown', onKey);
      }
    };

    this.directInput.addEventListener('keydown', onKey);
    this.directInput.addEventListener('blur', commit, { once: true });
  }

  toNormalized(val) {
    if (this.isLog) {
      const minLog = Math.log(Math.max(1e-4, this.min));
      const maxLog = Math.log(this.max);
      const valLog = Math.log(Math.max(1e-4, val));
      return (valLog - minLog) / (maxLog - minLog);
    }
    return (val - this.min) / (this.max - this.min);
  }

  fromNormalized(norm) {
    norm = Math.max(0, Math.min(1, norm));
    if (this.isLog) {
      const minLog = Math.log(Math.max(1e-4, this.min));
      const maxLog = Math.log(this.max);
      return Math.exp(minLog + norm * (maxLog - minLog));
    }
    return this.min + norm * (this.max - this.min);
  }

  formatValue(val) {
    if (this.precision > 0) {
      return Number(val).toFixed(this.precision);
    }
    return Math.round(val).toString();
  }

  setValue(val, triggerChange = true) {
    // Prevent stale host echo from overriding user's active drag
    if (!triggerChange && this.isDragging) {
      return;
    }

    // Clamp to min/max
    let clamped = Math.max(this.min, Math.min(this.max, val));

    // Quantize to step if specified
    if (this.step > 0 && !this.isLog) {
      clamped = Math.round((clamped - this.min) / this.step) * this.step + this.min;
      clamped = Math.max(this.min, Math.min(this.max, clamped));
    }

    this.value = clamped;

    const norm = this.toNormalized(this.value);
    const angle = this.startAngle + norm * this.angleRange;

    // Update physical pointer rotation (hardware-accelerated CSS transform)
    if (this.cap) {
      this.cap.style.transform = `rotate(${angle}deg) translateZ(0)`;
    }

    // Update SVG progress arc directly without layout reflow
    if (this.fillCircle && this.arcLength) {
      const offset = this.arcLength * (1 - norm);
      this.fillCircle.style.strokeDashoffset = `${offset}`;
    }

    // Update digital readout with DOM text caching
    if (this.valueText) {
      const formatted = this.formatValue(this.value);
      if (this._lastFormatted !== formatted) {
        this._lastFormatted = formatted;
        this.valueText.textContent = formatted;
      }
    }

    if (this.element && this._lastAria !== clamped) {
      this._lastAria = clamped;
      this.element.setAttribute('aria-valuenow', clamped);
    }

    if (triggerChange && typeof this.onChange === 'function') {
      this.onChange(this.value);
    }
  }

  getValue() {
    return this.value;
  }
}
