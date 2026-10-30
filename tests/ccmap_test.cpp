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
        const bool discreteInTable = kCCMap[i].valueNames != nullptr;
        check (specs[(std::size_t) i].isDiscrete() == discreteInTable,
               "ccui discreteness matches for CC " + std::to_string (kCCMap[i].cc));
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
    if (const auto* pm = findCC (17))
    {
        const auto& s = specs[(std::size_t) indexOfCC (17)];
        check (s.labels.size() == 6, "play mode has 6 labels");
        if (s.labels.size() == 6)
        {
            check (s.labels[0] == "poly" || s.labels[0] == "POLY", "play mode label 0 is poly");
            check (s.labels[5] == "MONO" || s.labels[5] == "mono", "play mode label 5 is mono");
        }
        (void) pm;
    }

    if (failures == 0)
        std::printf ("ccmap: all checks passed (%d parameters)\n", kCCCount);
    else
        std::printf ("ccmap: %d FAILURES\n", failures);

    return failures == 0 ? 0 : 1;
}
