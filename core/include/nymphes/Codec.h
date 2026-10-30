// nymphes/Codec.h - Dreadbox Nymphes preset codec (canonical, shipped).
//
// Built and owned by lane L4 after adopted votes 4a6410d80fc5 (A) / f066755d8590:
// this file is THE preset codec for the VST3, the Standalone app and the headless CLI.
// It is dependency-free C++17 and does not include JUCE.
//
// Wire format (verified against the vendor dumps by this implementation, 147/147
// byte-identical, see core/tests/test_codec.cpp):
//
//   preset frame : F0 00 21 35 00 06 FF AA BB PP <2098 nibble bytes> F7   (2109 bytes)
//   FF           : 0x00 = load/audition without saving, 0x01 = write to memory
//   AA           : 0x00 = USER page, 0x01 = FACTORY page
//   BB           : bank 1..7 (A..G)
//   PP           : program 1..7
//   nibble order : byte = n[2k+1] << 4 | n[2k]      (low nibble first)
//   payload      : 1049 bytes = CRC-8(payload[1..1048]) + 1048 protobuf bytes
//   CRC-8        : polynomial 0x31, initial value 0x00, no reflection, no final xor
//   full dump req: F0 00 21 35 00 06 02 F7
//
// Safety rule enforced by this API: a preset addressed to the FACTORY page can never
// be serialised with the write flag set unless the caller passes an explicit override
// that names the slot. Factory presets are read-only by default (brief hard constraint).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nymphes {

using Bytes = std::vector<std::uint8_t>;

// ------------------------------------------------------------------ constants
// The 1049-byte payload is the shape of the two VENDOR dumps and of the sysEx the
// manual documents. It is NOT a wire invariant: a live V2.1 unit returns payloads from
// 1049 up to ~1156 bytes for the same 22 top-level fields, because (a) the four optional
// reverb floats in fields 18..21 are present in some presets and absent in others, and
// (b) chord_info int32s may be NEGATIVE and then take a 10-byte varint instead of 1 byte.
// Never reject a frame for its length alone, and never assume a fixed frame size when
// receiving - see Preset::payloadSize() and evidence/L4-device/device_dump_*.syx.
inline constexpr std::size_t kPayloadSize = 1049;   // canonical/vendor payload size
inline constexpr std::size_t kProtoSize = 1048;     // canonical protobuf size
inline constexpr std::size_t kFrameSize = 2109;     // canonical F0..F7 frame size
inline constexpr std::size_t kMinPayloadSize = 32;    // sanity floor for a received frame
inline constexpr std::size_t kMaxPayloadSize = 4096;  // sanity ceiling for a received frame
inline constexpr std::uint8_t kManufacturer[5] = {0x00, 0x21, 0x35, 0x00, 0x06};
inline constexpr std::uint8_t kPageUser = 0x00;
inline constexpr std::uint8_t kPageFactory = 0x01;
inline constexpr std::uint8_t kFlagLoad = 0x00;
inline constexpr std::uint8_t kFlagWrite = 0x01;

// ------------------------------------------------------------------ low level
std::uint8_t crc8(const std::uint8_t* data, std::size_t n);

// 2098 nibble bytes -> 1049 payload bytes (low nibble first).
bool unpackNibbles(const std::uint8_t* nib, std::size_t n, Bytes& payload);
// 1049 payload bytes -> 2098 nibble bytes.
Bytes packNibbles(const Bytes& payload);

// ------------------------------------------------------------------ protobuf
enum class Wire : std::uint8_t { Varint = 0, Fixed64 = 1, Length = 2, Fixed32 = 5 };

struct PbField {
    std::uint32_t number = 0;
    Wire wire = Wire::Varint;
    std::uint64_t varint = 0;   // wire == Varint
    Bytes bytes;                // wire == Length (raw payload) or Fixed32/Fixed64 (LE bytes)
    bool hasSub = false;        // true when a Length payload parsed as a submessage
    std::vector<PbField> sub;   // decoded submessage fields
    const PbField* subFind(std::uint32_t n) const;
    PbField* subFind(std::uint32_t n);
};

struct PbMessage {
    std::vector<PbField> fields;
    const PbField* find(std::uint32_t number) const;
    PbField* find(std::uint32_t number);
};

// Canonical protobuf parse. `depth` bounds recursion (0 = do not recurse).
bool pbParse(const std::uint8_t* data, std::size_t n, PbMessage& out, int depth = 1);
Bytes pbEncode(const PbMessage& msg);

// ------------------------------------------------------------------ addressing
struct Address {
    std::uint8_t page = kPageUser;    // kPageUser | kPageFactory
    std::uint8_t bank = 1;            // 1..7 -> A..G
    std::uint8_t program = 1;         // 1..7

    bool isUser() const { return page == kPageUser; }
    // "userD5" / "factoryA1"
    std::string name() const;
    static std::optional<Address> parse(const std::string& s);
    bool operator==(const Address& o) const {
        return page == o.page && bank == o.bank && program == o.program;
    }
};

// ------------------------------------------------------------------ sections
// Top-level protobuf field numbers of the `preset` message.
enum TopField : std::uint32_t {
    kMain = 1, kReverb = 2, kLfo2 = 3, kModWheel = 4, kVelocity = 5, kAftertouch = 6,
    kLfoSettings = 7, kLegato = 8, kVoiceMode = 9,
    kChord1 = 10, kChord2, kChord3, kChord4, kChord5, kChord6, kChord7, kChord8,
    kExtraLfo2 = 18, kExtraModWheel = 19, kExtraVelocity = 20, kExtraAftertouch = 21,
    kAmpLevel = 22,
};

// The 28 float slots, in field order, shared by the main / lfo_2 / mod_w / velo /
// after sections (`basic_inputs`). Names are the verified schema's own names.
enum BasicParam : std::uint32_t {
    kWave = 1, kLvl, kSub, kNoise, kOscLfo, kCut, kReson, kCutEg,
    kA1, kD1, kS1, kR1, kLfoRate, kLfoWave, kPw, kGlide, kDtune, kChordAmount,
    kOscEg, kHpf, kTrack, kCutLfo, kA2, kD2, kS2, kR2, kLfoDelay, kLfoFade,
    kBasicCount = 28,
};
// Human-readable name for a basic_inputs slot (1..28), nullptr if out of range.
const char* basicName(std::uint32_t slot);
// Human-readable name for a top-level field, nullptr if unknown.
const char* topName(std::uint32_t field);
// Reverb slots (field 2 submessage): size, decay, filter, mix.
enum ReverbParam : std::uint32_t { kReverbSize = 1, kReverbDecay, kReverbFilter, kReverbMix };

// ------------------------------------------------------------------ preset
class Preset {
public:
    Preset() = default;

    // ---- decode / encode
    // A whole 2109-byte SysEx frame (with or without the F0/F7 bounds - both accepted
    // when `hasBounds` is true; pass the frame exactly as it appears on the wire).
    static bool decodeFrame(const std::uint8_t* data, std::size_t n, Preset& out,
                            std::string* err = nullptr);
    static std::optional<Preset> decodeFrame(const Bytes& frame, std::string* err = nullptr);

    // Serialise as a 2109-byte frame. Returns an empty vector (and sets err) if the
    // target slot is a factory slot and `persist` would set the write flag, unless
    // `allowFactoryWrite` names the intent explicitly.
    Bytes encodeFrame(bool persist, bool allowFactoryWrite = false,
                      std::string* err = nullptr) const;

    // Exact frame as it came off the wire / is stored in a .syx, flag byte preserved.
    // Use this for file I/O and for byte-identity checks against the vendor dumps.
    Bytes encodeFrameRaw() const;
    std::uint8_t flag() const { return flag_; }
    void setFlag(std::uint8_t f) { flag_ = f; }

    // Serialise for audition: always flag 0x00, never modifies the device memory.
    Bytes encodeAudition() const;

    // ---- payload access (the ground truth; byte identity is defined on this)
    const Bytes& payload() const { return payload_; }      // [0] = CRC, then protobuf
    std::size_t payloadSize() const { return payload_.size(); }
    std::size_t frameSize() const { return 11 + 2 * payload_.size(); }
    bool crcValid() const;
    void setPayload(Bytes p);                              // recomputes/validates nothing
    void seal();                                           // recompute CRC over [1..1048]

    // ---- address
    const Address& address() const { return addr_; }
    void setAddress(const Address& a) { addr_ = a; }

    // ---- typed accessors (all bounds-checked; setters mark the preset edited)
    bool hasSection(std::uint32_t top) const;
    float basic(std::uint32_t top, std::uint32_t slot) const;             // 1..28
    bool setBasic(std::uint32_t top, std::uint32_t slot, float value);
    float reverb(std::uint32_t slot) const;                               // 1..4
    bool setReverb(std::uint32_t slot, float value);
    float extra(std::uint32_t top, std::uint32_t slot) const;             // 18..21 containers
    bool setExtra(std::uint32_t top, std::uint32_t slot, float value);
    float ampLevel() const;
    bool setAmpLevel(float v);

    std::uint64_t chordInfo(std::uint32_t chord, std::uint32_t slot) const;  // chord 1..8, slot 1..6
    bool setChordInfo(std::uint32_t chord, std::uint32_t slot, std::uint64_t v);
    std::uint64_t lfoSetting(std::uint32_t slot) const;                   // slot 1..4
    bool setLfoSetting(std::uint32_t slot, std::uint64_t v);
    bool legato() const;
    bool setLegato(bool on);
    std::uint64_t voiceMode() const;                                      // 0..5
    bool setVoiceMode(std::uint64_t mode);

    // ---- file formats
    // .syx : one or more frames concatenated (what the device and the vendor dumps use).
    static bool readSyx(const std::string& path, std::vector<Preset>& out, std::string* err = nullptr);
    bool writeSyxAppend(const std::string& path, std::string* err = nullptr) const;
    static bool writeSyx(const std::string& path, const std::vector<Preset>& presets,
                         std::string* err = nullptr);
    static std::vector<Bytes> splitSyx(const Bytes& blob);

    // .txt : human-readable and losslessly round-trippable. `payload=` carries the exact
    // bytes; a per-parameter line that disagrees with the payload is applied as an edit.
    std::string toText() const;
    static bool fromText(const std::string& text, Preset& out, std::string* err = nullptr);

    // True when re-encoding the parsed tree reproduces payload() byte for byte.
    bool isCanonical() const { return canonical_; }

private:
    Address addr_{};
    Bytes payload_;          // 1049
    PbMessage msg_;
    bool canonical_ = true;
    bool edited_ = true;     // recompute on every encode unless false
    Bytes original_;         // payload as decoded
    std::uint8_t flag_ = kFlagLoad;  // FF byte as decoded
};

}  // namespace nymphes
