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

#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
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

std::vector<std::uint8_t> readFile (const std::string& path)
{
    std::ifstream in (path, std::ios::binary);
    if (! in)
        return {};
    return std::vector<std::uint8_t> ((std::istreambuf_iterator<char> (in)), {});
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

    const std::string live = gate + "/evidence/L4-device/device_dump_run1.syx";
    const std::string vendorFactory = gate + "/vendor/Nymphes_Factory_Presets_Factory_Bank.syx";
    const std::string vendorUser    = gate + "/vendor/Nymphes_Factory_Presets_User_Bank.syx";

    // --- 1. the live dump: real frames, 8 real lengths ------------------------
    const auto liveImage = readFile (live);
    if (liveImage.empty())
    {
        std::printf ("SKIP  live corpus not readable at %s\n", live.c_str());
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
    const auto factoryImage = readFile (vendorFactory);
    if (factoryImage.empty())
    {
        std::printf ("SKIP  vendor factory corpus not readable at %s\n", vendorFactory.c_str());
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
            check (decodeFrame (frames[0].data(), frames[0].size(), p, &err),
                   "the first vendor factory frame decodes");

            check (! p.address().isUser(), "a vendor factory frame reports the FACTORY page");

            const auto persisted = p.encodeFrame (true /* persist */, false, &err);
            check (persisted.empty(),
                   "encodeFrame(persist=true) REFUSES a factory address");
            check (! err.empty(), "the factory refusal carries a reason");

            // The same preset must still be encodable as an audition: the refusal is
            // about writing, not about reading.
            const auto audition = p.encodeAudition();
            check (! audition.empty(), "encodeAudition() still works for a factory preset");
            check (audition.size() > 6 && audition[6] == 0x00,
                   "encodeAudition() sets FF=0x00");
        }
    }

    // --- 3. a USER address with the write flag is allowed ---------------------
    const auto userImage = readFile (vendorUser);
    if (userImage.empty())
    {
        std::printf ("SKIP  vendor user corpus not readable at %s\n", vendorUser.c_str());
    }
    else
    {
        const auto frames = splitSyx (userImage);
        check (frames.size() == 98,
               "vendor user dump splits into 98 frames, got "
                   + std::to_string (frames.size()));

        int userFrames = 0;
        for (const auto& frame : frames)
        {
            Preset p;
            std::string err;
            if (! decodeFrame (frame.data(), frame.size(), p, &err))
                continue;
            if (! p.address().isUser())
                continue;
            ++userFrames;

            const auto persisted = p.encodeFrame (true, false, &err);
            check (! persisted.empty(),
                   "encodeFrame(persist=true) allows USER bank "
                       + std::to_string ((int) p.address().bank) + " program "
                       + std::to_string ((int) p.address().program));
            break;   // one is enough; the rule is per page, not per slot
        }
        check (userFrames == 1, "the vendor user dump contains a USER-page frame");
    }

    // --- 4. malformed input is rejected, not silently accepted ---------------
    {
        Preset p;
        std::string err;
        check (! decodeFrame (nullptr, 0, p, &err), "a null frame is rejected");

        const std::uint8_t notSysex[] = { 0x90, 0x40, 0x7F };
        check (! decodeFrame (notSysex, sizeof (notSysex), p, &err),
               "a 3-byte non-SysEx buffer is rejected");

        std::vector<std::uint8_t> truncated (30, 0x00);
        truncated[0] = 0xF0;
        truncated[29] = 0xF7;
        check (! decodeFrame (truncated.data(), truncated.size(), p, &err),
               "a truncated frame is rejected");
    }

    if (failures == 0)
        std::printf ("preset safety: all checks passed\n");
    else
        std::printf ("preset safety: %d FAILURES\n", failures);

    return failures == 0 ? 0 : 1;
}
