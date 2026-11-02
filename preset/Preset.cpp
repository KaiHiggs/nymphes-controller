// Facade implementation. No codec logic lives here: every function below is a
// delegation to the vendored canonical codec in core/ (owner: L4, adopted by
// votes 4a6410d80fc5 / f066755d8590). What this file adds is vocabulary for the
// editor and the CLI: named groups, named parameters, slot formatting, and a
// one-line summary.
//
// The one non-delegating decision here is that this file never manufactures a
// FACTORY write. setFloat/setInt touch the payload only; the write flag is applied by
// encodeForDevice(), which does NOT forward core's `allowFactoryWrite` escape hatch:
// it passes false unconditionally, so FACTORY stays unwritable from every caller of
// this facade (the codec's own tests still exercise the hatch on the codec directly).

#include "Preset.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace nymphes
{

// ---------------------------------------------------------------------------
// Slot
// ---------------------------------------------------------------------------

std::string Slot::toString() const
{
    if (! isValid()) return "??";
    return std::string (1, bankLetter()) + std::to_string (static_cast<int> (program));
}

bool Slot::fromString (const std::string& s, Slot& out)
{
    size_t i = 0;
    while (i < s.size() && std::isspace (static_cast<unsigned char> (s[i]))) ++i;
    if (i >= s.size()) return false;

    const char c = static_cast<char> (std::toupper (static_cast<unsigned char> (s[i])));
    if (c < 'A' || c > 'G') return false;
    ++i;
    while (i < s.size() && std::isspace (static_cast<unsigned char> (s[i]))) ++i;
    if (i >= s.size() || s[i] < '1' || s[i] > '7') return false;
    const int program = s[i] - '0';
    ++i;
    while (i < s.size() && std::isspace (static_cast<unsigned char> (s[i]))) ++i;
    if (i != s.size()) return false;

    out.bank    = static_cast<uint8_t> (c - 'A' + 1);
    out.program = static_cast<uint8_t> (program);
    return true;
}

Slot Slot::fromAddress (const Address& a)
{
    return Slot { a.bank, a.program };
}

Address Slot::toAddress (Page page) const
{
    Address a;
    a.page    = (page == Page::Factory) ? kPageFactory : kPageUser;
    a.bank    = bank;
    a.program = program;
    return a;
}

std::string displayName (const Preset& p)
{
    const auto& a = p.address();
    return std::string (a.isUser() ? "USER " : "FACTORY ") + Slot::fromAddress (a).toString();
}

// ---------------------------------------------------------------------------
// Frame I/O (delegating)
// ---------------------------------------------------------------------------

bool decodeFrame (const uint8_t* data, size_t size, Preset& out, std::string* error)
{
    return Preset::decodeFrame (data, size, out, error);
}

std::vector<uint8_t> encodeFrame (const Preset& p)
{
    return p.encodeFrameRaw();
}

std::vector<uint8_t> encodeForDevice (const Preset& p, bool persist,
                                      bool allowFactoryWrite, std::string* error)
{
    // The shipping contract: a preset write goes to USER slots only. A FACTORY slot is
    // refused by core when persist is set, and the escape hatch core offers is not
    // exposed here - `allowFactoryWrite` is accepted for source compatibility only.
    (void) allowFactoryWrite;
    auto frame = p.encodeFrame (persist, /*allowFactoryWrite*/ false, error);
    if (! frame.empty() && error != nullptr) error->clear();   // no stale text on success
    return frame;
}

std::vector<Preset> importSyxFile (const std::string& path, std::string& error)
{
    std::vector<Preset> out;
    std::string err;
    if (! Preset::readSyx (path, out, &err))
    {
        // Never hand back a partially decoded file: a non-empty result must mean "the
        // whole file parsed", or a caller that logs the error and loads anyway gets a
        // truncated bank that looks valid.
        out.clear();
        error = err.empty() ? ("cannot read " + path) : err;
        return out;
    }
    error.clear();                     // no stale failure on a successful read
    return out;
}

bool exportSyxFile (const std::string& path, const std::vector<Preset>& presets, std::string& error)
{
    std::string err;
    if (! Preset::writeSyx (path, presets, &err))
    {
        error = err.empty() ? ("cannot write " + path) : err;
        return false;
    }
    error.clear();                     // no stale failure on a successful write
    return true;
}

std::vector<std::vector<uint8_t>> splitSyx (const std::vector<uint8_t>& blob)
{
    return Preset::splitSyx (blob);
}

// ---------------------------------------------------------------------------
// Section accessors
// ---------------------------------------------------------------------------

namespace
{

bool isBasicGroup (Group g) noexcept
{
    return g == Group::Main || g == Group::Lfo2 || g == Group::ModWheel
        || g == Group::Velocity || g == Group::Aftertouch;
}

bool isExtraGroup (Group g) noexcept
{
    return g == Group::ExtraLfo2 || g == Group::ExtraModWheel
        || g == Group::ExtraVelocity || g == Group::ExtraAftertouch;
}

uint32_t topFor (Group g) noexcept { return static_cast<uint32_t> (static_cast<int> (g)); }

int chordIndexFor (Group g) noexcept
{
    if (g >= Group::Chord1 && g <= Group::Chord8)
        return static_cast<int> (g) - static_cast<int> (Group::Chord1) + 1;
    return 0;
}

const char* const kReverbNames[4] = { "size", "decay", "filter", "mix" };
const char* const kExtraLfoNames[8] = { "lfo_1_rate", "lfo_1_wave", "lfo_1_delay", "lfo_1_fade",
                                        "lfo_2_rate", "lfo_2_wave", "lfo_2_delay", "lfo_2_fade" };
// The extra_* containers have more slots than the facade has names for, and the schema
// itself does not name the trailing ones: core's own text dump calls them f<n>
// (core/src/Codec.cpp toText). Same convention here, so a name means the same thing in
// both view layers instead of borrowing the LFO names for mod-wheel/velocity/aftertouch.
const char* const kExtraSlotNames[12] = { "f1", "f2", "f3", "f4", "f5", "f6",
                                          "f7", "f8", "f9", "f10", "f11", "f12" };
// Slots per extra_* container: extra_lfo_2 carries 12, the other three carry 8
// (docs/BUILD-REPORT.md: their trailing slots are optional and absent in some presets).
constexpr int extraSlotCount (uint32_t top) noexcept { return top == kExtraLfo2 ? 12 : 8; }
const char* const kLfoSettingNames[4] = { "lfo_1_speed_mode", "lfo_1_sync_mode",
                                          "lfo_2_speed_mode", "lfo_2_sync_mode" };
const char* const kChordNames[6] = { "root", "semi_1", "semi_2", "semi_3", "semi_4", "semi_5" };

// basic_inputs entries that are enumerated rather than continuous.
bool basicIsDiscrete (uint32_t slot) noexcept
{
    return slot == kWave || slot == kLfoWave || slot == kChordAmount;
}

} // namespace

bool getFloat (const Preset& p, Group g, int subField, float& out)
{
    if (subField < 1) return false;

    if (isBasicGroup (g))
    {
        const auto top = topFor (g);
        if (! p.hasSection (top) || subField > static_cast<int> (kBasicCount)) return false;
        out = p.basic (top, static_cast<uint32_t> (subField));
        return true;
    }
    if (g == Group::Reverb)
    {
        if (! p.hasSection (kReverb) || subField > 4) return false;
        out = p.reverb (static_cast<uint32_t> (subField));
        return true;
    }
    if (isExtraGroup (g))
    {
        const auto top = topFor (g);
        if (! p.hasSection (top) || subField > 12) return false;
        out = p.extra (top, static_cast<uint32_t> (subField));
        return true;
    }
    if (g == Group::AmpLevel)
    {
        if (subField != 1) return false;
        // amp_level is an optional top-level field: absent is not zero.
        if (! p.hasSection (kAmpLevel)) return false;
        out = p.ampLevel();
        return true;
    }
    return false;
}

bool setFloat (Preset& p, Group g, int subField, float value)
{
    if (subField < 1) return false;

    if (isBasicGroup (g))
    {
        const auto top = topFor (g);
        if (subField > static_cast<int> (kBasicCount)) return false;
        return p.setBasic (top, static_cast<uint32_t> (subField), value);
    }
    if (g == Group::Reverb)
        return subField <= 4 && p.setReverb (static_cast<uint32_t> (subField), value);
    if (isExtraGroup (g))
        return subField <= 12 && p.setExtra (topFor (g), static_cast<uint32_t> (subField), value);
    if (g == Group::AmpLevel)
        return subField == 1 && p.setAmpLevel (value);
    return false;
}

bool getInt (const Preset& p, Group g, int subField, int64_t& out)
{
    if (subField < 1) return false;

    // Every branch below repeats the section check the float getters do: an absent
    // section must not be reported as a real zero ("absent" and "zero" are different
    // presets on this device).
    if (g == Group::LfoSettings)
    {
        if (! p.hasSection (kLfoSettings) || subField > 4) return false;
        out = static_cast<int64_t> (p.lfoSetting (static_cast<uint32_t> (subField)));
        return true;
    }
    if (g == Group::Legato)
    {
        if (! p.hasSection (kLegato) || subField != 1) return false;
        out = p.legato() ? 1 : 0;
        return true;
    }
    if (g == Group::VoiceMode)
    {
        if (! p.hasSection (kVoiceMode) || subField != 1) return false;
        out = static_cast<int64_t> (p.voiceMode());
        return true;
    }
    if (const int chord = chordIndexFor (g); chord != 0)
    {
        if (! p.hasSection (static_cast<uint32_t> (kChord1 + chord - 1)) || subField > 6) return false;
        out = static_cast<int64_t> (p.chordInfo (static_cast<uint32_t> (chord), static_cast<uint32_t> (subField)));
        return true;
    }
    return false;
}

bool setInt (Preset& p, Group g, int subField, int64_t value)
{
    if (subField < 1) return false;

    // Chords genuinely go negative: the device sends chord_info as an int32 varint and
    // docs/BUILD-REPORT.md records negative values in the live corpus, which getInt()
    // already decodes as negative. Rejecting them here made a read/modify/write
    // round-trip lossy. Only the unsigned fields keep the blanket rejection; the
    // two's-complement reinterpretation is what core stores (setChordInfo takes
    // uint64_t and writes the raw varint), so re-encoding is byte-identical.
    const int chord = chordIndexFor (g);
    if (value < 0)
    {
        if (chord == 0) return false;
        return subField <= 6 && p.setChordInfo (static_cast<uint32_t> (chord),
                                                static_cast<uint32_t> (subField),
                                                static_cast<uint64_t> (value));
    }

    if (g == Group::LfoSettings)
        return subField <= 4 && p.setLfoSetting (static_cast<uint32_t> (subField), static_cast<uint64_t> (value));
    if (g == Group::Legato)
        return subField == 1 && p.setLegato (value != 0);
    if (g == Group::VoiceMode)
        return subField == 1 && value <= 5 && p.setVoiceMode (static_cast<uint64_t> (value));
    if (chord != 0)
        return subField <= 6 && p.setChordInfo (static_cast<uint32_t> (chord),
                                                static_cast<uint32_t> (subField),
                                                static_cast<uint64_t> (value));
    return false;
}

const char* voiceModeName (uint64_t mode)
{
    static const char* const names[6] = { "poly", "uni-6", "uni-4", "uni-3", "uni-2", "mono" };
    return mode < 6 ? names[mode] : "?";
}

std::vector<FieldView> describe (const Preset& p)
{
    std::vector<FieldView> out;
    out.reserve (96);

    auto addBasic = [&] (Group g)
    {
        const auto top = topFor (g);
        if (! p.hasSection (top)) return;             // absent section stays absent
        for (uint32_t slot = 1; slot <= kBasicCount; ++slot)
        {
            const char* name = basicName (slot);
            if (name == nullptr) continue;
            out.push_back ({ g, static_cast<int> (slot), name, p.basic (top, slot),
                             basicIsDiscrete (slot) });
        }
    };

    addBasic (Group::Main);
    addBasic (Group::Lfo2);
    addBasic (Group::ModWheel);
    addBasic (Group::Velocity);
    addBasic (Group::Aftertouch);

    if (p.hasSection (kReverb))
        for (int i = 1; i <= 4; ++i)
            out.push_back ({ Group::Reverb, i, kReverbNames[i - 1], p.reverb (static_cast<uint32_t> (i)), false });

    if (p.hasSection (kExtraLfo2))
        for (int i = 1; i <= extraSlotCount (kExtraLfo2); ++i)
            out.push_back ({ Group::ExtraLfo2, i,
                             i <= 8 ? kExtraLfoNames[i - 1] : kExtraSlotNames[i - 1],
                             p.extra (kExtraLfo2, static_cast<uint32_t> (i)), false });

    for (auto g : { Group::ExtraModWheel, Group::ExtraVelocity, Group::ExtraAftertouch })
    {
        const auto top = topFor (g);
        if (! p.hasSection (top)) continue;
        for (int i = 1; i <= extraSlotCount (top); ++i)
            out.push_back ({ g, i, kExtraSlotNames[i - 1],
                             p.extra (top, static_cast<uint32_t> (i)), false });
    }

    // amp_level is an optional top-level field in live payloads: list it only when the
    // preset carries it, never as a phantom zero.
    if (p.hasSection (kAmpLevel))
        out.push_back ({ Group::AmpLevel, 1, "amp_level", p.ampLevel(), false });

    if (p.hasSection (kLfoSettings))
        for (int i = 1; i <= 4; ++i)
            out.push_back ({ Group::LfoSettings, i, kLfoSettingNames[i - 1],
                             static_cast<float> (p.lfoSetting (static_cast<uint32_t> (i))), true });

    if (p.hasSection (kLegato))
        out.push_back ({ Group::Legato, 1, "legato", p.legato() ? 1.0f : 0.0f, true });

    if (p.hasSection (kVoiceMode))
        out.push_back ({ Group::VoiceMode, 1, "voice_mode", static_cast<float> (p.voiceMode()), true });

    // Chords are only listed when the preset actually carries that chord.
    for (int c = 1; c <= 8; ++c)
    {
        const auto top = static_cast<uint32_t> (kChord1 + c - 1);
        if (! p.hasSection (top)) continue;
        for (int s = 1; s <= 6; ++s)
            out.push_back ({ static_cast<Group> (static_cast<int> (Group::Chord1) + c - 1), s, kChordNames[s - 1],
                             static_cast<float> (p.chordInfo (static_cast<uint32_t> (c), static_cast<uint32_t> (s))),
                             true });
    }

    return out;
}

std::string summaryLine (const Preset& p)
{
    // An absent section is not zero: say so instead of printing 0.000, which reads like
    // a real value in the browser list and in `nymphes-cli show`.
    auto field = [&p] (Group g, int slot)
    {
        float v = 0.0f;
        if (! getFloat (p, g, slot, v)) return std::string ("--");
        char b[24];
        std::snprintf (b, sizeof b, "%.3f", double (v));
        return std::string (b);
    };

    const std::string wave = field (Group::Main, static_cast<int> (kWave));
    const std::string lvl  = field (Group::Main, static_cast<int> (kLvl));
    const std::string cut  = field (Group::Main, static_cast<int> (kCut));
    const std::string res  = field (Group::Main, static_cast<int> (kReson));

    char buf[224];
    std::snprintf (buf, sizeof buf,
                   "%-12s wave %s  lvl %s  cut %s  res %s  %s  payload %zu B  crc %s",
                   displayName (p).c_str(), wave.c_str(), lvl.c_str(), cut.c_str(), res.c_str(),
                   voiceModeName (p.voiceMode()), p.payloadSize(),
                   p.crcValid() ? "ok" : "BAD");
    return buf;
}

} // namespace nymphes
