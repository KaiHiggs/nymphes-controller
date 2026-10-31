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

`cli/gen/nymphes_params_gen.h` is **generated**, but neither its generator
(`tools/gen_cc_header.py`) nor its input (`params/params.json`) is present in this
tree, and no source file includes it today. The checked-in header is therefore the
only copy we have; it carries its own compile-time integrity checks, and
`tests/params_gen_test.cpp` is its only consumer. Do not hand-edit the tables; the
asserts and the test are the exception (they belong in the generator if it returns).

## Commands

```
file commands:
  verify <file.syx>                   framing, CRC, structure and re-encode report
  list <file.syx>                     list the presets in a dump
  show <file.syx> [--index N]          every named parameter of one preset
  extract <in.syx> --index N --out F  write one preset as its own .syx
  split <in.syx> --outdir DIR         one .syx per preset
  merge <a.syx> [b.syx ...] --out F   concatenate dumps into one .syx
  txt <in.syx> [--index N] [--out F]  human-readable parameter text
  export-txt <in.syx> --out F         tab-separated table of all presets

device commands (each takes --lock <file>):
  devices                             list MIDI endpoints
  ccmap                               print the 82-CC parameter map
  read --slot A1 [--out F]            load that slot, capture what the device sends
  write <file.syx> --slot A1 --yes    write a USER slot, read back, restore the original
  capture --seconds N --out F         record a MENU-driven dump
  dump-request                        send the documented full-dump request
  cc <cc> <value> [--channel N]       send one CC
  panic [--channel N]                 all-sound-off + all-notes-off
```

`--channel` is 1..16 (validated before a session opens). `write` also takes
`--no-restore`. Sending a CC or a SysEx message that is not a preset frame is the
only way this tool touches a device other than `read`/`write`/`capture`.

## Rules the binary enforces (each one is a fact, not a preference)

1. **Payload length is data.** Vendor dumps are 2109-byte frames with 1049-byte
   payloads; the live unit sends payloads 1049..1156. `verify` prints the observed
   sizes instead of comparing to a constant, and nothing sizes a buffer from 1049.
2. **A slot write preserves bytes.** `write` re-emits the payload it decoded,
   never a message rebuilt from the semantic view: rebuilding drops the optional
   reverb floats and re-encodes negative chords unsigned, i.e. it would silently
   shrink the preset. Read-back is compared on payload bytes, verbatim.
3. **Factory slots are read-only.** `write` refuses `--page` other than `user`
   before a byte is sent, and the frame is encoded through
   `encodeForDevice(persist=true, allowFactoryWrite=false)`, which refuses a
   FACTORY address.
4. **Every device session holds an flock.** `--lock <file>` takes a non-blocking
   exclusive `flock(2)` and refuses to continue if another process holds it
   (macOS has no `flock(1)` binary, so the lock is taken in-process). A device
   command without `--lock` is a usage error.
5. **The factory write flag is never set silently.** `FF=0x01` only comes from
   `write`; the file commands preserve the flag byte that was decoded.

## What is *not* verified by this tool

- The CC↔preset mapping is **lossy**: preset floats are normalised 0..1 but are
  not `cc/127` (F-A1 osc wave = 0.2358719, i.e. CC 29.96). `show` prints
  `(cc~N)` as an approximation only.
- `device dump` uses the documented full-dump request
  (`F0 00 21 35 00 06 02 F7`) and simply collects whatever comes back for
  `--timeout` ms; the device's reply ordering is not asserted.

## Smoke test

`cd build && ctest` runs `tests/cli_smoke_test.cpp`, which drives this binary
end-to-end on fixtures it builds itself (no device, no corpus): the verify gate over
a file with one corrupt frame, the list column, the write plan without `--yes`, the
unwritable-output and bad-`--outdir` error paths, and `--channel` validation.
`sh core/run_tests.sh [vendor-dir]` runs the codec self-test and the corpus runner
(the vendor dumps are not in this tree, so that gate skips unless you pass a
directory holding both).
