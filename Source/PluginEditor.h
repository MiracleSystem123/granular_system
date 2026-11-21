#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
const juce::Colour bg { 0xffffffff }, ink { 0xff0b0b0b }, mid { 0xff8a8a86 }, faint { 0xffececea }, grey { 0xffdcdcd8 },
                   blue { 0xff1638ff }, green { 0xff00a650 };
juce::Font mono (float size, bool bold = false);
int textW (const juce::Font&, const juce::String&);
}

class GSLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GSLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui::mono (11); }
    juce::Font getPopupMenuFont() override { return ui::mono (11.5f); }
    juce::Font getLabelFont (juce::Label&) override { return ui::mono (10); }
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int, int, int, int, bool, int, int, bool, bool) override;
};

// Rotary knob: name above, value below, right-click for MIDI Learn.
class Knob : public juce::Component, private juce::Timer
{
public:
    Knob (GSProcessor&, const juce::String& paramId, const juce::String& title);
    void resized() override;
    void setModulated (bool m);
    struct Slider : juce::Slider { std::function<void()> onRightClick; void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; } juce::Slider::mouseDown (e); } };
    Slider slider;
private:
    void timerCallback() override;
    GSProcessor& proc; juce::String id, title; juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    bool modulated = false;
};

// Row of buttons for a choice/bool parameter.
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (GSProcessor&, const juce::String& paramId, const juce::String& title, int labelWidth = 80);
    void resized() override;
    int preferredWidth() const;
private:
    void refresh (float v);
    juce::Label label; juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> att; int labelW;
};

class LabeledCombo : public juce::Component
{
public:
    LabeledCombo (GSProcessor&, const juce::String& paramId, const juce::String& title, int labelWidth = 60);
    void resized() override;
    juce::ComboBox box;
private:
    juce::Label label; std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att; int labelW;
};

class WaveView : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit WaveView (GSProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropping = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropping = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    static constexpr int strip = 24;
private:
    void timerCallback() override;
    int markerAt (float x) const;
    void setPos (float x);
    GSProcessor& proc; std::vector<float> peaks; int peaksVer = -1, peaksW = 0;
    int dragMarker = -1, hover = -1; bool posDrag = false, dropping = false;
};

class WindowPreview : public juce::Component { public: explicit WindowPreview (GSProcessor& p) : proc (p) {} void paint (juce::Graphics&) override; private: GSProcessor& proc; };
class EnvPreview    : public juce::Component { public: explicit EnvPreview (GSProcessor& p) : proc (p) {}    void paint (juce::Graphics&) override; private: GSProcessor& proc; };

class SeqView : public juce::Component, private juce::Timer
{
public:
    explicit SeqView (GSProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    void edit (juce::Point<int>, bool first);
    void push() { proc.setSteps (steps); }
    GSProcessor& proc; std::array<gs::Step, 16> steps {};
    int lane = 1; bool dragging = false, togTo = true, inGates = false;
    juce::TextButton randomBtn { "RANDOM" }, orderBtn { "IN ORDER" }, clearBtn { "CLEAR" }, resetBtn { "RESET LANE" };
    juce::OwnedArray<juce::TextButton> tabs;
    LabeledCombo slen, div, asgA, asgB;
    Knob gate, swing;
    juce::Rectangle<int> gateArea, laneArea;
    juce::Random rng;
};

class Section : public juce::Component
{
public:
    Section (const juce::String& num, const juce::String& title) : n (num), t (title) {}
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> body() const { return getBounds().reduced (8).withTrimmedTop (22); }   // in the parent's coordinates: controls are siblings of the box
private:
    juce::String n, t;
};

class Content : public juce::Component
{
public:
    explicit Content (GSProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setModulated (int assignA, int assignB);
    void repaintPreviews() { winPrev.repaint(); envPrev.repaint(); }
    int heightFor (int width);
private:
    Knob* K (const char* id, const char* title);
    GSProcessor& proc;
    juce::OwnedArray<juce::Component> owned;
    std::map<juce::String, Knob*> knobs;
    Section seqBox { "", "SEQUENCER" }, kbBox { "", "KEYBOARD / MIDI" }, grains { "01_", "GRAINS" }, time { "02_", "TIME" }, filter { "03_", "FILTER" }, pitch { "04_", "PITCH" },
            modfx { "05_", "MOD_FX" }, delay { "06_", "DELAY" }, reverb { "07_", "REVERB" }, lfo1 { "08_", "LFO_1" }, lfo2 { "09_", "LFO_2" }, output { "10_", "OUTPUT" }, info { "11_", "INSTRUCTIONS" };
    SeqView seq;
    juce::MidiKeyboardComponent keyboard;
    juce::TextButton octDown { "OCT -" }, octUp { "OCT +" }, holdBtn { "HOLD" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> holdAtt;
    WindowPreview winPrev; EnvPreview envPrev;
    juce::OwnedArray<juce::TextButton> winBtns;
    juce::TextEditor infoText;
    int lastA = -1, lastB = -1;
};

class GSEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GSEditor (GSProcessor&);
    ~GSEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyStateChanged (bool isKeyDown) override;
private:
    void timerCallback() override;
    GSProcessor& proc;
    GSLookAndFeel lnf;
    juce::TextButton runBtn { "RUN" }, testBtn { "TEST" }, droneBtn { "DRONE" }, mangleBtn { "MANGLE" }, loadBtn { "LOAD..." },
                     slice8 { "SLICE 8" }, slice16 { "SLICE 16" }, detectBtn { "DETECT HITS" }, clearBtn { "CLEAR" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAtt, droneAtt;
    std::unique_ptr<Knob> tpitch, dpitch, tempo;
    juce::ComboBox presetBox, demoBox;
    juce::Label sampleName, markersLbl;
    WaveView wave;
    Content content;
    juce::Viewport viewport;
    std::unique_ptr<juce::FileChooser> chooser;
    bool tDown = false;
};
