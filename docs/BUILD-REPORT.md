# NymphesController — preset browser/saver + CC controller VST3

A Dreadbox Nymphes controller that runs as a **VST3** (and a **Standalone** app) in
Ableton Live 12, plus a headless **CLI** preset browser/saver. Built and tested against
the operator's unit in this run.

Everything below was executed, not planned. Where something is a limitation it says so
in *Known limitations* rather than being left out.

Accepted by the fleet in vote a6b1a2142024 (4 AGREE, no OBJECT).

---

## 1. Build

```sh
cd worktrees/integration
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Requirements that are already on this machine: CMake 4.4.4, Apple clang, JUCE 8.0.12 (a local
checkout via `-DJUCE_DIR=...`, otherwise fetched). No package installs, no
network, no Python needed for the build.

Verified configure output (abridged):

```
-- Nymphes: JUCE          /path/to/JUCE
-- Nymphes: shipped codec core
-- Nymphes: codec = core/ (L4); preset/ is the delegating facade
-- Nymphes: NYMPHES_CORE_SOURCES = 6 files
-- Nymphes: NYMPHES_PLUGIN_SOURCES = 7 files
-- Nymphes: NYMPHES_CLI_SOURCES = 1 files
-- Nymphes: NYMPHES_TEST_SOURCES = 1 files
```

Verified build output:

```
[100%] Built target NymphesController_VST3
[100%] Built target NymphesController_Standalone
[100%] Built target nymphes-cli
[100%] Built target nymphes-tests
[100%] Built target nymphes-oracle
```

Artefacts:

| target | path |
|---|---|
| VST3 | `build/NymphesController_artefacts/Release/VST3/NymphesController.vst3` |
| Standalone | `build/NymphesController_artefacts/Release/Standalone/NymphesController.app` |
| CLI | `build/bin/nymphes-cli` |
| tests | `build/bin/nymphes-tests`, `build/bin/nymphes-oracle` |

Install into Live: `./install.sh` (copies the VST3 to `~/Library/Audio/Plug-Ins/VST3`).
Ableton will find it after a rescan. Nothing is installed by the build itself.

### One-command acceptance

```sh
cd worktrees/integration
./acceptance.sh
```

`acceptance.sh` builds nothing; it checks the artefacts, runs `ctest`, verifies all
three corpora through the shipping CLI, and asserts the variable-length behaviour.
The corpora are shared roots, not copies inside this tree, so the script takes the gate
root and defaults to it:

```sh
GATE=/path/to/corpus-root
# corpora used:
#   $GATE/vendor/Nymphes_Factory_Presets_Factory_Bank.syx
#   $GATE/vendor/Nymphes_Factory_Presets_User_Bank.syx
#   $GATE/evidence/L4-device/device_dump_run1.syx
```

Last run: **ACCEPTANCE PASSED** — 16 checks, every one green.

---

## 2. Test output

```
$ ctest --test-dir build --output-on-failure
    Start 1: ccmap_test
1/3 Test #1: ccmap_test .......................   Passed    0.13 sec
    Start 2: preset_safety_test
2/3 Test #2: preset_safety_test ...............   Passed    0.19 sec
    Start 3: nymphes-oracle
3/3 Test #3: nymphes-oracle ...................   Passed    0.05 sec

100% tests passed out of 3
```

`ccmap_test` asserts the CC map the UI is built from: 82 parameters, CC 38 absent,
the modulation-depth runs 31–37 and 39–63 plus 86–89, the table strictly ascending and
duplicate-free, every entry named with a sane range, the eight discrete parameters
(17/22/23/28/29/30/64/68) at their real bounds with value names, and the UI view
(`plugin/cc/CcSpec`) agreeing with the table one-for-one.

`preset_safety_test` runs the shipping codec over the **real corpora**, not synthetic
frames: all 98 live frames decode and are CRC-valid at their own length, `frameSize()`
equals the received size for all 98, `decode -> encodeFrameRaw` is byte-identical for all
98, the 8 distinct payload sizes occur with exactly the measured histogram
(1049×82, 1058×3, 1076, 1103, 1129×8, 1139, 1148, 1156), and a **real vendor factory
frame** cannot be encoded with the write flag while it still encodes as an audition
(`FF=0x00`).

`nymphes-oracle` is the independent codec (L4's `core/`) run as its own executable:
**1083 checks, 0 failures** over the 147 vendor preset messages, byte-identical re-encode.

### Preset corpus, through the shipping CLI

`GATE` is the shared gate root; the corpora are not copies inside this tree.

```
$ nymphes-cli verify $GATE/vendor/Nymphes_Factory_Presets_Factory_Bank.syx
messages  49 decoded, 0 rejected
pages     0 USER, 49 FACTORY
crc       49/49 valid
payload   1049..1049 bytes (0 of 49 not the canonical 1049)

$ nymphes-cli verify $GATE/vendor/Nymphes_Factory_Presets_User_Bank.syx
messages  98 decoded, 0 rejected
pages     49 USER, 49 FACTORY
crc       98/98 valid
payload   1049..1049 bytes (0 of 98 not the canonical 1049)

$ nymphes-cli verify $GATE/evidence/L4-device/device_dump_run1.syx
messages  98 decoded, 0 rejected
pages     49 USER, 49 FACTORY
crc       98/98 valid
payload   1049..1156 bytes (16 of 98 not the canonical 1049)
```

**Why the live corpus matters.** All 147 vendor frames are exactly 1049 bytes, so a
fixed-size codec passes the vendor corpus and then fails on the real synth. The unit
emits payloads from 1049 to 1156 bytes: the four optional reverb floats
(`extra_lfo_2` fields 9–12, `extra_mod_w`/`extra_velo`/`extra_after` fields 5–8) are
present in some presets and absent in others, and a negative `chord_info` int32 takes a
10-byte varint instead of 1. The shipping codec derives the payload size from the frame
and never from a constant. Acceptance therefore requires **both** corpora, and both pass.

---

## 3. Device evidence — user slot write, read-back, restore

Measured on the operator's unit in this run. Full record:
`evidence/L4-device/device_roundtrip_userA1.json`, with the four full dumps beside it
(`device_dump_{run1,pre_userA1,post_write_userA1,post_restore_userA1}.syx`).

| step | result |
|---|---|
| original `userA1` recorded | frame 2269 B, payload 1129 B, CRC ok, payload sha256 `c1c5f81a3c0ef44fac1952f57400f5a5a67ae148c490a758949a939c84be7744` |
| write `FF=01` with one float edited | payload sha256 `6544cdb39456680a854a7da43fa2b966575f8802d6d44c5eba3d214727df5717` |
| read back from the device | payload sha256 `6544cdb3…5717`, **byte-identical**, diff count 0, CRC valid |
| restore original bytes | **byte-identical**, payload sha256 back to `c1c5f81a…7744` |
| post-restore full dump | 98 frames, matches the pre-write baseline |

### The shipped CLI saver, run on the hardware

Three consecutive runs of the **shipped** binary against the unit, each under the
exclusive lock, on the fixed tree:

```
nymphes-cli write /tmp/cli-rt/src_A3.syx --slot A1 --yes --lock <GATE>/vendor/midi.lock
  -> readback byte-identical (payload 1049 B)
  -> restore verified byte-identical to the original
```

That closes the gap the device-harness round trip above left open: the binary in
`build/bin/nymphes-cli`, not just the codec, writes a USER slot, reads it back
byte-identically and restores the original. Independently re-derived from the raw dump
bytes by two other lanes (payload sha256 `c1c5f81a…7744` in pre/run1/post-restore and
`6544cdb3…5717` in post-write). No factory slot, no firmware SysEx.

### The device does not echo, which is why read-back is dump-based

Measured on this unit (L4, same port, same session, listening 3–5 s):

| what was sent | frames heard |
|---|---|
| preset frame `FF=0x01` (write) | **0** |
| same frame `FF=0x00` (load/audition) | **0** |
| full-dump request `F0 00 21 35 00 06 02 F7` | 78 frames in 5 s, all 98 within ~9 s |

So nothing may wait for the synth to echo a preset back. Both the plugin and the CLI
verify a write the same way: send the write frame, then send the **dump request** and
select the frame whose `(AA,BB,PP)` matches the slot, comparing `payload()` byte for
byte. The plugin gives that exchange 12 s and reports `byte identity is UNVERIFIED`
rather than claiming success if the frames do not arrive. An earlier revision waited
2.5 s for an echo that never comes; that was found on hardware and fixed.

Session discipline: the whole session held an exclusive `fcntl.flock` on
`vendor/midi.lock` (macOS has no `/usr/bin/flock` binary, so the lock is taken with
Python's `fcntl.flock(LOCK_EX)` on that exact path), opened only the endpoint whose name
contains `Nymphes`, and never opened the Scarlett 4i4. **No firmware SysEx, no
bootloader interaction, no factory write.**

---

## 4. What the plugin does

**MIDI.** The plugin opens the `Nymphes` CoreMIDI destination itself, by name, from
inside the plugin (`midi/MidiPort.h`), so it works on an Ableton track without depending
on the host's MIDI routing. It also passes MIDI through untouched — it is a controller,
not a synth, and does not swallow the clip. The port is matched **by name**, never by
VID:PID: the device answers to `0483:5740`, which is STMicroelectronics' generic
virtual-COM pair and would match unrelated hardware.

**82 CC parameters.** Every MIDI-controllable parameter, with the device's own names,
ranges and discrete value labels (`midi/CCMap.h`, 82 entries, CC 38 genuinely absent).
Discrete parameters (play mode 0–5, LFO type 0–3, key sync, sustain, legato, modulation
source) are combo boxes with real labels, and the host sees those names too — sending
127 to CC 22 is out of range on this device, not "the last type". The 36
modulation-depth CCs are shared by all four modulation sources and reinterpreted by the
MS selector (CC 30); the UI marks them so a depth edit is never ambiguous.

**MIDI learn and soft takeover.** Every control has an `L` button: arm it, move a knob
on your keyboard, and that controller CC becomes an alias for the parameter. One alias
per parameter, one parameter per alias. In soft-takeover mode a learned physical control
must *cross* the current value before it moves anything, so touching a knob at 3 does not
snap the cutoff to 3 — the strip shows `waiting` until it engages. The learn map travels
in the plugin state.

**Plugin state.** `getStateInformation` writes the APVTS tree, the CC engine's learn map
and the **exact payload bytes** of the current preset (base64). Reopening a session
restores the bytes that were being edited, not a re-derived approximation — which matters
because a semantic rebuild drops the optional reverb containers and re-encodes negative
chord varints.

**Preset browser.** All 98 slots (49 USER then 49 FACTORY, 7 banks × 7 programs each).
Audition sends `FF=00`, which loads on the synth and writes nothing. Save sends `FF=01`
to a **USER slot only**; a FACTORY row disables the button and the action refuses even if
invoked directly. Import/export `.syx`. Presets carry no name — the payload is a CRC plus
a 22-field protobuf with no string in it — so a slot is identified as `USER A1` /
`FACTORY C3`, which is the truth rather than an invented label.

**Headless CLI.** `nymphes-cli` shares the same codec: `verify`, `list`, `show`,
`extract`, `split`, `merge`, `txt`, `export-txt`, plus device commands (`devices`,
`ccmap`, `read`, `write`, `capture`, `dump-request`, `cc`, `panic`). It builds without
JUCE. Its `write` refuses factory slots, requires `--yes`, verifies read-back and
restores the original.

---

## 5. Layout and who wrote what

```
CMakeLists.txt          one tree, three targets + tests + oracle
core/                   L4 — the shipping codec (variable-length payload)
preset/                 L3 — delegating facade: PresetBank (98 slots), named view
midi/                   L3 — CCMap.h (82 CCs, single owner of the data), MidiPort
plugin/PluginProcessor.*    L3 owns this API; written by L1 when no AudioProcessor
                            existed in any worktree (flagged to L3 for review)
plugin/cc/              L1 — CC engine (learn, soft takeover, state) + CC map view
plugin/ui/              L1 — editor, browser panel, parameter panel
cli/                    L3 — headless front-end
tests/                  L1 — CC map / UI view tests (L6's vendor corpus test and
                            L4's devcheck run alongside; see exchange/)
tools/                  L1 — ccmap.json provenance + the generator for it
```

One codec ships. `core/` is the only wire implementation; `preset/` is a facade with no
frame/CRC/protobuf logic of its own. `preset/` and `core/` are never linked as two
codecs: `core/src/Codec.cpp` is the single implementation, and a build option
(`-DNYMPHES_SHIPPED_CODEC=preset`) exists only as an escape hatch. `nymphes-oracle`
links L4's codec separately so it can never collide with the shipping codec on
`nymphes::crc8` / `nymphes::Preset`.

---

## 6. Known limitations

1. **The browser list is empty until presets arrive.** The plugin does not ship the
   vendor dumps (they are the operator's downloads, not redistributable), and a preset
   frame carries no name. Press *Dump* with the synth connected — it sends the vendor's
   documented request `F0 00 21 35 00 06 02 F7` and ingests every frame that decodes —
   or import a `.syx`. Slots the plugin has not seen read as empty and are not writable
   with zeros.
2. **The full-dump request is the documented type-`0x02` message, not verified
   end-to-end here.** The request byte sequence comes from the vendor documentation and
   from `prior/`; what this run verified is that the device's *response* decodes (98/98
   frames of the captured dump). If a unit ignores the request, use its own MENU dump
   function with the port open — capture mode ingests that too.
3. **Read-back verification is asynchronous and dump-based.** The device never echoes a
   preset (measured: 0 frames for both `FF=0x01` and `FF=0x00`), so after a slot write the
   plugin sends the full-dump request and matches the target frame, with a 12 s deadline.
   If the frames do not arrive it **restores the captured original** and says so; it
   never leaves unverified bytes in a slot it had recorded. The byte-identical round trip
   in §3 was measured twice: by the device harness through the same codec, and by the
   shipped `nymphes-cli` binary against the unit. The plugin's own path through Ableton
   was not run against hardware in this session, and is listed here rather than claimed.
4. **The Standalone build is a wrapper**, useful for checking the editor and the port
   without a DAW. It does not host other plugins.
5. **Ableton Live 12 Suite on this Mac was not launched by this fleet.** The VST3 bundle
   is built and installed by `install.sh`; insertion into a Live track is the operator's
   step. Everything the plugin needs (CoreMIDI direct, VST3 + Standalone formats, editor,
   state) is present, but "it appears in Live's browser" was not exercised here.
6. **No firmware work.** No firmware SysEx, no bootloader path, no update message type.
   The only messages this software can construct are the preset frame (type `00 06`), the
   dump request (type `02`) and single CC bytes.
7. **CC↔parameter mapping is not read from firmware.** `prior/L2-firmware.md` §6 shows
   the CC dispatch table is not in the application region, so the 82-entry map is merged
   from three public sources with per-field provenance in `tools/ccmap.json`. The 28
   `basic_inputs` CCs decoded from the preset protobuf cross-check it and agree.
8. **`.txt` round-trip is the codec's, not the plugin's.** The plugin exports `.syx`; the
   CLI does `.txt` in both directions.

---

## 6a. Defects found in review and fixed before acceptance

Two one-line defects in `plugin/PluginProcessor.cpp` were found by L2 and confirmed by
L3 after the build was otherwise green. Both are fixed, rebuilt and re-accepted:

1. `saveToUserSlot` cleared `lastError` three lines after setting the "read-back dump
   request could not be sent" message, so that failure was silent.
2. The read-back **timeout** branch reported the timeout but did not restore the slot,
   while the read-back **mismatch** branch did. That is the one failure mode the
   "record the original bytes and restore them" rule exists to prevent, so it now
   mirrors the mismatch branch: if the original was captured, it is sent back, and the
   error says which of the three outcomes happened.

## 7. Safety rules this software enforces in code

- Factory slots are never written: the UI disables it, `saveToUserSlot` refuses
  `AA=0x01`, and the CLI refuses it before a byte is sent.
- Audition is `FF=0x00` and cannot write flash.
- The original bytes of any slot about to be written are recorded first, and are sent
  back if the read-back disagrees **or times out** — the two branches behave the same.
- No firmware SysEx, no bootloader interaction, no update message type.
- One process owns the MIDI port at a time; device sessions hold an exclusive lock on
  `vendor/midi.lock`.
