// plugin/cc/CcSpec.cpp  —  L1

#include "plugin/cc/CcSpec.h"

#include <array>

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
    std::vector<std::string_view> splitLabels (const char* packed)
    {
        std::vector<std::string_view> out;
        if (packed == nullptr)
            return out;

        std::string_view all (packed);
        std::size_t start = 0;
        while (start <= all.size())
        {
            const auto bar = all.find ('|', start);
            if (bar == std::string_view::npos)
            {
                out.push_back (all.substr (start));
                break;
            }
            out.push_back (all.substr (start, bar - start));
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
                    splitLabels (p.valueNames),
                    isModulationDepthCc (static_cast<int> (p.cc))
                });
            }
            return v;
        }();
        return table;
    }
}

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
    return "Other";
}

bool isModulationDepthCc (int cc) noexcept
{
    // The modulation-depth run is 31..37 then 39..63, plus the four reverb depths
    // 86..89. CC 38 is genuinely absent from the device, which is why this is two
    // ranges rather than one.
    if (cc >= 31 && cc <= 37)  return true;
    if (cc >= 39 && cc <= 63)  return true;
    if (cc >= 86 && cc <= 89)  return true;
    return false;
}

const std::vector<CcSpec>& allSpecs() { return build(); }

const CcSpec* findSpec (int cc)
{
    const auto& all = allSpecs();
    for (const auto& s : all)
        if (static_cast<int> (s.cc) == cc)
            return &s;
    return nullptr;
}

} // namespace nymphes::ccui
