#pragma once
#include "PVProcessor.h"

// FIX (requested): recoloured to the shared PQ/PD palette instead of ad-hoc RGB values, for a
// consistent "family" look across all three plugins, and given the premium button-depth treatment
// PQ/PD already have.
class PVLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PVLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class PVAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PVAudioProcessorEditor(PVAudioProcessor&);
    ~PVAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PVAudioProcessor& processor;
    PVLookAndFeel pvLaf;
    juce::ToggleButton bypass;
    // FIX (requested): Magic moved from a top on/off switch to a knob (with Tape and Chorus) that
    // controls the volume of just the two pitched/panned voices - 0 is silent (identical to bypass),
    // up to a ceiling of unity level with the centre line.
    juce::Slider magic, tape, chorus;
    juce::Label title, subtitle, magicLabel, tapeLabel, chorusLabel, magicHint;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> magicA, tapeA, chorusA;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PVAudioProcessorEditor)
};
