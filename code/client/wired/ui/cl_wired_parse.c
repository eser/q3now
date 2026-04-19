/*
===========================================================================
cl_wired_parse.c — Wired UI: menu file parser

Parses .menu and .hud files using the engine's botlib PC token parser.
This is a simplified extraction of the v6 parser from code/ui/ui_shared.c,
adapted to run directly in the client without a VM.

File format reference:
  menus.txt manifest → lists .menu files to load
  .menu files → menuDef { properties... itemDef { properties... } }
  .hud files → same syntax with hudOverlay flag (Phase 3)
===========================================================================
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

static char wired_menuPool[WIRED_MENU_POOL_SIZE];
static int  wired_menuPoolUsed = 0;

static void *WiredUI_Alloc( int size ) {
	char *p;

	if ( wired_menuPoolUsed + size > WIRED_MENU_POOL_SIZE ) {
		Com_Printf( S_COLOR_RED "WiredUI_Alloc: pool exhausted (%d + %d > %d)\n",
			wired_menuPoolUsed, size, WIRED_MENU_POOL_SIZE );
		return NULL;
	}

	p = &wired_menuPool[wired_menuPoolUsed];
	wired_menuPoolUsed += ( ( size + 31 ) & ~31 );  // 32-byte align
	Com_Memset( p, 0, size );
	return p;
}

void WiredUI_ResetPool( void ) {
	wired_menuPoolUsed = 0;
	Com_Memset( wired_menuPool, 0, sizeof( wired_menuPool ) );
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
static pc_token_t wired_pendingToken;
static qboolean   wired_hasPendingToken = qfalse;

static int WiredPC_ReadToken( int handle, pc_token_t *token ) {
	if ( wired_hasPendingToken ) {
		*token = wired_pendingToken;
		wired_hasPendingToken = qfalse;
		return 1;
	}
	return botlib_export->PC_ReadTokenHandle( handle, token );
}

static void WiredPC_UnreadToken( pc_token_t *token ) {
	wired_pendingToken = *token;
	wired_hasPendingToken = qtrue;
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
		qboolean isInt = ( token->string[5] == 'i' );
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

static wiredMenuDef_t *wired_menus[WIRED_MAX_MENUS];
static int              wired_menuCount = 0;

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

static qboolean WiredUI_ParseItemProperties( int handle, wiredItemDef_t *item ) {
	pc_token_t      token;
	const char     *str;
	int             depth;

	// expect opening brace
	if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) {
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
		if ( !Q_stricmp( token.string, "name" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->name, str, sizeof( item->name ) );
		}
		else if ( !Q_stricmp( token.string, "text" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->text, str, sizeof( item->text ) );
		}
		else if ( !Q_stricmp( token.string, "group" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->group, str, sizeof( item->group ) );
		}
		else if ( !Q_stricmp( token.string, "type" ) ) {
			WiredPC_Int( handle, &item->type );
		}
		else if ( !Q_stricmp( token.string, "rect" ) ) {
			// .wmenu: parse with unit awareness
			item->wuiRect.x = WiredPC_ParseValue( handle );
			item->wuiRect.y = WiredPC_ParseValue( handle );
			item->wuiRect.w = WiredPC_ParseValue( handle );
			item->wuiRect.h = WiredPC_ParseValue( handle );
			// Back-fill old rect for draw code (real screen pixels)
			item->rect.x = WUI_BackfillToScreen( item->wuiRect.x, (float)cls.glconfig.vidWidth );
			item->rect.y = WUI_BackfillToScreen( item->wuiRect.y, (float)cls.glconfig.vidHeight );
			item->rect.w = WUI_BackfillToScreen( item->wuiRect.w, (float)cls.glconfig.vidWidth );
			item->rect.h = WUI_BackfillToScreen( item->wuiRect.h, (float)cls.glconfig.vidHeight );
		}
		else if ( !Q_stricmp( token.string, "style" ) ) {
			WiredPC_Int( handle, &item->style );
		}
		else if ( !Q_stricmp( token.string, "textalign" ) ) {
			WiredPC_Int( handle, &item->textalign );
		}
		else if ( !Q_stricmp( token.string, "textalignx" ) ) {
			WiredPC_Float( handle, &item->textalignx );
			// Legacy .menu textalignx values are in pixels — keep as-is
			// .wmenu uses textoffset instead, textalignx not used in native format
		}
		else if ( !Q_stricmp( token.string, "textaligny" ) ) {
			WiredPC_Float( handle, &item->textaligny );
			// Legacy .menu textaligny values are in pixels — keep as-is
		}
		else if ( !Q_stricmp( token.string, "textscale" ) ) {
			WiredPC_Float( handle, &item->textscale );
			// Legacy .menu textscale is 0.0-1.0 fraction — keep as-is for render loop
			// .wmenu uses font "name" pointsize instead, textscale not used in native format
		}
		else if ( !Q_stricmp( token.string, "textstyle" ) ) {
			WiredPC_Int( handle, &item->textstyle );
		}
		else if ( !Q_stricmp( token.string, "forecolor" ) ) {
			WiredPC_Color( handle, &item->forecolor );
		}
		else if ( !Q_stricmp( token.string, "backcolor" ) ) {
			WiredPC_Color( handle, &item->backcolor );
		}
		else if ( !Q_stricmp( token.string, "bordercolor" ) ) {
			WiredPC_Color( handle, &item->bordercolor );
		}
		else if ( !Q_stricmp( token.string, "border" ) ) {
			WiredPC_Int( handle, &item->border );
		}
		else if ( !Q_stricmp( token.string, "bordersize" ) ) {
			WiredPC_Float( handle, &item->bordersize );
		}
		else if ( !Q_stricmp( token.string, "background" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->background, str, sizeof( item->background ) );
		}
		else if ( !Q_stricmp( token.string, "visible" ) ) {
			int v;
			WiredPC_Int( handle, &v );
			item->visible = (qboolean)v;
		}
		else if ( !Q_stricmp( token.string, "decoration" ) ) {
			item->decoration = qtrue;
		}
		else if ( !Q_stricmp( token.string, "ownerdraw" ) ) {
			WiredPC_Int( handle, &item->ownerdraw );
		}
		else if ( !Q_stricmp( token.string, "ownerdrawFlag" ) ) {
			WiredPC_Int( handle, &item->ownerdrawFlag );
		}
		else if ( !Q_stricmp( token.string, "cvar" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->cvar, str, sizeof( item->cvar ) );
		}
		else if ( !Q_stricmp( token.string, "cvarTest" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->cvarTest, str, sizeof( item->cvarTest ) );
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
				if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) continue;
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
				if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) continue;
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
		else if ( !Q_stricmp( token.string, "populateCallback" ) ) {
			// populateCallback "audio_devices"
			//
			// Marks an ITEM_TYPE_MULTI as dynamic: at render time the named
			// callback (registered via WiredUI_RegisterPopulateCallback) is
			// invoked to populate the option list. Replaces cvarFloatList /
			// cvarStrList for runtime-discovered options.
			if ( WiredPC_String( handle, &str ) ) {
				Q_strncpyz( item->populateCallback, str, sizeof( item->populateCallback ) );
			}
		}
		else if ( !Q_stricmp( token.string, "maxChars" ) ) {
			WiredPC_Int( handle, &item->maxChars );
		}
		else if ( !Q_stricmp( token.string, "maxPaintChars" ) ) {
			WiredPC_Int( handle, &item->maxPaintChars );
		}
		else if ( !Q_stricmp( token.string, "feeder" ) ) {
			WiredPC_Float( handle, &item->feeder );
		}
		else if ( !Q_stricmp( token.string, "elementwidth" ) ) {
			WiredPC_Float( handle, &item->elementwidth );
		}
		else if ( !Q_stricmp( token.string, "elementheight" ) ) {
			WiredPC_Float( handle, &item->elementheight );
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
		else if ( !Q_stricmp( token.string, "notselectable" ) ) {
			item->notselectable = qtrue;
		}
		else if ( !Q_stricmp( token.string, "horizontalscroll" ) ) {
			item->horizontalScroll = qtrue;
		}
		else if ( !Q_stricmp( token.string, "outlinecolor" ) ) {
			WiredPC_Color( handle, &item->outlinecolor );
		}
		else if ( !Q_stricmp( token.string, "special" ) ) {
			WiredPC_Float( handle, &item->special );
		}
		else if ( !Q_stricmp( token.string, "anchor" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				if      ( !Q_stricmp( str, "TOP_LEFT" ) )      item->anchor = ANCHOR_TOP_LEFT;
				else if ( !Q_stricmp( str, "TOP_CENTER" ) )     item->anchor = ANCHOR_TOP_CENTER;
				else if ( !Q_stricmp( str, "TOP_RIGHT" ) )      item->anchor = ANCHOR_TOP_RIGHT;
				else if ( !Q_stricmp( str, "CENTER_LEFT" ) )    item->anchor = ANCHOR_CENTER_LEFT;
				else if ( !Q_stricmp( str, "CENTER" ) )         item->anchor = ANCHOR_CENTER;
				else if ( !Q_stricmp( str, "CENTER_RIGHT" ) )   item->anchor = ANCHOR_CENTER_RIGHT;
				else if ( !Q_stricmp( str, "BOTTOM_LEFT" ) )    item->anchor = ANCHOR_BOTTOM_LEFT;
				else if ( !Q_stricmp( str, "BOTTOM_CENTER" ) )  item->anchor = ANCHOR_BOTTOM_CENTER;
				else if ( !Q_stricmp( str, "BOTTOM_RIGHT" ) )   item->anchor = ANCHOR_BOTTOM_RIGHT;
				else {
					Com_Printf( S_COLOR_YELLOW "WARNING: unknown anchor '%s'\n", str );
					item->anchor = ANCHOR_NONE;
				}
			}
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
			char *dest2;
			int destSize2;
			int depth2 = 0;
			if ( !Q_stricmp( token.string, "enableCvar" ) ) {
				dest2 = item->enableCvar; destSize2 = sizeof( item->enableCvar );
			} else {
				dest2 = item->disableCvar; destSize2 = sizeof( item->disableCvar );
			}
			if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) {
				continue;
			}
			depth2 = 1;
			dest2[0] = '\0';
			while ( depth2 > 0 ) {
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "{" ) ) { depth2++; continue; }
				if ( !Q_stricmp( token.string, "}" ) ) { depth2--; continue; }
				if ( dest2[0] ) Q_strcat( dest2, destSize2, " " );
				Q_strcat( dest2, destSize2, token.string );
			}
		}
		else if ( !Q_stricmp( token.string, "asset_model" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				Q_strncpyz( item->assetModel, str, sizeof( item->assetModel ) );
			}
		}
		else if ( !Q_stricmp( token.string, "asset_shader" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				Q_strncpyz( item->assetShader, str, sizeof( item->assetShader ) );
			}
		}
		else if ( !Q_stricmp( token.string, "model_origin" ) ) {
			WiredPC_Float( handle, &item->modelOrigin[0] );
			WiredPC_Float( handle, &item->modelOrigin[1] );
			WiredPC_Float( handle, &item->modelOrigin[2] );
		}
		else if ( !Q_stricmp( token.string, "model_fovx" ) ) {
			WiredPC_Float( handle, &item->modelFovX );
		}
		else if ( !Q_stricmp( token.string, "model_fovy" ) ) {
			WiredPC_Float( handle, &item->modelFovY );
		}
		else if ( !Q_stricmp( token.string, "model_rotation" ) ) {
			WiredPC_Float( handle, &item->modelRotation );
		}
		else if ( !Q_stricmp( token.string, "model_angle" ) ) {
			WiredPC_Float( handle, &item->modelAngle );
		}
		else if ( !Q_stricmp( token.string, "widescreen" ) ) {
			WiredPC_Int( handle, &item->modelWidescreen );
		}
		else if ( !Q_stricmp( token.string, "hudElement" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->hudElement, str, sizeof( item->hudElement ) );
		}
		else if ( !Q_stricmp( token.string, "bind" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				Q_strncpyz( item->bind, str, sizeof( item->bind ) );
				Q_strncpyz( item->storeBind, str, sizeof( item->storeBind ) );
			}
		}
		else if ( !Q_stricmp( token.string, "bindcolor" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->storeBindColor, str, sizeof( item->storeBindColor ) );
		}
		else if ( !Q_stricmp( token.string, "bindicon" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->storeBindIcon, str, sizeof( item->storeBindIcon ) );
		}
		else if ( !Q_stricmp( token.string, "bindvalue" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->storeBindValue, str, sizeof( item->storeBindValue ) );
		}
		else if ( !Q_stricmp( token.string, "showbind" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->showBind, str, sizeof( item->showBind ) );
		}
		else if ( !Q_stricmp( token.string, "hidebind" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->hideBind, str, sizeof( item->hideBind ) );
		}
		// ── ModernHUD element properties (Phase 3) ────────────────────
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
				if ( !Q_stricmp( str, "light" ) )          item->fontWeight = 300;
				else if ( !Q_stricmp( str, "regular" ) )   item->fontWeight = 400;
				else if ( !Q_stricmp( str, "medium" ) )    item->fontWeight = 500;
				else if ( !Q_stricmp( str, "semibold" ) )  item->fontWeight = 600;
				else if ( !Q_stricmp( str, "bold" ) )      item->fontWeight = 700;
				else if ( !Q_stricmp( str, "extrabold" ) ) item->fontWeight = 800;
				else                                         item->fontWeight = atoi( str );
			}
		}
		else if ( !Q_stricmp( token.string, "letterspacing" ) ) {
			WiredPC_Float( handle, &item->letterSpacing );
		}
		else if ( !Q_stricmp( token.string, "direction" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				if ( !Q_stricmp( str, "R" ) || !Q_stricmp( str, "right" ) )       item->direction = 0;
				else if ( !Q_stricmp( str, "L" ) || !Q_stricmp( str, "left" ) )   item->direction = 1;
				else if ( !Q_stricmp( str, "T" ) || !Q_stricmp( str, "top" ) )    item->direction = 2;
				else if ( !Q_stricmp( str, "B" ) || !Q_stricmp( str, "bottom" ) ) item->direction = 3;
				else item->direction = atoi( str );
			}
		}
		else if ( !Q_stricmp( token.string, "fill" ) ) {
			item->fillFlag = qtrue;
		}
		else if ( !Q_stricmp( token.string, "monospace" ) ) {
			item->monospace = qtrue;
		}
		else if ( !Q_stricmp( token.string, "color2" ) ) {
			WiredPC_Color( handle, &item->color2 );
		}
		else if ( !Q_stricmp( token.string, "alignv" ) || !Q_stricmp( token.string, "alignV" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				if ( !Q_stricmp( str, "T" ) || !Q_stricmp( str, "top" ) )        item->alignV = 0;
				else if ( !Q_stricmp( str, "C" ) || !Q_stricmp( str, "center" ) ) item->alignV = 1;
				else if ( !Q_stricmp( str, "B" ) || !Q_stricmp( str, "bottom" ) ) item->alignV = 2;
				else item->alignV = atoi( str );
			}
		}
		else if ( !Q_stricmp( token.string, "fade" ) ) {
			WiredPC_Color( handle, &item->fadeColor );
		}
		else if ( !Q_stricmp( token.string, "fadedelay" ) ) {
			WiredPC_Int( handle, &item->fadeDelay );
		}
		else if ( !Q_stricmp( token.string, "time" ) ) {
			WiredPC_Int( handle, &item->timeMs );
		}
		else if ( !Q_stricmp( token.string, "image" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->image, str, sizeof( item->image ) );
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

			if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) {
				continue;
			}
			depth = 1;
			if ( dest ) { dest[0] = '\0'; }
			while ( depth > 0 ) {
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "{" ) ) { depth++; continue; }
				if ( !Q_stricmp( token.string, "}" ) ) { depth--; continue; }
				if ( dest ) {
					// append token to script string, space-separated
					if ( dest[0] ) Q_strcat( dest, destSize, " " );
					Q_strcat( dest, destSize, token.string );
				}
			}
		}
		else if ( !Q_stricmp( token.string, "execKey" ) ) {
			// ET:Legacy: execKey "key" { script }
			const char *keyStr;
			if ( WiredPC_String( handle, &keyStr ) ) {
				item->execKeyCode = keyStr[0];  // single char key code
			}
			// capture the action block
			if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) {
				continue;
			}
			depth = 1;
			item->execKeyAction[0] = '\0';
			while ( depth > 0 ) {
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "{" ) ) { depth++; continue; }
				if ( !Q_stricmp( token.string, "}" ) ) { depth--; continue; }
				if ( item->execKeyAction[0] ) Q_strcat( item->execKeyAction, WIRED_MAX_SCRIPT_LEN, " " );
				Q_strcat( item->execKeyAction, WIRED_MAX_SCRIPT_LEN, token.string );
			}
		}
		else if ( !Q_stricmp( token.string, "tooltip" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->tooltip, str, sizeof( item->tooltip ) );
		}
		else if ( !Q_stricmp( token.string, "showCvar" ) || !Q_stricmp( token.string, "hideCvar" ) ||
		          !Q_stricmp( token.string, "enableCvar" ) || !Q_stricmp( token.string, "disableCvar" ) ) {
			// capture cvar value lists { "val1" "val2" ... } as space-separated string
			char *dest = NULL;
			int destSize = 0;
			if ( !Q_stricmp( token.string, "showCvar" ) )       { dest = item->showCvar; destSize = sizeof( item->showCvar ); }
			else if ( !Q_stricmp( token.string, "hideCvar" ) )   { dest = item->hideCvar; destSize = sizeof( item->hideCvar ); }

			if ( dest ) dest[0] = '\0';

			if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) {
				continue;
			}
			depth = 1;
			while ( depth > 0 ) {
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "{" ) ) { depth++; continue; }
				if ( !Q_stricmp( token.string, "}" ) ) { depth--; continue; }
				if ( dest && destSize > 0 ) {
					if ( dest[0] ) Q_strcat( dest, destSize, " " );
					Q_strcat( dest, destSize, token.string );
				}
			}
		}
		// ── Layer 2: flex container keywords ──────────────────────────
		else if ( !Q_stricmp( token.string, "layout" ) ) {
			pc_token_t layoutToken;
			if ( WiredPC_ReadTokenEval( handle, &layoutToken ) ) {
				if ( !Q_stricmp( layoutToken.string, "row" ) ) {
					item->flexContainer.direction = WUI_LAYOUT_ROW;
					item->isFlexContainer = qtrue;
				} else if ( !Q_stricmp( layoutToken.string, "column" ) ) {
					item->flexContainer.direction = WUI_LAYOUT_COLUMN;
					item->isFlexContainer = qtrue;
				}
				// Check for "wrap" modifier
				if ( WiredPC_ReadToken( handle, &layoutToken ) ) {
					if ( !Q_stricmp( layoutToken.string, "wrap" ) ) {
						item->flexContainer.wrap = qtrue;
					} else {
						WiredPC_UnreadToken( &layoutToken );
					}
				}
			}
		}
		else if ( !Q_stricmp( token.string, "gap" ) ) {
			item->flexContainer.gap = WiredPC_ParseValue( handle );
		}
		else if ( !Q_stricmp( token.string, "padding" ) ) {
			// Support 1, 2, or 4 value forms
			pc_token_t peek;
			item->flexContainer.padding[0] = WiredPC_ParseValue( handle );
			// Try reading more values
			if ( WiredPC_ReadToken( handle, &peek ) ) {
				if ( peek.type == TT_NUMBER || peek.string[0] == '0' || peek.string[0] == '.' ) {
					// At least 2 values
					WiredPC_UnreadToken( &peek );
					item->flexContainer.padding[1] = WiredPC_ParseValue( handle );
					// Try 3rd and 4th
					if ( WiredPC_ReadToken( handle, &peek ) ) {
						if ( peek.type == TT_NUMBER || peek.string[0] == '0' || peek.string[0] == '.' ) {
							WiredPC_UnreadToken( &peek );
							item->flexContainer.padding[2] = WiredPC_ParseValue( handle );
							item->flexContainer.padding[3] = WiredPC_ParseValue( handle );
						} else {
							WiredPC_UnreadToken( &peek );
							// 2-value form: top/bottom = [0], left/right = [1]
							item->flexContainer.padding[2] = item->flexContainer.padding[0];
							item->flexContainer.padding[3] = item->flexContainer.padding[1];
						}
					}
				} else {
					WiredPC_UnreadToken( &peek );
					// 1-value form: all sides equal
					item->flexContainer.padding[1] = item->flexContainer.padding[0];
					item->flexContainer.padding[2] = item->flexContainer.padding[0];
					item->flexContainer.padding[3] = item->flexContainer.padding[0];
				}
			}
		}
		else if ( !Q_stricmp( token.string, "align" ) ) {
			// Native format: flex align keyword (overrides legacy int align)
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) {
				if ( !Q_stricmp( val.string, "start" ) ) item->flexContainer.align = WUI_ALIGN_START;
				else if ( !Q_stricmp( val.string, "center" ) ) item->flexContainer.align = WUI_ALIGN_CENTER;
				else if ( !Q_stricmp( val.string, "end" ) ) item->flexContainer.align = WUI_ALIGN_END;
				else if ( !Q_stricmp( val.string, "stretch" ) ) item->flexContainer.align = WUI_ALIGN_STRETCH;
				else {
					// Legacy numeric align value in native format
					item->align = atoi( val.string );
				}
			}
		}
		else if ( !Q_stricmp( token.string, "justify" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) {
				if ( !Q_stricmp( val.string, "start" ) ) item->flexContainer.justify = WUI_JUSTIFY_START;
				else if ( !Q_stricmp( val.string, "center" ) ) item->flexContainer.justify = WUI_JUSTIFY_CENTER;
				else if ( !Q_stricmp( val.string, "end" ) ) item->flexContainer.justify = WUI_JUSTIFY_END;
				else if ( !Q_stricmp( val.string, "space-between" ) ) item->flexContainer.justify = WUI_JUSTIFY_SPACE_BETWEEN;
			}
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
				if ( !Q_stricmp( val.string, "start" ) ) item->flexChild.alignSelf = WUI_ALIGN_START;
				else if ( !Q_stricmp( val.string, "center" ) ) item->flexChild.alignSelf = WUI_ALIGN_CENTER;
				else if ( !Q_stricmp( val.string, "end" ) ) item->flexChild.alignSelf = WUI_ALIGN_END;
				else if ( !Q_stricmp( val.string, "stretch" ) ) item->flexChild.alignSelf = WUI_ALIGN_STRETCH;
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
					Com_Memset( child, 0, sizeof( *child ) );
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
				if ( WiredPC_ReadToken( handle, &token ) && !Q_stricmp( token.string, "{" ) ) {
					int skipDepth = 1;
					while ( skipDepth > 0 ) {
						if ( !WiredPC_ReadToken( handle, &token ) ) break;
						if ( !Q_stricmp( token.string, "{" ) ) skipDepth++;
						else if ( !Q_stricmp( token.string, "}" ) ) skipDepth--;
					}
				}
			}
		}
		// ── Layer 5: transition animation ─────────────────────────────
		else if ( !Q_stricmp( token.string, "transition" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) {
				item->wuiTransition.duration = val.intvalue;
			}
			// Check for optional easing keyword
			if ( WiredPC_ReadToken( handle, &val ) ) {
				if ( !Q_stricmp( val.string, "ease-in" ) ) item->wuiTransition.easing = WUI_EASE_IN;
				else if ( !Q_stricmp( val.string, "ease-out" ) ) item->wuiTransition.easing = WUI_EASE_OUT;
				else if ( !Q_stricmp( val.string, "ease-in-out" ) ) item->wuiTransition.easing = WUI_EASE_IN_OUT;
				else if ( !Q_stricmp( val.string, "linear" ) ) item->wuiTransition.easing = WUI_EASE_LINEAR;
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
		/* ── TABLE widget keywords ─────────────────────────────────── */
		else if ( !Q_stricmp( token.string, "source" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->tableSource, str, sizeof( item->tableSource ) );
		}
		else if ( !Q_stricmp( token.string, "countbind" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->tableCountBind, str, sizeof( item->tableCountBind ) );
		}
		else if ( !Q_stricmp( token.string, "teamfilter" ) ) {
			int v;
			if ( WiredPC_Int( handle, &v ) )
				item->tableTeamFilter = v;
		}
		else if ( !Q_stricmp( token.string, "column" ) ) {
			/* column { field "name" header "Player" width 0.25 align 0 colorfield "namecolor" } */
			if ( item->numTableColumns < WUI_TABLE_MAX_COLUMNS ) {
				pc_token_t subToken;
				wuiTableColumn_t *col = &item->tableColumns[item->numTableColumns];
				Com_Memset( col, 0, sizeof( *col ) );
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
					/* block parameter -- skip balanced braces */
					int skipDepth = 1;
					while ( skipDepth > 0 ) {
						if ( !WiredPC_ReadToken( handle, &token ) ) break;
						if ( !Q_stricmp( token.string, "{" ) ) skipDepth++;
						else if ( !Q_stricmp( token.string, "}" ) ) skipDepth--;
					}
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
	int              depth;

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
	if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) {
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
		if ( !Q_stricmp( token.string, "name" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( menu->name, str, sizeof( menu->name ) );
		}
		else if ( !Q_stricmp( token.string, "fullscreen" ) ) {
			int v;
			WiredPC_Int( handle, &v );
			menu->fullscreen = (qboolean)v;
		}
		else if ( !Q_stricmp( token.string, "rect" ) ) {
			// .wmenu: parse with unit awareness
			menu->wuiRect.x = WiredPC_ParseValue( handle );
			menu->wuiRect.y = WiredPC_ParseValue( handle );
			menu->wuiRect.w = WiredPC_ParseValue( handle );
			menu->wuiRect.h = WiredPC_ParseValue( handle );
			// Back-fill old rect for draw code (real screen pixels)
			menu->rect.x = WUI_BackfillToScreen( menu->wuiRect.x, (float)cls.glconfig.vidWidth );
			menu->rect.y = WUI_BackfillToScreen( menu->wuiRect.y, (float)cls.glconfig.vidHeight );
			menu->rect.w = WUI_BackfillToScreen( menu->wuiRect.w, (float)cls.glconfig.vidWidth );
			menu->rect.h = WUI_BackfillToScreen( menu->wuiRect.h, (float)cls.glconfig.vidHeight );
		}
		else if ( !Q_stricmp( token.string, "style" ) ) {
			WiredPC_Int( handle, &menu->style );
		}
		else if ( !Q_stricmp( token.string, "visible" ) ) {
			int v;
			WiredPC_Int( handle, &v );
			menu->visible = (qboolean)v;
		}
		else if ( !Q_stricmp( token.string, "forecolor" ) ) {
			WiredPC_Color( handle, &menu->forecolor );
		}
		else if ( !Q_stricmp( token.string, "backcolor" ) ) {
			WiredPC_Color( handle, &menu->backcolor );
		}
		else if ( !Q_stricmp( token.string, "focuscolor" ) ) {
			WiredPC_Color( handle, &menu->focuscolor );
		}
		else if ( !Q_stricmp( token.string, "background" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( menu->background, str, sizeof( menu->background ) );
		}
		else if ( !Q_stricmp( token.string, "soundLoop" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( menu->soundLoop, str, sizeof( menu->soundLoop ) );
		}
		else if ( !Q_stricmp( token.string, "hudOverlay" ) ) {
			int v;
			WiredPC_Int( handle, &v );
			menu->hudOverlay = (qboolean)v;
		}
		else if ( !Q_stricmp( token.string, "onOpen" ) ||
		          !Q_stricmp( token.string, "onClose" ) ||
		          !Q_stricmp( token.string, "onESC" ) ) {
			// capture menu-level script blocks
			char *dest = NULL;
			int destSize = WIRED_MAX_SCRIPT_LEN;

			if ( !Q_stricmp( token.string, "onOpen" ) )  dest = menu->onOpen;
			else if ( !Q_stricmp( token.string, "onClose" ) ) dest = menu->onClose;
			else if ( !Q_stricmp( token.string, "onESC" ) )  dest = menu->onESC;

			if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) continue;
			depth = 1;
			if ( dest ) { dest[0] = '\0'; }
			while ( depth > 0 ) {
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "{" ) ) { depth++; continue; }
				if ( !Q_stricmp( token.string, "}" ) ) { depth--; continue; }
				if ( dest ) {
					if ( dest[0] ) Q_strcat( dest, destSize, " " );
					Q_strcat( dest, destSize, token.string );
				}
			}
		}
		else if ( !Q_stricmp( token.string, "modal" ) ) {
			menu->modal = qtrue;
		}
		else if ( !Q_stricmp( token.string, "alwaysontop" ) ) {
			menu->alwaysOnTop = qtrue;
		}
		// ── Phase 2.5: additional menu-level keywords ────────────────
		else if ( !Q_stricmp( token.string, "popup" ) ) {
			menu->popup = qtrue;
		}
		else if ( !Q_stricmp( token.string, "outOfBoundsClick" ) ) {
			menu->outOfBoundsClick = qtrue;
		}
		else if ( !Q_stricmp( token.string, "anchor" ) ) {
			if ( WiredPC_String( handle, &str ) ) {
				if      ( !Q_stricmp( str, "TOP_LEFT" ) )      menu->anchor = ANCHOR_TOP_LEFT;
				else if ( !Q_stricmp( str, "TOP_CENTER" ) )     menu->anchor = ANCHOR_TOP_CENTER;
				else if ( !Q_stricmp( str, "TOP_RIGHT" ) )      menu->anchor = ANCHOR_TOP_RIGHT;
				else if ( !Q_stricmp( str, "CENTER_LEFT" ) )    menu->anchor = ANCHOR_CENTER_LEFT;
				else if ( !Q_stricmp( str, "CENTER" ) )         menu->anchor = ANCHOR_CENTER;
				else if ( !Q_stricmp( str, "CENTER_RIGHT" ) )   menu->anchor = ANCHOR_CENTER_RIGHT;
				else if ( !Q_stricmp( str, "BOTTOM_LEFT" ) )    menu->anchor = ANCHOR_BOTTOM_LEFT;
				else if ( !Q_stricmp( str, "BOTTOM_CENTER" ) )  menu->anchor = ANCHOR_BOTTOM_CENTER;
				else if ( !Q_stricmp( str, "BOTTOM_RIGHT" ) )   menu->anchor = ANCHOR_BOTTOM_RIGHT;
				else {
					Com_Printf( S_COLOR_YELLOW "WARNING: unknown anchor '%s'\n", str );
					menu->anchor = ANCHOR_NONE;
				}
			}
		}
		else if ( !Q_stricmp( token.string, "border" ) ) {
			WiredPC_Int( handle, &menu->border );
		}
		else if ( !Q_stricmp( token.string, "bordersize" ) ) {
			WiredPC_Float( handle, &menu->bordersize );
		}
		else if ( !Q_stricmp( token.string, "bordercolor" ) ) {
			WiredPC_Color( handle, &menu->bordercolor );
		}
		else if ( !Q_stricmp( token.string, "disablecolor" ) ) {
			WiredPC_Color( handle, &menu->disablecolor );
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
		else if ( !Q_stricmp( token.string, "fadeClamp" ) ) {
			WiredPC_Float( handle, &menu->fadeClamp );
		}
		else if ( !Q_stricmp( token.string, "fadeCycle" ) ) {
			WiredPC_Int( handle, &menu->fadeCycle );
		}
		else if ( !Q_stricmp( token.string, "fadeAmount" ) ) {
			WiredPC_Float( handle, &menu->fadeAmount );
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
		// ── Layer 2: flex container keywords (menu level) ────────────
		else if ( !Q_stricmp( token.string, "layout" ) ) {
			pc_token_t layoutToken;
			if ( WiredPC_ReadTokenEval( handle, &layoutToken ) ) {
				if ( !Q_stricmp( layoutToken.string, "row" ) ) {
					menu->flexContainer.direction = WUI_LAYOUT_ROW;
					menu->isFlexContainer = qtrue;
				} else if ( !Q_stricmp( layoutToken.string, "column" ) ) {
					menu->flexContainer.direction = WUI_LAYOUT_COLUMN;
					menu->isFlexContainer = qtrue;
				}
				// Check for "wrap" modifier
				if ( WiredPC_ReadToken( handle, &layoutToken ) ) {
					if ( !Q_stricmp( layoutToken.string, "wrap" ) ) {
						menu->flexContainer.wrap = qtrue;
					} else {
						WiredPC_UnreadToken( &layoutToken );
					}
				}
			}
		}
		else if ( !Q_stricmp( token.string, "gap" ) ) {
			menu->flexContainer.gap = WiredPC_ParseValue( handle );
		}
		else if ( !Q_stricmp( token.string, "padding" ) ) {
			// Support 1, 2, or 4 value forms
			pc_token_t peek;
			menu->flexContainer.padding[0] = WiredPC_ParseValue( handle );
			if ( WiredPC_ReadToken( handle, &peek ) ) {
				if ( peek.type == TT_NUMBER || peek.string[0] == '0' || peek.string[0] == '.' ) {
					WiredPC_UnreadToken( &peek );
					menu->flexContainer.padding[1] = WiredPC_ParseValue( handle );
					if ( WiredPC_ReadToken( handle, &peek ) ) {
						if ( peek.type == TT_NUMBER || peek.string[0] == '0' || peek.string[0] == '.' ) {
							WiredPC_UnreadToken( &peek );
							menu->flexContainer.padding[2] = WiredPC_ParseValue( handle );
							menu->flexContainer.padding[3] = WiredPC_ParseValue( handle );
						} else {
							WiredPC_UnreadToken( &peek );
							menu->flexContainer.padding[2] = menu->flexContainer.padding[0];
							menu->flexContainer.padding[3] = menu->flexContainer.padding[1];
						}
					}
				} else {
					WiredPC_UnreadToken( &peek );
					menu->flexContainer.padding[1] = menu->flexContainer.padding[0];
					menu->flexContainer.padding[2] = menu->flexContainer.padding[0];
					menu->flexContainer.padding[3] = menu->flexContainer.padding[0];
				}
			}
		}
		else if ( !Q_stricmp( token.string, "align" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) {
				if ( !Q_stricmp( val.string, "start" ) ) menu->flexContainer.align = WUI_ALIGN_START;
				else if ( !Q_stricmp( val.string, "center" ) ) menu->flexContainer.align = WUI_ALIGN_CENTER;
				else if ( !Q_stricmp( val.string, "end" ) ) menu->flexContainer.align = WUI_ALIGN_END;
				else if ( !Q_stricmp( val.string, "stretch" ) ) menu->flexContainer.align = WUI_ALIGN_STRETCH;
			}
		}
		else if ( !Q_stricmp( token.string, "justify" ) ) {
			pc_token_t val;
			if ( WiredPC_ReadTokenEval( handle, &val ) ) {
				if ( !Q_stricmp( val.string, "start" ) ) menu->flexContainer.justify = WUI_JUSTIFY_START;
				else if ( !Q_stricmp( val.string, "center" ) ) menu->flexContainer.justify = WUI_JUSTIFY_CENTER;
				else if ( !Q_stricmp( val.string, "end" ) ) menu->flexContainer.justify = WUI_JUSTIFY_END;
				else if ( !Q_stricmp( val.string, "space-between" ) ) menu->flexContainer.justify = WUI_JUSTIFY_SPACE_BETWEEN;
			}
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
					// block parameter — skip balanced braces
					int skipDepth = 1;
					while ( skipDepth > 0 ) {
						if ( !WiredPC_ReadToken( handle, &token ) ) break;
						if ( !Q_stricmp( token.string, "{" ) ) skipDepth++;
						else if ( !Q_stricmp( token.string, "}" ) ) skipDepth--;
					}
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

	if ( wired_menuCount < WIRED_MAX_MENUS ) {
		wired_menus[wired_menuCount++] = menu;
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
			if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) {
				Com_Printf( S_COLOR_YELLOW "WiredUI: expected '{' for assetGlobalDef\n" );
				continue;
			}
			while ( 1 ) {
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "}" ) ) break;

				if ( !Q_stricmp( token.string, "cursor" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						Q_strncpyz( ag->cursor, token.string, sizeof( ag->cursor ) );
				}
				else if ( !Q_stricmp( token.string, "gradientBar" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						Q_strncpyz( ag->gradientBar, token.string, sizeof( ag->gradientBar ) );
				}
				else if ( !Q_stricmp( token.string, "fadeClamp" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						ag->fadeClamp = atof( token.string );
				}
				else if ( !Q_stricmp( token.string, "fadeCycle" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						ag->fadeCycle = atoi( token.string );
				}
				else if ( !Q_stricmp( token.string, "fadeAmount" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						ag->fadeAmount = atof( token.string );
				}
				else if ( !Q_stricmp( token.string, "shadowColor" ) ) {
					WiredPC_ReadToken( handle, &token ); ag->shadowColor[0] = atof( token.string );
					WiredPC_ReadToken( handle, &token ); ag->shadowColor[1] = atof( token.string );
					WiredPC_ReadToken( handle, &token ); ag->shadowColor[2] = atof( token.string );
					WiredPC_ReadToken( handle, &token ); ag->shadowColor[3] = atof( token.string );
				}
				else if ( !Q_stricmp( token.string, "itemFocusSound" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						Q_strncpyz( ag->focusSound, token.string, sizeof( ag->focusSound ) );
				}
				else if ( !Q_stricmp( token.string, "focusColor" ) ) {
					WiredPC_ReadToken( handle, &token ); ag->focusColor[0] = atof( token.string );
					WiredPC_ReadToken( handle, &token ); ag->focusColor[1] = atof( token.string );
					WiredPC_ReadToken( handle, &token ); ag->focusColor[2] = atof( token.string );
					WiredPC_ReadToken( handle, &token ); ag->focusColor[3] = atof( token.string );
				}
				else if ( !Q_stricmp( token.string, "shadowX" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						ag->shadowX = atof( token.string );
				}
				else if ( !Q_stricmp( token.string, "shadowY" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						ag->shadowY = atof( token.string );
				}
				else if ( !Q_stricmp( token.string, "gradientBarColor" ) ) {
					WiredPC_ReadToken( handle, &token ); ag->gradientBarColor[0] = atof( token.string );
					WiredPC_ReadToken( handle, &token ); ag->gradientBarColor[1] = atof( token.string );
					WiredPC_ReadToken( handle, &token ); ag->gradientBarColor[2] = atof( token.string );
					WiredPC_ReadToken( handle, &token ); ag->gradientBarColor[3] = atof( token.string );
				}
				else if ( !Q_stricmp( token.string, "radialGlow" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						Q_strncpyz( ag->radialGlowShader, token.string, sizeof( ag->radialGlowShader ) );
				}
				else if ( !Q_stricmp( token.string, "defaultSerifFont" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						Q_strncpyz( ag->defaultSerifFontName, token.string, sizeof( ag->defaultSerifFontName ) );
				}
				else if ( !Q_stricmp( token.string, "defaultSerifFontItalic" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						Q_strncpyz( ag->defaultSerifFontItalicName, token.string, sizeof( ag->defaultSerifFontItalicName ) );
				}
				else if ( !Q_stricmp( token.string, "defaultSansFont" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						Q_strncpyz( ag->defaultSansFontName, token.string, sizeof( ag->defaultSansFontName ) );
				}
				else if ( !Q_stricmp( token.string, "defaultSansFontMedium" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						Q_strncpyz( ag->defaultSansFontMediumName, token.string, sizeof( ag->defaultSansFontMediumName ) );
				}
				else if ( !Q_stricmp( token.string, "defaultMonoFont" ) ) {
					if ( WiredPC_ReadToken( handle, &token ) )
						Q_strncpyz( ag->defaultMonoFontName, token.string, sizeof( ag->defaultMonoFontName ) );
				}
				// skip unknown keywords gracefully
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
	return wired_menuCount;
}

wiredMenuDef_t *WiredUI_GetMenuByIndex( int index ) {
	if ( index < 0 || index >= wired_menuCount ) return NULL;
	return wired_menus[index];
}

wiredMenuDef_t *WiredUI_FindMenu( const char *name ) {
	int i;
	for ( i = 0; i < wired_menuCount; i++ ) {
		if ( !Q_stricmp( wired_menus[i]->name, name ) ) {
			return wired_menus[i];
		}
	}
	return NULL;
}

void WiredUI_ClearMenus( void ) {
	wired_menuCount = 0;
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
	Com_Memcpy( wired_backup->pool, wired_menuPool, wired_menuPoolUsed );
	wired_backup->poolUsed = wired_menuPoolUsed;
	Com_Memcpy( wired_backup->menus, wired_menus, sizeof( wired_menus[0] ) * wired_menuCount );
	wired_backup->menuCount = wired_menuCount;
	wired_backup->assetGlobals = *WiredUI_GetAssetGlobals();

	// phase 2: clear and reparse from menus.lua
	WiredUI_ResetAssetGlobalsDefaults();
	WiredUI_ClearMenus();
	WiredUI_LoadMenusFromLua();
	qboolean ok = ( wired_menuCount > 0 );

	if ( !ok ) {
		// parse failed — restore old menus
		Com_Printf( S_COLOR_YELLOW "Menu reload failed — keeping old menus.\n" );
		Com_Memcpy( wired_menuPool, wired_backup->pool, wired_backup->poolUsed );
		wired_menuPoolUsed = wired_backup->poolUsed;
		Com_Memcpy( wired_menus, wired_backup->menus, sizeof( wired_menus[0] ) * wired_backup->menuCount );
		wired_menuCount = wired_backup->menuCount;
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
