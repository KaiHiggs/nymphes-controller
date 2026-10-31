// tests/juce/ccengine_test.cpp — L1
//
// Behavioural tests for plugin/cc/CCEngine.cpp. The engine is JUCE, so this file
// has its own console target (see the "ccengine_test" block in CMakeLists.txt) and
// does not go through the JUCE-free tests/ glob.
//
// Every check below is one of the engine defects the OCR scan reported, written as
// the assertion that FAILS on f030926 and passes after the fix. The state-blob and
// alias checks are the deterministic ones; the data race needs a thread sanitizer,
// which the tree has no target for (it was reproduced by hand with -fsanitize=thread;
// see evidence/L1-ccengine.md).
//
// Exits non-zero on any failure so ctest reports it.

#include "plugin/cc/CCEngine.h"

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace
{
int failures = 0;

void check (bool ok, const std::string& what)
{
    if (! ok)
    {
        std::printf ("FAIL  %s\n", what.c_str());
        ++failures;
    }
}

std::string num (int v) { return std::to_string (v); }

// Sink that records what the engine emitted, so "did it emit" is observable.
struct Recorder
{
    std::vector<juce::MidiMessage> sent;
    nymphes::CCEngine::MidiSink sink()
    {
        return [this] (const juce::MidiMessage& m) { sent.push_back (m); };
    }
    int countFor (int cc) const
    {
        int n = 0;
        for (const auto& m : sent)
            if (m.isController() && m.getControllerNumber() == cc)
                ++n;
        return n;
    }
};

juce::MidiMessage ccMsg (int cc, int value, int channel = 1)
{
    return juce::MidiMessage::controllerEvent (channel, cc, value);
}

// True when the blob has a <param cc="..."> child.
bool stateHasParam (const juce::XmlElement& xml, int cc)
{
    for (auto* e : xml.getChildWithTagNameIterator ("param"))
        if (e->getIntAttribute ("cc", -1) == cc)
            return true;
    return false;
}
}

int main()
{
    using namespace nymphes;

    // ---- 1. a fresh engine starts at its documented defaults -------------------
    // f030926: params[] was zeroed, so getValue(7) was 0 while defaultFor(7) is 127,
    // i.e. the amp level was non-default and still reported "not modified", and a
    // getState()/setState() round trip silently turned 0 into 127.
    {
        CCEngine e;
        check (e.getValue (7) == 127, "fresh engine: CC 7 (amp level) is at 127, got " + num (e.getValue (7)));
        check (! e.isModified (7), "fresh engine: CC 7 is not modified");
        check (e.getValue (22) == 0, "fresh engine: CC 22 is at 0");

        const auto xml = e.getState();
        check (xml != nullptr, "getState() returns a blob");
        if (xml != nullptr)
        {
            CCEngine restored;
            restored.setState (*xml);
            check (restored.getValue (7) == 127, "CC 7 survives a state round trip");
            check (restored.getValue (22) == 0, "CC 22 survives a state round trip");
        }
    }

    // ---- 2. setValue is idempotent for a default-valued parameter --------------
    // f030926: the guard was `p.value == value && p.modified`, and modified is
    // derived from the value, so a default value fell straight through and re-emitted
    // the CC and re-notified on every call.
    {
        CCEngine e;
        Recorder rec;
        e.setMidiSink (rec.sink());
        e.setValue (22, 0);          // already the default (BPM)
        check (rec.countFor (22) == 0, "setValue(default) emits nothing, got " + num (rec.countFor (22)));
        e.setValue (22, 5);
        check (rec.countFor (22) == 1, "setValue(non-default) emits once, got " + num (rec.countFor (22)));
        e.setValue (22, 5);
        check (rec.countFor (22) == 1, "setValue(same non-default) emits nothing more");
        e.setValue (22, 0);          // back to the default: this one IS a change
        check (rec.countFor (22) == 2, "setValue(default) after a change emits once");

        // The wire path an operator report is about: a value coming from a control
        // must reach the sink as ONE real channel message, on the channel the engine
        // is set to, and the emit gate must be open by default. If this ever fails,
        // nothing downstream (conversion, queue, MIDI port) matters.
        e.setValue (74, 100);        // CC 74 = LPF cutoff
        bool onTheWire = false;
        for (const auto& m : rec.sent)
            if (m.isController() && m.getControllerNumber() == 74
                && m.getControllerValue() == 100 && m.getChannel() == 1)
                onTheWire = true;
        check (onTheWire, "setValue(74,100) reaches the sink as 0xB0/74/100 on channel 1");
        check (e.isEmitEnabled(), "emitting is enabled by default");

        e.setEmitEnabled (false);
        const int before = rec.countFor (75);
        e.setValue (75, 64);
        check (rec.countFor (75) == before, "a closed emit gate suppresses the message");
        e.setEmitEnabled (true);
    }

    // ---- 3. a device-originated change re-arms soft takeover -------------------
    // f030926: setValueFromDevice cancelled the wait, so the learned knob wrote its
    // raw position straight into the parameter instead of catching up.
    {
        CCEngine e;
        e.setTakeover (23, CCEngine::Takeover::soft);
        e.setValue (23, 100);
        check (e.isWaitingForTakeover (23), "unengaged soft takeover starts waiting");

        e.handleIncomingMidi (ccMsg (23, 40));      // first message: reference point only
        check (e.getValue (23) == 100, "a soft-takeover control does not move the value before it crosses");

        e.setValueFromDevice (23, 10);              // another controller / a preset load
        check (e.getValue (23) == 10, "the device value is stored");
        check (e.isWaitingForTakeover (23),
               "a device-originated change re-arms soft takeover (f030926 cancelled it)");

        e.handleIncomingMidi (ccMsg (23, 20));      // did NOT cross 10 on the way up
        check (e.getValue (23) == 10,
               "after a device change the control must cross the value, got " + num (e.getValue (23)));

        e.handleIncomingMidi (ccMsg (23, 10));      // lands exactly on it
        check (! e.isWaitingForTakeover (23), "landing on the value engages the control");
    }

    // ---- 3b. the reference point is dropped when the target is re-armed --------
    // OCR-021. While a soft takeover is ENGAGED, applySoftTakeover returns true
    // before it updates `lastRawFromControl`, so the sample that engaged it can be
    // arbitrarily old. A UI/host write re-arms the wait; if that old sample
    // survives, the next control message is compared against a position the knob
    // left long before, and a knob moving AWAY from the value looks like a crossing.
    {
        CCEngine e;
        e.setTakeover (7, CCEngine::Takeover::soft);
        e.setValue (7, 50);

        e.handleIncomingMidi (ccMsg (7, 60));   // first message: reference only
        check (e.getValue (7) == 50, "still waiting after the reference message");
        e.handleIncomingMidi (ccMsg (7, 40));   // crosses 50 downwards: engages
        check (! e.isWaitingForTakeover (7), "a downward crossing engages the control");
        check (e.getValue (7) == 40, "the engaged control sets the value");

        e.handleIncomingMidi (ccMsg (7, 90));   // engaged: the sample is NOT updated
        check (e.getValue (7) == 90, "while engaged the value follows the control");

        e.setValue (7, 60);                     // host/UI write re-arms the wait
        check (e.isWaitingForTakeover (7), "a host write re-arms soft takeover");

        // The control is at 90 and moves to 89: it has NOT crossed 60.
        e.handleIncomingMidi (ccMsg (7, 89));
        check (e.getValue (7) == 60,
               "a nudge away from the value must not engage it, got " + num (e.getValue (7)));
        check (e.isWaitingForTakeover (7), "still waiting after a nudge away from the value");

        e.handleIncomingMidi (ccMsg (7, 60));   // lands exactly on the new value
        check (! e.isWaitingForTakeover (7), "landing on the new value engages the control");
    }

    // ---- 4. a CC the Nymphes does not implement is not stored -----------------
    // f030926: setValueFromDevice stored any 0..127, getState() serialised it, and
    // setState() dropped it again, so the value disappeared on the next load.
    {
        CCEngine e;
        e.setValueFromDevice (38, 55);              // CC 38 is absent from the device
        check (e.getValue (38) == 0, "an unknown CC is not stored");
        const auto xml = e.getState();
        check (xml != nullptr && ! stateHasParam (*xml, 38),
               "an unknown CC is not serialised into the state blob");
    }

    // ---- 5. MIDI learn refuses a Nymphes parameter CC as an alias -------------
    // f030926: learning CC 7 for CC 22 shadowed the direct mapping for CC 7 for the
    // rest of the session (resolveIncomingCc resolves aliases first).
    {
        CCEngine e;
        e.beginLearn (22);
        check (e.isLearning(), "beginLearn arms learning");
        e.handleIncomingMidi (ccMsg (7, 64));       // a real Nymphes parameter CC
        check (e.getLearnedCc (22) == -1,
               "a Nymphes parameter CC cannot become an alias, got "
                   + num (e.getLearnedCc (22)));
        check (e.isLearning(), "learning stays armed after refusing a parameter CC");
        check (e.resolveIncomingCc (7) == 7, "CC 7 still addresses its own parameter");

        e.handleIncomingMidi (ccMsg (2, 64));       // CC 2 is not a Nymphes parameter
        check (e.getLearnedCc (22) == 2, "a foreign CC is accepted as the alias");
        check (! e.isLearning(), "learning completes");
        check (e.resolveIncomingCc (2) == 22, "the alias resolves to its parameter");
        check (e.resolveIncomingCc (7) == 7, "the direct mapping for CC 7 is intact");
    }

    // ---- 6. one alias per parameter, one parameter per alias ------------------
    // f030926: setState() trusted the blob, so two parameters could share an alias
    // and resolveIncomingCc() silently drove whichever had the lower CC index.
    {
        CCEngine e;
        juce::XmlElement xml (CCEngine::stateTag);
        for (int cc : { 22, 23 })
        {
            auto* p = xml.createNewChildElement ("param");
            p->setAttribute ("cc", cc);
            p->setAttribute ("value", 3);
            p->setAttribute ("learnedCc", 2);       // both claim controller CC 2
        }
        e.setState (xml);

        int aliases = 0;
        for (int cc = 0; cc < 128; ++cc)
            if (e.getLearnedCc (cc) == 2)
                ++aliases;
        check (aliases == 1, "a duplicate alias in a state blob is reduced to one, got " + num (aliases));
        check (e.getValue (22) == 3 && e.getValue (23) == 3, "the values are still restored");
        check (e.resolveIncomingCc (2) >= 0, "the surviving alias still resolves");
    }

    // ---- 7. an impossible midiChannel in the blob falls back to omni ----------
    // f030926: the attribute was assigned verbatim, so midiChannel="37" made the
    // engine drop every message and look dead.
    {
        CCEngine e;
        juce::XmlElement xml (CCEngine::stateTag);
        xml.setAttribute ("midiChannel", 37);
        e.setState (xml);
        check (e.getMidiChannel() == 0, "midiChannel 37 falls back to omni, got " + num (e.getMidiChannel()));

        e.setMidiChannel (5);
        check (e.getMidiChannel() == 5, "a valid channel is kept");
        juce::XmlElement xml2 (CCEngine::stateTag);
        xml2.setAttribute ("midiChannel", 5);
        e.setState (xml2);
        check (e.getMidiChannel() == 5, "a valid channel survives the blob");
    }

    // ---- 8. the value store stays self-consistent under concurrent use --------
    // Not a substitute for a thread sanitizer (this tree has no TSan target; the
    // race was reproduced by hand with -fsanitize=thread on the pre-fix engine), but
    // it does pin the invariants the lock exists to keep: modified always matches
    // "value differs from the default", and no alias can be half-written.
    {
        CCEngine e;
        e.setMidiChannel (0);

        std::thread audio ([&e]
        {
            for (int i = 0; i < 2000; ++i)
                e.handleIncomingMidi (juce::MidiMessage::controllerEvent (1, 31 + (i % 8), i % 128));
        });
        std::thread host ([&e]
        {
            for (int i = 0; i < 2000; ++i)
                e.setValue (31 + (i % 8), 127 - (i % 128));
        });
        audio.join();
        host.join();

        int inconsistent = 0;
        for (int cc = 0; cc < 128; ++cc)
        {
            const int v = e.getValue (cc);
            const bool shouldBeModified = (v != (cc == 7 ? 127 : 0));
            if (e.isModified (cc) != shouldBeModified)
                ++inconsistent;
        }
        check (inconsistent == 0,
               "value/modified stay consistent after concurrent use, " + num (inconsistent)
                   + " inconsistent parameters");

        const auto xml = e.getState();
        check (xml != nullptr, "the state blob is still readable after concurrent use");
    }

    if (failures == 0)
        std::printf ("ccengine: all checks passed\n");
    else
        std::printf ("ccengine: %d FAILURES\n", failures);

    return failures == 0 ? 0 : 1;
}
