VST3 plug-in and standalone app for the Dreadbox Nymphes: all 82 CCs on a
panel laid out like the instrument, the hardware's mod-edit workflow, and a
7 × 7 preset browser for the USER and FACTORY pages. Unofficial; not affiliated
with Dreadbox.

| Download | Contents |
|---|---|
| `NymphesController-macOS-VST3.zip` | `NymphesController.vst3`, universal (Apple silicon + Intel), macOS 11+ |
| `NymphesController-macOS-Standalone.zip` | `NymphesController.app`, universal |
| `NymphesController-Windows-VST3.zip` | `NymphesController.vst3`, x64 |
| `NymphesController-Windows-Standalone.zip` | `NymphesController.exe`, x64 |

**macOS:** put the VST3 in `~/Library/Audio/Plug-Ins/VST3/`. The builds are
ad-hoc signed, not notarized; if macOS blocks them, run
`xattr -dr com.apple.quarantine <path>` once on the bundle.

**Windows:** put the `NymphesController.vst3` folder in
`C:\Program Files\Common Files\VST3\`. Windows MIDI ports are exclusive: if
your DAW has the Nymphes enabled as a MIDI port, disable it there so the
plug-in can open it.

The Windows build talks to the synth through WinMM. It is built and tested in
CI, but has not yet been tried against a Nymphes on Windows — reports welcome.
