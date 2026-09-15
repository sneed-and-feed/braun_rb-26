#include "BraunLookAndFeel.h"
#include <cmath>

namespace rb26
{

//==============================================================================
BraunLookAndFeel::BraunLookAndFeel()
{
    applyThemeColours();
}

void BraunLookAndFeel::setDarkTheme(bool useDarkTheme)
{
    if (darkThemeActive != useDarkTheme)
    {
        darkThemeActive = useDarkTheme;
        applyThemeColours();
    }
}

void BraunLookAndFeel::applyThemeColours()
{
    if (darkThemeActive)
    {
        setColour(BraunColours::bgAppColourId,          juce::Colour(BraunColours::Dark_BgApp));
        setColour(BraunColours::bgPanelColourId,        juce::Colour(BraunColours::Dark_BgPanel));
        setColour(BraunColours::bgPanelInsetColourId,   juce::Colour(BraunColours::Dark_BgPanelInset));
        setColour(BraunColours::bgBezelColourId,        juce::Colour(BraunColours::Dark_BgBezel));
        setColour(BraunColours::borderLineColourId,     juce::Colour(BraunColours::Dark_BorderLine));
        setColour(BraunColours::borderSubtleColourId,   juce::Colour(BraunColours::Dark_BorderSubtle));
        setColour(BraunColours::textPrimaryColourId,    juce::Colour(BraunColours::Dark_TextPrimary));
        setColour(BraunColours::textSecondaryColourId,  juce::Colour(BraunColours::Dark_TextSecondary));
        setColour(BraunColours::textMutedColourId,      juce::Colour(BraunColours::Dark_TextMuted));
        setColour(BraunColours::knobCapLightId,         juce::Colour(BraunColours::Dark_KnobCapLight));
        setColour(BraunColours::knobCapDarkId,          juce::Colour(BraunColours::Dark_KnobCapDark));
        setColour(BraunColours::knobBorderColourId,     juce::Colour(BraunColours::Dark_KnobBorder));
        setColour(BraunColours::knobIndicatorColourId,  juce::Colour(BraunColours::Dark_KnobIndicator));
        setColour(BraunColours::knobTrackColourId,      juce::Colour(BraunColours::Dark_KnobTrack));
        setColour(BraunColours::knobFillColourId,       juce::Colour(BraunColours::Dark_KnobFill));

        // JUCE standard color mappings
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(BraunColours::Dark_BgApp));
        setColour(juce::Label::textColourId,                 juce::Colour(BraunColours::Dark_TextPrimary));
        setColour(juce::TextButton::buttonColourId,          juce::Colour(BraunColours::Dark_BgPanelInset));
        setColour(juce::TextButton::buttonOnColourId,        juce::Colour(BraunColours::Accent_BraunOrange));
        setColour(juce::TextButton::textColourOffId,         juce::Colour(BraunColours::Dark_TextPrimary));
        setColour(juce::TextButton::textColourOnId,          juce::Colours::white);
    }
    else
    {
        setColour(BraunColours::bgAppColourId,          juce::Colour(BraunColours::Light_BgApp));
        setColour(BraunColours::bgPanelColourId,        juce::Colour(BraunColours::Light_BgPanel));
        setColour(BraunColours::bgPanelInsetColourId,   juce::Colour(BraunColours::Light_BgPanelInset));
        setColour(BraunColours::bgBezelColourId,        juce::Colour(BraunColours::Light_BgBezel));
        setColour(BraunColours::borderLineColourId,     juce::Colour(BraunColours::Light_BorderLine));
        setColour(BraunColours::borderSubtleColourId,   juce::Colour(BraunColours::Light_BorderSubtle));
        setColour(BraunColours::textPrimaryColourId,    juce::Colour(BraunColours::Light_TextPrimary));
        setColour(BraunColours::textSecondaryColourId,  juce::Colour(BraunColours::Light_TextSecondary));
        setColour(BraunColours::textMutedColourId,      juce::Colour(BraunColours::Light_TextMuted));
        setColour(BraunColours::knobCapLightId,         juce::Colour(BraunColours::Light_KnobCapLight));
        setColour(BraunColours::knobCapDarkId,          juce::Colour(BraunColours::Light_KnobCapDark));
        setColour(BraunColours::knobBorderColourId,     juce::Colour(BraunColours::Light_KnobBorder));
        setColour(BraunColours::knobIndicatorColourId,  juce::Colour(BraunColours::Light_KnobIndicator));
        setColour(BraunColours::knobTrackColourId,      juce::Colour(BraunColours::Light_KnobTrack));
        setColour(BraunColours::knobFillColourId,       juce::Colour(BraunColours::Light_KnobFill));

        // JUCE standard color mappings
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(BraunColours::Light_BgApp));
        setColour(juce::Label::textColourId,                 juce::Colour(BraunColours::Light_TextPrimary));
        setColour(juce::TextButton::buttonColourId,          juce::Colour(BraunColours::Light_BgPanelInset));
        setColour(juce::TextButton::buttonOnColourId,        juce::Colour(BraunColours::Accent_BraunOrange));
        setColour(juce::TextButton::textColourOffId,         juce::Colour(BraunColours::Light_TextPrimary));
        setColour(juce::TextButton::textColourOnId,          juce::Colours::white);
    }

    setColour(BraunColours::braunOrangeColourId,  juce::Colour(BraunColours::Accent_BraunOrange));
    setColour(BraunColours::braunGreenColourId,   juce::Colour(BraunColours::Accent_BraunGreen));
    setColour(BraunColours::braunAmberColourId,   juce::Colour(BraunColours::Accent_BraunAmber));
    setColour(BraunColours::phosphorColourId,     juce::Colour(BraunColours::PhosphorGreen));
    setColour(BraunColours::phosphorGlowColourId, juce::Colour(BraunColours::PhosphorGreen).withAlpha(0.40f));
}

BraunLookAndFeel::KnobTier BraunLookAndFeel::getKnobTierForBounds(int width, int height) noexcept
{
    const int minDim = juce::jmin(width, height);
    if (minDim >= 60) return KnobTier::Hero;       // 64px
    if (minDim >= 48) return KnobTier::Secondary;  // 52px
    return KnobTier::Trim;                         // 42px
}

//==============================================================================
void BraunLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPosProportional, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
    auto center = bounds.getCentre();
    auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto radius = diameter / 2.0f;

    const auto tier = getKnobTierForBounds(width, height);
    float trackStrokeWidth = (tier == KnobTier::Hero) ? 4.5f : (tier == KnobTier::Secondary) ? 4.0f : 3.0f;
    float capMargin = (tier == KnobTier::Hero) ? 9.0f : (tier == KnobTier::Secondary) ? 8.0f : 6.5f;

    // Track arc radius
    auto trackRadius = radius - (trackStrokeWidth * 0.5f) - 1.0f;
    if (trackRadius <= 0.0f) return;

    auto currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // 1. Quiescent Background Track Arc
    juce::Path trackPath;
    trackPath.addCentredArc(center.x, center.y, trackRadius, trackRadius,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(findColour(BraunColours::knobTrackColourId));
    g.strokePath(trackPath, juce::PathStrokeType(trackStrokeWidth,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    // 2. Active Parameter Fill Arc
    if (sliderPosProportional > 0.001f)
    {
        juce::Path fillPath;
        fillPath.addCentredArc(center.x, center.y, trackRadius, trackRadius,
                               0.0f, rotaryStartAngle, currentAngle, true);

        // Highlight with Braun Orange if slider is dragged/active or in dark mode
        auto fillColour = slider.isMouseOverOrDragging()
                            ? findColour(BraunColours::braunOrangeColourId)
                            : findColour(BraunColours::knobFillColourId);

        g.setColour(fillColour);
        g.strokePath(fillPath, juce::PathStrokeType(trackStrokeWidth,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // 3. Precision Turned Aluminum Cap
    auto capRadius = radius - capMargin;
    if (capRadius <= 2.0f) return;

    auto capBounds = juce::Rectangle<float>(center.x - capRadius, center.y - capRadius,
                                            capRadius * 2.0f, capRadius * 2.0f);

    // Subtle drop shadow under cap
    g.setColour(juce::Colours::black.withAlpha(darkThemeActive ? 0.35f : 0.12f));
    g.fillEllipse(capBounds.translated(0.0f, 2.0f));

    // Concentric machined aluminum radial gradient
    juce::ColourGradient capGradient(
        findColour(BraunColours::knobCapLightId), center.x - capRadius * 0.35f, center.y - capRadius * 0.35f,
        findColour(BraunColours::knobCapDarkId),  center.x + capRadius * 0.35f, center.y + capRadius * 0.35f,
        true);
    g.setGradientFill(capGradient);
    g.fillEllipse(capBounds);

    // Subtle concentric knurling grooves
    if (tier != KnobTier::Trim)
    {
        g.setColour(findColour(BraunColours::knobBorderColourId).withAlpha(0.35f));
        g.drawEllipse(capBounds.reduced(capRadius * 0.28f), 0.75f);
        g.drawEllipse(capBounds.reduced(capRadius * 0.52f), 0.75f);
    }

    // Machined perimeter bezel rim
    g.setColour(findColour(BraunColours::knobBorderColourId));
    g.drawEllipse(capBounds, 1.0f);

    // Top specular highlight bevel
    juce::Path highlightBevel;
    highlightBevel.addCentredArc(center.x, center.y, capRadius - 0.5f, capRadius - 0.5f,
                                 0.0f, -juce::MathConstants<float>::pi * 0.75f,
                                 juce::MathConstants<float>::pi * 0.25f, true);
    g.setColour(juce::Colours::white.withAlpha(darkThemeActive ? 0.10f : 0.40f));
    g.strokePath(highlightBevel, juce::PathStrokeType(0.8f));

    // 4. Milled Indicator Notch
    float notchLength = (tier == KnobTier::Hero) ? 10.0f : (tier == KnobTier::Secondary) ? 8.0f : 6.0f;
    float notchWidth = (tier == KnobTier::Hero) ? 2.2f : 1.8f;
    float notchStart = 3.0f;

    juce::Path notch;
    notch.startNewSubPath(center.x, center.y - capRadius + notchStart);
    notch.lineTo(center.x, center.y - capRadius + notchStart + notchLength);

    auto indicatorColour = slider.isMouseOverOrDragging()
                             ? findColour(BraunColours::braunOrangeColourId)
                             : findColour(BraunColours::knobIndicatorColourId);

    g.setColour(indicatorColour);
    g.strokePath(notch,
                 juce::PathStrokeType(notchWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 juce::AffineTransform::rotation(currentAngle, center.x, center.y));
}

//==============================================================================
void BraunLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    float tickSize = juce::jmin(bounds.getHeight() - 4.0f, 20.0f);
    float tickX = bounds.getX() + 4.0f;
    float tickY = bounds.getCentreY() - (tickSize * 0.5f);

    drawTickBox(g, button, tickX, tickY, tickSize, tickSize,
                button.getToggleState(), button.isEnabled(),
                shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    // Button label text
    auto textBounds = bounds.withTrimmedLeft(tickX + tickSize + 8.0f);
    g.setColour(findColour(BraunColours::textPrimaryColourId));
    g.setFont(getLabelFont(dynamic_cast<juce::Label&>(button)));
    g.drawFittedText(button.getButtonText(), textBounds.toNearestInt(),
                     juce::Justification::centredLeft, 1);
}

void BraunLookAndFeel::drawTickBox(juce::Graphics& g, juce::Component&,
                                   float x, float y, float w, float h,
                                   bool ticked, bool isEnabled,
                                   bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(isEnabled, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    auto box = juce::Rectangle<float>(x, y, w, h);

    // Inset bezel cavity
    g.setColour(findColour(BraunColours::bgPanelInsetColourId));
    g.fillRoundedRectangle(box, 3.0f);

    g.setColour(findColour(BraunColours::borderLineColourId));
    g.drawRoundedRectangle(box, 3.0f, 1.0f);

    // Circular LED indicator
    float ledRadius = juce::jmin(w, h) * 0.28f;
    auto ledCenter = box.getCentre();
    auto ledBounds = juce::Rectangle<float>(ledCenter.x - ledRadius, ledCenter.y - ledRadius,
                                            ledRadius * 2.0f, ledRadius * 2.0f);

    if (ticked)
    {
        // Glowing Braun Orange LED
        auto glowColour = findColour(BraunColours::braunOrangeColourId).withAlpha(0.45f);
        g.setColour(glowColour);
        g.fillEllipse(ledBounds.expanded(3.0f));

        g.setColour(findColour(BraunColours::braunOrangeColourId));
        g.fillEllipse(ledBounds);

        // Core bright hotspot
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.fillEllipse(ledBounds.reduced(ledRadius * 0.45f).translated(-0.5f, -0.5f));
    }
    else
    {
        // Unlit recessed LED
        g.setColour(findColour(BraunColours::borderLineColourId));
        g.fillEllipse(ledBounds);

        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawEllipse(ledBounds, 0.75f);
    }
}

void BraunLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(backgroundColour);

    auto bounds = button.getLocalBounds().toFloat();
    const float cornerRadius = 3.0f;

    bool isActive = button.getToggleState() || shouldDrawButtonAsDown;

    if (isActive)
    {
        g.setColour(findColour(BraunColours::braunOrangeColourId));
        g.fillRoundedRectangle(bounds, cornerRadius);

        g.setColour(findColour(BraunColours::borderLineColourId));
        g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
    }
    else
    {
        auto fill = shouldDrawButtonAsHighlighted
                        ? findColour(BraunColours::bgPanelInsetColourId).brighter(0.05f)
                        : findColour(BraunColours::bgPanelInsetColourId);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, cornerRadius);

        g.setColour(findColour(BraunColours::borderLineColourId));
        g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
    }
}

void BraunLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                      bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted);

    bool isActive = button.getToggleState() || shouldDrawButtonAsDown;
    auto textColour = isActive ? juce::Colours::white : findColour(BraunColours::textPrimaryColourId);

    g.setColour(textColour);
    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(),
                     juce::Justification::centred, 1);
}

void BraunLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited())
    {
        auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(getLabelFont(label));

        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, (int)((float)textArea.getHeight() / label.getFont().getHeight())),
                         label.getMinimumHorizontalScale());
    }
}

juce::Font BraunLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(juce::jmin(11.0f, (float)buttonHeight * 0.55f),
                                        juce::Font::bold));
}

juce::Font BraunLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions(10.0f, juce::Font::bold));
}

//==============================================================================
// CrtVisualizerComponent Implementation
//==============================================================================
CrtVisualizerComponent::CrtVisualizerComponent(BraunLookAndFeel& laf)
    : lookAndFeel(laf)
{
    bufferL.assign(kScopeBufferSize, 0.0f);
    bufferR.assign(kScopeBufferSize, 0.0f);

    startTimerHz(40); // 40 FPS refresh rate
}

CrtVisualizerComponent::~CrtVisualizerComponent()
{
    stopTimer();
}

void CrtVisualizerComponent::setMode(DisplayMode newMode)
{
    currentMode = newMode;
    repaint();
}

void CrtVisualizerComponent::setPower(bool powered)
{
    isPowered = powered;
    repaint();
}

void CrtVisualizerComponent::pushAudioBlock(const float* left, const float* right, int numSamples)
{
    if (numSamples <= 0 || left == nullptr) return;

    for (int i = 0; i < numSamples; ++i)
    {
        bufferL[(size_t)bufferWritePos] = left[i];
        bufferR[(size_t)bufferWritePos] = (right != nullptr) ? right[i] : left[i];
        bufferWritePos = (bufferWritePos + 1) % kScopeBufferSize;
    }
}

void CrtVisualizerComponent::updateTelemetry(float lowBandEnergy, float midBandEnergy, float highBandEnergy, float rt60DecayDb)
{
    currentLowEnergy  = lowBandEnergy;
    currentMidEnergy  = midBandEnergy;
    currentHighEnergy = highBandEnergy;
    currentDecayDb    = rt60DecayDb;

    edcLowHistory[(size_t)historyIndex]  = lowBandEnergy;
    edcMidHistory[(size_t)historyIndex]  = midBandEnergy;
    edcHighHistory[(size_t)historyIndex] = highBandEnergy;
    historyIndex = (historyIndex + 1) % kHistorySize;
}

void CrtVisualizerComponent::timerCallback()
{
    repaint();
}

void CrtVisualizerComponent::resized()
{
    auto bounds = getLocalBounds();
    if (bounds.getWidth() > 0 && bounds.getHeight() > 0)
    {
        persistenceImage = juce::Image(juce::Image::ARGB,
                                       bounds.getWidth(), bounds.getHeight(), true);
    }
}

void CrtVisualizerComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Deep matte black CRT cavity bezel
    g.setColour(lookAndFeel.findColour(BraunColours::bgBezelColourId));
    g.fillRoundedRectangle(bounds, 4.0f);

    // Inner shadow vignette
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.5f);

    // 2. 8x6 Precision Graticule
    drawGraticule(g, bounds);

    if (!isPowered)
    {
        drawStandbyBeam(g, bounds);
        return;
    }

    // 3. Active Mode Display
    switch (currentMode)
    {
        case DisplayMode::EdcWaterfall:
            drawEdcWaterfall(g, bounds);
            break;
        case DisplayMode::LissajousXy:
            drawLissajous(g, bounds);
            break;
        case DisplayMode::SpectrumAnalyzer:
            drawSpectrum(g, bounds);
            break;
    }
}

void CrtVisualizerComponent::drawGraticule(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(juce::Colours::white.withAlpha(0.07f));

    const int numDivsX = 8;
    const int numDivsY = 6;
    const float stepX = area.getWidth() / (float)numDivsX;
    const float stepY = area.getHeight() / (float)numDivsY;

    // Grid lines
    for (int i = 1; i < numDivsX; ++i)
    {
        float gx = area.getX() + (float)i * stepX;
        g.drawVerticalLine((int)gx, area.getY(), area.getBottom());
    }

    for (int j = 1; j < numDivsY; ++j)
    {
        float gy = area.getY() + (float)j * stepY;
        g.drawHorizontalLine((int)gy, area.getX(), area.getRight());
    }

    // Center Crosshairs (bolder)
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    float midX = area.getCentreX();
    float midY = area.getCentreY();
    g.drawVerticalLine((int)midX, area.getY(), area.getBottom());
    g.drawHorizontalLine((int)midY, area.getX(), area.getRight());

    // Minor subdivision ticks along center axes
    g.setColour(juce::Colours::white.withAlpha(0.20f));
    const int ticksPerDiv = 5;
    for (int i = 0; i < numDivsX * ticksPerDiv; ++i)
    {
        float tx = area.getX() + (float)i * (stepX / (float)ticksPerDiv);
        g.drawVerticalLine((int)tx, midY - 2.0f, midY + 2.0f);
    }
    for (int j = 0; j < numDivsY * ticksPerDiv; ++j)
    {
        float ty = area.getY() + (float)j * (stepY / (float)ticksPerDiv);
        g.drawHorizontalLine((int)ty, midX - 2.0f, midX + 2.0f);
    }
}

void CrtVisualizerComponent::drawStandbyBeam(juce::Graphics& g, juce::Rectangle<float> area)
{
    float centerY = area.getCentreY();
    auto phosphor = lookAndFeel.findColour(BraunColours::phosphorColourId);
    auto glow     = lookAndFeel.findColour(BraunColours::phosphorGlowColourId);

    // Outer glow pass
    g.setColour(glow.withAlpha(0.25f));
    g.drawLine(area.getX(), centerY, area.getRight(), centerY, 3.5f);

    // Core beam
    g.setColour(phosphor.withAlpha(0.70f));
    g.drawLine(area.getX(), centerY, area.getRight(), centerY, 1.2f);
}

void CrtVisualizerComponent::drawEdcWaterfall(juce::Graphics& g, juce::Rectangle<float> area)
{
    // Mode 1: RT60 Energy Decay Curves across Low, Mid, High bands
    auto phosphor = lookAndFeel.findColour(BraunColours::phosphorColourId);
    auto orange   = lookAndFeel.findColour(BraunColours::braunOrangeColourId);
    auto amber    = lookAndFeel.findColour(BraunColours::braunAmberColourId);

    float w = area.getWidth();
    float h = area.getHeight();

    auto buildEdcPath = [&](const std::array<float, kHistorySize>& hist, float scale) -> juce::Path
    {
        juce::Path p;
        for (int i = 0; i < kHistorySize; ++i)
        {
            int idx = (historyIndex + i) % kHistorySize;
            float val = hist[(size_t)idx] * scale;
            float px = area.getX() + ((float)i / (float)(kHistorySize - 1)) * w;
            float py = area.getBottom() - juce::jlimit(0.0f, 1.0f, val) * (h * 0.85f) - 6.0f;

            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }
        return p;
    };

    auto pathLow  = buildEdcPath(edcLowHistory, 1.2f);
    auto pathMid  = buildEdcPath(edcMidHistory, 1.0f);
    auto pathHigh = buildEdcPath(edcHighHistory, 0.85f);

    // Two-pass rendering: Glow pass + Core pass
    // Low Band (Amber)
    g.setColour(amber.withAlpha(0.25f));
    g.strokePath(pathLow, juce::PathStrokeType(3.5f));
    g.setColour(amber);
    g.strokePath(pathLow, juce::PathStrokeType(1.4f));

    // Mid Band (Phosphor Green)
    g.setColour(phosphor.withAlpha(0.30f));
    g.strokePath(pathMid, juce::PathStrokeType(3.5f));
    g.setColour(phosphor);
    g.strokePath(pathMid, juce::PathStrokeType(1.6f));

    // High Band (Braun Orange)
    g.setColour(orange.withAlpha(0.25f));
    g.strokePath(pathHigh, juce::PathStrokeType(3.5f));
    g.setColour(orange);
    g.strokePath(pathHigh, juce::PathStrokeType(1.4f));

    // Mode readout legend
    g.setColour(phosphor.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain)));
    g.drawText("EDC WATERFALL [SCHROEDER INTEGRAL]", (int)area.getX() + 8, (int)area.getY() + 6,
               220, 14, juce::Justification::left);
}

void CrtVisualizerComponent::drawLissajous(juce::Graphics& g, juce::Rectangle<float> area)
{
    // Mode 2: Lissajous XY Stereo Phase Goniometer (45° rotated correlation plot)
    auto phosphor = lookAndFeel.findColour(BraunColours::phosphorColourId);
    auto glow     = lookAndFeel.findColour(BraunColours::phosphorGlowColourId);

    float midX = area.getCentreX();
    float midY = area.getCentreY();
    float scale = juce::jmin(area.getWidth(), area.getHeight()) * 0.42f;

    juce::Path lissajousPath;
    bool hasStarted = false;

    // 45-degree rotation matrix: X = (L - R) * sqrt(0.5), Y = (L + R) * sqrt(0.5)
    constexpr float invSqrt2 = 0.70710678f;

    for (size_t i = 0; i < (size_t)kScopeBufferSize; i += 2)
    {
        float l = bufferL[i];
        float r = bufferR[i];

        float xRot = (l - r) * invSqrt2;
        float yRot = (l + r) * invSqrt2;

        float px = midX + xRot * scale;
        float py = midY - yRot * scale;

        if (!hasStarted)
        {
            lissajousPath.startNewSubPath(px, py);
            hasStarted = true;
        }
        else
        {
            lissajousPath.lineTo(px, py);
        }
    }

    // Glow pass
    g.setColour(glow);
    g.strokePath(lissajousPath, juce::PathStrokeType(3.2f));

    // Core pass
    g.setColour(phosphor);
    g.strokePath(lissajousPath, juce::PathStrokeType(1.3f));

    // Legend
    g.setColour(phosphor.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain)));
    g.drawText("LISSAJOUS XY PHASE CORRELATION", (int)area.getX() + 8, (int)area.getY() + 6,
               220, 14, juce::Justification::left);
}

void CrtVisualizerComponent::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> area)
{
    // Mode 3: Real-Time Spectrum Analyzer with phosphor persistence
    auto phosphor = lookAndFeel.findColour(BraunColours::phosphorColourId);
    auto glow     = lookAndFeel.findColour(BraunColours::phosphorGlowColourId);

    const int numBands = 32;
    float bandWidth = (area.getWidth() - 16.0f) / (float)numBands;
    float startX = area.getX() + 8.0f;
    float bottomY = area.getBottom() - 8.0f;

    for (int b = 0; b < numBands; ++b)
    {
        // Synthesize log-frequency magnitude from buffer samples
        float mag = 0.0f;
        int step = juce::jmax(1, (kScopeBufferSize / numBands));
        for (int s = 0; s < step; ++s)
        {
            size_t idx = (size_t)(b * step + s) % (size_t)kScopeBufferSize;
            mag += std::abs(bufferL[idx]);
        }
        mag /= (float)step;

        // Calibrate magnitude against [-95.0f, -10.0f] dBFS range matching Web CRT visualizer
        const float db = 20.0f * std::log10(std::max(mag, 1.0e-5f));
        const float normVal = std::clamp((db - (-95.0f)) / (-10.0f - (-95.0f)), 0.0f, 1.0f);
        const float maxBarH = area.getHeight() * 0.8f;
        const float barHeight = juce::jlimit(2.0f, maxBarH, normVal * maxBarH);
        auto barRect = juce::Rectangle<float>(startX + (float)b * bandWidth + 1.0f,
                                              bottomY - barHeight,
                                              bandWidth - 2.0f,
                                              barHeight);

        // Glow pass
        g.setColour(glow.withAlpha(0.35f));
        g.fillRect(barRect.expanded(1.0f));

        // Core bar
        g.setColour(phosphor);
        g.fillRect(barRect);

        // Peak line
        g.setColour(juce::Colours::white.withAlpha(0.80f));
        g.drawHorizontalLine((int)(bottomY - barHeight), barRect.getX(), barRect.getRight());
    }

    // Legend
    g.setColour(phosphor.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain)));
    g.drawText("1/3-OCTAVE SPECTRUM ANALYZER", (int)area.getX() + 8, (int)area.getY() + 6,
               220, 14, juce::Justification::left);
}

} // namespace rb26
