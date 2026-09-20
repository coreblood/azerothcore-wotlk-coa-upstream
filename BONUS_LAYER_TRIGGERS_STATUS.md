# Bonus Layer: Recompute Triggers Status

**Overall Progress**: 2 of 7 triggers implemented (29%)

---

## Trigger Implementation Status

### ✅ COMPLETE (2/7)

#### 1. ✅ RECOMPUTE_LOGIN
- **Status**: DONE
- **File**: `PlayerStorage.cpp::LoadFromDB()` (line 5508)
- **Description**: Rebuild cache from ledger on character load
- **Implementation**: 
  ```cpp
  RecomputeBonusLayer(RECOMPUTE_LOGIN);
  ```
- **Commit**: `7ec9af6` (Phase 1 integration)

#### 2. ✅ RECOMPUTE_GEAR_CHANGE
- **Status**: DONE
- **Files**: 
  - `PlayerStorage.cpp::EquipItem()` (lines 2935, 2945)
  - `PlayerStorage.cpp::RemoveItem()` (line 3075)
- **Description**: Recalculate when equipment is equipped/unequipped
- **Implementation**:
  ```cpp
  RecomputeBonusLayer(RECOMPUTE_GEAR_CHANGE);
  ```
- **Commit**: `edce408`

---

### ⏳ PENDING (5/7)

#### 3. ⏳ RECOMPUTE_SACRAMENT
- **Status**: NOT STARTED
- **Trigger**: Player makes altar offering (sacrament completion)
- **Location**: Need to find sacrament system in scripts or custom code
- **Search**: `grep -r "sacrament" src/server/scripts/` or find sacrament handler
- **Action Required**: Hook sacrament offering completion event
- **Est. Complexity**: Medium
- **Notes**: May be in CoA custom scripts module

#### 4. ⏳ RECOMPUTE_REAPING
- **Status**: NOT STARTED
- **Trigger**: Contract completion and reward claim
- **Location**: Custom reaping system scripts
- **Search**: `grep -r "reaping\|contract" src/server/scripts/` or `modules/`
- **Action Required**: Hook contract completion and reward claim events
- **Est. Complexity**: Medium
- **Notes**: May be in CoA custom scripts module

#### 5. ⏳ RECOMPUTE_SCAR
- **Status**: NOT STARTED
- **Trigger**: Passive slot or callus slot equipment changes
- **Location**: Custom scar system scripts
- **Search**: `grep -r "scar\|passive\|callus" src/server/scripts/` or `modules/`
- **Action Required**: Hook passive/callus slot change events
- **Est. Complexity**: Medium
- **Notes**: May be in CoA custom scripts module

#### 6. ⏳ RECOMPUTE_ASCENSION
- **Status**: NOT STARTED
- **Trigger**: Character ascends and resets with bloodline inheritance
- **Location**: Custom ascension logic
- **Search**: `grep -r "ascend\|bloodline" src/server/scripts/` or find ascension handler
- **Action Required**: 
  - Hook ascension event
  - Call `GetBonusLayer()->Reset()` to clear ledger
  - Inherit from account-wide bloodline bonus layer
- **Est. Complexity**: High (requires reset + inheritance logic)
- **Notes**: Critical for Ascension system integration

#### 7. ⏳ RECOMPUTE_RANK_UP
- **Status**: NOT STARTED
- **Trigger**: Legendary rank increases
- **Location**: Legendary rank/progression system
- **Search**: `grep -r "legendary\|rank" src/server/scripts/` or `modules/`
- **Action Required**: Hook rank progression completion event
- **Est. Complexity**: Medium
- **Notes**: May be in CoA custom scripts module

---

## Implementation Pattern

For each remaining trigger, follow this pattern:

```cpp
// 1. Find the event handler in the codebase
// 2. Add this call after the event completes (before any returns)
RecomputeBonusLayer(RECOMPUTE_<TRIGGER>);

// 3. For ascension, also reset the ledger:
GetBonusLayer()->Reset();
RecomputeBonusLayer(RECOMPUTE_ASCENSION);
// Then inherit from bloodline...
```

---

## Module Structure (CoA Systems)

Based on AGENTS.md, CoA custom systems are likely in:
- `modules/mod-ascension-compat/` - Ascension-specific features
- `src/server/scripts/Custom/` - Custom gameplay scripts
- `src/server/scripts/World/` - World events and systems

Common patterns:
- Each system has a manager class (e.g., `SacramentMgr`)
- Events fire via `sScriptMgr->OnEventName(player, args)`
- Custom event hooks may be in `src/server/scripts/Custom/Events/`

---

## Discovery Process for Each Trigger

### Step 1: Grep for keywords
```bash
grep -r "sacrament" src/server/scripts/ modules/ src/server/game/
grep -r "reaping\|contract" src/server/scripts/ modules/ src/server/game/
grep -r "scar\|passive.*slot" src/server/scripts/ modules/ src/server/game/
grep -r "ascend\|bloodline" src/server/scripts/ modules/ src/server/game/
grep -r "legendary.*rank" src/server/scripts/ modules/ src/server/game/
```

### Step 2: Find completion/event methods
```bash
grep -r "OnComplete\|OnFinish\|OnClaim\|OnAward" src/server/scripts/
```

### Step 3: Add recompute call
Once location found, add `RecomputeBonusLayer(RECOMPUTE_<TRIGGER>);` after event completes.

### Step 4: Test
- Load character with equipped gear → verify gear bonuses applied
- Trigger system event → verify cache recomputed
- Check logs for "BonusLayer::Recompute" debug output

---

## Testing Checklist

After implementing all 7 triggers:

- [ ] Load character → login trigger fires, cache populated
- [ ] Equip/unequip item → gear trigger fires, cache updated
- [ ] Make altar offering → sacrament trigger fires, cache updated
- [ ] Complete contract → reaping trigger fires, cache updated
- [ ] Change passive slot → scar trigger fires, cache updated
- [ ] Ascend character → ascension trigger fires, ledger reset, inheritance applied
- [ ] Rank up → rank trigger fires, cache updated
- [ ] Multiple sources on same field → sum correct, hard-cap routing works
- [ ] Negative bonus (revoke) → soft cap works, effective value correct
- [ ] Performance: 100 ledger entries → recompute < 100ms

---

## Priority Order

1. **Gear (DONE)** ✅ - Most frequent, affects all characters, must work
2. **Ascension** - Core feature, required for bloodline inheritance
3. **Sacrament** - Altar system, affects spell power scaling
4. **Reaping** - Contract rewards, affects attack power scaling
5. **Scar** - Passive system, affects damage modifiers
6. **Rank-up** - Legendary progression, affects multipliers

---

## Git Commit Template

For each trigger implementation:

```
feat: hook <trigger-name> trigger (RECOMPUTE_<TRIGGER>)

- Add RecomputeBonusLayer(RECOMPUTE_<TRIGGER>) in [file:method]
- Ensures bonus layer cache updates when [trigger-description]

This is trigger #X of 7 recompute events. Remaining:
- [List remaining triggers]

Co-Authored-By: Claude Haiku 4.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_01Dnukn9XLGfWekgfaT7eKWo
```

---

## Summary

| Trigger | Status | File | Method | Commit |
|---------|--------|------|--------|--------|
| Login | ✅ | PlayerStorage.cpp | LoadFromDB | 7ec9af6 |
| Gear | ✅ | PlayerStorage.cpp | EquipItem, RemoveItem | edce408 |
| Sacrament | ⏳ | TBD | TBD | - |
| Reaping | ⏳ | TBD | TBD | - |
| Scar | ⏳ | TBD | TBD | - |
| Ascension | ⏳ | TBD | TBD | - |
| Rank-up | ⏳ | TBD | TBD | - |

**Next Step**: Search for Sacrament system implementation and hook it.

