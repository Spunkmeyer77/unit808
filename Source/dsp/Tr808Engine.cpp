// UNIT808 sound engine v0.2 — Transistor Labs. Port of the UNIT 808 M4L gen~ engine.
// Each tick*() is a line-for-line translation of the matching codebox; History
// variables become struct members (codebox semantics: assignments take effect at once).
#include "Tr808Engine.h"
#include <algorithm>

namespace unit808 {

static inline double ex(double sec, double sr) { return std::exp(-1.0 / (sec * sr)); }
// Per-call-site memo of a pure function: the codeboxes evaluate exp/cos/sin of the same
// arguments every sample; recomputing only when the argument changes gives identical results.
#define U808_MEMO(fn, arg) ([&]() { static thread_local double key_ = std::nan(""), val_ = 0; const double a_ = (arg); \
                                     if (a_ != key_) { key_ = a_; val_ = fn(a_); } return val_; }())
#define EXP_(x) U808_MEMO(std::exp, x)
#define COS_(x) U808_MEMO(std::cos, x)
#define SIN_(x) U808_MEMO(std::sin, x)
static const double TWOPI = 2.0 * M_PI;

void Engine::prepare(double sampleRate) { sr = sampleRate; reset(); }

void Engine::reset() {
    bd = {}; sd = {}; rs = {}; cl = {}; cp = {}; ma = {}; cb = {}; mt = {};
    for (auto& t : tom) t = {};
    for (auto& c : conga) c = {};
    for (auto& t : trig) t = 0;
    lastCyTone = -1; lastFilterKey = -1;
}

void Engine::trigger(Voice v, bool accent) {
    // gen sequencer: amp = acb + (1-acb)*((1-accamt) + accamt*90/127)
    double amp = accent ? 1.0 : (1.0 - P.accent) + P.accent * 90.0 / 127.0;
    trig[v] = std::max(trig[v], amp);
    trigAlt[v] = -1;
}

void Engine::triggerLevel(Voice v, bool accent, double scale) {
    double amp = (accent ? 1.0 : (1.0 - P.accent) + P.accent * 90.0 / 127.0) * scale;
    trig[v] = std::max(trig[v], amp);
    trigAlt[v] = -1;
}

void Engine::triggerVelocity(Voice v, int vel, int alt) {
    // MIDI path: expr (1.-$f2)+$f2*$f1/127.
    double amp = (1.0 - P.accent) + P.accent * std::clamp(vel, 1, 127) / 127.0;
    trig[v] = std::max(trig[v], amp);
    trigAlt[v] = alt;
}

// ---------------- BD (codebox 0) ----------------
double Engine::tickBD(double x) {
    if (x <= 0 && bd.pt <= 0 && std::abs(bd.y1) + std::abs(bd.y2) + std::abs(bd.ex) + std::abs(bd.lp) + std::abs(bd.hy) + bd.ck < 1e-9) return 0;   // asleep until the next hit
    const double tune = P.bdTune, click = P.bdClick, drive = P.bdDrive;
    const double decay = dialMap(P.bdDecay, 12, 1500, 3), tone = P.bdTone;
    auto& s = bd;
    if (x > 0) { s.pt = 0.001 * sr; s.pa = x; s.fe = x; }
    double p = s.pt > 0 ? s.pa : 0;
    s.pt = std::max(s.pt - 1, 0.0);
    s.ex = s.ex + (p - s.ex) * (1 - EXP_(-1.0 / ((0.0008) * sr)));
    s.fe = s.fe * EXP_(-1.0 / ((0.003) * sr));
    s.pe = s.pe * EXP_(-1.0 / ((0.06) * sr)) + x;
    double f = tune * (1 + 2.5 * s.fe + 0.2 * std::min(s.pe, 1.5));
    double w = TWOPI * std::min(f, sr * 0.2) / sr;
    double dk = decay * (0.75 + 0.35 * s.pa);
    double r = EXP_(-1.0 / ((dk * 0.001) * sr));
    double y = 2 * r * COS_(w) * s.y1 - r * r * s.y2 + s.ex * SIN_(w) * 0.015;
    s.y2 = s.y1; s.y1 = y;
    double sh = y + 0.25 * y * y - 0.14 * y * y * y;
    double kt = EXP_(-TWOPI * (300 + tone * tone * 6000) / sr);
    double t1 = (1 - kt) * sh + kt * s.lp; s.lp = t1;
    s.ck = s.ck * EXP_(-1.0 / ((0.0015) * sr)) + x;
    double kc = EXP_(-TWOPI * 2500 / sr);
    double c = (1 - kc) * s.ck + kc * s.cl; s.cl = c;
    double k = EXP_(-TWOPI * 6.7 / sr);
    double h = k * (s.hy + t1 - s.hx); s.hx = t1; s.hy = h;
    double o = std::tanh((h + c * click * 2.0) * drive) / std::tanh(drive);
    return o * 1.02;
}

// ---------------- SD (codebox 1) ----------------
double Engine::tickSD(double x) {
    if (x <= 0 && std::abs(sd.a1) + std::abs(sd.a2) + std::abs(sd.b1) + std::abs(sd.b2) + sd.ne < 1e-9) return 0;
    const double tune = P.sdTune, decay = P.sdDecay, tone = P.sdTone, snappy = P.sdSnappy;
    auto& s = sd;
    double wa = TWOPI * 172 * tune / sr, ra = EXP_(-1.0 / ((0.028 * decay) * sr));
    double ya = 2 * ra * COS_(wa) * s.a1 - ra * ra * s.a2 + x * SIN_(wa); s.a2 = s.a1; s.a1 = ya;
    double wb = TWOPI * 340 * tune / sr, rb = EXP_(-1.0 / ((0.0085 * decay) * sr));
    double yb = 2 * rb * COS_(wb) * s.b1 - rb * rb * s.b2 + x * SIN_(wb); s.b2 = s.b1; s.b1 = yb;
    s.ne = s.ne * EXP_(-1.0 / ((0.032 * decay) * sr)) + x;
    double n = nz.next();
    double f2 = s.h2.tick(s.h1.tick(n));
    double kl = EXP_(-TWOPI * 4000 / sr);
    double nf = (1 - kl) * f2 + kl * s.lp; s.lp = nf;
    double sm = ya * (1 - 0.5 * tone) + yb * tone * 1.9 + nf * s.ne * snappy * 4.0;
    return std::tanh(sm * 2.2);
}

// ---------------- Toms / congas (codeboxes 2-4, 9-11) ----------------
double Engine::tickTom(TomState& s, double x, double tune, double decay, double bend, double noiseAmt) {
    if (x <= 0 && std::abs(s.y1) + std::abs(s.y2) + s.va + s.na + s.ve < 1e-9) return 0;
    double ot = decay * 4.0 * 0.001;
    s.ae = s.ae * EXP_(-1.0 / ((ot) * sr)) + x;
    double w = TWOPI * tune * (1 + bend * std::min(s.ae, 1.5)) / sr;
    double r = EXP_(-1.0 / ((ot) * sr));
    double y = 2 * r * COS_(w) * s.y1 - r * r * s.y2 + x * SIN_(w);
    s.y2 = s.y1; s.y1 = y;
    if (x > 0) s.ve = x; else s.ve = s.ve * EXP_(-1.0 / ((decay * 0.001) * sr));
    s.va = s.va + (s.ve - s.va) * (1 - EXP_(-1.0 / ((0.0007) * sr)));
    double vs = std::tanh(y * s.va);
    double o = vs;
    if (noiseAmt > 0) {
        if (x > 0) s.ne = x; else s.ne = s.ne * EXP_(-1.0 / ((decay * 1.3 * 0.001) * sr));
        double wn = nz.next();
        s.b0 = 0.99765 * s.b0 + wn * 0.0990460;
        s.b1 = 0.96300 * s.b1 + wn * 0.2965164;
        s.b2 = 0.57000 * s.b2 + wn * 1.0526913;
        double pn = (s.b0 + s.b1 + s.b2 + wn * 0.1848) * 0.25;
        double kn = EXP_(-TWOPI * 300 / sr);
        double nf = (1 - kn) * pn + kn * s.nl; s.nl = nf;
        s.na = s.na + (s.ne - s.na) * (1 - EXP_(-1.0 / ((0.0007) * sr)));
        o += nf * s.na * noiseAmt;
    }
    return o * 1.27;
}

// ---------------- RS (codebox 5) ----------------
double Engine::tickRS(double x) {
    if (x <= 0 && std::abs(rs.a1) + std::abs(rs.b1) + rs.ea + rs.ev + std::abs(rs.hy2) + std::abs(rs.hy) < 1e-9) return 0;
    const double tune = P.rsTune, decay = P.rsDecay;
    auto& s = rs;
    double wa = TWOPI * 455 * tune / sr, ra = EXP_(-1.0 / ((0.06) * sr));
    double ya = 2 * ra * COS_(wa) * s.a1 - ra * ra * s.a2 + x * SIN_(wa); s.a2 = s.a1; s.a1 = ya;
    double wb = TWOPI * 1667 * tune / sr, rb = EXP_(-1.0 / ((0.01) * sr));
    double yb = 2 * rb * COS_(wb) * s.b1 - rb * rb * s.b2 + x * SIN_(wb); s.b2 = s.b1; s.b1 = yb;
    if (x > 0) s.ev = x; else s.ev = s.ev * EXP_(-1.0 / ((0.0045 * decay) * sr));
    s.ea = s.ea + (s.ev - s.ea) * (1 - EXP_(-1.0 / ((0.0002) * sr)));
    double sg = (ya * 0.9 + yb) * s.ea;
    double v = std::tanh(sg * 12 + 0.7) - std::tanh(0.7);
    double k = EXP_(-TWOPI * 800 / sr);
    double h = k * (s.hy + v - s.hx); s.hx = v; s.hy = h;
    double h2 = k * (s.hy2 + h - s.hx2); s.hx2 = h; s.hy2 = h2;
    return h2 * 0.8 * 0.84;
}

// ---------------- CL (codebox 12) ----------------
double Engine::tickCL(double x) {
    if (x <= 0 && cl.pt <= 0 && std::abs(cl.y1) + std::abs(cl.y2) + cl.ex + cl.ex2 < 1e-9) return 0;
    const double tune = P.clTune, decay = P.clDecay;
    auto& s = cl;
    if (x > 0) { s.pt = 0.001 * sr; s.pa = x; }
    double p = s.pt > 0 ? s.pa : 0;
    s.pt = std::max(s.pt - 1, 0.0);
    double ks = 1 - EXP_(-1.0 / ((0.00015) * sr));
    s.ex = s.ex + (p - s.ex) * ks;
    s.ex2 = s.ex2 + (s.ex - s.ex2) * ks;
    double w = TWOPI * 2422 * tune / sr, r = EXP_(-1.0 / ((0.0109 * decay) * sr));
    double y = 2 * r * COS_(w) * s.y1 - r * r * s.y2 + (s.ex - s.ex2) * SIN_(w) * 0.3744;
    s.y2 = s.y1; s.y1 = y;
    return y;
}

// ---------------- CP (codebox 6) ----------------
double Engine::tickCP(double x) {
    const double decay = P.cpDecay;
    auto& s = cp;
    if (x > 0) { s.t = 0; s.amp = x; } else s.t = std::min(s.t + 1, 10000000.0);
    double ms = s.t * 1000 / sr;
    if (x <= 0 && ms > 45.2 && std::exp(-(ms - 33.9) / decay) * 0.25 * s.amp < 1e-9 && std::abs(s.hp.y1) + std::abs(s.hp.y2) < 1e-9) return 0;
    double ph = std::fmod(ms, 11.3);
    double burst = ms < 45.2 ? std::exp(-ph / 3.0) : 0;
    double tail = ms >= 33.9 ? std::exp(-(ms - 33.9) / decay) * 0.25 : 0;
    double n = nz.next();
    double f1 = s.bp.tick(n);
    double kl = EXP_(-TWOPI * 10000 / sr);
    double o = (1 - kl) * f1 + kl * s.lp; s.lp = o;
    double ce = o * (burst + tail) * s.amp;
    double f2 = s.hp.tick(ce);
    return f2 * 3 * 0.95;
}

// ---------------- MA (new: the device has no maracas) ----------------
// White noise high-passed (Q68) with a short rise/fall envelope (Q66/Q67).
double Engine::tickMA(double x) {
    auto& s = ma;
    if (x > 0) { s.t = 0; s.amp = x; } else s.t += 1;
    double t = s.t / sr;
    if (x <= 0 && t > 0.5) return 0;
    double env = t < 0.004 ? t / 0.004 : std::exp(-(t - 0.004) / 0.018);
    if (t > 0.5) env = 0;
    return std::tanh(s.hp.tick(nz.next()) * env * 1.8 * s.amp) * 0.9;
}

// ---------------- CB (codebox 7) ----------------
double Engine::tickCB(double x) {
    if (x <= 0 && cb.e1 + cb.e2 < 1e-9) { cb.a.ph += 540 / sr; cb.a.ph -= std::floor(cb.a.ph); cb.b.ph += 800 / sr; cb.b.ph -= std::floor(cb.b.ph); return 0; }
    const double tune = P.cbTune, decay = P.cbDecay;
    auto& s = cb;
    if (x > 0) { s.e1 = x; s.e2 = x; }
    else { s.e1 = s.e1 * EXP_(-1.0 / ((0.012) * sr)); s.e2 = s.e2 * EXP_(-1.0 / ((decay * 0.001) * sr)); }
    double sq = s.a.tick(540 * tune, sr) + s.b.tick(800 * tune, sr);
    double f1 = s.bp.tick(sq);
    return f1 * (0.55 * s.e1 + 0.45 * s.e2) * 0.9 * 0.53;
}

// ---------------- CH / OH / CY (codebox 8) ----------------
// The device sums CH and OH before two linear high-pass filters; here each has its own
// identical filter pair so the panel can give them separate levels (same sound).
void Engine::tickMetal(double in1, double in2, double in3, double& chOut, double& ohOut, double& cyOut) {
    const double chdecay = P.chDecay, hattune = P.hhTune, cytune = P.cyTune;
    const double ohdecay = dialMap(P.ohDecay, 25, 1000, 2);
    const double cydecay = dialMap(P.cyDecay, 50, 2000, 2);
    static const double F[6] = { 205.3, 304.4, 369.6, 522.7, 540, 800 };
    auto& s = mt;
    double hpm = 0, cpm = 0;
    for (int i = 0; i < 6; ++i) { hpm += s.hs[i].tick(F[i] * hattune, sr); cpm += s.cs[i].tick(F[i] * cytune, sr); }
    hpm /= 3; cpm /= 3;

    if (in3 > 0) { s.e1 = in3; s.e2 = in3; s.e3 = in3; }
    else { s.e1 *= EXP_(-1.0 / ((0.02) * sr)); s.e2 *= EXP_(-1.0 / ((cydecay * 0.001) * sr)); s.e3 *= EXP_(-1.0 / ((cydecay * 1.15 * 0.001) * sr)); }
    double f1 = s.f1.tick(hpm);
    if (in1 > 0 && in2 <= 0) { s.ec = in1; s.ko = EXP_(-1.0 / ((0.008) * sr)); } else s.ec *= EXP_(-1.0 / ((chdecay * 0.001) * sr));
    if (in2 > 0) { s.eo = in2; s.ot2 = ohdecay * 0.001 * sr; s.ko = EXP_(-1.0 / ((ohdecay * 1.3 * 0.001) * sr)); } else s.eo *= s.ko;
    s.ot2 = std::max(s.ot2 - 1, 0.0);
    if (s.ot2 <= 0 && in2 <= 0) s.ko = std::min(s.ko, EXP_(-1.0 / ((0.008) * sr)));
    s.sc = s.sc + (s.ec - s.sc) * (1 - EXP_(-1.0 / ((0.0008) * sr)));
    s.so = s.so + (s.eo - s.so) * (1 - EXP_(-1.0 / ((0.0008) * sr)));
    auto sp = [](double v) { return std::log(1 + std::exp(4 * v)) / 4; };   // softplus "swing VCA"
    double svc = s.sc * std::tanh(sp(f1 * 5 - 0.3) - sp(-0.3));
    double svo = s.so * std::tanh((sp(f1 * 2 + 0.2) - sp(0.2)) / 0.2) * 0.2 * 2.212;
    chOut = s.f3c.tick(s.f2c.tick(svc)) * 2.45;
    ohOut = s.f3o.tick(s.f2o.tick(svo)) * 2.45;

    double f5 = s.f5.tick(s.f4.tick(cpm));
    double f7 = s.f7.tick(s.f6.tick(cpm));
    cyOut = (f5 * (s.e1 + 0.3 * s.e2) + f7 * 0.25 * s.e3) * 2.0;
}

// ---------------- render ----------------
// Filter coefficients: fixed ones depend only on the sample rate, the cymbal's on CY TONE.
// (The codeboxes recompute them every sample; with constant inputs that gives the same values.)
void Engine::updateFilters() {
    sd.h1.hp(1200, 0.7, sr); sd.h2.hp(1200, 0.7, sr);
    ma.hp.hp(5200, 0.9, sr);
    cp.hp.hp(700, 0.7, sr);
    // trimmer-dependent: CP TONE, CB TUNE, HH TONE (recomputed only when they change)
    const double key = P.cpTone * 1e-3 + P.cbTune * 7.0 + P.hhTone * 131.0;
    if (key != lastFilterKey) {
        cp.bp.bp(P.cpTone, 0.8, sr);
        cb.bp.bp(1000 * P.cbTune, 1, sr);
        mt.f1.bp(8500 * P.hhTone, 0.8, sr);
        mt.f2c.hp(6500 * P.hhTone, 0.7, sr); mt.f3c.hp(6500 * P.hhTone, 0.7, sr);
        mt.f2o.hp(6500 * P.hhTone, 0.7, sr); mt.f3o.hp(6500 * P.hhTone, 0.7, sr);
        lastFilterKey = key;
    }
    const double cytone = dialMap(P.cyTone, 0.6f, 1.6f);
    if (cytone != lastCyTone) {
        mt.f4.bp(8000 * cytone, 1.0, sr); mt.f5.hp(3000 * cytone, 0.7, sr);
        mt.f6.bp(4000 * cytone, 1.0, sr); mt.f7.hp(2500 * cytone, 0.7, sr);
        lastCyTone = cytone;
    }
}

void Engine::render(float* mix, int n, float* const* outs) {
    updateFilters();
    // Panel tuning knobs -> device tune ranges (tom ranges / conga ranges); other device
    // controls stay at the device defaults.
    struct R { double lo, hi, decay; };
    static const R tomR[3] = { { 60, 130, 95 }, { 100, 200, 70 }, { 140, 280, 48 } };
    static const R conR[3] = { { 140, 260, 86 }, { 200, 380, 44 }, { 300, 560, 38 } };
    const float tunes[3] = { P.ltTuning, P.mtTuning, P.htTuning };
    const bool  isCon[3] = { P.ltConga, P.mtConga, P.htConga };
    const double lvl[NumVoices] = { P.bdLevel, P.sdLevel, P.ltLevel, P.mtLevel, P.htLevel, P.rsLevel,
                                    P.cpLevel, P.cbLevel, P.cyLevel, P.ohLevel, P.chLevel };
    const double master = P.master / 0.8;

    for (int i = 0; i < n; ++i) {
        double x[NumVoices];
        int alt[NumVoices];
        for (int k = 0; k < NumVoices; ++k) { x[k] = trig[k]; trig[k] = 0; alt[k] = trigAlt[k]; trigAlt[k] = -1; }
        // switches are latched on each hit, like the selected voice on the hardware
        auto pick = [&](int k, bool panel) { return alt[k] < 0 ? panel : alt[k] == 1; };
        if (x[RS] > 0) rsClavesActive = pick(RS, P.rsClaves);
        if (x[CP] > 0) cpMaracasActive = pick(CP, P.cpMaracas);
        for (int t = 0; t < 3; ++t) if (x[LT + t] > 0) congaActive[t] = pick(LT + t, isCon[t]);

        double v[NumVoices];
        v[BD] = tickBD(x[BD]);
        v[SD] = tickSD(x[SD]);
        for (int t = 0; t < 3; ++t) {
            const R& rt = tomR[t]; const R& rc = conR[t];
            double xt = congaActive[t] ? 0 : x[LT + t], xc = congaActive[t] ? x[LT + t] : 0;
            double a = tickTom(tom[t], xt, dialMap(tunes[t], rt.lo, rt.hi), P.tomDecay[t], P.tomBend[t], 0.02);
            double b = tickTom(conga[t], xc, dialMap(tunes[t], rc.lo, rc.hi), P.congaDecay[t], P.congaBend[t], 0.0);
            v[LT + t] = a + b;    // both keep ringing out naturally when the switch flips
        }
        v[RS] = tickRS(rsClavesActive ? 0 : x[RS]) + tickCL(rsClavesActive ? x[RS] : 0);
        v[CP] = tickCP(cpMaracasActive ? 0 : x[CP]) + tickMA(cpMaracasActive ? x[CP] : 0);
        v[CB] = tickCB(x[CB]);
        double ch, oh, cy;
        tickMetal(x[CH], x[OH], x[CY], ch, oh, cy);
        v[CH] = ch; v[OH] = oh; v[CY] = cy;

        double sum = 0;
        for (int k = 0; k < NumVoices; ++k) {
            double o = v[k] * lvl[k];
            if (outs && outs[k]) outs[k][i] = (float)o;
            sum += o;
        }
        mix[i] = (float)(sum * master);
    }
}

} // namespace unit808
