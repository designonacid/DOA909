#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using thump::hzToPitchY;

//==============================================================================
namespace
{
    const char* paramIds[] = { "length", "tune", "shape", "phase", "clickType", "clickLevel", "clickTone", "clickDecay",
                               "distType", "distDrive", "distMix", "lowBoost", "highCut", "volume", "velSens" };

    struct FactoryPreset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values; // real (denormalised) values
        thump::Curve pitch, amp;
    };

    thump::Curve curve (std::initializer_list<thump::CurveNode> n) { thump::Curve c; c.nodes = n; return c; }
    float hz (float f) { return hzToPitchY (f); }

    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const std::vector<FactoryPreset> presets = {
            { "Init Punch", {},
              thump::defaultPitchCurve(), thump::defaultAmpCurve() },
            { "Deep 808", { { "length", 1400 }, { "phase", 0 }, { "clickLevel", 0.12f }, { "shape", 0.1f }, { "lowBoost", 2 } },
              curve ({ { 0, hz (2000), -0.5f }, { 0.02f, hz (120), -0.4f }, { 0.15f, hz (52), 0 }, { 1, hz (48), 0 } }),
              curve ({ { 0, 1, 0 }, { 0.6f, 0.75f, -0.4f }, { 1, 0, 0 } }) },
            { "Hard Techno", { { "length", 420 }, { "shape", 0.3f }, { "distDrive", 0.55f }, { "clickLevel", 0.6f },
                               { "clickTone", 0.8f }, { "highCut", 12000 } },
              curve ({ { 0, hz (12000), -0.7f }, { 0.03f, hz (250), -0.4f }, { 0.18f, hz (65), 0 }, { 1, hz (55), 0 } }),
              curve ({ { 0, 1, 0 }, { 0.5f, 0.8f, -0.5f }, { 1, 0, 0 } }) },
            { "Hardstyle Tail", { { "length", 700 }, { "distType", 1 }, { "distDrive", 0.7f }, { "clickLevel", 0.4f },
                                  { "clickType", 1 }, { "highCut", 9000 } },
              curve ({ { 0, hz (15000), -0.7f }, { 0.02f, hz (300), -0.5f }, { 0.1f, hz (72), 0 }, { 0.6f, hz (70), 0.3f }, { 1, hz (52), 0 } }),
              curve ({ { 0, 1, 0 }, { 0.85f, 0.9f, 0 }, { 1, 0, 0 } }) },
            { "Lo-Fi Thud", { { "length", 500 }, { "distType", 3 }, { "distDrive", 0.4f }, { "distMix", 0.5f },
                              { "highCut", 3000 }, { "clickLevel", 0.1f } },
              curve ({ { 0, hz (900), -0.4f }, { 0.08f, hz (90), -0.2f }, { 1, hz (50), 0 } }),
              curve ({ { 0, 1, 0 }, { 0.3f, 0.6f, -0.3f }, { 1, 0, 0 } }) },

            // ---- Trance -------------------------------------------------------------
            // Uplifting / progressive (~138 BPM): warm ~55 Hz tail that is gone before the
            // offbeat bass, bright noise click to cut through supersaws.
            { "Trance - Uplifting", { { "length", 260 }, { "clickLevel", 0.55f }, { "clickTone", 0.75f }, { "clickDecay", 0.15f },
                                      { "distDrive", 0.25f }, { "shape", 0.1f }, { "lowBoost", 1.5f } },
              curve ({ { 0, hz (9000), -0.7f }, { 0.03f, hz (200), -0.4f }, { 0.25f, hz (60), 0 }, { 1, hz (55), 0 } }),
              curve ({ { 0, 1, 0 }, { 0.45f, 0.8f, -0.6f }, { 0.85f, 0.05f, 0 }, { 1, 0, 0 } }) },
            // Psytrance: exactly a 1/8 note at 145 BPM (207 ms), sweep resolves on G1 (49 Hz),
            // "fishtail" amp shape: snappy hit, brief dip, then the boom. Leaves room for the rolling bass.
            { "Trance - Psy 145 G1", { { "length", 207 }, { "clickType", 1 }, { "clickLevel", 0.45f }, { "clickTone", 0.85f },
                                       { "clickDecay", 0.1f }, { "distDrive", 0.15f } },
              curve ({ { 0, hz (12000), -0.7f }, { 0.02f, hz (400), -0.5f }, { 0.08f, hz (120), -0.3f }, { 0.35f, hz (49), 0 }, { 1, hz (49), 0 } }),
              curve ({ { 0, 1, -0.4f }, { 0.08f, 0.45f, 0.3f }, { 0.2f, 0.9f, 0 }, { 0.7f, 0.75f, -0.5f }, { 1, 0, 0 } }) },

            // ---- Melodic techno -----------------------------------------------------
            // Deep (~124 BPM): round, low ~48 Hz tail, soft dark click, gentle saturation.
            { "Melodic - Deep", { { "length", 470 }, { "clickLevel", 0.3f }, { "clickTone", 0.45f }, { "clickDecay", 0.25f },
                                  { "distDrive", 0.2f }, { "highCut", 9000 }, { "lowBoost", 2 } },
              curve ({ { 0, hz (4000), -0.6f }, { 0.03f, hz (160), -0.4f }, { 0.2f, hz (55), 0 }, { 1, hz (48), 0 } }),
              curve ({ { 0, 1, 0 }, { 0.6f, 0.7f, -0.5f }, { 1, 0, 0 } }) },
            // Main stage: punchier and shorter, transient-heavy, more drive. Sits between techno and trance.
            { "Melodic - Main Stage", { { "length", 380 }, { "shape", 0.15f }, { "distDrive", 0.35f }, { "clickLevel", 0.45f },
                                        { "clickTone", 0.6f }, { "clickDecay", 0.2f }, { "lowBoost", 1 } },
              curve ({ { 0, hz (7000), -0.7f }, { 0.025f, hz (220), -0.4f }, { 0.18f, hz (58), 0 }, { 1, hz (50), 0 } }),
              curve ({ { 0, 1, 0 }, { 0.5f, 0.8f, -0.5f }, { 1, 0, 0 } }) },
            // Rumble layer: put on its own track under the main kick, add reverb, sidechain it to the kick.
            // Long, dark, saturated sub tail with no click.
            { "Melodic - Rumble Layer", { { "length", 1200 }, { "phase", 0 }, { "clickLevel", 0 }, { "shape", 0.3f },
                                          { "distDrive", 0.6f }, { "highCut", 250 }, { "volume", -4 } },
              curve ({ { 0, hz (300), -0.4f }, { 0.1f, hz (55), 0 }, { 1, hz (47), 0 } }),
              curve ({ { 0, 1, 0 }, { 0.5f, 0.85f, -0.5f }, { 1, 0, 0 } }) },
        };
        return presets;
    }
}

APVTS::ParameterLayout ThumpProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
    auto unit = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
    auto f01 = [&] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (pct));
    };

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "length", 1 }, "Length", NormalisableRange<float> (50.0f, 2000.0f, 1.0f, 0.5f), 600.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "tune", 1 }, "Tune", NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("st")));
    l.add (f01 ("shape", "Shape", 0.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "phase", 1 }, "Start Phase", NormalisableRange<float> (0.0f, 360.0f, 1.0f), 90.0f,
                                                  AudioParameterFloatAttributes().withLabel ("deg")));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "clickType", 1 }, "Click Type", StringArray { "Noise", "Snap" }, 0));
    l.add (f01 ("clickLevel", "Click Level", 0.5f));
    l.add (f01 ("clickTone", "Click Tone", 0.6f));
    l.add (f01 ("clickDecay", "Click Decay", 0.2f));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "distType", 1 }, "Drive Type", StringArray { "Soft", "Hard", "Fold", "Crush" }, 0));
    l.add (f01 ("distDrive", "Drive", 0.2f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "distMix", 1 }, "Drive Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f,
                                                  AudioParameterFloatAttributes().withStringFromValueFunction (unit)));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "lowBoost", 1 }, "Low Boost", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "highCut", 1 }, "High Cut", NormalisableRange<float> (200.0f, 20000.0f, 1.0f, 0.3f), 20000.0f,
                                                  AudioParameterFloatAttributes().withLabel ("Hz")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (f01 ("velSens", "Velocity", 0.3f));
    return l;
}

//==============================================================================
ThumpProcessor::ThumpProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ThumpState", createLayout())
{
    pitchCurve = thump::defaultPitchCurve();
    ampCurve = thump::defaultAmpCurve();
    for (auto* id : paramIds) apvts.addParameterListener (id, this);
    renderNow();
    startTimerHz (30);
}

ThumpProcessor::~ThumpProcessor()
{
    stopTimer();
    for (auto* id : paramIds) apvts.removeParameterListener (id, this);
}

thump::KickParams ThumpProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    thump::KickParams p;
    p.lengthMs = v ("length");   p.tuneSemis = v ("tune");   p.shape = v ("shape");   p.phaseDeg = v ("phase");
    p.clickType = (int) v ("clickType"); p.clickLevel = v ("clickLevel"); p.clickTone = v ("clickTone"); p.clickDecay = v ("clickDecay");
    p.distType = (int) v ("distType");   p.distDrive = v ("distDrive");   p.distMix = v ("distMix");
    p.lowBoostDb = v ("lowBoost"); p.highCutHz = v ("highCut"); p.outGainDb = v ("volume");
    return p;
}

void ThumpProcessor::renderNow()
{
    // Message thread only
    dirty = false;
    auto r = std::make_unique<RenderedKick>();
    r->sampleRate = renderRate.load();
    pitchCurve.sortAndClamp();
    ampCurve.sortAndClamp();
    r->data = thump::renderKick (readParams(), pitchCurve, ampCurve, r->sampleRate);

    const double now = juce::Time::getMillisecondCounterHiRes();
    for (auto& [buf, retired] : pool)
        if (buf.get() == current.load()) retired = now;   // retire the outgoing buffer
    current.store (r.get());
    pool.emplace_back (std::move (r), -1.0);

    // free buffers retired > 10 s ago (longer than any kick can play)
    pool.erase (std::remove_if (pool.begin(), pool.end(), [&] (auto& e)
                { return e.second > 0.0 && now - e.second > 10000.0 && e.first.get() != current.load(); }),
                pool.end());
    ++renderVersion;
}

void ThumpProcessor::timerCallback()
{
    if (dirty) renderNow();
}

void ThumpProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& v : voices) v = {};
    fadeSamples = juce::jmax (8, (int) (sampleRate * 0.002));
    if (std::abs (sampleRate - renderRate.load()) > 0.5)
    {
        renderRate = sampleRate;
        if (juce::MessageManager::existsAndIsCurrentThread()) renderNow();
        else dirty = true;
    }
}

bool ThumpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void ThumpProcessor::startVoice (float gain)
{
    auto* buf = current.load();
    if (buf == nullptr || buf->data.empty()) return;

    // fade out whatever is still ringing so retriggers never click
    for (auto& v : voices)
        if (v.active() && v.fadeLeft < 0) v.fadeLeft = fadeSamples;

    Voice* slot = nullptr;
    for (auto& v : voices) if (! v.active()) { slot = &v; break; }
    if (slot == nullptr)   // all busy: steal the one closest to finishing its fade
    {
        slot = &voices[0];
        for (auto& v : voices) if (v.fadeLeft >= 0 && v.fadeLeft < slot->fadeLeft) slot = &v;
    }
    *slot = { buf, 0, gain, -1 };
    ++playCount;
}

void ThumpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    auto* out = buffer.getWritePointer (0);
    const float velSens = apvts.getRawParameterValue ("velSens")->load();

    if (auditionRequested.exchange (false)) startVoice (1.0f);

    auto midiIt = midi.cbegin();
    for (int i = 0; i < n; ++i)
    {
        for (; midiIt != midi.cend() && (*midiIt).samplePosition <= i; ++midiIt)
        {
            const auto m = (*midiIt).getMessage();
            if (m.isNoteOn())
                startVoice (1.0f - velSens * (1.0f - m.getFloatVelocity()));
        }

        float s = 0.0f;
        for (auto& v : voices)
        {
            if (! v.active()) continue;
            float g = v.gain;
            if (v.fadeLeft >= 0)
            {
                g *= (float) v.fadeLeft / (float) fadeSamples;
                if (--v.fadeLeft < 0) { v.buf = nullptr; continue; }
            }
            s += v.buf->data[(size_t) v.pos] * g;
            if (++v.pos >= (int) v.buf->data.size()) v.buf = nullptr;
        }
        // overlapping hits during a retrigger crossfade can sum above full scale:
        // same soft knee as the engine keeps the output under 0 dBFS
        const float a = std::abs (s);
        out[i] = a > 0.9f ? std::copysign (0.9f + 0.08f * std::tanh ((a - 0.9f) / 0.08f), s) : s;
    }

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, n);
}

//==============================================================================
int ThumpProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const juce::String ThumpProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? factoryPresets()[(size_t) i].name : "";
}

void ThumpProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentPreset = index;
    const auto& p = factoryPresets()[(size_t) index];

    // reset everything to defaults, then apply the preset's values
    for (auto* id : paramIds)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->getDefaultValue());
    for (auto& [id, value] : p.values)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (value));

    pitchCurve = p.pitch;
    ampCurve = p.amp;
    if (juce::MessageManager::existsAndIsCurrentThread()) renderNow(); else dirty = true;
}

//==============================================================================
juce::String ThumpProcessor::curveToString (const thump::Curve& c)
{
    juce::StringArray parts;
    for (auto& n : c.nodes)
        parts.add (juce::String (n.x, 5) + "," + juce::String (n.y, 5) + "," + juce::String (n.bend, 4));
    return parts.joinIntoString (";");
}

thump::Curve ThumpProcessor::curveFromString (const juce::String& s, const thump::Curve& fallback)
{
    thump::Curve c;
    for (auto& part : juce::StringArray::fromTokens (s, ";", ""))
    {
        auto v = juce::StringArray::fromTokens (part, ",", "");
        if (v.size() == 3) c.nodes.push_back ({ v[0].getFloatValue(), v[1].getFloatValue(), v[2].getFloatValue() });
    }
    if (c.nodes.size() < 2) return fallback;
    c.sortAndClamp();
    return c;
}

void ThumpProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto curves = state.getOrCreateChildWithName ("Curves", nullptr);
    curves.setProperty ("pitch", curveToString (pitchCurve), nullptr);
    curves.setProperty ("amp", curveToString (ampCurve), nullptr);
    curves.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ThumpProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (auto curves = state.getChildWithName ("Curves"); curves.isValid())
    {
        pitchCurve = curveFromString (curves["pitch"].toString(), thump::defaultPitchCurve());
        ampCurve = curveFromString (curves["amp"].toString(), thump::defaultAmpCurve());
        currentPreset = (int) curves.getProperty ("preset", 0);
    }
    apvts.replaceState (state);
    if (juce::MessageManager::existsAndIsCurrentThread()) renderNow(); else dirty = true;
}

juce::AudioProcessorEditor* ThumpProcessor::createEditor() { return new ThumpEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ThumpProcessor(); }
