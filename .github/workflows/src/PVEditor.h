#pragma once
#include "PVProcessor.h"

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
    juce::ToggleButton bypass, magic;
    juce::Slider tape, chorus;
    juce::Label title, subtitle, tapeLabel, chorusLabel, magicHint;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassA, magicA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tapeA, chorusA;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PVAudioProcessorEditor)
};
