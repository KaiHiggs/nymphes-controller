# `nymphes-cli` — headless preset browser and saver

Built by lane L2. The wire codec is **L4's `core/`** (`<nymphes/Codec.h>`), the
single shipped implementation per adopted votes `f066755d8590`,
`441276755365`, `4a6410d80fc5` and the superseding `2ec8c18bf843` / `9451d0f31582`.
This directory is the front-end only: argument parsing, file formats, device path.

## Build

The top-level `CMakeLists.txt` globs `cli/*.cpp` into the `nymphes-cli` console
app and links `nymphes_core`. To build it standalone, without JUCE:

```sh
c++ -std=c++17 -O2 -I<core>/include -Icli cli/main.cpp <core>/src/Codec.cpp \
    -o nymphes-cli
```

`cli/gen/nymphes_params_gen.h` is **generated** from `params/params.json` by
`tools/gen_cc_header.py`; never edit it by hand (`--check` fails when it drifts).

## Commands

```
nymphes-cli params   [--group G] [--json]           the 82 MIDI CCs
nymphes-cli list     <bank.syx>... [--json]         slots in a .syx
nymphes-cli show     <bank.syx> [--slot U-D5]       decoded parameters
nymphes-cli verify   <bank.syx>...                  framing, CRC, byte identity
nymphes-cli export   <bank.syx> --slot S --out F.syx|F.txt
nymphes-cli import   <F.syx|F.txt> --slot S [--out OUT.syx] [--append]
nymphes-cli convert  <in.syx|in.txt> --out <out.syx|out.txt>
nymphes-cli device   list
nymphes-cli device   dump  --out FILE  --lock FILE
nymphes-cli device   write --from FILE --slot U-A1 --lock FILE
```

Slot spellings accepted: `U-D5`, `u-d5`, `UD5`, `userD5`, `user-d5`, `F-A1`,
`factoryA1`.

## Rules the binary enforces (each one is a fact, not a preference)

1. **Payload length is data.** Vendor dumps are 2109-byte frames with 1049-byte
   payloads; the live unit sends frames up to 2323 bytes and payloads 1049..1156.
   `verify` prints the observed histogram instead of comparing to a constant, and
   nothing in the code sizes a buffer from 1049.
2. **A slot write preserves bytes.** `device write` re-emits the payload of the
   preset it read (`Preset::encodeFrame(persist=true)` on the decoded payload),
   never a message rebuilt from the semantic view: rebuilding drops the optional
   reverb floats and re-encodes negative chords unsigned, i.e. it would silently
   shrink the preset. Verify read-back by comparing `payload_sha256`, not values.
3. **Factory slots are read-only.** Addressing `F-*` requires
   `--i-know-this-overwrites`; `device write` to a factory slot is refused.
4. **Every device session holds an flock.** `--lock <file>` takes a non-blocking
   exclusive `flock` and refuses to continue if another process holds it. The
   fleet lock file is
   `<gate>/vendor/midi.lock`; the brief's outer `flock ... -c '...'` is still the
   intended wrapper, `--lock` exists so the binary cannot be run unprotected by
   accident.
5. **The factory write flag is never set silently.** `FF=0x01` only from
   `device write`; `export`/`import`/`convert` preserve the flag byte that was
   decoded.

## What is *not* verified by this tool

- The CC↔preset mapping is **lossy**: preset floats are normalised 0..1 but are
  not `cc/127` (F-A1 osc wave = 0.2358719, i.e. CC 29.96). `show` prints
  `(cc~N)` as an approximation only.
- `device dump` uses the documented full-dump request
  (`F0 00 21 35 00 06 02 F7`) and simply collects whatever comes back for
  `--timeout` ms; the device's reply ordering is not asserted.

## Smoke test

`sh tests/run_all.sh` builds this binary and the golden-vector gate against the
shipping codec and runs both corpora, the Python oracle suite and the export /
import / convert / factory-refusal paths.
