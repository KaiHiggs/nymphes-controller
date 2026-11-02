// plugin/cc/CcSpec.h  —  L1
//
// A UI-shaped view over the ONE CC table.
//
// The CC *data* has exactly one owner: midi/CCMap.h (L3), 82 constexpr entries.
// This file does not add a second copy. It only reshapes nymphes::CCParam into
// what the editor wants: a group name as text, and the "a|b|c" value-name string
// split into a vector so a combo box can iterate it.
//
// Nothing here is a second source of truth: if a name or range disagrees with
// midi/CCMap.h, midi/CCMap.h is right and this is a bug.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "midi/CCMap.h"

namespace nymphes::ccui
{

struct CcSpec
{
    std::uint8_t                 cc;
    std::string_view             name;
    std::string_view             group;       // "Oscillator", "Filter", ...
    int                          minValue;
    int                          maxValue;
    std::vector<std::string_view> labels;     // value names from the table, if any

    /** The data owner's rule (midi/CCMap.h CCParam::isDiscrete): a discrete
        parameter has a small enumerated range, a continuous one is 0..127.
        Deliberately NOT "labels is non-empty": the table carries five wave names
        for CC 19/25 while their range is 0..127 (ccmap.json has no labels for
        them), and a label list must never decide how the parameter is edited. */
    bool isDiscrete() const noexcept { return minValue != 0 || maxValue != 127; }

    /** True when the table carries value names for this CC. */
    bool hasValueNames() const noexcept { return ! labels.empty(); }

    /** True when the label list covers every value of the range: the only case
        in which a combo box can show and write the parameter without clamping
        (CC 19/25 have 5 names over 0..127, which is a sweep across named zones,
        not a 5-step enum — see FaderCell::zoneName). */
    bool labelsSpanRange() const noexcept
    {
        return ! labels.empty() && (int) labels.size() == maxValue - minValue + 1;
    }

    /** Shared across the 4 mod sources. Derived from the table, never stored, so
        it cannot disagree with isModulationDepthCc(). */
    bool isModDepth() const noexcept;
};

/** All 82 parameters in CC order. Built once; stable for the process lifetime. */
const std::vector<CcSpec>& allSpecs();

/** nullptr when the CC is not a Nymphes parameter (e.g. 38). */
const CcSpec* findSpec (int cc);

/** "Oscillator" for CCGroup::Oscillator, etc. */
std::string_view groupName (CCGroup);

/**
 * The modulation-depth CCs are shared by all four modulation sources and are
 * reinterpreted according to CC 30 (the MS selector). This is the fact that makes
 * a bare "depth 64" ambiguous in a UI, so it is exposed rather than inferred.
 */
bool isModulationDepthCc (int cc) noexcept;

// The three CC numbers the UI treats specially. They are duplicated here (the
// table is midi/CCMap.h's) only so the engine path can name them; tests/ccmap_test.cpp
// asserts each one still points at the expected table entry, so a table edit that
// renumbers them fails the suite instead of silently mislabelling the UI.
inline constexpr int kModSourceSelectorCc = 30;   // "ms_source"
inline constexpr int kSustainCc           = 64;   // "sustain_pedal"
inline constexpr int kModWheelCc          = 1;    // "mod_wheel"

} // namespace nymphes::ccui
