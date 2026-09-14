#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>
#include <atomic>

namespace rb26
{

//==============================================================================
/**
 * @struct BraunColours
 * @brief Dieter Rams / Braun functionalist design color tokens with exact AS-42 sibling parity.
 */
struct BraunColours
{
    // Colour IDs for LookAndFeel palette
    enum ColourIds
    {
        bgAppColourId               = 0x2600100,
        bgPanelColourId             = 0x2600101,
        bgPanelInsetColourId        = 0x2600102,
        bgBezelColourId             = 0x2600103,
        borderLineColourId          = 0x2600104,
        borderSubtleColourId        = 0x2600105,
        textPrimaryColourId         = 0x2600106,
        textSecondaryColourId       = 0x2600107,
        textMutedColourId           = 0x2600108,
        knobCapLightId              = 0x2600109,
        knobCapDarkId               = 0x260010A,
        knobBorderColourId          = 0x260010B,
        knobIndicatorColourId       = 0x260010C,
        knobTrackColourId           = 0x260010D,
        knobFillColourId            = 0x260010E,
        braunOrangeColourId         = 0x260010F,
        braunGreenColourId          = 0x2600110,
        braunAmberColourId          = 0x2600111,
        phosphorColourId            = 0x2600112,
        phosphorGlowColourId        = 0x2600113
    };

    // Static Color Definitions: Light Chassis (Default)
    static constexpr uint32_t Light_BgApp          = 0xFFECEBE4;
    static constexpr uint32_t Light_BgPanel        = 0xFFE2E0D8;
    static constexpr uint32_t Light_BgPanelInset   = 0xFFD7D5CC;
    static constexpr uint32_t Light_BgBezel        = 0xFF121414;
    static constexpr uint32_t Light_BorderLine     = 0xFFCBC8BD;
    static constexpr uint32_t Light_BorderSubtle   = 0xFFD8D6CD;
    static constexpr uint32_t Light_TextPrimary    = 0xFF1C1D1E;
    static constexpr uint32_t Light_TextSecondary  = 0xFF5E6064;
    static constexpr uint32_t Light_TextMuted      = 0xFF8E9094;
    static constexpr uint32_t Light_KnobCapLight   = 0xFFE0DED7;
    static constexpr uint32_t Light_KnobCapDark    = 0xFFC8C5BB;
    static constexpr uint32_t Light_KnobBorder     = 0xFFBBB8AD;
    static constexpr uint32_t Light_KnobIndicator  = 0xFF1C1D1E;
    static constexpr uint32_t Light_KnobTrack      = 0xFFD0CEC4;
    static constexpr uint32_t Light_KnobFill       = 0xFF1C1D1E;

    // Static Color Definitions: Dark Chassis (Anthracite / Matte Black)
    static constexpr uint32_t Dark_BgApp           = 0xFF141517;
    static constexpr uint32_t Dark_BgPanel         = 0xFF1E2023;
    static constexpr uint32_t Dark_BgPanelInset    = 0xFF151618;
    static constexpr uint32_t Dark_BgBezel         = 0xFF0A0B0C;
    static constexpr uint32_t Dark_BorderLine      = 0xFF3A3A3A;
    static constexpr uint32_t Dark_BorderSubtle    = 0xFF2C2E33;
    static constexpr uint32_t Dark_TextPrimary     = 0xFFF0F0F0;
    static constexpr uint32_t Dark_TextSecondary   = 0xFFBDBDBD;
    static constexpr uint32_t Dark_TextMuted       = 0xFF8E8E8E;
    static constexpr uint32_t Dark_KnobCapLight    = 0xFF35373C;
    static constexpr uint32_t Dark_KnobCapDark     = 0xFF232428;
    static constexpr uint32_t Dark_KnobBorder      = 0xFF4A4D52;
    static constexpr uint32_t Dark_KnobIndicator   = 0xFFF0F0F0;
    static constexpr uint32_t Dark_KnobTrack       = 0xFF2A2C30;
    static constexpr uint32_t Dark_KnobFill        = 0xFFEE592B; // Signature orange in dark mode

    // Signature Accent Colors (Shared)
    static constexpr uint32_t Accent_BraunOrange   = 0xFFEE592B;
    static constexpr uint32_t Accent_BraunGreen    = 0xFF24FF6A;
    static constexpr uint32_t Accent_BraunAmber    = 0xFFE5A93C;
    static constexpr uint32_t PhosphorGreen        = 0xFF24FF6A;
};

//==============================================================================
/**
 * @class BraunLookAndFeel
 * @brief Custom JUCE 8 LookAndFeel implementing Dieter Rams functionalist UI for the RB-26.
 */
class BraunLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BraunLookAndFeel();
    ~BraunLookAndFeel() override = default;

    // Theme Management
    void setDarkTheme(bool useDarkTheme);
    bool isDarkTheme() const noexcept { return darkThemeActive; }

    // Rotary Knob Hierarchy
    enum class KnobTier
    {
        Trim,       // 42px small knob (Pre-delay, Diff density, Output trim)
        Secondary,  // 52px medium knob (X-over, Damping, Mod rate, Pitch regen)
        Hero        // 64px large knob (Decay RT60, Room size, Dry/Wet mix, Shim/Dim blend)
    };

    static KnobTier getKnobTierForBounds(int width, int height) noexcept;

    // juce::LookAndFeel_V4 Overrides
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawTickBox(juce::Graphics& g, juce::Component& component,
                     float x, float y, float w, float h,
                     bool ticked, bool isEnabled,
                     bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont(juce::Label&) override;

private:
    bool darkThemeActive { false };
    void applyThemeColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BraunLookAndFeel)
};

//==============================================================================
/**
 * @class CrtVisualizerComponent
 * @brief 3-Mode Braun CRT Phosphor Display Component with 8x6 precision graticule.
 */
class CrtVisualizerComponent : public juce::Component,
                               private juce::Timer
{
public:
    enum class DisplayMode
    {
        EdcWaterfall,       // Mode 1: RT60 Energy Decay Curve (Schroeder backward integration)
        LissajousXy,        // Mode 2: Stereo Phase Goniometer (45° rotated correlation matrix)
        SpectrumAnalyzer    // Mode 3: Real-time 1/3-Octave / 512-point Spectrum Analyzer
    };

    explicit CrtVisualizerComponent(BraunLookAndFeel& laf);
    ~CrtVisualizerComponent() override;

    void setMode(DisplayMode newMode);
    DisplayMode getMode() const noexcept { return currentMode; }

    void setPower(bool powered);
    bool isPowerOn() const noexcept { return isPowered; }

    // Real-Time Audio Telemetry Injection (Thread-safe lock-free or message-thread pushed)
    void pushAudioBlock(const float* leftChannel, const float* rightChannel, int numSamples);
    void updateTelemetry(float lowBandEnergy, float midBandEnergy, float highBandEnergy, float rt60DecayDb);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    void drawGraticule(juce::Graphics& g, juce::Rectangle<float> area);
    void drawEdcWaterfall(juce::Graphics& g, juce::Rectangle<float> area);
    void drawLissajous(juce::Graphics& g, juce::Rectangle<float> area);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> area);
    void drawStandbyBeam(juce::Graphics& g, juce::Rectangle<float> area);

    BraunLookAndFeel& lookAndFeel;
    DisplayMode currentMode { DisplayMode::EdcWaterfall };
    bool isPowered { false };

    // Telemetry and history ring buffers
    static constexpr int kHistorySize = 128;
    static constexpr int kScopeBufferSize = 512;

    std::vector<float> bufferL;
    std::vector<float> bufferR;
    int bufferWritePos { 0 };

    std::array<float, kHistorySize> edcLowHistory {};
    std::array<float, kHistorySize> edcMidHistory {};
    std::array<float, kHistorySize> edcHighHistory {};
    int historyIndex { 0 };

    float currentLowEnergy { 0.0f };
    float currentMidEnergy { 0.0f };
    float currentHighEnergy { 0.0f };
    float currentDecayDb { -60.0f };

    // Phosphor persistence decay state
    juce::Image persistenceImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrtVisualizerComponent)
};

} // namespace rb26
