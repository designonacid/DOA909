#include "CurveEditor.h"

namespace
{
    const juce::Colour gridCol  { 0xff2a2d35 };
    const juce::Colour labelCol { 0xff8a8f9c };
    const juce::Colour lineCol  { 0xffe9ebf0 };
    constexpr float nodeR = 7.0f;

    juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz"
                             : juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

CurveEditor::CurveEditor (ThumpProcessor& p) : proc (p)
{
    setRepaintsOnMouseActivity (false);
    startTimerHz (30);
}

void CurveEditor::timerCallback()
{
    bool needs = false;
    if (proc.getRenderVersion() != lastVersion) { lastVersion = proc.getRenderVersion(); needs = true; }
    if (proc.playCount.load() != lastPlayCount)  { lastPlayCount = proc.playCount.load(); flash = 1.0f; }
    if (flash > 0.0f) { flash = juce::jmax (0.0f, flash - 0.08f); needs = true; }
    if (needs) repaint();
}

juce::Rectangle<float> CurveEditor::plot() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (58).withTrimmedRight (18).withTrimmedTop (16).withTrimmedBottom (28);
}

juce::Point<float> CurveEditor::toScreen (float x, float y) const
{
    auto r = plot();
    return { r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight() };
}

juce::Point<float> CurveEditor::fromScreen (juce::Point<float> p) const
{
    auto r = plot();
    return { juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()),
             juce::jlimit (0.0f, 1.0f, (r.getBottom() - p.y) / r.getHeight()) };
}

juce::Point<float> CurveEditor::bendHandle (int seg)
{
    auto& n = curve().nodes;
    const float xm = (n[(size_t) seg].x + n[(size_t) seg + 1].x) * 0.5f;
    return toScreen (xm, curve().eval (xm));
}

int CurveEditor::hitNode (juce::Point<float> p)
{
    auto& n = curve().nodes;
    for (int i = (int) n.size() - 1; i >= 0; --i)
        if (toScreen (n[(size_t) i].x, n[(size_t) i].y).getDistanceFrom (p) <= nodeR + 4.0f) return i;
    return -1;
}

int CurveEditor::hitBend (juce::Point<float> p)
{
    auto& n = curve().nodes;
    for (int i = 0; i + 1 < (int) n.size(); ++i)
    {
        // skip handles on near-vertical or tiny segments where they would overlap nodes
        if (toScreen (n[(size_t) i].x, 0).getDistanceFrom (toScreen (n[(size_t) i + 1].x, 0)) < 24.0f) continue;
        if (bendHandle (i).getDistanceFrom (p) <= 8.0f) return i;
    }
    return -1;
}

float CurveEditor::snapY (float y) const
{
    if (! snap) return y;
    if (mode == Mode::Amp) return std::round (y * 20.0f) / 20.0f;
    const float hz = thump::pitchYToHz (y);
    const float note = std::round (69.0f + 12.0f * std::log2 (hz / 440.0f));
    return thump::hzToPitchY (440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f));
}

juce::String CurveEditor::nodeLabel (const thump::CurveNode& n) const
{
    if (mode == Mode::Amp) return juce::String (juce::roundToInt (n.y * 100.0f)) + "%";
    const float hz = thump::pitchYToHz (n.y);
    const int note = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
    return juce::MidiMessage::getMidiNoteName (note, true, true, 3) + " / " + hzText (hz);
}

void CurveEditor::changed()
{
    proc.curvesChanged();
    repaint();
}

//==============================================================================
void CurveEditor::paint (juce::Graphics& g)
{
    auto r = plot();
    g.setColour (juce::Colour (0xff121317));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    // ---- grid ----
    g.setFont (juce::FontOptions (10.5f));
    if (mode == Mode::Pitch)
    {
        for (float hz : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
        {
            const float y = toScreen (0, thump::hzToPitchY (hz)).y;
            g.setColour (gridCol);
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
            g.setColour (labelCol);
            g.drawText (hz >= 1000 ? juce::String ((int) hz / 1000) + "k" : juce::String ((int) hz),
                        juce::Rectangle<float> (0, y - 7, r.getX() - 8, 14), juce::Justification::centredRight);
        }
    }
    else
    {
        for (int pc = 0; pc <= 100; pc += 25)
        {
            const float y = toScreen (0, pc / 100.0f).y;
            g.setColour (gridCol);
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
            g.setColour (labelCol);
            g.drawText (juce::String (pc) + "%", juce::Rectangle<float> (0, y - 7, r.getX() - 8, 14), juce::Justification::centredRight);
        }
    }

    const float lengthMs = proc.apvts.getRawParameterValue ("length")->load();
    float step = 10.0f;
    for (float s : { 10.f, 20.f, 50.f, 100.f, 200.f, 250.f, 500.f })
        if (lengthMs / s <= 10.0f) { step = s; break; }
    for (float t = 0; t <= lengthMs + 0.1f; t += step)
    {
        const float x = toScreen (t / lengthMs, 0).x;
        g.setColour (gridCol);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (labelCol);
        g.drawText (juce::String ((int) t) + (t < 0.5f ? " ms" : ""), juce::Rectangle<float> (x - 30, r.getBottom() + 6, 60, 14),
                    juce::Justification::centred);
    }

    // ---- rendered waveform ----
    if (auto* k = proc.getRendered(); k != nullptr && ! k->data.empty())
    {
        const auto& d = k->data;
        const int w = (int) r.getWidth();
        const float mid = r.getCentreY(), half = r.getHeight() * 0.46f;
        g.setColour (accent.withAlpha (0.28f + 0.3f * flash));
        for (int px = 0; px < w; ++px)
        {
            const size_t a = (size_t) ((double) px / w * d.size());
            const size_t b = juce::jmax (a + 1, (size_t) ((double) (px + 1) / w * d.size()));
            float lo = 0, hi = 0;
            for (size_t i = a; i < b && i < d.size(); ++i) { lo = juce::jmin (lo, d[i]); hi = juce::jmax (hi, d[i]); }
            g.drawVerticalLine ((int) r.getX() + px, mid - hi * half, mid - lo * half + 1.0f);
        }
    }

    // ---- curve ----
    auto& c = curve();
    juce::Path path;
    const int steps = juce::jmax (64, (int) r.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / steps;
        const auto p = toScreen (x, c.eval (x));
        if (i == 0) path.startNewSubPath (p); else path.lineTo (p);
    }
    g.setColour (lineCol);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bend handles
    for (int i = 0; i + 1 < (int) c.nodes.size(); ++i)
    {
        if (toScreen (c.nodes[(size_t) i].x, 0).getDistanceFrom (toScreen (c.nodes[(size_t) i + 1].x, 0)) < 24.0f) continue;
        const auto h = bendHandle (i);
        const bool hot = i == hoverBend || i == dragBend;
        juce::Path d;
        d.addRectangle (-4.0f, -4.0f, 8.0f, 8.0f);
        d.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi / 4).translated (h));
        g.setColour (hot ? accent : lineCol.withAlpha (0.45f));
        g.fillPath (d);
    }

    // nodes + labels
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    for (int i = 0; i < (int) c.nodes.size(); ++i)
    {
        const auto& n = c.nodes[(size_t) i];
        const auto p = toScreen (n.x, n.y);
        const bool hot = i == hoverNode || i == dragNode;
        g.setColour (juce::Colour (0xff121317));
        g.fillEllipse (p.x - nodeR, p.y - nodeR, nodeR * 2, nodeR * 2);
        g.setColour (hot ? accent : lineCol);
        g.drawEllipse (p.x - nodeR, p.y - nodeR, nodeR * 2, nodeR * 2, 2.0f);
        g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);

        const auto label = nodeLabel (n);
        const float lw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label) + 6.0f;
        float lx = p.x - lw * 0.5f;
        lx = juce::jlimit (r.getX(), r.getRight() - lw, lx);
        const float ly = p.y - nodeR - 18.0f < r.getY() ? p.y + nodeR + 3.0f : p.y - nodeR - 18.0f;
        g.setColour (hot ? accent : lineCol.withAlpha (0.85f));
        g.drawText (label, juce::Rectangle<float> (lx, ly, lw, 15), juce::Justification::centred);
    }

    if (c.nodes.size() <= 3 && hoverNode < 0)
    {
        g.setColour (labelCol.withAlpha (0.6f));
        g.setFont (juce::FontOptions (10.5f));
        g.drawText ("double-click to add a point  /  drag the diamonds to bend", r.reduced (8).removeFromBottom (16),
                    juce::Justification::bottomRight);
    }
}

//==============================================================================
void CurveEditor::mouseMove (const juce::MouseEvent& e)
{
    const int n = hitNode (e.position), b = n < 0 ? hitBend (e.position) : -1;
    if (n != hoverNode || b != hoverBend) { hoverNode = n; hoverBend = b; repaint(); }
    setMouseCursor (n >= 0 || b >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    dragNode = hitNode (e.position);
    dragBend = dragNode < 0 ? hitBend (e.position) : -1;

    auto& nodes = curve().nodes;
    if (e.mods.isPopupMenu() && dragNode > 0 && dragNode < (int) nodes.size() - 1)
    {
        nodes.erase (nodes.begin() + dragNode);
        dragNode = hoverNode = -1;
        changed();
        return;
    }
    if (dragBend >= 0) bendStart = nodes[(size_t) dragBend].bend;
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    auto& nodes = curve().nodes;
    if (dragNode >= 0)
    {
        const auto v = fromScreen (e.position);
        auto& n = nodes[(size_t) dragNode];
        if (dragNode > 0 && dragNode < (int) nodes.size() - 1)
            n.x = juce::jlimit (nodes[(size_t) dragNode - 1].x + 0.002f, nodes[(size_t) dragNode + 1].x - 0.002f, v.x);
        n.y = snapY (v.y);
        changed();
    }
    else if (dragBend >= 0)
    {
        const auto& a = nodes[(size_t) dragBend];
        const auto& b = nodes[(size_t) dragBend + 1];
        // dragging up always bulges the curve up, whichever way the segment slopes
        const float dir = b.y < a.y ? -1.0f : 1.0f;
        const float dy = (float) e.getDistanceFromDragStartY() / plot().getHeight();
        nodes[(size_t) dragBend].bend = juce::jlimit (-1.0f, 1.0f, bendStart + dir * dy * 2.5f);
        changed();
    }
}

void CurveEditor::mouseUp (const juce::MouseEvent&)
{
    dragNode = dragBend = -1;
    repaint();
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto& nodes = curve().nodes;
    const int hit = hitNode (e.position);
    if (hit > 0 && hit < (int) nodes.size() - 1)
    {
        nodes.erase (nodes.begin() + hit);
    }
    else if (hit < 0 && hitBend (e.position) < 0 && plot().contains (e.position))
    {
        const auto v = fromScreen (e.position);
        if (v.x <= 0.0f || v.x >= 1.0f) return;
        nodes.push_back ({ v.x, snapY (v.y), 0.0f });
        curve().sortAndClamp();
    }
    else if (hit >= 0 || hitBend (e.position) >= 0)
    {
        // double-click a bend handle: straighten that segment
        const int b = hitBend (e.position);
        if (b >= 0) nodes[(size_t) b].bend = 0.0f;
        else return;
    }
    hoverNode = -1;
    changed();
}
