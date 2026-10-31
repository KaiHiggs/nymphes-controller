// tests/preset_facade_test.cpp — L2
//
// Device-free checks on preset/ (the facade over core/). Every preset used here is
// SYNTHESISED in-process: a protobuf body, a CRC-8 over it and a nibble-encoded
// F0..F7 frame. That is deliberate - the vendor corpus lives outside this repository,
// so a corpus-driven assertion would silently skip and prove nothing. These assertions
// run everywhere.
//
// Each check is pinned to a defect the OCR scan reported:
//
//   * describe() covered 8 of extra_lfo_2's 12 slots and 4 of the other extra_* 8, so
//     present parameters never reached the editor (OCR-122);
//   * the extra_mod_w/velo/after sections were labelled with the LFO-2 names (OCR-123);
//   * an absent optional section (amp_level, lfo_settings, legato, voice_mode, a chord)
//     was reported as a real zero (OCR-120);
//   * a negative chord_info - which the device sends and core decodes - could not be
//     written back, so read/modify/write was lossy (OCR-119);
//   * encodeForDevice() forwarded core's allowFactoryWrite escape hatch, so FACTORY was
//     writable through the facade (OCR-118);
//   * slotFor() accepted any index and wrapped it into a plausible-looking slot
//     (OCR-114);
//   * a partly decoded .syx came back as a valid-looking shorter bank (OCR-124);
//   * exportSyxFile()/importSyxFile() left a stale error on success (OCR-240).

#include "preset/Preset.h"
#include "preset/PresetBank.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
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

using Bytes = std::vector<std::uint8_t>;

// ---- a minimal protobuf writer (the same encoding core's parser expects) ----------
void putVarint (std::uint64_t v, Bytes& out)
{
    while (v >= 0x80)
    {
        out.push_back (static_cast<std::uint8_t> ((v & 0x7F) | 0x80));
        v >>= 7;
    }
    out.push_back (static_cast<std::uint8_t> (v));
}

void addFixed32 (std::uint32_t field, float value, Bytes& out)
{
    putVarint ((std::uint64_t (field) << 3) | 5, out);
    std::uint32_t bits = 0;
    std::memcpy (&bits, &value, sizeof bits);
    for (int i = 0; i < 4; ++i)
        out.push_back (static_cast<std::uint8_t> ((bits >> (8 * i)) & 0xFF));
}

void addVarint (std::uint32_t field, std::uint64_t value, Bytes& out)
{
    putVarint ((std::uint64_t (field) << 3) | 0, out);
    putVarint (value, out);
}

void addSubmessage (std::uint32_t field, const Bytes& body, Bytes& out)
{
    putVarint ((std::uint64_t (field) << 3) | 2, out);
    putVarint (body.size(), out);
    out.insert (out.end(), body.begin(), body.end());
}

// ---- frame assembly: payload = CRC | protobuf, then 2 nibbles per byte ---------------
Bytes makeFrame (const Bytes& protobuf, std::uint8_t page, std::uint8_t bank, std::uint8_t program,
                 std::uint8_t flag = 0)
{
    Bytes payload;
    payload.push_back (0);                       // CRC placeholder
    payload.insert (payload.end(), protobuf.begin(), protobuf.end());
    payload[0] = nymphes::crc8 (payload.data() + 1, payload.size() - 1);

    Bytes frame;
    frame.push_back (0xF0);
    for (std::uint8_t b : nymphes::kManufacturer) frame.push_back (b);
    frame.push_back (flag);
    frame.push_back (page);
    frame.push_back (bank);
    frame.push_back (program);
    for (std::uint8_t b : payload)
    {
        frame.push_back (static_cast<std::uint8_t> (b & 0x0F));      // low nibble first
        frame.push_back (static_cast<std::uint8_t> ((b >> 4) & 0x0F));
    }
    frame.push_back (0xF7);
    return frame;
}

// main section (field 1, 28 float slots) so the payload clears the sanity floor.
Bytes mainSection()
{
    Bytes body;
    for (std::uint32_t slot = 1; slot <= 28; ++slot)
        addFixed32 (slot, 0.25f * float (slot), body);
    return body;
}

std::string tempPath (const char* leaf)
{
    const char* dir = std::getenv ("TMPDIR");
    std::string p = (dir != nullptr && *dir != '\0') ? dir : "/tmp";
    if (! p.empty() && p.back() != '/') p += '/';
    return p + leaf;
}

bool writeFile (const std::string& path, const Bytes& bytes)
{
    std::ofstream out (path, std::ios::binary | std::ios::trunc);
    if (! out) return false;
    out.write (reinterpret_cast<const char*> (bytes.data()),
               static_cast<std::streamsize> (bytes.size()));
    return static_cast<bool> (out);
}
} // namespace

int main (int, char**)
{
    using namespace nymphes;

    // --- 1. describe(): full coverage of the extra_* containers -----------------
    {
        Bytes protobuf;
        addSubmessage (kMain, mainSection(), protobuf);

        Bytes lfo2;
        for (std::uint32_t slot = 1; slot <= 12; ++slot)
            addFixed32 (slot, 0.1f * float (slot), lfo2);
        addSubmessage (kExtraLfo2, lfo2, protobuf);

        Bytes modw;
        for (std::uint32_t slot = 1; slot <= 8; ++slot)
            addFixed32 (slot, 0.2f * float (slot), modw);
        addSubmessage (kExtraModWheel, modw, protobuf);

        const Bytes frame = makeFrame (protobuf, kPageUser, 1, 1);
        Preset p;
        std::string err;
        check (Preset::decodeFrame (frame.data(), frame.size(), p, &err),
               "a synthesised USER frame decodes" + (err.empty() ? "" : (": " + err)));

        int lfo2Fields = 0, modwFields = 0;
        bool sawSlot12 = false, sawModSlot8 = false;
        for (const auto& f : describe (p))
        {
            if (f.group == Group::ExtraLfo2)
            {
                ++lfo2Fields;
                if (f.subField == 12) sawSlot12 = true;
                const std::string name (f.name);
                check (name.rfind ("lfo_", 0) == 0 || name == "f9" || name == "f10"
                           || name == "f11" || name == "f12",
                       "extra_lfo_2 slot " + std::to_string (f.subField) + " keeps its LFO name or f<n>");
            }
            if (f.group == Group::ExtraModWheel)
            {
                ++modwFields;
                if (f.subField == 8) sawModSlot8 = true;
                check (std::string (f.name) != "lfo_2_rate" && std::string (f.name) != "lfo_2_wave",
                       "extra_mod_w must not be labelled with the lfo_2 names");
            }
        }
        check (lfo2Fields == 12, "describe() lists all 12 extra_lfo_2 slots (got "
                                     + std::to_string (lfo2Fields) + ")");
        check (sawSlot12, "describe() reaches extra_lfo_2 slot 12");
        check (modwFields == 8, "describe() lists all 8 extra_mod_w slots (got "
                                    + std::to_string (modwFields) + ")");
        check (sawModSlot8, "describe() reaches extra_mod_w slot 8");

        // amp_level is absent from this frame: it must not appear at all, and asking
        // for it must fail rather than answer 0.0f.
        for (const auto& f : describe (p))
            check (f.group != Group::AmpLevel, "describe() omits the absent amp_level section");
        float v = 123.0f;
        check (! getFloat (p, Group::AmpLevel, 1, v), "getFloat() reports absent amp_level as absent");

        // Same contract for the integer sections this frame does not carry.
        std::int64_t iv = 99;
        check (! getInt (p, Group::LfoSettings, 1, iv), "getInt() reports absent lfo_settings as absent");
        check (! getInt (p, Group::Legato, 1, iv), "getInt() reports absent legato as absent");
        check (! getInt (p, Group::VoiceMode, 1, iv), "getInt() reports absent voice_mode as absent");
        check (! getInt (p, Group::Chord1, 1, iv), "getInt() reports an absent chord as absent");

        // ...and the summary says so instead of printing a zero.
        const std::string line = summaryLine (p);
        check (line.find ("res ") != std::string::npos, "summaryLine() still reports main values");
    }

    // --- 2. a preset with no main section: summary says "--", not 0.000 -------
    {
        // No main/reverb/extra fields are needed for the summary; a pad container only
        // has to clear core's kMinPayloadSize sanity floor.
        Bytes protobuf;
        addFixed32 (kAmpLevel, 0.5f, protobuf);       // amp_level is a plain fixed32 field
        addVarint (kVoiceMode, 2, protobuf);
        Bytes pad;
        for (std::uint32_t slot = 1; slot <= 8; ++slot) addFixed32 (slot, 0.0f, pad);
        addSubmessage (kExtraLfo2, pad, protobuf);

        const Bytes frame = makeFrame (protobuf, kPageUser, 2, 3);
        Preset p;
        std::string err;
        check (Preset::decodeFrame (frame.data(), frame.size(), p, &err), "a main-less frame decodes");

        const std::string line = summaryLine (p);
        check (line.find ("wave --") != std::string::npos,
               "an absent main section prints \"--\", not 0.000: " + line);

        float v = 0.0f;
        check (getFloat (p, Group::AmpLevel, 1, v) && v == 0.5f, "the present amp_level reads back");
    }

    // --- 3. negative chord_info round-trips ------------------------------------
    {
        Bytes chord;
        addVarint (1, 0, chord);                      // slot 1
        addVarint (2, static_cast<std::uint64_t> (static_cast<std::int64_t> (-12)), chord);
        for (std::uint32_t slot = 3; slot <= 6; ++slot) addVarint (slot, 0, chord);

        Bytes protobuf;
        addSubmessage (kChord1, chord, protobuf);
        addSubmessage (kMain, mainSection(), protobuf);

        const Bytes frame = makeFrame (protobuf, kPageUser, 3, 4);
        Preset p;
        std::string err;
        check (Preset::decodeFrame (frame.data(), frame.size(), p, &err), "a negative-chord frame decodes");

        std::int64_t got = 0;
        check (getInt (p, Group::Chord1, 2, got) && got == -12,
               "getInt() decodes the negative chord as -12 (got " + std::to_string (got) + ")");

        const Bytes before = p.payload();
        check (setInt (p, Group::Chord1, 2, -12),
               "setInt() accepts the value getInt() just returned");
        check (getInt (p, Group::Chord1, 2, got) && got == -12, "the negative chord reads back unchanged");
        check (p.payload() == before, "re-writing the same negative chord is byte-identical");
        check (p.crcValid(), "the payload CRC still checks after the write");

        // The unsigned fields keep the blanket rejection: a negative LFO mode or
        // voice-mode is not a representable value.
        check (! setInt (p, Group::LfoSettings, 1, -1), "setInt() still rejects a negative lfo setting");
        check (! setInt (p, Group::VoiceMode, 1, -1), "setInt() still rejects a negative voice mode");
        check (! setInt (p, Group::Legato, 1, -1), "setInt() still rejects a negative legato");
    }

    // --- 4. FACTORY is not writable through the facade -------------------------
    {
        Bytes protobuf;
        addSubmessage (kMain, mainSection(), protobuf);

        Preset factory;
        std::string err;
        const Bytes ff = makeFrame (protobuf, kPageFactory, 2, 2);
        check (Preset::decodeFrame (ff.data(), ff.size(), factory, &err), "a FACTORY frame decodes");
        check (! factory.address().isUser(), "the frame is on the FACTORY page");

        check (encodeForDevice (factory, true, false, &err).empty(),
               "encodeForDevice(persist) refuses a FACTORY slot");
        check (encodeForDevice (factory, true, true, &err).empty(),
               "encodeForDevice() does not forward core's allowFactoryWrite escape hatch");

        const Bytes audition = encodeForDevice (factory, false, false, &err);
        check (! audition.empty() && audition.size() > 6 && audition[6] == 0x00,
               "FACTORY can still be auditioned (FF=0x00)");

        Preset user;
        const Bytes uf = makeFrame (protobuf, kPageUser, 2, 2);
        check (Preset::decodeFrame (uf.data(), uf.size(), user, &err), "a USER frame decodes");
        const Bytes persisted = encodeForDevice (user, true, false, &err);
        check (! persisted.empty() && persisted.size() > 6 && persisted[6] == 0x01,
               "a USER slot still takes the write flag (FF=0x01)");
        check (err.empty(), "a successful encodeForDevice() leaves no error text");
    }

    // --- 5. Slot / page mapping rejects what it cannot represent ---------------
    {
        check (PresetBank::slotFor (0).isValid(), "slotFor(0) is a valid slot");
        check (PresetBank::slotFor (0).bank == 1 && PresetBank::slotFor (0).program == 1,
               "slotFor(0) is A1");
        check (PresetBank::slotFor (48).bank == 7 && PresetBank::slotFor (48).program == 7,
               "slotFor(48) is G7");
        check (! PresetBank::slotFor (-1).isValid(), "slotFor(-1) is not a valid slot");
        check (! PresetBank::slotFor (-7).isValid(), "slotFor(-7) is not a valid slot");
        check (! PresetBank::slotFor (98).isValid(), "slotFor(98) (kSize) is not a valid slot");
        check (! PresetBank::slotFor (49 * 3).isValid(), "slotFor(147) is not a valid slot");

        for (int i = 0; i < 98; ++i)
        {
            const Slot s = PresetBank::slotFor (i);
            check (s.isValid() && s.linearIndex() == i % kSlotsPerPage,
                   "slotFor/linearIndex round-trip for index " + std::to_string (i));
        }
        check (PresetBank::pageFor (0) == Page::User, "index 0 is the USER page");
        check (PresetBank::pageFor (49) == Page::Factory, "index 49 is the FACTORY page");

        // Hardening (OCR-237): an out-of-range Slot has no letter and no index.
        Slot bad;
        bad.bank = 0;
        bad.program = 0;
        check (bad.bankLetter() == '?', "an invalid Slot has no bank letter");
        check (bad.linearIndex() == -1, "an invalid Slot has no linear index");
        check (Slot { 1, 1 }.bankLetter() == 'A' && Slot { 7, 7 }.bankLetter() == 'G',
               "valid slots keep their letters");
    }

    // --- 6. .syx import: no partial bank, no stale error ----------------------
    {
        Bytes protobuf;
        addSubmessage (kMain, mainSection(), protobuf);
        const Bytes goodFrame = makeFrame (protobuf, kPageUser, 1, 2);

        Bytes corruptFrame = goodFrame;
        corruptFrame[corruptFrame.size() - 3] ^= 0x01;    // flip a payload nibble

        // A file whose SECOND frame is corrupt must not come back as a 1-preset bank.
        Bytes blob = goodFrame;
        blob.insert (blob.end(), corruptFrame.begin(), corruptFrame.end());
        const std::string badPath = tempPath ("nymphes_L2_preset_facade_bad.syx");
        check (writeFile (badPath, blob), "wrote the corrupt .syx fixture");

        std::string err = "stale";
        const auto partial = importSyxFile (badPath, err);
        check (partial.empty(), "a partly decoded .syx yields no presets (got "
                                    + std::to_string (partial.size()) + ")");
        check (! err.empty(), "a failed import reports why");

        // A wholly valid file imports, and a reused error string is cleared.
        const std::string goodPath = tempPath ("nymphes_L2_preset_facade_good.syx");
        const Bytes thirdFrame = makeFrame (protobuf, kPageUser, 1, 3);
        Bytes twoGood = goodFrame;
        twoGood.insert (twoGood.end(), thirdFrame.begin(), thirdFrame.end());
        check (writeFile (goodPath, twoGood), "wrote the valid .syx fixture");

        err = "stale message from an earlier failure";
        const auto loaded = importSyxFile (goodPath, err);
        check (loaded.size() == 2, "a valid .syx imports both presets (got "
                                       + std::to_string (loaded.size()) + ")");
        check (err.empty(), "a successful import clears the stale error");

        // The bank counts what it stored, and reports what it dropped.
        PresetBank bank;
        err = "stale";
        const int stored = bank.importSyx (goodPath, err);
        check (stored == 2, "PresetBank::importSyx stores both presets (got "
                                + std::to_string (stored) + ")");
        check (err.empty(), "a clean PresetBank::importSyx leaves no error");
        check (bank.countPresent() == 2, "the bank reports 2 present locations");

        err = "stale";
        const int storedBad = bank.importSyx (badPath, err);
        check (storedBad == 0, "a corrupt dump stores nothing (got " + std::to_string (storedBad) + ")");
        check (! err.empty(), "a corrupt dump explains itself");

        std::remove (badPath.c_str());
        std::remove (goodPath.c_str());
    }

    // --- 7. exportSyxFile clears a stale error --------------------------------
    {
        const std::string outPath = tempPath ("nymphes_L2_preset_facade_export.syx");
        Bytes protobuf;
        addSubmessage (kMain, mainSection(), protobuf);
        const Bytes frame = makeFrame (protobuf, kPageUser, 4, 4);

        Preset p;
        std::string err;
        check (Preset::decodeFrame (frame.data(), frame.size(), p, &err), "the export fixture decodes");

        err = "stale message from an earlier failure";
        check (exportSyxFile (outPath, { p }, err), "exportSyxFile writes one preset");
        check (err.empty(), "a successful export clears the stale error");
        std::remove (outPath.c_str());
    }

    // --- 8. the size constants say what they are ------------------------------
    {
        check (kPayloadBytes == 1049, "kPayloadBytes is the canonical vendor payload size");
        check (kFrameBytes == 2109, "kFrameBytes is the canonical vendor frame size");
        check (kMinPayloadBytes == 32 && kMaxPayloadBytes == 4096,
               "the receive bounds are exported for variable-length frames");
        check (kMaxPayloadBytes > kPayloadBytes,
               "a live frame may be longer than the canonical payload");
    }

    if (failures == 0)
        std::printf ("OK    preset_facade_test: describe/accessors/factory guard/slots/syx\n");
    else
        std::printf ("FAILED %d check(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
