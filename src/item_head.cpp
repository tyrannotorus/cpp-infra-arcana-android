// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_head.hpp"

#include "global.hpp"
#include "inventory.hpp"
#include "msg_log.hpp"
#include "saving.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
void GasMask::decr_turns_left(Inventory& carrier_inv)
{
        --m_nr_turns_left;

        if (m_nr_turns_left <= 0) {
                const std::string item_name = name(ItemNameType::plain, ItemNameInfo::none);

                msg_log::add(
                        "My " + item_name + " expires.",
                        colors::msg_note(),
                        MsgInterruptPlayer::yes,
                        MorePromptOnMsg::yes);

                carrier_inv.decr_item(this);
        }
}

std::string GasMask::name_info_str(const ItemNameIdentified id_type) const
{
        (void)id_type;

        return "(" + std::to_string(m_nr_turns_left) + " turns)";
}

void GasMask::save_hook() const
{
        saving::put_int(m_nr_turns_left);
}

void GasMask::load_hook()
{
        m_nr_turns_left = saving::get_int();
}

}  // namespace item
