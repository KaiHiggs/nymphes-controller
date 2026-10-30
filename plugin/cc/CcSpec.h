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
    std::vector<std::string_view> labels;     // empty for continuous parameters
    bool                         isModDepth;  // shared across the 4 mod sources

    bool isDiscrete() const noexcept { return ! labels.empty(); }
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

inline constexpr int kModSourceSelectorCc = 30;
inline constexpr int kSustainCc           = 64;
inline constexpr int kModWheelCc          = 1;

} // namespace nymphes::ccui
