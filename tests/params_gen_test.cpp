// tests/params_gen_test.cpp — L3
//
// The checked-in CC table (cli/gen/nymphes_params_gen.h) has no consumer in this tree:
// no translation unit includes it, so before this test nothing compiled it and nothing
// could notice a table/size mismatch. Including it here compiles its static_asserts
// (kParamCount vs the table, the frame arithmetic, unique+sorted CCs), and the checks
// below pin the invariants the lookup helpers rely on.
//
// No device, no JUCE, no corpus.

#include "cli/gen/nymphes_params_gen.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace params = nymphes::params;

static int gFail = 0, gPass = 0;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (cond) {                                         \
            ++gPass;                                        \
        } else {                                            \
            ++gFail;                                        \
            std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                       \
            std::printf("\n");                             \
        }                                                   \
    } while (0)

int main() {
    // ---- 1. every entry is reachable through both lookups, and the lookups agree
    int missingById = 0, missingByCc = 0, depthWithoutTarget = 0, labelProblems = 0;
    int discreteCount = 0;
    for (const auto& p : params::kParams) {
        const auto* byCc = params::findCc(p.cc);
        if (byCc != &p) ++missingByCc;
        if (params::findId(p.id) != &p) ++missingById;

        // A depth entry names the CC it modulates: that CC must exist.
        if (p.modTargetCc >= 0 && params::findCc(p.modTargetCc) == nullptr) ++depthWithoutTarget;

        // The label arrays are NOT nullptr-terminated, so their length cannot be
        // measured through the pointer. What is checkable is that labelFor() resolves
        // every value the entry declares and nothing outside that range.
        if (p.discrete) {
            ++discreteCount;
            if (p.labels == nullptr || params::labelFor(p, p.min) == nullptr ||
                params::labelFor(p, p.max) == nullptr)
                ++labelProblems;
        } else if (p.labels != nullptr || params::labelFor(p, p.min) != nullptr) {
            ++labelProblems;
        }
    }
    CHECK(missingByCc == 0, "%d entries are not returned by findCc(their own cc)", missingByCc);
    CHECK(missingById == 0, "%d entries are not returned by findId(their own id)", missingById);
    CHECK(depthWithoutTarget == 0, "%d depth entries modulate a CC that is not in the table",
          depthWithoutTarget);
    CHECK(labelProblems == 0, "%d entries have labels inconsistent with their discrete flag/range",
          labelProblems);
    std::printf("params: %zu entries, %d discrete\n", params::kParamCount, discreteCount);

    // ---- 2. unknown CCs are not silently matched. CC 38 is the documented gap.
    CHECK(params::findCc(-1) == nullptr, "findCc(-1) matched something");
    CHECK(params::findCc(128) == nullptr, "findCc(128) matched something");
    CHECK(params::findCc(38) == nullptr, "CC 38 is absent on this device and must not match");
    CHECK(params::findId(nullptr) == nullptr, "findId(nullptr) matched something");
    CHECK(params::findId("") == nullptr, "findId(\"\") matched something");
    CHECK(params::findId("oscillator.wave") != nullptr, "findId(oscillator.wave) must resolve");

    // ---- 3. labelFor() is bounded by the declared range, not by the array length
    {
        const auto* p = params::findCc(17);   // misc.mode: discrete, 0..5, six labels
        CHECK(p != nullptr, "CC 17 must exist");
        CHECK(p != nullptr && p->discrete, "CC 17 must be discrete");
        if (p != nullptr && p->discrete) {
            CHECK(params::labelFor(*p, p->min) != nullptr, "labelFor(min) must resolve");
            CHECK(params::labelFor(*p, p->max) != nullptr, "labelFor(max) must resolve");
            CHECK(params::labelFor(*p, p->min - 1) == nullptr, "labelFor(below min) must be null");
            CHECK(params::labelFor(*p, p->max + 1) == nullptr, "labelFor(above max) must be null");
        }
        const auto* cont = params::findCc(1);   // mod wheel: continuous, no labels
        CHECK(cont != nullptr && params::labelFor(*cont, 0) == nullptr,
              "labelFor on a continuous parameter must be null");
    }

    // ---- 4. the basic_inputs order is the 28-slot payload order, each with a real CC
    {
        CHECK(params::kBasicInputCount == 28, "basic_inputs must hold 28 slots");
        int bad = 0;
        for (const auto& b : params::kBasicInputs)
            if (b.name == nullptr || params::findCc(b.cc) == nullptr) ++bad;
        CHECK(bad == 0, "%d basic_inputs entries have no name or an unknown CC", bad);
    }

    std::printf("\n%d checks passed, %d failed\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}
