# L1 patch — JUCE plugin shell, CC engine + map view, browser UI, CMake, REPORT

L1 owns, per adopted vote 6186a3295c47: the JUCE editor/browser UI, the CC engine,
`CMakeLists.txt`, `REPORT.md` and `install.sh`. L3 owns `preset/`, `midi/`,
`cli/` and the AudioProcessor API; L4 owns `core/` (the shipping codec).

## What is here

```
CMakeLists.txt        the whole build: core lib, plugin, CLI, tests, oracle
acceptance.sh         the 16-check acceptance run (build nothing; verifies everything)
plugin/PluginProcessor.{h,cpp}
                      written by L1 because no AudioProcessor existed in any
                      worktree and the VST3 cannot link without one. L3 owns the
                      API (frozen in fleet message 6f4dd5f064be); review or take
                      the file over.
plugin/cc/CCEngine.*  value store, MIDI learn, soft takeover, state blob
plugin/cc/CcSpec.*    a UI-shaped view over midi/CCMap.h — NOT a second copy of
                      the CC data
plugin/ui/            editor, preset browser panel, 82-control parameter panel
tests/ccmap_test.cpp  asserts the CC map and the UI view agree
tools/extract_ccmap.py, tools/ccmap.json
                      how the 82-CC map was reconciled across three sources, with
                      per-field provenance
REPORT.md, install.sh
```

## How to assemble (the integration layout)

`worktrees/integration` already has this assembled and building. To rebuild it from
scratch:

```sh
I=worktrees/integration
rsync -a worktrees/L3/preset worktrees/L3/midi worktrees/L3/cli "$I"/
rsync -a exchange/L4.core/ "$I"/core/
rsync -a worktrees/L1/plugin worktrees/L1/tests worktrees/L1/tools \
         worktrees/L1/CMakeLists.txt worktrees/L1/REPORT.md \
         worktrees/L1/install.sh "$I"/
cd "$I" && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`worktrees/L1` on its own does **not** configure: `preset/` and `midi/` are L3's files
and `core/` is L4's, so a lane tree is a patch, not a project. That is why the exchange
mirror is the file list above rather than a second copy of other lanes' work.
