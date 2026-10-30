// The Nymphes CC map — all 82 MIDI-controllable parameters.
//
// Source: the vendor-published CC list for firmware 2.1, cross-checked against
// the public midi.guide table for the same firmware. This file is the single
// owner of the CC *data*; the CC *engine* (learn, soft takeover, state) lives in
// plugin/cc/ and only reads this table.
//
// Conventions that matter for a controller:
//   * Every continuous parameter is 0..127, "0-based" orientation: 0 is the
//     minimum, 127 the maximum, and the device does NOT rescale by the value it
//     last received. So a slider must send absolute values, not deltas.
//   * Discrete parameters have a small enumerated range (0..1, 0..3, 0..5) and
//     the device expects that small range, not 0..127. Sending 127 to CC 22
//     (LFO 1 type) is not "the last type", it is out of range.
//   * CC 38 is deliberately absent from the device: the modulation-depth run is
//     31..37 then 39..46, so a controller must not assume CC n ↔ parameter n.
//   * These CCs are for parameter control. They are NOT the SysEx preset path:
//     the plugin uses SysEx for preset read/write and CC for live parameter
//     changes, and the two must not be conflated.

#pragma once

#include <cstdint>

namespace nymphes
{

enum class CCGroup : uint8_t
{
    Oscillator = 0,
    Amp,
    Filter,
    Reverb,
    Lfo1,
    Lfo2,
    Modulation,
    General
};

struct CCParam
{
    uint8_t     cc;
    const char* name;
    int         minValue;     // 0 for continuous, otherwise the enum's low bound
    int         maxValue;     // 127 for continuous
    CCGroup     group;
    const char* valueNames;   // nullptr for continuous; "a|b|c" for discrete

    bool isDiscrete() const noexcept { return minValue != 0 || maxValue != 127; }
    bool isValidValue (int v) const noexcept { return v >= minValue && v <= maxValue; }
};

// 82 entries, ordered by CC number.
inline constexpr CCParam kCCMap[] = {
    {   1, "mod_wheel",                     0, 127, CCGroup::General,    nullptr },
    {   3, "lpf_eg_depth",                  0, 127, CCGroup::Filter,     nullptr },
    {   4, "lpf_tracking",                  0, 127, CCGroup::Filter,     nullptr },
    {   5, "glide",                         0, 127, CCGroup::General,    nullptr },
    {   7, "amp_level",                     0, 127, CCGroup::Amp,        nullptr },
    {   8, "lfo_cutoff_depth",              0, 127, CCGroup::Filter,     nullptr },
    {   9, "osc_level",                     0, 127, CCGroup::Oscillator, nullptr },
    {  10, "sub_level",                     0, 127, CCGroup::Oscillator, nullptr },
    {  11, "noise_level",                   0, 127, CCGroup::Oscillator, nullptr },
    {  12, "pulse_width",                   0, 127, CCGroup::Oscillator, nullptr },
    {  13, "lfo_pitch_depth",               0, 127, CCGroup::Oscillator, nullptr },
    {  14, "eg_pitch_depth",                0, 127, CCGroup::Oscillator, nullptr },
    {  15, "detune",                        0, 127, CCGroup::Oscillator, nullptr },
    {  16, "chord_selector",                0, 127, CCGroup::Oscillator, nullptr },
    {  17, "play_mode",                     0,   5, CCGroup::General,    "poly|uni-A|uni-B|TRI|DUO|MONO" },
    {  18, "lfo1_rate",                     0, 127, CCGroup::Lfo1,       nullptr },
    {  19, "lfo1_wave",                     0, 127, CCGroup::Lfo1,       "triangle|saw_down|saw_up|square|random" },
    {  20, "lfo1_delay",                    0, 127, CCGroup::Lfo1,       nullptr },
    {  21, "lfo1_fade",                     0, 127, CCGroup::Lfo1,       nullptr },
    {  22, "lfo1_type",                     0,   3, CCGroup::Lfo1,       "BPM|low|high|track" },
    {  23, "lfo1_key_sync",                 0,   1, CCGroup::Lfo1,       "off|on" },
    {  24, "lfo2_rate",                     0, 127, CCGroup::Lfo2,       nullptr },
    {  25, "lfo2_wave",                     0, 127, CCGroup::Lfo2,       "triangle|saw_down|saw_up|square|random" },
    {  26, "lfo2_delay",                    0, 127, CCGroup::Lfo2,       nullptr },
    {  27, "lfo2_fade",                     0, 127, CCGroup::Lfo2,       nullptr },
    {  28, "lfo2_type",                     0,   3, CCGroup::Lfo2,       "BPM|low|high|track" },
    {  29, "lfo2_key_sync",                 0,   1, CCGroup::Lfo2,       "off|on" },
    {  30, "ms_source",                     0,   3, CCGroup::Modulation, "lfo2|mod_wheel|velocity|aftertouch" },
    {  31, "ms_osc_wave_depth",             0, 127, CCGroup::Modulation, nullptr },
    {  32, "ms_osc_level_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  33, "ms_sub_level_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  34, "ms_noise_level_depth",          0, 127, CCGroup::Modulation, nullptr },
    {  35, "ms_lfo_pitch_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  36, "ms_pulse_width_depth",          0, 127, CCGroup::Modulation, nullptr },
    {  37, "ms_glide_depth",                0, 127, CCGroup::Modulation, nullptr },
    // CC 38 does not exist on this device.
    {  39, "ms_detune_depth",               0, 127, CCGroup::Modulation, nullptr },
    {  40, "ms_chord_selector_depth",       0, 127, CCGroup::Modulation, nullptr },
    {  41, "ms_eg_pitch_depth",             0, 127, CCGroup::Modulation, nullptr },
    {  42, "ms_lpf_cutoff_depth",           0, 127, CCGroup::Modulation, nullptr },
    {  43, "ms_resonance_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  44, "ms_lpf_eg_depth",               0, 127, CCGroup::Modulation, nullptr },
    {  45, "ms_hpf_cutoff_depth",           0, 127, CCGroup::Modulation, nullptr },
    {  46, "ms_lpf_tracking_depth",         0, 127, CCGroup::Modulation, nullptr },
    {  47, "ms_lpf_cutoff_lfo_depth",       0, 127, CCGroup::Modulation, nullptr },
    {  48, "ms_filter_eg_attack_depth",     0, 127, CCGroup::Modulation, nullptr },
    {  49, "ms_filter_eg_decay_depth",      0, 127, CCGroup::Modulation, nullptr },
    {  50, "ms_filter_eg_sustain_depth",    0, 127, CCGroup::Modulation, nullptr },
    {  51, "ms_filter_eg_release_depth",    0, 127, CCGroup::Modulation, nullptr },
    {  52, "ms_amp_eg_attack_depth",        0, 127, CCGroup::Modulation, nullptr },
    {  53, "ms_amp_eg_decay_depth",         0, 127, CCGroup::Modulation, nullptr },
    {  54, "ms_amp_eg_sustain_depth",       0, 127, CCGroup::Modulation, nullptr },
    {  55, "ms_amp_eg_release_depth",       0, 127, CCGroup::Modulation, nullptr },
    {  56, "ms_lfo1_rate_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  57, "ms_lfo1_wave_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  58, "ms_lfo1_delay_depth",           0, 127, CCGroup::Modulation, nullptr },
    {  59, "ms_lfo1_fade_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  60, "ms_lfo2_rate_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  61, "ms_lfo2_wave_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  62, "ms_lfo2_delay_depth",           0, 127, CCGroup::Modulation, nullptr },
    {  63, "ms_lfo2_fade_depth",            0, 127, CCGroup::Modulation, nullptr },
    {  64, "sustain_pedal",                 0,   1, CCGroup::General,    "off|on" },
    {  68, "legato",                        0,   1, CCGroup::General,    "off|on" },
    {  70, "osc_wave",                      0, 127, CCGroup::Oscillator, nullptr },
    {  71, "resonance",                     0, 127, CCGroup::Filter,     nullptr },
    {  72, "amp_eg_release",                0, 127, CCGroup::Amp,        nullptr },
    {  73, "amp_eg_attack",                 0, 127, CCGroup::Amp,        nullptr },
    {  74, "lpf_cutoff",                    0, 127, CCGroup::Filter,     nullptr },
    {  75, "reverb_size",                   0, 127, CCGroup::Reverb,     nullptr },
    {  76, "reverb_decay",                  0, 127, CCGroup::Reverb,     nullptr },
    {  77, "reverb_filter",                 0, 127, CCGroup::Reverb,     nullptr },
    {  78, "reverb_mix",                    0, 127, CCGroup::Reverb,     nullptr },
    {  79, "filter_eg_attack",              0, 127, CCGroup::Filter,     nullptr },
    {  80, "filter_eg_decay",               0, 127, CCGroup::Filter,     nullptr },
    {  81, "hpf_cutoff",                    0, 127, CCGroup::Filter,     nullptr },
    {  82, "filter_eg_sustain",             0, 127, CCGroup::Filter,     nullptr },
    {  83, "filter_eg_release",             0, 127, CCGroup::Filter,     nullptr },
    {  84, "amp_eg_decay",                  0, 127, CCGroup::Amp,        nullptr },
    {  85, "amp_eg_sustain",                0, 127, CCGroup::Amp,        nullptr },
    {  86, "ms_reverb_size_depth",          0, 127, CCGroup::Modulation, nullptr },
    {  87, "ms_reverb_decay_depth",         0, 127, CCGroup::Modulation, nullptr },
    {  88, "ms_reverb_filter_depth",        0, 127, CCGroup::Modulation, nullptr },
    {  89, "ms_reverb_mix_depth",           0, 127, CCGroup::Modulation, nullptr },
};

inline constexpr int kCCCount = static_cast<int> (sizeof (kCCMap) / sizeof (kCCMap[0]));

/** nullptr when the CC is not a Nymphes parameter. */
inline const CCParam* findCC (int cc) noexcept
{
    for (const auto& p : kCCMap)
        if (p.cc == static_cast<uint8_t> (cc))
            return &p;
    return nullptr;
}

/** Index in kCCMap, or -1. */
inline int indexOfCC (int cc) noexcept
{
    for (int i = 0; i < kCCCount; ++i)
        if (kCCMap[i].cc == static_cast<uint8_t> (cc))
            return i;
    return -1;
}

} // namespace nymphes
