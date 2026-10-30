// plugin/cc/CCEngine.cpp  —  L1

#include "plugin/cc/CCEngine.h"

namespace nymphes
{

// The engine is addressed by the *Nymphes* CC number. A learned controller CC is an
// alias that resolves to one of those; incoming CCs on a Nymphes number address that
// parameter directly. See resolveIncomingCc().

namespace
{
    constexpr int kMaxCc = 127;

    inline int clampCc (int v) noexcept { return juce::jlimit (0, kMaxCc, v); }

    // A CC number is "known" when the Nymphes actually implements it. CC 38 is
    // genuinely absent (see CcMapData.h), so we must not invent a parameter for
    // it: a learned alias pointing at an unknown CC is refused rather than
    // silently stored.
    inline bool isKnownCc (int cc) noexcept { return findCC (cc) != nullptr; }
}

CCEngine::CCEngine() = default;
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
void CCEngine::setMidiSink (MidiSink newSink)   { sink = std::move (newSink); }

void CCEngine::setMidiChannel (int oneBasedChannel)
{
    midiChannel = (oneBasedChannel >= 1 && oneBasedChannel <= 16) ? oneBasedChannel : 0;
}

void CCEngine::attachToProcessor (juce::AudioProcessor& p) { attachedProcessor = &p; }
void CCEngine::detachFromProcessor() { attachedProcessor = nullptr; }

//==============================================================================
int CCEngine::getValue (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return 0;
    return params[(size_t) nymphesCc].value;
}

void CCEngine::setValue (int nymphesCc, int value)
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128) || ! isKnownCc (nymphesCc))
        return;

    value = clampCc (value);
    auto& p = params[(size_t) nymphesCc];

    if (p.value == value && p.modified)
        return;

    p.value = value;
    p.modified = (value != defaultFor (nymphesCc));

    // A value set from the UI or the host invalidates any half-finished
    // takeover: the physical control has to catch up to the new position.
    p.waitingForTakeover = (p.takeover == Takeover::soft);

    emitCc (nymphesCc, value);
    notifyValueChanged (nymphesCc, value);
}

void CCEngine::setValueFromDevice (int nymphesCc, int value)
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return;

    value = clampCc (value);
    auto& p = params[(size_t) nymphesCc];
    if (p.value == value)
        return;

    p.value = value;
    p.modified = (value != defaultFor (nymphesCc));
    p.waitingForTakeover = false;
    notifyValueChanged (nymphesCc, value);
}

void CCEngine::refreshAll()
{
    for (int cc = 0; cc <= kMaxCc; ++cc)
        if (isKnownCc (cc) && params[(size_t) cc].value != defaultFor (cc))
            emitCc (cc, params[(size_t) cc].value);
}

bool CCEngine::isModified (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return false;
    return params[(size_t) nymphesCc].modified;
}

//==============================================================================
void CCEngine::emitCc (int nymphesCc, int value)
{
    if (! emitEnabled || sink == nullptr)
        return;

    sink (juce::MidiMessage::controllerEvent (midiChannel == 0 ? 1 : midiChannel,
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
    learnTarget = nymphesCc;
    sendChangeMessage();
}

void CCEngine::cancelLearn()
{
    if (learnTarget < 0)
        return;
    learnTarget = -1;
    sendChangeMessage();
}

int CCEngine::getLearnedCc (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return -1;
    return params[(size_t) nymphesCc].learnedCc;
}

void CCEngine::clearLearn (int nymphesCc)
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return;
    params[(size_t) nymphesCc].learnedCc = -1;
    params[(size_t) nymphesCc].waitingForTakeover = false;
    sendChangeMessage();
}

void CCEngine::clearAllLearn()
{
    for (auto& p : params)
    {
        p.learnedCc = -1;
        p.waitingForTakeover = false;
    }
    sendChangeMessage();
}

int CCEngine::resolveIncomingCc (int incomingCc) const
{
    if (! juce::isPositiveAndBelow (incomingCc, 128))
        return -1;

    for (int cc = 0; cc <= kMaxCc; ++cc)
        if (params[(size_t) cc].learnedCc == incomingCc)
            return cc;

    // No alias: an incoming CC on a Nymphes parameter addresses that parameter.
    return isKnownCc (incomingCc) ? incomingCc : -1;
}

//==============================================================================
void CCEngine::setTakeover (int nymphesCc, Takeover mode)
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return;
    auto& p = params[(size_t) nymphesCc];
    p.takeover = mode;
    p.waitingForTakeover = (mode == Takeover::soft);
    p.lastRawFromControl = -1;
}

CCEngine::Takeover CCEngine::getTakeover (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return defaultTakeover;
    return params[(size_t) nymphesCc].takeover;
}

bool CCEngine::isWaitingForTakeover (int nymphesCc) const
{
    if (! juce::isPositiveAndBelow (nymphesCc, 128))
        return false;
    return params[(size_t) nymphesCc].waitingForTakeover;
}

bool CCEngine::applySoftTakeover (int nymphesCc, int rawValue)
{
    auto& p = params[(size_t) nymphesCc];

    if (! p.waitingForTakeover)
        return true;

    const int previous = p.lastRawFromControl;
    p.lastRawFromControl = rawValue;

    // The control engages when it *crosses* the parameter's current value, which
    // is what stops a knob jumping from 3 to 120 the moment it is touched. The
    // first message after a jump is only a reference point.
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
    if (midiChannel != 0 && m.getChannel() != midiChannel)
        return;

    if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const int value = m.getControllerValue();

        // 1. MIDI learn captures the next control that moves.
        if (learnTarget >= 0)
        {
            // Ignore the Nymphes' own parameter CCs while learning, so that
            // moving a knob on the synth does not become the alias for itself
            // unless it is a different number. We accept any CC that is not the
            // learn target itself.
            if (cc != learnTarget)
            {
                const int target = learnTarget;
                // One alias per parameter, and one parameter per alias.
                for (auto& p : params)
                    if (p.learnedCc == cc)
                        p.learnedCc = -1;

                params[(size_t) target].learnedCc = cc;
                params[(size_t) target].waitingForTakeover = true;
                params[(size_t) target].lastRawFromControl = -1;
                learnTarget = -1;

                if (onLearnComplete)
                    onLearnComplete (target, cc);
                sendChangeMessage();
                return;
            }
        }

        const int nymphesCc = resolveIncomingCc (cc);
        if (nymphesCc < 0)
            return;   // a CC the Nymphes does not implement, and nothing learned

        // 2. Soft takeover gate.
        if (params[(size_t) nymphesCc].takeover == Takeover::soft)
            if (! applySoftTakeover (nymphesCc, value))
            {
                sendChangeMessage();   // let the UI show "waiting"
                return;
            }

        params[(size_t) nymphesCc].value = value;
        params[(size_t) nymphesCc].modified = (value != defaultFor (nymphesCc));

        // 3. Only emit when the alias came from elsewhere. A CC arriving on the
        //    Nymphes' own number has already reached the synth over the same
        //    port, so echoing it back would double every message.
        if (nymphesCc != cc)
            emitCc (nymphesCc, value);

        notifyValueChanged (nymphesCc, value);
        return;
    }

    if (m.isChannelPressure())
    {
        // Aftertouch is a modulation *source* on the Nymphes, not a parameter,
        // so it is passed straight through by the processor; nothing to store.
        return;
    }
}

//==============================================================================
std::unique_ptr<juce::XmlElement> CCEngine::getState() const
{
    auto xml = std::make_unique<juce::XmlElement> (stateTag);
    xml->setAttribute ("version", 1);
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

    midiChannel = xml.getIntAttribute ("midiChannel", 0);
    defaultTakeover = xml.getStringAttribute ("defaultTakeover", "jump") == "soft"
                        ? Takeover::soft : Takeover::jump;

    for (auto& p : params)
    {
        p = Param {};
        p.takeover = defaultTakeover;
    }

    for (auto* e : xml.getChildWithTagNameIterator ("param"))
    {
        const int cc = e->getIntAttribute ("cc", -1);
        if (! juce::isPositiveAndBelow (cc, 128) || ! isKnownCc (cc))
            continue;   // a state blob naming an unknown CC is dropped, not trusted

        auto& p = params[(size_t) cc];
        p.value = clampCc (e->getIntAttribute ("value", defaultFor (cc)));
        p.modified = (p.value != defaultFor (cc));
        const int learned = e->getIntAttribute ("learnedCc", -1);
        p.learnedCc = juce::isPositiveAndBelow (learned, 128) ? learned : -1;
        p.takeover = e->getStringAttribute ("takeover", "jump") == "soft"
                        ? Takeover::soft : Takeover::jump;
        p.waitingForTakeover = (p.takeover == Takeover::soft);
        p.lastRawFromControl = -1;
    }

    sendChangeMessage();
}

} // namespace nymphes
