/*
cl_wired_parse.c — Wired UI: menu file parser
*/

#include "../../client.h"
#include "cl_wired_ui.h"

#if FEAT_WIRED_UI

#include "../../../botlib/botlib.h"
#include "../../../qcommon/menudef.h"

// wiredRect_t defined in cl_wired_ui.h

extern botlib_export_t *botlib_export;

// ── format detection ──────────────────────────────────────────────────
// .wmenu/.whud files use normalized coordinates (0.0-1.0) and point sizes.

// ── memory pool ───────────────────────────────────────────────────────

static char wui_menuPool[WIRED_MENU_POOL_SIZE];
static int  wui_menuPoolUsed = 0;

static void *WiredUI_Alloc( int size ) {
	char *p;

	if ( wui_menuPoolUsed + size > WIRED_MENU_POOL_SIZE ) {
		Com_Printf( S_COLOR_RED "WiredUI_Alloc: pool exhausted (%d + %d > %d)\n",
			wui_menuPoolUsed, size, WIRED_MENU_POOL_SIZE );
		return NULL;
	}

	p = &wui_menuPool[wui_menuPoolUsed];
	wui_menuPoolUsed += ( ( size + 31 ) & ~31 );  // 32-byte align
	memset( p, 0, size );
	return p;
}

void WiredUI_ResetPool( void ) {
	wui_menuPoolUsed = 0;
	memset( wui_menuPool, 0, sizeof( wui_menuPool ) );
}

// ── PC parser wrappers ────────────────────────────────────────────────
// Direct calls to botlib's precompiler — no VM indirection.

static int WiredPC_LoadSource( const char *filename ) {
	return botlib_export->PC_LoadSourceHandle( filename );
}

static int WiredPC_FreeSource( int handle ) {
	return botlib_export->PC_FreeSourceHandle( handle );
}

// One-token pushback buffer (botlib PC API has no unread function)
static pc_token_t wui_pendingToken;
static qboolean   wui_hasPendingToken = qfalse;

static int WiredPC_ReadToken( int handle, pc_token_t *token ) {
	if ( wui_hasPendingToken ) {
		*token = wui_pendingToken;
		wui_hasPendingToken = qfalse;
		return 1;
	}
	return botlib_export->PC_ReadTokenHandle( handle, token );
}

static void WiredPC_UnreadToken( pc_token_t *token ) {
	wui_pendingToken = *token;
	wui_hasPendingToken = qtrue;
}

static int WiredPC_AddDefine( const char *define ) {
	return botlib_export->PC_AddGlobalDefine( define );
}

// ── $evalfloat / $evalint expression evaluator ────────────────────────
// ET:Legacy feature: inline math in .menu property values.
// Example: rect $evalfloat(MENU_W/2-100) 140 200 200
// Supports: + - * / ( ) and #define'd constants.
// Evaluated at parse time, not runtime.

static float WiredEval_Expr( const char **p );

static float WiredEval_Atom( const char **p ) {
	float val = 0;
	qboolean neg = qfalse;

	while ( **p == ' ' || **p == '\t' ) (*p)++;

	if ( **p == '-' ) { neg = qtrue; (*p)++; }
	else if ( **p == '+' ) { (*p)++; }

	while ( **p == ' ' || **p == '\t' ) (*p)++;

	if ( **p == '(' ) {
		(*p)++;
		val = WiredEval_Expr( p );
		if ( **p == ')' ) (*p)++;
	} else {
		// parse number
		val = atof( *p );
		// advance past number
		if ( **p == '-' || **p == '+' ) (*p)++;
		while ( ( **p >= '0' && **p <= '9' ) || **p == '.' ) (*p)++;
	}

	return neg ? -val : val;
}

static float WiredEval_Term( const char **p ) {
	float val = WiredEval_Atom( p );
	while ( 1 ) {
		while ( **p == ' ' || **p == '\t' ) (*p)++;
		if ( **p == '*' ) { (*p)++; val *= WiredEval_Atom( p ); }
		else if ( **p == '/' ) {
			(*p)++;
			float d = WiredEval_Atom( p );
			if ( d != 0 ) val /= d;
		}
		else break;
	}
	return val;
}

static float WiredEval_Expr( const char **p ) {
	float val = WiredEval_Term( p );
	while ( 1 ) {
		while ( **p == ' ' || **p == '\t' ) (*p)++;
		if ( **p == '+' ) { (*p)++; val += WiredEval_Term( p ); }
		else if ( **p == '-' ) { (*p)++; val -= WiredEval_Term( p ); }
		else break;
	}
	return val;
}

// Wraps WiredPC_ReadToken to handle $evalfloat() and $evalint() tokens
static int WiredPC_ReadTokenEval( int handle, pc_token_t *token ) {
	if ( !WiredPC_ReadToken( handle, token ) ) return 0;

	if ( !Q_stricmpn( token->string, "$evalfloat(", 11 ) || !Q_stricmpn( token->string, "$evalint(", 9 ) ) {
		qboolean isInt = !Q_stricmpn( token->string, "$evalint(", 9 );
		// the botlib tokenizer may split this across tokens — collect until closing )
		char exprBuf[256];
		int depth = 0;
		const char *start;
		float result;

		// find the opening ( in the token
		start = strchr( token->string, '(' );
		if ( !start ) return 1; // malformed, return as-is
		start++; // skip (

		Q_strncpyz( exprBuf, start, sizeof( exprBuf ) );

		// count parens — the initial $evalfloat( adds 1
		depth = 1;
		{
			const char *c;
			for ( c = exprBuf; *c; c++ ) {
				if ( *c == '(' ) depth++;
				if ( *c == ')' ) depth--;
			}
		}

		// if closing ) is already in this token, strip it
		if ( depth <= 0 ) {
			char *cp = strrchr( exprBuf, ')' );
			if ( cp ) *cp = '\0';
		} else {
			// read more tokens until balanced
			while ( depth > 0 ) {
				pc_token_t next;
				if ( !WiredPC_ReadToken( handle, &next ) ) break;
				Q_strcat( exprBuf, sizeof(exprBuf), next.string );
				{
					const char *c;
					for ( c = next.string; *c; c++ ) {
						if ( *c == '(' ) depth++;
						if ( *c == ')' ) depth--;
					}
				}
			}
			// strip trailing )
			{
				char *cp = strrchr( exprBuf, ')' );
				if ( cp ) *cp = '\0';
			}
		}

		// evaluate
		{
			const char *p = exprBuf;
			result = WiredEval_Expr( &p );
		}

		// replace token with result
		if ( isInt ) {
			Com_sprintf( token->string, sizeof( token->string ), "%d", (int)result );
			token->intvalue = (int)result;
			token->floatvalue = result;
		} else {
			Com_sprintf( token->string, sizeof( token->string ), "%g", result );
			token->floatvalue = result;
			token->intvalue = (int)result;
		}
	}

	return 1;
}

// ── token parsing helpers ─────────────────────────────────────────────

static qboolean WiredPC_String( int handle, const char **out ) {
	pc_token_t token;
	static char buf[1024];

	if ( !WiredPC_ReadTokenEval( handle, &token ) ) {
		return qfalse;
	}
	Q_strncpyz( buf, token.string, sizeof( buf ) );
	*out = buf;
	return qtrue;
}

static qboolean WiredPC_Int( int handle, int *out ) {
	pc_token_t token;

	if ( !WiredPC_ReadTokenEval( handle, &token ) ) {
		return qfalse;
	}
	*out = token.intvalue;
	return qtrue;
}

static qboolean WiredPC_Float( int handle, float *out ) {
	pc_token_t token;

	if ( !WiredPC_ReadTokenEval( handle, &token ) ) {
		return qfalse;
	}
	*out = token.floatvalue;
	return qtrue;
}

static qboolean WiredPC_Color( int handle, vec4_t *color ) {
	int i;
	float f;

	for ( i = 0; i < 4; i++ ) {
		if ( !WiredPC_Float( handle, &f ) ) {
			return qfalse;
		}
		(*color)[i] = f;
	}
	return qtrue;
}

// ── enum table lookup ─────────────────────────────────────────────────

#define WUI_ENUM_UNKNOWN 0x7FFFFFFF

typedef struct { const char *k; int v; } wuiEnumMap_t;

static int WiredPC_LookupEnum( const wuiEnumMap_t *map, const char *str, int defaultVal ) {
	int i;
	for ( i = 0; map[i].k; i++ ) {
		if ( !Q_stricmp( str, map[i].k ) ) return map[i].v;
	}
	return defaultVal;
}

static const wuiEnumMap_t s_anchorMap[] = {
	{ "TOP_LEFT",      ANCHOR_TOP_LEFT      },
	{ "TOP_CENTER",    ANCHOR_TOP_CENTER    },
	{ "TOP_RIGHT",     ANCHOR_TOP_RIGHT     },
	{ "CENTER_LEFT",   ANCHOR_CENTER_LEFT   },
	{ "CENTER",        ANCHOR_CENTER        },
	{ "CENTER_RIGHT",  ANCHOR_CENTER_RIGHT  },
	{ "BOTTOM_LEFT",   ANCHOR_BOTTOM_LEFT   },
	{ "BOTTOM_CENTER", ANCHOR_BOTTOM_CENTER },
	{ "BOTTOM_RIGHT",  ANCHOR_BOTTOM_RIGHT  },
	{ NULL, 0 }
};

static const wuiEnumMap_t s_fontWeightMap[] = {
	{ "light",     300 },
	{ "regular",   400 },
	{ "medium",    500 },
	{ "semibold",  600 },
	{ "bold",      700 },
	{ "extrabold", 800 },
	{ NULL, 0 }
};

static const wuiEnumMap_t s_directionMap[] = {
	{ "R", 0 }, { "right",  0 },
	{ "L", 1 }, { "left",   1 },
	{ "T", 2 }, { "top",    2 },
	{ "B", 3 }, { "bottom", 3 },
	{ NULL, 0 }
};

static const wuiEnumMap_t s_alignVMap[] = {
	{ "T", 0 }, { "top",    0 },
	{ "C", 1 }, { "center", 1 },
	{ "B", 2 }, { "bottom", 2 },
	{ NULL, 0 }
};

static const wuiEnumMap_t s_justifyMap[] = {
	{ "start",         WUI_JUSTIFY_START        },
	{ "center",        WUI_JUSTIFY_CENTER       },
	{ "end",           WUI_JUSTIFY_END          },
	{ "space-between", WUI_JUSTIFY_SPACE_BETWEEN },
	{ NULL, 0 }
};

static const wuiEnumMap_t s_easingMap[] = {
	{ "ease-in",     WUI_EASE_IN     },
	{ "ease-out",    WUI_EASE_OUT    },
	{ "ease-in-out", WUI_EASE_IN_OUT },
	{ "linear",      WUI_EASE_LINEAR },
	{ NULL, 0 }
};

static const wuiEnumMap_t s_alignMap[] = {
	{ "start",   WUI_ALIGN_START   },
	{ "center",  WUI_ALIGN_CENTER  },
	{ "end",     WUI_ALIGN_END     },
	{ "stretch", WUI_ALIGN_STRETCH },
	{ NULL, 0 }
};

// ── brace-balanced script capture ─────────────────────────────────────

static qboolean WiredPC_CaptureBracedScript( int handle, char *dest, int destSize ) {
	pc_token_t token;
	int depth;
	if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) {
		return qfalse;
	}
	depth = 1;
	if ( dest ) dest[0] = '\0';
	while ( depth > 0 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) break;
		if ( !Q_stricmp( token.string, "{" ) ) { depth++; continue; }
		if ( !Q_stricmp( token.string, "}" ) ) { depth--; continue; }
		if ( dest && destSize > 0 ) {
			if ( dest[0] ) Q_strcat( dest, destSize, " " );
			Q_strcat( dest, destSize, token.string );
		}
	}
	return qtrue;
}

/* Consume one token and return qtrue only if it equals literal. */
static qboolean WiredPC_Expect( int handle, const char *literal ) {
	pc_token_t token;
	if ( !WiredPC_ReadToken( handle, &token ) ) return qfalse;
	return ( Q_stricmp( token.string, literal ) == 0 ) ? qtrue : qfalse;
}

/* Skip a brace-balanced block starting from '{'. The opening '{' must
   already have been consumed by the caller before calling this. */
static void WiredPC_SkipBracedBlock( int handle ) {
	pc_token_t token;
	int depth = 1;
	while ( depth > 0 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) break;
		if      ( !Q_stricmp( token.string, "{" ) ) depth++;
		else if ( !Q_stricmp( token.string, "}" ) ) depth--;
	}
}

// ── unit-aware value parser ───────────────────────────────────────────
// Parse a value with optional unit suffix: "0.5" (norm), "50vw", "50vh", "16px"

static wuiValue_t WiredPC_ParseValue( int handle ) {
	pc_token_t token;
	wuiValue_t val;
	val.value = 0.0f;
	val.unit = UNIT_NORM;

	if ( !WiredPC_ReadTokenEval( handle, &token ) ) {
		return val;
	}

	// "auto" keyword — size determined by content
	if ( !Q_stricmp( token.string, "auto" ) ) {
		val.value = 0.0f;
		val.unit = UNIT_AUTO;
		return val;
	}

	val.value = token.floatvalue;

	// Peek at next token for unit keyword (tokenizer splits "50vw" → "50" + "vw")
	if ( WiredPC_ReadToken( handle, &token ) ) {
		if ( !Q_stricmp( token.string, "vw" ) ) {
			val.unit = UNIT_VW;
		} else if ( !Q_stricmp( token.string, "vh" ) ) {
			val.unit = UNIT_VH;
		} else if ( !Q_stricmp( token.string, "px" ) ) {
			val.unit = UNIT_PX;
		} else {
			// Not a unit keyword — push back for the next read
			WiredPC_UnreadToken( &token );
		}
	}

	return val;
}

// Back-fill a unit-aware value to real screen pixels for draw code.
// screenDim is cls.glconfig.vidWidth for x/w, cls.glconfig.vidHeight for y/h.
static float WUI_BackfillToScreen( wuiValue_t val, float screenDim ) {
	switch ( val.unit ) {
		case UNIT_VW:   return ( val.value / 100.0f ) * (float)cls.glconfig.vidWidth;
		case UNIT_VH:   return ( val.value / 100.0f ) * (float)cls.glconfig.vidHeight;
		case UNIT_PX:   return val.value;
		case UNIT_NORM:
		default:        return val.value * screenDim;
	}
}

// ── data structures ───────────────────────────────────────────────────

// wiredItemDef_t, wiredMenuDef_t defined in cl_wired_ui.h

// ── global menu state ─────────────────────────────────────────────────

static wiredMenuDef_t *wui_menus[WIRED_MAX_MENUS];
static int              wui_menuCount = 0;

// ── shared flex container keyword parser ──────────────────────────────
/* Returns qtrue and consumes one keyword block if keyword names a flex prop. */
static qboolean WiredPC_ParseFlexProps( int handle, const char *keyword,
                                        wuiFlexContainer_t *fc, qboolean *isFlexContainer ) {
	if ( !Q_stricmp( keyword, "layout" ) ) {
		pc_token_t t;
		if ( WiredPC_ReadTokenEval( handle, &t ) ) {
			if      ( !Q_stricmp( t.string, "row"    ) ) { fc->direction = WUI_LAYOUT_ROW;    *isFlexContainer = qtrue; }
			else if ( !Q_stricmp( t.string, "column" ) ) { fc->direction = WUI_LAYOUT_COLUMN; *isFlexContainer = qtrue; }
			if ( WiredPC_ReadToken( handle, &t ) ) {
				if ( !Q_stricmp( t.string, "wrap" ) ) fc->wrap = qtrue;
				else WiredPC_UnreadToken( &t );
			}
		}
		return qtrue;
	}
	if ( !Q_stricmp( keyword, "gap" ) ) {
		fc->gap = WiredPC_ParseValue( handle );
		return qtrue;
	}
	if ( !Q_stricmp( keyword, "padding" ) ) {
		pc_token_t peek;
		fc->padding[0] = WiredPC_ParseValue( handle );
		if ( WiredPC_ReadToken( handle, &peek ) ) {
			if ( peek.type == TT_NUMBER || peek.string[0] == '0' || peek.string[0] == '.' ) {
				WiredPC_UnreadToken( &peek );
				fc->padding[1] = WiredPC_ParseValue( handle );
				if ( WiredPC_ReadToken( handle, &peek ) ) {
					if ( peek.type == TT_NUMBER || peek.string[0] == '0' || peek.string[0] == '.' ) {
						WiredPC_UnreadToken( &peek );
						fc->padding[2] = WiredPC_ParseValue( handle );
						fc->padding[3] = WiredPC_ParseValue( handle );
					} else {
						WiredPC_UnreadToken( &peek );
						fc->padding[2] = fc->padding[0];
						fc->padding[3] = fc->padding[1];
					}
				}
			} else {
				WiredPC_UnreadToken( &peek );
				fc->padding[1] = fc->padding[2] = fc->padding[3] = fc->padding[0];
			}
		}
		return qtrue;
	}
	if ( !Q_stricmp( keyword, "align" ) ) {
		pc_token_t val;
		if ( WiredPC_ReadTokenEval( handle, &val ) )
			fc->align = (wuiAlign_t)WiredPC_LookupEnum( s_alignMap, val.string, WUI_ALIGN_START );
		return qtrue;
	}
	if ( !Q_stricmp( keyword, "justify" ) ) {
		pc_token_t val;
		if ( WiredPC_ReadTokenEval( handle, &val ) )
			fc->justify = (wuiJustify_t)WiredPC_LookupEnum( s_justifyMap, val.string, WUI_JUSTIFY_START );
		return qtrue;
	}
	return qfalse;
}

// ── item property parser (shared between top-level and nested items) ──

static qboolean WiredUI_ParseItemProperties( int handle, wiredItemDef_t *item );

// ── item parser ───────────────────────────────────────────────────────

static qboolean WiredUI_ParseItem( int handle, wiredMenuDef_t *menu ) {
	wiredItemDef_t *item;

	item = (wiredItemDef_t *)WiredUI_Alloc( sizeof( wiredItemDef_t ) );
	if ( !item ) {
		return qfalse;
	}

	// defaults
	item->visible = qtrue;
	item->forecolor[0] = item->forecolor[1] = item->forecolor[2] = item->forecolor[3] = 1.0f;
	item->textscale = 0.3f;
	item->textalign = -1;   // sentinel: -1 = not set (LEFT=0 is valid)
	item->alignV = -1;      // sentinel: -1 = not set (TOP=0 is valid)
	item->direction = -1;   // sentinel: -1 = not set (L2R=0 is valid)
	item->fadeAlphaItem = 1.0f;  // fully opaque by default
	item->anchor = ANCHOR_NONE;
	item->textoffsetX = 0;
	item->textoffsetY = 0;
	item->fontPointSize = 0;
	item->fontWeight = 0;
	item->letterSpacing = 0.0f;
	item->modelFovX = 40.0f;
	item->modelFovY = 40.0f;
	item->modelRotation = 0.0f;
	item->modelAngle = 0.0f;
	item->modelWidescreen = 0;
	item->flexChild.shrink = 1.0f;  // default shrink for flex children

	if ( !WiredUI_ParseItemProperties( handle, item ) ) {
		return qfalse;
	}

	if ( menu->itemCount < WIRED_MAX_ITEMS_PER_MENU ) {
		menu->items[menu->itemCount++] = item;
	}

	return qtrue;
}

static void WiredPC_ParseRectInto( int handle, wuiRect_t *wuiRect, wiredRect_t *rect ) {
	wuiRect->x = WiredPC_ParseValue( handle );
	wuiRect->y = WiredPC_ParseValue( handle );
	wuiRect->w = WiredPC_ParseValue( handle );
	wuiRect->h = WiredPC_ParseValue( handle );
	rect->x = WUI_BackfillToScreen( wuiRect->x, (float)cls.glconfig.vidWidth );
	rect->y = WUI_BackfillToScreen( wuiRect->y, (float)cls.glconfig.vidHeight );
	rect->w = WUI_BackfillToScreen( wuiRect->w, (float)cls.glconfig.vidWidth );
	rect->h = WUI_BackfillToScreen( wuiRect->h, (float)cls.glconfig.vidHeight );
}

static void WiredPC_ParseAnchorInto( int handle, wiredAnchor_t *anchor ) {
	const char *str;
	if ( WiredPC_String( handle, &str ) ) {
		int a = WiredPC_LookupEnum( s_anchorMap, str, WUI_ENUM_UNKNOWN );
		if ( a == WUI_ENUM_UNKNOWN ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: unknown anchor '%s'\n", str );
			*anchor = ANCHOR_NONE;
		} else {
			*anchor = (wiredAnchor_t)a;
		}
	}
}

// ── descriptor-table property dispatch ────────────────────────────────

typedef enum { WP_INT, WP_FLOAT, WP_STR, WP_COLOR, WP_FLAG } wuiPropType_t;
typedef struct {
	const char    *key;
	wuiPropType_t  type;
	int            off;
	int            size;
} wuiPropDef_t;

static qboolean WiredPC_ApplyProp( int handle, void *base, const wuiPropDef_t *tbl, const char *key ) {
	int i;
	for ( i = 0; tbl[i].key; i++ ) {
		if ( !Q_stricmp( key, tbl[i].key ) ) {
			char *ptr = (char *)base + tbl[i].off;
			switch ( tbl[i].type ) {
			case WP_INT:   { int v = 0; WiredPC_Int( handle, &v ); *(int *)ptr = v; break; }
			case WP_FLOAT: WiredPC_Float( handle, (float *)ptr ); break;
			case WP_STR:   { const char *s; if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ptr, s, tbl[i].size ); break; }
			case WP_COLOR: WiredPC_Color( handle, (vec4_t *)ptr ); break;
			case WP_FLAG:  *(qboolean *)ptr = qtrue; break;
			}
			return qtrue;
		}
	}
	return qfalse;
}

#define WP_I(k,T,f)   { k, WP_INT,   offsetof(T,f), 0 }
#define WP_F(k,T,f)   { k, WP_FLOAT, offsetof(T,f), 0 }
#define WP_S(k,T,f)   { k, WP_STR,   offsetof(T,f), sizeof(((T*)0)->f) }
#define WP_C(k,T,f)   { k, WP_COLOR, offsetof(T,f), 0 }
#define WP_FL(k,T,f)  { k, WP_FLAG,  offsetof(T,f), 0 }

static const wuiPropDef_t s_itemProps[] = {
	/* strings */
	WP_S( "name",             wiredItemDef_t, name             ),
	WP_S( "text",             wiredItemDef_t, text             ),
	WP_S( "group",            wiredItemDef_t, group            ),
	WP_S( "background",       wiredItemDef_t, background       ),
	WP_S( "cvar",             wiredItemDef_t, cvar             ),
	WP_S( "cvarTest",         wiredItemDef_t, cvarTest         ),
	WP_S( "populateCallback", wiredItemDef_t, populateCallback ),
	WP_S( "asset_model",      wiredItemDef_t, assetModel       ),
	WP_S( "asset_shader",     wiredItemDef_t, assetShader      ),
	WP_S( "hudElement",       wiredItemDef_t, hudElement       ),
	WP_S( "tooltip",          wiredItemDef_t, tooltip          ),
	WP_S( "image",            wiredItemDef_t, image            ),
	WP_S( "bindcolor",        wiredItemDef_t, storeBindColor   ),
	WP_S( "bindicon",         wiredItemDef_t, storeBindIcon    ),
	WP_S( "bindvalue",        wiredItemDef_t, storeBindValue   ),
	WP_S( "showbind",         wiredItemDef_t, showBind         ),
	WP_S( "hidebind",         wiredItemDef_t, hideBind         ),
	WP_S( "source",           wiredItemDef_t, tableSource      ),
	WP_S( "countbind",        wiredItemDef_t, tableCountBind   ),
	/* ints */
	WP_I( "type",             wiredItemDef_t, type             ),
	WP_I( "style",            wiredItemDef_t, style            ),
	WP_I( "textalign",        wiredItemDef_t, textalign        ),
	WP_I( "textstyle",        wiredItemDef_t, textstyle        ),
	WP_I( "border",           wiredItemDef_t, border           ),
	WP_I( "ownerdraw",        wiredItemDef_t, ownerdraw        ),
	WP_I( "ownerdrawFlag",    wiredItemDef_t, ownerdrawFlag    ),
	WP_I( "maxChars",         wiredItemDef_t, maxChars         ),
	WP_I( "maxPaintChars",    wiredItemDef_t, maxPaintChars    ),
	WP_I( "fadedelay",        wiredItemDef_t, fadeDelay        ),
	WP_I( "time",             wiredItemDef_t, timeMs           ),
	WP_I( "widescreen",       wiredItemDef_t, modelWidescreen  ),
	WP_I( "visible",          wiredItemDef_t, visible          ),
	WP_I( "teamfilter",       wiredItemDef_t, tableTeamFilter  ),
	/* floats */
	WP_F( "textalignx",       wiredItemDef_t, textalignx       ),
	WP_F( "textaligny",       wiredItemDef_t, textaligny       ),
	WP_F( "textscale",        wiredItemDef_t, textscale        ),
	WP_F( "bordersize",       wiredItemDef_t, bordersize       ),
	WP_F( "special",          wiredItemDef_t, special          ),
	WP_F( "feeder",           wiredItemDef_t, feeder           ),
	WP_F( "elementwidth",     wiredItemDef_t, elementwidth     ),
	WP_F( "elementheight",    wiredItemDef_t, elementheight    ),
	WP_F( "letterspacing",    wiredItemDef_t, letterSpacing    ),
	WP_F( "model_fovx",       wiredItemDef_t, modelFovX        ),
	WP_F( "model_fovy",       wiredItemDef_t, modelFovY        ),
	WP_F( "model_rotation",   wiredItemDef_t, modelRotation    ),
	WP_F( "model_angle",      wiredItemDef_t, modelAngle       ),
	/* colors */
	WP_C( "forecolor",        wiredItemDef_t, forecolor        ),
	WP_C( "backcolor",        wiredItemDef_t, backcolor        ),
	WP_C( "bordercolor",      wiredItemDef_t, bordercolor      ),
	WP_C( "outlinecolor",     wiredItemDef_t, outlinecolor     ),
	WP_C( "color2",           wiredItemDef_t, color2           ),
	WP_C( "fade",             wiredItemDef_t, fadeColor        ),
	/* flags (no value token consumed) */
	WP_FL( "decoration",      wiredItemDef_t, decoration       ),
	WP_FL( "notselectable",   wiredItemDef_t, notselectable    ),
	WP_FL( "horizontalscroll",wiredItemDef_t, horizontalScroll ),
	WP_FL( "fill",            wiredItemDef_t, fillFlag         ),
	WP_FL( "monospace",       wiredItemDef_t, monospace        ),
	{ NULL, 0, 0, 0 }
};

static const wuiPropDef_t s_menuProps[] = {
	/* strings */
	WP_S( "name",             wiredMenuDef_t, name             ),
	WP_S( "background",       wiredMenuDef_t, background       ),
	WP_S( "soundLoop",        wiredMenuDef_t, soundLoop        ),
	/* ints (qboolean fields are typedef int, WP_INT works) */
	WP_I( "style",            wiredMenuDef_t, style            ),
	WP_I( "border",           wiredMenuDef_t, border           ),
	WP_I( "fadeCycle",        wiredMenuDef_t, fadeCycle        ),
	WP_I( "fullscreen",       wiredMenuDef_t, fullscreen       ),
	WP_I( "visible",          wiredMenuDef_t, visible          ),
	WP_I( "hudOverlay",       wiredMenuDef_t, hudOverlay       ),
	/* floats */
	WP_F( "bordersize",       wiredMenuDef_t, bordersize       ),
	WP_F( "fadeClamp",        wiredMenuDef_t, fadeClamp        ),
	WP_F( "fadeAmount",       wiredMenuDef_t, fadeAmount       ),
	/* colors */
	WP_C( "forecolor",        wiredMenuDef_t, forecolor        ),
	WP_C( "backcolor",        wiredMenuDef_t, backcolor        ),
	WP_C( "focuscolor",       wiredMenuDef_t, focuscolor       ),
	WP_C( "bordercolor",      wiredMenuDef_t, bordercolor      ),
	WP_C( "disablecolor",     wiredMenuDef_t, disablecolor     ),
	/* flags */
	WP_FL( "modal",           wiredMenuDef_t, modal            ),
	WP_FL( "alwaysontop",     wiredMenuDef_t, alwaysOnTop      ),
	WP_FL( "popup",           wiredMenuDef_t, popup            ),
	WP_FL( "outOfBoundsClick",wiredMenuDef_t, outOfBoundsClick ),
	{ NULL, 0, 0, 0 }
};

static qboolean WiredUI_ParseItemProperties( int handle, wiredItemDef_t *item ) {
	pc_token_t      token;
	const char     *str;

	// expect opening brace
	if ( !WiredPC_Expect( handle, "{" ) ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: expected '{' for itemDef\n" );
		return qfalse;
	}

	while ( 1 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) {
			return qfalse;
		}
		if ( !Q_stricmp( token.string, "}" ) ) {
			break;
		}

		// ── item keywords ─────────────────────────────────────────
		if ( WiredPC_ApplyProp( handle, item, s_itemProps, token.string ) ) {
			/* consumed by table */
		}
		else if ( !Q_stricmp( token.string, "rect" ) ) {
			WiredPC_ParseRectInto( handle, &item->wuiRect, &item->rect );
		}
		else if ( !Q_stricmp( token.string, "cvarFloat" ) ) {
			// cvarFloat "cvarname" defVal minVal maxVal
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->cvar, str, sizeof( item->cvar ) );
			WiredPC_Float( handle, &item->sliderData.defVal );
			WiredPC_Float( handle, &item->sliderData.minVal );
			WiredPC_Float( handle, &item->sliderData.maxVal );
		}
		else if ( !Q_stricmp( token.string, "cvarStrList" ) ) {
			// cvarStrList { "Label1" "value1" "Label2" "value2" ... }
			item->multiData = (wiredMultiDef_t *)WiredUI_Alloc( sizeof( wiredMultiDef_t ) );
			if ( item->multiData ) {
				item->multiData->isStringList = qtrue;
				item->multiData->count = 0;
				if ( !WiredPC_Expect( handle, "{" ) ) continue;
				while ( item->multiData->count < WIRED_MAX_MULTI_CHOICES ) {
					if ( !WiredPC_ReadToken( handle, &token ) ) break;
					if ( !Q_stricmp( token.string, "}" ) ) break;
					Q_strncpyz( item->multiData->labels[item->multiData->count], token.string,
						sizeof( item->multiData->labels[0] ) );
					if ( !WiredPC_ReadToken( handle, &token ) ) break;
					if ( !Q_stricmp( token.string, "}" ) ) break;
					Q_strncpyz( item->multiData->strValues[item->multiData->count], token.string,
						sizeof( item->multiData->strValues[0] ) );
					item->multiData->count++;
				}
			}
		}
		else if ( !Q_stricmp( token.string, "cvarFloatList" ) ) {
			// cvarFloatList { "Label1" value1 "Label2" value2 ... }
			item->multiData = (wiredMultiDef_t *)WiredUI_Alloc( sizeof( wiredMultiDef_t ) );
			if ( item->multiData ) {
				item->multiData->isStringList = qfalse;
				item->multiData->count = 0;
				if ( !WiredPC_Expect( handle, "{" ) ) continue;
				while ( item->multiData->count < WIRED_MAX_MULTI_CHOICES ) {
					if ( !WiredPC_ReadToken( handle, &token ) ) break;
					if ( !Q_stricmp( token.string, "}" ) ) break;
					Q_strncpyz( item->multiData->labels[item->multiData->count], token.string,
						sizeof( item->multiData->labels[0] ) );
					if ( !WiredPC_ReadToken( handle, &token ) ) break;
					if ( !Q_stricmp( token.string, "}" ) ) break;
					item->multiData->floatValues[item->multiData->count] = atof( token.string );
					item->multiData->count++;
				}
			}
		}
		else if ( !Q_stricmp( token.string, "columns" ) ) {
			// columns N pos1 width1 maxchars1 pos2 width2 maxchars2 ...
			int numCols, c;
			WiredPC_Int( handle, &numCols );
			if ( numCols > 8 ) numCols = 8;
			item->columns = numCols;
			for ( c = 0; c < numCols; c++ ) {
				int pos, width, maxchars;
				WiredPC_Int( handle, &pos );     // column position (unused for now)
				WiredPC_Int( handle, &width );
				WiredPC_Int( handle, &maxchars ); // max chars per column (unused for now)
				item->columnWidths[c] = width;
			}
		}
		else if ( !Q_stricmp( token.string, "elementtype" ) ) {
			int et;
			WiredPC_Int( handle, &et ); // LISTBOX_TEXT or LISTBOX_IMAGE — store if needed
		}
		else if ( !Q_stricmp( token.string, "anchor" ) ) {
			WiredPC_ParseAnchorInto( handle, &item->anchor );
		}
		else if ( !Q_stricmp( token.string, "textoffset" ) ) {
			WiredPC_Float( handle, &item->textoffsetX );
			WiredPC_Float( handle, &item->textoffsetY );
		}
		else if ( !Q_stricmp( token.string, "addColorRange" ) ) {
			if ( item->numColorRanges < WIRED_MAX_COLOR_RANGES ) {
				int idx = item->numColorRanges;
				WiredPC_Float( handle, &item->colorRanges[idx].low );
				WiredPC_Float( handle, &item->colorRanges[idx].high );
				WiredPC_Color( handle, &item->colorRanges[idx].color );
				item->numColorRanges++;
			} else {
				float dummy; vec4_t dc;
				WiredPC_Float( handle, &dummy ); WiredPC_Float( handle, &dummy );
				WiredPC_Color( handle, &dc );
			}
		}
		else if ( !Q_stricmp( token.string, "enableCvar" ) || !Q_stricmp( token.string, "disableCvar" ) ) {
			char *dest2    = !Q_stricmp( token.string, "enableCvar" ) ? item->enableCvar  : item->disableCvar;
			int destSize2  = !Q_stricmp( token.string, "enableCvar" ) ? sizeof( item->enableCvar ) : sizeof( item->disableCvar );
			if ( !WiredPC_CaptureBracedScript( handle, dest2, destSize2 ) ) continue;
		}
		else if ( !Q_stricmp( token.string, "model_origin" ) ) {
			WiredPC_Float( handle, &item->modelOrigin[0] );
			WiredPC_Float( handle, &item->modelOrigin[1] );
			WiredPC_Float( handle, &item->modelOrigin[2] );
		}
		else if ( !Q_stricmp( token.string, "bind" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				Q_strncpyz( item->bind, str, sizeof( item->bind ) );
				Q_strncpyz( item->storeBind, str, sizeof( item->storeBind ) );
			}
		}
		else if ( !Q_stricmp( token.string, "font" ) ) {
			// font "name" [pointsize]
			// ModernHUD items: font name only (fontsize parsed separately)
			// Native format (.wmenu/.whud): font "name" pointsize
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->fontName, str, sizeof( item->fontName ) );
			if ( !item->hudElement[0] ) {
				WiredPC_Float( handle, &item->fontPointSize );
			}
		}
		else if ( !Q_stricmp( token.string, "fontsize" ) ) {
			WiredPC_Float( handle, &item->fontSize[0] );
			WiredPC_Float( handle, &item->fontSize[1] );
			// Legacy fontsize W+H — keep as-is for render loop
		}
		else if ( !Q_stricmp( token.string, "fontweight" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				int w = WiredPC_LookupEnum( s_fontWeightMap, str, WUI_ENUM_UNKNOWN );
				item->fontWeight = ( w != WUI_ENUM_UNKNOWN ) ? w : atoi( str );
			}
		}
		else if ( !Q_stricmp( token.string, "direction" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				int d = WiredPC_LookupEnum( s_directionMap, str, WUI_ENUM_UNKNOWN );
				item->direction = ( d != WUI_ENUM_UNKNOWN ) ? d : atoi( str );
			}
		}
		else if ( !Q_stricmp( token.string, "alignv" ) || !Q_stricmp( token.string, "alignV" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				int v = WiredPC_LookupEnum( s_alignVMap, str, WUI_ENUM_UNKNOWN );
				item->alignV = ( v != WUI_ENUM_UNKNOWN ) ? v : atoi( str );
			}
		}
		else if ( !Q_stricmp( token.string, "action" ) ||
		          !Q_stricmp( token.string, "onFocus" ) ||
		          !Q_stricmp( token.string, "leaveFocus" ) ||
		          !Q_stricmp( token.string, "mouseEnter" ) ||
		          !Q_stricmp( token.string, "mouseExit" ) ||
		          !Q_stricmp( token.string, "onEsc" ) ||
		          !Q_stricmp( token.string, "onEnter" ) ||
		          !Q_stricmp( token.string, "onTab" ) ||
		          !Q_stricmp( token.string, "doubleclick" ) ) {
			// capture script block into the appropriate field
			char *dest = NULL;
			int destSize = WIRED_MAX_SCRIPT_LEN;

			if ( !Q_stricmp( token.string, "action" ) )       dest = item->action;
			else if ( !Q_stricmp( token.string, "onFocus" ) )  dest = item->onFocus;
			else if ( !Q_stricmp( token.string, "leaveFocus" )) dest = item->leaveFocus;
			else if ( !Q_stricmp( token.string, "mouseEnter" )) dest = item->mouseEnter;
			else if ( !Q_stricmp( token.string, "mouseExit" )) dest = item->mouseExit;
			else if ( !Q_stricmp( token.string, "onEsc" ) )   dest = item->onEsc;
			else if ( !Q_stricmp( token.string, "onEnter" ) ) dest = item->onEnter;
			else if ( !Q_stricmp( token.string, "onTab" ) )   dest = item->onTab;
			else if ( !Q_stricmp( token.string, "doubleclick" )) dest = item->doubleClick;

			if ( !WiredPC_CaptureBracedScript( handle, dest, destSize ) ) continue;
		}
		else if ( !Q_stricmp( token.string, "execKey" ) ) {
			const char *keyStr;
			if ( WiredPC_String( handle, &keyStr ) ) {
				item->execKeyCode = keyStr[0];
			}
			if ( !WiredPC_CaptureBracedScript( handle, item->execKeyAction, WIRED_MAX_SCRIPT_LEN ) ) continue;
		}
		else if ( !Q_stricmp( token.string, "showCvar" ) || !Q_stricmp( token.string, "hideCvar" ) ) {
			char *dest    = !Q_stricmp( token.string, "showCvar" ) ? item->showCvar  : item->hideCvar;
			int destSize  = !Q_stricmp( token.string, "showCvar" ) ? sizeof( item->showCvar ) : sizeof( item->hideCvar );
			if ( !WiredPC_CaptureBracedScript( handle, dest, destSize ) ) continue;
		}
		// ── flex container keywords ──────────────────────────────────
		else if ( WiredPC_ParseFlexProps( handle, token.string, &item->flexContainer, &item->isFlexContainer ) ) {
			/* consumed */
		}
		// ── positioning mode ──────────────────────────────────────────
		else if ( !Q_stricmp( token.string, "position" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) {
				if ( !Q_stricmp( val.string, "absolute" ) ) item->position = POSITION_ABSOLUTE;
				else if ( !Q_stricmp( val.string, "viewport" ) ) item->position = POSITION_VIEWPORT;
				else item->position = POSITION_STATIC;
			}
		}
		// ── Layer 2: flex child keywords ──────────────────────────────
		else if ( !Q_stricmp( token.string, "grow" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) item->flexChild.grow = val.floatvalue;
		}
		else if ( !Q_stricmp( token.string, "shrink" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) item->flexChild.shrink = val.floatvalue;
		}
		else if ( !Q_stricmp( token.string, "basis" ) ) {
			item->flexChild.basis = WiredPC_ParseValue( handle );
		}
		else if ( !Q_stricmp( token.string, "alignSelf" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) {
				item->flexChild.alignSelf = (wuiAlign_t)WiredPC_LookupEnum( s_alignMap, val.string, WUI_ALIGN_START );
			}
		}
		else if ( !Q_stricmp( token.string, "aspect" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) {
				const char *slash;
				item->aspect.active = qtrue;
				// Check for "N/M" format (e.g. "16/9")
				slash = strchr( val.string, '/' );
				if ( slash ) {
					float num = atof( val.string );
					float den = atof( slash + 1 );
					item->aspect.ratio = ( den > 0 ) ? num / den : 1.0f;
				} else {
					item->aspect.ratio = val.floatvalue;
					if ( item->aspect.ratio <= 0 ) item->aspect.ratio = 1.0f;
				}
			}
		}
		else if ( !Q_stricmp( token.string, "minWidth" ) ) {
			item->flexChild.minWidth = WiredPC_ParseValue( handle );
		}
		else if ( !Q_stricmp( token.string, "maxWidth" ) ) {
			item->flexChild.maxWidth = WiredPC_ParseValue( handle );
		}
		else if ( !Q_stricmp( token.string, "minHeight" ) ) {
			item->flexChild.minHeight = WiredPC_ParseValue( handle );
		}
		else if ( !Q_stricmp( token.string, "maxHeight" ) ) {
			item->flexChild.maxHeight = WiredPC_ParseValue( handle );
		}
		// ── Layer 2: nested itemDef (flex container children) ─────────
		else if ( !Q_stricmp( token.string, "itemDef" ) ) {
			if ( item->isFlexContainer && item->childCount < WIRED_MAX_ITEMS_PER_MENU ) {
				wiredItemDef_t *child = (wiredItemDef_t *)WiredUI_Alloc( sizeof( wiredItemDef_t ) );
				if ( child ) {
					memset( child, 0, sizeof( *child ) );
					// defaults for nested child
					child->visible = qtrue;
					child->forecolor[0] = child->forecolor[1] = child->forecolor[2] = child->forecolor[3] = 1.0f;
					child->textscale = 0.3f;
					child->textalign = -1;
					child->alignV = -1;
					child->direction = -1;
					child->fadeAlphaItem = 1.0f;
					child->anchor = ANCHOR_NONE;
					child->flexChild.shrink = 1.0f;
					if ( WiredUI_ParseItemProperties( handle, child ) ) {
						item->children[item->childCount++] = child;
					}
				}
			} else {
				// Not a flex container or children full — skip the block
				if ( WiredPC_Expect( handle, "{" ) ) {
					WiredPC_SkipBracedBlock( handle );
				}
			}
		}
		// ── Layer 5: transition animation ─────────────────────────────
		else if ( !Q_stricmp( token.string, "transition" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) {
				item->wuiTransition.duration = val.intvalue;
			}
			/* optional easing keyword — push back if not recognised */
			if ( WiredPC_ReadToken( handle, &val ) ) {
				int e = WiredPC_LookupEnum( s_easingMap, val.string, WUI_ENUM_UNKNOWN );
				if ( e != WUI_ENUM_UNKNOWN ) item->wuiTransition.easing = (wuiEasing_t)e;
				else WiredPC_UnreadToken( &val );
			}
		}
		// ── Layer 5: responsive breakpoint ────────────────────────────
		else if ( !Q_stricmp( token.string, "breakpoint" ) ) {
			if ( item->breakpointCount < WUI_MAX_BREAKPOINTS ) {
				wuiBreakpoint_t *bp = &item->breakpoints[item->breakpointCount];
				bp->active = qtrue;
				// Parse: breakpoint minW maxW { rect ... }
				pc_token_t val;
				if ( WiredPC_ReadTokenEval( handle, &val ) ) bp->minWidth = val.intvalue;
				if ( WiredPC_ReadTokenEval( handle, &val ) ) bp->maxWidth = val.intvalue;
				// Expect opening brace
				if ( WiredPC_ReadToken( handle, &val ) && val.string[0] == '{' ) {
					// Parse inner properties (currently only rect)
					while ( WiredPC_ReadToken( handle, &val ) ) {
						if ( val.string[0] == '}' ) break;
						if ( !Q_stricmp( val.string, "rect" ) ) {
							bp->rect.x = WiredPC_ParseValue( handle );
							bp->rect.y = WiredPC_ParseValue( handle );
							bp->rect.w = WiredPC_ParseValue( handle );
							bp->rect.h = WiredPC_ParseValue( handle );
						}
					}
				}
				item->breakpointCount++;
			}
		}
		else if ( !Q_stricmp( token.string, "column" ) ) {
			/* column { field "name" header "Player" width 0.25 align 0 colorfield "namecolor" } */
			if ( item->numTableColumns < WUI_TABLE_MAX_COLUMNS ) {
				pc_token_t subToken;
				wuiTableColumn_t *col = &item->tableColumns[item->numTableColumns];
				memset( col, 0, sizeof( *col ) );
				col->align = 0; /* default left */

				if ( WiredPC_ReadToken( handle, &subToken ) && !Q_stricmp( subToken.string, "{" ) ) {
					while ( WiredPC_ReadToken( handle, &subToken ) ) {
						if ( !Q_stricmp( subToken.string, "}" ) ) {
							break;
						}
						if ( !Q_stricmp( subToken.string, "field" ) ) {
							if ( WiredPC_String( handle, &str ) )
								Q_strncpyz( col->field, str, sizeof( col->field ) );
						}
						else if ( !Q_stricmp( subToken.string, "header" ) ) {
							if ( WiredPC_String( handle, &str ) )
								Q_strncpyz( col->header, str, sizeof( col->header ) );
						}
						else if ( !Q_stricmp( subToken.string, "width" ) ) {
							float fw;
							if ( WiredPC_Float( handle, &fw ) )
								col->width = fw;
						}
						else if ( !Q_stricmp( subToken.string, "align" ) ) {
							int av;
							if ( WiredPC_Int( handle, &av ) )
								col->align = av;
						}
						else if ( !Q_stricmp( subToken.string, "colorfield" ) ) {
							if ( WiredPC_String( handle, &str ) )
								Q_strncpyz( col->colorfield, str, sizeof( col->colorfield ) );
						}
						else if ( !Q_stricmp( subToken.string, "iconfield" ) ) {
							if ( WiredPC_String( handle, &str ) )
								Q_strncpyz( col->iconfield, str, sizeof( col->iconfield ) );
						}
					}
					item->numTableColumns++;
				}
			}
		}
		else {
			/* unknown keyword -- smart skip to avoid poisoning subsequent parsing */
			Com_DPrintf( "WiredUI: unknown item keyword '%s'\n", token.string );
			if ( WiredPC_ReadToken( handle, &token ) ) {
				if ( !Q_stricmp( token.string, "{" ) ) {
					WiredPC_SkipBracedBlock( handle );
				}
				/* else: consumed one token (the value) -- good enough */
			}
		}
	}

	return qtrue;
}

// ── menu parser ───────────────────────────────────────────────────────

static qboolean WiredUI_ParseMenu( int handle ) {
	pc_token_t       token;
	wiredMenuDef_t  *menu;
	const char      *str;

	menu = (wiredMenuDef_t *)WiredUI_Alloc( sizeof( wiredMenuDef_t ) );
	if ( !menu ) {
		return qfalse;
	}

	// defaults
	menu->visible = qtrue;
	menu->anchor = ANCHOR_NONE;
	menu->cinematicHandle = -1;
	menu->focuscolor[0] = 1.0f;
	menu->focuscolor[1] = 0.75f;
	menu->focuscolor[2] = 0.0f;
	menu->focuscolor[3] = 1.0f;

	// expect opening brace
	if ( !WiredPC_Expect( handle, "{" ) ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: expected '{' for menuDef\n" );
		return qfalse;
	}

	while ( 1 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) {
			return qfalse;
		}
		if ( !Q_stricmp( token.string, "}" ) ) {
			break;
		}

		// ── menu keywords ─────────────────────────────────────────
		if ( WiredPC_ApplyProp( handle, menu, s_menuProps, token.string ) ) {
			/* consumed by table */
		}
		else if ( !Q_stricmp( token.string, "rect" ) ) {
			WiredPC_ParseRectInto( handle, &menu->wuiRect, &menu->rect );
		}
		else if ( !Q_stricmp( token.string, "onOpen" ) ||
		          !Q_stricmp( token.string, "onClose" ) ||
		          !Q_stricmp( token.string, "onESC" ) ) {
			char *dest = !Q_stricmp( token.string, "onOpen" )  ? menu->onOpen  :
			             !Q_stricmp( token.string, "onClose" ) ? menu->onClose : menu->onESC;
			if ( !WiredPC_CaptureBracedScript( handle, dest, WIRED_MAX_SCRIPT_LEN ) ) continue;
		}
		else if ( !Q_stricmp( token.string, "anchor" ) ) {
			WiredPC_ParseAnchorInto( handle, &menu->anchor );
		}
		else if ( !Q_stricmp( token.string, "font" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( menu->font, str, sizeof( menu->font ) );
			// optional point size follows
			if ( WiredPC_ReadToken( handle, &token ) ) {
				if ( token.string[0] >= '0' && token.string[0] <= '9' ) {
					// consumed point size
				} else {
					// not a number — push back by re-parsing
					// botlib doesn't support pushback, so just ignore
				}
			}
		}
		else if ( !Q_stricmp( token.string, "cinematic" ) ) {
			if ( WiredPC_ReadToken( handle, &token ) )
				Q_strncpyz( menu->cinematic, token.string, sizeof( menu->cinematic ) );
		}
		else if ( !Q_stricmp( token.string, "ownerdraw" ) ) {
			int od; WiredPC_Int( handle, &od ); // menu-level ownerdraw (rare)
		}
		else if ( !Q_stricmp( token.string, "ownerdrawFlag" ) ) {
			int odf; WiredPC_Int( handle, &odf );
		}
		// ── flex container keywords ──────────────────────────────────
		else if ( WiredPC_ParseFlexProps( handle, token.string, &menu->flexContainer, &menu->isFlexContainer ) ) {
			/* consumed */
		}
		else if ( !Q_stricmp( token.string, "itemDef" ) ) {
			if ( !WiredUI_ParseItem( handle, menu ) ) {
				Com_Printf( S_COLOR_YELLOW "WiredUI: failed to parse itemDef in menu '%s'\n", menu->name );
			}
		}
		else {
			// unknown keyword — smart skip to avoid poisoning subsequent parsing
			Com_DPrintf( "WiredUI: unknown menu keyword '%s'\n", token.string );
			if ( WiredPC_ReadToken( handle, &token ) ) {
				if ( !Q_stricmp( token.string, "{" ) ) {
					WiredPC_SkipBracedBlock( handle );
				}
				// else: consumed one token value
			}
		}
	}

	// compute content height for scroll support
	{
		int ci;
		float maxBottom = 0;
		for ( ci = 0; ci < menu->itemCount; ci++ ) {
			float bottom = menu->items[ci]->rect.y + menu->items[ci]->rect.h;
			if ( bottom > maxBottom ) maxBottom = bottom;
		}
		menu->contentHeight = maxBottom;
		menu->scrollOffset = 0;
		menu->scrollVelocity = 0;
		menu->scrollBarFadeTime = 0;
	}

	if ( wui_menuCount < WIRED_MAX_MENUS ) {
		wui_menus[wui_menuCount++] = menu;
		Com_DPrintf( "WiredUI: loaded menu '%s' (%d items)\n", menu->name, menu->itemCount );
	}

	return qtrue;
}

// ── file loader ───────────────────────────────────────────────────────

qboolean WiredUI_LoadMenuFile( const char *filename ) {
	int         handle;
	pc_token_t  token;

	handle = WiredPC_LoadSource( filename );
	if ( !handle ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: could not load '%s'\n", filename );
		return qfalse;
	}

	Com_DPrintf( "WiredUI: loading native format '%s'\n", filename );

	while ( 1 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) {
			break;
		}

		if ( !Q_stricmp( token.string, "menuDef" ) ) {
			WiredUI_ParseMenu( handle );
		}
		else if ( !Q_stricmp( token.string, "assetGlobalDef" ) ) {
			wiredAssetGlobals_t *ag = WiredUI_GetAssetGlobals();
			if ( !WiredPC_Expect( handle, "{" ) ) {
				Com_Printf( S_COLOR_YELLOW "WiredUI: expected '{' for assetGlobalDef\n" );
				continue;
			}
			while ( 1 ) {
				const char *s;
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "}" ) ) break;

				if      ( !Q_stricmp( token.string, "cursor" ) )
					{ if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ag->cursor, s, sizeof( ag->cursor ) ); }
				else if ( !Q_stricmp( token.string, "gradientBar" ) )
					{ if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ag->gradientBar, s, sizeof( ag->gradientBar ) ); }
				else if ( !Q_stricmp( token.string, "itemFocusSound" ) )
					{ if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ag->focusSound, s, sizeof( ag->focusSound ) ); }
				else if ( !Q_stricmp( token.string, "radialGlow" ) )
					{ if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ag->radialGlowShader, s, sizeof( ag->radialGlowShader ) ); }
				else if ( !Q_stricmp( token.string, "defaultSerifFont" ) )
					{ if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ag->defaultSerifFontName, s, sizeof( ag->defaultSerifFontName ) ); }
				else if ( !Q_stricmp( token.string, "defaultSerifFontItalic" ) )
					{ if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ag->defaultSerifFontItalicName, s, sizeof( ag->defaultSerifFontItalicName ) ); }
				else if ( !Q_stricmp( token.string, "defaultSansFont" ) )
					{ if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ag->defaultSansFontName, s, sizeof( ag->defaultSansFontName ) ); }
				else if ( !Q_stricmp( token.string, "defaultSansFontMedium" ) )
					{ if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ag->defaultSansFontMediumName, s, sizeof( ag->defaultSansFontMediumName ) ); }
				else if ( !Q_stricmp( token.string, "defaultMonoFont" ) )
					{ if ( WiredPC_String( handle, &s ) ) Q_strncpyz( ag->defaultMonoFontName, s, sizeof( ag->defaultMonoFontName ) ); }
				else if ( !Q_stricmp( token.string, "fadeClamp" ) )   WiredPC_Float( handle, &ag->fadeClamp );
				else if ( !Q_stricmp( token.string, "fadeAmount" ) )  WiredPC_Float( handle, &ag->fadeAmount );
				else if ( !Q_stricmp( token.string, "shadowX" ) )     WiredPC_Float( handle, &ag->shadowX );
				else if ( !Q_stricmp( token.string, "shadowY" ) )     WiredPC_Float( handle, &ag->shadowY );
				else if ( !Q_stricmp( token.string, "fadeCycle" ) )   WiredPC_Int( handle, &ag->fadeCycle );
				else if ( !Q_stricmp( token.string, "shadowColor" ) )      WiredPC_Color( handle, &ag->shadowColor );
				else if ( !Q_stricmp( token.string, "focusColor" ) )       WiredPC_Color( handle, &ag->focusColor );
				else if ( !Q_stricmp( token.string, "gradientBarColor" ) ) WiredPC_Color( handle, &ag->gradientBarColor );
			}
			Com_DPrintf( "WiredUI: parsed assetGlobalDef (cursor=%s)\n", ag->cursor );
		}
		else if ( !Q_stricmp( token.string, "{" ) || !Q_stricmp( token.string, "}" ) ) {
			// top-level braces — Q3:TA/QL files wrap content in { }. Just ignore.
		}
		else {
			Com_DPrintf( "WiredUI: unexpected token '%s' in '%s'\n", token.string, filename );
		}
	}

	WiredPC_FreeSource( handle );
	return qtrue;
}

// menus.txt parser removed — menus are now loaded exclusively from
// scripts/menus.lua via WiredUI_LoadMenusFromLua / load_menu() Lua binding.

// ── public accessors ──────────────────────────────────────────────────

int WiredUI_GetMenuCount( void ) {
	return wui_menuCount;
}

wiredMenuDef_t *WiredUI_GetMenuByIndex( int index ) {
	if ( index < 0 || index >= wui_menuCount ) return NULL;
	return wui_menus[index];
}

wiredMenuDef_t *WiredUI_FindMenu( const char *name ) {
	int i;
	for ( i = 0; i < wui_menuCount; i++ ) {
		if ( !Q_stricmp( wui_menus[i]->name, name ) ) {
			return wui_menus[i];
		}
	}
	return NULL;
}

void WiredUI_ClearMenus( void ) {
	wui_menuCount = 0;
	WiredUI_ResetPool();
}

// ── two-phase safe reload ─────────────────────────────────────────────
// Parses menus into a fresh pool. If parsing succeeds, the new menus
// become active. If parsing fails, the old menus are restored.

typedef struct {
	char             pool[WIRED_MENU_POOL_SIZE];
	int              poolUsed;
	wiredMenuDef_t  *menus[WIRED_MAX_MENUS];
	int              menuCount;
	wiredAssetGlobals_t assetGlobals;
} wiredMenuBackup_t;

static wiredMenuBackup_t *wired_backup = NULL;  // heap-allocated on demand

qboolean WiredUI_SafeReload( void ) {
	// allocate backup on first use (8MB+ — too large for stack)
	if ( !wired_backup ) {
		wired_backup = Z_Malloc( sizeof( wiredMenuBackup_t ) );
	}

	// phase 1: save current state
	memcpy( wired_backup->pool, wui_menuPool, wui_menuPoolUsed );
	wired_backup->poolUsed = wui_menuPoolUsed;
	memcpy( wired_backup->menus, wui_menus, sizeof( wui_menus[0] ) * wui_menuCount );
	wired_backup->menuCount = wui_menuCount;
	wired_backup->assetGlobals = *WiredUI_GetAssetGlobals();

	// phase 2: clear and reparse from menus.lua
	WiredUI_ResetAssetGlobalsDefaults();
	WiredUI_ClearMenus();
	WiredUI_LoadMenusFromLua();
	qboolean ok = ( wui_menuCount > 0 );

	if ( !ok ) {
		// parse failed — restore old menus
		Com_Printf( S_COLOR_YELLOW "Menu reload failed — keeping old menus.\n" );
		memcpy( wui_menuPool, wired_backup->pool, wired_backup->poolUsed );
		wui_menuPoolUsed = wired_backup->poolUsed;
		memcpy( wui_menus, wired_backup->menus, sizeof( wui_menus[0] ) * wired_backup->menuCount );
		wui_menuCount = wired_backup->menuCount;
		*WiredUI_GetAssetGlobals() = wired_backup->assetGlobals;
		return qfalse;
	}

	// parse succeeded — new menus are now active
	return qtrue;
}

// ── menus.lua support ─────────────────────────────────────────────────
// load_menu(path) Lua binding. Registered before WiredScript_PostInit so
// it is available when scripts/menus.lua executes during WiredUI_Init.

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "../../../qcommon/wired/scripting/wired_scripting.h"

static int WiredMenuLua_LoadMenu( lua_State *L ) {
	const char *path = luaL_checkstring( L, 1 );
	if ( path[0] == '\0' || strlen( path ) >= MAX_QPATH ) {
		return luaL_error( L, "load_menu: invalid path" );
	}
	WiredUI_LoadMenuFile( path );
	return 0;
}

static const luaL_Reg s_menuLuaLib[] = {
	{ NULL, NULL }
};

static void WiredMenuLua_Register( lua_State *L ) {
	/* Expose load_menu as a plain global function, not a table method. */
	lua_pushcfunction( L, WiredMenuLua_LoadMenu );
	lua_setglobal( L, "load_menu" );
}

void WiredUI_MenuLuaInit( void ) {
	WiredScript_RegisterBindings( WiredMenuLua_Register );
}

/* Execute scripts/menus.lua to populate the menu pool. */
void WiredUI_LoadMenusFromLua( void ) {
	WiredScript_ExecFile( "scripts/menus.lua" );
}

#endif // FEAT_WIRED_UI
