// tests/ui_modedit_test.cpp
//
// Mod edit sends depth CCs the player never sees: move LPF CUT with VELO selected
// and the plug-in sends CC 30 = 2, then CC 42. A wrong pair in depthCcFor would
// silently modulate the wrong parameter, so the pairing is checked two
// independent ways: coverage (every depth CC belongs to exactly one panel
// control) and naming (the CC map's own names agree on each pair).

#include "midi/CCMap.h"
#include "plugin/ui/ModEdit.h"

#include <cstdio>
#include <map>
#include <string>

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

bool startsWith (const std::string& s, const std::string& p) { return s.rfind (p, 0) == 0; }

std::string stripDepth (std::string s)
{
    const std::string suffix = "_depth";
    if (s.size() > suffix.size() && s.compare (s.size() - suffix.size(), suffix.size(), suffix) == 0)
        s.resize (s.size() - suffix.size());
    return s;
}
}

int main()
{
    using namespace nymphes;
    using nymphes::ui::depthCcFor;

    // 1. Coverage: each shared depth CC (ms_* except the selector, CC 30) is the
    //    twin of exactly one base control.
    std::map<int, int> claimedBy;   // depth cc -> base cc
    for (int base = 0; base < 128; ++base)
    {
        const int depth = depthCcFor (base);
        if (depth < 0)
            continue;
        check (claimedBy.count (depth) == 0,
               "depth CC " + std::to_string (depth) + " claimed twice");
        claimedBy[depth] = base;
    }

    int sharedDepthCount = 0;
    for (const auto& p : kCCMap)
    {
        const std::string name = p.name;
        if (! startsWith (name, "ms_") || p.cc == 30)
            continue;
        ++sharedDepthCount;
        check (claimedBy.count (p.cc) == 1,
               "depth CC " + std::to_string (p.cc) + " (" + name + ") has no panel control");
    }
    check (sharedDepthCount == 36, "36 shared depth CCs, got " + std::to_string (sharedDepthCount));
    check ((int) claimedBy.size() == 36, "36 pairs, got " + std::to_string (claimedBy.size()));

    // 2. Naming: "ms_<x>_depth" is the depth of "<x>" (or of "<x>_depth"). The map
    //    names one pair differently; it is listed, not pattern-matched away.
    const std::map<int, std::string> aliases { { 8, "lpf_cutoff_lfo" } };   // lfo_cutoff_depth
    for (const auto& [depth, base] : claimedBy)
    {
        const auto* d = findCC (depth);
        const auto* b = findCC (base);
        check (d != nullptr && b != nullptr,
               "pair " + std::to_string (base) + " -> " + std::to_string (depth) + " exists in the map");
        if (d == nullptr || b == nullptr)
            continue;

        check (! b->isDiscrete(), std::string ("base ") + b->name + " is continuous (it is a fader)");
        check (! startsWith (b->name, "ms_"), std::string ("base ") + b->name + " is not itself a depth");

        const std::string depthKey = stripDepth (std::string (d->name).substr (3));
        const std::string baseKey = aliases.count (base) ? aliases.at (base) : stripDepth (b->name);
        check (depthKey == baseKey,
               std::string ("names agree: ") + b->name + " (CC " + std::to_string (base) + ") <-> "
               + d->name + " (CC " + std::to_string (depth) + ")");
    }

    // 3. Controls without a depth twin stay base controls in mod edit.
    for (int cc : { 1, 7, 17, 22, 23, 28, 29, 30, 64, 68 })
        check (depthCcFor (cc) == -1, "CC " + std::to_string (cc) + " has no depth twin");

    // 4. The state: unknown until seen, per source, and bounds-safe.
    ui::ModEditState state;
    check (! state.active(), "starts in base mode");
    check (state.depth (2, 42) == -1, "a depth never seen is unknown");
    state.remember (2, 42, 90);
    check (state.depth (2, 42) == 90, "a remembered depth reads back");
    check (state.depth (1, 42) == -1, "depths are per source");
    state.remember (7, 42, 1);
    state.remember (0, 300, 1);
    check (state.depth (7, 42) == -1 && state.depth (0, 300) == -1, "out-of-range is ignored");

    if (failures == 0)
        std::printf ("ui_modedit_test: all checks passed (36 depth pairs)\n");
    return failures == 0 ? 0 : 1;
}
