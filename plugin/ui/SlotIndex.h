// plugin/ui/SlotIndex.h  —  L1
//
// JUCE-free slot maths shared by the UI's ProcessorAccess helpers.
//
// Why this is not inline in ProcessorAccess.h: that header includes the processor
// and therefore JUCE, so anything in it can only be tested by linking the whole
// plugin. The two rules that actually matter — "an index outside the bank is never
// a slot", and "the bytes we hand the write path must be addressed to the slot we
// are writing" — are pure functions over preset/Preset.h, so they live here and
// tests/slot_index_test.cpp covers them without JUCE.
//
// PresetBank's own pageFor()/slotFor() are index arithmetic without a validity
// check: pageFor(-1) answers USER, and slotFor(98) wraps to USER A1 while
// pageFor(98) says FACTORY. The bank index convention is bank-major:
//   0..48  = USER    A1..G7
//   49..97 = FACTORY A1..G7
// and -1 is the documented "no slot" sentinel (PresetBank::indexFor returns it).

#pragma once

#include <optional>

#include "preset/PresetBank.h"

namespace nymphes::ui
{

/** 0 <= index < PresetBank's 98 slots. */
inline bool isValidSlotIndex (int index) noexcept
{
    return index >= 0 && index < PresetBank::kSize;
}

/** The page of a valid index, or nothing when the index is outside the bank. */
inline std::optional<Page> pageOfSlotIndex (int index) noexcept
{
    if (! isValidSlotIndex (index))
        return std::nullopt;
    return PresetBank::pageFor (index);
}

/** The slot of a valid index, or an invalid Slot (isValid() == false) otherwise. */
inline Slot slotOfIndex (int index) noexcept
{
    if (! isValidSlotIndex (index))
        return Slot { 0, 0 };   // deliberately not a valid A1..G7 location
    return PresetBank::slotFor (index);
}

/** True only for an in-range FACTORY index; out-of-range is not "maybe factory". */
inline bool isFactorySlotIndex (int index) noexcept
{
    return isValidSlotIndex (index) && PresetBank::pageFor (index) == Page::Factory;
}

/** The bytes to hand the write path for "save this preset into USER slot `target`".
 *
 *  Preset::encodeFrame()/the processor's saveToUserSlot() refuse any preset whose
 *  own address is FACTORY, and the address is what the refusal reads. A preset
 *  auditioned from a FACTORY slot therefore carries FACTORY, so copying it into a
 *  USER slot used to fail with "Refusing to write a FACTORY slot" even though the
 *  target had already been checked to be a USER slot. Re-addressing a copy here
 *  keeps that guard intact (it still refuses a factory-addressed write) while
 *  making factory -> USER copy work, which is a normal thing to want.
 */
inline Preset presetForUserWrite (const Preset& source, Slot target)
{
    Preset copy = source;
    copy.setAddress (Address { kPageUser, target.bank, target.program });
    return copy;
}

} // namespace nymphes::ui
