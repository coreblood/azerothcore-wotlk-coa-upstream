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

#include "AccountVault.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"

void AccountVault::Load()
{
    _slots.clear();
    _counts.clear();

    // item_instance carries the entry and stack size; the vault row only owns
    // placement, so there is nothing to keep in sync between the two.
    QueryResult result = CharacterDatabase.Query(
        "SELECT v.slot, v.item_guid, i.itemEntry, i.count FROM account_vault v "
        "JOIN item_instance i ON i.guid = v.item_guid WHERE v.account_id = {}", _accountId);

    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint16 slot = fields[0].Get<uint16>();

        VaultEntry entry;
        entry.itemGuid = fields[1].Get<uint32>();
        entry.itemEntry = fields[2].Get<uint32>();
        entry.count = fields[3].Get<uint32>();

        if (slot >= CAPACITY)
        {
            LOG_ERROR("entities.player.items", "AccountVault: account {} has item {} in out-of-range slot {}",
                _accountId, entry.itemGuid, slot);
            continue;
        }

        _slots[slot] = entry;
        IndexAdd(entry.itemEntry, entry.count);
    } while (result->NextRow());
}

bool AccountVault::Deposit(Item* item, Player* depositor)
{
    if (!item || !depositor)
        return false;

    uint16 slot = FindFreeSlot();
    if (slot == INVALID_SLOT)
        return false;

    ObjectGuid::LowType itemGuid = item->GetGUID().GetCounter();

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    // A freshly looted item is still ITEM_NEW with no item_instance row, so the
    // vault's foreign key would reject it. Flush it first.
    item->SaveToDB(trans);

    // Detach from the depositing character. Player::DeleteFromDB runs
    // DELETE FROM item_instance WHERE owner_guid = ?, and characters are deleted
    // routinely here by Maw permadeath and by Ascension, so an item left
    // attached would be destroyed along with whoever happened to deposit it -
    // taking the whole account's vault with it.
    trans->Append("UPDATE item_instance SET owner_guid = 0 WHERE guid = {}", itemGuid);
    trans->Append("INSERT INTO account_vault (account_id, slot, item_guid, deposited_by) VALUES ({}, {}, {}, {})",
        _accountId, slot, itemGuid, depositor->GetGUID().GetCounter());
    CharacterDatabase.CommitTransaction(trans);

    VaultEntry entry;
    entry.itemGuid = itemGuid;
    entry.itemEntry = item->GetEntry();
    entry.count = item->GetCount();

    _slots[slot] = entry;
    IndexAdd(entry.itemEntry, entry.count);

    // Drop the object without ITEM_REMOVED, which would delete the row we just
    // pointed the vault at. Leaving the update queue while ownership still
    // matches keeps RemoveFromUpdateQueueOf quiet.
    item->RemoveFromUpdateQueueOf(depositor);
    if (item->IsInWorld())
        item->RemoveFromWorld();

    delete item;
    return true;
}

Item* AccountVault::Withdraw(uint16 slot, Player* receiver)
{
    if (!receiver)
        return nullptr;

    auto itr = _slots.find(slot);
    if (itr == _slots.end())
        return nullptr;

    VaultEntry const entry = itr->second;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry.itemEntry);
    if (!proto)
    {
        LOG_ERROR("entities.player.items", "AccountVault: account {} slot {} holds item {} with unknown entry {}",
            _accountId, slot, entry.itemGuid, entry.itemEntry);
        return nullptr;
    }

    QueryResult result = CharacterDatabase.Query(
        "SELECT creatorGuid, giftCreatorGuid, count, duration, charges, flags, enchantments, randomPropertyId, "
        "durability, playedTime, text FROM item_instance WHERE guid = {}", entry.itemGuid);

    if (!result)
    {
        LOG_ERROR("entities.player.items", "AccountVault: account {} slot {} references missing item_instance {}",
            _accountId, slot, entry.itemGuid);
        return nullptr;
    }

    Item* item = NewItemOrBag(proto);
    if (!item->LoadFromDB(entry.itemGuid, receiver->GetGUID(), result->Fetch(), entry.itemEntry))
    {
        delete item;
        return nullptr;
    }

    CharacterDatabase.Execute("DELETE FROM account_vault WHERE account_id = {} AND slot = {}", _accountId, slot);

    IndexRemove(entry.itemEntry, entry.count);
    _slots.erase(itr);
    return item;
}

uint32 AccountVault::GetItemCount(uint32 itemEntry) const
{
    auto itr = _counts.find(itemEntry);
    return itr != _counts.end() ? itr->second : 0;
}

uint16 AccountVault::FindFreeSlot() const
{
    for (uint16 slot = 0; slot < CAPACITY; ++slot)
        if (!_slots.count(slot))
            return slot;

    return INVALID_SLOT;
}

void AccountVault::IndexAdd(uint32 itemEntry, uint32 count)
{
    _counts[itemEntry] += count;
}

void AccountVault::IndexRemove(uint32 itemEntry, uint32 count)
{
    auto itr = _counts.find(itemEntry);
    if (itr == _counts.end())
        return;

    if (itr->second <= count)
        _counts.erase(itr);
    else
        itr->second -= count;
}
