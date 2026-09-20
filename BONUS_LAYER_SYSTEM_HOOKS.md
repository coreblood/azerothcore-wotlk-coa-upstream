# Bonus Layer System Integration: Trigger Hook Templates

This document provides code templates and implementation guides for integrating BonusLayer recompute triggers into Ascension custom systems that have not yet been fully built.

---

## Overview

The Ascension game systems (Sacrament, Reaping, Scar, Ascension, Legendary Rank) are being developed separately from the bonus layer. This document describes how to hook the bonus layer whenever these systems complete an event.

**Key Principle**: After any event that modifies a player's stats (equipping gear, completing a contract, etc.), call:
```cpp
player->RecomputeBonusLayer(RECOMPUTE_<REASON>);
```

---

## 1. Sacrament System (RECOMPUTE_SACRAMENT)

### Trigger: Altar Offering Completion

**When**: Player makes an offering at an altar to gain sacrament bonuses (spell power multiplier, meta fields)

**Example Flow**:
```
Player interacts with Altar NPC
  → NPC asks for offering (mats/money)
  → Player confirms offering
  → Offering recorded in database
  → Bonuses applied to bonus layer
  → Cache recomputed
```

### Implementation Template

**Option A: If Sacrament Manager Class Exists**

```cpp
// In SacramentMgr::CompleteOffering() or similar
void SacramentMgr::CompleteOffering(Player* player, uint32 itemId, uint32 amount)
{
    // ... existing offering logic ...
    
    // Record offering in bonus layer
    if (player->GetBonusLayer())
    {
        player->GetBonusLayer()->Write(
            "sp_multiplier",
            "sacrament",
            amount,  // bonus amount
            "Altar offering item " + itemId
        );
    }

    // Recompute cache
    player->RecomputeBonusLayer(RECOMPUTE_SACRAMENT);
}
```

**Option B: If Using NPC Dialog/Event**

```cpp
// In npc_altar script or custom spell handler
bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 action)
{
    if (action == GOSSIP_ACTION_OFFER_SACRAMENT)
    {
        // ... validate offering, deduct cost, apply bonus ...
        
        // Hook bonus layer
        player->RecomputeBonusLayer(RECOMPUTE_SACRAMENT);
        
        player->CLOSE_GOSSIP_MENU();
    }
    return true;
}
```

**Option C: If Using Custom Spell Effect**

```cpp
// In custom spell script
void HandleSacramentEffect(SpellEffIndex effIndex)
{
    Unit* caster = GetCaster();
    if (Player* player = caster->ToPlayer())
    {
        // ... process offering ...
        
        // Hook bonus layer
        player->RecomputeBonusLayer(RECOMPUTE_SACRAMENT);
    }
}

// Register in spell scripts
RegisterSpellScript(spell_sacrament_offering);
```

### Database Schema

When building Sacrament system, ensure the bonus layer ledger captures the offering:

```sql
-- Example: Player makes 1000g altar offering
INSERT INTO bonus_layer_ledger 
  (character_id, field_name, source, value, context)
VALUES
  (12345, 'sp_multiplier', 'sacrament', 50, 'Altar offering 1000g');
```

### Testing

```bash
# 1. Load character, check initial cache
SELECT * FROM bonus_layer_cache WHERE character_id = <guid>;

# 2. Make sacrament offering in-game
# 3. Check cache was recomputed
SELECT * FROM bonus_layer_cache WHERE character_id = <guid>;

# 4. Verify ledger has new entry
SELECT * FROM bonus_layer_ledger WHERE character_id = <guid> 
  AND source = 'sacrament' ORDER BY created_at DESC LIMIT 1;
```

---

## 2. Reaping System (RECOMPUTE_REAPING)

### Trigger: Contract Completion & Reward Claim

**When**: Player completes a contract (kills X enemies, farms Y mats) and claims the reward

**Example Flow**:
```
Player accepts contract
  → Progresses contract
  → Contract complete
  → Player claims reward
  → Reward (AP multiplier, gold %) recorded
  → Cache recomputed
```

### Implementation Template

**In ReapingMgr or Contract Handler**:

```cpp
void ReapingMgr::ClaimContractReward(Player* player, uint32 contractId)
{
    Contract* contract = GetContract(contractId);
    if (!contract)
        return;

    // Validate completion
    if (!contract->IsComplete(player))
        return;

    // Award primary reward (gold, items)
    uint32 goldReward = contract->GetGoldReward();
    player->ModifyMoney(goldReward * MONEY);

    // Record bonus in bonus layer
    if (player->GetBonusLayer())
    {
        int32 apBonus = contract->GetAPBonus();
        if (apBonus > 0)
        {
            player->GetBonusLayer()->Write(
                "ap_multiplier",
                "reaping",
                apBonus,
                "Contract " + contract->GetName()
            );
        }
    }

    // Recompute cache
    player->RecomputeBonusLayer(RECOMPUTE_REAPING);

    // Mark claimed
    contract->MarkClaimed();
}
```

### Revoke Pattern (If Contract is Abandoned)

```cpp
void ReapingMgr::AbandonContract(Player* player, uint32 contractId)
{
    Contract* contract = GetContract(contractId);
    
    // Revoke the bonus (append negative entry)
    if (player->GetBonusLayer())
    {
        int32 apBonus = contract->GetAPBonus();
        if (apBonus > 0)
        {
            player->GetBonusLayer()->Revoke(
                "ap_multiplier",
                "reaping",
                "Contract " + contract->GetName() + " abandoned"
            );
        }
    }

    player->RecomputeBonusLayer(RECOMPUTE_REAPING);
}
```

---

## 3. Scar System (RECOMPUTE_SCAR)

### Trigger: Passive Slot or Callus Slot Change

**When**: Player equips/unequips a passive ability or callus passive

**Example Flow**:
```
Player opens Scar UI
  → Selects passive from inventory (5 passive slots)
  → Selects callus modifier (3 callus slots)
  → Changes committed
  → Passive bonuses recorded
  → Cache recomputed
```

### Implementation Template

**In ScarSystem or Passive Manager**:

```cpp
class ScarSystem
{
    // ... existing members ...
    
    bool EquipPassive(Player* player, uint32 passiveId, uint8 slotIndex)
    {
        if (slotIndex >= 5)  // 5 passive slots
            return false;

        Passive* passive = GetPassive(passiveId);
        if (!passive)
            return false;

        // Unequip existing passive in slot
        uint32 oldPassiveId = m_playerPassives[player->GetGUID()][slotIndex];
        if (oldPassiveId)
        {
            UnequipPassive(player, oldPassiveId, slotIndex);
        }

        // Equip new passive
        m_playerPassives[player->GetGUID()][slotIndex] = passiveId;

        // Record in bonus layer
        if (player->GetBonusLayer())
        {
            int32 critBonus = passive->GetCritBonus();
            if (critBonus > 0)
            {
                player->GetBonusLayer()->Write(
                    "crit_rating",
                    "scar",
                    critBonus,
                    "Passive " + passive->GetName() + " slot " + slotIndex
                );
            }
        }

        // Recompute cache
        player->RecomputeBonusLayer(RECOMPUTE_SCAR);
        
        // Update client UI
        SendScarUpdate(player);
        return true;
    }

    bool UnequipPassive(Player* player, uint32 passiveId, uint8 slotIndex)
    {
        Passive* passive = GetPassive(passiveId);
        if (!passive)
            return false;

        m_playerPassives[player->GetGUID()][slotIndex] = 0;

        if (player->GetBonusLayer())
        {
            player->GetBonusLayer()->Revoke(
                "crit_rating",
                "scar",
                "Passive " + passive->GetName() + " unequipped"
            );
        }

        player->RecomputeBonusLayer(RECOMPUTE_SCAR);
        SendScarUpdate(player);
        return true;
    }
};
```

---

## 4. Ascension System (RECOMPUTE_ASCENSION)

### Trigger: Character Ascension & Reset

**When**: Player resets character with bloodline inheritance (highest level reached)

**Example Flow**:
```
Player initiates Ascension
  → Character resets to level 1
  → Previous achievements stored in Bloodline
  → Old bonus layer ledger cleared
  → Inherited bonuses applied
  → Cache recomputed with inherited values
```

### Implementation Template

**In AscensionMgr or Character Reset Handler**:

```cpp
bool AscensionMgr::AscendCharacter(Player* player)
{
    if (!CanAscend(player))
        return false;

    uint64 accountId = player->GetSession()->GetAccountId();

    // Step 1: Save current bloodline state
    SaveBloodlineState(accountId, player->GetGUID());

    // Step 2: CRITICAL - Reset bonus layer
    // This clears the ledger for this character
    if (player->GetBonusLayer())
    {
        player->GetBonusLayer()->Reset();
    }

    // Step 3: Apply inherited bonuses from account Bloodline
    // Load inherited offerings, claims, and rank bonuses
    InheritBloodlineBonuses(player, accountId);

    // Step 4: Recompute cache with inherited values
    player->RecomputeBonusLayer(RECOMPUTE_ASCENSION);

    // Step 5: Reset character (level 1, exp 0, etc.)
    player->SetLevel(1);
    player->SetUInt64Value(PLAYER_XP, 0);
    // ... other reset logic ...

    // Step 6: Update client
    player->SendInitialPacketsAfterAddToMap();

    return true;
}

// Helper: Load inherited bonuses from Bloodline
void AscensionMgr::InheritBloodlineBonuses(Player* player, uint64 accountId)
{
    BloodlineData bloodline = GetAccountBloodline(accountId);
    
    if (player->GetBonusLayer())
    {
        // Inherit sacrament plateau
        for (const auto& offering : bloodline.sacraments)
        {
            player->GetBonusLayer()->Write(
                "sp_multiplier",
                "bloodline",
                offering.bonus,
                "Inherited from ascension level " + bloodline.maxLevel
            );
        }

        // Inherit reaping rewards
        for (const auto& reward : bloodline.reapingRewards)
        {
            player->GetBonusLayer()->Write(
                "ap_multiplier",
                "bloodline",
                reward.bonus,
                "Inherited from ascension level " + bloodline.maxLevel
            );
        }

        // Inherit legendary rank bonuses
        if (bloodline.legendaryRank > 0)
        {
            player->GetBonusLayer()->Write(
                "loot_pct",
                "bloodline",
                bloodline.legendaryRank * 5,  // 5% per rank
                "Inherited rank " + bloodline.legendaryRank
            );
        }
    }
}

// Critical: Revoke pattern when ascension is cancelled
bool AscensionMgr::CancelAscension(Player* player)
{
    // If ascending was started but not yet confirmed...
    if (player->GetBonusLayer())
    {
        player->GetBonusLayer()->Reset();  // Undo the reset
        // Reload original ledger from database
        player->RecomputeBonusLayer(RECOMPUTE_LOGIN);  // Refresh from DB
    }
    return true;
}
```

### Database Schema for Bloodline

```sql
-- Account-wide bloodline storage
CREATE TABLE IF NOT EXISTS `bloodline` (
  `account_id` int unsigned NOT NULL PRIMARY KEY,
  `max_level_reached` int NOT NULL DEFAULT 1,
  `max_ascensions` int NOT NULL DEFAULT 0,
  `sacrament_data` longblob,  -- JSON of inherited sacraments
  `reaping_data` longblob,    -- JSON of inherited rewards
  `legendary_rank` int NOT NULL DEFAULT 0,
  
  KEY `idx_account` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### Critical Points

1. **Always call Reset()** before inheritance - clears the old ledger completely
2. **Load from Bloodline** after reset - inherits account-wide bonuses
3. **Recompute after inheritance** - ensures cache reflects inherited values
4. **Test rollback** - ensure cancelled ascension restores state

---

## 5. Legendary Rank System (RECOMPUTE_RANK_UP)

### Trigger: Legendary Rank Increases

**When**: Player accumulates enough XP to rank up in legendary progression

**Example Flow**:
```
Player gains experience
  → Rank-up threshold reached
  → Legendary rank increases
  → Rank reward (loot %, healing bonus) applied
  → Cache recomputed
```

### Implementation Template

**In LegendaryRankSystem**:

```cpp
class LegendaryRankSystem
{
    bool GainRankXP(Player* player, uint32 xpAmount)
    {
        uint32 currentRank = GetPlayerRank(player->GetGUID());
        uint32 currentXP = GetPlayerRankXP(player->GetGUID());
        uint32 nextLevelXP = GetXPForRank(currentRank + 1);

        currentXP += xpAmount;

        // Check for rank up
        if (currentXP >= nextLevelXP)
        {
            return RankUp(player, currentRank + 1);
        }

        return false;
    }

    bool RankUp(Player* player, uint32 newRank)
    {
        uint32 oldRank = newRank - 1;
        uint32 rankDiff = newRank - oldRank;

        // Award rank-up bonus
        int32 lootBonus = rankDiff * 5;  // 5% loot per rank

        // Record in bonus layer
        if (player->GetBonusLayer())
        {
            player->GetBonusLayer()->Write(
                "loot_pct",
                "legendary_rank",
                lootBonus,
                "Rank " + newRank + " achieved"
            );
        }

        // Store rank in database
        SetPlayerRank(player->GetGUID(), newRank);

        // Recompute cache
        player->RecomputeBonusLayer(RECOMPUTE_RANK_UP);

        // Notify player
        ChatHandler(player->GetSession()).SendSysMessage("Legendary Rank: {}", newRank);

        return true;
    }

    bool RankDown(Player* player, uint32 newRank)
    {
        // If rank is lost (rare), revoke the bonus
        if (player->GetBonusLayer())
        {
            player->GetBonusLayer()->Revoke(
                "loot_pct",
                "legendary_rank",
                "Rank lost"
            );
        }

        SetPlayerRank(player->GetGUID(), newRank);
        player->RecomputeBonusLayer(RECOMPUTE_RANK_UP);
        return true;
    }
};

// Hook into XP gain
void OnPlayerGainXP(Player* player, uint32 xp)
{
    // ... existing XP logic ...
    
    // Also award rank XP
    bool rankedUp = sLegendaryRankMgr->GainRankXP(player, xp / 10);  // 10:1 ratio
    // If rankedUp, RecomputeBonusLayer was already called
}
```

---

## Summary: Hook Checklist

For each system, ensure:

- ✅ System completes an event (offering, contract, passive, ascension, rank-up)
- ✅ Call `player->RecomputeBonusLayer(RECOMPUTE_<REASON>)` after event
- ✅ For revoke patterns, call `->Revoke()` then recompute
- ✅ For ascension, call `->Reset()` then `->Recompute(RECOMPUTE_ASCENSION)`
- ✅ Add context string to ledger entries for audit trail
- ✅ Test: verify cache is updated after event
- ✅ Test: verify ledger has new entries in database

---

## Testing Template

```cpp
// In-game test command
void OnCommand_TestBonusLayer(Player* player, const char* args)
{
    if (!player->GetBonusLayer())
        return;

    // Simulate sacrament offering
    player->GetBonusLayer()->Write("sp_multiplier", "sacrament", 50, "Test offering");
    player->RecomputeBonusLayer(RECOMPUTE_SACRAMENT);

    int32 value = player->GetBonusValue("sp_multiplier");
    ChatHandler(player->GetSession()).SendSysMessage("SP Multiplier bonus: {}", value);

    // Check database
    // SELECT * FROM bonus_layer_ledger WHERE character_id = <guid>;
    // SELECT * FROM bonus_layer_cache WHERE character_id = <guid>;
}
```

---

## References

- `BONUS_LAYER_INTEGRATION.md` - Main integration guide
- `BONUS_LAYER_TRIGGERS_STATUS.md` - Status tracking
- `/server/systems/bonus_layer/` - TypeScript system definitions (reference for fields, hard-caps, surplus routing)

