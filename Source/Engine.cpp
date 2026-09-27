#include "Engine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <sstream>

namespace goa
{
const char* NOTE_NAMES[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

namespace
{
using PcSet = std::array<bool, 12>;
using BoolVec = std::vector<bool>;

inline int imod (int a, int n) { return ((a % n) + n) % n; }
inline int fdiv (int a, int n) { return (int) std::floor ((double) a / (double) n); }
inline double clampd (double v, double a, double b) { return std::max (a, std::min (b, v)); }
inline int clampi (int v, int a, int b) { return std::max (a, std::min (b, v)); }

int pickI (Rng& R, std::initializer_list<int> l)
{
    auto it = l.begin();
    std::advance (it, std::min ((int) (R.next() * l.size()), (int) l.size() - 1));
    return *it;
}
double pickD (Rng& R, std::initializer_list<double> l)
{
    auto it = l.begin();
    std::advance (it, std::min ((int) (R.next() * l.size()), (int) l.size() - 1));
    return *it;
}
template <class T> const T& pickRef (Rng& R, const std::vector<T>& v)
{
    return v[(size_t) std::min ((int) (R.next() * v.size()), (int) v.size() - 1)];
}
bool startsWith (const std::string& s, const char* p) { return s.rfind (p, 0) == 0; }

const std::vector<std::pair<std::string, StyleProfile>>& styles()
{
    static const std::vector<std::pair<std::string, StyleProfile>> s {
        { "Goa Melody", { .35, .55, .15, 4, 5, 4, .08, .15, .08, .25, .12, "roll", 0.0 } },
        { "Acid",       { .30, .40, .25, 3, 4, 5, .20, .35, .30, .40, .30, "acid", 0.0 } },
        { "Hypnotic",   { .55, .60, .35, 2, 3, 3, .10, .20, .05, .20, .10, "roll", 0.0 } },
        { "Emotional",  { .10, .75, .10, 4, 6, 5, .15, .10, .03, .10, .20, "long", 0.0 } },
        { "Dark",       { .45, .60, .20, 3, 4, 3, .12, .25, .12, .30, .15, "roll", -.25 } },
        { "Cosmic",     { .15, .30, .10, 4, 5, 7, .12, .20, .20, .20, .25, "roll", .20 } },
        { "90s Goa",    { .60, .50, .15, 4, 4, 4, .03, .05, .10, .20, .10, "roll", 0.0 } },
        { "Modern Goa", { .30, .50, .15, 3, 5, 5, .20, .40, .12, .30, .15, "sync", 0.0 } },
    };
    return s;
}
StyleProfile styleFor (const std::string& name)
{
    for (auto& s : styles())
        if (s.first == name) return s.second;
    return styles().front().second;
}

PcSet pcSetOf (const Params& p)
{
    PcSet s {};
    for (int x : scaleOf (p)) s[(size_t) imod (x + p.root, 12)] = true;
    return s;
}
int degToMidi (int deg, const std::vector<int>& sc, int base)
{
    const int n = (int) sc.size();
    return base + fdiv (deg, n) * 12 + sc[(size_t) imod (deg, n)];
}
int snapToScale (int m, const PcSet& set)
{
    if (set[(size_t) imod (m, 12)]) return m;
    for (int d = 1; d < 7; ++d)
    {
        if (set[(size_t) imod (m - d, 12)]) return m - d;
        if (set[(size_t) imod (m + d, 12)]) return m + d;
    }
    return m;
}
int scaleStep (int m, int k, const PcSet& set)
{
    int x = m, dir = k > 0 ? 1 : -1, n = std::abs (k), guard = 0;
    while (n > 0 && guard++ < 40)
    {
        x += dir;
        if (set[(size_t) imod (x, 12)]) --n;
    }
    return x;
}
int nearestChordTone (int m, const PcSet& pcs)
{
    for (int d = 0; d < 7; ++d)
    {
        if (pcs[(size_t) imod (m - d, 12)]) return m - d;
        if (pcs[(size_t) imod (m + d, 12)]) return m + d;
    }
    return m;
}
int regBase (const Params& p, const StyleProfile& st)
{
    const double r = clampd (p.reg + st.regShift, 0, 1);
    int oct = 2 + (int) std::lround (r * 3);
    if (p.mode == Mode::Acid) oct = std::max (1, oct - 1);
    return 12 * (oct + 1) + p.root;
}

bool parseChord (const std::string& tok, Chord& c)
{
    if (tok.empty()) return false;
    const char L = (char) std::toupper ((unsigned char) tok[0]);
    if (L < 'A' || L > 'G') return false;
    static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };
    int pc = base[L - 'A'];
    size_t i = 1;
    if (i < tok.size() && (tok[i] == '#' || tok[i] == 'b')) { pc += tok[i] == '#' ? 1 : -1; ++i; }
    std::string q = tok.substr (i);
    for (auto& ch : q) ch = (char) std::tolower ((unsigned char) ch);
    std::vector<int> iv;
    if (startsWith (q, "maj7")) iv = { 0, 4, 7, 11 };
    else if (startsWith (q, "m7b5")) iv = { 0, 3, 6, 10 };
    else if (startsWith (q, "dim")) iv = { 0, 3, 6 };
    else if (startsWith (q, "aug") || q == "+") iv = { 0, 4, 8 };
    else if (startsWith (q, "sus2")) iv = { 0, 2, 7 };
    else if (startsWith (q, "sus")) iv = { 0, 5, 7 };
    else if (startsWith (q, "min7") || startsWith (q, "m7")) iv = { 0, 3, 7, 10 };
    else if ((startsWith (q, "m") && ! startsWith (q, "maj")) || startsWith (q, "min")) iv = { 0, 3, 7 };
    else if (startsWith (q, "7")) iv = { 0, 4, 7, 10 };
    else iv = { 0, 4, 7 };
    c.root = imod (pc, 12);
    c.iv = iv;
    c.name = tok;
    return true;
}
std::vector<Chord> parseChords (std::string s)
{
    for (auto& ch : s)
        if ((unsigned char) ch >= 0x80 || ch == ',' || ch == '|' || ch == '>') ch = ' ';
    std::istringstream in (s);
    std::vector<Chord> out;
    std::string tok;
    while (in >> tok)
    {
        Chord c;
        if (parseChord (tok, c)) out.push_back (c);
    }
    return out;
}
Chord tonicChord (const Params& p, const std::vector<int>& sc)
{
    auto has = [&] (int x) { return std::find (sc.begin(), sc.end(), x) != sc.end(); };
    const int third = has (3) ? 3 : has (4) ? 4 : has (2) ? 2 : sc[1 % sc.size()];
    const int fifth = has (7) ? 7 : has (6) ? 6 : has (8) ? 8 : sc[sc.size() / 2];
    std::set<int> s { 0, third, fifth };
    Chord c;
    c.root = p.root;
    c.iv.assign (s.begin(), s.end());
    c.name = NOTE_NAMES[p.root];
    return c;
}
PcSet chordPcs (const Chord& ch)
{
    PcSet s {};
    for (int x : ch.iv) s[(size_t) imod (ch.root + x, 12)] = true;
    return s;
}
std::vector<int> extend7 (const Chord& ch, const PcSet& set)
{
    if (ch.iv.size() >= 4) return ch.iv;
    for (int c : { 10, 11 })
        if (set[(size_t) imod (ch.root + c, 12)])
        {
            auto v = ch.iv;
            v.push_back (c);
            return v;
        }
    return ch.iv;
}
int chordOff (const Chord& ch, const Params& p, const std::vector<int>& sc)
{
    const int rel = imod (ch.root - p.root, 12);
    int idx = -1;
    for (size_t i = 0; i < sc.size(); ++i)
        if (sc[i] == rel) idx = (int) i;
    if (idx < 0)
    {
        int bd = 99;
        for (size_t i = 0; i < sc.size(); ++i)
        {
            const int dd = std::min (imod (rel - sc[i], 12), imod (sc[i] - rel, 12));
            if (dd < bd) { bd = dd; idx = (int) i; }
        }
    }
    const int n = (int) sc.size();
    return std::abs (idx - n) < idx ? idx - n : idx;
}

// Rhythm is built from curated one-beat cells (real Goa/psy grooves), arranged b1 b2 b1 b4
// so the groove repeats inside the bar instead of being random per step.
BoolVec cellFromString (const char* c)
{
    BoolVec v;
    for (const char* p = c; *p != 0; ++p) v.push_back (*p == '1');
    return v;
}
const std::vector<const char*>& beatCells (int spbeat, int tier, const std::string& cells)
{
    static const std::vector<std::vector<const char*>> s16 { { "1000", "1010", "1001", "1010" },
                                                             { "1010", "1011", "1001", "1101", "1010" },
                                                             { "1011", "1101", "1111", "1110", "1011" },
                                                             { "1111", "1111", "1011", "1101" } };
    static const std::vector<std::vector<const char*>> sync16 { { "1000", "0010", "1001" },
                                                                { "1010", "0110", "1001", "0011" },
                                                                { "1011", "0111", "1101", "0110" },
                                                                { "1111", "0111", "1011", "1101" } };
    static const std::vector<std::vector<const char*>> acid16 { { "1010", "1001", "1000" },
                                                                { "1010", "1011", "1101", "0110" },
                                                                { "1011", "1101", "1110", "0111", "1010" },
                                                                { "1111", "1011", "1101", "1110" } };
    static const std::vector<std::vector<const char*>> t24 { { "100000", "100100" },
                                                             { "100100", "101100", "100110" },
                                                             { "110110", "101101", "111011" },
                                                             { "111111", "110111", "111111" } };
    static const std::vector<std::vector<const char*>> e8 { { "10" }, { "10", "11" }, { "11", "10", "11" }, { "11" } };
    tier = clampi (tier, 0, 3);
    if (spbeat == 6) return t24[(size_t) tier];
    if (spbeat == 2) return e8[(size_t) tier];
    if (cells == "sync") return sync16[(size_t) tier];
    if (cells == "acid") return acid16[(size_t) tier];
    return s16[(size_t) tier];
}
BoolVec makeBarRhythm (int spb, const Params& p, const StyleProfile& st, Rng& R)
{
    const int spbeat = spb / 4;
    int tier = clampi ((int) std::floor (p.density * 4.0), 0, 3);
    if (st.cells == "long") tier = std::max (0, tier - 1);
    auto cellAt = [&] {
        int t = tier;
        if (R.chance (.2)) t += R.chance (.5) ? 1 : -1;
        return cellFromString (pickRef (R, beatCells (spbeat, t, st.cells)));
    };
    const BoolVec b1 = cellAt(), b2 = cellAt();
    const BoolVec b4 = R.chance (.5) ? b2 : cellAt();
    BoolVec bar;
    for (const BoolVec* c : { &b1, &b2, &b1, &b4 }) bar.insert (bar.end(), c->begin(), c->end());
    if (std::none_of (bar.begin(), bar.end(), [] (bool b) { return b; })) bar[0] = true;
    return bar;
}
BoolVec rhythmVariant (const BoolVec& on, double amt, int spbeat, Rng& R)
{
    BoolVec r = on;
    for (size_t i = 1; i < r.size(); ++i)
        if ((int) i % spbeat != 0 && R.chance (amt)) r[i] = ! r[i];
    if (std::none_of (r.begin(), r.end(), [] (bool b) { return b; })) r[0] = true;
    return r;
}

std::vector<int> makeMotif (const Params& p, const StyleProfile& st, int L, Rng& R)
{
    const double mv = p.movement;
    int cur = pickI (R, { 0, 0, 0, 2, 4 });
    std::vector<int> m { cur };
    int lastIv = 0;
    for (int k = 1; k < L; ++k)
    {
        int iv;
        if (std::abs (lastIv) >= 3 && R.chance (.7)) iv = (lastIv > 0 ? -1 : 1) * pickI (R, { 1, 1, 2 }); // fill the gap after a leap
        else if (R.chance (st.rep * (1 - mv * .7))) iv = 0;
        else if (R.chance (.55 + .35 * (1 - mv))) iv = pickI (R, { -1, 1 });
        else
        {
            const int mx = std::max (2, 2 + (int) std::lround (mv * (st.leapMax - 1) + p.weird * 2));
            iv = R.ri (2, mx) * pickI (R, { -1, 1 });
        }
        cur = clampi (cur + iv, -3, 9);
        m.push_back (cur);
        lastIv = iv;
    }
    if (std::all_of (m.begin(), m.end(), [&] (int x) { return x == m[0]; })) m.back() += pickI (R, { 2, -1, 4 });
    return m;
}
int nearestStable (int deg, int n)
{
    // nearest degree on the tonic triad (degrees 0, 2, 4 of the scale), any octave
    int best = deg, bd = 99;
    for (int o = -2; o <= 2; ++o)
        for (int d : { 0, 2, 4 })
        {
            const int c = o * n + std::min (d, n - 1);
            if (std::abs (c - deg) < bd) { bd = std::abs (c - deg); best = c; }
        }
    return best;
}
std::vector<int> answerOf (const std::vector<int>& m, double mv, int n, Rng& R)
{
    std::vector<int> r = m;
    if (mv < .3 && R.chance (.35)) return r; // exact repeat, hypnotic
    const double t = R.next();
    if (t < .55 || r.size() < 3)
    {
        const size_t k = std::max<size_t> (1, r.size() / 2);
        for (size_t i = r.size() - k; i < r.size(); ++i) r[i] += pickI (R, { -2, -1, 1, 2 });
    }
    else if (t < .8) { const int s = pickI (R, { 1, 2, -1, -2 }); for (auto& x : r) x += s; }
    else if (t < .92 && mv > .45) { const int f = r[0]; for (auto& x : r) x = 2 * f - x; }
    else std::reverse (r.begin(), r.end());
    r.back() = nearestStable (r.back(), n); // an answer comes to rest on a chord tone
    return r;
}
std::vector<int> varyMotif (const std::vector<int>& m, Rng& R)
{
    const double r = R.next();
    std::vector<int> c = m;
    if (r < .45 || c.size() < 2) return c;
    if (r < .7) { c[(size_t) R.ri (1, (int) c.size() - 1)] += pickI (R, { -2, -1, 1, 2 }); return c; }
    if (r < .85) { std::reverse (c.begin(), c.end()); return c; }
    const int f = c[0];
    for (auto& x : c) x = 2 * f - x;
    return c;
}

std::vector<std::string> formFor (int bars)
{
    switch (bars)
    {
        case 1: return { "A" };
        case 4: return { "A", "A2", "A", "B" };
        case 8: return { "A", "A2", "A", "B", "A", "A2", "A3", "C" };
        case 16: return { "A", "A2", "A", "B", "A", "A2", "A3", "C", "A", "A2", "A", "B2", "A3", "B", "A2", "C" };
        default: return { "A", "A2" };
    }
}

struct Ev { bool on = false; int deg = 0; bool pedal = false, hold = false; };
using Bar = std::vector<Ev>;

// ---- riff archetypes: 1 pedal riff, 2 motif, 3 chord tones, 4 octave bounce, 5 scale run, 6 stabs ----
std::vector<double> archetypeWeights (const Params& p)
{
    std::vector<double> w { .25, .2, .15, .15, .15, .1 };
    const auto& s = p.style;
    if (s == "Acid") w = { .3, .15, .05, .3, .1, .1 };
    else if (s == "Hypnotic") w = { .35, .3, .1, .15, .0, .1 };
    else if (s == "Emotional") w = { .05, .45, .3, .0, .1, .1 };
    else if (s == "Dark") w = { .3, .2, .1, .25, .1, .05 };
    else if (s == "Cosmic") w = { .1, .2, .3, .15, .2, .05 };
    else if (s == "90s Goa") w = { .35, .15, .1, .2, .2, .0 };
    else if (s == "Modern Goa") w = { .15, .2, .1, .15, .1, .3 };
    if (p.mode == Mode::Acid) { w[0] += .1; w[3] += .1; w[2] *= .5; }
    if (p.mode == Mode::Chords) w[2] += .2;
    if (p.density < .35) { w[0] = 0; w[3] = 0; w[4] *= .3; w[1] += .2; w[5] += .2; }
    return w;
}
int pickWeighted (const std::vector<double>& w, Rng& R)
{
    double t = 0;
    for (double x : w) t += std::max (0.0, x);
    if (t <= 0) return 2;
    double r = R.next() * t;
    for (size_t i = 0; i < w.size(); ++i) { r -= std::max (0.0, w[i]); if (r <= 0) return (int) i + 1; }
    return (int) w.size();
}
std::vector<int> chordToneMotif (int L, Rng& R)
{
    static const std::vector<int> tones { -3, 0, 2, 4, 7, 9 }; // 5th below, root, 3rd, 5th, octave, 10th
    int idx = pickI (R, { 1, 1, 3, 2 });
    std::vector<int> m;
    int dir = R.chance (.6) ? 1 : -1;
    for (int k = 0; k < L; ++k)
    {
        m.push_back (tones[(size_t) idx]);
        if (idx + dir < 0 || idx + dir >= (int) tones.size() || R.chance (.25)) dir = -dir;
        idx = clampi (idx + dir * (R.chance (.2) ? 2 : 1), 0, (int) tones.size() - 1);
    }
    return m;
}
std::vector<int> runMotif (int L, Rng& R)
{
    const int dir = R.chance (.55) ? 1 : -1;
    const int start = dir > 0 ? pickI (R, { 0, 0, 2, -3 }) : pickI (R, { 4, 7, 4, 2 });
    std::vector<int> m;
    for (int k = 0; k < L; ++k) m.push_back (start + dir * k);
    return m;
}
std::vector<int> stabMotif (Rng& R)
{
    static const std::vector<std::vector<int>> s { { 0, 4, 3 }, { 4, 2, 0 }, { 0, 2, 4 }, { 7, 4, 2 }, { 0, 7, 4 }, { 2, 4, 0 } };
    return pickRef (R, s);
}

std::vector<Ev> genMelodic (const Params& p, const StyleProfile& st, const std::vector<int>* motifIn, std::vector<int>& motifOut,
                            int spb, int bars, const std::vector<std::string>& form, int n, const std::vector<BoolVec>* poolIn, Rng& R,
                            int& archOut)
{
    const int spbeat = spb / 4, half = spb / 2;
    const double mv = p.movement;
    int arch = (p.archetype >= 1 && p.archetype <= 6) ? p.archetype : pickWeighted (archetypeWeights (p), R);
    if (p.density < .35 && (arch == 1 || arch == 4)) arch = 2;
    archOut = arch;
    const bool pedOn = arch == 1 || arch == 4;
    const int pedDeg = arch == 4 ? -n : (R.chance (.6) ? 0 : -n);
    const int parity = R.chance (.5) ? 0 : 1;
    Params rp = p; // rhythm needs of the archetype
    if (arch == 5) rp.density = std::max (p.density, .78);
    if (arch == 6) rp.density = std::min (p.density, .45);
    const std::vector<BoolVec>* pool = (poolIn != nullptr && spb == 16 && ! poolIn->empty()) ? poolIn : nullptr;
    const BoolVec baseR = pool != nullptr ? rhythmVariant (pickRef (R, *pool), .06, spbeat, R) : makeBarRhythm (spb, rp, st, R);

    // melodic (non-pedal) notes in the first half bar decide the motif length
    auto melIn = [&] (const BoolVec& r, int len) {
        int c = 0, on = 0;
        for (int i = 0; i < len; ++i) if (r[(size_t) i]) { if (! (pedOn && on % 2 == parity)) ++c; ++on; }
        return c;
    };
    // sparse grooves: the motif spans the whole bar and is answered in the next bar
    const int unit = melIn (baseR, half) < 3 ? spb : half;
    std::vector<int> motif;
    if (motifIn != nullptr && ! motifIn->empty()) motif = *motifIn;
    else
    {
        const int L = clampi (melIn (baseR, unit), 2, std::max (3, st.motifMax) + (unit == spb ? 2 : 0));
        if (arch == 3) motif = chordToneMotif (std::max (3, L), R);
        else if (arch == 5) motif = runMotif (clampi (L, 3, 4), R);
        else if (arch == 6) motif = stabMotif (R);
        else motif = makeMotif (p, st, L, R);
    }
    motifOut = motif;

    // the motif restarts every half bar: call in the first half, answer in the second
    auto fillBar = [&] (const BoolVec& r, const std::vector<int>& h0, const std::vector<int>& h1) {
        Bar b ((size_t) spb);
        int onIdx = 0, pedCount = 0;
        for (int h = 0; h < spb / unit; ++h)
        {
            const auto& inst = h == 0 ? h0 : h1;
            int k = 0;
            for (int i = h * unit; i < (h + 1) * unit; ++i)
            {
                if (! r[(size_t) i]) continue;
                b[(size_t) i].on = true;
                if (pedOn && onIdx % 2 == parity)
                {
                    b[(size_t) i].deg = arch == 4 ? ((pedCount++ % 2 == 0) ? pedDeg : pedDeg + n) : pedDeg; // octave bounce
                    b[(size_t) i].pedal = true;
                }
                else b[(size_t) i].deg = inst[(size_t) (k++ % (int) inst.size())];
                ++onIdx;
            }
        }
        return b;
    };
    auto shifted = [] (std::vector<int> v, int s) { for (auto& x : v) x += s; return v; };

    std::vector<int> M1;
    if (arch == 5) { M1 = motif; std::reverse (M1.begin(), M1.end()); M1.back() = nearestStable (M1.back(), n); } // run answers the other way
    else M1 = answerOf (motif, mv, n, R);
    const int seqStep = pickI (R, { 2, 3, -2, 4, 2 });
    std::map<std::string, Bar> cache;
    std::function<const Bar& (const std::string&)> get;
    std::function<Bar (const std::string&)> make = [&] (const std::string& l) -> Bar {
        if (l == "A2")
        {
            BoolVec r = baseR;
            if (R.chance (.3)) { const int i = R.ri (1, spb - 1); if (i % spbeat != 0) r[(size_t) i] = ! r[(size_t) i]; }
            const auto ans = answerOf (motif, mv, n, R);
            return unit == spb ? fillBar (r, ans, ans) : fillBar (r, motif, ans);
        }
        if (l == "A3") { const int s = pickI (R, { n, 2, -2 }); return fillBar (baseR, shifted (motif, s), shifted (M1, s)); }
        if (l == "B" || l == "B2")
        {
            const BoolVec r = pool != nullptr && pool->size() > 1 ? rhythmVariant (pickRef (R, *pool), .06, spbeat, R)
                                                                 : (R.chance (.5) ? baseR : makeBarRhythm (spb, p, st, R));
            const auto T = shifted (motif, seqStep);
            return fillBar (r, T, answerOf (T, mv, n, R));
        }
        if (l == "C")
        {
            Bar c = get ("A2");
            int last = -1;
            for (int i = 0; i < spb; ++i) if (c[(size_t) i].on && ! c[(size_t) i].pedal) last = i;
            if (last >= 0)
            {
                int t = last;
                for (int i = spb - spbeat * 2; i < spb; ++i) if (c[(size_t) i].on && ! c[(size_t) i].pedal) { t = i; break; }
                c[(size_t) t].deg = pickI (R, { 0, 0, n, 4 });
                c[(size_t) t].hold = true;
                for (int i = t + 1; i < spb; ++i) c[(size_t) i] = Ev {};
            }
            return c;
        }
        return fillBar (baseR, motif, M1);
    };
    get = [&] (const std::string& l) -> const Bar& {
        auto it = cache.find (l);
        if (it != cache.end()) return it->second;
        Bar b = make (l);
        return cache.emplace (l, std::move (b)).first->second;
    };

    std::vector<Ev> out;
    for (int b = 0; b < bars; ++b)
    {
        const Bar& c = get (form[(size_t) b]);
        out.insert (out.end(), c.begin(), c.end());
    }
    return out;
}

std::vector<Step> genArp (const Params& p, const StyleProfile& st, int spb, int bars, const std::vector<std::string>& form,
                          const std::vector<Chord>& chords, const std::function<int (int)>& chordIdx, const PcSet& set,
                          int base, Rng& R)
{
    static const std::vector<std::pair<std::string, std::vector<int>>> PAT {
        { "goa", { 0, 1, 2, 3, 2, 1 } }, { "up", { 0, 1, 2, 3, 4, 5 } }, { "down", { 5, 4, 3, 2, 1, 0 } },
        { "updown", { 0, 1, 2, 3, 4, 3, 2, 1 } }, { "pedal", { 0, 1, 0, 2, 0, 3, 0, 2 } },
        { "broken", { 0, 2, 1, 3, 2, 4, 3, 5 } }, { "leap", { 0, 3, 1, 4, 2, 5 } }, { "rolling", { 0, 1, 2, 1, 3, 2, 4, 3 } }
    };
    auto patOf = [&] (const std::string& k) -> const std::vector<int>& {
        for (auto& e : PAT) if (e.first == k) return e.second;
        return PAT[0].second;
    };
    static const std::vector<std::string> calm { "goa", "goa", "up", "pedal", "updown" }, wild { "broken", "leap", "rolling", "goa", "updown" };
    std::string key = R.chance (1 - p.movement) ? pickRef (R, calm) : pickRef (R, wild);
    if ((p.style == "90s Goa" || p.style == "Hypnotic") && R.chance (.5)) key = R.chance (.5) ? "pedal" : "goa";
    const std::vector<int> basePat = patOf (key);
    const int spbeat = spb / 4;
    BoolVec baseR;
    for (int i = 0; i < spb; ++i) baseR.push_back (i % spbeat == 0 || R.chance (clampd (.3 + .68 * p.density - st.rests * .4, .05, .98)));

    struct Cell { std::vector<int> pat; BoolVec r; };
    std::map<std::string, Cell> cache;
    std::function<const Cell& (const std::string&)> get = [&] (const std::string& l) -> const Cell& {
        auto it = cache.find (l);
        if (it != cache.end()) return it->second;
        Cell c { basePat, baseR };
        if (l == "A2") { const int i = R.ri (0, (int) c.pat.size() - 2); std::swap (c.pat[(size_t) i], c.pat[(size_t) i + 1]); }
        else if (l == "A3") { const int s = R.chance (.5) ? 1 : 3; for (auto& x : c.pat) x += s; }
        else if (l == "B")
        {
            std::vector<std::string> others;
            for (auto& e : PAT) if (e.first != key) others.push_back (e.first);
            c.pat = patOf (pickRef (R, others));
            c.r = rhythmVariant (baseR, .1 + p.movement * .2, spbeat, R);
        }
        else if (l == "B2") { c = get ("B"); std::reverse (c.pat.begin(), c.pat.end()); }
        else if (l == "C") c.pat.back() = 0;
        return cache.emplace (l, std::move (c)).first->second;
    };

    std::map<int, std::vector<int>> toneCache;
    auto tonesFor = [&] (int ci) -> const std::vector<int>& {
        auto it = toneCache.find (ci);
        if (it != toneCache.end()) return it->second;
        const Chord& ch = chords[(size_t) ci];
        const auto ivs = extend7 (ch, set);
        int s = base - imod (base - ch.root, 12);
        if (base - s > 6) s += 12;
        std::vector<int> t;
        for (int o = 0; o < 3; ++o) for (int iv : ivs) t.push_back (s + 12 * o + iv);
        return toneCache.emplace (ci, t).first->second;
    };

    std::vector<Step> steps;
    int lastCh = -1, k = 0;
    for (int b = 0; b < bars; ++b)
    {
        const Cell& cell = get (form[(size_t) b]);
        k = 0;
        for (int i = 0; i < spb; ++i)
        {
            const int ci = chordIdx (b * spb + i);
            if (ci != lastCh) { lastCh = ci; k = 0; }
            if (! cell.r[(size_t) i]) { steps.push_back (Step {}); continue; }
            const auto& tn = tonesFor (ci);
            const int t = cell.pat[(size_t) (k % (int) cell.pat.size())];
            ++k;
            Step s;
            s.on = true;
            s.note = tn[(size_t) clampi (t, 0, (int) tn.size() - 1)];
            if (R.chance (st.octJump * p.movement * .6)) s.note += 12;
            s.pedal = t == 0;
            steps.push_back (s);
        }
    }
    return steps;
}

std::vector<int> onsets (const std::vector<Step>& st)
{
    std::vector<int> o;
    for (int i = 0; i < (int) st.size(); ++i) if (st[(size_t) i].on) o.push_back (i);
    return o;
}

// ================= Hook generator =================
// A hook is a short, memorable phrase: a recognisable rhythm that repeats, one clear peak,
// a first ending that stays open (5th) and a final ending that comes home (root).
struct HNote { int pos, dur; }; // in 16ths inside one bar

const std::vector<HNote>& hookRhythm (int shape, int tier)
{
    static const std::vector<std::vector<std::vector<HNote>>> T {
        { { { 0, 4 }, { 6, 2 }, { 8, 8 } },
          { { 0, 2 }, { 2, 2 }, { 4, 3 }, { 7, 1 }, { 8, 6 }, { 14, 2 } },
          { { 0, 2 }, { 2, 1 }, { 3, 2 }, { 6, 2 }, { 8, 2 }, { 10, 2 }, { 12, 3 }, { 15, 1 } } },
        { { { 0, 3 }, { 3, 3 }, { 6, 6 } },
          { { 0, 3 }, { 3, 3 }, { 6, 2 }, { 8, 3 }, { 11, 3 }, { 14, 2 } },
          { { 0, 3 }, { 3, 3 }, { 6, 1 }, { 7, 1 }, { 8, 3 }, { 11, 3 }, { 14, 1 }, { 15, 1 } } },
        { { { 0, 4 }, { 4, 4 }, { 8, 4 }, { 12, 4 } },
          { { 2, 2 }, { 4, 2 }, { 6, 2 }, { 8, 2 }, { 10, 2 }, { 12, 4 } },
          { { 0, 1 }, { 1, 1 }, { 2, 2 }, { 4, 1 }, { 5, 1 }, { 6, 2 }, { 8, 1 }, { 9, 1 }, { 10, 2 }, { 12, 4 } } },
        { { { 0, 4 }, { 4, 2 }, { 8, 4 }, { 12, 4 } },
          { { 0, 3 }, { 3, 1 }, { 4, 2 }, { 6, 2 }, { 8, 3 }, { 11, 1 }, { 12, 4 } },
          { { 0, 2 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 6, 2 }, { 8, 2 }, { 10, 1 }, { 11, 1 }, { 12, 2 }, { 14, 2 } } },
        { { { 0, 8 }, { 8, 8 } },
          { { 0, 6 }, { 6, 2 }, { 8, 8 } },
          { { 0, 4 }, { 4, 4 }, { 8, 4 }, { 12, 2 }, { 14, 2 } } },
    };
    return T[(size_t) clampi (shape - 1, 0, 4)][(size_t) clampi (tier, 0, 2)];
}

int autoHookShape (const std::string& style, Rng& R)
{
    // weights for: call, stab, climb, octave, anthem
    std::vector<double> w { .25, .25, .2, .2, .1 };
    if (style == "Emotional") w = { .4, .05, .2, .0, .35 };
    else if (style == "Acid") w = { .1, .4, .15, .35, .0 };
    else if (style == "Hypnotic") w = { .3, .4, .0, .3, .0 };
    else if (style == "Dark") w = { .3, .3, .0, .4, .0 };
    else if (style == "Cosmic") w = { .15, .0, .35, .25, .25 };
    else if (style == "90s Goa") w = { .3, .3, .0, .4, .0 };
    else if (style == "Modern Goa") w = { .1, .5, .2, .2, .0 };
    double t = 0;
    for (double x : w) t += x;
    double r = R.next() * t;
    for (size_t i = 0; i < w.size(); ++i) { r -= w[i]; if (r <= 0) return (int) i + 1; }
    return 1;
}

std::vector<int> hookDegrees (int shape, int N, int bar, bool closed, int n, double mv, Rng& R,
                              int start, int peak, const std::vector<int>& cell, const std::vector<int>& tail, int climbStep)
{
    std::vector<int> d ((size_t) std::max (0, N));
    if (N <= 0) return d;
    const int end = closed ? 0 : pickI (R, { 4, 4, 2 });
    auto t = [&] (int i) { return N > 1 ? (double) i / (N - 1) : 1.0; };
    switch (shape)
    {
        case 1: // call & response: climb to the single peak, answer falls home
            for (int i = 0; i < N; ++i)
            {
                if (bar == 0) d[(size_t) i] = start + (int) std::lround ((peak - start) * std::pow (t (i), 0.8));
                else d[(size_t) i] = (peak - 1) + (int) std::lround ((0 - (peak - 1)) * t (i)); // fall home; open ending set below
            }
            if (N > 3 && R.chance (.35 + .3 * mv)) { const int k = R.ri (1, N - 2); d[(size_t) k] += pickI (R, { -1, 1 }); }
            break;
        case 2: // 3-3-2 stab: the same 3-note cell, answer resolves
            for (int i = 0; i < N; ++i) d[(size_t) i] = cell[(size_t) (i % 3)];
            if (bar == 1 && N >= 4 && mv > .4) d[(size_t) 3] += 1;
            break;
        case 3: // climb: a 2-note cell one step higher each time, then a fall
        {
            const int g = 2;
            const int groups0 = std::max (1, (N + g - 1) / g);
            if (bar == 0)
                for (int i = 0; i < N; ++i) d[(size_t) i] = start + i / g + (i % g == 1 ? climbStep : 0);
            else
            {
                const int top = start + groups0 + climbStep;
                for (int i = 0; i < N; ++i) d[(size_t) i] = std::max (0, top - i);
            }
            break;
        }
        case 4: // octave jump: root / octave on the beat, a tail in between
        {
            int strong = 0, k = 0;
            for (int i = 0; i < N; ++i)
            {
                if (i == 0 || i % 2 == 0) d[(size_t) i] = (strong++ % 2 == 0) ? start : start + n;
                else d[(size_t) i] = tail[(size_t) (k++ % (int) tail.size())];
            }
            break;
        }
        default: // anthem: long notes, wide intervals
            if (bar == 0)
                for (int i = 0; i < N; ++i) d[(size_t) i] = i == 0 ? start : cell[0] - (i - 1);
            else
                for (int i = 0; i < N; ++i) d[(size_t) i] = peak + (int) std::lround ((0 - peak) * t (i));
            break;
    }
    if (bar == 1 || N == 1) d.back() = end;
    return d;
}

std::vector<Step> genHook (const Params& p, int& shapeOut, std::vector<int>& motifOut, std::vector<std::string>& formOut,
                           int spb, int bars, int n, const std::vector<int>& sc, int base, Rng& R)
{
    const int shape = (p.hookShape >= 1 && p.hookShape <= 5) ? p.hookShape : autoHookShape (p.style, R);
    shapeOut = shape;
    const int tier = p.density < .35 ? 0 : p.density < .7 ? 1 : 2;
    const double mv = p.movement;

    const int start = shape == 4 ? pickI (R, { 0, 0, -n }) : pickI (R, { 0, 0, 2, -3 });
    const int peak = clampi (start + 4 + (int) std::lround (mv * 3) + R.ri (0, 1), start + 3, start + 9);
    static const std::vector<std::vector<int>> cells { { 4, 3, 2 }, { 0, 2, 4 }, { 2, 4, 3 }, { 4, 2, 0 }, { 0, 4, 2 }, { 2, 1, 0 } };
    std::vector<int> cell = pickRef (R, cells);
    if (shape == 5) cell = { start + pickI (R, { 3, 4, 4 }) };
    const std::vector<int> tail = pickRef (R, std::vector<std::vector<int>> { { n - 1, n - 3 }, { n + 2, n }, { 4, 3 }, { n - 1, 4 }, { n - 2, n - 3 } });
    const int climbStep = pickI (R, { 1, 2 });

    const auto& rhythm = hookRhythm (shape, tier);
    std::vector<HNote> closedRhythm = rhythm;
    if (closedRhythm.size() > 2 && R.chance (.5)) // final ending: merge the last two notes into one long note
    {
        closedRhythm[closedRhythm.size() - 2].dur += closedRhythm.back().dur;
        closedRhythm.pop_back();
    }
    const auto D0 = hookDegrees (shape, (int) rhythm.size(), 0, bars == 1, n, mv, R, start, peak, cell, tail, climbStep);
    const auto Dopen = hookDegrees (shape, (int) rhythm.size(), 1, false, n, mv, R, start, peak, cell, tail, climbStep);
    const auto Dclosed = hookDegrees (shape, (int) closedRhythm.size(), 1, true, n, mv, R, start, peak, cell, tail, climbStep);
    motifOut.assign (D0.begin(), D0.begin() + std::min<size_t> (6, D0.size()));

    // bar labels: H1 = statement, H2 = open answer, H3 = closed answer
    formOut.clear();
    for (int b = 0; b < bars; ++b)
    {
        if (bars == 1) formOut.push_back ("H1");
        else if (b % 2 == 0) formOut.push_back ("H1");
        else formOut.push_back ((b % 4 == 3 || bars == 2) ? "H3" : "H2");
    }

    const PcSet tonic = chordPcs (tonicChord (p, sc));
    std::vector<Step> steps ((size_t) (spb * bars));
    for (int b = 0; b < bars; ++b)
    {
        const std::string& l = formOut[(size_t) b];
        const auto& rh = l == "H3" ? closedRhythm : rhythm;
        const auto& dg = l == "H1" ? D0 : l == "H2" ? Dopen : Dclosed;
        for (size_t k = 0; k < rh.size(); ++k)
        {
            const int idx = b * spb + rh[k].pos * spb / 16;
            Step& s = steps[(size_t) idx];
            if (s.on) continue;
            s.on = true;
            s.note = degToMidi (dg[k], sc, base);
            const bool downbeat = rh[k].pos == 0, last = k + 1 == rh.size();
            if (shape != 4 && (downbeat || (last && l != "H1")) && R.chance (.85)) s.note = nearestChordTone (s.note, tonic);
            s.vel = rh[k].pos % 4 == 0 ? 108 : 92;
            s.len = std::max (1, rh[k].dur * spb / 16) * (shape == 2 ? .8 : .92);
        }
    }
    // accent the single peak of every phrase
    for (int b = 0; b + 1 < bars || b == 0; b += 2)
    {
        int best = -1;
        for (int i = b * spb; i < std::min (bars, b + 2) * spb; ++i)
            if (steps[(size_t) i].on && (best < 0 || steps[(size_t) i].note > steps[(size_t) best].note)) best = i;
        if (best >= 0) { steps[(size_t) best].acc = true; steps[(size_t) best].vel = 127; }
        if (bars == 1) break;
    }
    return steps;
}

void weirdify (std::vector<Step>& steps, const Params& p, const PcSet& set, int spbeat, Rng& R)
{
    const double w = p.weird;
    if (w < .12) return; // below 12 the melody stays strictly in the scale
    const auto ons = onsets (steps);
    const int N = (int) ons.size();
    if (N < 3) return;
    for (int j = 0; j < N; ++j)
    {
        Step& s = steps[(size_t) ons[(size_t) j]];
        if (s.pedal && w < .8) continue;
        const bool strong = ons[(size_t) j] % spbeat == 0;
        const Step& next = steps[(size_t) ons[(size_t) ((j + 1) % N)]];
        const Step& prev = steps[(size_t) ons[(size_t) ((j - 1 + N) % N)]];
        const double r = R.next();
        if (! strong && r < w * .25) s.note = next.note + pickI (R, { -1, 1 });
        else if (! strong && w > .35 && r < w * .38)
        {
            int m = prev.note + pickI (R, { 6, -6, 11, -11, 13, -13, 10 });
            if (w < .55) m = snapToScale (m, set);
            s.note = m;
        }
        else if (w > .5 && r < w * .44) s.note += pickI (R, { 12, -12 });
    }
    if (w > .72)
    {
        std::vector<int> pcs;
        for (int pc = 0; pc < 12; ++pc) if (set[(size_t) pc] && pc != p.root) pcs.push_back (pc);
        if (pcs.empty()) return;
        const int target = pickRef (R, pcs), shift = pickI (R, { -1, 1 });
        for (int i : ons)
            if (i % spbeat != 0 && imod (steps[(size_t) i].note, 12) == target && R.chance ((w - .7) * 2.5))
                steps[(size_t) i].note += shift;
    }
}

void articulate (std::vector<Step>& steps, const Params& p, const StyleProfile& st, int spbeat, int base, Rng& R, int arch)
{
    // One articulation for the whole pattern: same gate, same accent groove. That is what makes it sound played, not random.
    const int total = (int) steps.size();
    const bool acid = p.mode == Mode::Acid;
    const double gate = acid ? pickD (R, { .45, .55, .7 }) : st.cells == "long" ? .92 : pickD (R, { .5, .62, .75, .85 });
    const bool holds = arch == 6 || st.cells == "long";
    const int accMode = R.ri (0, 2); // 0 = downbeats, 1 = off-beat 8ths, 2 = every third note
    const auto ons = onsets (steps);
    for (size_t j = 0; j < ons.size(); ++j)
    {
        const int i = ons[j];
        Step& s = steps[(size_t) i];
        const int nj = ons[(j + 1) % ons.size()];
        int gap = imod (nj - i, total);
        if (gap == 0) gap = total;
        const bool accentPos = accMode == 0 ? i % spbeat == 0 : accMode == 1 ? (spbeat > 1 && i % spbeat == spbeat / 2) : j % 3 == 0;
        if (acid)
        {
            if (! s.pedal && R.chance (st.octJump * (.4 + p.movement))) s.note += s.note > base + 6 ? -12 : 12;
            s.acc = R.chance (clampd (st.acc * 1.25 * (accentPos ? 1.5 : .7), 0, .9));
            s.slide = gap == 1 && R.chance (st.slide * 1.4 * (.5 + p.movement));
            s.vel = s.acc ? 127 : 92;
        }
        else
        {
            s.acc = false;
            s.slide = gap == 1 && ! s.pedal && R.chance (st.slide * .25 * (.5 + p.movement));
            s.vel = accentPos ? 112 : (s.pedal ? 84 : 96);
        }
        if (s.slide) s.len = 1.1;
        else if (s.hold || holds) s.len = std::max (.5, std::min (gap, 4) * .9);
        else s.len = (gap >= 2 ? std::min (gap, 2) : 1) * gate;
    }
}

Pattern mutateLocal (const Pattern& src, double amt, Rng& R)
{
    Pattern pat = src;
    auto& st = pat.steps;
    const PcSet set = pcSetOf (pat.params);
    const int spb = pat.spb;
    const bool small = amt <= .15;
    const int nOps = small ? R.ri (1, 2) : std::max (3, (int) std::lround (amt * 9 * std::sqrt ((double) pat.bars)));
    enum Op { NOTE, OCT, ACC, SLIDE, SWAP, ADD, DEL, SHIFT, HOLD };
    const std::vector<Op> smallOps { NOTE, NOTE, OCT, ACC, SLIDE, SWAP };
    const std::vector<Op> bigOps { NOTE, NOTE, NOTE, NOTE, ADD, DEL, SHIFT, HOLD, ACC, SLIDE, OCT, SWAP };
    const auto& ops = small ? smallOps : bigOps;
    int lo = 127, hi = 0;
    for (auto& s : st) if (s.on) { lo = std::min (lo, s.note); hi = std::max (hi, s.note); }
    if (lo > hi) return pat;
    lo -= 3; hi += 3;
    auto onsetNear = [&] (int b, int off) {
        for (int d = 0; d < spb; ++d)
            for (int o : { off + d, off - d })
                if (o >= 0 && o < spb && st[(size_t) (b * spb + o)].on) return b * spb + o;
        return -1;
    };
    for (int k = 0; k < nOps; ++k)
    {
        const Op op = pickRef (R, ops);
        const int b = R.ri (0, pat.bars - 1);
        std::vector<int> targets { b };
        for (int j = 0; j < pat.bars; ++j)
            if (j != b && pat.form[(size_t) j] == pat.form[(size_t) b] && R.chance (.85)) targets.push_back (j);
        const int off = R.ri (0, spb - 1), delta = pickI (R, { -2, -1, 1, 2 }), dir = pickI (R, { 12, -12 }), sh = pickI (R, { -1, 1 });
        for (int tb : targets)
        {
            const int i = onsetNear (tb, off);
            if (i < 0) continue;
            Step& s = st[(size_t) i];
            switch (op)
            {
                case NOTE: { const int m = scaleStep (s.note, delta, set); if (m >= lo && m <= hi) s.note = m; break; }
                case OCT: { const int m = s.note + dir; if (m >= lo - 9 && m <= hi + 9) s.note = m; break; }
                case ACC: s.acc = ! s.acc; s.vel = s.acc ? 127 : 95; break;
                case SLIDE: s.slide = ! s.slide; s.len = s.slide ? 1.1 : .6; break;
                case HOLD: s.len = std::max (s.len, 1.9); break;
                case SWAP: {
                    int j = i + 1;
                    while (j < (int) st.size() && ! st[(size_t) j].on) ++j;
                    if (j < (int) st.size() && j / spb == tb) std::swap (s.note, st[(size_t) j].note);
                    break;
                }
                case DEL: if (i % spb != 0) st[(size_t) i] = Step {}; break;
                case ADD: {
                    const int e = tb * spb + off;
                    if (! st[(size_t) e].on)
                    {
                        int pn = s.note;
                        for (int j = e - 1; j >= tb * spb; --j) if (st[(size_t) j].on) { pn = st[(size_t) j].note; break; }
                        Step ns;
                        ns.on = true; ns.note = scaleStep (pn, sh, set); ns.vel = 92; ns.len = .6;
                        st[(size_t) e] = ns;
                    }
                    break;
                }
                case SHIFT: {
                    const int t = i + sh;
                    if (i % spb != 0 && t >= tb * spb && t < (tb + 1) * spb && ! st[(size_t) t].on)
                    {
                        st[(size_t) t] = st[(size_t) i];
                        st[(size_t) i] = Step {};
                    }
                    break;
                }
            }
        }
    }
    fixSlides (st);
    return pat;
}

// ---- MIDI reading ----
struct Reader
{
    const std::vector<uint8_t>& b;
    size_t p = 0;
    bool ok = true;
    uint8_t u8() { if (p >= b.size()) { ok = false; return 0; } return b[p++]; }
    uint32_t u16() { const uint32_t a = u8(); return (a << 8) | u8(); }
    uint32_t u32() { const uint32_t a = u16(); return (a << 16) | u16(); }
    uint32_t vl() { uint32_t v = 0; uint8_t c; int g = 0; do { c = u8(); v = (v << 7) | (c & 0x7f); } while ((c & 0x80) && ok && ++g < 5); return v; }
    std::string str (int n) { std::string s; for (int i = 0; i < n; ++i) s += (char) u8(); return s; }
};
struct RawNote { int pitch; long start, end; };
} // namespace

// ================= public =================
double Rng::next()
{
    a += 0x6D2B79F5u;
    uint32_t t = a;
    t = (t ^ (t >> 15)) * (t | 1u);
    t = (t + ((t ^ (t >> 7)) * (t | 61u))) ^ t;
    return (double) (t ^ (t >> 14)) / 4294967296.0;
}
int Rng::ri (int lo, int hi) { return lo + std::min (hi - lo, (int) std::floor (next() * (hi - lo + 1))); }

std::string noteName (int m) { return std::string (NOTE_NAMES[imod (m, 12)]) + std::to_string (fdiv (m, 12) - 1); }

const std::vector<ScaleGroup>& scaleGroups()
{
    static const std::vector<ScaleGroup> g {
        { "Goa essentials", { { "Minor", { 0, 2, 3, 5, 7, 8, 10 } }, { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 } },
                              { "Melodic Minor", { 0, 2, 3, 5, 7, 9, 11 } }, { "Phrygian", { 0, 1, 3, 5, 7, 8, 10 } },
                              { "Phrygian Dominant", { 0, 1, 4, 5, 7, 8, 10 } }, { "Dorian", { 0, 2, 3, 5, 7, 9, 10 } },
                              { "Hungarian Minor", { 0, 2, 3, 6, 7, 8, 11 } }, { "Double Harmonic", { 0, 1, 4, 5, 7, 8, 11 } } } },
        { "Eastern and exotic", { { "Neapolitan Minor", { 0, 1, 3, 5, 7, 8, 11 } }, { "Neapolitan Major", { 0, 1, 3, 5, 7, 9, 11 } },
                                  { "Ukrainian Dorian", { 0, 2, 3, 6, 7, 9, 10 } }, { "Persian", { 0, 1, 4, 5, 6, 8, 11 } },
                                  { "Oriental", { 0, 1, 4, 5, 6, 9, 10 } }, { "Raga Todi", { 0, 1, 3, 6, 7, 8, 11 } },
                                  { "Raga Marwa", { 0, 1, 4, 6, 7, 9, 11 } }, { "Raga Purvi", { 0, 1, 4, 6, 7, 8, 11 } },
                                  { "Enigmatic", { 0, 1, 4, 6, 8, 10, 11 } }, { "Spanish 8-Tone", { 0, 1, 3, 4, 5, 6, 8, 10 } } } },
        { "Modes", { { "Major", { 0, 2, 4, 5, 7, 9, 11 } }, { "Lydian", { 0, 2, 4, 6, 7, 9, 11 } },
                     { "Mixolydian", { 0, 2, 4, 5, 7, 9, 10 } }, { "Mixolydian b6", { 0, 2, 4, 5, 7, 8, 10 } },
                     { "Locrian", { 0, 1, 3, 5, 6, 8, 10 } }, { "Super Locrian", { 0, 1, 3, 4, 6, 8, 10 } } } },
        { "Pentatonic and symmetric", { { "Minor Pentatonic", { 0, 3, 5, 7, 10 } }, { "Blues", { 0, 3, 5, 6, 7, 10 } },
                                        { "Hirajoshi", { 0, 2, 3, 7, 8 } }, { "In Sen", { 0, 1, 5, 7, 10 } },
                                        { "Egyptian", { 0, 2, 5, 7, 10 } }, { "Pelog", { 0, 1, 3, 7, 8 } },
                                        { "Whole Tone", { 0, 2, 4, 6, 8, 10 } }, { "Diminished (half-whole)", { 0, 1, 3, 4, 6, 7, 9, 10 } },
                                        { "Augmented", { 0, 3, 4, 7, 8, 11 } }, { "Prometheus", { 0, 2, 4, 6, 9, 10 } } } },
    };
    return g;
}
const std::vector<int>* findScale (const std::string& name)
{
    for (auto& g : scaleGroups())
        for (auto& s : g.scales)
            if (s.first == name) return &s.second;
    return nullptr;
}
std::vector<std::string> riffTypeNames()
{
    return { "Auto (mix)", "Pedal riff", "Motif", "Chord tones", "Octave bounce", "Scale run", "Stabs" };
}
std::vector<std::string> hookShapeNames()
{
    return { "Auto", "Call & response", "3-3-2 stab", "Climb", "Octave jump", "Anthem" };
}
std::vector<std::string> styleNames()
{
    std::vector<std::string> v;
    for (auto& s : styles()) v.push_back (s.first);
    return v;
}
std::vector<int> scaleOf (const Params& p)
{
    if (p.scale == "Custom")
    {
        std::vector<int> s;
        for (int i = 0; i < 12; ++i) if (p.custom[(size_t) i] || i == 0) s.push_back (i);
        return s;
    }
    if (auto* s = findScale (p.scale)) return *s;
    return *findScale ("Minor");
}
std::string scaleNotesText (const Params& p)
{
    std::string t;
    for (int x : scaleOf (p)) { if (! t.empty()) t += "  "; t += NOTE_NAMES[imod (x + p.root, 12)]; }
    return t;
}
std::vector<int> motifToNotes (const Pattern& pat)
{
    const auto sc = scaleOf (pat.params);
    std::vector<int> o;
    for (int d : pat.motif) o.push_back (degToMidi (d, sc, 60 + pat.params.root));
    return o;
}

void fixSlides (std::vector<Step>& st)
{
    const int total = (int) st.size();
    for (int i = 0; i < total; ++i)
    {
        Step& s = st[(size_t) i];
        if (! s.on) continue;
        const Step& nx = st[(size_t) ((i + 1) % total)];
        if (s.slide && ! nx.on) { s.slide = false; s.len = .6; }
        if (s.slide) s.len = std::max (s.len, 1.05);
        else
        {
            int gap = 1;
            while (gap < total && ! st[(size_t) ((i + gap) % total)].on) ++gap;
            s.len = std::min (s.len, gap * .95);
        }
    }
}

Pattern generate (const Params& pIn, uint32_t seed, const std::vector<int>* motifIn, const StyleProfile* dna,
                  const std::vector<std::vector<bool>>* pool)
{
    Rng R (seed);
    Params p = pIn;
    if (p.spb != 8 && p.spb != 24) p.spb = 16;
    if (p.bars != 1 && p.bars != 2 && p.bars != 4 && p.bars != 8 && p.bars != 16) p.bars = 2;
    StyleProfile st = styleFor (p.style);
    if (dna != nullptr) { const double rs = st.regShift; st = *dna; st.regShift = rs; }
    if (p.mode == Mode::Acid)
    {
        st.slide = std::max (st.slide, .25); st.acc = std::max (st.acc, .3); st.octJump = std::max (st.octJump, .2);
        if (st.cells != "sync") st.cells = "acid";
        st.motifMin = 3; st.motifMax = 4; st.pedal = std::min (st.pedal, .3);
    }
    const auto sc = scaleOf (p);
    const PcSet set = pcSetOf (p);
    const int n = (int) sc.size(), spb = p.spb, bars = p.bars, total = spb * bars, spbeat = spb / 4;
    const int base = regBase (p, st);
    const auto form = formFor (bars);

    std::vector<Chord> chords;
    const bool useCh = p.mode == Mode::Arp || p.mode == Mode::Chords;
    if (useCh)
    {
        chords = parseChords (p.chords);
        if (chords.empty()) chords.push_back (tonicChord (p, sc));
    }
    std::function<int (int)> chordIdx = [&] (int i) {
        const int nc = (int) chords.size();
        const double per = nc >= bars ? (double) total / nc : (double) spb;
        return imod ((int) std::floor (i / per), nc);
    };

    std::vector<int> motif;
    std::vector<Step> steps;
    std::vector<std::string> formUsed = form;
    Params pUsed = p;
    const bool hook = p.mode == Mode::Hook;
    if (hook)
    {
        int shape = 1;
        steps = genHook (p, shape, motif, formUsed, spb, bars, n, sc, base, R);
        pUsed.hookShape = shape; // so a 70% mutation keeps the same hook shape
    }
    else if (p.mode == Mode::Arp) steps = genArp (p, st, spb, bars, form, chords, chordIdx, set, base, R);
    else
    {
        int arch = 2;
        const auto evs = genMelodic (p, st, motifIn, motif, spb, bars, form, n, pool, R, arch);
        pUsed.archetype = arch; // a 70% mutation keeps the riff type
        const PcSet tonic = chordPcs (tonicChord (p, sc));
        steps.resize ((size_t) total);
        for (int i = 0; i < total; ++i)
        {
            const Ev& e = evs[(size_t) i];
            if (! e.on) continue;
            Step s;
            s.on = true; s.pedal = e.pedal; s.hold = e.hold;
            if (useCh)
            {
                const Chord& ch = chords[(size_t) chordIdx (i)];
                const PcSet pcs = chordPcs (ch);
                if (s.pedal) { const int m = degToMidi (e.deg, sc, base); s.note = m - imod (m - ch.root, 12); }
                else
                {
                    s.note = degToMidi (e.deg + chordOff (ch, p, sc), sc, base);
                    const bool strong = i % spbeat == 0 || s.hold;
                    const int pc = imod (s.note, 12);
                    const bool avoid = ! pcs[(size_t) pc] && pcs[(size_t) imod (pc - 1, 12)];
                    if (strong || (avoid && R.chance (.7))) s.note = nearestChordTone (s.note, pcs);
                }
            }
            else
            {
                s.note = degToMidi (e.deg, sc, base);
                const bool strong = i % spbeat == 0 || s.hold;
                if (strong && ! s.pedal && R.chance (.9 - .4 * p.movement)) s.note = nearestChordTone (s.note, tonic);
            }
            steps[(size_t) i] = s;
        }
    }
    if (hook)
    {
        Params softer = p;
        softer.weird *= .6; // hooks stay recognisable
        weirdify (steps, softer, set, spbeat, R);
    }
    else
    {
        weirdify (steps, p, set, spbeat, R);
        articulate (steps, p, st, spbeat, base, R, pUsed.archetype);
        // bars with the same role in the phrase repeat exactly (same notes, accents, gates)
        for (int b = 1; b < bars; ++b)
            for (int b0 = 0; b0 < b; ++b0)
                if (formUsed[(size_t) b0] == formUsed[(size_t) b])
                {
                    std::copy (steps.begin() + b0 * spb, steps.begin() + (b0 + 1) * spb, steps.begin() + b * spb);
                    break;
                }
    }
    const int lo = base - (p.mode == Mode::Acid ? 12 : 14), hi = base + (p.mode == Mode::Acid ? 19 : 26);
    for (auto& s : steps)
    {
        if (! s.on) continue;
        while (s.note < lo) s.note += 12;
        while (s.note > hi) s.note -= 12;
        s.note = clampi (s.note, 0, 127);
        s.hold = false;
    }
    fixSlides (steps);

    Pattern out;
    out.steps = std::move (steps);
    out.spb = spb; out.bars = bars; out.mode = p.mode; out.form = formUsed;
    if (useCh) for (auto& c : chords) out.chordNames.push_back (c.name);
    out.motif = motif; out.seed = seed; out.params = pUsed;
    if (dna != nullptr) { out.hasDnaStyle = true; out.dnaStyle = *dna; }
    if (pool != nullptr) out.rhythmPool = *pool;
    return out;
}

double musicality (const Pattern& pat)
{
    std::vector<int> mel;
    for (auto& s : pat.steps) if (s.on && ! s.pedal) mel.push_back (s.note);
    if (mel.size() < 3) return -10.0;
    double step = 0, leap = 0, sum = 0;
    int lo = 127, hi = 0;
    double harsh = 0;
    for (size_t i = 1; i < mel.size(); ++i)
    {
        const int a = std::abs (mel[i] - mel[i - 1]);
        sum += a;
        if (a >= 1 && a <= 4) ++step;
        if (a > 7) ++leap;
        if (a == 6 || a == 10 || a == 11 || a == 13 || a == 14) ++harsh; // tritone, 7ths, 9ths
    }
    for (int m : mel) { lo = std::min (lo, m); hi = std::max (hi, m); }
    const double N = (double) (mel.size() - 1);
    std::set<int> distinct (mel.begin(), mel.end());
    const double calm = 1.0 - pat.params.weird; // weirdness is allowed to break these rules
    double score = step / N * 2.0 - leap / N * 3.0 * calm - std::max (0, hi - lo - 14) * .15 * calm - std::abs (sum / N - 2.8) * .3;
    score -= harsh / N * 2.5 * calm;
    if (! pat.steps.empty() && pat.steps[0].on) score += .2;
    if (distinct.size() <= 2) score -= 1.0;
    if (distinct.size() >= 3 && distinct.size() <= 7) score += .5;
    if (pat.mode == Mode::Hook)
    {
        int top = 0;
        for (int m : mel) if (m == hi) ++top;
        const int reps = std::max (1, pat.bars / 2);
        if (top <= reps) score += .6;                                   // one clear peak per phrase
        if (((mel.back() - pat.params.root) % 12 + 12) % 12 == 0) score += .8; // ends at home
        if (hi - lo >= 7 && hi - lo <= 14) score += .5;                 // a real contour, still singable
    }
    return score;
}

Pattern generateBest (const Params& p, uint32_t seed, int tries, const std::vector<int>* motif, const StyleProfile* dna,
                      const std::vector<std::vector<bool>>* pool)
{
    Rng R (seed);
    if (p.mode == Mode::Arp) tries = 1;
    if (p.mode == Mode::Hook) tries = std::max (tries, 10);
    std::vector<std::pair<double, Pattern>> cands;
    for (int t = 0; t < std::max (1, tries); ++t)
    {
        Pattern c = generate (p, R.u32(), motif, dna, pool);
        const double sc = musicality (c);
        cands.emplace_back (sc, std::move (c));
    }
    std::stable_sort (cands.begin(), cands.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
    const double r = R.next();
    const size_t pickIdx = std::min (cands.size() - 1, (size_t) (r < .55 ? 0 : r < .85 ? 1 : 2));
    return std::move (cands[pickIdx].second);
}

double similarity (const Pattern& a, const Pattern& b)
{
    if (a.spb != b.spb || a.steps.empty() || b.steps.empty()) return .2;
    const int spb = a.spb;
    int inter = 0, uni = 0;
    for (int i = 0; i < spb; ++i)
    {
        const bool x = a.steps[(size_t) i].on, y = b.steps[(size_t) i].on;
        inter += (x && y) ? 1 : 0;
        uni += (x || y) ? 1 : 0;
    }
    const double rj = uni > 0 ? (double) inter / uni : 1.0;
    auto mel = [spb] (const Pattern& p) {
        std::vector<int> m;
        for (int i = 0; i < std::min ((int) p.steps.size(), spb * 2); ++i)
            if (p.steps[(size_t) i].on && ! p.steps[(size_t) i].pedal) m.push_back (p.steps[(size_t) i].note);
        return m;
    };
    const auto ma = mel (a), mb = mel (b);
    const size_t L = std::min (ma.size(), mb.size());
    if (L < 2) return rj;
    int ivEq = 0, pcEq = 0;
    for (size_t i = 0; i < L; ++i)
    {
        if (imod (ma[i], 12) == imod (mb[i], 12)) ++pcEq;
        if (i > 0 && (ma[i] - ma[i - 1]) == (mb[i] - mb[i - 1])) ++ivEq;
    }
    return .45 * rj + .35 * ivEq / (double) (L - 1) + .2 * pcEq / (double) L;
}

std::vector<Pattern> generateSet (const Params& p, int count, uint32_t seed, const std::vector<const Pattern*>& existing)
{
    Rng R (seed);
    std::vector<Pattern> out;
    if (count <= 0) return out;
    out.reserve ((size_t) count);
    // plan riff types so the ideas differ: no type more than twice
    std::vector<int> plan;
    const bool usesArch = p.mode == Mode::Melody || p.mode == Mode::Acid || p.mode == Mode::Chords;
    if (usesArch && p.archetype == 0)
    {
        const auto w = archetypeWeights (p);
        std::array<int, 7> used {};
        for (int i = 0; i < count; ++i)
        {
            auto ww = w;
            for (size_t k = 0; k < ww.size(); ++k) if (used[k + 1] >= 2) ww[k] = 0;
            const int a = pickWeighted (ww, R);
            ++used[(size_t) a];
            plan.push_back (a);
        }
    }
    std::vector<const Pattern*> ref = existing;
    for (int i = 0; i < count; ++i)
    {
        Params q = p;
        if (! plan.empty()) q.archetype = plan[(size_t) i];
        Pattern best;
        double bestSim = 9;
        for (int attempt = 0; attempt < 5; ++attempt)
        {
            Pattern c = generateBest (q, R.u32());
            double sim = 0;
            for (auto* r : ref) sim = std::max (sim, similarity (c, *r));
            if (sim < bestSim) { bestSim = sim; best = std::move (c); }
            if (bestSim < .55) break; // different enough from everything else in the set
        }
        out.push_back (std::move (best));
        ref.push_back (&out.back());
    }
    return out;
}

Pattern mutate (const Pattern& src, double amt, const Params& current, uint32_t seed)
{
    Rng R (seed);
    if (amt >= 1.0) return generateBest (current, R.u32());
    if (amt >= .7)
    {
        const auto m = varyMotif (src.motif, R);
        return generateBest (src.params, R.u32(), 4, &m, src.hasDnaStyle ? &src.dnaStyle : nullptr,
                         src.rhythmPool.empty() ? nullptr : &src.rhythmPool);
    }
    return mutateLocal (src, amt, R);
}

static void vlqPush (std::vector<uint8_t>& o, uint32_t n)
{
    uint8_t buf[5];
    int k = 0;
    buf[k++] = n & 0x7f;
    while ((n >>= 7) > 0) buf[k++] = (uint8_t) ((n & 0x7f) | 0x80);
    while (k > 0) o.push_back (buf[--k]);
}

std::vector<uint8_t> toMidiFile (const Pattern& pat, double bpm, const std::string& name)
{
    const int tpq = 480;
    const double tps = tpq * 4.0 / pat.spb;
    const auto& st = pat.steps;
    const int total = (int) st.size();
    struct E { long t; int on, n, v; };
    std::vector<E> ev;
    for (int i = 0; i < total; ++i)
    {
        if (! st[(size_t) i].on) continue;
        int j = i;
        while (st[(size_t) j].slide && j + 1 < total && st[(size_t) j + 1].on && st[(size_t) j + 1].note == st[(size_t) j].note) ++j;
        const long start = std::lround (i * tps);
        long end = std::min (std::lround ((j + st[(size_t) j].len) * tps), std::lround (total * tps));
        if (end <= start) end = start + 10;
        ev.push_back ({ start, 1, st[(size_t) i].note, clampi (st[(size_t) i].vel, 1, 127) });
        ev.push_back ({ end, 0, st[(size_t) i].note, 64 });
        i = j;
    }
    std::stable_sort (ev.begin(), ev.end(), [] (const E& a, const E& b) { return a.t != b.t ? a.t < b.t : a.on < b.on; });
    std::vector<uint8_t> trk;
    trk.insert (trk.end(), { 0, 0xFF, 0x03 });
    vlqPush (trk, (uint32_t) name.size());
    trk.insert (trk.end(), name.begin(), name.end());
    const uint32_t us = (uint32_t) std::lround (60000000.0 / std::max (20.0, bpm));
    trk.insert (trk.end(), { 0, 0xFF, 0x51, 3, (uint8_t) ((us >> 16) & 255), (uint8_t) ((us >> 8) & 255), (uint8_t) (us & 255) });
    trk.insert (trk.end(), { 0, 0xFF, 0x58, 4, 4, 2, 24, 8 });
    long last = 0;
    for (auto& e : ev)
    {
        vlqPush (trk, (uint32_t) (e.t - last));
        trk.push_back ((uint8_t) (e.on ? 0x90 : 0x80));
        trk.push_back ((uint8_t) (e.n & 127));
        trk.push_back ((uint8_t) e.v);
        last = e.t;
    }
    vlqPush (trk, (uint32_t) std::max (0L, std::lround (total * tps) - last));
    trk.insert (trk.end(), { 0xFF, 0x2F, 0 });
    std::vector<uint8_t> out { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, (uint8_t) (tpq >> 8), (uint8_t) (tpq & 255), 'M', 'T', 'r', 'k' };
    const uint32_t L = (uint32_t) trk.size();
    out.insert (out.end(), { (uint8_t) (L >> 24), (uint8_t) (L >> 16), (uint8_t) (L >> 8), (uint8_t) L });
    out.insert (out.end(), trk.begin(), trk.end());
    return out;
}

bool analyzeMidiFile (const std::vector<uint8_t>& bytes, Dna& d, std::string& error)
{
    Reader r { bytes };
    if (bytes.size() < 14 || r.str (4) != "MThd") { error = "That file is not a standard MIDI file."; return false; }
    const uint32_t hl = r.u32();
    r.u16();
    const uint32_t ntr = r.u16(), div = r.u16();
    r.p = 8 + hl;
    if (div & 0x8000) { error = "SMPTE-timed MIDI files are not supported. Export with bars and beats."; return false; }
    if (div == 0) { error = "The MIDI file has no timing information."; return false; }
    std::vector<RawNote> best;
    for (uint32_t t = 0; t < ntr && r.p + 8 <= bytes.size() && r.ok; ++t)
    {
        const std::string id = r.str (4);
        const uint32_t len = r.u32();
        const size_t end = std::min ((size_t) r.p + len, bytes.size());
        if (id != "MTrk") { r.p = end; continue; }
        long tick = 0;
        uint8_t rs = 0;
        std::map<int, std::vector<long>> on;
        std::vector<RawNote> notes;
        while (r.p < end && r.ok)
        {
            tick += (long) r.vl();
            uint8_t s = r.p < bytes.size() ? bytes[r.p] : 0;
            if (s < 0x80) s = rs;
            else { ++r.p; if (s < 0xF0) rs = s; }
            if (s == 0xFF) { r.u8(); const uint32_t l = r.vl(); r.p += l; continue; }
            if (s == 0xF0 || s == 0xF7) { const uint32_t l = r.vl(); r.p += l; continue; }
            if (s < 0x80) break; // corrupt stream
            const int hi = s & 0xF0;
            const int a = r.u8();
            const int c = (hi == 0xC0 || hi == 0xD0) ? 0 : r.u8();
            if (hi == 0x90 && c > 0) on[a].push_back (tick);
            else if (hi == 0x80 || (hi == 0x90 && c == 0))
            {
                auto& q = on[a];
                if (! q.empty()) { notes.push_back ({ a, q.front(), tick }); q.erase (q.begin()); }
            }
        }
        r.p = end;
        if (notes.size() > best.size()) best = notes;
    }
    if (best.size() < 4) { error = "The file needs at least 4 notes to read its DNA."; return false; }

    const double q = div / 4.0;
    struct N { int pitch, dur; };
    std::map<long, N> by;
    for (auto& nt : best)
    {
        const long s = std::lround (nt.start / q);
        const int dur = std::max (1, (int) std::lround ((nt.end - nt.start) / q));
        auto it = by.find (s);
        if (it == by.end() || nt.pitch > it->second.pitch) by[s] = { nt.pitch, dur };
    }
    const long first = (long) std::floor (by.begin()->first / 16.0) * 16;
    struct S { long s; int pitch, dur; };
    std::vector<S> seq;
    for (auto& e : by) seq.push_back ({ e.first - first, e.second.pitch, e.second.dur });
    if (seq.size() < 4) { error = "The file needs at least 4 separate note onsets to read its DNA."; return false; }
    const long lastEnd = seq.back().s + seq.back().dur;
    const int rawBars = std::max (1, (int) std::ceil (lastEnd / 16.0));
    int bars = 16;
    for (int b : { 1, 2, 4, 8, 16 }) if (b >= rawBars) { bars = b; break; }

    std::array<double, 12> hist {};
    double tw = 0;
    for (auto& x : seq) { hist[(size_t) (x.pitch % 12)] += x.dur; tw += x.dur; }
    double bestScore = -1e18;
    int bestRoot = 0;
    std::string bestScale = "Minor";
    for (int root = 0; root < 12; ++root)
        for (auto& g : scaleGroups())
            for (auto& sc : g.scales)
            {
                if (sc.second.size() != 7) continue;
                PcSet set {};
                for (int x : sc.second) set[(size_t) ((x + root) % 12)] = true;
                double score = 0;
                for (int pc = 0; pc < 12; ++pc) score += set[(size_t) pc] ? hist[(size_t) pc] : -1.5 * hist[(size_t) pc];
                score += hist[(size_t) root] * .6;
                if (seq.front().pitch % 12 == root) score += tw * .06;
                if (seq.back().pitch % 12 == root) score += tw * .04;
                if (sc.first == "Minor") score += tw * .005;
                if (score > bestScore) { bestScore = score; bestRoot = root; bestScale = sc.first; }
            }

    std::vector<int> pitches, ivs;
    for (auto& x : seq) pitches.push_back (x.pitch);
    for (size_t i = 1; i < pitches.size(); ++i) ivs.push_back (pitches[i] - pitches[i - 1]);
    const double N = (double) std::max<size_t> (1, ivs.size());
    double sumAbs = 0, stepC = 0, octC = 0, zeroC = 0;
    for (int iv : ivs)
    {
        const int a = std::abs (iv);
        sumAbs += a;
        if (a >= 1 && a <= 2) ++stepC;
        if (a >= 12) ++octC;
        if (a == 0) ++zeroC;
    }
    std::map<int, int> counts;
    int top = 0;
    for (int m : pitches) top = std::max (top, ++counts[m]);
    int syncC = 0;
    for (auto& x : seq) if (x.s % 2 == 1 && by.find (x.s + first - 1) == by.end()) ++syncC;
    const auto& sc = *findScale (bestScale);
    PcSet set {};
    for (int x : sc) set[(size_t) ((x + bestRoot) % 12)] = true;
    int chrom = 0;
    double mean = 0;
    int minP = 127;
    for (int m : pitches) { if (! set[(size_t) (m % 12)]) ++chrom; mean += m; minP = std::min (minP, m); }
    mean /= (double) pitches.size();
    const int nsc = (int) sc.size();
    const int ref = bestRoot + 12 * fdiv (minP - bestRoot, 12);
    d.degSeq.clear();
    for (int m : pitches)
    {
        const int s = snapToScale (m, set), rel = s - ref;
        int idx = 0;
        for (int i = 0; i < nsc; ++i) if (sc[(size_t) i] == imod (rel, 12)) idx = i;
        d.degSeq.push_back (fdiv (rel, 12) * nsc + idx);
    }
    std::vector<int> dAbs;
    for (size_t i = 1; i < d.degSeq.size(); ++i) dAbs.push_back (std::abs (d.degSeq[i] - d.degSeq[i - 1]));
    std::sort (dAbs.begin(), dAbs.end());
    const int p90 = dAbs.empty() ? 3 : dAbs[(size_t) std::floor (dAbs.size() * .9)];
    d.rhythms.clear();
    for (int b = 0; b < rawBars && b < 16; ++b)
    {
        std::vector<bool> rr (16, false);
        bool any = false;
        for (auto& x : seq) if (x.s >= b * 16 && x.s < b * 16 + 16) { rr[(size_t) (x.s - b * 16)] = true; any = true; }
        if (any) d.rhythms.push_back (rr);
    }
    const double density = (double) seq.size() / (rawBars * 16.0);
    d.root = bestRoot; d.scale = bestScale; d.bars = bars; d.noteCount = (int) seq.size();
    d.rawDensity = density;
    d.density = clampd ((density - .12) / .75, 0, 1);
    d.avgAbs = sumAbs / N;
    d.movement = clampd ((d.avgAbs - 1) / 5, 0, 1);
    d.reg = clampd ((mean - 43) / 36, 0, 1);
    d.stepwise = stepC / N;
    d.sync = (double) syncC / (double) seq.size();
    d.pedal = (double) top / (double) pitches.size();
    d.octJump = octC / N;
    d.chromatic = (double) chrom / (double) pitches.size();
    d.style = StyleProfile { clampd ((d.pedal - .12) * 2, 0, .8), clampd (d.stepwise * 1.1, .15, .9), clampd (zeroC / N, 0, .5),
                             4, 5, clampi (p90, 2, 7), 0.0, d.sync, clampd (d.octJump * 2, 0, .5), .22, .12,
                             density < .3 ? "long" : "roll", 0.0 };
    return true;
}

std::vector<int> dnaMotif (const Dna& d, Rng& R)
{
    const auto& s = d.degSeq;
    const int L = std::min ((int) s.size(), pickI (R, { 4, 4, 5 }));
    const int st = R.ri (0, std::max (0, (int) s.size() - L));
    const int start = pickI (R, { 0, 0, 2, 4 });
    std::vector<int> m;
    for (int i = 0; i < L; ++i) m.push_back (s[(size_t) (st + i)] - s[(size_t) st] + start);
    if (R.chance (.75) && m.size() > 1)
    {
        const double r = R.next();
        if (r < .4) m[(size_t) R.ri (1, (int) m.size() - 1)] += pickI (R, { -2, -1, 1, 2 });
        else if (r < .6) std::reverse (m.begin(), m.end());
        else if (r < .8) { const int f = m[0]; for (auto& x : m) x = 2 * f - x; }
        else { const int sh = pickI (R, { 1, 2, -1 }); for (auto& x : m) x += sh; }
    }
    for (auto& x : m) x = clampi (x, -6, 12);
    return m;
}

// ---- serialization: simple line format "key value" ----
static std::string oneLine (std::string s)
{
    for (auto& c : s) if (c == '\n' || c == '\r') c = ' ';
    return s;
}
std::string serializeParams (const Params& p)
{
    std::ostringstream o;
    std::string cu;
    for (int v : p.custom) cu += v ? '1' : '0';
    o << "p.mode " << (int) p.mode << "\n" << "p.root " << p.root << "\n" << "p.scale " << oneLine (p.scale) << "\n"
      << "p.custom " << cu << "\n" << "p.style " << oneLine (p.style) << "\n" << "p.density " << p.density << "\n"
      << "p.movement " << p.movement << "\n" << "p.reg " << p.reg << "\n" << "p.weird " << p.weird << "\n"
      << "p.bars " << p.bars << "\n" << "p.spb " << p.spb << "\n" << "p.chords " << oneLine (p.chords) << "\n" << "p.hook " << p.hookShape << "\n" << "p.arch " << p.archetype << "\n";
    return o.str();
}
static void applyParamLine (const std::string& k, const std::string& v, Params& p)
{
    try
    {
        if (k == "p.mode") p.mode = (Mode) clampi (std::stoi (v), 0, 4);
        else if (k == "p.root") p.root = imod (std::stoi (v), 12);
        else if (k == "p.scale") p.scale = (v == "Custom" || findScale (v) != nullptr) ? v : "Minor";
        else if (k == "p.custom") { for (size_t i = 0; i < 12 && i < v.size(); ++i) p.custom[i] = v[i] == '1'; }
        else if (k == "p.style") p.style = v;
        else if (k == "p.density") p.density = clampd (std::stod (v), 0, 1);
        else if (k == "p.movement") p.movement = clampd (std::stod (v), 0, 1);
        else if (k == "p.reg") p.reg = clampd (std::stod (v), 0, 1);
        else if (k == "p.weird") p.weird = clampd (std::stod (v), 0, 1);
        else if (k == "p.bars") p.bars = std::stoi (v);
        else if (k == "p.spb") p.spb = std::stoi (v);
        else if (k == "p.chords") p.chords = v;
        else if (k == "p.hook") p.hookShape = clampi (std::stoi (v), 0, 5);
        else if (k == "p.arch") p.archetype = clampi (std::stoi (v), 0, 6);
    }
    catch (...) {}
}
static bool splitLine (const std::string& line, std::string& k, std::string& v)
{
    const auto sp = line.find (' ');
    if (sp == std::string::npos) { k = line; v.clear(); return ! k.empty(); }
    k = line.substr (0, sp);
    v = line.substr (sp + 1);
    return true;
}
void deserializeParams (const std::string& s, Params& p)
{
    std::istringstream in (s);
    std::string line, k, v;
    while (std::getline (in, line)) if (splitLine (line, k, v)) applyParamLine (k, v, p);
}
std::string serialize (const Pattern& pat)
{
    std::ostringstream o;
    o << "GOA1\n" << serializeParams (pat.params);
    o << "spb " << pat.spb << "\nbars " << pat.bars << "\nmode " << (int) pat.mode << "\nform";
    for (auto& f : pat.form) o << " " << f;
    o << "\nchords";
    for (auto& c : pat.chordNames) o << " " << c;
    o << "\nmotif";
    for (int m : pat.motif) o << " " << m;
    o << "\nseed " << pat.seed << "\norigin " << oneLine (pat.origin) << "\n";
    if (pat.hasDnaStyle)
    {
        const auto& s = pat.dnaStyle;
        o << "dna " << s.pedal << " " << s.step << " " << s.rep << " " << s.motifMin << " " << s.motifMax << " " << s.leapMax
          << " " << s.rests << " " << s.sync << " " << s.octJump << " " << s.acc << " " << s.slide << " " << s.cells << " " << s.regShift << "\n";
    }
    o << "pool";
    for (auto& r : pat.rhythmPool) { o << " "; for (bool b : r) o << (b ? '1' : '0'); }
    o << "\nsteps";
    for (auto& s : pat.steps)
    {
        if (! s.on) o << " 0";
        else o << " 1:" << s.note << ":" << s.vel << ":" << s.len << ":" << (s.acc ? 1 : 0) << ":" << (s.slide ? 1 : 0) << ":" << (s.pedal ? 1 : 0);
    }
    o << "\n";
    return o.str();
}
bool deserialize (const std::string& str, Pattern& pat)
{
    std::istringstream in (str);
    std::string line, k, v;
    if (! std::getline (in, line) || line != "GOA1") return false;
    Pattern p;
    try
    {
        while (std::getline (in, line))
        {
            if (! splitLine (line, k, v)) continue;
            std::istringstream vs (v);
            if (startsWith (k, "p.")) applyParamLine (k, v, p.params);
            else if (k == "spb") vs >> p.spb;
            else if (k == "bars") vs >> p.bars;
            else if (k == "mode") { int m = 0; vs >> m; p.mode = (Mode) clampi (m, 0, 4); }
            else if (k == "form") { std::string t; while (vs >> t) p.form.push_back (t); }
            else if (k == "chords") { std::string t; while (vs >> t) p.chordNames.push_back (t); }
            else if (k == "motif") { int t; while (vs >> t) p.motif.push_back (t); }
            else if (k == "seed") vs >> p.seed;
            else if (k == "origin") p.origin = v;
            else if (k == "dna")
            {
                auto& s = p.dnaStyle;
                vs >> s.pedal >> s.step >> s.rep >> s.motifMin >> s.motifMax >> s.leapMax >> s.rests >> s.sync >> s.octJump >> s.acc >> s.slide >> s.cells >> s.regShift;
                p.hasDnaStyle = ! vs.fail();
            }
            else if (k == "pool") { std::string t; while (vs >> t) { std::vector<bool> r; for (char c : t) r.push_back (c == '1'); p.rhythmPool.push_back (r); } }
            else if (k == "steps")
            {
                std::string t;
                while (vs >> t)
                {
                    Step s;
                    if (t != "0")
                    {
                        std::replace (t.begin(), t.end(), ':', ' ');
                        std::istringstream ts (t);
                        int on = 0, a = 0, sl = 0, pe = 0;
                        ts >> on >> s.note >> s.vel >> s.len >> a >> sl >> pe;
                        s.on = on == 1; s.acc = a == 1; s.slide = sl == 1; s.pedal = pe == 1;
                        s.note = clampi (s.note, 0, 127); s.vel = clampi (s.vel, 1, 127);
                    }
                    p.steps.push_back (s);
                }
            }
        }
    }
    catch (...) { return false; }
    if (p.spb != 8 && p.spb != 16 && p.spb != 24) return false;
    if (p.bars < 1 || (int) p.steps.size() != p.spb * p.bars || (int) p.form.size() != p.bars) return false;
    pat = std::move (p);
    return true;
}
} // namespace goa
