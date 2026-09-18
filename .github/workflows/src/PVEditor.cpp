#include "PVEditor.h"

PVLookAndFeel::PVLookAndFeel()
{
    setColour(juce::Slider::thumbColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGB(29,31,36));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromRGB(55,58,66));
}

void PVLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                     float pos, float start, float end, juce::Slider&)
{
    const float cx = x + w * .5f, cy = y + h * .5f;
    const float radius = juce::jmin(w, h) * .37f;
    const float angle = start + pos * (end - start);

    g.setColour(juce::Colour::fromRGB(11,12,15));
    g.fillEllipse(cx-radius, cy-radius, radius*2, radius*2);
    g.setColour(juce::Colour::fromRGB(54,57,65));
    g.drawEllipse(cx-radius, cy-radius, radius*2, radius*2, 2.0f);

    juce::Path bg, fg;
    bg.addCentredArc(cx, cy, radius-5, radius-5, 0, start, end, true);
    fg.addCentredArc(cx, cy, radius-5, radius-5, 0, start, angle, true);
    g.setColour(juce::Colour::fromRGB(55,58,66));
    g.strokePath(bg, juce::PathStrokeType(4.0f));
    g.setColour(juce::Colour::fromRGB(218,221,226));
    g.strokePath(fg, juce::PathStrokeType(4.0f));

    const float tx = cx + std::cos(angle - juce::MathConstants<float>::halfPi) * (radius-10);
    const float ty = cy + std::sin(angle - juce::MathConstants<float>::halfPi) * (radius-10);
    g.setColour(juce::Colours::white);
    g.fillEllipse(tx-2.5f, ty-2.5f, 5, 5);
}

void PVLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced(1);
    bool on = b.getToggleState();
    auto fill = on ? juce::Colour::fromRGB(232,235,240) : juce::Colour::fromRGB(29,31,36);
    if (over && !on) fill = juce::Colour::fromRGB(38,41,47);
    g.setColour(fill); g.fillRoundedRectangle(r, 9);
    g.setColour(juce::Colour::fromRGB(67,70,78)); g.drawRoundedRectangle(r, 9, 1);
    g.setColour(on ? juce::Colour::fromRGB(14,15,18) : juce::Colour::fromRGB(190,194,201));
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

PVAudioProcessorEditor::PVAudioProcessorEditor(PVAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(620, 370);
    setResizable(false, false);
    setLookAndFeel(&pvLaf);

    title.setText("PV", juce::dontSendNotification);
    title.setFont(juce::Font(38, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    subtitle.setText("VOCAL MICRO-PITCH  /  PRESENCE", juce::dontSendNotification);
    subtitle.setFont(juce::Font(10.5f));
    subtitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(125,129,138));
    addAndMakeVisible(subtitle);

    bypass.setButtonText("BYPASS");
    magic.setButtonText("MAGIC");
    addAndMakeVisible(bypass); addAndMakeVisible(magic);

    magicHint.setText("CENTER  •  −10c L  •  +10c R", juce::dontSendNotification);
    magicHint.setFont(juce::Font(10));
    magicHint.setColour(juce::Label::textColourId, juce::Colour::fromRGB(125,129,138));
    magicHint.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(magicHint);

    auto setup = [](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 21);
        s.setRange(0.0, 0.20, 0.001);
        s.setNumDecimalPlacesToDisplay(0);
        s.setTextValueSuffix("%");
    };
    setup(tape); setup(chorus);
    tape.setValue(.08); chorus.setValue(.06);
    addAndMakeVisible(tape); addAndMakeVisible(chorus);

    tapeLabel.setText("TAPE", juce::dontSendNotification);
    chorusLabel.setText("CHORUS", juce::dontSendNotification);
    for (auto* l : { &tapeLabel, &chorusLabel })
    {
        l->setFont(juce::Font(11, juce::Font::bold));
        l->setColour(juce::Label::textColourId, juce::Colour::fromRGB(205,208,214));
        l->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    }

    bypassA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "bypass", bypass);
    magicA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "magic", magic);
    tapeA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "tape", tape);
    chorusA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "chorus", chorus);
}

PVAudioProcessorEditor::~PVAudioProcessorEditor() { setLookAndFeel(nullptr); }

void PVAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(9,10,12));
    auto p = getLocalBounds().reduced(16).toFloat();
    g.setColour(juce::Colour::fromRGB(19,20,24)); g.fillRoundedRectangle(p, 18);
    g.setColour(juce::Colour::fromRGB(49,52,60)); g.drawRoundedRectangle(p, 18, 1);
    g.setColour(juce::Colour::fromRGB(39,41,48));
    g.drawLine(38,91,582,91,1);
    g.drawLine(310,120,310,322,1);

    g.setColour(juce::Colour::fromRGB(27,29,34));
    for (int i=0;i<18;++i) g.drawLine(52.0f,112.0f+i*4.0f,568.0f,112.0f+i*4.0f,.35f);

    g.setColour(juce::Colour::fromRGB(112,116,126));
    g.setFont(juce::Font(9, juce::Font::bold));
    g.drawText("ANALOG-STYLE WIDTH",48,104,150,14,juce::Justification::left);
    g.drawText("SPREAD",395,104,170,14,juce::Justification::right);
}

void PVAudioProcessorEditor::resized()
{
    title.setBounds(40,27,65,44);
    subtitle.setBounds(105,42,260,20);
    bypass.setBounds(470,30,62,30);
    magic.setBounds(538,30,62,30);
    magicHint.setBounds(385,101,175,18);
    tape.setBounds(105,143,145,145);
    chorus.setBounds(360,143,145,145);
    tapeLabel.setBounds(105,292,145,20);
    chorusLabel.setBounds(360,292,145,20);
}
