// tests/codec_text_test.cpp — L3
//
// Regression tests for the TEXT and ADDRESS half of the shipping codec
// (core/ via nymphes/Codec.h). No vendor corpus and no device: every payload here
// is built with the codec's own API, so this runs everywhere and proves:
//
//   * Preset::fromText honours its bool/err contract: malformed numbers, a bad
//     flag byte, an unknown key, an out-of-range index, a bad CRC or an oversized
//     payload are REJECTED, and none of them throws out of the API;
//   * Address::parse rejects an address whose program does not fit 1..7 instead of
//     overflowing `int` on the way to the check;
//   * chord/slot accessors are bounds-checked, so chord 9 cannot alias extra_lfo_2;
//   * isCanonical() means "re-encoding reproduces the payload", not "the parse
//     succeeded" (setPayload must report a non-canonical payload as such);
//   * seal() covers the WHOLE payload (payload[1..size-1]), not a fixed 1048 bytes;
//   * encodeFrameRaw() keeps the on-wire flag byte (that is its documented job) and
//     the factory read-only rule lives in encodeFrame(persist, ...), which must
//     refuse a FACTORY write no matter what setFlag() stored.

#include "nymphes/Codec.h"

#include <cstdio>
#include <exception>
#include <string>
#include <vector>

using namespace nymphes;

static int gFail = 0, gPass = 0;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        if (cond) {                                         \
            ++gPass;                                        \
        } else {                                            \
            ++gFail;                                        \
            std::printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            std::printf(__VA_ARGS__);                       \
            std::printf("\n");                              \
        }                                                   \
    } while (0)

static std::string hexOf(const Bytes& b) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (std::uint8_t x : b) {
        s += d[x >> 4];
        s += d[x & 0x0F];
    }
    return s;
}

// A payload-only preset: kMinPayloadSize zero bytes with a valid CRC, no sections.
static Preset emptyPreset() {
    Preset p;
    p.setPayload(Bytes(kMinPayloadSize, 0));
    p.seal();
    return p;
}

// A preset carrying a real extra_lfo_2 section (protobuf field 18, 12 fixed32
// slots). chord = 9 maps to field 18, so this is what an unchecked chord index
// would silently overwrite.
static Preset presetWithExtraLfo() {
    PbMessage msg;
    PbField extra;
    extra.number = kExtraLfo2;
    extra.wire = Wire::Length;
    extra.hasSub = true;
    for (std::uint32_t slot = 1; slot <= 12; ++slot) {
        PbField f;
        f.number = slot;
        f.wire = Wire::Fixed32;
        f.bytes.assign(4, 0);
        extra.sub.push_back(f);
    }
    msg.fields.push_back(extra);
    const Bytes body = pbEncode(msg);
    Bytes payload(1, 0);
    payload.insert(payload.end(), body.begin(), body.end());
    payload[0] = crc8(payload.data() + 1, payload.size() - 1);
    Preset p;
    p.setPayload(std::move(payload));
    return p;
}

// Append a line to a preset's text. `flag`, `address` and `payload` are parsed in
// the header loop, so a later duplicate line wins; the kv line just adds an edit.
static std::string withLine(const std::string& text, const std::string& line) {
    return text + line + "\n";
}

// fromText must return false + err, never throw.
static void rejects(const std::string& text, const char* what) {
    Preset q;
    std::string e;
    bool ok = false;
    try {
        ok = Preset::fromText(text, q, &e);
    } catch (const std::exception& ex) {
        CHECK(false, "%s: fromText threw '%s' instead of returning false", what, ex.what());
        return;
    } catch (...) {
        CHECK(false, "%s: fromText threw instead of returning false", what);
        return;
    }
    CHECK(!ok, "%s: malformed text was accepted", what);
    CHECK(!e.empty(), "%s: rejected without an error message", what);
}

int main() {
    const Preset base = emptyPreset();
    const std::string text = base.toText();

    // ---- 0. the text format still round-trips (this is the gate every fix below
    //         must not break: toText -> fromText -> identical payload)
    {
        Preset q;
        std::string e;
        CHECK(Preset::fromText(text, q, &e), "toText->fromText failed: %s", e.c_str());
        CHECK(q.payload() == base.payload(), "text round trip changed the payload");
    }

    // ---- 1. malformed text is rejected through err, not by throwing
    rejects(withLine(text, "flag=abc"), "flag=abc");
    rejects(withLine(text, "flag=9"), "flag=9");
    rejects(withLine(text, "flag=-1"), "flag=-1");
    rejects(withLine(text, "voice_mode=abc"), "voice_mode=abc");
    rejects(withLine(text, "voice_mode=99999999999999999999999999"), "voice_mode=overflow");
    rejects(withLine(text, "amp_level=abc"), "amp_level=abc");
    rejects(withLine(text, "legato=abc"), "legato=abc");
    rejects(withLine(text, "chord1.i1=abc"), "chord1.i1=abc");
    rejects(withLine(text, "chord1.i1=7"), "chord1.i1=7 (no chord section)");
    rejects(withLine(text, "chord9.i1=7"), "chord9.i1=7 (index out of range)");
    rejects(withLine(text, "chord2.i9=7"), "chord2.i9=7 (slot out of range)");
    rejects(withLine(text, "lfo_settings.s9=7"), "lfo_settings.s9=7 (slot out of range)");
    rejects(withLine(text, "main.cut=abc"), "main.cut=abc");
    rejects(withLine(text, "main.cut=0.5"), "main.cut=0.5 (no main section)");
    rejects(withLine(text, "nosuchkey=1"), "nosuchkey=1 (unknown key)");
    rejects(withLine(text, "main.nosuchparam=1"), "main.nosuchparam=1 (unknown parameter)");

    // a corrupted payload= hex (CRC-8 byte flipped) must not import as a valid preset
    {
        Bytes bad = base.payload();
        bad[0] = static_cast<std::uint8_t>(bad[0] ^ 0xFF);
        rejects(withLine(text, "payload=" + hexOf(bad)), "bad CRC in payload=");
    }
    // an oversized payload= must be refused (the wire format has a sane ceiling)
    {
        Bytes huge(kMaxPayloadSize + 1, 0);
        huge[0] = crc8(huge.data() + 1, huge.size() - 1);
        rejects(withLine(text, "payload=" + hexOf(huge)), "payload= above kMaxPayloadSize");
    }

    // ---- 2. Address::parse must not overflow `int` reaching the 1..7 check.
    // 4294967300 == 2^32 + 4: the digit loop wraps to program 4 before the range
    // check if the accumulation is unchecked.
    {
        CHECK(!Address::parse("userA4294967300"), "userA4294967300 must be rejected");
        CHECK(!Address::parse("userA2147483648"), "userA2147483648 must be rejected");
        CHECK(!Address::parse("userA" + std::string(40, '9')), "40-digit program must be rejected");
        auto ok = Address::parse("userA7");
        CHECK(ok && ok->bank == 1 && ok->program == 7, "userA7 must still parse");
    }

    // ---- 3. chord/slot accessors are bounds-checked. Field 18 is extra_lfo_2, so an
    // unchecked chord 9 reaches a submessage that is not a chord at all.
    {
        Preset p = presetWithExtraLfo();
        CHECK(p.hasSection(kExtraLfo2), "fixture: extra_lfo_2 present");
        const Bytes before = p.payload();
        CHECK(!p.setChordInfo(9, 1, 7), "setChordInfo(chord 9) must be refused");
        CHECK(!p.setChordInfo(0, 1, 7), "setChordInfo(chord 0) must be refused");
        CHECK(!p.setChordInfo(1, 7, 7), "setChordInfo(slot 7) must be refused");
        CHECK(p.chordInfo(9, 1) == 0, "chordInfo(chord 9) must read 0");
        CHECK(p.payload() == before, "a refused chord write must not touch the payload");
        CHECK(!p.setLfoSetting(0, 1), "setLfoSetting(slot 0) must be refused");
        CHECK(!p.setLfoSetting(5, 1), "setLfoSetting(slot 5) must be refused");
    }

    // ---- 4. isCanonical() means "re-encoding reproduces payload()", not "parsed".
    // 0x40 0x80 0x00 is field 8 / varint 0 encoded with a redundant continuation
    // byte: it parses, and it re-encodes as 0x40 0x00, so it is NOT canonical.
    {
        Bytes payload = {0x00, 0x40, 0x80, 0x00};
        payload[0] = crc8(payload.data() + 1, payload.size() - 1);
        Preset p;
        p.setPayload(payload);
        CHECK(!p.isCanonical(), "setPayload must report a non-canonical payload as non-canonical");

        Bytes ok = {0x00, 0x40, 0x00};
        ok[0] = crc8(ok.data() + 1, ok.size() - 1);
        Preset q;
        q.setPayload(ok);
        CHECK(q.isCanonical(), "setPayload must report a canonical payload as canonical");
    }

    // ---- 5. seal() covers [1 .. payloadSize()-1], not a fixed 1048 bytes.
    {
        Bytes q(40, 0xA5);
        Preset p;
        p.setPayload(q);
        p.seal();
        CHECK(p.crcValid(), "seal() must produce a payload whose CRC verifies");
        CHECK(p.payload()[0] == crc8(p.payload().data() + 1, p.payload().size() - 1),
              "seal() CRC must cover the whole payload body");
        Bytes shortBody(3, 0x11);
        Preset s;
        s.setPayload(shortBody);
        s.seal();
        CHECK(s.crcValid(), "seal() must work below the canonical 1049-byte size");
    }

    // ---- 6. the factory read-only rule is enforced by encodeFrame(persist, ...),
    // and encodeFrameRaw() preserves the wire flag byte by design (file I/O and
    // byte-identity against the vendor dumps depend on it).
    {
        Preset f = base;
        Address a;
        a.page = kPageFactory;
        a.bank = 1;
        a.program = 1;
        f.setAddress(a);
        f.setFlag(kFlagWrite);
        std::string e;
        CHECK(f.encodeFrame(true, false, &e).empty(),
              "encodeFrame(persist=true) must refuse a FACTORY slot");
        CHECK(!e.empty(), "the factory refusal must carry an error message");
        const Bytes raw = f.encodeFrameRaw();
        CHECK(raw.size() == f.frameSize() && raw[6] == kFlagWrite,
              "encodeFrameRaw preserves the flag byte (documented, byte-identity path)");
        CHECK(f.encodeFrame(false, false, &e).size() == f.frameSize(),
              "a FACTORY load/audition frame is allowed");
    }

    std::printf("\n%d checks passed, %d failed\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}
