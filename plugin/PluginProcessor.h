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

#include "midi/MidiPort.h"
#include "plugin/cc/CCEngine.h"
#include "preset/Preset.h"
#include "preset/PresetBank.h"

namespace nymphes
{
class CCEngine;
}

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

    const nymphes::Preset& getCurrentPreset() const     { return currentPreset; }

    /** FF=0x00: loads/auditions on the synth, never writes flash. */
    void loadPresetToDevice (const nymphes::Preset&);
    /** FF=0x01 on a USER slot only. Returns false and fills getLastError(). */
    bool saveToUserSlot (const nymphes::Preset&, nymphes::Slot);

    juce::String getLastError() const                   { return lastError; }

    juce::ChangeBroadcaster& presetListChanged()        { return presetBroadcaster; }
    juce::ChangeBroadcaster& deviceStateChanged()       { return deviceBroadcaster; }

    //--- actions the browser UI needs -----------------------------------------
    /** Arms capture of preset SysEx from the device. The Nymphes emits its presets
        when its own MENU dump function is used; there is no verified host-side
        "request full dump" message in the prior notes, so this deliberately does
        not invent one. */
    void requestFullDump();
    bool importSyx (const juce::File&, juce::String& error);
    bool exportSyx (const juce::File&, juce::String& error);

    /** "open" / "closed" plus the endpoint name, for the editor's device bar. */
    juce::String getDeviceStatus() const                { return deviceStatus; }
    void openMidiPort (const juce::String& nameOrEmpty);

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
    bool producesMidi() const override                  { return true; }
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
    void onChannelFromDevice (const nymphes::ChannelMessage&);
    void sendFrame (const std::vector<uint8_t>& frame);
    void setError (const juce::String&);
    void updateDeviceStatus();
    juce::String paramIdForCc (int cc) const;
    static int ccForParamId (const juce::String& id);

    juce::AudioProcessorValueTreeState apvts;
    nymphes::CCEngine   ccEngine;
    nymphes::MidiPort   midi;
    nymphes::PresetBank bank;

    nymphes::Preset currentPreset;
    juce::ChangeBroadcaster presetBroadcaster, deviceBroadcaster;

    juce::String lastError, deviceStatus;
    juce::String requestedPort;

    bool   updatingFromEngine = false;
    bool   capturing = false;

    // Read-back verification state for a slot write in flight.
    bool                  writeInFlight = false;
    nymphes::Slot         writeSlot;
    std::vector<uint8_t>  writeSentPayload;      // payload bytes we asked the device to store
    nymphes::Preset       writeOriginalPreset;   // the slot's contents before the write
    bool                  writeOriginalKnown = false;
    juce::int64           writeDeadlineMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NymphesControllerAudioProcessor)
};
