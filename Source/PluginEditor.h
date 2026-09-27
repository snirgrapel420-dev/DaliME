#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace GoaColours
{
const juce::Colour bg { 0xff150c2b }, panel { 0xff1b113a }, panel2 { 0xff22164a }, line { 0xff35255e }, ink { 0xfff0eaff },
    dim { 0xffa294cc }, mag { 0xffff4fd8 }, cyan { 0xff43f0ff }, uv { 0xffb173ff }, roll { 0xff0f0822 };
}

class GoaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoaLookAndFeel();
};

class SlotView : public juce::Component, public juce::SettableTooltipClient
{
public:
    SlotView (GoaProcessor& p, int index) : proc (p), idx (index) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void()> onChange;

private:
    juce::Rectangle<float> heartArea() const;
    GoaProcessor& proc;
    int idx;
};

class BigRoll : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit BigRoll (GoaProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setPlayIdx (int i) { if (i != playIdx) { playIdx = i; repaint(); } }
    std::function<void (int step, int lane, int value)> onEdit; // lane 0 = notes (value = pitch), 1 = velocity (value 1..127), 2 = accent, 3 = slide
    static constexpr int laneH = 18, lanes = 3;

private:
    GoaProcessor& proc;
    int playIdx = -1;
};

class MidiDragSource : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MidiDragSource (GoaProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    GoaProcessor& proc;
    bool dragging = false;
};

class GoaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GoaEditor (GoaProcessor&);
    ~GoaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshAll();
    void layoutRoll();
    void updateHelpTexts();
    void editStep (int step, int lane, int value);
    juce::Label& makeLabel (juce::Label&, const juce::String&, bool dimText = true);

    GoaProcessor& proc;
    GoaLookAndFeel lnf;

    // left column
    juce::Label title, modeL, modeHelp, rootL, scaleL, scaleNotes, styleL, styleHelp, chordsL, barsL, rateL;
    std::array<juce::TextButton, 5> modeBtns;
    juce::ComboBox rootBox, scaleBox, styleBox;
    std::vector<std::string> scaleIds;
    std::array<juce::TextButton, 12> customBtns;
    juce::TextEditor chordsEd;
    juce::Label hookL, hookHelp, riffL, riffHelp, bpmInfo;
    juce::ComboBox riffBox;
    juce::ComboBox hookBox;
    struct Knob { juce::Label name, value, help; juce::Slider slider; };
    std::array<Knob, 4> knobs;
    std::array<juce::TextButton, 5> barBtns;
    std::array<juce::TextButton, 3> rateBtns;

    // right column
    juce::Label gridHint;
    std::array<std::unique_ptr<SlotView>, 8> slotViews;
    juce::Label bigLetter, detailTitle, detailSub;
    BigRoll bigRoll;
    juce::Viewport rollView;
    juce::TextButton playBtn { "Play" }, genBtn { "Generate" }, mutBtn { "Mutate" }, undoBtn { "Undo" };
    std::array<juce::TextButton, 4> amtBtns;
    juce::Label amtL, amtHint;
    juce::TextButton loadDnaBtn { "Load reference MIDI..." }, similarBtn { "Generate similar" };
    MidiDragSource dragSrc;
    juce::Label midiOutL;
    juce::ComboBox midiOutBox;
    juce::StringArray midiOutItems;
    void rebuildMidiOutList();
    juce::ToggleButton previewTgl { "Built-in sound" };
    juce::Slider gainSlider;
    juce::Label dnaInfo, status;
    juce::TooltipWindow tooltips { this, 500 };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> leftPanel, detailPanel;
    int seenVersion = -1;
    bool lastCustomVisible = false, lastChordsVisible = false, lastHookVisible = false, lastRiffVisible = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoaEditor)
};
