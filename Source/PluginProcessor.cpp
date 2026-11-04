#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace unit808;

namespace
{
// panel knobs: id, name, default (0..1, the UNIT 808 device's defaults); shown 0..10 like the hardware
struct KnobDef { const char* id; const char* name; float def; };
const KnobDef kKnobs[23] = {
    { "ac-level", "Accent", 0.6f },
    { "bd-level", "BD Level", 0.5f }, { "bd-tone", "BD Tone", 0.5f }, { "bd-decay", "BD Decay", 0.32f },
    { "sd-level", "SD Level", 0.3f }, { "sd-tone", "SD Tone", 1.0f }, { "sd-snappy", "SD Snappy", 0.6f },
    { "lt-level", "LT/LC Level", 0.45f }, { "lt-tuning", "LT/LC Tuning", 0.386f },
    { "mt-level", "MT/MC Level", 0.42f }, { "mt-tuning", "MT/MC Tuning", 0.32f },
    { "ht-level", "HT/HC Level", 0.40f }, { "ht-tuning", "HT/HC Tuning", 0.314f },
    { "rs-level", "RS/CL Level", 0.46f }, { "cp-level", "CP/MA Level", 0.70f }, { "cb-level", "CB Level", 0.35f },
    { "cy-level", "CY Level", 0.60f }, { "cy-tone", "CY Tone", 0.4f }, { "cy-decay", "CY Decay", 0.577f },
    { "oh-level", "OH Level", 0.45f }, { "oh-decay", "OH Decay", 0.559f }, { "ch-level", "CH Level", 0.45f },
    { "master-volume", "Master Volume", 0.8f } };
const char* const kSwitchIds[5]   = { "lt-select", "mt-select", "ht-select", "rs-select", "cp-select" };
const char* const kSwitchNames[5] = { "LT/LC Select", "MT/MC Select", "HT/HC Select", "RS/CL Select", "CP/MA Select" };

// PRE-SCALE: step length in 16th notes. 1 = 16th triplets, 2 = 32nds, 3 = 16ths (normal), 4 = 8th triplets
constexpr double kStepLen16[4] = { 2.0 / 3.0, 0.5, 1.0, 4.0 / 3.0 };
constexpr int kAutoFill[6] = { 0, 16, 12, 8, 4, 2 };

juce::String tenths (float v, int) { return juce::String (v * 10.0f, 1); }
} // namespace

// The board inside: the UNIT 808 device's controls that are not on the front panel, in its own ranges.
// skew 0.5 = the device's exponent 2 dials. Defaults = the device's defaults (the sound of v0.3).
const Unit808Processor::TrimDef Unit808Processor::kTrims[kNumTrims] = {
    { "bd-tune", "BD Tune", 40, 65, 49.4f, 1, "Hz" }, { "bd-click", "BD Click", 0, 1, 0.01f, 1, "" }, { "bd-drive", "BD Drive", 1, 6, 1, 1, "" },
    { "sd-tune", "SD Tune", 0.8f, 1.25f, 1, 1, "x" }, { "sd-decay", "SD Decay", 0.5f, 2, 0.73f, 1, "x" },
    { "lt-tom-decay", "LT Tom Decay", 20, 400, 95, 0.5f, "ms" }, { "lt-tom-bend", "LT Tom Bend", 0, 0.5f, 0.03f, 1, "" },
    { "lt-conga-decay", "LC Conga Decay", 20, 400, 86, 0.5f, "ms" }, { "lt-conga-bend", "LC Conga Bend", 0, 0.5f, 0.03f, 1, "" },
    { "mt-tom-decay", "MT Tom Decay", 20, 400, 70, 0.5f, "ms" }, { "mt-tom-bend", "MT Tom Bend", 0, 0.5f, 0.03f, 1, "" },
    { "mt-conga-decay", "MC Conga Decay", 20, 400, 44, 0.5f, "ms" }, { "mt-conga-bend", "MC Conga Bend", 0, 0.5f, 0.03f, 1, "" },
    { "ht-tom-decay", "HT Tom Decay", 20, 400, 48, 0.5f, "ms" }, { "ht-tom-bend", "HT Tom Bend", 0, 0.5f, 0.03f, 1, "" },
    { "ht-conga-decay", "HC Conga Decay", 20, 400, 38, 0.5f, "ms" }, { "ht-conga-bend", "HC Conga Bend", 0, 0.5f, 0.03f, 1, "" },
    { "rs-tune", "RS Tune", 0.7f, 1.4f, 1, 1, "x" }, { "rs-decay", "RS Decay", 0.5f, 3, 1, 1, "x" },
    { "cl-tune", "CL Tune", 0.7f, 1.4f, 1, 1, "x" }, { "cl-decay", "CL Decay", 0.5f, 3, 1, 1, "x" },
    { "cp-tone", "CP Tone", 600, 5000, 1300, 1, "Hz" }, { "cp-decay", "CP Decay", 10, 300, 60, 0.5f, "ms" },
    { "cb-tune", "CB Tune", 0.5f, 2, 1, 1, "x" }, { "cb-decay", "CB Decay", 50, 1200, 97, 1, "ms" },
    { "cy-tune", "CY Tune", 0.5f, 2, 1, 1, "x" },
    { "hh-tune", "HH Tune", 0.5f, 2, 1, 1, "x" }, { "hh-tone", "HH Tone", 0.6f, 1.6f, 1, 1, "x" }, { "ch-decay", "CH Decay", 5, 100, 24, 1, "ms" } };

const char* const Unit808Processor::kInstrumentNames[kNumRows] = { "AC", "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH" };

juce::AudioProcessorValueTreeState::ParameterLayout Unit808Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& k : kKnobs)
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { k.id, 1 }, k.name, juce::NormalisableRange<float> (0.0f, 1.0f), k.def,
                                                            juce::AudioParameterFloatAttributes().withStringFromValueFunction (tenths)));
    for (int i = 0; i < 5; ++i)
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { kSwitchIds[i], 1 }, kSwitchNames[i], false,
                                                           juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                                                               [i] (bool v, int) { static const char* a[5][2] = { { "Low Tom", "Low Conga" }, { "Mid Tom", "Mid Conga" },
                                                                   { "Hi Tom", "Hi Conga" }, { "Rim Shot", "Claves" }, { "Hand Clap", "Maracas" } }; return juce::String (a[i][v ? 1 : 0]); })));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "swing", 1 }, "Swing", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                                        juce::AudioParameterFloatAttributes().withStringFromValueFunction (tenths)));
    // FLAM: the time between the grace note and the main hit of flammed steps, 5 .. 40 ms
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "flam", 1 }, "Flam", juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f,
                                                        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                            [] (float v, int) { return juce::String (5.0f + 35.0f * v, 1) + " ms"; })));
    for (int i = 0; i < kNumRows; ++i)
    {
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { muteId (i), 1 }, juce::String (kInstrumentNames[i]) + " Mute", false));
        if (i > 0) l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { soloId (i), 1 }, juce::String (kInstrumentNames[i]) + " Solo", false));
    }
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode",
        juce::StringArray { "Pattern Clear", "1st Part", "2nd Part", "Manual Play", "Play", "Compose" }, ManualPlay));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "instrument-select", 1 }, "Instrument Select",
        juce::StringArray { "1 AC", "2 BD", "3 SD", "4 LT", "5 MT", "6 HT", "7 RS", "8 CP", "9 CB", "10 CY", "11 OH", "12 CH" }, 1));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "auto-fill", 1 }, "Auto Fill In",
        juce::StringArray { "Manual", "16", "12", "8", "4", "2" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "basic-variation", 1 }, "Basic Variation", juce::StringArray { "A", "AB", "B" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "if-variation", 1 }, "I/F Variation", juce::StringArray { "A", "B" }, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "pre-scale", 1 }, "Pre-Scale", juce::StringArray { "1", "2", "3", "4" }, 2));
    for (auto& t : kTrims)
    {
        juce::NormalisableRange<float> r (t.lo, t.hi);
        r.skew = t.skew;
        const juce::String unit = t.unit;
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { juce::String ("t-") + t.pad, 1 }, juce::String ("Trim ") + t.name, r, t.def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([unit] (float v, int)
            {
                if (unit == "x")  return juce::String (v, 2) + " x";
                if (unit == "ms") return juce::String (juce::roundToInt (v)) + " ms";
                if (unit == "Hz") return juce::String (v, v < 100 ? 1 : 0) + " Hz";
                return juce::String (v, 2);
            })));
    }
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "standalone-bpm", 1 }, "Standalone Tempo",
                                                        juce::NormalisableRange<float> (40.0f, 300.0f, 0.1f), 120.0f,
                                                        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                            [] (float v, int) { return juce::String (v, 1) + " BPM"; })));
    return l;
}

Unit808Processor::Unit808Processor()
    : AudioProcessor (BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "UNIT808", createLayout())
{
    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    for (int i = 0; i < 23; ++i) raw.knob[i] = p (kKnobs[i].id);
    for (int i = 0; i < 5; ++i) raw.sw[i] = p (kSwitchIds[i]);
    raw.mode = p ("mode"); raw.instrument = p ("instrument-select"); raw.autoFill = p ("auto-fill");
    raw.basicVar = p ("basic-variation"); raw.fillVar = p ("if-variation"); raw.preScale = p ("pre-scale");
    for (int i = 0; i < kNumTrims; ++i) raw.trim[i] = apvts.getRawParameterValue (trimId (i));
    raw.swing = p ("swing"); raw.flam = p ("flam");
    for (int i = 0; i < kNumRows; ++i)
    {
        raw.mute[i] = apvts.getRawParameterValue (muteId (i));
        raw.solo[i] = i > 0 ? apvts.getRawParameterValue (soloId (i)) : raw.mute[i];
    }
    flamHits.reserve (64); raw.bpm = p ("standalone-bpm");
    cycle.reserve (8);
}

bool Unit808Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && layouts.inputBuses.isEmpty();
}

void Unit808Processor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    engine.prepare (sr);
    wasPlaying = false;
    playStep = -1;
}

void Unit808Processor::pullParams()
{
    auto k = [this] (int i) { return raw.knob[i]->load(); };
    auto s = [this] (int i) { return raw.sw[i]->load() > 0.5f; };
    Params& P = sound;
    P.accent = k (0);
    P.bdLevel = k (1); P.bdTone = k (2); P.bdDecay = k (3);
    P.sdLevel = k (4); P.sdTone = k (5); P.sdSnappy = k (6);
    P.ltLevel = k (7); P.ltTuning = k (8); P.mtLevel = k (9); P.mtTuning = k (10); P.htLevel = k (11); P.htTuning = k (12);
    P.rsLevel = k (13); P.cpLevel = k (14); P.cbLevel = k (15);
    P.cyLevel = k (16); P.cyTone = k (17); P.cyDecay = k (18);
    P.ohLevel = k (19); P.ohDecay = k (20); P.chLevel = k (21); P.master = k (22);
    auto t = [this] (int i) { return raw.trim[i]->load(); };
    P.bdTune = t (0); P.bdClick = t (1); P.bdDrive = t (2); P.sdTune = t (3); P.sdDecay = t (4);
    for (int d = 0; d < 3; ++d)
    {
        P.tomDecay[d] = t (5 + d * 4); P.tomBend[d] = t (6 + d * 4); P.congaDecay[d] = t (7 + d * 4); P.congaBend[d] = t (8 + d * 4);
    }
    P.rsTune = t (17); P.rsDecay = t (18); P.clTune = t (19); P.clDecay = t (20);
    P.cpTone = t (21); P.cpDecay = t (22); P.cbTune = t (23); P.cbDecay = t (24); P.cyTune = t (25);
    P.hhTune = t (26); P.hhTone = t (27); P.chDecay = t (28);
    P.ltConga = s (0); P.mtConga = s (1); P.htConga = s (2); P.rsClaves = s (3); P.cpMaracas = s (4);
    engine.setParams (P);
}

// ---- panel actions -------------------------------------------------------------------------------
void Unit808Processor::stepKey (int key, juce::ModifierKeys mods)
{
    if (key < 0 || key >= 16) return;
    const int mode = getMode();
    if (mode == FirstPart || mode == SecondPart)
    {
        auto& part = memory.rhythm[selectedSlot.load()].part[editVar()][editPart()];
        if (mods.isShiftDown() || stepNumberArmed.load())
        {
            part.length = key + 1;          // STEP NUMBER: CLEAR (or Shift) + step key = the part's last step
            stepNumberArmed = false;
        }
        else if (mods.isAltDown() && getInstrument() > 0)
        {
            // FLAM: Alt/Option-click turns the flam of the selected instrument on this step on / off
            if (editPart() == 1 && part.length.load() == 0) part.length = 16;
            part.setFlam (getInstrument(), key, ! part.flam (getInstrument(), key));
        }
        else
        {
            if (editPart() == 1 && part.length.load() == 0) part.length = 16;
            part.set (getInstrument(), key, ! part.cell (getInstrument(), key));
        }
        ++memory.version;
    }
    else if (mode == Compose)
    {
        composeChoice = key;
        selectedSlot = key;
    }
    else   // Pattern Clear, Manual Play, Play: 1-12 choose the basic rhythm, 13-16 the intro / fill in
    {
        selectedSlot = key;
        if (key < kFirstFill) pendingSlot = key;
        else selectedFill = key;
    }
    ++uiVersion;
}

void Unit808Processor::setStep (int key, bool on)
{
    const int mode = getMode();
    if (key < 0 || key >= 16 || (mode != FirstPart && mode != SecondPart)) return;
    auto& part = memory.rhythm[selectedSlot.load()].part[editVar()][editPart()];
    if (editPart() == 1 && part.length.load() == 0) part.length = 16;
    part.set (getInstrument(), key, on);
    ++memory.version; ++uiVersion;
}

bool Unit808Processor::stepIsSet (int key) const { return key >= 0 && key < 16 && editedPart().cell (getInstrument(), key); }

void Unit808Processor::tapKey()
{
    const int mode = getMode();
    if (mode == FirstPart || mode == SecondPart)
    {
        // tap-write the selected instrument on the playing step
        const int st = playStep.load();
        if (st >= 0 && playSlot.load() == selectedSlot.load())
        {
            auto& part = memory.rhythm[selectedSlot.load()].part[editVar()][editPart()];
            part.set (getInstrument(), st, true);
            ++memory.version;
        }
    }
    else if (mode == Compose)
    {
        if (composeChoice.load() >= 0) memory.append (getTrack(), { composeChoice.load(), getBasicVar() });
    }
    else if (isRunning())
        fillRequested = true;          // INTRO SET / FILL IN TRIGGER: the fill plays as the next measure
    ++uiVersion;
}

void Unit808Processor::clearKey (juce::ModifierKeys mods)
{
    const int mode = getMode();
    if (mode == PatternClear) memory.clearRhythm (selectedSlot.load(), getBasicVar());
    else if (mode == Compose) { memory.clearTrack (getTrack()); composeChoice = -1; }
    else if (mode == FirstPart || mode == SecondPart)
    {
        if (mods.isAltDown())          // clear the selected instrument in this part
        {
            auto& part = memory.rhythm[selectedSlot.load()].part[editVar()][editPart()];
            part.clearRow (getInstrument());
            ++memory.version;
        }
        else stepNumberArmed = ! stepNumberArmed.load();
    }
    ++uiVersion;
}

void Unit808Processor::startStopKey() { if (isStandalone()) runEnabled = ! runEnabled.load(); ++uiVersion; }

// ---- copy / paste --------------------------------------------------------------------------------
Unit808Processor::PartData Unit808Processor::save (const Part& p)
{
    PartData d;
    for (int r = 0; r < kNumRows; ++r) { d.rows[r] = p.rows[r].load(); d.flams[r] = p.flams[r].load(); }
    d.length = p.length.load();
    return d;
}

void Unit808Processor::load (Part& p, const PartData& d)
{
    for (int r = 0; r < kNumRows; ++r) { p.rows[r] = d.rows[r]; p.flams[r] = d.flams[r]; }
    p.length = d.length;
}

void Unit808Processor::keepUndo (int slot)
{
    undo.valid = true; undo.slot = slot;
    for (int v = 0; v < 2; ++v) for (int p = 0; p < 2; ++p) undo.parts[v][p] = save (memory.rhythm[slot].part[v][p]);
}

void Unit808Processor::copyRhythm (int slot)
{
    clip.kind = ClipRhythm; clip.fromSlot = slot;
    for (int v = 0; v < 2; ++v) for (int p = 0; p < 2; ++p) clip.parts[v][p] = save (memory.rhythm[slot].part[v][p]);
}

void Unit808Processor::copyVariation (int slot, int var)
{
    clip.kind = ClipVariation; clip.fromSlot = slot; clip.fromVar = var;
    for (int p = 0; p < 2; ++p) clip.parts[0][p] = save (memory.rhythm[slot].part[var][p]);
}

void Unit808Processor::copyPart (int slot, int var, int part)
{
    clip.kind = ClipPart; clip.fromSlot = slot; clip.fromVar = var; clip.fromPart = part;
    clip.parts[0][0] = save (memory.rhythm[slot].part[var][part]);
}

void Unit808Processor::copyRow (int slot, int var, int part, int row)
{
    clip.kind = ClipRow; clip.fromSlot = slot; clip.fromVar = var; clip.fromPart = part; clip.row = row;
    clip.parts[0][0] = save (memory.rhythm[slot].part[var][part]);
}

bool Unit808Processor::pasteRhythm (int slot)
{
    if (clip.kind != ClipRhythm) return false;
    keepUndo (slot);
    for (int v = 0; v < 2; ++v) for (int p = 0; p < 2; ++p) load (memory.rhythm[slot].part[v][p], clip.parts[v][p]);
    ++memory.version; ++uiVersion;
    return true;
}

bool Unit808Processor::pasteVariation (int slot, int var)
{
    if (clip.kind != ClipVariation) return false;
    keepUndo (slot);
    for (int p = 0; p < 2; ++p) load (memory.rhythm[slot].part[var][p], clip.parts[0][p]);
    ++memory.version; ++uiVersion;
    return true;
}

bool Unit808Processor::pastePart (int slot, int var, int part)
{
    if (clip.kind != ClipPart) return false;
    keepUndo (slot);
    auto d = clip.parts[0][0];
    if (part == 0 && d.length == 0) d.length = 16;          // a 1st part always has steps
    load (memory.rhythm[slot].part[var][part], d);
    ++memory.version; ++uiVersion;
    return true;
}

bool Unit808Processor::pasteRow (int slot, int var, int part, int row)
{
    if (clip.kind != ClipRow) return false;
    keepUndo (slot);
    auto& dst = memory.rhythm[slot].part[var][part];
    dst.rows[row] = clip.parts[0][0].rows[clip.row];
    dst.flams[row] = clip.parts[0][0].flams[clip.row];
    if (part == 1 && dst.length.load() == 0) dst.length = 16;
    ++memory.version; ++uiVersion;
    return true;
}

void Unit808Processor::undoPaste()
{
    if (! undo.valid) return;
    for (int v = 0; v < 2; ++v) for (int p = 0; p < 2; ++p) load (memory.rhythm[undo.slot].part[v][p], undo.parts[v][p]);
    undo.valid = false;
    ++memory.version; ++uiVersion;
}

static juce::String slotName (int slot)
{
    return slot < kFirstFill ? "rhythm " + juce::String (slot + 1) : "intro/fill " + juce::String (slot - kFirstFill + 1);
}

juce::String Unit808Processor::clipDescription() const
{
    const char* v[2] = { "A", "B" };
    const char* p[2] = { "1st", "2nd" };
    switch (clip.kind)
    {
        case ClipRhythm:    return slotName (clip.fromSlot);
        case ClipVariation: return slotName (clip.fromSlot) + " " + v[clip.fromVar];
        case ClipPart:      return slotName (clip.fromSlot) + " " + v[clip.fromVar] + " " + p[clip.fromPart] + " part";
        case ClipRow:       return juce::String (kInstrumentNames[clip.row]) + " of " + slotName (clip.fromSlot) + " " + v[clip.fromVar] + " " + p[clip.fromPart] + " part";
        default:            return {};
    }
}

// ---- sequencer (audio thread) ----------------------------------------------------------------------
void Unit808Processor::segmentFor (int slot, int varMode, std::vector<Segment>& out) const
{
    const int vars[2] = { varMode == VarB ? 1 : 0, 1 };
    const int nv = varMode == VarAB ? 2 : 1;
    for (int i = 0; i < nv; ++i)
    {
        out.push_back ({ slot, vars[i], 0 });
        if (memory.rhythm[slot].part[vars[i]][1].length.load() > 0) out.push_back ({ slot, vars[i], 1 });
    }
}

int Unit808Processor::partLength (const Segment& s) const
{
    return juce::jlimit (1, 16, memory.rhythm[s.slot].part[s.var][s.part].length.load());
}

void Unit808Processor::buildCycle()
{
    cycle.clear();
    cycleIsFill = false;
    const int mode = getMode();
    if (mode == FirstPart || mode == SecondPart)
    {
        cycle.push_back ({ selectedSlot.load(), editVar(), editPart() });   // writing: loop the part being written
        return;
    }
    const int track = getTrack();
    const int tlen = memory.trackLen[track].load();
    if ((mode == Play || mode == Compose) && tlen > 0)
    {
        trackPos = (trackPos + 1) % tlen;
        const auto e = memory.entry (track, trackPos);
        currentRhythm = e.slot;
        segmentFor (e.slot, e.var, cycle);
        return;
    }
    if (pendingSlot.load() >= 0) { currentRhythm = pendingSlot.load(); pendingSlot = -1; }
    const int n = kAutoFill[juce::jlimit (0, 5, (int) raw.autoFill->load())];
    const bool doFill = fillRequested.load() || (n > 0 && measureCount % n == n - 1);
    if (doFill)
    {
        segmentFor (selectedFill.load(), getFillVar() == 0 ? VarA : VarB, cycle);
        fillRequested = false;
        cycleIsFill = true;
    }
    else segmentFor (currentRhythm, getBasicVar(), cycle);
    ++measureCount;
}

void Unit808Processor::advanceStep()
{
    if (cycle.empty()) { buildCycle(); segIndex = 0; stepInSeg = -1; }
    if (++stepInSeg >= partLength (cycle[(size_t) segIndex]))
    {
        stepInSeg = 0;
        if (++segIndex >= (int) cycle.size()) { buildCycle(); segIndex = 0; }
    }
    const auto& seg = cycle[(size_t) segIndex];
    const auto& part = memory.rhythm[seg.slot].part[seg.var][seg.part];
    const bool acc = part.cell (0, stepInSeg) && ! isMuted (0);   // AC muted: no accents
    const int flamSamples = juce::roundToInt ((5.0 + 35.0 * raw.flam->load()) * 0.001 * sampleRate);
    for (int r = 1; r < kNumRows; ++r)
    {
        if (! part.cell (r, stepInSeg) || ! voiceAllowed (r)) continue;
        if (part.flam (r, stepInSeg))
        {
            engine.triggerLevel ((Voice) (r - 1), acc, 0.55);           // the grace note, softer, on the step
            if (flamHits.size() < 64) flamHits.push_back ({ flamSamples, r - 1, acc });   // the main hit follows
        }
        else engine.trigger ((Voice) (r - 1), acc);
    }
    playSlot = seg.slot; playVar = seg.var; playPart = seg.part; playStep = stepInSeg;
    ++uiVersion;
}

bool Unit808Processor::voiceAllowed (int row) const
{
    if (isMuted (row)) return false;
    bool anySolo = false;
    for (int i = 1; i < kNumRows; ++i) anySolo |= isSoloed (i);
    return ! anySolo || isSoloed (row);
}

// A mode change keeps the place in the bar: the new mode carries on from the same step, lined up with the song
void Unit808Processor::realign()
{
    cycle.clear(); trackPos = -1; measureCount = 0;
    buildCycle();
    segIndex = 0;
    const int len0 = partLength (cycle[0]);
    stepInSeg = (int) (((nextAbsStep % len0) + len0) % len0) - 1;
}

static int noteToVoice (int note, int& alt)
{
    alt = -1;
    switch (note)
    {
        case 35: case 36: return BD;
        case 37: alt = 0; return RS;
        case 75: alt = 1; return RS;               // claves
        case 38: case 40: return SD;
        case 39: alt = 0; return CP;
        case 70: alt = 1; return CP;               // maracas
        case 41: case 43: alt = 0; return LT;
        case 64: alt = 1; return LT;               // low conga
        case 45: case 47: alt = 0; return MT;
        case 63: alt = 1; return MT;               // open hi conga -> mid conga
        case 48: case 50: alt = 0; return HT;
        case 62: alt = 1; return HT;               // mute hi conga -> hi conga
        case 42: case 44: return CH;
        case 46: return OH;
        case 49: case 51: case 52: case 55: case 57: case 59: return CY;
        case 56: return CB;
        default: return -1;
    }
}

void Unit808Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    pullParams();

    // ---- transport: always the host's; the standalone app has its own clock
    bool playing = false, hostTransport = false;
    double ppq = 0.0, bpm = 120.0;
    if (! isStandalone())
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto b = pos->getBpm()) bpm = *b;
                if (auto q = pos->getPpqPosition()) { hostTransport = true; playing = pos->getIsPlaying(); ppq = *q; }
            }
    if (! hostTransport)
    {
        bpm = raw.bpm->load();
        playing = runEnabled.load();
        if (! playing) internalPpq = 0.0;
        ppq = internalPpq;
    }
    transportRunning = playing;
    bpm = juce::jlimit (20.0, 999.0, bpm);
    const double sps = bpm / 60.0 * 4.0 / sampleRate;         // 16th notes per sample
    if (! hostTransport && playing) internalPpq += numSamples * sps / 4.0;

    const int mode = getMode();
    if (mode != lastMode)
    {
        if (lastMode >= 0) realignPending = true;
        lastMode = mode;
    }
    if (! playing)
    {
        if (wasPlaying) ++uiVersion;
        wasPlaying = false;
        playStep = -1;
        trackPos = -1;
        cycle.clear();
        measureCount = 0;
        fillRequested = false;
        realignPending = false;
        if (pendingSlot.load() >= 0 && pendingSlot.load() < kFirstFill) { currentRhythm = pendingSlot.load(); pendingSlot = -1; }
    }

    // ---- step timing: PRE-SCALE step length, SWING delays every second step (50 % .. 75 %)
    const double L = kStepLen16[juce::jlimit (0, 3, (int) raw.preScale->load())];
    const double swingAmt = juce::jlimit (0.0, 0.5, 0.5 * (double) raw.swing->load());
    auto stepTime = [&] (long long k) { return ((double) k + ((k & 1) ? swingAmt : 0.0)) * L; };
    auto firstAtOrAfter = [&] (double q) { long long k = (long long) std::floor (q / L); return stepTime (k) >= q - 1e-9 ? k : k + 1; };

    const double q0 = ppq * 4.0;
    if (playing && wasPlaying && std::abs (q0 - lastQ) > juce::jmax (4.0 * sps, 0.02))
        wasPlaying = false;                                   // the host jumped or looped: line up again
    if (playing && ! wasPlaying)
    {
        nextAbsStep = firstAtOrAfter (q0);
        realign();                                            // patterns line up with the song
        wasPlaying = true;
        realignPending = false;
    }
    if (playing && realignPending) { realign(); realignPending = false; }

    buffer.clear();
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    int done = 0;
    auto renderTo = [&] (int end) { if (end > done) { engine.render (left + done, end - done); done = end; } };

    auto midiIt = midi.cbegin();
    for (int i = 0; i < numSamples; ++i)
    {
        if (playing)
        {
            const double q = q0 + i * sps;
            while (stepTime (nextAbsStep) < q + sps)
            {
                renderTo (i);
                advanceStep();
                ++nextAbsStep;
            }
        }
        for (size_t f = 0; f < flamHits.size();)                    // flam main hits
        {
            if (--flamHits[f].samples <= 0)
            {
                renderTo (i);
                engine.trigger ((Voice) flamHits[f].voice, flamHits[f].accent);
                flamHits.erase (flamHits.begin() + (long) f);
            }
            else ++f;
        }
        for (; midiIt != midi.cend() && (*midiIt).samplePosition <= i; ++midiIt)
        {
            const auto m = (*midiIt).getMessage();
            if (! m.isNoteOn()) continue;
            int alt;
            const int v = noteToVoice (m.getNoteNumber(), alt);
            if (v < 0 || ! voiceAllowed (v + 1)) continue;
            renderTo (i);
            engine.triggerVelocity ((Voice) v, m.getVelocity(), alt);
        }
    }
    renderTo (numSamples);
    lastQ = q0 + numSamples * sps;

    if (right) std::copy (left, left + numSamples, right);
}

// ---- state: parameters + rhythm memory + tracks ---------------------------------------------------
void Unit808Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("MEMORY"), nullptr);
    juce::ValueTree mem ("MEMORY");
    mem.setProperty ("selected", selectedSlot.load(), nullptr);
    mem.setProperty ("fill", selectedFill.load(), nullptr);
    mem.setProperty ("rhythm", currentRhythm.load(), nullptr);
    for (int s = 0; s < kNumSlots; ++s)
        for (int v = 0; v < 2; ++v)
            for (int p = 0; p < 2; ++p)
            {
                const auto& part = memory.rhythm[s].part[v][p];
                juce::StringArray rows;
                for (int r = 0; r < kNumRows; ++r) rows.add (juce::String::toHexString ((int) part.rows[r].load()));
                const juce::String key = "r" + juce::String (s) + "v" + juce::String (v) + "p" + juce::String (p);
                mem.setProperty (key, rows.joinIntoString (" "), nullptr);
                mem.setProperty (key + "len", part.length.load(), nullptr);
                juce::StringArray fl;
                for (int r = 0; r < kNumRows; ++r) fl.add (juce::String::toHexString ((int) part.flams[r].load()));
                mem.setProperty (key + "flam", fl.joinIntoString (" "), nullptr);
            }
    for (int t = 0; t < kNumTracks; ++t)
    {
        juce::StringArray e;
        for (int i = 0; i < memory.trackLen[t].load(); ++i) e.add (juce::String (memory.trackEntries[t][i].load()));
        mem.setProperty ("track" + juce::String (t), e.joinIntoString (" "), nullptr);
    }
    state.appendChild (mem, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void Unit808Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    auto mem = state.getChildWithName ("MEMORY");
    if (mem.isValid())
    {
        for (int s = 0; s < kNumSlots; ++s)
            for (int v = 0; v < 2; ++v)
                for (int p = 0; p < 2; ++p)
                {
                    auto& part = memory.rhythm[s].part[v][p];
                    const juce::String key = "r" + juce::String (s) + "v" + juce::String (v) + "p" + juce::String (p);
                    juce::StringArray rows;
                    rows.addTokens (mem.getProperty (key).toString(), " ", "");
                    for (int r = 0; r < kNumRows; ++r) part.rows[r] = (uint16_t) (r < rows.size() ? rows[r].getHexValue32() & 0xffff : 0);
                    part.length = juce::jlimit (p == 0 ? 1 : 0, 16, (int) mem.getProperty (key + "len", p == 0 ? 16 : 0));
                    juce::StringArray fl;
                    fl.addTokens (mem.getProperty (key + "flam").toString(), " ", "");
                    for (int r = 0; r < kNumRows; ++r) part.flams[r] = (uint16_t) (r < fl.size() ? fl[r].getHexValue32() & 0xffff : 0);
                }
        for (int t = 0; t < kNumTracks; ++t)
        {
            juce::StringArray e;
            e.addTokens (mem.getProperty ("track" + juce::String (t)).toString(), " ", "");
            int n = 0;
            for (auto& x : e) if (x.isNotEmpty() && n < kMaxTrack) memory.trackEntries[t][n++] = juce::jlimit (0, kNumSlots * 4 - 1, x.getIntValue());
            memory.trackLen[t] = n;
        }
        selectedSlot = juce::jlimit (0, kNumSlots - 1, (int) mem.getProperty ("selected", 0));
        selectedFill = juce::jlimit (kFirstFill, kNumSlots - 1, (int) mem.getProperty ("fill", kFirstFill));
        currentRhythm = juce::jlimit (0, kFirstFill - 1, (int) mem.getProperty ("rhythm", 0));
        state.removeChild (mem, nullptr);
        ++memory.version;
    }
    pendingSlot = -1;
    apvts.replaceState (state);
    ++uiVersion;
}

juce::AudioProcessorEditor* Unit808Processor::createEditor() { return new Unit808Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Unit808Processor(); }
