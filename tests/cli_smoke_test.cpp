// tests/cli_smoke_test.cpp — L3
//
// End-to-end regression tests for the headless CLI's FILE commands. Each case runs the
// real `nymphes-cli` binary built next to this test (same bin/ directory) and inspects
// its exit code and output, so it covers the command layer that a unit test cannot
// reach (cmdVerify's bookkeeping, cmdList's column, cmdWrite's plan, the output-stream
// and --outdir error paths, and --channel validation).
//
// No device, no JUCE, no vendor corpus: the fixtures are built here with the shipping
// codec (one frame that decodes, one frame whose nibble stream is corrupt).

#include "nymphes/Codec.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#if defined(_WIN32)
#  include <process.h>
#  define NYMPHES_SHELL_EXIT(c) (c)
#else
#  include <sys/wait.h>
#  include <unistd.h>
#  define NYMPHES_SHELL_EXIT(c) ((c) == -1 ? -1 : (WIFEXITED (c) ? WEXITSTATUS (c) : 128 + WTERMSIG (c)))
#endif

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

namespace fs = std::filesystem;

// A frame that decodes: a canonical 32-byte protobuf body (15x legato=0, voice_mode=0),
// sealed so the CRC verifies. Address FACTORY C2 so the write PLAN has something to
// report as the origin.
static Bytes goodFrame() {
    PbMessage msg;
    for (int i = 0; i < 15; ++i) {
        PbField f;
        f.number = kLegato;
        f.wire = Wire::Varint;
        f.varint = 0;
        msg.fields.push_back(f);
    }
    PbField vm;
    vm.number = kVoiceMode;
    vm.wire = Wire::Varint;
    vm.varint = 0;
    msg.fields.push_back(vm);
    const Bytes body = pbEncode(msg);

    Bytes payload(1, 0);
    payload.insert(payload.end(), body.begin(), body.end());
    payload[0] = crc8(payload.data() + 1, payload.size() - 1);

    Preset p;
    p.setPayload(payload);
    Address a;
    a.page = kPageFactory;
    a.bank = 3;      // C
    a.program = 2;
    p.setAddress(a);
    return p.encodeFrameRaw();
}

static bool writeFile(const fs::path& path, const Bytes& bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(f);
}

struct Run {
    int code = -1;
    std::string out;
};

static std::string quote(const fs::path& p) { return "\"" + p.string() + "\""; }

int main(int argc, char** argv) {
    const fs::path self = (argc > 0 && argv[0] != nullptr) ? fs::path(argv[0]) : fs::path(".");
#if defined(_WIN32)
    const fs::path cli = self.parent_path() / "nymphes-cli.exe";
#else
    const fs::path cli = self.parent_path() / "nymphes-cli";
#endif
    if (!fs::exists(cli)) {
        // Not a skip: this test exists to exercise that binary, and a missing binary
        // means the build is incomplete (it is built in the same bin/ directory).
        std::printf("FAIL  nymphes-cli not found next to the test (%s)\n",
                    cli.string().c_str());
        return 1;
    }

    const fs::path dir = fs::temp_directory_path() / ("nymphes_cli_smoke_" + std::to_string(
#if defined(_WIN32)
        static_cast<long long>(_getpid())
#else
        static_cast<long long>(::getpid())
#endif
    ));
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    if (ec) {
        std::printf("FAIL  cannot create %s: %s\n", dir.string().c_str(), ec.message().c_str());
        return 1;
    }

    const Bytes good = goodFrame();
    Bytes corrupt = good;
    corrupt[20] = 0x7F;  // a nibble byte above 0x0F: framing still splits, decode rejects
    const fs::path goodSyx = dir / "good.syx";
    const fs::path mixedSyx = dir / "mixed.syx";
    const fs::path outFile = dir / "run.out";
    CHECK(writeFile(goodSyx, good), "fixture write %s", goodSyx.string().c_str());
    {
        Bytes mixed = corrupt;
        mixed.insert(mixed.end(), good.begin(), good.end());  // bad frame FIRST
        CHECK(writeFile(mixedSyx, mixed), "fixture write %s", mixedSyx.string().c_str());
    }

    auto run = [&](const std::string& args) {
        Run r;
        const std::string cmd = quote(cli) + " " + args + " > " + quote(outFile) + " 2>&1";
#if defined(_WIN32)
        // cmd.exe /c strips its outer quotes; keep the executable/path quotes
        // inside a second pair so paths containing spaces remain intact.
        const int raw = std::system(("\"" + cmd + "\"").c_str());
#else
        const int raw = std::system(cmd.c_str());
#endif
        r.code = NYMPHES_SHELL_EXIT(raw);
        std::ifstream in(outFile);
        r.out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return r;
    };

    // ---- 1. a clean dump passes, and the re-encode gate counts the right bytes
    {
        const Run r = run("verify " + quote(goodSyx));
        CHECK(r.code == 0, "verify good.syx should pass, got %d:\n%s", r.code, r.out.c_str());
        CHECK(r.out.find("PASS verify") != std::string::npos, "verify good.syx: %s", r.out.c_str());
        CHECK(r.out.find("re-encode  0 of 1 differ") != std::string::npos,
              "verify good.syx re-encode line:\n%s", r.out.c_str());
    }

    // ---- 2. a dump with ONE bad frame: the decoded preset must still be compared with
    //         ITS OWN source frame. Before the fix presets[0] (the good frame) was
    //         compared with frames[0] (the corrupt one), so the tool claimed its own
    //         re-encode was wrong: "re-encode 1 of 1 differ".
    {
        const Run r = run("verify " + quote(mixedSyx));
        CHECK(r.code == 1, "verify mixed.syx should fail on the corrupt frame, got %d", r.code);
        CHECK(r.out.find("re-encode  0 of 1 differ") != std::string::npos,
              "verify mixed.syx must not blame the good frame:\n%s", r.out.c_str());
        CHECK(r.out.find("ERROR message 0:") != std::string::npos,
              "the corrupt frame must be reported by index:\n%s", r.out.c_str());
        CHECK(r.out.find("1 decoded, 1 rejected") != std::string::npos,
              "verify mixed.syx counts:\n%s", r.out.c_str());
    }

    // ---- 3. list: the summary column keeps the same offset for every slot name
    //         (summaryLine pads the name to 12 characters; stripping name.size()+1
    //         characters left 2..5 extra spaces).
    {
        const Run r = run("list " + quote(goodSyx));
        CHECK(r.code == 0, "list exit %d", r.code);
        CHECK(r.out.find("FACTORY C2") != std::string::npos, "list lost the slot name:\n%s",
              r.out.c_str());
        CHECK(r.out.find("B  wave") != std::string::npos,
              "list summary column has a wrong offset:\n%s", r.out.c_str());
        CHECK(r.out.find("B   wave") == std::string::npos,
              "list summary column has trailing padding spaces:\n%s", r.out.c_str());
    }

    // ---- 4. the write PLAN must name the slot the preset came from, not the target.
    //         Without --yes the command stops before opening the device, so this is
    //         testable with no hardware.
    {
        const fs::path lock = dir / "write_plan.lock";
        const Run r = run("write " + quote(goodSyx) + " --slot A1 --lock " + quote(lock));
        CHECK(r.code == 2, "write without --yes should exit 2, got %d:\n%s", r.code, r.out.c_str());
#if defined(_WIN32)
        // The CLI intentionally refuses device sessions without flock. Windows
        // VST3/Standalone use WinMM directly; this headless tool is not shipped.
        CHECK(r.out.find("no file locking; refusing a device session") != std::string::npos,
              "Windows CLI must fail closed without device locking:\n%s", r.out.c_str());
#else
        CHECK(r.out.find("originally FACTORY C2") != std::string::npos,
              "write plan must report the ORIGIN slot:\n%s", r.out.c_str());
        CHECK(r.out.find("originally USER A1") == std::string::npos,
              "write plan reported the destination as the origin:\n%s", r.out.c_str());
#endif
    }

    // ---- 5. an unwritable --out must fail instead of printing "wrote ..." and exiting 0
    {
        const fs::path bad = dir / "no_such_dir" / "x.txt";
        const Run r = run("txt " + quote(goodSyx) + " --out " + quote(bad));
        CHECK(r.code == 1, "txt to an unwritable path should exit 1, got %d:\n%s", r.code,
              r.out.c_str());
        CHECK(r.out.find("cannot open") != std::string::npos, "txt error text:\n%s", r.out.c_str());
        CHECK(r.out.find("wrote ") == std::string::npos, "txt claimed success:\n%s", r.out.c_str());

        const Run r2 = run("export-txt " + quote(goodSyx) + " --out " + quote(bad));
        CHECK(r2.code == 1, "export-txt to an unwritable path should exit 1, got %d:\n%s", r2.code,
              r2.out.c_str());
        CHECK(r2.out.find("wrote ") == std::string::npos, "export-txt claimed success:\n%s",
              r2.out.c_str());
    }

    // ---- 6. --outdir that is an existing regular file must be a clean error, not an
    //         uncaught std::filesystem_error aborting the process.
    {
        const fs::path asFile = dir / "outdir_is_a_file";
        CHECK(writeFile(asFile, Bytes{1, 2, 3}), "fixture write %s", asFile.string().c_str());
        const Run r = run("split " + quote(goodSyx) + " --outdir " + quote(asFile));
        CHECK(r.code == 1, "split with a file as --outdir should exit 1, got %d:\n%s", r.code,
              r.out.c_str());
        CHECK(r.out.find("cannot create") != std::string::npos, "split error text:\n%s",
              r.out.c_str());
    }

    // ---- 7. --channel is validated before a session is opened: 0 and 17 are usage
    //         errors (exit 2), not a port that opens and silently drops the message.
    {
        const fs::path lock = dir / "midi.lock";
        const Run a = run("cc 7 100 --channel 0 --lock " + quote(lock));
        CHECK(a.code == 2, "cc --channel 0 should exit 2, got %d:\n%s", a.code, a.out.c_str());
#if defined(_WIN32)
        const char* expectedError = "no file locking; refusing a device session";
#else
        const char* expectedError = "--channel must be 1..16";
#endif
        CHECK(a.out.find(expectedError) != std::string::npos, "cc error text:\n%s",
              a.out.c_str());
        const Run b = run("cc 7 100 --channel 17 --lock " + quote(lock));
        CHECK(b.code == 2, "cc --channel 17 should exit 2, got %d:\n%s", b.code, b.out.c_str());
        const Run c = run("panic --channel 99 --lock " + quote(lock));
        CHECK(c.code == 2, "panic --channel 99 should exit 2, got %d:\n%s", c.code, c.out.c_str());
        CHECK(c.out.find(expectedError) != std::string::npos, "panic error text:\n%s",
              c.out.c_str());
    }

    fs::remove_all(dir, ec);
    std::printf("\n%d checks passed, %d failed\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}
