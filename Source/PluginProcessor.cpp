#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

static const char* LETTERS = "ABCDEFGH";

// ================= MonoSynth =================
static inline double polyBlep (double t, double dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

void MonoSynth::prepare (double sampleRate)
{
    sr = sampleRate;
    glide = 1.0 - std::exp (-1.0 / (0.035 * sr));
    ic1 = ic2 = 0; amp = 0; gate = false; current = -1;
}

void MonoSynth::noteOn (int note, int velocity)
{
    const double f = 440.0 * std::pow (2.0, (note - 69) / 12.0);
    const bool legato = gate; // overlapping note = 303 slide
    target = f;
    if (! legato) freq = f;
    accent = velocity >= 120;
    ampTarget = accent ? 0.9 : 0.45 + 0.3 * velocity / 127.0;
    if (! legato || accent) fenv = 1.0;
    gate = true;
    current = note;
}

void MonoSynth::noteOff (int note)
{
    if (note == current) { gate = false; current = -1; }
}

void MonoSynth::render (float* L, float* R, int n, float gain)
{
    const double attack = 1.0 - std::exp (-1.0 / (0.002 * sr));
    const double release = 1.0 - std::exp (-1.0 / ((acid ? 0.012 : 0.06) * sr));
    const double fdecay = std::exp (-1.0 / ((acid ? (accent ? 0.07 : 0.14) : 0.18) * sr));
    const double k = 1.0 / (acid ? 7.0 : 1.4);
    const double baseCut = acid ? 260.0 : 900.0, envAmt = acid ? (accent ? 3400.0 : 1900.0) : 2800.0;
    for (int i = 0; i < n; ++i)
    {
        freq += (target - freq) * glide;
        amp += ((gate ? ampTarget : 0.0) - amp) * (gate ? attack : release);
        fenv *= fdecay;
        if (amp < 1.0e-5 && ! gate) { amp = 0; continue; }
        const double fc = std::min (baseCut + envAmt * fenv, sr * 0.45);
        const double dt = freq / sr;
        phase1 += dt; if (phase1 >= 1.0) phase1 -= 1.0;
        double s = 2.0 * phase1 - 1.0 - polyBlep (phase1, dt);
        if (! acid)
        {
            const double dt2 = dt * 1.0064;
            phase2 += dt2; if (phase2 >= 1.0) phase2 -= 1.0;
            s = 0.6 * s + 0.6 * (2.0 * phase2 - 1.0 - polyBlep (phase2, dt2));
        }
        const double g = std::tan (3.141592653589793 * fc / sr);
        const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const double v3 = s - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2;
        const float out = (float) (std::tanh (v2 * 1.4) * amp * gain * 0.5);
        L[i] += out;
        if (R != nullptr) R[i] += out;
    }
}

// ================= Processor =================
DaliProcessor::DaliProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    {
        auto first = dali::generateSet (params, 8, newSeed());
        for (size_t i = 0; i < 8; ++i) slots[i] = std::make_shared<const dali::Pattern> (std::move (first[i]));
    }
    publish();
    pending.reserve (256);
    events.reserve (1024);
    startTimerHz (2);
}

DaliProcessor::~DaliProcessor()
{
    stopTimer();
    const juce::SpinLock::ScopedLockType sl (outLock);
    if (extOut != nullptr) extOut->sendMessageNow (juce::MidiMessage::allNotesOff (1));
    extOut.reset();
}

juce::StringArray DaliProcessor::availableMidiOuts() const
{
    juce::StringArray names;
   #if JUCE_MAC || JUCE_LINUX
    names.add (virtualPortName());
   #endif
    for (auto& d : juce::MidiOutput::getAvailableDevices()) names.addIfNotAlreadyThere (d.name);
    return names;
}

void DaliProcessor::setMidiOut (const juce::String& name)
{
    std::unique_ptr<juce::MidiOutput> dev;
    if (name.isNotEmpty())
    {
       #if JUCE_MAC || JUCE_LINUX
        if (name == virtualPortName()) dev = juce::MidiOutput::createNewDevice ("DaliME");
       #endif
        if (dev == nullptr)
            for (auto& d : juce::MidiOutput::getAvailableDevices())
                if (d.name == name) { dev = juce::MidiOutput::openDevice (d.identifier); break; }
        if (dev != nullptr) dev->startBackgroundThread();
        else lastMessage = "Could not open the MIDI port " + name + ". Is it in use or disconnected?";
    }
    {
        const juce::SpinLock::ScopedLockType sl (outLock);
        std::swap (extOut, dev);
    }
    if (dev != nullptr) dev->sendMessageNow (juce::MidiMessage::allNotesOff (1)); // old port: release hanging notes
    midiOutName = extOut != nullptr ? name : juce::String();
    ++modelVersion;
}

void DaliProcessor::sendExternal (const juce::MidiBuffer& midi)
{
    if (midi.isEmpty()) return;
    const juce::SpinLock::ScopedTryLockType sl (outLock);
    if (sl.isLocked() && extOut != nullptr)
        extOut->sendBlockOfMessages (midi, juce::Time::getMillisecondCounterHiRes(), sampleRate);
}

void DaliProcessor::startListening()
{
    if (! hostIsPlaying.load()) { enabled = true; internalPlay = true; }
}

bool DaliProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void DaliProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    synth.prepare (sr);
    pending.clear();
    wasRunning = false;
}

void DaliProcessor::flushAll (juce::MidiBuffer& midi, int sample)
{
    for (auto& p : pending) midi.addEvent (juce::MidiMessage::noteOff (1, p.note), sample);
    midi.addEvent (juce::MidiMessage::allNotesOff (1), sample);
    pending.clear();
    synth.allOff();
    prevSlide = false;
    prevNote = -1;
}

void DaliProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    midi.clear(); // the plugin is a generator: incoming MIDI is not passed through
    events.clear();
    if (n <= 0) return;

    const auto pat = std::atomic_load (&live);
    bool hostPlaying = false, havePpq = false;
    double ppq = 0.0, bpm = internalBpm.load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = *b; hostBpm.store (*b); }
            hostPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) { ppq = *p; havePpq = true; }
        }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    currentBpm.store (bpm);
    const double ppqPerSample = bpm / 60.0 / sampleRate;

    bool running = false, internalClock = false;
    double start = 0.0;
    hostIsPlaying.store (hostPlaying);
    if (hostPlaying && havePpq)
    {
        running = true;
        start = ppq;
        wasInternal = false;
        internalPlay.store (false);
    }
    else if (internalPlay.load())
    {
        if (! wasInternal) { internalPpq = 0.0; wasInternal = true; wasRunning = false; }
        running = internalClock = true;
        start = internalPpq;
    }
    else wasInternal = false;

    synth.acid = pat != nullptr && pat->mode == dali::Mode::Acid;
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool preview = previewOn.load();
    const float gain = previewGain.load();

    if (! running)
    {
        if (wasRunning) flushAll (midi, 0);
        wasRunning = false;
        outOn = enabled.load(); // takes effect immediately while stopped
        outputOn.store (outOn);
        playStep.store (-1);
        if (preview) synth.render (L, R, n, gain); // let the last note release
        sendExternal (midi);
        return;
    }

    const double end = start + n * ppqPerSample;
    if (! wasRunning || std::abs (start - lastEnd) > 0.02) flushAll (midi, 0); // start, loop or jump
    if (! wasRunning) { originPpq = 0.0; outOn = enabled.load(); } // melody locked to the song's bars

    // Start/Stop requested while the DAW plays: switch on the next bar line (4/4), so it enters in time
    const bool want = enabled.load();
    double switchAt = -1.0;
    if (want != outOn)
    {
        switchAt = internalClock ? start : std::ceil (start / 4.0 - 1e-9) * 4.0;
        if (switchAt >= end) switchAt = -1.0;
    }
    auto applySwitch = [&] (double at) {
        outOn = want;
        if (outOn) { originPpq = at; prevSlide = false; prevNote = -1; }
        else
        {
            const int sAt = juce::jlimit (0, n - 1, (int) std::floor ((at - start) / ppqPerSample));
            for (auto& pe : pending) events.push_back ({ sAt, false, pe.note, 0 });
            pending.clear();
            prevSlide = false;
            prevNote = -1;
        }
        switchAt = -1.0;
    };

    auto sampleAt = [&] (double t) { return juce::jlimit (0, n - 1, (int) std::floor ((t - start) / ppqPerSample)); };

    if (pat != nullptr && ! pat->steps.empty())
    {
        const double stepLen = 4.0 / pat->spb;
        const long long total = (long long) pat->steps.size();
        for (long long k = (long long) std::ceil (start / stepLen - 1e-9); (double) k * stepLen < end; ++k)
        {
            const double t = (double) k * stepLen;
            if (switchAt >= 0.0 && t >= switchAt - 1e-9) applySwitch (switchAt);
            const int s = sampleAt (t);
            const long long rel = (long long) std::llround ((t - originPpq) / stepLen);
            const int idx = (int) (((rel % total) + total) % total);
            if (! outOn) { playStep.store (-1); continue; }
            playStep.store (idx);
            const auto& st = pat->steps[(size_t) idx];
            if (! st.on) { prevSlide = false; continue; }
            const double offT = t + st.len * stepLen;
            if (prevSlide && prevNote == st.note)
            {
                for (auto& p : pending) if (p.note == st.note) p.ppqOff = std::max (p.ppqOff, offT); // tie
            }
            else
            {
                for (auto it = pending.begin(); it != pending.end();)
                {
                    if (it->note == st.note)
                    {
                        events.push_back ({ std::min (s, sampleAt (std::min (it->ppqOff, t))), false, st.note, 0 });
                        it = pending.erase (it);
                    }
                    else ++it;
                }
                events.push_back ({ s, true, st.note, st.vel });
                if (pending.size() < pending.capacity()) pending.push_back ({ offT, st.note });
            }
            prevSlide = st.slide;
            prevNote = st.note;
        }
    }
    if (switchAt >= 0.0) applySwitch (switchAt); // switch point after the last step of this block
    outputOn.store (outOn);
    for (auto it = pending.begin(); it != pending.end();)
    {
        if (it->ppqOff < end) { events.push_back ({ sampleAt (it->ppqOff), false, it->note, 0 }); it = pending.erase (it); }
        else ++it;
    }

    // insertion sort (no allocation): by sample, note-offs before note-ons at the same sample
    for (size_t i = 1; i < events.size(); ++i)
    {
        const Ev e = events[i];
        size_t j = i;
        while (j > 0 && (events[j - 1].sample > e.sample || (events[j - 1].sample == e.sample && events[j - 1].on && ! e.on)))
        {
            events[j] = events[j - 1];
            --j;
        }
        events[j] = e;
    }

    int pos = 0;
    for (const auto& e : events)
    {
        if (preview && e.sample > pos) { synth.render (L + pos, R != nullptr ? R + pos : nullptr, e.sample - pos, gain); pos = e.sample; }
        if (e.on) { midi.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.vel), e.sample); synth.noteOn (e.note, e.vel); }
        else { midi.addEvent (juce::MidiMessage::noteOff (1, e.note), e.sample); synth.noteOff (e.note); }
    }
    if (preview && pos < n) synth.render (L + pos, R != nullptr ? R + pos : nullptr, n - pos, gain);

    sendExternal (midi);
    if (internalClock) internalPpq = end;
    lastEnd = end;
    wasRunning = true;
}

// ================= model =================
void DaliProcessor::publish()
{
    auto old = std::atomic_load (&live);
    if (old != nullptr) retired.push_back (old);
    std::atomic_store (&live, slots[(size_t) sel]);
}

void DaliProcessor::timerCallback()
{
    retired.erase (std::remove_if (retired.begin(), retired.end(), [] (const PatternPtr& p) { return p.use_count() == 1; }), retired.end());
}

void DaliProcessor::pushHistory()
{
    history.push_back ({ slots, keep, sel });
    if (history.size() > 40) history.erase (history.begin());
}

bool DaliProcessor::undo()
{
    if (history.empty()) { lastMessage = "Nothing to undo."; ++modelVersion; return false; }
    const auto s = history.back();
    history.pop_back();
    slots = s.slots; keep = s.keep; sel = s.sel;
    publish();
    lastMessage = "Undone.";
    ++modelVersion;
    return true;
}

int DaliProcessor::keptCount() const
{
    int c = 0;
    for (bool k : keep) c += k ? 1 : 0;
    return c;
}

void DaliProcessor::generateAll()
{
    std::vector<int> freeSlots;
    for (int i = 0; i < 8; ++i) if (! keep[(size_t) i]) freeSlots.push_back (i);
    if (freeSlots.empty()) { lastMessage = "All eight are kept. Unkeep one to make room."; ++modelVersion; return; }
    pushHistory();
    std::vector<const dali::Pattern*> kept;
    for (int i = 0; i < 8; ++i) if (keep[(size_t) i]) kept.push_back (slots[(size_t) i].get());
    auto fresh = dali::generateSet (params, (int) freeSlots.size(), newSeed(), kept);
    for (size_t k = 0; k < freeSlots.size(); ++k) slots[(size_t) freeSlots[k]] = std::make_shared<const dali::Pattern> (std::move (fresh[k]));
    if (keep[(size_t) sel]) sel = freeSlots.front();
    publish();
    lastMessage = {};
    ++modelVersion;
}

void DaliProcessor::mutateAction()
{
    std::vector<int> kept, freeSlots;
    for (int i = 0; i < 8; ++i) (keep[(size_t) i] ? kept : freeSlots).push_back (i);
    const int pct = (int) std::lround (amount * 100);
    if (kept.empty())
    {
        pushHistory();
        auto m = dali::mutate (*slots[(size_t) sel], amount, params, newSeed());
        m.origin = std::string (1, LETTERS[sel]) + " mutated " + std::to_string (pct) + "%";
        slots[(size_t) sel] = std::make_shared<const dali::Pattern> (std::move (m));
    }
    else
    {
        if (freeSlots.empty()) { lastMessage = "All eight are kept. Unkeep one to make room for variations."; ++modelVersion; return; }
        pushHistory();
        for (size_t k = 0; k < freeSlots.size(); ++k)
        {
            const int si = kept[k % kept.size()];
            auto m = dali::mutate (*slots[(size_t) si], amount, params, newSeed());
            m.origin = amount >= 1.0 ? std::string ("New") : "From " + std::string (1, LETTERS[si]) + ", " + std::to_string (pct) + "%";
            slots[(size_t) freeSlots[k]] = std::make_shared<const dali::Pattern> (std::move (m));
        }
        if (! keep[(size_t) sel] && std::find (freeSlots.begin(), freeSlots.end(), sel) == freeSlots.end()) sel = freeSlots.front();
    }
    publish();
    lastMessage = {};
    ++modelVersion;
}

void DaliProcessor::generateSimilar()
{
    if (dna == nullptr) return;
    std::vector<int> freeSlots;
    for (int i = 0; i < 8; ++i) if (! keep[(size_t) i]) freeSlots.push_back (i);
    if (freeSlots.empty()) { lastMessage = "All eight are kept. Unkeep one to make room."; ++modelVersion; return; }
    const auto& d = *dna;
    params.mode = dali::Mode::Melody; params.root = d.root; params.scale = d.scale; params.density = d.density;
    params.movement = d.movement; params.reg = d.reg; params.weird = juce::jlimit (0.0, 0.6, d.chromatic * 1.6);
    params.bars = d.bars; params.spb = 16;
    pushHistory();
    dali::Rng r (newSeed());
    for (int i : freeSlots)
    {
        const auto m = dali::dnaMotif (d, r);
        auto p = dali::generateBest (params, newSeed(), 4, &m, &d.style, &d.rhythms);
        p.origin = "Similar to " + dnaName.upToLastOccurrenceOf (".", false, false).toStdString();
        slots[(size_t) i] = std::make_shared<const dali::Pattern> (std::move (p));
    }
    if (keep[(size_t) sel]) sel = freeSlots.front();
    publish();
    lastMessage = {};
    ++modelVersion;
}

void DaliProcessor::select (int i)
{
    sel = juce::jlimit (0, 7, i);
    publish();
    ++modelVersion;
}

void DaliProcessor::toggleKeep (int i)
{
    keep[(size_t) i] = ! keep[(size_t) i];
    ++modelVersion;
}

void DaliProcessor::replaceSelected (dali::Pattern edited)
{
    pushHistory();
    dali::fixSlides (edited.steps);
    if (edited.origin.size() < 7 || edited.origin.substr (edited.origin.size() - 6) != "edited")
        edited.origin = edited.origin.empty() ? std::string ("Edited") : edited.origin + ", edited";
    slots[(size_t) sel] = std::make_shared<const dali::Pattern> (std::move (edited));
    publish();
    ++modelVersion;
}

void DaliProcessor::loadDna (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) { lastMessage = "Could not open " + f.getFileName(); ++modelVersion; return; }
    std::vector<uint8_t> bytes ((const uint8_t*) mb.getData(), (const uint8_t*) mb.getData() + mb.getSize());
    auto d = std::make_unique<dali::Dna>();
    std::string err;
    if (! dali::analyzeMidiFile (bytes, *d, err)) { lastMessage = juce::String (err); ++modelVersion; return; }
    dna = std::move (d);
    dnaName = f.getFileName();
    lastMessage = "DNA read from " + dnaName;
    ++modelVersion;
}

std::vector<uint8_t> DaliProcessor::midiFor (int slot) const
{
    const double bpm = hostBpm.load() > 0 ? hostBpm.load() : internalBpm.load();
    return dali::toMidiFile (*slots[(size_t) slot], bpm, "DaliME " + std::string (1, LETTERS[slot]));
}

juce::String DaliProcessor::fileBaseFor (int slot) const
{
    const auto& gp = slots[(size_t) slot]->params;
    static const char* modes[] = { "melody", "acid", "arp", "chords", "hook" };
    juce::String scale (gp.scale);
    scale = scale.retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    return "DaliME_" + juce::String::charToString (LETTERS[slot]) + "_" + juce::String (dali::NOTE_NAMES[gp.root]).replace ("#", "s")
           + "-" + scale + "_" + modes[(int) slots[(size_t) slot]->mode];
}

// ================= state =================
static void putString (juce::ValueTree& t, const juce::Identifier& id, const std::string& s)
{
    t.setProperty (id, juce::var (juce::MemoryBlock (s.data(), s.size())), nullptr);
}
static std::string getString (const juce::ValueTree& t, const juce::Identifier& id)
{
    const juce::var& v = t.getProperty (id);
    if (auto* mb = v.getBinaryData()) return std::string ((const char*) mb->getData(), mb->getSize());
    return v.toString().toStdString();
}

void DaliProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree t ("DaliState");
    putString (t, "params", dali::serializeParams (params));
    t.setProperty ("sel", sel, nullptr);
    t.setProperty ("amount", amount, nullptr);
    int mask = 0;
    for (int i = 0; i < 8; ++i) if (keep[(size_t) i]) mask |= 1 << i;
    t.setProperty ("keep", mask, nullptr);
    t.setProperty ("preview", previewOn.load(), nullptr);
    t.setProperty ("gain", (double) previewGain.load(), nullptr);
    t.setProperty ("follow", followHost.load(), nullptr);
    t.setProperty ("bpm", internalBpm.load(), nullptr);
    t.setProperty ("midiOut", midiOutName, nullptr);
    t.setProperty ("enabled", enabled.load(), nullptr);
    for (int i = 0; i < 8; ++i) putString (t, juce::Identifier ("slot" + juce::String (i)), dali::serialize (*slots[(size_t) i]));
    if (auto xml = t.createXml()) copyXmlToBinary (*xml, dest);
}

void DaliProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    const auto t = juce::ValueTree::fromXml (*xml);
    if (! t.hasType ("DaliState")) return;
    dali::deserializeParams (getString (t, "params"), params);
    for (int i = 0; i < 8; ++i)
    {
        dali::Pattern p;
        if (dali::deserialize (getString (t, juce::Identifier ("slot" + juce::String (i))), p))
            slots[(size_t) i] = std::make_shared<const dali::Pattern> (std::move (p));
    }
    const int mask = (int) t.getProperty ("keep", 0);
    for (int i = 0; i < 8; ++i) keep[(size_t) i] = (mask >> i) & 1;
    sel = juce::jlimit (0, 7, (int) t.getProperty ("sel", 0));
    amount = (double) t.getProperty ("amount", 0.3);
    previewOn = (bool) t.getProperty ("preview", true);
    previewGain = (float) (double) t.getProperty ("gain", 0.5);
    followHost = (bool) t.getProperty ("follow", true);
    internalBpm = (double) t.getProperty ("bpm", 145.0);
    enabled = (bool) t.getProperty ("enabled", true);
    const juce::String out = t.getProperty ("midiOut", "").toString();
    if (out != midiOutName) setMidiOut (out);
    history.clear();
    publish();
    ++modelVersion;
}

juce::AudioProcessorEditor* DaliProcessor::createEditor() { return new DaliEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DaliProcessor(); }
