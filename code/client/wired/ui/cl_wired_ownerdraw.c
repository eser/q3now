/*
cl_wired_ownerdraw.c — TA ownerdraw dispatch table + rendering callbacks
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_hud.h"
#include "cl_wired_text.h"
#include "cl_wired_draw.h"
#include "cl_wired_background.h"
#include "../../../qcommon/menudef.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

/* ── color ramp helper ─────────────────────────────────────────────── */

#define WUI_RAMP_END (-9999)

typedef struct {
	int   threshold;
	float color[4];
} wuiColorRamp_t;

static void WUI_RampColor( int value, const wuiColorRamp_t *ramp, vec4_t out ) {
	for ( ; ramp->threshold != WUI_RAMP_END; ramp++ ) {
		if ( value > ramp->threshold ) {
			Vector4Copy( ramp->color, out );
			return;
		}
	}
	Vector4Copy( ramp->color, out );
}

static const wuiColorRamp_t healthRamp[] = {
	{ 100, { 1,     1,     1,    1 } },
	{  50, { 1,     0.85f, 0,    1 } },
	{  25, { 1,     0.3f,  0,    1 } },
	{ WUI_RAMP_END, { 1, 0, 0, 1 } },
};
static const wuiColorRamp_t armorRamp[] = {
	{ 100, { 1,     1,     1,    1    } },
	{  50, { 1,     0.85f, 0,    1    } },
	{ WUI_RAMP_END, { 0.6f, 0.6f, 0.6f, 1 } },
};
static const wuiColorRamp_t ammoRamp[] = {
	{  10, { 1,     0.85f, 0,    1    } },
	{   0, { 1,     0,     0,    1    } },
	{ WUI_RAMP_END, { 0.4f, 0.4f, 0.4f, 1 } },
};

/* ── centered number/text renderer ────────────────────────────────── */

static void WUI_OD_DrawNumber( float x, float y, float w, float h,
                               const char *buf, const vec4_t color ) {
	Text_Draw( buf, x + w * 0.5f, y + h * 0.25f,
		FONT_DISPLAY, h * 0.5f, color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
}

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
		} // UI range
		return ( wiredHud->uiShowFlags & testFlags ) != 0;
	}

	return qtrue;
}

// ── P1 ownerdraw renderers ───────────────────────────────────────────

static void WiredOD_PlayerHealth( float x, float y, float w, float h, vec4_t itemColor ) {
	char buf[16];
	vec4_t color;
	if ( !wiredHud || !wiredHud->valid ) return;
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->health );
	WUI_RampColor( wiredHud->health, healthRamp, color );
	WUI_OD_DrawNumber( x, y, w, h, buf, color );
}

static void WiredOD_PlayerArmor( float x, float y, float w, float h, vec4_t itemColor ) {
	char buf[16];
	vec4_t color;
	if ( !wiredHud || !wiredHud->valid ) return;
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->armor );
	WUI_RampColor( wiredHud->armor, armorRamp, color );
	WUI_OD_DrawNumber( x, y, w, h, buf, color );
}

static void WiredOD_PlayerAmmoValue( float x, float y, float w, float h, vec4_t itemColor ) {
	int weapon, ammo;
	char buf[16];
	vec4_t color;
	if ( !wiredHud || !wiredHud->valid ) return;
	weapon = wiredHud->weapon;
	if ( weapon <= 0 || weapon >= (int)(sizeof(wiredHud->ammo) / sizeof(wiredHud->ammo[0])) ) return;
	ammo = wiredHud->ammo[weapon];
	Com_sprintf( buf, sizeof( buf ), "%d", ammo );
	WUI_RampColor( ammo, ammoRamp, color );
	WUI_OD_DrawNumber( x, y, w, h, buf, color );
}

static void WiredOD_PlayerArmorIcon( float x, float y, float w, float h, vec4_t itemColor ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	// draw the appropriate armor tier icon
	qhandle_t icon = wiredHud->combatArmorIcon;  // default to combat armor
	if ( icon ) {
		re.SetColor( NULL );
		WUI_DrawPic( x, y, w, h, icon );
	}
}

static void WiredOD_PlayerAmmoIcon( float x, float y, float w, float h, vec4_t itemColor ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	int weapon = wiredHud->weapon;
	if ( weapon <= 0 || weapon >= (int)(sizeof(wiredHud->ammoIcons) / sizeof(wiredHud->ammoIcons[0])) ) return;
	qhandle_t icon = wiredHud->ammoIcons[weapon];
	if ( icon ) {
		re.SetColor( NULL );
		WUI_DrawPic( x, y, w, h, icon );
	}
}

static void WiredOD_PlayerScore( float x, float y, float w, float h, vec4_t itemColor ) {
	char buf[16];
	vec4_t color = { 1, 1, 1, 1 };
	if ( !wiredHud || !wiredHud->valid ) return;
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->persistant[PERS_SCORE] );
	WUI_OD_DrawNumber( x, y, w, h, buf, color );
}

// ── P2 ownerdraw renderers ───────────────────────────────────────────

static void WiredOD_BlueScore( float x, float y, float w, float h, vec4_t itemColor ) {
	char buf[16];
	vec4_t color = { 0.3f, 0.5f, 1.0f, 1.0f };
	if ( !wiredHud || !wiredHud->valid ) return;
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->scores2 );
	Text_Draw( buf, x, y + h * 0.25f,
		FONT_DISPLAY, h * 0.5f, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
}

static void WiredOD_RedScore( float x, float y, float w, float h, vec4_t itemColor ) {
	char buf[16];
	vec4_t color = { 1.0f, 0.3f, 0.3f, 1.0f };
	if ( !wiredHud || !wiredHud->valid ) return;
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->scores1 );
	Text_Draw( buf, x, y + h * 0.25f,
		FONT_DISPLAY, h * 0.5f, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
}

static void WiredOD_Killer( float x, float y, float w, float h, vec4_t itemColor ) {
	char msg[256];
	vec4_t color = { 1.0f, 1.0f, 0.2f, 1.0f };
	if ( !wiredHud || !wiredHud->valid ) return;
	if ( !wiredHud->killerName[0] ) return;
	Com_sprintf( msg, sizeof( msg ), "Fragged by %s", wiredHud->killerName );
	WUI_OD_DrawNumber( x, y, w, h, msg, color );
}

static void WiredOD_GameType( float x, float y, float w, float h, vec4_t itemColor ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	const char *gt;
	gt = wiredHud->gametypeName;
	vec4_t color = { 1, 1, 1, 1 };
	float charSize = 8.0f;
	Text_Draw( gt, x, y + h * 0.25f,
		FONT_UI, charSize, color, TEXT_ALIGN_LEFT, 0 );
}

// ── background grid (scanlines) ──────────────────────────────────────

static void WiredOD_BackgroundGrid( float x, float y, float w, float h, vec4_t itemColor ) {
	vec4_t lineColor;
	if ( itemColor && ( itemColor[0] > 0.0f || itemColor[1] > 0.0f || itemColor[2] > 0.0f ) ) {
		Vector4Copy( itemColor, lineColor );
	} else {
		Vector4Set( lineColor, 1, 1, 1, 0.03f );
	}
	WUI_DrawScanlines( 0, 0, (float)cls.glconfig.vidWidth, (float)cls.glconfig.vidHeight,
	                   lineColor, 48.0f );
}

// ── background full (3-layer) ─────────────────────────────────────────

static void WiredOD_BackgroundFull( float x, float y, float w, float h, vec4_t itemColor ) {
	WUI_DrawBackground( x, y, w, h );
}

// ── UI ownerdraw renderers ────────────────────────────────────────────

static void WiredOD_NetMapPreview( float x, float y, float w, float h, vec4_t itemColor ) {
	// Draw the levelshot for the currently selected server's map.
	// The cvar ui_mapLevelshot is set by server selection in the feeder.
	char lsBuf[MAX_QPATH];
	qhandle_t shader;

	WiredUI_StateGetString( "ui_mapLevelshot", lsBuf, sizeof( lsBuf ) );
	if ( !lsBuf[0] ) return;

	shader = re.RegisterShaderNoMip( lsBuf );
	if ( shader ) {
		re.SetColor( NULL );
		WUI_DrawPic( x, y, w, h, shader );
	}
}

// ── player model preview ──────────────────────────────────────────────
// Cached per-frame state: (char, skin) → (head model handle, skin handle).
// Re-resolved only when either cvar changes; otherwise we just reuse the cached handles.

static char      wui_previewChar[MAX_QPATH];
static char      wui_previewSkin[CM_SKIN_NAME_LEN];
static qhandle_t wui_previewModel = 0;
static qhandle_t wui_previewSkinHandle = 0;

// Resolve a model handle for the given path prefix (no extension).
// Tries .iqm first, then .md3, mirroring CG_LoadCharacter.
static qhandle_t WUI_RegisterCharacterMesh( const char *prefix ) {
	char tryPath[MAX_QPATH];
	qhandle_t handle;

	Com_sprintf( tryPath, sizeof( tryPath ), "%s.iqm", prefix );
	handle = re.RegisterModel( tryPath );
	if ( handle ) return handle;

	Com_sprintf( tryPath, sizeof( tryPath ), "%s.md3", prefix );
	handle = re.RegisterModel( tryPath );
	if ( !handle ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredOD_PlayerModel: no model at '%s.iqm' or '%s.md3'\n",
			prefix, prefix );
	}
	return handle;
}

static void WiredOD_PlayerModel( float x, float y, float w, float h, vec4_t itemColor ) {
	refdef_t    refdef;
	refEntity_t ent;
	char        charNameBuf[MAX_QPATH];
	char        skinNameBuf[CM_SKIN_NAME_LEN];
	vec3_t      angles;

	// read char and skin cvars
	Cvar_VariableStringBuffer( "char", charNameBuf, sizeof( charNameBuf ) );
	if ( !charNameBuf[0] ) Q_strncpyz( charNameBuf, "visor", sizeof( charNameBuf ) );
	Cvar_VariableStringBuffer( "skin", skinNameBuf, sizeof( skinNameBuf ) );
	if ( !skinNameBuf[0] ) Q_strncpyz( skinNameBuf, "default", sizeof( skinNameBuf ) );

	// re-resolve the cached model + skin handles when either cvar changed
	if ( Q_stricmp( charNameBuf, wui_previewChar ) || Q_stricmp( skinNameBuf, wui_previewSkin ) ) {
		const characterManifest_t *mf = CL_Characters_Get( charNameBuf );
		int i;
		const char *resolvedPrefix = "(no manifest)";
		int resolvedNumSkins = 0;

		Q_strncpyz( wui_previewChar, charNameBuf, sizeof( wui_previewChar ) );
		Q_strncpyz( wui_previewSkin, skinNameBuf, sizeof( wui_previewSkin ) );
		wui_previewModel = 0;
		wui_previewSkinHandle = 0;

		if ( mf ) {
			// pick the "head" part if present; else fall back to the first available part
			int chosenPart = -1;
			for ( i = 0; i < mf->partCount; i++ ) {
				if ( !Q_stricmp( mf->partNames[i], "head" ) ) { chosenPart = i; break; }
			}
			if ( chosenPart < 0 && mf->partCount > 0 ) chosenPart = 0;
			if ( chosenPart >= 0 ) {
				wui_previewModel = WUI_RegisterCharacterMesh( mf->partPaths[chosenPart] );
				resolvedPrefix = mf->partPaths[chosenPart];
			}

			resolvedNumSkins = mf->numSkins;
			for ( i = 0; i < mf->numSkins; i++ ) {
				if ( !Q_stricmp( mf->skins[i].name, skinNameBuf ) ) {
					wui_previewSkinHandle = mf->skins[i].skinHandle;
					break;
				}
			}
		}

		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"WiredOD_PlayerModel: char='%s' skin='%s' parts=%d skins=%d prefix='%s' modelHandle=%d skinHandle=%d\n",
			charNameBuf, skinNameBuf,
			mf ? mf->partCount : -1, resolvedNumSkins,
			resolvedPrefix, (int)wui_previewModel, (int)wui_previewSkinHandle );
	}

	if ( !wui_previewModel ) return;

	// coordinates are already real screen pixels
	memset( &refdef, 0, sizeof( refdef ) );
	refdef.rdflags = RDF_NOWORLDMODEL;
	refdef.x       = (int)x;
	refdef.y       = (int)y;
	refdef.width   = (int)w;
	refdef.height  = (int)h;
	if ( refdef.width < 1 || refdef.height < 1 ) return;
	refdef.fov_x   = 25.0f;
	refdef.fov_y   = 25.0f * (float)refdef.height / (float)refdef.width;
	refdef.time    = cls.realtime;

	// Camera at +X 80, looking down -X toward the model at origin.
	// Q3's viewaxis[0] is the FORWARD direction (see R_RotateForViewer's
	// "looking down X" comment), so AxisClear (= identity) would point the
	// camera AWAY from the model. YAW 180 swings forward to -X.
	{
		vec3_t viewangles;
		VectorSet( viewangles, 0, 180, 0 );
		AnglesToAxis( viewangles, refdef.viewaxis );
	}
	VectorSet( refdef.vieworg, 80, 0, 0 );

	// model entity — spinning head at origin
	memset( &ent, 0, sizeof( ent ) );
	ent.reType  = RT_MODEL;
	ent.hModel  = wui_previewModel;
	ent.characterSkin = wui_previewSkinHandle;
	ent.origin[2] = -5.0f;

	angles[PITCH] = 0;
	angles[YAW]   = (float)( cls.realtime % 10000 ) / 10000.0f * 360.0f;
	angles[ROLL]  = 0;
	AnglesToAxis( angles, ent.axis );

	VectorCopy( ent.origin, ent.lightingOrigin );
	ent.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;

	re.ClearScene();
	re.AddRefEntityToScene( &ent, qfalse );
	re.RenderScene( &refdef );
}

// ── player setup ownerdraws ───────────────────────────────────────────

// Display the player's effect (per-weapon player-effect tint) — cvar "color1", 1-7.
static const char *wui_effectNames[] = {
	"None", "Red", "Yellow", "Green", "Cyan", "Blue", "Magenta", "White"
};

static void WiredOD_Effects( float x, float y, float w, float h, vec4_t itemColor ) {
	char  buf[32];
	int   effect;
	vec4_t color;
	const int numEffects = (int)( sizeof( wui_effectNames ) / sizeof( wui_effectNames[0] ) );

	effect = (int)Cvar_VariableValue( "color1" );
	if ( effect < 0 || effect >= numEffects ) effect = 0;
	Q_strncpyz( buf, wui_effectNames[effect], sizeof( buf ) );

	if ( itemColor ) Vector4Copy( itemColor, color );
	else             Vector4Set( color, 1, 1, 1, 1 );

	Text_Draw( buf, x + w - 8.0f, y + h * 0.5f + 5.0f,
		FONT_UI, 14.0f, color, TEXT_ALIGN_RIGHT, 0 );
}

// ── dispatch table ────────────────────────────────────────────────────

typedef void (*ownerDrawFunc_t)( float x, float y, float w, float h, vec4_t itemColor );

typedef struct {
	int              id;
	ownerDrawFunc_t  draw;
	const char      *name;  // script-side identifier; .wmenu writes `ownerdraw "name"`
} ownerDrawEntry_t;

static const ownerDrawEntry_t ownerDrawTable[] = {
	// P1: most-used in TA HUD widgets
	{ CG_PLAYER_HEALTH,       WiredOD_PlayerHealth,     "player_health" },
	{ CG_PLAYER_ARMOR_VALUE,  WiredOD_PlayerArmor,      "player_armor" },
	{ CG_PLAYER_AMMO_VALUE,   WiredOD_PlayerAmmoValue,  "player_ammo" },
	{ CG_PLAYER_ARMOR_ICON,   WiredOD_PlayerArmorIcon,  "player_armor_icon" },
	{ CG_PLAYER_AMMO_ICON,    WiredOD_PlayerAmmoIcon,   "player_ammo_icon" },
	{ CG_PLAYER_ARMOR_ICON2D, WiredOD_PlayerArmorIcon,  "player_armor_icon_2d" },
	{ CG_PLAYER_AMMO_ICON2D,  WiredOD_PlayerAmmoIcon,   "player_ammo_icon_2d" },
	{ CG_PLAYER_SCORE,        WiredOD_PlayerScore,      "player_score" },

	// P2: team/CTF/match info
	{ CG_BLUE_SCORE,          WiredOD_BlueScore,        "blue_score" },
	{ CG_RED_SCORE,           WiredOD_RedScore,         "red_score" },
	{ CG_KILLER,              WiredOD_Killer,           "killer" },
	{ CG_GAME_TYPE,           WiredOD_GameType,         "game_type" },

	// Wired UI extensions
	{ UI_BACKGROUND_GRID,     WiredOD_BackgroundGrid,   "background_grid" },
	{ UI_BACKGROUND_FULL,     WiredOD_BackgroundFull,   "background_full" },

	// UI ownerdraw items (server browser, player setup, etc.)
	{ UI_NETMAPPREVIEW,       WiredOD_NetMapPreview,    "net_map_preview" },
	{ UI_PLAYERMODEL,         WiredOD_PlayerModel,      "player_model" },
	{ UI_EFFECTS,             WiredOD_Effects,          "effects" },

	{ 0, NULL, NULL }  // sentinel
};

// Look up an ownerdraw numeric ID by its registered script name.
// Case-insensitive. Returns 0 (treated as "no ownerdraw") if not found.
int WiredUI_OwnerDrawIDByName( const char *name ) {
	int i;
	if ( !name || !name[0] ) return 0;
	for ( i = 0; ownerDrawTable[i].draw != NULL; i++ ) {
		if ( ownerDrawTable[i].name && !Q_stricmp( ownerDrawTable[i].name, name ) ) {
			return ownerDrawTable[i].id;
		}
	}
	return 0;
}

// track which ownerdraw IDs we've already warned about (log once per ID)
static unsigned long long wui_odWarnedBits[2] = {0, 0};  // 128 bits = IDs 0-127

void WiredUI_OwnerDraw( int ownerDraw, float x, float y, float w, float h,
                         vec4_t color, int style ) {
	int i;

	if ( ownerDraw <= 0 ) return;

	// search dispatch table
	for ( i = 0; ownerDrawTable[i].draw != NULL; i++ ) {
		if ( ownerDrawTable[i].id == ownerDraw ) {
			ownerDrawTable[i].draw( x, y, w, h, color );
			return;
		}
	}

	// P3: unimplemented ownerdraw — log once per ID
	if ( ownerDraw < 128 ) {
		int word = ownerDraw / 64;
		unsigned long long bit = 1ULL << ( ownerDraw % 64 );
		if ( !( wui_odWarnedBits[word] & bit ) ) {
			wui_odWarnedBits[word] |= bit;
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: unimplemented ownerdraw %d\n", ownerDraw );
		}
	}
}

#endif // FEAT_WIRED_UI
