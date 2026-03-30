/*
===========================================================================
cl_wired_ownerdraw.c — TA ownerdraw dispatch table + rendering callbacks

Evaluates CG_SHOW / UI_SHOW visibility flags and renders CG_OWNERDRAW
item types for Team Arena menu compatibility. Each callback receives the
item's rect and renders within it using data from wiredHudState_t.

Tiered implementation:
  P1: health, armor, ammo, powerups, head, score (most-used in TA menus)
  P2: killer, CTF powerup, flag status, gametype
  P3: everything else (stub: log once, render nothing)
===========================================================================
*/

#include "../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_hud.h"
#include "cl_wired_fonts.h"
#include "../../ui/menudef.h"

#if FEAT_WIRED_UI

// ── ownerdrawFlag evaluation ──────────────────────────────────────────
// Returns qtrue if the item should be visible given current game state.
// TA menus use ownerdrawFlag with CG_SHOW_* or UI_SHOW_* hex values.

qboolean WiredUI_OwnerDrawVisible( int flags ) {
	if ( !flags ) return qtrue;  // no flag = always visible

	if ( !wiredHud || !wiredHud->valid ) return qtrue;  // show everything if no game state

	// CG_SHOW_* flags (lower range — game state)
	// TA uses these as a bitmask: item is visible if (flags & cgShowFlags) != 0
	// CG_SHOW_2DONLY (0x10000000) is always true in 2D menus
	unsigned int testFlags = flags & ~0x10000000;  // strip 2DONLY bit

	if ( testFlags ) {
		// check if any of the requested flags match current state
		if ( testFlags < UI_SHOW_LEADER ) {
			// CG range (values below 0x00000001 of UI range)
			return ( wiredHud->cgShowFlags & testFlags ) != 0;
		} else {
			// UI range
			return ( wiredHud->uiShowFlags & testFlags ) != 0;
		}
	}

	return qtrue;
}

// ── P1 ownerdraw renderers ───────────────────────────────────────────

static void WiredOD_PlayerHealth( float x, float y, float w, float h ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	char buf[16];
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->health );
	vec4_t color;
	if ( wiredHud->health > 100 ) {
		Vector4Set( color, 1, 1, 1, 1 );
	} else if ( wiredHud->health > 50 ) {
		Vector4Set( color, 1, 0.85f, 0, 1 );
	} else if ( wiredHud->health > 25 ) {
		Vector4Set( color, 1, 0.3f, 0, 1 );
	} else {
		Vector4Set( color, 1, 0, 0, 1 );
	}
	float scale = h > 20 ? 0.6f : 0.4f;
	fontInfo_t *font = WiredUI_GetTAFont( TA_FONT_NORMAL );
	float tw = WiredUI_TextWidth_TA( buf, scale, 0, font );
	WiredUI_DrawText_TA( x + ( w - tw ) * 0.5f, y + h * 0.25f, scale, color, buf, 0, 3, font );
}

static void WiredOD_PlayerArmor( float x, float y, float w, float h ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	char buf[16];
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->armor );
	vec4_t color;
	if ( wiredHud->armor > 100 ) {
		Vector4Set( color, 1, 1, 1, 1 );
	} else if ( wiredHud->armor > 50 ) {
		Vector4Set( color, 1, 0.85f, 0, 1 );
	} else {
		Vector4Set( color, 0.6f, 0.6f, 0.6f, 1 );
	}
	float scale = h > 20 ? 0.6f : 0.4f;
	fontInfo_t *font = WiredUI_GetTAFont( TA_FONT_NORMAL );
	float tw = WiredUI_TextWidth_TA( buf, scale, 0, font );
	WiredUI_DrawText_TA( x + ( w - tw ) * 0.5f, y + h * 0.25f, scale, color, buf, 0, 3, font );
}

static void WiredOD_PlayerAmmoValue( float x, float y, float w, float h ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	int weapon = wiredHud->weapon;
	if ( weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS ) return;
	int ammo = wiredHud->ammo[weapon];
	char buf[16];
	Com_sprintf( buf, sizeof( buf ), "%d", ammo );
	vec4_t color;
	if ( ammo > 10 ) {
		Vector4Set( color, 1, 0.85f, 0, 1 );
	} else if ( ammo > 0 ) {
		Vector4Set( color, 1, 0, 0, 1 );
	} else {
		Vector4Set( color, 0.4f, 0.4f, 0.4f, 1 );
	}
	float scale = h > 20 ? 0.6f : 0.4f;
	fontInfo_t *font = WiredUI_GetTAFont( TA_FONT_NORMAL );
	float tw = WiredUI_TextWidth_TA( buf, scale, 0, font );
	WiredUI_DrawText_TA( x + ( w - tw ) * 0.5f, y + h * 0.25f, scale, color, buf, 0, 3, font );
}

static void WiredOD_PlayerArmorIcon( float x, float y, float w, float h ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	// draw the appropriate armor tier icon
	qhandle_t icon = wiredHud->combatArmorIcon;  // default to combat armor
	if ( icon ) {
		re.SetColor( NULL );
		SCR_DrawPic( x, y, w, h, icon );
	}
}

static void WiredOD_PlayerAmmoIcon( float x, float y, float w, float h ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	int weapon = wiredHud->weapon;
	if ( weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS ) return;
	qhandle_t icon = wiredHud->ammoIcons[weapon];
	if ( icon ) {
		re.SetColor( NULL );
		SCR_DrawPic( x, y, w, h, icon );
	}
}

static void WiredOD_PlayerScore( float x, float y, float w, float h ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	int score = wiredHud->persistant[PERS_SCORE];
	char buf[16];
	Com_sprintf( buf, sizeof( buf ), "%d", score );
	vec4_t color = { 1, 1, 1, 1 };
	float scale = h > 20 ? 0.6f : 0.4f;
	fontInfo_t *font = WiredUI_GetTAFont( TA_FONT_NORMAL );
	float tw = WiredUI_TextWidth_TA( buf, scale, 0, font );
	WiredUI_DrawText_TA( x + ( w - tw ) * 0.5f, y + h * 0.25f, scale, color, buf, 0, 3, font );
}

// ── P2 ownerdraw renderers ───────────────────────────────────────────

static void WiredOD_BlueScore( float x, float y, float w, float h ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	char buf[16];
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->scores2 );
	vec4_t color = { 0.3f, 0.5f, 1.0f, 1.0f };
	float scale = h > 20 ? 0.6f : 0.4f;
	fontInfo_t *font = WiredUI_GetTAFont( TA_FONT_NORMAL );
	WiredUI_DrawText_TA( x, y + h * 0.25f, scale, color, buf, 0, 3, font );
}

static void WiredOD_RedScore( float x, float y, float w, float h ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	char buf[16];
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->scores1 );
	vec4_t color = { 1.0f, 0.3f, 0.3f, 1.0f };
	float scale = h > 20 ? 0.6f : 0.4f;
	fontInfo_t *font = WiredUI_GetTAFont( TA_FONT_NORMAL );
	WiredUI_DrawText_TA( x, y + h * 0.25f, scale, color, buf, 0, 3, font );
}

static void WiredOD_Killer( float x, float y, float w, float h ) {
	// The "who killed you" text — uses killerName from the bridge
	if ( !wiredHud || !wiredHud->valid ) return;
	// TODO: add killerName to wiredHudState_t — for now, stub
}

static void WiredOD_GameType( float x, float y, float w, float h ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	const char *gt;
	switch ( wiredHud->gametype ) {
		case 0: gt = "Free For All"; break;
		case 1: gt = "Tournament"; break;
		case 2: gt = "Single Player"; break;
		case 3: gt = "Team Deathmatch"; break;
		case 4: gt = "CTF"; break;
		case 5: gt = "1-Flag CTF"; break;
		case 6: gt = "Overload"; break;
		case 7: gt = "Harvester"; break;
		case 8: gt = "FreezeTag"; break;
		default: gt = "Unknown"; break;
	}
	vec4_t color = { 1, 1, 1, 1 };
	float scale = 0.35f;
	fontInfo_t *font = WiredUI_GetTAFont( TA_FONT_SMALL );
	WiredUI_DrawText_TA( x, y + h * 0.25f, scale, color, gt, 0, 0, font );
}

// ── dispatch table ────────────────────────────────────────────────────

typedef void (*ownerDrawFunc_t)( float x, float y, float w, float h );

typedef struct {
	int              id;
	ownerDrawFunc_t  draw;
	const char      *name;  // for debug logging
} ownerDrawEntry_t;

static const ownerDrawEntry_t ownerDrawTable[] = {
	// P1: most-used in TA HUD menus
	{ CG_PLAYER_HEALTH,       WiredOD_PlayerHealth,     "CG_PLAYER_HEALTH" },
	{ CG_PLAYER_ARMOR_VALUE,  WiredOD_PlayerArmor,      "CG_PLAYER_ARMOR_VALUE" },
	{ CG_PLAYER_AMMO_VALUE,   WiredOD_PlayerAmmoValue,  "CG_PLAYER_AMMO_VALUE" },
	{ CG_PLAYER_ARMOR_ICON,   WiredOD_PlayerArmorIcon,  "CG_PLAYER_ARMOR_ICON" },
	{ CG_PLAYER_AMMO_ICON,    WiredOD_PlayerAmmoIcon,   "CG_PLAYER_AMMO_ICON" },
	{ CG_PLAYER_ARMOR_ICON2D, WiredOD_PlayerArmorIcon,  "CG_PLAYER_ARMOR_ICON2D" },
	{ CG_PLAYER_AMMO_ICON2D,  WiredOD_PlayerAmmoIcon,   "CG_PLAYER_AMMO_ICON2D" },
	{ CG_PLAYER_SCORE,        WiredOD_PlayerScore,      "CG_PLAYER_SCORE" },

	// P2: team/CTF/match info
	{ CG_BLUE_SCORE,          WiredOD_BlueScore,        "CG_BLUE_SCORE" },
	{ CG_RED_SCORE,           WiredOD_RedScore,         "CG_RED_SCORE" },
	{ CG_KILLER,              WiredOD_Killer,           "CG_KILLER" },
	{ CG_GAME_TYPE,           WiredOD_GameType,         "CG_GAME_TYPE" },

	{ 0, NULL, NULL }  // sentinel
};

// track which ownerdraw IDs we've already warned about (log once per ID)
static unsigned long long wired_odWarnedBits[2] = {0, 0};  // 128 bits = IDs 0-127

void WiredUI_OwnerDraw( int ownerDraw, float x, float y, float w, float h,
                         vec4_t color, int style ) {
	int i;

	if ( ownerDraw <= 0 ) return;

	// search dispatch table
	for ( i = 0; ownerDrawTable[i].draw != NULL; i++ ) {
		if ( ownerDrawTable[i].id == ownerDraw ) {
			ownerDrawTable[i].draw( x, y, w, h );
			return;
		}
	}

	// P3: unimplemented ownerdraw — log once per ID
	if ( ownerDraw < 128 ) {
		int word = ownerDraw / 64;
		unsigned long long bit = 1ULL << ( ownerDraw % 64 );
		if ( !( wired_odWarnedBits[word] & bit ) ) {
			wired_odWarnedBits[word] |= bit;
			Com_DPrintf( "WiredUI: unimplemented ownerdraw %d\n", ownerDraw );
		}
	}
}

#endif // FEAT_WIRED_UI
