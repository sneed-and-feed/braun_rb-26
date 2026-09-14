#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel/BraunLookAndFeel.h"
#include <cassert>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "[RB-26 LookAndFeel Test] Initializing BraunLookAndFeel...\n";
    rb26::BraunLookAndFeel laf;

    // 1. Verify Light Palette
    assert(laf.findColour(rb26::BraunColours::bgAppColourId).getARGB() == rb26::BraunColours::Light_BgApp);
    assert(laf.findColour(rb26::BraunColours::textPrimaryColourId).getARGB() == rb26::BraunColours::Light_TextPrimary);

    // 2. Verify Dark Palette Toggle
    laf.setDarkTheme(true);
    assert(laf.isDarkTheme());
    assert(laf.findColour(rb26::BraunColours::bgAppColourId).getARGB() == rb26::BraunColours::Dark_BgApp);
    assert(laf.findColour(rb26::BraunColours::textPrimaryColourId).getARGB() == rb26::BraunColours::Dark_TextPrimary);
    assert(laf.findColour(rb26::BraunColours::knobFillColourId).getARGB() == rb26::BraunColours::Dark_KnobFill);

    // 3. Verify 3-Tier Knob Sizing
    assert(rb26::BraunLookAndFeel::getKnobTierForBounds(64, 64) == rb26::BraunLookAndFeel::KnobTier::Hero);
    assert(rb26::BraunLookAndFeel::getKnobTierForBounds(52, 52) == rb26::BraunLookAndFeel::KnobTier::Secondary);
    assert(rb26::BraunLookAndFeel::getKnobTierForBounds(42, 42) == rb26::BraunLookAndFeel::KnobTier::Trim);

    // 4. Verify CrtVisualizerComponent & Mode Switching
    std::cout << "[RB-26 LookAndFeel Test] Initializing CrtVisualizerComponent...\n";
    rb26::CrtVisualizerComponent crt(laf);
    crt.setBounds(0, 0, 600, 200);

    crt.setMode(rb26::CrtVisualizerComponent::DisplayMode::EdcWaterfall);
    assert(crt.getMode() == rb26::CrtVisualizerComponent::DisplayMode::EdcWaterfall);

    crt.setMode(rb26::CrtVisualizerComponent::DisplayMode::LissajousXy);
    assert(crt.getMode() == rb26::CrtVisualizerComponent::DisplayMode::LissajousXy);

    crt.setMode(rb26::CrtVisualizerComponent::DisplayMode::SpectrumAnalyzer);
    assert(crt.getMode() == rb26::CrtVisualizerComponent::DisplayMode::SpectrumAnalyzer);

    // 5. Verify Telemetry & Audio Buffer Push
    float dummyL[128] = { 0.05f };
    float dummyR[128] = { 0.05f };
    crt.pushAudioBlock(dummyL, dummyR, 128);
    crt.updateTelemetry(0.4f, 0.3f, 0.2f, -18.0f);

    std::cout << "[RB-26 LookAndFeel Test] ALL NATIVE CHECKS PASSED!\n";
    return 0;
}
