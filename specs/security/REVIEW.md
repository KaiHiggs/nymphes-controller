# v1.0.1 release security spot-check

Scope: MidiChannelSelector, CCEngine::setMidiChannel notification, ParamPanel
wiring, CMake version/vendor metadata and regression target. Earlier OCR fixes
are recorded in commit 365fc50; this is not a renewed security audit of them.

- Channel input is a closed ComboBox choice (1–16 or the existing Omni mode).
  CCEngine validates the range again. Routing changes send no MIDI by themselves.
- No new file/network/shell input, dependency, credential handling or device writes.
- No new sensitive data is packaged in this change.
- Selector unregisters its engine listener on destruction. State-to-UI updates use
  dontSendNotification to avoid callback feedback. The broadcaster is asynchronous.
- No HIGH findings in this scoped review. Hardware diagnosis remains unverified.
