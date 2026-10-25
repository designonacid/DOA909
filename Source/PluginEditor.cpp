#include "PluginEditor.h"

using namespace thumpui;
using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, acid.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::Label::textColourId, dim);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, panelHi);
    setColour (juce::TextButton::buttonOnColourId, acid);
    setColour (juce::TextButton::textColourOffId, dim);
    setColour (juce::TextButton::textColourOnId, bg);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float a0, float a1, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = a0 + pos * (a1 - a0);
    const float track = juce::jmax (3.0f, r * 0.12f);

    juce::Path bgArc, valArc;
    bgArc.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, a0, a1, true);
    g.setColour (panelHi.brighter (0.1f));
    g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar params (tune) fill from the centre
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    valArc.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (s.isEnabled() ? acid : dim);
    g.strokePath (valArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knob cap
    const float capR = r - track * 2.4f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d46), c.x, c.y - capR,
                                             juce::Colour (0xff1e2025), c.x, c.y + capR, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -capR + 3.0f, 3.0f, capR * 0.45f, 1.5f);
    g.setColour (text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    auto col = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
    if (over && ! on) col = col.brighter (0.12f);
    if (down) col = col.darker (0.15f);
    if (! b.isEnabled()) col = col.withMultipliedAlpha (0.35f);
    g.setColour (col);
    g.fillRoundedRectangle (r, 4.0f);
    if (on)
    {
        g.setColour (col.withAlpha (0.35f));
        g.drawRoundedRectangle (r.expanded (1.0f), 5.0f, 2.0f);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                  .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.fillRoundedRectangle (r, 4.0f);
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (box.getLocalBounds());
    l.setJustificationType (juce::Justification::centred);
    l.setFont (getComboBoxFont (box));
}

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, const juce::String& name)
    : attachment (s, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider.setDoubleClickReturnValue (true, s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (slider);

    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

//==============================================================================
ThumpEditor::ThumpEditor (ThumpProcessor& p) : AudioProcessorEditor (&p), proc (p), curveEditor (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    curveEditor.setAccent (acid);
    addAndMakeVisible (curveEditor);

    for (auto* b : { &pitchTab, &ampTab }) { b->setRadioGroupId (1); b->setClickingTogglesState (true); addAndMakeVisible (b); }
    pitchTab.setToggleState (true, juce::dontSendNotification);
    pitchTab.onClick = [this] { if (pitchTab.getToggleState()) setMode (CurveEditor::Mode::Pitch); };
    ampTab.onClick   = [this] { if (ampTab.getToggleState())   setMode (CurveEditor::Mode::Amp); };

    snapBtn.setClickingTogglesState (true);
    snapBtn.setTooltip ("Snap pitch points to semitones (amp points to 5% steps)");
    snapBtn.onClick = [this] { curveEditor.setSnap (snapBtn.getToggleState()); };
    addAndMakeVisible (snapBtn);

    auditionBtn.setColour (juce::TextButton::buttonColourId, acid);
    auditionBtn.setColour (juce::TextButton::textColourOffId, bg);
    auditionBtn.onClick = [this] { proc.audition(); };
    addAndMakeVisible (auditionBtn);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        proc.setCurrentProgram (presetBox.getSelectedId() - 1);
        curveEditor.repaint();
    };
    addAndMakeVisible (presetBox);

    clickType.addItemList ({ "NOISE", "SNAP" }, 1);
    distType.addItemList ({ "SOFT", "HARD", "FOLD", "CRUSH" }, 1);
    addAndMakeVisible (clickType); addAndMakeVisible (distType);
    clickTypeA = std::make_unique<APVTS::ComboBoxAttachment> (s, "clickType", clickType);
    distTypeA  = std::make_unique<APVTS::ComboBoxAttachment> (s, "distType", distType);

    auto add = [&] (juce::OwnedArray<Knob>& arr, const char* id, const char* name) { addAndMakeVisible (arr.add (new Knob (s, id, name))); };
    add (bodyKnobs, "length", "Length"); add (bodyKnobs, "tune", "Tune"); add (bodyKnobs, "shape", "Shape"); add (bodyKnobs, "phase", "Phase");
    add (clickKnobs, "clickLevel", "Level"); add (clickKnobs, "clickTone", "Tone"); add (clickKnobs, "clickDecay", "Decay");
    add (driveKnobs, "distDrive", "Drive"); add (driveKnobs, "distMix", "Mix");
    add (eqKnobs, "lowBoost", "Low"); add (eqKnobs, "highCut", "High Cut");
    add (outKnobs, "velSens", "Velocity"); add (outKnobs, "volume", "Volume");

    setSize (1060, 660);
}

ThumpEditor::~ThumpEditor() { setLookAndFeel (nullptr); }

void ThumpEditor::setMode (CurveEditor::Mode m) { curveEditor.setMode (m); }

void ThumpEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    auto header = getLocalBounds().removeFromTop (60).reduced (24, 0);
    g.setColour (acid);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("THUMP", header, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("KICK SYNTH", header.withTrimmedLeft (122), juce::Justification::centredLeft);

    for (auto& sec : sections)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (sec.area.toFloat(), 10.0f);
        g.setColour (acid.withAlpha (0.9f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (sec.title, sec.area.reduced (12, 8).removeFromTop (14), juce::Justification::centredLeft);
    }
}

void ThumpEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto header = r.removeFromTop (44);

    auto right = header.removeFromRight (560);
    auditionBtn.setBounds (right.removeFromRight (80).reduced (0, 6));
    right.removeFromRight (12);
    presetBox.setBounds (right.removeFromRight (180).reduced (0, 6));
    right.removeFromRight (20);
    snapBtn.setBounds (right.removeFromRight (70).reduced (0, 6));
    right.removeFromRight (12);
    ampTab.setBounds (right.removeFromRight (70).reduced (0, 6));
    pitchTab.setBounds (right.removeFromRight (74).reduced (0, 6));

    r.removeFromTop (6);
    auto bottom = r.removeFromBottom (190);
    r.removeFromBottom (12);
    curveEditor.setBounds (r);

    // sections sized by how many controls they hold
    struct Def { const char* title; int units; };
    const Def defs[] = { { "BODY", 4 }, { "CLICK", 4 }, { "DRIVE", 3 }, { "EQ", 2 }, { "OUTPUT", 2 } };
    const int gap = 10, totalUnits = 15;
    const int unitW = (bottom.getWidth() - gap * 4) / totalUnits;
    sections.clear();
    for (auto& d : defs)
    {
        sections.push_back ({ d.title, bottom.removeFromLeft (unitW * d.units) });
        bottom.removeFromLeft (gap);
    }

    auto layout = [unitW] (juce::Rectangle<int> area, juce::Component* combo, juce::OwnedArray<Knob>& knobs)
    {
        auto a = area.reduced (6, 0).withTrimmedTop (28).withTrimmedBottom (10);
        const int w = (a.getWidth()) / (knobs.size() + (combo != nullptr ? 1 : 0));
        juce::ignoreUnused (unitW);
        if (combo != nullptr)
            combo->setBounds (a.removeFromLeft (w).withSizeKeepingCentre (w - 8, 30).translated (0, -8));
        for (auto* k : knobs) k->setBounds (a.removeFromLeft (w).reduced (2, 0));
    };
    layout (sections[0].area, nullptr, bodyKnobs);
    layout (sections[1].area, &clickType, clickKnobs);
    layout (sections[2].area, &distType, driveKnobs);
    layout (sections[3].area, nullptr, eqKnobs);
    layout (sections[4].area, nullptr, outKnobs);
}
