// Goa Melody Engine - core generator. Pure C++17, no JUCE dependency, so it can be unit tested alone.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace goa
{
struct Rng
{
    uint32_t a;
    explicit Rng (uint32_t seed) : a (seed) {}
    double next();
    uint32_t u32() { return (uint32_t) (next() * 4294967296.0); }
    int ri (int lo, int hi);
    bool chance (double p) { return next() < p; }
};

enum class Mode { Melody = 0, Acid = 1, Arp = 2, Chords = 3, Hook = 4 };

struct Params
{
    Mode mode = Mode::Melody;
    int root = 7; // G
    std::string scale = "Harmonic Minor";
    std::array<int, 12> custom { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1 };
    std::string style = "Goa Melody";
    double density = 0.7, movement = 0.4, reg = 0.55, weird = 0.0;
    int bars = 2;
    int spb = 16; // steps per bar: 16 = 1/16, 24 = 1/16T, 8 = 1/8
    std::string chords = "Gm Eb F Dm";
    int hookShape = 0; // 0 = Auto, 1..5 = see hookShapeNames()
    int archetype = 0; // riff type, 0 = Auto mix, 1..6 = see riffTypeNames()
};

struct Step
{
    bool on = false;
    int note = 60, vel = 100;
    double len = 0.6; // gate, in steps (>1 = tied / sliding into the next step)
    bool acc = false, slide = false, pedal = false, hold = false;
};

struct StyleProfile
{
    double pedal, step, rep;
    int motifMin, motifMax, leapMax;
    double rests, sync, octJump, acc, slide;
    std::string cells;
    double regShift = 0.0;
};

struct Pattern
{
    std::vector<Step> steps;
    int spb = 16, bars = 2;
    Mode mode = Mode::Melody;
    std::vector<std::string> form, chordNames;
    // genome ("DNA") so a pattern can be mutated at 70% later
    std::vector<int> motif;
    uint32_t seed = 0;
    Params params;
    bool hasDnaStyle = false;
    StyleProfile dnaStyle {};
    std::vector<std::vector<bool>> rhythmPool;
    std::string origin;
};

struct Chord { int root = 0; std::vector<int> iv; std::string name; };

struct ScaleGroup { std::string name; std::vector<std::pair<std::string, std::vector<int>>> scales; };

extern const char* NOTE_NAMES[12];
std::string noteName (int midi);
const std::vector<ScaleGroup>& scaleGroups();
const std::vector<int>* findScale (const std::string& name);
std::vector<std::string> styleNames();
std::vector<std::string> hookShapeNames(); // index 0 = Auto
std::vector<std::string> riffTypeNames();  // index 0 = Auto mix
std::vector<int> scaleOf (const Params&);
std::string scaleNotesText (const Params&);
std::vector<int> motifToNotes (const Pattern&);

Pattern generate (const Params&, uint32_t seed,
                  const std::vector<int>* motif = nullptr,
                  const StyleProfile* dnaStyle = nullptr,
                  const std::vector<std::vector<bool>>* rhythmPool = nullptr);
// Generates several candidates and keeps the most singable one (steps, few wild leaps, compact range).
Pattern generateBest (const Params&, uint32_t seed, int tries = 8,
                      const std::vector<int>* motif = nullptr,
                      const StyleProfile* dnaStyle = nullptr,
                      const std::vector<std::vector<bool>>* rhythmPool = nullptr);
double musicality (const Pattern&);
double similarity (const Pattern&, const Pattern&); // 0 = unrelated, 1 = same
// Fills `count` slots with ideas that differ from each other and from `existing` (the kept ones).
std::vector<Pattern> generateSet (const Params&, int count, uint32_t seed, const std::vector<const Pattern*>& existing = {});
Pattern mutate (const Pattern& src, double amount, const Params& current, uint32_t seed);
void fixSlides (std::vector<Step>&);

std::vector<uint8_t> toMidiFile (const Pattern&, double bpm, const std::string& trackName);

struct Dna
{
    int root = 0;
    std::string scale;
    int bars = 2, noteCount = 0;
    double density = 0, movement = 0, reg = 0, stepwise = 0, sync = 0, pedal = 0, octJump = 0,
           chromatic = 0, rawDensity = 0, avgAbs = 0;
    std::vector<int> degSeq;
    std::vector<std::vector<bool>> rhythms;
    StyleProfile style {};
};
bool analyzeMidiFile (const std::vector<uint8_t>& bytes, Dna& out, std::string& error);
std::vector<int> dnaMotif (const Dna&, Rng&);

std::string serializeParams (const Params&);
void deserializeParams (const std::string&, Params&);
std::string serialize (const Pattern&);
bool deserialize (const std::string&, Pattern&);
} // namespace goa
