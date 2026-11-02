#include "PresetBank.h"

#include <algorithm>

namespace nymphes
{

namespace
{
// Banks and programs per page: the 49 locations of a page are 7 x 7. Named once so the
// forward (Slot::linearIndex) and reverse (slotFor) mappings cannot drift apart.
constexpr int kProgramsPerBank = 7;
static_assert (kSlotsPerPage == kProgramsPerBank * kProgramsPerBank,
               "a page is 7 banks of 7 programs");

// A stable empty preset for lookups of slots the bank has never seen.
const Preset& emptyPreset()
{
    static const Preset p {};
    return p;
}

Page pageOf (const Preset& p)
{
    return p.address().isUser() ? Page::User : Page::Factory;
}
} // namespace

int PresetBank::indexFor (Page page, Slot s) noexcept
{
    if (! s.isValid()) return -1;
    const int base = (page == Page::Factory) ? kSlotsPerPage : 0;
    return base + s.linearIndex();
}

/** Page of a 0..kSize-1 index. Only that range is in contract: a negative index is
    reported as USER and an index >= kSize as FACTORY, so validate before calling -
    slotFor() and contains() both reject out-of-range input, this one cannot (a Page
    has no "invalid" value). */
Page PresetBank::pageFor (int index) noexcept
{
    return index >= kSlotsPerPage ? Page::Factory : Page::User;
}

Slot PresetBank::slotFor (int index) noexcept
{
    // An out-of-range index has no slot: '%' keeps the sign of the dividend, so a
    // negative index used to produce bank 0 / program 0 (a plausible-looking but
    // invalid Slot), and index 98 wrapped silently back to A1.
    if (index < 0 || index >= kSize) return Slot { 0, 0 };   // isValid() == false

    const int within = index % kSlotsPerPage;
    Slot s;
    s.bank    = static_cast<uint8_t> (within / kProgramsPerBank + 1);
    s.program = static_cast<uint8_t> (within % kProgramsPerBank + 1);
    return s;
}

const Preset& PresetBank::at (int index) const
{
    if (! contains (index)) return emptyPreset();
    return presets_[static_cast<size_t> (index)];
}

std::string PresetBank::nameOf (int index) const
{
    if (! contains (index)) return {};
    return displayName (presets_[static_cast<size_t> (index)]);
}

const Preset* PresetBank::find (Page page, Slot s) const noexcept
{
    const int i = indexFor (page, s);
    if (i < 0 || ! present_[static_cast<size_t> (i)]) return nullptr;
    return &presets_[static_cast<size_t> (i)];
}

bool PresetBank::set (const Preset& p)
{
    const int i = indexFor (pageOf (p), Slot::fromAddress (p.address()));
    if (i < 0) return false;
    if (! p.crcValid()) return false;
    presets_[static_cast<size_t> (i)] = p;
    present_[static_cast<size_t> (i)] = true;
    return true;
}

int PresetBank::importSyx (const std::string& path, std::string& error)
{
    // A partial import must not look like a clean one: the count says how many were
    // stored, and `error` names how many were dropped and why. `error` is cleared first
    // so a stale message cannot survive a successful import.
    error.clear();
    const auto presets = importSyxFile (path, error);
    int stored = 0, rejected = 0;
    for (const auto& p : presets)
        if (set (p)) ++stored;
        else         ++rejected;

    if (rejected > 0)
    {
        const std::string note = std::to_string (rejected)
                               + " preset(s) skipped (invalid slot or CRC)";
        error = error.empty() ? note : (error + "; " + note);
    }
    return stored;
}

bool PresetBank::exportSyx (const std::string& path, const Page* page, std::string& error) const
{
    std::vector<Preset> out;
    for (int i = 0; i < kSize; ++i)
    {
        if (! present_[static_cast<size_t> (i)]) continue;
        if (page != nullptr && pageOf (presets_[static_cast<size_t> (i)]) != *page) continue;
        out.push_back (presets_[static_cast<size_t> (i)]);
    }
    if (out.empty())
    {
        error = "nothing to export";
        return false;
    }
    return exportSyxFile (path, out, error);
}

int PresetBank::count (Page page) const noexcept
{
    int n = 0;
    for (int i = 0; i < kSize; ++i)
        if (present_[static_cast<size_t> (i)] && pageOf (presets_[static_cast<size_t> (i)]) == page) ++n;
    return n;
}

int PresetBank::countPresent() const noexcept
{
    return static_cast<int> (std::count (present_.begin(), present_.end(), true));
}

void PresetBank::clear()
{
    presets_.fill (Preset {});
    present_.fill (false);
}

const char* PresetBank::vendorFactoryFile() { return "Nymphes_Factory_Presets_Factory_Bank.syx"; }
const char* PresetBank::vendorUserFile()    { return "Nymphes_Factory_Presets_User_Bank.syx"; }

} // namespace nymphes
