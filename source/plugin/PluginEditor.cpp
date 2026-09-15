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

// Optional BinaryData inclusion if assets were generated
#if __has_include(<BinaryData.h>)
#include <BinaryData.h>
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
</style>
</head>
<body>
<header>
  <div class="title-group">
    <h1>BRAUN RB-26</h1>
    <p>STUDIO REVERBERATION UNIT — WENIGER, ABER BESSER</p>
  </div>
  <div class="badge">ACTIVE</div>
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

<div class="rack-grid" id="deckContainer">
  <!-- Controls rendered dynamically via JS -->
</div>

<script>
const paramsMeta = [
  { id: 'pre_delay_ms', webId: 'preDelayMs', label: 'Pre-Delay', deck: 'Input & Pre-Delay', min: 0, max: 500, def: 20, unit: 'ms' },
  { id: 'dry_wet_mix', webId: 'dryWetMix', label: 'Dry/Wet', deck: 'Input & Pre-Delay', min: 0, max: 1, def: 0.35, unit: '%' },
  { id: 'early_late_mix', webId: 'earlyLateMix', label: 'Early/Late', deck: 'Input & Pre-Delay', min: 0, max: 1, def: 0.50, unit: '%' },

  { id: 'low_crossover_hz', webId: 'lowCrossoverHz', label: 'Crossover', deck: 'Low Modal Matrix', min: 60, max: 400, def: 180, unit: 'Hz' },
  { id: 'bass_rt60_mult', webId: 'bassRt60Mult', label: 'Bass Mult', deck: 'Low Modal Matrix', min: 0.2, max: 4.0, def: 1.0, unit: 'x' },
  { id: 'punch_ducking', webId: 'punchDucking', label: 'Punch Duck', deck: 'Low Modal Matrix', min: 0, max: 1, def: 0.40, unit: '%' },
  { id: 'sub_mono_hz', webId: 'subMonoHz', label: 'Sub Mono', deck: 'Low Modal Matrix', min: 20, max: 250, def: 120, unit: 'Hz' },

  { id: 'room_size', webId: 'roomSize', label: 'Room Size', deck: 'FDN Tank', min: 0.1, max: 2.0, def: 0.65, unit: '' },
  { id: 'decay_rt60_sec', webId: 'decayRt60Sec', label: 'Decay Time', deck: 'FDN Tank', min: 0.2, max: 30.0, def: 3.5, unit: 's' },
  { id: 'high_damping_hz', webId: 'highDampingHz', label: 'Damping', deck: 'FDN Tank', min: 1000, max: 20000, def: 6500, unit: 'Hz' },
  { id: 'diffusion_density', webId: 'diffusionDensity', label: 'Diffusion', deck: 'FDN Tank', min: 0, max: 1, def: 0.75, unit: '%' },
  { id: 'freeze_hold', webId: 'freezeHold', label: 'Freeze', deck: 'FDN Tank', min: 0, max: 1, def: 0, unit: '', isBool: true },

  { id: 'shimmer_send', webId: 'shimmerSend', label: 'Shimmer Send', deck: 'Pitch Diffusion', min: 0, max: 1, def: 0.30, unit: '%' },
  { id: 'dimmer_send', webId: 'dimmerSend', label: 'Dimmer Send', deck: 'Pitch Diffusion', min: 0, max: 1, def: 0.25, unit: '%' },
  { id: 'shimmer_interval', webId: 'shimmerInterval', label: 'Shimmer Int', deck: 'Pitch Diffusion', min: 0, max: 2, def: 1, unit: '', isChoice: true, choices: ['+7 st', '+12 st', '+24 st'] },
  { id: 'dimmer_interval', webId: 'dimmerInterval', label: 'Dimmer Int', deck: 'Pitch Diffusion', min: 0, max: 1, def: 0, unit: '', isChoice: true, choices: ['-12 st', '-24 st'] },
  { id: 'pitch_blend', webId: 'pitchBlend', label: 'Pitch Blend', deck: 'Pitch Diffusion', min: -1, max: 1, def: 0, unit: '' },
  { id: 'pitch_feedback', webId: 'pitchFeedback', label: 'Pitch FB', deck: 'Pitch Diffusion', min: 0, max: 0.95, def: 0.50, unit: '%' },

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
  decks[p.deck].appendChild(ku);
});

function emitParam(id, value) {
  if (window.__JUCE__ && window.__JUCE__.backend) {
    window.__JUCE__.backend.emitEvent('paramChange', { id, value });
  }
}

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
        .withUserScript("window.__IS_JUCE__ = true;")
        .withNativeIntegrationEnabled()
        .withResourceProvider([&editor](const juce::String& url) {
            return editor.getResource(url);
        })
        .withEventListener("paramChange", [&editor](const juce::var& data) {
            editor.handleParamChangeFromWeb(data);
        })
        .withEventListener("exciterTrigger", [&editor](const juce::var& data) {
            editor.handleExciterTriggerFromWeb(data);
        });

    return options;
}

BRAUN_RB26AudioProcessorEditor::BRAUN_RB26AudioProcessorEditor(BRAUN_RB26AudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      webComponent(createWebOptions(*this))
{
    setLookAndFeel(&braunLookAndFeel);
    setOpaque(true);
    webComponent.setOpaque(true);

    addAndMakeVisible(webComponent);
    registerParameterListeners();

    // 19" studio rack aspect ratio: 1080x720 default, resizable
    setSize(1080, 720);
    setResizable(true, true);
    setResizeLimits(800, 560, 2560, 1440);

    // 60 Hz telemetry polling timer for smooth phosphor CRT waterfall and goniometer
    startTimerHz(60);

    webComponent.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
}

BRAUN_RB26AudioProcessorEditor::~BRAUN_RB26AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    stopTimer();
    unregisterParameterListeners();
}

void BRAUN_RB26AudioProcessorEditor::resized()
{
    webComponent.setBounds(getLocalBounds());
}

void BRAUN_RB26AudioProcessorEditor::parentHierarchyChanged()
{
    AudioProcessorEditor::parentHierarchyChanged();
    hwndStylesConfigured = false;
    ensureHwndStyles();
}

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

void BRAUN_RB26AudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
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
}

void BRAUN_RB26AudioProcessorEditor::sendParameterUpdateToWeb(const juce::String& apvtsId, const juce::String& webId, float newValue)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", webId);
    obj->setProperty("apvtsId", apvtsId);
    obj->setProperty("value", newValue);
    webComponent.emitEventIfBrowserIsVisible("paramUpdate", juce::var(obj));
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
}

void BRAUN_RB26AudioProcessorEditor::timerCallback()
{
    // Windows HWND style guard
    if (!hwndStylesConfigured || ++hwndCheckCounter >= 60)
    {
        hwndCheckCounter = 0;
        ensureHwndStyles();
    }

    // Initial state synchronization on webview ready
    if (!initialSyncDone && webComponent.isVisible())
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

    // Repaint native presentation layer fallback
    if (!webComponent.isVisible())
    {
        repaint();
    }
}

void BRAUN_RB26AudioProcessorEditor::sendTelemetryToWeb()
{
    if (!webComponent.isVisible())
        return;

    rb26::Rb26ReverbEngine::VisualizerFrame frame;
    bool hasNewFrame = false;
    while (processorRef.popVisualizerFrame(frame))
    {
        latestTelemetryFrame = frame;
        hasNewFrame = true;
    }

    if (!hasNewFrame)
        return;

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

    webComponent.emitEventIfBrowserIsVisible("telemetryFrame", juce::var(obj));
}

void BRAUN_RB26AudioProcessorEditor::sendScopeDataToWeb()
{
    if (!webComponent.isVisible())
        return;

    constexpr int kSamples = 240;
    float sL[kSamples];
    float sR[kSamples];
    processorRef.getScopeSamples(sL, sR, kSamples);

    bool hasSignal = false;
    for (int i = 0; i < kSamples; ++i)
    {
        if (std::abs(sL[i]) > 0.001f || std::abs(sR[i]) > 0.001f)
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

    juce::Array<juce::var> sampleArray;
    sampleArray.ensureStorageAllocated(kSamples);
    for (int i = 0; i < kSamples; ++i)
    {
        const float mono = 0.5f * (sL[i] + sR[i]);
        sampleArray.add(mono);
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty("samples", juce::var(sampleArray));
    webComponent.emitEventIfBrowserIsVisible("scopeFrame", juce::var(obj));
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

    if (incomingId.equalsIgnoreCase("power"))
    {
        processorRef.setPower(incomingVal > 0.5f);
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

std::optional<juce::WebBrowserComponent::Resource> BRAUN_RB26AudioProcessorEditor::getResource(const juce::String& url)
{
    juce::String path = url;

    // Strip virtual hostname
    if (path.startsWithIgnoreCase("https://juce.backend/"))
        path = path.substring(21);
    else if (path.startsWithIgnoreCase("http://juce.backend/"))
        path = path.substring(20);

    const int queryIdx = path.indexOfChar('?');
    if (queryIdx >= 0) path = path.substring(0, queryIdx);
    const int hashIdx = path.indexOfChar('#');
    if (hashIdx >= 0) path = path.substring(0, hashIdx);

    while (path.startsWithChar('/') || path.startsWithChar('\\'))
        path = path.substring(1);

    if (path.isEmpty())
        path = "index.html";

    juce::String mimeType = "application/octet-stream";
    if (path.endsWithIgnoreCase(".html") || path.endsWithIgnoreCase(".htm")) mimeType = "text/html; charset=utf-8";
    else if (path.endsWithIgnoreCase(".css")) mimeType = "text/css; charset=utf-8";
    else if (path.endsWithIgnoreCase(".js") || path.endsWithIgnoreCase(".mjs")) mimeType = "text/javascript; charset=utf-8";
    else if (path.endsWithIgnoreCase(".json")) mimeType = "application/json";
    else if (path.endsWithIgnoreCase(".svg")) mimeType = "image/svg+xml";
    else if (path.endsWithIgnoreCase(".png")) mimeType = "image/png";

    // 1. Search local filesystem (rb-26/ui, rb-26/web, ui, web)
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
    if (auto res = checkDiskFile(cwd.getChildFile("ui").getChildFile(path))) return res;
    if (auto res = checkDiskFile(cwd.getChildFile("web").getChildFile(path))) return res;
    if (auto res = checkDiskFile(cwd.getChildFile("rb-26/ui").getChildFile(path))) return res;
    if (auto res = checkDiskFile(cwd.getChildFile("rb-26/web").getChildFile(path))) return res;
    if (auto res = checkDiskFile(cwd.getChildFile(path))) return res;

    // Search relative to executable
    auto dir = juce::File::getSpecialLocation(juce::File::SpecialLocationType::currentExecutableFile).getParentDirectory();
    for (int depth = 0; depth < 5; ++depth)
    {
        if (auto res = checkDiskFile(dir.getChildFile("ui").getChildFile(path))) return res;
        if (auto res = checkDiskFile(dir.getChildFile("web").getChildFile(path))) return res;
        if (auto res = checkDiskFile(dir.getChildFile("rb-26/ui").getChildFile(path))) return res;
        if (auto res = checkDiskFile(dir.getChildFile("rb-26/web").getChildFile(path))) return res;
        if (auto res = checkDiskFile(dir.getChildFile(path))) return res;
        dir = dir.getParentDirectory();
    }

    // 2. Embedded fallback HTML
    if (path.equalsIgnoreCase("index.html"))
    {
        const size_t len = std::strlen(kEmbeddedBraunFallbackHtml);
        std::vector<std::byte> bytes(len);
        std::memcpy(bytes.data(), kEmbeddedBraunFallbackHtml, len);
        return juce::WebBrowserComponent::Resource { std::move(bytes), mimeType };
    }

    return std::nullopt;
}

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
    // Matte dark anthracite chassis (#141517)
    g.fillAll(braunLookAndFeel.findColour(rb26::BraunColours::bgAppColourId));

    // Outer precision bevel & shadow
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
    g.drawText("BRAUN RB-26", headerArea.removeFromLeft(200).reduced(16, 0), juce::Justification::centredLeft);

    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::textMutedColourId));
    g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    g.drawText("STUDIO REVERBERATION UNIT — WENIGER, ABER BESSER", headerArea.reduced(16, 0), juce::Justification::centredLeft);

    // Braun Orange Active Indicator Badge
    auto badgeArea = headerArea.removeFromRight(120).reduced(16, 14);
    g.setColour(braunLookAndFeel.findColour(rb26::BraunColours::braunOrangeColourId));
    g.fillRoundedRectangle(badgeArea.toFloat(), 2.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("STUDIO FX", badgeArea, juce::Justification::centred);

    // Central CRT Phosphor Visualizer Scope
    auto crtArea = bounds.removeFromTop(130).reduced(16, 8);
    drawCrtDisplay(g, crtArea);

    // 6 Signal-Flow Decks Grid
    auto gridArea = bounds.reduced(16, 8);
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
            
            g.setColour(juce::Colour(0xff1C1D20));
            g.fillRoundedRectangle(cell.toFloat(), 3.0f);
            g.setColour(juce::Colour(0xff2E3035));
            g.drawRoundedRectangle(cell.toFloat(), 3.0f, 1.0f);

            // Deck header
            auto deckHeader = cell.removeFromTop(24);
            g.setColour(juce::Colour(0xffEE592B));
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText(deckTitles[idx], deckHeader.reduced(8, 0), juce::Justification::centredLeft);
        }
    }
}

void BRAUN_RB26AudioProcessorEditor::drawCrtDisplay(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // CRT Bezel
    g.setColour(juce::Colour(0xff090B0A));
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

    g.setColour(juce::Colour(0xff24FF6A)); // Phosphor Green
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

    drawBar("IN L", latestTelemetryFrame.inputRmsL, juce::Colour(0xff24FF6A), 4);
    drawBar("IN R", latestTelemetryFrame.inputRmsR, juce::Colour(0xff24FF6A), 22);
    drawBar("OUT L", latestTelemetryFrame.outputRmsL, juce::Colour(0xff24FF6A), 40);
    drawBar("OUT R", latestTelemetryFrame.outputRmsR, juce::Colour(0xff24FF6A), 58);
    drawBar("CORR", 0.5f * (latestTelemetryFrame.correlation + 1.0f), juce::Colour(0xff24B8FF), 76);
}
