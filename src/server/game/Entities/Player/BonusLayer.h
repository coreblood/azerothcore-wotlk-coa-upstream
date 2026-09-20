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

#ifndef _BONUS_LAYER_H_
#define _BONUS_LAYER_H_

#include "Common.h"
#include <map>
#include <string>
#include <vector>

enum RecomputeReason
{
    RECOMPUTE_LOGIN = 0,
    RECOMPUTE_GEAR_CHANGE = 1,
    RECOMPUTE_SACRAMENT = 2,
    RECOMPUTE_REAPING = 3,
    RECOMPUTE_SCAR = 4,
    RECOMPUTE_ASCENSION = 5,
    RECOMPUTE_RANK_UP = 6
};

enum SourceTag
{
    SOURCE_GEAR = 0,
    SOURCE_SACRAMENT = 1,
    SOURCE_BLOODLINE = 2,
    SOURCE_LEGENDARY_RANK = 3,
    SOURCE_CLASS_SET = 4,
    SOURCE_REAPING = 5,
    SOURCE_SCAR = 6,
    SOURCE_ORIGIN_PATH = 7
};

struct BonusEntry
{
    uint64 id;
    std::string fieldName;
    std::string source;
    int32 value;
    uint64 createdAt;
    std::string context;
};

struct ComputedBonus
{
    std::string fieldName;
    int32 effectiveValue;
    uint64 lastRecomputeAt;
};

class BonusLayer
{
public:
    explicit BonusLayer(uint32 characterId);
    ~BonusLayer();

    void Initialize();
    void Recompute(RecomputeReason reason);

    int32 GetEffectiveValue(const std::string& fieldName) const;
    std::vector<ComputedBonus> GetAllEffectiveValues() const;

    void Write(const std::string& fieldName, const std::string& sourceTag, int32 value, const std::string& context = "");
    void Revoke(const std::string& fieldName, const std::string& sourceTag, const std::string& context = "");

    void Reset();
    void LoadFromDatabase();
    void SaveToDatabase();

    uint32 GetCharacterId() const { return m_characterId; }

private:
    uint32 m_characterId;
    std::map<std::string, std::vector<BonusEntry>> m_ledgers;
    std::map<std::string, int32> m_cache;
    uint64 m_lastRecomputeTime;

    void ApplyLedgerEntry(const std::string& fieldName, int32 value);
    void ClearCache();
    int32 ComputeFieldValue(const std::string& fieldName);
};

#endif
