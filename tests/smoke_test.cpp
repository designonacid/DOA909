// Headless smoke test: drives the real processor + editor.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    ThumpProcessor proc;
    const double sr = 48000.0; const int block = 256;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    juce::AudioBuffer<float> buf (2, block), all (2, (int) (sr * 4));
    juce::MidiBuffer midi;
    bool bad = false;
    auto run = [&] (int startBlock, int numBlocks, std::initializer_list<int> hitBlocks, float vel)
    {
        for (int b = 0; b < numBlocks; ++b)
        {
            midi.clear();
            for (int h : hitBlocks) if (h == b) midi.addEvent (juce::MidiMessage::noteOn (1, 36, vel), 17);
            proc.processBlock (buf, midi);
            const int pos = (startBlock + b) * block;
            if (pos + block <= all.getNumSamples())
                for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, pos, buf, ch, 0, block);
        }
    };

    for (int preset = 0; preset < proc.getNumPrograms(); ++preset)
    {
        proc.setCurrentProgram (preset);
        all.clear();
        run (0, (int) (sr * 4) / block, { 0, 100, 102, 400 }, preset == 0 ? 0.5f : 1.0f); // includes a fast retrigger
        float peak = all.getMagnitude (0, 0, all.getNumSamples());
        for (int i = 0; i < all.getNumSamples(); ++i) if (! std::isfinite (all.getSample (0, i))) bad = true;
        // largest sample-to-sample jump right at the retrigger (block 102) -> click check
        float jump = 0; int p0 = 102 * block + 17;
        for (int i = p0 - 200; i < p0 + 200; ++i) jump = juce::jmax (jump, std::abs (all.getSample (0, i) - all.getSample (0, i - 1)));
        std::printf ("preset %-15s render %6zu samples  peak %.3f  max step near retrigger %.3f\n",
                     proc.getProgramName (preset).toRawUTF8(), proc.getRendered()->data.size(), peak, jump);
        if (preset == 2)
        {
            juce::WavAudioFormat wav;
            auto f = juce::File::getCurrentWorkingDirectory().getChildFile ("plugin_render.wav"); f.deleteFile();
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0)))
                w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        }
    }

    // Genre loops: 2 bars of four-on-the-floor per new preset, at a fitting tempo
    {
        struct G { int preset; double bpm; };
        const G genres[] = { { 5, 138.0 }, { 6, 145.0 }, { 7, 124.0 }, { 8, 124.0 }, { 9, 124.0 } };
        juce::AudioBuffer<float> loops (2, (int) (sr * 25)); loops.clear();
        int writePos = 0;
        for (auto& gnr : genres)
        {
            proc.setCurrentProgram (gnr.preset);
            const int beat = (int) std::round (sr * 60.0 / gnr.bpm);
            const int total = beat * 8 + (int) (sr * 0.6);
            for (int pos = 0; pos < total; pos += block)
            {
                midi.clear();
                for (int bt = 0; bt < 8; ++bt)
                    if (bt * beat >= pos && bt * beat < pos + block) midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), bt * beat - pos);
                proc.processBlock (buf, midi);
                if (writePos + pos + block <= loops.getNumSamples())
                    for (int ch = 0; ch < 2; ++ch) loops.copyFrom (ch, writePos + pos, buf, ch, 0, block);
            }
            const auto& d = proc.getRendered()->data;
            int zc = 0; const size_t a = d.size() * 6 / 10, b = d.size() * 9 / 10;
            for (size_t i = a + 1; i < b; ++i) if (d[i - 1] < 0 && d[i] >= 0) ++zc;
            std::printf ("loop %-24s %5.0f BPM  kick %4.0f ms  tail ~%4.1f Hz  (1/8 note = %3.0f ms)\n",
                         proc.getProgramName (gnr.preset).toRawUTF8(), gnr.bpm, d.size() * 1000.0 / sr,
                         zc / ((b - a) / sr), 30000.0 / gnr.bpm);
            writePos += total;
        }
        juce::WavAudioFormat wav;
        auto f = juce::File::getCurrentWorkingDirectory().getChildFile ("genre_loops.wav"); f.deleteFile();
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 16, {}, 0)))
            w->writeFromAudioSampleBuffer (loops, 0, writePos);
    }

    // state round trip including curves
    proc.setCurrentProgram (1);
    juce::MemoryBlock state; proc.getStateInformation (state);
    const auto before = proc.getPitchCurve().nodes.size();
    proc.setCurrentProgram (0);
    proc.setStateInformation (state.getData(), (int) state.getSize());
    std::printf ("STATE: pitch nodes %zu -> %zu, length %.0f ms (expect 1400)\n", before, proc.getPitchCurve().nodes.size(),
                 proc.apvts.getRawParameterValue ("length")->load());

    // UI snapshots
    proc.setCurrentProgram (6);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    juce::PNGImageFormat png;
    auto snap = [&] (const char* name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
        auto pf = juce::File::getCurrentWorkingDirectory().getChildFile (name); pf.deleteFile();
        juce::FileOutputStream os (pf); png.writeImageToStream (img, os);
    };
    snap ("ui_pitch.png");
    if (auto* ce = dynamic_cast<CurveEditor*> (ed->getChildComponent (0))) ce->setMode (CurveEditor::Mode::Amp);
    snap ("ui_amp.png");
    std::printf ("UI: %dx%d snapshots written\n", ed->getWidth(), ed->getHeight());
    ed.reset();
    return bad ? 1 : 0;
}
