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

#include "BonusLayer.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <chrono>

BonusLayer::BonusLayer(uint32 characterId)
    : m_characterId(characterId),
      m_lastRecomputeTime(0)
{
}

BonusLayer::~BonusLayer()
{
    SaveToDatabase();
}

void BonusLayer::Initialize()
{
    LoadFromDatabase();
    Recompute(RECOMPUTE_LOGIN);
}

void BonusLayer::Recompute(RecomputeReason reason)
{
    ClearCache();
    m_lastRecomputeTime = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;

    std::vector<std::string> fields = {
        "strength", "agility", "stamina", "intellect", "spirit",
        "armor", "dodge_rating", "parry_rating", "block_rating",
        "armor_penetration_rating", "hit_rating", "expertise_rating",
        "crit_rating", "haste_rating", "damage_reduction",
        "move_speed", "cc_mitigation",
        "ap_multiplier", "sp_multiplier", "healing_bonus"
    };

    for (const auto& field : fields)
    {
        int32 value = ComputeFieldValue(field);
        m_cache[field] = value;
        LOG_DEBUG("entities.player.bonus_layer", "BonusLayer::Recompute() Character {} field {} = {} (reason: {})",
            m_characterId, field, value, reason);
    }
}

int32 BonusLayer::GetEffectiveValue(const std::string& fieldName) const
{
    auto it = m_cache.find(fieldName);
    if (it != m_cache.end())
    {
        return it->second;
    }
    return 0;
}

std::vector<ComputedBonus> BonusLayer::GetAllEffectiveValues() const
{
    std::vector<ComputedBonus> result;
    for (const auto& [fieldName, value] : m_cache)
    {
        result.push_back({fieldName, value, m_lastRecomputeTime});
    }
    return result;
}

void BonusLayer::Write(const std::string& fieldName, const std::string& sourceTag, int32 value, const std::string& context)
{
    BonusEntry entry{
        0, fieldName, sourceTag, value,
        std::chrono::system_clock::now().time_since_epoch().count() / 1000000,
        context
    };

    m_ledgers[fieldName].push_back(entry);
    LOG_DEBUG("entities.player.bonus_layer", "BonusLayer::Write() Character {} field {} source {} value {} context {}",
        m_characterId, fieldName, sourceTag, value, context);
}

void BonusLayer::Revoke(const std::string& fieldName, const std::string& sourceTag, const std::string& context)
{
    Write(fieldName, sourceTag, 0, context);
}

void BonusLayer::Reset()
{
    m_ledgers.clear();
    m_cache.clear();
    m_lastRecomputeTime = 0;
    LOG_INFO("entities.player.bonus_layer", "BonusLayer::Reset() Character {} - all bonuses cleared", m_characterId);
}

void BonusLayer::LoadFromDatabase()
{
    auto result = CharacterDatabase.Query("SELECT id, field_name, source, value, created_at, context FROM bonus_layer_ledger WHERE character_id = {} ORDER BY created_at ASC", m_characterId);
    if (!result)
    {
        LOG_DEBUG("entities.player.bonus_layer", "BonusLayer::LoadFromDatabase() Character {} - no entries found", m_characterId);
        return;
    }

    do
    {
        auto fields = result->Fetch();
        BonusEntry entry{
            fields[0].Get<uint64>(),
            fields[1].Get<std::string>(),
            fields[2].Get<std::string>(),
            fields[3].Get<int32>(),
            fields[4].Get<uint64>(),
            fields[5].Get<std::string>()
        };
        m_ledgers[entry.fieldName].push_back(entry);
    } while (result->NextRow());

    LOG_DEBUG("entities.player.bonus_layer", "BonusLayer::LoadFromDatabase() Character {} - loaded {} ledger entries", m_characterId, result->GetRowCount());
}

void BonusLayer::SaveToDatabase()
{
    for (const auto& [fieldName, entries] : m_ledgers)
    {
        for (const auto& entry : entries)
        {
            CharacterDatabase.Execute(
                "INSERT INTO bonus_layer_ledger (character_id, field_name, source, value, context) VALUES ({}, '{}', '{}', {}, '{}')",
                m_characterId, fieldName, entry.source, entry.value, entry.context
            );
        }
    }
}

void BonusLayer::ClearCache()
{
    m_cache.clear();
}

int32 BonusLayer::ComputeFieldValue(const std::string& fieldName)
{
    auto it = m_ledgers.find(fieldName);
    if (it == m_ledgers.end())
    {
        return 0;
    }

    int32 sum = 0;
    for (const auto& entry : it->second)
    {
        sum += entry.value;
    }

    return sum;
}
