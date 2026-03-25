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

#include "../client.h"
#include "cl_wired_ui.h"

#if FEAT_WIRED_UI

#include "../../botlib/botlib.h"
#include "../../ui/menudef.h"

// wiredRect_t defined in cl_wired_ui.h

extern botlib_export_t *botlib_export;

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

static int WiredPC_ReadToken( int handle, pc_token_t *token ) {
	return botlib_export->PC_ReadTokenHandle( handle, token );
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

// ── data structures ───────────────────────────────────────────────────

// wiredItemDef_t, wiredMenuDef_t defined in cl_wired_ui.h

// ── global menu state ─────────────────────────────────────────────────

static wiredMenuDef_t *wired_menus[WIRED_MAX_MENUS];
static int              wired_menuCount = 0;

// ── item parser ───────────────────────────────────────────────────────

static qboolean WiredUI_ParseItem( int handle, wiredMenuDef_t *menu ) {
	pc_token_t      token;
	wiredItemDef_t *item;
	const char     *str;
	int             depth;

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
			WiredPC_Float( handle, &item->rect.x );
			WiredPC_Float( handle, &item->rect.y );
			WiredPC_Float( handle, &item->rect.w );
			WiredPC_Float( handle, &item->rect.h );
		}
		else if ( !Q_stricmp( token.string, "style" ) ) {
			WiredPC_Int( handle, &item->style );
		}
		else if ( !Q_stricmp( token.string, "textalign" ) ) {
			WiredPC_Int( handle, &item->textalign );
		}
		else if ( !Q_stricmp( token.string, "textalignx" ) ) {
			WiredPC_Float( handle, &item->textalignx );
		}
		else if ( !Q_stricmp( token.string, "textaligny" ) ) {
			WiredPC_Float( handle, &item->textaligny );
		}
		else if ( !Q_stricmp( token.string, "textscale" ) ) {
			WiredPC_Float( handle, &item->textscale );
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
			item->decoration = qtrue; // treat same as decoration for interaction
		}
		else if ( !Q_stricmp( token.string, "wrapped" ) || !Q_stricmp( token.string, "autowrapped" ) ) {
			// text wrapping — noted, not yet rendered
		}
		else if ( !Q_stricmp( token.string, "horizontalscroll" ) ) {
			// horizontal scroll — noted, not yet rendered
		}
		else if ( !Q_stricmp( token.string, "outlinecolor" ) ) {
			WiredPC_Color( handle, &item->outlinecolor );
		}
		else if ( !Q_stricmp( token.string, "special" ) ) {
			WiredPC_Float( handle, &item->special );
		}
		else if ( !Q_stricmp( token.string, "align" ) ) {
			WiredPC_Int( handle, &item->align );
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
		else if ( !Q_stricmp( token.string, "asset_model" ) || !Q_stricmp( token.string, "asset_shader" ) ||
		          !Q_stricmp( token.string, "model_origin" ) || !Q_stricmp( token.string, "model_fovx" ) ||
		          !Q_stricmp( token.string, "model_fovy" ) || !Q_stricmp( token.string, "model_rotation" ) ||
		          !Q_stricmp( token.string, "model_angle" ) ) {
			// model keywords — consume value, not yet rendered (type 7 MODEL)
			if ( !Q_stricmp( token.string, "model_origin" ) ) {
				float dummy; WiredPC_Float( handle, &dummy ); WiredPC_Float( handle, &dummy ); WiredPC_Float( handle, &dummy );
			} else {
				WiredPC_ReadToken( handle, &token ); // consume single value
			}
		}
		else if ( !Q_stricmp( token.string, "textfont" ) ) {
			WiredPC_ReadToken( handle, &token ); // consume font name (not yet stored)
		}
		else if ( !Q_stricmp( token.string, "cinematic" ) ) {
			WiredPC_ReadToken( handle, &token ); // consume video name
		}
		else if ( !Q_stricmp( token.string, "widescreen" ) ) {
			int dummy; WiredPC_Int( handle, &dummy ); // consume widescreen mode (QL-specific)
		}
		else if ( !Q_stricmp( token.string, "hudElement" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->hudElement, str, sizeof( item->hudElement ) );
		}
		else if ( !Q_stricmp( token.string, "bind" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->bind, str, sizeof( item->bind ) );
		}
		// ── SuperHUD element properties (Phase 3) ────────────────────
		else if ( !Q_stricmp( token.string, "font" ) && item->hudElement[0] ) {
			// font name for hudElement items (SuperHUD style)
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->fontName, str, sizeof( item->fontName ) );
		}
		else if ( !Q_stricmp( token.string, "fontsize" ) ) {
			WiredPC_Float( handle, &item->fontSize[0] );
			WiredPC_Float( handle, &item->fontSize[1] );
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
		else {
			// unknown keyword — smart skip to avoid poisoning subsequent parsing
			Com_DPrintf( "WiredUI: unknown item keyword '%s'\n", token.string );
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
				// else: consumed one token (the value) — good enough
			}
		}
	}

	if ( menu->itemCount < WIRED_MAX_ITEMS_PER_MENU ) {
		menu->items[menu->itemCount++] = item;
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
			WiredPC_Float( handle, &menu->rect.x );
			WiredPC_Float( handle, &menu->rect.y );
			WiredPC_Float( handle, &menu->rect.w );
			WiredPC_Float( handle, &menu->rect.h );
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
		else if ( !Q_stricmp( token.string, "outlinecolor" ) ) {
			vec4_t oc; WiredPC_Color( handle, &oc ); // consume, not stored on menu
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
			WiredPC_ReadToken( handle, &token ); // consume video name
		}
		else if ( !Q_stricmp( token.string, "ownerdraw" ) ) {
			int od; WiredPC_Int( handle, &od ); // menu-level ownerdraw (rare)
		}
		else if ( !Q_stricmp( token.string, "ownerdrawFlag" ) ) {
			int odf; WiredPC_Int( handle, &odf );
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
		Com_Printf( "WiredUI: loaded menu '%s' (%d items)\n", menu->name, menu->itemCount );
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

	while ( 1 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) {
			break;
		}

		if ( !Q_stricmp( token.string, "menuDef" ) ) {
			WiredUI_ParseMenu( handle );
		}
		else if ( !Q_stricmp( token.string, "assetGlobalDef" ) ) {
			// TODO Phase 2: parse global assets (fonts, cursor, sounds)
			// for now, skip the block
			if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) continue;
			int depth = 1;
			while ( depth > 0 ) {
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "{" ) ) depth++;
				else if ( !Q_stricmp( token.string, "}" ) ) depth--;
			}
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

// ── manifest loader ───────────────────────────────────────────────────

qboolean WiredUI_LoadMenus( const char *manifestFile ) {
	int         handle;
	pc_token_t  token;

	handle = WiredPC_LoadSource( manifestFile );
	if ( !handle ) {
		Com_Printf( S_COLOR_YELLOW "WiredUI: could not load manifest '%s'\n", manifestFile );
		return qfalse;
	}

	while ( 1 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) {
			break;
		}

		if ( !Q_stricmp( token.string, "{" ) ) {
			continue;  // opening brace of manifest
		}
		if ( !Q_stricmp( token.string, "}" ) ) {
			break;     // closing brace of manifest
		}

		if ( !Q_stricmp( token.string, "loadMenu" ) ) {
			// loadMenu { "ui/filename.menu" }
			if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) continue;
			if ( !WiredPC_ReadToken( handle, &token ) ) continue;
			WiredUI_LoadMenuFile( token.string );
			// consume closing }
			WiredPC_ReadToken( handle, &token );
			continue;
		}

		// unknown token in manifest — warn and skip
		if ( Q_stricmp( token.string, "{" ) && Q_stricmp( token.string, "}" ) ) {
			Com_Printf( S_COLOR_YELLOW "WiredUI: unknown manifest token '%s'\n", token.string );
		}
	}

	WiredPC_FreeSource( handle );

	Com_Printf( "WiredUI: loaded %d menus from '%s'\n", wired_menuCount, manifestFile );
	return qtrue;
}

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

#endif // FEAT_WIRED_UI
