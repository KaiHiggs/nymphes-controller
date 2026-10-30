// plugin/cc/CCEngine.h  —  L1
//
// The CC engine: the value store, the MIDI-learn map and the soft-takeover ramp
// that sit between "a control moved" and "a CC byte goes out to the Nymphes".
//
// The Nymphes has one CC per parameter (see CcMapData.h, 82 of them), so the
// engine is addressed by the *Nymphes* CC number. A learned controller CC is an
// alias that resolves to one of those.
//
// Ownership (fleet vote 6186a3295c47): the 82-CC *data* is L3's midi/CCMap.h;
// this engine and its use of the data are L1's. There is deliberately no second copy
// of the table here — plugin/cc/CcSpec.cpp is only a view over midi/CCMap.h.

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
    void setMidiChannel (int oneBasedChannel);        // 0 = omni
    int  getMidiChannel() const noexcept              { return midiChannel; }
    void attachToProcessor (juce::AudioProcessor&);   // optional: only for host param sync
    void detachFromProcessor();

    // --- values -----------------------------------------------------------
    int  getValue (int nymphesCc) const;
    void setValue (int nymphesCc, int value);         // 0..127 clamped; emits unless suppressed
    void setValueFromDevice (int nymphesCc, int value); // no emit: the synth told us
    void refreshAll();                                // emit every non-default value
    void setEmitEnabled (bool shouldEmit) noexcept    { emitEnabled = shouldEmit; }
    bool isEmitEnabled() const noexcept               { return emitEnabled; }

    // --- MIDI learn -------------------------------------------------------
    void beginLearn (int nymphesCc);
    void cancelLearn();
    bool isLearning() const noexcept                  { return learnTarget >= 0; }
    int  getLearnTarget() const noexcept              { return learnTarget; }
    int  getLearnedCc (int nymphesCc) const;          // -1 when not learned
    void clearLearn (int nymphesCc);
    void clearAllLearn();
    /** Called with the learned CC number when a learn completes (may be -1). */
    std::function<void (int nymphesCc, int learnedCc)> onLearnComplete;

    // --- soft takeover ----------------------------------------------------
    void setTakeover (int nymphesCc, Takeover mode);
    Takeover getTakeover (int nymphesCc) const;
    void setDefaultTakeover (Takeover mode) noexcept  { defaultTakeover = mode; }
    Takeover getDefaultTakeover() const noexcept      { return defaultTakeover; }
    /** True while a soft-takeover control has not yet reached the current value. */
    bool isWaitingForTakeover (int nymphesCc) const;

    // --- incoming ---------------------------------------------------------
    void handleIncomingMidi (const juce::MidiMessage&);

    // --- state ------------------------------------------------------------
    std::unique_ptr<juce::XmlElement> getState() const;
    void setState (const juce::XmlElement&);
    static constexpr const char* stateTag = "NYMPHES_CC_ENGINE";

    // --- introspection for the UI ----------------------------------------
    /** Bit mask of the 82 parameters, set when the value differs from the default. */
    bool isModified (int nymphesCc) const;
    /** The Nymphes CC that a given controller CC currently drives, or -1. */
    int  resolveIncomingCc (int incomingCc) const;

private:
    struct Param
    {
        int value = 0;            // what the synth should be at
        int learnedCc = -1;       // controller CC alias, -1 = none
        int lastRawFromControl = -1; // for the soft-takeover crossing test
        bool waitingForTakeover = false;
        bool modified = false;
        Takeover takeover = Takeover::jump;
    };

    void emitCc (int nymphesCc, int value);
    void notifyValueChanged (int nymphesCc, int value);
    bool applySoftTakeover (int nymphesCc, int rawValue);

    std::array<Param, 128> params {};
    int learnTarget = -1;
    int midiChannel = 0;
    bool emitEnabled = true;
    Takeover defaultTakeover = Takeover::jump;
    MidiSink sink;
    juce::AudioProcessor* attachedProcessor = nullptr;

    // Cached so the state blob is stable and diffable.
    static int defaultFor (int nymphesCc);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CCEngine)
};

} // namespace nymphes
