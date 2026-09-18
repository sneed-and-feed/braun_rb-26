#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_data_structures/juce_data_structures.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include "LookAndFeel/BraunLookAndFeel.h"
#include <atomic>
#include <optional>
#include <array>
#include <vector>
#include <memory>

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

    // Native vs WebView GUI switching & persistence
    bool isNativeModeActive() const noexcept { return useNativeUI; }
    void setNativeMode(bool native);
    static juce::File getSettingsFile();
    static bool loadPersistedNativeUIPreference();
    static void savePersistedNativeUIPreference(bool native);

#if JUCE_WEB_BROWSER
    // Web Integration & Bridge Methods
    void handleParamChangeFromWeb(const juce::var& data);
    void handleExciterTriggerFromWeb(const juce::var& data);
    void handleStartRecordingFromWeb();
    void handleStopRecordingFromWeb();
    void sendParameterUpdateToWeb(const juce::String& apvtsId, const juce::String& webId, float newValue);
    void sendRecordingStateUpdateToWeb(bool isRecording);
    void syncAllParametersToWeb();
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);
#endif

private:
    void timerCallback() override;

    BRAUN_RB26AudioProcessor& processorRef;
    rb26::BraunLookAndFeel braunLookAndFeel;
    bool useNativeUI { false };

#if JUCE_WEB_BROWSER
    static juce::WebBrowserComponent::Options createWebOptions(BRAUN_RB26AudioProcessorEditor& editor);
    std::unique_ptr<juce::WebBrowserComponent> webComponent;
    bool initialSyncDone { false };

    // Coalescing array for all 26 APVTS parameters (prevents Win32 message loop stalls)
    static constexpr size_t kNumParams = 26;
    std::atomic<float> pendingParamValues[kNumParams] {};
    std::atomic<bool> paramDirty[kNumParams] {};

    // Windows HWND style configuration (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)
    bool hwndStylesConfigured { false };
    int hwndCheckCounter { 0 };
    void ensureHwndStyles();

    // Telemetry & Scope streaming at 60 Hz with idle throttling
    int silentFrameCounter { 0 };
    int silentTelemetryCounter { 0 };
    void sendTelemetryToWeb();
    void sendScopeDataToWeb();
#endif

    rb26::Rb26ReverbEngine::VisualizerFrame latestTelemetryFrame {};

    void registerParameterListeners();
    void unregisterParameterListeners();

    // Secondary / Native Presentation Layer: Dieter Rams Vector Graphics
    void drawBraunChassis(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawCrtDisplay(juce::Graphics& g, juce::Rectangle<int> bounds);

    // Native JUCE UI Components
    // Header controls
    juce::TextButton powerButton;
    juce::TextButton themeButton;
    juce::TextButton recordButton;
#if JUCE_WEB_BROWSER
    juce::TextButton viewModeButton;
#endif

    // Preset management controls for Native UI
    juce::Label presetLabel;
    juce::ComboBox presetComboBox;
    juce::TextButton prevPresetBtn;
    juce::TextButton nextPresetBtn;

    // Rotary Sliders & Labels for APVTS parameters
    struct KnobSlot {
        juce::String paramId;
        juce::Slider slider;
        juce::Label nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::vector<std::unique_ptr<KnobSlot>> knobSlots;

    // Buttons / Toggles
    struct ButtonSlot {
        juce::String paramId;
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    std::vector<std::unique_ptr<ButtonSlot>> buttonSlots;

    // ComboBoxes (for Shimmer and Dimmer intervals)
    struct ComboSlot {
        juce::String paramId;
        juce::Label label;
        juce::ComboBox comboBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    std::vector<std::unique_ptr<ComboSlot>> comboSlots;

    // Audition Exciter trigger buttons
    juce::TextButton impulseTriggerBtn;
    juce::TextButton hammerTriggerBtn;
    juce::TextButton chordTriggerBtn;

    void setupNativeControls();
    void updateNativeControlLayout();
    KnobSlot* findKnob(const juce::ParameterID& id);
    ButtonSlot* findButton(const juce::ParameterID& id);
    ComboSlot* findCombo(const juce::ParameterID& id);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BRAUN_RB26AudioProcessorEditor)
};
