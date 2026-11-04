#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Tr808Engine.h"
#include "Sequencer.h"

// UNIT808: the UNIT 808 sound engine behind a TR-808-style panel and sequencer.
//   MODE   Pattern Clear / 1st Part / 2nd Part / Manual Play / Play / Compose
//   12 basic rhythms + 4 intro/fill patterns, each with variation A and B, each with a 1st and 2nd part
//   12 rhythm tracks (songs). Tempo and transport always follow the DAW.
class Unit808Processor : public juce::AudioProcessor
{
public:
    Unit808Processor();
    ~Unit808Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    // the 29 trimmers on the board inside: parameter id "t-" + pad id
    struct TrimDef { const char* pad; const char* name; float lo, hi, def, skew; const char* unit; };
    static constexpr int kNumTrims = 29;
    static const TrimDef kTrims[kNumTrims];
    static juce::String trimId (int i) { return juce::String ("t-") + kTrims[i].pad; }
    juce::AudioProcessorValueTreeState apvts;

    enum Mode { PatternClear = 0, FirstPart, SecondPart, ManualPlay, Play, Compose };
    enum Variation { VarA = 0, VarAB, VarB };
    static const char* const kInstrumentNames[unit808::kNumRows];

    unit808::Memory memory;

    // ---- panel state (read by the editor)
    int  getMode() const        { return juce::jlimit (0, 5, (int) raw.mode->load()); }
    int  getInstrument() const  { return juce::jlimit (0, 11, (int) raw.instrument->load()); }
    int  getBasicVar() const    { return juce::jlimit (0, 2, (int) raw.basicVar->load()); }
    int  getFillVar() const     { return juce::jlimit (0, 1, (int) raw.fillVar->load()); }
    bool isPowerOn() const      { return true; }   // POWER now opens the board inside; UNIT808 never switches off
    bool isRunning() const      { return playStep.load() >= 0; }
    int  getPlayStep() const    { return playStep.load(); }        // step in the playing part, -1 = stopped
    int  getPlaySlot() const    { return playSlot.load(); }
    int  getPlayVar() const     { return playVar.load(); }         // 0 = A, 1 = B
    int  getPlayPart() const    { return playPart.load(); }        // 0 / 1
    int  getSelectedSlot() const { return selectedSlot.load(); }   // rhythm being edited / chosen (0..15)
    int  getPendingSlot() const { return pendingSlot.load(); }
    int  getSelectedFill() const { return selectedFill.load(); }
    bool isFillRequested() const { return fillRequested.load(); }
    bool isStepNumberArmed() const { return stepNumberArmed.load(); }
    int  editVar() const        { return getBasicVar() == VarB ? 1 : 0; }   // AB edits A
    int  editPart() const       { return getMode() == SecondPart ? 1 : 0; }
    const unit808::Part& editedPart() const { return memory.rhythm[selectedSlot.load()].part[editVar()][editPart()]; }
    int  getTrack() const       { return getInstrument(); }        // INSTRUMENT-SELECT doubles as RHYTHM TRACK 1-12
    bool isStandalone() const   { return wrapperType == wrapperType_Standalone; }
    // mute / solo per INSTRUMENT-SELECT position (AC can only be muted: it removes the accents)
    static juce::String muteId (int inst)  { return juce::String ("mute-") + juce::String (kInstrumentNames[inst]).toLowerCase(); }
    static juce::String soloId (int inst)  { return juce::String ("solo-") + juce::String (kInstrumentNames[inst]).toLowerCase(); }
    bool isMuted (int inst) const  { return raw.mute[inst]->load() > 0.5f; }
    bool isSoloed (int inst) const { return inst > 0 && raw.solo[inst]->load() > 0.5f; }
    bool isTransportRunning() const { return transportRunning.load(); }

    // ---- panel actions (message thread)
    void stepKey (int key, juce::ModifierKeys mods);   // Shift: step number, Alt: flam on / off
    void setStep (int key, bool on);          // drag painting in 1st / 2nd PART
    bool stepIsSet (int key) const;
    void tapKey();                            // write the instrument / fill-in trigger / add to the track
    void clearKey (juce::ModifierKeys mods);  // CLEAR: pattern clear, track clear, step number
    void startStopKey();                      // standalone app only (in a DAW it shows the transport)

    // ---- copy / paste (rhythms only; one clipboard per UNIT808)
    enum ClipKind { ClipNone = 0, ClipRhythm, ClipVariation, ClipPart, ClipRow };
    int  clipKind() const { return clip.kind; }
    void copyRhythm (int slot);
    void copyVariation (int slot, int var);
    void copyPart (int slot, int var, int part);
    void copyRow (int slot, int var, int part, int row);
    bool pasteRhythm (int slot);
    bool pasteVariation (int slot, int var);
    bool pastePart (int slot, int var, int part);
    bool pasteRow (int slot, int var, int part, int row);
    bool canUndoPaste() const { return undo.valid; }
    void undoPaste();
    juce::String clipDescription() const;

    std::atomic<int> uiVersion { 0 };

private:
    unit808::Engine engine;
    unit808::Params sound;
    double sampleRate = 48000.0;

    std::atomic<int> playStep { -1 }, playSlot { 0 }, playVar { 0 }, playPart { 0 };
    std::atomic<int> selectedSlot { 0 }, pendingSlot { -1 }, selectedFill { unit808::kFirstFill };
    std::atomic<int> composeChoice { -1 };
    std::atomic<bool> fillRequested { false }, stepNumberArmed { false }, transportRunning { false }, runEnabled { false };

    // playback (audio thread)
    std::vector<unit808::Segment> cycle;
    int segIndex = 0, stepInSeg = -1, measureCount = 0, trackPos = -1;
    std::atomic<int> currentRhythm { 0 };
    bool cycleIsFill = false, wasPlaying = false;
    long long nextAbsStep = 0;
    bool realignPending = false;
    struct Pending { int samples; int voice; bool accent; };
    std::vector<Pending> flamHits;   // main hits of flams, waiting their delay
    double lastQ = 0.0, internalPpq = 0.0;
    int lastMode = -1;

    struct Raw
    {
        std::atomic<float> *mode, *instrument, *autoFill, *basicVar, *fillVar, *preScale, *swing, *flam, *bpm;
        std::atomic<float> *mute[unit808::kNumRows] {}, *solo[unit808::kNumRows] {};
        std::atomic<float> *trim[29] {};
        std::atomic<float> *knob[23] {};
        std::atomic<float> *sw[5] {};
    } raw {};

    struct PartData { uint16_t rows[unit808::kNumRows] {}, flams[unit808::kNumRows] {}; int length = 16; };
    struct Clip { int kind = ClipNone; PartData parts[2][2]; int fromSlot = 0, fromVar = 0, fromPart = 0, row = 0; } clip;
    struct Undo { bool valid = false; int slot = 0; PartData parts[2][2]; } undo;
    static PartData save (const unit808::Part&);
    static void load (unit808::Part&, const PartData&);
    void keepUndo (int slot);

    void pullParams();
    void buildCycle();
    void realign();
    bool voiceAllowed (int row) const;
    void advanceStep();
    void segmentFor (int slot, int varMode, std::vector<unit808::Segment>& out) const;
    int  partLength (const unit808::Segment& s) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Unit808Processor)
};
