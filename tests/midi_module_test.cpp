// tests/midi_module_test.cpp — L2
//
// Device-free checks on the rules midi/ owns. These do not need a synth, a MIDI
// driver or a corpus: they pin the shared byte-stream reassembler (midi/MidiStream.h),
// the case-insensitive endpoint match every backend must agree on, and the CC table's
// lookup contract.
//
// Every assertion here was written against a defect that the OCR scan reported:
//
//   * a lost 0xF7 used to leave the port permanently deaf and growing memory;
//   * a close/reopen used to keep the running status, so the first data byte of the
//     next session completed a message left over from the previous one;
//   * the no-backend build matched "Nymphes" case-sensitively while macOS/Windows did
//     not, so a synthetic MidiDeviceInfo answered differently per platform;
//   * findCC()/indexOfCC() cast an int to uint8_t, so 257 and 513 aliased CC 1.

#include "midi/CCMap.h"
#include "midi/MidiPort.h"
#include "midi/MidiStream.h"

#include <cstdint>
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

using Bytes      = std::vector<std::uint8_t>;
using nymphes::midi_detail::StreamParser;
} // namespace

int main()
{
    using namespace nymphes;

    // --- 1. case-insensitive endpoint matching, identical on every backend -------
    // Windows decorates the name ("2- Nymphes", "MIDIIN2 (Nymphes)") and macOS
    // endpoints can carry a UTF-8 suffix; the no-backend build must classify a
    // synthetic entry exactly as the real backends do.
    {
        MidiDeviceInfo info;
        info.name = "2- nymphes";
        check (MidiPort::looksLikeNymphes (info), "looksLikeNymphes matches a lower-case name");

        info.name = "NYMPHES";
        check (MidiPort::looksLikeNymphes (info), "looksLikeNymphes matches an upper-case name");

        info.name = "MIDIIN2 (Nymphes)";
        check (MidiPort::looksLikeNymphes (info), "looksLikeNymphes matches a decorated name");

        info.name = "Nymphes \xE2\x80\x93 Studio";     // UTF-8 en dash
        check (MidiPort::looksLikeNymphes (info),
               "looksLikeNymphes survives a non-ASCII suffix (no ::tolower on a negative char)");

        info.name = "Some Other Synth";
        info.model = "NYMPHES";
        check (MidiPort::looksLikeNymphes (info), "looksLikeNymphes falls back to the model");

        info.name = "Some Other Synth";
        info.model = "Dreadbox";
        check (! MidiPort::looksLikeNymphes (info), "looksLikeNymphes rejects an unrelated device");

        check (midi_detail::containsIgnoreCase ("Nymphes", "nyMpHeS"),
               "containsIgnoreCase ignores case on both sides");
        check (! midi_detail::containsIgnoreCase ("Nymphes", "harp"),
               "containsIgnoreCase is a substring test, not a fuzzy match");
        check (midi_detail::containsIgnoreCase ("anything", ""),
               "containsIgnoreCase with an empty needle matches (callers skip it instead)");
    }

    // --- 2. channel-voice byte counts -------------------------------------------
    {
        check (midi_detail::bytesNeeded (0x90) == 2, "note-on needs 2 data bytes");
        check (midi_detail::bytesNeeded (0xC3) == 1, "program change needs 1 data byte");
        check (midi_detail::bytesNeeded (0xD0) == 1, "channel pressure needs 1 data byte");
        check (midi_detail::bytesNeeded (0xE0) == 2, "pitch bend needs 2 data bytes");
        check (midi_detail::bytesNeeded (0xF0) == 0, "a system status byte is not channel voice");
    }

    // --- 3. SysEx reassembly across arbitrary fragmentation ---------------------
    {
        const Bytes frame { 0xF0, 0x00, 0x21, 0x35, 0x00, 0x06, 0x01, 0xF7 };

        StreamParser oneByteAtATime;
        for (std::uint8_t b : frame)
            oneByteAtATime.push (&b, 1);
        const auto frames = oneByteAtATime.takeSysEx();
        check (frames.size() == 1, "a frame fed one byte at a time is reassembled once");
        check (! frames.empty() && frames[0] == frame, "the reassembled frame is byte-identical");

        StreamParser inOneGo;
        inOneGo.push (frame.data(), frame.size());
        check (inOneGo.takeSysEx().size() == 1, "a frame fed in one call is reassembled once");

        StreamParser two;
        const Bytes pair = { 0xF0, 0x7D, 0xF7, 0xF0, 0xF7 };
        two.push (pair.data(), pair.size());
        const auto both = two.takeSysEx();
        check (both.size() == 2, "two frames in one buffer are both reported");
        check (both.size() == 2 && both[1].size() == 2,
               "the shortest legal frame (F0 F7) is delivered");
    }

    // --- 4. real-time bytes inside a frame are dropped, not appended -----------
    {
        StreamParser p;
        const Bytes stream { 0xF0, 0x01, 0xF8, 0x02, 0xFE, 0x03, 0xF7 };
        p.push (stream.data(), stream.size());
        const auto frames = p.takeSysEx();
        check (frames.size() == 1, "clock/active-sensing inside a frame do not split it");
        check (frames.size() == 1 && frames[0] == Bytes ({ 0xF0, 0x01, 0x02, 0x03, 0xF7 }),
               "real-time bytes are dropped from the reassembled frame");
    }

    // --- 5. a lost 0xF7 must not make the port deaf -----------------------------
    // Before this rule the parser appended every following byte to the dead frame, so
    // no channel message was ever produced again.
    {
        StreamParser p;
        const Bytes stream { 0xF0, 0x01, 0x02,            // frame start, terminator lost
                             0x90, 0x3C, 0x40 };         // note-on C4 velocity 64
        p.push (stream.data(), stream.size());
        check (p.takeSysEx().empty(), "an unterminated frame is abandoned, not delivered");
        const auto msgs = p.takeChannel();
        check (msgs.size() == 1, "the byte after a lost terminator starts a new message");
        check (msgs.size() == 1 && msgs[0].status == 0x90 && msgs[0].data1 == 0x3C
                   && msgs[0].data2 == 0x40,
               "the recovered message is the note-on that followed the lost terminator");
        check (! p.inSysEx(), "the parser is not stuck in SysEx after recovery");
    }

    // --- 6. memory is bounded ---------------------------------------------------
    {
        StreamParser p;
        const std::uint8_t f0 = 0xF0;
        p.push (&f0, 1);
        const Bytes block (1024, 0x11);
        for (int i = 0; i < 200; ++i)               // 200 KiB with no terminator
            p.push (block.data(), block.size());
        check (p.pendingSysExBytes() <= StreamParser::kMaxSysExBytes,
               "an unbounded frame cannot grow past kMaxSysExBytes");
        check (! p.inSysEx(), "an over-long frame is abandoned");
        check (p.takeSysEx().empty(), "an over-long frame is never delivered");
    }

    // --- 7. running status, and the reset that teardown must do -----------------
    {
        StreamParser p;
        const Bytes running { 0x90, 0x3C, 0x40, 0x3E, 0x40 };
        p.push (running.data(), running.size());
        check (p.takeChannel().size() == 2, "running status produces one message per data pair");

        StreamParser afterClose;
        const Bytes partial { 0xB0, 0x07 };          // CC 7, value missing
        afterClose.push (partial.data(), partial.size());
        check (afterClose.takeChannel().empty(), "an incomplete message is not emitted");
        afterClose.reset();                          // what every teardown path does
        const std::uint8_t stray = 0x40;
        afterClose.push (&stray, 1);
        check (afterClose.takeChannel().empty(),
               "after reset() a stray data byte does not complete the previous message");
        check (afterClose.pendingSysExBytes() == 0 && ! afterClose.inSysEx(),
               "reset() leaves no partial SysEx state");

        // MIDI 1.0: System Exclusive cancels running status, so a data byte after a
        // frame must not complete a channel message from before it (raised by L4's
        // review of this parser).
        StreamParser acrossFrame;
        const Bytes note { 0x90, 0x3C, 0x40 };
        acrossFrame.push (note.data(), note.size());
        check (acrossFrame.takeChannel().size() == 1, "the note-on before the frame is emitted");
        const Bytes frame { 0xF0, 0x01, 0x02, 0xF7 };
        acrossFrame.push (frame.data(), frame.size());
        check (acrossFrame.takeSysEx().size() == 1, "the frame between them still completes");
        const std::uint8_t afterFrame = 0x40;
        acrossFrame.push (&afterFrame, 1);
        check (acrossFrame.takeChannel().empty(),
               "SysEx cancels running status: a stray byte after a frame emits nothing");
    }

    // --- 7b. running status survives a real-time byte but not a reset -----------
    {
        StreamParser p;
        const Bytes withClock { 0x90, 0x3C, 0xF8, 0x40 };   // clock inside the message
        p.push (withClock.data(), withClock.size());
        const auto msgs = p.takeChannel();
        check (msgs.size() == 1 && msgs[0].data2 == 0x40,
               "a real-time byte between the data bytes does not break the message");
    }

    // --- 8. the CC table's lookup contract --------------------------------------
    {
        check (kCCCount == 82, "the table describes 82 parameters");
        check (findCC (38) == nullptr, "CC 38 is absent on this device");
        check (findCC (74) != nullptr && std::string (findCC (74)->name) == "lpf_cutoff",
               "CC 74 resolves to lpf_cutoff");

        // Out-of-range numbers are not parameters: a uint8_t cast would alias them.
        for (int cc : { -1, -255, 128, 255, 257, 513, 1024 })
        {
            check (findCC (cc) == nullptr,
                   "findCC(" + std::to_string (cc) + ") is not a parameter");
            check (indexOfCC (cc) == -1,
                   "indexOfCC(" + std::to_string (cc) + ") is -1");
        }
        check (findCC (1) != nullptr && std::string (findCC (1)->name) == "mod_wheel",
               "CC 1 still resolves (the alias the range check prevents)");

        // Strictly ascending and duplicate-free: indexOfCC's index is findCC's entry.
        bool ascending = true;
        for (int i = 1; i < kCCCount; ++i)
            if (kCCMap[i - 1].cc >= kCCMap[i].cc)
                ascending = false;
        check (ascending, "kCCMap is strictly ascending with unique CC numbers");
        for (int i = 0; i < kCCCount; ++i)
            check (indexOfCC (kCCMap[i].cc) == i && findCC (kCCMap[i].cc) == &kCCMap[i],
                   "indexOfCC/findCC round-trip for CC " + std::to_string (kCCMap[i].cc));

        // The wave CCs are continuous 0..127 on the device (ccmap.json, three sources):
        // discreteness comes from the range, not from the presence of valueNames.
        if (const auto* wave = findCC (19))
        {
            check (! wave->isDiscrete(), "CC 19 lfo1_wave is a 0..127 controller, not an enum");
            check (wave->isValidValue (127), "CC 19 accepts 127 (the device's maximum)");
            check (wave->isValidValue (4), "CC 19 accepts 4 (one of its five shapes)");
            check (! wave->isValidValue (128), "CC 19 rejects a value above its range");
        }
        else
        {
            check (false, "CC 19 is present");
        }
    }

    if (failures == 0)
        std::printf ("OK    midi_module_test: reassembly, matching, CC lookup\n");
    else
        std::printf ("FAILED %d check(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
