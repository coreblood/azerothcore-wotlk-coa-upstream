/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ACCOUNT_VAULT_H_
#define _ACCOUNT_VAULT_H_

#include "Common.h"
#include "ObjectGuid.h"
#include <unordered_map>

class Item;
class Player;

struct VaultEntry
{
    ObjectGuid::LowType itemGuid;
    uint32 itemEntry;
    uint32 count;

    // Live object for items deposited this session, otherwise nullptr and the
    // row is the only representation. The acquisition API hands its caller the
    // Item it just stored - LootHandler passes it straight to SendNewItem - so
    // a deposit cannot destroy the object the way a row-only vault would want
    // to. Bounded by what a session actually acquires, not by CAPACITY.
    Item* live;
};

/*
 * Account-wide item storage, and the only storage on this server: there are no
 * bags and no backpack, so everything a character acquires ends up here.
 *
 * Rows are deliberately NOT instantiated as Item objects the way
 * character_inventory is. They become items on withdraw, so a full vault costs
 * rows rather than 1200 live objects per online player.
 *
 * Deposited items are detached from their depositing character
 * (item_instance.owner_guid = 0). Player::DeleteFromDB runs
 * "DELETE FROM item_instance WHERE owner_guid = ?", so an item still carrying a
 * character's guid would be destroyed when that character is deleted - and
 * characters are deleted routinely here, by Maw permadeath and by Ascension.
 * Detaching is what stops one character's death emptying the whole account's
 * vault.
 */
class AccountVault
{
public:
    static constexpr uint16 CAPACITY = 1200;
    static constexpr uint16 INVALID_SLOT = 0xFFFF;

    explicit AccountVault(uint32 accountId) : _accountId(accountId) { }
    ~AccountVault();

    AccountVault(AccountVault const&) = delete;
    AccountVault& operator=(AccountVault const&) = delete;

    void Load();

    // Persists the item and takes ownership of the object, returning it so the
    // caller still has the live pointer the acquisition API promises. The caller
    // must already have taken it out of the player's slots with
    // Player::RemoveItem, which leaves the row intact, never DestroyItem.
    // Returns nullptr and stores nothing if the vault is full.
    Item* Deposit(Item* item, Player* depositor);

    // Rebuilds the row into an Item owned by the receiver and frees the slot.
    // Returns nullptr if the slot is empty or the row cannot be read.
    Item* Withdraw(uint16 slot, Player* receiver);

    // O(1). This is what makes the vault visible to Player::GetItemCount, which
    // quest completion reads, so a linear scan here would sit on a hot path.
    uint32 GetItemCount(uint32 itemEntry) const;

    [[nodiscard]] uint16 FindFreeSlot() const;
    [[nodiscard]] bool IsFull() const { return _slots.size() >= CAPACITY; }
    [[nodiscard]] uint16 GetUsedSlots() const { return static_cast<uint16>(_slots.size()); }

private:
    void IndexAdd(uint32 itemEntry, uint32 count);
    void IndexRemove(uint32 itemEntry, uint32 count);

    uint32 _accountId;
    std::unordered_map<uint16, VaultEntry> _slots;
    std::unordered_map<uint32, uint32> _counts;
};

#endif
