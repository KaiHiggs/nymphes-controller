// plugin/ui/ModEdit.h  —  the hardware's modulation workflow, on screen
//
// On the Nymphes you pick a modulation source in the MENU (LFO 2, MOD W, VELO,
// AFTER) and the same faders then set that source's depth. The CC map mirrors
// this: every panel fader has exactly one depth twin, and CC 30 says which source
// the twin currently means. This file holds that pairing and what the UI knows
// about each source's depths. JUCE-free on purpose.
//
// What the UI knows is only what it sent, or saw arrive while that source was
// selected. A depth it has never seen is unknown (-1) and is drawn as unknown —
// the CC engine keeps one value per CC, which for a shared depth CC is "the last
// source anyone touched", and showing that under another source would be a lie.

#pragma once

#include <array>

namespace nymphes::ui
{

/** The depth CC that modulates `baseCc`, or -1 when the control has no depth. */
constexpr int depthCcFor (int baseCc) noexcept
{
    switch (baseCc)
    {
        case 70: return 31;  case  9: return 32;  case 10: return 33;  case 11: return 34;
        case 13: return 35;  case 12: return 36;  case  5: return 37;  case 15: return 39;
        case 16: return 40;  case 14: return 41;  case 74: return 42;  case 71: return 43;
        case  3: return 44;  case 81: return 45;  case  4: return 46;  case  8: return 47;
        case 79: return 48;  case 80: return 49;  case 82: return 50;  case 83: return 51;
        case 73: return 52;  case 84: return 53;  case 85: return 54;  case 72: return 55;
        case 18: return 56;  case 19: return 57;  case 20: return 58;  case 21: return 59;
        case 24: return 60;  case 25: return 61;  case 26: return 62;  case 27: return 63;
        case 75: return 86;  case 76: return 87;  case 77: return 88;  case 78: return 89;
        default: return -1;
    }
}

inline constexpr int kModSourceCount = 4;   // CC 30: LFO 2, MOD WHEEL, VELOCITY, AFTERTOUCH
inline constexpr const char* kModSourceShort[kModSourceCount] = { "LFO 2", "MOD W", "VELO", "AFTER" };
inline constexpr const char* kModSourceLong[kModSourceCount]  = { "LFO 2", "mod wheel", "velocity", "aftertouch" };

struct ModEditState
{
    /** -1 = the faders set the parameters; 0..3 = they set that source's depths. */
    int source = -1;

    bool active() const noexcept { return source >= 0; }

    int depth (int src, int depthCc) const noexcept
    {
        return inRange (src, depthCc) ? known[(std::size_t) src][(std::size_t) depthCc] : -1;
    }

    void remember (int src, int depthCc, int value) noexcept
    {
        if (inRange (src, depthCc))
            known[(std::size_t) src][(std::size_t) depthCc] = value;
    }

private:
    static bool inRange (int src, int cc) noexcept
    {
        return src >= 0 && src < kModSourceCount && cc >= 0 && cc < 128;
    }

    std::array<std::array<int, 128>, kModSourceCount> known = []
    {
        std::array<std::array<int, 128>, kModSourceCount> a {};
        for (auto& row : a) row.fill (-1);
        return a;
    }();
};

} // namespace nymphes::ui
