// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// cl_wired_ui_hud_register.c — Wired UI HUD: element registration + runtime
// infrastructure (PERMANENT unified-custom-draw HUD layer).
//
// This file is the HUD half of the unified custom-draw registry. It is
// NOT a transition shim — earlier comments framed it as temporary "until the
// element files relocate and self-register," but that relocation has since
// landed (the 46 cl_wired_hud_elem_*.c modules now live in
// code/client/wired/ui/elements/) and self-registration turned out to be
// STRUCTURALLY IMPOSSIBLE here, by design — see "Why the table is permanent"
// below. Treat this file as the durable HUD-registration home.
//
// What it provides:
//   1. The registration TABLES: wiredHudElementDefs[] (name → create/routine,
//      with SE_* default-visibility + FEAT_* gating) and wiredHudFamilyDefs[]
//      (indexed families team<N>/powerup<N>). WiredHud_RegisterElements()
//      walks them once at WiredUI_Init and binds each into the unified
//      registry under the "hud:<name>" sigil.
//   2. The config-bridge ADAPTER (wui_hud_unified_create + _family + _routine
//      + _destroy): the load-bearing reason the table is permanent — see below.
//   3. Runtime infrastructure: the WiredUI_HUD bump arena
//      (WiredHud_ArenaAlloc/…), the active-element list (WiredHud_CreateElement
//      / DestroyAllElements / GetElementCount), the SE_* visibility predicate
//      (WiredHud_SE_Visible), and the family-index parser (WhudParseFamilyIndex).
//
// Why the table is permanent (NOT a "retire me" shim):
//   The unified registry's create callback is
//     void *(*create)( const wuiCustomDrawConfig_t *cfg )      [cl_wired_customdraw.h]
//   but every HUD element's create is
//     void *CG_ModernHUDElement*Create( const modernhudConfig_t *config )
//   — a deliberate signature boundary: modernhudConfig_t is the stable HUD
//   authoring contract; wuiCustomDrawConfig_t is the generic registry envelope.
//   Because the signatures differ, the registry cannot store an element's own
//   create directly. The single shared adapter wui_hud_unified_create bridges
//   them: at per-item-create time it re-resolves the element by name through
//   WiredHud_FindElementDef (this table) and converts the config via
//   WiredHud_ItemToConfig. So the table is read at every item instantiation,
//   not just at boot. Distributing it into 46 self-registering elements would
//   require either widening the generic registry struct with legacy fields or
//   scattering one elegant bridge into 46 per-element adapters — both are net
//   regressions and reopen the silent-HUD-drop risk. Do NOT re-attempt a
//   "retire the table"; this asymmetry (ownerdraw/loading register direct,
//   HUD's 70+3 entries go through the relookup adapter) is intentional.

#include "../../client.h"
#include "cl_wired_ui_hud_compat.h"
#include "cl_wired_ui_hud_private.h"
#include "cl_wired_customdraw.h"
#include "cl_wired_ui.h"

/* ItemToConfig is now non-static in cl_wired_hud.c. */
extern void WiredHud_ItemToConfig( const wiredItemDef_t *item, modernhudConfig_t *cfg );
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

// ── WiredUI_HUD arena ────────────────────────────────────────────────
//
// every HUD-element context (the per-element struct
// instantiated by ModernHUD_ELEMENT_INIT / WHUD_ELEMENT_INIT_TEXT) moves
// from Z_Malloc → bump arena. The textual call sites in
// code/client/wired/ui/elements/cl_wired_hud_elem_*.c (relocated 2026-05-31
// from hud/elements/) funnel through a single macro that now routes through
// WiredHud_ArenaAlloc below. Bulk reclaim happens in
// WiredHud_DestroyAllElements via Arena_Reset — replaces the per-element
// Z_Free that ran in the destroy loop. No per-element free during normal
// operation (verified by the WiredUI_Arena-investigation turn). Engine-
// layer lifetime: lazy-created on first allocation, released by
// WiredHud_ShutdownArena (called from WiredHud_Shutdown).
//
// Sizing: worst-case 256 active slots × ~1 KB per element ≈ 256 KB. The
// /meminfo `Arena_PrintStats` line shows peak vs capacity at runtime.
#define HUD_ARENA_SIZE  ( 256 * 1024 )

static arena_t *s_hudArena = NULL;

static void WiredHud_EnsureArena( void )
{
	if ( s_hudArena ) return;   /* idempotent (re-entered from every Create) */
	s_hudArena = Arena_Create( "WiredUI_HUD", HUD_ARENA_SIZE );
}

void *WiredHud_ArenaAlloc( int size )
{
	WiredHud_EnsureArena();
	/* 16-byte alignment is safe for every HUD element struct (POD members
	 * + pointers + floats; no SIMD over-alignment). Matches the libc
	 * malloc baseline that Z_Malloc previously provided. */
	return Arena_Alloc( s_hudArena, (size_t)size, 16 );
}

void WiredHud_ShutdownArena( void )
{
	if ( s_hudArena ) {
		Arena_Destroy( s_hudArena );
		s_hudArena = NULL;
	}
}


// ── element registry ─────────────────────────────────────────────────

typedef struct {
	const char *name;
	int         defaultVisibility;
	void*       (*create)(const modernhudConfig_t*);
	void        (*routine)(void*);
	void        (*destroy)(void*);
} wiredHudElementDef_t;

static const wiredHudElementDef_t wiredHudElementDefs[] = {
	{ "!default", 0, NULL, NULL, NULL },
	{ "grid", 0, CG_ModernHUDElementGridCreate, CG_ModernHUDElementGridRoutine, NULL },
	{ "predecorate", 0, CG_ModernHUDElementDecorCreate, CG_ModernHUDElementDecorRoutine, NULL },
	{ "ammomessage", 0, CG_ModernHUDElementAmmoMessageCreate, CG_ModernHUDElementAmmoMessageRoutine, NULL },
	{ "audio_waveform", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementAudioWaveformCreate, CG_ModernHUDElementAudioWaveformRoutine, NULL },
	{ "flagstatus_nme", SE_SIDES_ONLY, CG_ModernHUDElementFlagStatusNMECreate, CG_ModernHUDElementFlagStatusRoutine, NULL },
	{ "flagstatus_own", SE_SIDES_ONLY, CG_ModernHUDElementFlagStatusOWNCreate, CG_ModernHUDElementFlagStatusRoutine, NULL },
	{ "followmessage", 0, CG_ModernHUDElementFollowMessageCreate, CG_ModernHUDElementFollowMessageRoutine, NULL },
	{ "disconnect", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementDisconnectCreate, CG_ModernHUDElementDisconnectRoutine, NULL },
	{ "fps", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementFPSCreate, CG_ModernHUDElementFPSRoutine, NULL },
	{ "fragmessage", 0, CG_ModernHUDElementFragMessageCreate, CG_ModernHUDElementFragMessageRoutine, NULL },
	{ "gametime", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementGameTimeCreate, CG_ModernHUDElementGameTimeRoutine, NULL },
	{ "gametype", 0, CG_ModernHUDElementGameTypeCreate, CG_ModernHUDElementGameTypeRoutine, NULL },
	{ "itempickup", 0, CG_ModernHUDElementItemPickupCreate, CG_ModernHUDElementItemPickupRoutine, NULL },
	{ "itempickupicon", 0, CG_ModernHUDElementItemPickupIconCreate, CG_ModernHUDElementItemPickupIconRoutine, NULL },
#if FEAT_MOVEMENT_KEYS
	{ "keydown_attack", SE_SPECT, CG_ModernHUDElementKeyDownAttackCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keydown_back", SE_SPECT, CG_ModernHUDElementKeyDownBackCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keydown_crouch", SE_SPECT, CG_ModernHUDElementKeyDownCrouchCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keydown_forward", SE_SPECT, CG_ModernHUDElementKeyDownForwardCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keydown_gesture", SE_SPECT, CG_ModernHUDElementKeyDownGestureCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keydown_jump", SE_SPECT, CG_ModernHUDElementKeyDownJumpCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keydown_left", SE_SPECT, CG_ModernHUDElementKeyDownLeftCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keydown_right", SE_SPECT, CG_ModernHUDElementKeyDownRightCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keydown_use", SE_SPECT, CG_ModernHUDElementKeyDownUseCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keydown_walk", SE_SPECT, CG_ModernHUDElementKeyDownWalkCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_attack", SE_SPECT, CG_ModernHUDElementKeyUpAttackCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_back", SE_SPECT, CG_ModernHUDElementKeyUpBackCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_crouch", SE_SPECT, CG_ModernHUDElementKeyUpCrouchCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_forward", SE_SPECT, CG_ModernHUDElementKeyUpForwardCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_gesture", SE_SPECT, CG_ModernHUDElementKeyUpGestureCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_jump", SE_SPECT, CG_ModernHUDElementKeyUpJumpCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_left", SE_SPECT, CG_ModernHUDElementKeyUpLeftCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_right", SE_SPECT, CG_ModernHUDElementKeyUpRightCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_use", SE_SPECT, CG_ModernHUDElementKeyUpUseCreate, CG_ModernHUDElementKeyRoutine, NULL },
	{ "keyup_walk", SE_SPECT, CG_ModernHUDElementKeyUpWalkCreate, CG_ModernHUDElementKeyRoutine, NULL },
#else
	{ "keydown_attack", 0, NULL, NULL, NULL },
	{ "keydown_back", 0, NULL, NULL, NULL },
	{ "keydown_crouch", 0, NULL, NULL, NULL },
	{ "keydown_forward", 0, NULL, NULL, NULL },
	{ "keydown_gesture", 0, NULL, NULL, NULL },
	{ "keydown_jump", 0, NULL, NULL, NULL },
	{ "keydown_left", 0, NULL, NULL, NULL },
	{ "keydown_right", 0, NULL, NULL, NULL },
	{ "keydown_use", 0, NULL, NULL, NULL },
	{ "keydown_walk", 0, NULL, NULL, NULL },
	{ "keyup_attack", 0, NULL, NULL, NULL },
	{ "keyup_back", 0, NULL, NULL, NULL },
	{ "keyup_crouch", 0, NULL, NULL, NULL },
	{ "keyup_forward", 0, NULL, NULL, NULL },
	{ "keyup_gesture", 0, NULL, NULL, NULL },
	{ "keyup_jump", 0, NULL, NULL, NULL },
	{ "keyup_left", 0, NULL, NULL, NULL },
	{ "keyup_right", 0, NULL, NULL, NULL },
	{ "keyup_use", 0, NULL, NULL, NULL },
	{ "keyup_walk", 0, NULL, NULL, NULL },
#endif
	{ "localtime", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementLocalTimeCreate, CG_ModernHUDElementLocalTimeRoutine, NULL },
	{ "localdate", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementLocalDateCreate, CG_ModernHUDElementLocalTimeRoutine, NULL },
	{ "msgqueue", 0, CG_ModernHUDElementMsgQueueCreate, CG_ModernHUDElementMsgQueueRoutine, NULL },
	{ "subtitle", 0, CG_ModernHUDElementSubtitleCreate, CG_ModernHUDElementSubtitleRoutine, NULL },
	{ "objectives", 0, CG_ModernHUDElementObjectivesCreate, CG_ModernHUDElementObjectivesRoutine, NULL },
	{ "name_nme", 0, CG_ModernHUDElementNameNMECreate, CG_ModernHUDElementNameRoutine, NULL },
	{ "name_own", 0, CG_ModernHUDElementNameOWNCreate, CG_ModernHUDElementNameRoutine, NULL },
	{ "netgraph", SE_IM | SE_SPECT | SE_DEAD | SE_DEMO_HIDE, CG_ModernHUDElementNGCreate, CG_ModernHUDElementNGRoutine, NULL },
	{ "netgraphping", SE_IM | SE_SPECT | SE_DEAD | SE_DEMO_HIDE, CG_ModernHUDElementNGPCreate, CG_ModernHUDElementNGPRoutine, NULL },
	{ "playerspeed", 0, CG_ModernHUDElementPlayerSpeedCreate, CG_ModernHUDElementPlayerSpeedRoutine, NULL },
	{ "rankmessage", 0, CG_ModernHUDElementRankMessageCreate, CG_ModernHUDElementRankMessageRoutine, NULL },
	{ "score_limit", 0, CG_ModernHUDElementScoreMAXCreate, CG_ModernHUDElementScoreRoutine, NULL },
	{ "score_nme", 0, CG_ModernHUDElementScoreNMECreate, CG_ModernHUDElementScoreRoutine, NULL },
	{ "score_own", 0, CG_ModernHUDElementScoreOWNCreate, CG_ModernHUDElementScoreRoutine, NULL },
	{ "specmessage", SE_SPECT, CG_ModernHUDElementSpecMessageCreate, CG_ModernHUDElementSpecMessageRoutine, NULL },
	{ "spectators", SE_IM, CG_ModernHUDElementSpectatorsCreate, CG_ModernHUDElementSpectatorsRoutine, NULL },
	{ "targetname", 0, CG_ModernHUDElementTargetNameCreate, CG_ModernHUDElementTargetNameRoutine, NULL },
	{ "targetstatus", SE_SIDES_ONLY, CG_ModernHUDElementTargetStatusCreate, CG_ModernHUDElementTargetStatusRoutine, NULL },
	{ "teamcount_nme", SE_SIDES_ONLY, CG_ModernHUDElementTeamCountNMECreate, CG_ModernHUDElementTeamCountRoutine, NULL },
	{ "teamcount_own", SE_SIDES_ONLY, CG_ModernHUDElementTeamCountOWNCreate, CG_ModernHUDElementTeamCountRoutine, NULL },
	{ "votemessageworld", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementVMWCreate, CG_ModernHUDElementVMWRoutine, NULL },
	// SE_SIDES_ONLY ("teamonly") in addition to the regular vote's flags: a
	// team vote only exists in team gametypes, and the bridge already zeroes
	// the block for anyone not resolved onto TEAM_RED / TEAM_BLUE.
	{ "teamvotemessageworld", SE_IM | SE_SIDES_ONLY | SE_SPECT | SE_DEAD, CG_ModernHUDElementTVMWCreate, CG_ModernHUDElementTVMWRoutine, NULL },
	{ "warmupinfo", 0, CG_ModernHUDElementWarmupInfoCreate, CG_ModernHUDElementWarmupInfoRoutine, NULL },
	{ "weaponlist",    0, CG_ModernHUDElementWeaponListCreate,    CG_ModernHUDElementWeaponListRoutine,    NULL },
	{ "holdablelist",  0, CG_ModernHUDElementHoldableListCreate,  CG_ModernHUDElementHoldableListRoutine,  NULL },
	{ "rewardicons", 0, CG_ModernHUDElementRewardIconCreate, CG_ModernHUDElementRewardRoutine, NULL },
	{ "rewardnumbers", 0, CG_ModernHUDElementRewardCountCreate, CG_ModernHUDElementRewardRoutine, NULL },
	{ "awards", 0, CG_ModernHUDElementAwardsCreate, CG_ModernHUDElementAwardsRoutine, NULL },
	{ "crosshair", 0, CG_ModernHUDElementCrosshairCreate, CG_ModernHUDElementCrosshairRoutine, NULL },
	{ "markerlist", 0, CG_ModernHUDElementMarkerListCreate, CG_ModernHUDElementMarkerListRoutine, NULL },
	{ "statusbar_value", 0, CG_ModernHUDElementStatusbarValueCreate, CG_ModernHUDElementStatusbarValueRoutine, NULL },
	{ "statusbar_icon", 0, CG_ModernHUDElementStatusbarIconCreate, CG_ModernHUDElementStatusbarIconRoutine, NULL },
	{ "statusbar_bar", 0, CG_ModernHUDElementStatusbarBarCreate, CG_ModernHUDElementStatusbarBarRoutine, NULL },
	{ "location", 0, CG_ModernHUDElementLocationCreate, CG_ModernHUDElementLocationRoutine, NULL },
	{ "tempAcc_current", SE_IM | SE_DEAD, CG_ModernHUDElementTempAccTextCreate, CG_ModernHUDElementTempAccRoutine, NULL },
	{ "tempAcc_icon", SE_IM | SE_DEAD, CG_ModernHUDElementTempAccIconCreate, CG_ModernHUDElementTempAccRoutine, NULL },
	{ "currentWeaponStats", SE_IM, CG_ModernHUDElementCreateCurrentWeapon, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_MG", SE_IM, CG_ModernHUDElementWeaponStatsCreateMG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_SG", SE_IM, CG_ModernHUDElementWeaponStatsCreateSG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_GL", SE_IM, CG_ModernHUDElementWeaponStatsCreateGL, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_RL", SE_IM, CG_ModernHUDElementWeaponStatsCreateRL, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_LG", SE_IM, CG_ModernHUDElementWeaponStatsCreateLG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_RG", SE_IM, CG_ModernHUDElementWeaponStatsCreateRG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_PG", SE_IM, CG_ModernHUDElementWeaponStatsCreatePG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "currentWeaponStats_icon", SE_IM, CG_ModernHUDElementIconCreateCurrentWeapon, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_MG_icon", SE_IM, CG_ModernHUDElementIconCreateMG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_SG_icon", SE_IM, CG_ModernHUDElementIconCreateSG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_GL_icon", SE_IM, CG_ModernHUDElementIconCreateGL, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_RL_icon", SE_IM, CG_ModernHUDElementIconCreateRL, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_LG_icon", SE_IM, CG_ModernHUDElementIconCreateLG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_RG_icon", SE_IM, CG_ModernHUDElementIconCreateRG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "weaponStats_PG_icon", SE_IM, CG_ModernHUDElementIconCreatePG, CG_ModernHUDElementWeaponStatsRoutine, NULL },
	{ "playerStats_DG", SE_IM, CG_ModernHUDElementCreatePlayerStatsDG, CG_ModernHUDElementPlayerStatsRoutine, NULL },
	{ "playerStats_DR", SE_IM, CG_ModernHUDElementCreatePlayerStatsDR, CG_ModernHUDElementPlayerStatsRoutine, NULL },
	{ "playerStats_DG_icon", SE_IM, CG_ModernHUDElementCreatePlayerStatsDGIcon, CG_ModernHUDElementPlayerStatsRoutine, NULL },
	{ "playerStats_DR_icon", SE_IM, CG_ModernHUDElementCreatePlayerStatsDRIcon, CG_ModernHUDElementPlayerStatsRoutine, NULL },
	{ "playerStats_damageRatio", SE_IM, CG_ModernHUDElementCreatePlayerStatsDamageRatio, CG_ModernHUDElementPlayerStatsRoutine, NULL },
	{ "player_name", 0, CG_ModernHUDElementPlayerNameCreate, CG_ModernHUDElementPlayerNameRoutine, NULL },
	{ "postdecorate", 0, CG_ModernHUDElementDecorCreate, CG_ModernHUDElementDecorRoutine, NULL },
	{ "netstats", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementNetStatsCreate, CG_ModernHUDElementNetStatsRoutine, NULL },
	{ NULL, 0, NULL, NULL, NULL }
};

// ── family registry (indexed families: chat<N>, team<N>, powerup<N>_icon, powerup<N>_time) ─

typedef struct {
	const char *family;
	int         defaultVisibility;
	void*       (*createIndexed)(const modernhudConfig_t*, int);
	void        (*routine)(void*);
	void        (*destroy)(void*);
} wiredHudFamilyDef_t;

static const wiredHudFamilyDef_t wiredHudFamilyDefs[] = {
	/* chat retired: history is exposed via the WiredStore `hud.chat.*`
	 * prefix; default.wui consumes via a `repeat` block. */
	{ "team",         SE_SIDES_ONLY,              CG_ModernHUDElementTeamCreate,   CG_ModernHUDElementTeamRoutine, NULL },
	{ "powerup_icon", 0,                          CG_ModernHUDElementPwIconCreate, CG_ModernHUDElementPwRoutine,   NULL },
	{ "powerup_time", 0,                          CG_ModernHUDElementPwTimeCreate, CG_ModernHUDElementPwRoutine,   NULL },
	{ NULL, 0, NULL, NULL, NULL }
};

// Parses "chat8" → family="chat", index=8
// Parses "powerup1_icon" → family="powerup_icon", index=1
// Returns qfalse if name contains no digit run or index out of [1..16].
static qboolean WhudParseFamilyIndex( const char *name, char *familyBuf, int familyBufSize, int *outIndex ) {
	/* Shared digit-scan + prefix reconstruction (cl_wired_customdraw.c), with the
	 * hud family bounds [1..16] and this subsystem's out-of-range debug log. */
	if ( !WiredUI_ParseIndexedName( name, familyBuf, familyBufSize, 1, 16, outIndex ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredHud: family index out of range or unparsable in '%s'\n", name );
		return qfalse;
	}
	return qtrue;
}

// ── active element list ──────────────────────────────────────────────

#define WIRED_HUD_MAX_ACTIVE_ELEMENTS  256

typedef struct {
	const char *name;
	void       *context;
	void       (*routine)(void*);
	void       (*destroy)(void*);
	int         visibility;
	int         order;
	qboolean    active;
} wiredHudActiveElement_t;

static wiredHudActiveElement_t wired_hudElements[WIRED_HUD_MAX_ACTIVE_ELEMENTS];
static int wired_hudElementCount = 0;

// ── public API ───────────────────────────────────────────────────────

const wiredHudElementDef_t *WiredHud_FindElementDef( const char *name ) {
	for ( int i = 0; wiredHudElementDefs[i].name; i++ ) {
		if ( !Q_stricmp( wiredHudElementDefs[i].name, name ) ) {
			return &wiredHudElementDefs[i];
		}
	}
	return NULL;
}

qboolean WiredHud_CreateElement( const char *name, const modernhudConfig_t *config ) {
	const wiredHudElementDef_t *def;
	wiredHudActiveElement_t *elem;
	void *ctx;
	char familyName[64];
	int familyIndex;

	if ( wired_hudElementCount >= WIRED_HUD_MAX_ACTIVE_ELEMENTS ) {
		COM_WARN( LOG_CH(ch_ui), "WiredHud: too many active elements\n" );
		return qfalse;
	}

	def = WiredHud_FindElementDef( name );
	if ( def && def->create ) {
		ctx = def->create( config );
	} else if ( WhudParseFamilyIndex( name, familyName, sizeof( familyName ), &familyIndex ) ) {
		const wiredHudFamilyDef_t *fam = NULL;
		for ( int i = 0; wiredHudFamilyDefs[i].family; i++ ) {
			if ( !Q_stricmp( wiredHudFamilyDefs[i].family, familyName ) ) {
				fam = &wiredHudFamilyDefs[i];
				break;
			}
		}
		if ( !fam ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredHud: unknown element '%s'\n", name );
			return qfalse;
		}
		ctx = fam->createIndexed( config, familyIndex );
		if ( !ctx ) return qfalse;
		elem = &wired_hudElements[wired_hudElementCount++];
		elem->name       = name;
		elem->context    = ctx;
		elem->routine    = fam->routine;
		elem->destroy    = fam->destroy;
		elem->visibility = fam->defaultVisibility;
		elem->order      = wired_hudElementCount;
		elem->active     = qtrue;
		return qtrue;
	} else {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredHud: unknown element '%s'\n", name );
		return qfalse;
	}
	if ( !ctx ) return qfalse;

	elem = &wired_hudElements[wired_hudElementCount++];
	elem->name       = def->name;
	elem->context     = ctx;
	elem->routine     = def->routine;
	elem->destroy     = def->destroy;
	elem->visibility  = config->visflags.isSet ? config->visflags.value : def->defaultVisibility;
	elem->order       = wired_hudElementCount;
	elem->active      = qtrue;

	return qtrue;
}

void WiredHud_DestroyAllElements( void ) {
	for ( int i = 0; i < wired_hudElementCount; i++ ) {
		if ( wired_hudElements[i].active && wired_hudElements[i].context ) {
			/* Keep the per-element destroy callback — it may release
			 * non-arena resources (signals/listeners). All currently-
			 * registered defs (wiredHudElementDefs/wiredHudFamilyDefs)
			 * leave .destroy == NULL, but the hook stays available for
			 * future elements that need it. The arena reclaims the
			 * context memory itself in the Arena_Reset below; no per-
			 * element Z_Free is needed (or correct) any more. */
			if ( wired_hudElements[i].destroy )
				wired_hudElements[i].destroy( wired_hudElements[i].context );
		}
		wired_hudElements[i].active = qfalse;
		wired_hudElements[i].context = NULL;
	}
	wired_hudElementCount = 0;

	/* Bulk-reclaim every element context allocated through
	 * WiredHud_ArenaAlloc since the last reset. O(1), no fragmentation. */
	if ( s_hudArena ) {
		Arena_Reset( s_hudArena );
	}
}

/* visibility check exposed for the compositor's CUSTOM
 * dispatch (cl_wired_clay.c) so the unified registry can mirror the
 * existing SE_* gating without duplicating the predicate. Returns qtrue
 * iff an element with the given SE_* flag set would be drawn this frame.
 * Pass 0 for "always visible". */
qboolean WiredHud_SE_Visible( int vflags ) {
	qboolean is_dead, is_intermission, is_team_game, is_spectator, is_scores;
	qboolean skip;

	if ( !wiredHud || !wiredHud_state_valid ) return qtrue;

	/* cinematic director: a scene with its HUD flag off hides the ENTIRE game HUD
	   (every element, regardless of SE_* flags) — the Wired-UI equivalent of the
	   legacy 2D suppress. Pushed by cgame each frame, so the HUD returns on
	   scene-end automatically. */
	if ( wiredHud->hud2DHidden || wiredHud->sceneHudHidden ) return qfalse;

	is_dead         = wiredHud->predictedPlayerState.pm_type == PM_DEAD;
	is_intermission = wiredHud->predictedPlayerState.pm_type == PM_INTERMISSION;
	is_team_game    = wiredHud->isTeamGame;
	is_spectator    = wired_IsSpectator();
	is_scores       = wiredHud->showScores;

	skip = ( !( vflags & SE_IM ) && is_intermission )                 ||
	       ( ( vflags & SE_SIDES_ONLY ) && !is_team_game )            ||
	       ( !( vflags & SE_DEAD ) && is_dead )                       ||
	       ( !( vflags & SE_SPECT ) && is_spectator )                 ||
	       ( ( vflags & SE_SCORES_HIDE ) && is_scores )               ||
	       ( ( vflags & SE_DEMO_HIDE ) && wiredHud->demoPlayback );

	return skip ? qfalse : qtrue;
}

/* WiredHud_RenderElements retired. HUD elements render via the
 * compositor's unified-registry CUSTOM dispatch (LANDED). The
 * legacy per-element routine dispatch is replaced by the wmenu's
 * `custom "hud:<name>"` items resolving to the same hudElement routines
 * through wuiCustomDrawDef_t entries. */

int WiredHud_GetElementCount( void ) {
	return wired_hudElementCount;
}

// ── unified-registry bootstrap ──────────────────────
//
// Adapter envelope: one heap-of-arena slot per per-item context. Stores
// the legacy create+routine+destroy callbacks alongside the legacy
// context pointer so the generic adapters can dispatch without a
// per-element wrapper function. Lifetime tied to s_hudArena — bulk
// reclaim via Arena_Reset in WiredHud_DestroyAllElements.

typedef struct {
	void  (*legacyRoutine)( void *context );
	void  (*legacyDestroy)( void *context );
	void   *legacyContext;
} wuiHudEnvelope_t;

/* Rect-less native WiredUI authoring gets its geometry from Clay, not from the
 * legacy parser backfill.  Preserve an explicitly-authored `rect` for old
 * ModernHUD-compatible files, but seed a missing rect from the resolved custom
 * command before the element's create callback builds its cached draw context.
 * cfg->rect is already in the physical-pixel coordinate space consumed by the
 * ModernHUD draw helpers. */
static void wui_hud_apply_resolved_rect( const wuiCustomDrawConfig_t *cfg,
                                         modernhudConfig_t *hudCfg )
{
	if ( !cfg || !hudCfg || hudCfg->rect.isSet ) return;
	hudCfg->rect.isSet = qtrue;
	hudCfg->rect.value[ 0 ] = cfg->rect[ 0 ];
	hudCfg->rect.value[ 1 ] = cfg->rect[ 1 ];
	hudCfg->rect.value[ 2 ] = cfg->rect[ 2 ];
	hudCfg->rect.value[ 3 ] = cfg->rect[ 3 ];
	/* ModernHUD text contexts derive their anchor from alignH, while WUI
	 * authoring exposes textalign.  Legacy rect literals encoded the anchor
	 * directly in x, but a Clay bounding box encodes its left edge.  For native
	 * flex leaves, map the authored text alignment onto the corresponding point
	 * inside the resolved box so right/center-aligned text remains inside it. */
	if ( !hudCfg->alignH.isSet && hudCfg->textAlign.isSet ) {
		hudCfg->alignH.isSet = qtrue;
		hudCfg->alignH.value = hudCfg->textAlign.value;
	}
}

// Generic create adapter for direct (non-indexed) entries. Re-resolves
// the legacy def by name (cfg->unprefixedName), then converts the source
// wiredItemDef_t into the legacy modernhudConfig_t, then invokes the
// legacy create() and packages the result into the envelope.
static void *wui_hud_unified_create( const wuiCustomDrawConfig_t *cfg )
{
	const wiredHudElementDef_t *def;
	wuiHudEnvelope_t           *env;
	modernhudConfig_t           hudCfg;

	if ( !cfg || !cfg->unprefixedName || !cfg->item ) return NULL;
	def = WiredHud_FindElementDef( cfg->unprefixedName );
	if ( !def || !def->create ) return NULL;

	memset( &hudCfg, 0, sizeof( hudCfg ) );
	WiredHud_ItemToConfig( cfg->item, &hudCfg );
	wui_hud_apply_resolved_rect( cfg, &hudCfg );

	env = (wuiHudEnvelope_t *) WiredHud_ArenaAlloc( (int) sizeof( *env ) );
	if ( !env ) return NULL;
	env->legacyRoutine = def->routine;
	env->legacyDestroy = def->destroy;
	env->legacyContext = def->create( &hudCfg );
	if ( !env->legacyContext ) return NULL;
	return env;
}

// Generic create adapter for family-indexed entries. Looks up the
// family def by parsing the cfg's unprefixedName + uses the parsed
// familyIndex to invoke createIndexed.
static void *wui_hud_unified_create_family( const wuiCustomDrawConfig_t *cfg )
{
	const wiredHudFamilyDef_t  *fam = NULL;
	wuiHudEnvelope_t           *env;
	modernhudConfig_t           hudCfg;
	char                        familyName[ 64 ];
	int                         idx;

	if ( !cfg || !cfg->unprefixedName || !cfg->item ) return NULL;
	if ( !WhudParseFamilyIndex( cfg->unprefixedName, familyName, sizeof( familyName ), &idx ) ) {
		return NULL;
	}
	for ( int i = 0; wiredHudFamilyDefs[ i ].family; i++ ) {
		if ( !Q_stricmp( wiredHudFamilyDefs[ i ].family, familyName ) ) {
			fam = &wiredHudFamilyDefs[ i ];
			break;
		}
	}
	if ( !fam || !fam->createIndexed ) return NULL;

	memset( &hudCfg, 0, sizeof( hudCfg ) );
	WiredHud_ItemToConfig( cfg->item, &hudCfg );
	wui_hud_apply_resolved_rect( cfg, &hudCfg );

	env = (wuiHudEnvelope_t *) WiredHud_ArenaAlloc( (int) sizeof( *env ) );
	if ( !env ) return NULL;
	env->legacyRoutine = fam->routine;
	env->legacyDestroy = fam->destroy;
	env->legacyContext = fam->createIndexed( &hudCfg, idx );
	if ( !env->legacyContext ) return NULL;
	return env;
}

static void wui_hud_unified_routine( void *envPtr, float x, float y, float w, float h, vec4_t color )
{
	wuiHudEnvelope_t *env = (wuiHudEnvelope_t *) envPtr;
	(void) x; (void) y; (void) w; (void) h; (void) color;
	if ( env && env->legacyRoutine ) {
		env->legacyRoutine( env->legacyContext );
	}
}

static void wui_hud_unified_destroy( void *envPtr )
{
	wuiHudEnvelope_t *env = (wuiHudEnvelope_t *) envPtr;
	if ( env && env->legacyDestroy ) {
		env->legacyDestroy( env->legacyContext );
	}
	/* envelope itself is arena-owned; reclaimed via Arena_Reset. */
}

void WiredHud_RegisterElements( void )
{
	int direct = 0, family = 0;

	for ( int i = 0; wiredHudElementDefs[ i ].name; i++ ) {
		const wiredHudElementDef_t *src = &wiredHudElementDefs[ i ];
		wuiCustomDrawDef_t          def;

		/* "!default" sentinel + #else-disabled key entries leave create
		 * NULL — skip them; nothing to register. */
		if ( !src->create ) continue;

		memset( &def, 0, sizeof( def ) );
		Com_sprintf( def.name, sizeof( def.name ), "hud:%s", src->name );
		def.defaultVisibility = src->defaultVisibility;
		def.isStateful        = qtrue;
		def.create            = wui_hud_unified_create;
		def.destroy           = wui_hud_unified_destroy;
		def.routine.stateful  = wui_hud_unified_routine;
		WiredUI_RegisterCustomDraw( &def );
		direct++;
	}

	for ( int i = 0; wiredHudFamilyDefs[ i ].family; i++ ) {
		const wiredHudFamilyDef_t   *src = &wiredHudFamilyDefs[ i ];
		wuiCustomDrawFamilyDef_t     def;

		if ( !src->createIndexed ) continue;

		memset( &def, 0, sizeof( def ) );
		Com_sprintf( def.prefix, sizeof( def.prefix ), "hud:%s", src->family );
		def.minIndex          = 1;   /* WhudParseFamilyIndex enforces [1..16] */
		def.maxIndex          = 16;
		def.defaultVisibility = src->defaultVisibility;
		def.create            = wui_hud_unified_create_family;
		def.destroy           = wui_hud_unified_destroy;
		def.routine_stateful  = wui_hud_unified_routine;
		WiredUI_RegisterCustomDrawFamily( &def );
		family++;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"WiredHud: registered %d direct + %d family entries in unified registry\n",
		direct, family );
}

#endif // FEAT_WIRED_UI
