// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_ammo.hpp"

#include "item_data.hpp"
#include "saving.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
AmmoMag::AmmoMag(ItemData* const item_data) :
        Ammo(item_data)
{
        set_full_ammo();
}

void AmmoMag::save_hook() const
{
        saving::put_int(m_ammo);
}

void AmmoMag::load_hook()
{
        m_ammo = saving::get_int();
}

void AmmoMag::set_full_ammo()
{
        m_ammo = m_data->ranged.max_ammo;
}

std::string AmmoMag::name_info_str(const ItemNameIdentified id_type) const
{
        (void)id_type;

        return "(" + std::to_string(m_ammo) + ")";
}

}  // namespace item
