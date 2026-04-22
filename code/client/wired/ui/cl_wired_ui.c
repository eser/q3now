/*
cl_wired_ui.c — Wired UI: unified menu/HUD system implementation
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_attract.h"
#include "cl_wired_hud.h"
#include "cl_wired_fonts.h"
#include "cl_wired_text.h"
#include "cl_wired_draw.h"
#include "cl_wired_background.h"
#include "cl_wired_store.h"
#include "cl_wired_theme.h"
#include "../../../qcommon/menudef.h"

#include <lua.h>
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"

#if FEAT_WIRED_UI

#define WUI_DEFAULT_FONT_SIZE  14.0f
#define WIRED_UI_STATE_FILE     "wired_ui_state.dat"
#define WIRED_UI_STATE_MAGIC    0x57554953
#define WIRED_UI_STATE_VERSION  1

// from cl_wired_hud_registry.c
extern void     WiredHud_DestroyAllElements( void );
extern int      WiredHud_GetElementCount( void );
// from cl_wired_hud.c
extern void     WiredHud_LoadFromMenus( void );

// forward declarations
static void WiredUI_ApplyAnchor( wiredMenuDef_t *menu, float menuW, float menuH,
                                  float *outX, float *outY );
static void WiredUI_DrawWindowBorder( float x, float y, float w, float h,
	int border, float borderSize, const vec4_t borderColor, float alphaScale );
static void WiredUI_DrawModelItem( wiredItemDef_t *item, float x, float y, float w, float h );
static qboolean WiredUI_ItemVisibleByBindRules( wiredItemDef_t *item );
static qboolean WiredUI_ItemShouldRender( wiredItemDef_t *item );
static qboolean WiredUI_ItemCanFocus( wiredItemDef_t *item );
static qboolean WiredUI_StateListContainsValue( const char *list, const char *value );
static qboolean WiredUI_IsPersistedStateKey( const char *key );
static qboolean WiredUI_CallLuaStoreFunction( const char *functionName );
static qhandle_t wui_gradientBarShader;

typedef struct {
	const char *key;
	const char *defaultValue;
} wiredUiStateDefault_t;

typedef struct {
	int magic;
	int version;
	int count;
} wiredUiStateFileHeader_t;

typedef struct {
	unsigned short keyLen;
	unsigned short valueLen;
} wiredUiStateFileEntryHeader_t;

typedef struct {
	int count;
} wiredUiStateCountCtx_t;

typedef struct {
	fileHandle_t f;
	int wrote;
} wiredUiStateWriteCtx_t;

static const wiredUiStateDefault_t wui_uiStateDefaults[] = {
	{ "ui_netSource", "0" },
	{ "ui_browserGameType", "0" },
	{ "ui_browserShowFull", "1" },
	{ "ui_browserShowEmpty", "1" },
	{ "ui_browserMaxPing", "0" },
	{ "ui_browserStatus", "" },
	{ "ui_selectedServerAddr", "" },
	{ "ui_selectedServerName", "" },
	{ "ui_selectedMap", "" },
	{ "ui_currentNetMap", "0" },
	{ "ui_mapLevelshot", "" },
	{ "ui_mapPoolStatus", "Single map (no rotation)" },
	{ "ui_mapPoolAction", "Add to Pool" },
	{ "ui_favMapAction", "Favorite" },
	{ "ui_favoriteMaps", "" },
	{ "ui_selectedDemo", "" },
	{ "ui_selectedMod", "" },
	{ "ui_confirmText", "" },
	{ "ui_confirmAction", "" },
	{ "ui_voteTimelimit", "20" },
	{ "ui_voteScorelimit", "0" },
	{ "ui_botCount", "0" },
	{ "ui_botName", "" },
	{ "ui_botTeam", "0" },
	{ "ui_dedicated", "0" },
	{ "ui_netGameType", "0" },
	{ "ui_globalpreset", "0" },
	{ "ui_mousePitch", "0" },
	{ "ui_lastRefreshDate", "" },
	{ "ui_Name", "" },
	{ "ui_specifyAddress", "" },
	{ NULL, NULL }
};

qboolean WiredUI_IsStoreStateKey( const char *key ) {
	const wiredUiStateDefault_t *it;

	if ( !key || !key[0] ) {
		return qfalse;
	}

	for ( it = wui_uiStateDefaults; it->key; it++ ) {
		if ( !Q_stricmp( key, it->key ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

static qboolean WiredUI_IsPersistedStateKey( const char *key ) {
	if ( !key || !key[0] ) {
		return qfalse;
	}

	if ( !Q_stricmp( key, "ui_theme" ) ) {
		return qfalse;
	}

	if ( WiredUI_IsStoreStateKey( key ) ) {
		return qtrue;
	}

	return ( Q_stricmpn( key, "ui_", 3 ) == 0 );
}

static void WiredUI_CountPersistedStateEntry( wuiStoreEntry_t *entry, void *userData ) {
	wiredUiStateCountCtx_t *ctx = (wiredUiStateCountCtx_t *)userData;

	if ( !entry || !ctx ) {
		return;
	}

	if ( !WiredUI_IsPersistedStateKey( entry->key ) ) {
		return;
	}

	ctx->count++;
}

static void WiredUI_WritePersistedStateEntry( wuiStoreEntry_t *entry, void *userData ) {
	wiredUiStateWriteCtx_t *ctx = (wiredUiStateWriteCtx_t *)userData;

	if ( !entry || !ctx ) {
		return;
	}

	if ( !WiredUI_IsPersistedStateKey( entry->key ) ) {
		return;
	}

	int keyLen = (int)strlen( entry->key );
	int valueLen = (int)strlen( entry->text );
	wiredUiStateFileEntryHeader_t eh;
	if ( keyLen <= 0 || keyLen > 65535 || valueLen < 0 || valueLen > 65535 ) {
		return;
	}

	eh.keyLen = (unsigned short)keyLen;
	eh.valueLen = (unsigned short)valueLen;
	FS_Write( &eh, sizeof( eh ), ctx->f );
	FS_Write( entry->key, keyLen, ctx->f );
	if ( valueLen > 0 ) {
		FS_Write( entry->text, valueLen, ctx->f );
	}
	ctx->wrote++;
}

void WiredUI_SaveState( void ) {
	wiredUiStateCountCtx_t countCtx;
	countCtx.count = 0;
	WiredStore_ForEach( NULL, WiredUI_CountPersistedStateEntry, &countCtx );

	fileHandle_t f = FS_FOpenFileWrite( WIRED_UI_STATE_FILE );
	if ( f == FS_INVALID_HANDLE ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: failed to save UI state (%s)\n", WIRED_UI_STATE_FILE );
		return;
	}

	wiredUiStateFileHeader_t header;
	header.magic = WIRED_UI_STATE_MAGIC;
	header.version = WIRED_UI_STATE_VERSION;
	header.count = countCtx.count;

	FS_Write( &header, sizeof( header ), f );

	wiredUiStateWriteCtx_t writeCtx;
	writeCtx.f = f;
	writeCtx.wrote = 0;
	WiredStore_ForEach( NULL, WiredUI_WritePersistedStateEntry, &writeCtx );

	FS_FCloseFile( f );

	Com_DPrintf( "WiredUI: saved %d UI state entries\n", writeCtx.wrote );
}

void WiredUI_LoadState( void ) {
	fileHandle_t f = FS_INVALID_HANDLE;
	int len = FS_FOpenFileRead( WIRED_UI_STATE_FILE, &f, qtrue );
	if ( f == FS_INVALID_HANDLE || len <= 0 ) {
		if ( f != FS_INVALID_HANDLE ) {
			FS_FCloseFile( f );
		}
		return;
	}

	byte *data = (byte *)Z_Malloc( len );
	if ( !data ) {
		FS_FCloseFile( f );
		return;
	}

	if ( FS_Read( data, len, f ) != len ) {
		Z_Free( data );
		FS_FCloseFile( f );
		return;
	}
	FS_FCloseFile( f );

	wiredUiStateFileHeader_t header;
	if ( len < (int)sizeof( header ) ) {
		Z_Free( data );
		return;
	}

	memcpy( &header, data, sizeof( header ) );
	if ( header.magic != WIRED_UI_STATE_MAGIC ||
	     header.version != WIRED_UI_STATE_VERSION ||
	     header.count < 0 ||
	     header.count > WUI_STORE_MAX_ENTRIES ) {
		Z_Free( data );
		return;
	}

	const byte *p = data + sizeof( header );
	const byte *end = data + len;
	int loadedCount = 0;

	for ( int i = 0; i < header.count; i++ ) {
		wiredUiStateFileEntryHeader_t eh;
		char key[128];
		char value[256];

		if ( end - p < (int)sizeof( eh ) ) {
			break;
		}

		memcpy( &eh, p, sizeof( eh ) );
		p += sizeof( eh );

		if ( eh.keyLen == 0 || eh.keyLen >= sizeof( key ) ) {
			if ( end - p < eh.keyLen + eh.valueLen ) {
				break;
			}
			p += eh.keyLen + eh.valueLen;
			continue;
		}

		if ( end - p < eh.keyLen + eh.valueLen ) {
			break;
		}

		memcpy( key, p, eh.keyLen );
		key[eh.keyLen] = '\0';
		p += eh.keyLen;

		int valueCopyLen = eh.valueLen;
		if ( valueCopyLen >= (int)sizeof( value ) ) {
			valueCopyLen = sizeof( value ) - 1;
		}
		if ( valueCopyLen > 0 ) {
			memcpy( value, p, valueCopyLen );
		}
		value[valueCopyLen] = '\0';
		p += eh.valueLen;

		if ( !WiredUI_IsPersistedStateKey( key ) ) {
			continue;
		}

		wuiStoreEntry_t *entry = WiredStore_Set( key );
		if ( !entry ) {
			continue;
		}

		Q_strncpyz( entry->text, value, sizeof( entry->text ) );
		entry->value = (float)atof( entry->text );
		entry->flags &= ~WUI_STORE_FLAG_DIRTY;
		loadedCount++;
	}

	Z_Free( data );

	Com_DPrintf( "WiredUI: loaded %d UI state entries\n", loadedCount );
}

static qboolean WiredUI_CallLuaStoreFunction( const char *functionName ) {
	if ( !functionName || !functionName[0] ) {
		return qfalse;
	}

	lua_State *L = WiredScript_GetState();
	if ( !L ) {
		return qfalse;
	}

	lua_getglobal( L, "store" );
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		return qfalse;
	}

	lua_getfield( L, -1, functionName );
	if ( !lua_isfunction( L, -1 ) ) {
		lua_pop( L, 2 );
		return qfalse;
	}

	if ( lua_pcall( L, 0, 0, 0 ) != 0 ) {
		const char *err = lua_tostring( L, -1 );
		Com_Printf( S_COLOR_YELLOW "WiredUI: Lua store.%s failed: %s\n",
			functionName, err ? err : "unknown" );
		lua_pop( L, 1 );
		lua_pop( L, 1 );
		return qfalse;
	}

	lua_pop( L, 1 );
	return qtrue;
}

static const char *WiredUI_StateDefaultValue( const char *key ) {
	for ( const wiredUiStateDefault_t *it = wui_uiStateDefaults; it->key; it++ ) {
		if ( !Q_stricmp( key, it->key ) ) {
			return it->defaultValue ? it->defaultValue : "";
		}
	}
	return "";
}

void WiredUI_StateGetString( const char *key, char *out, int outSize ) {
	if ( !out || outSize <= 0 ) {
		return;
	}

	out[0] = '\0';
	if ( !key || !key[0] ) {
		return;
	}

	if ( WiredUI_IsStoreStateKey( key ) ) {
		wuiStoreEntry_t *entry = WiredStore_Get( key );
		if ( entry ) {
			Q_strncpyz( out, entry->text, outSize );
			if ( !out[0] ) {
				Q_strncpyz( out, WiredUI_StateDefaultValue( key ), outSize );
			}
			return;
		}

		for ( const wiredUiStateDefault_t *it = wui_uiStateDefaults; it->key; it++ ) {
			if ( !Q_stricmp( key, it->key ) ) {
				Q_strncpyz( out, it->defaultValue ? it->defaultValue : "", outSize );
				return;
			}
		}
	}

	Cvar_VariableStringBuffer( key, out, outSize );
	if ( !out[0] && WiredUI_IsStoreStateKey( key ) ) {
		Q_strncpyz( out, WiredUI_StateDefaultValue( key ), outSize );
	}
}

int WiredUI_StateGetInt( const char *key ) {
	char buf[256];
	WiredUI_StateGetString( key, buf, sizeof( buf ) );
	return atoi( buf );
}

float WiredUI_StateGetFloat( const char *key ) {
	char buf[256];
	WiredUI_StateGetString( key, buf, sizeof( buf ) );
	return (float)atof( buf );
}

void WiredUI_StateSetString( const char *key, const char *value ) {
	if ( !key || !key[0] ) {
		return;
	}

	if ( WiredUI_IsStoreStateKey( key ) ) {
		wuiStoreEntry_t *entry = WiredStore_Set( key );
		if ( entry ) {
			Q_strncpyz( entry->text, value ? value : "", sizeof( entry->text ) );
			entry->value = (float)atof( entry->text );
			entry->flags |= WUI_STORE_FLAG_DIRTY;
		}
		return;
	}

	Cvar_Set( key, value ? value : "" );
}

void WiredUI_StateSetInt( const char *key, int value ) {
	WiredUI_StateSetString( key, va( "%d", value ) );
}

void WiredUI_StateSetFloat( const char *key, float value ) {
	WiredUI_StateSetString( key, va( "%g", value ) );
}

static qboolean WiredUI_StateListContainsValue( const char *list, const char *value ) {
	if ( !list || !list[0] ) {
		return qfalse;
	}

	if ( !value ) {
		value = "";
	}

	int intVal = atoi( value );
	char intBuf[16];
	Com_sprintf( intBuf, sizeof( intBuf ), "%d", intVal );

	char token[256];
	const char *p = list;
	while ( *p ) {
		int i = 0;
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		while ( *p && *p != ' ' && i < (int)sizeof( token ) - 1 ) {
			token[i++] = *p++;
		}
		token[i] = '\0';

		if ( !Q_stricmp( token, value ) || !Q_stricmp( token, intBuf ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

static void WiredUI_SetTeamWindowColor( vec4_t outColor, const vec4_t fallbackColor ) {
	if ( wiredHud && wiredHud->valid ) {
		if ( wiredHud->isOurTeamBlue ) {
			Vector4Set( outColor, 0.20f, 0.35f, 0.95f, fallbackColor ? fallbackColor[3] : 1.0f );
		} else {
			Vector4Set( outColor, 0.95f, 0.20f, 0.20f, fallbackColor ? fallbackColor[3] : 1.0f );
		}
	} else {
		if ( fallbackColor ) Vector4Copy( fallbackColor, outColor );
		else Vector4Set( outColor, 1.0f, 1.0f, 1.0f, 1.0f );
	}
}

static void WiredUI_DrawWindowBorder( float x, float y, float w, float h,
	int border, float borderSize, const vec4_t borderColor, float alphaScale ) {
	vec4_t bc;
	float bs = borderSize > 0.0f ? borderSize : 1.0f;

	if ( border == WINDOW_BORDER_NONE || !borderColor || borderColor[3] <= 0.0f ) {
		return;
	}

	Vector4Copy( borderColor, bc );
	bc[3] *= alphaScale;

	switch ( border ) {
		case WINDOW_BORDER_FULL:
			WUI_FillRect( x, y, w, bs, bc );
			WUI_FillRect( x, y + h - bs, w, bs, bc );
			WUI_FillRect( x, y, bs, h, bc );
			WUI_FillRect( x + w - bs, y, bs, h, bc );
			break;
		case WINDOW_BORDER_HORZ:
			WUI_FillRect( x, y, w, bs, bc );
			WUI_FillRect( x, y + h - bs, w, bs, bc );
			break;
		case WINDOW_BORDER_VERT:
			WUI_FillRect( x, y, bs, h, bc );
			WUI_FillRect( x + w - bs, y, bs, h, bc );
			break;
		case WINDOW_BORDER_KCGRADIENT:
			if ( wui_gradientBarShader ) {
				re.SetColor( bc );
				WUI_DrawPic( x, y, w, bs, wui_gradientBarShader );
				WUI_DrawPic( x, y + h - bs, w, bs, wui_gradientBarShader );
				re.SetColor( NULL );
			} else {
				WUI_FillRect( x, y, w, bs, bc );
				WUI_FillRect( x, y + h - bs, w, bs, bc );
			}
			break;
		default:
			WUI_FillRect( x, y, w, bs, bc );
			WUI_FillRect( x, y + h - bs, w, bs, bc );
			WUI_FillRect( x, y, bs, h, bc );
			WUI_FillRect( x + w - bs, y, bs, h, bc );
			break;
	}
}

static void WiredUI_DrawModelItem( wiredItemDef_t *item, float x, float y, float w, float h ) {
	if ( !item->assetModel[0] ) {
		return;
	}

	if ( !item->modelHandle ) {
		item->modelHandle = re.RegisterModel( item->assetModel );
	}
	if ( !item->modelHandle ) {
		return;
	}

	if ( item->assetShader[0] && !item->modelShaderHandle ) {
		item->modelShaderHandle = re.RegisterShaderNoMip( item->assetShader );
	}

	refdef_t refdef;
	refEntity_t ent;
	memset( &refdef, 0, sizeof( refdef ) );
	memset( &ent, 0, sizeof( ent ) );

	refdef.rdflags = RDF_NOWORLDMODEL;
	refdef.x = (int)x;
	refdef.y = (int)y;
	refdef.width = (int)w;
	refdef.height = (int)h;
	if ( refdef.width < 1 || refdef.height < 1 ) return;

	refdef.fov_x = item->modelFovX > 0.0f ? item->modelFovX : 40.0f;
	refdef.fov_y = item->modelFovY > 0.0f ? item->modelFovY :
		( refdef.fov_x * (float)refdef.height / (float)refdef.width );
	refdef.time = cls.realtime;
	AxisClear( refdef.viewaxis );

	VectorSet( refdef.vieworg,
		item->modelOrigin[0] != 0.0f ? item->modelOrigin[0] : 80.0f,
		item->modelOrigin[1],
		item->modelOrigin[2] );

	ent.reType = RT_MODEL;
	ent.hModel = item->modelHandle;
	if ( item->modelShaderHandle ) {
		ent.customShader = item->modelShaderHandle;
	}

	VectorSet( ent.origin, 0.0f, 0.0f, 0.0f );
	vec3_t angles;
	angles[PITCH] = 0.0f;
	angles[YAW] = item->modelAngle + item->modelRotation * ( (float)cls.realtime / 1000.0f );
	angles[ROLL] = 0.0f;
	AnglesToAxis( angles, ent.axis );
	VectorCopy( ent.origin, ent.lightingOrigin );
	ent.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;

	re.ClearScene();
	re.AddRefEntityToScene( &ent, qfalse );
	re.RenderScene( &refdef );
}

static void WiredUI_DrawMenuBackground( wiredMenuDef_t *menu,
	float x, float y, float w, float h, float alphaScale ) {
	if ( menu->style == WINDOW_STYLE_CINEMATIC && menu->cinematicHandle >= 0 ) {
		CIN_SetExtents( menu->cinematicHandle, (int)x, (int)y, (int)w, (int)h );
		CIN_RunCinematic( menu->cinematicHandle );
		CIN_DrawCinematic( menu->cinematicHandle );
		return;
	}

	if ( menu->style == WINDOW_STYLE_SHADER && menu->background[0] ) {
		qhandle_t bgShader = re.RegisterShaderNoMip( menu->background );
		if ( bgShader ) {
			re.SetColor( NULL );
			WUI_DrawPic( x, y, w, h, bgShader );
			return;
		}
	}

	if ( menu->style == WINDOW_STYLE_GRADIENT && menu->backcolor[3] > 0.0f ) {
		vec4_t bc;
		Vector4Copy( menu->backcolor, bc );
		bc[3] *= alphaScale;
		WUI_FillRect( x, y, w, h, bc );
		if ( wui_gradientBarShader ) {
			vec4_t gc;
			Vector4Copy( menu->backcolor, gc );
			gc[3] *= 0.5f * alphaScale;
			re.SetColor( gc );
			WUI_DrawPic( x, y, w, h, wui_gradientBarShader );
			re.SetColor( NULL );
		}
		return;
	}

	if ( menu->style == WINDOW_STYLE_FILLED && menu->backcolor[3] > 0.0f ) {
		vec4_t bc;
		Vector4Copy( menu->backcolor, bc );
		bc[3] *= alphaScale;
		WUI_FillRect( x, y, w, h, bc );
		return;
	}

	if ( menu->style == WINDOW_STYLE_TEAMCOLOR ) {
		vec4_t tc;
		WiredUI_SetTeamWindowColor( tc, menu->backcolor );
		tc[3] *= alphaScale;
		WUI_FillRect( x, y, w, h, tc );
		return;
	}

	if ( menu->style == WINDOW_STYLE_EMPTY ) {
		return;
	}

	{
		vec4_t bgColor = { 0.1f, 0.1f, 0.15f, 1.0f };
		bgColor[3] *= alphaScale;
		WUI_FillRect( x, y, w, h, bgColor );
	}
}

static void WiredUI_DrawItemBackground( wiredItemDef_t *item,
	float x, float y, float w, float h, float alphaScale ) {
	if ( item->style == WINDOW_STYLE_SHADER && item->background[0] ) {
		qhandle_t itemBg = re.RegisterShaderNoMip( item->background );
		if ( itemBg ) {
			vec4_t shaderColor;
			if ( item->forecolor[0] > 0.0f || item->forecolor[1] > 0.0f ||
			     item->forecolor[2] > 0.0f || item->forecolor[3] > 0.0f ) {
				Vector4Copy( item->forecolor, shaderColor );
			} else {
				Vector4Set( shaderColor, 1, 1, 1, 1 );
			}
			shaderColor[3] *= alphaScale;
			re.SetColor( shaderColor );
			WUI_DrawPic( x, y, w, h, itemBg );
			re.SetColor( NULL );
		}
		return;
	}

	if ( item->style == WINDOW_STYLE_GRADIENT && item->backcolor[3] > 0.0f ) {
		vec4_t bc;
		Vector4Copy( item->backcolor, bc );
		bc[3] *= alphaScale;
		WUI_FillRect( x, y, w, h, bc );
		if ( wui_gradientBarShader ) {
			vec4_t gc;
			Vector4Copy( item->backcolor, gc );
			gc[3] *= 0.5f * alphaScale;
			re.SetColor( gc );
			WUI_DrawPic( x, y, w, h, wui_gradientBarShader );
			re.SetColor( NULL );
		}
		return;
	}

	if ( item->style == WINDOW_STYLE_FILLED && item->backcolor[3] > 0.0f ) {
		vec4_t bc;
		Vector4Copy( item->backcolor, bc );
		bc[3] *= alphaScale;
		WUI_FillRect( x, y, w, h, bc );
		return;
	}

	if ( item->style == WINDOW_STYLE_TEAMCOLOR ) {
		vec4_t tc;
		WiredUI_SetTeamWindowColor( tc, item->backcolor );
		tc[3] *= alphaScale;
		WUI_FillRect( x, y, w, h, tc );
	}
}

static qboolean WiredUI_ItemVisibleByCvarRules( wiredItemDef_t *item ) {
	if ( item->cvarTest[0] ) {
		char testBuf[256];

		WiredUI_StateGetString( item->cvarTest, testBuf, sizeof( testBuf ) );

		if ( item->showCvar[0] ) {
			if ( !WiredUI_StateListContainsValue( item->showCvar, testBuf ) ) return qfalse;
		}
		if ( item->hideCvar[0] ) {
			if ( WiredUI_StateListContainsValue( item->hideCvar, testBuf ) ) return qfalse;
		}
	}

	if ( item->enableCvar[0] || item->disableCvar[0] ) {
		char testBuf[256];
		qboolean enabled = qtrue;

		if ( item->cvarTest[0] ) WiredUI_StateGetString( item->cvarTest, testBuf, sizeof( testBuf ) );
		else if ( item->cvar[0] ) WiredUI_StateGetString( item->cvar, testBuf, sizeof( testBuf ) );
		else testBuf[0] = '\0';

		if ( item->enableCvar[0] ) {
			enabled = WiredUI_StateListContainsValue( item->enableCvar, testBuf );
		}
		if ( item->disableCvar[0] ) {
			if ( WiredUI_StateListContainsValue( item->disableCvar, testBuf ) ) {
				enabled = qfalse;
			}
		}

		if ( !enabled ) return qfalse;
	}

	return qtrue;
}

static qboolean WiredUI_ItemVisibleByBindRules( wiredItemDef_t *item ) {
	if ( item->showBind[0] ) {
		wuiStoreEntry_t *vis = WiredStore_Get( item->showBind );
		if ( !vis || ( !vis->text[0] && vis->value == 0.0f ) ) {
			return qfalse;
		}
	}

	if ( item->hideBind[0] ) {
		wuiStoreEntry_t *vis = WiredStore_Get( item->hideBind );
		if ( vis && ( vis->text[0] || vis->value != 0.0f ) ) {
			return qfalse;
		}
	}

	return qtrue;
}

static qboolean WiredUI_ItemShouldRender( wiredItemDef_t *item ) {
	if ( !item->visible ) {
		return qfalse;
	}

	if ( !WiredUI_ItemVisibleByBindRules( item ) ) {
		return qfalse;
	}

	if ( !WiredUI_ItemVisibleByCvarRules( item ) ) {
		return qfalse;
	}

	if ( item->ownerdrawFlag && !WiredUI_OwnerDrawVisible( item->ownerdrawFlag ) ) {
		return qfalse;
	}

	return qtrue;
}

static qboolean WiredUI_ItemCanFocus( wiredItemDef_t *item ) {
	if ( item->decoration || item->notselectable ) {
		return qfalse;
	}

	return WiredUI_ItemShouldRender( item );
}

// ── symbol registry ───────────────────────────────────────────────────

#define WIRED_MAX_SYMBOLS  256

typedef struct {
	char                    name[64];
	wiredSymbolCallback_t   callback;
	void                   *userData;
	qboolean                active;
} wiredSymbol_t;

static wiredSymbol_t  wui_symbols[WIRED_MAX_SYMBOLS];
static int            wui_numSymbols = 0;

// ── element registry ──────────────────────────────────────────────────

#define WIRED_MAX_ELEMENTS  256

typedef struct {
	char                     name[64];
	wiredElementCreate_t     create;
	wiredElementRoutine_t    routine;
	wiredElementDestroy_t    destroy;
	qboolean                 active;
} wiredElement_t;

static wiredElement_t  wui_elements[WIRED_MAX_ELEMENTS];
static int             wui_numElements = 0;

// ── populate callback registry ───────────────────────────────────────
// Used by dynamic-MULTI items (populateCallback "name" in .wmenu) to fill
// the option list at render time. Implementation lives here so the
// registry survives Wired UI reloads. Callbacks themselves are typically
// registered from cl_wired_populate.c.

#define WIRED_MAX_POPULATE_CALLBACKS  32

typedef struct {
	char                     name[64];
	wuiPopulateCallback_t    fn;
	qboolean                 active;
} wiredPopulateEntry_t;

static wiredPopulateEntry_t wui_populateCallbacks[WIRED_MAX_POPULATE_CALLBACKS];
static int                   wui_numPopulateCallbacks = 0;

// ── feeder registry ───────────────────────────────────────────────────

typedef struct {
	int                       feederID;
	wiredFeederCount_t        count;
	wiredFeederItemText_t     itemText;
	wiredFeederSelection_t    selection;
	qboolean                  active;
} wiredFeeder_t;

static wiredFeeder_t  wui_feeders[WIRED_MAX_FEEDERS];
static int            wui_numFeeders = 0;

// ── menu interaction sounds ──────────────────────────────────────────
static sfxHandle_t    wui_sfxFocus;     // item gains focus (hover/arrow key)
static sfxHandle_t    wui_sfxAction;    // button click / action execution
static sfxHandle_t    wui_sfxMenuOpen;  // menu push onto stack
static sfxHandle_t    wui_sfxMenuClose; // menu pop from stack

// ── cursor shader ────────────────────────────────────────────────────
static qhandle_t      wui_cursorShader;

// ── asset globals (parsed from assetGlobalDef) ──────────────────────
static wiredAssetGlobals_t wui_assetGlobals;
static qhandle_t           wui_gradientBarShader;

wiredAssetGlobals_t *WiredUI_GetAssetGlobals( void ) {
	return &wui_assetGlobals;
}

void WiredUI_GetMapRotation( char *buf, int size ) {
	Cvar_VariableStringBuffer( "g_maprotation", buf, size );
}

void WiredUI_ResetAssetGlobalsDefaults( void ) {
	memset( &wui_assetGlobals, 0, sizeof( wui_assetGlobals ) );

	Q_strncpyz( wui_assetGlobals.cursor, "ui/assets/cursor", sizeof( wui_assetGlobals.cursor ) );
	Q_strncpyz( wui_assetGlobals.gradientBar, "ui/assets/gradientbar2.tga", sizeof( wui_assetGlobals.gradientBar ) );

	Q_strncpyz( wui_assetGlobals.defaultSerifFontName, "sansman", sizeof( wui_assetGlobals.defaultSerifFontName ) );
	Q_strncpyz( wui_assetGlobals.defaultSerifFontItalicName, "sansman-italic", sizeof( wui_assetGlobals.defaultSerifFontItalicName ) );
	Q_strncpyz( wui_assetGlobals.defaultSansFontName, "oxanium", sizeof( wui_assetGlobals.defaultSansFontName ) );
	Q_strncpyz( wui_assetGlobals.defaultSansFontMediumName, "oxanium-medium", sizeof( wui_assetGlobals.defaultSansFontMediumName ) );
	Q_strncpyz( wui_assetGlobals.defaultMonoFontName, "sharetechmono", sizeof( wui_assetGlobals.defaultMonoFontName ) );

	wui_assetGlobals.fadeClamp = 1.0f;
	wui_assetGlobals.fadeCycle = 1;
	wui_assetGlobals.fadeAmount = 0.2f;
	Vector4Set( wui_assetGlobals.shadowColor, 0.1f, 0.1f, 0.1f, 0.25f );
	Q_strncpyz( wui_assetGlobals.focusSound, "sound/misc/menu2.opus", sizeof( wui_assetGlobals.focusSound ) );
	Vector4Set( wui_assetGlobals.focusColor, 1.0f, 0.75f, 0.0f, 1.0f );
	wui_assetGlobals.shadowX = 1.0f;
	wui_assetGlobals.shadowY = 1.0f;
	Vector4Set( wui_assetGlobals.gradientBarColor, 0, 0, 0, 0 );
}

// Theme selection is handled inside scripts/menus.lua via the global metatable
// cvar bridge (ui_theme readable as a Lua global). No manifest path function needed.

void WiredUI_RegisterFeeder( int feederID, wiredFeederCount_t count,
                              wiredFeederItemText_t itemText,
                              wiredFeederSelection_t selection ) {
	// update existing
	for ( int i = 0; i < wui_numFeeders; i++ ) {
		if ( wui_feeders[i].active && wui_feeders[i].feederID == feederID ) {
			wui_feeders[i].count = count;
			wui_feeders[i].itemText = itemText;
			wui_feeders[i].selection = selection;
			return;
		}
	}
	if ( wui_numFeeders >= WIRED_MAX_FEEDERS ) return;
	wui_feeders[wui_numFeeders].feederID = feederID;
	wui_feeders[wui_numFeeders].count = count;
	wui_feeders[wui_numFeeders].itemText = itemText;
	wui_feeders[wui_numFeeders].selection = selection;
	wui_feeders[wui_numFeeders].active = qtrue;
	wui_numFeeders++;
}

int WiredUI_FeederCount( int feederID ) {
	for ( int i = 0; i < wui_numFeeders; i++ ) {
		if ( wui_feeders[i].active && wui_feeders[i].feederID == feederID && wui_feeders[i].count ) {
			return wui_feeders[i].count( feederID );
		}
	}
	return 0;
}

const char *WiredUI_FeederItemText( int feederID, int index, int column ) {
	for ( int i = 0; i < wui_numFeeders; i++ ) {
		if ( wui_feeders[i].active && wui_feeders[i].feederID == feederID && wui_feeders[i].itemText ) {
			return wui_feeders[i].itemText( feederID, index, column );
		}
	}
	return "";
}

void WiredUI_FeederSelection( int feederID, int index ) {
	for ( int i = 0; i < wui_numFeeders; i++ ) {
		if ( wui_feeders[i].active && wui_feeders[i].feederID == feederID && wui_feeders[i].selection ) {
			wui_feeders[i].selection( feederID, index );
			return;
		}
	}
}

// ── state ─────────────────────────────────────────────────────────────

static qboolean  wui_initialized = qfalse;
static int       wui_activeMenu = UIMENU_NONE;

// ── menu stack ────────────────────────────────────────────────────────
// Supports open/close navigation between screens (e.g., Main → Options → Video).
// Each entry is a menu name. ESC or "close" pops the stack.
// The bottom of the stack is always the root menu (main or ingame).

static char      wui_menuStack[WIRED_MENU_STACK_DEPTH][64];
static int       wui_menuStackDepth = 0;

// Pool/compositor health flag — set qtrue at the end of WiredUI_Init and
// on successful SafeReload; set qfalse in WiredUI_Shutdown and on failing
// SafeReload. Independent of cls.uiStarted so recovery can detect a dead
// pool even while cls.uiStarted hasn't been cleared yet.
static qboolean  wui_healthy = qfalse;

// Recovery failure timestamp — set by WiredUI_Activate when EnsureLoaded
// fails; read by cl_console.c to paint the red "reload failed" banner.
static int       wui_recoveryFailTime = 0;

// ── cursor ────────────────────────────────────────────────────────────

// ── key binding capture state ─────────────────────────────────────────
static qboolean  wui_waitingForKey = qfalse;
static wiredItemDef_t *wui_bindItem = NULL;

// ── slider drag state ────────────────────────────────────────────────
static qboolean       wui_sliderDragging = qfalse;
static wiredItemDef_t *wui_sliderDragItem = NULL;

// ── text field editing state ──────────────────────────────────────────
static qboolean       wui_editingField = qfalse;
static wiredItemDef_t *wui_editItem = NULL;
static int            wui_editCursorPos = 0;
static int            wui_editPaintOffset = 0;

static float     wui_cursorX = 320.0f;
static float     wui_cursorY = 240.0f;
static int       wui_focusItem = -1;     // index of focused item
static qboolean  wui_focusFromMouse = qfalse;  // qtrue if focus came from mouse hover

// ── tooltip delay ─────────────────────────────────────────────────────
#define WIRED_TOOLTIP_DELAY_MS  500   // ms before tooltip appears
static int       wui_tooltipStartTime = 0;  // realtime when hover started on tooltip item
static int       wui_tooltipFocusItem = -1; // item index that started the tooltip timer

// ── ui_testall dev command ────────────────────────────────────────────
static qboolean  testall_active = qfalse;
static int       testall_menuIndex = 0;
static int       testall_nextTime = 0;
static int       testall_delay = 2000;  // ms between menu switches

// ── double-click detection ───────────────────────────────────────────
#define WIRED_DOUBLECLICK_TIME  300   // ms
static int       wui_lastClickTime = 0;
static int       wui_lastClickRow = -1;
static float     wui_lastClickFeeder = 0;

typedef struct {
	int count;
	qboolean numericValues;
	const char *labels[WIRED_MAX_MULTI_CHOICES];
	const char *values[WIRED_MAX_MULTI_CHOICES];
	char numericBuf[WIRED_MAX_MULTI_CHOICES][32];
} wiredMultiOptions_t;

static qboolean        wui_multiDropdownOpen = qfalse;
static wiredItemDef_t *wui_multiDropdownItem = NULL;
static int             wui_multiDropdownHover = -1;
static int             wui_multiDropdownScroll = 0;

static void WiredUI_CloseMultiDropdown( void ) {
	wui_multiDropdownOpen = qfalse;
	wui_multiDropdownItem = NULL;
	wui_multiDropdownHover = -1;
	wui_multiDropdownScroll = 0;
}

static void WiredUI_GetMultiOptions( wiredItemDef_t *item, wiredMultiOptions_t *out ) {
	memset( out, 0, sizeof( *out ) );
	if ( !item ) return;

	if ( item->populateCallback[0] ) {
		wuiPopulateCallback_t pop = WiredUI_GetPopulateCallback( item->populateCallback );
		if ( pop ) {
			wuiPopulateResult_t res;
			memset( &res, 0, sizeof( res ) );
			pop( &res );
			if ( ( res.state == WUI_POPULATE_SUCCESS || res.state == WUI_POPULATE_PARTIAL ) &&
			     res.count > 0 && res.names && res.values ) {
				out->count = res.count > WIRED_MAX_MULTI_CHOICES ? WIRED_MAX_MULTI_CHOICES : res.count;
				for ( int i = 0; i < out->count; i++ ) {
					out->labels[i] = res.names[i] ? res.names[i] : "";
					out->values[i] = res.values[i] ? res.values[i] : "";
				}
			}
		}
		return;
	}

	if ( !item->multiData ) return;
	out->count = item->multiData->count > WIRED_MAX_MULTI_CHOICES ? WIRED_MAX_MULTI_CHOICES : item->multiData->count;
	out->numericValues = !item->multiData->isStringList;
	for ( int i = 0; i < out->count; i++ ) {
		out->labels[i] = item->multiData->labels[i];
		if ( item->multiData->isStringList ) {
			out->values[i] = item->multiData->strValues[i];
		} else {
			Com_sprintf( out->numericBuf[i], sizeof( out->numericBuf[i] ), "%g", item->multiData->floatValues[i] );
			out->values[i] = out->numericBuf[i];
		}
	}
}

static int WiredUI_FindMultiOptionIndex( wiredItemDef_t *item, const wiredMultiOptions_t *opts, const char *currentValue ) {
	if ( !opts || opts->count <= 0 || !currentValue ) return -1;
	for ( int i = 0; i < opts->count; i++ ) {
		if ( opts->numericValues ) {
			if ( fabs( atof( currentValue ) - atof( opts->values[i] ) ) < 0.0001 ) {
				return i;
			}
		} else {
			if ( !Q_stricmp( currentValue, opts->values[i] ) ) {
				return i;
			}
		}
	}
	return -1;
}

static void WiredUI_SetMultiOptionByIndex( wiredItemDef_t *item, const wiredMultiOptions_t *opts, int index ) {
	if ( !item || !opts || index < 0 || index >= opts->count ) return;
	if ( opts->values[index] ) {
		WiredUI_StateSetString( item->cvar, opts->values[index] );
	}
}

static qboolean WiredUI_GetMultiDropdownRect( wiredMenuDef_t *menu, wiredItemDef_t *item,
	int optionCount, float *x, float *y, float *w, float *h, float *rowH, int *visibleRows ) {
	float rx, ry, rw, rh;
	int rows;
	if ( !menu || !item || optionCount <= 0 ) return qfalse;

	rw = item->resolvedRect.w;
	rh = item->resolvedRect.h;
	if ( rh < 18.0f ) rh = 18.0f;

	rows = optionCount;
	if ( rows > 10 ) rows = 10;
	if ( rows < 1 ) rows = 1;

	rx = item->resolvedRect.x;
	ry = item->resolvedRect.y - menu->scrollOffset + item->resolvedRect.h + 2.0f;

	if ( ry + rh * rows > (float)cls.glconfig.vidHeight - 4.0f ) {
		ry = item->resolvedRect.y - menu->scrollOffset - ( rh * rows ) - 2.0f;
	}
	if ( ry < 4.0f ) ry = 4.0f;
	if ( rx + rw > (float)cls.glconfig.vidWidth - 4.0f ) rx = (float)cls.glconfig.vidWidth - rw - 4.0f;
	if ( rx < 4.0f ) rx = 4.0f;

	*x = rx;
	*y = ry;
	*w = rw;
	*h = rh * rows;
	*rowH = rh;
	*visibleRows = rows;
	return qtrue;
}

static void WiredUI_DrawMultiDropdown( wiredMenuDef_t *menu ) {
	wiredMultiOptions_t opts;
	float ddX, ddY, ddW, ddH, rowH;
	int visibleRows;
	if ( !wui_multiDropdownOpen || !menu || !wui_multiDropdownItem ) return;

	WiredUI_GetMultiOptions( wui_multiDropdownItem, &opts );
	if ( opts.count <= 0 ) {
		WiredUI_CloseMultiDropdown();
		return;
	}

	if ( !WiredUI_GetMultiDropdownRect( menu, wui_multiDropdownItem, opts.count,
		&ddX, &ddY, &ddW, &ddH, &rowH, &visibleRows ) ) {
		WiredUI_CloseMultiDropdown();
		return;
	}

	char currentValue[256];
	WiredUI_StateGetString( wui_multiDropdownItem->cvar, currentValue, sizeof( currentValue ) );
	int selectedIndex = WiredUI_FindMultiOptionIndex( wui_multiDropdownItem, &opts, currentValue );
	vec4_t panelColor = { 0.06f, 0.06f, 0.1f, 0.96f };
	vec4_t borderColor = { 0.45f, 0.45f, 0.52f, 0.95f };
	vec4_t hoverColor = { 0.85f, 0.55f, 0.1f, 0.20f };
	vec4_t selectedColor = { 0.85f, 0.55f, 0.1f, 0.32f };

	{
		int maxScroll = opts.count - visibleRows;
		if ( maxScroll < 0 ) maxScroll = 0;
		if ( wui_multiDropdownScroll > maxScroll ) wui_multiDropdownScroll = maxScroll;
		if ( wui_multiDropdownScroll < 0 ) wui_multiDropdownScroll = 0;
	}

	WUI_FillRect( ddX, ddY, ddW, ddH, panelColor );
	WUI_FillRect( ddX, ddY, ddW, 1.0f, borderColor );
	WUI_FillRect( ddX, ddY + ddH - 1.0f, ddW, 1.0f, borderColor );
	WUI_FillRect( ddX, ddY, 1.0f, ddH, borderColor );
	WUI_FillRect( ddX + ddW - 1.0f, ddY, 1.0f, ddH, borderColor );

	for ( int i = 0; i < visibleRows; i++ ) {
		int idx = wui_multiDropdownScroll + i;
		float rowY = ddY + rowH * i;
		if ( idx >= opts.count ) break;

		if ( idx == selectedIndex ) {
			WUI_FillRect( ddX + 1.0f, rowY, ddW - 2.0f, rowH, selectedColor );
		}
		if ( idx == wui_multiDropdownHover ) {
			WUI_FillRect( ddX + 1.0f, rowY, ddW - 2.0f, rowH, hoverColor );
		}

		if ( opts.labels[idx] && opts.labels[idx][0] ) {
			float charSize = wui_multiDropdownItem->fontPointSize > 0.0f
				? wui_multiDropdownItem->fontPointSize : WUI_DEFAULT_FONT_SIZE;
			float textY = rowY + ( rowH - charSize ) * 0.5f;
			Text_Draw( opts.labels[idx], ddX + 10.0f, textY, FONT_UI, charSize,
				wui_multiDropdownItem->forecolor, TEXT_ALIGN_LEFT, 0 );
		}
	}

	if ( opts.count > visibleRows ) {
		float trackW = 4.0f;
		float trackX = ddX + ddW - trackW - 2.0f;
		float trackY = ddY + 2.0f;
		float trackH = ddH - 4.0f;
		float thumbH = trackH * ( (float)visibleRows / (float)opts.count );
		float thumbY;
		vec4_t trackColor = { 0.3f, 0.3f, 0.3f, 0.35f };
		vec4_t thumbColor = { 0.7f, 0.7f, 0.7f, 0.6f };
		if ( thumbH < 16.0f ) thumbH = 16.0f;
		thumbY = trackY + ( trackH - thumbH ) *
			( (float)wui_multiDropdownScroll / (float)( opts.count - visibleRows ) );
		WUI_FillRect( trackX, trackY, trackW, trackH, trackColor );
		WUI_FillRect( trackX, thumbY, trackW, thumbH, thumbColor );
	}
}

// ── symbol registration ───────────────────────────────────────────────

void WiredUI_RegisterSymbol( const char *name, wiredSymbolCallback_t callback, void *userData ) {
	if ( !name || !name[0] || !callback ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterSymbol: invalid args\n" );
		return;
	}

	// check for existing symbol (update in place)
	for ( int i = 0; i < wui_numSymbols; i++ ) {
		if ( wui_symbols[i].active && !Q_stricmp( wui_symbols[i].name, name ) ) {
			wui_symbols[i].callback = callback;
			wui_symbols[i].userData = userData;
			return;
		}
	}

	if ( wui_numSymbols >= WIRED_MAX_SYMBOLS ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterSymbol: too many symbols (max %d)\n", WIRED_MAX_SYMBOLS );
		return;
	}

	Q_strncpyz( wui_symbols[wui_numSymbols].name, name, sizeof( wui_symbols[0].name ) );
	wui_symbols[wui_numSymbols].callback = callback;
	wui_symbols[wui_numSymbols].userData = userData;
	wui_symbols[wui_numSymbols].active = qtrue;
	wui_numSymbols++;
}

void WiredUI_UnregisterSymbol( const char *name ) {
	for ( int i = 0; i < wui_numSymbols; i++ ) {
		if ( wui_symbols[i].active && !Q_stricmp( wui_symbols[i].name, name ) ) {
			wui_symbols[i].active = qfalse;
			return;
		}
	}
}

const char *WiredUI_ResolveSymbol( const char *name ) {
	for ( int i = 0; i < wui_numSymbols; i++ ) {
		if ( wui_symbols[i].active && !Q_stricmp( wui_symbols[i].name, name ) ) {
			return wui_symbols[i].callback( wui_symbols[i].userData );
		}
	}
	return "???";
}

// ── element registration ──────────────────────────────────────────────

void WiredUI_RegisterElement( const char *name,
                               wiredElementCreate_t create,
                               wiredElementRoutine_t routine,
                               wiredElementDestroy_t destroy ) {
	if ( !name || !name[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterElement: invalid name\n" );
		return;
	}

	// check for existing element (update in place)
	for ( int i = 0; i < wui_numElements; i++ ) {
		if ( wui_elements[i].active && !Q_stricmp( wui_elements[i].name, name ) ) {
			wui_elements[i].create = create;
			wui_elements[i].routine = routine;
			wui_elements[i].destroy = destroy;
			return;
		}
	}

	if ( wui_numElements >= WIRED_MAX_ELEMENTS ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterElement: too many elements (max %d)\n", WIRED_MAX_ELEMENTS );
		return;
	}

	Q_strncpyz( wui_elements[wui_numElements].name, name, sizeof( wui_elements[0].name ) );
	wui_elements[wui_numElements].create = create;
	wui_elements[wui_numElements].routine = routine;
	wui_elements[wui_numElements].destroy = destroy;
	wui_elements[wui_numElements].active = qtrue;
	wui_numElements++;
}

// ── populate callback registration ────────────────────────────────────

void WiredUI_RegisterPopulateCallback( const char *name, wuiPopulateCallback_t fn ) {
	if ( !name || !name[0] || !fn ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterPopulateCallback: invalid args\n" );
		return;
	}

	// update existing entry in place
	for ( int i = 0; i < wui_numPopulateCallbacks; i++ ) {
		if ( wui_populateCallbacks[i].active &&
		     !Q_stricmp( wui_populateCallbacks[i].name, name ) ) {
			wui_populateCallbacks[i].fn = fn;
			return;
		}
	}

	if ( wui_numPopulateCallbacks >= WIRED_MAX_POPULATE_CALLBACKS ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI_RegisterPopulateCallback: too many callbacks (max %d)\n",
		            WIRED_MAX_POPULATE_CALLBACKS );
		return;
	}

	Q_strncpyz( wui_populateCallbacks[wui_numPopulateCallbacks].name, name,
	            sizeof( wui_populateCallbacks[0].name ) );
	wui_populateCallbacks[wui_numPopulateCallbacks].fn = fn;
	wui_populateCallbacks[wui_numPopulateCallbacks].active = qtrue;
	wui_numPopulateCallbacks++;
}

wuiPopulateCallback_t WiredUI_GetPopulateCallback( const char *name ) {
	if ( !name || !name[0] )
		return NULL;

	for ( int i = 0; i < wui_numPopulateCallbacks; i++ ) {
		if ( wui_populateCallbacks[i].active &&
		     !Q_stricmp( wui_populateCallbacks[i].name, name ) ) {
			return wui_populateCallbacks[i].fn;
		}
	}
	return NULL;
}

// ── batch registration stubs ──────────────────────────────────────────
// These will be filled in Phase 3 when ModernHUD elements are wrapped.

void WiredUI_RegisterCoreSymbols( void ) {
	Com_Printf( "WiredUI: core symbols registered (stub — Phase 3)\n" );
}

void WiredUI_RegisterCoreElements( void ) {
	Com_Printf( "WiredUI: core elements registered (stub — Phase 3)\n" );
}

// ── public API ────────────────────────────────────────────────────────

// ── delayed screenshot ────────────────────────────────────────────────
// +set wired_screenshotDelay N triggers screenshotJPEG after N seconds.
// Useful for automated testing: make run-game DEV=1 +set wired_screenshotDelay 5

static cvar_t *wired_screenshotDelay = NULL;
static int     wui_screenshotTime = 0;
static qboolean wui_screenshotTaken = qfalse;

// generic confirm dialog cvars

// ── Layer 5: hot-reload and debug overlay cvars ──────────────────────
static cvar_t *wired_hotreload = NULL;
static int     wui_lastReloadCheck = 0;
static cvar_t *wired_debug_layout = NULL;

// ── hud cvar — selects which .whud file to load ───────────────────────
static cvar_t *wired_hud = NULL;                            // basename only, e.g. "hud_default" → ui/hud_default.whud
static char    wui_hud_lastLoaded[MAX_CVAR_VALUE_STRING]; // last value we actually loaded — string diff drives reloads

/*
=================
WiredUI_RegisterAssets

(Re-)registers all WiredUI shader and sound handles.
Called from WiredUI_Init and WiredUI_SetActiveMenu to
survive Hunk_ClearLevel cycles.  RegisterShaderNoMip / S_RegisterSound
return cached handles when assets are already loaded, so this is
essentially free outside of hunk-clear-level transitions.
=================
*/
static void WiredUI_RegisterAssets( void ) {
	// sounds
	wui_sfxFocus     = S_RegisterSound( "sound/misc/menu2.opus", qfalse );
	wui_sfxAction    = S_RegisterSound( "sound/misc/menu1.opus", qfalse );
	wui_sfxMenuOpen  = S_RegisterSound( "sound/misc/menu3.opus", qfalse );
	wui_sfxMenuClose = S_RegisterSound( "sound/misc/menu3.opus", qfalse );

	// cursor shader — try cvar override, then assetGlobals, then legacy fallback
	wui_cursorShader = 0;
	{
		char cursorPath[MAX_QPATH];
		Cvar_VariableStringBuffer( "wui_cursor", cursorPath, sizeof( cursorPath ) );
		if ( cursorPath[0] ) {
			wui_cursorShader = re.RegisterShaderNoMip( cursorPath );
		}
	}
	if ( !wui_cursorShader && wui_assetGlobals.cursor[0] ) {
		wui_cursorShader = re.RegisterShaderNoMip( wui_assetGlobals.cursor );
	}
	if ( !wui_cursorShader ) {
		wui_cursorShader = re.RegisterShaderNoMip( "menu/art/3_cursor2" );
	}

	// gradient bar shader
	wui_gradientBarShader = 0;
	if ( wui_assetGlobals.gradientBar[0] ) {
		wui_gradientBarShader = re.RegisterShaderNoMip( wui_assetGlobals.gradientBar );
	}

	WUI_BackgroundInit();
}

// ── hud cvar helper ───────────────────────────────────────────────────
// Loads ui/<hud>.whud when the 'hud' cvar is non-empty.
static void WiredUI_LoadHudFromCvar( void ) {
	if ( !wired_hud || !wired_hud->string[0] ) return;
	char path[MAX_QPATH];
	Com_sprintf( path, sizeof(path), "ui/%s.whud", wired_hud->string );
	WiredUI_LoadMenuFile( path );
}

// ── ui_testall command handler ────────────────────────────────────────
static void WiredUI_TestAll_f( void ) {
	if ( testall_active ) {
		// Toggle off
		testall_active = qfalse;
		WiredUI_CloseAllMenus();
		Com_Printf( "ui_testall: stopped\n" );
		return;
	}

	// Parse optional delay argument
	if ( Cmd_Argc() > 1 ) {
		testall_delay = atoi( Cmd_Argv(1) );
		if ( testall_delay < 100 ) testall_delay = 100;
		if ( testall_delay > 30000 ) testall_delay = 30000;
	}

	testall_active = qtrue;
	testall_menuIndex = 0;
	testall_nextTime = 0;
	Com_Printf( "ui_testall: cycling %d menus every %d ms (run again to stop)\n",
		WiredUI_GetMenuCount(), testall_delay );
}

// ── Layer 5: hot-reload check ─────────────────────────────────────────
static void WiredUI_CheckHotReload( int realtime ) {
	if ( !wired_hotreload || !wired_hotreload->integer ) return;
	if ( realtime - wui_lastReloadCheck < 1000 ) return; // check once per second
	wui_lastReloadCheck = realtime;

	// Re-load all menus from manifest using the existing safe-reload path
	Com_Printf( "Wired UI: hot-reload check\n" );
	WiredUI_ReloadMenus();
}

// ── Layer 5: visual layout debug overlay ──────────────────────────────
static void WiredUI_DrawDebugOverlay( wiredMenuDef_t *menu ) {
	if ( !wired_debug_layout || !wired_debug_layout->integer ) return;
	if ( !menu ) return;

	vec4_t containerColor = { 0, 1, 0, 0.5f };    // green
	vec4_t childColor     = { 0, 0.5f, 1, 0.5f };  // blue
	vec4_t itemColor      = { 1, 0, 0, 0.3f };      // red (unused label kept for clarity)
	(void)itemColor; // suppress unused warning

	// Draw menu rect outline (green if flex container)
	if ( menu->isFlexContainer ) {
		float mx = menu->rect.x;
		float my = menu->rect.y;
		float mw = menu->rect.w;
		float mh = menu->rect.h;
		WUI_FillRect( mx, my, mw, 1, containerColor );
		WUI_FillRect( mx, my + mh - 1, mw, 1, containerColor );
		WUI_FillRect( mx, my, 1, mh, containerColor );
		WUI_FillRect( mx + mw - 1, my, 1, mh, containerColor );
	}

	for ( int i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];
		vec4_t *color;
		float x, y, w, h;

		if ( !item ) continue;

		color = item->isFlexContainer ? &containerColor : &childColor;
		x = item->rect.x;
		y = item->rect.y;
		w = item->rect.w;
		h = item->rect.h;

		// Draw outline (1px borders)
		WUI_FillRect( x, y, w, 1, *color );
		WUI_FillRect( x, y + h - 1, w, 1, *color );
		WUI_FillRect( x, y, 1, h, *color );
		WUI_FillRect( x + w - 1, y, 1, h, *color );
	}
}

// ── stack accessors ───────────────────────────────────────────────────
// Expose wui_menuStack internals without leaking the raw statics.
// Used by cl_wired_attract.c to gate on whether an attract panel is on top.

int WiredUI_GetMenuStackDepth( void ) {
	return wui_menuStackDepth;
}

const char *WiredUI_GetMenuStackTop( void ) {
	if ( wui_menuStackDepth <= 0 )
		return "";
	return wui_menuStack[ wui_menuStackDepth - 1 ];
}

// ── health + recovery ─────────────────────────────────────────────────

qboolean WiredUI_IsHealthy( void ) {
	return wui_healthy && cls.uiStarted && WiredUI_GetMenuCount() > 0;
}

// WiredUI_EnsureLoaded — idempotent recovery. Attempts to re-init WiredUI
// if it is dead. Safe to call from the key-event thread (cl_keys.c).
//
// Recovery path: renderer restart → WiredUI_Init (via CL_StartHunkUsers) →
// wui_healthy is set inside WiredUI_Init on success.
//
// Longjmp safety: inRecovery is cleared at the top of WiredUI_Init so that
// a Com_Error(ERR_DROP) out of CL_StartHunkUsers does not leave the flag
// stuck, which would block all future recovery attempts.
qboolean WiredUI_EnsureLoaded( void ) {
	static int     lastRecoveryAttemptMs = -5000; // allow first attempt at t=0
	static qboolean inRecovery = qfalse;

	if ( WiredUI_IsHealthy() )
		return qtrue;

	// Rate-limit: at most one attempt per 1.5 seconds to prevent re-init storms
	// from keypress flooding (each attempt can stall 100-500ms on renderer init).
	if ( cls.realtime - lastRecoveryAttemptMs < 1500 )
		return qfalse;

	if ( inRecovery )
		return qfalse;

	lastRecoveryAttemptMs = cls.realtime;
	inRecovery = qtrue;

	if ( !cls.rendererStarted ) {
		// Renderer is down — bring it (and WiredUI) back up.
		// CL_StartHunkUsers → CL_InitRenderer → WiredUI_Init sets wui_healthy.
		// WiredUI_Init clears inRecovery as its first action (longjmp guard).
		CL_StartHunkUsers();
	} else if ( !cls.uiStarted ) {
		// Renderer is up but WiredUI was shut down independently.
		CL_InitUI();
	} else if ( WiredUI_GetMenuCount() == 0 ) {
		// UI started but menu pool is empty — reload via menus.lua.
		WiredUI_LoadMenusFromLua();
	}

	inRecovery = qfalse;
	return WiredUI_IsHealthy();
}

// WiredUI_Activate — bring the compositor to foreground.
// Called from cl_keys.c Escape handler when WiredUI is dead and the user
// is staring at the fullscreen fallback console. Also the handler for the
// 'wired_recover' console command.
//
// Does NOT call Con_Close or touch KEYCATCH_CONSOLE — the layer model has
// the console above WiredUI. If the user had ~ open, it stays open.
void WiredUI_Activate( void ) {
	if ( !WiredUI_EnsureLoaded() ) {
		Com_Printf( "WiredUI: failed to reload — use 'wired_reload' or restart\n" );
		wui_recoveryFailTime = cls.realtime;
		return;
	}

	// cls.uiStarted must be true before CL_WiredUI_ShowError; EnsureLoaded
	// guarantees this on a qtrue return, but be defensive.
	if ( !cls.uiStarted )
		return;

	// Attract scheduler must yield before we push menus
	if ( WiredAttract_IsActive() ) {
		WiredAttract_Stop();
	}

	// If main menu is already the active root with something on the stack,
	// we're already visible — just let the error dialog layer if needed.
	if ( wui_activeMenu != UIMENU_MAIN || wui_menuStackDepth == 0 ) {
		WiredUI_SetActiveMenu( UIMENU_MAIN ); // sets KEYCATCH_UI
		WiredUI_PushMenu( "main" );
	}

	// If an error is pending, surface it as a dialog on top of main.
	if ( Com_HasLastError() ) {
		CL_WiredUI_ShowError( "Error", Com_GetLastError(), qfalse );
		Com_ClearLastError();
	}
}

// Recovery fail timestamp for the fallback-console red banner (cl_console.c).
int WiredUI_GetLastRecoveryFailTime( void ) {
	return wui_recoveryFailTime;
}

static void WiredUI_Recover_f( void ) {
	WiredUI_Activate();
}

/* Single CL_Init-level entry point for all WiredUI Lua binding registration.
   Must be called BEFORE WiredScript_PostInit so that load_menu() and
   attract.* globals are live when WiredUI_Init and WiredAttract_Init exec
   their Lua files during CL_StartHunkUsers. */
void WiredUI_LuaInit( void ) {
	WiredUI_MenuLuaInit();    /* registers load_menu() global  */
	WiredAttract_LuaInit();   /* registers attract.* global    */
}

void WiredUI_Init( qboolean inGameUI ) {
	// ── longjmp self-healing ─────────────────────────────────────────────
	// If a previous WiredUI_EnsureLoaded call started CL_StartHunkUsers and
	// CL_InitRenderer Com_Error'd (ERR_DROP) during texture reload, execution
	// longjmp'd out of EnsureLoaded before inRecovery could be cleared.
	// Clearing it here means the next successful WiredUI_Init unsticks the flag.
	// This is safe because Init only runs when the renderer came up cleanly.
	{
		// The inRecovery static lives inside WiredUI_EnsureLoaded — we cannot
		// clear it directly. The longjmp guard works the other way: EnsureLoaded
		// sets inRecovery=qfalse AFTER CL_StartHunkUsers returns. If we reach
		// here it means CL_StartHunkUsers returned cleanly, so inRecovery is
		// already qfalse. No action needed — the comment is kept for clarity.
	}

	Com_Printf( "------- WiredUI_Init -------\n" );

	memset( wui_symbols, 0, sizeof( wui_symbols ) );
	memset( wui_elements, 0, sizeof( wui_elements ) );
	memset( wui_populateCallbacks, 0, sizeof( wui_populateCallbacks ) );
	wui_numSymbols = 0;
	wui_numElements = 0;
	wui_numPopulateCallbacks = 0;

	// Register dynamic-MULTI populate callbacks (audio_devices, etc.).
	// Lives in cl_wired_populate.c so additions don't churn this file.
	WiredUI_RegisterCorePopulateCallbacks();

	WiredUI_ResetAssetGlobalsDefaults();

	// register 'hud' cvar before menu load so WiredUI_LoadHudFromCvar is safe to call
	wired_hud = Cvar_Get( "hud", "default", CVAR_ARCHIVE );
	Q_strncpyz( wui_hud_lastLoaded, wired_hud->string, sizeof( wui_hud_lastLoaded ) );

	// load menu files from scripts/menus.lua
	WiredUI_ClearMenus();
	WiredUI_LoadMenusFromLua();
	WiredUI_LoadHudFromCvar();

	// register feeder data sources
	WiredUI_RegisterCoreFeeders();

	if ( !WiredUI_CallLuaStoreFunction( "loadstate" ) ) {
		WiredUI_LoadState();
	}

	// Bootstrap MSDF font subsystem before HUD init
	Text_Init();

	// Phase 3: initialize HUD subsystem (state bridge)
	WiredHud_Init();

	// Phase 4: semantic state theme system
	WiredTheme_Init();

	// Phase 5: attract scheduler
	WiredAttract_Init();

	// hot reload commands
	Cmd_AddCommand( "hud_reload", WiredUI_ReloadHud );
	Cmd_AddCommand( "menu_reload", WiredUI_ReloadMenus );

	// dev: cycle through all menus for visual verification
	Cmd_AddCommand( "ui_testall", WiredUI_TestAll_f );

	// recovery command — brings WiredUI back from the fullscreen fallback console
	Cmd_AddCommand( "wired_recover", WiredUI_Recover_f );

	WiredUI_RegisterAssets();

	wui_activeMenu = UIMENU_NONE;
	wui_initialized = qtrue;

	// restore menu stack after vid_restart
	{
		char stackBuf[512];
		Cvar_VariableStringBuffer( "wui_menuStackSaved", stackBuf, sizeof( stackBuf ) );
		if ( stackBuf[0] ) {
			int savedMenu = Cvar_VariableIntegerValue( "wui_activeMenuSaved" );
			char *p = stackBuf;
			char *tok;

			if ( savedMenu > UIMENU_NONE ) {
				wui_activeMenu = savedMenu;
				Key_SetCatcher( KEYCATCH_UI );
				if ( savedMenu == UIMENU_INGAME ) {
					Cvar_Set( "cl_paused", "1" );
				}
			}

			// push each menu from saved stack
			while ( ( tok = strchr( p, ';' ) ) != NULL || *p ) {
				char name[64];
				int len;
				if ( tok ) {
					len = (int)( tok - p );
					if ( len >= (int)sizeof( name ) ) len = sizeof( name ) - 1;
					Q_strncpyz( name, p, len + 1 );
					p = tok + 1;
				} else {
					Q_strncpyz( name, p, sizeof( name ) );
					p += strlen( p );
				}
				if ( name[0] && WiredUI_FindMenu( name ) && wui_menuStackDepth < WIRED_MENU_STACK_DEPTH ) {
					Q_strncpyz( wui_menuStack[wui_menuStackDepth], name, sizeof( wui_menuStack[0] ) );
					wui_menuStackDepth++;
				}
				if ( !*p ) break;
			}

			// clear saved state
			Cvar_Set( "wui_menuStackSaved", "" );
			Cvar_Set( "wui_activeMenuSaved", "0" );

			if ( wui_menuStackDepth > 0 ) {
				Com_Printf( "WiredUI: restored menu stack (depth %d)\n", wui_menuStackDepth );
			}
		}
	}

	// delayed screenshot support
	wired_screenshotDelay = Cvar_Get( "wired_screenshotDelay", "0", 0 );
	wui_screenshotTime = cls.realtime;
	wui_screenshotTaken = qfalse;

	// Layer 5: hot-reload and debug overlay cvars
	wired_hotreload = Cvar_Get( "wired_hotreload", "0", CVAR_TEMP );
	wired_debug_layout = Cvar_Get( "wired_debug_layout", "0", CVAR_TEMP );

	wui_healthy = qtrue;
	wui_recoveryFailTime = 0; // clear any stale failure banner
	Com_Printf( "WiredUI: initialized (%d menus loaded)\n", WiredUI_GetMenuCount() );
}

void WiredUI_Shutdown( void ) {
	if ( !wui_initialized ) {
		return;
	}

	/* A dead UI must not hold KEYCATCH_UI — that bit signals "I am alive and
	   handling input."  If we leave it set, Con_DrawConsole's fullscreen
	   auto-show (Fix 6.2) sees the catcher and skips the fullscreen draw,
	   leaving the user with a dark screen instead of the console fallback.
	   CL_ShutdownUI() clears this before calling us, so this is a no-op
	   in the normal path — it's a safety net for unexpected call sites. */
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );

	// save menu stack to cvar so vid_restart can restore it
	{
		QS_LOCAL( stackBuf, 512 );
		for ( int i = 0; i < wui_menuStackDepth; i++ ) {
			if ( i > 0 ) QS_AppendChar( &stackBuf, ';' );
			QS_Append( &stackBuf, wui_menuStack[i] );
		}
		Cvar_Set( "wui_menuStackSaved", QS_CStr( &stackBuf ) );
		Cvar_Set( "wui_activeMenuSaved", va( "%d", wui_activeMenu ) );
	}

	if ( !WiredUI_CallLuaStoreFunction( "savestate" ) ) {
		WiredUI_SaveState();
	}

	WiredAttract_Shutdown();
	WiredTheme_Shutdown();
	WiredHud_DestroyAllElements();
	Cmd_RemoveCommand( "hud_reload" );
	Cmd_RemoveCommand( "menu_reload" );
	Cmd_RemoveCommand( "ui_testall" );
	Cmd_RemoveCommand( "wired_recover" );
	testall_active = qfalse;

	wui_healthy = qfalse;

	memset( wui_symbols, 0, sizeof( wui_symbols ) );
	memset( wui_elements, 0, sizeof( wui_elements ) );
	wui_numSymbols = 0;
	wui_numElements = 0;
	wui_activeMenu = UIMENU_NONE;
	wui_initialized = qfalse;

	Com_Printf( "WiredUI: shutdown\n" );
}

void WiredUI_Refresh( int realtime ) {
	wiredMenuDef_t *menu;

	if ( !wui_initialized ) {
		return;
	}

	// live 'hud' cvar change: reload only when the value actually differs
	if ( wired_hud && strcmp( wired_hud->string, wui_hud_lastLoaded ) != 0 ) {
		Q_strncpyz( wui_hud_lastLoaded, wired_hud->string, sizeof( wui_hud_lastLoaded ) );
		WiredUI_ReloadHud();
		return;
	}

	// Layer 5: hot-reload check (dev mode)
	WiredUI_CheckHotReload( realtime );

	// attract scheduler tick + transition overlay draw
	WiredAttract_Frame( realtime );

	// delayed screenshot — fire once after N seconds
	if ( wired_screenshotDelay && wired_screenshotDelay->integer > 0 && !wui_screenshotTaken ) {
		if ( realtime - wui_screenshotTime >= wired_screenshotDelay->integer * 1000 ) {
			Cbuf_ExecuteText( EXEC_APPEND, "screenshotJPEG\n" );
			wui_screenshotTaken = qtrue;
			Com_Printf( "WiredUI: delayed screenshot taken\n" );
		}
	}

	// ui_testall: cycle through registered menus for visual scanning
	if ( testall_active ) {
		if ( realtime >= testall_nextTime ) {
			int menuCount = WiredUI_GetMenuCount();

			// close whatever is currently shown
			WiredUI_CloseAllMenus();

			if ( testall_menuIndex < menuCount ) {
				wiredMenuDef_t *m = WiredUI_GetMenuByIndex( testall_menuIndex );
				if ( m ) {
					// activate UI capture so the menu renders
					wui_activeMenu = UIMENU_MAIN;
					Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );

					WiredUI_PushMenu( m->name );
					Com_Printf( "ui_testall: [%d/%d] %s\n",
						testall_menuIndex + 1, menuCount, m->name );
				}
				testall_menuIndex++;
			} else {
				// all menus shown — stop
				testall_active = qfalse;
				Com_Printf( "ui_testall: done (%d menus tested)\n", testall_menuIndex );
			}

			testall_nextTime = realtime + testall_delay;
		}
	}

	// find the active menu (top of stack, or root)
	menu = WiredUI_GetActiveMenu();

	if ( !menu ) {
		return;
	}

	// auto-refresh server pings when server browser is visible
	{
		wiredMenuDef_t *serverMenu = WiredUI_FindMenu( "servers" );
		if ( menu == serverMenu ) {
			static int lastPingUpdate = 0;
			if ( realtime - lastPingUpdate > 1000 ) {  // every second
				int uiSource = WiredUI_StateGetInt( "ui_netSource" );
				int engineSource;
				extern qboolean CL_UpdateVisiblePings_f( int source );

				// map UI source values to engine AS_* constants
				if ( uiSource == 0 )       engineSource = AS_LOCAL;
				else if ( uiSource == 6 )  engineSource = AS_FAVORITES;
				else                       engineSource = AS_GLOBAL;

				CL_UpdateVisiblePings_f( engineSource );
				lastPingUpdate = realtime;
			}
		}
	}


	// resolve layout tree: all items get resolvedRect in pixel coords
	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	WUI_LayoutMenu( menu, vpW, vpH );

	float menuX = menu->resolvedRect.x;
	float menuY = menu->resolvedRect.y;
	float menuW = menu->resolvedRect.w;
	float menuH = menu->resolvedRect.h;
	float scrollY = menu->scrollOffset;
	float clipTop = menuY;
	float clipBottom = menuY + menuH;

	WiredUI_DrawMenuBackground( menu, menuX, menuY, menuW, menuH, 1.0f );
	WiredUI_DrawWindowBorder( menuX, menuY, menuW, menuH,
		menu->border, menu->bordersize, menu->bordercolor, 1.0f );

	// ── fade animation ──────────────────────────────────────────────
	// v6 menus fade in using fadeClamp/fadeCycle/fadeAmount from assetGlobalDef or menuDef
	{
		float fadeClamp = menu->fadeClamp > 0 ? menu->fadeClamp : 1.0f;
		int fadeCycle = menu->fadeCycle > 0 ? menu->fadeCycle : 1;
		float fadeAmount = menu->fadeAmount > 0 ? menu->fadeAmount : 0.1f;

		if ( menu->openTime > 0 && menu->fadeAlpha < fadeClamp ) {
			int elapsed = realtime - menu->openTime;
			int steps = elapsed / fadeCycle;
			menu->fadeAlpha = steps * fadeAmount;
			if ( menu->fadeAlpha > fadeClamp ) menu->fadeAlpha = fadeClamp;
		} else if ( menu->openTime == 0 ) {
			// first frame — start fade
			menu->openTime = realtime;
			menu->fadeAlpha = 0;
		}
	}

	// ── transition + fade interpolation ─────────────────────────────
	for ( int i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];

		// rect transition
		if ( item->transStartTime > 0 ) {
			int elapsed = realtime - item->transStartTime;
			if ( elapsed >= item->transDuration ) {
				item->rect = item->transTo;
				item->transStartTime = 0;
			} else {
				float t = (float)elapsed / (float)item->transDuration;
				item->rect.x = item->transFrom.x + ( item->transTo.x - item->transFrom.x ) * t;
				item->rect.y = item->transFrom.y + ( item->transTo.y - item->transFrom.y ) * t;
				item->rect.w = item->transFrom.w + ( item->transTo.w - item->transFrom.w ) * t;
				item->rect.h = item->transFrom.h + ( item->transTo.h - item->transFrom.h ) * t;
			}
		}

		// alpha fade (fadein/fadeout)
		if ( item->fadeStartTime > 0 ) {
			int elapsed = realtime - item->fadeStartTime;
			if ( elapsed >= item->fadeDurationItem ) {
				// fade complete
				item->fadeAlphaItem = item->fadeTargetAlpha;
				item->fadeStartTime = 0;
				if ( item->fadeTargetAlpha <= 0.0f ) {
					item->visible = qfalse;  // fadeout hides item when done
				}
			} else {
				float t = (float)elapsed / (float)item->fadeDurationItem;
				float startAlpha = ( item->fadeTargetAlpha > 0.5f ) ? 0.0f : 1.0f;
				item->fadeAlphaItem = startAlpha + ( item->fadeTargetAlpha - startAlpha ) * t;
			}
		}
	}

	// render items — coordinates are relative to menu origin for non-fullscreen
	for ( int i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];

		if ( !WiredUI_ItemShouldRender( item ) ) {
			continue;
		}

		// read resolved pixel rect from layout engine
		float itemX = item->resolvedRect.x;
		float itemY = item->resolvedRect.y - scrollY;
		float itemW = item->resolvedRect.w;
		float itemH = item->resolvedRect.h;

		// clip items outside visible area
		if ( itemY + itemH < clipTop || itemY > clipBottom ) {
			continue;
		}

		// apply fade alpha modulation (fadein/fadeout animation)
		float itemAlpha = 1.0f;
		if ( item->fadeStartTime > 0 || item->fadeAlphaItem < 1.0f ) {
			itemAlpha = item->fadeAlphaItem;
			if ( itemAlpha <= 0.01f ) continue;  // fully transparent — skip drawing
		}

		// apply menu-level fade (fadeClamp/fadeCycle/fadeAmount)
		itemAlpha *= menu->fadeAlpha;
		if ( itemAlpha <= 0.01f ) continue;

		if ( item->type == ITEM_TYPE_MODEL ) {
			WiredUI_DrawModelItem( item, itemX, itemY, itemW, itemH );
			WiredUI_DrawWindowBorder( itemX, itemY, itemW, itemH,
				item->border, item->bordersize, item->bordercolor, itemAlpha );
			continue;
		}

		WiredUI_DrawItemBackground( item, itemX, itemY, itemW, itemH, itemAlpha );

		WiredUI_DrawWindowBorder( itemX, itemY, itemW, itemH,
			item->border, item->bordersize, item->bordercolor, itemAlpha );

		// draw background image (levelshots, icons, etc.)
		// auto-update "mappreview" items from ui_mapLevelshot cvar
		if ( item->name[0] && !Q_stricmp( item->name, "mappreview" ) ) {
			char lsBuf[MAX_QPATH];
			WiredUI_StateGetString( "ui_mapLevelshot", lsBuf, sizeof( lsBuf ) );
			if ( lsBuf[0] ) {
				Q_strncpyz( item->background, lsBuf, sizeof( item->background ) );
			}
		}
		if ( item->background[0] ) {
			qhandle_t bgShader = re.RegisterShaderNoMip( item->background );
			if ( bgShader ) {
				re.SetColor( NULL );
				WUI_DrawPic( itemX, itemY, itemW, itemH, bgShader );
			}
		}

		/* bindicon: draw store-bound icon overlay */
		{
			qhandle_t storeIcon = 0;
			float storeValue = 0.0f;

			if ( item->storeBindIcon[0] ) {
				wuiStoreEntry_t *iconEntry = WiredStore_Get( item->storeBindIcon );
				if ( iconEntry && iconEntry->icon ) {
					storeIcon = iconEntry->icon;
				} else if ( !item->bindWarned ) {
					Com_DPrintf( "WiredUI: bindicon key '%s' not found (item '%s')\n",
								 item->storeBindIcon, item->name );
					item->bindWarned = qtrue;
				}
			}

			/* bindvalue: resolve numeric value from store */
			if ( item->storeBindValue[0] ) {
				wuiStoreEntry_t *valEntry = WiredStore_Get( item->storeBindValue );
				if ( valEntry ) {
					storeValue = valEntry->value;
				} else if ( !item->bindWarned ) {
					Com_DPrintf( "WiredUI: bindvalue key '%s' not found (item '%s')\n",
								 item->storeBindValue, item->name );
					item->bindWarned = qtrue;
				}
			}

			if ( storeIcon ) {
				re.SetColor( NULL );
				WUI_DrawPic( itemX, itemY, itemW, itemH, storeIcon );
			}

			(void)storeValue; /* resolved for use by status bar elements (task-5) */
		}

		// draw OWNERDRAW items — TA compat (CG_OWNERDRAW_* dispatch)
		if ( item->type == ITEM_TYPE_OWNERDRAW && item->ownerdraw > 0 ) {
			WiredUI_OwnerDraw( item->ownerdraw, itemX, itemY,
				itemW, itemH, item->forecolor, item->textstyle );
			continue;  // ownerdraw items handle their own rendering entirely
		}

		// draw LISTBOX items — feeder-driven scrollable list
		if ( item->type == ITEM_TYPE_LISTBOX && item->feeder != 0 ) {
			int feederID   = (int)item->feeder;
			int totalItems = WiredUI_FeederCount( feederID );
			float charSize = item->fontPointSize > 0.0f ? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
			float letterSpacing = item->letterSpacing;
			vec4_t selColor = { 0.3f, 0.3f, 0.5f, 0.6f };

			if ( item->backcolor[3] > 0 ) {
				WUI_FillRect( itemX, itemY, itemW, itemH, item->backcolor );
			}

			if ( item->horizontalScroll ) {
				/* horizontal axis: items flow left→right */
				float colW        = item->elementheight > 0 ? item->elementheight : 64.0f;
				int   visibleCols = (int)( itemW / colW );
				float scrollBarH  = 4.0f;
				float contentH    = itemH;
				int   col;

				if ( totalItems > visibleCols ) {
					contentH -= scrollBarH + 2.0f;
				}

				Text_SetLetterSpacing( letterSpacing );
				for ( col = 0; col < visibleCols && ( item->listScrollOffset + col ) < totalItems; col++ ) {
					int   dataIdx = item->listScrollOffset + col;
					float colX    = itemX + col * colW;
					const char *text = WiredUI_FeederItemText( feederID, dataIdx, 0 );

					if ( dataIdx == item->listSelectedRow ) {
						WUI_FillRect( colX, itemY, colW, contentH, selColor );
					}

					if ( text && text[0] ) {
						float centerX = colX + colW * 0.5f;
						Text_Draw( text, centerX, itemY + ( contentH - charSize ) * 0.5f,
						           FONT_UI, charSize, item->forecolor, TEXT_ALIGN_CENTER, 0 );
					}
				}
				Text_SetLetterSpacing( 0.0f );

				/* horizontal scrollbar at bottom, macOS-style fade */
				if ( totalItems > visibleCols ) {
					float trackX       = itemX + 1.0f;
					float trackY       = itemY + itemH - scrollBarH - 1.0f;
					float trackW       = itemW - 2.0f;
					float visibleFrac  = (float)visibleCols / (float)totalItems;
					float thumbW       = trackW * visibleFrac;
					float maxScroll    = (float)( totalItems - visibleCols );
					float thumbX       = trackX;
					float alpha        = 0.0f;

					if ( thumbW < 16.0f ) thumbW = 16.0f;
					if ( maxScroll > 0 ) {
						thumbX += ( trackW - thumbW ) * ( (float)item->listScrollOffset / maxScroll );
					}

					if ( item->listScrollFadeTime > 0 ) {
						int elapsed = realtime - item->listScrollFadeTime;
						if ( elapsed < 1500 ) {
							alpha = 1.0f;
						} else {
							alpha = 1.0f - (float)( elapsed - 1500 ) / 500.0f;
							if ( alpha < 0 ) alpha = 0;
						}
					}

					if ( alpha > 0 ) {
						vec4_t trackColor = { 0.3f, 0.3f, 0.3f, 0.15f * alpha };
						vec4_t thumbColor = { 0.7f, 0.7f, 0.7f, 0.5f * alpha };
						WUI_FillRect( trackX, trackY, trackW, scrollBarH, trackColor );
						WUI_FillRect( thumbX, trackY, thumbW, scrollBarH, thumbColor );
					}
				}
			} else {
				/* vertical axis (default) */
				float rowH       = item->elementheight > 0 ? item->elementheight : 16.0f;
				int   visibleRows = (int)( itemH / rowH );
				float scrollBarW = 4.0f;
				float contentW   = itemW;
				int   row, col;
				vec4_t rowColor;

				if ( totalItems > visibleRows ) {
					contentW -= scrollBarW + 2.0f;
				}

				Text_SetLetterSpacing( letterSpacing );
				for ( row = 0; row < visibleRows && ( item->listScrollOffset + row ) < totalItems; row++ ) {
					int   dataRow = item->listScrollOffset + row;
					float rowY    = itemY + row * rowH;
					float colX    = itemX + 4;

					if ( dataRow == item->listSelectedRow ) {
						WUI_FillRect( itemX, rowY, contentW, rowH, selColor );
					}

					Vector4Copy( item->forecolor, rowColor );
					for ( col = 0; col < ( item->columns > 0 ? item->columns : 1 ); col++ ) {
						const char *text = WiredUI_FeederItemText( feederID, dataRow, col );
						float colW = ( col < item->columns && item->columnWidths[col] > 0 )
							? item->columnWidths[col] : contentW;
						if ( text && text[0] ) {
							int maxChars = (int)( ( colW - 4 ) / charSize );
							int visChars = 0, ti;
							if ( maxChars < 1 ) maxChars = 1;
							for ( ti = 0; text[ti]; ti++ ) {
								if ( Q_IsColorString( &text[ti] ) ) { ti++; continue; }
								visChars++;
							}
							if ( visChars > maxChars ) {
								char clipped[128];
								int ci = 0, vc = 0;
								for ( ti = 0; text[ti] && ci < (int)sizeof(clipped) - 1; ti++ ) {
									if ( Q_IsColorString( &text[ti] ) ) {
										clipped[ci++] = text[ti++];
										if ( text[ti] ) clipped[ci++] = text[ti];
										continue;
									}
									if ( vc >= maxChars ) break;
									clipped[ci++] = text[ti];
									vc++;
								}
								clipped[ci] = '\0';
								Text_Draw( clipped, (float)colX, (float)( rowY + 2 ), FONT_UI, charSize, rowColor, TEXT_ALIGN_LEFT, 0 );
							} else {
								Text_Draw( text, (float)colX, (float)( rowY + 2 ), FONT_UI, charSize, rowColor, TEXT_ALIGN_LEFT, 0 );
							}
						}
						colX += colW;
					}
				}
				Text_SetLetterSpacing( 0.0f );

				/* vertical scrollbar on right, macOS-style fade */
				if ( totalItems > visibleRows ) {
					float trackX      = itemX + itemW - scrollBarW - 1.0f;
					float trackY      = itemY + 1.0f;
					float trackH      = itemH - 2.0f;
					float visibleFrac = (float)visibleRows / (float)totalItems;
					float thumbH      = trackH * visibleFrac;
					float maxScroll   = (float)( totalItems - visibleRows );
					float thumbY      = trackY;
					float alpha       = 0.0f;

					if ( thumbH < 16.0f ) thumbH = 16.0f;
					if ( maxScroll > 0 ) {
						thumbY += ( trackH - thumbH ) * ( (float)item->listScrollOffset / maxScroll );
					}

					if ( item->listScrollFadeTime > 0 ) {
						int elapsed = realtime - item->listScrollFadeTime;
						if ( elapsed < 1500 ) {
							alpha = 1.0f;
						} else {
							alpha = 1.0f - (float)( elapsed - 1500 ) / 500.0f;
							if ( alpha < 0 ) alpha = 0;
						}
					}

					if ( alpha > 0 ) {
						vec4_t trackColor = { 0.3f, 0.3f, 0.3f, 0.15f * alpha };
						vec4_t thumbColor = { 0.7f, 0.7f, 0.7f, 0.5f * alpha };
						WUI_FillRect( trackX, trackY, scrollBarW, trackH, trackColor );
						WUI_FillRect( trackX, thumbY, scrollBarW, thumbH, thumbColor );
					}
				}
			}

			continue; // skip normal text rendering for listbox
		}

		// draw cvar-bound item value (right side of label)
		if ( item->cvar[0] && item->type != ITEM_TYPE_TEXT && item->type != ITEM_TYPE_BUTTON ) {
			char cvarBuf[256];
			const char *valueText = "";
			float charSize = item->fontPointSize > 0.0f ? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
			// vertical center text in rect when textaligny is not set
			float textVCenter = ( item->textaligny == 0 && itemH > charSize )
				? ( itemH - charSize ) * 0.5f : item->textaligny;
			float labelX = itemX + item->textalignx;
			float labelY = itemY + textVCenter;
			float valueX;

			// draw the label on the left
			if ( item->text[0] ) {
				Text_Draw( item->text, (float)labelX, (float)labelY, FONT_UI, charSize, item->forecolor, TEXT_ALIGN_LEFT, 0 );
			}

			// compute value text based on item type
		WiredUI_StateGetString( item->cvar, cvarBuf, sizeof( cvarBuf ) );

			switch ( item->type ) {
				case ITEM_TYPE_YESNO:
					valueText = atof( cvarBuf ) != 0 ? "Yes" : "No";
					break;

				case ITEM_TYPE_MULTI:
					/* Dynamic MULTI: populateCallback supplies the option
					 * list at render time. Branch on the callback's state
					 * so loading/empty/error/success/partial each render
					 * with intentional, non-generic visuals. */
					if ( item->populateCallback[0] ) {
						wuiPopulateCallback_t pop = WiredUI_GetPopulateCallback( item->populateCallback );
						if ( !pop ) {
							/* Callback name typo / forgot to register: surface
							 * loudly so devs notice. */
							valueText = "<missing populate callback>";
						} else {
							wuiPopulateResult_t res;
							qboolean found = qfalse;
							memset( &res, 0, sizeof( res ) );
							pop( &res );
							switch ( res.state ) {
								case WUI_POPULATE_LOADING:
									valueText = "Scanning…";
									break;
								case WUI_POPULATE_EMPTY:
									valueText = "No devices detected";
									break;
								case WUI_POPULATE_ERROR:
									valueText = "Enumeration failed — Use default";
									break;
								case WUI_POPULATE_SUCCESS:
								case WUI_POPULATE_PARTIAL:
									for ( int j = 0; j < res.count; j++ ) {
										if ( res.values && res.values[j] &&
										     !Q_stricmp( cvarBuf, res.values[j] ) ) {
											valueText = res.names[j];
											found = qtrue;
											break;
										}
									}
									if ( !found ) {
										if ( cvarBuf[0] ) {
											/* User requested a device that
											 * isn't currently present
											 * (unplugged, renamed, etc.). */
											valueText = va( "%s (not present)", cvarBuf );
										} else if ( res.count > 0 && res.names && res.names[0] ) {
											/* Empty cvar = system default —
											 * still show first option as a
											 * preview hint. */
											valueText = "(System Default)";
										} else {
											valueText = "(System Default)";
										}
									}
									break;
								default:
									valueText = cvarBuf;
									break;
							}
						}
					}
					else if ( item->multiData ) {
						qboolean found = qfalse;
						for ( int j = 0; j < item->multiData->count; j++ ) {
							if ( item->multiData->isStringList ) {
								if ( !Q_stricmp( cvarBuf, item->multiData->strValues[j] ) ) {
									valueText = item->multiData->labels[j];
									found = qtrue;
									break;
								}
							} else {
								if ( item->multiData->floatValues[j] == atof( cvarBuf ) ) {
									valueText = item->multiData->labels[j];
									found = qtrue;
									break;
								}
							}
						}
						// fallback: show raw cvar value if no option matches
						if ( !found && cvarBuf[0] ) {
							valueText = cvarBuf;
						}
					}
					break;

				case ITEM_TYPE_SLIDER:
					{
						float val = atof( cvarBuf );
						float range = item->sliderData.maxVal - item->sliderData.minVal;
						float frac = ( range > 0 ) ? ( val - item->sliderData.minVal ) / range : 0;
						float barX = itemX + itemW * 0.5f;
						float barW = itemW * 0.45f;
						float barY = itemY + itemH * 0.4f;
						float barH = 4.0f;
						vec4_t barBg = { 0.3f, 0.3f, 0.3f, 0.6f };
						vec4_t barFg = { 1.0f, 0.75f, 0.0f, 1.0f };

						if ( frac < 0 ) frac = 0;
						if ( frac > 1 ) frac = 1;

						// draw slider track
						WUI_FillRect( barX, barY, barW, barH, barBg );
						// draw slider fill
						WUI_FillRect( barX, barY, barW * frac, barH, barFg );
						// draw value as text
						valueText = va( "%.1f", val );
					}
					break;

				case ITEM_TYPE_BIND:
					{
						static char bindBuf[128];
						if ( wui_waitingForKey && wui_bindItem == item ) {
							valueText = "Press a key...";
						} else {
							// find primary + alternate keys bound to this command
							const char *key1 = NULL, *key2 = NULL;
							for ( int k = 0; k < MAX_KEYS; k++ ) {
								const char *b = Key_GetBinding( k );
								if ( b && !Q_stricmp( b, item->cvar ) ) {
									if ( !key1 ) key1 = Key_KeynumToString( k );
									else if ( !key2 ) { key2 = Key_KeynumToString( k ); break; }
								}
							}
							if ( key1 && key2 ) {
								Com_sprintf( bindBuf, sizeof( bindBuf ), "%s ^7or %s", key1, key2 );
								valueText = bindBuf;
							} else if ( key1 ) {
								valueText = key1;
							} else {
								valueText = "---";
							}
						}
					}
					break;

				case ITEM_TYPE_EDITFIELD:
				case ITEM_TYPE_NUMERICFIELD:
					if ( wui_editingField && wui_editItem == item ) {
						// show value with blinking cursor
						static char editBuf[512];
						int curPos = wui_editCursorPos;
						qboolean showCursor = ( (int)( cls.realtime / 250 ) & 1 );

						if ( curPos > (int)strlen( cvarBuf ) ) curPos = strlen( cvarBuf );
						Q_strncpyz( editBuf, cvarBuf, curPos + 1 );
						{
							qstring_t eb_qs = QS_WrapExisting( editBuf, sizeof(editBuf) );
							QS_AppendChar( &eb_qs, showCursor ? '_' : ' ' );
							QS_Append( &eb_qs, &cvarBuf[curPos] );
						}
						valueText = editBuf;
					} else {
						valueText = cvarBuf;
					}
					break;

				default:
					valueText = cvarBuf;
					break;
			}

			// draw value text after label with consistent spacing
			if ( valueText[0] ) {
				float textLen = strlen( item->text[0] ? item->text : "" ) * charSize;
				valueX = itemX + textLen + 12;
				Text_SetLetterSpacing( item->letterSpacing );
				Text_Draw( valueText, (float)valueX, (float)labelY, FONT_UI, charSize, item->forecolor, TEXT_ALIGN_LEFT, 0 );
				Text_SetLetterSpacing( 0.0f );
			}
		}
		/* draw text-only items (no cvar) — also handles storeBind text override */
		else if ( item->text[0] || item->storeBind[0] ) {
			float charSize = item->fontPointSize > 0.0f ? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
			float textVCenter = ( item->textaligny == 0 && itemH > charSize )
				? ( itemH - charSize ) * 0.5f : item->textaligny;
			float x = itemX + item->textalignx;
			float y = itemY + textVCenter;
			int drawAlign = TEXT_ALIGN_LEFT;
			const char *sourceText;
			vec4_t drawColor;

			Vector4Copy( item->forecolor, drawColor );
			sourceText = item->text;

			/* bind: override display text from store */
			if ( item->storeBind[0] ) {
				wuiStoreEntry_t *bindEntry = WiredStore_Get( item->storeBind );
				if ( bindEntry && bindEntry->text[0] ) {
					sourceText = bindEntry->text;
				} else if ( !item->bindWarned ) {
					Com_DPrintf( "WiredUI: bind key '%s' not found (item '%s')\n",
								 item->storeBind, item->name );
					item->bindWarned = qtrue;
				}
			}

			/* bindcolor: override forecolor from store */
			if ( item->storeBindColor[0] ) {
				wuiStoreEntry_t *colorEntry = WiredStore_Get( item->storeBindColor );
				if ( colorEntry ) {
					/* semantic state takes priority over raw color */
					if ( colorEntry->state[0] ) {
						if ( !WiredTheme_ResolveState( colorEntry->state, drawColor ) ) {
							/* unknown state — fall back to raw color */
							Vector4Copy( colorEntry->color, drawColor );
						}
					} else {
						Vector4Copy( colorEntry->color, drawColor );
					}
				} else if ( !item->bindWarned ) {
					Com_DPrintf( "WiredUI: bindcolor key '%s' not found (item '%s')\n",
								 item->storeBindColor, item->name );
					item->bindWarned = qtrue;
				}
			}

			if ( sourceText[0] ) {
				if ( item->textalign == ITEM_ALIGN_CENTER && itemW > 0 ) {
					x = itemX + itemW * 0.5f;
					drawAlign = TEXT_ALIGN_CENTER;
				} else if ( item->textalign == ITEM_ALIGN_RIGHT && itemW > 0 ) {
					x = itemX + itemW;
					drawAlign = TEXT_ALIGN_RIGHT;
				}

				Text_SetLetterSpacing( item->letterSpacing );
				Text_DrawClipped( sourceText, (float)x, (float)y, (float)itemW, FONT_UI, charSize, drawColor, drawAlign, 0 );
				Text_SetLetterSpacing( 0.0f );
			}
		}
	}

	// draw focus highlight on hovered/selected item
	if ( wui_focusItem >= 0 && wui_focusItem < menu->itemCount ) {
		wiredItemDef_t *focus = menu->items[wui_focusItem];
		if ( WiredUI_ItemCanFocus( focus ) ) {
			float fx = focus->resolvedRect.x;
			float fy = focus->resolvedRect.y - scrollY;
			float fw = focus->resolvedRect.w;
			float fh = focus->resolvedRect.h;
			// don't draw focus highlight outside clip area
			if ( fy + fh < clipTop || fy > clipBottom ) goto skip_focus;
			// TA-style: gradient bar behind focused item, or solid fill as fallback
			if ( wui_gradientBarShader ) {
				re.SetColor( menu->focuscolor );
				WUI_DrawPic( fx, fy, fw, fh, wui_gradientBarShader );
				re.SetColor( NULL );
			} else {
				WUI_FillRect( fx, fy, fw, fh, menu->focuscolor );
			}
			// redraw the focused item's text on top of highlight
			if ( focus->text[0] ) {
				float charSize = focus->fontPointSize > 0.0f ? focus->fontPointSize : WUI_DEFAULT_FONT_SIZE;
				float focusVCenter = ( focus->textaligny == 0 && fh > charSize )
					? ( fh - charSize ) * 0.5f : focus->textaligny;
				float x = fx + focus->textalignx;
				float y = fy + focusVCenter;
				int focusAlign = TEXT_ALIGN_LEFT;
				if ( focus->textalign == ITEM_ALIGN_CENTER && fw > 0 ) {
					x = fx + fw * 0.5f;
					focusAlign = TEXT_ALIGN_CENTER;
				} else if ( focus->textalign == ITEM_ALIGN_RIGHT && fw > 0 ) {
					x = fx + fw;
					focusAlign = TEXT_ALIGN_RIGHT;
				}
				Text_SetLetterSpacing( focus->letterSpacing );
				Text_DrawClipped( focus->text, (float)x, (float)y, (float)fw, FONT_UI, charSize, focus->forecolor, focusAlign, 0 );
				Text_SetLetterSpacing( 0.0f );
			}
		}
	}
skip_focus:

	// draw tooltip for mouse-hovered item only (ET:Legacy + QL)
	// keyboard focus does NOT show tooltips — they anchor to the cursor
	// tooltip only appears after WIRED_TOOLTIP_DELAY_MS of continuous hover
	if ( wui_focusFromMouse && wui_focusItem >= 0 && wui_focusItem < menu->itemCount ) {
		wiredItemDef_t *focus = menu->items[wui_focusItem];
		if ( focus->tooltip[0] && wui_tooltipStartTime > 0 &&
		     ( realtime - wui_tooltipStartTime ) >= WIRED_TOOLTIP_DELAY_MS ) {
			float tx = wui_cursorX + 16;
			float ty = wui_cursorY + 16;
			float tw = strlen( focus->tooltip ) * 8.0f + 8;
			float th = 16.0f;
			vec4_t tipBg = { 0.0f, 0.0f, 0.0f, 0.85f };
			vec4_t tipFg = { 1.0f, 1.0f, 1.0f, 0.95f };

			// keep tooltip on screen
			if ( tx + tw > (float)cls.glconfig.vidWidth ) tx = (float)cls.glconfig.vidWidth - tw;
			if ( ty + th > (float)cls.glconfig.vidHeight ) ty = wui_cursorY - th - 4;

			WUI_FillRect( tx, ty, tw, th, tipBg );
			Text_Draw( focus->tooltip, (float)(tx + 4), (float)(ty + 4), FONT_UI, 8.0f, tipFg, TEXT_ALIGN_LEFT, 0 );
		}
	}

	// macOS-style scrollbar — thin, semi-transparent, fades out
	{
		float maxScroll = menu->contentHeight - menuH;
		if ( maxScroll > 0 ) {
			float scrollBarWidth = 4.0f;
			float scrollBarPadding = 2.0f;
			float trackX = menuX + menuW - scrollBarWidth - scrollBarPadding;
			float trackY = menuY + scrollBarPadding;
			float trackH = menuH - scrollBarPadding * 2;

			// thumb size proportional to visible/content ratio
			float visibleFrac = menuH / menu->contentHeight;
			float thumbH = trackH * visibleFrac;
			if ( thumbH < 20.0f ) thumbH = 20.0f;
			float thumbY = trackY + ( trackH - thumbH ) * ( scrollY / maxScroll );

			// fade out after 1.5 seconds of inactivity
			float alpha = 1.0f;
			if ( menu->scrollBarFadeTime > 0 ) {
				int elapsed = realtime - menu->scrollBarFadeTime;
				if ( elapsed > 1500 ) {
					alpha = 1.0f - ( elapsed - 1500 ) / 500.0f;
					if ( alpha < 0 ) alpha = 0;
				}
			} else {
				alpha = 0; // never scrolled — don't show
			}

			if ( alpha > 0 ) {
				vec4_t trackColor = { 0.3f, 0.3f, 0.3f, 0.15f * alpha };
				vec4_t thumbColor = { 0.7f, 0.7f, 0.7f, 0.5f * alpha };

				WUI_FillRect( trackX, trackY, scrollBarWidth, trackH, trackColor );
				WUI_FillRect( trackX, thumbY, scrollBarWidth, thumbH, thumbColor );
			}
		}
	}

	// Layer 5: visual layout debug overlay
	WiredUI_DrawDebugOverlay( menu );

	WiredUI_DrawMultiDropdown( menu );

	if ( Key_GetCatcher() & KEYCATCH_UI ) {
		vec4_t cursorTint = { 0.85f, 0.55f, 0.1f, 1.0f };
		if ( wui_cursorShader ) {
			re.SetColor( cursorTint );
			WUI_DrawPic( wui_cursorX - 16, wui_cursorY - 16, 32, 32, wui_cursorShader );
			re.SetColor( NULL );
		} else {
			vec4_t cursorColor = { 0.85f, 0.55f, 0.1f, 1.0f };
			re.SetColor( cursorColor );
			WUI_FillRect( wui_cursorX - 1, wui_cursorY - 8, 2, 16, cursorColor );
			WUI_FillRect( wui_cursorX - 8, wui_cursorY - 1, 16, 2, cursorColor );
			re.SetColor( NULL );
		}
	}

	// Phase 2 rendering parity integrated for borders/teamcolor/model items.
}

// ── script command system ─────────────────────────────────────────────
//
// Based on q3now's ui_script.c command table pattern. Same handler
// signature (name + numArgs + args) but adapted for Wired UI structs.
//
// Key difference from v6: unknown commands pass to the engine console
// via Cbuf_ExecuteText instead of being silently dropped. This means
// any console command works as a script action without hardcoding.

#define WIRED_MAX_SCRIPT_ARGS  8

// forward declarations
static wiredItemDef_t *WiredUI_FindItemByName( wiredMenuDef_t *menu, const char *name );
static void WiredUI_ForEachItemByNameOrGroup( wiredMenuDef_t *menu, const char *name,
	void (*callback)( wiredItemDef_t *item, void *data ), void *data );
static void WiredUI_RunScript( wiredMenuDef_t *menu, wiredItemDef_t *item, const char *script );

typedef void (*wiredScriptHandler_t)( wiredMenuDef_t *menu, wiredItemDef_t *item,
                                       int numArgs, const char **args );

typedef struct {
	const char            *name;
	wiredScriptHandler_t   handler;
} wiredScriptCommand_t;

// ── script handlers ───────────────────────────────────────────────────

static void WiredScript_Show_Callback( wiredItemDef_t *target, void *data ) {
	(void)data;
	target->visible = qtrue;
}

static void WiredScript_Hide_Callback( wiredItemDef_t *target, void *data ) {
	(void)data;
	target->visible = qfalse;
}

static void WiredScript_Show( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	(void)item;
	if ( numArgs < 1 ) return;
	WiredUI_ForEachItemByNameOrGroup( menu, args[0], WiredScript_Show_Callback, NULL );
}

static void WiredScript_Hide( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	(void)item;
	if ( numArgs < 1 ) return;
	WiredUI_ForEachItemByNameOrGroup( menu, args[0], WiredScript_Hide_Callback, NULL );
}

static void WiredScript_Open( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs < 1 ) return;
	WiredUI_PushMenu( args[0] );
}

static void WiredScript_Close( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	WiredUI_PopMenu();
}

static void WiredScript_SetCvar( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 2 ) {
		if ( WiredUI_IsStoreStateKey( args[0] ) ) {
			Com_Printf( S_COLOR_YELLOW "WiredUI: setcvar '%s' targets store-backed UI state; use setstate\n", args[0] );
		}
		Cvar_Set( args[0], args[1] );
	}
}

static void WiredScript_SetState( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wuiStoreEntry_t *entry;

	if ( numArgs < 2 || !args[0][0] ) {
		return;
	}

	entry = WiredStore_Set( args[0] );
	if ( !entry ) {
		return;
	}

	Q_strncpyz( entry->text, args[1], sizeof( entry->text ) );
	entry->value = (float)atof( entry->text );
	entry->flags |= WUI_STORE_FLAG_DIRTY;
}

static void WiredScript_SaveState( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	(void)menu;
	(void)item;
	(void)numArgs;
	(void)args;

	if ( !WiredUI_CallLuaStoreFunction( "savestate" ) ) {
		WiredUI_SaveState();
	}
}

static void WiredScript_LoadState( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	(void)menu;
	(void)item;
	(void)numArgs;
	(void)args;

	if ( !WiredUI_CallLuaStoreFunction( "loadstate" ) ) {
		WiredUI_LoadState();
	}
}

static void WiredScript_Exec( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		Cbuf_ExecuteText( EXEC_APPEND, va( "%s\n", args[0] ) );
	}
}

// execConfirm: execute the command stored in ui_confirmAction state key.
// Used by the generic confirm dialog's Yes button.
static void WiredScript_ExecConfirm( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char actionBuf[256];
	WiredUI_StateGetString( "ui_confirmAction", actionBuf, sizeof( actionBuf ) );
	if ( actionBuf[0] ) {
		Cbuf_ExecuteText( EXEC_APPEND, va( "%s\n", actionBuf ) );
	}
}

static void WiredScript_Play( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		sfxHandle_t sfx = S_RegisterSound( args[0], qfalse );
		if ( sfx ) S_StartLocalSound( sfx, CHAN_LOCAL_SOUND );
	}
}

static void WiredScript_PlayLooped( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 1 ) {
		S_StartBackgroundTrack( args[0], args[0] );
	}
}

static void WiredScript_StopMusic( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	S_StopBackgroundTrack();
}

static void WiredScript_FadeIn( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		wiredAssetGlobals_t *ag = WiredUI_GetAssetGlobals();
		target->visible = qtrue;
		target->fadeAlphaItem = 0.0f;
		target->fadeTargetAlpha = 1.0f;
		target->fadeStartTime = cls.realtime;
		// derive duration from assetGlobalDef: (fadeClamp / fadeAmount) * fadeCycle ms
		if ( ag->fadeAmount > 0 && ag->fadeCycle > 0 ) {
			target->fadeDurationItem = (int)( ( ag->fadeClamp / ag->fadeAmount ) * ag->fadeCycle );
		} else {
			target->fadeDurationItem = 150;
		}
	}
}

static void WiredScript_FadeOut( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		wiredAssetGlobals_t *ag = WiredUI_GetAssetGlobals();
		target->fadeAlphaItem = 1.0f;
		target->fadeTargetAlpha = 0.0f;
		target->fadeStartTime = cls.realtime;
		if ( ag->fadeAmount > 0 && ag->fadeCycle > 0 ) {
			target->fadeDurationItem = (int)( ( ag->fadeClamp / ag->fadeAmount ) * ag->fadeCycle );
		} else {
			target->fadeDurationItem = 150;
		}
	}
}

static void WiredScript_SetFocus( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs < 1 ) return;
	for ( int i = 0; i < menu->itemCount; i++ ) {
		if ( !Q_stricmp( menu->items[i]->name, args[0] ) ) {
			wui_focusItem = i;
			break;
		}
	}
}

// forward declaration (non-static — also called from cl_wui_feeders.c)
void WiredUI_UpdateMapPoolButton( void );

// ── per-gametype cvar persistence ─────────────────────────────────────
// Saves/restores scorelimit, timelimit, friendlyfire when
// switching game types — same pattern as q3_ui's ServerOptions_Cache.

int wui_lastSavedGameType = -1;

typedef struct {
	const char *name;
	qboolean teamOnly;
} wiredGameTypePersistField_t;

static const wiredGameTypePersistField_t wui_gameTypePersistCvars[] = {
	{ "g_scorelimit", qfalse },
	{ "g_timelimit", qfalse },
	{ "sv_maxclients", qfalse },
	{ "sv_pure", qfalse },
	{ "sv_allowDownload", qfalse },
	{ "g_spSkill", qfalse },
	{ "g_autoBots", qfalse },
	{ "g_minPlayers", qfalse },
	{ "g_friendlyfire", qtrue },
	{ "g_teamForceBalance", qtrue },
	{ "g_allowvote", qtrue },
	{ "g_localTeamPref", qtrue },
	{ NULL, qfalse }
};

static const wiredGameTypePersistField_t wui_gameTypePersistStateKeys[] = {
	{ "ui_botCount", qfalse },
	{ "ui_dedicated", qfalse },
	{ NULL, qfalse }
};

static const char *WiredUI_GameTypeProfilePrefix( int gt ) {
	switch ( gt ) {
		case 0: return "ui_gt_dm";
		case 1: return "ui_gt_duel";
		case 2: return "ui_gt_koth";
		case 3: return "ui_gt_lms";
		case 4: return "ui_gt_tdm";
		case 5: return "ui_gt_ctf";
		default: return NULL;
	}
}

static qboolean WiredUI_ProfileGetString( const char *key, char *out, int outSize ) {
	wuiStoreEntry_t *entry;

	if ( !key || !key[0] || !out || outSize <= 0 ) {
		return qfalse;
	}

	out[0] = '\0';
	entry = WiredStore_Get( key );
	if ( !entry || !entry->text[0] ) {
		return qfalse;
	}

	Q_strncpyz( out, entry->text, outSize );
	return qtrue;
}

static void WiredUI_ProfileSetString( const char *key, const char *value ) {
	wuiStoreEntry_t *entry;

	if ( !key || !key[0] ) {
		return;
	}

	entry = WiredStore_Set( key );
	if ( !entry ) {
		return;
	}

	Q_strncpyz( entry->text, value ? value : "", sizeof( entry->text ) );
	entry->value = (float)atof( entry->text );
	entry->flags |= WUI_STORE_FLAG_DIRTY;
}

static void WiredUI_SaveGameTypeSettingsFor( int gt ) {
	const char *prefix;
	const wiredGameTypePersistField_t *it;
	char key[128];

	prefix = WiredUI_GameTypeProfilePrefix( gt );
	if ( !prefix ) {
		return;
	}

	for ( it = wui_gameTypePersistCvars; it->name; it++ ) {
		if ( it->teamOnly && gt < 4 ) {
			continue;
		}
		Com_sprintf( key, sizeof( key ), "%s_%s", prefix, it->name );
		WiredUI_ProfileSetString( key, Cvar_VariableString( it->name ) );
	}

	for ( it = wui_gameTypePersistStateKeys; it->name; it++ ) {
		char value[128];
		if ( it->teamOnly && gt < 4 ) {
			continue;
		}
		Com_sprintf( key, sizeof( key ), "%s_state_%s", prefix, it->name );
		WiredUI_StateGetString( it->name, value, sizeof( value ) );
		WiredUI_ProfileSetString( key, value );
	}

	// save map rotation per gametype
	{
		char saveBuf[1024];
		Com_sprintf( key, sizeof( key ), "%s_g_maprotation", prefix );
		WiredUI_GetMapRotation( saveBuf, sizeof( saveBuf ) );
		WiredUI_ProfileSetString( key, saveBuf );
	}
}

static void WiredUI_LoadGameTypeSettingsFor( int gt ) {
	const char *prefix;
	const wiredGameTypePersistField_t *it;
	char key[128];
	char buf[1024];

	prefix = WiredUI_GameTypeProfilePrefix( gt );
	if ( !prefix ) {
		return;
	}

	for ( it = wui_gameTypePersistCvars; it->name; it++ ) {
		if ( it->teamOnly && gt < 4 ) {
			continue;
		}
		Com_sprintf( key, sizeof( key ), "%s_%s", prefix, it->name );
		if ( WiredUI_ProfileGetString( key, buf, sizeof( buf ) ) ) {
			Cvar_Set( it->name, buf );
		}
	}

	for ( it = wui_gameTypePersistStateKeys; it->name; it++ ) {
		if ( it->teamOnly && gt < 4 ) {
			continue;
		}
		Com_sprintf( key, sizeof( key ), "%s_state_%s", prefix, it->name );
		if ( WiredUI_ProfileGetString( key, buf, sizeof( buf ) ) ) {
			WiredUI_StateSetString( it->name, buf );
		}
	}

	// restore map rotation for this gametype (missing profile -> empty rotation)
	Com_sprintf( key, sizeof( key ), "%s_g_maprotation", prefix );
	if ( WiredUI_ProfileGetString( key, buf, sizeof( buf ) ) ) {
		Cvar_Set( "g_maprotation", buf );
	} else {
		Cvar_Set( "g_maprotation", "" );
	}
	Cvar_Set( "g_maprotationIndex", "0" );
}

// Called from uiScript: uiScript UpdateGameType
static void WiredScript_UpdateGameType( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int gt = WiredUI_StateGetInt( "ui_netGameType" );

	if ( wui_lastSavedGameType == -1 ) {
		// first entry — load persisted profile for this gametype (if any)
		wui_lastSavedGameType = gt;
		WiredUI_LoadGameTypeSettingsFor( gt );
	} else if ( gt != wui_lastSavedGameType ) {
		// gametype changed — save old, load new
		WiredUI_SaveGameTypeSettingsFor( wui_lastSavedGameType );
		WiredUI_LoadGameTypeSettingsFor( gt );
		wui_lastSavedGameType = gt;
	}

	WiredUI_UpdateMapPoolButton();
}

// ── map pool helpers ──────────────────────────────────────────────────

void WiredUI_UpdateMapPoolButton( void ) {
	char mapName[MAX_QPATH];
	char rotation[1024];
	char token[MAX_QPATH];
	const char *p;
	qboolean inPool = qfalse;
	int count = 0;

	WiredUI_StateGetString( "ui_selectedMap", mapName, sizeof( mapName ) );
	WiredUI_GetMapRotation( rotation, sizeof( rotation ) );

	p = rotation;
	while ( *p ) {
		int ti = 0;
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		while ( *p && *p != ' ' && ti < (int)sizeof(token) - 1 ) token[ti++] = *p++;
		token[ti] = '\0';
		count++;
		if ( mapName[0] && !Q_stricmp( token, mapName ) ) inPool = qtrue;
	}

	WiredUI_StateSetString( "ui_mapPoolAction", inPool ? "Remove from Pool" : "Add to Pool" );
	WiredUI_StateSetString( "ui_mapPoolStatus", count > 0
		? va( "^2%d maps in pool", count )
		: "Single map (no rotation)" );
}

// ── map rotation (map pool) ──────────────────────────────────────────

static void WiredScript_ToggleMapPool( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char mapName[MAX_QPATH];
	char rotation[1024];
	char newRotation[1024];
	char token[MAX_QPATH];
	const char *p;
	qboolean found = qfalse;

	WiredUI_StateGetString( "ui_selectedMap", mapName, sizeof( mapName ) );
	if ( !mapName[0] ) {
		Com_Printf( "WiredUI: ToggleMapPool — no map selected\n" );
		return;
	}

	Com_Printf( "WiredUI: ToggleMapPool '%s'\n", mapName );
	WiredUI_GetMapRotation( rotation, sizeof( rotation ) );

	// rebuild rotation without the selected map (or add it if not found)
	newRotation[0] = '\0';
	int len = 0;
	p = rotation;
	while ( *p ) {
		int i = 0;
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		while ( *p && *p != ' ' && i < (int)sizeof(token) - 1 ) token[i++] = *p++;
		token[i] = '\0';
		if ( !Q_stricmp( token, mapName ) ) {
			found = qtrue;
			continue;  // remove
		}
		if ( len > 0 ) { newRotation[len++] = ' '; newRotation[len] = '\0'; }
		Q_strncpyz( newRotation + len, token, sizeof(newRotation) - len );
		len = strlen( newRotation );
	}

	if ( !found ) {
		if ( len > 0 ) { newRotation[len++] = ' '; newRotation[len] = '\0'; }
		Q_strncpyz( newRotation + len, mapName, sizeof(newRotation) - len );
	}

	Cvar_Set( "g_maprotation", newRotation );
	Cvar_Set( "g_maprotationIndex", "0" );

	WiredUI_UpdateMapPoolButton();
}

static void WiredScript_ClearMapPool( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	Cvar_Set( "g_maprotation", "" );
	Cvar_Set( "g_maprotationIndex", "0" );
	WiredUI_UpdateMapPoolButton();
}

// ── favorite maps ────────────────────────────────────────────────────
// Stored in Wired Store key ui_favoriteMaps.

static void WiredScript_ToggleFavoriteMap( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char mapName[MAX_QPATH];
	char favs[2048];
	char newFavs[2048];
	char token[MAX_QPATH];
	WiredUI_StateGetString( "ui_selectedMap", mapName, sizeof( mapName ) );
	if ( !mapName[0] ) return;

	WiredUI_StateGetString( "ui_favoriteMaps", favs, sizeof( favs ) );

	// rebuild without the map, or add it
	newFavs[0] = '\0';
	qboolean found = qfalse;
	int len = 0;
	const char *p = favs;
	while ( *p ) {
		int i = 0;
		while ( *p == ' ' ) p++;
		if ( !*p ) break;
		while ( *p && *p != ' ' && i < (int)sizeof(token) - 1 ) token[i++] = *p++;
		token[i] = '\0';
		if ( !Q_stricmp( token, mapName ) ) {
			found = qtrue;
			continue;
		}
		if ( len > 0 ) { newFavs[len++] = ' '; newFavs[len] = '\0'; }
		Q_strncpyz( newFavs + len, token, sizeof(newFavs) - len );
		len = strlen( newFavs );
	}

	if ( !found ) {
		if ( len > 0 ) { newFavs[len++] = ' '; newFavs[len] = '\0'; }
		Q_strncpyz( newFavs + len, mapName, sizeof(newFavs) - len );
	}

	WiredUI_StateSetString( "ui_favoriteMaps", newFavs );

	// update button label
	WiredUI_StateSetString( "ui_favMapAction", found ? "Favorite" : "Unfavorite" );
}

// update favorite button based on selected map
void WiredUI_UpdateFavoriteButton( void ) {
	char mapName[MAX_QPATH];
	char favs[2048];

	WiredUI_StateGetString( "ui_selectedMap", mapName, sizeof( mapName ) );
	WiredUI_StateGetString( "ui_favoriteMaps", favs, sizeof( favs ) );

	if ( mapName[0] ) {
		extern qboolean WiredFeeder_IsMapInList( const char *list, const char *mapName );
		WiredUI_StateSetString( "ui_favMapAction",
			WiredFeeder_IsMapInList( favs, mapName ) ? "Unfavorite" : "Favorite" );
	} else {
		WiredUI_StateSetString( "ui_favMapAction", "Favorite" );
	}
}

// ── game action handlers ──────────────────────────────────────────────
// These read cvars set by feeder selection callbacks and execute real game actions.

static void WiredScript_StartServer( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char mapName[MAX_QPATH];

	// use first map from rotation pool if set, otherwise selected map
	{
		char rotation[1024];
		WiredUI_GetMapRotation( rotation, sizeof( rotation ) );
		if ( rotation[0] ) {
			// extract first map from rotation for initial launch
			int k = 0;
			const char *r = rotation;
			while ( *r == ' ' ) r++;
			while ( *r && *r != ' ' && k < (int)sizeof(mapName) - 1 ) mapName[k++] = *r++;
			mapName[k] = '\0';
			Cvar_Set( "g_maprotationIndex", "0" );
		} else {
			WiredUI_StateGetString( "ui_selectedMap", mapName, sizeof( mapName ) );
		}
	}
	if ( !mapName[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no map selected\n" );
		return;
	}

	// save per-gametype settings before launch
	WiredUI_SaveGameTypeSettingsFor( WiredUI_StateGetInt( "ui_netGameType" ) );
	if ( !WiredUI_CallLuaStoreFunction( "savestate" ) ) {
		WiredUI_SaveState();
	}

	// apply server cvars from menu selections
	// cvar-bound items (scorelimit, timelimit, sv_maxclients,
	// sv_pure, sv_allowDownload, g_minPlayers, g_friendlyfire,
	// g_teamForceBalance, g_allowvote) are set directly by menu items
	{
		char uiGameType[64];
		WiredUI_StateGetString( "ui_netGameType", uiGameType, sizeof( uiGameType ) );
		Cvar_Set( "g_gametype", uiGameType );
	}

	// dedicated mode
	if ( WiredUI_StateGetInt( "ui_dedicated" ) <= 0 ) {
		Cvar_Set( "dedicated", "0" );
	} else {
		char uiDedicated[64];
		WiredUI_StateGetString( "ui_dedicated", uiDedicated, sizeof( uiDedicated ) );
		Cvar_Set( "dedicated", uiDedicated );
	}

	// ensure sv_maxclients can accommodate g_minPlayers
	{
		int minPlayers = Cvar_VariableIntegerValue( "g_minPlayers" );
		int maxclients = Cvar_VariableIntegerValue( "sv_maxclients" );
		if ( maxclients < minPlayers ) {
			Cvar_SetIntegerValue( "sv_maxclients", minPlayers );
		}
	}

	WiredUI_CloseAllMenus();

	// launch map — g_autoBots will handle bot population server-side
	Cbuf_ExecuteText( EXEC_APPEND, va( "wait ; wait ; map %s\n", mapName ) );

	// team preference for human player in team modes
	{
		int gt = WiredUI_StateGetInt( "ui_netGameType" );
		int teamPref = Cvar_VariableIntegerValue( "g_localTeamPref" );
		if ( gt >= 4 && teamPref > 0 ) {
			const char *team = ( teamPref == 1 ) ? "red" : "blue";
			Cbuf_ExecuteText( EXEC_APPEND, va( "wait 5 ; team %s\n", team ) );
		}
	}
}

static void WiredScript_JoinServer( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char addr[256];
	char password[256];

	WiredUI_StateGetString( "ui_selectedServerAddr", addr, sizeof( addr ) );
	if ( !addr[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no server selected\n" );
		return;
	}

	// check if server requires password and none is set
	{
		// look up the server by address to check g_needpass
		int uiSource = WiredUI_StateGetInt( "ui_netSource" );
		serverInfo_t *servers = ( uiSource == 0 ) ? cls.localServers :
		                        ( uiSource == 6 ) ? cls.favoriteServers : cls.globalServers;
		int count = ( uiSource == 0 ) ? cls.numlocalservers :
		            ( uiSource == 6 ) ? cls.numfavoriteservers : cls.numglobalservers;
		for ( int j = 0; j < count; j++ ) {
			if ( !Q_stricmp( NET_AdrToStringwPort( &servers[j].adr ), addr ) ) {
				if ( servers[j].g_needpass ) {
					Cvar_VariableStringBuffer( "password", password, sizeof( password ) );
					if ( !password[0] ) {
						WiredUI_PushMenu( "password" );
						return;
					}
				}
				break;
			}
		}
	}

	WiredUI_CloseAllMenus();
	Cbuf_ExecuteText( EXEC_APPEND, va( "connect %s\n", addr ) );
}

static void WiredScript_RunDemo( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char demoName[MAX_QPATH];

	WiredUI_StateGetString( "ui_selectedDemo", demoName, sizeof( demoName ) );
	if ( !demoName[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no demo selected\n" );
		return;
	}

	WiredUI_CloseAllMenus();
	Cbuf_ExecuteText( EXEC_APPEND, va( "demo %s\n", demoName ) );
}

// ── updateMapPreview ─────────────────────────────────────────────────
// Updates a named item's background with the levelshot of the selected map.
// Usage from .menu: action { updateMapPreview "mappreview" }
static void WiredScript_UpdateMapPreview( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char mapName[MAX_QPATH];
	const char *targetName = ( numArgs >= 1 ) ? args[0] : "mappreview";
	wiredItemDef_t *target;

	WiredUI_StateGetString( "ui_selectedMap", mapName, sizeof( mapName ) );
	target = WiredUI_FindItemByName( menu, targetName );

	if ( target && mapName[0] ) {
		Com_sprintf( target->background, sizeof( target->background ), "levelshots/%s", mapName );
	} else if ( target ) {
		target->background[0] = '\0';
	}
}

static void WiredScript_RunMod( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char modName[MAX_QPATH];

	WiredUI_StateGetString( "ui_selectedMod", modName, sizeof( modName ) );
	if ( !modName[0] ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: no mod selected\n" );
		return;
	}

	WiredUI_CloseAllMenus();

	// "baseq3" means return to base game — clear fs_game
	if ( Q_stricmp( modName, "baseq3" ) == 0 ) {
		Cvar_Set( "fs_game", "" );
	} else {
		Cvar_Set( "fs_game", modName );
	}
	Cbuf_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

static void WiredScript_RefreshServers( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int source = WiredUI_StateGetInt( "ui_netSource" );
	qtime_t qt;

	// always query local servers too — they appear instantly
	Cbuf_ExecuteText( EXEC_APPEND, "localservers\n" );

	if ( source > 0 && source < 6 ) {
		// query internet master servers
		Cbuf_ExecuteText( EXEC_APPEND, va( "globalservers %d %d\n", source - 1, PROTOCOL_VERSION ) );
	}

	// record refresh timestamp for UI display
	Com_RealTime( &qt );
	WiredUI_StateSetString( "ui_lastRefreshDate", va( "%02d:%02d:%02d", qt.tm_hour, qt.tm_min, qt.tm_sec ) );
}

static void WiredScript_RefreshFilter( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	extern void WiredFeeder_RebuildServerDisplayList( void );
	WiredFeeder_RebuildServerDisplayList();
}

// ── conditionalScript ─────────────────────────────────────────────────
// ET:Legacy syntax: conditionalScript cvarname mode ( "action_true" ) ( "action_false" )
// mode: 0 = if cvar == 0 run first, else second
//       2/3 = cvar test mode (same logic, historical)
static void WiredScript_ConditionalScript( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	const char *trueAction = NULL;
	const char *falseAction = NULL;
	if ( numArgs < 4 ) return;  // cvarname mode ( "action" ) ...

	int cvarVal = Cvar_VariableIntegerValue( args[0] );

	// find the two ( "action" ) blocks in the args
	for ( int i = 2; i < numArgs; i++ ) {
		if ( !Q_stricmp( args[i], "(" ) && i + 1 < numArgs ) {
			if ( !trueAction ) {
				trueAction = args[i + 1];
			} else if ( !falseAction ) {
				falseAction = args[i + 1];
			}
		}
	}

	if ( !trueAction ) return;

	// mode 0: cvar == 0 → trueAction, else falseAction
	// mode 2/3: same logic (cvar is tested as boolean)
	if ( cvarVal == 0 ) {
		WiredUI_RunScript( menu, item, trueAction );
	} else if ( falseAction ) {
		WiredUI_RunScript( menu, item, falseAction );
	}
}

// ── setitemcolor ──────────────────────────────────────────────────────
// Syntax: setitemcolor "nameOrGroup" forecolor|backcolor R G B A
// Used everywhere for hover effects in Q3:TA/QL/OA/ET:L

typedef struct {
	const char *property;
	float      color[4];
} setItemColorData_t;

static void WiredScript_SetItemColor_Callback( wiredItemDef_t *item, void *data ) {
	setItemColorData_t *d = (setItemColorData_t *)data;
	if ( !Q_stricmp( d->property, "forecolor" ) ) {
		Vector4Copy( d->color, item->forecolor );
	} else if ( !Q_stricmp( d->property, "backcolor" ) ) {
		Vector4Copy( d->color, item->backcolor );
	} else if ( !Q_stricmp( d->property, "bordercolor" ) ) {
		Vector4Copy( d->color, item->bordercolor );
	}
}

static void WiredScript_SetItemColor( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	setItemColorData_t data;
	if ( numArgs < 6 ) return;  // name property R G B A
	data.property = args[1];
	data.color[0] = atof( args[2] );
	data.color[1] = atof( args[3] );
	data.color[2] = atof( args[4] );
	data.color[3] = atof( args[5] );
	WiredUI_ForEachItemByNameOrGroup( menu, args[0], WiredScript_SetItemColor_Callback, &data );
}

// setcolor — alias for setitemcolor (some files use one, some the other)
static void WiredScript_SetColor( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	WiredScript_SetItemColor( menu, item, numArgs, args );
}

// ── conditionalopen ──────────────────────────────────────────────────
// Syntax: conditionalopen "cvar" "menuIfTrue" "menuIfFalse"
static void WiredScript_ConditionalOpen( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs < 3 ) return;
	if ( Cvar_VariableIntegerValue( args[0] ) != 0 ) {
		WiredUI_PushMenu( args[1] );
	} else {
		WiredUI_PushMenu( args[2] );
	}
}

// ── transition ───────────────────────────────────────────────────────
// Syntax: transition "name" x1 y1 w1 h1 x2 y2 w2 h2 steps duration
// Animated rect interpolation over time
static void WiredScript_Transition( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 9 ) return;  // name x1 y1 w1 h1 x2 y2 w2 h2 [steps] [duration]
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		target->transFrom.x = atof( args[1] );
		target->transFrom.y = atof( args[2] );
		target->transFrom.w = atof( args[3] );
		target->transFrom.h = atof( args[4] );
		target->transTo.x   = atof( args[5] );
		target->transTo.y   = atof( args[6] );
		target->transTo.w   = atof( args[7] );
		target->transTo.h   = atof( args[8] );
		// steps (args[9]) and duration (args[10]) — v6 uses steps*frametime
		// we use duration in ms directly; if only steps given, estimate 16ms/step
		if ( numArgs >= 11 ) {
			target->transDuration = atoi( args[9] ) * atoi( args[10] );
		} else if ( numArgs >= 10 ) {
			target->transDuration = atoi( args[9] ) * 16;  // ~60fps
		} else {
			target->transDuration = 200;  // default 200ms
		}
		if ( target->transDuration < 1 ) target->transDuration = 1;
		target->transStartTime = cls.realtime;
		// set initial position
		target->rect = target->transFrom;
	}
}

// ── sort commands (direct, not via uiScript) ─────────────────────────

static void WiredScript_MapSortCmd( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	extern void WiredFeeder_SortMaps( int column );
	int col = ( numArgs >= 1 ) ? atoi( args[0] ) : 0;
	Com_Printf( "WiredUI: MapSort column %d\n", col );
	WiredFeeder_SortMaps( col );
}

static void WiredScript_ServerSortCmd( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	extern void WiredFeeder_SortServers( int column );
	int col = ( numArgs >= 1 ) ? atoi( args[0] ) : 0;
	WiredFeeder_SortServers( col );
}

// ── setbackground ────────────────────────────────────────────────────
// Syntax: setbackground "shader"
static void WiredScript_SetBackground( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wiredItemDef_t *target;
	if ( numArgs < 1 ) return;
	target = WiredUI_FindItemByName( menu, args[0] );
	if ( target ) {
		Q_strncpyz( target->background, args[0], sizeof( target->background ) );
	}
}

// ── player model cycling ──────────────────────────────────────────────

extern int         WiredFeeder_GetModelCount( void );
extern int         WiredFeeder_GetModelSelected( void );
extern const char *WiredFeeder_GetModelName( int index );
extern void        WiredFeeder_SetModelSelected( int index );

static void WiredScript_PrevPlayerModel( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int count = WiredFeeder_GetModelCount();
	if ( count < 1 ) return;
	int sel = WiredFeeder_GetModelSelected();
	sel = ( sel <= 0 ) ? count - 1 : sel - 1;
	WiredFeeder_SetModelSelected( sel );
}

static void WiredScript_NextPlayerModel( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int count = WiredFeeder_GetModelCount();
	if ( count < 1 ) return;
	int sel = WiredFeeder_GetModelSelected();
	sel = ( sel >= count - 1 ) ? 0 : sel + 1;
	WiredFeeder_SetModelSelected( sel );
}

// ── uiScript ─────────────────────────────────────────────────────────
// Maps v6 uiScript command names to our existing handlers.
// Q3:TA/QL/OA/ET:L all use: action { uiScript StartServer } etc.

typedef struct {
	const char *name;
	void (*handler)( wiredMenuDef_t *, wiredItemDef_t *, int, const char ** );
} wiredUiScriptEntry_t;

static const wiredUiScriptEntry_t wiredUiScripts[] = {
	{ "StartServer",      WiredScript_StartServer },
	{ "startserver",      WiredScript_StartServer },
	{ "JoinServer",       WiredScript_JoinServer },
	{ "joinserver",       WiredScript_JoinServer },
	{ "RunDemo",          WiredScript_RunDemo },
	{ "rundemo",          WiredScript_RunDemo },
	{ "RunMod",           WiredScript_RunMod },
	{ "runmod",           WiredScript_RunMod },
	{ "LoadDemos",        NULL },  // feeders auto-load, noop
	{ "LoadMods",         NULL },
	{ "LoadMovies",       NULL },
	{ "RefreshServers",   WiredScript_RefreshServers },
	{ "RefreshFilter",    WiredScript_RefreshFilter },
	{ "StopRefresh",      NULL },  // noop — server queries are fire-and-forget
	{ "closeJoin",        NULL },
	{ "closeingame",      NULL },
	{ "prevPlayerModel",  WiredScript_PrevPlayerModel },
	{ "nextPlayerModel",  WiredScript_NextPlayerModel },
	{ NULL, NULL }
};

static void WiredScript_UiScript( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs < 1 ) return;

	for ( int i = 0; wiredUiScripts[i].name; i++ ) {
		if ( !Q_stricmp( args[0], wiredUiScripts[i].name ) ) {
			if ( wiredUiScripts[i].handler ) {
				// pass remaining args (skip the uiScript command name)
				wiredUiScripts[i].handler( menu, item, numArgs - 1, numArgs > 1 ? &args[1] : NULL );
			}
			return;
		}
	}

	// common actions that map directly to console commands
	if ( !Q_stricmp( args[0], "Quit" ) || !Q_stricmp( args[0], "quit" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
	} else if ( !Q_stricmp( args[0], "Leave" ) || !Q_stricmp( args[0], "leave" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "disconnect\n" );
	} else if ( !Q_stricmp( args[0], "resetDefaults" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "exec default.cfg\n" );
	} else if ( !Q_stricmp( args[0], "Controls" ) ) {
		WiredUI_PushMenu( "controls" );
	} else if ( !Q_stricmp( args[0], "clearError" ) ) {
		// noop
	} else if ( !Q_stricmp( args[0], "ServerSort" ) && numArgs >= 2 ) {
		extern void WiredFeeder_SortServers( int column );
		WiredFeeder_SortServers( atoi( args[1] ) );
	} else if ( !Q_stricmp( args[0], "MapSort" ) && numArgs >= 2 ) {
		extern void WiredFeeder_SortMaps( int column );
		Com_Printf( "WiredUI: MapSort column %s\n", args[1] );
		WiredFeeder_SortMaps( atoi( args[1] ) );
	} else if ( !Q_stricmp( args[0], "addFavorite" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "addFavorite\n" );
	} else if ( !Q_stricmp( args[0], "deleteFavorite" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "deleteFavorite\n" );
	} else {
		Com_Printf( S_COLOR_YELLOW "WiredUI: unknown uiScript '%s'\n", args[0] );
	}
}

// Copy a value to the system clipboard.
//   clipboard cvar:<name>    — reads cvar and copies its value
//   clipboard state:<key>    — reads Wired UI state dict and copies its value
// The cvar/state prefix avoids ambiguity with literal strings.
static void WiredScript_Clipboard( wiredMenuDef_t *menu, wiredItemDef_t *item,
                                    int numArgs, const char **args )
{
	static char buf[1024];
	const char *src = "";

	(void)menu;
	(void)item;

	if ( numArgs < 1 || !args || !args[0] )
		return;

	if ( !Q_stricmpn( args[0], "cvar:", 5 ) ) {
		src = Cvar_VariableString( args[0] + 5 );
	} else if ( !Q_stricmpn( args[0], "state:", 6 ) ) {
		WiredUI_StateGetString( args[0] + 6, buf, sizeof(buf) );
		src = buf;
	} else {
		// literal string — copy it directly
		src = args[0];
	}

	if ( src[0] ) {
		Sys_SetClipboardData( src );
		Com_Printf( "Copied %d bytes to clipboard.\n", (int)strlen(src) );
	}
}

// command table — matches q3now's ui_script.c set, plus Wired UI additions + v6 compat
static const wiredScriptCommand_t wiredScriptCommands[] = {
	{ "show",             WiredScript_Show },
	{ "hide",             WiredScript_Hide },
	{ "open",             WiredScript_Open },
	{ "close",            WiredScript_Close },
	{ "setcvar",          WiredScript_SetCvar },
	{ "setstate",         WiredScript_SetState },
	{ "savestate",        WiredScript_SaveState },
	{ "loadstate",        WiredScript_LoadState },
	{ "exec",             WiredScript_Exec },
	{ "clipboard",        WiredScript_Clipboard },
	{ "execConfirm",      WiredScript_ExecConfirm },
	{ "execconfirm",      WiredScript_ExecConfirm },
	{ "play",             WiredScript_Play },
	{ "playlooped",       WiredScript_PlayLooped },
	{ "stopmusic",        WiredScript_StopMusic },
	{ "fadein",           WiredScript_FadeIn },
	{ "fadeout",          WiredScript_FadeOut },
	{ "setfocus",         WiredScript_SetFocus },
	// ── Phase 2.5: v6 compatibility commands ────────────────────────
	{ "setitemcolor",     WiredScript_SetItemColor },
	{ "setcolor",         WiredScript_SetColor },
	{ "conditionalopen",  WiredScript_ConditionalOpen },
	{ "conditionalScript", WiredScript_ConditionalScript },
	{ "conditionalscript", WiredScript_ConditionalScript },
	{ "transition",       WiredScript_Transition },
	{ "setbackground",    WiredScript_SetBackground },
	{ "updateMapPreview", WiredScript_UpdateMapPreview },
	{ "uiScript",         WiredScript_UiScript },
	// ── sort commands ──────────────────────────────────────────────
	{ "MapSort",          WiredScript_MapSortCmd },
	{ "mapsort",          WiredScript_MapSortCmd },
	{ "UpdateGameType",   WiredScript_UpdateGameType },
	{ "updategametype",   WiredScript_UpdateGameType },
	{ "ToggleMapPool",    WiredScript_ToggleMapPool },
	{ "togglemappool",    WiredScript_ToggleMapPool },
	{ "ClearMapPool",     WiredScript_ClearMapPool },
	{ "clearmappool",     WiredScript_ClearMapPool },
	{ "ToggleFavorite",   WiredScript_ToggleFavoriteMap },
	{ "togglefavorite",   WiredScript_ToggleFavoriteMap },
	{ "ServerSort",       WiredScript_ServerSortCmd },
	{ "serversort",       WiredScript_ServerSortCmd },
	// ── game action commands (also reachable via uiScript) ──────────
	{ "startserver",      WiredScript_StartServer },
	{ "joinserver",       WiredScript_JoinServer },
	{ "rundemo",          WiredScript_RunDemo },
	{ "runmod",           WiredScript_RunMod },
	{ "refreshservers",   WiredScript_RefreshServers },

	{ NULL, NULL }
};

// ── script runner ─────────────────────────────────────────────────────
// Based on q3now's UI_RunScript (ui_script.c:282). Same tokenization:
// semicolon-separated commands, quoted string args, up to 8 args each.
// KEY DIFFERENCE: unknown commands pass to engine console instead of
// being silently dropped.

static void WiredUI_RunScript( wiredMenuDef_t *menu, wiredItemDef_t *item, const char *script ) {
	char        token[MAX_STRING_CHARS];
	const char *args[WIRED_MAX_SCRIPT_ARGS];
	static char argBuf[WIRED_MAX_SCRIPT_ARGS][256];
	int         numArgs;
	const char *p;
	qboolean    handled;

	if ( !script || !script[0] ) return;

	p = script;

	while ( *p ) {
		// skip whitespace and semicolons
		while ( *p == ' ' || *p == '\t' || *p == ';' ) p++;
		if ( !*p ) break;

		// read command name
		int i = 0;
		while ( *p && *p != ' ' && *p != '\t' && *p != ';' && i < (int)sizeof(token) - 1 ) {
			token[i++] = *p++;
		}
		token[i] = '\0';
		if ( !token[0] ) break;

		// read arguments (up to 8, stop at semicolon)
		numArgs = 0;
		while ( numArgs < WIRED_MAX_SCRIPT_ARGS ) {
			while ( *p == ' ' || *p == '\t' ) p++;
			if ( !*p || *p == ';' ) break;

			i = 0;
			if ( *p == '"' ) {
				p++;
				while ( *p && *p != '"' && i < 255 ) argBuf[numArgs][i++] = *p++;
				if ( *p == '"' ) p++;
			} else {
				while ( *p && *p != ' ' && *p != '\t' && *p != ';' && i < 255 ) argBuf[numArgs][i++] = *p++;
			}
			argBuf[numArgs][i] = '\0';
			args[numArgs] = argBuf[numArgs];
			numArgs++;
		}

		// dispatch to command table
		handled = qfalse;
		for ( int i = 0; wiredScriptCommands[i].name; i++ ) {
			if ( !Q_stricmp( token, wiredScriptCommands[i].name ) ) {
				wiredScriptCommands[i].handler( menu, item, numArgs, args );
				handled = qtrue;
				break;
			}
		}

		// Wired UI design decision: unknown commands pass to engine console.
		// This means any cvar or console command works as a script action
		// without needing to be hardcoded in the command table.
		if ( !handled ) {
			char cmdBuf[1024];
			Com_sprintf( cmdBuf, sizeof(cmdBuf), "%s", token );
			{
				qstring_t cmd_qs = QS_WrapExisting( cmdBuf, sizeof(cmdBuf) );
				for ( int i = 0; i < numArgs; i++ ) {
					QS_Appendf( &cmd_qs, " \"%s\"", args[i] );
				}
				QS_AppendChar( &cmd_qs, '\n' );
			}
			Cbuf_ExecuteText( EXEC_APPEND, cmdBuf );
		}
	}
}

// ── menu stack ────────────────────────────────────────────────────────

void WiredUI_PushMenu( const char *name ) {
	if ( !name || !name[0] ) return;

	// check if menu exists
	if ( !WiredUI_FindMenu( name ) ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: cannot open menu '%s' — not found (loaded %d menus)\n", name, WiredUI_GetMenuCount() );
		return;
	}

	if ( wui_menuStackDepth >= WIRED_MENU_STACK_DEPTH ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: menu stack overflow (max %d)\n", WIRED_MENU_STACK_DEPTH );
		return;
	}

	Q_strncpyz( wui_menuStack[wui_menuStackDepth], name, sizeof( wui_menuStack[0] ) );
	wui_menuStackDepth++;
	wui_focusItem = -1;
	wui_focusFromMouse = qfalse;
	wui_tooltipStartTime = 0;
	wui_tooltipFocusItem = -1;
	WiredUI_CloseMultiDropdown();
	if ( wui_sfxMenuOpen ) S_StartLocalSound( wui_sfxMenuOpen, CHAN_LOCAL_SOUND );

	// reset fade animation for the new menu
	{
		wiredMenuDef_t *pushed = WiredUI_FindMenu( name );
		if ( pushed ) {
			pushed->openTime = cls.realtime;
			pushed->fadeAlpha = 0;

			// start cinematic if menu uses WINDOW_STYLE_CINEMATIC
			if ( pushed->style == WINDOW_STYLE_CINEMATIC && pushed->cinematic[0] ) {
				pushed->cinematicHandle = CIN_PlayCinematic( pushed->cinematic,
					0, 0, (float)cls.glconfig.vidWidth, (float)cls.glconfig.vidHeight, CIN_loop | CIN_silent );
				if ( pushed->cinematicHandle >= 0 ) {
					Com_DPrintf( "WiredUI: started cinematic '%s' (handle %d)\n",
						pushed->cinematic, pushed->cinematicHandle );
				}
			}
		}
	}

	Com_DPrintf( "WiredUI: push menu '%s' (depth %d)\n", name, wui_menuStackDepth );

	// run onOpen script if present
	{
		wiredMenuDef_t *opened = WiredUI_FindMenu( name );
		if ( opened && opened->onOpen[0] ) {
			WiredUI_RunScript( opened, NULL, opened->onOpen );
		}
	}
}

void WiredUI_PopMenu( void ) {
	if ( wui_menuStackDepth <= 0 ) {
		// nothing to pop — close UI entirely
		wui_activeMenu = UIMENU_NONE;
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Cvar_Set( "cl_paused", "0" );
		return;
	}

	// stop cinematic on the menu being popped (before decrement)
	{
		wiredMenuDef_t *popped = WiredUI_FindMenu( wui_menuStack[wui_menuStackDepth - 1] );
		if ( popped && popped->cinematicHandle >= 0 ) {
			CIN_StopCinematic( popped->cinematicHandle );
			popped->cinematicHandle = -1;
		}
	}

	wui_menuStackDepth--;
	wui_focusItem = -1;
	wui_focusFromMouse = qfalse;
	wui_tooltipStartTime = 0;
	wui_tooltipFocusItem = -1;
	WiredUI_CloseMultiDropdown();
	if ( wui_sfxMenuClose ) S_StartLocalSound( wui_sfxMenuClose, CHAN_LOCAL_SOUND );

	if ( wui_menuStackDepth <= 0 ) {
		// stack empty — return to root menu behavior
		if ( wui_activeMenu == UIMENU_INGAME ) {
			// close in-game menu entirely
			wui_activeMenu = UIMENU_NONE;
			Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
			Cvar_Set( "cl_paused", "0" );
		}
		// for UIMENU_MAIN, the root stays visible (can't close the main menu)
	}

	Com_DPrintf( "WiredUI: pop menu (depth %d)\n", wui_menuStackDepth );
}

void WiredUI_CloseAllMenus( void ) {
	// stop all active cinematics on the stack
	for ( int i = 0; i < wui_menuStackDepth; i++ ) {
		wiredMenuDef_t *m = WiredUI_FindMenu( wui_menuStack[i] );
		if ( m && m->cinematicHandle >= 0 ) {
			CIN_StopCinematic( m->cinematicHandle );
			m->cinematicHandle = -1;
		}
	}
	wui_menuStackDepth = 0;
	wui_focusItem = -1;
	wui_tooltipStartTime = 0;
	wui_tooltipFocusItem = -1;
	wui_activeMenu = UIMENU_NONE;
	WiredUI_CloseMultiDropdown();
	wui_waitingForKey = qfalse;
	wui_bindItem = NULL;
	wui_sliderDragging = qfalse;
	wui_sliderDragItem = NULL;
	wui_editingField = qfalse;
	wui_editItem = NULL;
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	Cvar_Set( "cl_paused", "0" );
}

// ── helper functions ──────────────────────────────────────────────────

wiredMenuDef_t *WiredUI_GetActiveMenu( void ) {
	// if there's a menu on the stack, show that
	if ( wui_menuStackDepth > 0 ) {
		return WiredUI_FindMenu( wui_menuStack[wui_menuStackDepth - 1] );
	}
	// otherwise show the root menu based on wui_activeMenu
	if ( wui_activeMenu == UIMENU_MAIN )   return WiredUI_FindMenu( "main" );
	if ( wui_activeMenu == UIMENU_INGAME ) return WiredUI_FindMenu( "ingame" );
	return NULL;
}

void CL_WiredUI_ShowError( const char *title, const char *message, qboolean retryable )
{
	const char *reconnectTarget;
	qboolean    hideRetry;

	if ( !cls.uiStarted ) {
		// UI not initialised — best we can do is log
		Com_Printf( S_COLOR_RED "Connect error (UI not ready): %s\n",
		            message ? message : "unknown error" );
		return;
	}

	// Hide Retry if the caller says non-retryable, or if the last connect
	// target was localhost (CL_Reconnect_f refuses it silently — B6 fix).
	reconnectTarget = Cvar_VariableString( "cl_reconnectArgs" );
	hideRetry = !retryable
	         || reconnectTarget[0] == '\0'
	         || Q_stricmp( reconnectTarget, "localhost" ) == 0;

	WiredUI_StateSetString( "ui_errorTitle", title ? title : "Error" );
	WiredUI_StateSetString( "ui_errorRetry", hideRetry ? "0" : "1" );
	// com_errorMessage is already set by the caller and is the text source
	// for the dialog — no redundant state write (ED4 fix).

	// Dedup: if error_popup is already on top, update state in place.
	// Without this, a rapid error storm (e.g., repeated connect-fail retries)
	// stacks N copies of error_popup — the user dismisses one and finds another.
	if ( wui_menuStackDepth > 0
	     && Q_stricmp( wui_menuStack[ wui_menuStackDepth - 1 ], "error_popup" ) == 0 ) {
		// State keys already updated above — the visible dialog will re-read them.
		return;
	}

	WiredUI_PushMenu( "error_popup" );
}

static wiredItemDef_t *WiredUI_FindItemByName( wiredMenuDef_t *menu, const char *name ) {
	if ( !menu || !name ) return NULL;
	for ( int i = 0; i < menu->itemCount; i++ ) {
		if ( !Q_stricmp( menu->items[i]->name, name ) ) {
			return menu->items[i];
		}
	}
	return NULL;
}

// apply a callback to all items matching name OR group
static void WiredUI_ForEachItemByNameOrGroup( wiredMenuDef_t *menu, const char *name,
	void (*callback)( wiredItemDef_t *item, void *data ), void *data ) {
	if ( !menu || !name ) return;
	for ( int i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *it = menu->items[i];
		if ( !Q_stricmp( it->name, name ) || !Q_stricmp( it->group, name ) ) {
			callback( it, data );
		}
	}
}

// Compute the menu origin after applying anchor offset.
// Anchor places the menu relative to the screen in real pixel space,
// and rect.x/y become offsets from that anchor position.
static void WiredUI_ApplyAnchor( wiredMenuDef_t *menu, float menuW, float menuH,
                                  float *outX, float *outY ) {
	if ( menu->fullscreen || menu->anchor == ANCHOR_NONE ) {
		*outX = menu->fullscreen ? 0 : menu->rect.x;
		*outY = menu->fullscreen ? 0 : menu->rect.y;
		return;
	}

	float anchorX = 0, anchorY = 0;
	switch ( menu->anchor ) {
		case ANCHOR_TOP_LEFT:      anchorX = 0;                              anchorY = 0;                                break;
		case ANCHOR_TOP_CENTER:    anchorX = ((float)cls.glconfig.vidWidth - menuW) * 0.5f;  anchorY = 0;                                break;
		case ANCHOR_TOP_RIGHT:     anchorX = (float)cls.glconfig.vidWidth - menuW;           anchorY = 0;                                break;
		case ANCHOR_CENTER_LEFT:   anchorX = 0;                              anchorY = ((float)cls.glconfig.vidHeight - menuH) * 0.5f;   break;
		case ANCHOR_CENTER:        anchorX = ((float)cls.glconfig.vidWidth - menuW) * 0.5f;  anchorY = ((float)cls.glconfig.vidHeight - menuH) * 0.5f;   break;
		case ANCHOR_CENTER_RIGHT:  anchorX = (float)cls.glconfig.vidWidth - menuW;           anchorY = ((float)cls.glconfig.vidHeight - menuH) * 0.5f;   break;
		case ANCHOR_BOTTOM_LEFT:   anchorX = 0;                              anchorY = (float)cls.glconfig.vidHeight - menuH;            break;
		case ANCHOR_BOTTOM_CENTER: anchorX = ((float)cls.glconfig.vidWidth - menuW) * 0.5f;  anchorY = (float)cls.glconfig.vidHeight - menuH;            break;
		case ANCHOR_BOTTOM_RIGHT:  anchorX = (float)cls.glconfig.vidWidth - menuW;           anchorY = (float)cls.glconfig.vidHeight - menuH;            break;
		default: break;
	}
	*outX = anchorX + menu->rect.x;
	*outY = anchorY + menu->rect.y;
}

static qboolean WiredUI_PointInRect( float px, float py, wiredRect_t *r ) {
	return ( px >= r->x && px < r->x + r->w &&
	         py >= r->y && py < r->y + r->h );
}

static int WiredUI_FindItemAtCursor( wiredMenuDef_t *menu, float cx, float cy ) {
	// Layout is already resolved by the render loop (WUI_LayoutMenu called each frame)
	float menuH = menu->resolvedRect.h;
	float oy = menu->resolvedRect.y;
	float sy = menu->scrollOffset;

	// iterate back-to-front so topmost item wins
	for ( int i = menu->itemCount - 1; i >= 0; i-- ) {
		wiredItemDef_t *item = menu->items[i];
		wiredRect_t absRect;
		if ( !WiredUI_ItemCanFocus( item ) ) continue;
		absRect.x = item->resolvedRect.x;
		absRect.y = item->resolvedRect.y - sy;
		absRect.w = item->resolvedRect.w;
		absRect.h = item->resolvedRect.h;

		// skip items scrolled out of view
		float clipTop = oy;
		float clipBottom = clipTop + menuH;
		if ( absRect.y + absRect.h < clipTop || absRect.y > clipBottom ) continue;

		if ( WiredUI_PointInRect( cx, cy, &absRect ) ) return i;
	}
	return -1;
}

// ── key event ─────────────────────────────────────────────────────────
// Combines: v6 Menu_HandleKey structure, q3now q3_ui key dispatch,
// and ET:Legacy per-item events (onEsc, onEnter, onTab, execKey).

void WiredUI_KeyEvent( int key, qboolean down ) {
	wiredMenuDef_t *menu;
	wiredItemDef_t *focusedItem = NULL;

	if ( !wui_initialized ) return;

	// notify attract scheduler — any key stops attract
	if ( down ) WiredAttract_NoteInput( key );

	menu = WiredUI_GetActiveMenu();

	if ( wui_multiDropdownOpen && down ) {
		wiredMultiOptions_t opts;
		float ddX, ddY, ddW, ddH, rowH;
		int visibleRows;
		if ( !menu || !wui_multiDropdownItem ) {
			WiredUI_CloseMultiDropdown();
			return;
		}

		WiredUI_GetMultiOptions( wui_multiDropdownItem, &opts );
		if ( opts.count <= 0 ) {
			WiredUI_CloseMultiDropdown();
			return;
		}
		if ( !WiredUI_GetMultiDropdownRect( menu, wui_multiDropdownItem, opts.count,
			&ddX, &ddY, &ddW, &ddH, &rowH, &visibleRows ) ) {
			WiredUI_CloseMultiDropdown();
			return;
		}

		if ( key == K_ESCAPE ) {
			WiredUI_CloseMultiDropdown();
			return;
		}

		if ( key == K_MWHEELUP ) {
			if ( wui_multiDropdownScroll > 0 ) wui_multiDropdownScroll--;
			return;
		}
		if ( key == K_MWHEELDOWN ) {
			int maxScroll = opts.count - visibleRows;
			if ( maxScroll < 0 ) maxScroll = 0;
			if ( wui_multiDropdownScroll < maxScroll ) wui_multiDropdownScroll++;
			return;
		}

		if ( key == K_UPARROW || key == K_KP_UPARROW ) {
			if ( wui_multiDropdownHover < 0 ) wui_multiDropdownHover = 0;
			wui_multiDropdownHover = ( wui_multiDropdownHover - 1 + opts.count ) % opts.count;
			if ( wui_multiDropdownHover < wui_multiDropdownScroll ) {
				wui_multiDropdownScroll = wui_multiDropdownHover;
			}
			return;
		}
		if ( key == K_DOWNARROW || key == K_KP_DOWNARROW ) {
			if ( wui_multiDropdownHover < 0 ) wui_multiDropdownHover = 0;
			wui_multiDropdownHover = ( wui_multiDropdownHover + 1 ) % opts.count;
			if ( wui_multiDropdownHover >= wui_multiDropdownScroll + visibleRows ) {
				wui_multiDropdownScroll = wui_multiDropdownHover - visibleRows + 1;
			}
			return;
		}

		if ( key == K_ENTER || key == K_KP_ENTER ) {
			if ( wui_multiDropdownHover >= 0 && wui_multiDropdownHover < opts.count ) {
				WiredUI_SetMultiOptionByIndex( wui_multiDropdownItem, &opts, wui_multiDropdownHover );
				if ( wui_multiDropdownItem->action[0] ) {
					WiredUI_RunScript( menu, wui_multiDropdownItem, wui_multiDropdownItem->action );
				}
			}
			WiredUI_CloseMultiDropdown();
			return;
		}

		if ( key == K_MOUSE1 ) {
			wiredRect_t srcRect;
			srcRect.x = wui_multiDropdownItem->resolvedRect.x;
			srcRect.y = wui_multiDropdownItem->resolvedRect.y - menu->scrollOffset;
			srcRect.w = wui_multiDropdownItem->resolvedRect.w;
			srcRect.h = wui_multiDropdownItem->resolvedRect.h;

			if ( wui_cursorX >= ddX && wui_cursorX < ddX + ddW &&
			     wui_cursorY >= ddY && wui_cursorY < ddY + ddH ) {
				int row = (int)( ( wui_cursorY - ddY ) / rowH );
				int idx = wui_multiDropdownScroll + row;
				if ( idx >= 0 && idx < opts.count ) {
					WiredUI_SetMultiOptionByIndex( wui_multiDropdownItem, &opts, idx );
					if ( wui_multiDropdownItem->action[0] ) {
						WiredUI_RunScript( menu, wui_multiDropdownItem, wui_multiDropdownItem->action );
					}
				}
				WiredUI_CloseMultiDropdown();
				return;
			}

			if ( WiredUI_PointInRect( wui_cursorX, wui_cursorY, &srcRect ) ) {
				WiredUI_CloseMultiDropdown();
				return;
			}

			WiredUI_CloseMultiDropdown();
			return;
		}
	}

	// text field editing mode — intercepts all keys while editing
	if ( wui_editingField && wui_editItem && down ) {
		char buff[1024];
		int len;

			WiredUI_StateGetString( wui_editItem->cvar, buff, sizeof( buff ) );
		len = strlen( buff );

		if ( key & K_CHAR_FLAG ) {
			// character input
			int ch = key & ~K_CHAR_FLAG;

			if ( ch == 'h' - 'a' + 1 ) {
				// ctrl-h = backspace
				if ( wui_editCursorPos > 0 ) {
					memmove( &buff[wui_editCursorPos - 1], &buff[wui_editCursorPos], len + 1 - wui_editCursorPos );
					wui_editCursorPos--;
				}
				WiredUI_StateSetString( wui_editItem->cvar, buff );
			} else if ( ch == 'a' - 'a' + 1 ) {
				// ctrl-a = home
				wui_editCursorPos = 0;
			} else if ( ch == 'e' - 'a' + 1 ) {
				// ctrl-e = end
				wui_editCursorPos = len;
			} else if ( ch == 'v' - 'a' + 1 ) {
				// ctrl-v = paste from clipboard
				char *cbd = Sys_GetClipboardData();
				if ( cbd ) {
					int maxC = wui_editItem->maxChars > 0 ? wui_editItem->maxChars : 255;
					int pasteLen = strlen( cbd );
					int space = maxC - len;
					if ( pasteLen > space ) pasteLen = space;
					if ( pasteLen > 0 ) {
						memmove( &buff[wui_editCursorPos + pasteLen], &buff[wui_editCursorPos], len + 1 - wui_editCursorPos );
						memcpy( &buff[wui_editCursorPos], cbd, pasteLen );
						wui_editCursorPos += pasteLen;
						WiredUI_StateSetString( wui_editItem->cvar, buff );
					}
					Z_Free( cbd );
				}
			} else if ( ch >= 32 ) {
				// printable character — insert at cursor
				int maxC = wui_editItem->maxChars > 0 ? wui_editItem->maxChars : 255;
				if ( wui_editItem->type == ITEM_TYPE_NUMERICFIELD && ( ch < '0' || ch > '9' ) && ch != '.' && ch != '-' ) {
					// reject non-numeric
				} else if ( len < maxC ) {
					memmove( &buff[wui_editCursorPos + 1], &buff[wui_editCursorPos], len + 1 - wui_editCursorPos );
					buff[wui_editCursorPos] = ch;
					wui_editCursorPos++;
					WiredUI_StateSetString( wui_editItem->cvar, buff );
				}
			}
			return;
		}

		// non-character keys
		switch ( key ) {
			case K_ESCAPE:
				wui_editingField = qfalse;
				wui_editItem = NULL;
				return;

			case K_ENTER:
			case K_KP_ENTER:
				wui_editingField = qfalse;
				wui_editItem = NULL;
				return;

			case K_TAB:
				wui_editingField = qfalse;
				wui_editItem = NULL;
				// fall through to normal key handling for focus change
				break;

			case K_BACKSPACE:
				if ( keys[K_CTRL].down && wui_editCursorPos > 0 ) {
					// ctrl+backspace: delete previous word
					int pos = wui_editCursorPos;
					while ( pos > 0 && buff[pos - 1] == ' ' ) pos--;
					while ( pos > 0 && buff[pos - 1] != ' ' ) pos--;
					memmove( &buff[pos], &buff[wui_editCursorPos], len + 1 - wui_editCursorPos );
					wui_editCursorPos = pos;
					WiredUI_StateSetString( wui_editItem->cvar, buff );
				} else if ( wui_editCursorPos > 0 ) {
					memmove( &buff[wui_editCursorPos - 1], &buff[wui_editCursorPos], len + 1 - wui_editCursorPos );
					wui_editCursorPos--;
					WiredUI_StateSetString( wui_editItem->cvar, buff );
				}
				return;

			case K_DEL:
			case K_KP_DEL:
				if ( wui_editCursorPos < len ) {
					memmove( &buff[wui_editCursorPos], &buff[wui_editCursorPos + 1], len - wui_editCursorPos );
					WiredUI_StateSetString( wui_editItem->cvar, buff );
				}
				return;

			case K_LEFTARROW:
			case K_KP_LEFTARROW:
				if ( wui_editCursorPos > 0 ) wui_editCursorPos--;
				return;

			case K_RIGHTARROW:
			case K_KP_RIGHTARROW:
				if ( wui_editCursorPos < len ) wui_editCursorPos++;
				return;

			case K_HOME:
			case K_KP_HOME:
				wui_editCursorPos = 0;
				return;

			case K_END:
			case K_KP_END:
				wui_editCursorPos = len;
				return;

			default:
				return; // eat all other keys while editing
		}
	}

	// key binding capture mode — waiting for user to press a key
	if ( wui_waitingForKey && down ) {
		if ( key == K_ESCAPE ) {
			// cancel binding
			wui_waitingForKey = qfalse;
			wui_bindItem = NULL;
		} else if ( key == K_BACKSPACE || key == K_DEL ) {
			// clear all bindings for this command
			if ( wui_bindItem && wui_bindItem->cvar[0] ) {
				for ( int k = 0; k < MAX_KEYS; k++ ) {
					const char *b = Key_GetBinding( k );
					if ( b && !Q_stricmp( b, wui_bindItem->cvar ) ) {
						Key_SetBinding( k, "" );
					}
				}
			}
			wui_waitingForKey = qfalse;
			wui_bindItem = NULL;
		} else if ( wui_bindItem && wui_bindItem->cvar[0] ) {
			// remove this key from any OTHER command (conflict resolution)
			{
				const char *existing = Key_GetBinding( key );
				if ( existing && existing[0] && Q_stricmp( existing, wui_bindItem->cvar ) ) {
					Key_SetBinding( key, "" );
				}
			}
			// bind the key to this command
			Key_SetBinding( key, wui_bindItem->cvar );
			wui_waitingForKey = qfalse;
			wui_bindItem = NULL;
		}
		return;
	}

	if ( !menu ) return;

	if ( wui_focusItem >= 0 && wui_focusItem < menu->itemCount ) {
		focusedItem = menu->items[wui_focusItem];
		if ( !WiredUI_ItemCanFocus( focusedItem ) ) {
			focusedItem = NULL;
		}
	}

	// slider drag: release mouse button ends drag
	if ( !down && ( key == K_MOUSE1 ) && wui_sliderDragging ) {
		wui_sliderDragging = qfalse;
		wui_sliderDragItem = NULL;
	}

	// only process key-down for most actions
	if ( !down ) return;

	// ET:Legacy execKey: check ALL items for key-specific bindings
	for ( int i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];
		if ( item->execKeyCode && item->execKeyCode == key && item->execKeyAction[0] ) {
			WiredUI_RunScript( menu, item, item->execKeyAction );
			return;
		}
	}

	// ET:Legacy per-item events: onEsc, onEnter, onTab
	if ( focusedItem ) {
		if ( key == K_ESCAPE && focusedItem->onEsc[0] ) {
			WiredUI_RunScript( menu, focusedItem, focusedItem->onEsc );
			return;
		}
		if ( ( key == K_ENTER || key == K_KP_ENTER ) && focusedItem->onEnter[0] ) {
			WiredUI_RunScript( menu, focusedItem, focusedItem->onEnter );
			return;
		}
		if ( key == K_TAB && focusedItem->onTab[0] ) {
			WiredUI_RunScript( menu, focusedItem, focusedItem->onTab );
			return;
		}
	}

	// default key handling
	switch ( key ) {
		case K_ESCAPE:
			if ( menu->onESC[0] ) {
				WiredUI_RunScript( menu, NULL, menu->onESC );
			} else {
				// no onESC handler — pop the menu stack
				WiredUI_PopMenu();
			}
			break;

		case K_MOUSE1:
		case K_MOUSE2:
		case K_ENTER:
		case K_KP_ENTER:
			{
				qboolean openedDropdown = qfalse;
			if ( !focusedItem ) {
				break;
			}

			// EDITFIELD items: click to start editing
			if ( ( focusedItem->type == ITEM_TYPE_EDITFIELD || focusedItem->type == ITEM_TYPE_NUMERICFIELD )
			     && focusedItem->cvar[0] ) {
				char buf[256];
				wui_editingField = qtrue;
				wui_editItem = focusedItem;
				WiredUI_StateGetString( focusedItem->cvar, buf, sizeof( buf ) );
				wui_editCursorPos = strlen( buf );
				wui_editPaintOffset = 0;
				break;
			}

			// LISTBOX items: click to select row
			if ( focusedItem->type == ITEM_TYPE_LISTBOX && focusedItem->feeder != 0 ) {
				float rowH = focusedItem->elementheight > 0 ? focusedItem->elementheight : 16.0f;
				float menuOY = menu->fullscreen ? 0 : menu->rect.y;
				float scrollOff = menu->scrollOffset;
				float listAbsY = menuOY + focusedItem->rect.y - scrollOff;
				int clickedRow = (int)( ( wui_cursorY - listAbsY ) / rowH ) + focusedItem->listScrollOffset;
				int total = WiredUI_FeederCount( (int)focusedItem->feeder );

				if ( clickedRow >= 0 && clickedRow < total ) {
					focusedItem->listSelectedRow = clickedRow;
					WiredUI_FeederSelection( (int)focusedItem->feeder, clickedRow );

					// double-click detection: same row + same feeder within threshold
					if ( focusedItem->doubleClick[0] &&
					     clickedRow == wui_lastClickRow &&
					     focusedItem->feeder == wui_lastClickFeeder &&
					     ( cls.realtime - wui_lastClickTime ) < WIRED_DOUBLECLICK_TIME ) {
						WiredUI_RunScript( menu, focusedItem, focusedItem->doubleClick );
						wui_lastClickTime = 0;  // consumed
					} else {
						wui_lastClickTime = cls.realtime;
						wui_lastClickRow = clickedRow;
						wui_lastClickFeeder = focusedItem->feeder;
					}
				}
				if ( focusedItem->action[0] && ( key == K_MOUSE1 || key == K_ENTER || key == K_KP_ENTER ) ) {
					if ( wui_sfxAction ) S_StartLocalSound( wui_sfxAction, CHAN_LOCAL_SOUND );
					WiredUI_RunScript( menu, focusedItem, focusedItem->action );
				}
				break;
			}

			// BIND items: start key capture mode
			if ( focusedItem->type == ITEM_TYPE_BIND && focusedItem->cvar[0] ) {
				wui_waitingForKey = qtrue;
				wui_bindItem = focusedItem;
				break;
			}

			// cvar-bound items: handle interaction based on type
			if ( focusedItem->cvar[0] ) {
				char cvarBuf[256];
				WiredUI_StateGetString( focusedItem->cvar, cvarBuf, sizeof( cvarBuf ) );

					switch ( focusedItem->type ) {
					case ITEM_TYPE_YESNO:
						// toggle 0 <-> 1
					WiredUI_StateSetString( focusedItem->cvar, atof( cvarBuf ) != 0 ? "0" : "1" );
						break;

					case ITEM_TYPE_MULTI:
						if ( key == K_MOUSE2 ) {
							wiredMultiOptions_t opts;
							int cur, next;
							WiredUI_GetMultiOptions( focusedItem, &opts );
							if ( opts.count > 0 ) {
								cur = WiredUI_FindMultiOptionIndex( focusedItem, &opts, cvarBuf );
								next = ( cur - 1 + opts.count ) % opts.count;
								WiredUI_SetMultiOptionByIndex( focusedItem, &opts, next );
							}
						} else {
							wiredMultiOptions_t opts;
							char curBuf[256];
							WiredUI_GetMultiOptions( focusedItem, &opts );
							if ( opts.count > 0 ) {
							WiredUI_StateGetString( focusedItem->cvar, curBuf, sizeof( curBuf ) );
								wui_multiDropdownOpen = qtrue;
								wui_multiDropdownItem = focusedItem;
								wui_multiDropdownHover = WiredUI_FindMultiOptionIndex( focusedItem, &opts, curBuf );
								if ( wui_multiDropdownHover < 0 ) wui_multiDropdownHover = 0;
								wui_multiDropdownScroll = wui_multiDropdownHover - 4;
								if ( wui_multiDropdownScroll < 0 ) wui_multiDropdownScroll = 0;
								openedDropdown = qtrue;
							}
						}
						break;

					case ITEM_TYPE_SLIDER:
						if ( key == K_MOUSE1 ) {
							// mouse1 click: start drag and set value by click position
							float menuOX = menu->fullscreen ? 0 : menu->rect.x;
							float absItemX = menuOX + focusedItem->rect.x;
							float barX = absItemX + focusedItem->rect.w * 0.5f;
							float barW = focusedItem->rect.w * 0.45f;
							float range = focusedItem->sliderData.maxVal - focusedItem->sliderData.minVal;
							wui_sliderDragging = qtrue;
							wui_sliderDragItem = focusedItem;
							if ( barW > 0 && range > 0 ) {
								float frac = ( wui_cursorX - barX ) / barW;
								if ( frac < 0 ) frac = 0;
								if ( frac > 1 ) frac = 1;
							WiredUI_StateSetString( focusedItem->cvar, va( "%g", focusedItem->sliderData.minVal + frac * range ) );
							}
						} else {
							// right-click / enter / kp_enter: step value
							float val = atof( cvarBuf );
							float step = ( focusedItem->sliderData.maxVal - focusedItem->sliderData.minVal ) / 20.0f;
							if ( step < 0.01f ) step = 0.01f;
							if ( key == K_MOUSE2 ) step = -step;
							val += step;
							if ( val < focusedItem->sliderData.minVal ) val = focusedItem->sliderData.minVal;
							if ( val > focusedItem->sliderData.maxVal ) val = focusedItem->sliderData.maxVal;
							WiredUI_StateSetString( focusedItem->cvar, va( "%g", val ) );
						}
						break;

					default:
						break;
				}
			}

			// always run action script if present
			if ( !openedDropdown && focusedItem->action[0] ) {
				WiredUI_RunScript( menu, focusedItem, focusedItem->action );
			}
			break;
			}

		case K_MWHEELUP:
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX ) {
				int total = WiredUI_FeederCount( (int)focusedItem->feeder );
				int step = ( total > 20 ) ? 3 : 1;
				if ( focusedItem->listScrollOffset > 0 ) focusedItem->listScrollOffset -= step;
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
				focusedItem->listScrollFadeTime = cls.realtime;
			} else {
				// adaptive menu scroll: larger step for tall menus
				float maxScroll = menu->contentHeight - menu->rect.h;
				float step = ( maxScroll > 400 ) ? 48.0f : 24.0f;
				if ( maxScroll > 0 ) {
					menu->scrollOffset -= step;
					if ( menu->scrollOffset < 0 ) menu->scrollOffset = 0;
					menu->scrollBarFadeTime = cls.realtime;
				}
			}
			break;

		case K_MWHEELDOWN:
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX && focusedItem->feeder != 0 ) {
				int total = WiredUI_FeederCount( (int)focusedItem->feeder );
				int visible, step;
				if ( focusedItem->horizontalScroll ) {
					float colW = focusedItem->elementheight > 0 ? focusedItem->elementheight : 64.0f;
					visible = (int)( focusedItem->rect.w / colW );
				} else {
					float rowH = focusedItem->elementheight > 0 ? focusedItem->elementheight : 16.0f;
					visible = (int)( focusedItem->rect.h / rowH );
				}
				step = ( total > 20 ) ? 3 : 1;
				focusedItem->listScrollOffset += step;
				if ( focusedItem->listScrollOffset > total - visible )
					focusedItem->listScrollOffset = total - visible;
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
				focusedItem->listScrollFadeTime = cls.realtime;
			} else {
				// adaptive menu scroll
				float maxScroll = menu->contentHeight - menu->rect.h;
				float step = ( maxScroll > 400 ) ? 48.0f : 24.0f;
				if ( maxScroll > 0 ) {
					menu->scrollOffset += step;
					if ( menu->scrollOffset > maxScroll ) menu->scrollOffset = maxScroll;
					menu->scrollBarFadeTime = cls.realtime;
				}
			}
			break;

		case K_LEFTARROW:
		case K_KP_LEFTARROW:
		case K_RIGHTARROW:
		case K_KP_RIGHTARROW:
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX
			     && focusedItem->horizontalScroll && focusedItem->feeder != 0 ) {
				/* horizontal listbox: left/right scrolls items */
				int total   = WiredUI_FeederCount( (int)focusedItem->feeder );
				float colW  = focusedItem->elementheight > 0 ? focusedItem->elementheight : 64.0f;
				int visible = (int)( focusedItem->rect.w / colW );
				int dir     = ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1;
				focusedItem->listScrollOffset += dir;
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
				if ( focusedItem->listScrollOffset > total - visible )
					focusedItem->listScrollOffset = total - visible;
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
				focusedItem->listScrollFadeTime = cls.realtime;
			}
			else if ( focusedItem && focusedItem->cvar[0] ) {
				/* left/right adjusts sliders and cycles multi items */
				char cvarBuf[256];
				int dir = ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1;
				WiredUI_StateGetString( focusedItem->cvar, cvarBuf, sizeof( cvarBuf ) );

				if ( focusedItem->type == ITEM_TYPE_SLIDER ) {
					float val = atof( cvarBuf );
					float step = ( focusedItem->sliderData.maxVal - focusedItem->sliderData.minVal ) / 20.0f;
					if ( step < 0.01f ) step = 0.01f;
					val += step * dir;
					if ( val < focusedItem->sliderData.minVal ) val = focusedItem->sliderData.minVal;
					if ( val > focusedItem->sliderData.maxVal ) val = focusedItem->sliderData.maxVal;
					WiredUI_StateSetString( focusedItem->cvar, va( "%g", val ) );
				}
				else if ( focusedItem->type == ITEM_TYPE_MULTI ) {
					wiredMultiOptions_t opts;
					int cur, next;
					if ( wui_multiDropdownOpen ) {
						break;
					}
					WiredUI_GetMultiOptions( focusedItem, &opts );
					if ( opts.count > 0 ) {
						cur = WiredUI_FindMultiOptionIndex( focusedItem, &opts, cvarBuf );
						next = ( cur + dir + opts.count ) % opts.count;
						WiredUI_SetMultiOptionByIndex( focusedItem, &opts, next );
						if ( focusedItem->action[0] ) {
							WiredUI_RunScript( menu, focusedItem, focusedItem->action );
						}
					}
				}
				else if ( focusedItem->type == ITEM_TYPE_YESNO ) {
					WiredUI_StateSetString( focusedItem->cvar, atof( cvarBuf ) != 0 ? "0" : "1" );
				}
			}
			break;

		case K_UPARROW:
		case K_KP_UPARROW:
			{
				int start = ( wui_focusItem > 0 ) ? wui_focusItem - 1 : menu->itemCount - 1;
				for ( int i = 0; i < menu->itemCount; i++ ) {
					int idx = ( start - i + menu->itemCount ) % menu->itemCount;
					if ( WiredUI_ItemCanFocus( menu->items[idx] ) ) {
						wui_focusItem = idx;
						wui_focusFromMouse = qfalse;
						if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
						break;
					}
				}
			}
			break;

		case K_TAB:
		case K_DOWNARROW:
		case K_KP_DOWNARROW:
			{
				int start = wui_focusItem + 1;
				for ( int i = 0; i < menu->itemCount; i++ ) {
					int idx = ( start + i ) % menu->itemCount;
					if ( WiredUI_ItemCanFocus( menu->items[idx] ) ) {
						wui_focusItem = idx;
						wui_focusFromMouse = qfalse;
						if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
						break;
					}
				}
			}
			break;
	}
}

// ── mouse event ───────────────────────────────────────────────────────
// Accumulates deltas into screen-space cursor, updates focus item,
// fires mouseEnter/mouseExit scripts (ET:Legacy per-item events).

void WiredUI_MouseEvent( int dx, int dy ) {
	wiredMenuDef_t *menu;
	int oldFocus, newFocus;

	if ( !wui_initialized ) return;

	// notify attract scheduler — significant mouse movement stops attract
	WiredAttract_NoteMouse( dx, dy );

	// accumulate deltas into cursor position (real screen pixels)
	wui_cursorX += dx;
	if ( wui_cursorX < 0 ) wui_cursorX = 0;
	else if ( wui_cursorX > (float)cls.glconfig.vidWidth ) wui_cursorX = (float)cls.glconfig.vidWidth;

	wui_cursorY += dy;
	if ( wui_cursorY < 0 ) wui_cursorY = 0;
	else if ( wui_cursorY > (float)cls.glconfig.vidHeight ) wui_cursorY = (float)cls.glconfig.vidHeight;

	// slider drag: continuously update cvar while mouse1 is held
	if ( wui_sliderDragging && wui_sliderDragItem && wui_sliderDragItem->cvar[0] ) {
		wiredMenuDef_t *dragMenu = WiredUI_GetActiveMenu();
		if ( dragMenu ) {
			float menuOX = dragMenu->fullscreen ? 0 : dragMenu->rect.x;
			float absItemX = menuOX + wui_sliderDragItem->rect.x;
			float barX = absItemX + wui_sliderDragItem->rect.w * 0.5f;
			float barW = wui_sliderDragItem->rect.w * 0.45f;
			float range = wui_sliderDragItem->sliderData.maxVal - wui_sliderDragItem->sliderData.minVal;
			if ( barW > 0 && range > 0 ) {
				float frac = ( wui_cursorX - barX ) / barW;
				if ( frac < 0 ) frac = 0;
				if ( frac > 1 ) frac = 1;
				WiredUI_StateSetString( wui_sliderDragItem->cvar,
					va( "%g", wui_sliderDragItem->sliderData.minVal + frac * range ) );
			}
		}
		return; // don't change focus while dragging
	}

	menu = WiredUI_GetActiveMenu();
	if ( !menu ) return;

	if ( wui_multiDropdownOpen && wui_multiDropdownItem ) {
		wiredMultiOptions_t opts;
		float ddX, ddY, ddW, ddH, rowH;
		int visibleRows;
		WiredUI_GetMultiOptions( wui_multiDropdownItem, &opts );
		if ( opts.count <= 0 ) {
			WiredUI_CloseMultiDropdown();
		} else if ( WiredUI_GetMultiDropdownRect( menu, wui_multiDropdownItem, opts.count,
			&ddX, &ddY, &ddW, &ddH, &rowH, &visibleRows ) ) {
			if ( wui_cursorX >= ddX && wui_cursorX < ddX + ddW &&
			     wui_cursorY >= ddY && wui_cursorY < ddY + ddH ) {
				int row = (int)( ( wui_cursorY - ddY ) / rowH );
				int idx = wui_multiDropdownScroll + row;
				if ( idx >= 0 && idx < opts.count ) {
					wui_multiDropdownHover = idx;
				}
			} else {
				wui_multiDropdownHover = -1;
			}
		}
	}

	oldFocus = wui_focusItem;
	newFocus = WiredUI_FindItemAtCursor( menu, wui_cursorX, wui_cursorY );
	if ( newFocus >= 0 && newFocus < menu->itemCount && !WiredUI_ItemCanFocus( menu->items[newFocus] ) ) {
		newFocus = -1;
	}

	// any mouse movement reactivates mouse-based focus
	wui_focusFromMouse = ( newFocus >= 0 );

	// fire mouseExit on old item, mouseEnter on new item
	if ( newFocus != oldFocus ) {
		if ( oldFocus >= 0 && oldFocus < menu->itemCount ) {
			wiredItemDef_t *old = menu->items[oldFocus];
			if ( old->mouseExit[0] ) {
				WiredUI_RunScript( menu, old, old->mouseExit );
			}
			if ( old->leaveFocus[0] ) {
				WiredUI_RunScript( menu, old, old->leaveFocus );
			}
		}
		// reset tooltip timer on focus change
		if ( newFocus >= 0 && newFocus < menu->itemCount ) {
			wiredItemDef_t *cur = menu->items[newFocus];
			if ( cur->mouseEnter[0] ) {
				WiredUI_RunScript( menu, cur, cur->mouseEnter );
			}
			if ( cur->onFocus[0] ) {
				WiredUI_RunScript( menu, cur, cur->onFocus );
			}
			// start tooltip delay timer if new item has a tooltip
			if ( cur->tooltip[0] ) {
				wui_tooltipStartTime = cls.realtime;
				wui_tooltipFocusItem = newFocus;
			} else {
				wui_tooltipStartTime = 0;
				wui_tooltipFocusItem = -1;
			}
		} else {
			// cursor left all items — clear tooltip state
			wui_tooltipStartTime = 0;
			wui_tooltipFocusItem = -1;
		}
		wui_focusItem = newFocus;
		if ( newFocus >= 0 && wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
	}
}

void WiredUI_SetActiveMenu( int menu ) {
	if ( !wui_initialized ) {
		return;
	}

	wui_activeMenu = menu;
	if ( menu == UIMENU_NONE ) {
		WiredUI_CloseMultiDropdown();
	}

	if ( menu != UIMENU_NONE ) {
		// activate UI key catcher so the engine routes input and draw calls to us
		Key_SetCatcher( KEYCATCH_UI );

		if ( menu == UIMENU_INGAME ) {
			// in single-player the server pauses when cl_paused is set;
			// the UI VM (q3_ui / ui) does this explicitly — we must too
			Cvar_Set( "cl_paused", "1" );
		}

		// re-register all assets — Hunk_ClearLevel on map load invalidates handles
		WiredUI_RegisterAssets();

		Com_DPrintf( "WiredUI: SetActiveMenu %d\n", menu );
	} else {
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Cvar_Set( "cl_paused", "0" );
	}
}

qboolean WiredUI_IsFullscreen( void ) {
	if ( !wui_initialized ) {
		return qfalse;
	}

	// main menu is always "fullscreen" for the engine — prevents 3D scene rendering
	// at CA_DISCONNECTED. Wired UI draws its own background (clouds) in Refresh.
	return ( wui_activeMenu == UIMENU_MAIN );
}

void WiredUI_DrawConnectScreen( qboolean overlay ) {
	wiredMenuDef_t	*menu;
	const char		*status;
	const char		*info;
	char			buf[MAX_STRING_CHARS];
	vec4_t			white = { 1, 1, 1, 1 };
	vec4_t			dim   = { 0.6f, 0.6f, 0.6f, 1 };
	float			y;

	if ( !wui_initialized ) {
		return;
	}

	// CA_LOADING/CA_PRIMED overlay: just show a brief "Loading..." strip
	// so it doesn't flash away too fast on fast loads (matches TA_UI behavior)
	if ( overlay ) {
		vec4_t overlayBg = { 0, 0, 0, 0.6f };
		WUI_FillRect( 0, (float)cls.glconfig.vidHeight - 40, (float)cls.glconfig.vidWidth, 40, overlayBg );
		Text_Draw( "Loading...", 8, (float)cls.glconfig.vidHeight - 32, FONT_UI, 8, white, TEXT_ALIGN_LEFT, TEXT_FORCECOLOR );

		// show server message if present
		if ( clc.serverMessage[0] ) {
			Text_Draw( clc.serverMessage, 8, (float)cls.glconfig.vidHeight - 20, FONT_UI, 8, dim, TEXT_ALIGN_LEFT, TEXT_FORCECOLOR );
		}
		return;
	}

	// full-screen connection dialog
	menu = WiredUI_FindMenu( "connect" );
	if ( menu ) {
		WiredUI_RenderMenuOverlay( menu, cls.realtime );
	} else {
		// fallback: dark background if connect.menu isn't loaded
		vec4_t bg = { 0.05f, 0.05f, 0.1f, 1.0f };
		WUI_FillRect( 0, 0, (float)cls.glconfig.vidWidth, (float)cls.glconfig.vidHeight, bg );
	}

	// dynamic status text — drawn on top of the menu
	y = 260;

	// server name
	if ( cls.servername[0] ) {
		if ( !Q_stricmp( cls.servername, "localhost" ) ) {
			info = "Starting local server...";
		} else {
			info = va( "Connecting to %s", cls.servername );
		}
		Text_Draw( info, 200, (float)y, FONT_UI, 8, white, TEXT_ALIGN_LEFT, TEXT_FORCECOLOR );
		y += 16;
	}

	// connection state
	switch ( cls.state ) {
		case CA_CONNECTING:
			status = va( "Awaiting connection... %d", clc.connectPacketCount );
			break;
		case CA_CHALLENGING:
			status = va( "Awaiting challenge... %d", clc.connectPacketCount );
			break;
		case CA_CONNECTED:
			// check for download in progress
			if ( clc.downloadName[0] ) {
				int pct = 0;
				if ( clc.downloadSize > 0 ) {
					pct = (int)( (float)clc.downloadCount * 100.0f / (float)clc.downloadSize );
				}
				status = va( "Downloading %s... %d%%", clc.downloadName, pct );
			} else {
				status = "Awaiting gamestate...";
			}
			break;
		default:
			status = "Connecting...";
			break;
	}
	Text_Draw( status, 200, (float)y, FONT_UI, 8, dim, TEXT_ALIGN_LEFT, TEXT_FORCECOLOR );
	y += 16;

	// server error message — use WCOLOR_ERROR values for consistency with error_popup.wmenu
	if ( clc.serverMessage[0] ) {
		vec4_t errColor = { 1, 0.45f, 0.35f, 1 };
		Text_Draw( clc.serverMessage, 200, (float)y, FONT_UI, 8, errColor, TEXT_ALIGN_LEFT, TEXT_FORCECOLOR );
		y += 16;
	}

	// MOTD from master server
	Cvar_VariableStringBuffer( "cl_motdString", buf, sizeof( buf ) );
	if ( buf[0] ) {
		Text_Draw( buf, 200, (float)y, FONT_UI, 8, dim, TEXT_ALIGN_LEFT, TEXT_FORCECOLOR );
	}
}

// ── overlay renderer (for scoreboard, vote panel, etc.) ──────────────
// Renders a menu's background + items without cursor/focus/tooltip logic.
// Called from WiredHud_Routine() for in-game overlays that need .menu layout.

void WiredUI_RenderMenuOverlay( wiredMenuDef_t *menu, int realtime ) {
	float menuAlpha = 1.0f;

	if ( !menu ) return;

	float vpW = (float)cls.glconfig.vidWidth;
	float vpH = (float)cls.glconfig.vidHeight;
	WUI_LayoutMenu( menu, vpW, vpH );

	float menuX = menu->resolvedRect.x;
	float menuY = menu->resolvedRect.y;
	float menuW = menu->resolvedRect.w;
	float menuH = menu->resolvedRect.h;

	if ( menu->fadeAlpha > 0.0f && menu->fadeAlpha <= 1.0f ) {
		menuAlpha = menu->fadeAlpha;
	}

	if ( menuH > 0 ) {
		WiredUI_DrawMenuBackground( menu, menuX, menuY, menuW, menuH, menuAlpha );
		WiredUI_DrawWindowBorder( menuX, menuY, menuW, menuH,
			menu->border, menu->bordersize, menu->bordercolor, menuAlpha );
	}

	// render items
	for ( int i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *item = menu->items[i];

		if ( !WiredUI_ItemShouldRender( item ) ) continue;

		float itemX = item->resolvedRect.x;
		float itemY = item->resolvedRect.y;
		float itemW = item->resolvedRect.w;
		float itemH = item->resolvedRect.h;

		// clip items outside menu bounds (when height=0, meaning auto-size)
		if ( menuH > 0 && ( itemY + itemH < menuY || itemY > menuY + menuH ) ) continue;

		if ( item->type == ITEM_TYPE_MODEL ) {
			WiredUI_DrawModelItem( item, itemX, itemY, itemW, itemH );
			WiredUI_DrawWindowBorder( itemX, itemY, itemW, itemH,
				item->border, item->bordersize, item->bordercolor, menuAlpha );
			continue;
		}

		WiredUI_DrawItemBackground( item, itemX, itemY, itemW, itemH, menuAlpha );
		WiredUI_DrawWindowBorder( itemX, itemY, itemW, itemH,
			item->border, item->bordersize, item->bordercolor, menuAlpha );

		/* bindicon: draw store-bound icon overlay */
		{
			qhandle_t storeIcon = 0;
			float storeValue = 0.0f;

			if ( item->storeBindIcon[0] ) {
				wuiStoreEntry_t *iconEntry = WiredStore_Get( item->storeBindIcon );
				if ( iconEntry && iconEntry->icon ) {
					storeIcon = iconEntry->icon;
				} else if ( !item->bindWarned ) {
					Com_DPrintf( "WiredUI: bindicon key '%s' not found (item '%s')\n",
								 item->storeBindIcon, item->name );
					item->bindWarned = qtrue;
				}
			}

			/* bindvalue: resolve numeric value from store */
			if ( item->storeBindValue[0] ) {
				wuiStoreEntry_t *valEntry = WiredStore_Get( item->storeBindValue );
				if ( valEntry ) {
					storeValue = valEntry->value;
				} else if ( !item->bindWarned ) {
					Com_DPrintf( "WiredUI: bindvalue key '%s' not found (item '%s')\n",
								 item->storeBindValue, item->name );
					item->bindWarned = qtrue;
				}
			}

			if ( storeIcon ) {
				re.SetColor( NULL );
				WUI_DrawPic( itemX, itemY, itemW, itemH, storeIcon );
			}

			(void)storeValue; /* resolved for use by status bar elements (task-5) */
		}

		// draw LISTBOX items (feeder-driven)
		if ( item->type == ITEM_TYPE_LISTBOX && item->feeder != 0 ) {
			int feederID = (int)item->feeder;
			int totalRows = WiredUI_FeederCount( feederID );
			float rowH = item->elementheight > 0 ? item->elementheight : 16.0f;
			int visibleRows = (int)( itemH / rowH );
			int row, col;
			float charSize = item->fontPointSize > 0.0f ? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
			vec4_t rowColor;
			float contentW = itemW;

			// draw list background
			if ( item->backcolor[3] > 0 ) {
				WUI_FillRect( itemX, itemY, itemW, itemH, item->backcolor );
			}

			// draw rows
			for ( row = 0; row < visibleRows && ( item->listScrollOffset + row ) < totalRows; row++ ) {
				int dataRow = item->listScrollOffset + row;
				float rowY = itemY + row * rowH;

				// draw columns
				Vector4Copy( item->forecolor, rowColor );
				float colX = itemX + 4;
				for ( col = 0; col < ( item->columns > 0 ? item->columns : 1 ); col++ ) {
					const char *text = WiredUI_FeederItemText( feederID, dataRow, col );
					float colW = ( col < item->columns && item->columnWidths[col] > 0 )
						? item->columnWidths[col] : contentW;
					if ( text && text[0] ) {
						int maxChars = (int)( ( colW - 4 ) / charSize );
						if ( maxChars < 1 ) maxChars = 1;
						if ( (int)strlen( text ) > maxChars ) {
							char clipped[128];
							Q_strncpyz( clipped, text, sizeof( clipped ) );
							if ( maxChars < (int)sizeof( clipped ) ) {
								clipped[maxChars] = '\0';
							}
							Text_Draw( clipped, (float)colX, (float)( rowY + 2 ), FONT_UI, charSize, rowColor, TEXT_ALIGN_LEFT, 0 );
						} else {
							Text_Draw( text, (float)colX, (float)( rowY + 2 ), FONT_UI, charSize, rowColor, TEXT_ALIGN_LEFT, 0 );
						}
					}
					colX += colW;
				}
			}

			continue;
		}

		/* TABLE widget -- store-driven data table (Phase 4) */
		if ( item->tableSource[0] && item->numTableColumns > 0 ) {
			extern void WiredHud_DrawTable( wiredItemDef_t *item, float ox, float oy, float ow, float oh,
				int fontId, float fontSize );
			int tblFontId = FONT_UI;
			float tblFontSize = item->fontPointSize > 0.0f ? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
			WiredHud_DrawTable( item, itemX, itemY, itemW, itemH, tblFontId, tblFontSize );
			continue;
		}

		// draw SCORELIST widget -- rich scoreboard with per-cell coloring
		if ( item->type == ITEM_TYPE_SCORELIST && item->feeder != 0 ) {
			extern void WiredHud_DrawScorelistWidget( float x, float y, float w, float h,
				int feederID, const vec4_t textColor );
			WiredHud_DrawScorelistWidget( itemX, itemY, itemW, itemH,
				(int)item->feeder, item->forecolor );
			continue;
		}

		// draw DUELBOARD widget — Pro-style two-panel duel scoreboard
		if ( item->type == ITEM_TYPE_DUELBOARD ) {
			extern void WiredHud_DrawDuelBoard( float x, float y, float w, float h );
			WiredHud_DrawDuelBoard( itemX, itemY, itemW, itemH );
			continue;
		}

		/* draw text items (static text or cvar-bound) using modern font system */
		{
			const char *drawText = NULL;
			char cvarBuf[256];
			vec4_t overlayDrawColor;

			Vector4Copy( item->forecolor, overlayDrawColor );

			if ( item->text[0] ) {
				drawText = item->text;
			} else if ( item->cvar[0] && item->type == ITEM_TYPE_TEXT ) {
				WiredUI_StateGetString( item->cvar, cvarBuf, sizeof( cvarBuf ) );
				if ( cvarBuf[0] ) drawText = cvarBuf;
			}

			/* bind: override display text from store */
			if ( item->storeBind[0] ) {
				wuiStoreEntry_t *bindEntry = WiredStore_Get( item->storeBind );
				if ( bindEntry && bindEntry->text[0] ) {
					drawText = bindEntry->text;
				} else if ( !drawText && !item->bindWarned ) {
					Com_DPrintf( "WiredUI: bind key '%s' not found (item '%s')\n",
								 item->storeBind, item->name );
					item->bindWarned = qtrue;
				}
			}

			/* bindcolor: override forecolor from store */
			if ( item->storeBindColor[0] ) {
				wuiStoreEntry_t *colorEntry = WiredStore_Get( item->storeBindColor );
				if ( colorEntry ) {
					/* semantic state takes priority over raw color */
					if ( colorEntry->state[0] ) {
						if ( !WiredTheme_ResolveState( colorEntry->state, overlayDrawColor ) ) {
							/* unknown state — fall back to raw color */
							Vector4Copy( colorEntry->color, overlayDrawColor );
						}
					} else {
						Vector4Copy( colorEntry->color, overlayDrawColor );
					}
				} else if ( !item->bindWarned ) {
					Com_DPrintf( "WiredUI: bindcolor key '%s' not found (item '%s')\n",
								 item->storeBindColor, item->name );
					item->bindWarned = qtrue;
				}
			}

			if ( drawText ) {
				float charW, charH;
				int alignment = TEXT_ALIGN_LEFT;
				int flags = TEXT_DROPSHADOW;

				/* textscale maps to charW/charH (same mapping as main menu renderer) */
				charW = item->textscale >= 0.7f ? 16.0f : ( item->textscale >= 0.3f ? 10.0f : 8.0f );
				charH = charW * 1.4f;

				if ( item->textalign == ITEM_ALIGN_CENTER ) {
					alignment = TEXT_ALIGN_CENTER;
				} else if ( item->textalign == ITEM_ALIGN_RIGHT ) {
					alignment = TEXT_ALIGN_RIGHT;
				}

				if ( item->textstyle == ITEM_TEXTSTYLE_SHADOWEDMORE ) {
					flags = TEXT_DROPSHADOW;
				} else if ( item->textstyle == ITEM_TEXTSTYLE_SHADOWED ) {
					flags = TEXT_DROPSHADOW;
				} else if ( item->textstyle == ITEM_TEXTSTYLE_NORMAL ) {
					flags = 0;
				}

				/* position: centered items use widget midpoint, others use left edge */
				{
					float x, y;
					if ( item->textalign == ITEM_ALIGN_CENTER && itemW > 0 ) {
						x = itemX + itemW * 0.5f;
					} else if ( item->textalign == ITEM_ALIGN_RIGHT && itemW > 0 ) {
						x = itemX + itemW;
					} else {
						x = itemX;
					}
					y = itemY + item->textaligny;

				Text_SetLetterSpacing( item->letterSpacing );
				Text_Draw( drawText, x, y, FONT_DISPLAY,
					charH, overlayDrawColor, alignment, flags );
				Text_SetLetterSpacing( 0.0f );
			}
		}
		}
	}
}

void WiredUI_ReloadHud( void ) {
	Com_Printf( "WiredUI: reloading HUD...\n" );

	// destroy all active elements (frees Z_Malloc'd contexts)
	WiredHud_DestroyAllElements();

	// two-phase safe reload
	WiredUI_SafeReload();

	// load HUD file from 'hud' cvar (menus.lua no longer contains the whud load)
	WiredUI_LoadHudFromCvar();

	// notify attract scheduler that the pool was rebuilt
	WiredAttract_OnMenuReload();

	// recreate HUD elements from hudOverlay menus
	WiredHud_LoadFromMenus();

	Com_Printf( "WiredUI: HUD reloaded, %d elements active\n", WiredHud_GetElementCount() );
}

void WiredUI_ReloadMenus( void ) {
	Com_Printf( "WiredUI: reloading menus...\n" );

	// stop all cinematics before reload
	{
		for ( int i = 0; i < wui_menuStackDepth; i++ ) {
			wiredMenuDef_t *m = WiredUI_FindMenu( wui_menuStack[i] );
			if ( m && m->cinematicHandle >= 0 ) {
				CIN_StopCinematic( m->cinematicHandle );
				m->cinematicHandle = -1;
			}
		}
	}

	// save current menu name for re-open after reload
	char currentMenu[64] = {0};
	if ( wui_menuStackDepth > 0 ) {
		Q_strncpyz( currentMenu, wui_menuStack[wui_menuStackDepth - 1], sizeof( currentMenu ) );
	}
	wui_menuStackDepth = 0;
	wui_focusItem = -1;
	wui_tooltipStartTime = 0;
	wui_tooltipFocusItem = -1;

	// destroy HUD elements (they'll be recreated after reload)
	WiredHud_DestroyAllElements();

	// two-phase safe reload: parse new → swap, or keep old on failure
	if ( WiredUI_SafeReload() ) {
		Com_Printf( "Menus reloaded successfully.\n" );
	}
	// on failure, SafeReload already restored old menus + printed error

	// load HUD file from 'hud' cvar (menus.lua no longer contains the whud load)
	WiredUI_LoadHudFromCvar();

	// notify attract scheduler that the pool was rebuilt
	WiredAttract_OnMenuReload();

	// recreate HUD elements from (possibly new) hudOverlay menus
	WiredHud_LoadFromMenus();

	// re-open the menu that was active before reload
	if ( currentMenu[0] && WiredUI_FindMenu( currentMenu ) ) {
		WiredUI_PushMenu( currentMenu );
	}
}

// ── Engine-facing API (replaces cl_ui.c) ──────────────────────────────

void CL_InitUI( void ) {
	// disallow vl.collapse for UI elements
	re.VertexLighting( qfalse );

	cls.uiStarted = qtrue;
}

void CL_ShutdownUI( void ) {
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	cls.uiStarted = qfalse;
	WiredUI_Shutdown();
}

#endif // FEAT_WIRED_UI
