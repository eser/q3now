// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save_funcs.h — savegame callback registry (Phase-2): the scoped name<->ptr
// table that lets SG_FUNCTION-typed fields serialize a callback as its NAME and
// re-bind to the live pointer on load. This REPLACES RealRTCW's funcList[] +
// `extractfuncs` build-time codegen (F5 dropped the codegen); the C-native,
// no-codegen, WASM-portable equivalent is distributed registration:
//
//   TIER 1 — central list (SG_CALLBACK_LIST): the callbacks that are already
//     extern-referenceable (external linkage). One X( Fn ) line each; g_save.c
//     expands the list once for the extern prototypes and once for the
//     { "Fn", &Fn } table row. ~65 entries.
//
//   TIER 2 — file-local sub-lists: 58 of the assigned callbacks are file-`static`
//     (see the 9 defining .c files). A static symbol is only visible inside its
//     own translation unit, so it CANNOT sit in the central g_save.c table. Each
//     such file declares its own SG_LOCAL_CALLBACKS(X) macro, expands it there
//     into a file-local static { "Fn", &Fn } sub-table, and publishes that
//     sub-table to the resolver through a SG_Register_<file>() hook. Statics stay
//     static; the registry stays scoped; nothing is bulk-externed.
//
// Both tiers resolve as one logical registry through SG_FunctionToName /
// SG_NameToFunction. Phase-2 is byte-identical: the tables + resolvers + hooks
// are data + functions called by NOBODY. The Phase-4 serializer is the first
// caller. Registration is lazy (first resolver use walks an explicit init-list of
// the 9 hooks) — NOT a static/global constructor (init-order pitfalls in a C game
// lib; also unavailable in the WASM build). Nothing uses a resolver yet, so no
// registration runs yet either.
//
// Naming: SG_* to match the Phase-1 field-type enum (g_save.h); unrelated to the
// spawn parser's F_* dialect.

#ifndef G_SAVE_FUNCS_H
#define G_SAVE_FUNCS_H

#include <stddef.h>

// ── registry types (game-type-free: names + opaque pointers only) ────────────
// A single name<->pointer row. The pointer is stored opaque (void*) — the
// serializer stores/loads the NAME; the field's own C type re-casts on rebind.
typedef struct {
	const char *name;
	void       *ptr;
} sgCallback_t;

// A registered file-local sub-table: a pointer to the file's static
// sgCallback_t[] plus its element count. The resolver walks the central table
// then every registered sub-table.
typedef struct {
	const sgCallback_t *table;
	size_t              count;
} sgCallbackTable_t;

// ── TIER 1: the central list (extern-referenceable callbacks) ────────────────
// One X( Fn ) per callback with EXTERNAL linkage that some SG_FUNCTION field is
// assigned. Grouped by the field whose signature it carries (think/reached/
// blocked/touch/use/die — no extern-linkage `pain` callback exists today). The
// grouping documents each callback's arity so the extern prototypes below are
// correct; the registry itself is signature-agnostic (it stores void*).
//
// Q3_ReturnToPos1 is here (the canonical hardest case: a mover think reassigned
// at RUNTIME when a door reaches pos2 — not re-derivable from moverState, so it
// MUST round-trip through a name). Also the classname/item-re-derivable set
// (Touch_Item, Use_Item, RespawnItem, FinishSpawningItem, Behavior_MonsterDie,
// player_die, body_die) — included for completeness: a name that always resolves
// is strictly safer than a special-case rebuild path, and the registry is the
// single source of truth for "is this callback savable".
#define SG_CALLBACK_LIST_THINK(X)   \
	X( BodySink )                    \
	X( FinishSpawningItem )          \
	X( G_ExplodeMissile )            \
	X( G_FreeEntity )                \
	X( Kamikaze_DeathActivate )      \
	X( Offhand_Grapple_Free )        \
	X( Offhand_Grapple_Hook_Think )  \
	X( Q3_AimAtTarget )              \
	X( Q3_ReturnToPos1 )             \
	X( Q3_Think_BeginMoving )        \
	X( Q3_Think_MatchTeam )          \
	X( Q3_Think_SetupTrainTargets )  \
	X( Q3_Think_SpawnNewDoorTrigger )\
	X( Q3_Think_Target_Delay )       \
	X( Q3_func_timer_think )         \
	X( Q3_locateCamera )             \
	X( Q3_multi_wait )               \
	X( Q3_target_laser_start )       \
	X( Q3_target_laser_think )       \
	X( Q3_target_unlink_think )      \
	X( Q3_trigger_always_think )     \
	X( RespawnItem )                 \
	X( Team_DroppedFlagThink )

#define SG_CALLBACK_LIST_REACHED(X) \
	X( Q3_Reached_BinaryMover )      \
	X( Q3_Reached_Train )

#define SG_CALLBACK_LIST_BLOCKED(X) \
	X( Q3_Blocked_Door )

#define SG_CALLBACK_LIST_TOUCH(X)   \
	X( Q3_Touch_Button )             \
	X( Q3_Touch_DoorTrigger )        \
	X( Q3_Touch_Lock )               \
	X( Q3_Touch_Multi )              \
	X( Q3_Touch_Plat )               \
	X( Q3_Touch_PlatCenterTrigger )  \
	X( Q3_hurt_touch )               \
	X( Q3_trigger_push_touch )       \
	X( Q3_trigger_teleporter_touch ) \
	X( Touch_Item )

#define SG_CALLBACK_LIST_USE(X)     \
	X( Q3_Use_BinaryMover )          \
	X( Q3_Use_Multi )                \
	X( Q3_Use_Shooter )              \
	X( Q3_Use_Target_Delay )         \
	X( Q3_Use_Target_Give )          \
	X( Q3_Use_Target_Print )         \
	X( Q3_Use_Target_Score )         \
	X( Q3_Use_Target_Speaker )       \
	X( Q3_Use_target_modify )        \
	X( Q3_Use_target_music )         \
	X( Q3_Use_target_push )          \
	X( Q3_Use_target_remove_powerups )\
	X( Q3_Use_target_unlink )        \
	X( Q3_func_timer_use )           \
	X( Q3_hurt_use )                 \
	X( Q3_target_gravity_use )       \
	X( Q3_target_kill_use )          \
	X( Q3_target_laser_use )         \
	X( Q3_target_logic_use )         \
	X( Q3_target_playerspeed_use )   \
	X( Q3_target_playerstats_use )   \
	X( Q3_target_relay_use )         \
	X( Q3_target_teleporter_use )    \
	X( Q3_trigger_death_use )        \
	X( Q3_trigger_frag_use )         \
	X( Use_Item )

#define SG_CALLBACK_LIST_DIE(X)     \
	X( Behavior_MonsterDie )         \
	X( body_die )                    \
	X( player_die )

// The full central list = the per-arity groups concatenated. Expand this once for
// the extern prototypes (grouped, below) and once for the { "Fn", &Fn } table row
// (g_save.c). One logical X() line per callback, exactly as ratified.
#define SG_CALLBACK_LIST(X)   \
	SG_CALLBACK_LIST_THINK(X)   \
	SG_CALLBACK_LIST_REACHED(X) \
	SG_CALLBACK_LIST_BLOCKED(X) \
	SG_CALLBACK_LIST_TOUCH(X)   \
	SG_CALLBACK_LIST_USE(X)     \
	SG_CALLBACK_LIST_DIE(X)

// ── TIER 1 prototypes (registry-engine TU only) ──────────────────────────────
// The central table takes &Fn for each callback, so the TU that BUILDS the table
// (g_save_registry.c, and the standalone unit test) needs a declaration of all 65
// in scope. Most (53 of 65) have no prototype in any game header.
//
// They are declared void fn( void ), NOT with their true field signature: the
// registry is game-type-free (no gentity_t / trace_t in scope) and only ever
// stores the callback ADDRESS as void*, never calls through it. Taking &fn yields
// a void(*)(void) that converts to void* regardless of fn's real definition — the
// C symbol name is what links. The same declaration lets the standalone test bind
// &fn to its void-void stubs.
//
// Emitted ONLY when SG_REGISTRY_IMPL is defined (g_save_registry.c / the test), so
// the 9 game files that include this header alongside g_local.h do NOT get a
// void-void declaration that would conflict with a real gentity_t prototype in
// their TU. Those files never take &Fn for a central callback anyway.
#ifdef SG_REGISTRY_IMPL
#define SG_PROTO_ANY( fn ) void fn( void );
SG_CALLBACK_LIST( SG_PROTO_ANY )
#undef SG_PROTO_ANY
#endif

// ── resolvers (span TIER 1 + all registered TIER 2 sub-tables) ───────────────
// SG_FunctionToName: callback address -> registered name. NULL ptr -> "" (an
//   empty token, so a NULL field serializes as an unambiguous "no callback").
//   Unknown NON-NULL ptr -> "" as well (defensive: a save-time surprise).
// SG_NameToFunction: name -> live callback address. NULL/"" -> NULL. An UNKNOWN
//   name (a save referencing a callback this build lacks) -> NULL. On the load
//   path (Phase-6) an unknown name is a version/corruption signal and must HALT,
//   NOT be silently swallowed; Phase-2 only provides the NULL return + this note.
// Linear scan is fine at N~=123 — save/load is not a hot path.
const char *SG_FunctionToName( void *ptr );
void       *SG_NameToFunction( const char *name );

// ── file-local registration hooks (TIER 2) ──────────────────────────────────
// Each static-holding file implements exactly one of these; it hands its
// file-local sub-table to the resolver via the passed callback. g_save.c owns the
// init-list that invokes all of them once, lazily, on first resolver use.
typedef void (*sgRegisterFn_t)( const sgCallback_t *table, size_t count );

void SG_Register_g_trigger_q1( sgRegisterFn_t reg );
void SG_Register_g_mover_q1( sgRegisterFn_t reg );
void SG_Register_g_misc_q3( sgRegisterFn_t reg );
void SG_Register_g_misc_q1( sgRegisterFn_t reg );
void SG_Register_g_team( sgRegisterFn_t reg );
void SG_Register_g_target_q3( sgRegisterFn_t reg );
void SG_Register_g_mover_q3( sgRegisterFn_t reg );
void SG_Register_g_weapon( sgRegisterFn_t reg );
void SG_Register_g_missile( sgRegisterFn_t reg );

// The SG_LOCAL_CALLBACKS boilerplate a static-holding .c file expands to build its
// sub-table + its SG_Register_<file>() hook. LIST is that file's own
// SG_LOCAL_CALLBACKS(X) macro; FN is its SG_Register_<file> name. Kept here so the
// 9 files share one definition (the sub-table shape + the hook body are identical;
// only the name list differs). The sub-table is `static const` (file-scoped); the
// statics it names are visible because this expands INSIDE their TU.
//
// A trailing { NULL, NULL } sentinel keeps the array non-empty even when EVERY
// entry is compiled out by a feature guard (a zero-length array is illegal in
// C); the hook passes count = (elements - 1) so the sentinel is never scanned.
// The resolvers would ignore a NULL name/ptr anyway, so the sentinel is inert.
#define SG_DEFINE_LOCAL_REGISTRY( LIST, FN )                              \
	static const sgCallback_t FN##_table[] = {                             \
		LIST( SG_CALLBACK_ROW )                                            \
		{ NULL, NULL }                                                     \
	};                                                                     \
	void FN( sgRegisterFn_t reg ) {                                        \
		reg( FN##_table, ( sizeof( FN##_table ) / sizeof( FN##_table[0] ) ) - 1 ); \
	}

// The row expansion shared by the central table and every file-local sub-table:
// X( Fn ) -> { "Fn", (void *)&Fn },   (the name token stringized, the address cast
// opaque). Requires Fn to be visible where expanded (extern prototype for TIER 1,
// the static definition itself for TIER 2).
#define SG_CALLBACK_ROW( fn ) { #fn, (void *)&fn },

#endif // G_SAVE_FUNCS_H
