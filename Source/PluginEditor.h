#pragma once

#include "PluginProcessor.h"
#include "CurveEditor.h"

namespace thumpui
{
    const juce::Colour bg       { 0xff17181c };
    const juce::Colour panel    { 0xff22242a };
    const juce::Colour panelHi  { 0xff2d3038 };
    const juce::Colour text     { 0xffd9dce3 };
    const juce::Colour dim      { 0xff8a8f9c };
    const juce::Colour acid     { 0xffff4d6d };   // Thump accent: hot coral

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f, juce::Font::bold); }
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    };
}

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& name);
    void resized() override;
private:
    juce::Slider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
class ThumpEditor : public juce::AudioProcessorEditor
{
public:
    explicit ThumpEditor (ThumpProcessor&);
    ~ThumpEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Section { juce::String title; juce::Rectangle<int> area; };
    void setMode (CurveEditor::Mode);

    ThumpProcessor& proc;
    thumpui::LookAndFeel lnf;
    CurveEditor curveEditor;

    juce::TextButton pitchTab { "PITCH" }, ampTab { "AMP" }, snapBtn { "SNAP" }, auditionBtn { "PLAY" };
    juce::ComboBox presetBox, clickType, distType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> clickTypeA, distTypeA;

    juce::OwnedArray<Knob> bodyKnobs, clickKnobs, driveKnobs, eqKnobs, outKnobs;
    std::vector<Section> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThumpEditor)
};
