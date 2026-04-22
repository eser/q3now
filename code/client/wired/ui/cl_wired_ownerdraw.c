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
		} else {
			// UI range
			return ( wiredHud->uiShowFlags & testFlags ) != 0;
		}
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

static qhandle_t wui_headModel = 0;
static char      wui_headModelName[MAX_QPATH];

static void WiredOD_PlayerModel( float x, float y, float w, float h, vec4_t itemColor ) {
	refdef_t    refdef;
	refEntity_t ent;
	char        charNameBuf[MAX_QPATH];
	char        headPath[MAX_QPATH];
	vec3_t      angles;

	// read char cvar
	Cvar_VariableStringBuffer( "char", charNameBuf, sizeof( charNameBuf ) );
	if ( !charNameBuf[0] ) Q_strncpyz( charNameBuf, "visor", sizeof( charNameBuf ) );

	// cache head model handle
	Com_sprintf( headPath, sizeof( headPath ), "characters/%s/models/head.md3", charNameBuf );
	if ( Q_stricmp( headPath, wui_headModelName ) ) {
		Q_strncpyz( wui_headModelName, headPath, sizeof( wui_headModelName ) );
		wui_headModel = re.RegisterModel( headPath );
	}
	if ( !wui_headModel ) return;

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
	AxisClear( refdef.viewaxis );

	// camera looks down -X toward the model at origin
	VectorSet( refdef.vieworg, 80, 0, 0 );

	// model entity — spinning head at origin
	memset( &ent, 0, sizeof( ent ) );
	ent.reType  = RT_MODEL;
	ent.hModel  = wui_headModel;
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

// ── dispatch table ────────────────────────────────────────────────────

typedef void (*ownerDrawFunc_t)( float x, float y, float w, float h, vec4_t itemColor );

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

	// Wired UI extensions
	{ UI_BACKGROUND_GRID,     WiredOD_BackgroundGrid,   "UI_BACKGROUND_GRID" },
	{ UI_BACKGROUND_FULL,     WiredOD_BackgroundFull,    "UI_BACKGROUND_FULL" },

	// UI ownerdraw items (server browser, etc.)
	{ UI_NETMAPPREVIEW,       WiredOD_NetMapPreview,    "UI_NETMAPPREVIEW" },
	{ UI_PLAYERMODEL,         WiredOD_PlayerModel,      "UI_PLAYERMODEL" },

	{ 0, NULL, NULL }  // sentinel
};

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
			Com_DPrintf( "WiredUI: unimplemented ownerdraw %d\n", ownerDraw );
		}
	}
}

#endif // FEAT_WIRED_UI
