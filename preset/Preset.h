// nymphes::Preset — the fleet-facing preset API.
//
// IMPORTANT — where the codec actually lives
// ------------------------------------------
// The shipped codec is L4's, vendored verbatim at `core/` in this tree
// (core/include/nymphes/Codec.h + core/src/Codec.cpp). Fleet votes 4a6410d80fc5
// (option A) and f066755d8590 adopted it as the single shipped implementation, so
// this header contains NO frame, nibble, CRC or protobuf logic of its own: every
// function below delegates to `nymphes::Preset` in core/. It exists so that the
// VST3, the Standalone app, the CLI and the tests all speak one stable vocabulary
// (the one published to L1 for the editor), and so the editor can ask for named
// parameter values without knowing the protobuf layout.
//
// Vendored copies, byte-identical to L4's tree:
//   core/include/nymphes/Codec.h   sha256 558a854f45bbedd8a43dba724c6b085c18387b83c5f131994a00485023aa18ba
//   core/src/Codec.cpp             sha256 1d48da67fb6dedb574b98ac893119f6b9534a025f7eb6eb4bdf14198fddfe6fe
//   core/tests/test_codec.cpp      sha256 bee9d814a1f0f9bce85dced27d10f111c8074d4d5e9efc6245d2cd987393291b
// Re-vendor rather than patch: if the codec must change, it changes in L4's file
// and every consumer takes it at once.
//
// What that codec does, so a reader of this file has the wire format in front of
// them (all of it verified against the 147 vendor preset messages):
//
//   F0 00 21 35 00 06 | FF | AA | BB | PP | <2n nibble bytes> | F7
//   FF 0x00 load/audition, 0x01 write to memory
//   AA 0x00 USER page, 0x01 FACTORY page
//   BB bank 1..7 (A..G), PP program 1..7
//   payload[k] = n[2k+1] << 4 | n[2k]        (low nibble first)
//   payload[0] = CRC-8 over payload[1..], poly 0x31, init 0x00, no reflect,
//                no final xor; payload[1..] = nanopb protobuf, 22 top-level fields
//
// The vendor dumps are 1049-byte payloads (2109-byte frames) but the live device
// emits payloads from 1049 up to ~1156 bytes for the same 22 fields, because the
// four optional reverb floats in fields 18..21 are sometimes absent and a negative
// chord_info varint takes 10 bytes instead of 1. Never assume a fixed length on a
// received frame — use payloadSize()/frameSize().

#pragma once

#include "nymphes/Codec.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nymphes
{

// Convenience constants mirroring core/Codec.h so call sites that were written
// against the older naming keep working.
inline constexpr int kPayloadBytes = static_cast<int> (kPayloadSize);   // 1049, canonical
inline constexpr int kFrameBytes   = static_cast<int> (kFrameSize);     // 2109, canonical
inline constexpr int kHeaderBytes  = 10;
inline constexpr int kSlotsPerPage = 49;                                // 7 banks x 7 programs

enum class WriteMode : uint8_t { LoadOnly = kFlagLoad, WriteToMemory = kFlagWrite };
enum class Page : uint8_t { User = kPageUser, Factory = kPageFactory };

/** BB/PP as the UI shows them: bank letter A..G, program 1..7. */
struct Slot
{
    uint8_t bank    = 1;
    uint8_t program = 1;

    char bankLetter() const noexcept { return static_cast<char> ('A' + (bank - 1)); }
    std::string toString() const;
    static bool fromString (const std::string&, Slot&);      // "A1", "a7", " C 3 "
    bool isValid() const noexcept { return bank >= 1 && bank <= 7 && program >= 1 && program <= 7; }
    int linearIndex() const noexcept { return (bank - 1) * 7 + (program - 1); }

    static Slot fromAddress (const Address&);
    Address toAddress (Page) const;
};

// ---------------------------------------------------------------------------
// The preset itself IS core's type. No wrapper struct, no second payload buffer.
// ---------------------------------------------------------------------------
using Preset = ::nymphes::Preset;

/** "USER A1" / "FACTORY C3". The payload carries no name: the protobuf has 22
    fields and none of them is a string, so a preset is identified by its slot. */
std::string displayName (const Preset&);

// ---------------------------------------------------------------------------
// Frame I/O — all four delegate to core.
// ---------------------------------------------------------------------------

/** Decode one frame exactly as it appears on the wire (F0..F7). */
bool decodeFrame (const uint8_t* data, size_t size, Preset& out, std::string* error = nullptr);

/** Re-emit the frame that was decoded, flag byte preserved: byte-identical. */
std::vector<uint8_t> encodeFrame (const Preset&);

/** Emit for the device: `persist` chooses FF=0x01 (write to memory) vs FF=0x00
    (audition). A factory slot with persist=true is refused unless
    `allowFactoryWrite` is set, which nothing in this tree does. */
std::vector<uint8_t> encodeForDevice (const Preset&, bool persist,
                                      bool allowFactoryWrite = false, std::string* error = nullptr);

std::vector<Preset> importSyxFile (const std::string& path, std::string& error);
bool exportSyxFile (const std::string& path, const std::vector<Preset>& presets, std::string& error);

/** Split a raw blob into frames (used for capture streams). */
std::vector<std::vector<uint8_t>> splitSyx (const std::vector<uint8_t>& blob);

// ---------------------------------------------------------------------------
// Named parameter view — what the editor's parameter panel and `nymphes-cli show`
// need, without either of them knowing a protobuf field number.
// ---------------------------------------------------------------------------

enum class Group : int
{
    Main = kMain, Reverb = kReverb, Lfo2 = kLfo2, ModWheel = kModWheel,
    Velocity = kVelocity, Aftertouch = kAftertouch, LfoSettings = kLfoSettings,
    Legato = kLegato, VoiceMode = kVoiceMode,
    Chord1 = kChord1, Chord2 = kChord2, Chord3 = kChord3, Chord4 = kChord4,
    Chord5 = kChord5, Chord6 = kChord6, Chord7 = kChord7, Chord8 = kChord8,
    ExtraLfo2 = kExtraLfo2, ExtraModWheel = kExtraModWheel,
    ExtraVelocity = kExtraVelocity, ExtraAftertouch = kExtraAftertouch,
    AmpLevel = kAmpLevel
};

struct FieldView
{
    Group       group;
    int         subField;      // 1-based field number inside the section
    const char* name;          // schema name, e.g. "cut", "amp_level"
    float       value;
    bool        discrete;      // UI hint: render as an enum/integer picker
};

/** Every named parameter present in the preset, in a stable order. Sections the
    payload does not carry are omitted rather than defaulted, because "absent" and
    "zero" are different presets on this device. */
std::vector<FieldView> describe (const Preset&);

bool getFloat (const Preset&, Group, int subField, float& out);
bool setFloat (Preset&, Group, int subField, float value);
bool getInt (const Preset&, Group, int subField, int64_t& out);
bool setInt (Preset&, Group, int subField, int64_t value);

/** One-line description used by the CLI and the browser list. */
std::string summaryLine (const Preset&);

/** Voice-mode names, index 0..5 (poly, uni-6, uni-4, uni-3, uni-2, mono). */
const char* voiceModeName (uint64_t mode);

} // namespace nymphes
