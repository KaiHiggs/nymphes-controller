// nymphes/Codec.cpp - see include/nymphes/Codec.h for the format documentation.

#include "nymphes/Codec.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace nymphes {
namespace {

inline void putLe32(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
    p[2] = static_cast<std::uint8_t>(v >> 16);
    p[3] = static_cast<std::uint8_t>(v >> 24);
}
inline std::uint32_t getLe32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

float bitsToFloat(const Bytes& b) {
    if (b.size() != 4) return 0.0f;
    std::uint32_t v = getLe32(b.data());
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
Bytes floatToBits(float f) {
    std::uint32_t v;
    std::memcpy(&v, &f, 4);
    Bytes b(4);
    putLe32(b.data(), v);
    return b;
}

void putVarint(std::uint64_t v, Bytes& out) {
    do {
        std::uint8_t byte = static_cast<std::uint8_t>(v & 0x7F);
        v >>= 7;
        if (v) byte |= 0x80;
        out.push_back(byte);
    } while (v);
}

bool getVarint(const std::uint8_t* d, std::size_t n, std::size_t& i, std::uint64_t& v) {
    v = 0;
    int shift = 0;
    while (i < n) {
        std::uint8_t b = d[i++];
        if (shift <= 63) v |= static_cast<std::uint64_t>(b & 0x7F) << shift;
        shift += 7;
        if (!(b & 0x80)) return true;
        if (shift > 70) return false;
    }
    return false;
}

const std::uint32_t kBasicSections[] = {kMain, kLfo2, kModWheel, kVelocity, kAftertouch};

}  // namespace

// ------------------------------------------------------------------ low level
std::uint8_t crc8(const std::uint8_t* data, std::size_t n) {
    std::uint8_t crc = 0x00;
    for (std::size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x80) ? static_cast<std::uint8_t>((crc << 1) ^ 0x31)
                               : static_cast<std::uint8_t>(crc << 1);
    }
    return crc;
}

bool unpackNibbles(const std::uint8_t* nib, std::size_t n, Bytes& payload) {
    if (n == 0 || (n % 2) != 0) return false;
    if (n / 2 < kMinPayloadSize || n / 2 > kMaxPayloadSize) return false;
    payload.assign(n / 2, 0);
    for (std::size_t i = 0; i < n; i += 2) {
        if (nib[i] > 0x0F || nib[i + 1] > 0x0F) return false;
        payload[i / 2] = static_cast<std::uint8_t>((nib[i + 1] << 4) | nib[i]);
    }
    return true;
}

Bytes packNibbles(const Bytes& payload) {
    Bytes out;
    out.reserve(2 * payload.size());
    for (std::uint8_t b : payload) {
        out.push_back(static_cast<std::uint8_t>(b & 0x0F));
        out.push_back(static_cast<std::uint8_t>((b >> 4) & 0x0F));
    }
    return out;
}

// ------------------------------------------------------------------ protobuf
const PbField* PbField::subFind(std::uint32_t n) const {
    for (const auto& f : sub)
        if (f.number == n) return &f;
    return nullptr;
}
PbField* PbField::subFind(std::uint32_t n) {
    for (auto& f : sub)
        if (f.number == n) return &f;
    return nullptr;
}

const PbField* PbMessage::find(std::uint32_t number) const {
    for (const auto& f : fields)
        if (f.number == number) return &f;
    return nullptr;
}
PbField* PbMessage::find(std::uint32_t number) {
    for (auto& f : fields)
        if (f.number == number) return &f;
    return nullptr;
}

static bool parseFields(const std::uint8_t* data, std::size_t n,
                        std::vector<PbField>& out, int depth) {
    out.clear();
    std::size_t i = 0;
    while (i < n) {
        std::uint64_t tag = 0;
        if (!getVarint(data, n, i, tag)) return false;
        const std::uint32_t number = static_cast<std::uint32_t>(tag >> 3);
        const std::uint32_t wire = static_cast<std::uint32_t>(tag & 0x07);
        if (number == 0) return false;
        PbField f;
        f.number = number;
        switch (wire) {
            case 0: {
                f.wire = Wire::Varint;
                if (!getVarint(data, n, i, f.varint)) return false;
                break;
            }
            case 1: {
                f.wire = Wire::Fixed64;
                if (i + 8 > n) return false;
                f.bytes.assign(data + i, data + i + 8);
                i += 8;
                break;
            }
            case 5: {
                f.wire = Wire::Fixed32;
                if (i + 4 > n) return false;
                f.bytes.assign(data + i, data + i + 4);
                i += 4;
                break;
            }
            case 2: {
                f.wire = Wire::Length;
                std::uint64_t len = 0;
                if (!getVarint(data, n, i, len)) return false;
                if (len > n - i) return false;
                f.bytes.assign(data + i, data + i + static_cast<std::size_t>(len));
                i += static_cast<std::size_t>(len);
                if (depth > 0 && !f.bytes.empty()) {
                    std::vector<PbField> sub;
                    if (parseFields(f.bytes.data(), f.bytes.size(), sub, depth - 1)) {
                        f.sub = std::move(sub);
                        f.hasSub = true;
                    }
                }
                break;
            }
            default:
                return false;  // groups are not used by this schema
        }
        out.push_back(std::move(f));
    }
    return true;
}

bool pbParse(const std::uint8_t* data, std::size_t n, PbMessage& out, int depth) {
    return parseFields(data, n, out.fields, depth);
}

Bytes pbEncode(const PbMessage& msg) {
    Bytes out;
    for (const auto& f : msg.fields) {
        const std::uint32_t tag = (f.number << 3) | static_cast<std::uint32_t>(f.wire);
        putVarint(tag, out);
        switch (f.wire) {
            case Wire::Varint:
                putVarint(f.varint, out);
                break;
            case Wire::Fixed64:
                out.insert(out.end(), f.bytes.begin(), f.bytes.end());
                break;
            case Wire::Fixed32:
                out.insert(out.end(), f.bytes.begin(), f.bytes.end());
                break;
            case Wire::Length: {
                Bytes body = f.bytes;
                if (f.hasSub) {
                    body.clear();
                    for (const auto& sf : f.sub) {
                        PbMessage one;
                        one.fields.push_back(sf);
                        const Bytes enc = pbEncode(one);
                        body.insert(body.end(), enc.begin(), enc.end());
                    }
                }
                putVarint(body.size(), out);
                out.insert(out.end(), body.begin(), body.end());
                break;
            }
        }
    }
    return out;
}

// ------------------------------------------------------------------ naming
const char* basicName(std::uint32_t slot) {
    static const char* kNames[kBasicCount] = {
        "wave",   "lvl",      "sub",       "noise",  "osc_lfo",  "cut",     "reson",
        "cut_eg", "a1",       "d1",        "s1",     "r1",       "lfo_rate", "lfo_wave",
        "pw",     "glide",    "dtune",     "chord",  "osc_eg",   "hpf",     "track",
        "cut_lfo", "a2",      "d2",        "s2",     "r2",       "lfo_delay", "lfo_fade"};
    return (slot >= 1 && slot <= kBasicCount) ? kNames[slot - 1] : nullptr;
}

const char* topName(std::uint32_t field) {
    switch (field) {
        case kMain: return "main";
        case kReverb: return "reverb";
        case kLfo2: return "lfo_2";
        case kModWheel: return "mod_w";
        case kVelocity: return "velo";
        case kAftertouch: return "after";
        case kLfoSettings: return "lfo_settings";
        case kLegato: return "legato";
        case kVoiceMode: return "voice_mode";
        case kExtraLfo2: return "extra_lfo_2";
        case kExtraModWheel: return "extra_mod_w";
        case kExtraVelocity: return "extra_velo";
        case kExtraAftertouch: return "extra_after";
        case kAmpLevel: return "amp_level";
        default: break;
    }
    if (field >= kChord1 && field <= kChord8) return "chord";
    return nullptr;
}

// ------------------------------------------------------------------ address
std::string Address::name() const {
    std::string s = (page == kPageFactory) ? "factory" : "user";
    s += static_cast<char>('A' + (bank >= 1 && bank <= 7 ? bank - 1 : 0));
    s += std::to_string(program);
    return s;
}

std::optional<Address> Address::parse(const std::string& in) {
    std::string s;
    for (char c : in)
        if (c != ' ' && c != '\t') s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    Address a;
    if (s.rfind("factory", 0) == 0) {
        a.page = kPageFactory;
        s = s.substr(7);
    } else if (s.rfind("user", 0) == 0) {
        a.page = kPageUser;
        s = s.substr(4);
    }
    while (!s.empty() && (s[0] == '-' || s[0] == '_' || s[0] == '/')) s.erase(s.begin());
    if (s.size() < 2) return std::nullopt;
    const char b = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    if (b < 'A' || b > 'G') return std::nullopt;
    a.bank = static_cast<std::uint8_t>(b - 'A' + 1);
    int prog = 0;
    for (std::size_t i = 1; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return std::nullopt;
        prog = prog * 10 + (s[i] - '0');
    }
    if (prog < 1 || prog > 7) return std::nullopt;
    a.program = static_cast<std::uint8_t>(prog);
    return a;
}

// ------------------------------------------------------------------ decode
bool Preset::decodeFrame(const std::uint8_t* data, std::size_t n, Preset& out,
                         std::string* err) {
    auto fail = [&](const char* m) {
        if (err) *err = m;
        return false;
    };
    if (n < 12) return fail("frame too short");
    if ((n - 11) % 2) return fail("nibble stream has an odd length");
    if (data[0] != 0xF0 || data[n - 1] != 0xF7) return fail("frame is not bounded by F0/F7");
    for (int i = 0; i < 5; ++i)
        if (data[1 + i] != kManufacturer[i]) return fail("unexpected manufacturer / product id");
    const std::uint8_t flag = data[6], page = data[7], bank = data[8], prog = data[9];
    if (flag > 1) return fail("write-enable flag is neither 0x00 nor 0x01");
    if (page > 1) return fail("page byte is neither USER (0) nor FACTORY (1)");
    if (bank < 1 || bank > 7) return fail("bank byte outside 1..7");
    if (prog < 1 || prog > 7) return fail("program byte outside 1..7");
    Bytes payload;
    if (!unpackNibbles(data + 10, n - 11, payload)) return fail("nibble stream above 0x0F");
    if (payload.size() < kMinPayloadSize || payload.size() > kMaxPayloadSize)
        return fail("payload size outside the sane range");
    if (payload[0] != crc8(payload.data() + 1, payload.size() - 1)) return fail("CRC-8 mismatch");
    PbMessage msg;
    if (!pbParse(payload.data() + 1, payload.size() - 1, msg, 1))
        return fail("protobuf parse failed");
    out.addr_ = Address{page, bank, prog};
    out.flag_ = flag;
    out.original_ = payload;
    out.payload_ = std::move(payload);
    out.msg_ = std::move(msg);
    out.canonical_ = (pbEncode(out.msg_) == Bytes(out.payload_.begin() + 1, out.payload_.end()));
    out.edited_ = false;
    return true;
}

std::optional<Preset> Preset::decodeFrame(const Bytes& frame, std::string* err) {
    Preset p;
    if (!decodeFrame(frame.data(), frame.size(), p, err)) return std::nullopt;
    return p;
}

void Preset::setPayload(Bytes p) {
    original_ = p;
    payload_ = std::move(p);
    msg_ = PbMessage{};
    canonical_ = false;
    edited_ = false;
    if (payload_.size() > 1)
        canonical_ = pbParse(payload_.data() + 1, payload_.size() - 1, msg_, 1);
}

void Preset::seal() {
    if (payload_.size() < 2) payload_.assign(kPayloadSize, 0);
    payload_[0] = crc8(payload_.data() + 1, payload_.size() - 1);
}

bool Preset::crcValid() const {
    return payload_.size() >= 2 && payload_[0] == crc8(payload_.data() + 1, payload_.size() - 1);
}

Bytes Preset::encodeFrame(bool persist, bool allowFactoryWrite, std::string* err) const {
    if (payload_.size() < 2) {
        if (err) *err = "preset has no payload";
        return {};
    }
    const bool write = persist;
    if (write && addr_.page == kPageFactory && !allowFactoryWrite) {
        if (err) *err = "refusing to write a FACTORY preset (read-only by policy)";
        return {};
    }
    Bytes frame;
    frame.reserve(frameSize());
    frame.push_back(0xF0);
    frame.insert(frame.end(), kManufacturer, kManufacturer + 5);
    frame.push_back(write ? kFlagWrite : kFlagLoad);
    frame.push_back(addr_.page);
    frame.push_back(addr_.bank);
    frame.push_back(addr_.program);
    const Bytes nib = packNibbles(payload_);
    frame.insert(frame.end(), nib.begin(), nib.end());
    frame.push_back(0xF7);
    return frame;
}

Bytes Preset::encodeFrameRaw() const {
    if (payload_.size() < 2) return {};
    Bytes frame;
    frame.reserve(frameSize());
    frame.push_back(0xF0);
    frame.insert(frame.end(), kManufacturer, kManufacturer + 5);
    frame.push_back(flag_);
    frame.push_back(addr_.page);
    frame.push_back(addr_.bank);
    frame.push_back(addr_.program);
    const Bytes nib = packNibbles(payload_);
    frame.insert(frame.end(), nib.begin(), nib.end());
    frame.push_back(0xF7);
    return frame;
}

Bytes Preset::encodeAudition() const {
    std::string e;
    return encodeFrame(false, true, &e);
}

// ------------------------------------------------------------------ typed access
namespace {
// Re-encode the in-memory tree into payload_ so payload() is always current.
}  // namespace

bool Preset::hasSection(std::uint32_t top) const { return msg_.find(top) != nullptr; }

static void resync(PbMessage& msg, Bytes& payload) {
    Bytes body = pbEncode(msg);
    payload.assign(1, 0);
    payload.insert(payload.end(), body.begin(), body.end());
    payload[0] = crc8(payload.data() + 1, payload.size() - 1);
}

static float readF32(const PbField* f) { return f ? bitsToFloat(f->bytes) : 0.0f; }

static bool writeF32(PbField* f, float v) {
    if (!f || f->wire != Wire::Fixed32) return false;
    f->bytes = floatToBits(v);
    return true;
}

float Preset::basic(std::uint32_t top, std::uint32_t slot) const {
    const PbField* sec = msg_.find(top);
    if (!sec || !sec->hasSub) return 0.0f;
    return readF32(sec->subFind(slot));
}

bool Preset::setBasic(std::uint32_t top, std::uint32_t slot, float value) {
    PbField* sec = msg_.find(top);
    if (!sec || !sec->hasSub) return false;
    if (!writeF32(sec->subFind(slot), value)) return false;
    resync(msg_, payload_);
    edited_ = true;
    return true;
}

float Preset::reverb(std::uint32_t slot) const { return basic(kReverb, slot); }
bool Preset::setReverb(std::uint32_t slot, float v) { return setBasic(kReverb, slot, v); }

float Preset::extra(std::uint32_t top, std::uint32_t slot) const { return basic(top, slot); }
bool Preset::setExtra(std::uint32_t top, std::uint32_t slot, float v) {
    return setBasic(top, slot, v);
}

float Preset::ampLevel() const {
    return readF32(msg_.find(kAmpLevel));
}
bool Preset::setAmpLevel(float v) {
    if (!writeF32(msg_.find(kAmpLevel), v)) return false;
    resync(msg_, payload_);
    edited_ = true;
    return true;
}

std::uint64_t Preset::chordInfo(std::uint32_t chord, std::uint32_t slot) const {
    const PbField* f = msg_.find(kChord1 + chord - 1);
    if (!f || !f->hasSub) return 0;
    const PbField* s = f->subFind(slot);
    return s ? s->varint : 0;
}

bool Preset::setChordInfo(std::uint32_t chord, std::uint32_t slot, std::uint64_t v) {
    PbField* f = msg_.find(kChord1 + chord - 1);
    if (!f || !f->hasSub) return false;
    PbField* s = f->subFind(slot);
    if (!s) return false;
    s->varint = v;
    resync(msg_, payload_);
    edited_ = true;
    return true;
}

std::uint64_t Preset::lfoSetting(std::uint32_t slot) const {
    const PbField* f = msg_.find(kLfoSettings);
    if (!f || !f->hasSub) return 0;
    const PbField* s = f->subFind(slot);
    return s ? s->varint : 0;
}

bool Preset::setLfoSetting(std::uint32_t slot, std::uint64_t v) {
    PbField* f = msg_.find(kLfoSettings);
    if (!f || !f->hasSub) return false;
    PbField* s = f->subFind(slot);
    if (!s) return false;
    s->varint = v;
    resync(msg_, payload_);
    edited_ = true;
    return true;
}

bool Preset::legato() const {
    const PbField* f = msg_.find(kLegato);
    return f && f->varint != 0;
}

bool Preset::setLegato(bool on) {
    PbField* f = msg_.find(kLegato);
    if (!f) return false;
    f->varint = on ? 1 : 0;
    resync(msg_, payload_);
    edited_ = true;
    return true;
}

std::uint64_t Preset::voiceMode() const {
    const PbField* f = msg_.find(kVoiceMode);
    return f ? f->varint : 0;
}

bool Preset::setVoiceMode(std::uint64_t mode) {
    PbField* f = msg_.find(kVoiceMode);
    if (!f) return false;
    f->varint = mode;
    resync(msg_, payload_);
    edited_ = true;
    return true;
}

// ------------------------------------------------------------------ .syx
std::vector<Bytes> Preset::splitSyx(const Bytes& blob) {
    std::vector<Bytes> out;
    std::size_t i = 0;
    while (i < blob.size()) {
        const auto b = std::find(blob.begin() + static_cast<long>(i), blob.end(), 0xF0);
        if (b == blob.end()) break;
        const std::size_t s = static_cast<std::size_t>(b - blob.begin());
        const auto e = std::find(blob.begin() + static_cast<long>(s), blob.end(), 0xF7);
        if (e == blob.end()) break;
        const std::size_t end = static_cast<std::size_t>(e - blob.begin());
        out.emplace_back(blob.begin() + static_cast<long>(s), blob.begin() + static_cast<long>(end) + 1);
        i = end + 1;
    }
    return out;
}

bool Preset::readSyx(const std::string& path, std::vector<Preset>& out, std::string* err) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    Bytes blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    out.clear();
    for (const auto& frame : splitSyx(blob)) {
        Preset p;
        std::string e;
        if (!decodeFrame(frame.data(), frame.size(), p, &e)) {
            if (err) *err = path + ": " + e;
            return false;
        }
        out.push_back(std::move(p));
    }
    return !out.empty();
}

bool Preset::writeSyx(const std::string& path, const std::vector<Preset>& presets,
                      std::string* err) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (err) *err = "cannot write " + path;
        return false;
    }
    for (const auto& p : presets) {
        const Bytes f = p.encodeFrameRaw();
        out.write(reinterpret_cast<const char*>(f.data()), static_cast<std::streamsize>(f.size()));
    }
    return static_cast<bool>(out);
}

bool Preset::writeSyxAppend(const std::string& path, std::string* err) const {
    const Bytes f = encodeFrameRaw();
    if (f.empty()) return false;
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) {
        if (err) *err = "cannot append " + path;
        return false;
    }
    out.write(reinterpret_cast<const char*>(f.data()), static_cast<std::streamsize>(f.size()));
    return static_cast<bool>(out);
}

// ------------------------------------------------------------------ .txt
static std::string hex(const Bytes& b) {
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(b.size() * 2);
    for (std::uint8_t x : b) {
        s += d[x >> 4];
        s += d[x & 0x0F];
    }
    return s;
}

static bool unhex(const std::string& s, Bytes& out) {
    if (s.size() % 2) return false;
    out.clear();
    auto v = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < s.size(); i += 2) {
        const int hi = v(s[i]), lo = v(s[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return true;
}

static std::string fmtFloat(float f) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.9g", static_cast<double>(f));
    return buf;
}

std::string Preset::toText() const {
    std::ostringstream o;
    o << "nymphes-preset 1\n";
    o << "address=" << addr_.name() << "\n";
    o << "flag=" << static_cast<int>(flag_) << "\n";
    o << "payload=" << hex(payload_) << "\n";
    for (std::uint32_t sec : kBasicSections) {
        const char* sn = topName(sec);
        if (!hasSection(sec)) continue;
        for (std::uint32_t i = 1; i <= kBasicCount; ++i)
            o << sn << "." << basicName(i) << "=" << fmtFloat(basic(sec, i)) << "\n";
    }
    for (std::uint32_t i = 1; i <= 4; ++i)
        o << "reverb." << basicName(i) << "=" << fmtFloat(reverb(i)) << "\n";
    for (std::uint32_t top : {kExtraLfo2, kExtraModWheel, kExtraVelocity, kExtraAftertouch}) {
        const char* sn = topName(top);
        for (std::uint32_t i = 1; i <= (top == kExtraLfo2 ? 12u : 8u); ++i)
            o << sn << ".f" << i << "=" << fmtFloat(extra(top, i)) << "\n";
    }
    for (std::uint32_t i = 1; i <= 4; ++i) o << "lfo_settings.s" << i << "=" << lfoSetting(i) << "\n";
    o << "legato=" << (legato() ? 1 : 0) << "\n";
    o << "voice_mode=" << voiceMode() << "\n";
    for (std::uint32_t c = 1; c <= 8; ++c)
        for (std::uint32_t s = 1; s <= 6; ++s)
            o << "chord" << c << ".i" << s << "=" << chordInfo(c, s) << "\n";
    o << "amp_level=" << fmtFloat(ampLevel()) << "\n";
    return o.str();
}

bool Preset::fromText(const std::string& text, Preset& out, std::string* err) {
    auto fail = [&](const char* m) {
        if (err) *err = m;
        return false;
    };
    std::istringstream in(text);
    std::string line, payloadHex;
    std::uint8_t flagByte = kFlagLoad;
    Address addr;
    std::vector<std::pair<std::string, std::string>> kv;
    bool sawHeader = false;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            if (line.rfind("nymphes-preset", 0) == 0) {
                sawHeader = true;
                continue;
            }
            return fail("unparseable line in preset text");
        }
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "payload") payloadHex = v;
        else if (k == "flag") flagByte = static_cast<std::uint8_t>(std::stoi(v));
        else if (k == "address") {
            auto a = Address::parse(v);
            if (!a) return fail("bad address");
            addr = *a;
        } else kv.emplace_back(k, v);
    }
    if (!sawHeader) return fail("missing nymphes-preset header");
    Bytes payload;
    if (payloadHex.empty() || !unhex(payloadHex, payload) || payload.size() < kMinPayloadSize)
        return fail("missing or malformed payload=");
    out.setPayload(std::move(payload));
    out.addr_ = addr;
    out.flag_ = flagByte;
    // Apply only the lines that actually differ from the payload, so an untouched
    // export re-imports to identical bytes even if the source was not canonical.
    auto trailingNumber = [](const std::string& in) -> int {
        std::size_t i = 0;
        while (i < in.size() && (in[i] < '0' || in[i] > '9')) ++i;
        return (i < in.size()) ? std::stoi(in.substr(i)) : 0;
    };
    auto sameFloat = [](float a, float b) {
        std::uint32_t x, y;
        std::memcpy(&x, &a, 4);
        std::memcpy(&y, &b, 4);
        return x == y;
    };
    for (const auto& [k, v] : kv) {
        const auto dot = k.find('.');
        if (k == "legato") {
            const bool want = std::stoi(v) != 0;
            if (want != out.legato()) out.setLegato(want);
        } else if (k == "voice_mode") {
            const auto want = static_cast<std::uint64_t>(std::stoull(v));
            if (want != out.voiceMode()) out.setVoiceMode(want);
        } else if (k == "amp_level") {
            const float want = std::stof(v);
            if (!sameFloat(want, out.ampLevel())) out.setAmpLevel(want);
        } else if (k.rfind("chord", 0) == 0 && dot != std::string::npos) {
            const int c = trailingNumber(k.substr(5, dot - 5));
            const int s = trailingNumber(k.substr(dot + 1));
            const auto want = static_cast<std::uint64_t>(std::stoull(v));
            if (c >= 1 && c <= 8 && s >= 1 && s <= 6 && out.chordInfo(c, s) != want)
                out.setChordInfo(static_cast<std::uint32_t>(c), static_cast<std::uint32_t>(s), want);
        } else if (k.rfind("lfo_settings.", 0) == 0) {
            const int s = trailingNumber(k.substr(13));
            const auto want = static_cast<std::uint64_t>(std::stoull(v));
            if (s >= 1 && s <= 4 && out.lfoSetting(s) != want)
                out.setLfoSetting(static_cast<std::uint32_t>(s), want);
        } else if (dot != std::string::npos) {
            const std::string sec = k.substr(0, dot), name = k.substr(dot + 1);
            std::uint32_t top = 0;
            if (sec == "main") top = kMain;
            else if (sec == "lfo_2") top = kLfo2;
            else if (sec == "mod_w") top = kModWheel;
            else if (sec == "velo") top = kVelocity;
            else if (sec == "after") top = kAftertouch;
            else if (sec == "reverb") top = kReverb;
            else if (sec == "extra_lfo_2") top = kExtraLfo2;
            else if (sec == "extra_mod_w") top = kExtraModWheel;
            else if (sec == "extra_velo") top = kExtraVelocity;
            else if (sec == "extra_after") top = kExtraAftertouch;
            else return fail("unknown section in preset text");
            std::uint32_t slot = 0;
            if (top == kExtraLfo2 || top == kExtraModWheel || top == kExtraVelocity ||
                top == kExtraAftertouch) {
                slot = static_cast<std::uint32_t>(trailingNumber(name));
            } else {
                for (std::uint32_t i = 1; i <= kBasicCount; ++i)
                    if (name == basicName(i)) slot = i;
            }
            if (slot == 0) return fail("unknown parameter name in preset text");
            const float want = std::stof(v);
            if (!sameFloat(want, out.basic(top, slot))) out.setBasic(top, slot, want);
        }
    }
    return true;
}

}  // namespace nymphes
