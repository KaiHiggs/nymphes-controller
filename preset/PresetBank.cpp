#include "PresetBank.h"

#include <algorithm>

namespace nymphes
{

namespace
{
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

Page PresetBank::pageFor (int index) noexcept
{
    return index >= kSlotsPerPage ? Page::Factory : Page::User;
}

Slot PresetBank::slotFor (int index) noexcept
{
    const int within = index % kSlotsPerPage;
    Slot s;
    s.bank    = static_cast<uint8_t> (within / 7 + 1);
    s.program = static_cast<uint8_t> (within % 7 + 1);
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
    const auto presets = importSyxFile (path, error);
    int stored = 0;
    for (const auto& p : presets)
        if (set (p)) ++stored;
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
