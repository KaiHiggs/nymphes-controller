// plugin/cc/CCEngine.h  —  L1
//
// The CC engine: the value store, the MIDI-learn map and the soft-takeover ramp
// that sit between "a control moved" and "a CC byte goes out to the Nymphes".
//
// The Nymphes has one CC per parameter (see midi/CCMap.h, 82 of them), so the
// engine is addressed by the *Nymphes* CC number. A learned controller CC is an
// alias that resolves to one of those.
//
// Ownership (fleet vote 6186a3295c47): the 82-CC *data* is L3's midi/CCMap.h;
// this engine and its use of the data are L1's. There is deliberately no second copy
// of the table here — plugin/cc/CcSpec.cpp is only a view over midi/CCMap.h.
//
// Threading model (this is a contract, not an implementation detail):
//   * THE SAME ENGINE IS REACHED FROM THREE THREADS. The processor calls
//     handleIncomingMidi() from processBlock() (audio thread), setValue() from
//     parameterChanged() (message/host thread) and setValueFromDevice() from the
//     MIDI port callback (CoreMIDI/WinMM thread); the UI and getStateInformation()
//     call the getters from the message thread.
//   * Every member function below is safe to call from any of those threads. All
//     state is guarded by one recursive lock (stateLock).
//   * The lock is never held while your code runs: `sink` and `onLearnComplete`
//     are invoked after it is released, so a callback may call back into the
//     engine. The consequence is that `sink` is called on whichever thread
//     delivered the event — a preset load on the MIDI thread emits from the MIDI
//     thread — and the caller must treat it as such.
//   * handleIncomingMidi() is called from the audio thread, so it allocates only
//     what a lock and a MidiMessage require; sendChangeMessage() is asynchronous
//     and coalesces.

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <array>
#include <functional>

#include "midi/CCMap.h"

namespace juce { class AudioProcessor; }

namespace nymphes
{

class CCEngine final : public juce::ChangeBroadcaster
{
public:
    // Where CC bytes go once the engine decides to emit them. The processor
    // wires this to its CoreMIDI port; tests wire it to a vector.
    // Called on the thread that caused the emit (see the threading model above).
    using MidiSink = std::function<void (const juce::MidiMessage&)>;

    // How a parameter gets from "host/UI asked for X" to "the knob is at X".
    enum class Takeover
    {
        jump,   ///< always follow the newest value (default for the UI + host)
        soft    ///< a learned physical control must cross the value before it moves it
    };

    CCEngine();
    ~CCEngine() override;

    // --- wiring -----------------------------------------------------------
    void setMidiSink (MidiSink sink);
    /** 1..16 select the receive/transmit channel. 0 (or an invalid value) means
        receive on all channels, transmit CCs on 1. Preset SysEx is independent of
        this setting. Notifies the editor asynchronously; emits no MIDI. */
    void setMidiChannel (int oneBasedChannel);
    int  getMidiChannel() const noexcept;
    /** Optional host-parameter sync hook. Currently only recorded (never
        dereferenced). Contract: `detachFromProcessor()` must run before the
        processor is destroyed; the pointer is non-owning and is not observed. */
    void attachToProcessor (juce::AudioProcessor&);   // optional: only for host param sync
    void detachFromProcessor();

    // --- values -----------------------------------------------------------
    int  getValue (int nymphesCc) const;
    void setValue (int nymphesCc, int value);         // 0..127 clamped; emits unless suppressed
    void setValueFromDevice (int nymphesCc, int value); // no emit: the synth told us
    void refreshAll();                                // emit every non-default value
    void setEmitEnabled (bool shouldEmit) noexcept;
    bool isEmitEnabled() const noexcept;

    // --- MIDI learn -------------------------------------------------------
    void beginLearn (int nymphesCc);
    void cancelLearn();
    bool isLearning() const noexcept;
    int  getLearnTarget() const noexcept;
    int  getLearnedCc (int nymphesCc) const;          // -1 when not learned
    void clearLearn (int nymphesCc);
    void clearAllLearn();
    /** Called with the learned CC number when a learn completes. Only a real
        completion is reported (learnedCc >= 0); cancelLearn() does NOT call this
        with -1, so a client that needs cancel notifications must watch
        isLearning()/the change message. Runs on the thread that delivered the
        learning CC (audio/MIDI thread), outside the engine lock. */
    std::function<void (int nymphesCc, int learnedCc)> onLearnComplete;

    // --- soft takeover ----------------------------------------------------
    void setTakeover (int nymphesCc, Takeover mode);
    Takeover getTakeover (int nymphesCc) const;
    /** The mode applied to parameters that are (re)initialised: setState() and
        newly restored entries. It does NOT retroactively change a parameter that
        already has an explicit mode; use setTakeover() for those. */
    void setDefaultTakeover (Takeover mode) noexcept;
    Takeover getDefaultTakeover() const noexcept;
    /** True while a soft-takeover control has not yet reached the current value. */
    bool isWaitingForTakeover (int nymphesCc) const;

    // --- incoming ---------------------------------------------------------
    void handleIncomingMidi (const juce::MidiMessage&);

    // --- state ------------------------------------------------------------
    std::unique_ptr<juce::XmlElement> getState() const;
    /** Restores the blob written by getState(). Validation: unknown/out-of-range
        `cc` children are dropped; `value` is clamped to 0..127; `learnedCc`
        outside 0..127 becomes -1 and a duplicate alias is cleared (one alias per
        parameter); `midiChannel` is passed through setMidiChannel(), so an
        impossible value falls back to omni; `takeover` defaults to the blob's
        defaultTakeover. Restored values are NOT emitted to the synth (the synth
        is the source of this state), and the `version` attribute is not yet
        used to reject an unknown future format. No success indication: a blob
        with a foreign tag is ignored. */
    void setState (const juce::XmlElement&);
    static constexpr const char* stateTag = "NYMPHES_CC_ENGINE";

    // --- introspection for the UI ----------------------------------------
    /** True when the given parameter's value differs from its default. */
    bool isModified (int nymphesCc) const;
    /** The Nymphes CC that a given controller CC currently drives, or -1. */
    int  resolveIncomingCc (int incomingCc) const;

private:
    struct Param
    {
        int value = 0;            // what the synth should be at
        int learnedCc = -1;       // controller CC alias, -1 = none
        int lastRawFromControl = -1; // last raw position seen; -1 = no sample yet
                                     // (soft takeover's first message only sets this)
        bool waitingForTakeover = false;
        bool modified = false;
        Takeover takeover = Takeover::jump;
    };

    void emitCc (int nymphesCc, int value);
    void notifyValueChanged (int nymphesCc, int value);
    /** True when the raw move crosses (or lands on) the stored value.
        Call with stateLock held; `nymphesCc` must be in 0..127. */
    bool applySoftTakeover (int nymphesCc, int rawValue);
    /** resolveIncomingCc() without taking the lock (call with stateLock held). */
    int  resolveIncomingCcUnlocked (int incomingCc) const;

    std::array<Param, 128> params {};
    int learnTarget = -1;
    int midiChannel = 0;
    bool emitEnabled = true;
    Takeover defaultTakeover = Takeover::jump;
    MidiSink sink;
    juce::AudioProcessor* attachedProcessor = nullptr;

    // Guards every member above. Recursive, so a helper may call a public
    // function that also locks. Never held across a user callback.
    mutable juce::CriticalSection stateLock;

    // Cached so the state blob is stable and diffable.
    static int defaultFor (int nymphesCc);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CCEngine)
};

} // namespace nymphes
