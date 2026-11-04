// Headless tests for UNIT808: modes, writing, variations, 2nd part, fills, auto fill, tracks,
// swing, pre-scale, host jumps, MIDI, state; writes audio and editor snapshots.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

struct FakeHost : juce::AudioPlayHead
{
    bool playing = false;
    double ppq = 0.0, bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        return p;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();
    using P = Unit808Processor;

    P proc;
    FakeHost host;
    proc.setPlayHead (&host);
    const double sr = 48000.0;
    const int block = 256;
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (2, block);
    auto setParam = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    juce::ModifierKeys none, shift (juce::ModifierKeys::shiftModifier), alt (juce::ModifierKeys::altModifier);

    std::vector<float> rec;
    struct StepLog { long long sample; int slot, var, part, step; };
    std::vector<StepLog> log;
    long long t = 0;
    int bad = 0, lastSeen = -2, lastSlot = -1, lastPart = -1;
    auto run = [&] (int blocks, juce::MidiBuffer* midiIn = nullptr)
    {
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (midiIn && b == 0) midi = *midiIn;
            proc.processBlock (buf, midi);
            if (host.playing) host.ppq += block / sr * host.bpm / 60.0;
            const int st = proc.getPlayStep();
            if (st >= 0 && (st != lastSeen || proc.getPlaySlot() != lastSlot || proc.getPlayPart() != lastPart))
                log.push_back ({ t, proc.getPlaySlot(), proc.getPlayVar(), proc.getPlayPart(), st });
            lastSeen = st; lastSlot = proc.getPlaySlot(); lastPart = proc.getPlayPart();
            for (int i = 0; i < block; ++i) { const float v = buf.getSample (0, i); if (! std::isfinite (v)) ++bad; rec.push_back (v); }
            t += block;
        }
    };
    const int blocksPerBar = (int) std::ceil (sr * 2.0 / block);   // 120 BPM: one 16-step bar = 2 s

    check (proc.getMode() == P::ManualPlay && proc.getInstrument() == 1 && proc.getBasicVar() == P::VarA, "defaults: Manual Play, BD, variation A");

    // ---- write rhythm 1, variation A, 1st part: a basic beat
    setParam ("mode", P::FirstPart);
    auto write = [&] (int inst, std::initializer_list<int> steps) { setParam ("instrument-select", (float) inst); for (int s : steps) proc.stepKey (s, none); };
    write (1, { 0, 6, 10 });                       // BD
    write (2, { 4, 12 });                          // SD
    write (11, { 0, 2, 4, 6, 8, 10, 12, 14 });     // CH
    write (0, { 0, 8 });                           // AC
    const auto& a1 = proc.memory.rhythm[0].part[0][0];
    check (a1.rows[1].load() == ((1 << 0) | (1 << 6) | (1 << 10)) && a1.rows[0].load() == 0x0101, "1st Part writes the selected instrument (BD, AC)");
    setParam ("instrument-select", 1.0f);
    proc.stepKey (6, none);
    check (! a1.cell (1, 6) && a1.rows[0].load() == 0x0101, "pressing a set step again clears it (only on the selected instrument)");
    proc.stepKey (6, none);
    check (a1.cell (1, 6), "and sets it again");

    // step number: CLEAR then a step key, or Shift-click
    proc.clearKey (none);
    check (proc.isStepNumberArmed(), "CLEAR in 1st Part arms STEP NUMBER");
    proc.stepKey (11, none);
    check (a1.length.load() == 12 && ! proc.isStepNumberArmed(), "CLEAR + step 12: the part has 12 steps");
    proc.stepKey (15, shift);
    check (a1.length.load() == 16, "Shift-click step 16: back to 16 steps");

    // variation B: the lever chooses which variation is written
    setParam ("basic-variation", P::VarB);
    write (1, { 0, 8 });
    check (proc.memory.rhythm[0].part[1][0].rows[1].load() == 0x0101 && a1.rows[1].load() != 0x0101, "lever on B: writes variation B");

    // 2nd part of variation A
    setParam ("basic-variation", P::VarA);
    setParam ("mode", P::SecondPart);
    check (proc.memory.rhythm[0].part[0][1].length.load() == 0, "2nd part is unused until written");
    write (3, { 0, 4, 8, 12 });                    // LT
    check (proc.memory.rhythm[0].part[0][1].length.load() == 16 && proc.memory.rhythm[0].part[0][1].rows[3].load() == 0x1111, "writing the 2nd part makes it 16 steps");
    proc.clearKey (alt);
    check (proc.memory.rhythm[0].part[0][1].rows[3].load() == 0, "Alt-CLEAR clears the instrument in the part");
    write (3, { 0, 4, 8, 12 });

    // fill pattern (slot 13 = step key 13) variation A
    setParam ("mode", P::ManualPlay);
    proc.stepKey (12, none);
    check (proc.getSelectedFill() == 12 && proc.getSelectedSlot() == 12, "Manual Play: step key 13 picks intro/fill 1");
    setParam ("mode", P::FirstPart);
    write (2, { 0, 2, 4, 6, 8, 10, 12, 14 });     // snare roll
    setParam ("mode", P::ManualPlay);
    proc.stepKey (0, none);                        // back to rhythm 1

    // ---- play: variation A = A1 then A2 (2nd part)
    host.playing = true; host.ppq = 0.0;
    log.clear();
    run (blocksPerBar * 2 + 2);
    bool order = log.size() >= 32;
    for (size_t i = 0; i < 32 && order; ++i) order = log[i].slot == 0 && log[i].var == 0 && log[i].part == (i < 16 ? 0 : 1) && log[i].step == (int) (i % 16);
    check (order, "Manual Play, variation A with a 2nd part: plays 1st part then 2nd part, 16 steps each");
    // step timing: 16ths at 120 BPM = 6000 samples, first step at 0
    bool timing = log.size() >= 17;
    for (size_t i = 1; i < 17 && timing; ++i) timing = std::llabs ((log[i].sample - log[i - 1].sample) - 6000) <= block;
    check (timing && log[0].sample == 0, "steps follow the host tempo (16ths at 120 BPM)");

    // AB alternates
    host.playing = false; run (2);
    setParam ("basic-variation", P::VarAB);
    host.playing = true; host.ppq = 0.0; log.clear();
    run (blocksPerBar * 3 + 2);
    check (log.size() >= 48 && log[0].var == 0 && log[16].var == 0 && log[16].part == 1 && log[32].var == 1, "AB: A (both parts) then B");
    host.playing = false; run (2);
    setParam ("basic-variation", P::VarA);

    // fill in trigger: TAP while playing -> next measure is the fill
    host.playing = true; host.ppq = 0.0; log.clear();
    run (4);
    proc.tapKey();
    check (proc.isFillRequested(), "TAP while running requests the fill in");
    run (blocksPerBar * 3);
    bool sawFill = false;
    for (auto& l : log) sawFill |= l.slot == 12;
    check (sawFill && ! proc.isFillRequested(), "the intro/fill pattern plays in the next measure");
    host.playing = false; run (2);

    // auto fill every 2 measures (rhythm without 2nd part: write rhythm 2 = slot 1)
    setParam ("mode", P::ManualPlay); proc.stepKey (1, none);
    setParam ("mode", P::FirstPart); write (1, { 0, 4, 8, 12 });
    setParam ("mode", P::ManualPlay);
    setParam ("auto-fill", 5.0f);                 // "2"
    host.playing = true; host.ppq = 0.0; log.clear();
    run (blocksPerBar * 4 + 2);
    std::vector<int> bars;
    for (auto& l : log) if (l.step == 0) bars.push_back (l.slot);
    check (bars.size() >= 4 && bars[0] == 1 && bars[1] == 12 && bars[2] == 1 && bars[3] == 12, "auto fill in 2: every 2nd measure is the fill");
    host.playing = false; run (2);
    setParam ("auto-fill", 0.0f);

    // queued rhythm change happens at the end of the measure
    host.playing = true; host.ppq = 0.0; log.clear();
    run (8);
    proc.stepKey (0, none);
    check (proc.getPendingSlot() == 0, "choosing a rhythm while playing queues it");
    run (blocksPerBar + 2);
    check (log.front().slot == 1 && log.back().slot == 0, "it takes over at the next measure");
    host.playing = false; run (2);

    // ---- compose a track: rhythm 1, rhythm 2, rhythm 2 on track 3
    setParam ("mode", P::Compose);
    setParam ("instrument-select", 2.0f);          // track 3
    proc.stepKey (0, none); proc.tapKey();
    proc.stepKey (1, none); proc.tapKey(); proc.tapKey();
    check (proc.memory.trackLen[2].load() == 3 && proc.memory.entry (2, 1).slot == 1, "Compose: step key + TAP adds rhythms to the track");
    setParam ("mode", P::Play);
    host.playing = true; host.ppq = 0.0; log.clear();
    run (blocksPerBar * 5 + 2);
    bars.clear();
    for (auto& l : log) if (l.step == 0) bars.push_back (l.slot * 10 + l.part);
    // rhythm 1 has a 2nd part: 1A1 1A2 2A1 2A1 1A1
    check (bars.size() >= 5 && bars[0] == 0 && bars[1] == 1 && bars[2] == 10 && bars[3] == 10 && bars[4] == 0, "Play: the track plays in order and loops");
    host.playing = false; run (2);
    setParam ("mode", P::Compose); proc.clearKey (none);
    check (proc.memory.trackLen[2].load() == 0, "Compose: CLEAR clears the track (TRACK CLEAR)");

    // ---- swing and pre-scale
    setParam ("mode", P::ManualPlay); proc.stepKey (1, none);
    setParam ("swing", 1.0f);
    host.playing = true; host.ppq = 0.0; log.clear();
    run (blocksPerBar / 2);
    check (log.size() >= 3 && std::llabs ((log[1].sample - log[0].sample) - 9000) <= block, "swing at 10: the 2nd step comes 75 % into the pair");
    host.playing = false; run (2);
    setParam ("swing", 0.0f);
    setParam ("pre-scale", 1.0f);                  // 2 = 32nd notes
    host.playing = true; host.ppq = 0.0; log.clear();
    run (blocksPerBar / 2);
    check (log.size() >= 3 && std::llabs ((log[1].sample - log[0].sample) - 3000) <= block, "PRE-SCALE 2: 32nd-note steps");
    host.playing = false; run (2);
    setParam ("pre-scale", 2.0f);

    // host jump: lines the pattern up with the song again
    host.playing = true; host.ppq = 0.0; log.clear();
    run (blocksPerBar / 2);
    host.ppq = 4.0 * 3 + 1.0;                      // bar 4, beat 2
    log.clear();
    run (8);
    check (! log.empty() && log.front().step == 4, "a host jump to beat 2 of a bar plays step 5");
    host.playing = false; run (4);

    // ---- pattern clear: clears the chosen rhythm
    setParam ("mode", P::PatternClear); proc.stepKey (1, none); proc.clearKey (none);
    check (proc.memory.rhythm[1].part[0][0].empty(), "Pattern Clear + CLEAR clears the rhythm");

    // ---- MIDI: GM notes play the voices; low conga note plays the conga even with the switch on tom
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 0);
        m.addEvent (juce::MidiMessage::noteOn (10, 64, (juce::uint8) 100), 100);
        rec.clear();
        run (40, &m);
        float pk = 0; for (float v : rec) pk = juce::jmax (pk, std::abs (v));
        check (pk > 0.05f, "MIDI notes play the sounds");
    }

    // ---- power off mutes
    setParam ("power", 0.0f);
    {
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (10, 38, (juce::uint8) 127), 0);
        rec.clear(); run (20, &m);
        float pk = 0; for (float v : rec) pk = juce::jmax (pk, std::abs (v));
        check (pk == 0.0f, "power off: no sound");
    }
    setParam ("power", 1.0f);

    // ---- state round trip
    {
        juce::MemoryBlock mb;
        setParam ("bd-decay", 0.9f);
        proc.getStateInformation (mb);
        P other;
        other.setStateInformation (mb.getData(), (int) mb.getSize());
        const auto& o = other.memory.rhythm[0];
        check (o.part[0][0].rows[1].load() == proc.memory.rhythm[0].part[0][0].rows[1].load() && o.part[0][1].length.load() == 16
                   && o.part[1][0].rows[1].load() == 0x0101 && other.memory.rhythm[12].part[0][0].rows[2].load() == 0x5555
                   && std::abs (other.apvts.getRawParameterValue ("bd-decay")->load() - 0.9f) < 1e-4f,
               "state: rhythms, parts, lengths and knobs are saved with the project");
    }
    check (bad == 0, "no NaN / Inf in the output");

    // ---- a demo: rhythm 1 (A, with 2nd part) four bars with the fill, at 120 BPM
    {
        setParam ("bd-decay", 0.45f);
        setParam ("mode", P::ManualPlay); proc.stepKey (0, none);
        setParam ("auto-fill", 4.0f);              // fill every 4
        rec.clear();
        host.playing = true; host.ppq = 0.0;
        run (blocksPerBar * 4);
        host.playing = false; run (blocksPerBar / 2);
        juce::AudioBuffer<float> out (1, (int) rec.size());
        out.copyFrom (0, 0, rec.data(), (int) rec.size());
        juce::WavAudioFormat wav;
        auto f = outDir.getChildFile ("unit808_demo.wav");
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        if (os != nullptr)
            if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (1).withBitsPerSample (24)))
            { w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples()); }
    }

    // ---- editor snapshots
    {
        setParam ("mode", P::FirstPart);
        setParam ("instrument-select", 11.0f);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        for (float scale : { 0.6f, 1.0f })
        {
            ed->setSize ((int) (1870 * scale), (int) (1090 * scale));
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile (juce::String ("editor_") + juce::String (juce::roundToInt (scale * 100)) + ".png");
            f.deleteFile();
            juce::PNGImageFormat png;
            if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream())) png.writeImageToStream (img, *os);
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
