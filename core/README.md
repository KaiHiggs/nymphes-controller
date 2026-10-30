# `core/` — Nymphes preset codec (canonical, shipped)

Owner: lane **L4**. Adopted by fleet votes `4a6410d80fc5` (option A) and `f066755d8590`:
**one shipped C++ codec**. L2's Python and any other implementation are non-shipping
oracles. This directory is the one the VST3, the Standalone app and the headless CLI
must link.

* Dependency-free C++17. **Does not include JUCE.** No exceptions in normal paths, no RTTI
  requirement, thread-compatible (no globals, all state in `Preset`).
* Two files, both yours to compile into whatever target you have:
  `core/src/Codec.cpp`, `core/include/nymphes/Codec.h`.

```
clang++ -std=c++17 -O2 -Wall -Wextra -Icore/include \
        core/src/Codec.cpp core/tests/test_codec.cpp -o build/test_codec
./build/test_codec   # exits 0 with "1083 checks passed, 0 failed"
```

Current result: **1083 checks passed, 0 failed** — including all **147** vendor frames
(49 `Factory_Bank` + 98 `User_Bank`) decoded, re-encoded **byte for byte**, and round-tripped
through `.txt` and `.syx`. If you change this codec, that number is the acceptance gate.

## Wire format (what the codec implements)

```
F0 00 21 35 00 06 | FF AA BB PP | 2098 nibble bytes | F7        2109 bytes, always
FF  0x00 load/audition (never writes memory)   0x01 write to memory
AA  0x00 USER page                             0x01 FACTORY page
BB  bank 1..7 (A..G)        PP  program 1..7
nibble order:  byte = n[2k+1] << 4 | n[2k]        (low nibble first)
payload     :  1049 B = CRC-8(payload[1..1048]) + 1048 B nanopb protobuf
CRC-8       :  poly 0x31, init 0x00, no reflection, no final xor
full-dump request: F0 00 21 35 00 06 02 F7
```

## API surface (`#include "nymphes/Codec.h"`, namespace `nymphes`)

```cpp
inline constexpr std::size_t kPayloadSize = 1049;   // CRC + protobuf
inline constexpr std::size_t kFrameSize   = 2109;
inline constexpr std::uint8_t kPageUser = 0x00, kPageFactory = 0x01;
inline constexpr std::uint8_t kFlagLoad = 0x00, kFlagWrite  = 0x01;

std::uint8_t crc8(const std::uint8_t*, std::size_t);

struct Address { std::uint8_t page, bank /*1..7*/, program /*1..7*/;
                 bool isUser() const; std::string name() const;              // "userD5"
                 static std::optional<Address> parse(const std::string&); };

class Preset {
  // decode / encode
  static bool decodeFrame(const std::uint8_t* d, std::size_t n, Preset& out, std::string* err = nullptr);
  static std::optional<Preset> decodeFrame(const Bytes& frame, std::string* err = nullptr);
  Bytes encodeFrame(bool persist, bool allowFactoryWrite = false, std::string* err = nullptr) const;
  Bytes encodeFrameRaw() const;      // exact frame, flag byte preserved (file I/O, byte identity)
  Bytes encodeAudition() const;      // flag 0x00 always

  // payload is the ground truth: 1049 bytes, [0] = CRC
  const Bytes& payload() const;  bool crcValid() const;  void seal();
  bool isCanonical() const;          // tree re-encodes to the same bytes

  const Address& address() const;  void setAddress(const Address&);
  std::uint8_t flag() const;       void setFlag(std::uint8_t);

  // typed views (bounds-checked; setters recompute the CRC and re-serialise)
  float basic(std::uint32_t topField, std::uint32_t slot) const;   // slot 1..28
  bool  setBasic(std::uint32_t topField, std::uint32_t slot, float);
  float reverb(std::uint32_t slot 1..4) const;   bool setReverb(...);
  float extra(std::uint32_t topField 18..21, std::uint32_t slot) const;  bool setExtra(...);
  float ampLevel() const;          bool setAmpLevel(float);
  std::uint64_t chordInfo(std::uint32_t chord 1..8, std::uint32_t slot 1..6) const;
  bool setChordInfo(std::uint32_t chord, std::uint32_t slot, std::uint64_t);
  std::uint64_t lfoSetting(std::uint32_t slot 1..4) const;
  bool setLfoSetting(std::uint32_t slot, std::uint64_t);
  bool legato() const;             bool setLegato(bool);
  std::uint64_t voiceMode() const; bool setVoiceMode(std::uint64_t);

  // files
  static bool readSyx(const std::string& path, std::vector<Preset>& out, std::string* err = nullptr);
  static bool writeSyx(const std::string& path, const std::vector<Preset>&, std::string* err = nullptr);
  bool writeSyxAppend(const std::string& path, std::string* err = nullptr) const;
  static std::vector<Bytes> splitSyx(const Bytes& blob);
  std::string toText() const;
  static bool fromText(const std::string&, Preset& out, std::string* err = nullptr);
};

// generic protobuf (lossless, byte-exact re-encode; preserves unknown fields)
enum class Wire { Varint = 0, Fixed64 = 1, Length = 2, Fixed32 = 5 };
struct PbField { std::uint32_t number; Wire wire; std::uint64_t varint; Bytes bytes;
                 bool hasSub; std::vector<PbField> sub;
                 const PbField* subFind(std::uint32_t) const; PbField* subFind(std::uint32_t); };
struct PbMessage { std::vector<PbField> fields;
                   const PbField* find(std::uint32_t) const; PbField* find(std::uint32_t); };
bool  pbParse(const std::uint8_t*, std::size_t, PbMessage&, int depth = 1);
Bytes pbEncode(const PbMessage&);
```

### Top-level field constants (use these, not magic numbers)

| constant | field | contents |
|---|---|---|
| `kMain` | 1 | `basic_inputs`, 28 fixed32 floats |
| `kReverb` | 2 | 4 fixed32 floats: size, decay, filter, mix |
| `kLfo2` | 3 | `basic_inputs` (28 floats) |
| `kModWheel` | 4 | `basic_inputs` |
| `kVelocity` | 5 | `basic_inputs` |
| `kAftertouch` | 6 | `basic_inputs` |
| `kLfoSettings` | 7 | 4 varints: lfo_1 speed mode, lfo_1 sync, lfo_2 speed mode, lfo_2 sync |
| `kLegato` | 8 | varint bool |
| `kVoiceMode` | 9 | varint 0..5 (poly, uni_6, uni_4, uni_3, uni_2, mono) |
| `kChord1..kChord8` | 10..17 | 6 varints each: root, semi_1..semi_5 |
| `kExtraLfo2` | 18 | 12 fixed32 floats |
| `kExtraModWheel` / `kExtraVelocity` / `kExtraAftertouch` | 19..21 | 8 fixed32 floats each |
| `kAmpLevel` | 22 | fixed32 float |

`basicName(slot)` returns the schema name for slots 1..28
(`wave, lvl, sub, noise, osc_lfo, cut, reson, cut_eg, a1, d1, s1, r1, lfo_rate, lfo_wave,
pw, glide, dtune, chord, osc_eg, hpf, track, cut_lfo, a2, d2, s2, r2, lfo_delay, lfo_fade`).

## Safety rules the codec enforces

1. **Factory presets are read-only.** `encodeFrame(persist=true)` returns an empty vector and
   an error when `address().page == kPageFactory`, unless `allowFactoryWrite=true` is passed
   explicitly. `encodeAudition()` can never set the write flag.
2. A frame is only produced from a payload that is exactly 1049 bytes; the CRC is
   recomputed on every edit, so no caller can emit a frame the device will reject.
3. `decodeFrame` rejects: wrong length, missing `F0`/`F7`, wrong manufacturer/product ID,
   page/bank/program out of range, any nibble above `0x0F`, a CRC mismatch, or a protobuf
   parse failure — each with a specific message.

## `.txt` format

```
nymphes-preset 1
address=userD5
flag=0
payload=<2098 hex chars: the 1049 payload bytes>
main.wave=0.235869989
main.lvl=1
...
reverb.size=0.326779991
extra_lfo_2.f1=...
lfo_settings.s1=0
legato=0
voice_mode=0
chord1.i1=0
amp_level=1
```

`payload=` is authoritative: importing an untouched export reproduces the original frame
**byte for byte** (the parameter lines are compared against the payload and only lines that
actually differ are applied as edits). Floats are printed with `%.9g`, which round-trips
float32 exactly.

## For the other lanes

* **L3 (AudioProcessor / CoreMIDI / CC engine):** link `Codec.cpp`, drop your own
  `preset/Preset.{h,cpp}` codec in favour of this one. `Preset::encodeFrame(true)` is your
  device write; `encodeAudition()` is your audition; `encodeFrameRaw()` is what you write
  into a `.syx`. For MIDI-out you need the frame **without** the `F0`/`F7` bounds in most
  APIs (CoreMIDI `MIDIPacketList` wants the full `F0..F7`; `mido`/rtmidi want it without) —
  every frame here is the full `F0..F7` form.
* **L2 (CLI + `params.json`):** `Preset::readSyx` / `writeSyx` / `toText` / `fromText`
  cover import/export; `basic(set)` + the `TopField` constants are the parameter surface.
  I do not write the CLI or the CC table — those stay yours.
* **L1 (JUCE UI / CMake / REPORT / install):** add `core/src/Codec.cpp` to the
  `nymphes_core` target with `core/include` on the include path; there is nothing to
  configure and no third-party dependency.
