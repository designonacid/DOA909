#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "DSP/KickEngine.h"
#include <array>
#include <memory>
#include <vector>

struct RenderedKick
{
    std::vector<float> data;
    double sampleRate = 44100.0;
};

class ThumpProcessor : public juce::AudioProcessor,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::Timer
{
public:
    ThumpProcessor();
    ~ThumpProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- message-thread API for the editor ----
    thump::Curve& getPitchCurve() { return pitchCurve; }
    thump::Curve& getAmpCurve()   { return ampCurve; }
    void curvesChanged()          { dirty = true; }
    void renderNow();
    const RenderedKick* getRendered() const { return current.load(); }
    int getRenderVersion() const { return renderVersion.load(); }
    void audition() { auditionRequested = true; }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int> playCount { 0 }; // bumps on every hit, for the UI

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override { dirty = true; }
    void timerCallback() override;
    thump::KickParams readParams() const;
    void startVoice (float gain);

    static juce::String curveToString (const thump::Curve&);
    static thump::Curve curveFromString (const juce::String&, const thump::Curve& fallback);

    thump::Curve pitchCurve, ampCurve;

    // Rendered buffers: the audio thread only reads `current`. Old buffers are kept
    // alive for a while so voices still playing them are never left dangling.
    std::atomic<RenderedKick*> current { nullptr };
    std::vector<std::pair<std::unique_ptr<RenderedKick>, double>> pool;
    std::atomic<bool> dirty { true };
    std::atomic<int> renderVersion { 0 };
    std::atomic<double> renderRate { 44100.0 };
    std::atomic<bool> auditionRequested { false };

    struct Voice
    {
        const RenderedKick* buf = nullptr;
        int pos = 0;
        float gain = 1.0f;
        int fadeLeft = -1; // -1 = not fading
        bool active() const { return buf != nullptr; }
    };
    std::array<Voice, 3> voices;
    int fadeSamples = 96;
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThumpProcessor)
};
