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

    // Trigger first chord button
    await evaluate(`document.querySelector('.braun-chord-btn[data-chord-index="0"]').click()`);

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

    // Test Calibrated Reset
    console.log('[BrowserTest] Testing Calibrated Reset...');
    await evaluate(`document.getElementById('btn-reset-all').click()`);
    const defaultRt60 = await evaluate(`window.__RB26__.knobs.rt60_decay.getValue()`);
    console.log(`[BrowserTest] Reset RT60 value: ${defaultRt60}s (expected 6.5s)`);
    if (Math.abs(defaultRt60 - 6.5) > 0.1) throw new Error(`Expected reset RT60 6.5, got ${defaultRt60}`);

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
