#include "PVEditor.h"

// ---- Shared PQ/PD colour palette (requested - move PV to the same family look) ----
namespace {
    juce::Colour bg(){ return juce::Colour(0xff0d1117); }
    juce::Colour panel(){ return juce::Colour(0xff161e2b); }
    juce::Colour raised(){ return juce::Colour(0xff1d2633); }
    juce::Colour recess(){ return juce::Colour(0xff080b0f); }
    juce::Colour border(){ return juce::Colour(0xff313942); }
    juce::Colour track(){ return juce::Colour(0xff252d36); }
    juce::Colour text(){ return juce::Colour(0xffeef0f2); }
    juce::Colour secondaryText(){ return juce::Colour(0xff9da2a8); }
    juce::Colour muted(){ return juce::Colour(0xff6f7a86); }
    juce::Colour accent(){ return juce::Colour(0xff296095); }
    juce::Colour accentHighlight(){ return juce::Colour(0xff69a1d0); }
}

PVLookAndFeel::PVLookAndFeel()
{
    setColour(juce::Slider::thumbColourId, text());
    setColour(juce::Slider::textBoxTextColourId, text());
    setColour(juce::Slider::textBoxBackgroundColourId, recess());
    setColour(juce::Slider::textBoxOutlineColourId, border());
}

void PVLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                     float pos, float start, float end, juce::Slider&)
{
    const float cx = x + w * .5f, cy = y + h * .5f;
    const float radius = juce::jmin(w, h) * .37f;
    const float angle = start + pos * (end - start);

    // Same premium depth language as PQ/PD: dark recess, subtle border, coloured progress arc.
    g.setColour(recess());
    g.fillEllipse(cx-radius, cy-radius, radius*2, radius*2);
    g.setColour(border());
    g.drawEllipse(cx-radius, cy-radius, radius*2, radius*2, 2.0f);

    juce::Path bgArc, fgArc;
    bgArc.addCentredArc(cx, cy, radius-5, radius-5, 0, start, end, true);
    fgArc.addCentredArc(cx, cy, radius-5, radius-5, 0, start, angle, true);
    g.setColour(track());
    g.strokePath(bgArc, juce::PathStrokeType(4.0f));
    g.setColour(accentHighlight());
    g.strokePath(fgArc, juce::PathStrokeType(4.0f));

    const float tx = cx + std::cos(angle - juce::MathConstants<float>::halfPi) * (radius-10);
    const float ty = cy + std::sin(angle - juce::MathConstants<float>::halfPi) * (radius-10);
    g.setColour(text());
    g.fillEllipse(tx-2.5f, ty-2.5f, 5, 5);
}

void PVLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    // Same premium button-depth treatment as PQ/PD's LookAndFeel: dark recess, raised face, subtle
    // top highlight, accent border/text when active.
    auto bounds = b.getLocalBounds().toFloat();
    bool on = b.getToggleState();
    g.setColour(recess()); g.fillRoundedRectangle(bounds, 9);
    auto face = bounds.reduced(0.5f).withTrimmedBottom(2.0f);
    juce::Colour faceColour = on ? raised() : panel();
    if(over && !on) faceColour = faceColour.interpolatedWith(border(), 0.3f);
    g.setColour(faceColour); g.fillRoundedRectangle(face, 9);
    juce::Path topHighlight;
    topHighlight.addRoundedRectangle(face.getX(), face.getY(), face.getWidth(), face.getHeight()*0.45f, 9.f, 9.f, true, true, false, false);
    g.setColour(border().withAlpha(on?0.3f:0.18f)); g.fillPath(topHighlight);
    g.setColour(on ? accent() : border()); g.drawRoundedRectangle(face, 9, on?1.4f:1.0f);
    g.setColour(on ? accentHighlight() : muted());
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

PVAudioProcessorEditor::PVAudioProcessorEditor(PVAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(680, 380);
    setResizable(false, false);
    setLookAndFeel(&pvLaf);

    title.setText("PV", juce::dontSendNotification);
    title.setFont(juce::Font(38, juce::Font::bold));
    title.setColour(juce::Label::textColourId, text());
    addAndMakeVisible(title);

    // FIX (requested): creator credit added, matching PQ/PD's convention.
    subtitle.setText("VOCAL MICRO-PITCH / PRESENCE   /   POURIA MOTABEAN", juce::dontSendNotification);
    subtitle.setFont(juce::Font(10.5f));
    subtitle.setColour(juce::Label::textColourId, muted());
    addAndMakeVisible(subtitle);

    bypass.setButtonText("BYPASS");
    addAndMakeVisible(bypass);

    magicHint.setText("CENTER  \u2022  \u221210c L  \u2022  +10c R", juce::dontSendNotification);
    magicHint.setFont(juce::Font(10));
    magicHint.setColour(juce::Label::textColourId, muted());
    magicHint.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(magicHint);

    auto setupPercent = [](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 21);
        s.setRange(0.0, 1.0, 0.001);
        s.setNumDecimalPlacesToDisplay(0);
        s.setTextValueSuffix("%");
    };
    auto setupTapeChorus = [](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 21);
        s.setRange(0.0, 0.20, 0.001);
        s.setNumDecimalPlacesToDisplay(0);
        s.setTextValueSuffix("%");
    };
    setupPercent(magic); setupTapeChorus(tape); setupTapeChorus(chorus);
    addAndMakeVisible(magic); addAndMakeVisible(tape); addAndMakeVisible(chorus);

    magicLabel.setText("MAGIC", juce::dontSendNotification);
    tapeLabel.setText("TAPE", juce::dontSendNotification);
    chorusLabel.setText("CHORUS", juce::dontSendNotification);
    for (auto* l : { &magicLabel, &tapeLabel, &chorusLabel })
    {
        l->setFont(juce::Font(11, juce::Font::bold));
        l->setColour(juce::Label::textColourId, secondaryText());
        l->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    }

    bypassA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "bypass", bypass);
    // FIX (requested): Magic is now a knob attached like Tape/Chorus, defaulting to 0 (the parameter
    // itself defaults to 0 - see PVProcessor.cpp - so the attachment picks that up automatically,
    // no manual setValue() needed here, which is also what avoids any startup click).
    magicA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "magic", magic);
    tapeA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "tape", tape);
    chorusA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "chorus", chorus);
}

PVAudioProcessorEditor::~PVAudioProcessorEditor() { setLookAndFeel(nullptr); }

void PVAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg());
    auto p = getLocalBounds().reduced(16).toFloat();
    // Premium panel depth, same 4-layer language as PQ/PD.
    g.setColour(recess()); g.fillRoundedRectangle(p.expanded(1.f), 18);
    g.setColour(panel()); g.fillRoundedRectangle(p, 18);
    juce::Path topHighlight;
    topHighlight.addRoundedRectangle(p.getX(),p.getY(),p.getWidth(),p.getHeight()*0.35f,18.f,18.f,true,true,false,false);
    g.setColour(raised().withAlpha(0.15f)); g.fillPath(topHighlight);
    g.setColour(border()); g.drawRoundedRectangle(p, 18, 1);

    g.setColour(border().withAlpha(0.5f));
    g.drawLine(38,91,642,91,1);
    g.drawLine(340,120,340,332,1);

    g.setColour(border().withAlpha(0.35f));
    for (int i=0;i<20;++i) g.drawLine(52.0f,112.0f+i*4.0f,628.0f,112.0f+i*4.0f,.35f);

    g.setColour(muted());
    g.setFont(juce::Font(9, juce::Font::bold));
    g.drawText("ANALOG-STYLE WIDTH",48,104,150,14,juce::Justification::left);
    g.drawText("SHAPE",455,104,170,14,juce::Justification::right);
}

void PVAudioProcessorEditor::resized()
{
    title.setBounds(40,27,65,44);
    subtitle.setBounds(105,42,420,20);
    bypass.setBounds(578,30,66,30);
    magicHint.setBounds(455,101,175,18);

    // Three knobs evenly spaced across the lower area: MAGIC (left, alone) | TAPE, CHORUS (right pair)
    // FIX (real bug found in review): Magic is intentionally bigger than Tape/Chorus (it's the primary
    // control) but was positioned at the same TOP y-coordinate as them, which put its CENTRE 12.5px
    // lower than theirs - visibly "not centered" relative to the other two. Aligning all three knobs'
    // vertical centers instead, regardless of their differing sizes, fixes this properly.
    const int tapeChorusCenterY = 143 + 120/2;
    magic.setBounds(70, tapeChorusCenterY-145/2, 145, 145);
    tape.setBounds(380,143,120,120);
    chorus.setBounds(520,143,120,120);
    magicLabel.setBounds(70, tapeChorusCenterY+145/2+4, 145, 20);
    tapeLabel.setBounds(380,268,120,20);
    chorusLabel.setBounds(520,268,120,20);
}
