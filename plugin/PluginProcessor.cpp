// plugin/PluginProcessor.cpp  —  written by L1 (see PluginProcessor.h header note)
//
// Two invariants that were found the hard way and must not regress:
//   1. The device never echoes a preset, so a slot write is verified by sending the
//      full-dump request and matching the frame whose (AA,BB,PP) is the target.
//   2. A failed or timed-out read-back RESTORES the captured original bytes. Reporting
//      a timeout without restoring is the one failure the brief's restore rule exists
//      to prevent.

#include "plugin/PluginProcessor.h"

#include "plugin/cc/CcSpec.h"
#include "plugin/ui/PluginEditor.h"

#include <algorithm>
#include <memory>

namespace
{
    constexpr const char* kStateTag   = "NYMPHES_CONTROLLER";
    constexpr const char* kStateVersion = "1";
    // The device does NOT echo a preset on load or on write (measured on this unit:
    // 0 frames heard in 3-5 s for both FF=0x01 and FF=0x00). The only way to see a
    // slot's contents is to ask for a full dump, which delivers 78 frames in ~5 s and
    // all 98 within ~9 s. So a write is verified by requesting the dump and selecting
    // the frame whose (AA,BB,PP) matches the slot.
    constexpr int kWriteTimeoutMs = 12000;
    constexpr std::uint8_t kDumpRequest[] = { 0xF0, 0x00, 0x21, 0x35, 0x00, 0x06, 0x02, 0xF7 };

    juce::String toJuce (const std::string& s)
    {
        return juce::String::fromUTF8 (s.c_str(), (int) s.size());
    }

    // Host parameter ids are "cc_<number>"; the CC number is the parameter.
    juce::String idForCc (int cc) { return "cc_" + juce::String (cc); }

    // A label list that outlives the parameter: the strings are literals inside
    // midi/CCMap.h, so a vector of views is enough and costs nothing.
    using LabelList = std::vector<std::string_view>;
    LabelList labelsForCc (int cc)
    {
        if (const auto* s = nymphes::ccui::findSpec (cc))
            return s->labels;
        return {};
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
NymphesControllerAudioProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& spec : nymphes::ccui::allSpecs())
    {
        const int cc = (int) spec.cc;

        const juce::String displayName (spec.name.data(), (int) spec.name.size());
        const juce::String displayGroup (spec.group.data(), (int) spec.group.size());

        // Discrete parameters get the device's own names in the host's automation
        // list; continuous ones get the raw 0..127 value. The label list is copied
        // into the lambda because the host may ask for the string at any time.
        auto labels = std::make_shared<LabelList> (spec.labels);

        std::function<juce::String (int, int)> stringFromInt;
        std::function<int (const juce::String&)> intFromString;

        if (! labels->empty())
        {
            stringFromInt = [labels] (int v, int)
            {
                const int i = juce::jlimit (0, (int) labels->size() - 1, v);
                const auto sv = (*labels)[(std::size_t) i];
                return juce::String (sv.data(), (int) sv.size());
            };
            intFromString = [labels] (const juce::String& text)
            {
                for (std::size_t i = 0; i < labels->size(); ++i)
                    if (text == juce::String ((*labels)[i].data(), (int) (*labels)[i].size()))
                        return (int) i;
                return text.getIntValue();
            };
        }

        // CC 7 (amp level) is the one parameter the vendor documents at 127; every
        // other parameter starts at its own minimum.
        const int defaultValue = (cc == 7) ? 127 : spec.minValue;

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { idForCc (cc), 1 },
            displayName,
            spec.minValue,
            spec.maxValue,
            defaultValue,
            displayGroup,
            stringFromInt,
            intFromString));
    }

    return layout;
}

//==============================================================================
NymphesControllerAudioProcessor::NymphesControllerAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", makeLayout())
{
    for (const auto& spec : nymphes::ccui::allSpecs())
        apvts.addParameterListener (idForCc ((int) spec.cc), this);

    // The CC engine reaches the synth through the port this processor owns. The
    // MidiPort is JUCE-free, so the raw bytes of the JUCE message are handed over.
    ccEngine.setMidiSink ([this] (const juce::MidiMessage& m)
    {
        const auto* d = m.getRawData();
        const int n = m.getRawDataSize();
        if (n >= 3)
            midi.sendChannel (d[0], d[1], d[2]);
        else if (n == 2)
            midi.sendChannel (d[0], d[1], 0);
    });
    ccEngine.attachToProcessor (*this);

    // Nothing is learned and nothing is emitted until the port is actually open;
    // emitting into a closed port is a no-op anyway, but this keeps state clean.
    ccEngine.setEmitEnabled (true);

    wireMidi();

    // Try the documented endpoint name. Failure is normal (no synth plugged in)
    // and must not be an error the user sees on first launch.
    midi.open ("Nymphes");
    updateDeviceStatus();

    startTimer (40);
}

NymphesControllerAudioProcessor::~NymphesControllerAudioProcessor()
{
    stopTimer();
    for (const auto& spec : nymphes::ccui::allSpecs())
        apvts.removeParameterListener (idForCc ((int) spec.cc), this);

    midi.setSysExCallback (nullptr);
    midi.setChannelCallback (nullptr);
    midi.setConnectionCallback (nullptr);
    midi.close();
}

//==============================================================================
void NymphesControllerAudioProcessor::wireMidi()
{
    midi.setSysExCallback ([this] (const std::vector<uint8_t>& frame)
    {
        // CoreMIDI thread. Hand the frame to the message thread: decoding and the
        // bank mutation are not real-time work.
        auto copy = frame;
        juce::MessageManager::callAsync ([this, copy]
        {
            onSysExFromDevice (copy);
        });
    });

    midi.setChannelCallback ([this] (const nymphes::ChannelMessage& m)
    {
        // CC from the synth: a preset load with CC-out enabled, or a knob move.
        // Safe on the MIDI thread: the engine only stores ints and calls
        // sendChangeMessage(), which is itself thread-safe.
        if (m.type() == 0xB0)
            ccEngine.setValueFromDevice (m.data1, m.data2);
    });

    midi.setConnectionCallback ([this] (bool, const std::string&)
    {
        juce::MessageManager::callAsync ([this] { updateDeviceStatus(); });
    });
}

void NymphesControllerAudioProcessor::openMidiPort (const juce::String& nameOrEmpty)
{
    requestedPort = nameOrEmpty;
    midi.close();
    std::string error;
    const bool ok = midi.open (nameOrEmpty.isEmpty() ? std::string ("Nymphes")
                                                     : nameOrEmpty.toStdString(),
                               &error);
    if (! ok)
        setError ("Could not open MIDI port: " + toJuce (error));
    updateDeviceStatus();
    deviceBroadcaster.sendChangeMessage();
}

void NymphesControllerAudioProcessor::updateDeviceStatus()
{
    const bool open = midi.isOpen();
    deviceStatus = open ? ("connected: " + toJuce (midi.openedName()))
                        : juce::String ("no Nymphes port open");
    deviceBroadcaster.sendChangeMessage();
}

void NymphesControllerAudioProcessor::setError (const juce::String& e)
{
    lastError = e;
    deviceBroadcaster.sendChangeMessage();
}

//==============================================================================
void NymphesControllerAudioProcessor::parameterChanged (const juce::String& parameterID,
                                                       float newValue)
{
    if (updatingFromEngine)
        return;
    const int cc = ccForParamId (parameterID);
    if (cc < 0)
        return;
    ccEngine.setValue (cc, (int) juce::roundToInt (newValue));
}

int NymphesControllerAudioProcessor::ccForParamId (const juce::String& id)
{
    if (! id.startsWith ("cc_"))
        return -1;
    const int cc = id.fromFirstOccurrenceOf ("cc_", false, false).getIntValue();
    return juce::isPositiveAndBelow (cc, 128) ? cc : -1;
}

juce::String NymphesControllerAudioProcessor::paramIdForCc (int cc) const
{
    return idForCc (cc);
}

void NymphesControllerAudioProcessor::timerCallback()
{
    // 1. Push engine values into the host parameters so automation and the saved
    //    state follow a MIDI-learned knob or a preset load.
    updatingFromEngine = true;
    for (const auto& spec : nymphes::ccui::allSpecs())
    {
        const int cc = (int) spec.cc;
        if (auto* p = apvts.getParameter (idForCc (cc)))
        {
            const int engineValue = ccEngine.getValue (cc);
            const int paramValue = (int) juce::roundToInt (p->convertFrom0to1 (p->getValue()));
            if (engineValue != paramValue)
            {
                const float normalised = p->convertTo0to1 ((float) engineValue);
                p->setValueNotifyingHost (normalised);
            }
        }
    }
    updatingFromEngine = false;

    // 2. Read-back timeout for a slot write.
    if (writeInFlight && juce::Time::currentTimeMillis() > writeDeadlineMs)
    {
        writeInFlight = false;

        // A dump that never completes must be treated exactly like a dump that
        // disagreed: if we captured the original, put it back. The brief's rule is
        // "record the original bytes of any slot you touch and restore them before
        // you finish", and this is the branch where we recorded them and had not yet
        // restored them.
        if (writeOriginalKnown)
        {
            nymphes::Preset restore = writeOriginalPreset;
            restore.setFlag (nymphes::kFlagWrite);
            restore.setAddress (nymphes::Address { nymphes::kPageUser,
                                                   writeSlot.bank,
                                                   writeSlot.program });
            if (midi.sendSysEx (restore.encodeFrameRaw()))
            {
                bank.set (restore);
                setError ("Read-back timed out ("
                          + juce::String (kWriteTimeoutMs / 1000.0, 1)
                          + " s); the original slot contents were sent back.");
            }
            else
            {
                setError ("Read-back timed out AND the restore could not be sent; "
                          "the slot is unverified.");
            }
        }
        else
        {
            setError ("Read-back timed out and the original was never captured; "
                      "the slot is unverified.");
        }
        presetBroadcaster.sendChangeMessage();
    }
}

//==============================================================================
void NymphesControllerAudioProcessor::loadPresetToDevice (const nymphes::Preset& p)
{
    if (! midi.isOpen())
    {
        setError ("No Nymphes port open - cannot load a preset on the synth.");
        return;
    }

    // FF=0x00. This never writes device memory.
    const auto frame = p.encodeAudition();
    if (frame.empty())
    {
        setError ("Could not encode the preset for audition.");
        return;
    }
    if (! midi.sendSysEx (frame))
    {
        setError ("Sending the preset failed.");
        return;
    }

    currentPreset = p;
    lastError.clear();
    deviceBroadcaster.sendChangeMessage();
}

bool NymphesControllerAudioProcessor::saveToUserSlot (const nymphes::Preset& p,
                                                      nymphes::Slot slot)
{
    // The single most important check in this file. AA=0x01 is FACTORY and must
    // never be written, whatever the caller asked for.
    if (p.address().page == nymphes::kPageFactory)
    {
        setError ("Refusing to write a FACTORY slot (AA=0x01). Choose a USER slot.");
        return false;
    }
    if (! slot.isValid())
    {
        setError ("Invalid slot: bank and program must be 1..7.");
        return false;
    }
    if (! midi.isOpen())
    {
        setError ("No Nymphes port open - cannot write a slot.");
        return false;
    }

    // Preserve the slot's original bytes before touching it, so a failed
    // verification can put them back.
    const int index = nymphes::PresetBank::indexFor (nymphes::Page::User, slot);
    writeOriginalKnown = bank.contains (index);
    writeOriginalPreset = writeOriginalKnown ? bank.at (index) : nymphes::Preset {};

    // Build the write frame from the CURRENT payload bytes verbatim. Nothing is
    // re-serialised from a semantic view: a rebuild would drop the optional
    // reverb containers and re-encode negative chord varints, and the stored
    // preset would differ from the one being saved.
    nymphes::Preset toWrite = p;
    toWrite.setAddress (nymphes::Address { nymphes::kPageUser, slot.bank, slot.program });
    toWrite.setFlag (nymphes::kFlagWrite);

    const auto frame = toWrite.encodeFrameRaw();
    if (frame.empty())
    {
        setError ("Could not encode the write frame.");
        return false;
    }

    writeSentPayload.assign (toWrite.payload().begin(), toWrite.payload().end());
    writeSlot = slot;
    writeInFlight = true;
    writeDeadlineMs = juce::Time::currentTimeMillis() + kWriteTimeoutMs;

    if (! midi.sendSysEx (frame))
    {
        writeInFlight = false;
        setError ("Sending the write frame failed; the slot was not modified.");
        return false;
    }

    // Ask for the dump that will carry the read-back. The device never echoes the
    // preset back on its own, so without this the write could never be verified.
    const std::vector<uint8_t> dumpRequest (kDumpRequest,
                                            kDumpRequest + sizeof (kDumpRequest));
    const bool dumpRequested = midi.sendSysEx (dumpRequest);

    // Optimistically record the new contents; the read-back below decides whether
    // that record is true.
    bank.set (toWrite);
    currentPreset = toWrite;
    if (dumpRequested)
        lastError.clear();
    else
        setError ("Write sent, but the read-back dump request could not be sent, so "
                  "byte identity is UNVERIFIED.");
    presetBroadcaster.sendChangeMessage();
    return true;
}

//==============================================================================
void NymphesControllerAudioProcessor::requestFullDump()
{
    if (! midi.isOpen())
    {
        setError ("No Nymphes port open. Connect the synth (or use its own MENU dump "
                  "function with the port open) and try again.");
        return;
    }

    // The full-dump request is the vendor's documented message: F0 00 21 35 00 06 02 F7
    // - product prefix, message type 0x02, no payload. It is NOT a firmware message
    // (type 0x05) and this plugin cannot construct one: no firmware SysEx, no
    // bootloader path, nothing that can write flash outside a USER preset slot.
    const std::vector<uint8_t> request (kDumpRequest, kDumpRequest + sizeof (kDumpRequest));

    if (! midi.sendSysEx (request))
    {
        setError ("Sending the full-dump request failed.");
        return;
    }

    // Arm capture as well: the device answers with a stream of preset frames, and
    // every one that decodes is ingested by onSysExFromDevice().
    capturing = true;
    lastError = "Full-dump request sent. Presets arriving from the synth are added to "
                "the browser list as they decode.";
    deviceBroadcaster.sendChangeMessage();
}

void NymphesControllerAudioProcessor::onSysExFromDevice (const std::vector<uint8_t>& frame)
{
    nymphes::Preset decoded;
    std::string error;
    if (! nymphes::decodeFrame (frame.data(), frame.size(), decoded, &error))
        return;   // not a preset frame; CC-out and other traffic is ignored

    // A preset arriving on load, or during a dump, is the synth telling us what it
    // holds. Store it so the browser list reflects reality.
    bank.set (decoded);

    if (writeInFlight
        && decoded.address().page == nymphes::kPageUser
        && decoded.address().bank == writeSlot.bank
        && decoded.address().program == writeSlot.program)
    {
        writeInFlight = false;
        const auto& readBack = decoded.payload();
        const bool identical = readBack.size() == writeSentPayload.size()
                               && std::equal (readBack.begin(), readBack.end(),
                                              writeSentPayload.begin());
        if (identical)
        {
            lastError = "Slot write verified: read-back is byte-identical.";
        }
        else
        {
            // Put the original back rather than leaving the slot in a state we
            // cannot describe.
            if (writeOriginalKnown)
            {
                nymphes::Preset restore = writeOriginalPreset;
                restore.setFlag (nymphes::kFlagWrite);
                midi.sendSysEx (restore.encodeFrameRaw());
                bank.set (restore);
                lastError = "Read-back did NOT match the bytes sent. The original "
                            "slot contents were sent back to restore it.";
            }
            else
            {
                lastError = "Read-back did NOT match the bytes sent, and the "
                            "original contents of this slot were never captured, "
                            "so it could not be restored.";
            }
        }
        presetBroadcaster.sendChangeMessage();
    }

    presetBroadcaster.sendChangeMessage();
    deviceBroadcaster.sendChangeMessage();
}

void NymphesControllerAudioProcessor::onChannelFromDevice (const nymphes::ChannelMessage& m)
{
    if (m.type() == 0xB0)
        ccEngine.setValueFromDevice (m.data1, m.data2);
}

void NymphesControllerAudioProcessor::sendFrame (const std::vector<uint8_t>& frame)
{
    midi.sendSysEx (frame);
}

//==============================================================================
bool NymphesControllerAudioProcessor::importSyx (const juce::File& file, juce::String& error)
{
    std::string err;
    const int n = bank.importSyx (file.getFullPathName().toStdString(), err);
    if (n <= 0)
    {
        error = err.empty() ? juce::String ("No preset frames found in that file.")
                            : toJuce (err);
        return false;
    }
    error.clear();
    presetBroadcaster.sendChangeMessage();
    return true;
}

bool NymphesControllerAudioProcessor::exportSyx (const juce::File& file, juce::String& error)
{
    std::string err;
    // nullptr page = export both pages, in USER-then-FACTORY order.
    if (! bank.exportSyx (file.getFullPathName().toStdString(), nullptr, err))
    {
        error = err.empty() ? juce::String ("Could not write the file.") : toJuce (err);
        return false;
    }
    error.clear();
    return true;
}

//==============================================================================
void NymphesControllerAudioProcessor::prepareToPlay (double, int) {}

void NymphesControllerAudioProcessor::releaseResources() {}

void NymphesControllerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // The plugin is a controller, not a synth: it passes MIDI through untouched so
    // it can sit on an Ableton track without swallowing the clip, and it learns
    // from whatever CC arrives. It does not consume anything.
    for (const auto meta : midiMessages)
    {
        const auto m = meta.getMessage();
        if (m.isController() || m.isChannelPressure())
            ccEngine.handleIncomingMidi (m);
    }
}

//==============================================================================
juce::AudioProcessorEditor* NymphesControllerAudioProcessor::createEditor()
{
    return new nymphes::ui::PluginEditor (*this);
}

//==============================================================================
void NymphesControllerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root (kStateTag);
    root.setAttribute ("version", kStateVersion);
    root.setAttribute ("product", "NymphesController");

    if (auto apvtsXml = apvts.copyState().createXml())
        root.addChildElement (apvtsXml.release());

    if (auto engineXml = ccEngine.getState())
        root.addChildElement (engineXml.release());

    // The current preset travels with the session so reopening a project restores
    // exactly the bytes that were being edited, not a re-derived approximation.
    auto* preset = root.createNewChildElement ("currentPreset");
    preset->setAttribute ("page", (int) currentPreset.address().page);
    preset->setAttribute ("bank", (int) currentPreset.address().bank);
    preset->setAttribute ("program", (int) currentPreset.address().program);
    preset->setAttribute ("flag", (int) currentPreset.flag());
    preset->setAttribute ("payload",
        juce::Base64::toBase64 (currentPreset.payload().data(),
                                currentPreset.payload().size()));

    copyXmlToBinary (root, destData);
}

void NymphesControllerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (kStateTag))
        return;

    if (auto* apvtsXml = xml->getChildByName (apvts.state.getType().toString()))
        apvts.replaceState (juce::ValueTree::fromXml (*apvtsXml));

    if (auto* engineXml = xml->getChildByName (nymphes::CCEngine::stateTag))
        ccEngine.setState (*engineXml);

    if (auto* preset = xml->getChildByName ("currentPreset"))
    {
        juce::MemoryOutputStream decoded;
        if (juce::Base64::convertFromBase64 (decoded, preset->getStringAttribute ("payload")))
        {
            nymphes::Bytes bytes ((const uint8_t*) decoded.getData(),
                                  (const uint8_t*) decoded.getData() + decoded.getDataSize());
            currentPreset.setPayload (std::move (bytes));
            currentPreset.setAddress (nymphes::Address {
                (std::uint8_t) preset->getIntAttribute ("page", 0),
                (std::uint8_t) preset->getIntAttribute ("bank", 1),
                (std::uint8_t) preset->getIntAttribute ("program", 1) });
            currentPreset.setFlag ((std::uint8_t) preset->getIntAttribute ("flag", 0));
        }
    }

    // Push the restored engine values into the host parameters.
    timerCallback();
    presetBroadcaster.sendChangeMessage();
    deviceBroadcaster.sendChangeMessage();
}

//==============================================================================
// The plugin entry point. JUCE's VST3/AU/Standalone wrappers call this; without
// it the shared object links but has no factory, which is what a "createPluginFilter()
// referenced from juce_audio_plugin_client_VST3" link error means.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NymphesControllerAudioProcessor();
}
