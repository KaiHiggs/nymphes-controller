// test_codec.cpp - self-test for the nymphes preset codec. No device, no JUCE.
//
//   clang++ -std=c++17 -O2 -Wall -Wextra -Icore/include \
//           core/src/Codec.cpp core/tests/test_codec.cpp -o build/test_codec
//   ./build/test_codec /path/to/vendor
//
// Acceptance gates proved here:
//   1. every frame in both vendor dumps decodes (CRC-8 poly 0x31, init 0)
//   2. re-encoding reproduces each of the 147 frames BYTE FOR BYTE
//   3. a parsed protobuf tree re-serialises canonically (no silent normalisation)
//   4. .txt export/import is lossless and byte-identical
//   5. editing a parameter keeps the frame valid and changes only what was set
//   6. FACTORY writes are refused unless explicitly allowed

#include "nymphes/Codec.h"

#include <cstdio>
#include <cstring>
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

static std::string hexn(const Bytes& b, std::size_t n = 16) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (std::size_t i = 0; i < b.size() && i < n; ++i) {
        s += d[b[i] >> 4];
        s += d[b[i] & 0xF];
    }
    return s;
}

static Bytes rawFrames(const std::string& path) {
    // read the file bytes then split, so we compare against the on-disk frames
    std::vector<Preset> ps;
    std::string err;
    if (!Preset::readSyx(path, ps, &err)) {
        std::printf("FAIL cannot read %s: %s\n", path.c_str(), err.c_str());
        ++gFail;
        return {};
    }
    Bytes out;
    for (auto& p : ps) {
        const Bytes f = p.encodeFrameRaw();
        out.insert(out.end(), f.begin(), f.end());
    }
    return out;
}

static Bytes slurp(const std::string& path) {
    std::vector<Preset> ps;
    std::string err;
    Bytes blob;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return blob;
    std::uint8_t buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) blob.insert(blob.end(), buf, buf + n);
    std::fclose(f);
    return blob;
}

static void testFrame(const Bytes& frame, int index, const char* tag) {
    Preset p;
    std::string err;
    if (!Preset::decodeFrame(frame.data(), frame.size(), p, &err)) {
        ++gFail;
        std::printf("FAIL %s[%d] decode: %s\n", tag, index, err.c_str());
        return;
    }
    ++gPass;
    CHECK(p.crcValid(), "%s[%d] crc invalid", tag, index);
    CHECK(p.isCanonical(), "%s[%d] protobuf tree re-encodes non-canonically", tag, index);
    const Bytes rt = p.encodeFrameRaw();
    CHECK(rt == frame, "%s[%d] re-encode differs (%zu vs %zu bytes) %s",
          tag, index, rt.size(), frame.size(), hexn(rt, 24).c_str());

    // .txt round trip
    const std::string txt = p.toText();
    Preset q;
    std::string e2;
    if (!Preset::fromText(txt, q, &e2)) {
        ++gFail;
        std::printf("FAIL %s[%d] fromText: %s\n", tag, index, e2.c_str());
        return;
    }
    ++gPass;
    CHECK(q.encodeFrameRaw() == frame, "%s[%d] .txt round trip differs (addr %s)", tag, index,
          q.address().name().c_str());
    CHECK(q.payload() == p.payload(), "%s[%d] .txt payload differs", tag, index);
}

int main(int argc, char** argv) {
    // The vendor dumps are Dreadbox's files and are not in this repository.
    const std::string dir = (argc > 1 && argv[1][0]) ? argv[1] : "vendor";
    const std::string fac = dir + "/Nymphes_Factory_Presets_Factory_Bank.syx";
    const std::string usr = dir + "/Nymphes_Factory_Presets_User_Bank.syx";

    // ---- 0. known-answer anchors from prior/L1-firmware.md sec.4 (independent of this code)
    {
        // non-reflected poly 0x31, init 0x00 (this is NOT the reflected CRC-8/MAXIM,
        // whose check value is 0xa1; the reflected variant fails all vendor frames)
        CHECK(crc8(reinterpret_cast<const std::uint8_t*>("123456789"), 9) == 0xA2,
              "CRC-8 poly 0x31 init 0 non-reflected check value for \"123456789\" is 0xa2, got %02x",
              crc8(reinterpret_cast<const std::uint8_t*>("123456789"), 9));
    }

    // ---- 1/2. both vendor dumps, all 147 frames, byte-exact re-encode
    std::vector<Preset> factory, user;
    std::string err;
    CHECK(Preset::readSyx(fac, factory, &err), "read %s: %s", fac.c_str(), err.c_str());
    CHECK(Preset::readSyx(usr, user, &err), "read %s: %s", usr.c_str(), err.c_str());
    std::printf("factory frames: %zu   user dump frames: %zu\n", factory.size(), user.size());
    CHECK(factory.size() == 49, "factory dump should hold 49 frames, got %zu", factory.size());
    CHECK(user.size() == 98, "user dump should hold 98 frames, got %zu", user.size());

    // split the files ourselves and compare frame-to-frame with the codec's output
    const Bytes facBlob = slurp(fac), usrBlob = slurp(usr);
    const auto facFrames = Preset::splitSyx(facBlob);
    const auto usrFrames = Preset::splitSyx(usrBlob);
    CHECK(facFrames.size() == 49, "raw split factory: %zu", facFrames.size());
    CHECK(usrFrames.size() == 98, "raw split user: %zu", usrFrames.size());
    int fIdx = 0;
    for (const auto& f : facFrames) testFrame(f, fIdx++, "factory");
    int uIdx = 0;
    for (const auto& f : usrFrames) testFrame(f, uIdx++, "userdump");

    // whole-file identity: re-serialising every preset reproduces the file byte for byte
    CHECK(rawFrames(fac) == facBlob, "factory .syx is not byte-reproducible");
    CHECK(rawFrames(usr) == usrBlob, "user .syx is not byte-reproducible");

    if (facFrames.empty() || usrFrames.empty()) {
        std::printf("FAIL no vendor frames decoded - cannot continue\n");
        return 1;
    }

    // ---- 3. addressing coverage: 7 banks x 7 programs per page, both pages present
    {
        int userSlots = 0, facSlots = 0;
        bool seenUser[8][8] = {}, seenFac[8][8] = {};
        for (const auto& p : user) {
            const Address a = p.address();
            if (a.isUser()) {
                ++userSlots;
                seenUser[a.bank][a.program] = true;
            } else {
                ++facSlots;
                seenFac[a.bank][a.program] = true;
            }
        }
        std::printf("user-page slots: %d   factory-page slots: %d\n", userSlots, facSlots);
        CHECK(userSlots == 49 && facSlots == 49, "expected 49+49 slots, got %d+%d", userSlots,
              facSlots);
        int missing = 0;
        for (int b = 1; b <= 7; ++b)
            for (int g = 1; g <= 7; ++g) missing += !(seenUser[b][g] && seenFac[b][g]);
        CHECK(missing == 0, "%d slot addresses missing from the user dump", missing);
    }

    // ---- 4. address parsing
    {
        auto a = Address::parse("userD5");
        CHECK(a && a->page == kPageUser && a->bank == 4 && a->program == 5, "parse userD5");
        auto b = Address::parse("factoryA1");
        CHECK(b && b->page == kPageFactory && b->bank == 1 && b->program == 1, "parse factoryA1");
        auto c = Address::parse("G7");
        CHECK(c && c->page == kPageUser && c->bank == 7 && c->program == 7, "parse G7");
        CHECK(!Address::parse("H3"), "H3 must be rejected");
        CHECK(!Address::parse("A8"), "A8 must be rejected");
        CHECK(a->name() == "userD5" && b->name() == "factoryA1", "name round trip");
    }

    // ---- 5. a real edit: same field, one byte changed, CRC still valid, tree canonical
    {
        Preset p = user[0];
        const Bytes before = p.payload();
        const float cut = p.basic(kMain, kCut);
        const float nv = (cut < 0.5f) ? 0.75f : 0.25f;
        CHECK(p.setBasic(kMain, kCut, nv), "setBasic(cut)");
        CHECK(p.basic(kMain, kCut) == nv, "read back edited cut: %f", p.basic(kMain, kCut));
        CHECK(p.crcValid(), "CRC invalid after edit");
        CHECK(p.payload() != before, "payload unchanged after edit");
        // everything else must be untouched
        int changed = 0;
        for (std::size_t i = 0; i < kPayloadSize; ++i) changed += (p.payload()[i] != before[i]);
        CHECK(changed == 5, "edit touched %d payload bytes, expected 5 (4 float + 1 CRC)", changed);
        CHECK(p.encodeFrameRaw().size() == kFrameSize, "edited frame size");
        Preset rt;
        std::string e;
        const Bytes f = p.encodeFrameRaw();
        CHECK(Preset::decodeFrame(f.data(), f.size(), rt, &e), "decode edited frame: %s", e.c_str());
        CHECK(rt.isCanonical(), "edited tree is not canonical");
    }

    // ---- 6. varint edit (lfo_settings / legato / voice mode / chord) keeps lengths sane
    {
        Preset p = user[1];
        const bool leg = p.legato();
        CHECK(p.setLegato(!leg), "setLegato");
        CHECK(p.legato() == !leg, "legato read back");
        CHECK(p.crcValid(), "CRC invalid after legato edit");
        const std::uint64_t vm = p.voiceMode();
        CHECK(p.setVoiceMode(vm == 5 ? 0 : 5), "setVoiceMode");
        CHECK(p.voiceMode() == (vm == 5 ? 0 : 5), "voice mode read back");
        CHECK(p.setLfoSetting(1, 3), "setLfoSetting");
        CHECK(p.lfoSetting(1) == 3, "lfo setting read back");
        CHECK(p.setChordInfo(2, 3, 7), "setChordInfo");
        CHECK(p.chordInfo(2, 3) == 7, "chord read back");
        CHECK(p.crcValid(), "CRC invalid after varint edits");
        const Bytes f = p.encodeFrameRaw();
        Preset rt;
        std::string e;
        CHECK(Preset::decodeFrame(f.data(), f.size(), rt, &e), "decode varint-edited frame: %s",
              e.c_str());
        CHECK(rt.voiceMode() == p.voiceMode() && rt.chordInfo(2, 3) == 7,
              "varint edits did not survive the wire");
    }

    // ---- 7. factory write refusal
    {
        Preset facp = factory[0];
        std::string e;
        const Bytes refused = facp.encodeFrame(true, false, &e);
        CHECK(refused.empty(), "factory write was NOT refused (got %zu bytes)", refused.size());
        CHECK(!e.empty(), "factory refusal produced no error text");
        const Bytes forced = facp.encodeFrame(true, true, &e);
        CHECK(forced.size() == kFrameSize, "explicit factory override should serialise");
        CHECK(forced[6] == kFlagWrite, "override frame should carry the write flag");
        Preset up = user[0];
        const Bytes okw = up.encodeFrame(true);
        CHECK(okw.size() == kFrameSize && okw[6] == kFlagWrite, "user write should serialise");
        CHECK(up.encodeAudition()[6] == kFlagLoad, "audition must never set the write flag");
    }

    // ---- 8. .syx write -> read round trip through the filesystem
    {
        const std::string tmp = "/tmp/nymphes_codec_roundtrip.syx";
        std::string e;
        CHECK(Preset::writeSyx(tmp, user, &e), "writeSyx: %s", e.c_str());
        std::vector<Preset> back;
        CHECK(Preset::readSyx(tmp, back, &e), "readSyx: %s", e.c_str());
        CHECK(back.size() == user.size(), "round trip count %zu != %zu", back.size(), user.size());
        bool same = back.size() == user.size();
        for (std::size_t i = 0; same && i < back.size(); ++i)
            same = back[i].payload() == user[i].payload();
        CHECK(same, ".syx round trip changed payloads");
        CHECK(slurp(tmp) == usrBlob, ".syx rewrite is not byte-identical to the vendor dump");
        std::remove(tmp.c_str());
    }

    // ---- 9. negative cases: corrupt CRC, truncated frame, bad nibble, bad bounds
    {
        Bytes f = facFrames[0];
        Bytes bad = f;
        bad[0] = 0x00;
        Preset p;
        std::string e;
        CHECK(!Preset::decodeFrame(bad.data(), bad.size(), p, &e), "frame without F0 accepted");
        bad = f;
        bad[bad.size() - 1] = 0x00;
        CHECK(!Preset::decodeFrame(bad.data(), bad.size(), p, &e), "frame without F7 accepted");
        bad = f;
        bad[500] = 0x7F;  // nibble above 0x0F
        CHECK(!Preset::decodeFrame(bad.data(), bad.size(), p, &e), "nibble 0x7f accepted");
        bad = f;
        bad[20] ^= 0x01;  // payload bit flip -> CRC must catch it
        CHECK(!Preset::decodeFrame(bad.data(), bad.size(), p, &e), "CRC mismatch accepted");
        bad.assign(f.begin(), f.end() - 1);
        CHECK(!Preset::decodeFrame(bad.data(), bad.size(), p, &e), "short frame accepted");
        CHECK(!Preset::decodeFrame(f.data(), 100, p, &e), "truncated frame accepted");
    }

    std::printf("\n%d checks passed, %d failed\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}
