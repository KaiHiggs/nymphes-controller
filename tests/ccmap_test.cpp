// tests/ccmap_test.cpp — L1
//
// The CC map has one owner (midi/CCMap.h). These checks assert the properties
// that a controller depends on, so a table edit cannot silently break the UI.
// Sixteen CC-map assertions plus the per-CC loop bodies; it exits non-zero on any
// failure so ctest reports it.

#include "midi/CCMap.h"
#include "plugin/cc/CcSpec.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check (bool ok, const std::string& what)
{
    if (! ok)
    {
        std::printf ("FAIL  %s\n", what.c_str());
        ++failures;
    }
}
}

int main()
{
    using namespace nymphes;

    // 1. Exactly 82 MIDI-controllable parameters.
    check (kCCCount == 82, "kCCCount == 82, got " + std::to_string (kCCCount));

    // 2. CC 38 is absent; the modulation-depth run is 31..37 then 39..63.
    check (findCC (38) == nullptr, "CC 38 must not exist");
    for (int cc = 31; cc <= 37; ++cc)
        check (findCC (cc) != nullptr, "mod depth CC " + std::to_string (cc) + " present");
    for (int cc = 39; cc <= 63; ++cc)
        check (findCC (cc) != nullptr, "mod depth CC " + std::to_string (cc) + " present");
    for (int cc = 86; cc <= 89; ++cc)
        check (findCC (cc) != nullptr, "reverb depth CC " + std::to_string (cc) + " present");

    // 3. The table is strictly ascending and free of duplicates, so indexOfCC and
    //    the UI's grouping cannot disagree.
    for (int i = 1; i < kCCCount; ++i)
        check (kCCMap[i - 1].cc < kCCMap[i].cc, "CC table ascending at index " + std::to_string (i));

    // 4. Every entry has a name and a group, and indexOfCC round-trips.
    for (int i = 0; i < kCCCount; ++i)
    {
        const auto& p = kCCMap[i];
        check (p.name != nullptr && p.name[0] != '\0', "name present for CC " + std::to_string (p.cc));
        check (p.minValue <= p.maxValue, "range sane for CC " + std::to_string (p.cc));
        // MIDI values are 0..127; a table edit outside that domain would pass the
        // ordering check above and break every controller mapping.
        check (p.minValue >= 0 && p.maxValue <= 127,
               "range inside MIDI 0..127 for CC " + std::to_string (p.cc));
        check (indexOfCC (p.cc) == i, "indexOfCC round-trip for CC " + std::to_string (p.cc));
    }

    // 5. The discrete parameters a controller must not send 0..127 to.
    struct { int cc; int lo; int hi; } discrete[] = {
        { 17, 0, 5 },   // play mode POLY..MONO
        { 22, 0, 3 },   // LFO 1 type
        { 28, 0, 3 },   // LFO 2 type
        { 23, 0, 1 },   // LFO 1 key sync
        { 29, 0, 1 },   // LFO 2 key sync
        { 30, 0, 3 },   // modulation source selector
        { 64, 0, 1 },   // sustain pedal
        { 68, 0, 1 },   // legato
    };
    for (const auto& d : discrete)
    {
        const auto* p = findCC (d.cc);
        check (p != nullptr, "discrete CC " + std::to_string (d.cc) + " present");
        if (p != nullptr)
        {
            check (p->minValue == d.lo && p->maxValue == d.hi,
                   "discrete range for CC " + std::to_string (d.cc));
            check (p->valueNames != nullptr,
                   "discrete CC " + std::to_string (d.cc) + " has value names");
            // The names are indexed by value - min, so a label list that does not
            // match the range would silently read out of bounds (or lose a name).
            if (p->valueNames != nullptr)
            {
                int labelCount = 1;
                for (const char* c = p->valueNames; *c != '\0'; ++c)
                    if (*c == '|')
                        ++labelCount;
                check (labelCount == d.hi - d.lo + 1,
                       "discrete CC " + std::to_string (d.cc) + " has one label per value ("
                           + std::to_string (labelCount) + " vs " + std::to_string (d.hi - d.lo + 1)
                           + ")");
            }
        }
    }

    // 6. The UI view agrees with the table one-for-one: same order, same count,
    //    and the packed "a|b|c" string splits into as many labels as it has bars.
    const auto& specs = ccui::allSpecs();
    check (specs.size() == (std::size_t) kCCCount, "ccui::allSpecs() size matches kCCCount");
    for (int i = 0; i < kCCCount && (std::size_t) i < specs.size(); ++i)
    {
        check (specs[(std::size_t) i].cc == kCCMap[i].cc, "ccui order matches kCCMap order");
        check (specs[(std::size_t) i].minValue == kCCMap[i].minValue, "ccui min matches");
        check (specs[(std::size_t) i].maxValue == kCCMap[i].maxValue, "ccui max matches");
        const bool discreteInTable = kCCMap[i].isDiscrete();
        // Compare against the DATA OWNER's rule, not against "has labels": the
        // table carries five wave names for CC 19/25 whose range is 0..127, so a
        // label-presence rule would disagree with kCCMap[i].isDiscrete().
        check (specs[(std::size_t) i].isDiscrete() == discreteInTable,
               "ccui discreteness matches for CC " + std::to_string (kCCMap[i].cc));
        check (specs[(std::size_t) i].isDiscrete() == kCCMap[i].isDiscrete(),
               "ccui discreteness matches the table rule for CC " + std::to_string (kCCMap[i].cc));

        // A managerable combo box needs one name per value. Every discrete
        // parameter in the table satisfies that; the wave CCs are the documented
        // exception and are continuous, so they must NOT be offered as a combo
        // (the bug: a 5-item combo clamped every value >= 4 to "random").
        const auto& s = specs[(std::size_t) i];
        if (s.isDiscrete() && s.hasValueNames())
            check (s.labelsSpanRange(),
                   "CC " + std::to_string (kCCMap[i].cc) + " labels span its range");
        if (! s.isDiscrete() && s.hasValueNames())
            check (! s.labelsSpanRange(),
                   "continuous CC " + std::to_string (kCCMap[i].cc)
                       + " with zone names is not offered as a combo");
    }

    // 7. The modulation-depth classification matches the device's two ranges.
    for (int cc = 0; cc <= 127; ++cc)
    {
        const bool expected = (cc >= 31 && cc <= 37) || (cc >= 39 && cc <= 63)
                              || (cc >= 86 && cc <= 89);
        check (ccui::isModulationDepthCc (cc) == expected,
               "isModulationDepthCc(" + std::to_string (cc) + ")");
    }

    // 8. Play mode labels are the six the device documents, in order.
    {
        const int playModeIndex = indexOfCC (17);
        // indexOfCC indexes kCCMap, not specs: if the view ever diverges, indexing
        // specs with it reads past the end (and this is the test that exists to
        // catch exactly that divergence).
        check (playModeIndex >= 0 && (std::size_t) playModeIndex < specs.size(),
               "indexOfCC(17) is inside the UI view");
        if (playModeIndex >= 0 && (std::size_t) playModeIndex < specs.size())
        {
            const auto& s = specs[(std::size_t) playModeIndex];
            check (s.labels.size() == 6, "play mode has 6 labels");
            if (s.labels.size() == 6)
            {
                check (s.labels[0] == "poly" || s.labels[0] == "POLY", "play mode label 0 is poly");
                check (s.labels[5] == "MONO" || s.labels[5] == "mono", "play mode label 5 is mono");
            }
        }
    }

    // 9. The three CC numbers CcSpec hardcodes still name the parameters the UI
    //    assumes (the constants exist only so the engine path can name them).
    {
        const auto* ms = findCC (ccui::kModSourceSelectorCc);
        check (ms != nullptr && std::string (ms->name) == "ms_source",
               "kModSourceSelectorCc names ms_source");
        const auto* sus = findCC (ccui::kSustainCc);
        check (sus != nullptr && std::string (sus->name) == "sustain_pedal",
               "kSustainCc names sustain_pedal");
        const auto* mw = findCC (ccui::kModWheelCc);
        check (mw != nullptr && std::string (mw->name) == "mod_wheel",
               "kModWheelCc names mod_wheel");
    }

    if (failures == 0)
        std::printf ("ccmap: all checks passed (%d parameters)\n", kCCCount);
    else
        std::printf ("ccmap: %d FAILURES\n", failures);

    return failures == 0 ? 0 : 1;
}
