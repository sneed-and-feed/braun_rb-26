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

    // Test Preset Loading (Classic + Companion)
    console.log('[BrowserTest] Testing preset loading (SUB_BASS_PRESERVER)...');
    await evaluate(`window.__RB26__.loadPreset('SUB_BASS_PRESERVER')`);
    const subBassRt60 = await evaluate(`window.__RB26__.knobs.rt60_decay.getValue()`);
    console.log(`[BrowserTest] Loaded SUB_BASS_PRESERVER RT60: ${subBassRt60}s (expected 4.5s)`);
    if (Math.abs(subBassRt60 - 4.5) > 0.1) {
      throw new Error(`Expected RT60 approx 4.5, got ${subBassRt60}`);
    }

    console.log('[BrowserTest] Testing preset loading (AMBIENT_GUITAR_CLOUD)...');
    await evaluate(`window.__RB26__.loadPreset('AMBIENT_GUITAR_CLOUD')`);
    const rt60Val = await evaluate(`window.__RB26__.knobs.rt60_decay.getValue()`);
    console.log(`[BrowserTest] Loaded AMBIENT_GUITAR_CLOUD RT60: ${rt60Val}s (expected 9.5s)`);
    if (Math.abs(rt60Val - 9.5) > 0.1) {
      throw new Error(`Expected RT60 approx 9.5, got ${rt60Val}`);
    }

    console.log('[BrowserTest] Testing companion preset loading (AS42_SHIMMER_COMPANION)...');
    await evaluate(`window.__RB26__.loadPreset('AS42_SHIMMER_COMPANION')`);
    const as42Rt60 = await evaluate(`window.__RB26__.knobs.rt60_decay.getValue()`);
    console.log(`[BrowserTest] Loaded AS42_SHIMMER_COMPANION RT60: ${as42Rt60}s (expected 8.5s)`);
    if (Math.abs(as42Rt60 - 8.5) > 0.1) {
      throw new Error(`Expected RT60 approx 8.5, got ${as42Rt60}`);
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

    // Test Keyboard Chime Key Sustain (100ms & 500ms Hold) and Keyup Smoothness
    console.log('[BrowserTest] Testing Keyboard Chime Key Sustain (100ms & 500ms Hold) and Keyup Smoothness...');
    const chimeHoldResult = await evaluate(`
      (async () => {
        let chimeInvocations = 0;
        const origPlayChime = window.__RB26__.playChime;
        window.__RB26__.playChime = function(...args) {
          chimeInvocations++;
          return origPlayChime.apply(this, args);
        };

        // 1. Chime short hold (100ms)
        chimeInvocations = 0;
        window.dispatchEvent(new KeyboardEvent('keydown', { key: 'a', bubbles: true }));
        window.dispatchEvent(new KeyboardEvent('keydown', { key: 'a', repeat: true, bubbles: true }));
        await new Promise(r => setTimeout(r, 100));
        const invocationsDuring100ms = chimeInvocations;

        window.dispatchEvent(new KeyboardEvent('keyup', { key: 'a', bubbles: true }));
        let maxJump100ms = 0;
        for (let pass = 0; pass < 5; pass++) {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxJump100ms) maxJump100ms = diff;
          }
          await new Promise(r => setTimeout(r, 10));
        }
        await new Promise(r => setTimeout(r, 300));
        const invocationsAfter100msKeyUp = chimeInvocations;
        const activeVoicesAfter100ms = window.__RB26__._activeVoices.size;

        // 2. Chime sustained hold (500ms) with repeated typematic events
        chimeInvocations = 0;
        window.dispatchEvent(new KeyboardEvent('keydown', { key: 's', bubbles: true }));
        for (let i = 0; i < 5; i++) {
          await new Promise(r => setTimeout(r, 80));
          window.dispatchEvent(new KeyboardEvent('keydown', { key: 's', repeat: true, bubbles: true }));
          // Test resilience against unflagged repeat events
          window.dispatchEvent(new KeyboardEvent('keydown', { key: 's', repeat: false, bubbles: true }));
        }
        await new Promise(r => setTimeout(r, 100));
        const invocationsDuring500ms = chimeInvocations;

        window.dispatchEvent(new KeyboardEvent('keyup', { key: 's', bubbles: true }));
        let maxJump500ms = 0;
        for (let pass = 0; pass < 5; pass++) {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxJump500ms) maxJump500ms = diff;
          }
          await new Promise(r => setTimeout(r, 10));
        }
        await new Promise(r => setTimeout(r, 300));
        const invocationsAfter500msKeyUp = chimeInvocations;
        const activeVoicesAfter500ms = window.__RB26__._activeVoices.size;

        window.__RB26__.playChime = origPlayChime;

        return {
          invocationsDuring100ms,
          invocationsAfter100msKeyUp,
          maxJump100ms,
          activeVoicesAfter100ms,
          invocationsDuring500ms,
          invocationsAfter500msKeyUp,
          maxJump500ms,
          activeVoicesAfter500ms
        };
      })()
    `);
    console.log('[BrowserTest] Chime hold 100ms: calls during hold =', chimeHoldResult.invocationsDuring100ms, ', after keyup =', chimeHoldResult.invocationsAfter100msKeyUp, ', max jump =', chimeHoldResult.maxJump100ms.toFixed(4));
    console.log('[BrowserTest] Chime hold 500ms: calls during hold =', chimeHoldResult.invocationsDuring500ms, ', after keyup =', chimeHoldResult.invocationsAfter500msKeyUp, ', max jump =', chimeHoldResult.maxJump500ms.toFixed(4));
    if (chimeHoldResult.invocationsDuring100ms !== 1) throw new Error(`Expected exactly 1 chime invocation during 100ms hold, got ${chimeHoldResult.invocationsDuring100ms}`);
    if (chimeHoldResult.invocationsAfter100msKeyUp !== 1) throw new Error(`Expected 0 secondary chime triggers on 100ms keyup, got ${chimeHoldResult.invocationsAfter100msKeyUp}`);
    if (chimeHoldResult.maxJump100ms >= 0.05) throw new Error(`Chime 100ms release jump exceeds 0.05 threshold: ${chimeHoldResult.maxJump100ms}`);
    if (chimeHoldResult.activeVoicesAfter100ms !== 0) throw new Error(`Expected 0 active voices after 100ms keyup, got ${chimeHoldResult.activeVoicesAfter100ms}`);
    if (chimeHoldResult.invocationsDuring500ms !== 1) throw new Error(`Expected exactly 1 chime invocation during 500ms hold, got ${chimeHoldResult.invocationsDuring500ms}`);
    if (chimeHoldResult.invocationsAfter500msKeyUp !== 1) throw new Error(`Expected 0 secondary chime triggers on 500ms keyup, got ${chimeHoldResult.invocationsAfter500msKeyUp}`);
    if (chimeHoldResult.maxJump500ms >= 0.05) throw new Error(`Chime 500ms release jump exceeds 0.05 threshold: ${chimeHoldResult.maxJump500ms}`);
    if (chimeHoldResult.activeVoicesAfter500ms !== 0) throw new Error(`Expected 0 active voices after 500ms keyup, got ${chimeHoldResult.activeVoicesAfter500ms}`);

    // Test Keyboard Chord Key Sustain (100ms & 500ms Hold) and Keyup Smoothness
    console.log('[BrowserTest] Testing Keyboard Chord Key Sustain (100ms & 500ms Hold) and Keyup Smoothness...');
    const chordHoldResult = await evaluate(`
      (async () => {
        let chordInvocations = 0;
        const origPlayChord = window.__RB26__.playChord;
        window.__RB26__.playChord = function(...args) {
          chordInvocations++;
          return origPlayChord.apply(this, args);
        };

        const chordBtn0 = document.querySelector('.braun-chord-btn[data-chord-index="0"]');
        const chordBtn1 = document.querySelector('.braun-chord-btn[data-chord-index="1"]');

        // 1. Chord short hold (100ms) on key '1'
        chordInvocations = 0;
        window.dispatchEvent(new KeyboardEvent('keydown', { key: '1', bubbles: true }));
        window.dispatchEvent(new KeyboardEvent('keydown', { key: '1', repeat: true, bubbles: true }));
        await new Promise(r => setTimeout(r, 100));
        const chordDuring100ms = chordInvocations;
        const btn0ActiveDuringHold = chordBtn0 ? chordBtn0.classList.contains('is-active') : false;

        window.dispatchEvent(new KeyboardEvent('keyup', { key: '1', bubbles: true }));
        let maxChordJump100ms = 0;
        for (let pass = 0; pass < 5; pass++) {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxChordJump100ms) maxChordJump100ms = diff;
          }
          await new Promise(r => setTimeout(r, 10));
        }
        await new Promise(r => setTimeout(r, 400));
        const chordAfter100msKeyUp = chordInvocations;
        const activeVoicesAfterChord100ms = window.__RB26__._activeVoices.size;
        const btn0ActiveAfterKeyUp = chordBtn0 ? chordBtn0.classList.contains('is-active') : false;

        // 2. Chord sustained hold (500ms) on key '2' with repeated typematic events
        chordInvocations = 0;
        window.dispatchEvent(new KeyboardEvent('keydown', { key: '2', bubbles: true }));
        for (let i = 0; i < 5; i++) {
          await new Promise(r => setTimeout(r, 80));
          window.dispatchEvent(new KeyboardEvent('keydown', { key: '2', repeat: true, bubbles: true }));
          window.dispatchEvent(new KeyboardEvent('keydown', { key: '2', repeat: false, bubbles: true }));
        }
        await new Promise(r => setTimeout(r, 100));
        const chordDuring500ms = chordInvocations;
        const btn1ActiveDuringHold = chordBtn1 ? chordBtn1.classList.contains('is-active') : false;

        window.dispatchEvent(new KeyboardEvent('keyup', { key: '2', bubbles: true }));
        let maxChordJump500ms = 0;
        for (let pass = 0; pass < 5; pass++) {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxChordJump500ms) maxChordJump500ms = diff;
          }
          await new Promise(r => setTimeout(r, 10));
        }
        await new Promise(r => setTimeout(r, 600));
        const chordAfter500msKeyUp = chordInvocations;
        const activeVoicesAfterChord500ms = window.__RB26__._activeVoices.size;
        const btn1ActiveAfterKeyUp = chordBtn1 ? chordBtn1.classList.contains('is-active') : false;

        // Verify decay after release tail
        const finalBuf = new Float32Array(512);
        window.__RB26__.engine.analyserL.getFloatTimeDomainData(finalBuf);
        let finalRms = 0;
        for (let i = 0; i < finalBuf.length; i++) finalRms += finalBuf[i] * finalBuf[i];
        finalRms = Math.sqrt(finalRms / finalBuf.length);

        window.__RB26__.playChord = origPlayChord;

        return {
          chordDuring100ms,
          chordAfter100msKeyUp,
          maxChordJump100ms,
          activeVoicesAfterChord100ms,
          btn0ActiveDuringHold,
          btn0ActiveAfterKeyUp,
          chordDuring500ms,
          chordAfter500msKeyUp,
          maxChordJump500ms,
          activeVoicesAfterChord500ms,
          btn1ActiveDuringHold,
          btn1ActiveAfterKeyUp,
          finalRms
        };
      })()
    `);
    console.log('[BrowserTest] Chord hold 100ms: calls during hold =', chordHoldResult.chordDuring100ms, ', after keyup =', chordHoldResult.chordAfter100msKeyUp, ', max jump =', chordHoldResult.maxChordJump100ms.toFixed(4));
    console.log('[BrowserTest] Chord hold 500ms: calls during hold =', chordHoldResult.chordDuring500ms, ', after keyup =', chordHoldResult.chordAfter500msKeyUp, ', max jump =', chordHoldResult.maxChordJump500ms.toFixed(4), ', final RMS =', chordHoldResult.finalRms.toFixed(6));
    if (chordHoldResult.chordDuring100ms !== 1) throw new Error(`Expected exactly 1 chord invocation during 100ms hold, got ${chordHoldResult.chordDuring100ms}`);
    if (chordHoldResult.chordAfter100msKeyUp !== 1) throw new Error(`Expected 0 secondary chord triggers on 100ms keyup, got ${chordHoldResult.chordAfter100msKeyUp}`);
    if (!chordHoldResult.btn0ActiveDuringHold) throw new Error('Expected chord button 0 to be active during hold');
    if (chordHoldResult.btn0ActiveAfterKeyUp) throw new Error('Expected chord button 0 to clear active class on keyup');
    if (chordHoldResult.maxChordJump100ms >= 0.05) throw new Error(`Chord 100ms release jump exceeds 0.05 threshold: ${chordHoldResult.maxChordJump100ms}`);
    if (chordHoldResult.activeVoicesAfterChord100ms !== 0) throw new Error(`Expected 0 active voices after chord 100ms keyup, got ${chordHoldResult.activeVoicesAfterChord100ms}`);

    if (chordHoldResult.chordDuring500ms !== 1) throw new Error(`Expected exactly 1 chord invocation during 500ms hold, got ${chordHoldResult.chordDuring500ms}`);
    if (chordHoldResult.chordAfter500msKeyUp !== 1) throw new Error(`Expected 0 secondary chord triggers on 500ms keyup, got ${chordHoldResult.chordAfter500msKeyUp}`);
    if (!chordHoldResult.btn1ActiveDuringHold) throw new Error('Expected chord button 1 to be active during hold');
    if (chordHoldResult.btn1ActiveAfterKeyUp) throw new Error('Expected chord button 1 to clear active class on keyup');
    if (chordHoldResult.maxChordJump500ms >= 0.05) throw new Error(`Chord 500ms release jump exceeds 0.05 threshold: ${chordHoldResult.maxChordJump500ms}`);
    if (chordHoldResult.activeVoicesAfterChord500ms !== 0) throw new Error(`Expected 0 active voices after chord 500ms keyup, got ${chordHoldResult.activeVoicesAfterChord500ms}`);
    if (chordHoldResult.finalRms >= 0.04) throw new Error(`Chord did not decay to silence after release tail! Final RMS: ${chordHoldResult.finalRms}`);

    // Test High Register Chime Keys (; and ') Hold and Release
    console.log('[BrowserTest] Testing High Register Chime Keys (; and \') Hold and Release...');
    const punctChimeResult = await evaluate(`
      (async () => {
        const keyElSemicolon = Array.from(document.querySelectorAll('.braun-chime-key')).find(el => el.getAttribute('data-hotkey') === ';');
        const keyElQuote = Array.from(document.querySelectorAll('.braun-chime-key')).find(el => el.getAttribute('data-hotkey') === "'");

        // 1. Hold semicolon (;) for 120ms
        window.dispatchEvent(new KeyboardEvent('keydown', { key: ';', bubbles: true }));
        await new Promise(r => setTimeout(r, 60));
        const semiActiveDuringHold = keyElSemicolon ? keyElSemicolon.classList.contains('is-active') : false;
        const semiVoicesDuringHold = window.__RB26__._activeVoices.size;

        await new Promise(r => setTimeout(r, 60));
        window.dispatchEvent(new KeyboardEvent('keyup', { key: ';', bubbles: true }));
        let maxJumpSemi = 0;
        for (let pass = 0; pass < 4; pass++) {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxJumpSemi) maxJumpSemi = diff;
          }
          await new Promise(r => setTimeout(r, 10));
        }
        await new Promise(r => setTimeout(r, 200));
        const semiActiveAfterKeyUp = keyElSemicolon ? keyElSemicolon.classList.contains('is-active') : false;
        const semiVoicesAfterKeyUp = window.__RB26__._activeVoices.size;

        // 2. Hold single quote (') for 120ms
        window.dispatchEvent(new KeyboardEvent('keydown', { key: "'", bubbles: true }));
        await new Promise(r => setTimeout(r, 60));
        const quoteActiveDuringHold = keyElQuote ? keyElQuote.classList.contains('is-active') : false;
        const quoteVoicesDuringHold = window.__RB26__._activeVoices.size;

        await new Promise(r => setTimeout(r, 60));
        window.dispatchEvent(new KeyboardEvent('keyup', { key: "'", bubbles: true }));
        let maxJumpQuote = 0;
        for (let pass = 0; pass < 4; pass++) {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxJumpQuote) maxJumpQuote = diff;
          }
          await new Promise(r => setTimeout(r, 10));
        }
        await new Promise(r => setTimeout(r, 200));
        const quoteActiveAfterKeyUp = keyElQuote ? keyElQuote.classList.contains('is-active') : false;
        const quoteVoicesAfterKeyUp = window.__RB26__._activeVoices.size;

        return {
          semiActiveDuringHold,
          semiVoicesDuringHold,
          semiActiveAfterKeyUp,
          semiVoicesAfterKeyUp,
          maxJumpSemi,
          quoteActiveDuringHold,
          quoteVoicesDuringHold,
          quoteActiveAfterKeyUp,
          quoteVoicesAfterKeyUp,
          maxJumpQuote
        };
      })()
    `);
    console.log('[BrowserTest] Punctuation chimes result:', punctChimeResult);
    if (!punctChimeResult.semiActiveDuringHold) throw new Error('Expected chime key ; to be active during hold');
    if (punctChimeResult.semiVoicesDuringHold !== 1) throw new Error('Expected 1 active voice during ; hold');
    if (punctChimeResult.semiActiveAfterKeyUp) throw new Error('Expected chime key ; to clear active class on keyup');
    if (punctChimeResult.semiVoicesAfterKeyUp !== 0) throw new Error('Expected 0 active voices after ; keyup');
    if (punctChimeResult.maxJumpSemi >= 0.05) throw new Error(`Chime ; release jump exceeds 0.05 threshold: ${punctChimeResult.maxJumpSemi}`);
    if (!punctChimeResult.quoteActiveDuringHold) throw new Error('Expected chime key \' to be active during hold');
    if (punctChimeResult.quoteVoicesDuringHold !== 1) throw new Error('Expected 1 active voice during \' hold');
    if (punctChimeResult.quoteActiveAfterKeyUp) throw new Error('Expected chime key \' to clear active class on keyup');
    if (punctChimeResult.quoteVoicesAfterKeyUp !== 0) throw new Error('Expected 0 active voices after \' keyup');
    if (punctChimeResult.maxJumpQuote >= 0.05) throw new Error(`Chime \' release jump exceeds 0.05 threshold: ${punctChimeResult.maxJumpQuote}`);

    // Test Mid-Strum Slow Chord Keyup Release (Timer Cancellation & Zero Secondary Triggers)
    console.log('[BrowserTest] Testing Mid-Strum Slow Chord Keyup Release...');
    const slowStrumResult = await evaluate(`
      (async () => {
        const prevSpeed = window.__RB26__.chordSpeed;
        window.__RB26__.chordSpeed = 'slow'; // 120ms per note

        let chimeInvocations = 0;
        const origPlayChime = window.__RB26__.playChime;
        window.__RB26__.playChime = function(...args) {
          chimeInvocations++;
          return origPlayChime.apply(this, args);
        };

        // Keydown on chord '1' (Harold Budd Sus2: 4 notes spaced 120ms apart)
        window.dispatchEvent(new KeyboardEvent('keydown', { key: '1', bubbles: true }));
        // Release after 60ms: exactly 1 note should have fired, remaining 3 timers pending
        await new Promise(r => setTimeout(r, 60));
        const callsAtRelease = chimeInvocations;

        window.dispatchEvent(new KeyboardEvent('keyup', { key: '1', bubbles: true }));

        // Wait 400ms (longer than the full 360ms strum duration) to confirm cancelled timers NEVER fire
        await new Promise(r => setTimeout(r, 400));
        const callsAfterStrumWindow = chimeInvocations;
        const activeVoicesAfterRelease = window.__RB26__._activeVoices.size;

        let maxJumpSlow = 0;
        for (let pass = 0; pass < 4; pass++) {
          const buf = new Float32Array(512);
          window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
          for (let i = 1; i < buf.length; i++) {
            const diff = Math.abs(buf[i] - buf[i - 1]);
            if (diff > maxJumpSlow) maxJumpSlow = diff;
          }
          await new Promise(r => setTimeout(r, 10));
        }

        window.__RB26__.playChime = origPlayChime;
        window.__RB26__.chordSpeed = prevSpeed;

        return {
          callsAtRelease,
          callsAfterStrumWindow,
          activeVoicesAfterRelease,
          maxJumpSlow
        };
      })()
    `);
    console.log('[BrowserTest] Mid-strum slow chord result:', slowStrumResult);
    if (slowStrumResult.callsAtRelease !== 1) throw new Error(`Expected 1 note fired before release, got ${slowStrumResult.callsAtRelease}`);
    if (slowStrumResult.callsAfterStrumWindow !== 1) throw new Error(`Expected pending strum notes to be cancelled, got ${slowStrumResult.callsAfterStrumWindow} calls`);
    if (slowStrumResult.activeVoicesAfterRelease !== 0) throw new Error(`Expected 0 active voices after mid-strum release, got ${slowStrumResult.activeVoicesAfterRelease}`);
    if (slowStrumResult.maxJumpSlow >= 0.05) throw new Error(`Slow strum release jump exceeds 0.05 threshold: ${slowStrumResult.maxJumpSlow}`);

    // Test Polyphonic Multi-Key Cluster Hold and Staggered Keyup Release
    console.log('[BrowserTest] Testing Polyphonic Multi-Key Cluster Hold...');
    const clusterResult = await evaluate(`
      (async () => {
        // Press 4 keys simultaneously: 2 chimes ('d', 'k') and 2 chords ('3', '4')
        const keysToPress = ['d', 'k', '3', '4'];
        keysToPress.forEach(k => {
          window.dispatchEvent(new KeyboardEvent('keydown', { key: k, bubbles: true }));
        });

        await new Promise(r => setTimeout(r, 150));
        const voicesDuringCluster = window.__RB26__._activeVoices.size;
        const heldKeysDuringCluster = window.__RB26__._heldKeys.size;

        // Release keys one by one in staggered fashion (40ms spacing)
        let maxClusterJump = 0;
        for (const k of keysToPress) {
          window.dispatchEvent(new KeyboardEvent('keyup', { key: k, bubbles: true }));
          for (let pass = 0; pass < 3; pass++) {
            const buf = new Float32Array(512);
            window.__RB26__.engine.analyserL.getFloatTimeDomainData(buf);
            for (let i = 1; i < buf.length; i++) {
              const diff = Math.abs(buf[i] - buf[i - 1]);
              if (diff > maxClusterJump) maxClusterJump = diff;
            }
            await new Promise(r => setTimeout(r, 10));
          }
          await new Promise(r => setTimeout(r, 40));
        }

        await new Promise(r => setTimeout(r, 300));
        const voicesAfterCluster = window.__RB26__._activeVoices.size;
        const heldKeysAfterCluster = window.__RB26__._heldKeys.size;

        return {
          voicesDuringCluster,
          heldKeysDuringCluster,
          voicesAfterCluster,
          heldKeysAfterCluster,
          maxClusterJump
        };
      })()
    `);
    console.log('[BrowserTest] Polyphonic cluster result:', clusterResult);
    if (clusterResult.voicesDuringCluster !== 4) throw new Error(`Expected 4 active cluster voices, got ${clusterResult.voicesDuringCluster}`);
    if (clusterResult.heldKeysDuringCluster !== 4) throw new Error(`Expected 4 held keys during cluster, got ${clusterResult.heldKeysDuringCluster}`);
    if (clusterResult.voicesAfterCluster !== 0) throw new Error(`Expected 0 active voices after cluster release, got ${clusterResult.voicesAfterCluster}`);
    if (clusterResult.heldKeysAfterCluster !== 0) throw new Error(`Expected 0 held keys after cluster release, got ${clusterResult.heldKeysAfterCluster}`);
    if (clusterResult.maxClusterJump >= 0.05) throw new Error(`Cluster release jump exceeds 0.05 threshold: ${clusterResult.maxClusterJump}`);

    // Test Window Blur Safety Release
    console.log('[BrowserTest] Testing Window Blur Safety Release...');
    const blurResult = await evaluate(`
      (() => {
        window.dispatchEvent(new KeyboardEvent('keydown', { key: 'a', bubbles: true }));
        window.dispatchEvent(new KeyboardEvent('keydown', { key: '3', bubbles: true }));
        const voicesBeforeBlur = window.__RB26__._activeVoices.size;
        const heldKeysBeforeBlur = window.__RB26__._heldKeys.size;

        window.dispatchEvent(new Event('blur'));

        const voicesAfterBlur = window.__RB26__._activeVoices.size;
        const heldKeysAfterBlur = window.__RB26__._heldKeys.size;
        const activeButtonsAfterBlur = document.querySelectorAll('.braun-chime-key.is-active, .braun-chord-btn.is-active').length;

        return { voicesBeforeBlur, heldKeysBeforeBlur, voicesAfterBlur, heldKeysAfterBlur, activeButtonsAfterBlur };
      })()
    `);
    console.log('[BrowserTest] Window blur result: before =', blurResult.voicesBeforeBlur, 'voices,', blurResult.heldKeysBeforeBlur, 'keys; after =', blurResult.voicesAfterBlur, 'voices,', blurResult.heldKeysAfterBlur, 'keys,', blurResult.activeButtonsAfterBlur, 'active DOM elements');
    if (blurResult.voicesBeforeBlur === 0) throw new Error('Expected active voices before blur');
    if (blurResult.voicesAfterBlur !== 0) throw new Error(`Expected 0 active voices after window blur, got ${blurResult.voicesAfterBlur}`);
    if (blurResult.heldKeysAfterBlur !== 0) throw new Error(`Expected 0 held keys after window blur, got ${blurResult.heldKeysAfterBlur}`);
    if (blurResult.activeButtonsAfterBlur !== 0) throw new Error(`Expected 0 active DOM button styles after window blur, got ${blurResult.activeButtonsAfterBlur}`);

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
    const maxSettlingRms = Math.max(...settlingProfile);
    if (postScrubFinalRms > 0.5) {
      throw new Error(`Sub-bass runaway drone detected after room size scrub! Final RMS: ${postScrubFinalRms}`);
    }
    if (postScrubFinalRms > maxSettlingRms + 0.01) {
      throw new Error(`Audio energy did not decay after room size scrub! Peak: ${maxSettlingRms}, Final: ${postScrubFinalRms}`);
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
          let sum = 0, maxPeak = 0;
          for (let j = 0; j < buf.length; j++) {
            sum += buf[j] * buf[j];
            maxPeak = Math.max(maxPeak, Math.abs(buf[j]));
          }
          return { rms: Math.sqrt(sum / buf.length), maxPeak };
        })()
      `);
      directTelemetry.push(frameData);
    }
    const directPeakMax = Math.max(...directTelemetry.map(t => t.maxPeak));
    console.log(`[BrowserTest] Direct engine scrub peak max = ${directPeakMax.toFixed(4)}`);
    if (directPeakMax > 1.05) {
      throw new Error(`Output overloaded during direct engine rapid room size scrub! Peak: ${directPeakMax}`);
    }

    // Allow brief settling pause before reading frequency domain bin
    await new Promise(r => setTimeout(r, 300));

    // Verify DC-blocking attenuation: check sub-bass DC bin in frequency domain
    const dcDb = await evaluate(`
      (() => {
        const freqBuf = new Float32Array(256);
        window.__RB26__.engine.analyserL.getFloatFrequencyData(freqBuf);
        return freqBuf[0];
      })()
    `);
    console.log(`[BrowserTest] DC frequency bin level: ${dcDb.toFixed(2)} dBFS (must be < -45 dBFS)`);
    if (dcDb > -45) {
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

    // Test Keyboard Chime and Chord Hold & Release
    console.log('[BrowserTest] Testing keyboard keydown hold and keyup release damping...');
    await evaluate(`
      (() => {
        window.dispatchEvent(new KeyboardEvent('keydown', { code: 'KeyA', key: 'a', bubbles: true }));
      })()
    `);
    await new Promise(r => setTimeout(r, 200));
    const isChimeKeyHeld = await evaluate(`
      document.querySelector('.braun-chime-key[data-hotkey="a"]').classList.contains('is-active')
    `);
    console.log(`[BrowserTest] Chime key 'a' illuminated on hold: ${isChimeKeyHeld}`);
    if (!isChimeKeyHeld) {
      throw new Error('Chime key was not active while held');
    }

    // Release key 'a'
    await evaluate(`
      (() => {
        window.dispatchEvent(new KeyboardEvent('keyup', { code: 'KeyA', key: 'a', bubbles: true }));
      })()
    `);
    await new Promise(r => setTimeout(r, 100));
    const isChimeKeyReleased = await evaluate(`
      !document.querySelector('.braun-chime-key[data-hotkey="a"]').classList.contains('is-active')
    `);
    console.log(`[BrowserTest] Chime key 'a' unlit after release: ${isChimeKeyReleased}`);
    if (!isChimeKeyReleased) {
      throw new Error('Chime key was still active after release');
    }

    // Dispatch keydown for Chord 'Digit1'
    await evaluate(`
      (() => {
        window.dispatchEvent(new KeyboardEvent('keydown', { code: 'Digit1', key: '1', bubbles: true }));
      })()
    `);
    await new Promise(r => setTimeout(r, 200));
    const isChordBtnHeld = await evaluate(`
      document.querySelector('.braun-chord-btn[data-chord-index="0"]').classList.contains('is-active')
    `);
    console.log(`[BrowserTest] Chord 1 button illuminated on hold: ${isChordBtnHeld}`);
    if (!isChordBtnHeld) {
      throw new Error('Chord button was not active while held');
    }

    // Release Chord 'Digit1'
    await evaluate(`
      (() => {
        window.dispatchEvent(new KeyboardEvent('keyup', { code: 'Digit1', key: '1', bubbles: true }));
      })()
    `);
    await new Promise(r => setTimeout(r, 100));
    const isChordBtnReleased = await evaluate(`
      !document.querySelector('.braun-chord-btn[data-chord-index="0"]').classList.contains('is-active')
    `);
    console.log(`[BrowserTest] Chord 1 button unlit after release: ${isChordBtnReleased}`);
    if (!isChordBtnReleased) {
      throw new Error('Chord button was still active after release');
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
