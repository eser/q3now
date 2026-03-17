# Three-Way Header Merge Plan: ioquake3 -> Quake3e Engine Swap

**Base:** ioquake3 stock (`upstream/main`)
**Ours:** q3now (current HEAD on `master`)
**Theirs:** Quake3e (`/Users/eser/projects/playground/Quake3e/`)

---

## 1. `code/game/bg_public.h` — NEEDS MANUAL MERGE

### q3now changes (vs stock ioquake3):
- **GAME_VERSION**: `BASEGAME "-1"` -> `"q3now-1"`
- **New defines**: `DEFAULT_MOVESPEED`, `JUMP_VELOCITY`, `WALLJUMP_BOOST`, `MAX_WALLJUMPS`, `QUAD_FACTOR`, `MAX_HEALTH`, `MAX_ARMOR`, `GIB_VELOCITY`, `GIB_JUMP`
- **Removed**: `ARMOR_PROTECTION` (0.66)
- **Shotgun tuning**: spread 700->600, count 11->16
- **New defines**: `TA_OBELISK_HEALTH/REGEN_PERIOD/REGEN_AMOUNT`, `TA_CUBE_TIMEOUT`
- **New configstring**: `CS_PRO_MODE` (28)
- **gametype_t enum**: Removed `GT_SINGLE_PLAYER`, added `GT_DUMMY`, `GT_KINGOFTHEHILL`, `GT_LASTMANSTANDING` (BREAKS ENUM NUMBERING — GT_TEAM shifts from 3->5)
- **pmtype_t**: Removed `PM_SPINTERMISSION` (last entry gone)
- **statIndex_t**: Removed `STAT_PERSISTANT_POWERUP` and `STAT_MAX_HEALTH`; added `STAT_ARMORCLASS`, `STAT_JUMPTIME`, `STAT_RAILTIME`, `STAT_WALLJUMPS` (ENUM REORDERED)
- **Entity flags**: Replaced `EF_TICKING` with `EF_DROPPED_ITEM` (0x00000002); added `EF_GRAPPLE` (0x00100000), `EF_BACKPACK` (0x00000001)
- **powerup_t**: Removed `PW_SCOUT/GUARD/DOUBLER/AMMOREGEN`; added `PW_KING`
- **weapon_t**: Removed `WP_BFG`, `WP_GRAPPLING_HOOK`, `WP_NAILGUN/PROX_LAUNCHER/CHAINGUN` (BREAKS ENUM NUMBERING)
- **New enum**: `armor_t` (`ARM_NONE/JACKET/COMBAT/HEAVY/ARM_NUM_ARMOR`)
- **New event**: `EV_LIGHTNING_DISCHARGE`; removed `EV_PROXIMITY_MINE_STICK/TRIGGER`
- **meansOfDeath_t**: Removed `MOD_PLASMA_SPLASH`, `MOD_BFG/BFG_SPLASH`, `MOD_NAIL/CHAINGUN/PROXIMITY_MINE/JUICED`; added `MOD_LIGHTNING_DISCHARGE`
- **Removed**: `IT_PERSISTANT_POWERUP` item type
- **gitem_t**: `MAX_ITEM_MODELS` 4->4 (kept), added `MAX_ITEM_CLASSNAMES` 3 (new)

### Quake3e changes (vs stock ioquake3):
- **Include guard**: Added `#ifndef _BG_PUBLIC_H` / `#define _BG_PUBLIC_H` / `#endif`
- **Removed defines** (moved to engine): `PLAYER_WIDTH`, `DEFAULT_HEIGHT`, `CROUCH_HEIGHT`, `DEAD_HEIGHT`, `INVUL_RADIUS`
- **Whitespace only**: Trailing space fixes on `CS_PARTICLES`, `WEAPON_READY`, `STAT_ARMOR`, `TEAMTASK_OFFENSE`

### Conflict Analysis:
- **NO semantic conflicts.** Quake3e changes are purely structural (include guard, removed defines that moved to engine, whitespace). q3now changes are purely gameplay.
- **CAUTION**: Quake3e removes `PLAYER_WIDTH`, `DEFAULT_HEIGHT`, `CROUCH_HEIGHT`, `DEAD_HEIGHT`, `INVUL_RADIUS` from this header — these are now defined in the engine. q3now still has them (unchanged from stock values). After merge, these defines should be REMOVED since Quake3e engine provides them.

### Merge Strategy:
**Start with q3now copy, then apply Quake3e's small changes on top:**
1. Add the `#ifndef _BG_PUBLIC_H` include guard at top and `#endif` at bottom
2. Remove `PLAYER_WIDTH`, `DEFAULT_HEIGHT`, `CROUCH_HEIGHT`, `DEAD_HEIGHT`, `INVUL_RADIUS` (Quake3e engine defines them)
3. Apply whitespace fixes (trivial)

---

## 2. `code/game/g_public.h` — QUAKE3E COPY SAFE (then verify at runtime)

### q3now changes (vs stock ioquake3):
- **NONE.** q3now's g_public.h is identical to stock ioquake3.

### Quake3e changes (vs stock ioquake3):
- **New SVF flag**: `SVF_SELF_PORTAL2` (0x00020000)
- **entityShared_t struct LAYOUT CHANGE**: `entityState_t unused` renamed to `entityState_t s` (comment changed from "accidentally" to "communicated by server to clients")
  - **CRITICAL**: This changes the *semantics* of the first field. The field `unused` was a compatibility pad in ioquake3; Quake3e actively uses it as `s`. The struct *size* is unchanged but code referencing `ent->r.unused` vs `ent->r.s` will differ.
- **ownerNum comment**: Changed from `ent->r.ownerNum` references to `ent->s.ownerNum`
- **Comment fixes**: "simultanious" -> "simultaneous", `const char *fmt, ...` -> `const char *text`
- **Trailing whitespace**: Multiple fixes
- **gameImport_t enum**:
  - Existing syscall numbers PRESERVED (no renumbering)
  - After `G_FS_SEEK`, added new math syscalls: `G_MATRIXMULTIPLY`, `G_ANGLEVECTORS`, `G_PERPENDICULARVECTOR`, `G_FLOOR`, `G_CEIL`, `G_TESTPRINTINT`, `G_TESTPRINTFLOAT` (before `BOTLIB_SETUP = 200`, so no conflict with botlib range)
  - After `BOTLIB_PC_SOURCE_FILE_AND_LINE`, added engine extensions: `G_CVAR_SETDESCRIPTION`, `G_TRAP_GETVALUE = COM_TRAP_GETVALUE` (=700)
- **gameExport_t enum**:
  - `GAME_SHUTDOWN` comment changed: `(void)` -> `( int restart )`
  - `BOTAI_START_FRAME` now has trailing comma
  - Added `GAME_EXPORT_LAST` sentinel value
- **NOTE**: `COM_TRAP_GETVALUE` is defined as 700 in `code/qcommon/qcommon.h` (Quake3e-specific header)

### Conflict Analysis:
- No conflicts since q3now made zero changes.

### Merge Strategy:
**Copy Quake3e's version directly.** No q3now changes to preserve. However, ensure `COM_TRAP_GETVALUE` (from `qcommon.h`) is available — this is a Quake3e engine header, should come with the engine swap.

---

## 3. `code/cgame/cg_public.h` — QUAKE3E COPY SAFE

### q3now changes (vs stock ioquake3):
- **NONE.** q3now's cg_public.h is identical to stock ioquake3.

### Quake3e changes (vs stock ioquake3):
- **cgameImport_t enum**:
  - Removed `CG_MEMSET=100` through `CG_SQRT` (100-106). Set `CG_FLOOR = 107` explicitly to preserve numbering of `CG_FLOOR` and everything after it.
  - Added engine extensions after `CG_ACOS`: `CG_R_ADDREFENTITYTOSCENE2`, `CG_R_FORCEFIXEDDLIGHTS`, `CG_R_ADDLINEARLIGHTTOSCENE`, `CG_IS_RECORDING_DEMO`, `CG_CVAR_SETDESCRIPTION`, `CG_TRAP_GETVALUE = COM_TRAP_GETVALUE`
- **cgameExport_t enum**:
  - `CG_EVENT_HANDLING` now has trailing comma
  - Added `CG_EXPORT_LAST` sentinel value

### Syscall number preservation:
- CG_FLOOR stays at 107, CG_CEIL at 108, etc. — existing numbers preserved.
- New engine extensions appended after CG_ACOS (which was ~112). Safe.

### Merge Strategy:
**Copy Quake3e's version directly.** No q3now changes to preserve.

---

## 4. `code/ui/ui_public.h` — QUAKE3E COPY SAFE

### q3now changes (vs stock ioquake3):
- **NONE.** q3now's ui_public.h is identical to stock ioquake3.

### Quake3e changes (vs stock ioquake3):
- **uiImport_t enum**:
  - Removed `UI_MEMSET=100` through `UI_SQRT` (100-106). Set `UI_FLOOR = 107` explicitly.
  - Added engine extensions: `UI_R_ADDREFENTITYTOSCENE2`, `UI_R_ADDLINEARLIGHTTOSCENE`, `UI_CVAR_SETDESCRIPTION`, `UI_TRAP_GETVALUE = COM_TRAP_GETVALUE`
- **uiExport_t enum**:
  - Comment fix: `UI_KeyEvent( int key )` -> `UI_KeyEvent( int key, int down )`
  - `UI_HASUNIQUECDKEY` now has trailing comma
  - Added `UI_EXPORT_LAST` sentinel value

### Syscall number preservation:
- UI_FLOOR stays at 107, UI_CEIL at 108. Existing numbers preserved.

### Merge Strategy:
**Copy Quake3e's version directly.** No q3now changes to preserve.

---

## 5. `code/botlib/be_ai_goal.c` — NEEDS MANUAL MERGE

### q3now changes (vs stock):
- **gametype_t local enum**: Removed `GT_SINGLE_PLAYER`, added `GT_DUMMY`, `GT_KINGOFTHEHILL`, `GT_LASTMANSTANDING`
- **New variable**: `int g_singlePlayer = 0;`
- **Logic change**: All 4 instances of `if (g_gametype == GT_SINGLE_PLAYER)` replaced with `if (g_singlePlayer)`, and `else if (g_gametype >= GT_TEAM)` changed to `if (g_gametype >= GT_TEAM)` (removed `else`)
- **Init**: Added `g_singlePlayer = LibVarValue("g_singlePlayer", "0");` in `BotSetupGoalAI()`

### Quake3e changes (vs stock):
- **Code quality**: Added `static` and `const` qualifiers throughout (variables, functions)
- **Function signatures**: `char *filename` -> `const char *filename`, etc.
- **Path**: `MAX_QPATH` -> `MAX_PATH`
- **Formatting**: Whitespace/brace style adjustments
- **NO gameplay logic changes** — purely code quality/safety

### Conflict Analysis:
- **One overlap**: q3now adds `int g_singlePlayer = 0;` right after `int g_gametype = 0;`. Quake3e changes `int g_gametype = 0;` to `static int g_gametype = 0;`. These touch adjacent lines but are non-conflicting: the merged result should be `static int g_gametype = 0;` + `static int g_singlePlayer = 0;`.

### Merge Strategy:
**Start with q3now copy, then apply Quake3e's `static`/`const` qualifiers on top.** Both changesets are compatible. Apply `static` to `g_singlePlayer` too for consistency.

---

## 6. `code/game/ai_dmq3.c` — Q3NOW COPY SAFE

### q3now changes (vs stock):
- Extensive gameplay changes: removed BFG/Missionpack weapon references, added `BotKingOfTheHillSeekGoals()`, `PW_KING` logic, renamed "Speed"->"Haste", removed `INVENTORY_BFG10K/GRAPPLINGHOOK/NAILGUN/PROXLAUNCHER/CHAINGUN` references, removed `STAT_PERSISTANT_POWERUP` references

### Quake3e:
- **File does not exist in Quake3e.** Quake3e does not ship game QVM sources — only engine + botlib.

### Merge Strategy:
**Keep q3now copy as-is.** No Quake3e counterpart exists.

---

## Summary Table

| File | q3now changed? | Quake3e changed? | Verdict |
|------|---------------|-----------------|---------|
| `bg_public.h` | YES (heavy) | YES (light) | **Manual merge**: q3now base + Q3e include guard + remove 5 defines |
| `g_public.h` | NO | YES (moderate) | **Quake3e copy safe** |
| `cg_public.h` | NO | YES (moderate) | **Quake3e copy safe** |
| `ui_public.h` | NO | YES (moderate) | **Quake3e copy safe** |
| `be_ai_goal.c` | YES (moderate) | YES (code quality) | **Manual merge**: q3now base + Q3e static/const qualifiers |
| `ai_dmq3.c` | YES (heavy) | N/A (not in Q3e) | **q3now copy safe** |

## Critical Watch Items

1. **`entityShared_t.unused` -> `.s` rename in g_public.h**: Any q3now game code referencing `ent->r.unused` must change to `ent->r.s`. Search the entire codebase for this.
2. **`GAME_SHUTDOWN` signature change**: Quake3e passes `int restart` — game shutdown handler needs to accept this parameter.
3. **`UI_KeyEvent` signature change**: Now takes `(int key, int down)` instead of `(int key)`.
4. **`COM_TRAP_GETVALUE`**: Needs `#include` path to Quake3e's `qcommon.h` or equivalent define.
5. **Removed math syscalls** (CG_MEMSET, CG_SIN, etc.): Quake3e's native QVM compiler handles these. If q3now code uses `trap_MemSet()` etc. via these syscall numbers, those trap functions need updating.
6. **`PLAYER_WIDTH`, `DEFAULT_HEIGHT`, `CROUCH_HEIGHT`, `DEAD_HEIGHT`, `INVUL_RADIUS`**: Removed from bg_public.h by Quake3e — now engine-side. Verify no game code redefines them.
