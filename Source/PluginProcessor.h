#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <atomic>
#include <memory>
#include "Engine.h"

// Small mono synth so the plugin makes sound on its own (303-style for Acid, detuned lead otherwise).
struct MonoSynth
{
    void prepare (double sampleRate);
    void noteOn (int note, int velocity);
    void noteOff (int note);
    void allOff() { gate = false; current = -1; }
    void render (float* L, float* R, int n, float gain);
    bool acid = false;

private:
    double sr = 44100.0, phase1 = 0, phase2 = 0, freq = 110, target = 110, glide = 0.001;
    double amp = 0, ampTarget = 0.6, fenv = 0, ic1 = 0, ic2 = 0;
    bool gate = false, accent = false;
    int current = -1;
};

class DaliProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    DaliProcessor();
    ~DaliProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---------- model (message thread only) ----------
    using PatternPtr = std::shared_ptr<const dali::Pattern>;
    dali::Params params;
    std::array<PatternPtr, 8> slots;
    std::array<bool, 8> keep {};
    int sel = 0;
    double amount = 0.3;
    std::unique_ptr<dali::Dna> dna;
    juce::String dnaName;

    void generateAll();
    void mutateAction();
    void generateSimilar();
    bool undo();
    void select (int i);
    void toggleKeep (int i);
    void replaceSelected (dali::Pattern edited); // step edits from the editor
    void loadDna (const juce::File&);
    std::vector<uint8_t> midiFor (int slot) const;
    juce::String fileBaseFor (int slot) const;
    int keptCount() const;
    void startListening(); // audition the selected melody while the DAW is stopped

    // Direct MIDI out to a port (loopMIDI on Windows, IAC / virtual port on Mac).
    // Needed for Ableton, which does not route MIDI coming out of VST3 plugins to other tracks.
    static juce::String virtualPortName() { return "DaliME (virtual port)"; }
    juce::StringArray availableMidiOuts() const;
    void setMidiOut (const juce::String& name); // empty = off
    juce::String midiOutName;

    std::atomic<int> modelVersion { 0 }; // editor polls this to know when to refresh
    juce::String lastMessage;

    // ---------- shared with audio thread ----------
    std::atomic<bool> internalPlay { false }, previewOn { true }, followHost { true }, hostIsPlaying { false };
    // Start/Stop: `enabled` is what the user asked for; with the DAW playing it takes effect on the next bar.
    // `outputOn` is what is actually sounding (for the button).
    std::atomic<bool> enabled { true }, outputOn { true };
    std::atomic<double> currentBpm { 145.0 };
    std::atomic<float> previewGain { 0.5f };
    std::atomic<double> internalBpm { 145.0 }, hostBpm { 0.0 };
    std::atomic<int> playStep { -1 };

private:
    void publish(); // make the selected slot the one the audio thread plays
    void pushHistory();
    void timerCallback() override;
    uint32_t newSeed() { return (uint32_t) random.nextInt64(); }

    struct Snapshot { std::array<PatternPtr, 8> slots; std::array<bool, 8> keep; int sel; };
    std::vector<Snapshot> history;
    std::vector<PatternPtr> retired; // keeps old patterns alive so the audio thread never frees memory
    juce::Random random;

    PatternPtr live; // read/written with std::atomic_load / std::atomic_store

    // audio thread state
    struct Pending { double ppqOff; int note; };
    std::vector<Pending> pending;
    struct Ev { int sample; bool on; int note, vel; };
    std::vector<Ev> events;
    MonoSynth synth;
    double sampleRate = 44100.0, internalPpq = 0.0, lastEnd = 0.0;
    bool wasRunning = false, prevSlide = false, wasInternal = false, outOn = true;
    double originPpq = 0.0; // where step 0 of the melody sits (song start, or the bar where Start took effect)
    int prevNote = -1;
    void flushAll (juce::MidiBuffer&, int sample);
    void sendExternal (const juce::MidiBuffer&);
    juce::SpinLock outLock;
    std::unique_ptr<juce::MidiOutput> extOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DaliProcessor)
};
