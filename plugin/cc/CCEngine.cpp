// plugin/cc/CCEngine.cpp  —  L1

#include "plugin/cc/CCEngine.h"

namespace nymphes
{

// The engine is addressed by the *Nymphes* CC number. A learned controller CC is an
// alias that resolves to one of those; incoming CCs on a Nymphes number address that
// parameter directly. See resolveIncomingCc().
//
// See the threading model in the header: every public member takes stateLock, and
// emitCc()/onLearnComplete()/sendChangeMessage() are invoked after it is released.

namespace
{
    constexpr int kMaxCc = 127;

    inline int clampCc (int v) noexcept { return juce::jlimit (0, kMaxCc, v); }

    // A CC number is "known" when the Nymphes actually implements it. CC 38 is
    // genuinely absent (see midi/CCMap.h), so we must not invent a parameter for
    // it: a learned alias pointing at an unknown CC is refused rather than
    // silently stored. This only reads the constexpr table, so it needs no lock.
    inline bool isKnownCc (int cc) noexcept { return findCC (cc) != nullptr; }
}

CCEngine::CCEngine()
{
    // A fresh engine starts AT its defaults. Leaving the array zeroed would make
    // getValue (7) == 0 while defaultFor (7) == 127, so an untouched amp level
    // would be "not modified" yet non-default, and a getState()/setState() round
    // trip would turn it into 127.
    for (int cc = 0; cc <= kMaxCc; ++cc)
        params[(size_t) cc].value = defaultFor (cc);
}

CCEngine::~CCEngine() = default;

//==============================================================================
int CCEngine::defaultFor (int nymphesCc)
{
    // Only two defaults are documented well enough to rely on:
    //   CC 7  amp level  - "set to default 127" (nymphescc messages.dhall)
    //   CC 22/28 LFO type - BPM (0)
    // Everything else starts at 0; the value is overwritten by the first preset
    // load or the first CC the device sends. This is a UI default, not a claim
    // about the synth's power-on state.
    if (nymphesCc == 7)
        return 127;
    return 0;
}

//==============================================================================
void CCEngine::setMidiSink (MidiSink newSink)
{
    const juce::ScopedLock sl (stateLock);
    sink = std::move (newSink);
}

void CCEngine::setMidiChannel (int oneBasedChannel)
{
    const juce::ScopedLock sl (stateLock);
    midiChannel = (oneBasedChannel >= 1 && oneBasedChannel <= 16) ? oneBasedChannel : 0;
}

int CCEngine::getMidiChannel() const noexcept
{
    const juce::ScopedLock sl (stateLock);
    return midiChannel;
}

void CCEngine::attachToProcessor (juce::AudioProcessor& p)
{
    const juce::ScopedLock sl (stateLock);
    attachedProcessor = &p;
}

void CCEngine::detachFromProcessor()
{
    const juce::ScopedLock sl (stateLock);
    attachedProcessor = nullptr;
}

//==============================================================================
int CCEngine::getValue (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return 0;
    const juce::ScopedLock sl (stateLock);
    return params[(size_t) nymphesCc].value;
}

void CCEngine::setValue (int nymphesCc, int value)
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128) || ! isKnownCc (nymphesCc))
        return;

    value = clampCc (value);

    {
        const juce::ScopedLock sl (stateLock);
        auto& p = params[(size_t) nymphesCc];

        // Value-based, not value+modified: `modified` is derived from the value
        // (`value != defaultFor`), so testing it here made the guard degenerate
        // into "skip only when the current value is non-default" — a default
        // value fell through on every call and re-emitted the CC, re-notified and
        // re-armed the takeover below.
        if (p.value == value)
            return;

        p.value = value;
        p.modified = (value != defaultFor (nymphesCc));

        // A value set from the UI or the host invalidates any half-finished
        // takeover: the physical control has to catch up to the new position.
        //
        // The raw reference point is dropped WITH the re-arm. applySoftTakeover
        // returns before updating it while the takeover is ENGAGED, so the retained
        // sample is not "the knob's current position" — it can be the position that
        // engaged the control long ago. Keeping it makes the next message look like
        // a crossing: with the value re-armed to 60, a knob sitting at 90 and moved
        // to 89 (away from 60, never through it) would engage and snap the parameter
        // to 89 against a stale sample of 40. This mirrors setTakeover(), setState()
        // and the learn path, which all reset it.
        p.waitingForTakeover = (p.takeover == Takeover::soft);
        p.lastRawFromControl = -1;
    }

    emitCc (nymphesCc, value);
    notifyValueChanged (nymphesCc, value);
}

void CCEngine::setValueFromDevice (int nymphesCc, int value)
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return;

    // A value for a CC the Nymphes does not implement is dropped here, exactly as
    // setState() drops it. Accepting it would store something getState() then
    // serialises and setState() silently discards — a state round-trip that loses
    // data.
    if (! isKnownCc (nymphesCc))
        return;

    value = clampCc (value);

    {
        const juce::ScopedLock sl (stateLock);
        auto& p = params[(size_t) nymphesCc];
        if (p.value == value)
            return;

        p.value = value;
        p.modified = (value != defaultFor (nymphesCc));

        // The device moved the parameter (preset load, another controller), so a
        // soft-takeover control has to cross the NEW value before it drives it.
        // Cancelling the wait instead let the learned knob jump straight in.
        // Re-arming is safe for our own echo: that path returns above. The raw
        // reference is dropped for the same reason as in setValue(): while engaged
        // it is frozen at the sample that engaged, so it cannot be trusted across
        // a target change.
        p.waitingForTakeover = (p.takeover == Takeover::soft);
        p.lastRawFromControl = -1;
    }

    notifyValueChanged (nymphesCc, value);
}

void CCEngine::refreshAll()
{
    std::vector<std::pair<int, int>> toEmit;

    {
        const juce::ScopedLock sl (stateLock);
        toEmit.reserve ((size_t) kCCCount);
        for (int cc = 0; cc <= kMaxCc; ++cc)
            if (isKnownCc (cc) && params[(size_t) cc].value != defaultFor (cc))
                toEmit.emplace_back (cc, params[(size_t) cc].value);
    }

    for (const auto& e : toEmit)
        emitCc (e.first, e.second);
}

void CCEngine::setEmitEnabled (bool shouldEmit) noexcept
{
    const juce::ScopedLock sl (stateLock);
    emitEnabled = shouldEmit;
}

bool CCEngine::isEmitEnabled() const noexcept
{
    const juce::ScopedLock sl (stateLock);
    return emitEnabled;
}

bool CCEngine::isModified (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return false;
    const juce::ScopedLock sl (stateLock);
    return params[(size_t) nymphesCc].modified;
}

//==============================================================================
void CCEngine::emitCc (int nymphesCc, int value)
{
    MidiSink toCall;
    int channel = 0;

    {
        const juce::ScopedLock sl (stateLock);
        if (! emitEnabled || sink == nullptr)
            return;
        toCall = sink;          // copy: the sink may be replaced from another thread
        channel = midiChannel;
    }

    toCall (juce::MidiMessage::controllerEvent (channel == 0 ? 1 : channel,
                                               nymphesCc, value));
}

void CCEngine::notifyValueChanged (int, int)
{
    // Asynchronous so a burst of CCs from a preset load coalesces into one
    // repaint instead of one repaint per parameter.
    sendChangeMessage();
}

//==============================================================================
void CCEngine::beginLearn (int nymphesCc)
{
    if (! isKnownCc (nymphesCc))
        return;
    {
        const juce::ScopedLock sl (stateLock);
        learnTarget = nymphesCc;
    }
    sendChangeMessage();
}

void CCEngine::cancelLearn()
{
    {
        const juce::ScopedLock sl (stateLock);
        if (learnTarget < 0)
            return;
        learnTarget = -1;
    }
    sendChangeMessage();
}

bool CCEngine::isLearning() const noexcept
{
    const juce::ScopedLock sl (stateLock);
    return learnTarget >= 0;
}

int CCEngine::getLearnTarget() const noexcept
{
    const juce::ScopedLock sl (stateLock);
    return learnTarget;
}

int CCEngine::getLearnedCc (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return -1;
    const juce::ScopedLock sl (stateLock);
    return params[(size_t) nymphesCc].learnedCc;
}

void CCEngine::clearLearn (int nymphesCc)
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return;
    {
        const juce::ScopedLock sl (stateLock);
        params[(size_t) nymphesCc].learnedCc = -1;
        params[(size_t) nymphesCc].waitingForTakeover = false;
        params[(size_t) nymphesCc].lastRawFromControl = -1;
    }
    sendChangeMessage();
}

void CCEngine::clearAllLearn()
{
    {
        const juce::ScopedLock sl (stateLock);
        for (auto& p : params)
        {
            p.learnedCc = -1;
            p.waitingForTakeover = false;
            p.lastRawFromControl = -1;
        }
    }
    sendChangeMessage();
}

int CCEngine::resolveIncomingCcUnlocked (int incomingCc) const
{
    for (int cc = 0; cc <= kMaxCc; ++cc)
        if (params[(size_t) cc].learnedCc == incomingCc)
            return cc;

    // No alias: an incoming CC on a Nymphes parameter addresses that parameter.
    return isKnownCc (incomingCc) ? incomingCc : -1;
}

int CCEngine::resolveIncomingCc (int incomingCc) const
{
    if (! juce::isPositiveAndBelow (incomingCc, 128))
        return -1;

    const juce::ScopedLock sl (stateLock);
    return resolveIncomingCcUnlocked (incomingCc);
}

//==============================================================================
void CCEngine::setTakeover (int nymphesCc, Takeover mode)
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return;
    const juce::ScopedLock sl (stateLock);
    auto& p = params[(size_t) nymphesCc];
    p.takeover = mode;
    p.waitingForTakeover = (mode == Takeover::soft);
    p.lastRawFromControl = -1;
}

CCEngine::Takeover CCEngine::getTakeover (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return getDefaultTakeover();
    const juce::ScopedLock sl (stateLock);
    return params[(size_t) nymphesCc].takeover;
}

void CCEngine::setDefaultTakeover (Takeover mode) noexcept
{
    const juce::ScopedLock sl (stateLock);
    defaultTakeover = mode;
}

CCEngine::Takeover CCEngine::getDefaultTakeover() const noexcept
{
    const juce::ScopedLock sl (stateLock);
    return defaultTakeover;
}

bool CCEngine::isWaitingForTakeover (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return false;
    const juce::ScopedLock sl (stateLock);
    return params[(size_t) nymphesCc].waitingForTakeover;
}

bool CCEngine::applySoftTakeover (int nymphesCc, int rawValue)
{
    // Called with stateLock held, from handleIncomingMidi() only.
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return false;

    auto& p = params[(size_t) nymphesCc];

    if (! p.waitingForTakeover)
        return true;

    const int previous = p.lastRawFromControl;
    p.lastRawFromControl = rawValue;

    // The control engages when it *crosses* the parameter's current value, which
    // is what stops a knob jumping from 3 to 120 the moment it is touched. The
    // first message after a jump is only a reference point (previous == -1).
    if (previous < 0)
        return false;

    const int current = p.value;
    const bool crossedUp   = previous < current && rawValue >= current;
    const bool crossedDown = previous > current && rawValue <= current;
    const bool landedOn    = previous == current;

    if (crossedUp || crossedDown || landedOn)
    {
        p.waitingForTakeover = false;
        return true;
    }

    return false;
}

//==============================================================================
void CCEngine::handleIncomingMidi (const juce::MidiMessage& m)
{
    int channel = 0;
    {
        const juce::ScopedLock sl (stateLock);
        channel = midiChannel;
    }
    if (channel != 0 && m.getChannel() != channel)
        return;

    if (! m.isController())
    {
        // Aftertouch is a modulation *source* on the Nymphes, not a parameter,
        // so it is passed straight through by the processor; nothing to store.
        return;
    }

    const int cc = m.getControllerNumber();
    const int value = m.getControllerValue();

    // Work out what to do under the lock, then run the callbacks and the emit
    // without it: onLearnComplete and sink are user code.
    bool learned = false;
    int  learnedTarget = -1;
    int  nymphesCc = -1;
    bool emitAliased = false;
    bool waitingShown = false;

    {
        const juce::ScopedLock sl (stateLock);

        // 1. MIDI learn captures the next control that moves.
        if (learnTarget >= 0 && cc != learnTarget)
        {
            // A Nymphes parameter CC is not eligible as an alias: resolveIncomingCc
            // resolves aliases first, so learning CC 7 for CC 22 would permanently
            // shadow the direct mapping for CC 7 (the synth's own knob would stop
            // editing its own parameter, and we would emit CC 22 for a message that
            // moved CC 7). Only a foreign controller CC can become an alias.
            if (! isKnownCc (cc))
            {
                learnedTarget = learnTarget;
                // One alias per parameter, and one parameter per alias.
                for (auto& p : params)
                    if (p.learnedCc == cc)
                        p.learnedCc = -1;

                params[(size_t) learnedTarget].learnedCc = cc;
                params[(size_t) learnedTarget].waitingForTakeover = true;
                params[(size_t) learnedTarget].lastRawFromControl = -1;
                learnTarget = -1;
                learned = true;
            }
        }
        else
        {
            nymphesCc = resolveIncomingCcUnlocked (cc);
            if (nymphesCc < 0)
                return;   // a CC the Nymphes does not implement, and nothing learned

            // 2. Soft takeover gate.
            if (params[(size_t) nymphesCc].takeover == Takeover::soft
                && ! applySoftTakeover (nymphesCc, value))
            {
                waitingShown = true;   // let the UI show "waiting"
            }
            else
            {
                params[(size_t) nymphesCc].value = value;
                params[(size_t) nymphesCc].modified = (value != defaultFor (nymphesCc));

                // 3. Only emit when the alias came from elsewhere. A CC arriving on
                //    the Nymphes' own number has already reached the synth over the
                //    same port, so echoing it back would double every message.
                emitAliased = (nymphesCc != cc);
            }
        }
    }

    if (learned)
    {
        if (onLearnComplete)
            onLearnComplete (learnedTarget, cc);
        sendChangeMessage();
        return;
    }

    if (waitingShown)
    {
        sendChangeMessage();
        return;
    }

    if (emitAliased)
        emitCc (nymphesCc, value);

    notifyValueChanged (nymphesCc, value);
}

//==============================================================================
std::unique_ptr<juce::XmlElement> CCEngine::getState() const
{
    auto xml = std::make_unique<juce::XmlElement> (stateTag);
    xml->setAttribute ("version", 1);

    const juce::ScopedLock sl (stateLock);
    xml->setAttribute ("midiChannel", midiChannel);
    xml->setAttribute ("defaultTakeover", defaultTakeover == Takeover::soft ? "soft" : "jump");

    for (int cc = 0; cc <= kMaxCc; ++cc)
    {
        const auto& p = params[(size_t) cc];
        const bool nonDefault = (p.value != defaultFor (cc)) || p.learnedCc >= 0
                                 || p.takeover != Takeover::jump;
        if (! nonDefault)
            continue;

        auto* e = xml->createNewChildElement ("param");
        e->setAttribute ("cc", cc);
        e->setAttribute ("value", p.value);
        if (p.learnedCc >= 0)
            e->setAttribute ("learnedCc", p.learnedCc);
        if (p.takeover != Takeover::jump)
            e->setAttribute ("takeover", "soft");
    }
    return xml;
}

void CCEngine::setState (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (stateTag))
        return;

    bool aliasTaken[128] = {};

    {
        const juce::ScopedLock sl (stateLock);

        // Route the channel through the setter so "37" in a hand-edited blob falls
        // back to omni instead of silencing every incoming message.
        setMidiChannel (xml.getIntAttribute ("midiChannel", 0));
        defaultTakeover = xml.getStringAttribute ("defaultTakeover", "jump") == "soft"
                            ? Takeover::soft : Takeover::jump;

        for (int cc = 0; cc <= kMaxCc; ++cc)
        {
            params[(size_t) cc] = Param {};
            params[(size_t) cc].value = defaultFor (cc);
            params[(size_t) cc].takeover = defaultTakeover;
        }

        for (auto* e : xml.getChildWithTagNameIterator ("param"))
        {
            const int cc = e->getIntAttribute ("cc", -1);
            if (! juce::isPositiveAndBelow (cc, 128) || ! isKnownCc (cc))
                continue;   // a state blob naming an unknown CC is dropped, not trusted

            auto& p = params[(size_t) cc];
            p.value = clampCc (e->getIntAttribute ("value", defaultFor (cc)));
            p.modified = (p.value != defaultFor (cc));

            // One alias per parameter, one parameter per alias (as the learn path
            // enforces): a duplicate in a blob would otherwise make
            // resolveIncomingCc() drive whichever CC has the lower index and leave
            // the other parameter's alias dead.
            const int learned = e->getIntAttribute ("learnedCc", -1);
            if (juce::isPositiveAndBelow (learned, 128) && ! aliasTaken[learned])
            {
                p.learnedCc = learned;
                aliasTaken[learned] = true;
            }
            else
            {
                p.learnedCc = -1;
            }

            p.takeover = e->getStringAttribute ("takeover", "jump") == "soft"
                            ? Takeover::soft : Takeover::jump;
            p.waitingForTakeover = (p.takeover == Takeover::soft);
            p.lastRawFromControl = -1;
        }
    }

    sendChangeMessage();
}

} // namespace nymphes
