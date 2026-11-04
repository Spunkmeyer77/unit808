// UNIT808 sound engine v0.2 — Ticco Ross
// C++ port of the UNIT 808 Max for Live device engine (gen~ codeboxes), sample-for-sample,
// so the plugin sounds the same as the device. Framework-independent.
#pragma once
#ifndef M_PI
 #define M_PI 3.14159265358979323846
#endif
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace unit808 {

// Sound channels on the panel. LT/MT/HT play tom or conga by their switch,
// RS plays rim shot or claves, CP plays hand clap or maracas.
enum Voice : int { BD = 0, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH, NumVoices };

// Panel controls, normalised 0..1 like the knobs (switches are bool).
struct Params {
    float accent = 0.6f;
    float bdLevel = 0.5f, bdTone = 0.5f, bdDecay = 0.32f;
    float sdLevel = 0.39f, sdTone = 1.0f, sdSnappy = 0.6f;
    float ltLevel = 0.45f, ltTuning = 0.386f; bool ltConga = false;
    float mtLevel = 0.42f, mtTuning = 0.32f;  bool mtConga = false;
    float htLevel = 0.40f, htTuning = 0.314f; bool htConga = false;
    float rsLevel = 0.46f;                    bool rsClaves = false;
    float cpLevel = 0.70f;                    bool cpMaracas = false;
    float cbLevel = 0.35f;
    float cyLevel = 0.60f, cyTone = 0.4f, cyDecay = 0.577f;
    float ohLevel = 0.45f, ohDecay = 0.559f;
    float chLevel = 0.45f;
    float master = 0.8f;                      // 0.8 = unity (same level as the M4L device)

    // ---- internal trimmers (the board inside), in the M4L device's own units; defaults = the device's
    float bdTune = 49.4f, bdClick = 0.01f, bdDrive = 1.0f;          // Hz, 0..1, 1..6
    float sdTune = 1.0f, sdDecay = 0.73f;                            // x, x
    float tomDecay[3] = { 95, 70, 48 }, tomBend[3] = { 0.03f, 0.03f, 0.03f };      // ms, 0..0.5
    float congaDecay[3] = { 86, 44, 38 }, congaBend[3] = { 0.03f, 0.03f, 0.03f };
    float rsTune = 1.0f, rsDecay = 1.0f, clTune = 1.0f, clDecay = 1.0f;             // x
    float cpTone = 1300, cpDecay = 60;                               // Hz, ms
    float cbTune = 1.0f, cbDecay = 97;                               // x, ms
    float cyTune = 1.0f;                                             // x
    float hhTune = 1.0f, hhTone = 1.0f, chDecay = 24;                // x, x, ms
};

// Same mapping as a live.dial with an exponent: min + (max-min) * x^exp.
inline float dialMap(float x, float mn, float mx, float ex = 1.0f) { return mn + (mx - mn) * std::pow(x < 0 ? 0 : (x > 1 ? 1 : x), ex); }

// ---------- gen~ building blocks ----------
struct GenNoise {                                // gen~ noise(): white, -1..1
    uint32_t s = 0x9E3779B9u;
    float next() { s = s * 1664525u + 1013904223u; return (float)((double)s / 2147483648.0 - 1.0); }
};
struct BiquadDF1 {                               // the q*_ biquads written out in the codeboxes
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void bp(double f, double q, double sr) { f = std::fmin(f, sr * 0.45); double w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + al;
        b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = 2 * c / a0; a2 = (1 - al) / a0; }
    void hp(double f, double q, double sr) { f = std::fmin(f, sr * 0.45); double w = 2 * M_PI * f / sr, al = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + al;
        b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = 2 * c / a0; a2 = (1 - al) / a0; }
    double tick(double x) { double y = b0 * x + b1 * x1 + b2 * x2 + a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; }
};
struct GenSquare {                               // phasor() + sq() with PolyBLEP, as in the codeboxes
    double ph = 0;
    static double blep(double t, double dt) { if (t < dt) { double u = t / dt; return u + u - u * u - 1; }
        if (t > 1 - dt) { double u = (t - 1) / dt; return u * u + u + u + 1; } return 0; }
    double tick(double f, double sr) { double dt = f / sr, p = ph; double p2 = p + 0.5; if (p2 >= 1) p2 -= 1;
        double v = (p < 0.5 ? 1.0 : -1.0) + blep(p, dt) - blep(p2, dt); ph += dt; ph -= std::floor(ph); return v; }
};

class Engine {
public:
    void prepare(double sampleRate);
    void reset();
    void setParams(const Params& p) { P = p; }
    void trigger(Voice v, bool accent);           // sequencer step: accented = full level
    void triggerLevel(Voice v, bool accent, double scale);   // same, times scale (flam grace notes)
    void triggerVelocity(Voice v, int velocity, int alt = -1);  // MIDI note: (1-AC) + AC*vel/127, as the device
    // alt: -1 = follow the panel switch, 0 = tom / rim shot / hand clap, 1 = conga / claves / maracas
    // Renders mono mix; optional per-voice outputs (post level) for multi-out.
    void render(float* mix, int numSamples, float* const* voiceOuts = nullptr);

private:
    double sr = 48000;
    Params P;
    GenNoise nz;
    double trig[NumVoices] = {};   // pending trigger amplitude for the next sample (click~)
    int trigAlt[NumVoices] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    bool rsClavesActive = false, cpMaracasActive = false;
    bool congaActive[3] = {};

    struct BdState { double y1 = 0, y2 = 0, fe = 0, pe = 0, pt = 0, pa = 0, ex = 0, lp = 0, hx = 0, hy = 0, ck = 0, cl = 0; } bd;
    struct SdState { double a1 = 0, a2 = 0, b1 = 0, b2 = 0, ne = 0, lp = 0; BiquadDF1 h1, h2; } sd;
    struct TomState { double y1 = 0, y2 = 0, ae = 0, ve = 0, va = 0, ne = 0, na = 0, b0 = 0, b1 = 0, b2 = 0, nl = 0; } tom[3], conga[3];
    struct RsState { double a1 = 0, a2 = 0, b1 = 0, b2 = 0, ev = 0, ea = 0, hy = 0, hx = 0, hy2 = 0, hx2 = 0; } rs;
    struct ClState { double y1 = 0, y2 = 0, pt = 0, pa = 0, ex = 0, ex2 = 0; } cl;
    struct CpState { double t = 100000, amp = 0, lp = 0; BiquadDF1 bp, hp; } cp;
    struct MaState { double t = 1e9, amp = 0; BiquadDF1 hp; } ma;  // maracas (not in the M4L device; panel CP/MA switch)
    struct CbState { double e1 = 0, e2 = 0; GenSquare a, b; BiquadDF1 bp; } cb;
    struct MetalState {
        GenSquare hs[6], cs[6];
        double he = 0, hm = 0, ot = 0, ec = 0, eo = 0, ko = 0, ot2 = 0, sc = 0, so = 0, hk = 0, hf = 0, hsm = 0, e1 = 0, e2 = 0, e3 = 0;
        BiquadDF1 f1, f2c, f3c, f2o, f3o, f4, f5, f6, f7;
    } mt;

    double lastCyTone = -1, lastFilterKey = -1;
    void updateFilters();
    double tickBD(double x);
    double tickSD(double x);
    double tickTom(TomState& s, double x, double tune, double decay, double bend, double gain);
    double tickRS(double x);
    double tickCL(double x);
    double tickCP(double x);
    double tickMA(double x);
    double tickCB(double x);
    void   tickMetal(double xch, double xoh, double xcy, double& ch, double& oh, double& cy);
};

} // namespace unit808
