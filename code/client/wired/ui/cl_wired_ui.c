// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_ui.c — Wired UI: unified menu/HUD system implementation
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "cl_wired_widget_core.h"
#include "cl_wired_compositor.h"
#include "cl_wired_customdraw.h"
#include "cl_wired_attract.h"
#include "cl_wired_ui_hud_state.h"
#include "cl_wired_fonts.h"
#include "cl_wired_text.h"
#include "cl_wired_draw.h"
#include "cl_wired_anim.h"
#include "cl_wired_viewport.h"   /* WiredUI_ViewportMultiSelfTest (#ifdef _DEBUG) */
#include <inttypes.h>

/* bootstrap helpers exposed from cl_wired_ownerdraw.c +
 * cl_wired_ui_hud_register.c so WiredUI_Init can drive the unified-registry
 * setup ordering. WiredHud_RegisterElements binds the permanent HUD element
 * + family tables into the unified custom-draw registry (not transitional —
 * see cl_wired_ui_hud_register.c). */
extern void WiredOwnerDraw_RegisterAll          ( void );
extern void WiredHud_RegisterElements           ( void );

/* loading-screen custom-draw bootstrap, defined in
 * cl_loading_ui.c — registers custom:loading_{wireframe,streaming_rows,
 * mapinfo_stats} alongside the hud + ownerdraw entries. Retires
 * with the rest of cl_loading_ui.c. */
extern void WiredLoadingCustomDraws_RegisterAll( void );

/* (debug-overlay-migration): the 6 debug-overlay custom-draw
 * handlers (demo_recording, voip_meter, graph, ping, snaps, packets), defined
 * in code/client/wired/ui/elements/debug_overlay.c. Registered alongside the
 * overlay cursor/tooltip entries so the parser's `custom` sigil-rewrite
 * resolves them against the live registry. */
extern void WiredDebugOverlay_RegisterAll( void );
#include "cl_wired_background.h"
#include "cl_wired_store.h"
#include "cl_wired_theme.h"
#include "cl_wired_palette.h"
#include "../../../qcommon/menudef.h"

#include <stdio.h>   /* sscanf for token colour parsing */
#include <lua.h>
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

/* ── LOADING-layer state→named-UI binding (path-identity) ──────────────────
 * The compositor's LOADING layer emits a SPECIFIC menu by path-identity (the
 * React `return <LoadingComponent/>` model) instead of blind-scanning the
 * registry for anything tagged `layer "loading"`. Connstate transitions set
 * the relative path of the menu to show (connect.wui during the handshake,
 * loading_screen.wui during map load); CA_ACTIVE clears it. Sequential — at
 * most one loading menu is active at a time — so a single path suffices. */
static char wui_loading_menu_path[ MAX_QPATH ];

void WiredUI_SetLoadingMenu( const char *relPath ) {
	char prev[ MAX_QPATH ];
	Q_strncpyz( prev, wui_loading_menu_path, sizeof( prev ) );
	if ( relPath && *relPath ) {
		Q_strncpyz( wui_loading_menu_path, relPath, sizeof( wui_loading_menu_path ) );
	} else {
		wui_loading_menu_path[ 0 ] = '\0';
	}
	if ( Q_stricmp( prev, wui_loading_menu_path ) != 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: loading menu -> '%s'\n",
			wui_loading_menu_path[ 0 ] ? wui_loading_menu_path : "(none)" );
	}
}

const char *WiredUI_GetLoadingMenuPath( void ) {
	return wui_loading_menu_path;
}

/* WUI_DEFAULT_FONT_SIZE promoted to cl_wired_ui.h (compositor emit path
 * needs the same fallback). */
#define WIRED_UI_STATE_FILE     "wired_ui_state.dat"
#define WIRED_UI_STATE_MAGIC    0x57554953
#define WIRED_UI_STATE_VERSION  1

// from cl_wired_ui_hud_register.c (permanent HUD registration/runtime infra)
extern void     WiredHud_DestroyAllElements( void );
extern int      WiredHud_GetElementCount( void );
// from cl_wired_ui_hud_state.c (transition shim)
extern void     WiredHud_LoadFromMenus( void );

// forward declarations
static void WiredUI_ApplyAnchor( wiredMenuDef_t *menu, float menuW, float menuH,
                                  float *outX, float *outY );
static qboolean WiredUI_ItemVisibleByBindRules( wiredItemDef_t *item );
static qboolean WiredUI_ItemShouldRender( wiredItemDef_t *item );
static qboolean WiredUI_ItemCanFocus( wiredItemDef_t *item );
/* non-static: shared with cl_wired_widget_core.c (declared in cl_wired_ui.h) */
qboolean WiredUI_StateListContainsValue( const char *list, const char *value );
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

// Open-addressing hash of wui_uiStateDefaults below. Built once by
// WiredUI_BuildStateDefaultsHash so per-frame WiredUI_IsStoreStateKey /
// StateDefaultValue lookups become O(1) instead of O(N) per call.
#define WUI_STATE_DEFAULTS_BUCKETS 128
static const wiredUiStateDefault_t *wui_stateDefaultsHash[WUI_STATE_DEFAULTS_BUCKETS];
static qboolean wui_stateDefaultsHashBuilt = qfalse;

static unsigned int WiredUI_StateKeyHash( const char *s ) {
	unsigned int h = 2166136261u; // FNV-1a
	while ( *s ) {
		unsigned char c = (unsigned char)*s++;
		if ( c >= 'A' && c <= 'Z' ) c += 'a' - 'A';
		h ^= c;
		h *= 16777619u;
	}
	return h;
}

static const wiredUiStateDefault_t wui_uiStateDefaults[] = {
	{ "ui_netSource", "0" },
	{ "ui_browserGameType", "0" },
	{ "ui_browserShowFull", "1" },
	{ "ui_browserShowEmpty", "1" },
	{ "ui_browserMaxPing", "0" },
	{ "ui_browserStatus", "" },
	{ "ui_selectedServerAddr", "" },
	{ "ui_selectedServerName", "" },
	{ "ui_joinPasswordError", "" },
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
	/* Which settings category is open. Store state, not a cvar: it is
	 * meaningless to persist across sessions or to set from the console, and as
	 * a cvar it never got created at all — `setcvar` in the seven target menus'
	 * onOpen blocks wrote nothing, so `active ui_settingsSection` matched no row
	 * and the rail stayed stuck on the first tab whichever one was open. */
	{ "ui_settingsSection", "sound" },
	{ "ui_voteTimelimit", "20" },
	{ "ui_voteScorelimit", "0" },
	{ "ui_botCount", "0" },
	{ "ui_botProfile", "visor" },
	{ "ui_botName", "" },
	{ "ui_botSkill", "3" },
	{ "ui_botTeam", "free" },
	{ "ui_hostListed", "0" },
	{ "ui_netGameType", "0" },
	{ "ui_globalpreset", "0" },
	{ "ui_mousePitch", "0" },
	{ "ui_lastRefreshDate", "" },
	{ "ui_Name", "" },
	{ "ui_specifyAddress", "" },
	{ NULL, NULL }
};

// Build the open-addressing hash from wui_uiStateDefaults. Idempotent — safe
// to call repeatedly; only the first call does work.
static void WiredUI_BuildStateDefaultsHash( void ) {
	const wiredUiStateDefault_t *it;
	if ( wui_stateDefaultsHashBuilt ) return;

	memset( wui_stateDefaultsHash, 0, sizeof( wui_stateDefaultsHash ) );
	for ( it = wui_uiStateDefaults; it->key; it++ ) {
		unsigned int slot = WiredUI_StateKeyHash( it->key ) & ( WUI_STATE_DEFAULTS_BUCKETS - 1 );
		while ( wui_stateDefaultsHash[slot] ) {
			slot = ( slot + 1 ) & ( WUI_STATE_DEFAULTS_BUCKETS - 1 );
		}
		wui_stateDefaultsHash[slot] = it;
	}
	wui_stateDefaultsHashBuilt = qtrue;
}

// O(1) lookup. Returns NULL if `key` isn't in the table. Lazily builds the
// hash on first call so callers don't need an explicit init step.
static const wiredUiStateDefault_t *WiredUI_StateLookup( const char *key ) {
	unsigned int slot, start;
	if ( !key || !key[0] ) return NULL;
	if ( !wui_stateDefaultsHashBuilt ) WiredUI_BuildStateDefaultsHash();

	slot = WiredUI_StateKeyHash( key ) & ( WUI_STATE_DEFAULTS_BUCKETS - 1 );
	start = slot;
	do {
		const wiredUiStateDefault_t *e = wui_stateDefaultsHash[slot];
		if ( !e ) return NULL;
		if ( !Q_stricmp( e->key, key ) ) return e;
		slot = ( slot + 1 ) & ( WUI_STATE_DEFAULTS_BUCKETS - 1 );
	} while ( slot != start );
	return NULL;
}

qboolean WiredUI_IsStoreStateKey( const char *key ) {
	return WiredUI_StateLookup( key ) != NULL;
}

static qboolean WiredUI_IsPersistedStateKey( const char *key ) {
	if ( !key || !key[0] ) {
		return qfalse;
	}

	if ( !Q_stricmp( key, "ui_theme" ) ) {
		return qfalse;
	}
	if ( !Q_stricmp( key, "ui_selectedServerAddr" )
	  || !Q_stricmp( key, "ui_selectedServerName" )
	  || !Q_stricmp( key, "ui_selectedDemo" )
	  || !Q_stricmp( key, "ui_joinPasswordError" ) ) {
		return qfalse;
	}
	if ( !Q_stricmp( key, "ui_settingsSection" ) ) {
		/* Reopening the settings menu should land on its own default tab, not
		 * on whatever was open when the game was last closed. */
		return qfalse;
	}
	if ( !Q_stricmp( key, "ui_palette_mode" )
	  || !Q_stricmp( key, "ui_palette_accent" ) ) {
		/* Cvar layer (CVAR_ARCHIVE) handles persistence; the store-state
		 * write path would double-persist these into wired_ui_state.dat. */
		return qfalse;
	}

	if ( WiredUI_IsStoreStateKey( key ) ) {
		return qtrue;
	}

	return ( Q_stricmpn( key, "ui_", 3 ) == 0 );
}

static qboolean WiredUI_IsTransientServerSelectionKey( const char *key ) {
	return key && ( !Q_stricmp( key, "ui_selectedServerAddr" )
		|| !Q_stricmp( key, "ui_selectedServerName" ) );
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI: failed to save UI state (%s)\n", WIRED_UI_STATE_FILE );
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

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: saved %d UI state entries\n", writeCtx.wrote );
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
	int ignoredTransientServerSelectionCount = 0;

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
			if ( WiredUI_IsTransientServerSelectionKey( key ) ) {
				ignoredTransientServerSelectionCount++;
			}
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

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: loaded %d UI state entries (ignored transient server selection %d)\n",
		loadedCount, ignoredTransientServerSelectionCount );
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI: Lua store.%s failed: %s\n",
			functionName, err ? err : "unknown" );
		lua_pop( L, 1 );
		lua_pop( L, 1 );
		return qfalse;
	}

	lua_pop( L, 1 );
	return qtrue;
}

static const char *WiredUI_StateDefaultValue( const char *key ) {
	const wiredUiStateDefault_t *e = WiredUI_StateLookup( key );
	if ( e && e->defaultValue ) return e->defaultValue;
	return "";
}

void WiredUI_StateGetString( const char *key, char *out, int outSize ) {
	const wiredUiStateDefault_t *def;

	if ( !out || outSize <= 0 ) {
		return;
	}

	out[0] = '\0';
	if ( !key || !key[0] ) {
		return;
	}

	// Single hash lookup: serves both the IsStoreStateKey check and the
	// default-value fallback below. Replaces three linear walks of
	// wui_uiStateDefaults from the previous implementation.
	def = WiredUI_StateLookup( key );

	if ( def ) {
		wuiStoreEntry_t *entry = WiredStore_Get( key );
		if ( entry ) {
			Q_strncpyz( out, entry->text, outSize );
			if ( !out[0] && def->defaultValue ) {
				Q_strncpyz( out, def->defaultValue, outSize );
			}
			return;
		}
		Q_strncpyz( out, def->defaultValue ? def->defaultValue : "", outSize );
		return;
	}

	Cvar_VariableStringBuffer( key, out, outSize );
}

int WiredUI_StateGetInt( const char *key ) {
	char buf[256];
	WiredUI_StateGetString( key, buf, sizeof( buf ) );
	return atoi( buf );
}

/* value-text resolution for cvar-bound items. Mirrors the legacy
 * SCR switch (cl_wired_ui.c label-and-value branch) so both renderers
 * stay in lockstep during the transition. Forward decls reach the
 * file-static MULTI/edit/bind state without exposing them. */
static qboolean wui_waitingForKey;
static wiredItemDef_t *wui_bindItem;
static qboolean       wui_editingField;
static wiredItemDef_t *wui_editItem;
static int            wui_editCursorPos;
/* legacy focus index — single definition (was previously a tentative
 * def here + an initialized def lower in the file; C merges those into one
 * object, but the duplicate read as two variables and invited a "two
 * highlights" misdiagnosis, so it is consolidated to this one initialized
 * definition). -1 = no top-level item focused (focus may be a nested child,
 * tracked by wui_focusedItemPtr). */
static int             wui_focusItem = -1;
/* authoritative focused item ptr — supports nested children
 * (top-level wui_focusItem index can only address menu->items[N] directly). */
static wiredItemDef_t *wui_focusedItemPtr;
/* authoritative mouse-hovered item ptr (real definition sits lower, with the
 * other pointer-state statics). Forward-declared here so WiredUI_GetHoveredItem
 * — defined next to WiredUI_GetFocusedItem — can read it. Tentative-def merge
 * folds this into the single initialized definition below. */
static wiredItemDef_t *wui_hoveredItemPtr;

typedef struct {
	qboolean valid;
	char address[MAX_STRING_CHARS];
	int selectionGeneration;
	char secret[33];
} wiredPasswordPrompt_t;

static wiredPasswordPrompt_t wui_passwordPrompt;

#define WUI_SECURE_JOIN_PASSWORD_BINDING "@secure_join_password"

static qboolean WiredUI_IsSecureJoinPasswordItem( const wiredItemDef_t *item ) {
	return item && !Q_stricmp( item->cvar, WUI_SECURE_JOIN_PASSWORD_BINDING );
}

static void WiredUI_ClearPasswordPromptState( qboolean clearError ) {
	Q_SecureZeroMemory( &wui_passwordPrompt, sizeof( wui_passwordPrompt ) );
	WiredUI_StateSetString( "ui_password_server_name", "" );
	if ( clearError )
		WiredUI_StateSetString( "ui_joinPasswordError", "" );
}

/* One cancellation boundary for every authored password dismissal path.  The
 * origin is fixed product metadata, never user input.  Publish only the
 * postcondition after PopMenu has run: no secret bytes or prior target are
 * observable through this diagnostic. */
static void WiredUI_CancelPasswordPrompt( const char *origin ) {
	const char *top;
	int depth;
	char target[256];
	char error[256];

	wui_editingField = qfalse;
	wui_editItem = NULL;
	wui_editCursorPos = 0;
	WiredUI_ClearPasswordPromptState( qtrue );
	WiredUI_PopMenu();
	WiredUI_StateGetString( "ui_password_server_name", target, sizeof( target ) );
	WiredUI_StateGetString( "ui_joinPasswordError", error, sizeof( error ) );
	depth = WiredUI_GetMenuStackDepth();
	top = WiredUI_GetMenuStackTop();
	if ( !top[0] ) top = "none";
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: password cancel postcondition origin=%s valid=%d secret_length=%d editing=%d target_empty=%d error_empty=%d top=%s depth=%d\n",
		origin, wui_passwordPrompt.valid ? 1 : 0,
		(int)strlen( wui_passwordPrompt.secret ),
		wui_editingField ? 1 : 0, target[0] ? 0 : 1,
		error[0] ? 0 : 1, top, depth );
	Q_SecureZeroMemory( target, sizeof( target ) );
	Q_SecureZeroMemory( error, sizeof( error ) );
}

/* Dedicated edit path for the join secret. It never calls State/Store/Cvar
 * APIs and commits through a full-buffer erase so deleted/replaced suffixes
 * cannot remain beyond the terminating NUL. */
static qboolean WiredUI_HandleSecureJoinEditKey( int key ) {
	char buff[sizeof( wui_passwordPrompt.secret )];
	int len;
	qboolean changed = qfalse;

	Q_strncpyz( buff, wui_passwordPrompt.secret, sizeof( buff ) );
	len = (int)strlen( buff );
	if ( wui_editCursorPos > len ) wui_editCursorPos = len;

	if ( key & K_CHAR_FLAG ) {
		int ch = key & ~K_CHAR_FLAG;
		if ( ch == 'h' - 'a' + 1 ) {
			if ( wui_editCursorPos > 0 ) {
				memmove( &buff[wui_editCursorPos - 1], &buff[wui_editCursorPos],
					(size_t)( len + 1 - wui_editCursorPos ) );
				wui_editCursorPos--;
				changed = qtrue;
			}
		} else if ( ch == 'a' - 'a' + 1 ) {
			wui_editCursorPos = 0;
		} else if ( ch == 'e' - 'a' + 1 ) {
			wui_editCursorPos = len;
		} else if ( ch == 'v' - 'a' + 1 ) {
			char *clip = Sys_GetClipboardData();
			if ( clip ) {
				int clipBytes = (int)strlen( clip );
				int pasteLen = clipBytes;
				int space = 32 - len;
				if ( pasteLen > space ) pasteLen = space;
				for ( int i = 0; i < pasteLen; i++ ) {
					unsigned char c = (unsigned char)clip[i];
					if ( c < 0x20 || c > 0x7e || c == '\\' || c == ';' || c == '"' ) {
						pasteLen = i;
						break;
					}
				}
				if ( pasteLen > 0 ) {
					memmove( &buff[wui_editCursorPos + pasteLen],
						&buff[wui_editCursorPos], (size_t)( len + 1 - wui_editCursorPos ) );
					memcpy( &buff[wui_editCursorPos], clip, (size_t)pasteLen );
					wui_editCursorPos += pasteLen;
					changed = qtrue;
				}
				Q_SecureZeroMemory( clip, (size_t)clipBytes );
				Z_Free( clip );
			}
		} else if ( ch >= 0x20 && ch <= 0x7e && ch != '\\' && ch != ';'
			&& ch != '"' && len < 32 ) {
			memmove( &buff[wui_editCursorPos + 1], &buff[wui_editCursorPos],
				(size_t)( len + 1 - wui_editCursorPos ) );
			buff[wui_editCursorPos++] = (char)ch;
			changed = qtrue;
		}
	} else {
		switch ( key ) {
		case K_ESCAPE:
			WiredUI_CancelPasswordPrompt( "edit-escape" );
			break;
		case K_ENTER:
		case K_KP_ENTER:
		case K_TAB:
			wui_editingField = qfalse;
			wui_editItem = NULL;
			break;
		case K_BACKSPACE:
			if ( wui_editCursorPos > 0 ) {
				int pos = wui_editCursorPos - 1;
				if ( keys[K_CTRL].down ) {
					pos = wui_editCursorPos;
					while ( pos > 0 && buff[pos - 1] == ' ' ) pos--;
					while ( pos > 0 && buff[pos - 1] != ' ' ) pos--;
				}
				memmove( &buff[pos], &buff[wui_editCursorPos],
					(size_t)( len + 1 - wui_editCursorPos ) );
				wui_editCursorPos = pos;
				changed = qtrue;
			}
			break;
		case K_DEL:
		case K_KP_DEL:
			if ( wui_editCursorPos < len ) {
				memmove( &buff[wui_editCursorPos], &buff[wui_editCursorPos + 1],
					(size_t)( len - wui_editCursorPos ) );
				changed = qtrue;
			}
			break;
		case K_LEFTARROW:
		case K_KP_LEFTARROW:
			if ( wui_editCursorPos > 0 ) wui_editCursorPos--;
			break;
		case K_RIGHTARROW:
		case K_KP_RIGHTARROW:
			if ( wui_editCursorPos < len ) wui_editCursorPos++;
			break;
		case K_HOME:
		case K_KP_HOME:
			wui_editCursorPos = 0;
			break;
		case K_END:
		case K_KP_END:
			wui_editCursorPos = len;
			break;
		default:
			break;
		}
	}
	if ( changed ) {
		Q_SecureZeroMemory( wui_passwordPrompt.secret,
			sizeof( wui_passwordPrompt.secret ) );
		Q_strncpyz( wui_passwordPrompt.secret, buff,
			sizeof( wui_passwordPrompt.secret ) );
	}
	Q_SecureZeroMemory( buff, sizeof( buff ) );
	return qtrue;
}

static qboolean WiredUI_IsSafePasswordValue( const char *value ) {
	const unsigned char *p = (const unsigned char *)value;
	if ( !value || !value[0] || strlen( value ) > 32
	     || !Info_ValidateKeyValue( value ) ) return qfalse;
	for ( ; *p; p++ ) {
		if ( *p < 0x20 || *p > 0x7e ) return qfalse;
	}
	return qtrue;
}

const char *WiredUI_BoundValueText( const wiredItemDef_t *item, char *out, int outSize ) {
	char cvarBuf[256];

	if ( !out || outSize <= 0 ) return out;
	out[0] = '\0';
	if ( !item || !item->cvar[0] ) return out;

	if ( WiredUI_IsSecureJoinPasswordItem( item ) )
		Q_strncpyz( cvarBuf, wui_passwordPrompt.secret, sizeof( cvarBuf ) );
	else
		WiredUI_StateGetString( item->cvar, cvarBuf, sizeof( cvarBuf ) );

	switch ( item->type ) {
	case ITEM_TYPE_YESNO:
		Q_strncpyz( out, atof( cvarBuf ) != 0 ? "Yes" : "No", outSize );
		return out;

	case ITEM_TYPE_CHECKBOX:
		/* Component-library F2: the checkbox draws a box + check glyph as its
		 * value cell (cl_wired_clay.c), so it emits NO value text — otherwise the
		 * default case below would print the raw "1"/"0" beside the box. */
		out[0] = '\0';
		return out;

	case ITEM_TYPE_SPINNER:
		/* Component-library: the spinner draws [-] value [+] as its own value
		 * cell (cl_wired_clay.c), so it emits NO trailing value text here —
		 * otherwise the numeric readout would render twice. */
		out[0] = '\0';
		return out;

	case ITEM_TYPE_MULTI:
		if ( item->populateCallback[0] ) {
			wuiPopulateCallback_t pop = WiredUI_GetPopulateCallback( item->populateCallback );
			if ( !pop ) {
				Q_strncpyz( out, "<missing populate callback>", outSize );
				return out;
			}
			{
				wuiPopulateResult_t res;
				qboolean found = qfalse;
				int j;
				memset( &res, 0, sizeof( res ) );
				pop( &res );
				switch ( res.state ) {
				case WUI_POPULATE_LOADING:
					Q_strncpyz( out, "Scanning…", outSize );
					return out;
				case WUI_POPULATE_EMPTY:
					Q_strncpyz( out, "No devices detected", outSize );
					return out;
				case WUI_POPULATE_ERROR:
					Q_strncpyz( out, "Enumeration failed — Use default", outSize );
					return out;
				case WUI_POPULATE_SUCCESS:
				case WUI_POPULATE_PARTIAL:
					for ( j = 0; j < res.count; j++ ) {
						if ( res.values && res.values[j] &&
						     !Q_stricmp( cvarBuf, res.values[j] ) ) {
							Q_strncpyz( out, res.names[j], outSize );
							found = qtrue;
							break;
						}
					}
					if ( !found ) {
						if ( cvarBuf[0] ) {
							Com_sprintf( out, outSize, "%s (not present)", cvarBuf );
						} else {
							Q_strncpyz( out, "(System Default)", outSize );
						}
					}
					return out;
				default:
					Q_strncpyz( out, cvarBuf, outSize );
					return out;
				}
			}
		}
		else if ( item->multiData ) {
			int j;
			for ( j = 0; j < item->multiData->count; j++ ) {
				if ( item->multiData->isStringList ) {
					if ( !Q_stricmp( cvarBuf, item->multiData->strValues[j] ) ) {
						Q_strncpyz( out, item->multiData->labels[j], outSize );
						return out;
					}
				} else {
					if ( item->multiData->floatValues[j] == atof( cvarBuf ) ) {
						Q_strncpyz( out, item->multiData->labels[j], outSize );
						return out;
					}
				}
			}
			if ( cvarBuf[0] ) Q_strncpyz( out, cvarBuf, outSize );
		}
		return out;

	case ITEM_TYPE_SLIDER:
		Com_sprintf( out, outSize, "%.1f", atof( cvarBuf ) );
		return out;

	case ITEM_TYPE_BIND:
		if ( wui_waitingForKey && wui_bindItem == item ) {
			Q_strncpyz( out, "Press a key...", outSize );
			return out;
		}
		{
			const char *key1 = NULL, *key2 = NULL;
			int k;
			for ( k = 0; k < MAX_KEYS; k++ ) {
				const char *b = Key_GetBinding( k );
				if ( b && !Q_stricmp( b, item->cvar ) ) {
					if ( !key1 ) key1 = Key_KeynumToString( k );
					else if ( !key2 ) { key2 = Key_KeynumToString( k ); break; }
				}
			}
			if ( key1 && key2 ) {
				Com_sprintf( out, outSize, "%s ^7or %s", key1, key2 );
			} else if ( key1 ) {
				Q_strncpyz( out, key1, outSize );
			} else {
				Q_strncpyz( out, "---", outSize );
			}
		}
		return out;

	case ITEM_TYPE_EDITFIELD:
	case ITEM_TYPE_NUMERICFIELD:
		if ( item->password ) {
			for ( int i = 0; cvarBuf[i]; i++ ) cvarBuf[i] = '*';
		}
		if ( wui_editingField && wui_editItem == item ) {
			int curPos = wui_editCursorPos;
			qboolean showCursor = ( (int)( cls.realtime / 250 ) & 1 );
			int cvarLen = (int) strlen( cvarBuf );
			if ( curPos > cvarLen ) curPos = cvarLen;
			Q_strncpyz( out, cvarBuf, curPos + 1 );
			{
				qstring_t qs = QS_WrapExisting( out, outSize );
				QS_AppendChar( &qs, showCursor ? '_' : ' ' );
				QS_Append( &qs, &cvarBuf[curPos] );
			}
		} else {
			Q_strncpyz( out, cvarBuf, outSize );
		}
		Q_SecureZeroMemory( cvarBuf, sizeof( cvarBuf ) );
		return out;

	default:
		Q_strncpyz( out, cvarBuf, outSize );
		return out;
	}
}

float WiredUI_SliderFraction( const wiredItemDef_t *item ) {
	char  cvarBuf[64];
	float val, range, frac;

	if ( !item || !item->cvar[0] ) return 0.0f;
	WiredUI_StateGetString( item->cvar, cvarBuf, sizeof( cvarBuf ) );
	val   = atof( cvarBuf );
	range = item->sliderData.maxVal - item->sliderData.minVal;
	frac  = ( range > 0 ) ? ( val - item->sliderData.minVal ) / range : 0.0f;
	if ( frac < 0.0f ) frac = 0.0f;
	if ( frac > 1.0f ) frac = 1.0f;
	return frac;
}

/* focus accessors. Legacy retains focus authority via wui_focusItem
 * (menu-local index) — driven by mouse hover, arrow nav, setfocus action,
 * Tab cycle. Compositor reads this accessor instead of its own parallel
 * wui_panel_state.focusedId (which only tracks Tab cycle today). Flips
 * once mouse/arrow paths land their compositor-side updates. */
const wiredItemDef_t *WiredUI_GetFocusedItem( void ) {
	wiredMenuDef_t *menu = WiredUI_GetActiveMenu();
	wiredItemDef_t *item;

	if ( !menu ) return NULL;
	/* prefer the authoritative pointer (may be nested). */
	if ( wui_focusedItemPtr && WiredUI_ItemCanFocus( wui_focusedItemPtr ) ) {
		return wui_focusedItemPtr;
	}
	if ( wui_focusItem < 0 || wui_focusItem >= menu->itemCount ) return NULL;
	item = menu->items[ wui_focusItem ];
	if ( !item ) return NULL;
	if ( !WiredUI_ItemCanFocus( item ) ) return NULL;
	return item;
}

/* Authoritative pointer-hover accessor. Formalizes the direct wui_hoveredItemPtr
 * read the tooltip path already performs; the framework core reads this for the
 * HOVER visual state. Distinct from focus — mouse hover updates hovered,
 * keyboard nav updates focused. */
const wiredItemDef_t *WiredUI_GetHoveredItem( void ) {
	if ( wui_hoveredItemPtr && WiredUI_ItemCanFocus( wui_hoveredItemPtr ) ) {
		return wui_hoveredItemPtr;
	}
	return NULL;
}

qhandle_t WiredUI_GradientBarShader( void ) {
	return wui_gradientBarShader;
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

qboolean WiredUI_StateListContainsValue( const char *list, const char *value ) {
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






qboolean WiredUI_ItemVisibleByCvarRules( const wiredItemDef_t *item ) {
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

	/* Disable is now orthogonal to visibility: the enable/disable-cvar test lives
	 * in WiredUI_ItemIsEnabled (framework core). A disabled item is HIDDEN only in
	 * legacy `enableMode hide` mode (default) — this preserves every existing menu
	 * exactly. `enableMode dim` items stay visible (rendered greyed +
	 * non-interactive by the framework), so visibility no longer culls them. */
	if ( ( item->enableCvar[0] || item->disableCvar[0] ) && !item->enableModeDim ) {
		if ( !WiredUI_ItemIsEnabled( item ) ) return qfalse;
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

	/* Disabled `dim`-mode items render but are non-interactive: drop them from
	 * BOTH focus traversals (wui_collect_focusable) and mouse-hover acceptance
	 * (WiredUI_ItemAcceptsMouseHover routes through here). `hide`-mode disabled
	 * items are already culled by WiredUI_ItemShouldRender below. */
	if ( item->enableModeDim && !WiredUI_ItemIsEnabled( item ) ) {
		return qfalse;
	}

	return WiredUI_ItemShouldRender( item );
}

/* Mouse-hover acceptance: mirrors the keyboard focus collector's rules
 * (wui_collect_focusable_recursive) so the same item taxonomy drives
 * both input paths. Pure layout containers (no action[]) are walked
 * THROUGH, not bound, and decorative TEXT items are not bound. Without
 * this filter, Clay's hit-test returns whatever element sits under the
 * cursor — including outer card containers and inner decoration labels
 * — and the legacy mouse path then flood-fills the focus highlight
 * gradient (cl_wired_clay.c WiredUI_GetFocusedItem branch) plus fires
 * wui_sfxFocus on every container traversal. */
static qboolean WiredUI_ItemAcceptsMouseHover( wiredItemDef_t *item ) {
	if ( !item ) return qfalse;
	if ( !WiredUI_ItemCanFocus( item ) ) return qfalse;
	if ( ( item->isFlexContainer || item->childCount > 0 )
	  && !item->action[0]
	  && !item->mouseEnter[0]
	  && !item->mouseExit[0]
	  && !item->onFocus[0]
	  && !item->leaveFocus[0] ) {
		return qfalse;
	}
	if ( item->type == ITEM_TYPE_TEXT
	  && !item->action[0]
	  && !item->mouseEnter[0]
	  && !item->mouseExit[0]
	  && !item->onFocus[0]
	  && !item->leaveFocus[0] ) {
		return qfalse;
	}
	return qtrue;
}

/* Depth-first flatten of the menu's focusable items so nested children
 * inside containers become reachable by K_UPARROW / K_DOWNARROW.
 * Layout-only containers (no action[] script) are walked into but not
 * added — focus belongs on actionable leaves (buttons, dropdowns,
 * editfields, sliders, bind rows, listboxes). Containers that DO carry
 * an action[] are themselves the focus target — their nested children
 * are decorative composition (num + label + sub + hot + arrow), so
 * the container is added and recursion stops. */
static void wui_collect_focusable_recursive( wiredItemDef_t *item,
                                              wiredItemDef_t **flat,
                                              int maxFlat, int *flatCount )
{
	int i;
	if ( !item || *flatCount >= maxFlat ) return;
	/* Containers with their own action script are themselves the focus
	 * target — their nested leaves are decorative composition, not
	 * separate actionable items. Add the container and stop descending. */
	if ( ( item->isFlexContainer || item->childCount > 0 ) && item->action[0] ) {
		if ( WiredUI_ItemCanFocus( item ) ) {
			flat[ ( *flatCount )++ ] = item;
		}
		return;
	}
	/* Containers without an action are pure layout — descend so the
	 * focusable leaves they hold become reachable. */
	if ( item->isFlexContainer || item->childCount > 0 ) {
		for ( i = 0; i < item->childCount; i++ ) {
			wui_collect_focusable_recursive( item->children[ i ], flat, maxFlat, flatCount );
		}
		return;
	}
	/* ITEM_TYPE_TEXT (type 0) is non-actionable display text — labels,
	 * subheaders, decorative copy. Skip unless it carries an explicit
	 * action script. */
	if ( item->type == ITEM_TYPE_TEXT && !item->action[0] ) {
		return;
	}
	if ( WiredUI_ItemCanFocus( item ) ) {
		flat[ ( *flatCount )++ ] = item;
	}
}

static void wui_collect_focusable( wiredMenuDef_t *menu,
                                    wiredItemDef_t **flat,
                                    int maxFlat, int *flatCount )
{
	int i;
	*flatCount = 0;
	if ( !menu ) return;
	for ( i = 0; i < menu->itemCount; i++ ) {
		wui_collect_focusable_recursive( menu->items[ i ], flat, maxFlat, flatCount );
	}
}

static int wui_find_in_flat( wiredItemDef_t **flat, int flatCount,
                              wiredItemDef_t *target )
{
	int i;
	if ( !target ) return -1;
	for ( i = 0; i < flatCount; i++ ) {
		if ( flat[ i ] == target ) return i;
	}
	return -1;
}

/* Update both the legacy top-level index (for back-compat with mouse +
 * listbox handlers) AND the new authoritative pointer. The index is set
 * to -1 when the focused item lives inside a container (i.e. is not at
 * top level), signalling read sites to consult the pointer. */
static void wui_set_focused( wiredMenuDef_t *menu, wiredItemDef_t *item )
{
	int i;
	wui_focusedItemPtr = item;
	wui_focusItem = -1;
	if ( !menu || !item ) return;
	for ( i = 0; i < menu->itemCount; i++ ) {
		if ( menu->items[ i ] == item ) {
			wui_focusItem = i;
			break;
		}
	}

	/* Focusing a populated listbox establishes an authoritative feeder
	 * selection even when its zero-initialized visual row is already 0. Without
	 * this, a one-row list can never fire its callback via keyboard Down because
	 * the clamped destination equals the pre-existing visual row. */
	if ( item->type == ITEM_TYPE_LISTBOX && item->feeder > 0 ) {
		int total = WiredUI_FeederCount( (int)item->feeder );
		if ( total > 0 ) {
			int row = item->listSelectedRow;
			if ( row < 0 || row >= total ) row = 0;
			item->listSelectedRow = row;
			WiredUI_FeederSelection( (int)item->feeder, row );
		}
	}
}

/* WiredUI F4 (settings tab strip): a "tab" in the settings cluster is a nav-rail
 * button bound to the ui_settingsSection cvar (`active ui_settingsSection "<sec>"`)
 * whose action does the `close ; open <submenu>` section swap. Detect one so
 * Left/Right can jump between sibling tabs without a Tab-into-the-rail dance. The
 * activeCvar test is the discriminator — ordinary buttons don't bind it. */
static qboolean wui_item_is_settings_tab( const wiredItemDef_t *item )
{
	if ( !item ) return qfalse;
	if ( item->type != ITEM_TYPE_BUTTON ) return qfalse;
	return ( Q_stricmp( item->activeCvar, "ui_settingsSection" ) == 0 );
}

/* Collect the settings-tab nav rows of `menu` in authored (nav) order into
 * `out` (cap `max`), returning the count and, via `curIdx`, the index of the tab
 * matching the current ui_settingsSection value (-1 if none matches). Walks the
 * SAME focusable flatten as nav so nested rail rows are found. */
static int wui_collect_settings_tabs( wiredMenuDef_t *menu, wiredItemDef_t **out,
                                      int max, int *curIdx )
{
	wiredItemDef_t *flat[ 128 ];
	int             flatCount = 0;
	int             n = 0;
	int             i;
	char            cur[ 64 ];
	if ( curIdx ) *curIdx = -1;
	if ( !menu ) return 0;
	WiredUI_StateGetString( "ui_settingsSection", cur, sizeof( cur ) );
	wui_collect_focusable( menu, flat, 128, &flatCount );
	for ( i = 0; i < flatCount && n < max; i++ ) {
		if ( !wui_item_is_settings_tab( flat[ i ] ) ) continue;
		if ( curIdx && *curIdx < 0 && Q_stricmp( flat[ i ]->activeValue, cur ) == 0 ) {
			*curIdx = n;
		}
		out[ n++ ] = flat[ i ];
	}
	return n;
}

/* WiredUI F4 (default button): return the first focusable item in `menu` whose
 * `defaultButton` flag is set, or NULL if none authored one. Searches the same
 * depth-first focusable flatten Tab/arrow nav walks, so a default button nested
 * inside a container (the dialog footer) is found. */
static wiredItemDef_t *wui_find_default_button( wiredMenuDef_t *menu )
{
	wiredItemDef_t *flat[ 128 ];
	int             flatCount = 0;
	int             i;
	if ( !menu ) return NULL;
	wui_collect_focusable( menu, flat, 128, &flatCount );
	for ( i = 0; i < flatCount; i++ ) {
		if ( flat[ i ]->defaultButton ) return flat[ i ];
	}
	return NULL;
}

/* WiredUI F4 (initial focus): give a freshly-opened menu keyboard focus so Enter
 * works immediately. Preference order:
 *   1. the authored `defaultButton` (dialogs: Enter confirms without a Tab),
 *   2. the first focusable control in nav order.
 * Marks focus as keyboard-provenance so the focus ring paints (WiredUI_FocusRingFor
 * gates on wui_ix.focusFromKeyboard). No-op when the menu has no focusable items
 * (pure display menus — HUD overlays, loading screens). */
/* fwd-decl: wui_focusFromMouse is defined further down (with the other
 * focus/hover statics); WiredUI_SetInitialFocus below references it before
 * that point. */
static qboolean wui_focusFromMouse;  /* fwd-decl, defined below */

static void WiredUI_SetInitialFocus( wiredMenuDef_t *menu )
{
	wiredItemDef_t *target;
	if ( !menu ) return;

	/* Scope guard (do not disturb non-dialog menus): auto-seed initial focus
	 * ONLY for menus that need Enter/keyboard entry on open — MODAL dialogs, or
	 * any menu that explicitly authored a `defaultButton`. Ordinary menus (main,
	 * ingame, the settings panels) keep today's mouse-first, no-initial-focus
	 * behaviour, so no focus ring appears before the user actually navigates. */
	target = wui_find_default_button( menu );
	if ( !target ) {
		/* Settings cluster: land focus on the ACTIVE nav tab so the Left/Right
		 * tab-strip nav has a starting point AND stays continuous across a tab
		 * switch (the `close ; open` swap re-opens here and re-homes focus on the
		 * new section's tab). Only fires when the menu actually carries settings
		 * tabs, so non-settings menus are unaffected. */
		wiredItemDef_t *tabs[ 32 ];
		int             curIdx = -1;
		int             tabCount = wui_collect_settings_tabs( menu, tabs, 32, &curIdx );
		if ( tabCount > 0 ) {
			target = ( curIdx >= 0 ) ? tabs[ curIdx ] : tabs[ 0 ];
		}
	}
	if ( !target ) {
		wiredItemDef_t *flat[ 128 ];
		int             flatCount = 0;
		if ( !menu->modal ) return;   /* non-modal, no defaultButton, no tabs => unchanged */
		wui_collect_focusable( menu, flat, 128, &flatCount );
		if ( flatCount > 0 ) target = flat[ 0 ];
	}
	if ( target ) {
		wui_set_focused( menu, target );
		wui_focusFromMouse       = qfalse;
		wui_ix.focusFromKeyboard = qtrue;
	}
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

// Open-addressing index for the populate-callback registry. Each bucket holds
// (1 + index into wui_populateCallbacks) so 0 = empty. Per-frame
// WiredUI_GetPopulateCallback lookups become O(1) instead of O(N) per item.
#define WUI_POPULATE_CB_BUCKETS 64
static unsigned char wui_populateCallbacksHash[WUI_POPULATE_CB_BUCKETS];

static void WiredUI_PopulateHashInsert( int idx ) {
	unsigned int slot = WiredUI_StateKeyHash( wui_populateCallbacks[idx].name )
		& ( WUI_POPULATE_CB_BUCKETS - 1 );
	while ( wui_populateCallbacksHash[slot] ) {
		// already-present entry with the same name overwrites in place; the
		// register path handles that before getting here, so we only land here
		// for genuine collisions.
		slot = ( slot + 1 ) & ( WUI_POPULATE_CB_BUCKETS - 1 );
	}
	wui_populateCallbacksHash[slot] = (unsigned char)( idx + 1 );
}

// ── feeder registry ───────────────────────────────────────────────────

// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding) — fields ordered by semantics, not packing; 8 bytes of tail padding accepted
typedef struct {
	int                       feederID;
	char                      name[32];     // optional symbolic name; .wmenu can write `feeder "name"`
	wiredFeederCount_t        count;
	wiredFeederItemText_t     itemText;
	wiredFeederSelection_t    selection;
	wiredFeederItemIcon_t     itemIcon;     // optional, NULL if feeder has no icons
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
	/* Focus default = the live theme accent ($accent). NOTE: this init runs
	 * before the implicit ui/_tokens.wui include, so the token table is
	 * usually empty here — the guarded read leaves the baked literal in place,
	 * and becomes theme-driven only if this reset is re-invoked after tokens
	 * load. .wui menus can also override per-theme with `focuscolor $accent`.
	 *
	 * FIX 2026-08-17: this read $primary_cyan while claiming to follow
	 * ui_palette_accent. It did not — the v2 accent overlays only rewrite
	 * accent/accentDim/accentSoft/accentWash, so primary_cyan stayed v1
	 * #00b4d8 under every accent. Because this default is the one that
	 * survives (the token table is empty at this point), the baked literal is
	 * what actually shipped: v1 cyan, on every theme. Read $accent and bake
	 * the shipped amber default (#f4a03a). */
	Vector4Set( wui_assetGlobals.focusColor, 0.957f, 0.627f, 0.227f, 1.0f );
	{
		extern const char *WiredToken_Find( const char *name );
		const char *v = WiredToken_Find( "accent" );
		unsigned    r, g, b;
		if ( v && v[0] == '#' && ( strlen( v ) == 7 || strlen( v ) == 9 )
		     && sscanf( v + 1, "%2x%2x%2x", &r, &g, &b ) == 3 ) {
			wui_assetGlobals.focusColor[0] = (float) r / 255.0f;
			wui_assetGlobals.focusColor[1] = (float) g / 255.0f;
			wui_assetGlobals.focusColor[2] = (float) b / 255.0f;
		}
	}
	wui_assetGlobals.shadowX = 1.0f;
	wui_assetGlobals.shadowY = 1.0f;
	Vector4Set( wui_assetGlobals.gradientBarColor, 0, 0, 0, 0 );
}

// Theme selection is handled inside scripts/menus.lua via the global metatable
// cvar bridge (ui_theme readable as a Lua global). No manifest path function needed.

void WiredUI_RegisterFeeder( int feederID, const char *name,
                              wiredFeederCount_t count,
                              wiredFeederItemText_t itemText,
                              wiredFeederSelection_t selection ) {
	// update existing
	for ( int i = 0; i < wui_numFeeders; i++ ) {
		if ( wui_feeders[i].active && wui_feeders[i].feederID == feederID ) {
			wui_feeders[i].count = count;
			wui_feeders[i].itemText = itemText;
			wui_feeders[i].selection = selection;
			if ( name && name[0] )
				Q_strncpyz( wui_feeders[i].name, name, sizeof( wui_feeders[i].name ) );
			return;
		}
	}
	if ( wui_numFeeders >= WIRED_MAX_FEEDERS ) return;
	wui_feeders[wui_numFeeders].feederID = feederID;
	wui_feeders[wui_numFeeders].name[0] = '\0';
	if ( name && name[0] )
		Q_strncpyz( wui_feeders[wui_numFeeders].name, name, sizeof( wui_feeders[wui_numFeeders].name ) );
	wui_feeders[wui_numFeeders].count = count;
	wui_feeders[wui_numFeeders].itemText = itemText;
	wui_feeders[wui_numFeeders].selection = selection;
	wui_feeders[wui_numFeeders].itemIcon = NULL;
	wui_feeders[wui_numFeeders].active = qtrue;
	wui_numFeeders++;
}

void WiredUI_RegisterFeederIcon( int feederID, wiredFeederItemIcon_t icon ) {
	for ( int i = 0; i < wui_numFeeders; i++ ) {
		if ( wui_feeders[i].active && wui_feeders[i].feederID == feederID ) {
			wui_feeders[i].itemIcon = icon;
			return;
		}
	}
}

int WiredUI_FeederIDByName( const char *name ) {
	if ( !name || !name[0] ) return 0;
	for ( int i = 0; i < wui_numFeeders; i++ ) {
		if ( wui_feeders[i].active && !Q_stricmp( wui_feeders[i].name, name ) ) {
			return wui_feeders[i].feederID;
		}
	}
	return 0;
}

qhandle_t WiredUI_FeederItemIcon( int feederID, int index ) {
	for ( int i = 0; i < wui_numFeeders; i++ ) {
		if ( wui_feeders[i].active && wui_feeders[i].feederID == feederID && wui_feeders[i].itemIcon ) {
			return wui_feeders[i].itemIcon( feederID, index );
		}
	}
	return 0;
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

/* WiredUI F4 (return-focus): the focused item on the menu that was on top when
 * a new menu was pushed OVER it. On pop, WiredUI_PopMenu restores focus to this
 * item so closing a dialog returns the caret to the control that opened it.
 * Indexed by the NEW top's depth-1 slot (i.e. wui_returnFocus[d] is the item to
 * restore when the menu at stack index d is popped). NULL = no saved focus (open
 * with default/first-focus as before). Cleared on CloseAllMenus. */
static wiredItemDef_t *wui_returnFocus[WIRED_MENU_STACK_DEPTH];


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

/* ── listbox scrollbar drag state ─────────────────────────────────────
 * Non-NULL while the user drags a listbox's vertical scrollbar thumb.
 * wui_listScrollGrabDY is the cursor's offset within the thumb at grab time
 * (so the thumb does not jump under the cursor). Mirrors the slider-drag
 * pattern; the flex scroll-container thumb drag lives in cl_wired_clay.c
 * (its scroll state is there). */
static wiredItemDef_t *wui_listScrollDragItem = NULL;
static float           wui_listScrollGrabDY   = 0.0f;

/* Spinner click-and-hold repeat: a held −/+ button steps once immediately, then
 * (after WUI_SPINNER_HOLD_DELAY_MS) auto-repeats every WUI_SPINNER_HOLD_RATE_MS
 * while the mouse button stays down. Latched on K_MOUSE1-down over a button
 * (WiredUI_KeyEvent), fired from WiredUI_TickFrame, cleared on release. */
#define WUI_SPINNER_HOLD_DELAY_MS  350   /* pause before auto-repeat kicks in */
#define WUI_SPINNER_HOLD_RATE_MS    60   /* interval between repeated steps */
static wiredItemDef_t *wui_spinnerHoldItem = NULL;
static int             wui_spinnerHoldDir  = 0;    /* +1 / -1 */
static int             wui_spinnerHoldNext = 0;    /* realtime of next repeat */

// ── text field editing state ──────────────────────────────────────────
static qboolean       wui_editingField = qfalse;
static wiredItemDef_t *wui_editItem = NULL;
static int            wui_editCursorPos = 0;
static int            wui_editPaintOffset = 0;

static float     wui_cursorX = 320.0f;
static float     wui_cursorY = 240.0f;
/* wui_focusItem is defined once near the top of the file (see the
 * consolidation note there); the duplicate definition that used to sit here
 * was removed. */
static wiredItemDef_t *wui_focusedItemPtr = NULL; // authoritative focused item ptr (supports nested children)
static wiredItemDef_t *wui_hoveredItemPtr = NULL; // authoritative mouse-hovered item ptr (supports nested children)
static qboolean        wui_focusFromMouse = qfalse;  // qtrue if focus came from mouse hover

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
typedef struct {
	qboolean valid;
	wiredMenuDef_t *menu;
	wiredItemDef_t *item;
	char menuName[64];
	char itemName[64];
	int feeder;
	int row;
	int listGeneration;
	int time;
	qboolean releaseObserved;
} wiredListboxClickLatch_t;

static wiredListboxClickLatch_t wui_listboxClickLatch;
static qboolean wui_compositorPointerDown;

void WiredUI_ResetListboxDoubleClick( const char *reason ) {
	if ( wui_listboxClickLatch.valid ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: listbox click phase=reset reason=%s menu=%s item=%s feeder=%d row=%d list_generation=%d\n",
			reason && reason[0] ? reason : "unspecified",
			wui_listboxClickLatch.menuName[0]
				? wui_listboxClickLatch.menuName : "none",
			wui_listboxClickLatch.itemName[0]
				? wui_listboxClickLatch.itemName : "none",
			wui_listboxClickLatch.feeder, wui_listboxClickLatch.row,
			wui_listboxClickLatch.listGeneration );
	}
	memset( &wui_listboxClickLatch, 0, sizeof( wui_listboxClickLatch ) );
	wui_listboxClickLatch.row = -1;
	wui_listboxClickLatch.listGeneration = -1;
}

static int WiredUI_ListboxIdentityGeneration( const wiredItemDef_t *item ) {
	if ( item && (int)item->feeder == FEEDER_SERVERS ) {
		return WiredFeeder_ServerDisplayGeneration();
	}
	return 0;
}

static void WiredUI_ReleaseCompositorPointer( const char *reason ) {
	qboolean wasDown = wui_compositorPointerDown;
	(void) WiredUI_CompositorMouseButton( qfalse );
	wui_compositorPointerDown = qfalse;
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: pointer phase=release reason=%s was_down=%d pointer_down=%d\n",
		reason && reason[0] ? reason : "unspecified", wasDown ? 1 : 0,
		wui_compositorPointerDown ? 1 : 0 );
}

/* ── overlay custom-draws ────────────────────────────────────────────
 *
 * The cursor sprite + hover tooltip render via the compositor
 * (WUI_LAYER_OVERLAY) rather than imperatively from WiredUI_Refresh. The
 * .wmenu item's resolved rect is ignored — both routines anchor to the
 * post-clamp wui_cursorX/wui_cursorY pixel coordinates (set by
 * WiredUI_MouseEvent). Layer gating (KEYCATCH_UI) lives in
 * wui_layer_active so the items are not emitted at all in gameplay; the
 * tooltip's own focus + delay gates live here in the routine. */

static void WiredUI_CustomDraw_Cursor( float x, float y, float w, float h, vec4_t color ) {
	vec4_t cursorTint = { 0.85f, 0.55f, 0.1f, 1.0f };

	( void ) x; ( void ) y; ( void ) w; ( void ) h; ( void ) color;

	if ( wui_cursorShader ) {
		/* The cursor art only occupies the sprite's lower-right quarter (the
		 * upper-left 3/4 is transparent padding), so a full-texture draw wastes
		 * 3/4 of the rect and renders tiny. Draw ONLY that quarter (UV 0.5,0.5→
		 * 1,1) stretched to fill the whole rect, doubling the visible arrow.
		 * Size scales with dpiScale (WiredUI is physical-pixel space; the old
		 * fixed 40px ignored HiDPI and read small). The arrow tip is the sprite
		 * hotspot; anchor the rect's top-left at the pointer. */
		float dpi  = WiredUI_GetDpiScale();
		float size = 28.0f * dpi;
		re.SetColor( cursorTint );
		re.DrawStretchPic( wui_cursorX, wui_cursorY, size, size,
		                   0.5f, 0.5f, 1.0f, 1.0f, wui_cursorShader );
		re.SetColor( NULL );
	} else {
		re.SetColor( cursorTint );
		WUI_FillRect( wui_cursorX - 1, wui_cursorY - 8, 2, 16, cursorTint );
		WUI_FillRect( wui_cursorX - 8, wui_cursorY - 1, 16, 2, cursorTint );
		re.SetColor( NULL );
	}
}

static void WiredUI_CustomDraw_Tooltip( float x, float y, float w, float h, vec4_t color ) {
	wiredMenuDef_t *menu;
	wiredItemDef_t *focus;
	float           tx, ty, tw, th;
	vec4_t          tipBg = { 0.0f, 0.0f, 0.0f, 0.85f };
	vec4_t          tipFg = { 1.0f, 1.0f, 1.0f, 0.95f };

	( void ) x; ( void ) y; ( void ) w; ( void ) h; ( void ) color;

	if ( !wui_focusFromMouse ) return;
	if ( wui_tooltipStartTime <= 0 ) return;
	if ( ( cls.realtime - wui_tooltipStartTime ) < WIRED_TOOLTIP_DELAY_MS ) return;

	menu = WiredUI_GetActiveMenu();
	if ( !menu ) return;
	/* resolve the hovered item through the authoritative pointer-based
	 * hover state, not the legacy top-level wui_focusItem index.
	 * wui_focusItem is forced to -1 when a NESTED flex child is hovered (the
	 * top-level scan at the hover-update site can't address children), so the
	 * old index guard suppressed tooltips for every nested item. The tooltip
	 * timer is already keyed on wui_hoveredItemPtr's tooltip, so consume the
	 * same pointer here. */
	focus = wui_hoveredItemPtr;
	if ( !focus || !focus->tooltip[0] ) return;

	tx = wui_cursorX + 16.0f;
	ty = wui_cursorY + 16.0f;
	tw = strlen( focus->tooltip ) * 8.0f + 8.0f;
	th = 16.0f;

	if ( tx + tw > (float)cls.glconfig.vidWidth )  tx = (float)cls.glconfig.vidWidth - tw;
	if ( ty + th > (float)cls.glconfig.vidHeight ) ty = wui_cursorY - th - 4.0f;

	WUI_FillRect( tx, ty, tw, th, tipBg );
	Text_Draw( focus->tooltip, tx + 4.0f, ty + 4.0f, FONT_UI, 8.0f, tipFg, TEXT_ALIGN_LEFT, 0 );
}

static void WiredUI_RegisterOverlayCustomDraws( void ) {
	wuiCustomDrawDef_t def;

	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:wui_cursor", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = WiredUI_CustomDraw_Cursor;
	WiredUI_RegisterCustomDraw( &def );

	memset( &def, 0, sizeof( def ) );
	Q_strncpyz( def.name, "custom:wui_tooltip", sizeof( def.name ) );
	def.isStateful        = qfalse;
	def.routine.stateless = WiredUI_CustomDraw_Tooltip;
	WiredUI_RegisterCustomDraw( &def );
}

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

/* The item whose dropdown popup is currently open, or NULL. The compositor
 * needs this to anchor the floating option list to the row's actual Clay
 * element (CLAY_ATTACH_TO_ELEMENT_WITH_ID) instead of the CPU-side
 * resolvedRect, which diverges for flex-container children. */
wiredItemDef_t *WiredUI_GetMultiDropdownItem( void ) {
	return wui_multiDropdownOpen ? wui_multiDropdownItem : NULL;
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

/* ── radio / segmented control (ITEM_TYPE_RADIOBUTTON) public helpers ─────────
 * A segmented control is a MULTI whose N options render inline as horizontal
 * segments instead of a dropdown. It shares the exact option-source machinery
 * above (cvarStrList / cvarFloatList → wiredMultiDef_t, populate callbacks) and
 * the same value↔index match logic, so radio and the faked dropdowns can never
 * diverge on "which option is selected". Both the renderer (cl_wired_clay.c) and
 * the input path consult these, keeping a single source of truth. */

/* Number of segments this control renders (its option count, clamped to the
 * multi maximum). 0 when the item has no option source. */
int WiredUI_RadioSegmentCount( wiredItemDef_t *item ) {
	wiredMultiOptions_t opts;
	if ( !item ) return 0;
	WiredUI_GetMultiOptions( item, &opts );
	return opts.count;
}

/* Display label for segment `index`, or NULL when out of range. The pointer is
 * owned by the item's multiData / populate result and is valid for this frame. */
const char *WiredUI_RadioSegmentLabel( wiredItemDef_t *item, int index ) {
	wiredMultiOptions_t opts;
	if ( !item ) return NULL;
	WiredUI_GetMultiOptions( item, &opts );
	if ( index < 0 || index >= opts.count ) return NULL;
	return opts.labels[index];
}

/* Index of the currently-selected segment (the option whose value matches the
 * bound cvar), or -1 when the cvar matches no option. */
int WiredUI_RadioSelectedIndex( wiredItemDef_t *item ) {
	wiredMultiOptions_t opts;
	char                curBuf[256];
	if ( !item ) return -1;
	WiredUI_GetMultiOptions( item, &opts );
	if ( opts.count <= 0 ) return -1;
	WiredUI_StateGetString( item->cvar, curBuf, sizeof( curBuf ) );
	return WiredUI_FindMultiOptionIndex( item, &opts, curBuf );
}

/* Select segment `index` (writes its value into the bound cvar). No-op when out
 * of range. The one mutation point for radio selection — click + keyboard both
 * route here so they stay in lockstep. */
void WiredUI_RadioSelectIndex( wiredItemDef_t *item, int index ) {
	wiredMultiOptions_t opts;
	if ( !item ) return;
	WiredUI_GetMultiOptions( item, &opts );
	WiredUI_SetMultiOptionByIndex( item, &opts, index );
}

/* Open the multi-select dropdown popup for `item`, seeding hover/scroll from
 * the item's current cvar value. This is the SINGLE source of the open
 * state-set — both the interactive click handler (ITEM_TYPE_MULTI mouse1) and
 * the #ifdef _DEBUG wui_dropdown_test command call it, so the two paths can
 * never diverge. Returns qtrue if the dropdown was opened (item has options). */
static qboolean WiredUI_OpenMultiDropdown( wiredItemDef_t *item ) {
	wiredMultiOptions_t opts;
	char curBuf[256];

	if ( !item ) return qfalse;
	WiredUI_GetMultiOptions( item, &opts );
	if ( opts.count <= 0 ) return qfalse;

	WiredUI_StateGetString( item->cvar, curBuf, sizeof( curBuf ) );
	wui_multiDropdownOpen = qtrue;
	wui_multiDropdownItem = item;
	wui_multiDropdownHover = WiredUI_FindMultiOptionIndex( item, &opts, curBuf );
	if ( wui_multiDropdownHover < 0 ) wui_multiDropdownHover = 0;
	wui_multiDropdownScroll = wui_multiDropdownHover - 4;
	if ( wui_multiDropdownScroll < 0 ) wui_multiDropdownScroll = 0;
	return qtrue;
}

static qboolean WiredUI_GetMultiDropdownRect( wiredMenuDef_t *menu, wiredItemDef_t *item,
	const wiredMultiOptions_t *opts, float *x, float *y, float *w, float *h, float *rowH, int *visibleRows ) {
	float rx, ry, rw, rh, widest = 0.0f;
	int rows, li;
	if ( !menu || !item || !opts || opts->count <= 0 ) return qfalse;

	/* Per-option row height: use the control's own elementheight (the listbox
	 * model, cl_wired_clay.c:1366), NOT resolvedRect.h. resolvedRect.h is the
	 * whole settings row's height, so multiplying it by the option count made
	 * the popup vastly taller than its options ("dropdown gereksiz uzun").
	 * elementheight * rows hugs the option list. */
	{
		/* Floor the row height to the glyph line box so option labels don't
		 * overlap vertically (draw is top-aligned with zero leading). The popup
		 * is laid out in physical pixels and the option font draws at
		 * fontSize × dpiScale, so scale the authored/derived logical row height
		 * by the same factor — otherwise on HiDPI the 2× text overflows an
		 * un-scaled row and the lines look cramped. */
		float dpi    = WiredUI_GetDpiScale();
		float minRow = WUI_DEFAULT_FONT_SIZE * WUI_LINE_HEIGHT_FACTOR * dpi;
		rh = item->elementheight > 0.0f ? item->elementheight * dpi : minRow;
		if ( rh < minRow ) rh = minRow;
	}
	/* Width from the widest option label, not the whole settings row: the popup
	 * hugs its content so it opens only as wide as its longest option ("neden
	 * yalnizca o field'i kaplamiyor"). Text_Measure resolves the face and skips
	 * colour codes; the option rows draw at FONT_UI / WUI_DEFAULT_FONT_SIZE, so
	 * measure with the same metrics + cell padding. Measured HERE (not in the
	 * caller) so the render path and the two hit-test callers all get the same
	 * rect. Floored to a usable minimum and never wider than the owning row. */
	for ( li = 0; li < opts->count; li++ ) {
		float lw = Text_Measure( opts->labels[ li ], FONT_UI, WUI_DEFAULT_FONT_SIZE );
		if ( lw > widest ) widest = lw;
	}
	/* Text_Measure returns a logical-px width, but the option labels DRAW at
	 * fontSize × dpiScale (like rowH above). The popup lives in physical-pixel
	 * Clay space, so scale the measured width to physical too — otherwise on
	 * HiDPI the box is ~half as wide as the 2× glyphs and long labels
	 * ("Capture the Flag") overflow the popup. */
	widest *= WiredUI_GetDpiScale();
	/* Anchor rect: prefer the ACTUAL Clay-rendered rect over the legacy
	 * WUI_LayoutMenu resolvedRect. Settings rows inside a flexbox panel are
	 * flex-positioned by Clay (emit uses CLAY_ATTACH_TO_NONE + CLAY_SIZING_GROW,
	 * cl_wired_clay.c ~2528/2537), so their true on-screen rect — where the
	 * right-aligned value actually draws — is Clay's layout, NOT resolvedRect.
	 * resolvedRect for these rows is both offset AND narrower than the visible
	 * row (measured: resolvedRect x=1494 w=396 vs Clay x=1700 w=645), which
	 * anchored the popup a quarter-box left of the value. Fall back to
	 * resolvedRect for legacy ATTACH_TO_ROOT rows Clay hasn't laid out. */
	wuiPixelRect_t anchor;
	if ( WiredUI_ClayItemRenderedRect( menu, item, &anchor ) ) {
		/* Clay coords are absolute screen px already; the vertical scroll of the
		 * owning panel is baked into Clay's layout, so don't re-subtract
		 * scrollOffset for the Clay path. */
	} else {
		/* Legacy fallback: resolvedRect y is pre-scroll, so subtract the panel
		 * scroll offset to land the popup under the on-screen row. */
		anchor = item->resolvedRect;
		anchor.y -= menu->scrollOffset;
	}

	rw = widest + 24.0f * WiredUI_GetDpiScale();   /* 12px padding each side (physical) */
	if ( rw < 160.0f * WiredUI_GetDpiScale() ) rw = 160.0f * WiredUI_GetDpiScale();
	/* The popup hugs its widest OPTION, not the owning row — a narrow field
	 * (e.g. server-settings "Type") must still show long labels like
	 * "Capture the Flag" in full. Do NOT clamp to anchor.w; the right-align
	 * below keeps the right edge under the value and the screen-edge clamps
	 * (further down) stop it from overflowing off-screen. */

	rows = opts->count;
	if ( rows > 10 ) rows = 10;
	if ( rows < 1 ) rows = 1;

	/* Right-align the popup under the VALUE field, not the row's left edge. The
	 * value (and the dropdown's current text) hugs the row's right edge, so the
	 * popup must open under it: anchor its right edge to the row's right edge.
	 * Using anchor.x alone dropped the list under the label (row's left),
	 * mid-row. rx = rowRight - popupWidth places it under the value column. */
	rx = anchor.x + anchor.w - rw;
	/* Right edge stays under the value; when the popup is wider than the field
	 * it grows LEFT past the row's left edge (fine inside the panel). Don't
	 * clamp to anchor.x — that would re-narrow it. The screen-edge clamps below
	 * keep it on-screen. */
	ry = anchor.y + anchor.h + 2.0f;

	if ( ry + rh * rows > (float)cls.glconfig.vidHeight - 4.0f ) {
		ry = anchor.y - ( rh * rows ) - 2.0f;
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

void WiredUI_QueryMultiDropdownRender( wuiMultiDropdownRender_t *out ) {
	wiredMenuDef_t      *menu;
	wiredMultiOptions_t  opts;
	char                 cvarBuf[256];
	int                  i;
	int                  copy;

	if ( !out ) return;
	memset( out, 0, sizeof( *out ) );
	out->hoverRow    = -1;
	out->selectedRow = -1;

	if ( !wui_multiDropdownOpen || !wui_multiDropdownItem ) return;

	menu = WiredUI_GetActiveMenu();
	if ( !menu ) return;

	WiredUI_GetMultiOptions( wui_multiDropdownItem, &opts );
	if ( opts.count <= 0 ) return;

	if ( !WiredUI_GetMultiDropdownRect( menu, wui_multiDropdownItem, &opts,
	     &out->x, &out->y, &out->w, &out->h, &out->rowH, &out->visibleRows ) ) {
		return;
	}

	if ( wui_multiDropdownItem->cvar[0] ) {
		WiredUI_StateGetString( wui_multiDropdownItem->cvar, cvarBuf, sizeof( cvarBuf ) );
		out->selectedRow = WiredUI_FindMultiOptionIndex( wui_multiDropdownItem, &opts, cvarBuf );
	}

	copy = opts.count;
	if ( copy > WIRED_MAX_MULTI_CHOICES ) copy = WIRED_MAX_MULTI_CHOICES;
	for ( i = 0; i < copy; i++ ) {
		out->labels[i] = opts.labels[i];
	}

	out->open         = qtrue;
	out->optionCount  = opts.count;
	out->hoverRow     = wui_multiDropdownHover;
	out->scrollOffset = wui_multiDropdownScroll;
}


// ── symbol registration ───────────────────────────────────────────────

void WiredUI_RegisterSymbol( const char *name, wiredSymbolCallback_t callback, void *userData ) {
	if ( !name || !name[0] || !callback ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI_RegisterSymbol: invalid args\n" );
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI_RegisterSymbol: too many symbols (max %d)\n", WIRED_MAX_SYMBOLS );
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI_RegisterElement: invalid name\n" );
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI_RegisterElement: too many elements (max %d)\n", WIRED_MAX_ELEMENTS );
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI_RegisterPopulateCallback: invalid args\n" );
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI_RegisterPopulateCallback: too many callbacks (max %d)\n",
		            WIRED_MAX_POPULATE_CALLBACKS );
		return;
	}

	Q_strncpyz( wui_populateCallbacks[wui_numPopulateCallbacks].name, name,
	            sizeof( wui_populateCallbacks[0].name ) );
	wui_populateCallbacks[wui_numPopulateCallbacks].fn = fn;
	wui_populateCallbacks[wui_numPopulateCallbacks].active = qtrue;
	WiredUI_PopulateHashInsert( wui_numPopulateCallbacks );
	wui_numPopulateCallbacks++;
}

wuiPopulateCallback_t WiredUI_GetPopulateCallback( const char *name ) {
	unsigned int slot, start;
	if ( !name || !name[0] )
		return NULL;

	slot = WiredUI_StateKeyHash( name ) & ( WUI_POPULATE_CB_BUCKETS - 1 );
	start = slot;
	do {
		unsigned char v = wui_populateCallbacksHash[slot];
		if ( !v ) return NULL;  // empty bucket → not registered
		{
			const wiredPopulateEntry_t *e = &wui_populateCallbacks[ v - 1 ];
			if ( e->active && !Q_stricmp( e->name, name ) ) {
				return e->fn;
			}
		}
		slot = ( slot + 1 ) & ( WUI_POPULATE_CB_BUCKETS - 1 );
	} while ( slot != start );
	return NULL;
}

// ── batch registration stubs ──────────────────────────────────────────
// These will be filled in later when ModernHUD elements are wrapped.

void WiredUI_RegisterCoreSymbols( void ) {
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: core symbols registered (stub — Phase 3)\n" );
}

void WiredUI_RegisterCoreElements( void ) {
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: core elements registered (stub — Phase 3)\n" );
}

// ── public API ────────────────────────────────────────────────────────

// ── delayed screenshot ────────────────────────────────────────────────
// +set wired_screenshotDelay N triggers `screenshot jpg` after N seconds.
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

	// Phase 7.15.4-a class-B pin: the cursor + gradient-bar are cached WiredUI
	// handles bound every menu frame without re-resolving residency — the
	// texture-LRU must not evict them. Dark in 7.15.4-a (no reader yet).
	if ( re.PinShaderImages ) {
		if ( wui_cursorShader )      re.PinShaderImages( wui_cursorShader );
		if ( wui_gradientBarShader ) re.PinShaderImages( wui_gradientBarShader );
	}

	WUI_BackgroundInit();
}

// ── hud cvar helper ───────────────────────────────────────────────────
// Loads ui/<hud>.wui when the 'hud' cvar is non-empty.
static void WiredUI_LoadHudFromCvar( void ) {
	if ( !wired_hud || !wired_hud->string[0] ) return;
	char path[MAX_QPATH];
	Com_sprintf( path, sizeof(path), "ui/%s.wui", wired_hud->string );
	WiredUI_LoadMenuFile( path );
}

// ── ui_testall command handler ────────────────────────────────────────
static void WiredUI_TestAll_f( void ) {
	if ( testall_active ) {
		// Toggle off
		testall_active = qfalse;
		WiredUI_CloseAllMenus();
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "ui_testall: stopped\n" );
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
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "ui_testall: cycling %d menus every %d ms (run again to stop)\n",
		WiredUI_GetMenuCount(), testall_delay );
}

/* ad-hoc loose-file menu loader. Bypasses no FS layers — relies on
 * the standard pak+loose search; for any path NOT present in a pak the FS
 * layer resolves to the loose file in homepath or installpath. Useful for
 * fixture .wmenu authoring under test_phase2c2/ etc. without rebuilding paks.
 *
 * Kept in tree as a dev workflow utility; a later pass decides whether to gate
 * behind a developer cvar for release. */
static void WiredUI_LoadMenuLoose_f( void ) {
	const char *path;
	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"Usage: wui_load_menu_loose <path>\n"
			"  e.g. wui_load_menu_loose ui/test_phase2c2/fixture_repeat_lua.wmenu\n" );
		return;
	}
	path = Cmd_Argv( 1 );
	if ( WiredUI_LoadMenuFile( path ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_load_menu_loose: loaded '%s' (menus=%d)\n",
			path, WiredUI_GetMenuCount() );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_load_menu_loose: failed to load '%s'\n", path );
	}
}

/* Dev workflow: exercise the repeat-block runtime path end-to-end.
 * Writes three rows of test data into the WiredStore under the
 * `test.list.*` prefix, loose-loads tests/fixtures/repeat_poc.wui,
 * then pushes the menu. The expansion driver in cl_wired_clay.c
 * iterates `test.list.count` rows and substitutes the template's
 * `{{ row.text }}` placeholder against `WiredStore_Get("test.list.<i>.text")`.
 * Kept in-tree alongside wui_load_menu_loose + ui_testall as a
 * permanent dev utility for verifying repeat-block resolution
 * without rebuilding a real cgame state path. */
static void WiredUI_TestRepeat_f( void ) {
	wuiStoreEntry_t *e;
	int i;
	const char *rows[3] = {
		"row 0 — hello",
		"row 1 — world",
		"row 2 — final"
	};

	e = WiredStore_Set( "test.list.count" );
	if ( e ) {
		Q_strncpyz( e->text, "3", sizeof( e->text ) );
		e->value = 3.0f;
	}
	for ( i = 0; i < 3; i++ ) {
		char key[ 64 ];
		Com_sprintf( key, sizeof( key ), "test.list.%d.text", i );
		e = WiredStore_Set( key );
		if ( e ) {
			Q_strncpyz( e->text, rows[ i ], sizeof( e->text ) );
		}
	}

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_test_repeat: wrote 3 rows to test.list.{0,1,2}.text\n" );

	if ( WiredUI_LoadMenuFile( "tests/fixtures/repeat_poc.wui" ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_test_repeat: loaded fixture (menus=%d); pushing 'repeat_poc'\n",
			WiredUI_GetMenuCount() );
		Cbuf_InsertText( "wui_push repeat_poc\n" );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_test_repeat: failed to load tests/fixtures/repeat_poc.wui — "
			"is the file under fs_installpath or fs_homepath?\n" );
	}
}

/* Dev workflow: exercise per-row recursive child emit. Writes two rows
 * of icon-shader + label test data into the WiredStore under the
 * `test.image_list.*` prefix, loose-loads
 * tests/fixtures/repeat_poc_image_text.wui, then pushes the menu. The
 * repeat expansion clones the template subtree per row, substitutes
 * `{{ row.icon }}` into the inner container's background shader path
 * and `{{ row.label }}` into the inner text leaf, and emits both
 * children via wui_clay_emit_item. Distinct per-row icon + label proves
 * end-to-end Mustache substitution into clone fields. Kept in-tree as a
 * permanent dev utility alongside wui_test_repeat. */
static void WiredUI_TestRepeatImage_f( void ) {
	wuiStoreEntry_t *e;
	int              i;
	const char *icons[2]  = { "levelshots/q3dm0",      "levelshots/q3dm1" };
	const char *labels[2] = { "Award 1 — first icon", "Award 2 — second icon" };

	e = WiredStore_Set( "test.image_list.count" );
	if ( e ) {
		Q_strncpyz( e->text, "2", sizeof( e->text ) );
		e->value = 2.0f;
	}
	for ( i = 0; i < 2; i++ ) {
		char key[ 64 ];
		Com_sprintf( key, sizeof( key ), "test.image_list.%d.icon", i );
		e = WiredStore_Set( key );
		if ( e ) Q_strncpyz( e->text, icons[ i ], sizeof( e->text ) );
		Com_sprintf( key, sizeof( key ), "test.image_list.%d.label", i );
		e = WiredStore_Set( key );
		if ( e ) Q_strncpyz( e->text, labels[ i ], sizeof( e->text ) );
	}

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_test_repeat_image: wrote 2 rows to test.image_list.{0,1}.{icon,label}\n" );

	if ( WiredUI_LoadMenuFile( "tests/fixtures/repeat_poc_image_text.wui" ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_test_repeat_image: loaded fixture (menus=%d); pushing 'repeat_poc_image_text'\n",
			WiredUI_GetMenuCount() );
		Cbuf_InsertText( "wui_push repeat_poc_image_text\n" );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_test_repeat_image: failed to load tests/fixtures/repeat_poc_image_text.wui — "
			"is the file under fs_installpath or fs_homepath?\n" );
	}
}

/* Dev workflow: exercise the STEP curve built-in (`blink`). Loose-loads
 * tests/fixtures/anim_step_blink.wui (a centred text leaf with
 * `animation "blink"`) and pushes the menu. Visual verification: the
 * label snaps fully visible / fully hidden at the 50% mark of each
 * 600ms cycle — no fade, no interpolation. Kept in-tree alongside
 * wui_test_repeat[_image] as a permanent dev utility. */
static void WiredUI_TestAnimStep_f( void ) {
	if ( WiredUI_LoadMenuFile( "tests/fixtures/anim_step_blink.wui" ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_test_anim_step: loaded fixture (menus=%d); pushing 'anim_step_blink'\n",
			WiredUI_GetMenuCount() );
		Cbuf_InsertText( "wui_push anim_step_blink\n" );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_test_anim_step: failed to load tests/fixtures/anim_step_blink.wui — "
			"is the file under fs_installpath or fs_homepath?\n" );
	}
}

/* Dev workflow: exercise the LOOP_LINEAR vertical sweep built-in
 * (`scan-y`). Loose-loads tests/fixtures/anim_scan_y.wui (a thin accent
 * strip with `animation "scan-y"`) and pushes the menu. Visual
 * verification: the strip sweeps from above the viewport to below it
 * over each 3500ms cycle and wraps with no easing. Models the
 * Claude-Design-v2 `qwscan` keyframe. */
static void WiredUI_TestAnimScan_f( void ) {
	if ( WiredUI_LoadMenuFile( "tests/fixtures/anim_scan_y.wui" ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_test_anim_scan: loaded fixture (menus=%d); pushing 'anim_scan_y'\n",
			WiredUI_GetMenuCount() );
		Cbuf_InsertText( "wui_push anim_scan_y\n" );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_test_anim_scan: failed to load tests/fixtures/anim_scan_y.wui — "
			"is the file under fs_installpath or fs_homepath?\n" );
	}
}

/* v2 primitive library demos — each loose-loads its fixture menu and
 * pushes it. Kept in-tree alongside wui_test_repeat[_image] /
 * wui_test_anim_step / wui_test_font_jbmono as permanent dev utilities
 * for visual verification of the qw_* primitive set. */
static void wui_primitive_push( const char *fixture, const char *menuName ) {
	char path[ 128 ];
	char push[ 128 ];
	Com_sprintf( path, sizeof( path ), "tests/fixtures/%s.wui", fixture );
	if ( WiredUI_LoadMenuFile( path ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_test_primitive: loaded '%s' (menus=%d); pushing '%s'\n",
			fixture, WiredUI_GetMenuCount(), menuName );
		Com_sprintf( push, sizeof( push ), "wui_push %s\n", menuName );
		Cbuf_InsertText( push );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_test_primitive: failed to load '%s'\n", path );
	}
}

static void WiredUI_TestPrimitiveQwSigil_f( void )     { wui_primitive_push( "primitive_qw_sigil",     "primitive_qw_sigil"     ); }
static void WiredUI_TestPrimitiveRunes_f( void )       { wui_primitive_push( "primitive_runes",        "primitive_runes"        ); }
static void WiredUI_TestPrimitiveBracket_f( void )     { wui_primitive_push( "primitive_bracket",      "primitive_bracket"      ); }
static void WiredUI_TestPrimitiveArenaThumb_f( void )  { wui_primitive_push( "primitive_arena_thumb",  "primitive_arena_thumb"  ); }
static void WiredUI_TestPrimitiveDisplayText_f( void ) { wui_primitive_push( "primitive_display_text", "primitive_display_text" ); }
static void WiredUI_TestPrimitiveMono_f( void )        { wui_primitive_push( "primitive_mono",         "primitive_mono"         ); }
static void WiredUI_TestPrimitivePlayerBadge_f( void ) { wui_primitive_push( "primitive_player_badge", "primitive_player_badge" ); }
static void WiredUI_TestBgDemo_f( void )                { wui_primitive_push( "bg_demo_backdrop",      "bg_demo_backdrop"      ); }
static void WiredUI_TestBgPlasma_f( void )              { wui_primitive_push( "bg_plasma_primitive",   "bg_plasma_primitive"   ); }
static void WiredUI_TestUtf8_f( void )                  { wui_primitive_push( "utf8_glyph_verify",     "utf8_glyph_verify"     ); }
static void WiredUI_TestColorcode_f( void )             { wui_primitive_push( "text_colorcode_regression", "text_colorcode_regression" ); }

/* Ticker demo needs WiredStore-driven repeat data; pre-populate
 * test.ticker.{0..2}.label + test.ticker.count, then push. */
static void WiredUI_TestPrimitiveTicker_f( void ) {
	wuiStoreEntry_t *e;
	int              i;
	const char      *labels[3] = {
		"PATCH 0.1.0 LIVE",
		"RAILGUN HITBOX REVISED",
		"QUAKECON QUALIFIERS OPEN",
	};
	e = WiredStore_Set( "test.ticker.count" );
	if ( e ) { Q_strncpyz( e->text, "3", sizeof( e->text ) ); e->value = 3.0f; }
	for ( i = 0; i < 3; i++ ) {
		char key[ 64 ];
		Com_sprintf( key, sizeof( key ), "test.ticker.%d.label", i );
		e = WiredStore_Set( key );
		if ( e ) Q_strncpyz( e->text, labels[ i ], sizeof( e->text ) );
	}
	wui_primitive_push( "primitive_ticker", "primitive_ticker" );
}

/* Dev workflow: render a JetBrains Mono visual sample (ASCII + v2
 * telemetry punctuation). $font_mono in the fixture resolves to the
 * jetbrainsmono atlas post-15.5; comparing this against the same
 * fixture pre-15.5 reveals the share_tech_mono → JBMono swap. */
static void WiredUI_TestFontJBMono_f( void ) {
	if ( WiredUI_LoadMenuFile( "tests/fixtures/font_jbmono_sample.wui" ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_test_font_jbmono: loaded fixture (menus=%d); pushing 'font_jbmono_sample'\n",
			WiredUI_GetMenuCount() );
		Cbuf_InsertText( "wui_push font_jbmono_sample\n" );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_test_font_jbmono: failed to load tests/fixtures/font_jbmono_sample.wui — "
			"is the file under fs_installpath or fs_homepath?\n" );
	}
}

// ── Layer 5: hot-reload check ─────────────────────────────────────────
static void WiredUI_CheckHotReload( int realtime ) {
	if ( !wired_hotreload || !wired_hotreload->integer ) return;
	if ( realtime - wui_lastReloadCheck < 1000 ) return; // check once per second
	wui_lastReloadCheck = realtime;

	// Re-load all menus from manifest using the existing safe-reload path
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "Wired UI: hot-reload check\n" );
	WiredUI_ReloadMenus();
}

// ── Layer 5: visual layout debug overlay ──────────────────────────────

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

void WiredUI_GetCursorNorm( float *nx, float *ny ) {
	/* Cursor position normalized to [-1..1] about the viewport centre
	 * (nx: -1 left edge, +1 right edge). Read-only view of the existing
	 * post-clamp wui_cursorX/Y — the background parallax layer's only window
	 * onto the mouse. Does not touch cursor handling. */
	float vw = (float) cls.glconfig.vidWidth;
	float vh = (float) cls.glconfig.vidHeight;
	if ( nx ) *nx = ( vw > 0.0f ) ? ( wui_cursorX / vw ) * 2.0f - 1.0f : 0.0f;
	if ( ny ) *ny = ( vh > 0.0f ) ? ( wui_cursorY / vh ) * 2.0f - 1.0f : 0.0f;
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
		// CL_StartHunkUsers → CL_InitRenderer, then WiredUI peer block → WiredUI_Init sets wui_healthy.
		// WiredUI_Init clears inRecovery as its first action (longjmp guard).
		CL_StartHunkUsers();
	} else if ( !cls.uiStarted ) {
		// Renderer is up but WiredUI was shut down independently.
		cls.uiStarted = qtrue;
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
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: failed to reload — use 'wired_reload' or restart\n" );
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

static int WiredUI_ServerEngineSource( int uiSource ) {
	if ( uiSource == 0 ) return AS_LOCAL;
	if ( uiSource == 6 ) return AS_FAVORITES;
	return AS_GLOBAL;
}

/* Deterministic acceptance seam for browser transaction lifecycle gates.
 * Production frames use the same source mapping and consumer entry point
 * below; this command only removes scheduler timing from com_automated runs. */
static void WiredUI_ServerPingTick_f( void ) {
	int uiSource;
	int engineSource;
	qboolean work;

	if ( !com_automated || !com_automated->integer ) return;
	uiSource = WiredUI_StateGetInt( "ui_netSource" );
	engineSource = WiredUI_ServerEngineSource( uiSource );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server ping tick dispatch source=%d engine_source=%d\n",
		uiSource, engineSource );
	work = CL_UpdateVisiblePings_f( engineSource );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server ping tick source=%d engine_source=%d work=%d\n",
		uiSource, engineSource, work ? 1 : 0 );
}

/* Privacy-safe active-match acceptance readback.  The transport handle is
 * intentionally reduced to presence; connected Server Info retains and
 * compares the exact handle internally. */
static void WiredUI_IngameTrace_f( void ) {
	const wiredMenuDef_t *top;
	const wiredItemDef_t *focused;
	const qboolean active = clientActiveApp && clientActiveApp->state == CA_ACTIVE;
	const qboolean connected = active && !clientActiveApp->clc.demoplaying
		&& clientActiveApp->clc.quic_conn != CONN_INVALID;
	const char *address = connected
		? NET_AdrToStringwPort( &clientActiveApp->clc.serverAddress ) : "none";

	if ( !com_automated || !com_automated->integer ) return;
	top = WiredUI_GetActiveMenu();
	focused = WiredUI_GetFocusedItem();
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: ingame state active=%d demo=%d catcher_ui=%d paused=%d connection_present=%d address=%s server_time=%d top=%s depth=%d focused=%s\n",
		active ? 1 : 0,
		active && clientActiveApp->clc.demoplaying ? 1 : 0,
		( Key_GetCatcher() & KEYCATCH_UI ) ? 1 : 0,
		Cvar_VariableIntegerValue( "cl_paused" ) ? 1 : 0,
		connected ? 1 : 0, address,
		active ? clientActiveApp->cl.serverTime : 0,
		top ? top->name : "none", wui_menuStackDepth,
		focused && focused->name[0] ? focused->name : "none" );
}


/* console-level entry point for pushing a named menu
 * onto the stack. The `open` script command is .wui-internal (only
 * callable from action {} blocks); without this, headless multimodal
 * smokes couldn't navigate to specific menus without temporarily
 * editing main.wui's onOpen. Production: low cost (matches the menu
 * by name + delegates to WiredUI_PushMenu, which already validates).
 * Modders: can bind to a key for quick navigation during testing. */
static void WiredUI_PushMenu_f( void ) {
	wuiBgIntent_t intent = WUI_BG_INTENT_INHERIT;
	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_push <menu_name> [invisible|animated|dim|none]\n" );
		return;
	}
	/* Optional 2nd arg overrides the menu's own `backdrop` declaration for
	 * this push — a testing affordance, not something production uses. It sets
	 * the menu's preset rather than passing one alongside, because the
	 * evaluator reads the declaration and nothing else. */
	if ( Cmd_Argc() >= 3 ) {
		wiredMenuDef_t *m = WiredUI_FindMenu( Cmd_Argv( 1 ) );
		wuiBgPreset_t   p;
		if ( !WUI_BgPresetParse( Cmd_Argv( 2 ), &p ) ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"wui_push: unknown backdrop '%s' (invisible|animated|dim|none)\n",
				Cmd_Argv( 2 ) );
		} else if ( m ) {
			m->bgPreset = p;
		}
	}
	WiredUI_PushMenu( Cmd_Argv( 1 ) );
}

/* enter the keybind capture state programmatically.
 * Mirrors what the type-13 widget's click handler does: walks the top
 * of the menu stack, finds the type-13 itemDef whose cvar matches
 * argv[1], and sets wui_waitingForKey + wui_bindItem so the next key
 * press writes the binding. Used for headless smoke captures of the
 * "Press a key..." visual indicator; modders can bind a key to it for
 * quick rebinding too. */
static wiredItemDef_t *wui_find_bind_item_recursive( wiredItemDef_t *item, const char *targetCvar )
{
	int i;
	if ( !item ) return NULL;
	if ( item->type == ITEM_TYPE_BIND && !Q_stricmp( item->cvar, targetCvar ) ) {
		return item;
	}
	for ( i = 0; i < item->childCount; i++ ) {
		wiredItemDef_t *found = wui_find_bind_item_recursive( item->children[ i ], targetCvar );
		if ( found ) return found;
	}
	return NULL;
}

static void WiredUI_CaptureKey_f( void ) {
	const char     *targetCvar;
	wiredMenuDef_t *menu;
	wiredItemDef_t *match = NULL;
	int             i;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_capture_key <cvar>\n" );
		return;
	}
	if ( wui_menuStackDepth <= 0 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_capture_key: no menu open\n" );
		return;
	}
	menu = WiredUI_FindMenu( wui_menuStack[ wui_menuStackDepth - 1 ] );
	if ( !menu ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_capture_key: top-of-stack menu not found\n" );
		return;
	}
	targetCvar = Cmd_Argv( 1 );
	for ( i = 0; i < menu->itemCount && !match; i++ ) {
		match = wui_find_bind_item_recursive( menu->items[ i ], targetCvar );
	}
	if ( match ) {
		wui_waitingForKey = qtrue;
		wui_bindItem      = match;
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"wui_capture_key: capture state set on item '%s' (cvar '%s')\n",
			match->name[0] ? match->name : "(unnamed)", targetCvar );
		return;
	}
	Com_Log( SEV_WARN, LOG_CH(ch_ui),
		"wui_capture_key: no type-13 itemDef with cvar '%s' in menu '%s'\n",
		targetCvar, menu->name );
}

/* scriptable ListBox interactive state. Two commands:
 *
 *   wui_listbox_select <index>
 *     Walks the top-of-stack menu's item tree, finds the first
 *     ITEM_TYPE_LISTBOX, and sets its listSelectedRow to argv[1].
 *     Mirrors the visual outcome of a mouse click on row N — the row
 *     gets the cyan selection highlight (wui_listbox_sel_color in
 *     cl_wired_clay.c) without needing actual cursor input. Pair with
 *     a screenshot to capture the selected-row visual state from a
 *     headless smoke.
 *
 *   wui_listbox_sort <column>
 *     Routes through WiredFeeder_SortServers (the canonical multi-
 *     column sortable feeder shipped with servers.wui). Other
 *     feeders (maps, characters, demos, etc.) carry their own sort
 *     paths via separate script commands (MapSort, etc.); a future
 *     wui_listbox_sort polymorphic variant could dispatch via the
 *     found ListBox's feeder name. This ships the servers-only
 *     form since that's the only multi-sort feeder in the canonical
 *     menus.
 */
static wiredItemDef_t *wui_find_listbox_recursive( wiredItemDef_t *item )
{
	int i;
	if ( !item ) return NULL;
	if ( item->type == ITEM_TYPE_LISTBOX ) return item;
	for ( i = 0; i < item->childCount; i++ ) {
		wiredItemDef_t *found = wui_find_listbox_recursive( item->children[ i ] );
		if ( found ) return found;
	}
	return NULL;
}

static wiredItemDef_t *wui_find_top_listbox( void )
{
	wiredMenuDef_t *menu;
	int             i;
	if ( wui_menuStackDepth <= 0 ) return NULL;
	menu = WiredUI_FindMenu( wui_menuStack[ wui_menuStackDepth - 1 ] );
	if ( !menu ) return NULL;
	for ( i = 0; i < menu->itemCount; i++ ) {
		wiredItemDef_t *found = wui_find_listbox_recursive( menu->items[ i ] );
		if ( found ) return found;
	}
	return NULL;
}

/* Recursively find the listbox whose vertical scrollbar THUMB is under the
 * cursor (cx, cy in physical px). Walks flex-container children too, so a
 * listbox nested in the settings-panel shell (not in menu->items[]) is reached.
 * Uses the single-source WiredUI_ListboxScrollbarGeom over each candidate's
 * Clay-rendered rect + a small horizontal grab pad. Returns NULL if none.
 * *outGrabDY receives the cursor's offset within the thumb (for steady drag). */
static wiredItemDef_t *wui_listbox_thumb_at( wiredMenuDef_t *menu, wiredItemDef_t *item,
                                             float cx, float cy, float *outGrabDY )
{
	int i;
	if ( !item ) return NULL;

	if ( item->type == ITEM_TYPE_LISTBOX && item->feeder != 0 ) {
		wuiPixelRect_t clayLB, thumb;
		float          trackTop, travel;
		if ( WiredUI_ClayItemRenderedRect( menu, item, &clayLB )
		  && WiredUI_ListboxScrollbarGeom( item, clayLB.x, clayLB.y, clayLB.w, clayLB.h,
		                                   &thumb, &trackTop, &travel ) ) {
			if ( cx >= thumb.x - 6.0f && cx <= thumb.x + thumb.w + 6.0f
			  && cy >= thumb.y        && cy <= thumb.y + thumb.h ) {
				if ( outGrabDY ) *outGrabDY = cy - thumb.y;
				return item;
			}
		}
	}
	for ( i = 0; i < item->childCount; i++ ) {
		wiredItemDef_t *found = wui_listbox_thumb_at( menu, item->children[ i ], cx, cy, outGrabDY );
		if ( found ) return found;
	}
	return NULL;
}

static void WiredUI_ListBoxSelect_f( void ) {
	wiredItemDef_t *lb;
	int             index;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_listbox_select <index>\n" );
		return;
	}
	lb = wui_find_top_listbox();
	if ( !lb ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_listbox_select: no ITEM_TYPE_LISTBOX in top-of-stack menu\n" );
		return;
	}
	index = atoi( Cmd_Argv( 1 ) );
	if ( index < 0 ) index = 0;
	lb->listSelectedRow = index;
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"wui_listbox_select: selected row %d on item '%s'\n",
		index, lb->name[0] ? lb->name : "(unnamed)" );
}

static void WiredUI_ListBoxSort_f( void ) {
	extern void WiredFeeder_SortServers( int column );
	extern void WiredFeeder_SortMaps( int column );
	wiredItemDef_t *lb;
	int             col;
	int             feederId;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_listbox_sort <column>\n" );
		return;
	}
	lb = wui_find_top_listbox();
	if ( !lb ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_listbox_sort: no ITEM_TYPE_LISTBOX in top-of-stack menu\n" );
		return;
	}
	col = atoi( Cmd_Argv( 1 ) );
	feederId = (int)lb->feeder;
	switch ( feederId ) {
	case FEEDER_SERVERS:
		WiredFeeder_SortServers( col );
		break;
	case FEEDER_MAPS:
	case FEEDER_ALLMAPS:
		WiredFeeder_SortMaps( col );
		break;
	default:
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_listbox_sort: feeder %d on item '%s' has no registered sorter\n",
			feederId, lb->name[0] ? lb->name : "(unnamed)" );
		return;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"wui_listbox_sort: sorted feeder %d on item '%s' by column %d\n",
		feederId, lb->name[0] ? lb->name : "(unnamed)", col );
}

/* clear all key bindings that map to a given engine
 * command (the cvar field on a type-13 itemDef is the command, not a
 * key name). Callable as `unbindcmd <cmd>` — it looks up the command and
 * clears every key bound to it. Distinct from the stock `unbind <key>`
 * command which takes a key NAME and only clears that one key's binding. */
static void WiredUI_UnbindCmd_f( void ) {
	const char *cmd;
	int         keynum;
	int         cleared = 0;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: unbindcmd <cmd>\n" );
		return;
	}
	cmd = Cmd_Argv( 1 );
	for ( keynum = 0; keynum < MAX_KEYS; keynum++ ) {
		const char *b = Key_GetBinding( keynum );
		if ( b && !Q_stricmp( b, cmd ) ) {
			Key_SetBinding( keynum, "" );
			cleared++;
		}
	}
	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"unbindcmd: cleared %d binding(s) for '%s'\n", cleared, cmd );
}

/* scripted keyboard / focus injection for headless
 * interactive proof. Subcommands:
 *
 *   wui_menu_nav up | down              synthesize K_UPARROW / K_DOWNARROW
 *   wui_menu_nav enter                  synthesize K_ENTER
 *   wui_menu_nav back                   synthesize K_ESCAPE
 *   wui_menu_nav backspace              synthesize K_BACKSPACE
 *   wui_menu_nav focus <name>           explicit item focus by name (recursive)
 *   wui_menu_nav type <quoted-text>     printable ASCII through K_CHAR_FLAG
 *
 * Synthesis path: calls WiredUI_KeyEvent(key, true) then false — the same
 * entry point CL_KeyEvent invokes when KEYCATCH_UI is held, so the dispatch
 * chain (multiDropdown, edit-field, key-bind capture, focus walk, ESC pop)
 * runs identically to a real keypress. */
static wiredItemDef_t *wui_find_item_recursive( wiredItemDef_t *item, const char *name )
{
	int i;
	if ( !item || !name ) return NULL;
	if ( item->name[0] && !Q_stricmp( item->name, name ) ) return item;
	for ( i = 0; i < item->childCount; i++ ) {
		wiredItemDef_t *found = wui_find_item_recursive( item->children[ i ], name );
		if ( found ) return found;
	}
	return NULL;
}

static int wui_find_item_index_in_menu( wiredMenuDef_t *menu, wiredItemDef_t *target )
{
	int i;
	if ( !menu || !target ) return -1;
	for ( i = 0; i < menu->itemCount; i++ ) {
		if ( menu->items[ i ] == target ) return i;
	}
	return -1;
}

static void WiredUI_MenuNav_f( void ) {
	const char *sub;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_menu_nav <up|down|enter|back|backspace|focus|type> [<arg>]\n" );
		return;
	}
	sub = Cmd_Argv( 1 );

	if ( !Q_stricmp( sub, "up" ) ) {
		WiredUI_KeyEvent( K_UPARROW, qtrue );
		WiredUI_KeyEvent( K_UPARROW, qfalse );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "wui_menu_nav: K_UPARROW dispatched\n" );
	} else if ( !Q_stricmp( sub, "down" ) ) {
		WiredUI_KeyEvent( K_DOWNARROW, qtrue );
		WiredUI_KeyEvent( K_DOWNARROW, qfalse );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "wui_menu_nav: K_DOWNARROW dispatched\n" );
	} else if ( !Q_stricmp( sub, "enter" ) ) {
		WiredUI_KeyEvent( K_ENTER, qtrue );
		WiredUI_KeyEvent( K_ENTER, qfalse );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "wui_menu_nav: K_ENTER dispatched\n" );
	} else if ( !Q_stricmp( sub, "back" ) ) {
		WiredUI_KeyEvent( K_ESCAPE, qtrue );
		WiredUI_KeyEvent( K_ESCAPE, qfalse );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "wui_menu_nav: K_ESCAPE dispatched\n" );
	} else if ( !Q_stricmp( sub, "backspace" ) ) {
		WiredUI_KeyEvent( K_BACKSPACE, qtrue );
		WiredUI_KeyEvent( K_BACKSPACE, qfalse );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "wui_menu_nav: K_BACKSPACE dispatched\n" );
	} else if ( !Q_stricmp( sub, "type" ) ) {
		const char *typed;
		int length;
		if ( Cmd_Argc() != 3 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_ui),
				"usage: wui_menu_nav type <quoted-text>\n" );
			return;
		}
		typed = Cmd_Argv( 2 );
		length = (int)strlen( typed );
		if ( length < 1 || length > 64 ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"wui_menu_nav type: text length must be 1..64\n" );
			return;
		}
		for ( int i = 0; i < length; i++ ) {
			const unsigned char ch = (unsigned char)typed[i];
			if ( ch < 32 || ch > 126 ) {
				Com_Log( SEV_WARN, LOG_CH(ch_ui),
					"wui_menu_nav type: printable ASCII only\n" );
				return;
			}
		}
		for ( int i = 0; i < length; i++ ) {
			const int key = ( (unsigned char)typed[i] ) | K_CHAR_FLAG;
			WiredUI_KeyEvent( key, qtrue );
			WiredUI_KeyEvent( key, qfalse );
		}
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"wui_menu_nav: typed %d printable character(s)\n", length );
	} else if ( !Q_stricmp( sub, "focus" ) ) {
		wiredMenuDef_t *menu;
		wiredItemDef_t *target = NULL;
		int             topIdx;
		int             i;
		const char     *name;
		if ( Cmd_Argc() < 3 ) {
			Com_Log( SEV_INFO, LOG_CH(ch_ui),
				"usage: wui_menu_nav focus <name>\n" );
			return;
		}
		name = Cmd_Argv( 2 );
		menu = WiredUI_GetActiveMenu();
		if ( !menu ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"wui_menu_nav focus: no active menu\n" );
			return;
		}
		for ( i = 0; i < menu->itemCount && !target; i++ ) {
			target = wui_find_item_recursive( menu->items[ i ], name );
		}
		if ( !target ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"wui_menu_nav focus: item '%s' not found in active menu\n", name );
			return;
		}
		/* wui_set_focused handles nested children via the
		 * authoritative pointer. */
		topIdx = wui_find_item_index_in_menu( menu, target );
		wui_set_focused( menu, target );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"wui_menu_nav focus: focused item '%s' (top index %d)\n",
			name, topIdx );
	} else {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_menu_nav: unknown subcommand '%s' (expected up|down|enter|back|focus)\n", sub );
	}
}

/* Headless verification of the selection-unification fix (mouse hover →
 * focus). Positions the cursor over a named item's bounding box and runs the
 * real hover-detection path (WiredUI_MouseEvent), so the engine resolves the
 * hovered item and sets focus exactly as a physical mouse move would — there is
 * no separate "mouse focus" code path to bypass. The layout dump (taken on the
 * next frame) then shows the gold focus-highlight moved to the hovered item and
 * still exactly one item focused (no second highlight). Scripted-proof
 * affordance like wui_menu_nav; inert unless invoked. */
static void WiredUI_HoverTest_f( void ) {
	wiredMenuDef_t *menu;
	wiredItemDef_t *target = NULL;
	const char     *name;
	float           cx, cy;
	int             i;

	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "usage: wui_hover_test <item_name>\n" );
		return;
	}
	name = Cmd_Argv( 1 );
	menu = WiredUI_GetActiveMenu();
	if ( !menu ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui), "wui_hover_test: no active menu\n" );
		return;
	}
	for ( i = 0; i < menu->itemCount && !target; i++ ) {
		target = wui_find_item_recursive( menu->items[ i ], name );
	}
	if ( !target ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui), "wui_hover_test: item '%s' not found\n", name );
		return;
	}

	/* Centre of the item's screen rect. Prefer the CLAY-rendered bounding box
	 * (physical-pixel space, honoring the ×dpiScale invariant) — that is where
	 * the item is actually drawn and hit-tested, so it matches where a real
	 * mouse would hover the visible widget. The legacy resolvedRect can diverge
	 * from the Clay flex position (cumulatively down a flex column, and further
	 * under HiDPI where resolvedRect is not in the same ×dpiScale space), so a
	 * cursor planted there misses the drawn widget. Fall back to resolvedRect
	 * only before the first Clay layout pass (Clay box not yet available). */
	{
		extern qboolean WiredUI_ClayItemRenderedRect( const wiredMenuDef_t *panel,
			const wiredItemDef_t *item, wuiPixelRect_t *out );
		wuiPixelRect_t crect;
		if ( WiredUI_ClayItemRenderedRect( menu, target, &crect ) && crect.w > 0.0f && crect.h > 0.0f ) {
			cx = crect.x + crect.w * 0.5f;
			cy = crect.y + crect.h * 0.5f;
		} else {
			cx = target->resolvedRect.x + target->resolvedRect.w * 0.5f;
			cy = target->resolvedRect.y + target->resolvedRect.h * 0.5f;
		}
	}

	/* Plant the cursor, then run the genuine hover path with a zero delta so
	 * WiredUI_FindItemAtCursor / the Clay hit-test re-resolve at this position
	 * and drive wui_set_focused — the same code a real mouse move executes.
	 * wui_cursorX/wui_cursorY are file-scope statics defined above. */
	wui_cursorX = cx;
	wui_cursorY = cy;
	WiredUI_MouseEvent( 0.0f, 0.0f );

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_hover_test: hovered '%s' at (%.0f,%.0f)\n", name, cx, cy );
}

/* Automated pointer acceptance for a rendered interactive item.  It resolves
 * only geometry, then enters through CL_MouseEvent; the caller must issue the
 * separate real K_MOUSE1 ingress with wui_pointer_click. */
static void WiredUI_PointerItem_f( void ) {
	wiredMenuDef_t *menu;
	wiredItemDef_t *item = NULL;
	wuiPixelRect_t rect;
	const char *name;
	float cx;
	float cy;

	if ( !com_automated || !com_automated->integer ) {
		COM_WARN( LOG_CH(ch_ui), "wui_pointer_item requires com_automated 1\n" );
		return;
	}
	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "usage: wui_pointer_item <item-name>\n" );
		return;
	}
	menu = WiredUI_GetActiveMenu();
	name = Cmd_Argv( 1 );
	if ( !menu ) {
		COM_WARN( LOG_CH(ch_ui), "wui_pointer_item: no active menu\n" );
		return;
	}
	for ( int i = 0; i < menu->itemCount && !item; i++ ) {
		item = wui_find_item_recursive( menu->items[i], name );
	}
	if ( !item || !WiredUI_ItemAcceptsMouseHover( item ) ) {
		COM_WARN( LOG_CH(ch_ui),
			"wui_pointer_item: '%s' is not an interactive item\n", name );
		return;
	}
	if ( !WiredUI_ClayItemRenderedRect( menu, item, &rect )
	     || rect.w <= 0.0f || rect.h <= 0.0f ) {
		COM_WARN( LOG_CH(ch_ui),
			"wui_pointer_item: item '%s' has no rendered rect\n", name );
		return;
	}
	cx = rect.x + rect.w * 0.5f;
	cy = rect.y + rect.h * 0.5f;
	CL_MouseEvent( cx - wui_cursorX, cy - wui_cursorY );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=%s item=%s x=%.0f y=%.0f\n",
		menu->name, item->name, cx, cy );
}

/* Automated pointer acceptance uses the same public engine ingress as SDL:
 * move to the centre of an actually rendered listbox row, allow Clay a frame
 * to publish hover authority, then send a separate paired K_MOUSE1 command.
 * Neither command calls a feeder callback or item action directly. */
static void WiredUI_PointerListbox_f( void ) {
	wiredMenuDef_t *menu;
	wiredItemDef_t *item = NULL;
	wuiPixelRect_t rect;
	const char *name;
	int row;
	int total;
	float dpi;
	float charSize;
	float rowHFloor;
	float rowH;
	float headerH;
	float cx;
	float cy;

	if ( !com_automated || !com_automated->integer ) {
		COM_WARN( LOG_CH(ch_ui), "wui_pointer_listbox requires com_automated 1\n" );
		return;
	}
	if ( Cmd_Argc() != 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_pointer_listbox <item-name> <display-row>\n" );
		return;
	}
	menu = WiredUI_GetActiveMenu();
	name = Cmd_Argv( 1 );
	row = atoi( Cmd_Argv( 2 ) );
	if ( !menu ) {
		COM_WARN( LOG_CH(ch_ui), "wui_pointer_listbox: no active menu\n" );
		return;
	}
	for ( int i = 0; i < menu->itemCount && !item; i++ ) {
		item = wui_find_item_recursive( menu->items[i], name );
	}
	if ( !item || item->type != ITEM_TYPE_LISTBOX || item->feeder == 0
	     || item->horizontalScroll ) {
		COM_WARN( LOG_CH(ch_ui),
			"wui_pointer_listbox: '%s' is not a vertical feeder listbox\n", name );
		return;
	}
	total = WiredUI_FeederCount( (int)item->feeder );
	if ( row < 0 || row >= total ) {
		COM_WARN( LOG_CH(ch_ui),
			"wui_pointer_listbox: row %d outside feeder count %d\n", row, total );
		return;
	}
	if ( !WiredUI_ClayItemRenderedRect( menu, item, &rect )
	     || rect.w <= 0.0f || rect.h <= 0.0f ) {
		COM_WARN( LOG_CH(ch_ui),
			"wui_pointer_listbox: item '%s' has no rendered rect\n", name );
		return;
	}

	dpi = WiredUI_GetDpiScale();
	charSize = item->fontPointSize > 0.0f
		? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
	rowHFloor = charSize * WUI_LINE_HEIGHT_FACTOR * dpi;
	rowH = item->elementheight > 0.0f ? item->elementheight * dpi : rowHFloor;
	if ( rowH < rowHFloor ) rowH = rowHFloor;
	headerH = WiredUI_ListboxHeaderHeight( item );
	if ( row < item->listScrollOffset
	     || headerH + ( row - item->listScrollOffset + 1 ) * rowH > rect.h ) {
		COM_WARN( LOG_CH(ch_ui),
			"wui_pointer_listbox: row %d is outside rendered viewport offset=%d\n",
			row, item->listScrollOffset );
		return;
	}

	cx = rect.x + rect.w * 0.5f;
	cy = rect.y + headerH + ( row - item->listScrollOffset + 0.5f ) * rowH;
	CL_MouseEvent( cx - wui_cursorX, cy - wui_cursorY );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: pointer phase=moved ingress=CL_MouseEvent menu=%s item=%s feeder=%d row=%d list_generation=%d x=%.0f y=%.0f\n",
		menu->name, item->name, (int)item->feeder, row,
		WiredUI_ListboxIdentityGeneration( item ), cx, cy );
}

static void WiredUI_PointerClick_f( void ) {
	if ( !com_automated || !com_automated->integer ) {
		COM_WARN( LOG_CH(ch_ui), "wui_pointer_click requires com_automated 1\n" );
		return;
	}
	if ( Cmd_Argc() != 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "usage: wui_pointer_click\n" );
		return;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: pointer phase=click ingress=CL_KeyEvent key=K_MOUSE1 x=%.0f y=%.0f\n",
		wui_cursorX, wui_cursorY );
	CL_KeyEvent( K_MOUSE1, qtrue, (unsigned)cls.realtime );
	CL_KeyEvent( K_MOUSE1, qfalse, (unsigned)cls.realtime );
}

static void WiredUI_PointerButton_f( void ) {
	qboolean down;
	const char *edge;

	if ( !com_automated || !com_automated->integer ) {
		COM_WARN( LOG_CH(ch_ui), "wui_pointer_button requires com_automated 1\n" );
		return;
	}
	if ( Cmd_Argc() != 2
	     || ( Q_stricmp( Cmd_Argv( 1 ), "down" )
	       && Q_stricmp( Cmd_Argv( 1 ), "up" ) ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_pointer_button <down|up>\n" );
		return;
	}
	down = !Q_stricmp( Cmd_Argv( 1 ), "down" ) ? qtrue : qfalse;
	edge = down ? "down" : "up";
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: pointer phase=button ingress=CL_KeyEvent key=K_MOUSE1 edge=%s x=%.0f y=%.0f\n",
		edge, wui_cursorX, wui_cursorY );
	CL_KeyEvent( K_MOUSE1, down, (unsigned)cls.realtime );
}

/* Headless verification of the error-dialog contract (fix #3). Mirrors the
 * real caller (cl_main.c): set com_errorMessage via Com_SetLastError, then
 * CL_WiredUI_ShowError. Logs a single machine-readable line the layout-check
 * consumes:
 *   wui_showerror_test: automated=<0|1> stackTop='<menu|none>' errMsg='<text>'
 * so the check asserts:
 *   * com_automated 1 → stackTop != error_popup (suppressed)
 *   * com_automated 0 → stackTop == error_popup AND errMsg == the message
 * Like wui_menu_nav, a scripted-proof affordance (no _DEBUG gate so release
 * smokes can run it; inert unless invoked). */
static void WiredUI_ShowErrorTest_f( void ) {
	const char *msg = ( Cmd_Argc() >= 2 ) ? Cmd_Argv( 1 ) : "test error message";
	const char *top;
	char        errBuf[256];

	/* The real caller sets com_errorMessage before showing — replicate it so
	 * the dialog's text binding (error_popup.wui -> com_errorMessage) is
	 * populated; this is exactly the cl_main.c fix path. */
	Com_SetLastError( "%s", msg );
	CL_WiredUI_ShowError( "Test Error", msg, qtrue );

	top = ( wui_menuStackDepth > 0 ) ? wui_menuStack[ wui_menuStackDepth - 1 ] : "none";
	Cvar_VariableStringBuffer( "com_errorMessage", errBuf, sizeof( errBuf ) );
	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_showerror_test: automated=%d stackTop='%s' errMsg='%s'\n",
		( com_automated && com_automated->integer ) ? 1 : 0, top, errBuf );
}

static void WiredUI_ServerStatusTrace_f( void ) {
	int feeder;
	int count;
	if ( !com_automated || !com_automated->integer ) {
		COM_WARN( LOG_CH(ch_ui), "wui_serverstatus_trace requires com_automated 1\n" );
		return;
	}
	WiredFeeder_ServerStatusTrace();
	feeder = WiredUI_FeederIDByName( "serverstatus" );
	count = feeder ? WiredUI_FeederCount( feeder ) : 0;
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: server status registry feeder=%d count=%d\n", feeder, count );
	for ( int i = 0; i < count; i++ ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: server status registry row=%d key=%s value=%s\n", i,
			WiredUI_FeederItemText( feeder, i, 0 ),
			WiredUI_FeederItemText( feeder, i, 1 ) );
	}
}

static void WiredUI_ServerFixture_f( void ) {
	int sentinelPort;
	int targetPort;
	qboolean targetNeedsPassword = qfalse;
	if ( !com_automated || !com_automated->integer ) {
		COM_WARN( LOG_CH(ch_ui), "wui_server_fixture requires com_automated 1\n" );
		return;
	}
	if ( Cmd_Argc() == 2 && !Q_stricmp( Cmd_Argv( 1 ), "clear" ) ) {
		WiredFeeder_ServerFixtureClear();
		return;
	}
	if ( Cmd_Argc() != 3 && Cmd_Argc() != 4 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_server_fixture <sentinel-port> <target-port> [target-needpass=0|1]|clear\n" );
		return;
	}
	sentinelPort = atoi( Cmd_Argv( 1 ) );
	targetPort = atoi( Cmd_Argv( 2 ) );
	if ( Cmd_Argc() == 4 ) {
		if ( Q_stricmp( Cmd_Argv( 3 ), "0" ) && Q_stricmp( Cmd_Argv( 3 ), "1" ) ) {
			COM_WARN( LOG_CH(ch_ui), "wui_server_fixture rejected invalid target-needpass\n" );
			return;
		}
		targetNeedsPassword = atoi( Cmd_Argv( 3 ) ) ? qtrue : qfalse;
	}
	if ( !WiredFeeder_ServerFixtureInstall( sentinelPort, targetPort,
		targetNeedsPassword ) ) {
		COM_WARN( LOG_CH(ch_ui), "wui_server_fixture rejected invalid ports\n" );
	}
}

static void WiredUI_BotTrace_f( void ) {
	if ( !com_automated || !com_automated->integer ) {
		COM_WARN( LOG_CH(ch_ui), "wui_bot_trace requires com_automated 1\n" );
		return;
	}
	WiredFeeder_BotTrace();
}

static void WiredUI_DemoTrace_f( void ) {
	if ( !com_automated || !com_automated->integer ) {
		COM_WARN( LOG_CH(ch_ui), "wui_demo_trace requires com_automated 1\n" );
		return;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: demo playback trace state=%d demoplaying=%d name=%s sequence=%d serverTime=%d\n",
		clientActiveApp ? clientActiveApp->state : CA_UNINITIALIZED,
		clientActiveApp ? clientActiveApp->clc.demoplaying : 0,
		( clientActiveApp && clientActiveApp->clc.demoName[0] )
			? clientActiveApp->clc.demoName : "none",
		clientActiveApp ? clientActiveApp->clc.serverMessageSequence : 0,
		clientActiveApp ? clientActiveApp->cl.serverTime : 0 );
}

static void WiredUI_PasswordTrace_f( void ) {
	const wiredItemDef_t *item;
	char rendered[MAX_CVAR_VALUE_STRING];
	qboolean masked = qtrue;
	int length;

	if ( !com_automated || !com_automated->integer ) {
		COM_WARN( LOG_CH(ch_ui), "wui_password_trace requires com_automated 1\n" );
		return;
	}
	if ( Cmd_Argc() == 2 && !Q_stricmp( Cmd_Argv( 1 ), "state" ) ) {
		const char *top = wui_menuStackDepth > 0
			? wui_menuStack[wui_menuStackDepth - 1] : "none";
		char target[256];
		char error[256];
		WiredUI_StateGetString( "ui_password_server_name", target, sizeof( target ) );
		WiredUI_StateGetString( "ui_joinPasswordError", error, sizeof( error ) );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: password state valid=%d secret_length=%d editing=%d selection_generation=%d address=%s target_empty=%d error_present=%d top=%s depth=%d\n",
			wui_passwordPrompt.valid ? 1 : 0,
			(int)strlen( wui_passwordPrompt.secret ),
			wui_editingField ? 1 : 0,
			wui_passwordPrompt.selectionGeneration,
			wui_passwordPrompt.address[0] ? wui_passwordPrompt.address : "none",
			target[0] ? 0 : 1, error[0] ? 1 : 0,
			top, wui_menuStackDepth );
		Q_SecureZeroMemory( target, sizeof( target ) );
		Q_SecureZeroMemory( error, sizeof( error ) );
		return;
	}
	if ( Cmd_Argc() != 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_password_trace [state]\n" );
		return;
	}
	item = WiredUI_GetFocusedItem();
	if ( !item || Q_stricmp( item->name, "row_password" ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: password render trace refused reason=wrong-focus\n" );
		return;
	}
	WiredUI_BoundValueText( item, rendered, sizeof( rendered ) );
	length = (int)strlen( rendered );
	if ( length < 1 ) masked = qfalse;
	for ( int i = 0; i < length; i++ ) {
		if ( rendered[i] != '*' ) {
			masked = qfalse;
			break;
		}
	}
	/* Never log the rendered buffer: if masking regresses it contains the
	 * credential. The boolean still distinguishes that mutation safely. */
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: password render trace length=%d masked=%d\n",
		length, masked ? 1 : 0 );
}

/* Scripted keypress injection for automated acceptance. Routes through
 * the public CL_KeyEvent entry point so the dispatch chain — first-input
 * attract→main promotion (cl_keys.c), KEYCATCH_UI forwarding,
 * WiredUI_KeyEvent menu pop, NoteInput taps — runs identically to a real
 * keypress. `wui_menu_nav` (above) only synthesises into WiredUI_KeyEvent
 * directly and bypasses CL_KeyEvent's outer gates, so it can't reach the
 * dispatch-2 first-input branch; this helper closes that gap. Accepts
 * decimal keycodes from keycodes.h (e.g. 27 = K_ESCAPE, 13 = K_ENTER,
 * 'q' = 113). Release-safe but inert unless com_automated is set. */
static void WiredUI_TestKeyDown_f( void ) {
	int key;
	if ( !com_automated || !com_automated->integer ) return;
	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_test_keydown <keycode-decimal>\n"
			"  27=ESC  13=ENTER  9=TAB  178=UPARROW  177=DOWNARROW\n" );
		return;
	}
	key = atoi( Cmd_Argv( 1 ) );
	CL_KeyEvent( key, qtrue,  cls.realtime );
	CL_KeyEvent( key, qfalse, cls.realtime );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"wui_test_keydown: dispatched keycode %d\n", key );
}

#ifdef _DEBUG
/* W-17 dispatch 5b S4: arms the next compositor emit pass to write the Clay
 * render command tree as JSON to <filename> under FS_FOpenFileWrite (lands
 * in the homepath fs_homepath). Matches the schema vcompare consumes for
 * structural diff against DOM extraction. Single-shot — fires once per
 * arm. */
extern void WiredUI_ClayDumpNext( const char *filename );
static void WiredUI_TestDumpClay_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"usage: wui_test_dump_clay <filename>\n" );
		return;
	}
	WiredUI_ClayDumpNext( Cmd_Argv( 1 ) );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"wui_test_dump_clay: armed next-frame Clay dump → %s\n", Cmd_Argv( 1 ) );
}

#endif

/* Single CL_Init-level entry point for all WiredUI Lua binding registration.
   Must be called BEFORE WiredScript_PostInit so that load_menu() and
   attract.* globals are live when WiredUI_Init and WiredAttract_Init exec
   their Lua files during CL_StartHunkUsers. */
void WiredUI_LuaInit( void ) {
	WiredUI_MenuLuaInit();    /* registers load_menu() global  */
	WiredAttract_LuaInit();   /* registers attract.* global    */
}

#ifdef _DEBUG
static void WiredUI_DropdownTest_f( void );   /* defined after WiredUI_FindItemByName */
static void WiredUI_ScoresTest_f( void );      /* defined alongside DropdownTest_f      */
#endif

/*
=================
WiredUI_LoadExplicitMenus

System menus loaded OUTSIDE menus.lua (the manifest intentionally omits them
— the parser doesn't dedupe by name, so a load_menu entry there would
double-register on the Init path, which runs menus.lua THEN these explicit
loads). They must run after EVERY WiredUI_LoadMenusFromLua so they survive
WiredUI_SafeReload (hud_reload / wired_reload / per-map ReloadHud) — not only
the one-time WiredUI_Init. Otherwise SafeReload (ClearMenus + LoadMenusFromLua)
drops loading_screen + overlay from the registry, and the LOADING layer's
by-path lookup (WiredUI_FindMenuByPath, dispatch 13-30) returns NULL → the
fail-loud fires → black loading screen.

The load-path strings here MUST be byte-identical to the paths the connstate
setters bind via WiredUI_SetLoadingMenu ("ui/loading_screen.wui"), because
WiredUI_ParseMenu stamps menu->sourcePath verbatim from this filename and
WiredUI_FindMenuByPath matches it Q_stricmp-exact.
=================
*/
void WiredUI_LoadExplicitMenus( void ) {
	// load the system loading screen menu. menus.lua may
	// or may not include it depending on theme; load explicitly here so
	// the compositor's LOADING layer finds it by path-identity. Idempotent
	// if menus.lua already loaded it (parser appends, latest wins).
	WiredUI_LoadMenuFile( "ui/loading_screen.wui" );

	/* load the cursor + tooltip overlay menu explicitly so it
	 * exists in the registry on the WUI_LAYER_OVERLAY layer regardless of
	 * what menus.lua opts in for. menus.lua intentionally omits this file
	 * — the parser does not dedupe by name, so a duplicate load_menu
	 * entry there would register two copies. */
	WiredUI_LoadMenuFile( "ui/overlay.wui" );

	/* (debug-overlay-migration): the debug-overlay panels live
	 * outside menus.lua for the same reason as overlay.wui — they must exist
	 * in the registry on the WUI_LAYER_DEBUG_OVERLAY layer regardless of theme
	 * and survive WiredUI_SafeReload. The layer is gated on wired_ui_debug, so
	 * loading the panels unconditionally is harmless when the gate is off. */
	WiredUI_LoadMenuFile( "ui/debug_graph.wui" );
	WiredUI_LoadMenuFile( "ui/debug_netstats.wui" );
}

qboolean WiredUI_Init( qboolean inGameUI ) {
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

	Com_Log( SEV_INFO, LOG_CH(ch_ui), "------- WiredUI_Init -------\n" );

	/* wui_required functional gate cvar. Registered
	 * at the top of WiredUI_Init so the boot path in cl_main.c can read
	 * it on the failure branch. CVAR_LATCH — boot-time decision, no
	 * mid-frame flips. Default "1" (UI mode required); dedicated /
	 * headless boots pass `+set wui_required 0` on the command line.
	 * Functional gate (NOT diagnostic) per Memory K16. */
	{
		static const cvarDesc_t d = CVAR_BOOL(
			"wui_required", "1",
			CVAR_LATCH,
			"WiredUI required for engine boot. 1: fail boot if WiredUI init "
			"fails. 0: graceful headless fallback (SEV_WARN, engine continues "
			"without UI mode)." );
		Cvar_Register( &d );
	}

	memset( wui_symbols, 0, sizeof( wui_symbols ) );
	memset( wui_elements, 0, sizeof( wui_elements ) );
	memset( wui_populateCallbacks, 0, sizeof( wui_populateCallbacks ) );
	memset( wui_populateCallbacksHash, 0, sizeof( wui_populateCallbacksHash ) );
	WiredUI_ResetListboxDoubleClick( "init" );
	wui_compositorPointerDown = qfalse;
	Q_SecureZeroMemory( &wui_passwordPrompt, sizeof( wui_passwordPrompt ) );
	Cvar_Get( "ui_password_server_name", "", CVAR_TEMP );
	Cvar_Set( "ui_password_server_name", "" );
	WiredUI_StateSetString( "ui_joinPasswordError", "" );
	wui_numSymbols = 0;
	wui_numElements = 0;
	wui_numPopulateCallbacks = 0;

	// Register dynamic-MULTI populate callbacks (audio_devices, etc.).
	// Lives in cl_wired_populate.c so additions don't churn this file.
	WiredUI_RegisterCorePopulateCallbacks();

	WiredUI_ResetAssetGlobalsDefaults();

	// register 'hud' cvar before menu load so WiredUI_LoadHudFromCvar is safe to call
	// "classic" (ui/classic.wui) is the V2 HUD and what the game opens with.
	// ui/default.wui is the older competitive layout, still selectable with
	// `hud default` for anyone who prefers it.
	wired_hud = Cvar_Get( "hud", "classic", CVAR_ARCHIVE );
	Q_strncpyz( wui_hud_lastLoaded, wired_hud->string, sizeof( wui_hud_lastLoaded ) );

	// register feeder data sources first — the menu parser resolves
	// `feeder "name"` against this registry while loading .wmenu files.
	WiredUI_RegisterCoreFeeders();

	// initialise the unified custom-draw registry +
	// bootstrap-register every entry in the legacy ownerdraw + hud
	// element + hud family tables under their sigil prefixes. Runs
	// BEFORE menus load so the parser's sigil rewrite has a populated
	// registry to look up against when items declare ownerdraw/hudElement.
	WiredUI_CustomDraw_Init();
	WiredOwnerDraw_RegisterAll();
	WiredHud_RegisterElements();

	// register the 3 loading-screen custom-draws
	// (loading_wireframe, loading_streaming_rows, loading_mapinfo_stats)
	// before menus parse loading_screen.wmenu, so the parser's `custom`
	// sigil-rewrite path has registry entries to match against. Retires
	// alongside cl_loading_ui.c's whole-file deletion.
	WiredLoadingCustomDraws_RegisterAll();

	/* cursor + tooltip overlay custom-draws — registered before
	 * menus parse overlay.wmenu so the parser's `custom` sigil-rewrite
	 * resolves wui_cursor / wui_tooltip against the live registry. */
	WiredUI_RegisterOverlayCustomDraws();

	/* register the 6 debug-overlay custom-draws before menus
	 * parse debug_graph.wui / debug_netstats.wui so the parser's `custom`
	 * sigil-rewrite resolves debug_{demo_recording,voip_meter,graph,ping,
	 * snaps,packets} against the live registry. */
	WiredDebugOverlay_RegisterAll();

	// load menu files from scripts/menus.lua
	WiredUI_ClearMenus();
	WiredUI_LoadMenusFromLua();
	WiredUI_LoadHudFromCvar();

	// Load the menus that live outside menus.lua (loading_screen + overlay)
	// via the shared helper, so WiredUI_SafeReload re-runs the exact same set
	// and they survive reloads (otherwise SafeReload drops them — see
	// WiredUI_LoadExplicitMenus + the LOADING by-path lookup in 13-30).
	WiredUI_LoadExplicitMenus();

	if ( !WiredUI_CallLuaStoreFunction( "loadstate" ) ) {
		WiredUI_LoadState();
	}
	/* Browser selection is process-local authority. Older state files may
	 * still contain these keys, so clear them after every load as well as
	 * excluding them from future saves. */
	WiredUI_StateSetString( "ui_selectedServerAddr", "" );
	WiredUI_StateSetString( "ui_selectedServerName", "" );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: reset transient server selection after state load\n" );

	// Bootstrap MSDF font subsystem before HUD init
	Text_Init();

	/* compositor lifecycle was lifted
	 * to CL_Init / CL_Shutdown so the WiredUI_Arena + Clay context survive
	 * map loads. Here we only refresh the Clay font indirection table now
	 * that MSDF's font registry has been (re)populated by Text_Init. */
	WiredUI_ClayRefreshFontTable();

	// initialize HUD subsystem (state bridge)
	WiredHud_Init();

	// semantic state theme system
	WiredTheme_Init();

	/* v2 palette overlay driver (dark/light × 5 accent). Registers
	 * ui_palette_mode + ui_palette_accent cvars; applies the initial
	 * overlay chain. Distinct module from WiredTheme above (which maps
	 * semantic state labels critical/warning/normal to colours). */
	WiredPalette_Init();

	// attract scheduler
	WiredAttract_Init();

	// hot reload commands
	Cmd_AddCommand( "hud_reload", WiredUI_ReloadHud );
	Cmd_AddCommand( "menu_reload", WiredUI_ReloadMenus );

	// dev: cycle through all menus for visual verification
	Cmd_AddCommand( "ui_testall", WiredUI_TestAll_f );

	/* load a .wmenu file from loose-files (homepath or modfiles),
	 * bypassing the pak-first FS layer. Used for fixture development +
	 * ad-hoc menu testing without rebuilding the pak. Kept in the tree as
	 * a dev workflow utility (it may later be gated behind a developer
	 * cvar for release builds). */
	Cmd_AddCommand( "wui_load_menu_loose", WiredUI_LoadMenuLoose_f );

	/* PoC harness for the repeat runtime path; pairs with
	 * tests/fixtures/repeat_poc.wui. */
	Cmd_AddCommand( "wui_test_repeat", WiredUI_TestRepeat_f );
	Cmd_AddCommand( "wui_test_repeat_image", WiredUI_TestRepeatImage_f );
	Cmd_AddCommand( "wui_test_anim_step", WiredUI_TestAnimStep_f );
	Cmd_AddCommand( "wui_test_anim_scan", WiredUI_TestAnimScan_f );
	Cmd_AddCommand( "wui_test_font_jbmono", WiredUI_TestFontJBMono_f );
	Cmd_AddCommand( "wui_test_primitive_qw_sigil",      WiredUI_TestPrimitiveQwSigil_f );
	Cmd_AddCommand( "wui_test_primitive_runes",         WiredUI_TestPrimitiveRunes_f );
	Cmd_AddCommand( "wui_test_primitive_bracket",       WiredUI_TestPrimitiveBracket_f );
	Cmd_AddCommand( "wui_test_primitive_ticker",        WiredUI_TestPrimitiveTicker_f );
	Cmd_AddCommand( "wui_test_primitive_arena_thumb",   WiredUI_TestPrimitiveArenaThumb_f );
	Cmd_AddCommand( "wui_test_primitive_display_text",  WiredUI_TestPrimitiveDisplayText_f );
	Cmd_AddCommand( "wui_test_primitive_mono",          WiredUI_TestPrimitiveMono_f );
	Cmd_AddCommand( "wui_test_primitive_player_badge",  WiredUI_TestPrimitivePlayerBadge_f );
	Cmd_AddCommand( "wui_test_bg_demo",                 WiredUI_TestBgDemo_f );
	Cmd_AddCommand( "wui_test_bg_plasma",               WiredUI_TestBgPlasma_f );
	Cmd_AddCommand( "wui_test_utf8",                    WiredUI_TestUtf8_f );
	Cmd_AddCommand( "wui_test_colorcode",               WiredUI_TestColorcode_f );

	// recovery command — brings WiredUI back from the fullscreen fallback console
	Cmd_AddCommand( "wired_recover", WiredUI_Recover_f );

	// push a menu by name from the console. The `open`
	// script command is only reachable from inside .wui action blocks;
	// `wui_push` exposes the same primitive for headless smokes + ad-hoc
	// menu navigation without editing main.wui.
	Cmd_AddCommand( "wui_push", WiredUI_PushMenu_f );
	Cmd_AddCommand( "wui_server_ping_tick", WiredUI_ServerPingTick_f );
	Cmd_AddCommand( "wui_ingame_trace", WiredUI_IngameTrace_f );

	// enter the keybind capture state programmatically.
	// Same effect as clicking a type-13 widget; used by headless smokes
	// to capture the "Press a key..." visual state.
	Cmd_AddCommand( "wui_capture_key", WiredUI_CaptureKey_f );

#ifdef _DEBUG
	// Dev/test: open a multi-select dropdown popup non-interactively so a
	// headless smoke can pixel-verify the floating panel render (the
	// interactive path needs a mouse click). Drives the SAME singleton open
	// state as the click handler via WiredUI_OpenMultiDropdown — adds no new
	// rendering logic, only a state-set hook. Removed in release builds.
	Cmd_AddCommand( "wui_dropdown_test", WiredUI_DropdownTest_f );

	// Dev/test: drive the cgame scoreboard-hold (+scores) non-interactively so
	// a headless smoke can pixel-verify the V2 scoreboard chrome. The +scores
	// button command can't be issued as a startup +arg (the leading + is the
	// arg-introducer, so the engine runs bare "scores" which isn't a cgame
	// command). This shim routes "+scores"/"-scores" through the SAME
	// console→cgame path a keybind uses (Cbuf → Cmd_ExecuteString →
	// CL_GameCommand → CG_CONSOLE_COMMAND → CG_ScoresDown_f), which sets
	// cg.showScores in the cgame VM. The bridge restages it into
	// wiredHud->showScores every frame, flipping the gametype scoreboard menu
	// visible exactly as a real TAB-hold would. No VM-memory poke, no new
	// render logic. Removed in release builds.
	Cmd_AddCommand( "wui_scores_test", WiredUI_ScoresTest_f );
#endif

	// clear all key bindings that map to a given engine
	// command. Clears every key bound to a given command; the keybind rows
	// invoke it (via the Del/Backspace unbind path and per-row Reset buttons).
	Cmd_AddCommand( "unbindcmd", WiredUI_UnbindCmd_f );

	// scriptable ListBox interactive state. Sets the
	// listSelectedRow / sort column on the top-of-stack menu's first
	// ListBox itemDef so headless smokes can capture the cyan
	// selection highlight + sorted row order without simulating mouse
	// input.
	Cmd_AddCommand( "wui_listbox_select", WiredUI_ListBoxSelect_f );
	Cmd_AddCommand( "wui_listbox_sort",   WiredUI_ListBoxSort_f );

	// scripted keyboard / focus injection for headless
	// interactive proof. up/down/enter/back synthesize the corresponding
	// key event via the same WiredUI_KeyEvent entry CL_KeyEvent uses;
	// focus <name> walks the active menu's item tree recursively.
	Cmd_AddCommand( "wui_menu_nav",       WiredUI_MenuNav_f );
	Cmd_AddCommand( "wui_hover_test",     WiredUI_HoverTest_f );
	Cmd_AddCommand( "wui_pointer_item",   WiredUI_PointerItem_f );
	Cmd_AddCommand( "wui_pointer_listbox", WiredUI_PointerListbox_f );
	Cmd_AddCommand( "wui_pointer_click",  WiredUI_PointerClick_f );
	Cmd_AddCommand( "wui_pointer_button", WiredUI_PointerButton_f );
	Cmd_AddCommand( "wui_showerror_test", WiredUI_ShowErrorTest_f );
	Cmd_AddCommand( "wui_serverstatus_trace", WiredUI_ServerStatusTrace_f );
	Cmd_AddCommand( "wui_server_fixture", WiredUI_ServerFixture_f );
	Cmd_AddCommand( "wui_bot_trace",      WiredUI_BotTrace_f );
	Cmd_AddCommand( "wui_demo_trace",     WiredUI_DemoTrace_f );
	Cmd_AddCommand( "wui_test_keydown",   WiredUI_TestKeyDown_f );
	Cmd_AddCommand( "wui_password_trace", WiredUI_PasswordTrace_f );
#ifdef _DEBUG
	Cmd_AddCommand( "wui_test_dump_clay", WiredUI_TestDumpClay_f );
#endif

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
					/* Intent isn't persisted across save/restore — restored
					 * entries use the authored background (INHERIT), and this
					 * clears any stale value left in the slot from a prior push. */
					wui_menuStackDepth++;
				}
				if ( !*p ) break;
			}

			// clear saved state
			Cvar_Set( "wui_menuStackSaved", "" );
			Cvar_Set( "wui_activeMenuSaved", "0" );

			if ( wui_menuStackDepth > 0 ) {
				Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: restored menu stack (depth %d)\n", wui_menuStackDepth );
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

	re.VertexLighting( qfalse ); // UI elements don't use vertex-light collapse

	/* component-library F1: clear interaction state (press channel + keyboard-
	 * focus provenance). The per-type value-arm registry (Table B) is populated
	 * by F2 as controls are added; F1 leaves it empty (nav-only) by design. */
	WiredUI_WidgetCoreReset();

	wui_healthy = qtrue;
	wui_recoveryFailTime = 0; // clear any stale failure banner
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: initialized (%d menus loaded)\n", WiredUI_GetMenuCount() );
#ifdef _DEBUG
	{
		const int head  = WiredUI_GetPoolHead();
		const int cap   = WiredUI_GetPoolCapacity();
		const int freeB = cap - head;
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI/pool: post-init head=%d B / cap=%d B (%d B free, %d%% used) "
			"sizeof(itemDef)=%zu sizeof(menuDef)=%zu sizeof(multiDef)=%zu\n",
			head, cap, freeB, ( head * 100 ) / cap,
			sizeof( wiredItemDef_t ), sizeof( wiredMenuDef_t ),
			sizeof( wiredMultiDef_t ) );
	}
#endif

	/* success return path. The caller in cl_main.c
	 * branches on this return + the `wui_required` cvar. Internal subsystem
	 * failures (Clay arena alloc, ClayInit) log SEV_ERROR but reach the
	 * end of WiredUI_Init with wui_clay_initialized = qfalse; we report
	 * that as the failure signal here so the caller's wui_required gate
	 * sees the partial-init state. */
	if ( !WiredUI_IsClayInitialized() ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_ui),
			"WiredUI_Init: Clay compositor failed to initialize\n" );
		return qfalse;
	}

#ifdef _DEBUG
	/* one-shot multi-viewport dispatch acceptance test —
	 * proves the viewport registry + render-dispatch primitive (the same one
	 * the WORLD_VIEWPORT per-panel walk uses) handles >1 provider. Logs
	 * PASS/FAIL. Verify-only, removed after acceptance (K16). */
	WiredUI_ViewportMultiSelfTest();
#endif

#ifdef _DEBUG
	/* debug-build-only fault injection for the
	 * wui_required failure path. The compile-time #ifdef _DEBUG
	 * guard keeps this stripped from release builds; runtime gating uses
	 * the WIRED_UI_FORCE_INIT_FAIL environment variable (NOT a cvar) so
	 * it doesn't add a diagnostic cvar (Memory K16). To run the
	 * simulated-init-fail acceptance:
	 *   WIRED_UI_FORCE_INIT_FAIL=1 wired.x64 +set wui_required 0 +map arena1
	 * Expected: SEV_WARN logged + engine continues headless (no Sys_Error
	 * because wui_required=0). With wui_required=1 the engine fatals. */
	{
		const char *forceFail = getenv( "WIRED_UI_FORCE_INIT_FAIL" );
		if ( forceFail && forceFail[ 0 ] && forceFail[ 0 ] != '0' ) {
			Com_Log( SEV_ERROR, LOG_CH(ch_ui),
				"WiredUI_Init: WIRED_UI_FORCE_INIT_FAIL=%s — simulated fault\n",
				forceFail );
			return qfalse;
		}
	}
#endif

	return qtrue;
}

void WiredUI_Shutdown( void ) {
	Q_SecureZeroMemory( &wui_passwordPrompt, sizeof( wui_passwordPrompt ) );
	WiredUI_StateSetString( "ui_joinPasswordError", "" );
	Cvar_Set( "ui_password_server_name", "" );
	if ( !wui_initialized ) {
		return;
	}

	WiredUI_ResetListboxDoubleClick( "shutdown" );
	WiredUI_ReleaseCompositorPointer( "shutdown" );
	WiredFeeder_ServerStatusCancel();
	WiredFeeder_ServerFixtureClear();
	/* A dead UI must not hold KEYCATCH_UI — that bit signals "I am alive and
	   handling input."  If we leave it set, Con_DrawConsole's fullscreen
	   auto-show sees the catcher and skips the fullscreen draw,
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
	Cmd_RemoveCommand( "wui_server_ping_tick" );
	Cmd_RemoveCommand( "wui_ingame_trace" );
	Cmd_RemoveCommand( "wui_test_keydown" );
	testall_active = qfalse;

	wui_healthy = qfalse;

	memset( wui_symbols, 0, sizeof( wui_symbols ) );
	memset( wui_elements, 0, sizeof( wui_elements ) );
	wui_numSymbols = 0;
	wui_numElements = 0;
	wui_activeMenu = UIMENU_NONE;
	wui_initialized = qfalse;

	/* wuiAnim pool reset so a hot-reload re-creates anims from
	 * fresh itemDef state instead of dangling target_ref pointers. */
	WUI_AnimStopAll();

	/* the compositor arena now lives at
	 * process scope (CL_Init / CL_Shutdown), so WiredUI_Shutdown — which runs
	 * every map load via CL_FlushMemory → CL_ShutdownAll → CL_ShutdownUI —
	 * MUST NOT tear it down. Compositor + Clay context + font indirection
	 * table all survive across map loads. CompositorShutdown is owned by
	 * CL_Shutdown only. */

	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: shutdown\n" );
}

void WiredUI_TickFrame( int realtime ) {
	wiredMenuDef_t *menu;

	if ( !wui_initialized ) {
		return;
	}

	/* Spinner click-and-hold auto-repeat. While a −/+ button is held
	 * (wui_spinnerHoldItem latched in WiredUI_KeyEvent on K_MOUSE1-down over a
	 * button), fire an extra step every WUI_SPINNER_HOLD_RATE_MS after the
	 * initial WUI_SPINNER_HOLD_DELAY_MS pause. Cleared on button release. The
	 * step/clamp math is the shared core helper so held-repeat matches a single
	 * click exactly. Stops repeating once the value pins at the range end. */
	if ( wui_spinnerHoldItem && wui_spinnerHoldDir != 0
	     && wui_spinnerHoldItem->type == ITEM_TYPE_SPINNER ) {
		while ( realtime >= wui_spinnerHoldNext ) {
			WiredUI_SpinnerAdjust( wui_spinnerHoldItem, wui_spinnerHoldDir, 1.0f );
			wui_spinnerHoldNext += WUI_SPINNER_HOLD_RATE_MS;
		}
	}

	/* poll-and-fire any palette reload deferred because
	 * cls.uiStarted was qfalse when the accent/mode cvar callback ran.
	 * No-op on most ticks (single-shot flag). */
	WiredPalette_TickPending();

	/* HUD state sync folded into the tick. Was a
	 * separate WiredHud_Routine / WiredUI_HudTick call from
	 * SCR_DrawScreenField; now lives here so the compositor walk emitted
	 * AFTER this tick sees fresh cgame state. The fail-fast gate
	 * (wiredHud_state_valid) keeps the tick a no-op when cgame hasn't
	 * pushed its first state frame yet. */
	if ( clientActiveApp->state == CA_ACTIVE && wiredHud_state_valid ) {
		WiredUI_HudTick( realtime );
	}

	// live 'hud' cvar change: reload only when the value actually differs
	if ( wired_hud && strcmp( wired_hud->string, wui_hud_lastLoaded ) != 0 ) {
		Q_strncpyz( wui_hud_lastLoaded, wired_hud->string, sizeof( wui_hud_lastLoaded ) );
		WiredUI_ReloadHud();
		return;
	}

	// Layer 5: hot-reload check (dev mode)
	WiredUI_CheckHotReload( realtime );

	/* wuiAnim per-frame tick. Walks active anims and writes
	 * eased values into per-item scalar fields (offsets / alpha) which
	 * the compositor's emit_item consumes. Cheap when pool is empty;
	 * dedicated mode never hits this path (WiredUI_TickFrame gated on
	 * wui_initialized which stays qfalse server-side). */
	WUI_AnimTick( realtime );

	// attract scheduler tick + transition overlay draw
	/* Skip the reel entirely while its layer is paused. Hiding it was never
	 * the expensive part — advancing the schedule and playing a demo behind
	 * an opaque backdrop is, and that is the work `paused` exists to skip. */
	if ( !WiredUI_LayerPaused( WUI_LAYER_BG_ATTRACT ) )
		WiredAttract_Frame( realtime );

	// delayed screenshot — fire once after N seconds
	if ( wired_screenshotDelay && wired_screenshotDelay->integer > 0 && !wui_screenshotTaken ) {
		if ( realtime - wui_screenshotTime >= wired_screenshotDelay->integer * 1000 ) {
			Cbuf_ExecuteText( EXEC_APPEND, "screenshot jpg\n" );
			wui_screenshotTaken = qtrue;
			Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: delayed screenshot taken\n" );
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
					Com_Log( SEV_INFO, LOG_CH(ch_ui), "ui_testall: [%d/%d] %s\n",
						testall_menuIndex + 1, menuCount, m->name );
				}
				testall_menuIndex++;
			} else {
				// all menus shown — stop
				testall_active = qfalse;
				Com_Log( SEV_INFO, LOG_CH(ch_ui), "ui_testall: done (%d menus tested)\n", testall_menuIndex );
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
			if ( !WiredFeeder_ServerFixtureActive() && realtime - lastPingUpdate > 1000 ) {  // every second
				int uiSource = WiredUI_StateGetInt( "ui_netSource" );
				int engineSource = WiredUI_ServerEngineSource( uiSource );

				CL_UpdateVisiblePings_f( engineSource );
				lastPingUpdate = realtime;
			}
		}
	}

	if ( menu == WiredUI_FindMenu( "serverinfo" ) ) {
		WiredFeeder_ServerStatusPoll();
	}


}

/* single dispatch authority. Replaces the legacy
 * SCR_DrawScreenField 5-dispatch (Con_DrawConsole / WiredHud_Routine /
 * CompositorEmitFrame / WiredUI_Refresh / SCR_Draw* helpers) with the
 * tick-then-walk pair. cl_scrn.c::SCR_DrawScreenField collapses to
 * re.BeginFrame + this. */
void WiredUI_RenderFrame( void )
{
	if ( !wui_initialized ) {
		/* wui_required + graceful fallback are wired here. For now:
		 * no-op until init succeeds (no current visible UI either). */
		return;
	}

	WiredUI_TickFrame      ( cls.realtime );
	WiredUI_CompositorEmitFrame();
}

// ── script command system ─────────────────────────────────────────────
//
// Based on the legacy ui_script.c command table pattern. Same handler
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
	wuiBgIntent_t intent = WUI_BG_INTENT_INHERIT;
	if ( numArgs < 1 ) return;
	/* Optional 2nd arg selects the background intent so a .wui action can open a
	 * menu ONTO the parallax scene: `open "startserver" scene`. Absent → INHERIT
	 * (today's authored-flags background), so every existing `open "x"` is unchanged. */
	if ( numArgs >= 2 ) {
		if      ( !Q_stricmp( args[1], "scene" ) ) intent = WUI_BG_INTENT_SCENE;
		else if ( !Q_stricmp( args[1], "dim"   ) ) intent = WUI_BG_INTENT_DIM;
		else if ( !Q_stricmp( args[1], "none"  ) ) intent = WUI_BG_INTENT_NONE;
	}
	WiredUI_PushMenu( args[0] );
}

static void WiredScript_Close( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	WiredUI_PopMenu();
}

static void WiredScript_SetCvar( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( numArgs >= 2 ) {
		if ( WiredUI_IsStoreStateKey( args[0] ) ) {
			COM_WARN( LOG_CH(ch_ui), "WiredUI: setcvar '%s' targets store-backed UI state; use setstate\n", args[0] );
		}
		Cvar_Set( args[0], args[1] );
	}
}

// cyclecvar <cvar> <min> <max> [step]
//   Steps an integer cvar by `step` (default 1) within [min, max], wrapping
//   around. Used by ownerdraw items (e.g. Effect color) where each click
//   should advance the value to the next enum.
static void WiredScript_CycleCvar( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int min, max, step, val, range;

	if ( numArgs < 3 ) return;
	min  = atoi( args[1] );
	max  = atoi( args[2] );
	step = ( numArgs >= 4 ) ? atoi( args[3] ) : 1;
	if ( step == 0 ) step = 1;
	if ( max < min ) { int t = min; min = max; max = t; }

	val   = (int)Cvar_VariableValue( args[0] );
	range = max - min + 1;
	if ( range <= 0 ) return;

	val = val + step;
	// wrap into [min, max]
	val = ( ( val - min ) % range + range ) % range + min;

	Cvar_Set( args[0], va( "%d", val ) );
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

/* Quick-add buttons are authored with a character directory, but should not
 * inherit the generic exec command's arbitrary-string surface or deferred
 * queue position.  Resolve the directory against the loaded character
 * registry, constrain its command-token grammar, and insert one canonical
 * local-server command for the next command-buffer pass. */
static qboolean WiredUI_IsSafeBotProfileToken( const char *profile ) {
	const unsigned char *p = (const unsigned char *)profile;
	if ( !profile || !profile[0] || strlen( profile ) >= MAX_QPATH ) return qfalse;
	for ( ; *p; p++ ) {
		if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' )
		  || ( *p >= '0' && *p <= '9' ) || *p == '_' || *p == '-' ) ) return qfalse;
	}
	return qtrue;
}

static qboolean WiredUI_IsSafeBotDisplayName( const char *name ) {
	const unsigned char *p = (const unsigned char *)name;
	int length;
	qboolean hasAlnum = qfalse;
	if ( !name ) return qfalse;
	length = (int)strlen( name );
	if ( length < 1 || length > 31 || name[0] == ' ' || name[length - 1] == ' ' ) return qfalse;
	for ( ; *p; p++ ) {
		if ( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' )
		  || ( *p >= '0' && *p <= '9' ) ) {
			hasAlnum = qtrue;
			continue;
		}
		if ( *p != ' ' && *p != '_' && *p != '-' ) return qfalse;
	}
	return hasAlnum;
}

static const clCharacterEntry_t *WiredUI_ResolveEligibleBotProfile( const char *profile ) {
	if ( !WiredUI_IsSafeBotProfileToken( profile ) ) return NULL;
	for ( int i = 0; i < CL_Characters_Count(); i++ ) {
		const clCharacterEntry_t *entry = CL_Characters_At( i );
		if ( entry && entry->loaded && entry->botEligible
		  && !Q_stricmp( entry->dirname, profile ) ) return entry;
	}
	return NULL;
}

typedef struct {
	qboolean valid;
	unsigned int generation;
	char profile[MAX_QPATH];
} wiredBotProfileSelection_t;

static wiredBotProfileSelection_t wui_botProfileSelection;

static qboolean WiredUI_CaptureBotProfileSelection( const char *profile ) {
	const clCharacterEntry_t *entry = WiredUI_ResolveEligibleBotProfile( profile );
	memset( &wui_botProfileSelection, 0, sizeof( wui_botProfileSelection ) );
	if ( !entry ) return qfalse;
	wui_botProfileSelection.valid = qtrue;
	wui_botProfileSelection.generation = CL_Characters_Generation();
	Q_strncpyz( wui_botProfileSelection.profile, entry->dirname,
		sizeof( wui_botProfileSelection.profile ) );
	WiredUI_StateSetString( "ui_botProfile", entry->dirname );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: custom bot profile selected profile=%s generation=%u\n",
		entry->dirname, wui_botProfileSelection.generation );
	return qtrue;
}

static void WiredScript_BotProfileInit( wiredMenuDef_t *menu, wiredItemDef_t *item,
	int numArgs, const char **args ) {
	const char *preferred = numArgs >= 1 ? args[0] : "visor";
	(void)menu;
	(void)item;
	if ( WiredUI_CaptureBotProfileSelection( preferred ) ) return;
	for ( int i = 0; i < CL_Characters_Count(); i++ ) {
		const clCharacterEntry_t *entry = CL_Characters_At( i );
		if ( entry && entry->loaded && entry->botEligible
		  && WiredUI_CaptureBotProfileSelection( entry->dirname ) ) return;
	}
	WiredUI_StateSetString( "ui_botProfile", "" );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: no eligible custom bot profile\n" );
}

static void WiredScript_BotProfileSelected( wiredMenuDef_t *menu, wiredItemDef_t *item,
	int numArgs, const char **args ) {
	char profile[MAX_QPATH];
	(void)menu;
	(void)item;
	(void)numArgs;
	(void)args;
	WiredUI_StateGetString( "ui_botProfile", profile, sizeof( profile ) );
	if ( !WiredUI_CaptureBotProfileSelection( profile ) ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: custom bot profile selection rejected\n" );
	}
}

static void WiredScript_AddQuickBot( wiredMenuDef_t *menu, wiredItemDef_t *item,
	int numArgs, const char **args ) {
	const clCharacterEntry_t *entry;

	(void)menu;
	(void)item;

	if ( !com_sv_running || !com_sv_running->integer ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: quick bot add refused without a running local server\n" );
		return;
	}
	if ( numArgs != 1 || !args || !args[0][0] ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: quick bot add rejected without a profile\n" );
		return;
	}
	entry = WiredUI_ResolveEligibleBotProfile( args[0] );
	if ( !entry ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: quick bot add rejected invalid or ineligible profile\n" );
		return;
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: queued validated quick bot add profile=%s skill=3 team=free\n",
		entry->dirname );
	Cbuf_ExecuteText( EXEC_INSERT, va( "addbot \"%s\" 3 free\n", entry->dirname ) );
}

static void WiredScript_AddCustomBot( wiredMenuDef_t *menu, wiredItemDef_t *item,
	int numArgs, const char **args ) {
	char profile[ MAX_QPATH ];
	/* Read the complete Store value before applying the authored 31-byte form
	 * contract.  A form-sized destination would silently accept an unsafe or
	 * overlong persisted value after Q_strncpyz truncation. */
	char name[ sizeof( ((wuiStoreEntry_t *)0)->text ) ];
	char skillText[ 16 ];
	char team[ 16 ];
	char *skillEnd = NULL;
	long skill;
	const char *canonicalTeam;
	const clCharacterEntry_t *entry;

	(void)menu;
	(void)item;
	(void)numArgs;
	(void)args;

	if ( !com_sv_running || !com_sv_running->integer ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: custom bot add refused without a running local server\n" );
		return;
	}

	WiredUI_StateGetString( "ui_botProfile", profile, sizeof( profile ) );
	WiredUI_StateGetString( "ui_botName", name, sizeof( name ) );
	WiredUI_StateGetString( "ui_botSkill", skillText, sizeof( skillText ) );
	WiredUI_StateGetString( "ui_botTeam", team, sizeof( team ) );

	if ( !wui_botProfileSelection.valid
	  || wui_botProfileSelection.generation != CL_Characters_Generation()
	  || Q_stricmp( profile, wui_botProfileSelection.profile ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: custom bot add rejected stale profile selection\n" );
		return;
	}
	entry = WiredUI_ResolveEligibleBotProfile( wui_botProfileSelection.profile );
	if ( !entry ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: custom bot add rejected ineligible profile\n" );
		return;
	}
	if ( !WiredUI_IsSafeBotDisplayName( name ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: custom bot add rejected invalid display name\n" );
		return;
	}
	skill = strtol( skillText, &skillEnd, 10 );
	if ( skillEnd == skillText || *skillEnd != '\0' || skill < 1 || skill > 5 ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: custom bot add rejected invalid skill\n" );
		return;
	}
	if ( !Q_stricmp( team, "free" ) ) canonicalTeam = "free";
	else if ( !Q_stricmp( team, "red" ) ) canonicalTeam = "red";
	else if ( !Q_stricmp( team, "blue" ) ) canonicalTeam = "blue";
	else {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: custom bot add rejected invalid team\n" );
		return;
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: queued validated custom bot add profile=%s name=%s skill=%ld team=%s\n",
		entry->dirname, name, skill, canonicalTeam );
	Cbuf_ExecuteText( EXEC_INSERT,
		va( "addbot \"%s\" %ld %s 0 \"%s\"\n", entry->dirname, skill, canonicalTeam, name ) );
	WiredUI_PopMenu();
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
	int i;
	wiredItemDef_t *target = NULL;
	if ( numArgs < 1 ) return;
	/* recursive find so nested children are reachable. */
	for ( i = 0; i < menu->itemCount && !target; i++ ) {
		target = wui_find_item_recursive( menu->items[ i ], args[0] );
	}
	if ( target ) wui_set_focused( menu, target );
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
	{ "g_skill", qfalse },
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
	{ "ui_hostListed", qfalse },
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
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: ToggleMapPool — no map selected\n" );
		return;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: ToggleMapPool '%s'\n", mapName );
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

// ── vote dispatch handlers ───────────────────────────────────────────
// callvote.wui / removebots.wui submit buttons deposit their selection into
// the state dict via the feeder selection callback, then invoke one of these
// handlers to read the value and dispatch the real console command. (q3now has
// no $-cvar expansion in WiredScript args, so the value must round-trip through
// the state dict — same convention as StartServer's ui_selectedMap flow.)
// Map vote reads ui_selectedMap (deposited by the allmaps feeder). Kick reads
// ui_selectedPlayerNum and leader reads ui_selectedTeamPlayerNum — the client
// NUMBER deposited by the two player-list feeders (FEEDER_PLAYER_LIST and
// FEEDER_TEAM_LIST respectively), since the game's robust vote path is numeric
// (clientkick <n> / callteamvote leader <n>). The keys are DISTINCT because
// both listboxes live on the same callvote.wui screen; sharing one key made a
// selection in either list overwrite the other's.

static void WiredScript_VoteMap( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char buf[MAX_QPATH];
	WiredUI_StateGetString( "ui_selectedMap", buf, sizeof( buf ) );
	if ( buf[0] ) Cbuf_ExecuteText( EXEC_APPEND, va( "callvote map %s\n", buf ) );
}

static void WiredScript_VoteKick( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char buf[MAX_QPATH];
	WiredUI_StateGetString( "ui_selectedPlayerNum", buf, sizeof( buf ) );
	if ( buf[0] ) Cbuf_ExecuteText( EXEC_APPEND, va( "callvote clientkick %s\n", buf ) );
}

static void WiredScript_VoteLeader( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char buf[MAX_QPATH];
	WiredUI_StateGetString( "ui_selectedTeamPlayerNum", buf, sizeof( buf ) );
	if ( buf[0] ) Cbuf_ExecuteText( EXEC_APPEND, va( "callteamvote leader %s\n", buf ) );
}

// removebots.wui: direct host-side kick (not a vote) of the selected client.
static void WiredScript_Kick( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	wuiBotSelection_t selection;
	if ( !com_sv_running || !com_sv_running->integer ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: bot kick refused without a running local server\n" );
		return;
	}
	if ( !WiredFeeder_GetSelectedBotIdentity( &selection ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: bot kick refused without a current bot selection\n" );
		return;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: queued verified bot kick client=%d allocation=%" PRIu64 "\n",
		selection.clientNum, selection.allocationId );
	Cbuf_ExecuteText( EXEC_INSERT, va( "botkick %d %" PRIu64 "\n",
		selection.clientNum, selection.allocationId ) );
	WiredFeeder_ClearBotSelection();
}

static void WiredScript_ClearBotSelection( wiredMenuDef_t *menu, wiredItemDef_t *item,
	int numArgs, const char **args ) {
	WiredFeeder_ClearBotSelection();
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI: no map selected\n" );
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

	// server listing policy: private (unlisted) vs public (announced to masters).
	// The server always runs in-process (the 'map' command below starts it);
	// the only choice is whether it advertises itself.
	Cvar_Set( "sv_hostListed", WiredUI_StateGetInt( "ui_hostListed" ) ? "1" : "0" );

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

static void WiredScript_ResetPasswordPrompt( qboolean clearError ) {
	WiredUI_ClearPasswordPromptState( clearError );
}

static qboolean WiredScript_BeginBrowserConnect( const char *normalized,
	const netadr_t *resolved, const char *password, int selectionGeneration ) {
	if ( !CL_ConnectBrowserServer( normalized, resolved, password,
		selectionGeneration ) ) {
		return qfalse;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: started validated connect origin=browser address=%s selection_generation=%d credential_present=%d\n",
		normalized, selectionGeneration, password && password[0] ? 1 : 0 );
	WiredUI_CloseAllMenus();
	return qtrue;
}

static void WiredScript_JoinServer( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char addr[256];
	char normalized[256];
	char password[MAX_CVAR_VALUE_STRING];
	qboolean needPassword;
	qboolean passwordSubmit = menu && !Q_stricmp( menu->name, "password" );
	int selectionGeneration;
	netadr_t resolved;

	if ( !WiredFeeder_GetSelectedServerConnection( addr, sizeof( addr ),
		NULL, 0, &needPassword, &selectionGeneration ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: browser connect refused without current selection\n" );
		if ( passwordSubmit ) {
			WiredScript_ResetPasswordPrompt( qtrue );
			WiredUI_PopMenu();
		}
		return;
	}
	if ( !CL_NormalizeServerAddress( addr, NA_UNSPEC, normalized, sizeof( normalized ), &resolved ) ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: rejected invalid server address origin=browser\n" );
		if ( passwordSubmit ) {
			WiredScript_ResetPasswordPrompt( qtrue );
			WiredUI_PopMenu();
		}
		return;
	}

	if ( needPassword && !passwordSubmit ) {
		/* A cached credential must never bypass target confirmation.  Bind the
		 * prompt to the feeder's typed selection epoch; do not seed reconnect. */
		WiredScript_ResetPasswordPrompt( qtrue );
		wui_passwordPrompt.valid = qtrue;
		Q_strncpyz( wui_passwordPrompt.address, normalized,
			sizeof( wui_passwordPrompt.address ) );
		wui_passwordPrompt.selectionGeneration = selectionGeneration;
		/* A hostile hostname must not impersonate the credential target. */
		WiredUI_StateSetString( "ui_password_server_name", normalized );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: password required origin=browser address=%s selection_generation=%d\n",
			normalized, selectionGeneration );
		WiredUI_PushMenu( "password" );
		return;
	}
	if ( passwordSubmit ) {
		Q_strncpyz( password, wui_passwordPrompt.secret, sizeof( password ) );
		if ( !needPassword || !wui_passwordPrompt.valid
		     || wui_passwordPrompt.selectionGeneration != selectionGeneration
		     || Q_stricmp( wui_passwordPrompt.address, normalized ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
				"WiredUI: password submit refused reason=stale-selection\n" );
			Q_SecureZeroMemory( password, sizeof( password ) );
			WiredScript_ResetPasswordPrompt( qtrue );
			WiredUI_PopMenu();
			return;
		}
		if ( !WiredUI_IsSafePasswordValue( password ) ) {
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
				"WiredUI: password submit refused reason=invalid-credential\n" );
			Q_SecureZeroMemory( password, sizeof( password ) );
			return;
		}
		if ( !WiredScript_BeginBrowserConnect( normalized, &resolved, password,
			selectionGeneration ) ) {
			Q_SecureZeroMemory( password, sizeof( password ) );
			return;
		}
		Q_SecureZeroMemory( password, sizeof( password ) );
		WiredScript_ResetPasswordPrompt( qtrue );
		return;
	}
	WiredScript_ResetPasswordPrompt( qtrue );
	WiredScript_BeginBrowserConnect( normalized, &resolved, NULL,
		selectionGeneration );
}

static void WiredScript_JoinServerPassword( wiredMenuDef_t *menu,
	wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( !menu || Q_stricmp( menu->name, "password" ) ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: password submit refused reason=inactive-prompt\n" );
		return;
	}
	WiredScript_JoinServer( menu, item, numArgs, args );
}

static void WiredScript_CancelServerPassword( wiredMenuDef_t *menu,
	wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( !menu || Q_stricmp( menu->name, "password" ) ) return;
	WiredUI_CancelPasswordPrompt( item ? "button" : "menu-escape" );
}

qboolean CL_WiredUI_ShowJoinPasswordRetry( const char *target,
	int selectionGeneration ) {
	char addr[256];
	char normalized[256];
	char serverName[256];
	qboolean needPassword;
	int currentGeneration;
	netadr_t resolved;

	if ( !target || !target[0] || !cls.uiStarted ) {
		return qfalse;
	}
	if ( !WiredFeeder_GetSelectedServerConnection( addr, sizeof( addr ),
		serverName, sizeof( serverName ), &needPassword, &currentGeneration )
	     || !needPassword || currentGeneration != selectionGeneration
	     || !CL_NormalizeServerAddress( addr, NA_UNSPEC, normalized,
			sizeof( normalized ), &resolved )
	     || Q_stricmp( normalized, target ) ) {
		WiredScript_ResetPasswordPrompt( qtrue );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: authentication retry suppressed reason=stale-selection selection_generation=%d\n",
			selectionGeneration );
		return qfalse;
	}

	WiredUI_CloseAllMenus();
	WiredUI_SetActiveMenu( UIMENU_MAIN );
	WiredUI_PushMenu( "servers" );
	WiredScript_ResetPasswordPrompt( qtrue );
	wui_passwordPrompt.valid = qtrue;
	Q_strncpyz( wui_passwordPrompt.address, normalized,
		sizeof( wui_passwordPrompt.address ) );
	wui_passwordPrompt.selectionGeneration = selectionGeneration;
	WiredUI_StateSetString( "ui_password_server_name", normalized );
	WiredUI_StateSetString( "ui_joinPasswordError",
		"Authentication failed. Enter the server password again." );
	WiredUI_PushMenu( "password" );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: authentication retry opened address=%s selection_generation=%d\n",
		normalized, selectionGeneration );
	return qtrue;
}

static void WiredScript_ConnectSpecified( wiredMenuDef_t *menu, wiredItemDef_t *item,
	int numArgs, const char **args ) {
	char raw[256];
	char normalized[256];

	WiredUI_StateGetString( "ui_specifyAddress", raw, sizeof( raw ) );
	if ( !CL_NormalizeServerAddress( raw, NA_UNSPEC, normalized, sizeof( normalized ), NULL ) ) {
		/* Never echo rejected Store data: it can contain control/command text. */
		COM_WARN( LOG_CH(ch_ui), "WiredUI: rejected invalid server address origin=specify\n" );
		return;
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: queued validated connect origin=specify address=%s\n", normalized );
	WiredUI_CloseAllMenus();
	Cbuf_ExecuteText( EXEC_APPEND, va( "connect \"%s\"\n", normalized ) );
}

static void WiredScript_RunDemo( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	char demoName[MAX_QPATH];
	const unsigned char *p;

	if ( !WiredFeeder_GetSelectedDemo( demoName, sizeof( demoName ) ) ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: no demo selected\n" );
		return;
	}
	if ( !Q_stricmp( demoName, "." ) || !Q_stricmp( demoName, ".." ) ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: rejected unsafe demo selection\n" );
		return;
	}
	for ( p = (const unsigned char *)demoName; *p; p++ ) {
		if ( !( ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' )
		     || ( *p >= '0' && *p <= '9' ) || *p == '_' || *p == '-' || *p == '.' ) ) {
			COM_WARN( LOG_CH(ch_ui), "WiredUI: rejected unsafe demo selection\n" );
			return;
		}
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI: queued validated demo playback name=%s\n", demoName );
	WiredUI_CloseAllMenus();
	Cbuf_ExecuteText( EXEC_INSERT, va( "demo_ui \"%s\"\n", demoName ) );
}

static void WiredScript_LoadDemos( wiredMenuDef_t *menu, wiredItemDef_t *item,
	int numArgs, const char **args ) {
	WiredFeeder_LoadDemos();
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI: no mod selected\n" );
		return;
	}

	WiredUI_CloseAllMenus();

	// BASEGAME means return to base game — clear fs_game
	if ( Q_stricmp( modName, BASEGAME ) == 0 ) {
		Cvar_Set( "fs_game", "" );
	} else {
		Cvar_Set( "fs_game", modName );
	}
	/* Defer the full game-directory lifecycle until this UI action returns.
	 * fs_game has already passed its CV_FSPATH validator, so the restart command
	 * needs no externally sourced argument and opens no command-string surface. */
	Cbuf_ExecuteText( EXEC_APPEND, "game_restart\n" );
}

static void WiredScript_RefreshServers( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int source = WiredUI_StateGetInt( "ui_netSource" );
	char command[128];
	qtime_t qt;
	if ( WiredFeeder_ServerFixtureActive() ) {
		WiredFeeder_RebuildServerDisplayList();
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: server refresh suppressed by automated fixture\n" );
		return;
	}

	/* Keep refresh causally adjacent to the authored action.  EXEC_APPEND can
	 * strand these queries behind an already-buffered automation/config tail,
	 * letting later UI steps observe a roster before the refresh ever ran. */
	Q_strncpyz( command, "localservers\n", sizeof( command ) );
	if ( source > 0 && source < 6 ) {
		// query internet master servers
		Com_sprintf( command, sizeof( command ),
			"localservers\nglobalservers %d %d\n", source - 1, PROTOCOL_VERSION );
	}
	Cbuf_ExecuteText( EXEC_INSERT, command );

	// record refresh timestamp for UI display
	Com_RealTime( &qt );
	WiredUI_StateSetString( "ui_lastRefreshDate", va( "%02d:%02d:%02d", qt.tm_hour, qt.tm_min, qt.tm_sec ) );
}

static void WiredScript_RefreshFilter( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	extern void WiredFeeder_RebuildServerDisplayList( void );
	WiredFeeder_RebuildServerDisplayList();
}

static qboolean WiredUI_ConfigureServerInfoControls( qboolean connected ) {
	wiredMenuDef_t *popup = WiredUI_FindMenu( "serverinfo" );
	wiredItemDef_t *back = NULL;
	wiredItemDef_t *connect = NULL;
	wiredItemDef_t *retry = NULL;
	wiredItemDef_t *close = NULL;

	for ( int i = 0; popup && i < popup->itemCount; i++ ) {
		if ( !back ) back = wui_find_item_recursive( popup->items[i], "btn_back" );
		if ( !connect ) connect = wui_find_item_recursive( popup->items[i], "btn_connect" );
		if ( !retry ) retry = wui_find_item_recursive( popup->items[i], "btn_retry" );
		if ( !close ) close = wui_find_item_recursive( popup->items[i], "btn_close" );
	}
	if ( !back || !connect || !retry || !close ) return qfalse;
	back->visible = connected ? qfalse : qtrue;
	connect->visible = connected ? qfalse : qtrue;
	retry->visible = qtrue;
	close->visible = qtrue;
	if ( connected ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: connected server status controls back=%d connect=%d retry=%d close=%d\n",
			back->visible ? 1 : 0, connect->visible ? 1 : 0,
			retry->visible ? 1 : 0, close->visible ? 1 : 0 );
	}
	return qtrue;
}

static void WiredScript_ServerStatusOpen( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( !WiredFeeder_ServerStatusBegin() ) {
		return;
	}
	if ( !WiredUI_ConfigureServerInfoControls( qfalse ) ) {
		WiredFeeder_ServerStatusCancel();
		return;
	}
	WiredUI_PushMenu( "serverinfo" );
}

static void WiredScript_ServerStatusOpenConnected( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	if ( !WiredFeeder_ServerStatusBeginConnected() ) {
		return;
	}
	if ( !WiredUI_ConfigureServerInfoControls( qtrue ) ) {
		WiredFeeder_ServerStatusCancel();
		return;
	}
	WiredUI_PushMenu( "serverinfo" );
}

static void WiredScript_ServerStatusCancel( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	WiredFeeder_ServerStatusCancel();
}

static void WiredScript_ServerStatusRetry( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	(void) WiredFeeder_ServerStatusRetry();
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
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: MapSort column %d\n", col );
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

// ── character cycling ─────────────────────────────────────────────────

extern int         WiredFeeder_GetCharacterCount( void );
extern int         WiredFeeder_GetCharacterSelected( void );
extern const char *WiredFeeder_GetCharacterName( int index );
extern void        WiredFeeder_SetCharacterSelected( int index );

static void WiredScript_PrevCharacter( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int count = WiredFeeder_GetCharacterCount();
	if ( count < 1 ) return;
	int sel = WiredFeeder_GetCharacterSelected();
	sel = ( sel <= 0 ) ? count - 1 : sel - 1;
	WiredFeeder_SetCharacterSelected( sel );
}

static void WiredScript_NextCharacter( wiredMenuDef_t *menu, wiredItemDef_t *item, int numArgs, const char **args ) {
	int count = WiredFeeder_GetCharacterCount();
	if ( count < 1 ) return;
	int sel = WiredFeeder_GetCharacterSelected();
	sel = ( sel >= count - 1 ) ? 0 : sel + 1;
	WiredFeeder_SetCharacterSelected( sel );
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
	{ "JoinServerPassword", WiredScript_JoinServerPassword },
	{ "joinserverpassword", WiredScript_JoinServerPassword },
	{ "CancelServerPassword", WiredScript_CancelServerPassword },
	{ "cancelserverpassword", WiredScript_CancelServerPassword },
	{ "ConnectSpecified", WiredScript_ConnectSpecified },
	{ "connectspecified", WiredScript_ConnectSpecified },
	{ "RunDemo",          WiredScript_RunDemo },
	{ "rundemo",          WiredScript_RunDemo },
	{ "RunMod",           WiredScript_RunMod },
	{ "runmod",           WiredScript_RunMod },
	{ "LoadDemos",        WiredScript_LoadDemos },
	{ "LoadMods",         NULL },
	{ "LoadMovies",       NULL },
	{ "RefreshServers",   WiredScript_RefreshServers },
	{ "RefreshFilter",    WiredScript_RefreshFilter },
	{ "ServerStatusOpen", WiredScript_ServerStatusOpen },
	{ "serverstatusopen", WiredScript_ServerStatusOpen },
	{ "ServerStatusOpenConnected", WiredScript_ServerStatusOpenConnected },
	{ "serverstatusopenconnected", WiredScript_ServerStatusOpenConnected },
	{ "ServerStatusCancel", WiredScript_ServerStatusCancel },
	{ "serverstatuscancel", WiredScript_ServerStatusCancel },
	{ "ServerStatusRetry", WiredScript_ServerStatusRetry },
	{ "serverstatusretry", WiredScript_ServerStatusRetry },
	{ "StopRefresh",      NULL },  // noop — server queries are fire-and-forget
	{ "closeJoin",        NULL },
	{ "closeingame",      NULL },
	{ "prevCharacter",    WiredScript_PrevCharacter },
	{ "nextCharacter",    WiredScript_NextCharacter },
	{ "Kick",             WiredScript_Kick },
	{ "kick",             WiredScript_Kick },
	{ "ClearBotSelection", WiredScript_ClearBotSelection },
	{ "clearbotselection", WiredScript_ClearBotSelection },
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
		/* controls is a settings-cluster tab (SCENE everywhere via the settings
		 * nav) — this alternate uiScript entry matches that so the tab is never
		 * the bare authored grid. */
		WiredUI_PushMenu( "controls" );
	} else if ( !Q_stricmp( args[0], "clearError" ) ) {
		// noop
	} else if ( !Q_stricmp( args[0], "ServerSort" ) && numArgs >= 2 ) {
		extern void WiredFeeder_SortServers( int column );
		WiredFeeder_SortServers( atoi( args[1] ) );
	} else if ( !Q_stricmp( args[0], "MapSort" ) && numArgs >= 2 ) {
		extern void WiredFeeder_SortMaps( int column );
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: MapSort column %s\n", args[1] );
		WiredFeeder_SortMaps( atoi( args[1] ) );
	} else if ( !Q_stricmp( args[0], "addFavorite" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "addFavorite\n" );
	} else if ( !Q_stricmp( args[0], "deleteFavorite" ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, "deleteFavorite\n" );
	} else {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: unknown uiScript '%s'\n", args[0] );
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
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "Copied %d bytes to clipboard.\n", (int)strlen(src) );
	}
}

// command table — matches the legacy ui_script.c set, plus Wired UI additions + v6 compat
static const wiredScriptCommand_t wiredScriptCommands[] = {
	{ "show",             WiredScript_Show },
	{ "hide",             WiredScript_Hide },
	{ "open",             WiredScript_Open },
	{ "close",            WiredScript_Close },
	{ "setcvar",          WiredScript_SetCvar },
	{ "cyclecvar",        WiredScript_CycleCvar },
	{ "setstate",         WiredScript_SetState },
	{ "savestate",        WiredScript_SaveState },
	{ "loadstate",        WiredScript_LoadState },
	{ "exec",             WiredScript_Exec },
	{ "addquickbot",      WiredScript_AddQuickBot },
	{ "botprofileinit",   WiredScript_BotProfileInit },
	{ "botprofileselected", WiredScript_BotProfileSelected },
	{ "addcustombot",     WiredScript_AddCustomBot },
	{ "clipboard",        WiredScript_Clipboard },
	{ "execConfirm",      WiredScript_ExecConfirm },
	{ "execconfirm",      WiredScript_ExecConfirm },
	{ "play",             WiredScript_Play },
	{ "playlooped",       WiredScript_PlayLooped },
	{ "stopmusic",        WiredScript_StopMusic },
	{ "fadein",           WiredScript_FadeIn },
	{ "fadeout",          WiredScript_FadeOut },
	{ "setfocus",         WiredScript_SetFocus },
	// ── v6 compatibility commands ────────────────────────
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
	// ── vote dispatch (callvote.wui submit buttons) ─────────────────
	{ "voteMap",          WiredScript_VoteMap },
	{ "voteKick",         WiredScript_VoteKick },
	{ "voteLeader",       WiredScript_VoteLeader },
	{ "ToggleFavorite",   WiredScript_ToggleFavoriteMap },
	{ "togglefavorite",   WiredScript_ToggleFavoriteMap },
	{ "ServerSort",       WiredScript_ServerSortCmd },
	{ "serversort",       WiredScript_ServerSortCmd },
	// ── game action commands (also reachable via uiScript) ──────────
	{ "startserver",      WiredScript_StartServer },
	{ "joinserver",       WiredScript_JoinServer },
	{ "joinserverpassword", WiredScript_JoinServerPassword },
	{ "cancelserverpassword", WiredScript_CancelServerPassword },
	{ "connectspecified", WiredScript_ConnectSpecified },
	{ "rundemo",          WiredScript_RunDemo },
	{ "runmod",           WiredScript_RunMod },
	{ "refreshservers",   WiredScript_RefreshServers },
	{ "serverstatusopen", WiredScript_ServerStatusOpen },
	{ "serverstatusopenconnected", WiredScript_ServerStatusOpenConnected },
	{ "serverstatuscancel", WiredScript_ServerStatusCancel },
	{ "serverstatusretry", WiredScript_ServerStatusRetry },

	{ NULL, NULL }
};

// ── script runner ─────────────────────────────────────────────────────
// Based on the legacy UI_RunScript (ui_script.c:282). Same tokenization:
// semicolon-separated commands, quoted string args, up to 8 args each.
// KEY DIFFERENCE: unknown commands pass to engine console instead of
// being silently dropped.

static void WiredUI_RunScript( wiredMenuDef_t *menu, wiredItemDef_t *item, const char *script ) {
	char        token[MAX_STRING_CHARS];
	const char *args[WIRED_MAX_SCRIPT_ARGS];
	/* Script actions can re-enter the runner (`open` -> menu `onOpen`). Keep
	 * argument storage call-local: a shared static buffer lets the nested script
	 * overwrite the outer handler's args while that handler is still using them
	 * (notably corrupting the menu name inside WiredUI_PushMenu). */
	char        argBuf[WIRED_MAX_SCRIPT_ARGS][256];
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
		COM_WARN( LOG_CH(ch_ui), "WiredUI: cannot open menu '%s' — not found (loaded %d menus)\n", name, WiredUI_GetMenuCount() );
		return;
	}
	WiredUI_ResetListboxDoubleClick( "push" );
	WiredUI_ReleaseCompositorPointer( "push" );

	/* Idempotent top-of-stack guard. The settings sub-menus return to main with a
	 * synchronous `close ; open "main"` batch (options/video/servers.wui et al):
	 * `close` pops the sub-menu leaving main (now a real stack entry — see the
	 * first-input / SetActiveMenu(MAIN) promotion) on top, then `open "main"` would
	 * stack a SECOND main. That duplicate makes the first ESC-on-main a no-op (it
	 * pops the dup back to the identical main instead of dismissing to attract) and
	 * confuses depth-based predicates. Since the menu already showing on top is the
	 * one being re-opened, collapse to it rather than push a copy. (error_popup has
	 * its own analogous dedup at ShowError; this generalises the rule.) */
	if ( wui_menuStackDepth > 0
	     && Q_stricmp( wui_menuStack[ wui_menuStackDepth - 1 ], name ) == 0 ) {
		/* Already on top — collapse. (The background intent that used to be
		 * refreshed here is gone: it now comes from the menu's own `backdrop`
		 * declaration, asked fresh every frame.) */
		/* legacy note: a re-open with a
		 * different intent (e.g. INHERIT→DIM when entering from gameplay) takes
		 * effect without stacking a duplicate. */
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: push menu '%s' collapsed (already on top, depth %d)\n", name, wui_menuStackDepth );
		Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
		return;
	}

	if ( wui_menuStackDepth >= WIRED_MENU_STACK_DEPTH ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: menu stack overflow (max %d)\n", WIRED_MENU_STACK_DEPTH );
		return;
	}

	/* WiredUI F4 (return-focus): before this new menu takes the top slot and
	 * clears the shared focus pointers, remember the control that was focused on
	 * the menu we're pushing OVER. WiredUI_PopMenu restores it when this menu is
	 * closed, returning the caret to the button/row that opened the dialog. The
	 * slot is keyed by the NEW depth (post-increment index below), so pop reads
	 * wui_returnFocus[depth-after-decrement]. */
	wui_returnFocus[ wui_menuStackDepth ] = wui_focusedItemPtr;

	Q_strncpyz( wui_menuStack[wui_menuStackDepth], name, sizeof( wui_menuStack[0] ) );
	wui_menuStackDepth++;
	/* Nudge the scene only when the backdrop actually CHANGES.
	 *
	 * This used to fire on every push. Switching settings tabs is a close +
	 * open of two menus declaring the SAME preset, so the backdrop was kicked
	 * and re-settled across a transition where nothing behind the menu
	 * differed — visible as the background restarting on every tab. It was
	 * inert while WUI_DrawBackgroundScene had no callers; wiring the live
	 * emitter back up made the nudge real, and with it the complaint.
	 *
	 * Comparing presets rather than menu names keeps the effect for the case it
	 * exists to serve: main (dim) -> a deep menu (animated) still shifts and
	 * settles, because there the scene genuinely arrives. */
	{
		const wiredMenuDef_t *prev = ( wui_menuStackDepth >= 2 )
		                           ? WiredUI_FindMenu( wui_menuStack[ wui_menuStackDepth - 2 ] )
		                           : NULL;
		const wiredMenuDef_t *cur  = WiredUI_FindMenu( name );
		if ( !prev || !cur || prev->bgPreset != cur->bgPreset )
			WiredUI_NotifyBgTransition();
	}
	wui_focusItem = -1;
	wui_focusedItemPtr = NULL;
	wui_hoveredItemPtr = NULL;
	wui_ix.pressTarget = NULL;
	wui_ix.focusFromKeyboard = qfalse;
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
					Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: started cinematic '%s' (handle %d)\n",
						pushed->cinematic, pushed->cinematicHandle );
				}
			}
		}
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: push menu '%s' (depth %d)\n", name, wui_menuStackDepth );

	// run onOpen script if present
	{
		wiredMenuDef_t *opened = WiredUI_FindMenu( name );
		if ( opened && opened->onOpen[0] ) {
			WiredUI_RunScript( opened, NULL, opened->onOpen );
		}
	}

	// Invariant: a menu on the stack means the UI owns input. The settings nav
	// buttons switch sub-menus with `close ; open` (options.wui:159 et al) run
	// synchronously — `close` pops the stack to empty and WiredUI_PopMenu drops
	// KEYCATCH_UI on that empty-stack edge (below), then this `open` re-fills the
	// stack. Without restoring the catcher here the UI would render (GetActiveMenu
	// reads the stack) but receive no mouse/key events until the next click
	// happened to re-set KEYCATCH_UI via the attract first-input path — the cursor
	// showed but wouldn't move. Re-assert the catcher whenever a menu is pushed so
	// input and the visible menu never disagree. (Idempotent: OR-ing a set bit is
	// a no-op on the normal open path where the catcher is already held.)
	Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );

	/* WiredUI F4 (initial focus): seed keyboard focus on the freshly-opened
	 * menu — the default button if authored, else the first focusable control —
	 * so Enter activates it and Tab has a starting point. Runs AFTER onOpen (so a
	 * script that toggles ui_* visibility has already run and the focusable set is
	 * final) and AFTER the catcher assertion. Menus with no focusable items (HUD
	 * overlays, loading screens, pure-display popups) are left untouched. */
	{
		wiredMenuDef_t *opened2 = WiredUI_FindMenu( name );
		WiredUI_SetInitialFocus( opened2 );
	}
}

void WiredUI_PopMenu( void ) {
	WiredUI_ResetListboxDoubleClick( "pop" );
	WiredUI_ReleaseCompositorPointer( "pop" );
	if ( wui_menuStackDepth <= 0 ) {
		WiredUI_ClearPasswordPromptState( qtrue );
		// nothing to pop — close UI entirely
		wui_activeMenu = UIMENU_NONE;
		wui_focusItem = -1;
		wui_focusedItemPtr = NULL;
		wui_hoveredItemPtr = NULL;
		wui_ix.pressTarget = NULL;
		wui_ix.focusFromKeyboard = qfalse;
		wui_focusFromMouse = qfalse;
		Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
		Cvar_Set( "cl_paused", "0" );
		return;
	}
	if ( !Q_stricmp( wui_menuStack[wui_menuStackDepth - 1], "password" ) )
		WiredUI_ClearPasswordPromptState( qtrue );

	// stop cinematic on the menu being popped (before decrement)
	{
		wiredMenuDef_t *popped = WiredUI_FindMenu( wui_menuStack[wui_menuStackDepth - 1] );
		if ( popped && popped->cinematicHandle >= 0 ) {
			CIN_StopCinematic( popped->cinematicHandle );
			popped->cinematicHandle = -1;
		}
	}

	// Dismissing the error dialog consumes the error, whatever the dismissal
	// path (ESC pop, button close, CloseAllMenus drain). Without this a stale
	// com_errorMessage survives the dismiss and the next
	// CL_Disconnect(showMainMenu) re-surfaces the dialog via the Plan C hook
	// (sticky/empty-popup family, qconsole-9 #3/#5).
	if ( !Q_stricmp( wui_menuStack[wui_menuStackDepth - 1], "error_popup" ) ) {
		Com_ClearLastError();
	}

	wui_menuStackDepth--;
	wui_focusItem = -1;
	wui_focusedItemPtr = NULL;
	wui_hoveredItemPtr = NULL;
	wui_ix.pressTarget = NULL;
	wui_ix.focusFromKeyboard = qfalse;
	wui_focusFromMouse = qfalse;
	wui_tooltipStartTime = 0;
	wui_tooltipFocusItem = -1;
	WiredUI_CloseMultiDropdown();
	if ( wui_sfxMenuClose ) S_StartLocalSound( wui_sfxMenuClose, CHAN_LOCAL_SOUND );

	/* WiredUI F4 (return-focus): restore the control that had focus on the menu
	 * we just uncovered — the caret returns to the button/row that opened the
	 * dialog. wui_returnFocus[wui_menuStackDepth] was stamped in PushMenu at the
	 * matching depth. Validate the saved pointer still belongs to the now-active
	 * menu AND is still focusable (a reload could have rebuilt the item pool);
	 * otherwise leave focus cleared (Tab from the top, as before). Depth 0 is
	 * valid here — GetActiveMenu resolves the root main/ingame menu, so a dialog
	 * opened from the top-level menu still returns focus to its opener. If the
	 * root then closes entirely below (in-game / disconnected-main edge), the
	 * restored focus is simply discarded — harmless. */
	if ( wui_menuStackDepth >= 0 && wui_returnFocus[ wui_menuStackDepth ] ) {
		wiredMenuDef_t *nowTop = WiredUI_GetActiveMenu();
		wiredItemDef_t *saved  = wui_returnFocus[ wui_menuStackDepth ];
		int             fi;
		int             flatCount = 0;
		wiredItemDef_t *flat[ 128 ];
		if ( nowTop ) {
			wui_collect_focusable( nowTop, flat, 128, &flatCount );
			for ( fi = 0; fi < flatCount; fi++ ) {
				if ( flat[ fi ] == saved ) {
					wui_set_focused( nowTop, saved );
					wui_ix.focusFromKeyboard = qtrue;   /* keyboard provenance => ring */
					break;
				}
			}
		}
	}
	wui_returnFocus[ wui_menuStackDepth ] = NULL;

	if ( wui_menuStackDepth <= 0 ) {
		// stack empty — return to root menu behavior
		if ( wui_activeMenu == UIMENU_INGAME ) {
			/* INGAME is an implicit depth-0 root. Reaching zero by popping a
			 * real depth-1 child reveals that root; it must not be confused
			 * with ESC/Close invoked while the root itself is already at zero
			 * (handled by the early branch above). Keep catcher, pause and the
			 * opener focus restored by wui_returnFocus[0]. */
			Key_SetCatcher( Key_GetCatcher() | KEYCATCH_UI );
			Cvar_Set( "cl_paused", "1" );
		}
		else if ( wui_activeMenu == UIMENU_MAIN && CL_ActiveApp()->state == CA_DISCONNECTED ) {
			// Reaching depth 0 with main as the active root means MAIN ITSELF was
			// just dismissed — main is now a real depth-1 stack entry (see the
			// SetActiveMenu(MAIN) promotion), so a sub-menu pop lands at depth>=1 and
			// never gets here; only the final ESC ON MAIN (last pop, 1->0) does.
			// Attract model change (Eser 2026-07-02): ESC at the main menu while
			// disconnected closes the menu and reveals attract again — the main menu
			// and attract are exclusive surfaces, so popping the root hands the screen
			// back to the running attract demo. This is the manual equivalent of the
			// idle attract timeout (Eser 2026-07-05: ESC on main MUST still go to
			// attract). (The legacy model kept UIMENU_MAIN non-closable here, trapping
			// the user in the menu with no way back to a clean attract screen.)
			wui_activeMenu = UIMENU_NONE;
			Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
			// Restart the reel here (Eser 2026-07-03): the menu HID attract while it
			// was up, so returning to it warrants a fresh demo. (Contrast: closing
			// the console over attract does NOT restart — attract kept running under
			// the console the whole time; see cl_keys.c K_CONSOLE handling.)
			Cbuf_ExecuteText( EXEC_APPEND, "attract_restart\n" );
		}
		// (in-game root without the disconnected guard: main menu is not a valid
		//  root while connected, so no other close path is needed.)
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: pop menu (depth %d)\n", wui_menuStackDepth );
}

void WiredUI_CloseAllMenus( void ) {
	qboolean cancelServerStatus = qfalse;

	WiredUI_ClearPasswordPromptState( qtrue );
	WiredUI_ResetListboxDoubleClick( "close-all" );
	WiredUI_ReleaseCompositorPointer( "close-all" );
	// stop all active cinematics on the stack
	for ( int i = 0; i < wui_menuStackDepth; i++ ) {
		wiredMenuDef_t *m = WiredUI_FindMenu( wui_menuStack[i] );
		if ( !Q_stricmp( wui_menuStack[i], "serverinfo" ) ) {
			cancelServerStatus = qtrue;
		}
		if ( m && m->cinematicHandle >= 0 ) {
			CIN_StopCinematic( m->cinematicHandle );
			m->cinematicHandle = -1;
		}
		// Draining a stack that holds the error dialog dismisses it —
		// consume the error, same contract as the WiredUI_PopMenu path.
		if ( !Q_stricmp( wui_menuStack[i], "error_popup" ) ) {
			Com_ClearLastError();
		}
	}
	// A validated Server Info Connect drains the popup rather than running its
	// authored Back/Close action.  Retire the status request exactly once at
	// this ownership boundary, but only after join validation has succeeded and
	// elected to close the stack.  Refused joins keep their popup data intact.
	if ( cancelServerStatus ) {
		WiredFeeder_ServerStatusCancelForCloseAll();
	}
	wui_menuStackDepth = 0;
	memset( wui_returnFocus, 0, sizeof( wui_returnFocus ) );  /* F4: drop saved return-focus */
	wui_focusItem = -1;
	wui_focusedItemPtr = NULL;
	wui_hoveredItemPtr = NULL;
	wui_ix.pressTarget = NULL;
	wui_ix.focusFromKeyboard = qfalse;
	wui_tooltipStartTime = 0;
	wui_tooltipFocusItem = -1;
	wui_activeMenu = UIMENU_NONE;
	WiredUI_CloseMultiDropdown();
	wui_waitingForKey = qfalse;
	wui_bindItem = NULL;
	wui_sliderDragging = qfalse;
	wui_sliderDragItem = NULL;
	wui_listScrollDragItem = NULL;
	WiredUI_CompositorScrollbarDragEnd();
	wui_editingField = qfalse;
	wui_editItem = NULL;
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	Cvar_Set( "cl_paused", "0" );

	/* Machine-readable lifecycle evidence for release behavior gates.  Read
	 * every field back from its authoritative owner after the mutations above;
	 * this is a postcondition observation, not a predicted success message. */
	{
		wiredMenuDef_t *active = WiredUI_GetActiveMenu();
		int catcherUI = ( Key_GetCatcher() & KEYCATCH_UI ) ? 1 : 0;
		int paused = Cvar_VariableIntegerValue( "cl_paused" );
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: close all postcondition depth=%d active=%s catcher_ui=%d paused=%d\n",
			wui_menuStackDepth, active ? active->name : "none", catcherUI, paused );
	}
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
		COM_ERROR( LOG_CH(ch_ui), "Connect error (UI not ready): %s\n",
		            message ? message : "unknown error" );
		return;
	}

	// Automated/headless run: suppress the blocking GUI error dialog (the
	// error has already been logged to stderr via Com_SetLastError). Without
	// this, a smoke/CI run that hits a connect error stalls on an unattended
	// modal nobody can dismiss. Interactive runs (com_automated 0) still get
	// the dialog with its message.
	if ( com_automated && com_automated->integer ) {
		COM_ERROR( LOG_CH(ch_ui), "Connect error (dialog suppressed, automated): %s\n",
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

#ifdef _DEBUG
/* the deep item-by-name walk this test hook needs is already provided by
 * wui_find_item_recursive (defined above, always compiled) — menu->items[]
 * holds only TOP-LEVEL children, so the real controls live nested in
 * children[]/childCount. The former WiredUI_FindItemDeep was a line-for-line
 * duplicate; collapsed onto the shared implementation. */

/* wui_dropdown_test <menuName> <itemName>  — open that menu's ITEM_TYPE_MULTI
 *                                            control's dropdown popup.
 * wui_dropdown_test --dismiss               — close it.
 * Dev/test hook so a headless smoke can pixel-verify the floating dropdown
 * panel render without an interactive mouse click. Drives the SAME singleton
 * open state as the click handler (WiredUI_OpenMultiDropdown); no new render
 * logic. _DEBUG-only. */
static void WiredUI_DropdownTest_f( void ) {
	const char     *menuName, *itemName;
	wiredMenuDef_t *menu;
	wiredItemDef_t *item;
	int             mi;

	if ( Cmd_Argc() >= 2 &&
	     ( !Q_stricmp( Cmd_Argv( 1 ), "--dismiss" ) || !Q_stricmp( Cmd_Argv( 1 ), "-d" ) ) ) {
		WiredUI_CloseMultiDropdown();
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "wui_dropdown_test: dismissed\n" );
		return;
	}
	if ( Cmd_Argc() < 3 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"Usage: wui_dropdown_test <menuName> <itemName>\n"
			"       wui_dropdown_test --dismiss\n" );
		return;
	}
	menuName = Cmd_Argv( 1 );
	itemName = Cmd_Argv( 2 );

	menu = WiredUI_FindMenu( menuName );
	if ( !menu ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui), "wui_dropdown_test: menu '%s' not found\n", menuName );
		return;
	}
	item = NULL;
	for ( mi = 0; mi < menu->itemCount && !item; mi++ ) {
		item = wui_find_item_recursive( menu->items[mi], itemName );
	}
	if ( !item ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui), "wui_dropdown_test: item '%s' not in menu '%s' (searched tree)\n", itemName, menuName );
		return;
	}
	if ( item->type != ITEM_TYPE_MULTI ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_dropdown_test: item '%s' is type %d, not ITEM_TYPE_MULTI\n", itemName, item->type );
		return;
	}
	/* the menu must be on the stack for the dropdown rect/render to resolve
	 * against it; push it if it isn't already the top. */
	if ( wui_menuStackDepth == 0 ||
	     Q_stricmp( wui_menuStack[ wui_menuStackDepth - 1 ], menuName ) != 0 ) {
		WiredUI_PushMenu( menuName );
	}
	if ( WiredUI_OpenMultiDropdown( item ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_dropdown_test: opened dropdown '%s.%s' (open=%d hover=%d scroll=%d)\n",
			menuName, itemName, wui_multiDropdownOpen, wui_multiDropdownHover, wui_multiDropdownScroll );
	} else {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"wui_dropdown_test: '%s.%s' has no options (not opened)\n", menuName, itemName );
	}
}

/* wui_scores_test [1|0|--dismiss]  — hold (1) / release (0) the scoreboard.
 * Dev/test hook so a headless smoke can pixel-verify the V2 scoreboard chrome
 * without an interactive TAB-hold. Issues the cgame's "+scores"/"-scores"
 * button command through the normal console→cgame route (the same path a
 * keybind takes); the cgame sets cg.showScores in its VM and the bridge
 * restages it into wiredHud->showScores each frame, flipping the gametype
 * scoreboard menu visible via the engine name-gate. Drives existing cgame
 * state only — no VM-memory poke, no new render logic. _DEBUG-only. */
static void WiredUI_ScoresTest_f( void ) {
	qboolean down = qtrue;

	if ( Cmd_Argc() >= 2 ) {
		const char *a = Cmd_Argv( 1 );
		if ( !Q_stricmp( a, "0" ) || !Q_stricmp( a, "--dismiss" ) || !Q_stricmp( a, "-d" ) ) {
			down = qfalse;
		}
	}

	/* Dispatch synchronously (EXEC_NOW) so the cgame's CG_CONSOLE_COMMAND
	 * runs +scores/-scores immediately while the client is connected and the
	 * cgame VM is live — same tokenize+dispatch path a keybind/console line
	 * takes, reaching CG_ScoresDown_f. EXEC_APPEND would queue it behind the
	 * remaining startup +cmd tokens and (observed) never reach the handler in
	 * the headless +arg-driven flow. */
	Cbuf_ExecuteText( EXEC_NOW, down ? "+scores\n" : "-scores\n" );
	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_scores_test: issued %s to cgame\n", down ? "+scores" : "-scores" );
}
#endif

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

/* Depth-first hit-test descending into flex containers. Flexbox-first
 * menus produce a single top-level container wrapping all actionable
 * leaves, so a top-level-only walk would return NULL and mouseEnter /
 * Exit / click never dispatch. Layout-only containers descend; action-
 * bearing containers ARE hover targets (mirrors the focusable collector
 * — their nested decoration children compose the row but the container
 * itself takes the click). Back-to-front iteration preserves topmost-
 * wins semantics. Clip-scroll skip matches the gate at the menu level. */
static wiredItemDef_t *wui_find_item_at_cursor_recursive(
	wiredItemDef_t *item, float cx, float cy, float sy,
	float clipTop, float clipBottom )
{
	int i;
	if ( !item ) return NULL;

	/* Container with action[] populated is itself the hover target —
	 * mirrors the focusable collector's container-action rule. The
	 * container's bounding box is computed by Clay from its children;
	 * fall through to the rect test below using the container's own
	 * resolvedRect (or skip if resolvedRect is degenerate — that's
	 * the flex-container 0,0 case which the Clay compositor hover
	 * path covers via Clay_GetPointerOverIds). */
	if ( ( item->isFlexContainer || item->childCount > 0 ) && !item->action[0] ) {
		for ( i = item->childCount - 1; i >= 0; i-- ) {
			wiredItemDef_t *hit = wui_find_item_at_cursor_recursive(
				item->children[ i ], cx, cy, sy, clipTop, clipBottom );
			if ( hit ) return hit;
		}
		return NULL;
	}

	if ( !WiredUI_ItemCanFocus( item ) ) return NULL;

	{
		wiredRect_t absRect;
		absRect.x = item->resolvedRect.x;
		absRect.y = item->resolvedRect.y - sy;
		absRect.w = item->resolvedRect.w;
		absRect.h = item->resolvedRect.h;

		/* Skip items scrolled out of view (same condition as the legacy
		 * top-level loop). */
		if ( absRect.y + absRect.h < clipTop || absRect.y > clipBottom ) {
			return NULL;
		}
		if ( WiredUI_PointInRect( cx, cy, &absRect ) ) return item;
	}
	return NULL;
}

static wiredItemDef_t *WiredUI_FindItemAtCursor( wiredMenuDef_t *menu, float cx, float cy ) {
	// Layout is already resolved by the render loop (WUI_LayoutMenu called each frame)
	float menuH = menu->resolvedRect.h;
	float oy = menu->resolvedRect.y;
	float sy = menu->scrollOffset;
	float clipTop = oy;
	float clipBottom = clipTop + menuH;
	int   i;

	// iterate back-to-front so topmost item wins; descend into each via the
	// recursive helper so nested leaves are reachable.
	for ( i = menu->itemCount - 1; i >= 0; i-- ) {
		wiredItemDef_t *hit = wui_find_item_at_cursor_recursive(
			menu->items[ i ], cx, cy, sy, clipTop, clipBottom );
		if ( hit ) return hit;
	}
	return NULL;
}

/* ── listbox keyboard row-nav helpers ───────────────────────────────────
 *
 * Shared by the vertical UP/DOWN/HOME/END/PAGE row-nav and the type-to-jump
 * search. The visible-row count is computed dpi-consistently with the render
 * + click hit-test (cl_wired_clay.c wui_clay_emit_listbox uses the same
 * charSize x WUI_LINE_HEIGHT_FACTOR x dpi row height), so the page-step and
 * keep-in-view scroll agree with what is actually drawn on HiDPI. */
static char wui_ascii_lower( char c )
{
	return ( c >= 'A' && c <= 'Z' ) ? (char)( c + 32 ) : c;
}

static int wui_listbox_visible_rows( const wiredItemDef_t *item )
{
	float dpi       = WiredUI_GetDpiScale();
	float charSize  = item->fontPointSize > 0.0f ? item->fontPointSize : WUI_DEFAULT_FONT_SIZE;
	float rowHFloor = charSize * WUI_LINE_HEIGHT_FACTOR * dpi;
	float rowH      = item->elementheight > 0 ? item->elementheight * dpi : rowHFloor;
	float headerH   = WiredUI_ListboxHeaderHeight( (wiredItemDef_t *)item );
	int   vis;
	if ( rowH < rowHFloor ) rowH = rowHFloor;
	vis = ( rowH > 0.0f ) ? (int)( ( item->rect.h - headerH ) / rowH ) : 1;
	return vis < 1 ? 1 : vis;
}

/* Move a vertical listbox's selection to `sel`, clamp to [0,total), update the
 * feeder selection, keep the row in view, and play the focus sfx if the
 * selection actually changed. Mirrors the horizontal-listbox LEFT/RIGHT logic
 * on the vertical axis so both axes agree. */
static void wui_listbox_set_selection( wiredItemDef_t *item, int sel )
{
	int total   = WiredUI_FeederCount( (int)item->feeder );
	int visible = wui_listbox_visible_rows( item );

	if ( total <= 0 ) return;
	if ( sel < 0 )      sel = 0;
	if ( sel >= total ) sel = total - 1;

	if ( sel != item->listSelectedRow ) {
		item->listSelectedRow = sel;
		WiredUI_FeederSelection( (int)item->feeder, sel );
		if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
	}

	/* keep the selection visible: scroll if it left the window */
	if ( sel < item->listScrollOffset ) {
		item->listScrollOffset   = sel;
		item->listScrollFadeTime = cls.realtime;
	} else if ( sel >= item->listScrollOffset + visible ) {
		item->listScrollOffset   = sel - visible + 1;
		item->listScrollFadeTime = cls.realtime;
	}
	if ( item->listScrollOffset < 0 ) item->listScrollOffset = 0;
}

/* Type-to-jump: fold `ch` into the type-ahead buffer (bound to `item`) and
 * jump the selection to the first row whose column-0 text starts with the
 * buffer. A pause longer than WUI_TYPEAHEAD_RESET_MS, or a focus change to a
 * different listbox, resets the buffer first. Returns qtrue if the key was
 * consumed as a search character. */
static qboolean wui_listbox_typeahead( wiredItemDef_t *item, int ch )
{
	int  total, i;
	char lc;

	if ( !item || item->feeder == 0 ) return qfalse;
	/* printable ASCII only; skip SPACE so it stays a pure ACTIVATE key on a
	 * focused listbox (K_SPACE keycode already toggles/activates) rather than
	 * seeding a leading-space search that can never match a row label. */
	if ( ch <= 32 || ch > 126 ) return qfalse;

	/* reset the buffer on target change or timeout */
	if ( wui_ix.typeTarget != item ||
	     ( cls.realtime - wui_ix.typeLastMs ) > WUI_TYPEAHEAD_RESET_MS ) {
		wui_ix.typeTarget = item;
		wui_ix.typeLen    = 0;
		wui_ix.typeBuf[0] = '\0';
	}
	wui_ix.typeLastMs = cls.realtime;

	lc = wui_ascii_lower( (char) ch );
	if ( wui_ix.typeLen < (int)sizeof( wui_ix.typeBuf ) - 1 ) {
		wui_ix.typeBuf[ wui_ix.typeLen++ ] = lc;
		wui_ix.typeBuf[ wui_ix.typeLen ]   = '\0';
	}

	/* find the first row whose column-0 text starts with the buffer (case-
	 * insensitive, skipping Quake ^colour codes so "^1Foo" matches "foo"). */
	total = WiredUI_FeederCount( (int)item->feeder );
	for ( i = 0; i < total; i++ ) {
		const char *raw = WiredUI_FeederItemText( (int)item->feeder, i, 0 );
		const char *p   = raw;
		int         j   = 0;
		if ( !raw ) continue;
		while ( *p && j < wui_ix.typeLen ) {
			if ( Q_IsColorString( p ) ) { p += 2; continue; }
			if ( wui_ascii_lower( *p ) != wui_ix.typeBuf[ j ] ) break;
			p++; j++;
		}
		if ( j == wui_ix.typeLen ) {
			wui_listbox_set_selection( item, i );
			return qtrue;
		}
	}
	/* no match - keep the buffer (so a following key can still narrow) but the
	 * key is still "consumed" so it doesn't fall through to the default switch
	 * (a printable key has no other listbox meaning). */
	return qtrue;
}

// ── key event ─────────────────────────────────────────────────────────
// Combines: v6 Menu_HandleKey structure, legacy q3_ui key dispatch,
// and ET:Legacy per-item events (onEsc, onEnter, onTab, execKey).

void WiredUI_KeyEvent( int key, qboolean down ) {
	wiredMenuDef_t *menu;
	wiredItemDef_t *focusedItem = NULL;

	if ( !wui_initialized ) return;

	// notify attract scheduler — any key stops attract
	if ( down ) WiredAttract_NoteInput( key );

	/* forward mouse-button + wheel +
	 * Tab events to the compositor's hit-test layer so it can track down-
	 * target, click resolution, Clay scroll-container updates, and focus
	 * cycle IN PARALLEL with the legacy dispatch below. The compositor's
	 * action-fire + focus-authority gates are OFF for now, so this
	 * runs as bookkeeping only — no behaviour change. */
	if ( key == K_MOUSE1 ) {
		(void) WiredUI_CompositorMouseButton( down );
		wui_compositorPointerDown = down;
	} else if ( down && key == K_MWHEELUP ) {
		/* Consume the wheel when it lands on a scroll viewport so it scrolls
		 * the content instead of falling through to legacy menu nav. */
		if ( WiredUI_CompositorMouseWheel( -1.0f ) ) return;
	} else if ( down && key == K_MWHEELDOWN ) {
		if ( WiredUI_CompositorMouseWheel(  1.0f ) ) return;
	} else if ( down && key == K_TAB ) {
		qboolean forward = !( keys[ K_SHIFT ].down );
		(void) WiredUI_CompositorTabFocus( forward );
	}

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
		if ( !WiredUI_GetMultiDropdownRect( menu, wui_multiDropdownItem, &opts,
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

		if ( WiredUI_IsSecureJoinPasswordItem( wui_editItem ) ) {
			const wiredItemDef_t *hovered = WiredUI_CompositorHoveredItem();
			/* A real primary-pointer press changes interaction ownership.  End
			 * secure editing but retain the prompt secret long enough for the
			 * normal hovered-item path below to run its authored action.  In
			 * particular, a visible Cancel click must wipe+pop in this same event
			 * instead of being swallowed by the hidden editor. A click on the edit
			 * item itself remains owned and consumed by the secure editor. */
			if ( key == K_MOUSE1 && hovered && hovered != wui_editItem
			     && WiredUI_ItemAcceptsMouseHover( (wiredItemDef_t *)hovered ) ) {
				wui_editingField = qfalse;
				wui_editItem = NULL;
				wui_editCursorPos = 0;
				goto password_edit_pointer_fallthrough;
			} else {
				WiredUI_HandleSecureJoinEditKey( key );
				return;
			}
		}

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
			// NOLINTNEXTLINE(misc-redundant-expression) — kept as `'a' - 'a' + 1` to match the ctrl-key formula style of nearby checks (ctrl-letter = letter - 'a' + 1)
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
					int space;
					// maxChars is parsed verbatim from the .wui with no upper bound —
					// clamp it to the real buffer so a large value can't drive the
					// memmove/memcpy below past buff[1024].
					if ( maxC > (int)sizeof( buff ) - 1 ) maxC = (int)sizeof( buff ) - 1;
					space = maxC - len;
					if ( pasteLen > space ) pasteLen = space;
					if ( pasteLen > 0 ) {
						memmove( &buff[wui_editCursorPos + pasteLen], &buff[wui_editCursorPos], len + 1 - wui_editCursorPos );
						// NOLINTNEXTLINE(bugprone-not-null-terminated-result) — buff is pre-NUL-terminated past the paste range by the memmove above; pasteLen is bounded
						memcpy( &buff[wui_editCursorPos], cbd, pasteLen );
						wui_editCursorPos += pasteLen;
						WiredUI_StateSetString( wui_editItem->cvar, buff );
					}
					Z_Free( cbd );
				}
			} else if ( ch >= 32 ) {
				// printable character — insert at cursor
				int maxC = wui_editItem->maxChars > 0 ? wui_editItem->maxChars : 255;
				// maxChars is unclamped .wui input; bound it to the buffer so the
				// insert can't write past buff[1024] (see paste path above).
				if ( maxC > (int)sizeof( buff ) - 1 ) maxC = (int)sizeof( buff ) - 1;
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

password_edit_pointer_fallthrough:

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

	/* Mouse-click focus adoption: a K_MOUSE1 press adopts the hovered item as
	 * the focused item, so the focusedItem-driven interactions (slider drag,
	 * spinner +/- buttons) fire on a click — not only after keyboard nav.
	 * Without this, wui_focusedItemPtr tracks ONLY keyboard focus, so clicking
	 * a spinner or slider never reaches its focusedItem branch below and the
	 * click is a no-op (the reported "-/+ buttons + sliders don't work"). */
	if ( down && key == K_MOUSE1 ) {
		const wiredItemDef_t *hov = WiredUI_CompositorHoveredItem();
		/* Adopt the clicked item ONLY through the SAME strict filter the hover path
		 * uses (WiredUI_ItemAcceptsMouseHover), not the weaker ItemCanFocus. The
		 * weak filter passes a childful, script-less full-span container (e.g.
		 * main_root, rect 0 0 1 1), so an empty-background click adopted it as the
		 * focused item and the focus fill then painted across the whole screen. The
		 * strict filter rejects such layout-only containers (and non-actionable
		 * text) while still accepting real interactive items — buttons/sliders/list
		 * rows carry action/onFocus and pass — so clicking empty space now adopts
		 * nothing, matching hover. */
		if ( hov && WiredUI_ItemAcceptsMouseHover( (wiredItemDef_t *) hov ) ) {
			wui_focusedItemPtr        = (wiredItemDef_t *) hov;
			wui_ix.focusFromKeyboard  = qfalse;
		}
	}

	/* prefer the authoritative pointer (may reference a
	 * nested child). Legacy top-level index is the fallback for mouse +
	 * listbox-click paths that still set wui_focusItem directly. */
	if ( wui_focusedItemPtr && WiredUI_ItemCanFocus( wui_focusedItemPtr ) ) {
		focusedItem = wui_focusedItemPtr;
	} else if ( wui_focusItem >= 0 && wui_focusItem < menu->itemCount ) {
		focusedItem = menu->items[wui_focusItem];
		if ( !WiredUI_ItemCanFocus( focusedItem ) ) {
			focusedItem = NULL;
		}
	}
	if ( down && key == K_MOUSE1 && wui_listboxClickLatch.valid
	     && ( menu != wui_listboxClickLatch.menu
	       || focusedItem != wui_listboxClickLatch.item ) ) {
		WiredUI_ResetListboxDoubleClick( "different-target" );
	}
	if ( !down && key == K_MOUSE1 && wui_listboxClickLatch.valid ) {
		if ( menu == wui_listboxClickLatch.menu
		     && focusedItem == wui_listboxClickLatch.item ) {
			wui_listboxClickLatch.releaseObserved = qtrue;
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
				"WiredUI: listbox click phase=release input=K_MOUSE1 menu=%s item=%s feeder=%d row=%d list_generation=%d pointer_down=%d\n",
				menu->name, focusedItem->name, wui_listboxClickLatch.feeder,
				wui_listboxClickLatch.row, wui_listboxClickLatch.listGeneration,
				wui_compositorPointerDown ? 1 : 0 );
		} else {
			WiredUI_ResetListboxDoubleClick( "release-target-change" );
		}
	}

	// Unbind the highlighted keybind row with Del/Backspace while browsing the
	// list (NOT in "press a key" capture mode — that path is handled above).
	// Standard keybind-list behavior: select a row, hit Del → clears its binding.
	if ( down && focusedItem && focusedItem->type == ITEM_TYPE_BIND &&
	     focusedItem->cvar[0] &&
	     ( key == K_BACKSPACE || key == K_DEL || key == K_KP_DEL ) ) {
		Cbuf_ExecuteText( EXEC_APPEND, va( "unbindcmd \"%s\"\n", focusedItem->cvar ) );
		return;
	}

	// slider drag: release mouse button ends drag
	if ( !down && ( key == K_MOUSE1 ) && wui_sliderDragging ) {
		wui_sliderDragging = qfalse;
		wui_sliderDragItem = NULL;
	}

	/* scrollbar drag: release mouse button ends any in-progress thumb drag
	 * (flex scroll-container thumb, or listbox thumb). Handled before the
	 * "only process key-down" gate so the release is not dropped. */
	if ( !down && ( key == K_MOUSE1 ) ) {
		if ( WiredUI_CompositorScrollbarDragging() ) WiredUI_CompositorScrollbarDragEnd();
		if ( wui_listScrollDragItem ) wui_listScrollDragItem = NULL;
	}

	// spinner click-and-hold: release ends the auto-repeat + clears press paint
	if ( !down && ( key == K_MOUSE1 ) && wui_spinnerHoldItem ) {
		wui_spinnerHoldItem = NULL;
		wui_spinnerHoldDir  = 0;
		if ( wui_ix.pressTarget && wui_ix.pressKey == K_MOUSE1 ) {
			wui_ix.pressTarget = NULL;
			wui_ix.pressKey    = 0;
		}
	}

	// only process key-down for most actions
	if ( !down ) return;

	/* SCROLLBAR THUMB GRAB (mouse-down): a click on a scrollbar thumb starts a
	 * drag instead of the normal click/focus path. Checked here — after the
	 * key-down gate, before any item hit-test — so the thumb wins over whatever
	 * sits beneath it. Flex scroll-container thumb first (its geometry is cached
	 * in cl_wired_clay.c), then the vertical listbox thumb (single-source
	 * WiredUI_ListboxScrollbarGeom over the item's Clay-rendered rect). Both use
	 * physical-px screen coords (wui_cursorX/Y). Consuming (return) suppresses
	 * the row-select / action that a body click would otherwise fire. */
	if ( key == K_MOUSE1 ) {
		if ( WiredUI_CompositorScrollbarDragStart( wui_cursorX, wui_cursorY ) ) {
			return;
		}
		/* Listbox thumb: probe every listbox on the active menu — including those
		 * nested in flex containers (the settings-panel shell) — via the recursive
		 * thumb hit-test. The grab is not limited to the focused item; the cursor
		 * may be over an unfocused list's thumb. */
		{
			int             li;
			float           grabDY = 0.0f;
			wiredItemDef_t *lb     = NULL;
			for ( li = 0; li < menu->itemCount && !lb; li++ ) {
				lb = wui_listbox_thumb_at( menu, menu->items[ li ],
				                           wui_cursorX, wui_cursorY, &grabDY );
			}
			if ( lb ) {
				wui_listScrollDragItem = lb;
				wui_listScrollGrabDY   = grabDY;
				lb->listScrollFadeTime = cls.realtime;
				return;
			}
		}
	}

	/* TYPE-TO-JUMP: a printable key (K_CHAR_FLAG) with a focused vertical
	 * listbox jumps the selection to the first row whose text starts with the
	 * accumulated prefix. Placed after the edit-field gate above (an active
	 * editfield already returned), so it only fires when a listbox — not a text
	 * control — holds focus. Char keys never collide with the execKey / onEsc /
	 * onEnter / nav handling below (those key off non-char keycodes). */
	if ( ( key & K_CHAR_FLAG ) && focusedItem
	     && focusedItem->type == ITEM_TYPE_LISTBOX
	     && !focusedItem->horizontalScroll && focusedItem->feeder != 0 ) {
		if ( wui_listbox_typeahead( focusedItem, key & ~K_CHAR_FLAG ) ) {
			return;
		}
	}

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
		/* component-library F1 (Table A: K_SPACE => ACTIVATE): Space activates
		 * the keyboard-focused control exactly like Enter — toggles yesno/
		 * checkbox, opens multi dropdowns, steps sliders, and runs action[].
		 * Additive: no existing binding changes; Space was previously inert on
		 * focused items. Slider/multi sub-branches key off K_MOUSE1/K_MOUSE2, so
		 * Space takes the same "enter" path they already handle. */
		case K_SPACE:
			{
				qboolean openedDropdown = qfalse;
			if ( !focusedItem ) {
				/* WiredUI F4 (default button): Enter/Space with nothing focused
				 * still confirms a dialog — fire the authored default button's
				 * action. Keyboard only (mouse click resolves its own hit-target);
				 * gated to K_ENTER/K_KP_ENTER/K_SPACE so a stray mouse event here
				 * never auto-fires. */
				if ( key == K_ENTER || key == K_KP_ENTER || key == K_SPACE ) {
					wiredItemDef_t *def = wui_find_default_button( menu );
					if ( def && def->action[0] ) {
						WiredUI_RunScript( menu, def, def->action );
					}
				}
				break;
			}

			// EDITFIELD items: click to start editing
			if ( ( focusedItem->type == ITEM_TYPE_EDITFIELD || focusedItem->type == ITEM_TYPE_NUMERICFIELD )
			     && focusedItem->cvar[0] ) {
				char buf[256];
				wui_editingField = qtrue;
				wui_editItem = focusedItem;
				if ( WiredUI_IsSecureJoinPasswordItem( focusedItem ) )
					Q_strncpyz( buf, wui_passwordPrompt.secret, sizeof( buf ) );
				else
					WiredUI_StateGetString( focusedItem->cvar, buf, sizeof( buf ) );
				wui_editCursorPos = strlen( buf );
				Q_SecureZeroMemory( buf, sizeof( buf ) );
				wui_editPaintOffset = 0;
				break;
			}

			// LISTBOX items: click to select row (or column, in horizontal mode)
			if ( focusedItem->type == ITEM_TYPE_LISTBOX && focusedItem->feeder != 0 ) {
				if ( key != K_MOUSE1 ) {
					const char *input = key == K_ENTER ? "K_ENTER"
						: key == K_KP_ENTER ? "K_KP_ENTER"
						: key == K_SPACE ? "K_SPACE" : "non-primary-pointer";
					/* Keyboard activation consumes the already-authoritative
					 * listSelectedRow established by navigation. It never re-hit-tests
					 * the cursor, re-calls the feeder, or participates in double-click. */
					WiredUI_ResetListboxDoubleClick( "non-primary-pointer" );
					Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
						"WiredUI: listbox activate input=%s menu=%s item=%s feeder=%d row=%d\n",
						input, menu->name, focusedItem->name,
						(int)focusedItem->feeder, focusedItem->listSelectedRow );
					if ( focusedItem->action[0]
					     && ( key == K_ENTER || key == K_KP_ENTER ) ) {
						if ( wui_sfxAction ) S_StartLocalSound( wui_sfxAction, CHAN_LOCAL_SOUND );
						WiredUI_RunScript( menu, focusedItem, focusedItem->action );
					}
					break;
				}
				int   clickedRow;
				int   total = WiredUI_FeederCount( (int)focusedItem->feeder );

				/* Prefer the ACTUAL Clay-rendered rect over the legacy
				 * WUI_LayoutMenu resolvedRect. The listbox is emitted by
				 * Clay flex (cl_wired_clay.c wui_clay_emit_listbox, ATTACH_TO_NONE
				 * inside a flex parent), so its true on-screen origin is Clay's
				 * layout, NOT resolvedRect — rows draw at clayRect.y + k*rowH.
				 * Using the legacy rect.y/x mapped clicks to the wrong/no row
				 * (same legacy-vs-Clay divergence as the multidropdown-anchor
				 * fix). Fall back to resolvedRect for the pre-Clay layout case. */
				wuiPixelRect_t clayLB;
				qboolean       haveClay = WiredUI_ClayItemRenderedRect( menu, focusedItem, &clayLB );

				if ( focusedItem->horizontalScroll ) {
					float colW     = focusedItem->elementwidth > 0 ? focusedItem->elementwidth : 64.0f;
					float listAbsX;
					if ( haveClay ) {
						listAbsX = clayLB.x;
					} else {
						float menuOX = menu->fullscreen ? 0 : menu->rect.x;
						listAbsX = menuOX + focusedItem->rect.x;
					}
					clickedRow = (int)( ( wui_cursorX - listAbsX ) / colW ) + focusedItem->listScrollOffset;
				} else {
					/* Match the listbox render rowH (cl_wired_clay.c emit_listbox):
					 * rows are scaled by dpiScale, so an authored elementheight and
					 * the derived glyph-line floor both multiply by dpi — else on
					 * HiDPI the click maps to the wrong row. */
					float dpi      = WiredUI_GetDpiScale();
					float charSize = focusedItem->fontPointSize > 0.0f ? focusedItem->fontPointSize : WUI_DEFAULT_FONT_SIZE;
					float rowHFloor = charSize * WUI_LINE_HEIGHT_FACTOR * dpi;
					float rowH     = focusedItem->elementheight > 0 ? focusedItem->elementheight * dpi : rowHFloor;
					float listAbsY, listAbsX, listW;
					float headerH  = WiredUI_ListboxHeaderHeight( focusedItem );
					if ( rowH < rowHFloor ) rowH = rowHFloor;
					if ( haveClay ) {
						/* Clay coords are absolute screen px with the panel scroll
						 * already baked into the layout — do NOT re-subtract
						 * menu->scrollOffset here (the Clay path in the dropdown
						 * anchor fix documents the same). */
						listAbsY = clayLB.y;
						listAbsX = clayLB.x;
						listW    = clayLB.w;
					} else {
						float menuOX = menu->fullscreen ? 0 : menu->rect.x;
						float menuOY = menu->fullscreen ? 0 : menu->rect.y;
						listAbsY = menuOY + focusedItem->rect.y - menu->scrollOffset;
						listAbsX = menuOX + focusedItem->rect.x;
						listW    = focusedItem->rect.w;
					}

					/* Header band click: the engine-drawn header (columnHeaders)
					 * occupies the top headerH of the listbox. A click there maps
					 * cursorX->column (same wui_listbox_column_geom model the header
					 * is drawn with) and fires "<columnSort> <col>", so the header
					 * and body share one column model AND the sort stays clickable. */
					if ( headerH > 0.0f &&
					     wui_cursorY >= listAbsY && wui_cursorY < listAbsY + headerH ) {
						WiredUI_ResetListboxDoubleClick( "header" );
						if ( focusedItem->columnSortCmd[0] ) {
							int hcol = WiredUI_ListboxHeaderColumnAtX( focusedItem,
							                                           listAbsX, listW, wui_cursorX );
							if ( hcol >= 0 ) {
								if ( wui_sfxAction ) S_StartLocalSound( wui_sfxAction, CHAN_LOCAL_SOUND );
								/* MapSort/ServerSort are WiredUI script commands (not console
								 * cmds), so dispatch through the wui script runner. */
								WiredUI_RunScript( menu, focusedItem,
									va( "%s %d", focusedItem->columnSortCmd, hcol ) );
							}
						}
						break;   /* header consumed the click - no row select */
					}

					/* Body rows begin below the header band. */
					clickedRow = (int)( ( wui_cursorY - listAbsY - headerH ) / rowH ) + focusedItem->listScrollOffset;
				}

				if ( clickedRow >= 0 && clickedRow < total ) {
					int rawIndex = -1;
					int source = -1;
					int listGeneration = WiredUI_ListboxIdentityGeneration( focusedItem );
					int selectionGeneration = -1;
					int elapsed;
					qboolean identityValid = qtrue;

					focusedItem->listSelectedRow = clickedRow;
					WiredUI_FeederSelection( (int)focusedItem->feeder, clickedRow );

					if ( (int)focusedItem->feeder == FEEDER_SERVERS ) {
						identityValid = WiredFeeder_GetSelectedServerIdentity( clickedRow,
							&rawIndex, &source, &listGeneration, &selectionGeneration );
					}
					if ( !identityValid ) {
						WiredUI_ResetListboxDoubleClick( "selection-invalid" );
						break;
					}

					elapsed = cls.realtime - wui_listboxClickLatch.time;
					if ( wui_listboxClickLatch.valid ) {
						if ( menu != wui_listboxClickLatch.menu
						     || focusedItem != wui_listboxClickLatch.item
						     || (int)focusedItem->feeder != wui_listboxClickLatch.feeder ) {
							WiredUI_ResetListboxDoubleClick( "different-target" );
						} else if ( clickedRow != wui_listboxClickLatch.row ) {
							WiredUI_ResetListboxDoubleClick( "different-row" );
						} else if ( listGeneration != wui_listboxClickLatch.listGeneration ) {
							WiredUI_ResetListboxDoubleClick( "generation-change" );
						} else if ( elapsed < 0 || elapsed >= WIRED_DOUBLECLICK_TIME ) {
							WiredUI_ResetListboxDoubleClick( "timeout" );
						} else if ( !wui_listboxClickLatch.releaseObserved ) {
							WiredUI_ResetListboxDoubleClick( "missing-release" );
						} else if ( focusedItem->doubleClick[0] ) {
							/* Consume before the action: JoinServer closes all menus, and a
							 * lifecycle reset must not observe stale item pointers. */
							wui_listboxClickLatch.valid = qfalse;
							Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
								"WiredUI: listbox click phase=double input=K_MOUSE1 menu=%s item=%s feeder=%d row=%d raw=%d source=%d list_generation=%d selection_generation=%d elapsed=%d\n",
								menu->name, focusedItem->name, (int)focusedItem->feeder,
								clickedRow, rawIndex, source, listGeneration,
								selectionGeneration, elapsed );
							WiredUI_RunScript( menu, focusedItem, focusedItem->doubleClick );
							break;
						}
					}

					wui_listboxClickLatch.valid = qtrue;
					wui_listboxClickLatch.menu = menu;
					wui_listboxClickLatch.item = focusedItem;
					Q_strncpyz( wui_listboxClickLatch.menuName, menu->name,
						sizeof( wui_listboxClickLatch.menuName ) );
					Q_strncpyz( wui_listboxClickLatch.itemName, focusedItem->name,
						sizeof( wui_listboxClickLatch.itemName ) );
					wui_listboxClickLatch.feeder = (int)focusedItem->feeder;
					wui_listboxClickLatch.row = clickedRow;
					wui_listboxClickLatch.listGeneration = listGeneration;
					wui_listboxClickLatch.time = cls.realtime;
					wui_listboxClickLatch.releaseObserved = qfalse;
					Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
						"WiredUI: listbox click phase=armed input=K_MOUSE1 menu=%s item=%s feeder=%d row=%d raw=%d source=%d list_generation=%d selection_generation=%d\n",
						menu->name, focusedItem->name, (int)focusedItem->feeder,
						clickedRow, rawIndex, source, listGeneration,
						selectionGeneration );
				} else {
					WiredUI_ResetListboxDoubleClick( "invalid-row" );
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
					case ITEM_TYPE_CHECKBOX:
						/* toggle 0 <-> 1. Checkbox binds a 0/1 cvar exactly like
						 * yesno; ACTIVATE (mouse1 / Enter / Space) flips it. The
						 * framework provides focus/hover/ring/disabled around this. */
					WiredUI_StateSetString( focusedItem->cvar, atof( cvarBuf ) != 0 ? "0" : "1" );
						break;

					case ITEM_TYPE_RADIOBUTTON:
						/* Segmented control. A left-click selects the segment under
						 * the cursor (hit-test via the ACTUAL Clay-rendered rect +
						 * the shared WiredUI_RadioSegmentAtX geometry, so the
						 * clickable zone matches the drawn segment exactly).
						 * Enter/Space/right-click with no cursor hit cycles forward
						 * to the next option (keyboard/gamepad activation). */
						{
							int seg = -1;
							if ( key == K_MOUSE1 ) {
								wuiPixelRect_t rr;
								if ( WiredUI_ClayItemRenderedRect( menu, focusedItem, &rr ) ) {
									seg = WiredUI_RadioSegmentAtX( focusedItem, rr.x, rr.w, wui_cursorX );
								}
							}
							if ( seg >= 0 ) {
								WiredUI_RadioSelectIndex( focusedItem, seg );
							} else {
								int cnt = WiredUI_RadioSegmentCount( focusedItem );
								int cur = WiredUI_RadioSelectedIndex( focusedItem );
								if ( cnt > 0 ) {
									int next = ( cur < 0 ) ? 0 : ( cur + 1 ) % cnt;
									WiredUI_RadioSelectIndex( focusedItem, next );
								}
							}
							if ( focusedItem->action[0] ) {
								WiredUI_RunScript( menu, focusedItem, focusedItem->action );
							}
						}
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
							if ( WiredUI_OpenMultiDropdown( focusedItem ) ) {
								openedDropdown = qtrue;
							}
						}
						break;

					case ITEM_TYPE_SLIDER:
						if ( key == K_MOUSE1 ) {
							/* mouse1 click: start drag + set value by click position.
							 * Hit geometry comes from the SAME single-source track rect
							 * the render draws the thumb from (WiredUI_SliderTrackGeom,
							 * Clay-rendered px) — never the legacy resolvedRect.x/.w*0.45,
							 * which does not match the flex-positioned track at HiDPI and
							 * was the drag-misalignment bug. Fallback to the legacy math
							 * only before the first layout pass. */
							float range = focusedItem->sliderData.maxVal - focusedItem->sliderData.minVal;
							float barX, barW;
							if ( !WiredUI_SliderTrackGeom( focusedItem, &barX, &barW, NULL, NULL ) ) {
								float menuOX = menu->fullscreen ? 0 : menu->rect.x;
								float absItemX = menuOX + focusedItem->rect.x;
								barX = absItemX + focusedItem->rect.w * 0.5f;
								barW = focusedItem->rect.w * 0.45f;
							}
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

					case ITEM_TYPE_SPINNER:
						if ( key == K_MOUSE1 ) {
							/* Hit-test the -/+ buttons via their single-source Clay
							 * rects (draw==hit). A click on a button steps once in that
							 * direction AND arms click-and-hold: the press target + dir
							 * are latched and WiredUI_TickFrame fires further steps after
							 * the initial delay while held (release clears it). A click on
							 * the numeric field between the buttons does nothing. */
							wuiPixelRect_t decR, incR;
							int  dir = 0;
							qboolean haveInc = WiredUI_SpinnerButtonRect( focusedItem, qtrue, &incR );
							qboolean haveDec = WiredUI_SpinnerButtonRect( focusedItem, qfalse, &decR );
							if ( haveInc
							     && wui_cursorX >= incR.x && wui_cursorX < incR.x + incR.w
							     && wui_cursorY >= incR.y && wui_cursorY < incR.y + incR.h ) {
								dir = +1;
							} else if ( haveDec
							     && wui_cursorX >= decR.x && wui_cursorX < decR.x + decR.w
							     && wui_cursorY >= decR.y && wui_cursorY < decR.y + decR.h ) {
								dir = -1;
							}
							if ( dir != 0 ) {
								WiredUI_SpinnerAdjust( focusedItem, dir, 1.0f );
								wui_spinnerHoldItem  = focusedItem;
								wui_spinnerHoldDir   = dir;
								wui_spinnerHoldNext  = cls.realtime + WUI_SPINNER_HOLD_DELAY_MS;
								wui_ix.pressTarget   = focusedItem;
								wui_ix.pressKey      = K_MOUSE1;
							}
						} else {
							/* right-click / enter: MOUSE2 decrements, others increment */
							WiredUI_SpinnerAdjust( focusedItem,
								( key == K_MOUSE2 ) ? -1 : +1, 1.0f );
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
			/* Slider wheel-adjust is handled in WiredUI_CompositorMouseWheel
			 * (reached via the early forward above), which consumes before this
			 * switch — so no slider branch is needed here. */
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
			/* Slider wheel-adjust handled in WiredUI_CompositorMouseWheel (see
			 * K_MWHEELUP above). */
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX && focusedItem->feeder != 0 ) {
				int total = WiredUI_FeederCount( (int)focusedItem->feeder );
				int visible, step;
				if ( focusedItem->horizontalScroll ) {
					float colW = focusedItem->elementwidth > 0 ? focusedItem->elementwidth : 64.0f;
					visible = (int)( focusedItem->rect.w / colW );
				} else {
					/* dpi-consistent with the render/hit-test rowH so the visible
					 * row count (and thus scroll clamp) matches what is drawn. */
					float dpi      = WiredUI_GetDpiScale();
					float charSize = focusedItem->fontPointSize > 0.0f ? focusedItem->fontPointSize : WUI_DEFAULT_FONT_SIZE;
					float rowHFloor = charSize * WUI_LINE_HEIGHT_FACTOR * dpi;
					float rowH = focusedItem->elementheight > 0 ? focusedItem->elementheight * dpi : rowHFloor;
					if ( rowH < rowHFloor ) rowH = rowHFloor;
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
			/* WiredUI F4 (settings tab strip): when focus rests on a settings
			 * nav-rail tab, Left/Right jumps to the previous/next sibling tab,
			 * clamped at the ends (no wrap — the rail is a finite strip). The
			 * jump fires the target tab's `setcvar ; close ; open` action, which
			 * swaps the visible sub-menu, so we RETURN immediately (menu/focus
			 * are torn down + rebuilt by the action). Conflict-free: nav-rail
			 * tabs are type-1 buttons that consume no Left/Right today. */
			if ( focusedItem && wui_item_is_settings_tab( focusedItem ) ) {
				wiredItemDef_t *tabs[ 32 ];
				int             curIdx = -1;
				int             tabCount = wui_collect_settings_tabs( menu, tabs, 32, &curIdx );
				int             dir = ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1;
				int             self = wui_find_in_flat( tabs, tabCount, focusedItem );
				int             next;
				if ( tabCount <= 1 ) break;
				if ( self < 0 ) self = curIdx;
				if ( self < 0 ) self = 0;
				next = self + dir;
				if ( next < 0 || next >= tabCount ) break;   /* clamp at ends */
				if ( tabs[ next ]->action[0] ) {
					WiredUI_RunScript( menu, tabs[ next ], tabs[ next ]->action );
					return;
				}
				break;
			}
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX
			     && focusedItem->horizontalScroll && focusedItem->feeder != 0 ) {
				/* horizontal listbox: left/right moves the selection.
				   Auto-scrolls to keep the selection in view. */
				int total   = WiredUI_FeederCount( (int)focusedItem->feeder );
				float colW  = focusedItem->elementwidth > 0 ? focusedItem->elementwidth : 64.0f;
				int visible = (int)( focusedItem->rect.w / colW );
				int dir     = ( key == K_LEFTARROW || key == K_KP_LEFTARROW ) ? -1 : 1;
				int sel     = focusedItem->listSelectedRow;

				if ( total <= 0 ) break;
				if ( visible < 1 ) visible = 1;

				if ( sel < 0 ) sel = ( dir > 0 ) ? 0 : total - 1;
				else           sel += dir;
				if ( sel < 0 )       sel = 0;
				if ( sel >= total )  sel = total - 1;

				if ( sel != focusedItem->listSelectedRow ) {
					focusedItem->listSelectedRow = sel;
					WiredUI_FeederSelection( (int)focusedItem->feeder, sel );
					if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
				}

				/* keep the selection visible: scroll if it left the window */
				if ( sel < focusedItem->listScrollOffset ) {
					focusedItem->listScrollOffset = sel;
					focusedItem->listScrollFadeTime = cls.realtime;
				} else if ( sel >= focusedItem->listScrollOffset + visible ) {
					focusedItem->listScrollOffset = sel - visible + 1;
					focusedItem->listScrollFadeTime = cls.realtime;
				}
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
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
				else if ( focusedItem->type == ITEM_TYPE_CHECKBOX ) {
					/* Directional: RIGHT (INC) checks, LEFT (DEC) unchecks. */
					WiredUI_StateSetString( focusedItem->cvar, dir > 0 ? "1" : "0" );
				}
				else if ( focusedItem->type == ITEM_TYPE_SPINNER ) {
					/* RIGHT increments, LEFT decrements — one step, clamped. The
					 * step/clamp math lives in the widget core so keyboard, +/-
					 * button click, click-and-hold, and wheel all agree. */
					WiredUI_SpinnerAdjust( focusedItem, dir, 1.0f );
				}
				else if ( focusedItem->type == ITEM_TYPE_RADIOBUTTON ) {
					/* Segmented control: left/right moves the selection one segment,
					 * CLAMPED at the ends (segmented controls don't wrap). Same
					 * select-by-index mutation as click, so both paths agree. */
					int cnt = WiredUI_RadioSegmentCount( focusedItem );
					int cur = WiredUI_RadioSelectedIndex( focusedItem );
					if ( cnt > 0 ) {
						int next;
						if ( cur < 0 ) next = ( dir > 0 ) ? 0 : cnt - 1;
						else           next = cur + dir;
						if ( next < 0 )    next = 0;
						if ( next >= cnt ) next = cnt - 1;
						if ( next != cur ) {
							WiredUI_RadioSelectIndex( focusedItem, next );
							if ( focusedItem->action[0] ) {
								WiredUI_RunScript( menu, focusedItem, focusedItem->action );
							}
						}
					}
				}
			}
			break;

		/* component-library F1 (Table A: HOME/END/PAGE): jump/page a focused
		 * listbox's selection. Additive — Home/End/PageUp/PageDown were inert on
		 * listboxes in the legacy switch; every other focus type ignores them (no
		 * behaviour change). Mirrors the horizontal-listbox row-move logic:
		 * updates listSelectedRow + FeederSelection + keeps the row in view. */
		case K_HOME:
		case K_END:
		case K_PGUP:
		case K_PGDN:
			if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX
			     && focusedItem->feeder != 0 ) {
				int   total   = WiredUI_FeederCount( (int)focusedItem->feeder );
				float rowUnit = focusedItem->horizontalScroll
				              ? ( focusedItem->elementwidth  > 0 ? focusedItem->elementwidth  : 64.0f )
				              : ( focusedItem->elementheight > 0 ? focusedItem->elementheight : 16.0f );
				float span    = focusedItem->horizontalScroll ? focusedItem->rect.w : focusedItem->rect.h;
				int   visible = ( rowUnit > 0 ) ? (int)( span / rowUnit ) : 1;
				int   sel     = focusedItem->listSelectedRow;

				if ( total <= 0 ) break;
				if ( visible < 1 ) visible = 1;
				if ( sel < 0 ) sel = 0;

				if      ( key == K_HOME ) sel = 0;
				else if ( key == K_END )  sel = total - 1;
				else if ( key == K_PGUP ) sel -= visible;
				else if ( key == K_PGDN ) sel += visible;
				if ( sel < 0 )      sel = 0;
				if ( sel >= total ) sel = total - 1;

				if ( sel != focusedItem->listSelectedRow ) {
					focusedItem->listSelectedRow = sel;
					WiredUI_FeederSelection( (int)focusedItem->feeder, sel );
					if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
				}
				/* keep selection visible */
				if ( sel < focusedItem->listScrollOffset ) {
					focusedItem->listScrollOffset = sel;
					focusedItem->listScrollFadeTime = cls.realtime;
				} else if ( sel >= focusedItem->listScrollOffset + visible ) {
					focusedItem->listScrollOffset = sel - visible + 1;
					focusedItem->listScrollFadeTime = cls.realtime;
				}
				if ( focusedItem->listScrollOffset < 0 ) focusedItem->listScrollOffset = 0;
			}
			else if ( focusedItem && focusedItem->type == ITEM_TYPE_SLIDER
			          && focusedItem->cvar[0] ) {
				/* Slider Home/End = jump to min/max. PageUp/PageDown are inert on
				 * sliders (fine-grained value stepping stays on Left/Right). */
				if ( key == K_HOME ) {
					WiredUI_StateSetString( focusedItem->cvar,
						va( "%g", focusedItem->sliderData.minVal ) );
					if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
				} else if ( key == K_END ) {
					WiredUI_StateSetString( focusedItem->cvar,
						va( "%g", focusedItem->sliderData.maxVal ) );
					if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
				}
			}
			else if ( focusedItem && focusedItem->type == ITEM_TYPE_SPINNER
			          && focusedItem->cvar[0] ) {
				/* Spinner: Home/End = min/max (via the core extreme setter);
				 * PageUp/PageDown = a coarse step (10× the normal step, clamped).
				 * All routes share WiredUI_Spinner* so keyboard/button/wheel agree. */
				if ( key == K_HOME ) {
					WiredUI_SpinnerSetExtreme( focusedItem, qfalse );
					if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
				} else if ( key == K_END ) {
					WiredUI_SpinnerSetExtreme( focusedItem, qtrue );
					if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
				} else if ( key == K_PGUP ) {
					WiredUI_SpinnerAdjust( focusedItem, +1, 10.0f );
				} else if ( key == K_PGDN ) {
					WiredUI_SpinnerAdjust( focusedItem, -1, 10.0f );
				}
			}
			else if ( key == K_PGUP || key == K_PGDN ) {
				/* Flex SCROLL-CONTAINER page scroll (F4 container behaviour): when
				 * the focused item is not one of the above value/list widgets,
				 * PageUp/PageDown pages the scroll viewport under the cursor by ±one
				 * viewport height. Cursor-addressed (the container is not a focusable
				 * item, so there is no "focused scroll container" to key off). No-op
				 * when the cursor is not over an overflowing container — the switch
				 * simply falls through with no menu-level page action today. */
				(void) WiredUI_CompositorScrollPage( wui_cursorX, wui_cursorY,
				                                     ( key == K_PGDN ) ? +1 : -1 );
			}
			break;

		case K_UPARROW:
		case K_KP_UPARROW:
			{
				/* Vertical listbox owns UP/DOWN: move the SELECTED ROW inside
				 * the list, not cross-item focus. A focused vertical listbox is
				 * the standard container behaviour (F4) - up/down walks its rows
				 * (keeping the selection in view) and stays inside the list.
				 * Horizontal listboxes keep cross-item nav here (they use
				 * LEFT/RIGHT for row-move, mirrored just above), and every
				 * non-listbox focus falls through to the flat cross-item walk. */
				if ( focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX
				     && !focusedItem->horizontalScroll && focusedItem->feeder != 0 ) {
					int sel = focusedItem->listSelectedRow;
					sel = ( sel < 0 ) ? 0 : sel - 1;
					wui_listbox_set_selection( focusedItem, sel );
					break;
				}

				/* tree-flatten the focusable items so nested
				 * children (inside containers) become reachable. Legacy top-
				 * level-only walk left the new flexbox-first menus inert —
				 * focus index stuck on the popup_overlay container with no
				 * visual change on K_UPARROW/K_DOWNARROW. */
				wiredItemDef_t *flat[ 128 ];
				int             flatCount = 0;
				int             cur, target;
				wui_collect_focusable( menu, flat, 128, &flatCount );
				if ( flatCount == 0 ) break;
				cur = wui_find_in_flat( flat, flatCount, wui_focusedItemPtr );
				target = ( cur > 0 ) ? cur - 1 : flatCount - 1;
				wui_set_focused( menu, flat[ target ] );
				wui_focusFromMouse = qfalse;
				wui_ix.focusFromKeyboard = qtrue;   /* keyboard nav => draw ring */
				if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
			}
			break;

		case K_TAB:
		case K_DOWNARROW:
		case K_KP_DOWNARROW:
			{
				wiredItemDef_t *flat[ 128 ];
				int             flatCount = 0;
				int             cur, target;

				/* Vertical listbox owns DOWN-ARROW: advance the SELECTED ROW.
				 * Gated on the arrow keys (K_TAB shares this case but must
				 * always LEAVE the control, so TAB still falls through to the
				 * cross-item walk). Symmetric with the UP case above. */
				if ( ( key == K_DOWNARROW || key == K_KP_DOWNARROW )
				     && focusedItem && focusedItem->type == ITEM_TYPE_LISTBOX
				     && !focusedItem->horizontalScroll && focusedItem->feeder != 0 ) {
					int sel = focusedItem->listSelectedRow;
					sel = ( sel < 0 ) ? 0 : sel + 1;
					wui_listbox_set_selection( focusedItem, sel );
					break;
				}

				wui_collect_focusable( menu, flat, 128, &flatCount );
				if ( flatCount == 0 ) break;
				cur = wui_find_in_flat( flat, flatCount, wui_focusedItemPtr );
				/* Shift+Tab (K_TAB with shift held) reverses traversal, mirroring
				 * the K_UPARROW walk: previous focusable, wrap to last at the head.
				 * Same flattened focusable set as the forward walk, so forward and
				 * reverse skip identical non-focusable/decoration items. Arrow keys
				 * (K_DOWNARROW) never reverse — only Tab reads the shift modifier,
				 * detected the same way as the compositor path (keys[K_SHIFT].down). */
				if ( key == K_TAB && keys[ K_SHIFT ].down ) {
					target = ( cur > 0 ) ? cur - 1 : flatCount - 1;
				} else {
					target = ( cur >= 0 ) ? ( cur + 1 ) % flatCount : 0;
				}
				wui_set_focused( menu, flat[ target ] );
				wui_focusFromMouse = qfalse;
				wui_ix.focusFromKeyboard = qtrue;   /* keyboard nav => draw ring */
				if ( wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
			}
			break;
	}
}

// ── mouse event ───────────────────────────────────────────────────────
// Accumulates deltas into screen-space cursor, updates focus item,
// fires mouseEnter/mouseExit scripts (ET:Legacy per-item events).

void WiredUI_MouseEvent( float dx, float dy ) {
	wiredMenuDef_t *menu;
	wiredItemDef_t *newHover;
	wiredItemDef_t *oldHover;

	if ( !wui_initialized ) return;

	// notify attract scheduler — significant mouse movement stops attract
	// (attract only needs movement magnitude; int is fine for that gate)
	WiredAttract_NoteMouse( (int)dx, (int)dy );

	// accumulate deltas into cursor position (real screen pixels)
	wui_cursorX += dx;
	if ( wui_cursorX < 0 ) wui_cursorX = 0;
	else if ( wui_cursorX > (float)cls.glconfig.vidWidth ) wui_cursorX = (float)cls.glconfig.vidWidth;

	wui_cursorY += dy;
	if ( wui_cursorY < 0 ) wui_cursorY = 0;
	else if ( wui_cursorY > (float)cls.glconfig.vidHeight ) wui_cursorY = (float)cls.glconfig.vidHeight;

	/* forward the post-clamp cursor
	 * to the compositor's hit-test layer so Clay_SetPointerState gets a
	 * coherent position next frame. Cheap pointer-copy; no behavior change
	 * — the compositor's tracking runs in parallel with the legacy focus
	 * machinery below. */
	WiredUI_CompositorPointerMoved( wui_cursorX, wui_cursorY );

	/* scrollbar thumb drag: while a thumb is held, map the cursor Y to the
	 * scroll position and skip focus/hover churn (mirrors the slider-drag early
	 * return below). Flex scroll-container thumb (state in cl_wired_clay.c) and
	 * vertical listbox thumb (state here) are independent — at most one is live. */
	if ( WiredUI_CompositorScrollbarDragging() ) {
		WiredUI_CompositorScrollbarDragUpdate( wui_cursorY );
		return;
	}
	if ( wui_listScrollDragItem ) {
		wiredMenuDef_t *dragMenu = WiredUI_GetActiveMenu();
		wuiPixelRect_t  clayLB, thumb;
		float           trackTop, travel;
		if ( dragMenu && wui_listScrollDragItem->feeder != 0
		  && WiredUI_ClayItemRenderedRect( dragMenu, wui_listScrollDragItem, &clayLB )
		  && WiredUI_ListboxScrollbarGeom( wui_listScrollDragItem, clayLB.x, clayLB.y,
		                                   clayLB.w, clayLB.h, &thumb, &trackTop, &travel ) ) {
			int total   = WiredUI_FeederCount( (int) wui_listScrollDragItem->feeder );
			int visible, maxScroll;
			/* dpi-consistent visibleRows (matches the render/hit-test rowH). */
			float dpi      = WiredUI_GetDpiScale();
			float charSize = wui_listScrollDragItem->fontPointSize > 0.0f
			               ? wui_listScrollDragItem->fontPointSize : WUI_DEFAULT_FONT_SIZE;
			float rowHFloor= charSize * WUI_LINE_HEIGHT_FACTOR * dpi;
			float rowH     = wui_listScrollDragItem->elementheight > 0
			               ? wui_listScrollDragItem->elementheight * dpi : rowHFloor;
			float headerH  = WiredUI_ListboxHeaderHeight( wui_listScrollDragItem );
			if ( rowH < rowHFloor ) rowH = rowHFloor;
			visible   = rowH > 0.0f ? (int)( ( clayLB.h - headerH ) / rowH ) : 1;
			if ( visible < 1 ) visible = 1;
			maxScroll = total - visible;
			if ( maxScroll < 0 ) maxScroll = 0;
			if ( travel > 0.0f && maxScroll > 0 ) {
				float thumbTop = wui_cursorY - wui_listScrollGrabDY;   /* desired top */
				float frac     = ( thumbTop - trackTop ) / travel;     /* 0..1 */
				int   off;
				if ( frac < 0.0f ) frac = 0.0f;
				if ( frac > 1.0f ) frac = 1.0f;
				off = (int)( frac * (float) maxScroll + 0.5f );
				if ( off < 0 ) off = 0;
				if ( off > maxScroll ) off = maxScroll;
				wui_listScrollDragItem->listScrollOffset  = off;
				wui_listScrollDragItem->listScrollFadeTime = cls.realtime;
			}
		}
		return;   // don't change focus while dragging
	}

	// slider drag: continuously update cvar while mouse1 is held
	if ( wui_sliderDragging && wui_sliderDragItem && wui_sliderDragItem->cvar[0] ) {
		wiredMenuDef_t *dragMenu = WiredUI_GetActiveMenu();
		if ( dragMenu ) {
			/* Continuous drag: resolve the value from the SAME single-source
			 * track rect the thumb is drawn from (Clay-rendered px), so the thumb
			 * tracks the cursor exactly at HiDPI. Legacy resolvedRect fallback only
			 * until the track has rendered once. */
			float range = wui_sliderDragItem->sliderData.maxVal - wui_sliderDragItem->sliderData.minVal;
			float barX, barW;
			if ( !WiredUI_SliderTrackGeom( wui_sliderDragItem, &barX, &barW, NULL, NULL ) ) {
				float menuOX = dragMenu->fullscreen ? 0 : dragMenu->rect.x;
				float absItemX = menuOX + wui_sliderDragItem->rect.x;
				barX = absItemX + wui_sliderDragItem->rect.w * 0.5f;
				barW = wui_sliderDragItem->rect.w * 0.45f;
			}
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
		} else if ( WiredUI_GetMultiDropdownRect( menu, wui_multiDropdownItem, &opts,
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
		/* The dropdown is MODAL: while it is open it captures the pointer.
		 * The main settings panels still run their own Clay_SetPointerState +
		 * emit pass each frame (cl_wired_clay.c per-panel loop), so the Clay
		 * hover lookup below would still resolve a background row under the
		 * cursor and commit it to wui_focusedItemPtr — painting the gold
		 * focus-gradient on a row hidden behind the overlay, and letting an
		 * `active` row fire on hover. Clay's own pointer capture can't stop
		 * this because the overlay is emitted in a SEPARATE Clay pass. Bail
		 * out here so no background row registers hover/focus; the dropdown's
		 * own option hover is tracked above via wui_multiDropdownHover, and
		 * click/dismiss is handled entirely in WiredUI_KeyEvent's
		 * wui_multiDropdownOpen branch. WiredUI_CloseMultiDropdown() above (on
		 * a degenerate empty-options dropdown) clears the flag, so a closed
		 * dropdown falls through to normal hover on the next event. */
		if ( wui_multiDropdownOpen ) {
			return;
		}
	}

	oldHover = wui_hoveredItemPtr;
	/* prefer the compositor's Clay-side hover lookup
	 * (prior-frame Clay_GetPointerOverIds → wui_id_map). Clay tracks the
	 * runtime bounding box per element, so this works for nested flex
	 * children whose resolvedRect.{x,y} cascade to 0. Falls back to the
	 * resolvedRect-based recursive walk for menus that haven't yet
	 * completed a render pass (first frame after a push) or when no Clay
	 * element matches. */
	{
		extern const wiredItemDef_t *WiredUI_CompositorHoveredItem( void );
		extern uint32_t WiredUI_CompositorGetHoveredId( void );
		const wiredItemDef_t *clayHit = WiredUI_CompositorHoveredItem();
		newHover = NULL;
		/* Clay returns whatever element sits topmost under the cursor —
		 * including pure layout containers (cards, rails, top/bottom bars)
		 * and inner decoration labels nested inside an actionable
		 * container. Accept Clay's hit only when it points at a hoverable
		 * item; otherwise fall through to the resolvedRect walker, which
		 * already implements the "actionable-container is the target,
		 * children are decorative composition" rule (see
		 * wui_find_item_at_cursor_recursive). */
		if ( clayHit && WiredUI_ItemAcceptsMouseHover( (wiredItemDef_t *) clayHit ) ) {
			newHover = (wiredItemDef_t *) clayHit;
		} else if ( !clayHit ) {
			/* Clay returned nothing — the menu hasn't completed a render pass
			 * yet (first frame after a push), so Clay has no hit-test data.
			 * Only then fall back to the resolvedRect walker. Once Clay HAS a
			 * result (clayHit != NULL) we trust it even if it points at a
			 * non-hoverable element (= cursor is over empty/decorative space,
			 * so no hover). Using the resolvedRect fallback in that case
			 * selected the WRONG row for flex-container children: their
			 * resolvedRect.{x,y} diverge from where Clay actually paints them
			 * (same divergence the dropdown-anchor fix documents), so the
			 * highlight landed one row off from the cursor. */
			newHover = WiredUI_FindItemAtCursor( menu, wui_cursorX, wui_cursorY );
			if ( newHover && !WiredUI_ItemAcceptsMouseHover( newHover ) ) {
				newHover = NULL;
			}
		}
	}

	// any mouse movement reactivates mouse-based focus
	wui_focusFromMouse = ( newHover != NULL );
	/* mouse hover clears the keyboard-focus provenance so the focus RING
	 * (keyboard-only) yields to the hover FILL when the cursor moves. */
	if ( newHover != NULL ) {
		wui_ix.focusFromKeyboard = qfalse;
	}

	// fire mouseExit on old item, mouseEnter on new item
	if ( newHover != oldHover ) {
		if ( oldHover ) {
			if ( oldHover->mouseExit[0] ) {
				WiredUI_RunScript( menu, oldHover, oldHover->mouseExit );
			}
			if ( oldHover->leaveFocus[0] ) {
				WiredUI_RunScript( menu, oldHover, oldHover->leaveFocus );
			}
		}
		// reset tooltip timer on focus change
		if ( newHover ) {
			if ( newHover->mouseEnter[0] ) {
				WiredUI_RunScript( menu, newHover, newHover->mouseEnter );
			}
			if ( newHover->onFocus[0] ) {
				WiredUI_RunScript( menu, newHover, newHover->onFocus );
			}
			// start tooltip delay timer if new item has a tooltip
			if ( newHover->tooltip[0] ) {
				wui_tooltipStartTime = cls.realtime;
				wui_tooltipFocusItem = -1;  // pointer-based; legacy int retained as -1
			} else {
				wui_tooltipStartTime = 0;
				wui_tooltipFocusItem = -1;
			}
		} else {
			// cursor left all items — clear tooltip state
			wui_tooltipStartTime = 0;
			wui_tooltipFocusItem = -1;
		}
		/* pointer-based mouse focus reaches nested
		 * leaves (mirrors the keyboard collector). wui_focusedItemPtr is
		 * the authoritative source consumed by WiredUI_GetFocusedItem +
		 * the K_MOUSE1 click branch in WiredUI_KeyEvent (which resolves
		 * focusedItem from the pointer before dispatching action). The
		 * legacy wui_focusItem int index maps back to top-level only
		 * when the hit happens to be at top level — otherwise -1. */
		wui_hoveredItemPtr  = newHover;
		wui_focusedItemPtr  = newHover;
		wui_focusItem       = -1;
		if ( newHover ) {
			int i;
			for ( i = 0; i < menu->itemCount; i++ ) {
				if ( menu->items[ i ] == newHover ) { wui_focusItem = i; break; }
			}
		}
		if ( newHover && wui_sfxFocus ) S_StartLocalSound( wui_sfxFocus, CHAN_LOCAL_SOUND );
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

		/* Main is a REAL resting surface, not a bare implicit root (Eser
		 * 2026-07-05). Promote it to an actual depth-1 stack entry so the menu
		 * navigation depth is honest:
		 *   - popping the FIRST sub-menu (startserver/campaign/servers/settings)
		 *     lands back on main at depth>=1 — the menu stays up, no attract flip;
		 *   - ESC on main itself is the last pop (depth 1->0), which IS the genuine
		 *     "user dismissed main" case that the depth-0 pop branch turns into
		 *     attract_restart. So that branch now fires ONLY on real main-dismissal.
		 * Previously main sat at depth 0 as an implicit root, so ANY pop that landed
		 * at depth 0 (including the first sub-menu pop) was misread as "main
		 * dismissed → attract", and the `close ; open "main"` batch's transient
		 * depth-0 midpoint queued a spurious attract_restart mid-transition
		 * (half-console / dead-input corruption).
		 * INGAME stays an implicit root (no push): its depth-0 pop deliberately
		 * closes the menu entirely and unpauses; pushing it would defeat that. Its
		 * onOpen is fired manually below as before. */
		if ( menu == UIMENU_MAIN ) {
			// PushMenu fires main's onOpen (initial focus seed) + re-asserts
			// KEYCATCH_UI; its own top-of-stack guard makes a redundant explicit
			// `open "main"` (cl_wired_ui.c:2330) collapse instead of duplicating,
			// and REFRESHES the background intent while collapsing (:5891).
			//
			// This used to be wrapped in `if ( wui_menuStackDepth == 0 )`, which
			// skipped the push whenever anything was left on the stack. Coming
			// back from a map that is exactly what happens: the menu appeared via
			// the stack fallback, but WUI_BG_INTENT_SCENE was never recorded for
			// the top slot, so the parallax scene did not draw and the main menu
			// came up on black. Opening any submenu pushed properly and fixed it,
			// which is what made the bug look specific to main. The guard is not
			// needed — PushMenu already collapses a redundant push.
			WiredUI_PushMenu( "main" );
		} else if ( menu == UIMENU_INGAME ) {
			/* Fire the ingame root's onOpen when activating via SetActiveMenu
			 * alone (it lives behind the stack fallback, not pushed). */
			if ( wui_menuStackDepth == 0 ) {
				wiredMenuDef_t *root = WiredUI_FindMenu( "ingame" );
				if ( root && root->onOpen[0] ) {
					WiredUI_RunScript( root, NULL, root->onOpen );
				}
			}
		}

		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: SetActiveMenu %d (depth %d)\n", menu, wui_menuStackDepth );
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

/*
================
CL_PublishConnectState

Extracted per-frame publisher (was inline at the top of
WiredUI_DrawConnectScreen). Fires from CL_Frame BEFORE SCR_UpdateScreen
runs the compositor emit walk, so storeBind reads see current-frame
values (eliminates the one-frame lag the render-time publisher had).

Gated on clientActiveApp->state range matching the legacy WiredUI_DrawConnectScreen
invocation conditions (CA_CONNECTING through CA_PRIMED via cl_scrn.c's
CA_CONNECTING/CHALLENGING/CONNECTED + CA_LOADING/PRIMED paths): outside
that range the function no-ops, preserving legacy behavior of "connect.*
keys untouched when not connecting." Eventually
WiredUI_DrawConnectScreen retires, publisher stays.

4 keys (B+X path, ratified 2026-05-24):
  connect.servername   — composed "Connecting to %s" / "Starting…"
  connect.state        — switch on clientActiveApp->state with download fold-in
  connect.error        — clientActiveApp->clc.serverMessage (legacy errColor red)
  connect.motd         — cl_motdString cvar (legacy dim)
================
*/
void CL_PublishConnectState( void ) {
	wuiStoreEntry_t *e;
	char             buf[ MAX_STRING_CHARS ];

	if ( clientActiveApp->state < CA_CONNECTING || clientActiveApp->state > CA_PRIMED ) {
		return;
	}

	/* connect.servername */
	e = WiredStore_Set( "connect.servername" );
	if ( e ) {
		if ( clientActiveApp->servername[ 0 ] ) {
			if ( !Q_stricmp( clientActiveApp->servername, "localhost" ) ) {
				Q_strncpyz( e->text, "Starting local server...", sizeof( e->text ) );
			} else {
				Com_sprintf( e->text, sizeof( e->text ), "Connecting to %s", clientActiveApp->servername );
			}
		} else {
			e->text[ 0 ] = '\0';
		}
	}

	/* connect.state — download folded in for CA_CONNECTED per legacy verbatim */
	e = WiredStore_Set( "connect.state" );
	if ( e ) {
		switch ( clientActiveApp->state ) {
		case CA_CONNECTING:
			Com_sprintf( e->text, sizeof( e->text ),
				"Awaiting connection... %d", clientActiveApp->clc.connectPacketCount );
			break;
		case CA_CHALLENGING:
			Com_sprintf( e->text, sizeof( e->text ),
				"Awaiting challenge... %d", clientActiveApp->clc.connectPacketCount );
			break;
		case CA_CONNECTED:
			if ( clientActiveApp->clc.downloadName[ 0 ] ) {
				int pct = 0;
				if ( clientActiveApp->clc.downloadSize > 0 ) {
					pct = (int)( (float) clientActiveApp->clc.downloadCount * 100.0f / (float) clientActiveApp->clc.downloadSize );
				}
				Com_sprintf( e->text, sizeof( e->text ),
					"Downloading %s... %d%%", clientActiveApp->clc.downloadName, pct );
			} else {
				Q_strncpyz( e->text, "Awaiting gamestate...", sizeof( e->text ) );
			}
			break;
		default:
			Q_strncpyz( e->text, "Connecting...", sizeof( e->text ) );
			break;
		}
	}

	/* connect.error — server error message (errColor red-ish) */
	e = WiredStore_Set( "connect.error" );
	if ( e ) Q_strncpyz( e->text, clientActiveApp->clc.serverMessage, sizeof( e->text ) );

	/* connect.motd — master-server MOTD (dim) */
	e = WiredStore_Set( "connect.motd" );
	if ( e ) {
		Cvar_VariableStringBuffer( "cl_motdString", buf, sizeof( buf ) );
		Q_strncpyz( e->text, buf, sizeof( e->text ) );
	}
}

/* WiredUI_DrawConnectScreen + WiredUI_RenderMenuOverlay retired.
 * Compositor is now the sole render path for connect dialog (connect.wmenu
 * + 4 storeBind text items + CL_PublishConnectState publisher) and for
 * scoreboards (8 scoreboard wmenu files + WiredHud_Routine's per-frame
 * visible-toggle). Earlier passes landed all replacements before
 * this deletion. */

void WiredUI_ReloadHud( void ) {
	WiredUI_ClearPasswordPromptState( qtrue );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: reloading HUD...\n" );
	WiredUI_ResetListboxDoubleClick( "reload" );
	WiredUI_ReleaseCompositorPointer( "reload" );

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

	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: HUD reloaded, %d elements active\n", WiredHud_GetElementCount() );
}

void WiredUI_ReloadMenus( void ) {
	WiredUI_ClearPasswordPromptState( qtrue );
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: reloading menus...\n" );
	WiredUI_ResetListboxDoubleClick( "reload" );
	WiredUI_ReleaseCompositorPointer( "reload" );

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
	wui_focusedItemPtr = NULL;
	wui_hoveredItemPtr = NULL;
	wui_ix.pressTarget = NULL;
	wui_ix.focusFromKeyboard = qfalse;
	wui_tooltipStartTime = 0;
	wui_tooltipFocusItem = -1;

	// destroy HUD elements (they'll be recreated after reload)
	WiredHud_DestroyAllElements();

	// two-phase safe reload: parse new → swap, or keep old on failure
	if ( WiredUI_SafeReload() ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui), "Menus reloaded successfully.\n" );
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

void CL_ShutdownUI( void ) {
	Key_SetCatcher( Key_GetCatcher() & ~KEYCATCH_UI );
	cls.uiStarted = qfalse;
	WiredUI_Shutdown();
}

#endif // FEAT_WIRED_UI
