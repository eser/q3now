// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save_localcbs.h — the TIER-2 (file-static) savegame callback name lists.
//
// Each static-holding .c file has a file-local X-macro naming the static
// callbacks it defines; those lists are collected HERE, one macro per file, so
// they have a SINGLE SOURCE. Two consumers expand them:
//   1. the owning .c file — expands ONLY its own macro, via
//      SG_DEFINE_LOCAL_REGISTRY, to build its static sub-table + SG_Register_<file>
//      hook (the statics are visible there, so &Fn resolves);
//   2. the registry unit test — expands ALL of them against stub symbols to drive
//      the round-trip/bijection gate for the file-static tier without linking the
//      game.
//
// Feature-gated statics (Q3_Portal* under FEAT_PW_PORTAL, Obelisk* under
// FEAT_OVERLOAD, Q3_Use_target_earthquake under FEAT_EARTHQUAKE_SYSTEM,
// G_MissileExplodeDie under FEAT_DESTROYABLE_MISSILES) are guarded identically to
// their definitions, so a list holds exactly what was compiled. The test defines
// those flags to a known state and checks the matching expectation.
//
// Ordering/content here is the authority; a build-time completeness cross-check
// (ctest) asserts these lists equal the callbacks actually assigned in the .c
// files, so a new static callback that is added but not listed fails CI.

#ifndef G_SAVE_LOCALCBS_H
#define G_SAVE_LOCALCBS_H

// ── code/game/entities/g_trigger_q1.c (18, all unconditional) ────────────────
#define SG_LOCAL_CB_g_trigger_q1( X ) \
	X( q1_hurt_touch )                 \
	X( q1_hurt_use )                   \
	X( q1_trigger_changelevel_touch )  \
	X( q1_trigger_counter_timed_use )  \
	X( q1_trigger_counter_use )        \
	X( q1_trigger_multiple_pain )      \
	X( q1_trigger_multiple_rearm )     \
	X( q1_trigger_multiple_touch )     \
	X( q1_trigger_multiple_use )       \
	X( q1_trigger_once_pain )          \
	X( q1_trigger_once_touch )         \
	X( q1_trigger_onlyregistered_touch )\
	X( q1_trigger_push_touch )         \
	X( q1_trigger_push_use )           \
	X( q1_trigger_relay_use )          \
	X( q1_trigger_secret_touch )       \
	X( q1_trigger_setskill_touch )     \
	X( q1_trigger_teleport_touch )

// ── code/game/entities/g_mover_q1.c (18, all unconditional) ──────────────────
#define SG_LOCAL_CB_g_mover_q1( X )  \
	X( Q1_Blocked_Door )              \
	X( Q1_Button_Die )                \
	X( Q1_Button_Reached )            \
	X( Q1_Button_Touch )              \
	X( Q1_DoorSecret_Die )            \
	X( Q1_DoorSecret_Pain )           \
	X( Q1_DoorSecret_Reached )        \
	X( Q1_DoorSecret_StartPhase2 )    \
	X( Q1_DoorSecret_StartPhase5 )    \
	X( Q1_Door_Touch )                \
	X( Q1_Door_Use )                  \
	X( Q1_Plat_TargetedUse )          \
	X( Q1_SecretDoor_Blocked )        \
	X( Q1_SecretDoor_StartClose )     \
	X( Q1_SecretDoor_Touch )          \
	X( Q1_SecretDoor_Use )            \
	X( Q1_Train_Blocked )             \
	X( Reached_Q1Plat )

// ── code/game/entities/g_misc_q3.c (6; Q3_Portal* under FEAT_PW_PORTAL) ───────
#if FEAT_PW_PORTAL
#define SG_LOCAL_CB_g_misc_q3_PORTAL( X ) \
	X( Q3_PortalDie )                      \
	X( Q3_PortalTouch )                    \
	X( Q3_PortalEnable )
#else
#define SG_LOCAL_CB_g_misc_q3_PORTAL( X )
#endif
#define SG_LOCAL_CB_g_misc_q3( X ) \
	X( Q3_InitShooter_Finish )      \
	X( light_toggle_use )           \
	X( misc_lightstyle_use )        \
	SG_LOCAL_CB_g_misc_q3_PORTAL( X )

// ── code/game/entities/g_misc_q1.c (6, all unconditional) ────────────────────
#define SG_LOCAL_CB_g_misc_q1( X ) \
	X( Q1_CountSecrets )            \
	X( q1_key_touch )               \
	X( q1_misc_explobox_die )       \
	X( q1_misc_fireball_think )     \
	X( q1_trap_shooter_think )      \
	X( q1_trap_spikeshooter_use )

// ── code/game/g_team.c (5 Obelisk* under FEAT_OVERLOAD) ──────────────────────
#if FEAT_OVERLOAD
#define SG_LOCAL_CB_g_team( X ) \
	X( ObeliskDie )              \
	X( ObeliskPain )            \
	X( ObeliskRegen )           \
	X( ObeliskRespawn )         \
	X( ObeliskTouch )
#else
#define SG_LOCAL_CB_g_team( X )
#endif

// ── code/game/entities/g_target_q3.c (2; earthquake under FEAT_EARTHQUAKE_SYSTEM) ─
#if FEAT_EARTHQUAKE_SYSTEM
#define SG_LOCAL_CB_g_target_q3_EQ( X ) \
	X( Q3_Use_target_earthquake )
#else
#define SG_LOCAL_CB_g_target_q3_EQ( X )
#endif
#define SG_LOCAL_CB_g_target_q3( X ) \
	X( Q3_target_location_linkup )    \
	SG_LOCAL_CB_g_target_q3_EQ( X )

// ── code/game/entities/g_mover_q3.c (1, unconditional) ───────────────────────
#define SG_LOCAL_CB_g_mover_q3( X ) \
	X( func_breakable_die )

// ── code/game/g_weapon.c (1, unconditional) ──────────────────────────────────
#define SG_LOCAL_CB_g_weapon( X ) \
	X( KamikazeDamage )

// ── code/game/g_missile.c (1; under FEAT_DESTROYABLE_MISSILES) ───────────────
#if FEAT_DESTROYABLE_MISSILES
#define SG_LOCAL_CB_g_missile( X ) \
	X( G_MissileExplodeDie )
#else
#define SG_LOCAL_CB_g_missile( X )
#endif

#endif // G_SAVE_LOCALCBS_H
