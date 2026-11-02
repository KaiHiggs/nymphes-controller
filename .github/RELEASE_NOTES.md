## Changes in v1.0.1

- Added **Controls → MENU → MIDI CH**: select channels 1–16 or **Omni (out 1)**.
  The control shows the saved routing and follows host state changes. Match it
  to the synth's receive channel; preset SysEx is channel-independent.
- Fixed soft-takeover crossings, malformed preset/text parsing, CLI verification
  and file-error handling, and MIDI stream handling following an external review.
- CC send failures now report an error instead of being silently ignored.
- Changed the displayed plug-in creator to **cleanroom**, keeping the existing
  plug-in identifiers for project compatibility.

The channel control is covered by UI-to-engine-to-MIDI-sink tests. The original
report that sliders did not change the synth still needs hardware confirmation;
this release does not claim it is conclusively resolved. Some review findings
remain deferred.

Unofficial controller for the Dreadbox Nymphes; not affiliated with Dreadbox.

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
