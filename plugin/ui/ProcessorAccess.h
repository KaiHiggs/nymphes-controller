// plugin/ui/ProcessorAccess.h  —  L1
//
// The ONE file in plugin/ui/ that knows the AudioProcessor's shape. Every other
// UI file goes through these functions, so if the processor's API shifts exactly
// one file needs to change.
//
// The processor API used below was frozen by L3 in fleet message 6f4dd5f064be:
//
//   class NymphesControllerAudioProcessor : public juce::AudioProcessor
//     juce::AudioProcessorValueTreeState& getAPVTS();
//     nymphes::MidiPort&   getMidi();
//     nymphes::CCEngine&   getCCEngine();
//     nymphes::PresetBank& getBank();
//     const nymphes::Preset& getCurrentPreset() const;
//     void loadPresetToDevice (const nymphes::Preset&);      // FF=00, audition
//     bool saveToUserSlot (const nymphes::Preset&, nymphes::Slot);  // refuses Factory
//     juce::String getLastError() const;
//     juce::ChangeBroadcaster& presetListChanged();
//     juce::ChangeBroadcaster& deviceStateChanged();
//
// PresetBank is the only part L3 did not freeze. The UI needs exactly three
// things from it and nothing more:
//
//     int  size() const;                       // 98 = 49 USER + 49 FACTORY
//     const nymphes::Preset& at (int index) const;
//     juce::String nameOf (int index) const;   // "" when the slot is empty
//
// If PresetBank ends up shaped differently, this file absorbs the difference.
//
// The page test is `address().isUser()`, not a member field: the shipping codec
// (core/Codec.h) carries the address in `Address{page,bank,program}`, and
// preset/Preset.h is a facade over it. Do not reintroduce aa/slot members here.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/PluginProcessor.h"
#include "plugin/cc/CCEngine.h"
#include "preset/Preset.h"

namespace nymphes::ui
{

using Processor = NymphesControllerAudioProcessor;

//--- engine / bank ------------------------------------------------------------
inline CCEngine&   ccEngine (Processor& p) { return p.getCCEngine(); }
inline PresetBank& bank     (Processor& p) { return p.getBank(); }

// A slot's identity comes from its INDEX (PresetBank's fixed convention), never
// from the preset stored there: an empty slot holds a default preset whose own
// address reads USER A1, which made every empty row claim to be USER A1 and made
// "save" on an empty row aim at A1.
inline int  numSlots   (Processor& p) { return bank (p).size(); }
inline bool slotPresent (Processor& p, int i) { return bank (p).contains (i); }
inline const Preset& slotPreset (Processor& p, int i) { return bank (p).at (i); }
inline bool slotIsFactory (Processor&, int i)
{
    return PresetBank::pageFor (i) == Page::Factory;
}
inline Slot slotOf (Processor&, int i)
{
    return PresetBank::slotFor (i);
}
inline juce::String slotName (Processor& p, int i)
{
    const auto s = slotOf (p, i);
    return juce::String (slotIsFactory (p, i) ? "FACTORY " : "USER ")
         + juce::String::charToString (s.bankLetter()) + juce::String ((int) s.program);
}
inline juce::String slotSummary (Processor& p, int i)
{
    if (! slotPresent (p, i))
        return {};
    const auto s = summaryLine (slotPreset (p, i));
    return juce::String::fromUTF8 (s.c_str(), (int) s.size());
}

/** The bank index of the preset last loaded or written, or -1 when none is. */
inline int loadedIndex (Processor& p)
{
    const auto& cur = p.getCurrentPreset();
    if (cur.payload().empty())
        return -1;
    const auto slot = Slot::fromAddress (cur.address());
    if (! slot.isValid())
        return -1;
    return PresetBank::indexFor (cur.address().isUser() ? Page::User : Page::Factory, slot);
}

//--- actions ------------------------------------------------------------------
inline bool audition (Processor& p, int i, juce::String& error)
{
    if (! slotPresent (p, i))
    {
        error = slotName (p, i) + " is empty. Request a dump or import a .syx first.";
        return false;
    }
    p.loadPresetToDevice (bank (p).at (i));      // FF=00: never touches flash
    error = p.getLastError();
    return error.isEmpty();
}

/** Copy the loaded preset into USER slot `i` (FF=01), then read back. */
inline bool saveLoadedTo (Processor& p, int i, juce::String& error)
{
    if (slotIsFactory (p, i))
    {
        // Hard stop, not a UI preference: a factory slot must never be written.
        error = "Refusing to write a FACTORY slot (AA=01). Choose a USER slot.";
        return false;
    }
    if (loadedIndex (p) < 0)
    {
        error = "Nothing is loaded. Audition a preset first; that is what gets copied.";
        return false;
    }
    const bool ok = p.saveToUserSlot (p.getCurrentPreset(), slotOf (p, i));
    if (! ok)
        error = p.getLastError();
    return ok;
}

inline juce::String lastError (Processor& p) { return p.getLastError(); }

//--- device + file actions the browser needs ----------------------------------
// These three are processor-level because they touch the MIDI port or mutate
// the bank; the UI must not own either. L3: if your method names differ, this is
// the only place that changes.
inline void requestFullDump (Processor& p) { p.requestFullDump(); }
inline bool importSyxFile (Processor& p, const juce::File& f, juce::String& error)
{
    return p.importSyx (f, error);
}
inline bool exportSyxFile (Processor& p, const juce::File& f, juce::String& error)
{
    return p.exportSyx (f, error);
}

inline juce::ChangeBroadcaster& presetChanged (Processor& p) { return p.presetListChanged(); }
inline juce::ChangeBroadcaster& deviceChanged (Processor& p) { return p.deviceStateChanged(); }

} // namespace nymphes::ui
