#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel/BraunLookAndFeel.h"
#include <iostream>
#include <typeinfo>
#include <cstdlib>

#define RB26_LAF_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAILED: " << (msg) << " (" #cond ") at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return 1; \
    } \
} while (false)

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "[RB-26 LookAndFeel Test] Initializing BraunLookAndFeel...\n";
    rb26::BraunLookAndFeel laf;

    // 1. Verify Light Palette
    RB26_LAF_ASSERT(laf.findColour(rb26::BraunColours::bgAppColourId).getARGB() == rb26::BraunColours::Light_BgApp, "Light bgAppColourId mismatch");
    RB26_LAF_ASSERT(laf.findColour(rb26::BraunColours::textPrimaryColourId).getARGB() == rb26::BraunColours::Light_TextPrimary, "Light textPrimaryColourId mismatch");
    RB26_LAF_ASSERT(laf.findColour(juce::ComboBox::textColourId).getARGB() == rb26::BraunColours::Light_TextPrimary, "Light ComboBox textColourId mismatch");
    RB26_LAF_ASSERT(laf.findColour(juce::ComboBox::backgroundColourId).getARGB() == rb26::BraunColours::Light_BgPanelInset, "Light ComboBox backgroundColourId mismatch");

    // 2. Verify Dark Palette Toggle
    laf.setDarkTheme(true);
    RB26_LAF_ASSERT(laf.isDarkTheme(), "isDarkTheme mismatch");
    RB26_LAF_ASSERT(laf.findColour(rb26::BraunColours::bgAppColourId).getARGB() == rb26::BraunColours::Dark_BgApp, "Dark bgAppColourId mismatch");
    RB26_LAF_ASSERT(laf.findColour(rb26::BraunColours::textPrimaryColourId).getARGB() == rb26::BraunColours::Dark_TextPrimary, "Dark textPrimaryColourId mismatch");
    RB26_LAF_ASSERT(laf.findColour(rb26::BraunColours::knobFillColourId).getARGB() == rb26::BraunColours::Dark_KnobFill, "Dark knobFillColourId mismatch");
    RB26_LAF_ASSERT(laf.findColour(juce::ComboBox::textColourId).getARGB() == rb26::BraunColours::Dark_TextPrimary, "Dark ComboBox textColourId mismatch");
    RB26_LAF_ASSERT(laf.findColour(juce::ComboBox::backgroundColourId).getARGB() == rb26::BraunColours::Dark_BgPanelInset, "Dark ComboBox backgroundColourId mismatch");
    RB26_LAF_ASSERT(laf.findColour(juce::PopupMenu::backgroundColourId).getARGB() == rb26::BraunColours::Dark_BgPanel, "Dark PopupMenu backgroundColourId mismatch");

    // 2b. Verify ComboBox attached to BraunLookAndFeel renders with Dark_TextPrimary (0xFFF0F0F0) and Light_TextPrimary
    {
        juce::ComboBox testBox;
        testBox.setLookAndFeel(&laf);

        laf.setDarkTheme(false);
        testBox.sendLookAndFeelChange();
        RB26_LAF_ASSERT(testBox.findColour(juce::ComboBox::textColourId).getARGB() == rb26::BraunColours::Light_TextPrimary,
                        "juce::ComboBox attached to BraunLookAndFeel must render with Light_TextPrimary in light theme");

        laf.setDarkTheme(true);
        testBox.sendLookAndFeelChange();
        RB26_LAF_ASSERT(testBox.findColour(juce::ComboBox::textColourId).getARGB() == rb26::BraunColours::Dark_TextPrimary,
                        "juce::ComboBox attached to BraunLookAndFeel must render with Dark_TextPrimary in dark theme");
        RB26_LAF_ASSERT(testBox.findColour(juce::ComboBox::textColourId).getARGB() == 0xFFF0F0F0,
                        "juce::ComboBox in dark theme must strictly equal 0xFFF0F0F0");

        testBox.setLookAndFeel(nullptr);
    }

    // 3. Verify 3-Tier Knob Sizing
    RB26_LAF_ASSERT(rb26::BraunLookAndFeel::getKnobTierForBounds(64, 64) == rb26::BraunLookAndFeel::KnobTier::Hero, "Hero knob tier mismatch");
    RB26_LAF_ASSERT(rb26::BraunLookAndFeel::getKnobTierForBounds(52, 52) == rb26::BraunLookAndFeel::KnobTier::Secondary, "Secondary knob tier mismatch");
    RB26_LAF_ASSERT(rb26::BraunLookAndFeel::getKnobTierForBounds(42, 42) == rb26::BraunLookAndFeel::KnobTier::Trim, "Trim knob tier mismatch");

    // 4. Verify CrtVisualizerComponent & Mode Switching
    std::cout << "[RB-26 LookAndFeel Test] Initializing CrtVisualizerComponent...\n";
    rb26::CrtVisualizerComponent crt(laf);
    crt.setBounds(0, 0, 600, 200);

    crt.setMode(rb26::CrtVisualizerComponent::DisplayMode::EdcWaterfall);
    RB26_LAF_ASSERT(crt.getMode() == rb26::CrtVisualizerComponent::DisplayMode::EdcWaterfall, "EdcWaterfall mode mismatch");

    crt.setMode(rb26::CrtVisualizerComponent::DisplayMode::LissajousXy);
    RB26_LAF_ASSERT(crt.getMode() == rb26::CrtVisualizerComponent::DisplayMode::LissajousXy, "LissajousXy mode mismatch");

    crt.setMode(rb26::CrtVisualizerComponent::DisplayMode::SpectrumAnalyzer);
    RB26_LAF_ASSERT(crt.getMode() == rb26::CrtVisualizerComponent::DisplayMode::SpectrumAnalyzer, "SpectrumAnalyzer mode mismatch");

    // 5. Verify Telemetry & Audio Buffer Push
    float dummyL[128] = { 0.05f };
    float dummyR[128] = { 0.05f };
    crt.pushAudioBlock(dummyL, dummyR, 128);
    crt.updateTelemetry(0.4f, 0.3f, 0.2f, -18.0f);

    // 6. Verify Off-screen Rendering of UI Widgets (No std::bad_cast or exceptions)
    std::cout << "[RB-26 LookAndFeel Test] Rendering UI widgets off-screen to verify no std::bad_cast...\n";
    {
        juce::Image testImage(juce::Image::ARGB, 400, 400, true);
        juce::Graphics g(testImage);

        // Test across both light and dark themes
        for (bool darkTheme : { false, true })
        {
            laf.setDarkTheme(darkTheme);

            // 6.1 juce::ToggleButton (regressed in Issue #1 with dynamic_cast<juce::Label&>)
            {
                juce::ToggleButton toggle("Bypass");
                toggle.setLookAndFeel(&laf);
                toggle.setBounds(0, 0, 150, 30);

                // Draw unchecked state
                toggle.setToggleState(false, juce::dontSendNotification);
                try {
                    toggle.paintEntireComponent(g, true);
                } catch (const std::bad_cast& e) {
                    std::cerr << "FAILED: std::bad_cast caught during ToggleButton rendering (unchecked): " << e.what() << "\n";
                    return 1;
                } catch (const std::exception& e) {
                    std::cerr << "FAILED: Exception caught during ToggleButton rendering (unchecked): " << e.what() << "\n";
                    return 1;
                }

                // Draw checked state
                toggle.setToggleState(true, juce::dontSendNotification);
                try {
                    toggle.paintEntireComponent(g, true);
                } catch (const std::bad_cast& e) {
                    std::cerr << "FAILED: std::bad_cast caught during ToggleButton rendering (checked): " << e.what() << "\n";
                    return 1;
                } catch (const std::exception& e) {
                    std::cerr << "FAILED: Exception caught during ToggleButton rendering (checked): " << e.what() << "\n";
                    return 1;
                }

                // Direct LookAndFeel invocation
                try {
                    laf.drawToggleButton(g, toggle, false, false);
                    laf.drawToggleButton(g, toggle, true, true);
                } catch (const std::bad_cast& e) {
                    std::cerr << "FAILED: std::bad_cast caught during direct drawToggleButton: " << e.what() << "\n";
                    return 1;
                } catch (const std::exception& e) {
                    std::cerr << "FAILED: Exception caught during direct drawToggleButton: " << e.what() << "\n";
                    return 1;
                }

                toggle.setLookAndFeel(nullptr);
            }

            // 6.2 juce::TextButton
            {
                juce::TextButton textButton("Mode");
                textButton.setLookAndFeel(&laf);
                textButton.setBounds(0, 0, 100, 32);

                try {
                    textButton.paintEntireComponent(g, true);
                    textButton.setToggleState(true, juce::dontSendNotification);
                    textButton.paintEntireComponent(g, true);
                    laf.drawButtonBackground(g, textButton, juce::Colours::transparentBlack, false, false);
                    laf.drawButtonText(g, textButton, false, false);
                } catch (const std::bad_cast& e) {
                    std::cerr << "FAILED: std::bad_cast caught during TextButton rendering: " << e.what() << "\n";
                    return 1;
                } catch (const std::exception& e) {
                    std::cerr << "FAILED: Exception caught during TextButton rendering: " << e.what() << "\n";
                    return 1;
                }

                textButton.setLookAndFeel(nullptr);
            }

            // 6.3 juce::ComboBox
            {
                juce::ComboBox comboBox;
                comboBox.setLookAndFeel(&laf);
                comboBox.setBounds(0, 0, 160, 28);
                comboBox.addItem("Hall A", 1);
                comboBox.addItem("Room B", 2);
                comboBox.setSelectedId(1, juce::dontSendNotification);

                const auto expectedColour = darkTheme ? rb26::BraunColours::Dark_TextPrimary
                                                      : rb26::BraunColours::Light_TextPrimary;
                RB26_LAF_ASSERT(comboBox.findColour(juce::ComboBox::textColourId).getARGB() == expectedColour,
                                "juce::ComboBox textColourId must match active theme TextPrimary");

                try {
                    comboBox.paintEntireComponent(g, true);
                    laf.drawComboBox(g, 160, 28, false, 0, 0, 160, 28, comboBox);
                } catch (const std::bad_cast& e) {
                    std::cerr << "FAILED: std::bad_cast caught during ComboBox rendering: " << e.what() << "\n";
                    return 1;
                } catch (const std::exception& e) {
                    std::cerr << "FAILED: Exception caught during ComboBox rendering: " << e.what() << "\n";
                    return 1;
                }

                comboBox.setLookAndFeel(nullptr);
            }

            // 6.4 juce::Slider (Hero 64px, Secondary 52px, Trim 42px)
            {
                for (int size : { 64, 52, 42 })
                {
                    juce::Slider slider;
                    slider.setLookAndFeel(&laf);
                    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                    slider.setBounds(0, 0, size, size);
                    slider.setValue(0.75);

                    try {
                        slider.paintEntireComponent(g, true);
                        laf.drawRotarySlider(g, 0, 0, size, size, 0.75f, 0.0f, 6.28f, slider);
                    } catch (const std::bad_cast& e) {
                        std::cerr << "FAILED: std::bad_cast caught during Slider (" << size << "px) rendering: " << e.what() << "\n";
                        return 1;
                    } catch (const std::exception& e) {
                        std::cerr << "FAILED: Exception caught during Slider (" << size << "px) rendering: " << e.what() << "\n";
                        return 1;
                    }

                    slider.setLookAndFeel(nullptr);
                }
            }

            // 6.5 juce::Label (exercising drawLabel and getLabelFont)
            {
                juce::Label label("Label", "REVERBERATION");
                label.setLookAndFeel(&laf);
                label.setBounds(0, 0, 120, 24);

                try {
                    label.paintEntireComponent(g, true);
                    laf.drawLabel(g, label);
                } catch (const std::bad_cast& e) {
                    std::cerr << "FAILED: std::bad_cast caught during Label rendering: " << e.what() << "\n";
                    return 1;
                } catch (const std::exception& e) {
                    std::cerr << "FAILED: Exception caught during Label rendering: " << e.what() << "\n";
                    return 1;
                }

                label.setLookAndFeel(nullptr);
            }
        }
    }

    std::cout << "[RB-26 LookAndFeel Test] ALL NATIVE CHECKS PASSED!\n";
    return 0;
}
