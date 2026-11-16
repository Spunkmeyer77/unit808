#include "CircuitEngine.h"

#include <algorithm>
#include <cmath>
#include <thread>

#if defined(__APPLE__)
 #include <mach/mach.h>
 #include <mach/mach_time.h>
 #include <mach/thread_policy.h>
 #include <pthread.h>
#elif defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#elif defined(__linux__)
 #include <pthread.h>
 #include <sched.h>
#endif
#if defined(__x86_64__) || defined(_M_X64)
 #include <xmmintrin.h>
#endif

namespace unit808 {

// The Transistor 808 calibration window, control for control (same ranges and factory values).
const CalDef kCal[NumCal] = {
    { "cb-osc1",   "Cowbell Oscillator 1",  250.0f, 1200.0f, 558.3f,  "Hz" },
    { "cb-osc2",   "Cowbell Oscillator 2",  350.0f, 1700.0f, 823.6f,  "Hz" },
    { "metal-osc3", "Metal Oscillator 3",   150.0f, 1000.0f, 486.39f, "Hz" },
    { "metal-osc4", "Metal Oscillator 4",   120.0f, 800.0f,  287.82f, "Hz" },
    { "metal-osc5", "Metal Oscillator 5",   120.0f, 900.0f,  378.58f, "Hz" },
    { "metal-osc6", "Metal Oscillator 6",   80.0f,  700.0f,  198.83f, "Hz" },
    { "metal-duty", "Metal Oscillator Duty", 20.0f, 80.0f,   54.5f,   "%" },
    { "noise-level", "Shared Noise Level",  20.0f,  350.0f,  190.0f,  "mV" },
    { "cp-offset", "Clap Offset",           0.0f,   100.0f,  37.0f,   "%" },
    { "cp-revision", "Clap Revision",       0.0f,   1.0f,    1.0f,    "" },
    { "sd-revision", "Snare Revision",      0.0f,   1.0f,    1.0f,    "" },
    { "cl-feedback", "Claves Feedback",     490.0f, 1000.0f, 500.0f,  "kOhm" },
    { "trig-width", "Common Trigger Width", 0.5f,   2.0f,    1.0f,    "ms" },
    { "bd-tune",   "Kick Tune",             -12.0f, 12.0f,   0.0f,    "st" },
};

namespace {

double kickCap (double semitones) { return 15e-9 * std::exp2 (-semitones / 12.0); }
constexpr int kGlideSamples = 16;       // a C41/C42 update every 16 samples while Kick Tune glides
constexpr double kGlideSeconds = 0.02;  // its time constant

void flushDenormals()
{
#if defined(__aarch64__) || defined(_M_ARM64)
 #if defined(_MSC_VER)
    // MSVC on ARM64: FTZ is the default for the C runtime
 #else
    uint64_t r;
    asm volatile ("mrs %0, fpcr" : "=r"(r));
    asm volatile ("msr fpcr, %0" : : "r"(r | (uint64_t (1) << 24)));
 #endif
#elif defined(__x86_64__) || defined(_M_X64)
    _mm_setcsr (_mm_getcsr() | 0x8040);
#endif
}

// The workers render inside the audio callback: audio-thread priority (and, on Apple Silicon, the
// performance cores). The host's workgroup, when it gives one, is joined as well (workerJoin).
void makeRealtime (int)
{
    flushDenormals();
#if defined(__APPLE__)
    mach_timebase_info_data_t tb;
    mach_timebase_info (&tb);
    const double msToAbs = 1e6 * double (tb.denom) / double (tb.numer);
    thread_time_constraint_policy_data_t policy;
    policy.period = uint32_t (2.9 * msToAbs);
    policy.computation = uint32_t (1.0 * msToAbs);
    policy.constraint = uint32_t (2.9 * msToAbs);
    policy.preemptible = 1;
    thread_policy_set (pthread_mach_thread_np (pthread_self()), THREAD_TIME_CONSTRAINT_POLICY,
                       reinterpret_cast<thread_policy_t> (&policy), THREAD_TIME_CONSTRAINT_POLICY_COUNT);
#elif defined(_WIN32)
    SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#elif defined(__linux__)
    sched_param sp {};
    sp.sched_priority = 70;
    pthread_setschedparam (pthread_self(), SCHED_FIFO, &sp);   // ignored if not permitted
#endif
}

int workerCount()
{
    const unsigned cores = std::thread::hardware_concurrency();
    return cores >= 4 ? 2 : (cores >= 2 ? 1 : 0);
}

int altGroup (Voice v)
{
    switch (v) { case LT: return 0; case MT: return 1; case HT: return 2; case RS: return 3; case CP: return 4; default: return -1; }
}

} // namespace

Engine::Engine()
{
    const int n = workerCount();
    tokens.resize ((size_t) n);
    parallel = std::make_unique<tr808::Parallel> (n, makeRealtime, [this] (int i) { workerJoin (i); });
    board = std::make_unique<tr808::VoiceBoard>();
    for (int i = 0; i < NumCal; ++i) want.cal[(size_t) i] = calApplied[(size_t) i] = kCal[i].def;
    board->setCalibration (tr808::unitCalibration());   // the factory values above are this unit's
    if (n > 0) board->setParallel (parallel.get());
}

Engine::~Engine()
{
    board.reset();        // holds a pointer to the workers
    parallel.reset();
}

void Engine::workerJoin (int index)
{
    std::lock_guard<std::mutex> lock (wgMutex);
    auto& token = tokens[(size_t) index];
    token.reset();
    if (workgroup) workgroup.join (token);
}

void Engine::setWorkgroup (const juce::AudioWorkgroup& wg)
{
    {
        std::lock_guard<std::mutex> lock (wgMutex);
        workgroup = wg;
    }
    parallel->rejoin();
}

void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    board->prepare (sr);
    // prepare() rebuilt the board from the factory calibration: re-apply the trimmers next block
    for (int i = 0; i < NumCal; ++i) calApplied[(size_t) i] = kCal[i].def;
    kickNow = kCal[KickTune].def;
    syncControls = true;
    pending = 0;
    hold = 0;
    pulseSamples = std::max (1, (int) std::ceil (sr * (kCal[MonoMs].def + 0.2) * 0.001));
    prepared = true;
}

void Engine::setParams (const Params& p)
{
    want = p;
    applyControls();
    applyCalibration();
}

void Engine::applyControls()
{
    tr808::Controls c = want.panel;
    bool* sw[5] = { &c.lc, &c.mc, &c.hc, &c.cl, &c.ma };
    for (int g = 0; g < 5; ++g)
        if (alt[g] >= 0) *sw[g] = alt[g] == 1;
    const auto same = [] (const tr808::Controls& a, const tr808::Controls& b)
    {
        return a.accent == b.accent && a.bdLevel == b.bdLevel && a.bdTone == b.bdTone && a.bdDecay == b.bdDecay
            && a.sdLevel == b.sdLevel && a.sdTone == b.sdTone && a.sdSnappy == b.sdSnappy
            && a.ltLevel == b.ltLevel && a.ltTuning == b.ltTuning && a.mtLevel == b.mtLevel && a.mtTuning == b.mtTuning
            && a.htLevel == b.htLevel && a.htTuning == b.htTuning && a.rsLevel == b.rsLevel && a.cpLevel == b.cpLevel
            && a.cbLevel == b.cbLevel && a.cyLevel == b.cyLevel && a.cyTone == b.cyTone && a.cyDecay == b.cyDecay
            && a.ohLevel == b.ohLevel && a.ohDecay == b.ohDecay && a.chLevel == b.chLevel && a.volume == b.volume
            && a.lc == b.lc && a.mc == b.mc && a.hc == b.hc && a.cl == b.cl && a.ma == b.ma;
    };
    if (syncControls || ! same (c, applied))
    {
        board->setControls (c);
        applied = c;
        syncControls = false;
    }
}

void Engine::applyCalibration()
{
    const auto& now = want.cal;
    auto moved = [&] (Cal k) { return now[(size_t) k] != calApplied[(size_t) k]; };
    auto value = [&] (Cal k) { return (double) now[(size_t) k]; };
    bool any = false;
    for (int i = 0; i < NumCal; ++i) any |= moved ((Cal) i);
    if (! any) return;
    if (moved (CbHz1) || moved (CbHz2) || moved (MetalHz3) || moved (MetalHz4) || moved (MetalHz5) || moved (MetalHz6) || moved (MetalDuty))
    {
        const double hz[6] = { value (CbHz1), value (CbHz2), value (MetalHz3), value (MetalHz4), value (MetalHz5), value (MetalHz6) };
        board->setLiveMetal (hz, value (MetalDuty) / 100.0);
    }
    if (moved (NoiseRms)) board->setLiveNoise (value (NoiseRms) / 1000.0);
    if (moved (CpOffset)) board->setLiveClapOffset (value (CpOffset) / 100.0);
    if (moved (LateClap))     // R200 (10k, Q70's emitter to -15 V) fitted in later units, open in early ones
        board->setLivePart (tr808::Voice::CP, "r200cp", value (LateClap) >= 0.5 ? 10e3 : 1e12);
    if (moved (LateSnare))    // the "PORTION CHANGED" values, or the schematic's
    {
        const bool late = value (LateSnare) >= 0.5;
        board->setLivePart (tr808::Voice::SD, "c58", late ? 0.056e-6 : 0.027e-6);
        board->setLivePart (tr808::Voice::SD, "c61", late ? 0.015e-6 : 0.0068e-6);
        board->setLivePart (tr808::Voice::SD, "r191", late ? 10e3 : 8.2e3);
        board->setLivePart (tr808::Voice::SD, "r200", late ? 0.1 : 47e3);
    }
    if (moved (ClaveR313)) board->setLivePart (tr808::Voice::RS, "r313", value (ClaveR313) * 1000.0);
    if (moved (MonoMs))
    {
        board->setLiveTriggerWidth (value (MonoMs));
        pulseSamples = std::max (1, (int) std::ceil (sr * (value (MonoMs) + 0.2) * 0.001));
    }
    if (moved (KickTune)) kickTarget = value (KickTune);   // render() glides C41/C42 there
    calApplied = now;
}

bool Engine::glideKick (int samples)
{
    const double gap = kickTarget - kickNow;
    if (std::abs (gap) < 1e-4)
        kickNow = kickTarget;
    else
        kickNow += gap * (1.0 - std::exp (-samples / (kGlideSeconds * sr)));
    const double cap = kickCap (kickNow);
    board->setLivePart (tr808::Voice::BD, "c41", cap);
    board->setLivePart (tr808::Voice::BD, "c42", cap);
    return kickNow != kickTarget;
}

void Engine::addHit (Voice v, bool accent)
{
    pending |= tr808::bitOf ((tr808::Voice) v);
    if (accent) pending |= tr808::kBitAC;
}

void Engine::trigger (Voice v, bool accent)
{
    const int g = altGroup (v);
    if (g >= 0 && alt[g] != -1) { alt[g] = -1; applyControls(); }
    addHit (v, accent);
}

void Engine::triggerLevel (Voice v, bool accent, double scale)
{
    trigger (v, accent && scale >= 0.8);   // the circuit has accent or not: a soft grace note is unaccented
}

void Engine::triggerVelocity (Voice v, int velocity, int a)
{
    if (velocity <= 0) return;
    const int g = altGroup (v);
    if (g >= 0 && alt[g] != a) { alt[g] = a; applyControls(); }
    addHit (v, velocity >= 100);
}

void Engine::render (float* mix, int numSamples)
{
    if (! prepared) { std::fill (mix, mix + numSamples, 0.0f); return; }
    int pos = 0;
    while (pos < numSamples)
    {
        if (hold == 0)
        {
            if (pending)
            {
                // the common trig: the step pulse on one sample, the instrument data held for its width
                board->setLines (true, pending);
                board->render (mix + pos, 1);
                ++pos;
                board->setLines (false, pending);
                pending = 0;
                hold = pulseSamples;
                continue;
            }
            board->setLines (false, 0);
        }
        int block = numSamples - pos;
        if (hold) block = std::min (block, hold);
        if (kickNow != kickTarget)
        {
            block = std::min (block, kGlideSamples);
            glideKick (block);
        }
        board->render (mix + pos, block);
        pos += block;
        if (hold) hold -= block;
    }
}

} // namespace unit808
