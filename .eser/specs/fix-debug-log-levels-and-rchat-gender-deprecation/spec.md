# Spec: fix-debug-log-levels-and-rchat-gender-deprecation

## Status: completed

## Concerns: beautiful-product, long-lived, move-fast

## Discovery Answers

### status_quo

Three issues: (1) Sys_LoadLibrary failure messages print with Com_Printf (always visible) instead of Com_DPrintf (developer-only). (2) Bot chat file rchat.c uses gender tokens (male/female/neuter) that the parser no longer recognizes because gender support was removed from g_client.c. The animation.cfg files still have sex m/f lines. (3) Sound codec fallback warnings already use Com_DPrintf — confirmed developer-only, no fix needed. User cannot change model during gameplay without reconnecting (already resolved by prior icon PNG fix).

### ambition

7-star: Sys_LoadLibrary Com_DPrintf + VM_LoadDll Com_DPrintf + restore bot gender from animation.cfg sex line. 1-star: just Sys_LoadLibrary. 5-star: log fixes + gender restore. 10-star: full bot chat modernization.

### reversibility

Two-way door. All changes are additive or log-level downgrades. Sys_LoadLibrary/VM_LoadDll messages just change from Com_Printf to Com_DPrintf. Bot gender restore re-adds reading animation.cfg sex line — no existing code removed. Correct in 2 years.

### user_impact

Positive only. Cleaner console output for non-developers. Bots use gender-appropriate chat again. No breaking changes.

### verification

(1) Run game without developer 1: verify no Sys_LoadLibrary or VM_LoadDll messages in console. Run with developer 1: verify they appear. (2) Start game with bots: verify rchat.c parse error (expected a string found male) is gone. No documentation changes needed — these are internal fixes.

### scope_boundary

Non-goals: No rewriting rchat.c data file. No gender-neutral chat modernization. No missing asset fixes (brass, muzzle, BFG — separate spec). No menu/art/font*.tga. No player gender selection UI. No technical debt introduced.

## v1 vs v2 Scope (move-fast)

_To be addressed during execution._

## Out of Scope

- Non-goals: No rewriting rchat.c data file
- No gender-neutral chat modernization
- No missing asset fixes (brass, muzzle, BFG — separate spec)
- No menu/art/font*.tga
- No player gender selection UI
- No technical debt introduced.

## Tasks

- [x] task-1: Change Sys_LoadLibrary failure message from Com_Printf to Com_DPrintf in code/unix/unix_shared.c:480 and code/win32/win_main.c (equivalent line). AC: Sys_LoadLibrary messages only appear when developer 1 is set.
- [x] task-2: Change VM_LoadDll failure and success messages from Com_Printf to Com_DPrintf in code/qcommon/vm.c:1759,1763,1772,1774. AC: VM_LoadDll messages only appear when developer 1 is set.
- [x] task-3: Restore bot gender extraction from animation.cfg. In g_client.c ClientUserinfoChanged(), re-add parsing the model animation.cfg file to extract the sex line (m/f/n). Pass gender to botlib via the bot characteristic system so rchat.c gender matching works. AC: rchat.c parse error (expected a string found male) is gone. Bots have correct gender context.

## Verification

- (1) Run game without developer 1: verify no Sys_LoadLibrary or VM_LoadDll messages in console
- Run with developer 1: verify they appear. (2) Start game with bots: verify rchat.c parse error (expected a string found male) is gone
- No documentation changes needed — these are internal fixes.
