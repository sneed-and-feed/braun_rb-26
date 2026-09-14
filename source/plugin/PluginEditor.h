#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include "LookAndFeel/BraunLookAndFeel.h"
#include <atomic>
#include <optional>
#include <array>

class BRAUN_RB26AudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::AudioProcessorValueTreeState::Listener,
                                       private juce::Timer
{
public:
    explicit BRAUN_RB26AudioProcessorEditor(BRAUN_RB26AudioProcessor&);
    ~BRAUN_RB26AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    // APVTS Listener callback
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Web Integration & Bridge Methods
    void handleParamChangeFromWeb(const juce::var& data);
    void handleExciterTriggerFromWeb(const juce::var& data);
    void sendParameterUpdateToWeb(const juce::String& apvtsId, const juce::String& webId, float newValue);
    void syncAllParametersToWeb();
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

private:
    void timerCallback() override;
    static juce::WebBrowserComponent::Options createWebOptions(BRAUN_RB26AudioProcessorEditor& editor);

    BRAUN_RB26AudioProcessor& processorRef;
    rb26::BraunLookAndFeel braunLookAndFeel;
    juce::WebBrowserComponent webComponent;
    bool initialSyncDone { false };

    // Coalescing array for all 24 APVTS parameters (prevents Win32 message loop stalls)
    static constexpr size_t kNumParams = 24;
    std::atomic<float> pendingParamValues[kNumParams] {};
    std::atomic<bool> paramDirty[kNumParams] {};

    // Windows HWND style configuration (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)
    bool hwndStylesConfigured { false };
    int hwndCheckCounter { 0 };
    void ensureHwndStyles();

    // Telemetry & Scope streaming at 60 Hz
    int silentFrameCounter { 0 };
    rb26::Rb26ReverbEngine::VisualizerFrame latestTelemetryFrame {};
    void sendTelemetryToWeb();
    void sendScopeDataToWeb();

    void registerParameterListeners();
    void unregisterParameterListeners();

    // Secondary / Fallback Presentation Layer: Dieter Rams Vector Graphics
    void drawBraunChassis(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawCrtDisplay(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawKnob(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& label, const juce::String& valueText, float normValue);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BRAUN_RB26AudioProcessorEditor)
};
