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
    const Preset& at (int index) const;              // returns an empty Preset when absent
    std::string nameOf (int index) const;            // "USER A1" / "FACTORY C3" / "" when absent

    /** Look up a specific location. nullptr when that location is empty. */
    const Preset* find (Page, Slot) const noexcept;

    /** Store a preset at the location its own aa/slot fields name. Returns false
        for an invalid slot or a preset whose CRC does not check out. */
    bool set (const Preset&);

    /** Import every message of a .syx dump. Returns the number stored; fills
        `error` with the first problem but keeps everything that did parse. */
    int importSyx (const std::string& path, std::string& error);

    /** Export one page (or both, when `page` is nullptr) as a single .syx. */
    bool exportSyx (const std::string& path, const Page* page, std::string& error) const;

    int count (Page) const noexcept;
    int countPresent() const noexcept;

    /** Drop everything. */
    void clear();

    /** The default location to look for the vendor dumps: <dir>/<name>. */
    static const char* vendorFactoryFile();
    static const char* vendorUserFile();

private:
    std::array<Preset, kSize> presets_ {};
    std::array<bool, kSize>   present_ {};
};

} // namespace nymphes
