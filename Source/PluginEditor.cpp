#include "PluginEditor.h"
#include <cmath>

using namespace GoaColours;
static const char* LETTERS = "ABCDEFGH";
static const juce::Colour DEG[7] = { juce::Colour (0xffddff4a), juce::Colour (0xff43f0ff), juce::Colour (0xffff4fd8), juce::Colour (0xffff9b45),
                                     juce::Colour (0xffb173ff), juce::Colour (0xff5cffa8), juce::Colour (0xffff6a7a) };
static juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
static int pcOf (int m) { return ((m % 12) + 12) % 12; }

// ================= LookAndFeel =================
GoaLookAndFeel::GoaLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::TextButton::buttonColourId, panel2);
    setColour (juce::TextButton::buttonOnColourId, ink);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, uv);
    setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::PopupMenu::headerTextColourId, dim);
    setColour (juce::Slider::backgroundColourId, line);
    setColour (juce::Slider::trackColourId, mag);
    setColour (juce::Slider::thumbColourId, ink);
    setColour (juce::Slider::textBoxTextColourId, ink);
    setColour (juce::Slider::textBoxBackgroundColourId, panel2);
    setColour (juce::Slider::textBoxOutlineColourId, line);
    setColour (juce::TextEditor::backgroundColourId, panel2);
    setColour (juce::TextEditor::textColourId, ink);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, cyan);
    setColour (juce::Label::textColourId, ink);
    setColour (juce::ToggleButton::textColourId, ink);
    setColour (juce::ToggleButton::tickColourId, mag);
    setColour (juce::ToggleButton::tickDisabledColourId, dim);
    setColour (juce::ScrollBar::thumbColourId, uv);
}

// ================= roll drawing =================
static void rollRange (const goa::Pattern& p, int& lo, int& hi)
{
    lo = 127; hi = 0;
    for (auto& s : p.steps) if (s.on) { lo = std::min (lo, s.note); hi = std::max (hi, s.note); }
    if (lo > hi) { lo = 60; hi = 72; return; }
    lo -= 1; hi += 1;
    if (hi - lo < 12) { const int c = (hi + lo) / 2; lo = c - 6; hi = lo + 12; }
}

static void drawRoll (juce::Graphics& g, juce::Rectangle<float> a, const goa::Pattern& p, int playIdx, bool big)
{
    g.setColour (roll);
    if (big) g.fillRect (a); else g.fillRoundedRectangle (a, 6.0f);
    const int N = (int) p.steps.size();
    if (N == 0) return;
    int lo, hi;
    rollRange (p, lo, hi);
    const float rh = a.getHeight() / (float) (hi - lo + 1), cw = a.getWidth() / (float) N;
    const int root = p.params.root;
    const auto sc = goa::scaleOf (p.params);
    g.setColour (juce::Colour (0xffddff4a).withAlpha (0.07f));
    for (int m = lo; m <= hi; ++m)
        if (pcOf (m - root) == 0) g.fillRect (a.getX(), a.getBottom() - (float) (m - lo + 1) * rh, a.getWidth(), rh);
    const int spbeat = std::max (1, p.spb / 4);
    for (int i = 0; i <= N; i += spbeat)
    {
        g.setColour (juce::Colours::white.withAlpha (i % p.spb == 0 ? 0.24f : 0.07f));
        g.fillRect (a.getX() + (float) i * cw, a.getY(), 1.0f, a.getHeight());
    }
    const float nh = std::max (2.0f, rh - (big ? 2.0f : 1.0f)), pad = big ? 1.2f : 0.5f;
    for (int i = 0; i < N; ++i)
    {
        const auto& s = p.steps[(size_t) i];
        if (! s.on) continue;
        const float x = a.getX() + (float) i * cw + pad * 0.5f;
        const float y = a.getBottom() - (float) (s.note - lo + 1) * rh + (rh - nh) * 0.5f;
        const float w = std::max (2.0f, (float) std::min (s.len, (double) (N - i)) * cw - pad);
        const int rel = pcOf (s.note - root);
        int di = -1;
        for (size_t k = 0; k < sc.size(); ++k) if (sc[k] == rel) di = (int) k;
        const juce::Colour col = di < 0 ? juce::Colours::white : DEG[di % 7];
        const float corner = std::min (3.0f, nh * 0.5f);
        g.setColour (col.withAlpha (0.55f + 0.45f * (float) s.vel / 127.0f));
        g.fillRoundedRectangle (x, y, w, nh, corner);
        if (big && s.acc) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (x, y, w, nh, corner, 1.4f); }
        if (s.slide && i + 1 < N && p.steps[(size_t) i + 1].on)
        {
            g.setColour (col);
            const float y2 = a.getBottom() - (float) (p.steps[(size_t) i + 1].note - lo + 1) * rh + rh * 0.5f;
            g.drawLine (x + w - 1.0f, y + nh * 0.5f, a.getX() + (float) (i + 1) * cw + pad * 0.5f + 1.0f, y2, big ? 2.0f : 1.0f);
        }
        if (big && cw >= 22.0f && nh >= 10.0f)
        {
            g.setColour (roll);
            g.setFont (font (10.0f, true));
            g.drawText (goa::NOTE_NAMES[pcOf (s.note)], juce::Rectangle<float> (x + 3.0f, y, w - 3.0f, nh), juce::Justification::centredLeft, false);
        }
    }
    if (playIdx >= 0 && playIdx < N)
    {
        g.setColour (cyan.withAlpha (0.28f));
        g.fillRect (a.getX() + (float) playIdx * cw, a.getY(), std::max (2.0f, cw), a.getHeight());
    }
}

static juce::Path heartPath (juce::Rectangle<float> r)
{
    juce::Path p;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    p.startNewSubPath (x + w * 0.5f, y + h * 0.92f);
    p.cubicTo (x - w * 0.12f, y + h * 0.45f, x + w * 0.15f, y - h * 0.08f, x + w * 0.5f, y + h * 0.28f);
    p.cubicTo (x + w * 0.85f, y - h * 0.08f, x + w * 1.12f, y + h * 0.45f, x + w * 0.5f, y + h * 0.92f);
    p.closeSubPath();
    return p;
}

static juce::String modeText (const goa::Pattern& p)
{
    switch (p.mode)
    {
        case goa::Mode::Acid: return "Acid bassline, " + juce::String (p.params.style);
        case goa::Mode::Arp: return "Goa arp, " + juce::String (p.params.style);
        case goa::Mode::Chords: return "Chord-aware melody, " + juce::String (p.params.style);
        case goa::Mode::Hook: return "Hook: " + juce::String (goa::hookShapeNames()[(size_t) juce::jlimit (0, 5, p.params.hookShape)]);
        default: return juce::String (p.params.style);
    }
}
static juce::String rateText (int spb) { return spb == 24 ? "1/16T" : spb == 8 ? "1/8" : "1/16"; }

// ================= SlotView =================
juce::Rectangle<float> SlotView::heartArea() const { return { (float) getWidth() - 32.0f, 4.0f, 28.0f, 26.0f }; }

void SlotView::paint (juce::Graphics& g)
{
    const auto& ptr = proc.slots[(size_t) idx];
    if (ptr == nullptr) return;
    const auto& p = *ptr;
    const bool selected = proc.sel == idx, kept = proc.keep[(size_t) idx];
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (kept ? panel2 : panel);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (selected ? mag : line);
    g.drawRoundedRectangle (b, 10.0f, selected ? 2.0f : 1.0f);

    auto r = b.reduced (9.0f, 7.0f);
    auto top = r.removeFromTop (20.0f);
    g.setColour (ink);
    g.setFont (font (17.0f, true));
    g.drawText (juce::String::charToString (LETTERS[idx]), top.removeFromLeft (22.0f), juce::Justification::centredLeft);
    top.removeFromRight (26.0f);
    g.setColour (dim);
    g.setFont (font (11.5f));
    g.drawText (juce::String (goa::NOTE_NAMES[p.params.root]) + " " + juce::String (p.params.scale).toLowerCase(), top,
                juce::Justification::centredLeft, true);

    auto heart = heartPath (heartArea().reduced (5.0f, 5.0f));
    if (kept) { g.setColour (mag); g.fillPath (heart); }
    else { g.setColour (dim); g.strokePath (heart, juce::PathStrokeType (1.6f)); }

    auto bottom = r.removeFromBottom (15.0f);
    r.removeFromTop (4.0f);
    r.removeFromBottom (4.0f);
    drawRoll (g, r, p, -1, false);
    g.setColour (dim);
    g.setFont (font (11.0f));
    const juce::String origin = p.origin.empty() ? modeText (p) : juce::String (p.origin);
    g.drawText (origin, bottom, juce::Justification::centredLeft, true);
}

void SlotView::mouseUp (const juce::MouseEvent& e)
{
    if (heartArea().contains (e.position)) proc.toggleKeep (idx);
    else { proc.select (idx); proc.startListening(); } // click to hear
}

// ================= BigRoll =================
void BigRoll::paint (juce::Graphics& g)
{
    const auto& ptr = proc.slots[(size_t) proc.sel];
    if (ptr == nullptr) return;
    const auto& p = *ptr;
    auto b = getLocalBounds().toFloat();
    auto lanesArea = b.removeFromBottom ((float) (laneH * lanes));
    drawRoll (g, b, p, playIdx, true);
    g.setColour (panel);
    g.fillRect (lanesArea);
    const int N = (int) p.steps.size();
    if (N == 0) return;
    const float cw = lanesArea.getWidth() / (float) N;
    for (int l = 0; l < lanes; ++l)
    {
        g.setColour (line);
        g.fillRect (lanesArea.getX(), lanesArea.getY() + (float) (l * laneH), lanesArea.getWidth(), 1.0f);
    }
    const int spbeat = std::max (1, p.spb / 4);
    for (int i = 0; i < N; ++i)
    {
        const float x = lanesArea.getX() + (float) i * cw;
        if (i % spbeat == 0) { g.setColour (line.withAlpha (i % p.spb == 0 ? 1.0f : 0.5f)); g.fillRect (x, lanesArea.getY(), 1.0f, lanesArea.getHeight()); }
        const auto& s = p.steps[(size_t) i];
        if (! s.on) continue;
        const float y0 = lanesArea.getY();
        const float vh = (float) s.vel / 127.0f * (float) (laneH - 5);
        g.setColour (s.acc ? mag : uv);
        g.fillRect (x + 2.0f, y0 + (float) laneH - 2.0f - vh, std::max (1.5f, cw - 4.0f), vh);
        const float cx = x + cw * 0.5f, ay = y0 + (float) laneH * 1.5f, sy = y0 + (float) laneH * 2.5f;
        if (s.acc) { g.setColour (mag); g.fillEllipse (cx - 4.0f, ay - 4.0f, 8.0f, 8.0f); }
        else { g.setColour (dim.withAlpha (0.5f)); g.fillEllipse (cx - 1.5f, ay - 1.5f, 3.0f, 3.0f); }
        if (s.slide) { g.setColour (cyan); g.drawLine (cx - cw * 0.35f, sy + 3.0f, cx + cw * 0.6f, sy - 3.0f, 2.0f); }
        else { g.setColour (dim.withAlpha (0.5f)); g.fillEllipse (cx - 1.5f, sy - 1.5f, 3.0f, 3.0f); }
    }
    g.setFont (font (10.5f, true));
    const char* names[] = { "Vel", "Acc", "Slide" };
    for (int l = 0; l < lanes; ++l)
    {
        auto lr = juce::Rectangle<float> (lanesArea.getX() + 3.0f, lanesArea.getY() + (float) (l * laneH) + 2.0f, 34.0f, (float) laneH - 4.0f);
        g.setColour (panel.withAlpha (0.85f));
        g.fillRoundedRectangle (lr, 3.0f);
        g.setColour (dim);
        g.drawText (names[l], lr, juce::Justification::centred);
    }
}

void BigRoll::mouseDown (const juce::MouseEvent& e)
{
    const auto& ptr = proc.slots[(size_t) proc.sel];
    if (ptr == nullptr || ! onEdit) return;
    const auto& p = *ptr;
    const int N = (int) p.steps.size();
    if (N == 0) return;
    const int step = juce::jlimit (0, N - 1, (int) (e.position.x / ((float) getWidth() / (float) N)));
    const float rollH = (float) (getHeight() - laneH * lanes);
    if (e.position.y < rollH)
    {
        int lo, hi;
        rollRange (p, lo, hi);
        const float rh = rollH / (float) (hi - lo + 1);
        onEdit (step, 0, juce::jlimit (0, 127, hi - (int) (e.position.y / rh)));
        return;
    }
    const int lane = juce::jlimit (0, lanes - 1, (int) ((e.position.y - rollH) / (float) laneH));
    if (lane == 0)
    {
        const float y01 = (e.position.y - rollH) / (float) laneH;
        onEdit (step, 1, juce::jlimit (1, 127, (int) std::lround ((1.0f - y01) * 127.0f)));
    }
    else onEdit (step, lane + 1, 0);
}

// ================= Drag source =================
void MidiDragSource::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (panel2);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (isMouseOver() ? cyan : uv);
    g.drawRoundedRectangle (b, 6.0f, 1.5f);
    g.setFont (font (13.5f, true));
    g.setColour (ink);
    g.drawText (dragging ? "Drop it on a MIDI track" : "Drag MIDI to a track", b, juce::Justification::centred);
}

void MidiDragSource::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5) return;
    dragging = true;
    repaint();
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("GoaMelodyEngine");
    dir.createDirectory();
    auto f = dir.getChildFile (proc.fileBaseFor (proc.sel) + ".mid");
    const auto data = proc.midiFor (proc.sel);
    f.replaceWithData (data.data(), data.size());
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                [safe = juce::Component::SafePointer<MidiDragSource> (this)] {
                                                                    if (safe != nullptr) { safe->dragging = false; safe->repaint(); }
                                                                });
}

// ================= Editor =================
juce::Label& GoaEditor::makeLabel (juce::Label& l, const juce::String& text, bool dimText)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (font (13.0f, true));
    l.setColour (juce::Label::textColourId, dimText ? dim : ink);
    l.setBorderSize (juce::BorderSize<int> (0));
    addAndMakeVisible (l);
    return l;
}

static juce::String modeDescription (goa::Mode m)
{
    switch (m)
    {
        case goa::Mode::Acid: return "303-style bassline with accents and slides.";
        case goa::Mode::Arp: return "Arpeggio that walks through your chords.";
        case goa::Mode::Chords: return "Melody that changes with your chords.";
        case goa::Mode::Hook: return "Short catchy phrase: one clear peak, comes home at the end.";
        default: return "Lead line built from one short motif.";
    }
}
static juce::String styleDescription (const std::string& s)
{
    if (s == "Acid") return "Syncopated, more rests, accents and slides.";
    if (s == "Hypnotic") return "2-3 note motif repeated almost unchanged.";
    if (s == "Emotional") return "Longer notes, stepwise, singable.";
    if (s == "Dark") return "Low, heavy root pedal, narrow steps.";
    if (s == "Cosmic") return "High, wide leaps and open 5ths.";
    if (s == "90s Goa") return "Constant rolling 16ths over a strong root pedal.";
    if (s == "Modern Goa") return "Off-beat syncopation and gaps, punchier.";
    return "Rolling 16ths, 4-5 note motif over the root.";
}

static juce::String riffDescription (int type)
{
    switch (type)
    {
        case 1: return "Melody notes alternate with the root. The classic Goa roll.";
        case 2: return "A short theme with a call and an answer.";
        case 3: return "Arpeggio shapes on the chord's notes. Very consonant.";
        case 4: return "Low and high root bounce under the melody notes.";
        case 5: return "Fast 4-note runs up and down the scale.";
        case 6: return "Fewer, longer, punchy notes.";
        default: return "Every Generate mixes different riff types across the 8 ideas.";
    }
}

static juce::String hookDescription (int shape)
{
    switch (shape)
    {
        case 1: return "Bar 1 climbs to a peak, bar 2 answers and comes home.";
        case 2: return "Tresillo rhythm (3+3+2) with a 3-note cell. Instantly memorable.";
        case 3: return "A small cell, one step higher each time, then a fall.";
        case 4: return "Root and octave on the beat, a melodic tail in between. Psy lead.";
        case 5: return "Long notes and wide intervals. Made for the breakdown.";
        default: return "Picks a shape that suits the style.";
    }
}

GoaEditor::GoaEditor (GoaProcessor& p) : AudioProcessorEditor (&p), proc (p), bigRoll (p), dragSrc (p)
{
    setLookAndFeel (&lnf);

    makeLabel (title, "Goa Melody Engine", false).setFont (font (24.0f, true));
    makeLabel (modeL, "Mode");
    makeLabel (modeHelp, "").setFont (font (11.5f));
    makeLabel (rootL, "Root");
    makeLabel (scaleL, "Scale");
    makeLabel (styleL, "Style");
    makeLabel (styleHelp, "").setFont (font (11.5f));
    makeLabel (chordsL, "Chords (one per bar)");
    makeLabel (barsL, "Phrase length (bars)");
    makeLabel (rateL, "Note grid");
    makeLabel (scaleNotes, "", false).setFont (font (13.0f, true));
    scaleNotes.setColour (juce::Label::textColourId, juce::Colour (0xffddff4a));
    scaleNotes.setTooltip ("The notes of the chosen scale, starting from the root.");

    const char* modeNames[5] = { "Melody", "Acid", "Arp", "Chords", "Hook" };
    for (int i = 0; i < 5; ++i)
    {
        auto& b = modeBtns[(size_t) i];
        b.setButtonText (modeNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1001);
        b.setTooltip (modeDescription ((goa::Mode) i) + " Applies on the next Generate.");
        b.onClick = [this, i] { if (modeBtns[(size_t) i].getToggleState()) { proc.params.mode = (goa::Mode) i; ++proc.modelVersion; } };
        addAndMakeVisible (b);
    }

    for (int i = 0; i < 12; ++i) rootBox.addItem (goa::NOTE_NAMES[i], i + 1);
    rootBox.onChange = [this] { proc.params.root = rootBox.getSelectedId() - 1; ++proc.modelVersion; };
    addAndMakeVisible (rootBox);

    int id = 1;
    for (auto& grp : goa::scaleGroups())
    {
        scaleBox.addSectionHeading (juce::String (grp.name));
        for (auto& s : grp.scales) { scaleBox.addItem (juce::String (s.first), id++); scaleIds.push_back (s.first); }
    }
    scaleBox.addSectionHeading ("Your own");
    scaleBox.addItem ("Custom", id);
    scaleIds.push_back ("Custom");
    scaleBox.onChange = [this] {
        const int sid = scaleBox.getSelectedId();
        if (sid < 1 || sid > (int) scaleIds.size()) return;
        const std::string prev = proc.params.scale;
        proc.params.scale = scaleIds[(size_t) sid - 1];
        if (proc.params.scale == "Custom")
            if (auto* s = goa::findScale (prev)) { proc.params.custom.fill (0); for (int x : *s) proc.params.custom[(size_t) x] = 1; }
        ++proc.modelVersion;
    };
    addAndMakeVisible (scaleBox);

    for (int i = 0; i < 12; ++i)
    {
        auto& b = customBtns[(size_t) i];
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, uv);
        b.setColour (juce::TextButton::textColourOnId, ink);
        b.onClick = [this, i] {
            if (i == 0) { customBtns[0].setToggleState (true, juce::dontSendNotification); return; }
            proc.params.custom[(size_t) i] = customBtns[(size_t) i].getToggleState() ? 1 : 0;
            ++proc.modelVersion;
        };
        addChildComponent (b);
    }

    auto styles = goa::styleNames();
    for (size_t i = 0; i < styles.size(); ++i) styleBox.addItem (juce::String (styles[i]), (int) i + 1);
    styleBox.onChange = [this, styles] {
        const int sid = styleBox.getSelectedId();
        if (sid >= 1 && sid <= (int) styles.size()) { proc.params.style = styles[(size_t) sid - 1]; ++proc.modelVersion; }
    };
    addAndMakeVisible (styleBox);

    makeLabel (riffL, "Riff type");
    makeLabel (riffHelp, "").setFont (font (11.5f));
    {
        const auto types = goa::riffTypeNames();
        for (size_t i = 0; i < types.size(); ++i) riffBox.addItem (juce::String (types[i]), (int) i + 1);
    }
    riffBox.setTooltip ("What kind of riff to write. Auto mixes all types so the 8 ideas sound different.");
    riffBox.onChange = [this] { proc.params.archetype = juce::jlimit (0, 6, riffBox.getSelectedId() - 1); ++proc.modelVersion; };
    addAndMakeVisible (riffBox);

    chordsEd.setTextToShowWhenEmpty ("Gm Eb F Dm", dim);
    chordsEd.setTooltip ("Type chord names separated by spaces, for example Gm Eb F Dm or Am7 Fmaj7.");
    chordsEd.onTextChange = [this] { proc.params.chords = chordsEd.getText().toStdString(); };
    addChildComponent (chordsEd);

    makeLabel (hookL, "Hook shape");
    makeLabel (hookHelp, "").setFont (font (11.5f));
    {
        const auto shapes = goa::hookShapeNames();
        for (size_t i = 0; i < shapes.size(); ++i) hookBox.addItem (juce::String (shapes[i]), (int) i + 1);
    }
    hookBox.setTooltip ("The musical shape of the hook. Every hook repeats its rhythm, has one peak and ends on the root.");
    hookBox.onChange = [this] { proc.params.hookShape = juce::jlimit (0, 5, hookBox.getSelectedId() - 1); ++proc.modelVersion; };
    addChildComponent (hookBox);

    const char* kn[4] = { "Density", "Movement", "Register", "Weirdness" };
    const char* tips[4] = { "How many notes play per beat, and which groove the rhythm uses.",
                            "How far notes jump, and how much the motif changes when it repeats.",
                            "How high or low the melody sits.",
                            "How far the melody may leave the scale. Downbeats always stay in key." };
    for (int i = 0; i < 4; ++i)
    {
        auto& k = knobs[(size_t) i];
        makeLabel (k.name, kn[i]);
        makeLabel (k.value, "", false);
        k.value.setJustificationType (juce::Justification::centredRight);
        makeLabel (k.help, "").setFont (font (11.5f));
        k.help.setJustificationType (juce::Justification::topLeft);
        k.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRange (0.0, 100.0, 1.0);
        k.slider.setTooltip (tips[i]);
        k.slider.onValueChange = [this, i] {
            double* f[4] = { &proc.params.density, &proc.params.movement, &proc.params.reg, &proc.params.weird };
            *f[i] = knobs[(size_t) i].slider.getValue() / 100.0;
            updateHelpTexts();
        };
        addAndMakeVisible (k.slider);
    }

    const int barVals[5] = { 1, 2, 4, 8, 16 };
    for (int i = 0; i < 5; ++i)
    {
        auto& b = barBtns[(size_t) i];
        b.setButtonText (juce::String (barVals[i]));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1002);
        b.setTooltip ("How many bars the melody lasts before it loops.");
        b.onClick = [this, i, v = barVals[i]] { if (barBtns[(size_t) i].getToggleState()) proc.params.bars = v; };
        addAndMakeVisible (b);
    }
    const int rateVals[3] = { 16, 24, 8 };
    const char* rateTips[3] = { "16th notes: the classic rolling Goa grid.", "16th-note triplets: a swinging, galloping feel.", "8th notes: slower and more open." };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = rateBtns[(size_t) i];
        b.setButtonText (rateText (rateVals[i]));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1003);
        b.setTooltip (rateTips[i]);
        b.onClick = [this, i, v = rateVals[i]] { if (rateBtns[(size_t) i].getToggleState()) proc.params.spb = v; };
        addAndMakeVisible (b);
    }

    makeLabel (gridHint, "8 ideas. Click one to hear it. The heart keeps it: Generate replaces the others, Mutate turns them into variations of what you kept.")
        .setFont (font (12.5f));
    for (int i = 0; i < 8; ++i)
    {
        slotViews[(size_t) i] = std::make_unique<SlotView> (proc, i);
        slotViews[(size_t) i]->setTooltip ("Click to select and hear. Click the heart to keep it.");
        addAndMakeVisible (*slotViews[(size_t) i]);
    }

    makeLabel (bigLetter, "A", false).setFont (font (40.0f, true));
    bigLetter.setColour (juce::Label::textColourId, mag);
    makeLabel (detailTitle, "", false).setFont (font (15.0f, true));
    makeLabel (detailSub, "").setFont (font (12.5f));

    rollView.setViewedComponent (&bigRoll, false);
    rollView.setScrollBarsShown (false, true);
    rollView.setScrollBarThickness (8);
    addAndMakeVisible (rollView);
    bigRoll.setTooltip ("Click to add or remove a note. Lanes below: velocity, accent, slide.");
    bigRoll.onEdit = [this] (int s, int lane, int v) { editStep (s, lane, v); };

    playBtn.setTooltip ("Start or stop the melody. While the DAW plays it switches on the next bar, so it always enters in time. "
                        "While the DAW is stopped it plays the selected idea on its own, at the DAW's tempo.");
    playBtn.onClick = [this] {
        if (proc.hostIsPlaying.load()) proc.enabled = ! proc.enabled.load();
        else if (proc.internalPlay.load()) proc.internalPlay = false;
        else { proc.enabled = true; proc.internalPlay = true; }
    };
    genBtn.setColour (juce::TextButton::buttonColourId, mag);
    genBtn.setColour (juce::TextButton::textColourOffId, bg);
    genBtn.setTooltip ("Write new melodies in every slot that is not kept.");
    genBtn.onClick = [this] { proc.generateAll(); };
    mutBtn.setColour (juce::TextButton::buttonColourId, ink);
    mutBtn.setColour (juce::TextButton::textColourOffId, bg);
    mutBtn.setTooltip ("With kept melodies: fill the other slots with variations of them. With none kept: change the selected melody.");
    mutBtn.onClick = [this] { proc.mutateAction(); };
    undoBtn.setTooltip ("Go back one step.");
    undoBtn.onClick = [this] { proc.undo(); };
    for (auto* b : { &playBtn, &genBtn, &mutBtn, &undoBtn }) addAndMakeVisible (*b);

    makeLabel (amtL, "How much to mutate");
    makeLabel (amtHint, "");
    const double amts[4] = { .1, .3, .7, 1.0 };
    const char* amtTips[4] = { "One small change: a note, an accent or a slide.", "Some rhythm and a few notes change.",
                               "Keeps the motif, writes a new melody around it.", "A completely new melody." };
    for (int i = 0; i < 4; ++i)
    {
        auto& b = amtBtns[(size_t) i];
        b.setButtonText (juce::String ((int) std::lround (amts[i] * 100)) + "%");
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1004);
        b.setTooltip (amtTips[i]);
        b.onClick = [this, i, v = amts[i]] { if (amtBtns[(size_t) i].getToggleState()) { proc.amount = v; ++proc.modelVersion; } };
        addAndMakeVisible (b);
    }

    dragSrc.setTooltip ("Drag onto a MIDI track to drop the selected melody in as a clip.");
    addAndMakeVisible (dragSrc);

    makeLabel (midiOutL, "MIDI out");
    midiOutBox.setTooltip ("Send the notes to a virtual MIDI port so a synth on another track can play them. "
                           "On Windows install loopMIDI; on Mac choose the virtual port.");
    midiOutBox.onChange = [this] {
        const int sid = midiOutBox.getSelectedId();
        if (sid == 1) proc.setMidiOut ({});
        else if (sid == 2) rebuildMidiOutList();
        else if (sid >= 3 && sid - 3 < midiOutItems.size()) proc.setMidiOut (midiOutItems[sid - 3]);
    };
    addAndMakeVisible (midiOutBox);
    rebuildMidiOutList();

    previewTgl.setTooltip ("The plugin's own synth, so you can hear ideas without routing. Turn it off when your synth plays the notes.");
    previewTgl.onClick = [this] { proc.previewOn = previewTgl.getToggleState(); };
    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.setRange (0.0, 1.0, 0.01);
    gainSlider.setTooltip ("Built-in sound volume.");
    gainSlider.onValueChange = [this] { proc.previewGain = (float) gainSlider.getValue(); };
    addAndMakeVisible (previewTgl);
    addAndMakeVisible (gainSlider);

    loadDnaBtn.setTooltip ("Load a MIDI melody you love. The engine measures its intervals, rhythm and phrasing.");
    loadDnaBtn.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Load a MIDI melody", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mid;*.midi");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (f.existsAsFile()) proc.loadDna (f);
        });
    };
    similarBtn.setTooltip ("Write new melodies with the same feel as the reference, without copying it.");
    similarBtn.onClick = [this] { proc.generateSimilar(); };
    addAndMakeVisible (loadDnaBtn);
    addAndMakeVisible (similarBtn);

    makeLabel (dnaInfo, "Optional: load a melody you love and generate new ones with the same feel.").setFont (font (12.0f));
    dnaInfo.setMinimumHorizontalScale (0.75f);
    makeLabel (bpmInfo, "", false).setFont (font (12.5f, true));
    bpmInfo.setJustificationType (juce::Justification::centredRight);
    bpmInfo.setTooltip ("The melody always runs at the DAW's tempo and is locked to its bars.");
    makeLabel (status, "", false).setFont (font (12.5f, true));
    status.setColour (juce::Label::textColourId, cyan);

    setSize (1200, 850);
    refreshAll();
    startTimerHz (30);
}

GoaEditor::~GoaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GoaEditor::rebuildMidiOutList()
{
    midiOutItems = proc.availableMidiOuts();
    midiOutBox.clear (juce::dontSendNotification);
    midiOutBox.addItem ("Off (DAW routing only)", 1);
    midiOutBox.addItem ("Rescan ports", 2);
    midiOutBox.addSeparator();
    for (int i = 0; i < midiOutItems.size(); ++i) midiOutBox.addItem (midiOutItems[i], i + 3);
    const int idx = midiOutItems.indexOf (proc.midiOutName);
    midiOutBox.setSelectedId (proc.midiOutName.isEmpty() || idx < 0 ? 1 : idx + 3, juce::dontSendNotification);
    if (midiOutItems.isEmpty()) midiOutBox.setTooltip ("No MIDI ports found. On Windows install the free loopMIDI, create a port, then choose Rescan ports.");
}

void GoaEditor::updateHelpTexts()
{
    const auto& P = proc.params;
    const int tier = juce::jlimit (0, 3, (int) std::floor (P.density * 4.0));
    const char* dens[4] = { "About 1-2 notes per beat. Lots of space.", "About 2-3 notes per beat, 8th-note feel.",
                            "About 3 notes per beat, galloping 16ths.", "About 4 notes per beat, full rolling 16ths." };
    juce::String d = dens[tier];
    if (P.density >= .45) d << " Root pedal can appear between notes.";

    const double mv = P.movement;
    const bool isHook = P.mode == goa::Mode::Hook;
    if (isHook)
    {
        const char* space[3] = { "Sparse hook: 2-4 long notes per bar.", "Medium: about 6 notes per bar.", "Busy: 8-10 notes per bar." };
        d = space[P.density < .35 ? 0 : P.density < .7 ? 1 : 2];
    }
    const juce::String m = isHook ? (mv < .34 ? "Peak about a 5th above the start. Answer repeats the ending."
                                   : mv < .67 ? "Peak about a 6th above the start. Answer varies a little."
                                              : "Peak up to an octave or more. Answer varies more.")
                         : mv < .25 ? "Moves by step. The motif repeats almost exactly (hypnotic)."
                         : mv < .5  ? "Mostly steps and 3rds. The answer changes the motif's ending."
                         : mv < .75 ? "Jumps up to a 5th. The motif is shifted and inverted."
                                    : "Leaps up to an octave. The motif is reversed and inverted.";

    int oct = 2 + (int) std::lround (P.reg * 3);
    if (P.mode == goa::Mode::Acid) oct = std::max (1, oct - 1);
    const juce::String root = goa::NOTE_NAMES[P.root];
    const juce::String r = "Centred on " + root + juce::String (oct) + ", range about " + root + juce::String (oct - 1) + " to " + root + juce::String (oct + 1) + ".";

    const double w = P.weird;
    const juce::String wt = w < .12 ? "Only scale notes. Clean and in key."
                          : w < .35 ? "Chromatic passing notes slide into the next note, off-beats only."
                          : w < .5  ? "Plus odd leaps (tritones, 7ths, flat 9ths), pulled back into the key."
                          : w < .72 ? "Plus octave jumps, and leaps may leave the key."
                                    : "Alien: one scale note is bent a semitone throughout. Downbeats stay in key.";
    const juce::String vals[4] = { juce::String ((int) std::lround (P.density * 100)), juce::String ((int) std::lround (mv * 100)),
                                   juce::String ((int) std::lround (P.reg * 100)),
                                   w < .12 ? "Clean" : w < .35 ? "Twisted" : w < .72 ? "Strange" : "Alien" };
    const juce::String helps[4] = { d, m, r, wt };
    for (int i = 0; i < 4; ++i)
    {
        knobs[(size_t) i].value.setText (vals[i], juce::dontSendNotification);
        knobs[(size_t) i].help.setText (helps[i], juce::dontSendNotification);
    }
    modeHelp.setText (modeDescription (P.mode), juce::dontSendNotification);
    hookHelp.setText (hookDescription (P.hookShape), juce::dontSendNotification);
    riffHelp.setText (riffDescription (P.archetype), juce::dontSendNotification);
    styleHelp.setText (styleDescription (P.style), juce::dontSendNotification);
}

void GoaEditor::editStep (int i, int lane, int value)
{
    goa::Pattern p = *proc.slots[(size_t) proc.sel];
    auto& st = p.steps;
    if (i < 0 || i >= (int) st.size()) return;
    auto& s = st[(size_t) i];
    if (lane == 0)
    {
        if (s.on && s.note == value) s = goa::Step {};
        else
        {
            const bool was = s.on;
            s.on = true;
            s.note = value;
            if (! was) { s.vel = 100; s.len = 0.6; s.acc = s.slide = false; }
        }
    }
    else
    {
        if (! s.on) return;
        if (lane == 1) { s.vel = value; s.acc = value >= 120; }
        else if (lane == 2) { s.acc = ! s.acc; s.vel = s.acc ? 127 : 95; }
        else
        {
            const auto& nx = st[(size_t) ((i + 1) % (int) st.size())];
            if (! s.slide && ! nx.on) { proc.lastMessage = "A slide needs a note on the next step."; ++proc.modelVersion; return; }
            s.slide = ! s.slide;
            s.len = s.slide ? 1.1 : 0.6;
        }
    }
    proc.replaceSelected (std::move (p));
}

void GoaEditor::refreshAll()
{
    seenVersion = proc.modelVersion.load();
    auto& P = proc.params;
    for (int i = 0; i < 5; ++i) modeBtns[(size_t) i].setToggleState ((int) P.mode == i, juce::dontSendNotification);
    rootBox.setSelectedId (P.root + 1, juce::dontSendNotification);
    for (size_t i = 0; i < scaleIds.size(); ++i) if (scaleIds[i] == P.scale) scaleBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
    const auto styles = goa::styleNames();
    for (size_t i = 0; i < styles.size(); ++i) if (styles[i] == P.style) styleBox.setSelectedId ((int) i + 1, juce::dontSendNotification);
    scaleNotes.setText (juce::String (goa::scaleNotesText (P)), juce::dontSendNotification);

    const bool customVisible = P.scale == "Custom";
    for (int i = 0; i < 12; ++i)
    {
        auto& b = customBtns[(size_t) i];
        b.setButtonText (goa::NOTE_NAMES[(P.root + i) % 12]);
        b.setToggleState (i == 0 || P.custom[(size_t) i] != 0, juce::dontSendNotification);
        b.setVisible (customVisible);
    }
    const bool chordsVisible = P.mode == goa::Mode::Arp || P.mode == goa::Mode::Chords;
    chordsEd.setVisible (chordsVisible);
    chordsL.setVisible (chordsVisible);
    if (chordsEd.getText().toStdString() != P.chords) chordsEd.setText (juce::String (P.chords), false);
    const bool hookVisible = P.mode == goa::Mode::Hook;
    const bool riffVisible = P.mode != goa::Mode::Hook && P.mode != goa::Mode::Arp;
    riffL.setVisible (riffVisible);
    riffBox.setVisible (riffVisible);
    riffHelp.setVisible (riffVisible);
    riffBox.setSelectedId (juce::jlimit (0, 6, P.archetype) + 1, juce::dontSendNotification);
    hookL.setVisible (hookVisible);
    hookBox.setVisible (hookVisible);
    hookHelp.setVisible (hookVisible);
    hookBox.setSelectedId (juce::jlimit (0, 5, P.hookShape) + 1, juce::dontSendNotification);

    const double vals[4] = { P.density, P.movement, P.reg, P.weird };
    for (int i = 0; i < 4; ++i) knobs[(size_t) i].slider.setValue (vals[i] * 100.0, juce::dontSendNotification);
    updateHelpTexts();
    const int barVals[5] = { 1, 2, 4, 8, 16 };
    for (int i = 0; i < 5; ++i) barBtns[(size_t) i].setToggleState (P.bars == barVals[i], juce::dontSendNotification);
    const int rateVals[3] = { 16, 24, 8 };
    for (int i = 0; i < 3; ++i) rateBtns[(size_t) i].setToggleState (P.spb == rateVals[i], juce::dontSendNotification);

    const double amts[4] = { .1, .3, .7, 1.0 };
    const char* hints[4] = { "one small change", "rhythm and a few notes", "same motif, new melody", "something new" };
    for (int i = 0; i < 4; ++i)
    {
        const bool on = std::abs (proc.amount - amts[i]) < 1e-6;
        amtBtns[(size_t) i].setToggleState (on, juce::dontSendNotification);
        if (on) amtHint.setText (hints[i], juce::dontSendNotification);
    }

    juce::StringArray kept;
    for (int i = 0; i < 8; ++i) if (proc.keep[(size_t) i]) kept.add (juce::String::charToString (LETTERS[i]));
    mutBtn.setButtonText (kept.isEmpty() ? "Mutate " + juce::String::charToString (LETTERS[proc.sel]) : "Mutate kept (" + kept.joinIntoString (", ") + ")");
    similarBtn.setEnabled (proc.dna != nullptr);

    if (proc.dna != nullptr)
    {
        const auto& d = *proc.dna;
        auto pc = [] (double v) { return juce::String ((int) std::lround (v * 100)) + "%"; };
        dnaInfo.setText (proc.dnaName + ": " + goa::NOTE_NAMES[d.root] + " " + juce::String (d.scale).toLowerCase() + ", " + juce::String (d.noteCount)
                             + " notes. Stepwise " + pc (d.stepwise) + ", syncopation " + pc (d.sync) + ", repetition " + pc (d.pedal)
                             + ", octave moves " + pc (d.octJump) + ".",
                         juce::dontSendNotification);
    }
    status.setText (proc.lastMessage, juce::dontSendNotification);

    const auto& pat = *proc.slots[(size_t) proc.sel];
    bigLetter.setText (juce::String::charToString (LETTERS[proc.sel]), juce::dontSendNotification);
    detailTitle.setText (juce::String (goa::NOTE_NAMES[pat.params.root]) + " " + juce::String (pat.params.scale).toLowerCase() + ", " + modeText (pat),
                         juce::dontSendNotification);
    juce::String sub;
    if (pat.mode != goa::Mode::Arp && ! pat.motif.empty())
    {
        juce::StringArray m;
        for (int n : goa::motifToNotes (pat)) m.add (goa::NOTE_NAMES[pcOf (n)]);
        sub << "Motif " << m.joinIntoString (" ") << ". ";
    }
    if (! pat.chordNames.empty())
    {
        juce::StringArray c;
        for (auto& n : pat.chordNames) c.add (juce::String (n));
        sub << "Over " << c.joinIntoString (" ") << ". ";
    }
    sub << pat.bars << (pat.bars > 1 ? " bars" : " bar") << " at " << rateText (pat.spb) << ".";
    if (! pat.origin.empty()) sub << " " << juce::String (pat.origin) << ".";
    detailSub.setText (sub, juce::dontSendNotification);

    previewTgl.setToggleState (proc.previewOn.load(), juce::dontSendNotification);
    gainSlider.setValue (proc.previewGain.load(), juce::dontSendNotification);
    const int outIdx = midiOutItems.indexOf (proc.midiOutName);
    midiOutBox.setSelectedId (proc.midiOutName.isEmpty() || outIdx < 0 ? 1 : outIdx + 3, juce::dontSendNotification);

    if (customVisible != lastCustomVisible || chordsVisible != lastChordsVisible || hookVisible != lastHookVisible
        || riffVisible != lastRiffVisible)
    {
        lastRiffVisible = riffVisible;
        lastCustomVisible = customVisible;
        lastChordsVisible = chordsVisible;
        lastHookVisible = hookVisible;
        resized();
    }
    layoutRoll();
    bigRoll.repaint();
    for (auto& s : slotViews) s->repaint();
}

void GoaEditor::layoutRoll()
{
    const auto& pat = *proc.slots[(size_t) proc.sel];
    const int per = pat.spb == 8 ? 16 : pat.spb == 24 ? 7 : 9;
    const int w = std::max (rollView.getWidth(), (int) pat.steps.size() * per);
    const int h = rollView.getHeight() - (w > rollView.getWidth() ? rollView.getScrollBarThickness() : 0);
    bigRoll.setSize (w, std::max (80, h));
}

void GoaEditor::timerCallback()
{
    if (proc.modelVersion.load() != seenVersion) refreshAll();
    bigRoll.setPlayIdx (proc.playStep.load());
    const bool host = proc.hostIsPlaying.load();
    const bool sounding = host ? proc.outputOn.load() : proc.internalPlay.load();
    juce::String t = sounding ? "Stop" : "Start";
    if (host && proc.enabled.load() != proc.outputOn.load()) t = proc.enabled.load() ? "Starts next bar" : "Stops next bar";
    if (playBtn.getButtonText() != t) playBtn.setButtonText (t);
    playBtn.setColour (juce::TextButton::buttonColourId, sounding ? cyan.withAlpha (0.85f) : panel2);
    playBtn.setColour (juce::TextButton::textColourOffId, sounding ? bg : ink);
    const juce::String b = juce::String (proc.currentBpm.load(), 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".")
                           + " BPM" + (proc.hostBpm.load() > 0 ? ", synced to DAW" : "");
    if (bpmInfo.getText() != b) bpmInfo.setText (b, juce::dontSendNotification);
}

void GoaEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (panel);
    g.fillRoundedRectangle (leftPanel.toFloat(), 12.0f);
    g.fillRoundedRectangle (detailPanel.toFloat(), 12.0f);
    g.setColour (line);
    g.drawRoundedRectangle (leftPanel.toFloat().reduced (0.5f), 12.0f, 1.0f);
    g.drawRoundedRectangle (detailPanel.toFloat().reduced (0.5f), 12.0f, 1.0f);
}

void GoaEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    // ---------- left column: sound settings ----------
    auto left = r.removeFromLeft (300);
    r.removeFromLeft (16);
    leftPanel = left;
    left = left.reduced (14);
    auto gap = [&] (int h) { left.removeFromTop (h); };
    title.setBounds (left.removeFromTop (32));
    gap (6);
    modeL.setBounds (left.removeFromTop (18));
    {
        auto row = left.removeFromTop (30);
        const int w = row.getWidth() / 5;
        for (auto& b : modeBtns) b.setBounds (row.removeFromLeft (w).reduced (1, 0));
    }
    modeHelp.setBounds (left.removeFromTop (18));
    gap (6);
    {
        auto lr = left.removeFromTop (18);
        rootL.setBounds (lr.removeFromLeft (74));
        lr.removeFromLeft (8);
        scaleL.setBounds (lr);
        auto cr = left.removeFromTop (28);
        rootBox.setBounds (cr.removeFromLeft (74));
        cr.removeFromLeft (8);
        scaleBox.setBounds (cr);
    }
    scaleNotes.setBounds (left.removeFromTop (22));
    if (lastCustomVisible)
    {
        for (int row = 0; row < 2; ++row)
        {
            auto rr = left.removeFromTop (26);
            const int w = rr.getWidth() / 6;
            for (int i = 0; i < 6; ++i) customBtns[(size_t) (row * 6 + i)].setBounds (rr.removeFromLeft (w).reduced (1));
        }
        gap (4);
    }
    gap (2);
    if (lastRiffVisible)
    {
        auto lr = left.removeFromTop (18);
        const int half = (lr.getWidth() - 8) / 2;
        styleL.setBounds (lr.removeFromLeft (half));
        lr.removeFromLeft (8);
        riffL.setBounds (lr);
        auto cr = left.removeFromTop (28);
        styleBox.setBounds (cr.removeFromLeft (half));
        cr.removeFromLeft (8);
        riffBox.setBounds (cr);
        styleHelp.setBounds (left.removeFromTop (16));
        riffHelp.setBounds (left.removeFromTop (16));
    }
    else
    {
        styleL.setBounds (left.removeFromTop (18));
        styleBox.setBounds (left.removeFromTop (28));
        styleHelp.setBounds (left.removeFromTop (18));
    }
    gap (6);
    if (lastChordsVisible)
    {
        chordsL.setBounds (left.removeFromTop (18));
        chordsEd.setBounds (left.removeFromTop (28));
        gap (8);
    }
    if (lastHookVisible)
    {
        hookL.setBounds (left.removeFromTop (18));
        hookBox.setBounds (left.removeFromTop (28));
        hookHelp.setBounds (left.removeFromTop (18));
        gap (6);
    }
    for (auto& k : knobs)
    {
        auto hr = left.removeFromTop (18);
        k.value.setBounds (hr.removeFromRight (80));
        k.name.setBounds (hr);
        k.slider.setBounds (left.removeFromTop (22));
        k.help.setBounds (left.removeFromTop (28));
        gap (4);
    }
    barsL.setBounds (left.removeFromTop (18));
    {
        auto row = left.removeFromTop (28);
        const int w = row.getWidth() / 5;
        for (auto& b : barBtns) b.setBounds (row.removeFromLeft (w).reduced (1, 0));
    }
    gap (8);
    rateL.setBounds (left.removeFromTop (18));
    {
        auto row = left.removeFromTop (28);
        const int w = row.getWidth() / 3;
        for (auto& b : rateBtns) b.setBounds (row.removeFromLeft (w).reduced (1, 0));
    }

    // ---------- right column: ideas, editor, actions ----------
    gridHint.setBounds (r.removeFromTop (20));
    r.removeFromTop (6);
    {
        auto grid = r.removeFromTop (232);
        const int cw = (grid.getWidth() - 3 * 8) / 4, ch = (grid.getHeight() - 8) / 2;
        for (int i = 0; i < 8; ++i)
            slotViews[(size_t) i]->setBounds (grid.getX() + (i % 4) * (cw + 8), grid.getY() + (i / 4) * (ch + 8), cw, ch);
    }
    r.removeFromTop (12);
    auto detail = r.removeFromTop (320);
    detailPanel = detail;
    detail = detail.reduced (12);
    {
        auto head = detail.removeFromTop (46);
        bigLetter.setBounds (head.removeFromLeft (46));
        head.removeFromLeft (8);
        detailTitle.setBounds (head.removeFromTop (22));
        detailSub.setBounds (head);
    }
    detail.removeFromTop (8);
    rollView.setBounds (detail);
    r.removeFromTop (12);

    {   // main actions
        auto row = r.removeFromTop (44);
        playBtn.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (8);
        genBtn.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (8);
        mutBtn.setBounds (row.removeFromLeft (190));
        row.removeFromLeft (12);
        undoBtn.setBounds (row.removeFromRight (80));
        row.removeFromRight (8);
        auto amtCol = row;
        auto amtTop = amtCol.removeFromTop (18);
        amtL.setBounds (amtTop.removeFromLeft (130));
        amtHint.setBounds (amtTop);
        const int w = amtCol.getWidth() / 4;
        for (auto& b : amtBtns) b.setBounds (amtCol.removeFromLeft (w).reduced (1, 0));
    }
    r.removeFromTop (10);
    {   // getting the notes out
        auto row = r.removeFromTop (32);
        dragSrc.setBounds (row.removeFromLeft (190));
        row.removeFromLeft (16);
        midiOutL.setBounds (row.removeFromLeft (62));
        midiOutBox.setBounds (row.removeFromLeft (250));
        row.removeFromLeft (16);
        previewTgl.setBounds (row.removeFromLeft (120));
        gainSlider.setBounds (row.removeFromLeft (130));
    }
    r.removeFromTop (10);
    {   // melody DNA (optional)
        auto row = r.removeFromTop (32);
        loadDnaBtn.setBounds (row.removeFromLeft (170));
        row.removeFromLeft (8);
        similarBtn.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (12);
        dnaInfo.setBounds (row);
    }
    r.removeFromTop (6);
    {
        auto row = r.removeFromTop (20);
        bpmInfo.setBounds (row.removeFromRight (240));
        status.setBounds (row);
    }
    layoutRoll();
}
