#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "PluginEditor.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

#if JUCE_WINDOWS
#include <windows.h>
#endif

// BinaryData inclusion for embedded web assets
#if __has_include(<BinaryData.h>)
#include <BinaryData.h>
#define RB26_HAS_BINARY_DATA 1
#elif __has_include("BinaryData.h")
#include "BinaryData.h"
#define RB26_HAS_BINARY_DATA 1
#else
#define RB26_HAS_BINARY_DATA 0
#endif

namespace {

// Clean embedded fallback HTML page adhering to Dieter Rams Braun design language
static const char* kEmbeddedBraunFallbackHtml = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>BRAUN RB-26 Studio Reverb</title>
<style>
  :root {
    --chassis-bg: #141517;
    --card-bg: #1C1D20;
    --border-color: #2E3035;
    --text-main: #ECEBE4;
    --text-dim: #7E8085;
    --braun-orange: #EE592B;
    --phosphor-green: #24FF6A;
    --knob-cap: #26282C;
    --knob-border: #3A3C42;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; }
  body {
    background: var(--chassis-bg);
    color: var(--text-main);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    padding: 16px;
    height: 100vh;
    overflow-y: auto;
  }
  header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-bottom: 2px solid var(--border-color);
    padding-bottom: 12px;
    margin-bottom: 16px;
  }
  .title-group h1 { font-size: 20px; font-weight: 700; letter-spacing: 2px; }
  .title-group p { font-size: 11px; color: var(--text-dim); letter-spacing: 1px; }
  .badge { background: var(--braun-orange); color: #fff; padding: 4px 10px; font-size: 11px; font-weight: 700; border-radius: 2px; letter-spacing: 1px; }
  .crt-container {
    background: #090B0A;
    border: 2px solid #202622;
    border-radius: 4px;
    padding: 12px;
    margin-bottom: 16px;
    display: flex;
    gap: 16px;
    align-items: center;
  }
  canvas { background: #000; border: 1px solid #162419; border-radius: 2px; }
  .meters { display: flex; flex-direction: column; gap: 6px; flex: 1; font-size: 11px; font-family: monospace; }
  .meter-row { display: flex; align-items: center; gap: 8px; }
  .meter-bar { flex: 1; height: 8px; background: #18201B; border-radius: 2px; overflow: hidden; }
  .meter-fill { height: 100%; width: 0%; background: var(--phosphor-green); transition: width 0.05s; }
  .rack-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
    gap: 12px;
  }
  .deck {
    background: var(--card-bg);
    border: 1px solid var(--border-color);
    border-radius: 4px;
    padding: 12px;
  }
  .deck-header {
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 1.5px;
    color: var(--braun-orange);
    border-bottom: 1px solid var(--border-color);
    padding-bottom: 6px;
    margin-bottom: 10px;
    text-transform: uppercase;
  }
  .knob-row {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(70px, 1fr));
    gap: 10px;
    text-align: center;
  }
  .knob-unit { display: flex; flex-direction: column; align-items: center; gap: 4px; }
  .knob-label { font-size: 10px; color: var(--text-dim); letter-spacing: 0.5px; text-transform: uppercase; }
  .knob-val { font-size: 11px; font-weight: 600; color: var(--text-main); }
  input[type=range] {
    width: 100%;
    accent-color: var(--braun-orange);
    cursor: pointer;
  }
  select, button {
    background: var(--knob-cap);
    color: var(--text-main);
    border: 1px solid var(--border-color);
    padding: 4px 8px;
    border-radius: 2px;
    font-size: 11px;
    cursor: pointer;
  }
  select option {
    background: var(--card-bg);
    color: var(--text-main);
  }
  .exciter-bar {
    background: var(--card-bg);
    border: 1px solid var(--border-color);
    border-radius: 4px;
    padding: 12px;
    margin-bottom: 16px;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .exciter-cluster { display: flex; gap: 8px; flex-wrap: wrap; align-items: center; }
  .exciter-btn {
    background: var(--knob-cap);
    color: var(--text-main);
    border: 1px solid var(--border-color);
    padding: 6px 12px;
    font-size: 11px;
    font-weight: 600;
    cursor: pointer;
    border-radius: 2px;
  }
  .exciter-btn:active { background: var(--braun-orange); color: #fff; }
  .chime-btn {
    flex: 1;
    min-width: 28px;
    height: 38px;
    background: #202226;
    border: 1px solid var(--border-color);
    color: var(--text-main);
    font-size: 11px;
    font-weight: 700;
    cursor: pointer;
    border-radius: 2px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
  }
  .chime-btn:active { background: var(--braun-orange); color: #fff; }
</style>
</head>
<body>
<header>
  <div class="title-group">
    <h1>BRAUN RB-26</h1>
    <p>STUDIO REVERBERATION UNIT &mdash; WENIGER, ABER BESSER</p>
  </div>
  <div style="display:flex; gap:8px; align-items:center;">
    <button id="btnPower" style="background:var(--knob-cap); color:var(--text-main); padding:4px 10px; font-size:11px; font-weight:700; border-radius:2px; cursor:pointer; border:1px solid var(--border-color); letter-spacing:1px;">STANDBY</button>
    <div class="badge">STANDBY</div>
  </div>
</header>

<div class="crt-container">
  <canvas id="scopeCanvas" width="240" height="90"></canvas>
  <div class="meters">
    <div class="meter-row"><span>IN L</span><div class="meter-bar"><div id="mInL" class="meter-fill"></div></div><span id="vInL">-inf</span></div>
    <div class="meter-row"><span>IN R</span><div class="meter-bar"><div id="mInR" class="meter-fill"></div></div><span id="vInR">-inf</span></div>
    <div class="meter-row"><span>OUT L</span><div class="meter-bar"><div id="mOutL" class="meter-fill"></div></div><span id="vOutL">-inf</span></div>
    <div class="meter-row"><span>OUT R</span><div class="meter-bar"><div id="mOutR" class="meter-fill"></div></div><span id="vOutR">-inf</span></div>
    <div class="meter-row"><span>CORR</span><div class="meter-bar"><div id="mCorr" class="meter-fill" style="background:#24B8FF;"></div></div><span id="vCorr">+1.0</span></div>
  </div>
</div>

<div class="exciter-bar">
  <div class="deck-header">DECK 07 &mdash; ON-BOARD ACOUSTIC EXCITER &amp; AUDITION</div>
  <div class="exciter-cluster">
    <span style="font-size:10px; color:var(--text-dim); text-transform:uppercase;">LABORATORY:</span>
    <button class="exciter-btn" onclick="emitExciter({type:'dirac'})">DIRAC IMPULSE [SPACE]</button>
    <button class="exciter-btn" onclick="emitExciter({type:'pink',duration:40})">PINK BURST [40 MS]</button>
    <button class="exciter-btn" onclick="emitExciter({type:'hammer'})">ACOUSTIC HAMMER [78 HZ]</button>
    <button class="exciter-btn" id="btnPoisson" onclick="togglePoisson()">POISSON CLOCK</button>
  </div>
  <div class="exciter-cluster" style="margin-top:4px;">
    <span style="font-size:10px; color:var(--text-dim); text-transform:uppercase;">CHIMES (A-'):</span>
    <div style="display:flex; gap:4px; flex:1;" id="chimeRow"></div>
  </div>
  <div class="exciter-cluster" style="margin-top:4px;">
    <span style="font-size:10px; color:var(--text-dim); text-transform:uppercase;">CHORDS (1-=):</span>
    <div style="display:flex; gap:4px; flex-wrap:wrap; flex:1;" id="chordRow"></div>
  </div>
</div>

<div class="rack-grid" id="deckContainer">
  <!-- Controls rendered dynamically via JS -->
</div>
)html"
R"html(
<script>
const paramsMeta = [
  { id: 'input_trim_db', webId: 'inputTrimDb', label: 'Input Trim', deck: 'Input & Pre-Delay', min: -18, max: 18, def: 0, unit: 'dB' },
  { id: 'pre_delay_ms', webId: 'preDelayMs', label: 'Pre-Delay', deck: 'Input & Pre-Delay', min: 0, max: 500, def: 20, unit: 'ms' },
  { id: 'diffusion_density', webId: 'diffusionDensity', label: 'Diffusion', deck: 'Input & Pre-Delay', min: 0, max: 1, def: 0.75, unit: '%' },
  { id: 'dry_wet_mix', webId: 'dryWetMix', label: 'Dry/Wet', deck: 'Input & Pre-Delay', min: 0, max: 1, def: 0.35, unit: '%' },
  { id: 'early_late_mix', webId: 'earlyLateMix', label: 'Early/Late', deck: 'Input & Pre-Delay', min: 0, max: 1, def: 0.50, unit: '%' },

  { id: 'low_crossover_hz', webId: 'lowCrossoverHz', label: 'Crossover', deck: 'Low Modal Matrix', min: 60, max: 400, def: 180, unit: 'Hz' },
  { id: 'bass_rt60_mult', webId: 'bassRt60Mult', label: 'Bass Mult', deck: 'Low Modal Matrix', min: 0.2, max: 4.0, def: 1.0, unit: 'x' },
  { id: 'punch_ducking', webId: 'punchDucking', label: 'Punch Duck', deck: 'Low Modal Matrix', min: 0, max: 1, def: 0.40, unit: '%' },
  { id: 'sub_mono_hz', webId: 'subMonoHz', label: 'Sub Mono', deck: 'Low Modal Matrix', min: 20, max: 250, def: 120, unit: 'Hz' },

  { id: 'room_size', webId: 'roomSize', label: 'Room Size', deck: 'FDN Tank', min: 0.1, max: 4.0, def: 0.65, unit: '' },
  { id: 'decay_rt60_sec', webId: 'decayRt60Sec', label: 'Decay Time', deck: 'FDN Tank', min: 0.2, max: 30.0, def: 3.5, unit: 's' },
  { id: 'high_damping_hz', webId: 'highDampingHz', label: 'Damping', deck: 'FDN Tank', min: 1000, max: 20000, def: 1800, unit: 'Hz' },
  { id: 'freeze_hold', webId: 'freezeHold', label: 'Freeze', deck: 'FDN Tank', min: 0, max: 1, def: 0, unit: '', isBool: true },

  { id: 'shimmer_send', webId: 'shimmerSend', label: 'Shimmer Send', deck: 'Pitch Diffusion', min: 0, max: 1, def: 0.30, unit: '%' },
  { id: 'dimmer_send', webId: 'dimmerSend', label: 'Dimmer Send', deck: 'Pitch Diffusion', min: 0, max: 1, def: 0.25, unit: '%' },
  { id: 'shimmer_interval', webId: 'shimmerInterval', label: 'Shimmer Int', deck: 'Pitch Diffusion', min: 0, max: 2, def: 1, unit: '', isChoice: true, choices: ['+7 st', '+12 st', '+24 st'] },
  { id: 'dimmer_interval', webId: 'dimmerInterval', label: 'Dimmer Int', deck: 'Pitch Diffusion', min: 0, max: 2, def: 2, unit: '', isChoice: true, choices: ['-2 st', '-7 st', '-12 st'] },
  { id: 'pitch_blend', webId: 'pitchBlend', label: 'Pitch Blend', deck: 'Pitch Diffusion', min: -1, max: 1, def: 0, unit: '' },
  { id: 'pitch_feedback', webId: 'pitchFeedback', label: 'Pitch FB', deck: 'Pitch Diffusion', min: 0, max: 0.95, def: 0.50, unit: '%' },
  { id: 'pitch_delay_ms', webId: 'pitchDelayMs', label: 'Pitch Delay', deck: 'Pitch Diffusion', min: 20, max: 500, def: 150, unit: 'ms' },

  { id: 'tail_mod_rate_hz', webId: 'tailModRateHz', label: 'Mod Rate', deck: 'Tail Bloom & Mod', min: 0.05, max: 5.0, def: 0.85, unit: 'Hz' },
  { id: 'tail_mod_depth_ms', webId: 'tailModDepthMs', label: 'Mod Depth', deck: 'Tail Bloom & Mod', min: 0, max: 5.0, def: 1.2, unit: 'ms' },
  { id: 'tail_bloom_ms', webId: 'tailBloomMs', label: 'Bloom Attack', deck: 'Tail Bloom & Mod', min: 20, max: 300, def: 85, unit: 'ms' },

  { id: 'stereo_width', webId: 'stereoWidth', label: 'Width', deck: 'Master Bus', min: 0, max: 2.0, def: 1.0, unit: '%' },
  { id: 'output_trim_db', webId: 'outputTrimDb', label: 'Trim', deck: 'Master Bus', min: -24, max: 12, def: 0, unit: 'dB' },
  { id: 'limiter_enable', webId: 'limiterEnable', label: 'Limiter', deck: 'Master Bus', min: 0, max: 1, def: 1, unit: '', isBool: true }
];

// Build deck UI
const container = document.getElementById('deckContainer');
const decks = {};
paramsMeta.forEach(p => {
  if (!decks[p.deck]) {
    const d = document.createElement('div');
    d.className = 'deck';
    d.innerHTML = `<div class="deck-header">${p.deck}</div><div class="knob-row" id="deck_${p.deck.replace(/[^a-zA-Z]/g, '')}"></div>`;
    container.appendChild(d);
    decks[p.deck] = d.querySelector('.knob-row');
  }
  const ku = document.createElement('div');
  ku.className = 'knob-unit';
  if (p.isChoice) {
    ku.innerHTML = `
      <div class="knob-label">${p.label}</div>
      <select id="ctrl_${p.id}">
        ${p.choices.map((c, i) => `<option value="${i}" ${i === p.def ? 'selected' : ''}>${c}</option>`).join('')}
      </select>
    `;
    const sel = ku.querySelector('select');
    sel.addEventListener('change', () => emitParam(p.id, parseFloat(sel.value)));
  } else if (p.isBool) {
    ku.innerHTML = `
      <div class="knob-label">${p.label}</div>
      <button id="ctrl_${p.id}">${p.def ? 'ON' : 'OFF'}</button>
    `;
    const btn = ku.querySelector('button');
    let state = !!p.def;
    btn.addEventListener('click', () => {
      state = !state;
      btn.textContent = state ? 'ON' : 'OFF';
      btn.style.color = state ? 'var(--braun-orange)' : 'var(--text-main)';
      emitParam(p.id, state ? 1 : 0);
    });
  } else {
    ku.innerHTML = `
      <div class="knob-label">${p.label}</div>
      <input type="range" id="ctrl_${p.id}" min="${p.min}" max="${p.max}" step="${(p.max - p.min) / 100}" value="${p.def}">
      <div class="knob-val" id="val_${p.id}">${p.def} ${p.unit}</div>
    `;
    const rng = ku.querySelector('input');
    const valDisp = ku.querySelector('.knob-val');
    rng.addEventListener('input', () => {
      valDisp.textContent = parseFloat(rng.value).toFixed(2) + ' ' + p.unit;
      emitParam(p.id, parseFloat(rng.value));
    });
  }
  ku.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    if (window.__JUCE__ && window.__JUCE__.backend) {
      window.__JUCE__.backend.emitEvent('showContextMenu', { id: p.id, x: e.screenX, y: e.screenY });
    }
  });
  decks[p.deck].appendChild(ku);
});

function emitParam(id, value) {
  if (window.__JUCE__ && window.__JUCE__.backend) {
    window.__JUCE__.backend.emitEvent('paramChange', { id, value });
  }
}

function emitExciter(data) {
  if (window.__JUCE__ && window.__JUCE__.backend) {
    window.__JUCE__.backend.emitEvent('exciterTrigger', data);
  }
}

// Power toggle
let isPowered = false;
const pBtn = document.getElementById('btnPower');
if (pBtn) {
  pBtn.addEventListener('click', () => {
    isPowered = !isPowered;
    pBtn.textContent = isPowered ? 'POWER ON' : 'STANDBY';
    pBtn.style.background = isPowered ? 'var(--braun-orange)' : 'var(--knob-cap)';
    emitParam('power', isPowered ? 1.0 : 0.0);
  });
}

// Chime Strip keys (A-')
const chimeNotes = [
  { k: 'a', m: 60, l: 'C4' }, { k: 's', m: 62, l: 'D4' }, { k: 'd', m: 64, l: 'E4' },
  { k: 'f', m: 67, l: 'G4' }, { k: 'g', m: 69, l: 'A4' }, { k: 'h', m: 72, l: 'C5' },
  { k: 'j', m: 74, l: 'D5' }, { k: 'k', m: 76, l: 'E5' }, { k: 'l', m: 79, l: 'G5' },
  { k: ';', m: 81, l: 'A5' }, { k: "'", m: 84, l: 'C6' }
];
const chimeRow = document.getElementById('chimeRow');
if (chimeRow) {
  chimeNotes.forEach(c => {
    const b = document.createElement('button');
    b.className = 'chime-btn';
    b.innerHTML = `<div>${c.k.toUpperCase()}</div><div style="font-size:9px;color:var(--text-dim);">${c.l}</div>`;
    b.onmousedown = () => emitExciter({ type: 'note', midi: c.m, velocity: 0.75, duration: 3.5 });
    chimeRow.appendChild(b);
  });
}

// Chords (1-=)
const chordNames = [
  '1 PAVILION', '2 PLATEAUX', '3 DEEP 5TH', '4 ETHEREAL', '5 LYDIAN', '6 SOLAR BEAT',
  '7 AVALON', '8 BLADE RUNNER', '9 MIN7 9', '0 SUS4 7', '- MAJOR 9', '= CLUSTER'
];
const chordKeys = ['1','2','3','4','5','6','7','8','9','0','-','='];
const chordRow = document.getElementById('chordRow');
if (chordRow) {
  chordNames.forEach((name, idx) => {
    const b = document.createElement('button');
    b.className = 'exciter-btn';
    b.style.fontSize = '10px';
    b.style.padding = '4px 8px';
    b.textContent = name;
    b.onmousedown = () => emitExciter({ type: 'chord', chordIndex: idx, speed: 'med' });
    chordRow.appendChild(b);
  });
}

// Poisson generator toggle
let poissonRunning = false;
function togglePoisson() {
  poissonRunning = !poissonRunning;
  const btn = document.getElementById('btnPoisson');
  if (btn) {
    btn.style.color = poissonRunning ? 'var(--braun-orange)' : 'var(--text-main)';
    btn.style.borderColor = poissonRunning ? 'var(--braun-orange)' : 'var(--border-color)';
  }
  emitExciter({ type: 'poisson', enable: poissonRunning, epm: 18.0, humanize: 0.5 });
}

// Computer keyboard bindings
window.addEventListener('keydown', (e) => {
  if (e.repeat) return;
  if (e.key === ' ') {
    e.preventDefault();
    emitExciter({ type: 'dirac' });
    return;
  }
  const cn = chimeNotes.find(c => c.k === e.key.toLowerCase());
  if (cn) {
    emitExciter({ type: 'note', midi: cn.m, velocity: 0.75, duration: 3.5 });
    return;
  }
  const ci = chordKeys.indexOf(e.key);
  if (ci >= 0) {
    emitExciter({ type: 'chord', chordIndex: ci, speed: 'med' });
    return;
  }
});

// Visualizer oscilloscope canvas
const canvas = document.getElementById('scopeCanvas');
const ctx = canvas.getContext('2d');
let scopeData = new Float32Array(240);

function drawScope() {
  ctx.fillStyle = '#090B0A';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.strokeStyle = '#18241C';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(0, canvas.height / 2);
  ctx.lineTo(canvas.width, canvas.height / 2);
  ctx.stroke();

  ctx.strokeStyle = '#24FF6A';
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  for (let i = 0; i < canvas.width; ++i) {
    const y = (canvas.height / 2) - (scopeData[i] * (canvas.height / 2) * 0.9);
    if (i === 0) ctx.moveTo(i, y);
    else ctx.lineTo(i, y);
  }
  ctx.stroke();
  requestAnimationFrame(drawScope);
}
requestAnimationFrame(drawScope);

// JUCE Bridge Event Listeners
if (window.__JUCE__ && window.__JUCE__.backend) {
  window.__JUCE__.backend.addEventListener('paramUpdate', (data) => {
    if (!data || !data.id) return;
    if (data.id === 'power' || data.apvtsId === 'power') {
      const p = data.value > 0.5;
      isPowered = p;
      if (pBtn) {
        pBtn.textContent = p ? 'POWER ON' : 'STANDBY';
        pBtn.style.background = p ? 'var(--braun-orange)' : 'var(--knob-cap)';
      }
      return;
    }
    const p = paramsMeta.find(m => m.id === data.id || m.webId === data.id);
    if (!p) return;
    const ctrl = document.getElementById('ctrl_' + p.id);
    const valDisp = document.getElementById('val_' + p.id);
    if (ctrl) {
      if (p.isBool) {
        ctrl.textContent = data.value > 0.5 ? 'ON' : 'OFF';
        ctrl.style.color = data.value > 0.5 ? 'var(--braun-orange)' : 'var(--text-main)';
      } else if (p.isChoice) {
        ctrl.value = Math.round(data.value);
      } else {
        ctrl.value = data.value;
        if (valDisp) valDisp.textContent = parseFloat(data.value).toFixed(2) + ' ' + p.unit;
      }
    }
  });

  window.__JUCE__.backend.addEventListener('telemetryFrame', (frame) => {
    if (!frame) return;
    const setM = (barId, valId, rms) => {
      const db = 20 * Math.log10(Math.max(1e-5, rms));
      const pct = Math.max(0, Math.min(100, (db + 60) * (100 / 60)));
      document.getElementById(barId).style.width = pct + '%';
      document.getElementById(valId).textContent = (db > -60) ? db.toFixed(1) + ' dB' : '-inf';
    };
    setM('mInL', 'vInL', frame.inputRmsL || 0);
    setM('mInR', 'vInR', frame.inputRmsR || 0);
    setM('mOutL', 'vOutL', frame.outputRmsL || 0);
    setM('mOutR', 'vOutR', frame.outputRmsR || 0);
    const corr = frame.correlation || 0;
    document.getElementById('mCorr').style.width = ((corr + 1) * 50) + '%';
    document.getElementById('vCorr').textContent = corr.toFixed(2);
  });

  window.__JUCE__.backend.addEventListener('scopeFrame', (data) => {
    if (data && data.samples) {
      for (let i = 0; i < canvas.width && i < data.samples.length; ++i) {
        scopeData[i] = data.samples[i];
      }
    }
  });

  // Request initial sync
  window.__JUCE__.backend.emitEvent('paramChange', { id: 'requestSync', value: 0 });
}
</script>
</body>
</html>
)html";

} // namespace

#if JUCE_WEB_BROWSER
juce::WebBrowserComponent::Options BRAUN_RB26AudioProcessorEditor::createWebOptions(BRAUN_RB26AudioProcessorEditor& editor)
{
#if JUCE_WINDOWS
    // Configure WebView2 arguments for low latency, no audio contention, and host DAW stability
    _wputenv_s(
        L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
        L"--mute-audio "
        L"--disable-audio-output "
        L"--disable-web-midi "
        L"--disable-background-timer-throttling "
        L"--disable-backgrounding-occluded-windows "
        L"--disable-renderer-backgrounding "
        L"--disable-features=Translate,OptimizationHints,MediaRouter,InterestFeedContentSuggestions,CalculateNativeWinOcclusion"
    );
#endif

    auto options = juce::WebBrowserComponent::Options{}
#if JUCE_WINDOWS
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(juce::File::getSpecialLocation(juce::File::SpecialLocationType::tempDirectory).getChildFile("BraunRB26_WebView2"))
                .withBackgroundColour(juce::Colour(0xff141517)))
#endif
        .withUserScript("window.__IS_JUCE__ = true; window.addEventListener('contextmenu', function(e) { if (!e.defaultPrevented) e.preventDefault(); }, false);")
        .withNativeIntegrationEnabled()
        .withResourceProvider([&editor](const juce::String& url) {
            return editor.getResource(url);
        })
        .withEventListener("paramChange", [&editor](const juce::var& data) {
            editor.handleParamChangeFromWeb(data);
        })
        .withEventListener("exciterTrigger", [&editor](const juce::var& data) {
            editor.handleExciterTriggerFromWeb(data);
        })
        .withEventListener("startRecording", [&editor](const juce::var& /*data*/) {
            editor.handleStartRecordingFromWeb();
        })
        .withEventListener("stopRecording", [&editor](const juce::var& /*data*/) {
            editor.handleStopRecordingFromWeb();
        })
        .withEventListener("showContextMenu", [&editor](const juce::var& data) {
            if (data.isObject())
            {
                const juce::String id = data.getProperty("id", "").toString();
                const int x = static_cast<int>(data.getProperty("x", 0));
                const int y = static_cast<int>(data.getProperty("y", 0));
                if (auto* slot = editor.findKnob(id))
                {
                    editor.showKnobContextMenu(*slot, { x, y });
                }
            }
        });

    return options;
}
#endif

juce::File BRAUN_RB26AudioProcessorEditor::getSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::SpecialLocationType::userApplicationDataDirectory)
        .getChildFile("Braun")
        .getChildFile("RB26_settings.xml");
}

bool BRAUN_RB26AudioProcessorEditor::loadPersistedNativeUIPreference()
{
    const auto file = getSettingsFile();
    if (!file.existsAsFile())
        return false;

    juce::PropertiesFile::Options opts;
    opts.applicationName = "BRAUN_RB26";
    opts.filenameSuffix = "xml";
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    opts.ignoreCaseOfKeyNames = true;
    opts.millisecondsBeforeSaving = 0;

    juce::PropertiesFile props(file, opts);
    return props.getBoolValue("useNativeUI", false);
}

void BRAUN_RB26AudioProcessorEditor::savePersistedNativeUIPreference(bool native)
{
    const auto file = getSettingsFile();
    auto parentDir = file.getParentDirectory();
    if (!parentDir.isDirectory())
        parentDir.createDirectory();

    juce::PropertiesFile::Options opts;
    opts.applicationName = "BRAUN_RB26";
    opts.filenameSuffix = "xml";
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    opts.ignoreCaseOfKeyNames = true;
    opts.millisecondsBeforeSaving = 0;

    juce::PropertiesFile props(file, opts);
    props.setValue("useNativeUI", native);
    props.saveIfNeeded();
    props.save();
}

BRAUN_RB26AudioProcessorEditor::BRAUN_RB26AudioProcessorEditor(BRAUN_RB26AudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p)
{
    setLookAndFeel(&braunLookAndFeel);
    setOpaque(true);

    setupNativeControls();

    const bool defaultToNative = loadPersistedNativeUIPreference();

#if JUCE_WEB_BROWSER
    webComponent = std::make_unique<juce::WebBrowserComponent>(createWebOptions(*this));
    webComponent->setOpaque(true);
    addAndMakeVisible(*webComponent);
    useNativeUI = defaultToNative;
    webComponent->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
#else
    useNativeUI = true;
#endif

    setNativeMode(useNativeUI);
    registerParameterListeners();

    // 19" studio rack aspect ratio: 1280x760 default, resizable
    setSize(1280, 760);
    setResizable(true, true);
    setResizeLimits(960, 600, 2560, 1440);

    // 60 Hz telemetry polling timer for smooth phosphor CRT waterfall and goniometer
    startTimerHz(60);
}

BRAUN_RB26AudioProcessorEditor::~BRAUN_RB26AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    stopTimer();
    unregisterParameterListeners();
    knobSlots.clear();
    buttonSlots.clear();
    comboSlots.clear();
}

void BRAUN_RB26AudioProcessorEditor::resized()
{
#if JUCE_WEB_BROWSER
    if (webComponent != nullptr)
    {
        if (!useNativeUI)
        {
            webComponent->setBounds(getLocalBounds());
        }
    }
#endif

    if (useNativeUI)
    {
        updateNativeControlLayout();
    }
}

void BRAUN_RB26AudioProcessorEditor::parentHierarchyChanged()
{
    AudioProcessorEditor::parentHierarchyChanged();
#if JUCE_WEB_BROWSER
    hwndStylesConfigured = false;
    if (!useNativeUI)
    {
        ensureHwndStyles();
    }
#endif
}

#if JUCE_WEB_BROWSER
void BRAUN_RB26AudioProcessorEditor::ensureHwndStyles()
{
#if JUCE_WINDOWS
    if (auto* peer = getPeer())
    {
        HWND hwnd = static_cast<HWND>(peer->getNativeHandle());
        if (hwnd == nullptr)
            return;

        LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
        if ((style & (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)) != (WS_CLIPCHILDREN | WS_CLIPSIBLINGS))
        {
            ::SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        }

        int childCount = 0;
        ::EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
            auto* count = reinterpret_cast<int*>(lParam);
            (*count)++;
            LONG_PTR childStyle = ::GetWindowLongPtr(child, GWL_STYLE);
            if ((childStyle & (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)) != (WS_CLIPCHILDREN | WS_CLIPSIBLINGS))
            {
                ::SetWindowLongPtr(child, GWL_STYLE, childStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&childCount));

        if (childCount > 0)
            hwndStylesConfigured = true;
    }
#endif
}
#endif

void BRAUN_RB26AudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
#if JUCE_WEB_BROWSER
    const auto& meta = rb26::getParameterMetadataTable();
    for (size_t i = 0; i < meta.size(); ++i)
    {
        if (parameterID == meta[i].apvtsId)
        {
            pendingParamValues[i].store(newValue, std::memory_order_relaxed);
            paramDirty[i].store(true, std::memory_order_relaxed);
            break;
        }
    }
#else
    juce::ignoreUnused(parameterID, newValue);
#endif
}

#if JUCE_WEB_BROWSER
void BRAUN_RB26AudioProcessorEditor::sendParameterUpdateToWeb(const juce::String& apvtsId, const juce::String& webId, float newValue)
{
    if (webComponent == nullptr) return;
    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", webId);
    obj->setProperty("apvtsId", apvtsId);
    obj->setProperty("value", newValue);
    webComponent->emitEventIfBrowserIsVisible("paramUpdate", juce::var(obj));
}

void BRAUN_RB26AudioProcessorEditor::syncAllParametersToWeb()
{
    const auto& meta = rb26::getParameterMetadataTable();
    for (size_t i = 0; i < meta.size(); ++i)
    {
        if (auto* rawVal = processorRef.getAPVTS().getRawParameterValue(meta[i].apvtsId))
        {
            sendParameterUpdateToWeb(meta[i].apvtsId, meta[i].webId, rawVal->load(std::memory_order_relaxed));
        }
    }
    sendParameterUpdateToWeb("power", "power", processorRef.isPower() ? 1.0f : 0.0f);
    sendRecordingStateUpdateToWeb(processorRef.isRecording());
}
#endif

void BRAUN_RB26AudioProcessorEditor::timerCallback()
{
    // Always drain visualizer telemetry from audio thread
    rb26::Rb26ReverbEngine::VisualizerFrame frame;
    while (processorRef.popVisualizerFrame(frame))
    {
        latestTelemetryFrame = frame;
    }

#if JUCE_WEB_BROWSER
    if (!useNativeUI && webComponent != nullptr)
    {
        // Windows HWND style guard
        if (!hwndStylesConfigured || ++hwndCheckCounter >= 60)
        {
            hwndCheckCounter = 0;
            ensureHwndStyles();
        }

        if (processorRef.consumeRecordingSavedDirty())
        {
            auto* obj = new juce::DynamicObject();
            obj->setProperty("path", processorRef.getLastRecordedFile().getFullPathName());
            webComponent->emitEventIfBrowserIsVisible("recordingSaved", juce::var(obj));
        }

        // Initial state synchronization on webview ready
        if (!initialSyncDone && webComponent->isVisible())
        {
            syncAllParametersToWeb();
            initialSyncDone = true;
        }

        // Coalesced dirty parameter dispatch at 60 Hz
        const auto& meta = rb26::getParameterMetadataTable();
        for (size_t i = 0; i < meta.size(); ++i)
        {
            if (paramDirty[i].exchange(false, std::memory_order_relaxed))
            {
                const float val = pendingParamValues[i].load(std::memory_order_relaxed);
                sendParameterUpdateToWeb(meta[i].apvtsId, meta[i].webId, val);
            }
        }

        // 60 Hz Visualizer Telemetry stream
        sendTelemetryToWeb();
        sendScopeDataToWeb();
    }
    else
#endif
    {
        // Native UI: update header button states
        powerButton.setButtonText(processorRef.isPower() ? "POWER ON" : "STANDBY");
        powerButton.setToggleState(processorRef.isPower(), juce::dontSendNotification);

        recordButton.setToggleState(processorRef.isRecording(), juce::dontSendNotification);

        // Sync preset combo box if external host program changed
        const int currentProg = processorRef.getCurrentProgram();
        if (presetComboBox.getSelectedId() != currentProg + 1)
        {
            presetComboBox.setSelectedId(currentProg + 1, juce::dontSendNotification);
        }

        // Repaint CRT scope display
        auto crtArea = getLocalBounds().withTrimmedTop(56).removeFromTop(130).reduced(16, 8);
        repaint(crtArea);
    }
}

#if JUCE_WEB_BROWSER
void BRAUN_RB26AudioProcessorEditor::sendTelemetryToWeb()
{
    if (webComponent == nullptr || !webComponent->isVisible())
        return;

    const bool isSilent = (latestTelemetryFrame.inputRmsL < 1.0e-5f &&
                           latestTelemetryFrame.inputRmsR < 1.0e-5f &&
                           latestTelemetryFrame.outputRmsL < 1.0e-5f &&
                           latestTelemetryFrame.outputRmsR < 1.0e-5f);

    if (isSilent)
    {
        if (++silentTelemetryCounter > 6)
        {
            if (silentTelemetryCounter % 30 != 0) // Throttle to 2Hz during sustained silence
                return;
        }
    }
    else
    {
        silentTelemetryCounter = 0;
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty("inputRmsL", latestTelemetryFrame.inputRmsL);
    obj->setProperty("inputRmsR", latestTelemetryFrame.inputRmsR);
    obj->setProperty("outputRmsL", latestTelemetryFrame.outputRmsL);
    obj->setProperty("outputRmsR", latestTelemetryFrame.outputRmsR);
    obj->setProperty("correlation", latestTelemetryFrame.correlation);
    obj->setProperty("lowEnergy", latestTelemetryFrame.lowEnergy);
    obj->setProperty("midEnergy", latestTelemetryFrame.midEnergy);
    obj->setProperty("highEnergy", latestTelemetryFrame.highEnergy);
    obj->setProperty("decayEnvelope", latestTelemetryFrame.decayEnvelope);

    webComponent->emitEventIfBrowserIsVisible("telemetryFrame", juce::var(obj));
}

void BRAUN_RB26AudioProcessorEditor::sendScopeDataToWeb()
{
    if (webComponent == nullptr || !webComponent->isVisible())
        return;

    constexpr int kSamples = 512;
    float sL[kSamples];
    float sR[kSamples];
    processorRef.getScopeSamples(sL, sR, kSamples);

    bool hasSignal = false;
    for (int i = 0; i < kSamples; ++i)
    {
        if (std::abs(sL[i]) > 0.0005f || std::abs(sR[i]) > 0.0005f)
        {
            hasSignal = true;
            break;
        }
    }

    if (!hasSignal)
    {
        if (++silentFrameCounter > 6)
        {
            if (silentFrameCounter % 30 != 0) // Throttle to 2Hz during long silence
                return;
        }
    }
    else
    {
        silentFrameCounter = 0;
    }

    juce::Array<juce::var> leftArray;
    juce::Array<juce::var> rightArray;
    leftArray.ensureStorageAllocated(kSamples);
    rightArray.ensureStorageAllocated(kSamples);
    for (int i = 0; i < kSamples; ++i)
    {
        leftArray.add(sL[i]);
        rightArray.add(sR[i]);
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty("samplesL", juce::var(leftArray));
    obj->setProperty("samplesR", juce::var(rightArray));
    obj->setProperty("samples", juce::var(leftArray)); // mono backward compatibility
    webComponent->emitEventIfBrowserIsVisible("scopeFrame", juce::var(obj));
}

void BRAUN_RB26AudioProcessorEditor::handleParamChangeFromWeb(const juce::var& data)
{
    if (!data.isObject())
        return;

    auto* obj = data.getDynamicObject();
    if (obj == nullptr)
        return;

    const juce::String incomingId = obj->getProperty("id").toString();
    const float incomingVal = static_cast<float>(obj->getProperty("value"));

    if (incomingId.equalsIgnoreCase("requestSync") || incomingId.equalsIgnoreCase("requestState"))
    {
        syncAllParametersToWeb();
        return;
    }

    if (incomingId.equalsIgnoreCase("toggleNativeUI") || 
        incomingId.equalsIgnoreCase("nativeUI") || 
        incomingId.equalsIgnoreCase("switchUI"))
    {
        setNativeMode(!useNativeUI);
        return;
    }

    if (incomingId.equalsIgnoreCase("power"))
    {
        processorRef.setPower(incomingVal > 0.5f);
        return;
    }

    // Handle recording commands sent as paramChange
    if (incomingId.equalsIgnoreCase("startRecording"))
    {
        processorRef.startRecording();
        sendRecordingStateUpdateToWeb(processorRef.isRecording());
        return;
    }
    if (incomingId.equalsIgnoreCase("stopRecording"))
    {
        processorRef.stopRecording();
        sendRecordingStateUpdateToWeb(processorRef.isRecording());
        return;
    }

    const auto& meta = rb26::getParameterMetadataTable();
    for (const auto& item : meta)
    {
        if (incomingId.equalsIgnoreCase(item.apvtsId) || incomingId.equalsIgnoreCase(item.webId))
        {
            if (auto* param = processorRef.getAPVTS().getParameter(item.apvtsId))
            {
                float targetVal = incomingVal;
                if (item.isChoice)
                {
                    // Choice parameters convert index to normalized 0..1
                    const float norm = param->convertTo0to1(targetVal);
                    param->setValueNotifyingHost(std::clamp(norm, 0.0f, 1.0f));
                }
                else if (item.isBool)
                {
                    param->setValueNotifyingHost(targetVal > 0.5f ? 1.0f : 0.0f);
                }
                else
                {
                    const float norm = param->convertTo0to1(targetVal);
                    param->setValueNotifyingHost(std::clamp(norm, 0.0f, 1.0f));
                }
            }
            break;
        }
    }
}

void BRAUN_RB26AudioProcessorEditor::handleExciterTriggerFromWeb(const juce::var& data)
{
    if (!data.isObject())
        return;

    auto* obj = data.getDynamicObject();
    if (obj == nullptr)
        return;

    const juce::String type = obj->getProperty("type").toString();

    // Auto-wake DSP engine to ensure audio output on exciter interaction
    if (!processorRef.isPower())
    {
        processorRef.setPower(true);
    }

    if (type.equalsIgnoreCase("note"))
    {
        const float midi = static_cast<float>(obj->getProperty("midi"));
        const float vel = obj->hasProperty("velocity") ? static_cast<float>(obj->getProperty("velocity")) : 0.7f;
        const float dur = obj->hasProperty("duration") ? static_cast<float>(obj->getProperty("duration")) : 3.5f;
        processorRef.getExciterEngine().triggerNoteAsync(midi, vel, dur);
    }
    else if (type.equalsIgnoreCase("chord"))
    {
        const int idx = static_cast<int>(obj->getProperty("chordIndex"));
        const juce::String spdStr = obj->getProperty("speed").toString();
        rb26::StrumSpeed spd = rb26::StrumSpeed::Med;
        if (spdStr.equalsIgnoreCase("slow")) spd = rb26::StrumSpeed::Slow;
        else if (spdStr.equalsIgnoreCase("fast")) spd = rb26::StrumSpeed::Fast;
        else if (spdStr.equalsIgnoreCase("instant")) spd = rb26::StrumSpeed::Instant;
        processorRef.getExciterEngine().triggerChordAsync(idx, 60.0f, 0.7f, spd);
    }
    else if (type.equalsIgnoreCase("dirac"))
    {
        processorRef.getExciterEngine().triggerDiracAsync(1.0f);
    }
    else if (type.equalsIgnoreCase("pink"))
    {
        const float dur = obj->hasProperty("duration") ? static_cast<float>(obj->getProperty("duration")) : 40.0f;
        processorRef.getExciterEngine().triggerPinkBurstAsync(dur);
    }
    else if (type.equalsIgnoreCase("hammer") || type.equalsIgnoreCase("mallet"))
    {
        processorRef.getExciterEngine().triggerHammerThudAsync(0.7f);
    }
    else if (type.equalsIgnoreCase("pad"))
    {
        processorRef.getExciterEngine().triggerChordAsync(3, 60.0f, 0.7f, rb26::StrumSpeed::Slow);
    }
    else if (type.equalsIgnoreCase("poisson"))
    {
        const bool en = static_cast<bool>(obj->getProperty("enable"));
        const float epm = obj->hasProperty("epm") ? static_cast<float>(obj->getProperty("epm")) : 18.0f;
        const float hum = obj->hasProperty("humanize") ? static_cast<float>(obj->getProperty("humanize")) : 0.5f;
        processorRef.getExciterEngine().setPoissonEnable(en);
        processorRef.getExciterEngine().setPoissonEpm(epm);
        processorRef.getExciterEngine().setPoissonHumanize(hum);
    }
}

void BRAUN_RB26AudioProcessorEditor::sendRecordingStateUpdateToWeb(bool isRecording)
{
    if (webComponent == nullptr)
        return;
    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", "isRecording");
    obj->setProperty("value", isRecording ? 1.0f : 0.0f);
    webComponent->emitEventIfBrowserIsVisible("paramUpdate", juce::var(obj));
}

void BRAUN_RB26AudioProcessorEditor::handleStartRecordingFromWeb()
{
    processorRef.startRecording();
    sendRecordingStateUpdateToWeb(processorRef.isRecording());
}

void BRAUN_RB26AudioProcessorEditor::handleStopRecordingFromWeb()
{
    processorRef.stopRecording();
    sendRecordingStateUpdateToWeb(processorRef.isRecording());
}

std::optional<juce::WebBrowserComponent::Resource> BRAUN_RB26AudioProcessorEditor::getResource(const juce::String& url)
{
    juce::String path = url;

    // Strip virtual hostname (both with and without trailing slash, supporting https, http, and juce protocols)
    if (path.startsWithIgnoreCase("https://juce.backend/"))
        path = path.substring(21);
    else if (path.startsWithIgnoreCase("http://juce.backend/"))
        path = path.substring(20);
    else if (path.startsWithIgnoreCase("juce://juce.backend/"))
        path = path.substring(20);
    else if (path.startsWithIgnoreCase("https://juce.backend"))
        path = path.substring(20);
    else if (path.startsWithIgnoreCase("http://juce.backend"))
        path = path.substring(19);
    else if (path.startsWithIgnoreCase("juce://juce.backend"))
        path = path.substring(19);

    const int queryIdx = path.indexOfChar('?');
    if (queryIdx >= 0) path = path.substring(0, queryIdx);
    const int hashIdx = path.indexOfChar('#');
    if (hashIdx >= 0) path = path.substring(0, hashIdx);

    while (path.startsWithChar('/') || path.startsWithChar('\\') || path.startsWith("./"))
    {
        if (path.startsWithChar('/') || path.startsWithChar('\\'))
            path = path.substring(1);
        else if (path.startsWith("./"))
            path = path.substring(2);
    }

    if (path.startsWithIgnoreCase("web/"))
        path = path.substring(4);
    else if (path.startsWithIgnoreCase("ui/"))
        path = path.substring(3);

    while (path.startsWithChar('/') || path.startsWithChar('\\'))
        path = path.substring(1);

    if (path.isEmpty())
        path = "index.html";

    juce::String mimeType = "application/octet-stream";
    if (path.endsWithIgnoreCase(".html") || path.endsWithIgnoreCase(".htm")) mimeType = "text/html; charset=utf-8";
    else if (path.endsWithIgnoreCase(".css")) mimeType = "text/css; charset=utf-8";
    else if (path.endsWithIgnoreCase(".js") || path.endsWithIgnoreCase(".mjs")) mimeType = "text/javascript; charset=utf-8";
    else if (path.endsWithIgnoreCase(".json")) mimeType = "application/json; charset=utf-8";
    else if (path.endsWithIgnoreCase(".svg")) mimeType = "image/svg+xml";
    else if (path.endsWithIgnoreCase(".png")) mimeType = "image/png";
    else if (path.endsWithIgnoreCase(".jpg") || path.endsWithIgnoreCase(".jpeg")) mimeType = "image/jpeg";
    else if (path.endsWithIgnoreCase(".woff2")) mimeType = "font/woff2";
    else if (path.endsWithIgnoreCase(".woff")) mimeType = "font/woff";
    else if (path.endsWithIgnoreCase(".ttf")) mimeType = "font/ttf";
    else if (path.endsWithIgnoreCase(".wasm")) mimeType = "application/wasm";

    // 1. Search local filesystem (for live dev iteration)
    auto checkDiskFile = [&](const juce::File& file) -> std::optional<juce::WebBrowserComponent::Resource> {
        if (file.existsAsFile())
        {
            juce::MemoryBlock mb;
            if (file.loadFileAsData(mb))
            {
                std::vector<std::byte> bytes(mb.getSize());
                std::memcpy(bytes.data(), mb.getData(), mb.getSize());
                return juce::WebBrowserComponent::Resource { std::move(bytes), mimeType };
            }
        }
        return std::nullopt;
    };

    const juce::File cwd = juce::File::getCurrentWorkingDirectory();
    if (auto res = checkDiskFile(cwd.getChildFile("web").getChildFile(path))) return res;
    if (auto res = checkDiskFile(cwd.getChildFile("ui").getChildFile(path))) return res;
    if (auto res = checkDiskFile(cwd.getChildFile("braun_rb-26/web").getChildFile(path))) return res;
    if (auto res = checkDiskFile(cwd.getChildFile("rb-26/web").getChildFile(path))) return res;

    // Search relative to executable
    auto dir = juce::File::getSpecialLocation(juce::File::SpecialLocationType::currentExecutableFile).getParentDirectory();
    for (int depth = 0; depth < 5; ++depth)
    {
        if (auto res = checkDiskFile(dir.getChildFile("web").getChildFile(path))) return res;
        if (auto res = checkDiskFile(dir.getChildFile("ui").getChildFile(path))) return res;
        if (auto res = checkDiskFile(dir.getChildFile("braun_rb-26/web").getChildFile(path))) return res;
        if (auto res = checkDiskFile(dir.getChildFile("rb-26/web").getChildFile(path))) return res;
        dir = dir.getParentDirectory();
    }

    // 2. Unpack from embedded binary zip archive (BraunRb26WebAssets)
#if RB26_HAS_BINARY_DATA
    if (BinaryData::web_assets_rb26_zipSize > 0 && BinaryData::web_assets_rb26_zip != nullptr)
    {
        juce::MemoryInputStream memStream(BinaryData::web_assets_rb26_zip, static_cast<size_t>(BinaryData::web_assets_rb26_zipSize), false);
        juce::ZipFile zip(memStream);

        juce::String normalizedPath = path.replaceCharacter('\\', '/');
        while (normalizedPath.startsWithChar('/') || normalizedPath.startsWith("./"))
        {
            if (normalizedPath.startsWithChar('/')) normalizedPath = normalizedPath.substring(1);
            else if (normalizedPath.startsWith("./")) normalizedPath = normalizedPath.substring(2);
        }
        if (normalizedPath.startsWithIgnoreCase("web/")) normalizedPath = normalizedPath.substring(4);
        else if (normalizedPath.startsWithIgnoreCase("ui/")) normalizedPath = normalizedPath.substring(3);

        int entryIndex = zip.getIndexOfFileName(normalizedPath);
        if (entryIndex < 0)
        {
            // Search all entries case-insensitively without leading slashes, ./, or web/ prefixes
            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                const auto* entry = zip.getEntry(i);
                if (entry != nullptr)
                {
                    juce::String name = entry->filename.replaceCharacter('\\', '/');
                    while (name.startsWithChar('/') || name.startsWith("./"))
                    {
                        if (name.startsWithChar('/'))
                            name = name.substring(1);
                        else if (name.startsWith("./"))
                            name = name.substring(2);
                    }
                    if (name.startsWithIgnoreCase("web/")) name = name.substring(4);
                    else if (name.startsWithIgnoreCase("ui/")) name = name.substring(3);

                    if (name.equalsIgnoreCase(normalizedPath))
                    {
                        entryIndex = i;
                        break;
                    }
                }
            }
        }

        if (entryIndex >= 0)
        {
            const auto* entry = zip.getEntry(entryIndex);
            if (entry != nullptr)
            {
                std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(*entry));
                if (stream != nullptr)
                {
                    juce::MemoryBlock mb;
                    stream->readIntoMemoryBlock(mb, -1);
                    std::vector<std::byte> data(mb.getSize());
                    std::memcpy(data.data(), mb.getData(), mb.getSize());
                    return juce::WebBrowserComponent::Resource { std::move(data), mimeType };
                }
            }
        }
    }
#endif

    // 3. Embedded fallback HTML (only if disk and binary assets both unavailable)
    if (path.equalsIgnoreCase("index.html"))
    {
        const size_t len = std::strlen(kEmbeddedBraunFallbackHtml);
        std::vector<std::byte> bytes(len);
        std::memcpy(bytes.data(), kEmbeddedBraunFallbackHtml, len);
        return juce::WebBrowserComponent::Resource { std::move(bytes), mimeType };
    }

    return std::nullopt;
}
#endif

void BRAUN_RB26AudioProcessorEditor::registerParameterListeners()
{
    const auto& meta = rb26::getParameterMetadataTable();
    for (const auto& item : meta)
    {
        processorRef.getAPVTS().addParameterListener(item.apvtsId, this);
    }
}

void BRAUN_RB26AudioProcessorEditor::unregisterParameterListeners()
{
    const auto& meta = rb26::getParameterMetadataTable();
    for (const auto& item : meta)
    {
        processorRef.getAPVTS().removeParameterListener(item.apvtsId, this);
    }
}

// ============================================================================
// Secondary Presentation Layer: Pure Native Dieter Rams Vector Graphics
// ============================================================================
void BRAUN_RB26AudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    drawBraunChassis(g, bounds);
}

void BRAUN_RB26AudioProcessorEditor::drawBraunChassis(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Chassis background
    g.fillAll(braunLookAndFeel.findColour(rb26::BraunColours::bgAppColourId));

    // Outer precision bevel & border
    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::borderLineColourId));
    g.drawRect(bounds.toFloat(), 1.5f);

    // Header Deck
    auto headerArea = bounds.removeFromTop(56);
    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::bgPanelColourId));
    g.fillRect(headerArea);
    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::borderLineColourId));
    g.drawHorizontalLine(headerArea.getBottom(), 0.0f, static_cast<float>(bounds.getWidth()));

    // Dieter Rams Functionalist Typography
    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::textPrimaryColourId));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("BRAUN RB-26", headerArea.removeFromLeft(180).reduced(16, 0), juce::Justification::centredLeft);

    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::textMutedColourId));
    g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
    g.drawText(juce::String("STUDIO REVERBERATOR ") + juce::String::charToString(0x00B7) + " DIN 1451", headerArea.removeFromLeft(220).reduced(4, 0), juce::Justification::centredLeft);

    // Central CRT Phosphor Visualizer Scope
    auto crtArea = bounds.removeFromTop(130).reduced(16, 6);
    drawCrtDisplay(g, crtArea);

    // Audition strip background & label
    auto auditionArea = bounds.removeFromTop(32).reduced(16, 2);
    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::bgPanelColourId));
    g.fillRoundedRectangle(auditionArea.toFloat(), 3.0f);
    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::borderLineColourId));
    g.drawRoundedRectangle(auditionArea.toFloat(), 3.0f, 1.0f);

    auto auditionLabelArea = auditionArea.removeFromLeft(130);
    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::textMutedColourId));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("AUDITION SOURCE:", auditionLabelArea.reduced(8, 0), juce::Justification::centredLeft);

    // 6 Signal-Flow Decks Grid
    auto gridArea = bounds.reduced(16, 6);
    const int numCols = 3;
    const int numRows = 2;
    const int colWidth = gridArea.getWidth() / numCols;
    const int rowHeight = gridArea.getHeight() / numRows;

    const char* deckTitles[6] = {
        "1. INPUT & PRE-DELAY",
        "2. LOW MODAL MATRIX",
        "3. REVERB TANK (FDN)",
        "4. PITCH DIFFUSION",
        "5. TAIL MODULATION",
        "6. MASTER BUS"
    };

    for (int r = 0; r < numRows; ++r)
    {
        for (int c = 0; c < numCols; ++c)
        {
            const int idx = r * numCols + c;
            auto cell = juce::Rectangle<int>(gridArea.getX() + c * colWidth, gridArea.getY() + r * rowHeight, colWidth, rowHeight).reduced(4);
            
            g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::bgPanelColourId));
            g.fillRoundedRectangle(cell.toFloat(), 3.0f);
            g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::borderLineColourId));
            g.drawRoundedRectangle(cell.toFloat(), 3.0f, 1.0f);

            // Deck header
            auto deckHeader = cell.removeFromTop(22);
            g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::braunOrangeColourId));
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText(deckTitles[idx], deckHeader.reduced(8, 0), juce::Justification::centredLeft);
        }
    }
}

void BRAUN_RB26AudioProcessorEditor::drawCrtDisplay(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // CRT Bezel
    g.setColour(juce::Colour(rb26::BraunColours::Dark_BgBezel));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff202622));
    g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.5f);

    auto inner = bounds.reduced(8);
    auto scopeArea = inner.removeFromLeft(inner.getWidth() / 2);
    auto meterArea = inner.reduced(8, 0);

    // Phosphor Grid lines
    g.setColour(juce::Colour(0xff162419));
    g.drawHorizontalLine(scopeArea.getCentreY(), static_cast<float>(scopeArea.getX()), static_cast<float>(scopeArea.getRight()));
    g.drawVerticalLine(scopeArea.getCentreX(), static_cast<float>(scopeArea.getY()), static_cast<float>(scopeArea.getBottom()));

    // Waveform rendering
    constexpr int kSamples = 240;
    float sL[kSamples], sR[kSamples];
    processorRef.getScopeSamples(sL, sR, kSamples);

    juce::Path wavePath;
    const float midY = static_cast<float>(scopeArea.getCentreY());
    const float heightScale = static_cast<float>(scopeArea.getHeight()) * 0.45f;
    const float stepX = static_cast<float>(scopeArea.getWidth()) / static_cast<float>(kSamples);

    for (int i = 0; i < kSamples; ++i)
    {
        const float x = static_cast<float>(scopeArea.getX()) + i * stepX;
        const float y = midY - (0.5f * (sL[i] + sR[i]) * heightScale);
        if (i == 0) wavePath.startNewSubPath(x, y);
        else wavePath.lineTo(x, y);
    }

    g.setColour(juce::Colour(rb26::BraunColours::PhosphorGreen));
    g.strokePath(wavePath, juce::PathStrokeType(1.5f));

    // Telemetry Bars
    auto drawBar = [&](const juce::String& label, float rms, juce::Colour col, int yOffset) {
        const float db = 20.0f * std::log10(juce::jmax(1e-5f, rms));
        const float pct = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto row = juce::Rectangle<int>(meterArea.getX(), meterArea.getY() + yOffset, meterArea.getWidth(), 14);
        g.setColour(juce::Colour(0xff8E9094));
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(label, row.removeFromLeft(45), juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xff18201B));
        g.fillRect(row);
        g.setColour(col);
        g.fillRect(row.removeFromLeft(static_cast<int>(row.getWidth() * pct)));
    };

    drawBar("IN L", latestTelemetryFrame.inputRmsL, juce::Colour(rb26::BraunColours::PhosphorGreen), 4);
    drawBar("IN R", latestTelemetryFrame.inputRmsR, juce::Colour(rb26::BraunColours::PhosphorGreen), 22);
    drawBar("OUT L", latestTelemetryFrame.outputRmsL, juce::Colour(rb26::BraunColours::PhosphorGreen), 40);
    drawBar("OUT R", latestTelemetryFrame.outputRmsR, juce::Colour(rb26::BraunColours::PhosphorGreen), 58);
    drawBar("CORR", 0.5f * (latestTelemetryFrame.correlation + 1.0f), juce::Colour(0xff24B8FF), 76);
}

void BRAUN_RB26AudioProcessorEditor::setupNativeControls()
{
    // Power button
    powerButton.setButtonText("STANDBY");
    powerButton.setToggleState(false, juce::dontSendNotification);
    powerButton.setClickingTogglesState(false);
    powerButton.onClick = [this] {
        processorRef.setPower(!processorRef.isPower());
        powerButton.setButtonText(processorRef.isPower() ? "POWER ON" : "STANDBY");
        powerButton.setToggleState(processorRef.isPower(), juce::dontSendNotification);
#if JUCE_WEB_BROWSER
        sendParameterUpdateToWeb("power", "power", processorRef.isPower() ? 1.0f : 0.0f);
#endif
    };
    addChildComponent(powerButton);

    // Theme button
    themeButton.setButtonText(braunLookAndFeel.isDarkTheme() ? "THEME: DARK" : "THEME: LIGHT");
    themeButton.onClick = [this] {
        braunLookAndFeel.setDarkTheme(!braunLookAndFeel.isDarkTheme());
        themeButton.setButtonText(braunLookAndFeel.isDarkTheme() ? "THEME: DARK" : "THEME: LIGHT");
        sendLookAndFeelChange();
        repaint();
    };
    addChildComponent(themeButton);

    // Record button
    recordButton.setButtonText("REC CAPTURE");
    recordButton.onClick = [this] {
        if (processorRef.isRecording())
            processorRef.stopRecording();
        else
            processorRef.startRecording();
        recordButton.setButtonText(processorRef.isRecording() ? "RECORDING..." : "REC CAPTURE");
        recordButton.setToggleState(processorRef.isRecording(), juce::dontSendNotification);
#if JUCE_WEB_BROWSER
        sendRecordingStateUpdateToWeb(processorRef.isRecording());
#endif
    };
    addChildComponent(recordButton);

#if JUCE_WEB_BROWSER
    // View Mode button (toggle between Native and Web UI)
    viewModeButton.setButtonText("SWITCH TO WEB UI");
    viewModeButton.onClick = [this] {
        setNativeMode(!useNativeUI);
    };
    addChildComponent(viewModeButton);
#endif

    // Preset management controls for Native UI
    presetLabel.setText("PRESET:", juce::dontSendNotification);
    presetLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    presetLabel.setJustificationType(juce::Justification::centredRight);
    addChildComponent(presetLabel);

    const int numPrograms = processorRef.getNumPrograms();
    for (int i = 0; i < numPrograms; ++i)
    {
        presetComboBox.addItem(processorRef.getProgramName(i), i + 1);
    }
    presetComboBox.setSelectedId(processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    presetComboBox.onChange = [this] {
        const int selected = presetComboBox.getSelectedId() - 1;
        if (selected >= 0 && selected < processorRef.getNumPrograms())
        {
            processorRef.setCurrentProgram(selected);
        }
    };
    addChildComponent(presetComboBox);

    prevPresetBtn.setButtonText("<");
    prevPresetBtn.onClick = [this] {
        const int total = processorRef.getNumPrograms();
        if (total > 0)
        {
            const int nextIdx = (processorRef.getCurrentProgram() - 1 + total) % total;
            processorRef.setCurrentProgram(nextIdx);
            presetComboBox.setSelectedId(nextIdx + 1, juce::dontSendNotification);
        }
    };
    addChildComponent(prevPresetBtn);

    nextPresetBtn.setButtonText(">");
    nextPresetBtn.onClick = [this] {
        const int total = processorRef.getNumPrograms();
        if (total > 0)
        {
            const int nextIdx = (processorRef.getCurrentProgram() + 1) % total;
            processorRef.setCurrentProgram(nextIdx);
            presetComboBox.setSelectedId(nextIdx + 1, juce::dontSendNotification);
        }
    };
    addChildComponent(nextPresetBtn);

    // Audition Exciter trigger buttons
    impulseTriggerBtn.setButtonText("DIRAC IMPULSE");
    impulseTriggerBtn.onClick = [this] {
        if (!processorRef.isPower()) processorRef.setPower(true);
        processorRef.getExciterEngine().triggerDiracAsync(1.0f);
    };
    addChildComponent(impulseTriggerBtn);

    hammerTriggerBtn.setButtonText("HAMMER THUD");
    hammerTriggerBtn.onClick = [this] {
        if (!processorRef.isPower()) processorRef.setPower(true);
        processorRef.getExciterEngine().triggerHammerThudAsync(0.7f);
    };
    addChildComponent(hammerTriggerBtn);

    chordTriggerBtn.setButtonText("PIANO CHORD");
    chordTriggerBtn.onClick = [this] {
        if (!processorRef.isPower()) processorRef.setPower(true);
        processorRef.getExciterEngine().triggerChordAsync(3, 60.0f, 0.7f, rb26::StrumSpeed::Slow);
    };
    addChildComponent(chordTriggerBtn);

    // 26 Parameters Binding
    const auto& meta = rb26::getParameterMetadataTable();
    for (const auto& item : meta)
    {
        if (item.isBool)
        {
            auto slot = std::make_unique<ButtonSlot>();
            slot->paramId = item.apvtsId;
            slot->button.setButtonText(item.name);
            slot->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processorRef.getAPVTS(), item.apvtsId, slot->button);
            addChildComponent(slot->button);
            buttonSlots.push_back(std::move(slot));
        }
        else if (item.isChoice)
        {
            auto slot = std::make_unique<ComboSlot>();
            slot->paramId = item.apvtsId;
            slot->label.setText(item.name, juce::dontSendNotification);
            slot->label.setJustificationType(juce::Justification::centred);
            slot->label.setFont(juce::FontOptions(10.0f, juce::Font::plain));

            if (juce::String(item.apvtsId) == rb26::ParamIDs::shimmerInterval.getParamID())
            {
                slot->comboBox.addItemList(rb26::getShimmerIntervalChoices(), 1);
            }
            else if (juce::String(item.apvtsId) == rb26::ParamIDs::dimmerInterval.getParamID())
            {
                slot->comboBox.addItemList(rb26::getDimmerIntervalChoices(), 1);
            }
            slot->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processorRef.getAPVTS(), item.apvtsId, slot->comboBox);
            addChildComponent(slot->label);
            addChildComponent(slot->comboBox);
            comboSlots.push_back(std::move(slot));
        }
        else
        {
            auto slot = std::make_unique<KnobSlot>();
            slot->paramId = item.apvtsId;
            slot->slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slot->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
            if (std::strlen(item.unit) > 0)
                slot->slider.setTextValueSuffix(juce::String(" ") + item.unit);

            slot->nameLabel.setText(item.name, juce::dontSendNotification);
            slot->nameLabel.setJustificationType(juce::Justification::centred);
            slot->nameLabel.setFont(juce::FontOptions(10.0f, juce::Font::plain));

            slot->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processorRef.getAPVTS(), item.apvtsId, slot->slider);

            slot->slider.addMouseListener(this, true);
            slot->nameLabel.addMouseListener(this, true);

            addChildComponent(slot->slider);
            addChildComponent(slot->nameLabel);
            knobSlots.push_back(std::move(slot));
        }
    }
}

void BRAUN_RB26AudioProcessorEditor::setNativeMode(bool native)
{
    useNativeUI = native;
    savePersistedNativeUIPreference(useNativeUI);

#if JUCE_WEB_BROWSER
    if (webComponent != nullptr)
    {
        if (useNativeUI)
        {
            removeChildComponent(webComponent.get());
            webComponent->setVisible(false);
            webComponent->setBounds(0, 0, 0, 0);
        }
        else
        {
            addAndMakeVisible(*webComponent);
            webComponent->setVisible(true);
            webComponent->setBounds(getLocalBounds());
            webComponent->toFront(false);
            ensureHwndStyles();
        }
    }
    viewModeButton.setVisible(useNativeUI);
    viewModeButton.setButtonText("SWITCH TO WEB UI");
    if (useNativeUI)
        viewModeButton.toFront(true);
#endif

    const bool nativeVisible = useNativeUI;
    powerButton.setVisible(nativeVisible);
    themeButton.setVisible(nativeVisible);
    recordButton.setVisible(nativeVisible);
    presetLabel.setVisible(nativeVisible);
    presetComboBox.setVisible(nativeVisible);
    prevPresetBtn.setVisible(nativeVisible);
    nextPresetBtn.setVisible(nativeVisible);
    impulseTriggerBtn.setVisible(nativeVisible);
    hammerTriggerBtn.setVisible(nativeVisible);
    chordTriggerBtn.setVisible(nativeVisible);

    for (auto& slot : knobSlots)
    {
        slot->slider.setVisible(nativeVisible);
        slot->nameLabel.setVisible(nativeVisible);
        if (nativeVisible)
        {
            slot->slider.toFront(false);
            slot->nameLabel.toFront(false);
        }
    }
    for (auto& slot : buttonSlots)
    {
        slot->button.setVisible(nativeVisible);
        if (nativeVisible)
            slot->button.toFront(false);
    }
    for (auto& slot : comboSlots)
    {
        slot->comboBox.setVisible(nativeVisible);
        slot->label.setVisible(nativeVisible);
        if (nativeVisible)
        {
            slot->comboBox.toFront(false);
            slot->label.toFront(false);
        }
    }

    if (useNativeUI)
    {
        powerButton.toFront(false);
        themeButton.toFront(false);
        recordButton.toFront(false);
        presetLabel.toFront(false);
        presetComboBox.toFront(false);
        prevPresetBtn.toFront(false);
        nextPresetBtn.toFront(false);
        impulseTriggerBtn.toFront(false);
        hammerTriggerBtn.toFront(false);
        chordTriggerBtn.toFront(false);

        updateNativeControlLayout();
    }
    repaint();
}

void BRAUN_RB26AudioProcessorEditor::updateNativeControlLayout()
{
    auto totalBounds = getLocalBounds();
    if (totalBounds.isEmpty()) return;

    // Header Controls Layout
    auto headerArea = totalBounds.removeFromTop(56);
    auto rightButtons = headerArea.removeFromRight(headerArea.getWidth() - 370).reduced(10, 10);

#if JUCE_WEB_BROWSER
    viewModeButton.setBounds(rightButtons.removeFromRight(140).reduced(4, 2));
#endif
    recordButton.setBounds(rightButtons.removeFromRight(110).reduced(4, 2));
    themeButton.setBounds(rightButtons.removeFromRight(105).reduced(4, 2));
    powerButton.setBounds(rightButtons.removeFromRight(95).reduced(4, 2));

    // Preset selector positioned in remaining space between title and buttons
    if (rightButtons.getWidth() >= 160)
    {
        nextPresetBtn.setBounds(rightButtons.removeFromRight(26).reduced(2, 3));
        const int comboW = juce::jlimit(90, 180, rightButtons.getWidth() - 85);
        presetComboBox.setBounds(rightButtons.removeFromRight(comboW).reduced(2, 3));
        prevPresetBtn.setBounds(rightButtons.removeFromRight(26).reduced(2, 3));
        presetLabel.setBounds(rightButtons.removeFromRight(juce::jmin(65, rightButtons.getWidth())).reduced(2, 3));
    }

    // Skip CRT display area
    totalBounds.removeFromTop(130);

    // Audition strip
    auto auditionArea = totalBounds.removeFromTop(32).reduced(16, 2);
    auditionArea.removeFromLeft(130); // Skip label area
    impulseTriggerBtn.setBounds(auditionArea.removeFromLeft(125).reduced(4, 2));
    hammerTriggerBtn.setBounds(auditionArea.removeFromLeft(125).reduced(4, 2));
    chordTriggerBtn.setBounds(auditionArea.removeFromLeft(125).reduced(4, 2));

    // 6 Signal-Flow Decks Grid
    auto gridArea = totalBounds.reduced(16, 6);
    const int numCols = 3;
    const int numRows = 2;
    const int colWidth = gridArea.getWidth() / numCols;
    const int rowHeight = gridArea.getHeight() / numRows;

    auto getCellBounds = [&](int row, int col) {
        return juce::Rectangle<int>(gridArea.getX() + col * colWidth,
                                    gridArea.getY() + row * rowHeight,
                                    colWidth, rowHeight).reduced(4);
    };

    auto layoutKnob = [](KnobSlot* slot, juce::Rectangle<int> area) {
        if (slot == nullptr) return;
        auto labelArea = area.removeFromBottom(16);
        slot->nameLabel.setBounds(labelArea);
        slot->slider.setBounds(area);
    };

    auto layoutCombo = [](ComboSlot* slot, juce::Rectangle<int> area) {
        if (slot == nullptr) return;
        auto labelArea = area.removeFromTop(16);
        slot->label.setBounds(labelArea);
        slot->comboBox.setBounds(area.reduced(4, 2));
    };

    // DECK 1: INPUT & PRE-DELAY (Row 0, Col 0)
    // 4 Knobs: input_trim_db, pre_delay_ms, dry_wet_mix, early_late_mix
    {
        auto cell = getCellBounds(0, 0);
        cell.removeFromTop(22); // Header space
        auto topRow = cell.removeFromTop(cell.getHeight() / 2);
        auto bottomRow = cell;

        layoutKnob(findKnob(rb26::ParamIDs::inputTrimDb), topRow.removeFromLeft(topRow.getWidth() / 2).reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::preDelayMs), topRow.reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::dryWetMix), bottomRow.removeFromLeft(bottomRow.getWidth() / 2).reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::earlyLateMix), bottomRow.reduced(4));
    }

    // DECK 2: LOW MODAL MATRIX (Row 0, Col 1)
    // 4 Knobs: low_crossover_hz, bass_rt60_mult, punch_ducking, sub_mono_hz
    {
        auto cell = getCellBounds(0, 1);
        cell.removeFromTop(22);
        auto topRow = cell.removeFromTop(cell.getHeight() / 2);
        auto bottomRow = cell;

        layoutKnob(findKnob(rb26::ParamIDs::lowCrossoverHz), topRow.removeFromLeft(topRow.getWidth() / 2).reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::bassRt60Mult), topRow.reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::punchDucking), bottomRow.removeFromLeft(bottomRow.getWidth() / 2).reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::subMonoHz), bottomRow.reduced(4));
    }

    // DECK 3: REVERB TANK (FDN) (Row 0, Col 2)
    // 4 Knobs: room_size, decay_rt60_sec, high_damping_hz, diffusion_density
    // 1 Toggle: freeze_hold
    {
        auto cell = getCellBounds(0, 2);
        cell.removeFromTop(22);
        auto btnArea = cell.removeFromBottom(24).reduced(6, 2);
        if (auto* btn = findButton(rb26::ParamIDs::freezeHold))
        {
            btn->button.setBounds(btnArea);
        }

        auto topRow = cell.removeFromTop(cell.getHeight() / 2);
        auto bottomRow = cell;

        layoutKnob(findKnob(rb26::ParamIDs::roomSize), topRow.removeFromLeft(topRow.getWidth() / 2).reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::decayRt60Sec), topRow.reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::highDampingHz), bottomRow.removeFromLeft(bottomRow.getWidth() / 2).reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::diffusionDensity), bottomRow.reduced(4));
    }

    // DECK 4: PITCH DIFFUSION (Row 1, Col 0)
    // 6 Knobs: shimmer_send, dimmer_send, pitch_blend, pitch_feedback, pitch_delay_ms, pitch_boost
    // 2 Combos: shimmer_interval, dimmer_interval
    {
        auto cell = getCellBounds(1, 0);
        cell.removeFromTop(22);
        
        // Combos row at bottom
        auto comboRow = cell.removeFromBottom(42);
        layoutCombo(findCombo(rb26::ParamIDs::shimmerInterval), comboRow.removeFromLeft(comboRow.getWidth() / 2).reduced(2));
        layoutCombo(findCombo(rb26::ParamIDs::dimmerInterval), comboRow.reduced(2));

        auto topRow = cell.removeFromTop(cell.getHeight() / 2);
        auto bottomRow = cell;

        const int topW = topRow.getWidth() / 3;
        layoutKnob(findKnob(rb26::ParamIDs::shimmerSend), topRow.removeFromLeft(topW).reduced(2));
        layoutKnob(findKnob(rb26::ParamIDs::dimmerSend), topRow.removeFromLeft(topW).reduced(2));
        layoutKnob(findKnob(rb26::ParamIDs::pitchBlend), topRow.reduced(2));

        const int btmW = bottomRow.getWidth() / 3;
        layoutKnob(findKnob(rb26::ParamIDs::pitchFeedback), bottomRow.removeFromLeft(btmW).reduced(2));
        layoutKnob(findKnob(rb26::ParamIDs::pitchDelayMs), bottomRow.removeFromLeft(btmW).reduced(2));
        layoutKnob(findKnob(rb26::ParamIDs::pitchBoost), bottomRow.reduced(2));
    }

    // DECK 5: TAIL MODULATION (Row 1, Col 1)
    // 3 Knobs: tail_mod_rate_hz, tail_mod_depth_ms, tail_bloom_ms
    {
        auto cell = getCellBounds(1, 1);
        cell.removeFromTop(22);
        const int knobW = cell.getWidth() / 3;
        layoutKnob(findKnob(rb26::ParamIDs::tailModRateHz), cell.removeFromLeft(knobW).reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::tailModDepthMs), cell.removeFromLeft(knobW).reduced(4));
        layoutKnob(findKnob(rb26::ParamIDs::tailBloomMs), cell.reduced(4));
    }

    // DECK 6: MASTER BUS (Row 1, Col 2)
    // 2 Knobs: stereo_width, output_trim_db
    // 1 Toggle: limiter_enable
    {
        auto cell = getCellBounds(1, 2);
        cell.removeFromTop(22);
        auto btnArea = cell.removeFromBottom(28).reduced(8, 3);
        if (auto* btn = findButton(rb26::ParamIDs::limiterEnable))
        {
            btn->button.setBounds(btnArea);
        }

        auto knobArea = cell;
        const int knobW = knobArea.getWidth() / 2;
        layoutKnob(findKnob(rb26::ParamIDs::stereoWidth), knobArea.removeFromLeft(knobW).reduced(6));
        layoutKnob(findKnob(rb26::ParamIDs::outputTrimDb), knobArea.reduced(6));
    }
}

BRAUN_RB26AudioProcessorEditor::KnobSlot* BRAUN_RB26AudioProcessorEditor::findKnob(const juce::ParameterID& id)
{
    return findKnob(id.getParamID());
}

BRAUN_RB26AudioProcessorEditor::KnobSlot* BRAUN_RB26AudioProcessorEditor::findKnob(const juce::String& paramId)
{
    for (auto& slot : knobSlots)
    {
        if (slot->paramId.equalsIgnoreCase(paramId))
            return slot.get();
    }

    for (const auto& item : rb26::getParameterMetadataTable())
    {
        if (paramId.equalsIgnoreCase(item.apvtsId) || paramId.equalsIgnoreCase(item.webId))
        {
            for (auto& slot : knobSlots)
            {
                if (slot->paramId.equalsIgnoreCase(item.apvtsId))
                    return slot.get();
            }
        }
    }

    juce::String cleanId = paramId;
    if (cleanId.startsWithIgnoreCase("knob-") || cleanId.startsWithIgnoreCase("knob_"))
        cleanId = cleanId.substring(5);
    cleanId = cleanId.replaceCharacter('-', '_');

    for (auto& slot : knobSlots)
    {
        if (slot->paramId.equalsIgnoreCase(cleanId))
            return slot.get();
    }
    for (const auto& item : rb26::getParameterMetadataTable())
    {
        if (cleanId.equalsIgnoreCase(item.apvtsId) || cleanId.equalsIgnoreCase(item.webId))
        {
            for (auto& slot : knobSlots)
            {
                if (slot->paramId.equalsIgnoreCase(item.apvtsId))
                    return slot.get();
            }
        }
    }

    return nullptr;
}

BRAUN_RB26AudioProcessorEditor::ButtonSlot* BRAUN_RB26AudioProcessorEditor::findButton(const juce::ParameterID& id)
{
    const juce::String target = id.getParamID();
    for (auto& slot : buttonSlots)
    {
        if (slot->paramId == target)
            return slot.get();
    }
    return nullptr;
}

BRAUN_RB26AudioProcessorEditor::ComboSlot* BRAUN_RB26AudioProcessorEditor::findCombo(const juce::ParameterID& id)
{
    const juce::String target = id.getParamID();
    for (auto& slot : comboSlots)
    {
        if (slot->paramId == target)
            return slot.get();
    }
    return nullptr;
}

void BRAUN_RB26AudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!useNativeUI)
        return;

    if (e.mods.isPopupMenu())
    {
        if (dynamic_cast<juce::TextEditor*>(e.eventComponent) != nullptr)
            return;

        for (auto& slot : knobSlots)
        {
            if (e.eventComponent == &slot->slider || slot->slider.isParentOf(e.eventComponent)
                || e.eventComponent == &slot->nameLabel || slot->nameLabel.isParentOf(e.eventComponent))
            {
                showKnobContextMenu(*slot, e.getScreenPosition());
                return;
            }
        }
    }
}

void BRAUN_RB26AudioProcessorEditor::showKnobContextMenu(KnobSlot& slot, juce::Point<int> screenPos)
{
    auto* param = processorRef.getAPVTS().getParameter(slot.paramId);
    if (auto* hContext = getHostContext())
    {
        if (auto hostMenu = hContext->getContextMenuForParameter(param))
        {
            auto menu = hostMenu->getEquivalentPopupMenu();
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)).withParentComponent(this));
            return;
        }
    }

    auto* rangedParam = dynamic_cast<juce::RangedAudioParameter*>(param);

    juce::PopupMenu menu;
    const juce::String currentValueStr = slot.slider.getTextFromValue(slot.slider.getValue());
    const juce::String title = slot.nameLabel.getText().isNotEmpty()
        ? (slot.nameLabel.getText().toUpperCase() + "  (" + currentValueStr + ")")
        : (slot.paramId.toUpperCase() + "  (" + currentValueStr + ")");
    menu.addSectionHeader(title);
    menu.addSeparator();

    juce::String defaultText;
    float defaultDenormVal = 0.0f;
    if (rangedParam != nullptr)
    {
        defaultDenormVal = rangedParam->getNormalisableRange().convertFrom0to1(rangedParam->getDefaultValue());
        defaultText = rangedParam->getText(rangedParam->getDefaultValue(), 1024);
        if (defaultText.isEmpty())
            defaultText = slot.slider.getTextFromValue(defaultDenormVal);
    }
    else
    {
        defaultDenormVal = static_cast<float>(slot.slider.getMinimum());
        defaultText = slot.slider.getTextFromValue(defaultDenormVal);
    }

    menu.addItem(1, "Reset to Default (" + defaultText + ")");
    menu.addItem(2, "Set to Minimum (" + slot.slider.getTextFromValue(slot.slider.getMinimum()) + ")");
    menu.addItem(3, "Set to Maximum (" + slot.slider.getTextFromValue(slot.slider.getMaximum()) + ")");
    menu.addSeparator();
    menu.addItem(4, "Set to Exact Value...");

    juce::Component::SafePointer<BRAUN_RB26AudioProcessorEditor> safeThis(this);
    const juce::String paramId = slot.paramId;

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)).withParentComponent(this),
        [safeThis, paramId, defaultDenormVal](int result)
        {
            if (safeThis == nullptr || result <= 0)
                return;

            auto* currentSlot = safeThis->findKnob(paramId);
            if (currentSlot == nullptr)
                return;

            if (result == 1) // Reset to Default
            {
                currentSlot->slider.setValue(defaultDenormVal, juce::sendNotificationSync);
            }
            else if (result == 2) // Minimum
            {
                currentSlot->slider.setValue(currentSlot->slider.getMinimum(), juce::sendNotificationSync);
            }
            else if (result == 3) // Maximum
            {
                currentSlot->slider.setValue(currentSlot->slider.getMaximum(), juce::sendNotificationSync);
            }
            else if (result == 4) // Exact Value
            {
                if (safeThis->isNativeModeActive())
                {
                    currentSlot->slider.showTextBox();
                }
            }
        });
}


