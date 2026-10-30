// GENERATED FILE - do not edit.  Source: params/params.json
// Regenerate with: python3 tools/gen_cc_header.py
// Verified current by: python3 tools/gen_cc_header.py --check
//
// The 82 MIDI CCs of the Dreadbox Nymphes, plus the 28-slot `basic_inputs`
// float order and the preset frame constants.  Canonical data lives in
// params/params.json; this header is derived from it.

#pragma once

#include <cstddef>
#include <cstdint>

namespace nymphes {
namespace params {

inline constexpr std::size_t kParamCount = 82;

struct Param {
    std::uint8_t cc;             // MIDI controller number
    const char*  id;             // stable machine id, e.g. "oscillator.wave"
    const char*  name;           // chart name, e.g. "OSC wave"
    const char*  group;          // oscillator | filter | ... | general
    std::uint8_t min;            // lowest legal value
    std::uint8_t max;            // highest legal value
    bool         discrete;       // true when `labels` names every legal value
    const char* const* labels;   // max-min+1 entries when discrete, else nullptr
    int          modDepthCc;     // CC of this parameter's mod depth, or -1
    int          modTargetCc;    // for a depth CC: the CC it modulates, or -1
};
inline constexpr int kNoCc = -1;

inline constexpr const char* kLabels_misc_mode[] = {
    "POLY",
    "UNI A",
    "UNI B",
    "TRI",
    "DUO",
    "MONO",

};

inline constexpr const char* kLabels_lfo_lfo_1_type[] = {
    "BPM",
    "LOW",
    "HIGH",
    "TRACK",

};

inline constexpr const char* kLabels_lfo_lfo_1_sync[] = {
    "FREE",
    "KEY SYNC",

};

inline constexpr const char* kLabels_lfo_lfo_2_type[] = {
    "BPM",
    "LOW",
    "HIGH",
    "TRACK",

};

inline constexpr const char* kLabels_lfo_lfo_2_sync[] = {
    "FREE",
    "KEY SYNC",

};

inline constexpr const char* kLabels_modulators_selector[] = {
    "LFO 2",
    "Mod Wheel",
    "Velocity",
    "Aftertouch",

};

inline constexpr const char* kLabels_general_cc64[] = {
    "Off",
    "On",

};

inline constexpr const char* kLabels_misc_legato[] = {
    "Off",
    "On",

};

inline constexpr Param kParams[] = {
    {  1, "general.cc1", "Mod wheel", "general",   0, 127, false, nullptr, kNoCc, kNoCc},
    {  3, "filter.eg", "LPF EG depth", "filter",   0, 127, false, nullptr,    44, kNoCc},
    {  4, "filter.track", "LPF tracking", "filter",   0, 127, false, nullptr,    46, kNoCc},
    {  5, "oscillator.gld", "Glide", "general",   0, 127, false, nullptr,    37, kNoCc},
    {  7, "misc.amp", "Amp level", "amp",   0, 127, false, nullptr, kNoCc, kNoCc},
    {  8, "filter.lfo", "LFO cutoff depth", "filter",   0, 127, false, nullptr,    47, kNoCc},
    {  9, "oscillator.lvl", "Oscillator level", "oscillator",   0, 127, false, nullptr,    32, kNoCc},
    { 10, "oscillator.sub", "Sub level", "oscillator",   0, 127, false, nullptr,    33, kNoCc},
    { 11, "oscillator.noise", "Noise level", "oscillator",   0, 127, false, nullptr,    34, kNoCc},
    { 12, "oscillator.pw", "Pulse width", "oscillator",   0, 127, false, nullptr,    36, kNoCc},
    { 13, "oscillator.lfo", "LFO pitch depth", "oscillator",   0, 127, false, nullptr,    35, kNoCc},
    { 14, "oscillator.eg", "EG pitch depth", "oscillator",   0, 127, false, nullptr,    41, kNoCc},
    { 15, "oscillator.dtn", "Detune", "oscillator",   0, 127, false, nullptr,    39, kNoCc},
    { 16, "oscillator.chord", "Chord selector", "oscillator",   0, 127, false, nullptr,    40, kNoCc},
    { 17, "misc.mode", "Play mode", "general",   0,   5, true , kLabels_misc_mode, kNoCc, kNoCc},
    { 18, "lfo.lfo-1.rate", "LFO 1 rate", "lfo1",   0, 127, false, nullptr,    56, kNoCc},
    { 19, "lfo.lfo-1.wave", "LFO 1 wave", "lfo1",   0, 127, false, nullptr,    57, kNoCc},
    { 20, "lfo.lfo-1.delay", "LFO 1 delay", "lfo1",   0, 127, false, nullptr,    58, kNoCc},
    { 21, "lfo.lfo-1.fade", "LFO 1 fade", "lfo1",   0, 127, false, nullptr,    59, kNoCc},
    { 22, "lfo.lfo-1.type", "LFO 1 type", "lfo1",   0,   3, true , kLabels_lfo_lfo_1_type, kNoCc, kNoCc},
    { 23, "lfo.lfo-1.sync", "LFO 1 key sync", "lfo1",   0,   1, true , kLabels_lfo_lfo_1_sync, kNoCc, kNoCc},
    { 24, "lfo.lfo-2.rate", "LFO 2 rate", "lfo2",   0, 127, false, nullptr,    60, kNoCc},
    { 25, "lfo.lfo-2.wave", "LFO 2 wave", "lfo2",   0, 127, false, nullptr,    61, kNoCc},
    { 26, "lfo.lfo-2.delay", "LFO 2 delay", "lfo2",   0, 127, false, nullptr,    62, kNoCc},
    { 27, "lfo.lfo-2.fade", "LFO 2 fade", "lfo2",   0, 127, false, nullptr,    63, kNoCc},
    { 28, "lfo.lfo-2.type", "LFO 2 type", "lfo2",   0,   3, true , kLabels_lfo_lfo_2_type, kNoCc, kNoCc},
    { 29, "lfo.lfo-2.sync", "LFO 2 key sync", "lfo2",   0,   1, true , kLabels_lfo_lfo_2_sync, kNoCc, kNoCc},
    { 30, "modulators.selector", "Modulation source selector", "modulation",   0,   3, true , kLabels_modulators_selector, kNoCc, kNoCc},
    { 31, "mod.oscillator.wave.depth", "MS OSC wave depth", "modulation",   0, 127, false, nullptr, kNoCc,    70},
    { 32, "mod.oscillator.lvl.depth", "MS OSC level depth", "modulation",   0, 127, false, nullptr, kNoCc,     9},
    { 33, "mod.oscillator.sub.depth", "MS SUB level depth", "modulation",   0, 127, false, nullptr, kNoCc,    10},
    { 34, "mod.oscillator.noise.depth", "MS Noise level depth", "modulation",   0, 127, false, nullptr, kNoCc,    11},
    { 35, "mod.oscillator.lfo.depth", "MS LFO pitch depth depth", "modulation",   0, 127, false, nullptr, kNoCc,    13},
    { 36, "mod.oscillator.pw.depth", "MS Pulse width depth", "modulation",   0, 127, false, nullptr, kNoCc,    12},
    { 37, "mod.oscillator.gld.depth", "MS Glide depth", "modulation",   0, 127, false, nullptr, kNoCc,     5},
    { 39, "mod.oscillator.dtn.depth", "MS Detune depth", "modulation",   0, 127, false, nullptr, kNoCc,    15},
    { 40, "mod.oscillator.chord.depth", "MS Chord selector depth", "modulation",   0, 127, false, nullptr, kNoCc,    16},
    { 41, "mod.oscillator.eg.depth", "MS EG pitch depth depth", "modulation",   0, 127, false, nullptr, kNoCc,    14},
    { 42, "mod.filter.cut.depth", "MS LPF cutoff depth", "modulation",   0, 127, false, nullptr, kNoCc,    74},
    { 43, "mod.filter.res.depth", "MS Resonance depth", "modulation",   0, 127, false, nullptr, kNoCc,    71},
    { 44, "mod.filter.eg.depth", "MS LPF EG depth depth", "modulation",   0, 127, false, nullptr, kNoCc,     3},
    { 45, "mod.filter.hpf.depth", "MS HPF cutoff depth", "modulation",   0, 127, false, nullptr, kNoCc,    81},
    { 46, "mod.filter.track.depth", "MS LPF tracking depth", "modulation",   0, 127, false, nullptr, kNoCc,     4},
    { 47, "mod.filter.lfo.depth", "MS LPF cutoff LFO depth depth", "modulation",   0, 127, false, nullptr, kNoCc,     8},
    { 48, "mod.envelope.filter.a.depth", "MS Filter EG attack depth", "modulation",   0, 127, false, nullptr, kNoCc,    79},
    { 49, "mod.envelope.filter.d.depth", "MS Filter EG decay depth", "modulation",   0, 127, false, nullptr, kNoCc,    80},
    { 50, "mod.envelope.filter.s.depth", "MS Filter EG sustain depth", "modulation",   0, 127, false, nullptr, kNoCc,    82},
    { 51, "mod.envelope.filter.r.depth", "MS Filter EG release depth", "modulation",   0, 127, false, nullptr, kNoCc,    83},
    { 52, "mod.envelope.amplitude.a.depth", "MS Amp EG attack depth", "modulation",   0, 127, false, nullptr, kNoCc,    73},
    { 53, "mod.envelope.amplitude.d.depth", "MS Amp EG decay depth", "modulation",   0, 127, false, nullptr, kNoCc,    84},
    { 54, "mod.envelope.amplitude.s.depth", "MS Amp EG sustain depth", "modulation",   0, 127, false, nullptr, kNoCc,    85},
    { 55, "mod.envelope.amplitude.r.depth", "MS Amp EG release depth", "modulation",   0, 127, false, nullptr, kNoCc,    72},
    { 56, "mod.lfo.lfo-1.rate.depth", "MS LFO 1 rate depth", "modulation",   0, 127, false, nullptr, kNoCc,    18},
    { 57, "mod.lfo.lfo-1.wave.depth", "MS LFO 1 wave depth", "modulation",   0, 127, false, nullptr, kNoCc,    19},
    { 58, "mod.lfo.lfo-1.delay.depth", "MS LFO 1 delay depth", "modulation",   0, 127, false, nullptr, kNoCc,    20},
    { 59, "mod.lfo.lfo-1.fade.depth", "MS LFO 1 fade depth", "modulation",   0, 127, false, nullptr, kNoCc,    21},
    { 60, "mod.lfo.lfo-2.rate.depth", "MS LFO 2 rate depth", "modulation",   0, 127, false, nullptr, kNoCc,    24},
    { 61, "mod.lfo.lfo-2.wave.depth", "MS LFO 2 wave depth", "modulation",   0, 127, false, nullptr, kNoCc,    25},
    { 62, "mod.lfo.lfo-2.delay.depth", "MS LFO 2 delay depth", "modulation",   0, 127, false, nullptr, kNoCc,    26},
    { 63, "mod.lfo.lfo-2.fade.depth", "MS LFO 2 fade depth", "modulation",   0, 127, false, nullptr, kNoCc,    27},
    { 64, "general.cc64", "Sustain pedal", "general",   0,   1, true , kLabels_general_cc64, kNoCc, kNoCc},
    { 68, "misc.legato", "Legato", "general",   0,   1, true , kLabels_misc_legato, kNoCc, kNoCc},
    { 70, "oscillator.wave", "OSC wave", "oscillator",   0, 127, false, nullptr,    31, kNoCc},
    { 71, "filter.res", "Resonance", "filter",   0, 127, false, nullptr,    43, kNoCc},
    { 72, "envelope.amplitude.r", "Amp EG release", "amp",   0, 127, false, nullptr,    55, kNoCc},
    { 73, "envelope.amplitude.a", "Amp EG attack", "amp",   0, 127, false, nullptr,    52, kNoCc},
    { 74, "filter.cut", "LPF cutoff", "filter",   0, 127, false, nullptr,    42, kNoCc},
    { 75, "reverb.size", "Reverb size", "reverb",   0, 127, false, nullptr,    86, kNoCc},
    { 76, "reverb.decay", "Reverb decay", "reverb",   0, 127, false, nullptr,    87, kNoCc},
    { 77, "reverb.filter", "Reverb filter", "reverb",   0, 127, false, nullptr,    88, kNoCc},
    { 78, "reverb.mix", "Reverb mix", "reverb",   0, 127, false, nullptr,    89, kNoCc},
    { 79, "envelope.filter.a", "Filter EG attack", "filter",   0, 127, false, nullptr,    48, kNoCc},
    { 80, "envelope.filter.d", "Filter EG decay", "filter",   0, 127, false, nullptr,    49, kNoCc},
    { 81, "filter.hpf", "HPF cutoff", "filter",   0, 127, false, nullptr,    45, kNoCc},
    { 82, "envelope.filter.s", "Filter EG sustain", "filter",   0, 127, false, nullptr,    50, kNoCc},
    { 83, "envelope.filter.r", "Filter EG release", "filter",   0, 127, false, nullptr,    51, kNoCc},
    { 84, "envelope.amplitude.d", "Amp EG decay", "amp",   0, 127, false, nullptr,    53, kNoCc},
    { 85, "envelope.amplitude.s", "Amp EG sustain", "amp",   0, 127, false, nullptr,    54, kNoCc},
    { 86, "mod.reverb.size.depth", "MS Reverb size depth", "modulation",   0, 127, false, nullptr, kNoCc,    75},
    { 87, "mod.reverb.decay.depth", "MS Reverb decay depth", "modulation",   0, 127, false, nullptr, kNoCc,    76},
    { 88, "mod.reverb.filter.depth", "MS Reverb filter depth", "modulation",   0, 127, false, nullptr, kNoCc,    77},
    { 89, "mod.reverb.mix.depth", "MS Reverb mix depth", "modulation",   0, 127, false, nullptr, kNoCc,    78},
};

inline constexpr std::size_t kBasicInputCount = 28;
struct BasicInput {
    const char* name;   // sample-schema slot name, e.g. "wave"
    std::uint8_t cc;    // the CC that carries this slot in the main section
};
inline constexpr BasicInput kBasicInputs[] = {
    {"wave",  70},
    {"lvl",   9},
    {"sub",  10},
    {"noise",  11},
    {"osc_lfo",  13},
    {"cut",  74},
    {"reson",  71},
    {"cut_eg",   3},
    {"a1",  79},
    {"d1",  80},
    {"s1",  82},
    {"r1",  83},
    {"lfo_rate",  18},
    {"lfo_wave",  19},
    {"pw",  12},
    {"glide",   5},
    {"dtune",  15},
    {"chord",  16},
    {"osc_eg",  14},
    {"hpf",  81},
    {"track",   4},
    {"cut_lfo",   8},
    {"a2",  73},
    {"d2",  84},
    {"s2",  85},
    {"r2",  72},
    {"lfo_delay",  20},
    {"lfo_fade",  21},
};

// ---- preset frame constants (2109-byte SysEx message)
inline constexpr std::size_t kFrameBytes     = 2109;
inline constexpr std::size_t kPayloadBytes   = 1049;
inline constexpr std::size_t kNibbleBytes    = 2098;
inline constexpr std::uint8_t kCrcPoly       = 0x31;
inline constexpr std::uint8_t kPageUser      = 0x00;
inline constexpr std::uint8_t kPageFactory   = 0x01;
inline constexpr std::uint8_t kFlagLoadOnly  = 0x00;
inline constexpr std::uint8_t kFlagWrite     = 0x01;
inline constexpr std::uint8_t kBanks         = 7;
inline constexpr std::uint8_t kPrograms      = 7;
inline constexpr std::size_t kSlotsPerPage   = 49;

// Lookups.  Return nullptr when the CC/name is unknown.
inline const Param* findCc(int cc) {
    for (const Param& p : kParams)
        if (p.cc == cc) return &p;
    return nullptr;
}
inline const Param* findId(const char* id) {
    if (id == nullptr) return nullptr;
    for (const Param& p : kParams) {
        const char* a = p.id;
        const char* b = id;
        while (*a && *a == *b) { ++a; ++b; }
        if (*a == '\0' && *b == '\0') return &p;
    }
    return nullptr;
}
// Label for a discrete value, nullptr when the value is out of range.
inline const char* labelFor(const Param& p, int value) {
    if (!p.discrete || p.labels == nullptr) return nullptr;
    if (value < p.min || value > p.max) return nullptr;
    return p.labels[value - p.min];
}
}  // namespace params
}  // namespace nymphes
