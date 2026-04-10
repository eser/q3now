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

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_hud.h"
#include "cl_wired_text.h"
#include "cl_wired_draw.h"
#include "cl_wired_background.h"
#include "../../../qcommon/menudef.h"

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

static void WiredOD_PlayerHealth( float x, float y, float w, float h, vec4_t itemColor ) {
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
	float charSize = h > 20.0f ? 12.0f : 8.0f;
	Text_Draw( buf, x + w * 0.5f, y + h * 0.25f,
		FONT_DISPLAY, charSize, color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
}

static void WiredOD_PlayerArmor( float x, float y, float w, float h, vec4_t itemColor ) {
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
	float charSize = h > 20.0f ? 12.0f : 8.0f;
	Text_Draw( buf, x + w * 0.5f, y + h * 0.25f,
		FONT_DISPLAY, charSize, color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
}

static void WiredOD_PlayerAmmoValue( float x, float y, float w, float h, vec4_t itemColor ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	int weapon = wiredHud->weapon;
	if ( weapon <= 0 || weapon >= (int)(sizeof(wiredHud->ammo) / sizeof(wiredHud->ammo[0])) ) return;
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
	float charSize = h > 20.0f ? 12.0f : 8.0f;
	Text_Draw( buf, x + w * 0.5f, y + h * 0.25f,
		FONT_DISPLAY, charSize, color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
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
	if ( !wiredHud || !wiredHud->valid ) return;
	int score = wiredHud->persistant[PERS_SCORE];
	char buf[16];
	Com_sprintf( buf, sizeof( buf ), "%d", score );
	vec4_t color = { 1, 1, 1, 1 };
	float charSize = h > 20.0f ? 12.0f : 8.0f;
	Text_Draw( buf, x + w * 0.5f, y + h * 0.25f,
		FONT_DISPLAY, charSize, color, TEXT_ALIGN_CENTER, TEXT_DROPSHADOW );
}

// ── P2 ownerdraw renderers ───────────────────────────────────────────

static void WiredOD_BlueScore( float x, float y, float w, float h, vec4_t itemColor ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	char buf[16];
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->scores2 );
	vec4_t color = { 0.3f, 0.5f, 1.0f, 1.0f };
	float charSize = h > 20.0f ? 12.0f : 8.0f;
	Text_Draw( buf, x, y + h * 0.25f,
		FONT_DISPLAY, charSize, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
}

static void WiredOD_RedScore( float x, float y, float w, float h, vec4_t itemColor ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	char buf[16];
	Com_sprintf( buf, sizeof( buf ), "%d", wiredHud->scores1 );
	vec4_t color = { 1.0f, 0.3f, 0.3f, 1.0f };
	float charSize = h > 20.0f ? 12.0f : 8.0f;
	Text_Draw( buf, x, y + h * 0.25f,
		FONT_DISPLAY, charSize, color, TEXT_ALIGN_LEFT, TEXT_DROPSHADOW );
}

static void WiredOD_Killer( float x, float y, float w, float h, vec4_t itemColor ) {
	if ( !wiredHud || !wiredHud->valid ) return;
	if ( !wiredHud->killerName[0] ) return;

	{
		char msg[256];
		vec4_t color;
		float charSize;

		Com_sprintf( msg, sizeof( msg ), "Fragged by %s", wiredHud->killerName );
		Vector4Set( color, 1.0f, 1.0f, 0.2f, 1.0f );
		charSize = h > 20.0f ? 12.0f : 8.0f;

		Text_Draw( msg,
			x + w * 0.5f,
			y + h * 0.25f,
			FONT_DISPLAY,
			charSize,
			color,
			TEXT_ALIGN_CENTER,
			TEXT_DROPSHADOW );
	}
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
	// Draw horizontal scanlines every 48 pixels across the full
	// screen.  Uses the item's forecolor for tint and alpha,
	// falling back to white 3% alpha if no color is provided.
	vec4_t lineColor;
	if ( itemColor && ( itemColor[0] > 0.0f || itemColor[1] > 0.0f || itemColor[2] > 0.0f ) ) {
		Vector4Copy( itemColor, lineColor );
	} else {
		Vector4Set( lineColor, 1, 1, 1, 0.03f );
	}
	float lineH   = 1.0f;   // 1 virtual pixel height
	float spacing = 48.0f;
	float sy;

	for ( sy = 0; sy < (float)cls.glconfig.vidHeight; sy += spacing ) {
		WUI_FillRect( 0, sy, (float)cls.glconfig.vidWidth, lineH, lineColor );
	}
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

static qhandle_t wiredOD_headModel = 0;
static char      wiredOD_headModelName[MAX_QPATH];

static void WiredOD_PlayerModel( float x, float y, float w, float h, vec4_t itemColor ) {
	refdef_t    refdef;
	refEntity_t ent;
	char        model[MAX_QPATH];
	char        headPath[MAX_QPATH];
	vec3_t      angles;
	char       *slash;

	// read model cvar (format: "model/skin")
	Cvar_VariableStringBuffer( "model", model, sizeof( model ) );
	slash = strchr( model, '/' );
	if ( slash ) *slash = '\0';
	if ( !model[0] ) Q_strncpyz( model, "sarge", sizeof( model ) );

	// cache head model handle
	Com_sprintf( headPath, sizeof( headPath ), "models/players/%s/head.md3", model );
	if ( Q_stricmp( headPath, wiredOD_headModelName ) ) {
		Q_strncpyz( wiredOD_headModelName, headPath, sizeof( wiredOD_headModelName ) );
		wiredOD_headModel = re.RegisterModel( headPath );
	}
	if ( !wiredOD_headModel ) return;

	// coordinates are already real screen pixels
	Com_Memset( &refdef, 0, sizeof( refdef ) );
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
	Com_Memset( &ent, 0, sizeof( ent ) );
	ent.reType  = RT_MODEL;
	ent.hModel  = wiredOD_headModel;
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
static unsigned long long wired_odWarnedBits[2] = {0, 0};  // 128 bits = IDs 0-127

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
		if ( !( wired_odWarnedBits[word] & bit ) ) {
			wired_odWarnedBits[word] |= bit;
			Com_DPrintf( "WiredUI: unimplemented ownerdraw %d\n", ownerDraw );
		}
	}
}

#endif // FEAT_WIRED_UI
