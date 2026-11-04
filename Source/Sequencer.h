// UNIT808 rhythm memory and the 808 play logic (no JUCE dependency, unit-testable).
#pragma once
#include <cstdint>
#include <vector>
#include <atomic>
#include <algorithm>

namespace unit808 {

constexpr int kNumRows  = 12;   // 0 = AC, 1..11 = BD SD LT MT HT RS CP CB CY OH CH (instrument-select order)
constexpr int kNumSlots = 16;   // 0..11 basic rhythms, 12..15 intro / fill in
constexpr int kNumTracks = 12;  // rhythm tracks (songs), chosen with INSTRUMENT-SELECT in PLAY / COMPOSE
constexpr int kMaxTrack  = 64;
constexpr int kFirstFill = 12;

// One part of one variation of one rhythm: up to 16 steps.
struct Part {
    std::atomic<uint16_t> rows[kNumRows] {};
    std::atomic<int> length { 16 };      // 1..16; for the 2nd part 0 = not used
    bool cell(int r, int s) const { return (rows[r].load() >> s) & 1; }
    void set(int r, int s, bool on) { uint16_t v = rows[r].load(); v = on ? (uint16_t)(v | (1u << s)) : (uint16_t)(v & ~(1u << s)); rows[r] = v; }
    void clear() { for (auto& r : rows) r = 0; }
    bool empty() const { for (auto& r : rows) if (r.load()) return false; return true; }
};

struct Rhythm { Part part[2][2]; };   // [variation A/B][1st/2nd part]

struct TrackEntry { int slot = 0; int var = 0; };   // var: 0 = A, 1 = AB, 2 = B (the lever when it was written)

struct Memory {
    Rhythm rhythm[kNumSlots];
    std::atomic<int> trackEntries[kNumTracks][kMaxTrack] {};   // slot * 4 + var
    std::atomic<int> trackLen[kNumTracks] {};
    std::atomic<int> version { 0 };
    Memory() { for (auto& r : rhythm) for (auto& v : r.part) v[1].length = 0; }

    TrackEntry entry(int t, int i) const { int e = trackEntries[t][i].load(); return { e / 4, e % 4 }; }
    void append(int t, TrackEntry e) { int n = trackLen[t].load(); if (n >= kMaxTrack) return; trackEntries[t][n] = e.slot * 4 + e.var; trackLen[t] = n + 1; ++version; }
    void clearTrack(int t) { trackLen[t] = 0; ++version; }
    void clearRhythm(int slot, int var) {   // var 0 = A, 1 = both, 2 = B
        for (int v = 0; v < 2; ++v) if (var == 1 || var == v * 2) { rhythm[slot].part[v][0].clear(); rhythm[slot].part[v][0].length = 16;
                                                                     rhythm[slot].part[v][1].clear(); rhythm[slot].part[v][1].length = 0; }
        ++version;
    }
};

// One played measure: a part of a variation of a slot.
struct Segment { int slot, var, part; };

} // namespace unit808
