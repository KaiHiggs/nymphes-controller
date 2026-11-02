// plugin/cc/CcSpec.cpp  —  L1

#include "plugin/cc/CcSpec.h"

#include <array>
#include <cassert>

namespace nymphes::ccui
{

namespace
{
    std::string_view sv (const char* s) noexcept
    {
        return s != nullptr ? std::string_view (s) : std::string_view {};
    }

    // Split "poly|uni-A|uni-B|TRI|DUO|MONO" into views. The source string is a
    // string literal in midi/CCMap.h, so the views stay valid for the lifetime of
    // the process; nothing is copied.
    //
    // Empty segments are skipped: a typo in the table ("off|on|" or "||") must not
    // produce a blank combo-box row that isDiscrete()/labelsSpanRange() would then
    // count as a real value.
    std::vector<std::string_view> splitLabels (const char* packed)
    {
        std::vector<std::string_view> out;
        if (packed == nullptr)
            return out;

        std::string_view all (packed);
        if (all.empty())
            return out;

        std::size_t start = 0;
        while (start <= all.size())
        {
            const auto bar = all.find ('|', start);
            const auto end = (bar == std::string_view::npos) ? all.size() : bar;
            if (end != start)
                out.push_back (all.substr (start, end - start));
            if (bar == std::string_view::npos)
                break;
            start = bar + 1;
        }
        return out;
    }

    const std::vector<CcSpec>& build()
    {
        static const std::vector<CcSpec> table = []
        {
            std::vector<CcSpec> v;
            v.reserve (static_cast<std::size_t> (kCCCount));
            for (int i = 0; i < kCCCount; ++i)
            {
                const auto& p = kCCMap[i];
                v.push_back (CcSpec {
                    p.cc,
                    sv (p.name),
                    groupName (p.group),
                    p.minValue,
                    p.maxValue,
                    splitLabels (p.valueNames)
                });
            }
            return v;
        }();
        return table;
    }
}

// Debug-only tripwire. It is inside #ifndef NDEBUG on purpose: in a Release build
// the assert macro compiles away, which would leave this helper and the loop
// variable unused and cost the project two -Wall warnings for a check that can no
// longer fire. The Release behaviour is decided at the point of use by
// CcSpec::labelsSpanRange(), which is what the widget actually asks.
#ifndef NDEBUG
// A label list is only usable as a value->name map when it covers the range. The
// two LFO wave CCs carry five zone names over a 0..127 sweep (see ccmap.json,
// which has no labels for them) — that is deliberate, so they are the documented
// exceptions here rather than a reason to leave the check out.
static bool labelsAreZonesNotValues (int cc) noexcept
{
    return cc == 19 || cc == 25;   // LFO 1/2 wave
}
#endif

std::string_view groupName (CCGroup g)
{
    switch (g)
    {
        case CCGroup::Oscillator: return "Oscillator";
        case CCGroup::Amp:        return "Amp";
        case CCGroup::Filter:     return "Filter";
        case CCGroup::Reverb:     return "Reverb";
        case CCGroup::Lfo1:       return "LFO 1";
        case CCGroup::Lfo2:       return "LFO 2";
        case CCGroup::Modulation: return "Modulation";
        case CCGroup::General:    return "General";
    }
    // Not an enumerator value: an out-of-range CCGroup renders as "Other" rather
    // than crashing a paint callback. Note this is NOT a `default:` label, so
    // -Wswitch still reports a newly added enumerator instead of silently
    // labelling it "Other".
    return "Other";
}

bool CcSpec::isModDepth() const noexcept
{
    return isModulationDepthCc (static_cast<int> (cc));
}

bool isModulationDepthCc (int cc) noexcept
{
    // Derived from the table, not restated as magic numbers: the depth CCs ARE the
    // modulation group minus CC 30, which selects which of the four sources the
    // depths apply to. A table edit that adds or removes a depth then changes this
    // answer, and tests/ccmap_test.cpp pins the expected sets (31..37, 39..63,
    // 86..89) so the change is reviewed instead of drifting.
    const auto* p = findCC (cc);
    if (p == nullptr)
        return false;
    return p->group == CCGroup::Modulation && cc != kModSourceSelectorCc;
}

const std::vector<CcSpec>& allSpecs()
{
    const auto& table = build();

    // Tripwire for a table edit that adds value names without a matching range:
    // the only legal mismatch is the LFO wave sweep above. Debug builds only, so a
    // release build still renders (labelsSpanRange() decides the widget) instead of
    // aborting the editor.
#ifndef NDEBUG
    assert (table.size() == static_cast<std::size_t> (kCCCount));
    for (const auto& s : table)
        assert (s.labels.empty() || s.labelsSpanRange() || labelsAreZonesNotValues ((int) s.cc));
#endif

    return table;
}

const CcSpec* findSpec (int cc)
{
    const auto& all = allSpecs();
    for (const auto& s : all)
        if (static_cast<int> (s.cc) == cc)
            return &s;
    return nullptr;
}

} // namespace nymphes::ccui
