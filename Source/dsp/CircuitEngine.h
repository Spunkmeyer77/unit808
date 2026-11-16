// UNIT808 sound engine v1.1 — Transistor Labs
// The Transistor 808 circuit engine (every voice simulated from the schematics, tr808::VoiceBoard)
// behind the calls the plug-in makes: triggers, panel controls, the 14 calibration trimmers.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "tr808/core/voiceboard.h"
#include "tr808/core/parallel.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace unit808 {

// Sound channels on the panel. LT/MT/HT play tom or conga by their switch,
// RS plays rim shot or claves, CP plays hand clap or maracas.
enum Voice : int { BD = 0, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH, NumVoices };

// The board inside: the 14 calibration trimmers of Transistor 808, in their own units.
enum Cal : int { CbHz1 = 0, CbHz2, MetalHz3, MetalHz4, MetalHz5, MetalHz6, MetalDuty, NoiseRms, CpOffset,
                 LateClap, LateSnare, ClaveR313, MonoMs, KickTune, NumCal };
struct CalDef { const char* id; const char* name; float lo, hi, def; const char* unit; };
extern const CalDef kCal[NumCal];

// Panel controls, normalised 0..1 like the knobs (switches are bool).
struct Params {
    tr808::Controls panel;
    std::array<float, NumCal> cal {};
};

class Engine {
public:
    Engine();
    ~Engine();

    void prepare (double sampleRate);
    void setParams (const Params& p);             // audio thread, every block: applies what moved, live
    void trigger (Voice v, bool accent);           // sequencer step
    void triggerLevel (Voice v, bool accent, double scale);   // flam grace note: played unaccented
    void triggerVelocity (Voice v, int velocity, int alt = -1);  // MIDI note: velocity 100+ accents
    // alt: -1 = follow the panel switch, 0 = tom / rim shot / hand clap, 1 = conga / claves / maracas
    void render (float* mix, int numSamples);

    // the host's audio workgroup (macOS): the voice workers join it so they run with the audio thread
    void setWorkgroup (const juce::AudioWorkgroup& wg);

private:
    std::unique_ptr<tr808::Parallel> parallel;
    std::unique_ptr<tr808::VoiceBoard> board;
    double sr = 48000;
    bool prepared = false;

    Params want;                                   // what the panel asks for
    tr808::Controls applied;                       // what the board has
    std::array<float, NumCal> calApplied {};
    bool syncControls = true;
    int alt[5] = { -1, -1, -1, -1, -1 };           // the switches as the last MIDI note forced them (LT MT HT RS CP)
    uint16_t pending = 0;                          // hits waiting for the next common trig
    int hold = 0, pulseSamples = 1;
    double kickNow = 0, kickTarget = 0;            // Kick Tune glides to the trimmer

    std::mutex wgMutex;
    juce::AudioWorkgroup workgroup;
    std::vector<juce::WorkgroupToken> tokens;      // one per worker, used only on its thread

    void applyControls();
    void applyCalibration();
    bool glideKick (int samples);
    void addHit (Voice v, bool accent);
    void workerJoin (int index);
};

} // namespace unit808
