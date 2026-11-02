// plugin/PluginProcessor.h  —  written by L1 because no AudioProcessor existed
// in any worktree and the VST3 cannot exist without one. L3 owns this file by
// vote; L3 should review or take it over, and the fleet should treat L1 as the
// author only until L3 confirms.
//
// What it is: the AudioProcessor that ties three things together.
//
//   * the CC engine  (plugin/cc/CCEngine)   — value store, learn, soft takeover
//   * the CoreMIDI port (midi/MidiPort)     — opens the "Nymphes" endpoint by
//                                             name, directly, so the plugin
//                                             works on an Ableton track without
//                                             depending on the host's routing
//   * the preset bank (preset/PresetBank)   — 98 slots over the shipped codec
//
// The API in the "editor-facing" block below was frozen by L3 in fleet message
// 6f4dd5f064be; plugin/ui/ProcessorAccess.h compiles against it.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <memory>

#include "midi/MidiPort.h"
#include "plugin/cc/CCEngine.h"
#include "preset/Preset.h"
#include "preset/PresetBank.h"

class NymphesControllerAudioProcessor final : public juce::AudioProcessor,
                                             private juce::AudioProcessorValueTreeState::Listener,
                                             private juce::Timer
{
public:
    NymphesControllerAudioProcessor();
    ~NymphesControllerAudioProcessor() override;

    //==========================================================================
    // Editor-facing API (frozen by L3, message 6f4dd5f064be)
    //==========================================================================
    juce::AudioProcessorValueTreeState& getAPVTS()      { return apvts; }
    nymphes::MidiPort&                  getMidi()       { return midi; }
    nymphes::CCEngine&                  getCCEngine()   { return ccEngine; }
    nymphes::PresetBank&                getBank()       { return bank; }

    /** A reference into this processor. Message thread only (like every getter here):
        the preset is reassigned from editor actions, never from the MIDI thread. */
    const nymphes::Preset& getCurrentPreset() const     { return currentPreset; }

    /** FF=0x00: loads/auditions on the synth, never writes flash. */
    void loadPresetToDevice (const nymphes::Preset&);
    /** FF=0x01 on a USER slot only. Returns false and fills getLastError(). */
    bool saveToUserSlot (const nymphes::Preset&, nymphes::Slot);

    /** Message thread only, like every other getter. */
    juce::String getLastError() const                   { return lastError; }

    juce::ChangeBroadcaster& presetListChanged()        { return presetBroadcaster; }
    juce::ChangeBroadcaster& deviceStateChanged()       { return deviceBroadcaster; }

    //--- actions the browser UI needs -----------------------------------------
    /*  Arms capture of preset SysEx from the device. The Nymphes emits its presets
        when its own MENU dump function is used; the documented full-dump request is
        the reliable path. Returns false (and fills getLastError()) when the request
        could not be sent, so a caller does not have to guess from the error string. */
    bool requestFullDump();
    bool importSyx (const juce::File&, juce::String& error);
    bool exportSyx (const juce::File&, juce::String& error);

    /** "connected: ..." / "no Nymphes port open", for the editor's device bar.
        Message thread only. */
    juce::String getDeviceStatus() const                { return deviceStatus; }
    /** Returns false (and fills getLastError()) when the port could not be opened. */
    bool openMidiPort (const juce::String& nameOrEmpty);

    //==========================================================================
    // AudioProcessor
    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return "NymphesController"; }
    bool acceptsMidi() const override                   { return true; }
    // Nothing is ever added to `midiMessages` in processBlock(), so this processor
    // does not produce MIDI. Claiming it does invites a host to merge this track's
    // (unchanged) output into a destination that is already the Nymphes, which would
    // send every CC and preset frame twice.
    bool producesMidi() const override                  { return false; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    void wireMidi();
    void onSysExFromDevice (const std::vector<uint8_t>& frame);
    /** Queue one short channel message for the message thread (callable from the
        audio thread: try-lock, fixed buffer, no allocation, drops on contention). */
    void queueCc (std::uint8_t status, std::uint8_t data1, std::uint8_t data2);
    /** Message thread: send every queued channel message through the port. */
    void drainCcQueue();
    void setError (const juce::String&);
    void updateDeviceStatus();
    juce::String paramIdForCc (int cc) const;
    static int ccForParamId (const juce::String& id);

    //==========================================================================
    // Threading model (this is why there is no lock on the members below)
    //==========================================================================
    // Everything from here down - currentPreset, lastError, deviceStatus, the write
    // verification block, the bank - is touched ONLY on the message thread. The
    // CoreMIDI callbacks installed by wireMidi() marshal to it through
    // juce::MessageManager::callAsync, and the timer, the editor and the change
    // broadcasters all run there too. The one hazard that IS real is a callback that
    // is already queued when the processor is destroyed: `aliveToken` below is set to
    // false in the destructor and checked at the top of every posted lambda.
    // The audio thread touches exactly two things: ccEngine.handleIncomingMidi() in
    // processBlock(), and the outbound CC queue.
    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    nymphes::CCEngine   ccEngine;
    nymphes::MidiPort   midi;
    nymphes::PresetBank bank;

    nymphes::Preset currentPreset;
    juce::ChangeBroadcaster presetBroadcaster, deviceBroadcaster;

    juce::String lastError, deviceStatus;
    juce::String requestedPort;

    // Written on the message thread (timerCallback) and read by parameterChanged(),
    // which a host may call from an automation thread, so it is atomic.
    std::atomic<bool> updatingFromEngine { false };

    // Life time token for queued message-thread work (see the threading model above).
    std::shared_ptr<std::atomic<bool>> aliveToken { std::make_shared<std::atomic<bool>> (true) };

    // Outbound channel messages from the audio thread. Fixed size, guarded by a
    // try-lock: a full queue or a contended lock drops the message instead of
    // allocating or blocking on the audio thread.
    static constexpr int kCcOutCapacity = 128;
    juce::SpinLock ccOutLock;
    std::array<std::uint8_t, (std::size_t) kCcOutCapacity * 3> ccOut {};
    int ccOutCount = 0;   // guarded by ccOutLock
    bool ccSendFailureReported = false;   // message thread (drainCcQueue) only
    // queueCc never blocks and never allocates, so it can also DROP: the message
    // thread is draining, or the queue is full for one tick. A drop is counted here
    // and reported by drainCcQueue, because a silently lost CC is exactly the
    // symptom the user cannot distinguish from a broken fader.
    std::atomic<int> ccDroppedCount { 0 };   // any thread increments, message thread consumes
    bool ccDropReported = false;             // message thread (drainCcQueue) only

    // Read-back verification state for a slot write in flight. Message thread only.
    bool                  writeInFlight = false;
    nymphes::Slot         writeSlot;
    std::vector<std::uint8_t> writeSentPayload;   // payload bytes we asked the device to store
    nymphes::Preset       writeOriginalPreset;    // the slot's contents before the write
    bool                  writeOriginalKnown = false;
    juce::int64           writeDeadlineMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NymphesControllerAudioProcessor)
};
