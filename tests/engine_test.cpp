// Builds and runs without JUCE: g++ -std=c++17 -O2 tests/engine_test.cpp Source/Engine.cpp -o engine_test
#include "../Source/Engine.h"
#include <cmath>
#include <cstdio>
#include <string>

using namespace goa;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; if (fails < 20) std::printf ("FAIL: %s (%s)\n", msg, ctx.c_str()); } } while (0)

static void checkPattern (const Pattern& p, const std::string& ctx)
{
    CHECK ((int) p.steps.size() == p.spb * p.bars, "step count");
    CHECK ((int) p.form.size() == p.bars, "form size");
    bool any = false;
    for (auto& s : p.steps)
        if (s.on)
        {
            any = true;
            CHECK (s.note >= 0 && s.note <= 127, "note range");
            CHECK (s.vel >= 1 && s.vel <= 127, "velocity range");
            CHECK (std::isfinite (s.len) && s.len > 0, "gate");
        }
    CHECK (any, "has notes");
}

int main()
{
    std::vector<std::string> scales;
    for (auto& g : scaleGroups()) for (auto& s : g.scales) scales.push_back (s.first);
    scales.push_back ("Custom");
    Rng meta (12345);
    long runs = 0;
    for (int mode = 0; mode < 5; ++mode)
        for (int hs : { 0, mode == 4 ? 3 : 0 })
        for (auto& style : styleNames())
            for (int bars : { 1, 2, 4, 8, 16 })
                for (int spb : { 16, 24, 8 })
                    for (double w : { 0.0, 0.6, 1.0 })
                        for (auto& sc : scales)
                        {
                            Params p;
                            p.mode = (Mode) mode; p.hookShape = mode == 4 ? (hs ? (int) (meta.next() * 5) + 1 : 0) : 0; p.style = style; p.bars = bars; p.spb = spb; p.weird = w; p.scale = sc;
                            p.density = meta.next(); p.movement = meta.next(); p.reg = meta.next();
                            if (meta.chance (.2)) p.chords = "";
                            const std::string ctx = std::to_string (mode) + " " + style + " " + sc + " bars=" + std::to_string (bars);
                            const Pattern g = generate (p, meta.u32());
                            checkPattern (g, ctx);
                            for (double a : { .1, .3, .7, 1.0 }) checkPattern (mutate (g, a, p, meta.u32()), ctx + " mut");
                            Pattern back;
                            CHECK (deserialize (serialize (g), back), "deserialize");
                            CHECK (serialize (back) == serialize (g), "serialize roundtrip");
                            const auto mid = toMidiFile (g, 145, "t");
                            Dna d; std::string err;
                            if (analyzeMidiFile (mid, d, err))
                            {
                                Rng r2 (meta.u32());
                                const auto m = dnaMotif (d, r2);
                                Params q = p; q.mode = Mode::Melody; q.spb = 16; q.root = d.root; q.scale = d.scale; q.bars = d.bars;
                                checkPattern (generate (q, meta.u32(), &m, &d.style, &d.rhythms), ctx + " dna");
                            }
                            else CHECK (err.find ("at least 4") != std::string::npos, err.c_str());
                            ++runs;
                        }
    // diverse sets for every mode and riff type
    for (int mode = 0; mode < 5; ++mode)
        for (int arch = 0; arch <= 6; ++arch)
        {
            const std::string ctx = "set mode " + std::to_string (mode) + " arch " + std::to_string (arch);
            Params p; p.mode = (Mode) mode; p.archetype = arch; p.bars = 4;
            const auto set = generateSet (p, 8, meta.u32());
            CHECK (set.size() == 8, "set size");
            for (auto& g : set) checkPattern (g, ctx);
        }
    // determinism: same seed => same pattern
    const std::string ctx = "determinism";
    Params p;
    CHECK (serialize (generate (p, 42)) == serialize (generate (p, 42)), "deterministic");
    const Pattern g = generate (p, 7);
    std::printf ("motif:");
    for (int n : motifToNotes (g)) std::printf (" %s", noteName (n).c_str());
    std::printf ("\nmelody:");
    for (auto& s : g.steps) std::printf (" %s", s.on ? noteName (s.note).c_str() : "-");
    Params a; a.mode = Mode::Acid; a.style = "Acid";
    const Pattern ac = generate (a, 99);
    std::printf ("\nacid:");
    for (int i = 0; i < 16; ++i) { auto& s = ac.steps[(size_t) i]; std::printf (" %s%s%s", s.on ? noteName (s.note).c_str() : "-", s.acc ? "!" : "", s.slide ? "~" : ""); }
    std::printf ("\n%ld runs, %d failures\n", runs, fails);
    return fails == 0 ? 0 : 1;
}
