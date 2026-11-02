// tests/slot_index_test.cpp — L1
//
// JUCE-free checks for plugin/ui/SlotIndex.h, the slot maths the editor's
// ProcessorAccess helpers are built on. Two properties matter and both used to be
// unguarded:
//
//   1. An index outside the bank is never a slot. PresetBank::pageFor(-1) answers
//      USER and PresetBank::slotFor(98) wraps to USER A1 while pageFor(98) is
//      FACTORY, so a stale index from a model row silently addressed a valid but
//      wrong location.
//   2. A preset auditioned from a FACTORY slot carries page FACTORY, and
//      Preset::encodeFrame()/the processor's saveToUserSlot() refuse a
//      factory-addressed write. Copying such a preset into a USER slot therefore
//      has to re-address a copy; the guard itself must stay intact.
//
// Exits non-zero on any failure so ctest reports it.

#include "plugin/ui/SlotIndex.h"

#include <cstdio>
#include <string>

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
}

int main()
{
    using namespace nymphes;
    using namespace nymphes::ui;

    // --- 1. index validity -----------------------------------------------------
    check (isValidSlotIndex (0), "index 0 is a slot");
    check (isValidSlotIndex (48), "index 48 is the last USER slot");
    check (isValidSlotIndex (49), "index 49 is the first FACTORY slot");
    check (isValidSlotIndex (97), "index 97 is the last slot");
    check (! isValidSlotIndex (-1), "index -1 is not a slot");
    check (! isValidSlotIndex (98), "index 98 is past the bank");
    check (! isValidSlotIndex (-2147483647 - 1), "INT_MIN is not a slot");
    check (! isValidSlotIndex (2147483647), "INT_MAX is not a slot");

    // --- 2. page / slot of an index --------------------------------------------
    check (! pageOfSlotIndex (-1).has_value(), "no page for -1");
    check (! pageOfSlotIndex (98).has_value(), "no page for 98");
    check (pageOfSlotIndex (0).value_or (Page::Factory) == Page::User, "index 0 is USER");
    check (pageOfSlotIndex (48).value_or (Page::Factory) == Page::User, "index 48 is USER");
    check (pageOfSlotIndex (49).value_or (Page::User) == Page::Factory, "index 49 is FACTORY");
    check (pageOfSlotIndex (97).value_or (Page::User) == Page::Factory, "index 97 is FACTORY");

    check (! slotOfIndex (-1).isValid(), "slotOfIndex(-1) is not a valid slot");
    check (! slotOfIndex (98).isValid(), "slotOfIndex(98) is not a valid slot");

    const auto a1 = slotOfIndex (0);
    check (a1.isValid() && a1.bank == 1 && a1.program == 1, "index 0 is A1");
    const auto g7 = slotOfIndex (48);
    check (g7.isValid() && g7.bank == 7 && g7.program == 7, "index 48 is G7");
    const auto fA1 = slotOfIndex (49);
    check (fA1.isValid() && fA1.bank == 1 && fA1.program == 1, "index 49 is FACTORY A1");

    // --- 3. factory test -------------------------------------------------------
    check (! isFactorySlotIndex (-1), "out-of-range index is not a factory slot");
    check (! isFactorySlotIndex (98), "index 98 is not a factory slot");
    check (! isFactorySlotIndex (48), "index 48 is a USER slot");
    check (isFactorySlotIndex (49), "index 49 is a FACTORY slot");
    check (isFactorySlotIndex (97), "index 97 is a FACTORY slot");

    // --- 4. the bytes handed to the write path ---------------------------------
    // A preset addressed to FACTORY, as auditioned from a factory slot.
    Bytes payload (80, 0);
    payload[1] = 0x08;   // arbitrary protobuf body; only the length matters here
    payload[2] = 0x01;
    payload[0] = crc8 (payload.data() + 1, payload.size() - 1);

    Preset fromFactory;
    fromFactory.setPayload (payload);
    fromFactory.setAddress (Address { kPageFactory, 1, 1 });

    // Negative control, and the reason this test exists: the factory-addressed copy
    // the browser used to hand to saveToUserSlot() is refused by the codec itself.
    check (fromFactory.encodeFrame (true, false).empty(),
           "a FACTORY-addressed preset is refused with the write flag (guard intact)");

    // The fixed path: re-addressed to the USER slot being written.
    const Slot target { 3, 4 };   // USER C4
    const Preset forWrite = presetForUserWrite (fromFactory, target);
    check (forWrite.address().page == kPageUser, "re-addressed copy is on the USER page");
    check (forWrite.address().bank == 3 && forWrite.address().program == 4,
           "re-addressed copy names the target slot");

    const Bytes frame = forWrite.encodeFrame (true, false);
    check (! frame.empty(), "a USER-addressed copy serialises with the write flag");
    check (frame.size() > 9 && frame[6] == kFlagWrite, "frame carries FF=0x01 (write)");
    check (frame.size() > 9 && frame[7] == kPageUser, "frame carries AA=0x00 (USER)");
    check (frame.size() > 9 && frame[8] == 3 && frame[9] == 4, "frame carries the target slot");
    check (forWrite.payload() == fromFactory.payload(), "the copy keeps the payload verbatim");

    // The source must be untouched: the editor reverts to it on a failed write.
    check (fromFactory.address().page == kPageFactory,
           "presetForUserWrite does not mutate its source");

    if (failures == 0)
        std::printf ("slot index: all checks passed\n");
    else
        std::printf ("slot index: %d FAILURES\n", failures);

    return failures == 0 ? 0 : 1;
}
