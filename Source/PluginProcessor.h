#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include <map>

class GSProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    GSProcessor();
    ~GSProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "GRANULAR_SYSTEM"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---------- shared with the editor ----------
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    bool loadSampleFile (const juce::File&);
    void loadDemo (int index);
    juce::String getSampleName() const;
    int getDemoIndex() const { return demoIndex.load(); }
    std::shared_ptr<const gs::Sample> getSample() const;
    std::atomic<int> sampleVersion { 0 };

    std::vector<float> getMarkers() const;
    void setMarkers (std::vector<float>);
    void deleteMarker (int index);
    std::array<gs::Step, 16> getSteps() const;
    void setSteps (const std::array<gs::Step, 16>&);
    void detectMarkers();

    void setParam (const juce::String& id, float plainValue);   // message thread, notifies host
    float getParam (const juce::String& id) const;
    void resetScan() { scanReset.store (true); }
    void applyPreset (int index);
    void mangle();

    // MIDI learn
    void startLearn (const juce::String& paramId)
    {
        ccFifo.finishedRead (ccFifo.getNumReady());   // forget controller messages that arrived before the click
        const juce::SpinLock::ScopedLockType l (learnLock); learnTarget = paramId;
    }
    void cancelLearn() { const juce::SpinLock::ScopedLockType l (learnLock); learnTarget.clear(); }
    juce::String getLearnTarget() const { const juce::SpinLock::ScopedLockType l (learnLock); return learnTarget; }
    int getCCFor (const juce::String& paramId) const;
    void clearCC (const juce::String& paramId);

    // visualisation snapshot written by the audio thread
    static constexpr int kVizMax = 200;
    std::array<std::atomic<float>, kVizMax> vizPos, vizAmp;
    std::atomic<int> vizCount { 0 }, vizStep { -1 }, vizAssignA { 0 }, vizAssignB { 0 };
    std::atomic<float> vizBase { 0 }, vizEnv { 0 };
    std::atomic<bool> seqRunning { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void handleMidi (const juce::MidiMessage&);
    void readParams();

    gs::Engine engine;
    gs::Params params;
    std::vector<std::atomic<float>*> floatRaw, choiceRaw;
    std::atomic<float>* runRaw = nullptr; std::atomic<float>* testRaw = nullptr; std::atomic<float>* droneRaw = nullptr;
    std::atomic<float>* holdRaw = nullptr; std::atomic<float>* bendRaw = nullptr; std::atomic<float>* mwDestRaw = nullptr;

    mutable juce::SpinLock sampleLock, dataLock, nameLock, learnLock;
    std::shared_ptr<const gs::Sample> sample;
    juce::String sampleName, samplePath;
    std::atomic<int> demoIndex { 0 };
    std::vector<float> markers;
    std::array<gs::Step, 16> steps {};
    std::array<gs::Step, 16> audioSteps {};
    std::array<float, 32> audioMarkers {}; int audioMarkerCount = 0;

    juce::AudioFormatManager formatManager;
    double sr = 44100.0;
    long long lastStepIndex = -1; double internalPpq = 0; bool seqWasRunning = false;
    std::atomic<bool> scanReset { false };
    bool sustain = false; std::vector<int> sustained; std::vector<int> heldNotes;
    float modWheel = 0;

    // MIDI learn: cc -> param id (message thread owns the map; audio thread reads a snapshot)
    juce::String learnTarget;
    std::map<int, juce::String> ccMap;
    std::array<int, 128> ccToParam {};           // index into learnables, -1 = none (audio-thread copy)
    juce::StringArray learnables;
    juce::AbstractFifo ccFifo { 512 };
    std::array<std::pair<int, float>, 512> ccQueue {};   // (cc, value 0..1)
    void rebuildCCTable();

    float vizP[kVizMax] {}, vizA[kVizMax] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GSProcessor)
};
