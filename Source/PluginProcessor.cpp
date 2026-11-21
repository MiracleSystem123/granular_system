#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Samples.h"
#include "Presets.h"
#include "Format.h"

using namespace gs;

juce::AudioProcessorValueTreeState::ParameterLayout GSProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& d : floatDefs)
    {
        const FloatDef* dp = &d;
        auto toText = [dp] (float v, int) { return fmtValue (*dp, v); };
        if (d.step >= 1)
        {
            layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { d.id, 1 }, d.name, (int) d.min, (int) d.max, (int) d.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction ([dp] (int v, int) { return fmtValue (*dp, (float) v); })));
            continue;
        }
        juce::NormalisableRange<float> range;
        if (d.map == Map::Lin) range = juce::NormalisableRange<float> (d.min, d.max);
        else
            range = juce::NormalisableRange<float> (d.min, d.max,
                [dp] (float, float, float u) { return fromNorm (*dp, u); },
                [dp] (float, float, float v) { return toNorm (*dp, v); },
                [dp] (float, float, float v) { return juce::jlimit (dp->min, dp->max, v); });
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 }, d.name, range, d.def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
    }
    for (const auto& d : choiceDefs)
    {
        juce::StringArray items; for (int i = 0; i < d.count; ++i) items.add (d.items[i]);
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { d.id, 1 }, d.name, items, d.def));
    }
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "run", 1 }, "Run (no host)", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "test", 1 }, "Test", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "drone", 1 }, "Drone", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "hold", 1 }, "Hold", false));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "bendRange", 1 }, "Pitch Bend Range", 0, 24, 2));
    juce::StringArray mw; for (int i = 0; i < numAssign; ++i) mw.add (kAssignNames[i]);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mwDest", 1 }, "Mod Wheel Target", mw, 0));
    return layout;
}

GSProcessor::GSProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GRANULAR_SYSTEM", createLayout())
{
    for (const auto& d : floatDefs)  floatRaw.push_back (apvts.getRawParameterValue (d.id));
    for (const auto& d : choiceDefs) choiceRaw.push_back (apvts.getRawParameterValue (d.id));
    runRaw = apvts.getRawParameterValue ("run"); testRaw = apvts.getRawParameterValue ("test"); droneRaw = apvts.getRawParameterValue ("drone");
    holdRaw = apvts.getRawParameterValue ("hold"); bendRaw = apvts.getRawParameterValue ("bendRange"); mwDestRaw = apvts.getRawParameterValue ("mwDest");
    for (auto& v : vizPos) v.store (0);
    for (auto& v : vizAmp) v.store (0);
    for (const auto& d : floatDefs) learnables.add (d.id);
    for (const auto& d : choiceDefs) learnables.add (d.id);
    for (auto* id : { "run", "test", "drone", "hold" }) learnables.add (id);
    ccToParam.fill (-1);
    heldNotes.reserve (128); sustained.reserve (128);
    formatManager.registerBasicFormats();

    // default pattern (same as the web version)
    const int pat[][2] = { {0,0},{2,1},{4,2},{6,3},{8,0},{10,4},{11,4},{12,2},{14,5} };
    for (auto& pm : pat) { steps[(size_t) pm[0]].on = true; steps[(size_t) pm[0]].m = pm[1]; }
    steps[11].len = 0.5f; steps[11].v = 0.6f; steps[14].len = 2; steps[12].a = 0.55f; steps[14].a = 0.85f; steps[6].b = 0.5f; steps[14].b = 0.7f;
    audioSteps = steps;
    loadDemo (0);
    startTimerHz (30);
}

GSProcessor::~GSProcessor() { stopTimer(); }

bool GSProcessor::isBusesLayoutSupported (const BusesLayout& l) const { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }

void GSProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate; engine.prepare (sampleRate); lastStepIndex = -1; internalPpq = 0; seqWasRunning = false;
}

void GSProcessor::readParams()
{
    for (size_t i = 0; i < floatRaw.size(); ++i)  params.*(floatDefs[i].field) = floatRaw[i]->load();
    for (size_t i = 0; i < choiceRaw.size(); ++i) params.*(choiceDefs[i].field) = juce::roundToInt (choiceRaw[i]->load());
    // mod wheel adds to its target parameter (up to the full range)
    const int mwd = juce::roundToInt (mwDestRaw->load());
    if (mwd > 0 && modWheel > 0)
        if (const FloatDef* d = findFloat (kAssignIds[mwd]))
            params.*(d->field) = fromNorm (*d, juce::jmin (1.f, toNorm (*d, params.*(d->field)) + modWheel));
}

void GSProcessor::handleMidi (const juce::MidiMessage& msg)
{
    const bool transpose = params.kmode == KTranspose;
    const bool holdOn = holdRaw->load() > 0.5f || sustain;
    if (msg.isNoteOn())
    {
        const int n = msg.getNoteNumber();
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end()); heldNotes.push_back (n);
        if (transpose) engine.setTranspose ((float) (n - 60));
        else engine.noteOn (n, msg.getFloatVelocity());
    }
    else if (msg.isNoteOff())
    {
        const int n = msg.getNoteNumber();
        if (holdOn) { sustained.push_back (n); return; }
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end());
        if (transpose) engine.setTranspose (heldNotes.empty() ? 0.f : (float) (heldNotes.back() - 60));
        else engine.noteOff (n);
    }
    else if (msg.isSustainPedalOn()) sustain = true;
    else if (msg.isSustainPedalOff())
    {
        sustain = false;
        if (holdRaw->load() < 0.5f)
        {
            for (int n : sustained) { heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end()); engine.noteOff (n); }
            sustained.clear();
            if (transpose) engine.setTranspose (heldNotes.empty() ? 0.f : (float) (heldNotes.back() - 60));
        }
    }
    else if (msg.isPitchWheel()) engine.setBend ((float) (msg.getPitchWheelValue() - 8192) / 8192.f * bendRaw->load());
    else if (msg.isController())
    {
        const int cc = msg.getControllerNumber(); const float val = (float) msg.getControllerValue() / 127.f;
        if (cc == 1) modWheel = val;
        // MIDI learn / mapped CCs are applied on the message thread
        int s1, n1, s2, n2; ccFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) { ccQueue[(size_t) s1] = { cc, val }; ccFifo.finishedWrite (1); }
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff()) { engine.allNotesOff(); heldNotes.clear(); sustained.clear(); engine.setTranspose (0); }
}

void GSProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int N = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, N, true);
    readParams();
    // when Hold / sustain is released, let go of latched notes
    if (holdRaw->load() < 0.5f && ! sustain && ! sustained.empty())
    {
        for (int n : sustained) { heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), n), heldNotes.end()); engine.noteOff (n); }
        sustained.clear();
    }

    const juce::SpinLock::ScopedTryLockType sl (sampleLock);
    if (! sl.isLocked()) return;
    engine.setSample (sample.get());
    {
        const juce::SpinLock::ScopedTryLockType dl (dataLock);
        if (dl.isLocked())
        {
            audioSteps = steps;
            audioMarkerCount = (int) juce::jmin ((size_t) 32, markers.size());
            for (int i = 0; i < audioMarkerCount; ++i) audioMarkers[(size_t) i] = markers[(size_t) i];
        }
    }
    engine.setParams (params);
    engine.setTest (testRaw->load() > 0.5f);
    engine.setDrone (droneRaw->load() > 0.5f);
    if (scanReset.exchange (false)) engine.resetScan();

    // ---- sequencer timing: follow the host transport, or the internal clock when RUN is on ----
    struct Ev { int off; long long step; } ev[96]; int nev = 0;
    double bpm = params.bpm, ppq = 0; bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) { ppq = *q; hostPlaying = true; }
        }
    engine.hostBpm = bpm;
    const bool running = hostPlaying || runRaw->load() > 0.5f;
    seqRunning.store (running);
    const double L = 4.0 * kDivVals[juce::jlimit (0, 11, params.div)];   // step length in beats
    const double ppqPerSample = bpm / 60.0 / sr;
    const double stepSamples = L / ppqPerSample;
    if (running)
    {
        if (! hostPlaying) ppq = internalPpq;
        const double sw = params.swing * L;
        auto startOf = [L, sw] (long long k) { return (double) k * L + ((k & 1) ? sw : 0.0); };
        long long k = (long long) std::floor (ppq / L) - 1;
        while (startOf (k + 1) <= ppq) ++k;          // k = step containing ppq
        if (! seqWasRunning || k != lastStepIndex) { if (k != lastStepIndex) ev[nev++] = { 0, k }; lastStepIndex = k; }
        for (long long j = k + 1; nev < 96; ++j)
        {
            const int off = (int) std::ceil ((startOf (j) - ppq) / ppqPerSample);
            if (off >= N) break;
            ev[nev++] = { juce::jmax (0, off), j }; lastStepIndex = j;
        }
        if (! hostPlaying) internalPpq += N * ppqPerSample;
    }
    else
    {
        if (seqWasRunning) { engine.stopSequence(); vizStep.store (-1); }
        lastStepIndex = -1; internalPpq = 0;
    }
    seqWasRunning = running;

    // ---- render, splitting at MIDI and step events ----
    float* oL = buffer.getWritePointer (0); float* oR = buffer.getWritePointer (1);
    const int slen = juce::jlimit (1, 16, (int) params.slen);
    auto it = midi.cbegin(); int pos = 0, ei = 0;
    while (pos < N)
    {
        while (it != midi.cend() && (*it).samplePosition <= pos) { handleMidi ((*it).getMessage()); ++it; }
        while (ei < nev && ev[ei].off <= pos)
        {
            const int idx = (int) (((ev[ei].step % slen) + slen) % slen);
            const auto& st = audioSteps[(size_t) idx];
            const float mpos = (st.m >= 0 && st.m < audioMarkerCount) ? audioMarkers[(size_t) st.m] : -1.f;
            engine.doStep (idx, st, mpos, stepSamples);
            vizStep.store (idx);
            vizAssignA.store (st.a >= 0 ? params.assignA : 0); vizAssignB.store (st.b >= 0 ? params.assignB : 0);
            ++ei;
        }
        const int nm = it != midi.cend() ? (*it).samplePosition : N;
        const int ns = ei < nev ? ev[ei].off : N;
        const int next = juce::jlimit (pos + 1, N, juce::jmin (nm, ns));
        engine.render (oL + pos, oR + pos, next - pos);
        pos = next;
    }
    if (! running) { vizAssignA.store (0); vizAssignB.store (0); }

    const int c = engine.getViz (vizP, vizA, kVizMax);
    for (int i = 0; i < c; ++i) { vizPos[(size_t) i].store (vizP[i], std::memory_order_relaxed); vizAmp[(size_t) i].store (vizA[i], std::memory_order_relaxed); }
    vizCount.store (c); vizBase.store (engine.basePosition()); vizEnv.store (engine.envLevel());
}

// ---------- MIDI learn (message thread) ----------
void GSProcessor::timerCallback()
{
    int s1, n1, s2, n2;
    const int ready = ccFifo.getNumReady();
    if (ready <= 0) return;
    ccFifo.prepareToRead (ready, s1, n1, s2, n2);
    auto handle = [this] (int cc, float val)
    {
        juce::String target;
        { const juce::SpinLock::ScopedLockType l (learnLock); target = learnTarget; if (target.isNotEmpty()) learnTarget.clear(); }
        if (target.isNotEmpty() && (cc == 64 || cc >= 120))   // never learn the sustain pedal or channel-mode messages
        {
            const juce::SpinLock::ScopedLockType l (learnLock); learnTarget = target; return;
        }
        if (target.isNotEmpty())
        {
            for (auto itr = ccMap.begin(); itr != ccMap.end();) itr = itr->second == target ? ccMap.erase (itr) : std::next (itr);
            ccMap[cc] = target; rebuildCCTable(); return;
        }
        auto f = ccMap.find (cc);
        if (f != ccMap.end())
            if (auto* p = apvts.getParameter (f->second)) p->setValueNotifyingHost (val);
    };
    for (int i = 0; i < n1; ++i) handle (ccQueue[(size_t) (s1 + i)].first, ccQueue[(size_t) (s1 + i)].second);
    for (int i = 0; i < n2; ++i) handle (ccQueue[(size_t) (s2 + i)].first, ccQueue[(size_t) (s2 + i)].second);
    ccFifo.finishedRead (n1 + n2);
}

void GSProcessor::rebuildCCTable()
{
    std::array<int, 128> t; t.fill (-1);
    for (auto& kv : ccMap) if (kv.first >= 0 && kv.first < 128) t[(size_t) kv.first] = learnables.indexOf (kv.second);
    ccToParam = t;
}
int GSProcessor::getCCFor (const juce::String& id) const { for (auto& kv : ccMap) if (kv.second == id) return kv.first; return -1; }
void GSProcessor::clearCC (const juce::String& id)
{
    for (auto itr = ccMap.begin(); itr != ccMap.end();) itr = itr->second == id ? ccMap.erase (itr) : std::next (itr);
    rebuildCCTable();
}

// ---------- samples & markers ----------
std::shared_ptr<const Sample> GSProcessor::getSample() const { const juce::SpinLock::ScopedLockType l (sampleLock); return sample; }
juce::String GSProcessor::getSampleName() const { const juce::SpinLock::ScopedLockType l (nameLock); return sampleName; }

bool GSProcessor::loadSampleFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 16) return false;
    const int len = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 120.0));
    juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), len);
    reader->read (&buf, 0, len, 0, true, true);
    auto s = std::make_shared<Sample>();
    s->sampleRate = reader->sampleRate;
    s->L.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    const int rch = buf.getNumChannels() > 1 ? 1 : 0;
    s->R.assign (buf.getReadPointer (rch), buf.getReadPointer (rch) + len);
    auto hits = detectHits (*s);
    std::shared_ptr<const Sample> old;
    { const juce::SpinLock::ScopedLockType l (sampleLock); old = std::move (sample); sample = std::move (s); }
    { const juce::SpinLock::ScopedLockType l (nameLock); sampleName = file.getFileName().toUpperCase(); samplePath = file.getFullPathName(); }
    demoIndex.store (-1);
    setMarkers (hits);
    ++sampleVersion;
    return true;
}

void GSProcessor::loadDemo (int index)
{
    index = juce::jlimit (0, numDemos - 1, index);
    std::shared_ptr<const Sample> s = makeDemo (index), old;
    auto hits = detectHits (*s);
    { const juce::SpinLock::ScopedLockType l (sampleLock); old = std::move (sample); sample = std::move (s); }
    { const juce::SpinLock::ScopedLockType l (nameLock); sampleName = demoNames[index]; samplePath = {}; }
    demoIndex.store (index);
    setMarkers (hits);
    ++sampleVersion;
}

std::vector<float> GSProcessor::getMarkers() const { const juce::SpinLock::ScopedLockType l (dataLock); return markers; }
void GSProcessor::setMarkers (std::vector<float> m)
{
    if (m.size() > 32) m.resize (32);
    const juce::SpinLock::ScopedLockType l (dataLock);
    markers = std::move (m);
    const int n = (int) markers.size();
    for (auto& s : steps) if (s.m >= n) s.m = n > 0 ? s.m % n : -1;
}
void GSProcessor::deleteMarker (int i)
{
    const juce::SpinLock::ScopedLockType l (dataLock);
    if (i < 0 || i >= (int) markers.size()) return;
    markers.erase (markers.begin() + i);
    for (auto& s : steps) { if (s.m == i) s.m = -1; else if (s.m > i) --s.m; }
}
void GSProcessor::detectMarkers() { if (auto s = getSample()) setMarkers (detectHits (*s)); }
std::array<Step, 16> GSProcessor::getSteps() const { const juce::SpinLock::ScopedLockType l (dataLock); return steps; }
void GSProcessor::setSteps (const std::array<Step, 16>& s) { const juce::SpinLock::ScopedLockType l (dataLock); steps = s; }

// ---------- parameters, presets ----------
void GSProcessor::setParam (const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (plain)); p->endChangeGesture(); }
}
float GSProcessor::getParam (const juce::String& id) const { if (auto* v = apvts.getRawParameterValue (id)) return v->load(); return 0; }

void GSProcessor::applyPreset (int index)
{
    if (index < 0 || index >= (int) presets().size()) return;
    static const juce::StringArray keep { "pos", "tpitch", "dpitch", "bpm", "gate", "swing", "slen", "div", "assignA", "assignB", "kmode" };
    for (const auto& d : floatDefs)  if (! keep.contains (d.id)) setParam (d.id, d.def);
    for (const auto& d : choiceDefs) if (! keep.contains (d.id)) setParam (d.id, (float) d.def);
    for (auto& kv : presets()[(size_t) index].v) setParam (kv.first, kv.second);
    resetScan();
}

void GSProcessor::mangle()
{
    juce::Random r;
    auto U = [&r] { return r.nextFloat(); };
    auto R = [&r] (float a, float b) { return a + r.nextFloat() * (b - a); };
    const auto& w = windowShapes[r.nextInt (8)];
    setParam ("spray", std::pow (U(), 1.5f)); setParam ("size", 0.003f * std::pow (300.f, U() * 0.9f)); setParam ("sizeJ", U());
    setParam ("dens", 2 * std::pow (U() < 0.2f ? 1000.f : 60.f, U())); setParam ("chaos", U());
    setParam ("gatk", w.a); setParam ("grel", w.r); setParam ("gcurve", w.c);
    setParam ("glitch", U() < 0.5f ? 0 : U() * 0.7f); { const int s[] = { 4, 8, 12, 16, 24, 32 }; setParam ("shuffle", U() < 0.5f ? 0.f : (float) s[r.nextInt (6)]); }
    setParam ("rev", U() < 0.5f ? 0 : U());
    setParam ("scan", U() < 0.3f ? 0 : U() < 0.15f ? (U() < 0.5f ? -1.f : 1.f) * R (2, 8) : R (-0.6f, 1.2f));
    setParam ("pspray", U() < 0.4f ? 0 : R (0, 14)); setParam ("pmode", (float) r.nextInt (5));
    { const float t[] = { -12, -7, -5, 5, 7, 12 }; setParam ("pitch", U() < 0.6f ? 0.f : t[r.nextInt (6)]); }
    const int ft = r.nextInt (4) == 3 ? 2 : (r.nextInt (3) == 2 ? 1 : 0);
    setParam ("ftype", (float) ft);
    float cut = 150 * std::pow (100.f, R (0.2f, 1)); if (ft == 2) cut = juce::jmin (cut, 3000.f); setParam ("cut", cut);
    setParam ("res", U() * 0.9f); setParam ("drive", U() < 0.5f ? 0 : U() * 0.8f); setParam ("fenv", U() < 0.4f ? 0 : R (-0.4f, 0.9f));
    setParam ("fdec", 0.02f * std::pow (50.f, U())); setParam ("fsus", U() * 0.6f); setParam ("crush", U() < 0.75f ? 0 : U() * 0.5f);
    setParam ("fxType", (float) juce::jmax (0, r.nextInt (6) - 1)); setParam ("fxRate", 0.05f * std::pow (100.f, U())); setParam ("fxDepth", U());
    setParam ("fxFb", U() * 0.9f); setParam ("fxMix", R (0.3f, 0.8f));
    setParam ("l1d", U() < 0.5f ? 0 : U() * 0.6f); setParam ("l1t", (float) (1 + r.nextInt (NumTargets - 1))); setParam ("l1r", 0.05f * std::pow (200.f, U()));
    setParam ("l1s", (float) r.nextInt (5)); setParam ("dmix", U() < 0.5f ? 0 : U() * 0.5f); setParam ("rmix", R (0.05f, 0.5f)); setParam ("width", R (0.2f, 1));
    setParam ("pitchOn", 1);
}

// ---------- state ----------
void GSProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    for (auto* name : { "Steps", "Markers", "MidiMap" }) if (auto c = state.getChildWithName (name); c.isValid()) state.removeChild (c, nullptr);
    juce::ValueTree st ("Steps");
    for (const auto& s : getSteps())
    {
        juce::ValueTree t ("S");
        t.setProperty ("on", s.on, nullptr); t.setProperty ("v", s.v, nullptr); t.setProperty ("m", s.m, nullptr); t.setProperty ("n", s.n, nullptr);
        t.setProperty ("len", s.len, nullptr); t.setProperty ("sz", s.sz, nullptr); t.setProperty ("a", s.a, nullptr); t.setProperty ("b", s.b, nullptr);
        st.appendChild (t, nullptr);
    }
    state.appendChild (st, nullptr);
    juce::ValueTree mk ("Markers"); for (float m : getMarkers()) { juce::ValueTree t ("M"); t.setProperty ("p", m, nullptr); mk.appendChild (t, nullptr); }
    state.appendChild (mk, nullptr);
    juce::ValueTree mm ("MidiMap"); for (auto& kv : ccMap) { juce::ValueTree t ("CC"); t.setProperty ("cc", kv.first, nullptr); t.setProperty ("id", kv.second, nullptr); mm.appendChild (t, nullptr); }
    state.appendChild (mm, nullptr);
    { const juce::SpinLock::ScopedLockType l (nameLock); state.setProperty ("samplePath", samplePath, nullptr); }
    state.setProperty ("demo", demoIndex.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void GSProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    const juce::String path = tree.getProperty ("samplePath", "").toString();
    const int demo = (int) tree.getProperty ("demo", 0);
    bool loaded = false;
    if (path.isNotEmpty()) { juce::File f (path); if (f.existsAsFile()) loaded = loadSampleFile (f); }
    if (! loaded) loadDemo (demo >= 0 ? demo : 0);
    if (auto mk = tree.getChildWithName ("Markers"); mk.isValid())
    {
        std::vector<float> m; for (int i = 0; i < mk.getNumChildren(); ++i) m.push_back ((float) mk.getChild (i).getProperty ("p", 0.0f));
        setMarkers (m);
    }
    if (auto st = tree.getChildWithName ("Steps"); st.isValid())
    {
        std::array<Step, 16> ns {};
        for (int i = 0; i < juce::jmin (16, st.getNumChildren()); ++i)
        {
            auto t = st.getChild (i); auto& s = ns[(size_t) i];
            s.on = (bool) t.getProperty ("on", false); s.v = (float) t.getProperty ("v", 0.9f); s.m = (int) t.getProperty ("m", -1); s.n = (int) t.getProperty ("n", 0);
            s.len = (float) t.getProperty ("len", 1.0f); s.sz = (float) t.getProperty ("sz", 1.0f); s.a = (float) t.getProperty ("a", -1.0f); s.b = (float) t.getProperty ("b", -1.0f);
        }
        setSteps (ns);
    }
    ccMap.clear();
    if (auto mm = tree.getChildWithName ("MidiMap"); mm.isValid())
        for (int i = 0; i < mm.getNumChildren(); ++i) ccMap[(int) mm.getChild (i).getProperty ("cc", -1)] = mm.getChild (i).getProperty ("id", "").toString();
    rebuildCCTable();
    apvts.replaceState (tree);
}

juce::AudioProcessorEditor* GSProcessor::createEditor() { return new GSEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GSProcessor(); }
