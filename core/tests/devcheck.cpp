// devcheck.cpp - run the canonical codec over any .syx corpus (vendor dump or a captured
// device dump) and prove byte-exact round-trip at every payload size actually present.
//   clang++ -std=c++17 -O2 -Icore/include core/src/Codec.cpp devcheck.cpp -o devcheck
//   ./devcheck <file.syx>
#include "nymphes/Codec.h"
#include <cstdio>
#include <map>
using namespace nymphes;
int main(int, char** argv) {
    std::vector<Preset> ps;
    std::string err;
    if (!Preset::readSyx(argv[1], ps, &err)) { std::printf("FAIL read: %s\n", err.c_str()); return 1; }
    std::map<std::size_t, int> sizes;
    int bad = 0;
    for (std::size_t i = 0; i < ps.size(); ++i) {
        sizes[ps[i].payloadSize()]++;
        if (!ps[i].crcValid()) { std::printf("FAIL crc at %zu\n", i); ++bad; }
        if (!ps[i].isCanonical()) { std::printf("FAIL non-canonical at %zu\n", i); ++bad; }
        const std::string t = ps[i].toText();
        Preset q; std::string e;
        if (!Preset::fromText(t, q, &e)) { std::printf("FAIL txt at %zu: %s\n", i, e.c_str()); ++bad; continue; }
        if (q.encodeFrameRaw() != ps[i].encodeFrameRaw()) { std::printf("FAIL txt rt at %zu %s\n", i, ps[i].address().name().c_str()); ++bad; }
        if (q.payload() != ps[i].payload()) { std::printf("FAIL txt payload at %zu\n", i); ++bad; }
    }
    std::printf("frames %zu  payload sizes:", ps.size());
    for (auto& kv : sizes) std::printf(" %zu(x%d)", kv.first, kv.second);
    std::printf("\nbad=%d\n", bad);
    return bad == 0 ? 0 : 1;
}
