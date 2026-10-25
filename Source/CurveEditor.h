#pragma once

#include "PluginProcessor.h"

// Interactive breakpoint editor for the pitch and amp curves, drawn over the
// rendered kick waveform.
//   drag node            move it (end nodes move vertically only)
//   drag segment handle  bend the curve
//   double-click empty   add a node
//   double-click node    delete it (or right-click)
class CurveEditor : public juce::Component, private juce::Timer
{
public:
    enum class Mode { Pitch, Amp };

    explicit CurveEditor (ThumpProcessor&);
    ~CurveEditor() override { stopTimer(); }

    void setMode (Mode m) { mode = m; dragNode = dragBend = hoverNode = hoverBend = -1; repaint(); }
    Mode getMode() const { return mode; }
    void setSnap (bool s) { snap = s; }
    void setAccent (juce::Colour c) { accent = c; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverNode = hoverBend = -1; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    thump::Curve& curve() { return mode == Mode::Pitch ? proc.getPitchCurve() : proc.getAmpCurve(); }

    juce::Rectangle<float> plot() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> fromScreen (juce::Point<float> p) const; // returns curve-space (x, y)
    juce::Point<float> bendHandle (int segment);
    int hitNode (juce::Point<float> p);
    int hitBend (juce::Point<float> p);
    float snapY (float y) const;
    juce::String nodeLabel (const thump::CurveNode&) const;
    void changed();

    ThumpProcessor& proc;
    Mode mode = Mode::Pitch;
    bool snap = false;
    juce::Colour accent { 0xffff4d6d };

    int dragNode = -1, dragBend = -1, hoverNode = -1, hoverBend = -1;
    float bendStart = 0.0f;
    int lastVersion = -1, lastPlayCount = 0;
    float flash = 0.0f;   // brief highlight when the kick is triggered
};
