# Bonus Layer Integration Guide

**Status**: Phase 1 (Core Infrastructure) + Basic C++ Integration | **Branch**: `feature/bonus-layer-integration`

---

## Overview

This document describes the integration of the TypeScript BonusLayer system with Ascensioncore's C++ Player entity. The integration follows a dual-store pattern:

- **Ledger**: Immutable SQL table (`bonus_layer_ledger`) recording all write/revoke events
- **Cache**: Computed values computed on 7 discrete triggers, stored in `bonus_layer_cache` SQL table

---

## Deliverables (Complete)

### 1. Database Migrations
- ✓ `rev_20260920_00_bonus_layer_ledger.sql` - Immutable entry log
- ✓ `rev_20260920_01_bonus_layer_cache.sql` - Computed cache

### 2. C++ Headers & Implementation
- ✓ `BonusLayer.h` - Header with public API
- ✓ `BonusLayer.cpp` - Core implementation

### 3. Player Integration
- ✓ Modified `Player.h`:
  - Added `#include "BonusLayer.h"`
  - Added `std::unique_ptr<BonusLayer> m_bonusLayer` member
  - Added public methods:
    - `BonusLayer* GetBonusLayer()`
    - `void RecomputeBonusLayer(RecomputeReason reason)`
    - `int32 GetBonusValue(const std::string& fieldName)`

- ✓ Modified `Player.cpp`:
  - Implemented `RecomputeBonusLayer()` - Creates and initializes BonusLayer on first call
  - Implemented `GetBonusValue()` - Returns cached effective value for a field

- ✓ Modified `PlayerStorage.cpp`:
  - Added `RecomputeBonusLayer(RECOMPUTE_LOGIN)` after `InitStatsForLevel()` in `LoadFromDB()`

---

## Recompute Triggers (7 Events)

Each trigger must call `player->RecomputeBonusLayer(RecomputeReason)` after the event completes.

### 1. Login (RECOMPUTE_LOGIN)
**When**: Character loaded from database  
**Where**: `PlayerStorage.cpp::LoadFromDB()` ✓ DONE  
**Action**: Rebuild cache from ledger

```cpp
RecomputeBonusLayer(RECOMPUTE_LOGIN);
```

### 2. Gear Change (RECOMPUTE_GEAR_CHANGE)
**When**: Item equipped, unequipped, or swapped  
**Where**: `Player.cpp::EquipItem()`, `Player.cpp::RemoveItem()`, Item swap logic  
**Action**: Recalculate armor, resistances, ratings from equipped items

**Implementation location**: Find in Player.cpp where equip events are handled, likely in:
- `Player::EquipItem()` - After item is equipped
- `Player::UnequipItem()` - After item is removed
- Item swap event handlers

### 3. Sacrament (RECOMPUTE_SACRAMENT)
**When**: Player makes altar offering (sacrament system)  
**Where**: Custom sacrament script in `/src/server/scripts/Custom/`  
**Action**: Recalculate stat plateaus from sacrament history

**Implementation location**: Hook sacrament completion event

### 4. Reaping (RECOMPUTE_REAPING)
**When**: Contract completion and reward claim  
**Where**: Custom reaping system scripts  
**Action**: Recalculate from reaping rewards  

**Implementation location**: Hook contract completion/reward claim

### 5. Scar (RECOMPUTE_SCAR)
**When**: Passive slot or callus slot changes  
**Where**: Custom scar system scripts  
**Action**: Recalculate from equipped passives/calluses

**Implementation location**: Hook passive/callus slot change events

### 6. Ascension (RECOMPUTE_ASCENSION)
**When**: Character ascends and resets with bloodline inheritance  
**Where**: Custom ascension logic  
**Action**: Reset all ledgers, then rebuild from inherited bloodline bonus layer

**Implementation location**: Hook ascension event, call `GetBonusLayer()->Reset()` then inherit

### 7. Rank-Up (RECOMPUTE_RANK_UP)
**When**: Legendary rank increases  
**Where**: Legendary rank/progression system  
**Action**: Recalculate from accumulated rank rewards

**Implementation location**: Hook rank progression completion

---

## Usage Examples

### Reading a Stat Bonus
```cpp
Player* player = GetPlayer();
int32 armorBonus = player->GetBonusValue("armor");
int32 dodgeRating = player->GetBonusValue("dodge_rating");
```

### Writing a Bonus (from gear system)
```cpp
BonusLayer* layer = player->GetBonusLayer();
if (layer)
{
    layer->Write("armor", "gear", 500, "Plate chest T10");
    player->RecomputeBonusLayer(RECOMPUTE_GEAR_CHANGE);
}
```

### Revoking a Bonus (on unequip)
```cpp
BonusLayer* layer = player->GetBonusLayer();
if (layer)
{
    layer->Revoke("armor", "gear", "Plate chest T10 removed");
    player->RecomputeBonusLayer(RECOMPUTE_GEAR_CHANGE);
}
```

### Resetting on Ascension
```cpp
player->GetBonusLayer()->Reset();
// Then inherit from account bloodline...
player->RecomputeBonusLayer(RECOMPUTE_ASCENSION);
```

---

## Field Names (18 Combat Fields)

All field names are **lowercase with underscores**:

### Primary Stats (5)
- `strength`
- `agility`
- `stamina`
- `intellect`
- `spirit`

### Armor & Mitigation (3)
- `armor`
- `damage_reduction`
- `block_value`

### Avoidance Ratings (3)
- `dodge_rating`
- `parry_rating`
- `block_rating`

### Offensive Ratings (3)
- `hit_rating`
- `expertise_rating`
- `crit_rating`

### DPS & Healing (3)
- `haste_rating`
- `armor_penetration_rating`
- `move_speed`

### Crowd Control (1)
- `cc_mitigation`

### Meta Fields (6 - computed multipliers)
- `ap_multiplier` - Attack power scaling
- `sp_multiplier` - Spell power scaling
- `healing_bonus` - Healing bonus multiplier
- `last_stand_pct` - DR temporary cap (60% max)
- `loot_pct` - Loot quality multiplier (uncapped)
- `gold_pct` - Gold reward multiplier

---

## Hard-Cap & Surplus Routing Rules

| Field | Cap | Overflow Route |
|-------|-----|-----------------|
| dodge_rating | 100% | → block_rating |
| parry_rating | 100% | → block_rating |
| block_rating | 100% | → armor |
| damage_reduction | 100% | → armor (can't be 0 incoming) |
| move_speed | 100% | → dodge_rating (can't be infinitely fast) |
| cc_mitigation | 100% | → duration_reduction (becomes partial immunity) |
| hit_rating | uncapped | → dust (economy resource) |
| expertise_rating | uncapped | → dust |
| armor_pen_rating | uncapped | → dust |
| ap_multiplier | uncapped | - |
| sp_multiplier | uncapped | - |
| damage mods | uncapped | - |

---

## Testing

### Unit Tests
TypeScript test suite in `server/systems/bonus_layer/`:
- `bonus_layer.test.ts` - Phase 1 core (23+ tests)
- `phase2.test.ts` - Integration (35+ tests)
- `phase3.test.ts` - Overflow & edge cases (30+ tests)

### Integration Tests (To Be Implemented)
- [ ] Load character, verify ledger loaded, cache populated
- [ ] Equip item, verify armor bonus written, cache recomputed
- [ ] Multiple sources on same field, verify sum correct
- [ ] Hard-cap field overflow, verify surplus routed
- [ ] Ascend character, verify ledger reset, bloodline inherited
- [ ] Negative bonus (revoke), verify soft cap works
- [ ] Performance: 100 ledger entries recompute < 100ms

---

## File Locations

| File | Purpose |
|------|---------|
| `src/server/game/Entities/Player/BonusLayer.h` | C++ header |
| `src/server/game/Entities/Player/BonusLayer.cpp` | C++ implementation |
| `src/server/game/Entities/Player/Player.h` | Modified to add BonusLayer member |
| `src/server/game/Entities/Player/Player.cpp` | Modified to add methods |
| `src/server/game/Entities/Player/PlayerStorage.cpp` | Modified to initialize on load |
| `data/sql/updates/pending_db_characters/rev_20260920_00_bonus_layer_ledger.sql` | Ledger table migration |
| `data/sql/updates/pending_db_characters/rev_20260920_01_bonus_layer_cache.sql` | Cache table migration |

---

## Next Steps (Move-2 Requirements)

1. **Implement 7 Recompute Triggers**
   - [ ] Hook equip/unequip events → RECOMPUTE_GEAR_CHANGE
   - [ ] Hook sacrament completion → RECOMPUTE_SACRAMENT
   - [ ] Hook contract completion → RECOMPUTE_REAPING
   - [ ] Hook passive/callus change → RECOMPUTE_SCAR
   - [ ] Hook ascension event → RECOMPUTE_ASCENSION
   - [ ] Hook rank progression → RECOMPUTE_RANK_UP

2. **Design Input: Faucet Ceilings**
   - Game design specifies max bonuses per source/field
   - Populate `MOVE_2_OVERFLOW_AUDIT.md` table
   - Calculate 32-bit safety margin (must be ≥ 14,000x)

3. **Performance & Scale Testing**
   - [ ] Recompute with 100+ ledger entries: must complete < 100ms
   - [ ] Cache hit (GetBonusValue): must be O(1), 10k calls < 10ms
   - [ ] Memory per character: ≤ 32KB

4. **Accept & Lock Phase 3**
   - All tests pass
   - Move-2 overflow audit passes (14,000x margin)
   - Design approves faucet ceilings

---

## Critical Design Decisions

1. **Immutable Ledger**: Each write/revoke appends a new entry. No deletion. Full audit trail preserved.

2. **Discrete Recompute Triggers**: Only 7 events trigger recompute. No continuous timer. Predictable, auditable.

3. **Soft Plateau vs Hard-Cap**: 
   - Soft (uncapped): Primaries, AP, SP, armor, crit, damage mods - "more strength" = more HP pools
   - Hard (capped at 100%): Avoidance %, DR, speed - Overflow routes intelligently to alternate defense

4. **Surplus Routing**: When a hard-cap field overflows, excess redirects to related field. E.g., dodge overflow → block, block overflow → armor.

5. **No Real-Time Sync**: Cache only recomputed on 7 triggers. Between triggers, cached values are stale. For client UI, send cache snapshot on login + after recompute.

---

## Debugging

### Check Ledger Data
```sql
SELECT field_name, source, SUM(value) as total FROM bonus_layer_ledger 
WHERE character_id = {guid} 
GROUP BY field_name, source;
```

### Check Cache Data
```sql
SELECT * FROM bonus_layer_cache WHERE character_id = {guid};
```

### Log Recompute
Enable debug logging:
```
entities.player.bonus_layer=debug
```

### Test Single Field
```cpp
player->GetBonusLayer()->Write("armor", "gear", 500, "test");
player->RecomputeBonusLayer(RECOMPUTE_GEAR_CHANGE);
int32 value = player->GetBonusValue("armor");
LOG_INFO("bonus_layer_test", "Armor bonus: {}", value);
```

---

## Acceptance Criteria

- ✓ Database schema created (2 tables, indexes, FKs)
- ✓ BonusLayer C++ class implemented
- ✓ Player entity integrated with BonusLayer
- ✓ Login trigger implemented (RECOMPUTE_LOGIN)
- ⏳ All 7 triggers hooked (6 remaining)
- ⏳ Integration tests passing
- ⏳ Performance audit passing (< 100ms recompute, O(1) cache hit)
- ⏳ Move-2 design input (faucet ceilings, 32-bit safety proof)

---

## Appendix: Source Tag Permissions

Each source can only write to certain fields:

| Source | Allowed Fields | Blocked Fields |
|--------|---|---|
| gear | armor, dodge, parry, block, ratings | ap_multiplier, sp_multiplier, bloodline mods |
| sacrament | ap_multiplier, sp_multiplier, last_stand_pct | primaries, ratings |
| bloodline | all fields | (none) |
| legendary_rank | ap_multiplier, healing_bonus, loot_pct | primaries |
| class_set | armor, dodge, parry, block | bloodline fields |
| reaping | ap_multiplier, sp_multiplier, gold_pct | primaries, bloodline fields |
| scar | damage mods, crit, ap_multiplier | primaries, bloodline fields |
| origin_path | ap_multiplier, sp_multiplier, gold_pct | primaries |

The C++ `isSourceAllowed()` function enforces this matrix at write time. Unauthorized writes are logged and rejected.

---

## Building & Testing

```bash
# No build required for SQL migrations - applied on schema update

# C++ changes will compile with next build
cmake ..
make -j $(nproc)

# Run TypeScript tests (from server/systems/bonus_layer/)
npm test
```

To run integration tests once triggers are hooked:
```bash
# In-game test: equip/unequip item, check logs
# Database test: query bonus_layer_ledger and bonus_layer_cache tables
# Performance test: /admin bonus_layer_perf_test
```

