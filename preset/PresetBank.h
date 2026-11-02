// The 98 preset locations the Nymphes has: 49 USER + 49 FACTORY, each page being
// 7 banks (A..G) x 7 programs (1..7).
//
// Index convention, fixed and used by the CLI, the tests and the browser list:
//     index  0..48  ->  USER,    bank 1..7, program 1..7
//     index 49..97  ->  FACTORY, bank 1..7, program 1..7
// which is the order the vendor's Nymphes_Factory_Presets_User_Bank.syx uses
// (49 USER messages, then 49 FACTORY messages).
//
// A location is "present" only when a preset for it has actually been loaded or
// captured. The bank never invents a preset for a slot it has not seen: an empty
// slot must not be silently writable with zeros.

#pragma once

#include "Preset.h"

#include <array>
#include <string>
#include <vector>

namespace nymphes
{

class PresetBank
{
public:
    static constexpr int kSize = 2 * kSlotsPerPage;   // 98

    PresetBank() = default;

    static int       indexFor (Page, Slot) noexcept;
    static Page      pageFor (int index) noexcept;
    static Slot      slotFor (int index) noexcept;

    int  size() const noexcept { return kSize; }
    bool contains (int index) const noexcept { return index >= 0 && index < kSize && present_[index]; }
    /** Returns an empty Preset when absent (and when `index` is out of range).
        The reference aliases the bank's own storage and is invalidated by set(),
        importSyx() and clear(); the bank is not internally synchronised, so a caller
        that keeps this reference across a mutation - or reads it from another thread -
        observes stale or raced data. */
    const Preset& at (int index) const;
    std::string nameOf (int index) const;            // "USER A1" / "FACTORY C3" / "" when absent

    /** Look up a specific location. nullptr when that location is empty. The pointer
        aliases internal storage, with the same invalidation and threading rules as
        at(). */
    const Preset* find (Page, Slot) const noexcept;

    /** Store a preset at the location its own aa/slot fields name. Returns false
        for an invalid slot or a preset whose CRC does not check out. */
    bool set (const Preset&);

    /** Import every message of a .syx dump. Returns the number stored; fills
        `error` with the first problem but keeps everything that did parse. */
    int importSyx (const std::string& path, std::string& error);

    /** Export one page (or both, when `page` is nullptr - a non-owning pointer that is
        read once, never stored) as a single .syx. Returns false and fills `error` when
        the selection is empty: an empty bank is refused rather than written as a
        0-message file that the CLI and the device would both read as an empty page. */
    bool exportSyx (const std::string& path, const Page* page, std::string& error) const;

    int count (Page) const noexcept;
    int countPresent() const noexcept;

    /** Drop everything. */
    void clear();

    /** The default location to look for the vendor dumps: <dir>/<name>.

        Two different files, not two views of one:
          * vendorUserFile()    - Nymphes_Factory_Presets_User_Bank.syx, 98 messages:
            49 USER then 49 FACTORY. That order is what the index convention above
            (0..48 USER, 49..97 FACTORY) reproduces.
          * vendorFactoryFile() - Nymphes_Factory_Presets_Factory_Bank.syx, 49 FACTORY
            messages only.

        Neither file ships in this repository (the shared gate root holds them); both
        are resolved relative to <dir>, so a caller must check whether the file it gets
        back exists before reading it. */
    static const char* vendorFactoryFile();
    static const char* vendorUserFile();

private:
    std::array<Preset, kSize> presets_ {};
    std::array<bool, kSize>   present_ {};
};

} // namespace nymphes
