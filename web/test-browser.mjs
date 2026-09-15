/**
 * @file test-browser.mjs
 * @brief Headless browser E2E test verifying RB-26 Web showcase in Microsoft Edge
 * Strict Dieter Rams functionalist austerity (zero emojis in logs and DOM).
 */

import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import os from 'node:os';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const PORT = 3826;
const CDP_PORT = 9223;

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png'
};

// 1. Start HTTP Static Server for rb-26/web/
const server = http.createServer((req, res) => {
  let reqPath = req.url.split('?')[0];
  if (reqPath === '/' || reqPath === '') reqPath = '/index.html';

  const safePath = path.normalize(reqPath).replace(/^(\.\.[\/\\])+/, '');
  const filePath = path.join(__dirname, safePath);

  fs.stat(filePath, (err, stats) => {
    if (err || !stats.isFile()) {
      res.writeHead(404, { 'Content-Type': 'text/plain' });
      res.end('Not Found');
      return;
    }

    const ext = path.extname(filePath).toLowerCase();
    const contentType = MIME_TYPES[ext] || 'application/octet-stream';

    res.writeHead(200, {
      'Content-Type': contentType,
      'Access-Control-Allow-Origin': '*'
    });
    fs.createReadStream(filePath).pipe(res);
  });
});

let serverStarted = false;

async function runBrowserTest() {
  try {
    await new Promise((resolve, reject) => {
      server.once('error', reject);
      server.listen(PORT, () => {
        serverStarted = true;
        resolve();
      });
    });
    console.log(`[BrowserTest] Static server listening on port ${PORT}`);
  } catch (err) {
    if (err.code === 'EADDRINUSE') {
      console.log(`[BrowserTest] Port ${PORT} already active, connecting to existing instance.`);
    } else {
      throw err;
    }
  }

  const edgePath = 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
  const userDataDir = path.join(os.tmpdir(), `rb26_edge_test_${Date.now()}`);

  const edgeProc = spawn(edgePath, [
    '--headless',
    `--remote-debugging-port=${CDP_PORT}`,
    `--user-data-dir=${userDataDir}`,
    '--no-first-run',
    '--no-default-browser-check',
    '--autoplay-policy=no-user-gesture-required',
    `http://localhost:${PORT}/index.html`
  ], { stdio: 'pipe' });

  let ws = null;

  try {
    // Wait for Edge CDP to become available
    let connected = false;
    let targetTab = null;
    for (let i = 0; i < 20; i++) {
      await new Promise((r) => setTimeout(r, 400));
      try {
        const res = await fetch(`http://127.0.0.1:${CDP_PORT}/json/list`);
        const tabs = await res.json();
        targetTab = tabs.find((t) => t.url.includes(`localhost:${PORT}`)) || tabs[0];
        if (targetTab && targetTab.webSocketDebuggerUrl) {
          connected = true;
          break;
        }
      } catch (err) {}
    }

    if (!connected || !targetTab) {
      throw new Error('Failed to connect to headless Edge via CDP');
    }

    ws = new WebSocket(targetTab.webSocketDebuggerUrl);
    await new Promise((resolve) => {
      ws.onopen = resolve;
    });

    let id = 100;
    const pending = new Map();
    const pageErrors = [];

    ws.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      if (msg.method === 'Runtime.exceptionThrown') {
        pageErrors.push(msg.params.exceptionDetails);
      }
      if (msg.id && pending.has(msg.id)) {
        const { resolve, reject } = pending.get(msg.id);
        pending.delete(msg.id);
        if (msg.error) reject(msg.error);
        else resolve(msg.result);
      }
    };

    const call = (method, params = {}) => {
      return new Promise((resolve, reject) => {
        const msgId = ++id;
        pending.set(msgId, { resolve, reject });
        ws.send(JSON.stringify({ id: msgId, method, params }));
      });
    };

    const evaluate = async (expr) => {
      const res = await call('Runtime.evaluate', {
        expression: expr,
        returnByValue: true,
        awaitPromise: true
      });
      if (res.exceptionDetails) {
        throw new Error('Eval Exception: ' + JSON.stringify(res.exceptionDetails));
      }
      return res.result.value;
    };

    await call('Page.enable');
    await call('Runtime.enable');

    // Wait 1.5s for DOM and modules to finish initialization
    await new Promise((r) => setTimeout(r, 1500));

    // Test Assertions
    console.log('[BrowserTest] Running DOM and UI state assertions...');

    const docTitle = await evaluate('document.title');
    console.log(`[BrowserTest] Page Title: "${docTitle}"`);
    if (docTitle !== 'BRAUN · RB-26 STUDIO REVERBERATOR') {
      throw new Error(`Unexpected document title: ${docTitle}`);
    }

    const appExists = await evaluate('Boolean(window.__RB26__)');
    console.log(`[BrowserTest] window.__RB26__ initialized: ${appExists}`);
    if (!appExists) throw new Error('window.__RB26__ is not defined');

    const knobCount = await evaluate('Object.keys(window.__RB26__.knobs).length');
    console.log(`[BrowserTest] Initialized knobs count: ${knobCount} / 21`);
    if (knobCount < 21) {
      throw new Error(`Expected at least 21 knobs, got ${knobCount}`);
    }

    // Verify Canvas size
    const canvasDimensions = await evaluate(`({
      width: document.getElementById('scope-canvas').width,
      height: document.getElementById('scope-canvas').height
    })`);
    console.log(`[BrowserTest] CRT Canvas Dimensions: ${canvasDimensions.width}x${canvasDimensions.height}`);
    if (canvasDimensions.width === 0 || canvasDimensions.height === 0) {
      throw new Error('Canvas dimensions are 0');
    }

    // Test Zero Emojis in DOM
    console.log('[BrowserTest] Auditing DOM for zero emojis...');
    const emojiCount = await evaluate(`
      (() => {
        const html = document.body.innerHTML;
        const emojiRegex = /[\\u{1F300}-\\u{1F9FF}]|[\\u{2600}-\\u{26FF}]|[\\u{2700}-\\u{27BF}]/u;
        const matches = html.match(new RegExp(emojiRegex, 'gu'));
        return matches ? matches.length : 0;
      })()
    `);
    console.log(`[BrowserTest] Emojis detected in DOM: ${emojiCount}`);
    if (emojiCount !== 0) {
      throw new Error(`Found ${emojiCount} emojis in DOM; strict Dieter Rams austerity requires 0`);
    }

    // Test Theme Switch to Dark
    console.log('[BrowserTest] Testing Dark theme toggle...');
    await evaluate(`
      document.getElementById('select-theme').value = 'dark';
      document.getElementById('select-theme').dispatchEvent(new Event('change'));
    `);
    const themeAttr = await evaluate('document.documentElement.getAttribute("data-theme")');
    console.log(`[BrowserTest] html data-theme attribute: "${themeAttr}"`);
    if (themeAttr !== 'dark') throw new Error(`Expected data-theme="dark", got "${themeAttr}"`);

    // Test Theme Persistence in localStorage
    const savedTheme = await evaluate(`localStorage.getItem('braun_rb26_theme')`);
    console.log(`[BrowserTest] localStorage braun_rb26_theme: "${savedTheme}"`);
    if (savedTheme !== 'dark') throw new Error(`Expected localStorage theme to be "dark", got "${savedTheme}"`);

    // Test Preset Loading (Classic + Non-Euclidean)
    console.log('[BrowserTest] Testing preset loading (AMBIENT_GUITAR_CLOUD)...');
    await evaluate(`window.__RB26__.loadPreset('AMBIENT_GUITAR_CLOUD')`);
    const rt60Val = await evaluate(`window.__RB26__.knobs.rt60_decay.getValue()`);
    console.log(`[BrowserTest] Loaded AMBIENT_GUITAR_CLOUD RT60: ${rt60Val}s (expected 16s)`);
    if (Math.abs(rt60Val - 16.0) > 0.1) {
      throw new Error(`Expected RT60 approx 16.0, got ${rt60Val}`);
    }

    console.log('[BrowserTest] Testing Non-Euclidean preset loading (POINCARE_CAVITY)...');
    await evaluate(`window.__RB26__.loadPreset('POINCARE_CAVITY')`);
    const poincareRt60 = await evaluate(`window.__RB26__.knobs.rt60_decay.getValue()`);
    console.log(`[BrowserTest] Loaded POINCARE_CAVITY RT60: ${poincareRt60}s (expected 8.5s)`);
    if (Math.abs(poincareRt60 - 8.5) > 0.1) {
      throw new Error(`Expected RT60 approx 8.5, got ${poincareRt60}`);
    }

    // Test A/B Comparison Buffer
    console.log('[BrowserTest] Testing A/B Comparison Buffer toggle...');
    const bufferState1 = await evaluate(`window.__RB26__.abBuffer.toggle()`);
    console.log(`[BrowserTest] A/B Active Buffer switched to: ${bufferState1}`);
    if (bufferState1 !== 'B') throw new Error(`Expected active buffer B, got ${bufferState1}`);

    const bufferState2 = await evaluate(`window.__RB26__.abBuffer.toggle()`);
    console.log(`[BrowserTest] A/B Active Buffer switched back to: ${bufferState2}`);
    if (bufferState2 !== 'A') throw new Error(`Expected active buffer A, got ${bufferState2}`);

    // Verify power is OFF (standby) by default
    console.log('[BrowserTest] Verifying default power state is standby...');
    const defaultPower = await evaluate(`window.__RB26__.isPowered`);
    console.log(`[BrowserTest] Default power state: ${defaultPower}`);
    if (defaultPower !== false) throw new Error('Expected power to be OFF by default');

    // Turn power on
    console.log('[BrowserTest] Powering on the unit...');
    await evaluate(`document.getElementById('btn-power').click()`);
    const poweredOn = await evaluate(`window.__RB26__.isPowered`);
    if (!poweredOn) throw new Error('Expected power to turn ON after clicking power button');

    // Test Audition Button trigger (Functionalist Nomenclature)
    console.log('[BrowserTest] Testing Dirac impulse audition button...');
    await evaluate(`document.getElementById('btn-audition-impulse').click()`);

    // Test Deck 07 Exciter Chime Keys
    console.log('[BrowserTest] Testing Deck 07 Chime Strip...');
    const chimeKeyCount = await evaluate(`document.querySelectorAll('.braun-chime-key').length`);
    console.log(`[BrowserTest] Chime keys count: ${chimeKeyCount} (expected 11)`);
    if (chimeKeyCount !== 11) throw new Error(`Expected 11 chime keys, got ${chimeKeyCount}`);

    // Trigger first chime key
    await evaluate(`document.querySelector('.braun-chime-key[data-hotkey="a"]').dispatchEvent(new PointerEvent('pointerdown', { clientX: 100, clientY: 100 }))`);

    // Test 12 Signature Chord Buttons
    console.log('[BrowserTest] Testing Deck 07 Chord Buttons...');
    const chordBtnCount = await evaluate(`document.querySelectorAll('.braun-chord-btn').length`);
    console.log(`[BrowserTest] Chord buttons count: ${chordBtnCount} (expected 12)`);
    if (chordBtnCount !== 12) throw new Error(`Expected 12 chord buttons, got ${chordBtnCount}`);

    // Trigger first chord button via programmatic click
    await evaluate(`document.querySelector('.braun-chord-btn[data-chord-index="0"]').click()`);

    // Test Chord Button Tactile Pointer and Duplicate Click Suppression
    console.log('[BrowserTest] Testing Chord Button Tactile Pointer and Duplicate Click Suppression...');
    const pointerTriggerResult = await evaluate(`
      (async () => {
        const btn = document.querySelector('.braun-chord-btn[data-chord-index="0"]');
        let playChordCalls = 0;
        const origPlayChord = window.__RB26__.playChord;
        window.__RB26__.playChord = function(...args) {
          playChordCalls++;
          return origPlayChord.apply(this, args);
        };

        // 1. Pointerdown starts chord
        btn.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true }));
        const isActiveOnDown = btn.classList.contains('is-active');
        const activeVoicesOnDown = window.__RB26__._activeChordButtons ? window.__RB26__._activeChordButtons.size : 0;

        await new Promise(r => setTimeout(r, 60));

        // 2. Pointerup releases chord
        btn.dispatchEvent(new PointerEvent('pointerup', { bubbles: true }));
        const isActiveOnUp = btn.classList.contains('is-active');
        const activeVoicesOnUp = window.__RB26__._activeChordButtons ? window.__RB26__._activeChordButtons.size : 0;

        // 3. Browser synthesizes click event immediately after pointerup
        btn.dispatchEvent(new MouseEvent('click', { bubbles: true }));

        // 4. Test sustained hold (> 500ms) to verify duplicate click suppression does not expire on release
        btn.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true }));
        await new Promise(r => setTimeout(r, 550));
        btn.dispatchEvent(new PointerEvent('pointerup', { bubbles: true }));
        btn.dispatchEvent(new MouseEvent('click', { bubbles: true }));

        // Restore original method
        window.__RB26__.playChord = origPlayChord;

        return {
          playChordCalls,
          isActiveOnDown,
          activeVoicesOnDown,
          isActiveOnUp,
          activeVoicesOnUp
        };
      })()
    `);
    console.log('[BrowserTest] Chord tactile pointer result:', pointerTriggerResult);
    if (!pointerTriggerResult.isActiveOnDown) throw new Error('Chord button must become active on pointerdown');
    if (pointerTriggerResult.activeVoicesOnDown !== 1) throw new Error('Chord button must register active voice on pointerdown');
    if (pointerTriggerResult.isActiveOnUp) throw new Error('Chord button must clear active state on pointerup');
    if (pointerTriggerResult.activeVoicesOnUp !== 0) throw new Error('Chord button must release active voice on pointerup');
    if (pointerTriggerResult.playChordCalls !== 2) {
      throw new Error(`Expected exactly 2 playChord invocations (1 short hold + 1 long hold, 0 duplicate clicks), got ${pointerTriggerResult.playChordCalls}`);
    }

    // Test Poisson Generator Toggle
    console.log('[BrowserTest] Testing Poisson Generator Toggle...');
    await evaluate(`document.getElementById('btn-poisson-toggle').click()`);
    const isPoissonActive = await evaluate(`window.__RB26__.isPoissonRunning`);
    console.log(`[BrowserTest] Poisson generator active: ${isPoissonActive}`);
    if (!isPoissonActive) throw new Error('Expected Poisson generator to be running');

    // Turn Poisson off
    await evaluate(`document.getElementById('btn-poisson-toggle').click()`);

    // Test Master WAV Recorder existence
    console.log('[BrowserTest] Verifying WAV Recorder Button...');
    const hasWavBtn = await evaluate(`Boolean(document.getElementById('btn-record-wav'))`);
    if (!hasWavBtn) throw new Error('Missing #btn-record-wav');

    // Test Vector Modulation Pad existence in Deck 05
    console.log('[BrowserTest] Verifying Deck 05 Vector Modulation Pad...');
    const hasVectorPad = await evaluate(`Boolean(window.__RB26__.vectorPad)`);
    console.log(`[BrowserTest] VectorPad initialized: ${hasVectorPad}`);
    if (!hasVectorPad) throw new Error('window.__RB26__.vectorPad is not defined');

    // Test CRT Display Mode Switch to WAVE
    console.log('[BrowserTest] Testing CRT Display Mode Switch to WAVE...');
    await evaluate(`
      const waveBtn = document.querySelector('.braun-mode-btn[data-mode="WAVE"]');
      if (waveBtn) waveBtn.click();
    `);
    const crtMode = await evaluate(`window.__RB26__.display.mode`);
    console.log(`[BrowserTest] CRT Display Mode: ${crtMode}`);
    if (crtMode !== 'WAVE') throw new Error(`Expected CRT Display mode to be WAVE, got ${crtMode}`);

    // Test Preset Selection and Keyboard Focus Management
    console.log('[BrowserTest] Testing Preset Select Focus and Keyboard Chime Triggering...');
    await evaluate(`
      const select = document.getElementById('select-preset');
      select.focus();
      select.value = 'ETHEREAL_SYNTH_PAD';
      select.dispatchEvent(new Event('change'));
      // Dispatch keydown for 'a' while select is active to verify focus release and note trigger
      const keyEvt = new KeyboardEvent('keydown', { key: 'a', bubbles: true });
      window.dispatchEvent(keyEvt);
    `);
    const keyIsActive = await evaluate(`
      document.querySelector('.braun-chime-key[data-hotkey="a"]').classList.contains('is-active')
    `);
    console.log(`[BrowserTest] Chime key triggered after preset change: ${keyIsActive}`);
    if (!keyIsActive) throw new Error('Expected chime key A to trigger after preset change');

    // Release key 'a'
    await evaluate(`
      window.dispatchEvent(new KeyboardEvent('keyup', { key: 'a', bubbles: true }));
    `);

    // Test Spacebar Dirac Impulse Hotkey
    console.log('[BrowserTest] Testing Spacebar Dirac Impulse Hotkey...');
    await evaluate(`
      window.dispatchEvent(new KeyboardEvent('keydown', { key: ' ', bubbles: true }));
    `);
    // Allow CSS transition (0.12s ease) to fully settle to steady-state
    await new Promise((r) => setTimeout(r, 180));

    const impulseStatus = await evaluate(`
      (() => {
        const audBtn = document.getElementById('btn-audition-impulse');
        const d7Btn = document.getElementById('btn-pulse-dirac');
        const audStyle = window.getComputedStyle(audBtn);
        const d7Style = window.getComputedStyle(d7Btn);
        return {
          audActive: audBtn.classList.contains('is-active'),
          d7Active: d7Btn.classList.contains('is-active'),
          audBg: audStyle.backgroundColor,
          d7Bg: d7Style.backgroundColor
        };
      })()
    `);
    console.log('[BrowserTest] Spacebar impulse visual status:', impulseStatus);
    if (!impulseStatus.audActive) throw new Error('Expected Dirac impulse audition button to be active on spacebar');
    if (!impulseStatus.d7Active) throw new Error('Expected Deck 07 Dirac impulse button to be active on spacebar');
    if (impulseStatus.audBg !== 'rgb(238, 89, 43)') {
      throw new Error(`Expected audition impulse button to render Braun safety orange, got: ${impulseStatus.audBg}`);
    }
    if (impulseStatus.d7Bg !== 'rgb(238, 89, 43)') {
      throw new Error(`Expected Deck 07 Dirac button to render Braun safety orange, got: ${impulseStatus.d7Bg}`);
    }

    // Sample audio energy in reverb tank to verify band-limited excitation bloom
    const impulseEnergy = await evaluate(`
      (() => {
        const buf = new Float32Array(512);
        window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
        let rms = 0;
        for (let i = 0; i < buf.length; i++) rms += buf[i] * buf[i];
        return Math.sqrt(rms / buf.length);
      })()
    `);
    console.log(`[BrowserTest] Dirac impulse excitation RMS: ${impulseEnergy.toFixed(6)}`);
    if (impulseEnergy <= 0.0001) throw new Error('Expected Dirac impulse to audibly excite the reverb tank');

    await evaluate(`
      window.dispatchEvent(new KeyboardEvent('keyup', { key: ' ', bubbles: true }));
    `);
    // Allow CSS transition (0.12s ease) to fully revert to idle styling
    await new Promise((r) => setTimeout(r, 180));

    const impulsePostStatus = await evaluate(`
      (() => {
        const audBtn = document.getElementById('btn-audition-impulse');
        const d7Btn = document.getElementById('btn-pulse-dirac');
        const audStyle = window.getComputedStyle(audBtn);
        const d7Style = window.getComputedStyle(d7Btn);
        return {
          isStillActive: audBtn.classList.contains('is-active') || d7Btn.classList.contains('is-active'),
          audBg: audStyle.backgroundColor,
          d7Bg: d7Style.backgroundColor
        };
      })()
    `);
    if (impulsePostStatus.isStillActive) {
      throw new Error('Expected Dirac impulse buttons to clear active state on spacebar keyup');
    }
    if (impulsePostStatus.audBg === 'rgb(238, 89, 43)') {
      throw new Error('Expected audition impulse button background to revert on spacebar keyup');
    }

    // Test Typematic Key Repeat Filter and Keyup Release
    console.log('[BrowserTest] Testing Typematic Key Repeat Filter and Keyup Release...');
    await evaluate(`
      // Send keydown for 's'
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 's', bubbles: true }));
      // Send repeat keydown (simulating OS typematic repeat)
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 's', repeat: true, bubbles: true }));
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 's', repeat: true, bubbles: true }));
    `);
    const activeVoiceCount = await evaluate(`window.__RB26__._activeVoices.size`);
    const sKeyHeld = await evaluate(`window.__RB26__._heldKeys.has('s')`);
    console.log(`[BrowserTest] Active voice count during hold (repeat ignored): ${activeVoiceCount}, held: ${sKeyHeld}`);
    if (activeVoiceCount !== 1) throw new Error(`Expected exactly 1 active voice during hold, got ${activeVoiceCount}`);

    // Now release key 's'
    await evaluate(`
      window.dispatchEvent(new KeyboardEvent('keyup', { key: 's', bubbles: true }));
    `);
    const activeVoiceCountAfterRelease = await evaluate(`window.__RB26__._activeVoices.size`);
    const sKeyHeldAfter = await evaluate(`window.__RB26__._heldKeys.has('s')`);
    console.log(`[BrowserTest] Active voice count after release: ${activeVoiceCountAfterRelease}, held: ${sKeyHeldAfter}`);
    if (activeVoiceCountAfterRelease !== 0) throw new Error('Expected active voices to be 0 after keyup');
    if (sKeyHeldAfter) throw new Error('Expected held keys to not contain s after keyup');

    // Set dry/wet mix to 0 (100% dry) to measure direct voice synthesis attack & release envelopes without 8.5s reverb tank circulation
    const prevDryWet = await evaluate(`window.__RB26__.knobs.dry_wet_mix.getValue()`);
    await evaluate(`window.__RB26__.knobs.dry_wet_mix.setValue(0, true)`);
    await new Promise(r => setTimeout(r, 100));

    const baselineRms = await evaluate(`
      (() => {
        const buf = new Float32Array(512);
        window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
        let sum = 0;
        for (let i = 0; i < buf.length; i++) sum += buf[i] * buf[i];
        return Math.sqrt(sum / buf.length);
      })()
    `);
    console.log(`[BrowserTest] Baseline RMS before note press: ${baselineRms.toFixed(6)}`);

    // Test Note Press Transient Smoothness (Zero Initial Jump / Anti-Click Attack)
    console.log('[BrowserTest] Testing Note Press Transient Smoothness (Zero Initial Jump)...');
    const pressTelemetry = await evaluate(`
      (async () => {
        await new Promise(r => setTimeout(r, 100));
        window.dispatchEvent(new KeyboardEvent('keydown', { key: 'd', bubbles: true }));
        await new Promise(r => setTimeout(r, 20));

        const buf = new Float32Array(512);
        window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
        let maxJump = 0;
        for (let i = 1; i < buf.length; i++) {
          const diff = Math.abs(buf[i] - buf[i - 1]);
          if (diff > maxJump) maxJump = diff;
        }
        return { maxJump, sampleCount: buf.length };
      })()
    `);
    console.log(`[BrowserTest] Note press max sample jump: ${pressTelemetry.maxJump.toFixed(4)} (must be < 0.20)`);
    if (pressTelemetry.maxJump >= 0.20) {
      throw new Error(`Note press transient pop detected! Max sample jump: ${pressTelemetry.maxJump}`);
    }

    // Test Note Release Transient Smoothness (Anti-Click setTargetAtTime Decay)
    console.log('[BrowserTest] Testing Note Release Transient Smoothness (Anti-Click setTargetAtTime Decay)...');
    await new Promise(r => setTimeout(r, 150));
    const releaseTelemetry = await evaluate(`
      (async () => {
        const beforeBuf = new Float32Array(512);
        window.__RB26__.engine.analyserL.getFloatTimeDomainData(beforeBuf);
        let beforeRms = 0;
        for (let i = 0; i < beforeBuf.length; i++) beforeRms += beforeBuf[i] * beforeBuf[i];
        beforeRms = Math.sqrt(beforeRms / beforeBuf.length);

        window.dispatchEvent(new KeyboardEvent('keyup', { key: 'd', bubbles: true }));

        // Continuous sampling across consecutive frames immediately upon keyup to catch any transient step discontinuity
        let maxReleaseJump = 0;
        for (let pass = 0; pass < 4; pass++) {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxReleaseJump) maxReleaseJump = diff;
          }
          await new Promise(r => setTimeout(r, 10));
        }

        await new Promise(r => setTimeout(r, 600));
        const tailBuf = new Float32Array(512);
        window.__RB26__.engine.analyserL.getFloatTimeDomainData(tailBuf);
        let tailRms = 0;
        for (let i = 0; i < tailBuf.length; i++) tailRms += tailBuf[i] * tailBuf[i];
        tailRms = Math.sqrt(tailRms / tailBuf.length);

        return { beforeRms, maxReleaseJump, tailRms };
      })()
    `);
    console.log(`[BrowserTest] Note release pre-RMS: ${releaseTelemetry.beforeRms.toFixed(4)}, max jump: ${releaseTelemetry.maxReleaseJump.toFixed(4)}, post-tail RMS: ${releaseTelemetry.tailRms.toFixed(6)}`);
    if (releaseTelemetry.maxReleaseJump >= 0.15) {
      throw new Error(`Note release pop discontinuity detected! Max sample jump: ${releaseTelemetry.maxReleaseJump}`);
    }
    if (releaseTelemetry.tailRms >= releaseTelemetry.beforeRms * 0.25) {
      throw new Error(`Audio did not decay sufficiently after note release tail! Start: ${releaseTelemetry.beforeRms}, Final: ${releaseTelemetry.tailRms}`);
    }

    // Test Rapid Staccato Key Churn (< 15ms note re-triggering under high audio load)
    console.log('[BrowserTest] Testing Rapid Staccato Key Churn (< 15ms retriggering)...');
    const staccatoTelemetry = await evaluate(`
      (async () => {
        let maxStaccatoJump = 0;
        for (let iter = 0; iter < 5; iter++) {
          window.dispatchEvent(new KeyboardEvent('keydown', { key: 'g', bubbles: true }));
          await new Promise(r => setTimeout(r, 12));
          window.dispatchEvent(new KeyboardEvent('keyup', { key: 'g', bubbles: true }));
          await new Promise(r => setTimeout(r, 12));

          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxStaccatoJump) maxStaccatoJump = diff;
          }
        }
        await new Promise(r => setTimeout(r, 400));
        return { maxStaccatoJump };
      })()
    `);
    console.log(`[BrowserTest] Rapid staccato max jump: ${staccatoTelemetry.maxStaccatoJump.toFixed(4)} (must be < 0.20)`);
    if (staccatoTelemetry.maxStaccatoJump >= 0.20) {
      throw new Error(`Rapid staccato pop detected! Max jump: ${staccatoTelemetry.maxStaccatoJump}`);
    }

    // Test Polyphonic Chord Release Smoothness
    console.log('[BrowserTest] Testing Polyphonic Chord Release Smoothness...');
    await evaluate(`
      window.dispatchEvent(new KeyboardEvent('keydown', { key: '1', bubbles: true }));
    `);
    await new Promise(r => setTimeout(r, 120));
    const chordReleaseTelemetry = await evaluate(`
      (async () => {
        window.dispatchEvent(new KeyboardEvent('keyup', { key: '1', bubbles: true }));
        let maxChordJump = 0;
        for (let pass = 0; pass < 4; pass++) {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxChordJump) maxChordJump = diff;
          }
          await new Promise(r => setTimeout(r, 10));
        }

        await new Promise(r => setTimeout(r, 800));
        const finalBuf = new Float32Array(512);
        window.__RB26__.engine.analyserL.getFloatTimeDomainData(finalBuf);
        let finalRms = 0;
        for (let i = 0; i < finalBuf.length; i++) finalRms += finalBuf[i] * finalBuf[i];
        finalRms = Math.sqrt(finalRms / finalBuf.length);
        return { maxChordJump, finalRms };
      })()
    `);
    console.log(`[BrowserTest] Chord release max jump: ${chordReleaseTelemetry.maxChordJump.toFixed(4)}, final RMS: ${chordReleaseTelemetry.finalRms.toFixed(6)}`);
    if (chordReleaseTelemetry.maxChordJump >= 0.20) {
      throw new Error(`Chord release pop discontinuity detected! Max jump: ${chordReleaseTelemetry.maxChordJump}`);
    }
    if (chordReleaseTelemetry.finalRms >= 0.04) {
      throw new Error(`Chord did not decay to silence after release tail! Final RMS: ${chordReleaseTelemetry.finalRms}`);
    }

    // Restore previous dry/wet mix
    await evaluate(`window.__RB26__.knobs.dry_wet_mix.setValue(${prevDryWet}, true)`);

    // Test CRT Monitor High-Performance Rendering Benchmark (< 4.0ms per frame)
    console.log('[BrowserTest] Testing CRT Monitor Rendering Performance Benchmark (< 4.0ms budget)...');
    const crtBenchmark = await evaluate(`
      (() => {
        const display = window.__RB26__.display;
        const modes = ['WAVE', 'EDC', 'LISSAJOUS', 'SPECTRUM'];
        const results = {};

        for (const mode of modes) {
          display.setMode(mode);
          for (let i = 0; i < 3; i++) display.draw();

          const tStart = performance.now();
          const iterations = 30;
          for (let i = 0; i < iterations; i++) {
            display.draw();
          }
          const tElapsed = performance.now() - tStart;
          results[mode] = tElapsed / iterations;
        }

        display.setMode('WAVE');
        return results;
      })()
    `);

    for (const [mode, avgMs] of Object.entries(crtBenchmark)) {
      console.log(`[BrowserTest] CRT Display ${mode} mode avg render time: ${avgMs.toFixed(3)} ms (budget < 4.0ms)`);
      if (avgMs >= 4.0) {
        throw new Error(`CRT Display rendering too slow in ${mode} mode! ${avgMs.toFixed(3)} ms exceeds 4.0ms budget`);
      }
    }

    // Test Vector Pad Bidirectional Knob Sync
    console.log('[BrowserTest] Testing Vector Pad Bidirectional Knob Sync...');
    await evaluate(`
      window.__RB26__.vectorPad.setCoordinates(0.80, 0.60, true);
    `);
    const syncedDepth = await evaluate(`window.__RB26__.knobs.tail_mod_depth.getValue()`);
    console.log(`[BrowserTest] Vector pad Y=0.60 synced knob tail_mod_depth: ${syncedDepth}% (expected 60%)`);
    if (syncedDepth !== 60) throw new Error(`Expected tail_mod_depth 60, got ${syncedDepth}`);

    // Test Calibrated Reset
    console.log('[BrowserTest] Testing Calibrated Reset...');
    await evaluate(`document.getElementById('btn-reset-all').click()`);
    await new Promise(r => setTimeout(r, 100));
    const defaultRt60 = await evaluate(`window.__RB26__.knobs.rt60_decay.getValue()`);
    console.log(`[BrowserTest] Reset RT60 value: ${defaultRt60}s (expected 6.5s)`);
    if (Math.abs(defaultRt60 - 6.5) > 0.1) throw new Error(`Expected reset RT60 6.5, got ${defaultRt60}`);

    // Test Room Size Parameter Slew Smoothing & Dual-Bank Crossfading
    console.log('[BrowserTest] Testing Room Size live parameter smoothing and dual-bank crossfading...');
    const hasDualBanks = await evaluate(`
      Boolean(window.__RB26__.engine.fdnDelaysA?.length === 8 &&
              window.__RB26__.engine.fdnDelaysB?.length === 8 &&
              window.__RB26__.engine.fdnXfadeA?.length === 8 &&
              window.__RB26__.engine.fdnXfadeB?.length === 8)
    `);
    console.log(`[BrowserTest] Dual-bank FDN architecture initialized: ${hasDualBanks}`);
    if (!hasDualBanks) throw new Error('Expected dual-bank FDN delay architecture (A & B)');

    const initialBank = await evaluate(`window.__RB26__.engine._activeFdnBank`);
    await evaluate(`window.__RB26__.knobs.room_size.setValue(180, true)`);
    await new Promise(r => setTimeout(r, 120)); // Allow requestAnimationFrame smoother to step
    const targetRoomSize = await evaluate(`window.__RB26__._targetRoomSize`);
    const activeBankAfter = await evaluate(`window.__RB26__.engine._activeFdnBank`);
    console.log(`[BrowserTest] Target room size: ${targetRoomSize} (expected 1.8), active bank: ${initialBank} -> ${activeBankAfter}`);
    if (Math.abs(targetRoomSize - 1.8) > 0.01) throw new Error(`Expected targetRoomSize 1.8, got ${targetRoomSize}`);
    if (activeBankAfter === initialBank) throw new Error('Expected active FDN bank to switch on room size change');

    // Test Rapid Room Size Scrubbing Stress Test under Audio Excitation (Sub-Bass Bound & No Tape Scratch)
    console.log('[BrowserTest] Testing rapid room size scrubbing under continuous audio excitation...');
    await evaluate(`
      document.getElementById('btn-audition-kick').click();
      window.__RB26__.engine.triggerDirac();
    `);

    const scrubValues = [30, 195, 45, 175, 25, 160, 50, 140, 75, 100];
    const scrubTelemetry = [];

    for (const val of scrubValues) {
      await evaluate(`window.__RB26__.knobs.room_size.setValue(${val}, true)`);
      await new Promise(r => setTimeout(r, 30));

      const frameData = await evaluate(`
        (() => {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          let sumSq = 0;
          let maxPeak = 0;
          for (let i = 0; i < buf.length; i++) {
            const abs = Math.abs(buf[i]);
            if (abs > maxPeak) maxPeak = abs;
            sumSq += buf[i] * buf[i];
          }
          const rms = Math.sqrt(sumSq / buf.length);
          return { rms, maxPeak };
        })()
      `);
      scrubTelemetry.push(frameData);
    }

    const peakMax = Math.max(...scrubTelemetry.map(t => t.maxPeak));
    const scrubFinalRms = scrubTelemetry[scrubTelemetry.length - 1].rms;
    console.log(`[BrowserTest] Rapid scrub telemetry: peak max = ${peakMax.toFixed(4)}, final RMS = ${scrubFinalRms.toFixed(6)}`);

    for (let i = 0; i < scrubTelemetry.length; i++) {
      if (scrubTelemetry[i].maxPeak > 1.05) {
        throw new Error(`Output saturated/overloaded during rapid room size scrub! Peak: ${scrubTelemetry[i].maxPeak}`);
      }
      if (Number.isNaN(scrubTelemetry[i].rms) || !Number.isFinite(scrubTelemetry[i].rms)) {
        throw new Error('NaN/Infinity encountered in audio telemetry during rapid room size scrub');
      }
    }

    // Allow reverb tail to decay over 2.0s and verify contractive dissipation and zero runaway
    const settlingProfile = [];
    for (let i = 0; i < 4; i++) {
      await new Promise(r => setTimeout(r, 500));
      const rms = await evaluate(`
        (() => {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          let sum = 0;
          for (let j = 0; j < buf.length; j++) sum += buf[j] * buf[j];
          return Math.sqrt(sum / buf.length);
        })()
      `);
      settlingProfile.push(rms);
    }
    console.log(`[BrowserTest] Post-scrub settling profile: ${settlingProfile.map(x => x.toFixed(6)).join(' -> ')}`);
    const postScrubFinalRms = settlingProfile[settlingProfile.length - 1];
    if (postScrubFinalRms > 0.5) {
      throw new Error(`Sub-bass runaway drone detected after room size scrub! Final RMS: ${postScrubFinalRms}`);
    }
    if (postScrubFinalRms >= scrubFinalRms) {
      throw new Error(`Audio energy did not decay after room size scrub! Start: ${scrubFinalRms}, Final: ${postScrubFinalRms}`);
    }

    // Test Direct Engine Rapid Room Size Scrubbing at High Frequency (Direct AudioParam Crossfade Queueing)
    console.log('[BrowserTest] Testing direct engine rapid room size scrubbing (15ms intervals, audio excitation)...');
    await evaluate(`
      window.__RB26__.engine.triggerNoiseBurst();
    `);
    const directValues = [0.3, 1.9, 0.4, 1.8, 0.5, 1.6, 0.6, 1.3, 1.0];
    const directTelemetry = [];
    for (const val of directValues) {
      await evaluate(`window.__RB26__.engine.setParam('roomSize', ${val})`);
      await new Promise(r => setTimeout(r, 15));
      const frameData = await evaluate(`
        (() => {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          let sumSq = 0;
          let maxPeak = 0;
          for (let i = 0; i < buf.length; i++) {
            const abs = Math.abs(buf[i]);
            if (abs > maxPeak) maxPeak = abs;
            sumSq += buf[i] * buf[i];
          }
          const rms = Math.sqrt(sumSq / buf.length);
          return { rms, maxPeak };
        })()
      `);
      directTelemetry.push(frameData);
    }
    const directPeakMax = Math.max(...directTelemetry.map(t => t.maxPeak));
    console.log(`[BrowserTest] Direct engine scrub peak max = ${directPeakMax.toFixed(4)}`);
    if (directPeakMax > 1.05) {
      throw new Error(`Output overloaded during direct engine rapid room size scrub! Peak: ${directPeakMax}`);
    }

    // Verify DC-blocking attenuation: check sub-bass DC bin in frequency domain
    const dcDb = await evaluate(`
      (() => {
        const freqBuf = new Float32Array(256);
        window.__RB26__.engine.analyserL.getFloatFrequencyData(freqBuf);
        return freqBuf[0];
      })()
    `);
    console.log(`[BrowserTest] DC frequency bin level: ${dcDb.toFixed(2)} dBFS (must be < -50 dBFS)`);
    if (dcDb > -50) {
      throw new Error(`DC accumulation detected in feedback loop! Level: ${dcDb.toFixed(2)} dBFS`);
    }

    // Test Low-End Matrix Loop Gain Boundedness strictly < 0.88
    console.log('[BrowserTest] Verifying low-end modal matrix loop gain is strictly < 0.88 across extreme damping settings...');
    await evaluate(`
      window.__RB26__.knobs.damping_low.setValue(2.5, true);
      window.__RB26__.knobs.rt60_decay.setValue(30.0, true);
      window.__RB26__.engine.setParam('freezeHold', true);
    `);
    const maxModalFb = await evaluate(`
      Math.max(...window.__RB26__.engine.modalFeedbackGains.map(g => g.gain.value))
    `);
    console.log(`[BrowserTest] Maximum modal feedback gain under extreme settings: ${maxModalFb.toFixed(6)} (must be < 0.88)`);
    if (maxModalFb >= 0.88) {
      throw new Error(`Modal matrix loop gain not strictly bounded < 0.88! Found: ${maxModalFb}`);
    }
    await evaluate(`
      window.__RB26__.knobs.damping_low.setValue(1.0, true);
      window.__RB26__.knobs.rt60_decay.setValue(6.5, true);
      window.__RB26__.engine.setParam('freezeHold', false);
    `);

    // Test Piano Chord Trigger & FDN Feedback Loop Stability
    console.log('[BrowserTest] Testing felt piano chord audition and FDN stability...');
    await evaluate(`document.getElementById('btn-audition-piano').click()`);
    // Sample audio energy over 1.5s to verify contractive decay and zero runaway
    const energySamples = [];
    for (let i = 0; i < 5; i++) {
      await new Promise(r => setTimeout(r, 300));
      const rms = await evaluate(`
        (() => {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          let sum = 0;
          for (let j = 0; j < buf.length; j++) sum += buf[j] * buf[j];
          return Math.sqrt(sum / buf.length);
        })()
      `);
      energySamples.push(rms);
    }
    console.log(`[BrowserTest] Post-chord audio RMS profile: ${energySamples.map(x => x.toFixed(6)).join(' -> ')}`);
    const finalRms = energySamples[energySamples.length - 1];
    if (finalRms > 1.0) {
      throw new Error(`FDN feedback loop runaway detected! Final RMS: ${finalRms}`);
    }

    // Test Power Cycle Flush
    console.log('[BrowserTest] Testing Power Cycle flush (Power OFF -> Standby)...');
    await evaluate(`document.getElementById('btn-power').click()`);
    await new Promise(r => setTimeout(r, 100)); // Allow 25ms thread drain + margin
    const isPowerOff = await evaluate(`window.__RB26__.isPowered === false && window.__RB26__.engine.isPowered === false`);
    console.log(`[BrowserTest] Power is standby: ${isPowerOff}`);
    if (!isPowerOff) throw new Error('Expected unit to be in standby after clicking power button');

    const fdnGainsZeroed = await evaluate(`
      window.__RB26__.engine.fdnFeedbackGains.every(g => Math.abs(g.gain.value) < 1e-6)
    `);
    console.log(`[BrowserTest] FDN feedback gains zeroed on power off: ${fdnGainsZeroed}`);
    if (!fdnGainsZeroed) throw new Error('Expected FDN feedback gains to be zeroed in standby');

    const standbyRms = await evaluate(`
      (() => {
        const buf = new Float32Array(512);
        window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
        let sum = 0;
        for (let j = 0; j < buf.length; j++) sum += buf[j] * buf[j];
        return Math.sqrt(sum / buf.length);
      })()
    `);
    console.log(`[BrowserTest] Standby output RMS: ${standbyRms}`);
    if (standbyRms > 1e-4) {
      throw new Error(`Trapped audio remained circulating in standby! RMS: ${standbyRms}`);
    }

    console.log('[BrowserTest] Testing Power Cycle restore (Power ON)...');
    await evaluate(`document.getElementById('btn-power').click()`);
    await new Promise(r => setTimeout(r, 100)); // Allow async ctx.resume and power restore
    const isPowerRestored = await evaluate(`window.__RB26__.isPowered === true && window.__RB26__.engine.isPowered === true`);
    console.log(`[BrowserTest] Power restored: ${isPowerRestored}`);
    if (!isPowerRestored) throw new Error('Expected power to be ON after clicking power button');

    const fdnGainsRestored = await evaluate(`
      window.__RB26__.engine.fdnFeedbackGains.every(g => g.gain.value > 0.5)
    `);
    console.log(`[BrowserTest] FDN feedback gains restored on power on: ${fdnGainsRestored}`);
    if (!fdnGainsRestored) throw new Error('Expected FDN feedback gains to be restored when powered on');

    if (pageErrors.length > 0) {
      throw new Error(`Page exceptions encountered: ${JSON.stringify(pageErrors)}`);
    }

    console.log('[PASS] ALL HEADLESS BROWSER ASSERTIONS PASSED SUCCESSFULLY!');
  } finally {
    if (ws) ws.close();
    edgeProc.kill();
    if (serverStarted) server.close();
  }
}

runBrowserTest().catch((err) => {
  console.error('[FAIL] Browser Test Failed:', err);
  process.exit(1);
});
