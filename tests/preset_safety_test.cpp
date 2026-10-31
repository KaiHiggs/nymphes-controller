// tests/preset_safety_test.cpp — L1
//
// Device-free checks on the rules that must never regress, asserted against the
// SHIPPING codec (core/ via preset/) on the REAL corpora, not on synthetic frames:
//
//   * every frame of the live device dump decodes, CRC-valid, at its own length
//   * decode -> encodeFrameRaw is byte-identical for all 98 of them
//   * frameSize() is derived from the payload (11 + 2*n), never a constant 2109
//   * a FACTORY address can never be encoded with the write flag
//   * audition is always flag 0x00
//
// The corpora live in the shared gate root, not inside this tree, so the gate root
// is passed in:  preset_safety_test <gate-root>
// and CMake registers the ctest entry with it.

#include "preset/Preset.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#ifndef NYMPHES_GATE_DIR
#define NYMPHES_GATE_DIR ""
#endif

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

// Returns false when the file cannot be opened or read. An empty file is a
// success with an empty image — the caller distinguishes "wrong path / permission"
// from "intentionally empty" instead of treating both as "nothing to check".
bool readFile (const std::string& path, std::vector<std::uint8_t>& out)
{
    out.clear();
    std::ifstream in (path, std::ios::binary);
    if (! in)
        return false;
    out.assign (std::istreambuf_iterator<char> (in), {});
    return ! in.bad();
}
}

int main (int argc, char** argv)
{
    using namespace nymphes;

    const std::string gate = (argc > 1) ? argv[1] : std::string (NYMPHES_GATE_DIR);
    if (gate.empty())
    {
        std::printf ("SKIP  no gate root given (pass it as argv[1] or set NYMPHES_GATE_DIR)\n");
        return 0;
    }

    // From here on the corpora are CONFIGURED. A missing or unreadable file is a
    // failure, not a skip: the old SKIP branches let a CI machine without the
    // corpora report a green ctest run in which not one safety rule was exercised.
    const std::string live = gate + "/evidence/L4-device/device_dump_run1.syx";
    const std::string vendorFactory = gate + "/vendor/Nymphes_Factory_Presets_Factory_Bank.syx";
    const std::string vendorUser    = gate + "/vendor/Nymphes_Factory_Presets_User_Bank.syx";

    // --- 1. the live dump: real frames, 8 real lengths ------------------------
    std::vector<std::uint8_t> liveImage;
    const bool liveOk = readFile (live, liveImage);
    check (liveOk, "configured live corpus is readable at " + live);
    check (! liveOk || ! liveImage.empty(), "configured live corpus is not empty at " + live);
    if (! liveOk || liveImage.empty())
    {
        // Continue: the factory/user blocks still run, and their failures are
        // reported independently.
    }
    else
    {
        const auto frames = splitSyx (liveImage);
        check (frames.size() == 98,
               "live dump splits into 98 frames, got " + std::to_string (frames.size()));

        std::map<std::size_t, int> sizeHistogram;
        int decoded = 0, crcOk = 0, byteIdentical = 0, sizeDerived = 0;

        for (const auto& frame : frames)
        {
            Preset p;
            std::string err;
            if (! decodeFrame (frame.data(), frame.size(), p, &err))
                continue;
            ++decoded;

            sizeHistogram[p.payloadSize()]++;
            if (p.crcValid())
                ++crcOk;
            if (p.frameSize() == frame.size())
                ++sizeDerived;
            if (p.encodeFrameRaw() == frame)
                ++byteIdentical;
        }

        check (decoded == 98, "all 98 live frames decode, got " + std::to_string (decoded));
        check (crcOk == 98, "all 98 live frames are CRC-8 valid, got " + std::to_string (crcOk));
        check (sizeDerived == 98,
               "frameSize() equals the received size for all 98, got "
                   + std::to_string (sizeDerived));
        check (byteIdentical == 98,
               "decode -> encodeFrameRaw is byte-identical for all 98, got "
                   + std::to_string (byteIdentical));

        // The whole point: 8 distinct payload lengths, none of them assumed.
        check (sizeHistogram.size() == 8,
               "live dump contains 8 distinct payload sizes, got "
                   + std::to_string (sizeHistogram.size()));

        const std::pair<std::size_t, int> expected[] = {
            { 1049, 82 }, { 1058, 3 }, { 1076, 1 }, { 1103, 1 },
            { 1129, 8 },  { 1139, 1 }, { 1148, 1 }, { 1156, 1 },
        };
        for (const auto& e : expected)
        {
            const auto it = sizeHistogram.find (e.first);
            const int got = (it == sizeHistogram.end()) ? 0 : it->second;
            check (got == e.second,
                   "payload size " + std::to_string (e.first) + " occurs "
                       + std::to_string (e.second) + " times, got " + std::to_string (got));
        }
    }

    // --- 2. factory writes are refused, on a REAL factory frame ---------------
    std::vector<std::uint8_t> factoryImage;
    const bool factoryOk = readFile (vendorFactory, factoryImage);
    check (factoryOk, "configured factory corpus is readable at " + vendorFactory);
    if (! factoryOk || factoryImage.empty())
    {
        // configured but unusable: already recorded as a failure above.
    }
    else
    {
        const auto frames = splitSyx (factoryImage);
        check (frames.size() == 49,
               "vendor factory dump splits into 49 frames, got "
                   + std::to_string (frames.size()));

        if (! frames.empty())
        {
            Preset p;
            std::string err;
            const bool decoded = decodeFrame (frames[0].data(), frames[0].size(), p, &err);
            check (decoded, "the first vendor factory frame decodes");

            // Only assert the write guard on a preset that actually decoded: a
            // default-constructed Preset is USER-addressed, so running these after
            // a failed decode reported "does not report the FACTORY page" and
            // "REFUSES a factory address", which points at the write guard instead
            // of at the decode failure.
            if (decoded)
            {
                check (! p.address().isUser(), "a vendor factory frame reports the FACTORY page");

                const auto persisted = p.encodeFrame (true /* persist */, false, &err);
                check (persisted.empty(),
                       "encodeFrame(persist=true) REFUSES a factory address");
                check (! err.empty(), "the factory refusal carries a reason");

                // The same preset must still be encodable as an audition: the
                // refusal is about writing, not about reading.
                const auto audition = p.encodeAudition();
                check (! audition.empty(), "encodeAudition() still works for a factory preset");
                check (audition.size() > 6 && audition[0] == 0xF0
                           && audition[6] == kFlagLoad,
                       "encodeAudition() is a frame with FF=0x00 (kFlagLoad)");
            }
        }
    }

    // --- 3. a USER address with the write flag is allowed ---------------------
    std::vector<std::uint8_t> userImage;
    const bool userOk = readFile (vendorUser, userImage);
    check (userOk, "configured user corpus is readable at " + vendorUser);
    if (! userOk || userImage.empty())
    {
        // configured but unusable: already recorded as a failure above.
    }
    else
    {
        const auto frames = splitSyx (userImage);
        check (frames.size() == 98,
               "vendor user dump splits into 98 frames, got "
                   + std::to_string (frames.size()));

        // Count every decodable USER frame, not just the first: `break` on the
        // first match made the counter a boolean, so a run in which 97 of 98 frames
        // failed to decode still passed.
        int userFrames = 0;
        bool exercised = false;
        for (const auto& frame : frames)
        {
            Preset p;
            std::string err;
            if (! decodeFrame (frame.data(), frame.size(), p, &err))
                continue;
            if (! p.address().isUser())
                continue;
            ++userFrames;

            if (! exercised)
            {
                exercised = true;
                const auto persisted = p.encodeFrame (true, false, &err);
                check (! persisted.empty(),
                       "encodeFrame(persist=true) allows USER bank "
                           + std::to_string ((int) p.address().bank) + " program "
                           + std::to_string ((int) p.address().program));
            }
        }
        check (userFrames == 49,
               "the vendor user dump holds 49 USER-page frames, got "
                   + std::to_string (userFrames));
    }

    // --- 4. malformed input is rejected, not silently accepted ---------------
    {
        Preset p;
        std::string err;
        check (! decodeFrame (nullptr, 0, p, &err), "a null frame is rejected");
        check (! err.empty(), "the null-frame rejection carries a reason");

        err.clear();
        const std::uint8_t notSysex[] = { 0x90, 0x40, 0x7F };
        check (! decodeFrame (notSysex, sizeof (notSysex), p, &err),
               "a 3-byte non-SysEx buffer is rejected");
        check (! err.empty(), "the non-SysEx rejection carries a reason");

        err.clear();
        std::vector<std::uint8_t> truncated (30, 0x00);
        truncated[0] = 0xF0;
        truncated[29] = 0xF7;
        check (! decodeFrame (truncated.data(), truncated.size(), p, &err),
               "a truncated frame is rejected");
        check (! err.empty(), "the truncated-frame rejection carries a reason");

        // The documented size bounds, not just an ad-hoc 30-byte buffer: a frame
        // whose payload is one byte below kMinPayloadSize, and one above
        // kMaxPayloadSize, must both be refused (a rejected frame must never leave
        // a caller with a half-populated Preset).
        auto wellFormedFrame = [] (std::size_t payloadBytes)
        {
            Bytes f;
            f.push_back (0xF0);
            f.insert (f.end(), kManufacturer, kManufacturer + 5);
            f.push_back (kFlagLoad);
            f.push_back (kPageUser);
            f.push_back (1);   // bank A
            f.push_back (1);   // program 1
            f.insert (f.end(), 2 * payloadBytes, 0x00);
            f.push_back (0xF7);
            return f;
        };

        err.clear();
        const auto belowMin = wellFormedFrame (kMinPayloadSize - 1);
        check (! decodeFrame (belowMin.data(), belowMin.size(), p, &err),
               "a payload below kMinPayloadSize is rejected");
        check (! err.empty(), "the kMinPayloadSize rejection carries a reason");

        err.clear();
        const auto aboveMax = wellFormedFrame (kMaxPayloadSize + 1);
        check (! decodeFrame (aboveMax.data(), aboveMax.size(), p, &err),
               "a payload above kMaxPayloadSize is rejected");
        check (! err.empty(), "the kMaxPayloadSize rejection carries a reason");
    }

    if (failures == 0)
        std::printf ("preset safety: all checks passed\n");
    else
        std::printf ("preset safety: %d FAILURES\n", failures);

    return failures == 0 ? 0 : 1;
}
