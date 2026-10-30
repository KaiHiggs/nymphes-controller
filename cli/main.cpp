// nymphes-cli — headless preset browser and saver for the Dreadbox Nymphes.
//
// The same codec the plugin uses (core/, via preset/Preset.h), so anything the
// plugin can decode this can decode, and vice versa. Device commands open the
// CoreMIDI port named "Nymphes" directly.
//
// SAFETY, enforced in code rather than documented:
//   * Only USER slots are ever written. A factory address is refused before a
//     byte is sent (AA=0x01 is rejected by encodeForDevice).
//   * `write` reads the slot first and refuses to write a slot it cannot restore,
//     then verifies the write by reading it back, then restores and verifies the
//     restore. Payload bytes are compared verbatim and never rebuilt from a
//     semantic view, because the device emits payloads of 1049..1156 bytes and a
//     rebuild would silently drop the optional fields.
//   * No firmware SysEx. The only messages constructed here are the preset frame
//     (product 00 06), the documented dump request, and single CC messages.

#include "../midi/CCMap.h"
#include "../midi/MidiPort.h"
#include "../preset/Preset.h"
#include "../preset/PresetBank.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__) || defined(__unix__)
#  include <fcntl.h>
#  include <sys/file.h>
#  include <unistd.h>
#  define NYMPHES_HAS_FLOCK 1
#endif

using namespace nymphes;

namespace
{

struct Options
{
    std::map<std::string, std::string> values;
    std::vector<std::string> positional;

    bool has (const std::string& k) const { return values.count (k) > 0; }
    std::string get (const std::string& k, const std::string& d = {}) const
    {
        auto it = values.find (k);
        return it == values.end() ? d : it->second;
    }
    int getInt (const std::string& k, int d) const
    {
        auto it = values.find (k);
        return it == values.end() ? d : std::atoi (it->second.c_str());
    }
};

Options parse (int argc, char** argv, int from)
{
    Options o;
    for (int i = from; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a.rfind ("--", 0) == 0)
        {
            const auto eq = a.find ('=');
            if (eq != std::string::npos) { o.values[a.substr (0, eq)] = a.substr (eq + 1); continue; }
            if (i + 1 < argc && std::string (argv[i + 1]).rfind ("--", 0) != 0)
                o.values[a] = argv[++i];
            else
                o.values[a] = "";
        }
        else o.positional.push_back (a);
    }
    return o;
}

void usage()
{
    std::puts (
        "nymphes-cli — Dreadbox Nymphes preset browser/saver\n"
        "\n"
        "file commands:\n"
        "  verify <file.syx>                   framing, CRC and structure report\n"
        "  list <file.syx>                     list the presets in a dump\n"
        "  show <file.syx> [--index N]         every named parameter of one preset\n"
        "  extract <in.syx> --index N --out F  write one preset as its own .syx\n"
        "  split <in.syx> --outdir DIR         one .syx per preset\n"
        "  merge <a.syx> [b.syx ...] --out F   concatenate dumps into one .syx\n"
        "  txt <in.syx> [--index N] [--out F]  human-readable parameter text\n"
        "  export-txt <in.syx> --out F         tab-separated table of all presets\n"
        "\n"
        "device commands (each takes --lock <file>; there is no flock(1) on macOS):\n"
        "  devices                             list MIDI endpoints\n"
        "  ccmap                               print the 82-CC parameter map\n"
        "  read --slot A1 [--out F]            load that slot, capture what the device sends\n"
        "  write <file.syx> --slot A1 --yes    write a USER slot, read back, restore the original\n"
        "  capture --seconds N --out F         record a MENU-driven dump\n"
        "  dump-request                        send the documented full-dump request\n"
        "  cc <cc> <value> [--channel N]       send one CC\n"
        "  panic [--channel N]                 all-sound-off + all-notes-off\n"
        "\n"
        "No preset carries a name: the payload is a CRC plus a 22-field protobuf with no\n"
        "strings, so a preset is identified by its slot. FACTORY slots are read-only here.\n");
}

// ---------------------------------------------------------------------------
// file commands
// ---------------------------------------------------------------------------

int cmdVerify (const Options& o)
{
    if (o.positional.empty()) { std::fprintf (stderr, "verify: need a .syx path\n"); return 2; }
    const std::string path = o.positional[0];

    std::ifstream in (path, std::ios::binary);
    if (! in) { std::fprintf (stderr, "cannot open %s\n", path.c_str()); return 1; }
    const std::vector<uint8_t> image ((std::istreambuf_iterator<char> (in)), {});

    const auto frames = splitSyx (image);
    std::vector<Preset> presets;
    std::vector<std::string> errors;

    for (size_t i = 0; i < frames.size(); ++i)
    {
        Preset p;
        std::string err;
        if (decodeFrame (frames[i].data(), frames[i].size(), p, &err)) presets.push_back (p);
        else errors.push_back ("message " + std::to_string (i) + ": " + err);
    }

    int user = 0, factory = 0, badCrc = 0;
    size_t variable = 0, badReencode = 0, nonCanonical = 0;
    size_t minPayload = SIZE_MAX, maxPayload = 0;
    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto& p = presets[i];
        if (p.address().isUser()) ++user; else ++factory;
        if (! p.crcValid()) ++badCrc;
        if (p.payloadSize() != static_cast<size_t> (kPayloadSize)) ++variable;
        minPayload = std::min (minPayload, p.payloadSize());
        maxPayload = std::max (maxPayload, p.payloadSize());
        // The two checks that make this a gate rather than a summary: the frame must
        // re-serialise byte-for-byte (the slot-write path depends on exactly that),
        // and the parsed protobuf must reproduce the payload (otherwise editing a
        // preset would silently rewrite untouched fields).
        if (p.encodeFrameRaw() != frames[i]) ++badReencode;
        if (! p.isCanonical()) ++nonCanonical;
    }

    std::printf ("file      %s\n", path.c_str());
    std::printf ("bytes     %zu\n", image.size());
    std::printf ("messages  %zu decoded, %zu rejected\n", presets.size(), errors.size());
    std::printf ("pages     %d USER, %d FACTORY\n", user, factory);
    std::printf ("crc       %zu/%zu valid\n", presets.size() - static_cast<size_t> (badCrc), presets.size());
    if (! presets.empty())
        std::printf ("payload   %zu..%zu bytes (%zu of %zu not the canonical %zu)\n",
                     minPayload, maxPayload, variable, presets.size(),
                     static_cast<std::size_t> (kPayloadSize));
    std::printf ("re-encode  %zu of %zu differ from the bytes on disk\n", badReencode, presets.size());
    std::printf ("protobuf    %zu of %zu not canonical\n", nonCanonical, presets.size());
    for (const auto& e : errors) std::printf ("  ERROR %s\n", e.c_str());

    const bool ok = errors.empty() && badCrc == 0 && badReencode == 0
                    && nonCanonical == 0 && ! presets.empty();
    std::printf ("%s\n", ok ? "PASS verify" : "FAIL verify");
    return ok ? 0 : 1;
}

int cmdList (const Options& o)
{
    if (o.positional.empty()) { std::fprintf (stderr, "list: need a .syx path\n"); return 2; }
    std::string err;
    const auto presets = importSyxFile (o.positional[0], err);
    if (presets.empty()) { std::fprintf (stderr, "no presets: %s\n", err.c_str()); return 1; }
    if (! err.empty()) std::fprintf (stderr, "warning: %s\n", err.c_str());

    std::printf ("%3s  %-12s %-7s %s\n", "#", "slot", "size", "summary");
    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto& p = presets[i];
        std::printf ("%3zu  %-12s %4zu B  %s\n", i, displayName (p).c_str(), p.payloadSize(),
                     summaryLine (p).c_str() + displayName (p).size() + 1);
    }
    return 0;
}

static const char* groupName (Group g)
{
    switch (g)
    {
        case Group::Main:            return "main";
        case Group::Reverb:          return "reverb";
        case Group::Lfo2:            return "lfo2";
        case Group::ModWheel:        return "mod_wheel";
        case Group::Velocity:        return "velocity";
        case Group::Aftertouch:      return "aftertouch";
        case Group::LfoSettings:     return "lfo_settings";
        case Group::Legato:          return "legato";
        case Group::VoiceMode:       return "voice_mode";
        case Group::Chord1:          return "chord_1";
        case Group::Chord2:          return "chord_2";
        case Group::Chord3:          return "chord_3";
        case Group::Chord4:          return "chord_4";
        case Group::Chord5:          return "chord_5";
        case Group::Chord6:          return "chord_6";
        case Group::Chord7:          return "chord_7";
        case Group::Chord8:          return "chord_8";
        case Group::ExtraLfo2:       return "extra_lfo_2";
        case Group::ExtraModWheel:   return "extra_mod_wheel";
        case Group::ExtraVelocity:   return "extra_velocity";
        case Group::ExtraAftertouch: return "extra_aftertouch";
        case Group::AmpLevel:        return "amp_level";
    }
    return "?";
}

int cmdShow (const Options& o)
{
    if (o.positional.empty()) { std::fprintf (stderr, "show: need a .syx path\n"); return 2; }
    std::string err;
    const auto presets = importSyxFile (o.positional[0], err);
    if (presets.empty()) { std::fprintf (stderr, "no presets: %s\n", err.c_str()); return 1; }

    const int index = o.getInt ("--index", 0);
    if (index < 0 || index >= static_cast<int> (presets.size()))
    { std::fprintf (stderr, "index %d out of range (0..%zu)\n", index, presets.size() - 1); return 2; }
    const auto& p = presets[static_cast<size_t> (index)];

    std::printf ("%s\n", summaryLine (p).c_str());
    std::printf ("payload   %zu bytes, CRC 0x%02X %s\n", p.payloadSize(),
                 p.payload().empty() ? 0 : p.payload()[0], p.crcValid() ? "(valid)" : "(INVALID)");
    std::printf ("frame     %zu bytes, flag=0x%02X page=0x%02X bank=%u program=%u\n",
                 p.frameSize(), p.flag(), p.address().page, p.address().bank, p.address().program);

    std::string current;
    for (const auto& f : describe (p))
    {
        const std::string g = groupName (f.group);
        if (g != current) { current = g; std::printf ("\n[%s]\n", g.c_str()); }
        std::printf ("  %-20s %s%.6f\n", f.name, f.discrete ? "(discrete) " : "", double (f.value));
    }
    return 0;
}

int cmdExtract (const Options& o)
{
    if (o.positional.empty()) { std::fprintf (stderr, "extract: need a .syx path\n"); return 2; }
    std::string err;
    const auto presets = importSyxFile (o.positional[0], err);
    if (presets.empty()) { std::fprintf (stderr, "no presets: %s\n", err.c_str()); return 1; }
    const int index = o.getInt ("--index", 0);
    if (index < 0 || index >= static_cast<int> (presets.size())) { std::fprintf (stderr, "index out of range\n"); return 2; }
    const std::string out = o.get ("--out");
    if (out.empty()) { std::fprintf (stderr, "extract: need --out\n"); return 2; }

    std::string werr;
    if (! exportSyxFile (out, { presets[static_cast<size_t> (index)] }, werr))
    { std::fprintf (stderr, "%s\n", werr.c_str()); return 1; }
    std::printf ("wrote %s (%s)\n", out.c_str(), displayName (presets[static_cast<size_t> (index)]).c_str());
    return 0;
}

int cmdSplit (const Options& o)
{
    if (o.positional.empty()) { std::fprintf (stderr, "split: need a .syx path\n"); return 2; }
    const std::string dir = o.get ("--outdir");
    if (dir.empty()) { std::fprintf (stderr, "split: need --outdir\n"); return 2; }
    std::filesystem::create_directories (dir);

    std::string err;
    const auto presets = importSyxFile (o.positional[0], err);
    if (presets.empty()) { std::fprintf (stderr, "no presets: %s\n", err.c_str()); return 1; }

    int n = 0;
    for (const auto& p : presets)
    {
        const std::string name = dir + "/" + (p.address().isUser() ? "user_" : "factory_")
                               + Slot::fromAddress (p.address()).toString() + ".syx";
        std::string werr;
        if (exportSyxFile (name, { p }, werr)) ++n;
        else std::fprintf (stderr, "%s\n", werr.c_str());
    }
    std::printf ("wrote %d files to %s\n", n, dir.c_str());
    return n > 0 ? 0 : 1;
}

int cmdMerge (const Options& o)
{
    const std::string out = o.get ("--out");
    if (out.empty() || o.positional.empty()) { std::fprintf (stderr, "merge: need inputs and --out\n"); return 2; }
    std::vector<Preset> all;
    for (const auto& path : o.positional)
    {
        std::string err;
        const auto v = importSyxFile (path, err);
        if (! err.empty()) std::fprintf (stderr, "%s: %s\n", path.c_str(), err.c_str());
        all.insert (all.end(), v.begin(), v.end());
    }
    if (all.empty()) { std::fprintf (stderr, "nothing to merge\n"); return 1; }
    // Order by (page, bank, program) so split -> merge reproduces the original dump
    // byte for byte. Sorting by filename is not enough: it puts factory_* before
    // user_*, which is the opposite of what the device sends.
    std::stable_sort (all.begin(), all.end(), [] (const Preset& a, const Preset& b)
    {
        if (a.address().page != b.address().page) return a.address().page < b.address().page;
        if (a.address().bank != b.address().bank) return a.address().bank < b.address().bank;
        return a.address().program < b.address().program;
    });
    std::string werr;
    if (! exportSyxFile (out, all, werr)) { std::fprintf (stderr, "%s\n", werr.c_str()); return 1; }
    std::printf ("wrote %s with %zu presets\n", out.c_str(), all.size());
    return 0;
}

int cmdTxt (const Options& o)
{
    if (o.positional.empty()) { std::fprintf (stderr, "txt: need a .syx path\n"); return 2; }
    std::string err;
    const auto presets = importSyxFile (o.positional[0], err);
    if (presets.empty()) { std::fprintf (stderr, "no presets: %s\n", err.c_str()); return 1; }
    const int index = o.getInt ("--index", 0);
    if (index < 0 || index >= static_cast<int> (presets.size())) { std::fprintf (stderr, "index out of range\n"); return 2; }
    const auto& p = presets[static_cast<size_t> (index)];

    const std::string out = o.get ("--out");
    std::ofstream f;
    if (! out.empty()) f.open (out, std::ios::trunc);
    std::ostream& os = out.empty() ? std::cout : f;

    os << "# Nymphes preset " << displayName (p) << "\n";
    os << "# payload " << p.payloadSize() << " B, CRC 0x" << std::hex
       << static_cast<int> (p.payload().empty() ? 0 : p.payload()[0]) << std::dec
       << (p.crcValid() ? " (valid)" : " (INVALID)") << "\n";
    for (const auto& fv : describe (p))
        os << fv.name << " " << fv.value << "\n";
    if (! out.empty()) std::printf ("wrote %s\n", out.c_str());
    return 0;
}

int cmdExportTxt (const Options& o)
{
    if (o.positional.empty()) { std::fprintf (stderr, "export-txt: need a .syx path\n"); return 2; }
    std::string err;
    const auto presets = importSyxFile (o.positional[0], err);
    if (presets.empty()) { std::fprintf (stderr, "no presets: %s\n", err.c_str()); return 1; }

    const std::string out = o.get ("--out");
    std::ofstream f;
    if (! out.empty()) f.open (out, std::ios::trunc);
    std::ostream& os = out.empty() ? std::cout : f;

    const auto fields = describe (presets.front());
    os << "slot";
    for (const auto& fv : fields) os << '\t' << fv.name;
    os << "\n";
    for (const auto& p : presets)
    {
        os << displayName (p);
        for (const auto& fv : describe (p)) os << '\t' << fv.value;
        os << "\n";
    }
    if (! out.empty()) std::printf ("wrote %s\n", out.c_str());
    return 0;
}

// ---------------------------------------------------------------------------
// device commands
// ---------------------------------------------------------------------------

class DeviceSession
{
public:
    bool open (std::string& error)
    {
        port_.setSysExCallback ([this] (const std::vector<uint8_t>& frame)
        {
            std::lock_guard<std::mutex> lock (mutex_);
            incoming_.push_back (frame);
        });
        return port_.open ("Nymphes", &error);
    }

    std::string name() const { return port_.openedName(); }
    MidiPort& port() { return port_; }

    /** Wait for the next preset frame the device emits. The device sends payloads
        of 1049..1156 bytes, so the only size test is "a whole F0..F7 frame". */
    bool awaitPreset (Preset& out, int timeoutMs, std::string& error)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            std::vector<uint8_t> frame;
            {
                std::lock_guard<std::mutex> lock (mutex_);
                if (! incoming_.empty())
                {
                    frame = std::move (incoming_.front());
                    incoming_.erase (incoming_.begin());
                }
            }
            if (! frame.empty())
            {
                Preset p;
                std::string err;
                if (decodeFrame (frame.data(), frame.size(), p, &err)) { out = p; return true; }
                error = err;
                return false;
            }
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
        error = "timed out waiting for the device to send a preset";
        return false;
    }

    size_t drain (int timeoutMs, std::vector<Preset>& out)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            Preset p;
            std::string err;
            if (awaitPreset (p, 60, err)) out.push_back (p);
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
        return out.size();
    }

    void reset() { std::lock_guard<std::mutex> lock (mutex_); incoming_.clear(); }

    /** Send the vendor's documented full-dump request and collect every preset frame
        the device answers with, stopping after `quietMs` of silence.

        This is the ONLY reliable way to read device state. Measured on the operator's
        unit (exchange/L4.review-device-protocol.md, measurement 1): the device sends
        NOTHING after a preset frame, whether FF=0x01 (write) or FF=0x00 (load), so any
        "send a load and wait for the echo" read times out by construction. The dump
        request, by contrast, answers with 78 frames in 5 s and all 98 in about 9 s.
        Returns the number of presets collected; `error` is set only on a send failure. */
    size_t requestFullDump (int maxMs, int quietMs, std::vector<Preset>& out, std::string& error)
    {
        static const std::vector<uint8_t> request { 0xF0, 0x00, 0x21, 0x35, 0x00, 0x06, 0x02, 0xF7 };
        reset();
        if (! port_.sendSysEx (request, &error)) return 0;

        const auto start = std::chrono::steady_clock::now();
        auto lastFrame = start;
        const auto deadline = start + std::chrono::milliseconds (maxMs);

        while (std::chrono::steady_clock::now() < deadline)
        {
            Preset p;
            std::string ignored;
            if (awaitPreset (p, 50, ignored)) { out.push_back (p); lastFrame = std::chrono::steady_clock::now(); }
            else if (! out.empty()
                     && std::chrono::steady_clock::now() - lastFrame > std::chrono::milliseconds (quietMs))
                break;   // the device has stopped talking; the dump is complete
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
        return out.size();
    }

private:
    MidiPort port_;
    std::mutex mutex_;
    std::vector<std::vector<uint8_t>> incoming_;
};

/** Read one slot by requesting the full dump and selecting the frame whose
    (page, bank, program) matches.

    This replaced a "send a load frame and wait for the echo" implementation, which
    could never work: the device does not echo presets (measured; see
    exchange/L4.review-device-protocol.md). A load frame sent to a slot that holds a
    valid preset does still make the synth load it — it just never reports back — so
    the old path both failed and left the device silently auditioning a preset. */
bool readSlot (DeviceSession& session, Page page, Slot slot, Preset& out,
               std::string& error, int maxMs = 15000, int quietMs = 2000)
{
    std::vector<Preset> dump;
    const size_t received = session.requestFullDump (maxMs, quietMs, dump, error);
    if (received == 0)
    {
        if (error.empty())
            error = "the device sent nothing after the full-dump request (is it in a menu?";
        if (error.empty()) error = "no frames received";
        return false;
    }

    const uint8_t wantPage = static_cast<uint8_t> (page);
    for (const auto& p : dump)
        if (p.address().page == wantPage && p.address().bank == slot.bank
            && p.address().program == slot.program)
        { out = p; return true; }

    error = "slot " + slot.toString() + " was not in the dump (" + std::to_string (received)
          + " frames received)";
    return false;
}

int cmdDevices()
{
    std::printf ("backend   %s\n", MidiPort::backendName());
    const auto dests = MidiPort::listDestinations();
    std::printf ("outputs   %zu\n", dests.size());
    for (const auto& d : dests)
        std::printf ("  %-28s %-14s %-14s %-14s%s\n", d.name.c_str(), d.manufacturer.c_str(),
                     d.model.c_str(), d.serial.c_str(),
                     MidiPort::looksLikeNymphes (d) ? "  <- Nymphes" : "");
    const auto sources = MidiPort::listSources();
    std::printf ("inputs    %zu\n", sources.size());
    for (const auto& d : sources)
        std::printf ("  %-28s %-14s %-14s %-14s%s\n", d.name.c_str(), d.manufacturer.c_str(),
                     d.model.c_str(), d.serial.c_str(),
                     MidiPort::looksLikeNymphes (d) ? "  <- Nymphes" : "");
    return 0;
}

int cmdCCMap()
{
    std::printf ("%3s  %-28s %7s  %s\n", "CC", "parameter", "range", "values");
    int continuous = 0, discrete = 0;
    for (const auto& p : kCCMap)
    {
        char range[16];
        std::snprintf (range, sizeof range, "%d-%d", p.minValue, p.maxValue);
        std::printf ("%3d  %-28s %7s  %s\n", p.cc, p.name, range, p.valueNames == nullptr ? "" : p.valueNames);
        (p.isDiscrete() ? discrete : continuous)++;
    }
    std::printf ("\n%d parameters: %d continuous, %d discrete. CC 38 is absent on this device.\n",
                 kCCCount, continuous, discrete);
    return 0;
}

int cmdRead (const Options& o)
{
    Slot slot;
    if (! Slot::fromString (o.get ("--slot", "A1"), slot))
    { std::fprintf (stderr, "read: --slot must be like A1..G7\n"); return 2; }

    std::string error;
    DeviceSession session;
    if (! session.open (error)) { std::fprintf (stderr, "cannot open the Nymphes port: %s\n", error.c_str()); return 1; }
    std::printf ("port      %s\n", session.name().c_str());

    Preset p;
    if (! readSlot (session, Page::User, slot, p, error))
    { std::fprintf (stderr, "read failed: %s\n", error.c_str()); return 1; }

    std::printf ("%s\n", summaryLine (p).c_str());

    const std::string out = o.get ("--out");
    if (! out.empty())
    {
        std::string werr;
        if (! exportSyxFile (out, { p }, werr)) { std::fprintf (stderr, "%s\n", werr.c_str()); return 1; }
        std::printf ("wrote     %s\n", out.c_str());
    }
    return 0;
}

int cmdWrite (const Options& o)
{
    if (o.positional.empty()) { std::fprintf (stderr, "write: need a .syx file with one preset\n"); return 2; }

    Slot slot;
    if (! Slot::fromString (o.get ("--slot", ""), slot))
    { std::fprintf (stderr, "write: --slot must be like A1..G7\n"); return 2; }

    if (o.get ("--page", "user") != "user")
    {
        std::fprintf (stderr, "write: refusing --page %s. This tool writes USER slots only; "
                              "factory presets are never overwritten.\n", o.get ("--page").c_str());
        return 2;
    }

    std::string err;
    auto presets = importSyxFile (o.positional[0], err);
    if (presets.empty()) { std::fprintf (stderr, "no preset in %s: %s\n", o.positional[0].c_str(), err.c_str()); return 1; }
    Preset toWrite = presets.front();

    // Move the preset to the requested USER slot. The payload is carried verbatim;
    // only the address and the CRC are touched, so optional fields and negative
    // varints survive exactly as they were.
    toWrite.setAddress (slot.toAddress (Page::User));

    std::string encErr;
    const auto writeFrame = encodeForDevice (toWrite, /*persist*/ true, /*allowFactoryWrite*/ false, &encErr);
    if (writeFrame.empty()) { std::fprintf (stderr, "refused: %s\n", encErr.c_str()); return 1; }

    std::printf ("plan\n");
    std::printf ("  slot      USER %s\n", slot.toString().c_str());
    std::printf ("  source    %s (originally %s, payload %zu B)\n", o.positional[0].c_str(),
                 displayName (toWrite).c_str(), toWrite.payloadSize());
    std::printf ("  steps     dump-read original -> write -> dump-read back -> compare -> restore -> verify\n");

    if (! o.has ("--yes"))
    {
        std::fprintf (stderr, "\nrefusing to touch the device without --yes\n");
        return 2;
    }

    std::string error;
    DeviceSession session;
    if (! session.open (error)) { std::fprintf (stderr, "cannot open the Nymphes port: %s\n", error.c_str()); return 1; }
    std::printf ("port      %s\n", session.name().c_str());

    Preset original;
    if (! readSlot (session, Page::User, slot, original, error))
    {
        std::fprintf (stderr, "could not read the slot's original contents (%s); refusing to write a "
                              "slot I cannot restore\n", error.c_str());
        return 1;
    }
    std::printf ("original  %s\n", summaryLine (original).c_str());

    // Write.
    session.reset();
    if (! session.port().sendSysEx (writeFrame, &error))
    { std::fprintf (stderr, "write failed: %s\n", error.c_str()); return 1; }
    std::printf ("write     sent (FF=01, AA=00, %s)\n", slot.toString().c_str());

    // Read back and compare payload bytes verbatim. The device does not echo the
    // write, so this re-dumps and picks the slot out of the dump.
    bool identical = false;
    Preset back;
    if (readSlot (session, Page::User, slot, back, error))
    {
        identical = back.payloadSize() == toWrite.payloadSize()
                 && std::memcmp (back.payload().data(), toWrite.payload().data(), toWrite.payloadSize()) == 0;
        std::printf ("readback  %s (payload %zu B)\n", identical ? "byte-identical" : "DIFFERS", back.payloadSize());
        if (! identical)
        {
            const size_t n = std::min (back.payloadSize(), toWrite.payloadSize());
            for (size_t i = 0; i < n; ++i)
                if (back.payload()[i] != toWrite.payload()[i])
                {
                    std::printf ("  first difference at payload byte %zu: wrote 0x%02X read 0x%02X\n",
                                 i, toWrite.payload()[i], back.payload()[i]);
                    break;
                }
        }
    }
    else std::fprintf (stderr, "read-back failed: %s\n", error.c_str());

    if (o.has ("--no-restore"))
    {
        std::printf ("restore   SKIPPED (--no-restore) — slot %s now holds the new preset\n", slot.toString().c_str());
        return identical ? 0 : 1;
    }

    // Restore the original contents.
    Preset restore = original;
    restore.setAddress (slot.toAddress (Page::User));
    std::string restoreErr;
    const auto restoreFrame = encodeForDevice (restore, /*persist*/ true, false, &restoreErr);
    if (restoreFrame.empty())
    { std::fprintf (stderr, "RESTORE FAILED to encode: %s — slot NOT restored\n", restoreErr.c_str()); return 1; }

    session.reset();
    if (! session.port().sendSysEx (restoreFrame, &error))
    {
        std::fprintf (stderr, "RESTORE FAILED: %s — slot %s is NOT restored\n", error.c_str(), slot.toString().c_str());
        return 1;
    }

    Preset restored;
    const bool restoredOk = readSlot (session, Page::User, slot, restored, error)
                         && restored.payloadSize() == original.payloadSize()
                         && std::memcmp (restored.payload().data(), original.payload().data(),
                                         original.payloadSize()) == 0;
    std::printf ("restore   %s\n", restoredOk ? "verified byte-identical to the original"
                                              : "FAILED TO VERIFY — check the slot");

    return (identical && restoredOk) ? 0 : 1;
}

int cmdCapture (const Options& o)
{
    const int seconds = o.getInt ("--seconds", 20);
    const std::string out = o.get ("--out");
    if (out.empty()) { std::fprintf (stderr, "capture: need --out\n"); return 2; }

    std::string error;
    DeviceSession session;
    if (! session.open (error)) { std::fprintf (stderr, "cannot open the Nymphes port: %s\n", error.c_str()); return 1; }

    std::printf ("port      %s\n", session.name().c_str());
    std::printf ("listening %d s — on the device press MENU, choose the dump, then SAVE\n", seconds);
    std::fflush (stdout);

    std::vector<Preset> captured;
    // Collect until the device goes quiet rather than trusting --seconds: measured,
    // 78 of 98 frames arrive in 5 s and the rest by ~9 s, so a fixed short window
    // silently truncates the dump.
    session.requestFullDump (std::max (seconds, 1) * 1000, 2500, captured, error);

    if (captured.empty()) { std::fprintf (stderr, "nothing received\n"); return 1; }
    std::string werr;
    if (! exportSyxFile (out, captured, werr)) { std::fprintf (stderr, "%s\n", werr.c_str()); return 1; }
    std::printf ("captured  %zu presets -> %s\n", captured.size(), out.c_str());
    if (captured.size() < 98)
        std::fprintf (stderr, "warning: a full unit holds 98; %zu means the dump was truncated or the "
                              "unit is not answering completely (try --seconds 20)\n", captured.size());
    return 0;
}

int cmdDumpRequest (const Options& o)
{
    (void) o;
    // Documented by the vendor: F0 00 21 35 00 06 02 F7. Message type 0x02, no
    // payload. This is NOT a firmware message (type 0x05) and nothing here can
    // construct one.
    const std::vector<uint8_t> request { 0xF0, 0x00, 0x21, 0x35, 0x00, 0x06, 0x02, 0xF7 };

    std::string error;
    DeviceSession session;
    if (! session.open (error)) { std::fprintf (stderr, "cannot open the Nymphes port: %s\n", error.c_str()); return 1; }
    if (! session.port().sendSysEx (request, &error)) { std::fprintf (stderr, "send failed: %s\n", error.c_str()); return 1; }
    std::printf ("sent      full-dump request to %s\n", session.name().c_str());
    return 0;
}

int cmdCC (const Options& o)
{
    if (o.positional.size() < 2) { std::fprintf (stderr, "cc: need <cc> <value>\n"); return 2; }
    const int cc = std::atoi (o.positional[0].c_str());
    const int value = std::atoi (o.positional[1].c_str());
    const int channel = o.getInt ("--channel", 1) - 1;

    const auto* spec = findCC (cc);
    if (spec == nullptr) { std::fprintf (stderr, "CC %d is not a Nymphes parameter\n", cc); return 2; }
    if (value < spec->minValue || value > spec->maxValue)
    {
        std::fprintf (stderr, "CC %d (%s) accepts %d..%d, not %d\n", cc, spec->name,
                      spec->minValue, spec->maxValue, value);
        return 2;
    }

    std::string error;
    DeviceSession session;
    if (! session.open (error)) { std::fprintf (stderr, "cannot open the Nymphes port: %s\n", error.c_str()); return 1; }
    if (! session.port().sendCC (channel, cc, value)) { std::fprintf (stderr, "send failed\n"); return 1; }
    std::printf ("sent      CC %d (%s) = %d on channel %d\n", cc, spec->name, value, channel + 1);
    return 0;
}

int cmdPanic (const Options& o)
{
    const int channel = o.getInt ("--channel", 1) - 1;
    std::string error;
    DeviceSession session;
    if (! session.open (error)) { std::fprintf (stderr, "cannot open the Nymphes port: %s\n", error.c_str()); return 1; }
    session.port().sendCC (channel, 123, 0);
    session.port().sendCC (channel, 120, 0);
    std::printf ("sent      all-sound-off + all-notes-off on channel %d\n", channel + 1);
    return 0;
}

} // namespace

#if defined(NYMPHES_HAS_FLOCK)
namespace
{
/** Exclusive advisory lock on the fleet's MIDI lock file, taken in-process.

    The brief asks for `flock(1)` around every device session, but macOS ships no
    `flock` binary (verified on this host), so shelling out is not available. This
    takes the same lock with flock(2) on the same file, which is what every other
    process in this fleet does. It is non-blocking on purpose: a busy lock means
    another lane is mid-session and the right answer is to say so, not to queue.

    The lock is taken before the MIDI port is opened and released when this object
    dies, i.e. when the process exits. */
class MidiLock
{
public:
    ~MidiLock()
    {
        if (fd_ >= 0) { ::flock (fd_, LOCK_UN); ::close (fd_); }
    }

    bool acquire (const std::string& path, std::string& error)
    {
        fd_ = ::open (path.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd_ < 0) { error = "cannot open lock file " + path; return false; }
        if (::flock (fd_, LOCK_EX | LOCK_NB) != 0)
        {
            error = "the MIDI lock is held by another process (" + path + ")";
            ::close (fd_);
            fd_ = -1;
            return false;
        }
        return true;
    }

private:
    int fd_ = -1;
};
} // namespace
#endif

int main (int argc, char** argv)
{
    if (argc < 2) { usage(); return 2; }

    const std::string cmd = argv[1];
    const Options o = parse (argc, argv, 2);

    // Device sessions must hold the fleet lock. `devices` only enumerates
    // endpoints, so it needs none; everything that opens the port does.
    const bool isDeviceCommand = cmd == "read" || cmd == "write" || cmd == "capture"
                                 || cmd == "dump-request" || cmd == "cc" || cmd == "panic";
#if defined(NYMPHES_HAS_FLOCK)
    MidiLock lock;
    if (isDeviceCommand)
    {
        const std::string lockPath = o.get ("--lock");
        if (lockPath.empty())
        {
            std::fprintf (stderr,
                "%s: device commands need --lock <file>; every MIDI session must hold an\n"
                "exclusive lock. The brief says `flock(1)`, but macOS has no flock binary,\n"
                "so this takes the same lock in-process:\n"
                "  nymphes-cli %s %s --lock vendor/midi.lock\n",
                cmd.c_str(), cmd.c_str(),
                o.positional.empty() ? "" : o.positional.front().c_str());
            return 2;
        }
        std::string lockError;
        if (! lock.acquire (lockPath, lockError))
        {
            std::fprintf (stderr, "%s\n", lockError.c_str());
            return 3;
        }
    }
#else
    if (isDeviceCommand)
    {
        std::fprintf (stderr, "%s: this build has no file locking; refusing a device session\n",
                      cmd.c_str());
        return 2;
    }
#endif

    if (cmd == "devices")      return cmdDevices();
    if (cmd == "ccmap")        return cmdCCMap();
    if (cmd == "verify")       return cmdVerify (o);
    if (cmd == "list")         return cmdList (o);
    if (cmd == "show")         return cmdShow (o);
    if (cmd == "extract")      return cmdExtract (o);
    if (cmd == "split")        return cmdSplit (o);
    if (cmd == "merge")        return cmdMerge (o);
    if (cmd == "txt")          return cmdTxt (o);
    if (cmd == "export-txt")   return cmdExportTxt (o);
    if (cmd == "read")         return cmdRead (o);
    if (cmd == "write")        return cmdWrite (o);
    if (cmd == "capture")      return cmdCapture (o);
    if (cmd == "dump-request") return cmdDumpRequest (o);
    if (cmd == "cc")           return cmdCC (o);
    if (cmd == "panic")        return cmdPanic (o);
    if (cmd == "--help" || cmd == "-h" || cmd == "help") { usage(); return 0; }

    std::fprintf (stderr, "unknown command: %s\n\n", cmd.c_str());
    usage();
    return 2;
}
