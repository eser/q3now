// cl_wired_hud_registry.c — Wired UI HUD: element registry + lifecycle

#include "../../client.h"
#include "cl_wired_hud_compat.h"
#include "cl_wired_hud_private.h"

#if FEAT_WIRED_UI


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
	{ "botdirectives", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementBotDirectivesCreate, CG_ModernHUDElementBotDirectivesRoutine, NULL },
	{ "spectators", SE_IM, CG_ModernHUDElementSpectatorsCreate, CG_ModernHUDElementSpectatorsRoutine, NULL },
	{ "targetname", 0, CG_ModernHUDElementTargetNameCreate, CG_ModernHUDElementTargetNameRoutine, NULL },
	{ "targetstatus", SE_SIDES_ONLY, CG_ModernHUDElementTargetStatusCreate, CG_ModernHUDElementTargetStatusRoutine, NULL },
	{ "teamcount_nme", SE_SIDES_ONLY, CG_ModernHUDElementTeamCountNMECreate, CG_ModernHUDElementTeamCountRoutine, NULL },
	{ "teamcount_own", SE_SIDES_ONLY, CG_ModernHUDElementTeamCountOWNCreate, CG_ModernHUDElementTeamCountRoutine, NULL },
	{ "votemessageworld", SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementVMWCreate, CG_ModernHUDElementVMWRoutine, NULL },
	{ "warmupinfo", 0, CG_ModernHUDElementWarmupInfoCreate, CG_ModernHUDElementWarmupInfoRoutine, NULL },
	{ "weaponlist", 0, CG_ModernHUDElementWeaponListCreate, CG_ModernHUDElementWeaponListRoutine, NULL },
	{ "rewardicons", 0, CG_ModernHUDElementRewardIconCreate, CG_ModernHUDElementRewardRoutine, NULL },
	{ "rewardnumbers", 0, CG_ModernHUDElementRewardCountCreate, CG_ModernHUDElementRewardRoutine, NULL },
	{ "awards", 0, CG_ModernHUDElementAwardsCreate, CG_ModernHUDElementAwardsRoutine, NULL },
	{ "crosshair", 0, CG_ModernHUDElementCrosshairCreate, CG_ModernHUDElementCrosshairRoutine, NULL },
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
	{ "chat",         SE_IM | SE_SPECT | SE_DEAD, CG_ModernHUDElementChatCreate,   CG_ModernHUDElementChatRoutine, NULL },
	{ "team",         SE_SIDES_ONLY,              CG_ModernHUDElementTeamCreate,   CG_ModernHUDElementTeamRoutine, NULL },
	{ "powerup_icon", 0,                          CG_ModernHUDElementPwIconCreate, CG_ModernHUDElementPwRoutine,   NULL },
	{ "powerup_time", 0,                          CG_ModernHUDElementPwTimeCreate, CG_ModernHUDElementPwRoutine,   NULL },
	{ NULL, 0, NULL, NULL, NULL }
};

// Parses "chat8" → family="chat", index=8
// Parses "powerup1_icon" → family="powerup_icon", index=1
// Returns qfalse if name contains no digit run or index out of [1..16].
static qboolean WhudParseFamilyIndex( const char *name, char *familyBuf, int familyBufSize, int *outIndex ) {
	const char *p = name;
	const char *digitStart;
	int index = 0;

	while ( *p && !( *p >= '0' && *p <= '9' ) ) p++;
	if ( !*p ) return qfalse;

	digitStart = p;
	while ( *p >= '0' && *p <= '9' ) { index = index * 10 + ( *p - '0' ); p++; }

	if ( index < 1 || index > 16 ) {
		Com_DPrintf( "WiredHud: family index out of range in '%s'\n", name );
		return qfalse;
	}

	// family = prefix + role-suffix (e.g. "powerup" + "_icon" = "powerup_icon")
	Com_sprintf( familyBuf, familyBufSize, "%.*s%s", (int)( digitStart - name ), name, p );
	*outIndex = index;
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
	int i;
	for ( i = 0; wiredHudElementDefs[i].name; i++ ) {
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
	int i;

	if ( wired_hudElementCount >= WIRED_HUD_MAX_ACTIVE_ELEMENTS ) {
		Com_Printf( S_COLOR_YELLOW "WiredHud: too many active elements\n" );
		return qfalse;
	}

	def = WiredHud_FindElementDef( name );
	if ( def && def->create ) {
		ctx = def->create( config );
	} else if ( WhudParseFamilyIndex( name, familyName, sizeof( familyName ), &familyIndex ) ) {
		const wiredHudFamilyDef_t *fam = NULL;
		for ( i = 0; wiredHudFamilyDefs[i].family; i++ ) {
			if ( !Q_stricmp( wiredHudFamilyDefs[i].family, familyName ) ) {
				fam = &wiredHudFamilyDefs[i];
				break;
			}
		}
		if ( !fam ) {
			Com_DPrintf( "WiredHud: unknown element '%s'\n", name );
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
		Com_DPrintf( "WiredHud: unknown element '%s'\n", name );
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
	int i;
	for ( i = 0; i < wired_hudElementCount; i++ ) {
		if ( wired_hudElements[i].active && wired_hudElements[i].context ) {
			if ( wired_hudElements[i].destroy )
				wired_hudElements[i].destroy( wired_hudElements[i].context );
			else
				Z_Free( wired_hudElements[i].context );
		}
		wired_hudElements[i].active = qfalse;
		wired_hudElements[i].context = NULL;
	}
	wired_hudElementCount = 0;
}

void WiredHud_RenderElements( void ) {
	int i;
	qboolean is_dead, is_intermission, is_team_game, is_spectator, is_scores;
	int vflags;
	qboolean skip;

	is_dead        = wiredHud->predictedPlayerState.pm_type == PM_DEAD;
	is_intermission = wiredHud->predictedPlayerState.pm_type == PM_INTERMISSION;
	is_team_game   = wiredHud->isTeamGame;
	is_spectator   = wired_IsSpectator();
	is_scores      = wiredHud->showScores;

	for ( i = 0; i < wired_hudElementCount; i++ ) {
		wiredHudActiveElement_t *elem = &wired_hudElements[i];
		if ( !elem->active || !elem->routine || !elem->context ) continue;

		vflags = elem->visibility;

		skip = (!(vflags & SE_IM) && is_intermission) ||
		       ((vflags & SE_SIDES_ONLY) && !is_team_game) ||
		       (!(vflags & SE_DEAD) && is_dead) ||
		       (!(vflags & SE_SPECT) && is_spectator) ||
		       ((vflags & SE_SCORES_HIDE) && is_scores) ||
		       ((vflags & SE_DEMO_HIDE) && wiredHud->demoPlayback);

		if ( !skip ) {
			elem->routine( elem->context );
		}
	}
}

int WiredHud_GetElementCount( void ) {
	return wired_hudElementCount;
}

#endif // FEAT_WIRED_UI
