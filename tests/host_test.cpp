// Headless host test: instantiates the real plugin processor + editor, plays MIDI and the
// sequencer through processBlock, round-trips state, and renders the editor to PNG files.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

struct FakePlayHead : juce::AudioPlayHead
{
    double ppq = 0, bpm = 120; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setPpqPosition (ppq); p.setIsPlaying (playing); return p;
    }
};

static int fails = 0;
static void check (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8()); if (! ok) ++fails; }

static float runBlocks (GSProcessor& proc, FakePlayHead& ph, int blocks, std::function<void (juce::MidiBuffer&, int)> midiFn = nullptr, double sr = 48000, int bs = 256)
{
    juce::AudioBuffer<float> buf (2, bs); float pk = 0;
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer m; if (midiFn) midiFn (m, b);
        proc.processBlock (buf, m);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) { const float v = buf.getSample (c, i); if (! std::isfinite (v)) return -1; pk = std::max (pk, std::abs (v)); }
        ph.ppq += bs * ph.bpm / 60.0 / sr;
    }
    return pk;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto proc = std::make_unique<GSProcessor>();
    FakePlayHead ph; proc->setPlayHead (&ph);
    proc->setRateAndBufferSizeDetails (48000, 256);
    proc->prepareToPlay (48000, 256);

    // 1. Host transport drives the sequencer
    float pk = runBlocks (*proc, ph, 400);
    check (pk > 0.01f, "sequencer follows host transport and makes sound (peak " + juce::String (pk, 3) + ")");
    check (proc->vizStep.load() >= 0, "step position reported (step " + juce::String (proc->vizStep.load()) + ")");

    // 2. Host stopped: silence, then MIDI notes play clouds
    ph.playing = false; runBlocks (*proc, ph, 200);
    pk = runBlocks (*proc, ph, 50);
    check (pk < 0.01f, "silent with transport stopped and no notes (peak " + juce::String (pk, 4) + ")");
    pk = runBlocks (*proc, ph, 200, [] (juce::MidiBuffer& m, int b) { if (b == 0) { m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 10); m.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 90), 10); } });
    check (pk > 0.01f, "MIDI notes play grain clouds (peak " + juce::String (pk, 3) + ")");
    runBlocks (*proc, ph, 10, [] (juce::MidiBuffer& m, int b) { if (b == 0) { m.addEvent (juce::MidiMessage::noteOff (1, 60), 0); m.addEvent (juce::MidiMessage::noteOff (1, 67), 0); } });
    runBlocks (*proc, ph, 200); pk = runBlocks (*proc, ph, 20);
    check (pk < 0.01f, "notes release after note-off");

    // 3. Sustain pedal holds notes
    runBlocks (*proc, ph, 5, [] (juce::MidiBuffer& m, int b) { if (b == 0) { m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0); m.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 1); } if (b == 2) m.addEvent (juce::MidiMessage::noteOff (1, 64), 0); });
    pk = runBlocks (*proc, ph, 100);
    check (pk > 0.01f, "sustain pedal holds a released note");
    runBlocks (*proc, ph, 2, [] (juce::MidiBuffer& m, int b) { if (b == 0) m.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0); });
    runBlocks (*proc, ph, 200); pk = runBlocks (*proc, ph, 20);
    check (pk < 0.01f, "pedal up releases it");

    // 4. Pitch bend + mod wheel + transpose mode don't break anything
    proc->setParam ("mwDest", 1.f);
    pk = runBlocks (*proc, ph, 100, [] (juce::MidiBuffer& m, int b) { if (b == 0) { m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); m.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 5); m.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 6); } });
    check (pk > 0.01f && pk <= 1.f, "pitch bend and mod wheel");
    runBlocks (*proc, ph, 2, [] (juce::MidiBuffer& m, int b) { if (b == 0) m.addEvent (juce::MidiMessage::allNotesOff (1), 0); });
    proc->setParam ("kmode", 1.f); ph.playing = true;
    pk = runBlocks (*proc, ph, 200, [] (juce::MidiBuffer& m, int b) { if (b == 0) m.addEvent (juce::MidiMessage::noteOn (1, 72, (juce::uint8) 100), 0); });
    check (pk > 0.01f, "TRANSPOSE SEQ mode with host playing");
    proc->setParam ("kmode", 0.f);

    // 5. MIDI learn: map CC 21 to cutoff
    proc->startLearn ("cut");
    runBlocks (*proc, ph, 1, [] (juce::MidiBuffer& m, int) { m.addEvent (juce::MidiMessage::controllerEvent (1, 21, 64), 0); });
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    check (proc->getCCFor ("cut") == 21, "MIDI learn assigns CC 21 to CUTOFF");
    runBlocks (*proc, ph, 1, [] (juce::MidiBuffer& m, int) { m.addEvent (juce::MidiMessage::controllerEvent (1, 21, 0), 0); });
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    check (proc->getParam ("cut") < 40, "CC 21 now moves CUTOFF (" + juce::String (proc->getParam ("cut"), 1) + " Hz)");

    // 6. State round trip (params, steps, markers, MIDI map)
    proc->setParam ("rdecay", 0.9f); auto st = proc->getSteps(); st[3].on = true; st[3].m = 2; st[3].a = 0.3f; proc->setSteps (st);
    proc->setMarkers ({ 0.f, 0.2f, 0.4f, 0.6f });
    juce::MemoryBlock mb; proc->getStateInformation (mb);
    auto proc2 = std::make_unique<GSProcessor>(); proc2->setStateInformation (mb.getData(), (int) mb.getSize());
    const auto st2 = proc2->getSteps(); const auto mk2 = proc2->getMarkers();
    check (std::abs (proc2->getParam ("rdecay") - 0.9f) < 1e-3f, "state restores parameters");
    check (st2[3].on && st2[3].m == 2 && std::abs (st2[3].a - 0.3f) < 1e-4f, "state restores sequencer steps");
    check (mk2.size() == 4 && std::abs (mk2[2] - 0.4f) < 1e-4f, "state restores markers");
    check (proc2->getCCFor ("cut") == 21, "state restores MIDI learn map");

    // 7. Presets and mangle all render cleanly
    ph.playing = true; float worst = 0; bool bad = false;
    for (size_t i = 0; i < 17; ++i) { proc->applyPreset ((int) i); const float p = runBlocks (*proc, ph, 60, [] (juce::MidiBuffer& m, int b) { if (b == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); if (b == 50) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0); }); if (p < 0) bad = true; worst = std::max (worst, p); }
    for (int i = 0; i < 25; ++i) { proc->mangle(); const float p = runBlocks (*proc, ph, 40); if (p < 0) bad = true; worst = std::max (worst, p); }
    check (! bad && worst <= 1.f, "17 presets + 25 mangles render with no NaN/clipping (peak " + juce::String (worst, 3) + ")");

    // 8. Different sample rates / block sizes
    for (double sr : { 44100.0, 96000.0 }) for (int bs : { 32, 1024 })
    {
        proc->prepareToPlay (sr, bs); proc->applyPreset (0);
        const float p = runBlocks (*proc, ph, (int) (sr / bs), nullptr, sr, bs);
        check (p > 0.005f && p <= 1.f, "renders at " + juce::String (sr, 0) + " Hz / " + juce::String (bs) + "-sample blocks");
    }

    // 9. Editor renders (headless) — two scroll positions
    {
        proc->prepareToPlay (48000, 256); proc->applyPreset (0);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (1240, 900);
        runBlocks (*proc, ph, 200);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out ("/tmp/gs_editor_top.png"); out.deleteFile();
        { juce::FileOutputStream fos (out); juce::PNGImageFormat().writeImageToStream (img, fos); }
        if (auto* vp = ed->findChildWithID ("vp")) (void) vp;
        for (auto* c : ed->getChildren()) if (auto* v = dynamic_cast<juce::Viewport*> (c))
        {
            v->setViewPosition (0, 760);
            auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File o2 ("/tmp/gs_editor_scrolled.png"); o2.deleteFile(); juce::FileOutputStream f2 (o2); juce::PNGImageFormat().writeImageToStream (img2, f2);
            v->setViewPosition (0, 99999);
            auto img3 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File o3 ("/tmp/gs_editor_bottom.png"); o3.deleteFile(); juce::FileOutputStream f3 (o3); juce::PNGImageFormat().writeImageToStream (img3, f3);
        }
        check (img.isValid(), "editor renders");
        ed.reset();
    }
    std::printf (fails ? "\n%d FAILED\n" : "\nALL HOST TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
