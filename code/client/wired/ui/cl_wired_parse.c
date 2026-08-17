// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_parse.c — Wired UI: menu file parser
*/

#include "../../client.h"
#include "cl_wired_ui.h"
#include "policy/wui_bg_preset.h"
#include "cl_wired_compositor.h"
#include "cl_wired_customdraw.h"
#include "cl_wired_bg.h"
#include "cl_wired_anim.h"   /* WUI_AnimStopAll — clear tweens before pool reuse */
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

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

int WiredUI_GetPoolHead( void )      { return wui_menuPoolUsed; }
int WiredUI_GetPoolCapacity( void )  { return WIRED_MENU_POOL_SIZE; }

static void *WiredUI_Alloc( int size ) {
	if ( wui_menuPoolUsed + size > WIRED_MENU_POOL_SIZE ) {
		COM_ERROR( LOG_CH(ch_ui), "WiredUI_Alloc: pool exhausted (%d + %d > %d)\n",
			wui_menuPoolUsed, size, WIRED_MENU_POOL_SIZE );
		return NULL;
	}

	char *p = &wui_menuPool[wui_menuPoolUsed];
	wui_menuPoolUsed += ( ( size + 31 ) & ~31 );  // 32-byte align
	memset( p, 0, size );
	return p;
}

void WiredUI_ResetTokenTable( void );  /* fwd decl, defined further down */
void WiredUI_ResetPool( void ) {
	wui_menuPoolUsed = 0;
	memset( wui_menuPool, 0, sizeof( wui_menuPool ) );
	WiredUI_ResetTokenTable();
}

// ── PC parser wrappers ────────────────────────────────────────────────
// Direct calls to botlib's precompiler — no VM indirection.

static int WiredPC_LoadSource( const char *filename ) {
	return botlib_export->PC_LoadSourceHandle( filename );
}

static int WiredPC_FreeSource( int handle ) {
	return botlib_export->PC_FreeSourceHandle( handle );
}

// Token pushback stack (botlib PC API has no unread function). LIFO so a
// caller may unread several peeked tokens in reverse order and have them
// re-read in their original source order (the `.active` dot-suffix fuse
// below unreads two — `activePeek` then `dotPeek` — and needs `dotPeek`
// back first). Depth 2 covers every current unread site; ASSERT-guards the
// floor in case a future fuse needs deeper lookahead.
#define WUI_PENDING_TOKEN_MAX 2
static pc_token_t wui_pendingTokens[ WUI_PENDING_TOKEN_MAX ];
static int        wui_pendingTokenCount = 0;

/* ── source-attribution context ───────────────────────────────────────
 * Filled while a wmenu/whud is being parsed; consumed by the engine's
 * "Unknown command" log path (cl_main.c::CL_ForwardCommandToServer) so
 * leaked parser keywords get blamed on the actual file + menu + item.
 * Cleared on parse exit (success OR failure path). All accesses are
 * single-threaded — the parser is a re-entrant call chain but never
 * concurrent. Production-default values are NULL/0 → CL_ForwardCommand
 * unconditionally appends nothing when the parser isn't active. */
static const char *s_wui_parse_file   = NULL;
static int         s_wui_parse_handle = 0;
static const char *s_wui_parse_menu   = NULL;
static const char *s_wui_parse_item   = NULL;

const char *WiredUI_ParseContextFile( void )  { return s_wui_parse_file; }
const char *WiredUI_ParseContextMenu( void )  { return s_wui_parse_menu; }
const char *WiredUI_ParseContextItem( void )  { return s_wui_parse_item; }
int         WiredUI_ParseContextLine( void )
{
	if ( s_wui_parse_handle == 0 ) return 0;
	{
		char filenameBuf[ MAX_QPATH ];
		int  line = 0;
		filenameBuf[ 0 ] = '\0';
		if ( botlib_export && botlib_export->PC_SourceFileAndLine ) {
			botlib_export->PC_SourceFileAndLine( s_wui_parse_handle, filenameBuf, &line );
		}
		return line;
	}
}

static int WiredPC_ReadToken( int handle, pc_token_t *token ) {
	if ( wui_pendingTokenCount > 0 ) {
		*token = wui_pendingTokens[ --wui_pendingTokenCount ];
		return 1;
	}
	return botlib_export->PC_ReadTokenHandle( handle, token );
}

static void WiredPC_UnreadToken( pc_token_t *token ) {
	if ( wui_pendingTokenCount >= WUI_PENDING_TOKEN_MAX ) {
		/* Pushback stack overflow — a lookahead unread more tokens than the
		 * buffer holds. Drop this token so we never write OOB; the parse
		 * desyncs but does not corrupt memory. Bump WUI_PENDING_TOKEN_MAX
		 * if a new fuse legitimately needs deeper lookahead. */
		COM_WARN( LOG_CH(ch_ui), "WiredUI: token pushback overflow (>%d)\n",
		          WUI_PENDING_TOKEN_MAX );
		return;
	}
	wui_pendingTokens[ wui_pendingTokenCount++ ] = *token;
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
static void WiredPC_ResolveTokenRef( pc_token_t *token );  /* fwd decl */

static int WiredPC_ReadTokenEval( int handle, pc_token_t *token ) {
	if ( !WiredPC_ReadToken( handle, token ) ) return 0;

	/* `$name` design-token substitution. A bare
	 * `$identifier` token (not the `$evalfloat(`/`$evalint(` literal-
	 * suffix forms handled below) is replaced with the literal value
	 * from the token table loaded by `_tokens.wui`. The resolver
	 * rewrites the pc_token_t in place; subsequent eval code reads
	 * the substituted text/floatvalue/intvalue as if the .wui had
	 * inlined the literal. */
	if ( token->string[ 0 ] == '$'
	  && Q_stricmpn( token->string, "$evalfloat(", 11 ) != 0
	  && Q_stricmpn( token->string, "$evalint(",    9 ) != 0 )
	{
		WiredPC_ResolveTokenRef( token );
	}

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
		qstring_t exprBuf_qs = QS_WrapExisting( exprBuf, sizeof(exprBuf) );

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
				QS_Append( &exprBuf_qs, next.string );
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

/* ── Design tokens (WUI Flexbox Authoring Migration) ─────────
 *
 * `_tokens.wui` declares name→value pairs at the top of the file. Any
 * value in any later `.wui` parse can reference a token via `$name`
 * (e.g. `forecolor $primary_cyan`); the parser substitutes the literal
 * value text in place before the value-typed read (color/float/string)
 * runs.
 *
 * Storage: small linear table keyed by name. ~256 slots is enough for
 * the spec §5 canonical table (60-70 entries) plus modder overrides
 * with headroom. Linear scan + Q_stricmp; lookup is parse-time only so
 * the O(N) scan is acceptable.
 *
 * Lifecycle: cleared in WiredUI_ResetPool. The base `_tokens.wui` is
 * loaded implicitly at every WiredUI_LoadMenuFile entry (idempotent —
 * second-call detection short-circuits). A mod variant may follow and
 * OVERRIDE base entries; NEW keys in the mod variant are rejected with
 * SEV_WARN per spec §3 ("modder yeni token add YASAK").
 */
#define WIRED_TOKEN_MAX        256
#define WIRED_TOKEN_NAME_LEN    64
#define WIRED_TOKEN_VALUE_LEN  128

typedef struct {
	char  name[ WIRED_TOKEN_NAME_LEN ];
	char  value[ WIRED_TOKEN_VALUE_LEN ];
} wuiToken_t;

static wuiToken_t s_wuiTokens[ WIRED_TOKEN_MAX ];
static int        s_wuiTokenCount     = 0;
static qboolean   s_wuiTokensBaseLoaded = qfalse;

/* For sub-token parses (we're recursively inside WiredPC_ReadTokenEval
 * when we substitute). Avoid infinite recursion on broken tokens. */
static int        s_wuiTokenRefDepth = 0;
#define WIRED_TOKEN_REF_MAX_DEPTH  4

const char *WiredToken_Find( const char *name )
{
	int i;
	if ( !name || !*name ) return NULL;
	for ( i = 0; i < s_wuiTokenCount; i++ ) {
		if ( !Q_stricmp( s_wuiTokens[ i ].name, name ) ) {
			return s_wuiTokens[ i ].value;
		}
	}
	return NULL;
}

/* Append or override a token entry. `allowNew` controls whether a name
 * not present in the table may be added (base load) vs the mod-override
 * path that must reject new entries. Returns qtrue when the value was
 * stored, qfalse when rejected (mod-overlay new key OR registry full).
 * The return value lets the self-test confirm the gate fires
 * without log-line scraping. */
static qboolean WiredToken_Set( const char *name, const char *value, qboolean allowNew )
{
	int i;
	if ( !name || !*name || !value ) return qfalse;
	for ( i = 0; i < s_wuiTokenCount; i++ ) {
		if ( !Q_stricmp( s_wuiTokens[ i ].name, name ) ) {
			Q_strncpyz( s_wuiTokens[ i ].value, value,
				sizeof( s_wuiTokens[ i ].value ) );
			return qtrue;
		}
	}
	if ( !allowNew ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredUI/tokens: mod added new token '%s' — modders may only "
			"override base tokens, not introduce new ones (ignored)\n", name );
		return qfalse;
	}
	if ( s_wuiTokenCount >= WIRED_TOKEN_MAX ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredUI/tokens: registry full (%d) — dropping '%s'\n",
			WIRED_TOKEN_MAX, name );
		return qfalse;
	}
	Q_strncpyz( s_wuiTokens[ s_wuiTokenCount ].name, name,
		sizeof( s_wuiTokens[ s_wuiTokenCount ].name ) );
	Q_strncpyz( s_wuiTokens[ s_wuiTokenCount ].value, value,
		sizeof( s_wuiTokens[ s_wuiTokenCount ].value ) );
	s_wuiTokenCount++;
	return qtrue;
}

void WiredUI_ResetTokenTable( void )
{
	s_wuiTokenCount        = 0;
	s_wuiTokensBaseLoaded  = qfalse;
}

int WiredUI_GetTokenCount( void )
{
	return s_wuiTokenCount;
}

/* Forward declaration — WiredUI_LoadMenuFile is defined later. The
 * tokens-include path calls into it for the implicit `_tokens.wui` load
 * (recursive WiredUI_LoadMenuFile invocation is safe because the
 * file's body only contains `token` declarations — no nested includes
 * fire and no menus get registered). */
qboolean WiredUI_LoadMenuFile( const char *filename );

/* Implicit `_tokens.wui` include. Called from WiredUI_LoadMenuFile entry
 * so every `.wui` boot sees a populated token table. Idempotent within
 * a single WiredUI_Init cycle: the s_wuiTokensBaseLoaded flag short-
 * circuits the second-and-after call. WiredUI_ResetPool clears the
 * flag so the next init reloads. */
static void WiredUI_LoadTokensIfNeeded( const char *requesterPath )
{
	if ( s_wuiTokensBaseLoaded ) return;
	/* Avoid infinite recursion: when we ARE the `_tokens.wui` being
	 * loaded, don't try to implicit-load again. The recursive guard
	 * triggers on requester path matching `_tokens.wui` AT ANY directory
	 * depth so future themed/modded `<mod>/ui/_tokens.wui` paths still
	 * short-circuit cleanly. */
	if ( requesterPath ) {
		const char *base = strrchr( requesterPath, '/' );
		base = base ? base + 1 : requesterPath;
		if ( !Q_stricmp( base, "_tokens.wui" ) ) return;
	}

	/* base/mod-overlay separation. The FIRST load registers
	 * the canonical set with `allowNew=qtrue` (token handler reads
	 * `!s_wuiTokensBaseLoaded` to derive that). After this call returns,
	 * `s_wuiTokensBaseLoaded` flips to qtrue so any SUBSEQUENT load —
	 * mod overlay via `wui_load_menu_loose <mod>/ui/_tokens.wui`, or a
	 * future engine-driven mod second-pass — runs through the same
	 * keyword handler with `allowNew=qfalse`. Reset-the-pool path clears
	 * the flag (see WiredUI_ResetTokenTable) so a full WiredUI_Init
	 * re-establishes the canonical set.
	 *
	 * Note on FS source detection: the q3 FS layered lookup gives
	 * mod-precedence by default. If a modder ships `<mod>/ui/_tokens.wui`
	 * AND nothing else has loaded base first, this FIRST-pass load will
	 * register the mod's tokens AS the canonical set. The strict
	 * `<base>/ui/_tokens.wui` MUST-LOAD-FIRST semantic (spec §3 strict
	 * form) would need an FS API that bypasses fs_game; we don't expose
	 * one today, so the load-order proxy is what the gate uses. Modder
	 * testing in practice means the workflow is: (a) ship full token set
	 * if you want overrides, (b) accept that "new mod tokens" get
	 * SEV_WARN'd on any second-pass overlay. */
	if ( !WiredUI_LoadMenuFile( "ui/_tokens.wui" ) ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_ui),
			"WiredUI/tokens: mandatory `ui/_tokens.wui` not found — "
			"every `.wui` file expects design tokens defined here\n" );
	}
	/* Flip the gate AFTER the parse so the first-pass token decls land
	 * with allowNew=qtrue. From here on, any `_tokens.wui` re-parse
	 * (engine reload OR mod overlay) sees `s_wuiTokensBaseLoaded=qtrue`
	 * → allowNew=qfalse → SEV_WARN on truly new keys. */
	s_wuiTokensBaseLoaded = qtrue;

	/* Apply current palette overlay (mode + accent token files) BEFORE
	 * any menu parses its body. Without this re-application the menu
	 * reload triggered by a runtime palette change would re-resolve
	 * $-refs against base tokens only — itemDef vec4 colour fields
	 * (forecolor / backcolor / bordercolor) would stay at base values
	 * even though the bg renderer's emit-time wui_bg_resolve() reads
	 * the live tokens. The palette module's ApplyOverlays no-ops
	 * before WiredPalette_Init completes, so the first boot pass is
	 * unaffected. */
	{
		extern void WiredPalette_ApplyOverlays( void );  /* fwd decl */
		WiredPalette_ApplyOverlays();
	}

#ifdef _DEBUG
	/* self-test (DEBUG only): demonstrate that the mod-override
	 * gate rejects an unknown name. Uses a synthetic key prefixed with
	 * `__wui_modgate_test_` so production logs stay clean and the gate
	 * path is exercised every DEBUG boot — gives us the evidence trace
	 * even without a real test-mod fixture. */
	{
		int      before = s_wuiTokenCount;
		qboolean ok     = WiredToken_Set(
			"__wui_modgate_test_should_reject", "ignored",
			qfalse /* allowNew */ );
		if ( ok || s_wuiTokenCount != before ) {
			Com_Log( SEV_ERROR, LOG_CH(ch_ui),
				"WiredUI/tokens: SELF-TEST FAIL — mod-override gate "
				"accepted new key (count %d→%d)\n",
				before, s_wuiTokenCount );
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_ui),
				"WiredUI/tokens: mod-override gate self-test OK "
				"(SEV_WARN above is intentional Phase 2b verification)\n" );
		}
	}
#endif
}

// ── token parsing helpers ─────────────────────────────────────────────

/* Resolve a `$<name>` reference if the token string starts with `$`.
 * If found, the input token's `string` and `floatvalue` / `intvalue`
 * fields are rewritten in place to the token's literal value. Calls
 * for unknown tokens log SEV_ERROR + leave the token untouched (the
 * caller will then likely produce a downstream parse error, which is
 * the desired noisy-failure behaviour). */
static void WiredPC_ResolveTokenRef( pc_token_t *token )
{
	const char *value;
	const char *name;

	if ( !token || token->string[ 0 ] != '$' ) return;
	if ( s_wuiTokenRefDepth >= WIRED_TOKEN_REF_MAX_DEPTH ) return;

	name = token->string + 1;
	value = WiredToken_Find( name );
	if ( !value ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_ui),
			"WiredUI/tokens: unknown reference `$%s` (file '%s' line %d)\n",
			name,
			s_wui_parse_file ? s_wui_parse_file : "(unknown)",
			WiredUI_ParseContextLine() );
		return;
	}

	s_wuiTokenRefDepth++;
	Q_strncpyz( token->string, value, sizeof( token->string ) );
	token->floatvalue = (float) atof( value );
	token->intvalue   = (int) token->floatvalue;
	/* No re-tokenisation: a token value is a single literal — single
	 * color hex, single float, single keyword. If a value LOOKED like
	 * a multi-token expression, the spec wouldn't allow it as a token
	 * declaration in the first place. */
	s_wuiTokenRefDepth--;
}

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

static qboolean WiredPC_DecodeColorString( const char *s, vec4_t out );  /* fwd decl */

static qboolean WiredPC_Color( int handle, vec4_t *color ) {
	/* accept BOTH the legacy 4-float form AND the spec-canonical
	 * single-token form (`#rrggbb`, `rgba(r,g,b,a)`, or `$ref` resolving to
	 * one of those). Peek the first token: if it's hex/rgba/rgb, decode in
	 * place; else treat as the first float of a quad and read 3 more. This
	 * makes ALL color-typed descriptor-table entries (forecolor, backcolor,
	 * bordercolor, focuscolor, etc.) accept the new spec syntax without
	 * duplicating disambiguation logic at every call site. */
	pc_token_t t;
	if ( !WiredPC_ReadTokenEval( handle, &t ) ) return qfalse;
	if ( t.string[ 0 ] == '#'
	  || !Q_stricmpn( t.string, "rgba", 4 )
	  || !Q_stricmpn( t.string, "rgb",  3 ) ) {
		return WiredPC_DecodeColorString( t.string, *color );
	}
	(*color)[ 0 ] = t.floatvalue;
	for ( int i = 1; i < 4; i++ ) {
		float f;
		if ( !WiredPC_Float( handle, &f ) ) {
			return qfalse;
		}
		(*color)[i] = f;
	}
	return qtrue;
}

/* decode a CSS-style color string into a vec4.
 *
 * Accepts:
 *   - `rgba(r,g,b,a)` / `rgb(r,g,b)` — RGB in CSS 0-255 ints; if any of
 *     r,g,b > 1.0 the whole triplet is scaled by 1/255 (alpha stays 0..1)
 *   - `#rrggbb` / `#rrggbbaa` — hex pairs scaled by 1/255
 *   - bare `r g b a` (space-separated, Quake-native 0..1 floats)
 *
 * Returns qfalse on parse failure (caller leaves target untouched). */
static qboolean WiredPC_DecodeColorString( const char *s, vec4_t out )
{
	const char *p;
	int         n;
	float       r, g, b, a;

	if ( !s || !*s ) return qfalse;
	if ( !Q_stricmpn( s, "rgba", 4 ) || !Q_stricmpn( s, "rgb", 3 ) ) {
		p = strchr( s, '(' );
		if ( !p ) return qfalse;
		p++;
		a = 1.0f;
		n = sscanf( p, "%f , %f , %f , %f", &r, &g, &b, &a );
		if ( n < 3 ) n = sscanf( p, "%f %f %f %f", &r, &g, &b, &a );
		if ( n < 3 ) return qfalse;
		/* CSS rgba() uses 0-255 ints for RGB; auto-detect by magnitude. */
		if ( r > 1.0f || g > 1.0f || b > 1.0f ) {
			r *= ( 1.0f / 255.0f );
			g *= ( 1.0f / 255.0f );
			b *= ( 1.0f / 255.0f );
		}
		out[0] = r; out[1] = g; out[2] = b;
		out[3] = ( n >= 4 ) ? a : 1.0f;
		return qtrue;
	}
	if ( s[ 0 ] == '#' ) {
		const char *h = s + 1;
		int len = (int) strlen( h );
		unsigned long v;
		if ( len != 6 && len != 8 ) return qfalse;
		v = strtoul( h, NULL, 16 );
		if ( len == 6 ) {
			out[0] = ( ( v >> 16 ) & 0xFF ) / 255.0f;
			out[1] = ( ( v >>  8 ) & 0xFF ) / 255.0f;
			out[2] = (   v         & 0xFF ) / 255.0f;
			out[3] = 1.0f;
		} else {
			out[0] = ( ( v >> 24 ) & 0xFF ) / 255.0f;
			out[1] = ( ( v >> 16 ) & 0xFF ) / 255.0f;
			out[2] = ( ( v >>  8 ) & 0xFF ) / 255.0f;
			out[3] = (   v         & 0xFF ) / 255.0f;
		}
		return qtrue;
	}

	a = 1.0f;
	n = sscanf( s, "%f %f %f %f", &r, &g, &b, &a );
	if ( n < 3 ) n = sscanf( s, "%f , %f , %f , %f", &r, &g, &b, &a );
	if ( n < 3 ) return qfalse;
	out[0] = r; out[1] = g; out[2] = b;
	out[3] = ( n >= 4 ) ? a : 1.0f;
	return qtrue;
}

/* read a color value that may be either:
 *   - four space-separated floats: `0.4 0.7 1.0 1.0`
 *   - a quoted/raw `rgba(...)` or `rgb(...)` string token
 *   - a `$ref` to a token holding either of the above
 *
 * The token-ref case is transparent — WiredPC_ReadTokenEval has already
 * substituted the value before we see it. We then dispatch by examining
 * the first token's leading characters. */
static qboolean WiredPC_ReadColorOrFloats( int handle, vec4_t out )
{
	pc_token_t t;
	if ( !WiredPC_ReadTokenEval( handle, &t ) ) return qfalse;

	if ( !Q_stricmpn( t.string, "rgba", 4 )
	  || !Q_stricmpn( t.string, "rgb",  3 )
	  ||  t.string[ 0 ] == '#' ) {
		return WiredPC_DecodeColorString( t.string, out );
	}
	/* Float quad — first float already consumed; read 3 more. */
	out[0] = t.floatvalue;
	{
		pc_token_t more;
		int        i;
		for ( i = 1; i < 4; i++ ) {
			if ( !WiredPC_ReadTokenEval( handle, &more ) ) return qfalse;
			out[i] = more.floatvalue;
		}
	}
	return qtrue;
}

// ── enum table lookup ─────────────────────────────────────────────────

#define WUI_ENUM_UNKNOWN 0x7FFFFFFF

typedef struct { const char *k; int v; } wuiEnumMap_t;

static int WiredPC_LookupEnum( const wuiEnumMap_t *map, const char *str, int defaultVal ) {
	for ( int i = 0; map[i].k; i++ ) {
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

static const wuiEnumMap_t s_compositeMap[] = {
	{ "diegetic", WUI_COMPOSITE_DIEGETIC },
	{ "overlay",  WUI_COMPOSITE_OVERLAY  },
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
	if ( !WiredPC_ReadToken( handle, &token ) || Q_stricmp( token.string, "{" ) != 0 ) {
		return qfalse;
	}
	int depth = 1;
	/* line-aware separator. pc_token_t doesn't carry the per-token
	 * line, but PC_SourceFileAndLine(handle) returns the scanner's current
	 * line — bumped past whitespace + the last-consumed token. Two
	 * consecutive ReadToken calls return strictly increasing values iff
	 * a newline lies between the tokens. That's exactly the signal we
	 * need: same line → continuation (space-join), new line → new
	 * statement (semicolon-join). Fixes the latent leak where .wmenu
	 * authors used newlines as statement separators only to have them
	 * silently flattened into argument-separator whitespace, so excess
	 * tokens fell off the end of one handler's arg cap and became
	 * pseudo-commands in the next iteration of WiredUI_RunScript. */
	qstring_t dest_qs  = { NULL, 0, 0 };
	if ( dest && destSize > 0 ) { dest[0] = '\0'; dest_qs = QS_Wrap( dest, destSize ); }
	/* Token joiner: legacy "line-aware" form (space within-line, ; across
	 * lines) leaked tokens together when botlib's PC_SourceFileAndLine
	 * returned the post-consume scanner position rather than the token's
	 * source line — adjacent quoted-string args ended up sharing the same
	 * curLine value with the *next* token's prev line, so the separator
	 * branch fell through to neither space nor ;. Always-space join +
	 * explicit `;` from authors via the captured body is unambiguous. */
	while ( depth > 0 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) break;
		if ( !Q_stricmp( token.string, "{" ) ) { depth++; continue; }
		if ( !Q_stricmp( token.string, "}" ) ) { depth--; continue; }
		if ( dest_qs.data ) {
			if ( !QS_Empty( &dest_qs ) ) {
				QS_AppendChar( &dest_qs, ' ' );
			}
			QS_Append( &dest_qs, token.string );
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

	/* Dispatch 5.16 S3: handle `$token` expansion where the resolver fused
	 * value+unit into token.string (e.g. "1.56vh"). Source-stream peek
	 * below would miss the unit since it isn't a separate token in the
	 * caller's source. */
	{
		const char *s = token.string;
		while ( *s && ( ( *s >= '0' && *s <= '9' ) || *s == '.' || *s == '-' ) ) s++;
		if ( *s ) {
			if      ( !Q_stricmp( s, "vw" ) ) { val.unit = UNIT_VW; return val; }
			else if ( !Q_stricmp( s, "vh" ) ) { val.unit = UNIT_VH; return val; }
			else if ( !Q_stricmp( s, "px" ) ) { val.unit = UNIT_PX; return val; }
		}
	}

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
	if ( !Q_stricmp( keyword, "scroll" ) ) {
		/* Mark this container as a vertical scroll viewport: children clip to
		 * the box and a wheel/drag offset scrolls them (Clay .clip.vertical).
		 * Needs a bounded height (e.g. height GROW / a rect height) so there is
		 * a viewport to clip against. Standalone keyword — no argument. */
		fc->scroll = qtrue;
		*isFlexContainer = qtrue;
		return qtrue;
	}
	if ( !Q_stricmp( keyword, "padding" ) ) {
		/* substitute `$ref` BEFORE checking whether the
		 * peeked token is numeric. Previously a `$content_padding_x`
		 * peek failed the digit/dot check (sees `$`) and the second/
		 * third/fourth padding args got silently dropped, eating
		 * subsequent keywords as collateral. Use ReadTokenEval +
		 * inspect the resolved string. */
		pc_token_t peek;
		fc->padding[0] = WiredPC_ParseValue( handle );
		if ( WiredPC_ReadTokenEval( handle, &peek ) ) {
			qboolean isNum1 = ( peek.type == TT_NUMBER
			                 || ( peek.string[0] >= '0' && peek.string[0] <= '9' )
			                 || peek.string[0] == '-'
			                 || peek.string[0] == '.' );
			if ( isNum1 ) {
				WiredPC_UnreadToken( &peek );
				fc->padding[1] = WiredPC_ParseValue( handle );
				if ( WiredPC_ReadTokenEval( handle, &peek ) ) {
					qboolean isNum2 = ( peek.type == TT_NUMBER
					                 || ( peek.string[0] >= '0' && peek.string[0] <= '9' )
					                 || peek.string[0] == '-'
					                 || peek.string[0] == '.' );
					if ( isNum2 ) {
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
	/* WUI Flexbox Authoring Migration spec §4
	 * keywords. `direction` is an alias for the existing `layout`
	 * keyword (column / row). `alignX` and `alignY` map onto the
	 * existing `align` (cross-axis) and `justify` (main-axis) state by
	 * the direction the container's set to — when direction=column
	 * `alignX` is cross-axis (= align) and `alignY` is main-axis
	 * (= justify); when direction=row the mapping flips. */
	if ( !Q_stricmp( keyword, "direction" ) ) {
		pc_token_t t;
		if ( WiredPC_ReadTokenEval( handle, &t ) ) {
			if      ( !Q_stricmp( t.string, "row"    ) ) { fc->direction = WUI_LAYOUT_ROW;    *isFlexContainer = qtrue; }
			else if ( !Q_stricmp( t.string, "column" ) ) { fc->direction = WUI_LAYOUT_COLUMN; *isFlexContainer = qtrue; }
		}
		return qtrue;
	}
	if ( !Q_stricmp( keyword, "alignX" ) || !Q_stricmp( keyword, "alignY" ) ) {
		pc_token_t val;
		int        which     = ( !Q_stricmp( keyword, "alignX" ) ) ? 0 : 1;
		int        mainAxis  = ( fc->direction == WUI_LAYOUT_ROW ) ? 0 : 1;
		if ( WiredPC_ReadTokenEval( handle, &val ) ) {
			/* `start` / `center` / `end` from the spec → existing enum
			 * values (WUI_ALIGN_* + WUI_JUSTIFY_* share a 3-element
			 * shape; `end` maps to the END value). */
			int isMain = ( which == mainAxis );
			if ( isMain ) {
				if      ( !Q_stricmp( val.string, "start"  ) ) fc->justify = WUI_JUSTIFY_START;
				else if ( !Q_stricmp( val.string, "center" ) ) fc->justify = WUI_JUSTIFY_CENTER;
				else if ( !Q_stricmp( val.string, "end"    ) ) fc->justify = WUI_JUSTIFY_END;
			} else {
				if      ( !Q_stricmp( val.string, "start"  ) ) fc->align = WUI_ALIGN_START;
				else if ( !Q_stricmp( val.string, "center" ) ) fc->align = WUI_ALIGN_CENTER;
				else if ( !Q_stricmp( val.string, "end"    ) ) fc->align = WUI_ALIGN_END;
			}
		}
		return qtrue;
	}
	return qfalse;
}

// ── item property parser (shared between top-level and nested items) ──

/* Parser threads the parent menu through ParseItemProperties so
 * compile-time chunk dispatch can resolve the menu's `vm` keyword. The
 * menu pointer is also useful for nested error messages + per-menu
 * parse context. */
static qboolean WiredUI_ParseItemProperties( int handle,
                                              const wiredMenuDef_t *menu,
                                              wiredItemDef_t *item );

// ── item parser ───────────────────────────────────────────────────────

static qboolean WiredUI_ParseItem( int handle, wiredMenuDef_t *menu ) {
	wiredItemDef_t *item = (wiredItemDef_t *)WiredUI_Alloc( sizeof( wiredItemDef_t ) );
	const char *prevItem = s_wui_parse_item;
	if ( !item ) {
		return qfalse;
	}
	/* source-attribution: item name not known until the
	 * `name "..."` keyword fires inside ParseItemProperties. Use the
	 * item pointer's address-stamped "(itemDef)" sentinel for now —
	 * line-number context already gives Eser enough to find it. */
	s_wui_parse_item = "(itemDef)";

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

	if ( !WiredUI_ParseItemProperties( handle, menu, item ) ) {
		s_wui_parse_item = prevItem;
		return qfalse;
	}

	if ( menu->itemCount < WIRED_MAX_ITEMS_PER_MENU ) {
		menu->items[menu->itemCount++] = item;
	}

	s_wui_parse_item = prevItem;
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
	const char *str = NULL;
	if ( WiredPC_String( handle, &str ) ) {
		int a = WiredPC_LookupEnum( s_anchorMap, str, WUI_ENUM_UNKNOWN );
		if ( a == WUI_ENUM_UNKNOWN ) {
			COM_WARN( LOG_CH(ch_ui), "WARNING: unknown anchor '%s'\n", str );
			*anchor = ANCHOR_NONE;
		} else {
			*anchor = (wiredAnchor_t)a;
		}
	}
}

static void WiredPC_ParseCompositeInto( int handle, wuiCompositeMode_t *mode ) {
	const char *str = NULL;
	if ( WiredPC_String( handle, &str ) ) {
		int m = WiredPC_LookupEnum( s_compositeMap, str, WUI_ENUM_UNKNOWN );
		if ( m == WUI_ENUM_UNKNOWN ) {
			COM_WARN( LOG_CH(ch_ui), "WARNING: unknown composite mode '%s'\n", str );
			*mode = WUI_COMPOSITE_DIEGETIC;
		} else {
			*mode = (wuiCompositeMode_t)m;
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
	for ( int i = 0; tbl[i].key; i++ ) {
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
	/* "background" handled manually below — disambiguates spec-syntax color
	 * (`background $token` / `background rgba(...)` / `background #hex`)
	 * from legacy shader-name form (`background "gfx/path"`). */
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
	/* declarative animated rect width. The compositor's
	 * emit scales the parsed rect's width by the [0..1]-clamped float at
	 * this store key. */
	WP_S( "bindwidth",        wiredItemDef_t, storeBindWidth   ),
	WP_S( "showbind",         wiredItemDef_t, showBind         ),
	WP_S( "hidebind",         wiredItemDef_t, hideBind         ),
	WP_S( "source",           wiredItemDef_t, tableSource      ),
	WP_S( "countbind",        wiredItemDef_t, tableCountBind   ),
	/* ints */
	/* "type" handled manually below — accepts numeric ID or string token
	 * ("viewport" → ITEM_TYPE_VIEWPORT). Future string-type tokens slot in
	 * the same handler. */
	WP_I( "style",            wiredItemDef_t, style            ),
	WP_I( "textalign",        wiredItemDef_t, textalign        ),
	WP_I( "textstyle",        wiredItemDef_t, textstyle        ),
	/* "border" handled manually below — disambiguates legacy single-int
	 * (`border 1` = WINDOW_BORDER_FULL) from spec shorthand
	 * (`border 1px <color|$ref>`). */
	/* "ownerdraw" is parsed manually below — accepts numeric ID or quoted name */
	WP_I( "ownerdrawFlag",    wiredItemDef_t, ownerdrawFlag    ),
	WP_I( "maxChars",         wiredItemDef_t, maxChars         ),
	WP_I( "maxPaintChars",    wiredItemDef_t, maxPaintChars    ),
	WP_I( "password",         wiredItemDef_t, password         ),
	WP_I( "fadedelay",        wiredItemDef_t, fadeDelay        ),
	WP_I( "time",             wiredItemDef_t, timeMs           ),
	WP_I( "widescreen",       wiredItemDef_t, modelWidescreen  ),
	/* "visible" handled manually below — accepts both int (0/1) and
	 * "lua:..." prefix (compile-time recognition; per-frame eval
	 * wired separately). */
	WP_I( "teamfilter",       wiredItemDef_t, tableTeamFilter  ),
	/* floats */
	WP_F( "textalignx",       wiredItemDef_t, textalignx       ),
	WP_F( "textaligny",       wiredItemDef_t, textaligny       ),
	WP_F( "textscale",        wiredItemDef_t, textscale        ),
	WP_F( "bordersize",       wiredItemDef_t, bordersize       ),
	WP_F( "special",          wiredItemDef_t, special          ),
	/* "feeder" is parsed manually below — accepts either a numeric ID or a quoted name */
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
	/* WiredUI F4: mark this control as the menu's DEFAULT (Enter confirms).
	 * On open, initial keyboard focus lands here; the menu-level Enter handler
	 * also falls back to this item's action when nothing actionable is focused. */
	WP_FL( "defaultButton",   wiredItemDef_t, defaultButton    ),
	{ NULL, 0, 0, 0 }
};

static const wuiPropDef_t s_menuProps[] = {
	/* strings */
	WP_S( "name",             wiredMenuDef_t, name             ),
	WP_S( "background",       wiredMenuDef_t, background       ),
	WP_S( "soundLoop",        wiredMenuDef_t, soundLoop        ),
	/* panel-level VM selection. Valid values:
	 * "system" | "user" | "" (infer from loader). Storage only for now;
	 * the per-chunk VM dispatch is wired separately. */
	WP_S( "vm",               wiredMenuDef_t, vm               ),
	/* ints (qboolean fields are typedef int, WP_INT works) */
	WP_I( "style",            wiredMenuDef_t, style            ),
	WP_I( "border",           wiredMenuDef_t, border           ),
	WP_I( "fadeCycle",        wiredMenuDef_t, fadeCycle        ),
	WP_I( "fullscreen",       wiredMenuDef_t, fullscreen       ),
	WP_I( "visible",          wiredMenuDef_t, visible          ),
	/* hudOverlay keyword retired; the `layer "hud"`
	 * keyword replaces it. Caught explicitly below with SEV_ERROR. */
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

static qboolean WiredUI_ParseItemProperties( int handle,
                                              const wiredMenuDef_t *menu,
                                              wiredItemDef_t *item ) {
	pc_token_t      token;
	const char     *str;

	// expect opening brace
	if ( !WiredPC_Expect( handle, "{" ) ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: expected '{' for itemDef\n" );
		return qfalse;
	}

	while ( 1 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) {
			return qfalse;
		}
		if ( !Q_stricmp( token.string, "}" ) ) {
			break;
		}

		/* `<keyword>.active` dot-suffix
		 * syntax: botlib tokenises the dot as P_REF punctuation, so the
		 * keyword arrives as three tokens (e.g. `forecolor`, `.`,
		 * `active`). Peek the next two tokens; if they form `.active`,
		 * synthesise a fused keyword string (`forecolor.active`) for
		 * the existing else-if chain to match. Otherwise push both back
		 * so the legacy match path (ApplyProp + manual keywords) sees
		 * the original token. */
		{
			pc_token_t dotPeek, activePeek;
			if ( WiredPC_ReadToken( handle, &dotPeek ) ) {
				if ( dotPeek.string[0] == '.' && dotPeek.string[1] == '\0' ) {
					if ( WiredPC_ReadToken( handle, &activePeek ) ) {
						if ( !Q_stricmp( activePeek.string, "active" ) ) {
							/* Fuse into `keyword.active`. */
							int baseLen = (int) strlen( token.string );
							if ( baseLen + 7 < (int) sizeof( token.string ) ) {
								token.string[ baseLen     ] = '.';
								memcpy( token.string + baseLen + 1, "active", 7 );
							}
						} else {
							WiredPC_UnreadToken( &activePeek );
							WiredPC_UnreadToken( &dotPeek );
						}
					} else {
						WiredPC_UnreadToken( &dotPeek );
					}
				} else {
					WiredPC_UnreadToken( &dotPeek );
				}
			}
		}

		// ── item keywords ─────────────────────────────────────────
		if ( WiredPC_ApplyProp( handle, item, s_itemProps, token.string ) ) {
			/* consumed by table */
		}
		else if ( !Q_stricmp( token.string, "rect" ) ) {
			WiredPC_ParseRectInto( handle, &item->wuiRect, &item->rect );
		}
		else if ( !Q_stricmp( token.string, "type" ) ) {
			/* Accept numeric ID (legacy ITEM_TYPE_*) or string token
			 * ("viewport" → ITEM_TYPE_VIEWPORT, "scorelist_widget" →
			 * ITEM_TYPE_SCORELIST_WIDGET, "console_view" →
			 * ITEM_TYPE_CONSOLE_VIEW, "container" → flex container
			 * marker per WUI Flexbox Authoring Migration). */
			pc_token_t ttok;
			if ( WiredPC_ReadToken( handle, &ttok ) ) {
				if ( !Q_stricmp( ttok.string, "viewport" ) ) {
					item->type = ITEM_TYPE_VIEWPORT;
				} else if ( !Q_stricmp( ttok.string, "scorelist_widget" ) ) {
					item->type = ITEM_TYPE_SCORELIST_WIDGET;
				} else if ( !Q_stricmp( ttok.string, "console_view" ) ) {
					item->type = ITEM_TYPE_CONSOLE_VIEW;
				} else if ( !Q_stricmp( ttok.string, "container" ) ) {
					/* Sets the existing flex-container flag so the
					 * nested-itemDef path (lines below this big switch)
					 * accepts children. type is left at the parser
					 * default (TEXT=0); the container has no drawable
					 * content itself, only layout + children. */
					item->isFlexContainer = qtrue;
				} else if ( !Q_stricmp( ttok.string, "checkbox" ) ) {
					/* Component-library F2: real checkbox control
					 * (ITEM_TYPE_CHECKBOX=3). Binds to a 0/1 cvar exactly
					 * like yesno; renders a box + check glyph (see
					 * cl_wired_clay.c) instead of the "Yes/No" text.
					 * Numeric `type 3` parses via atoi below too; this is
					 * the readable authoring alias. */
					item->type = ITEM_TYPE_CHECKBOX;
				} else if ( !Q_stricmp( ttok.string, "radio" )
				         || !Q_stricmp( ttok.string, "segmented" ) ) {
					/* Component-library: segmented / radio control
					 * (ITEM_TYPE_RADIOBUTTON=2). A cvar-bound mutually-
					 * exclusive choice rendered inline as N horizontal segments
					 * (2-4 typical). Options come from the same cvarStrList {
					 * "Label" "val" ... } / cvarFloatList source the dropdowns
					 * use; left/right + click select. Numeric `type 2` parses via
					 * atoi below too; this is the readable authoring alias. */
					item->type = ITEM_TYPE_RADIOBUTTON;
				} else if ( !Q_stricmp( ttok.string, "spinner" ) ) {
					/* Component-library: numeric stepper (ITEM_TYPE_SPINNER=14).
					 * A cvar-bound integer with up/down buttons, min/max/step.
					 * Bind via `cvarSpinner "cvar" def min max step`. */
					item->type = ITEM_TYPE_SPINNER;
				} else if ( !Q_stricmp( ttok.string, "slider" ) ) {
					/* Component-library: draggable slider (ITEM_TYPE_SLIDER=10).
					 * A cvar-bound float with a track + fill + thumb handle and a
					 * numeric readout. Bind via `cvarFloat "cvar" def min max`.
					 * Numeric `type 10` parses via atoi below too; this is the
					 * readable authoring alias. */
					item->type = ITEM_TYPE_SLIDER;
				} else {
					item->type = atoi( ttok.string );
				}
			}
		}
		else if ( !Q_stricmp( token.string, "id" ) ) {
			/* Provider-registry lookup key for viewport itemDefs. Other
			 * item types currently ignore the field. */
			if ( WiredPC_String( handle, &str ) ) {
				Q_strncpyz( item->viewportId, str, sizeof( item->viewportId ) );
			}
		}
		else if ( !Q_stricmp( token.string, "cvarFloat" ) ) {
			// cvarFloat "cvarname" defVal minVal maxVal
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->cvar, str, sizeof( item->cvar ) );
			WiredPC_Float( handle, &item->sliderData.defVal );
			WiredPC_Float( handle, &item->sliderData.minVal );
			WiredPC_Float( handle, &item->sliderData.maxVal );
		}
		else if ( !Q_stricmp( token.string, "cvarSpinner" ) ) {
			// cvarSpinner "cvarname" defVal minVal maxVal step
			// SPINNER value binding. Reuses sliderData for def/min/max and adds
			// the per-tick step. step<=0 => render/input fall back to range/20.
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( item->cvar, str, sizeof( item->cvar ) );
			WiredPC_Float( handle, &item->sliderData.defVal );
			WiredPC_Float( handle, &item->sliderData.minVal );
			WiredPC_Float( handle, &item->sliderData.maxVal );
			WiredPC_Float( handle, &item->sliderData.step );
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
			int numCols = 0;
			WiredPC_Int( handle, &numCols );
			if ( numCols > 8 ) numCols = 8;
			item->columns = numCols;
			for ( int c = 0; c < numCols; c++ ) {
				int pos, width, maxchars;
				WiredPC_Int( handle, &pos );     // column position (unused for now)
				WiredPC_Int( handle, &width );
				WiredPC_Int( handle, &maxchars ); // max chars per column (unused for now)
				// NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Assign) — WiredPC_Int writes through the address; analyzer doesn't model the parser callback
				item->columnWidths[c] = width;
			}
		}
		else if ( !Q_stricmp( token.string, "columnHeaders" ) ) {
			// columnHeaders { 0 "#" 1 "Map" 2 "Name" 3 "*" }
			// Per-column header labels for the engine-drawn header band, keyed by
			// column index. The index token between labels is REQUIRED — botlib's
			// PC tokenizer merges adjacent quoted string literals (C-style
			// "a" "b" -> "ab"), so a bare `{ "#" "Map" }` collapses into one
			// token. The integer separator (also lets a column be skipped) keeps
			// each label its own token. Order maps 1:1 onto the `columns` list;
			// the header band uses the same wui_listbox_column_geom geometry as
			// the body rows so the labels sit directly above their columns.
			item->columnHeaderCount = 0;
			if ( !WiredPC_Expect( handle, "{" ) ) continue;
			while ( 1 ) {
				int idx;
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "}" ) ) break;
				idx = atoi( token.string );
				if ( !WiredPC_ReadToken( handle, &token ) ) break;
				if ( !Q_stricmp( token.string, "}" ) ) break;
				if ( idx >= 0 && idx < 8 ) {
					Q_strncpyz( item->columnHeaders[idx], token.string,
						sizeof( item->columnHeaders[0] ) );
					if ( idx + 1 > item->columnHeaderCount )
						item->columnHeaderCount = idx + 1;
				}
			}
		}
		else if ( !Q_stricmp( token.string, "columnSort" ) ) {
			// columnSort "MapSort" — script command fired as "<cmd> <col>" when
			// a header cell is clicked. Empty = header labels drawn but inert.
			const char *cmd = NULL;
			if ( WiredPC_String( handle, &cmd ) && cmd ) {
				Q_strncpyz( item->columnSortCmd, cmd, sizeof( item->columnSortCmd ) );
			}
		}
		else if ( !Q_stricmp( token.string, "elementtype" ) ) {
			WiredPC_Int( handle, &item->elementtype );
		}
		else if ( !Q_stricmp( token.string, "feeder" ) ) {
			/* Accept either `feeder 16` (numeric ID) or `feeder "characters"` (symbolic name).
			   Strings are resolved against the registry; unknown names log a warning and
			   leave feeder == 0 (which the renderer treats as "no feeder set"). */
			const char *fstr = NULL;
			if ( WiredPC_String( handle, &fstr ) && fstr ) {
				if ( fstr[0] >= '0' && fstr[0] <= '9' ) {
					item->feeder = (float)atoi( fstr );
				} else {
					int id = WiredUI_FeederIDByName( fstr );
					if ( id == 0 ) {
						COM_WARN( LOG_CH(ch_ui), "WiredUI: unknown feeder name '%s'\n", fstr );
					}
					item->feeder = (float)id;
				}
			}
		}
		else if ( !Q_stricmp( token.string, "ownerdraw" ) ) {
			/* Same shape as `feeder`: numeric ID or quoted name resolved against
			   the ownerdraw dispatch table. Unknown names log a warning and leave
			   ownerdraw == 0 (renderer treats 0 as "no ownerdraw" and skips).
			   also write "od:<name>" into item->customDrawName
			   so the unified registry dispatch picks it up. The numeric
			   item->ownerdraw field stays populated for the legacy SCR
			   dispatch — half-life: dies once the legacy dispatch is retired. */
			const char *odstr = NULL;
			if ( WiredPC_String( handle, &odstr ) && odstr ) {
				if ( odstr[0] >= '0' && odstr[0] <= '9' ) {
					item->ownerdraw = atoi( odstr );
					/* Numeric form has no name to sigil-prefix; leave
					 * customDrawName empty. Legacy SCR dispatch still
					 * fires via the numeric id. */
				} else {
					int id = WiredUI_OwnerDrawIDByName( odstr );
					if ( id == 0 ) {
						COM_WARN( LOG_CH(ch_ui), "WiredUI: unknown ownerdraw name '%s'\n", odstr );
					}
					item->ownerdraw = id;
					Com_sprintf( item->customDrawName, sizeof( item->customDrawName ),
						"od:%s", odstr );
				}
			}
		}
		else if ( !Q_stricmp( token.string, "custom" ) ) {
			/* new keyword for the unified registry.
			 * `custom "<name>"` writes "custom:<name>" into the item's
			 * registry slot. No legacy backing — strictly the new path. */
			const char *cstr = NULL;
			if ( WiredPC_String( handle, &cstr ) && cstr ) {
				Com_sprintf( item->customDrawName, sizeof( item->customDrawName ),
					"custom:%s", cstr );
			}
		}
		else if ( !Q_stricmp( token.string, "anchor" ) ) {
			WiredPC_ParseAnchorInto( handle, &item->anchor );
		}
		else if ( !Q_stricmp( token.string, "composite" ) ) {
			WiredPC_ParseCompositeInto( handle, &item->compositeMode );
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
		/* component-library F1: how a disabled item renders.
		 *   enableMode hide  — legacy default (cull, unchanged for all menus)
		 *   enableMode dim   — render greyed + non-interactive (new path) */
		else if ( !Q_stricmp( token.string, "enableMode" ) ) {
			const char *mode = NULL;
			if ( WiredPC_String( handle, &mode ) && mode ) {
				if ( !Q_stricmp( mode, "dim" ) )       item->enableModeDim = qtrue;
				else if ( !Q_stricmp( mode, "hide" ) ) item->enableModeDim = qfalse;
			}
		}
		/* component-library F1: per-item interaction-state colours. Allocated on
		 * demand so items without a state keyword cost zero extra bytes. Each
		 * keyword sets one vec4 in the side-struct + its has* gate. Unset
		 * variants fall through the resolver chain (pressed→focused→hover→base),
		 * so authoring only `hovercolor` leaves pressed/focused/disabled at base.
		 * Exact-match keywords only (no prefix guard) so nothing else collides. */
		else if ( !Q_stricmp( token.string, "hovercolor" )          || !Q_stricmp( token.string, "hoverBackcolor" )     || !Q_stricmp( token.string, "hoverBordercolor" )
		       || !Q_stricmp( token.string, "pressedcolor" )        || !Q_stricmp( token.string, "pressedBackcolor" )   || !Q_stricmp( token.string, "pressedBordercolor" )
		       || !Q_stricmp( token.string, "focusedcolor" )        || !Q_stricmp( token.string, "focusedBackcolor" )   || !Q_stricmp( token.string, "focusedBordercolor" )
		       || !Q_stricmp( token.string, "disabledcolor" )       || !Q_stricmp( token.string, "disabledBackcolor" )  || !Q_stricmp( token.string, "disabledBordercolor" ) ) {
			vec4_t tmp;
			if ( WiredPC_Color( handle, &tmp ) ) {
				if ( !item->stateColors ) {
					item->stateColors = (wuiStateColors_t *)WiredUI_Alloc( sizeof( wuiStateColors_t ) );
				}
				if ( item->stateColors ) {
					wuiStateColors_t *sc = item->stateColors;
					vec_t   *dst  = NULL;
					qboolean *flag = NULL;
					if      ( !Q_stricmp( token.string, "hovercolor" )          ) { dst = sc->hoverFg;        flag = &sc->hasHover; }
					else if ( !Q_stricmp( token.string, "hoverBackcolor" )      ) { dst = sc->hoverBg;        flag = &sc->hasHover; }
					else if ( !Q_stricmp( token.string, "hoverBordercolor" )    ) { dst = sc->hoverBorder;    flag = &sc->hasHover; }
					else if ( !Q_stricmp( token.string, "pressedcolor" )        ) { dst = sc->pressedFg;      flag = &sc->hasPressed; }
					else if ( !Q_stricmp( token.string, "pressedBackcolor" )    ) { dst = sc->pressedBg;      flag = &sc->hasPressed; }
					else if ( !Q_stricmp( token.string, "pressedBordercolor" )  ) { dst = sc->pressedBorder;  flag = &sc->hasPressed; }
					else if ( !Q_stricmp( token.string, "focusedcolor" )        ) { dst = sc->focusedFg;      flag = &sc->hasFocused; }
					else if ( !Q_stricmp( token.string, "focusedBackcolor" )    ) { dst = sc->focusedBg;      flag = &sc->hasFocused; }
					else if ( !Q_stricmp( token.string, "focusedBordercolor" )  ) { dst = sc->focusedBorder;  flag = &sc->hasFocused; }
					else if ( !Q_stricmp( token.string, "disabledcolor" )       ) { dst = sc->disabledFg;     flag = &sc->hasDisabled; }
					else if ( !Q_stricmp( token.string, "disabledBackcolor" )   ) { dst = sc->disabledBg;     flag = &sc->hasDisabled; }
					else if ( !Q_stricmp( token.string, "disabledBordercolor" ) ) { dst = sc->disabledBorder; flag = &sc->hasDisabled; }
					if ( dst )  Vector4Copy( tmp, dst );
					if ( flag ) *flag = qtrue;
				}
			}
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

				/* route compile through the per-menu VM dispatcher
				 * so panels with `vm "user"` compile against the User VM.
				 * An earlier version hardcoded WiredScript_CompileChunk; the
				 * eval-time dispatcher already routes calls per menu->vm,
				 * so this aligns parse + eval to the same VM. */
				if ( str[0] == 'l' && str[1] == 'u' && str[2] == 'a' && str[3] == ':' ) {
					char chunkName[ 192 ];
					Com_sprintf( chunkName, sizeof( chunkName ),
						"%s.bind", item->name[0] ? item->name : "<anon>" );
					item->luaBindChunk = WiredUI_CompositorCompileChunkForMenu( menu, str + 4, chunkName );
				}
			}
		}
		else if ( !Q_stricmp( token.string, "visible" ) ) {
			/* visible accepts either an int (0/1,
			 * legacy) or "lua:..." (compiled chunk). Peek the next token to
			 * decide. The lua-form compile dispatches via the menu's VM. */
			pc_token_t vtok;
			if ( WiredPC_ReadToken( handle, &vtok ) ) {
				if ( vtok.string[0] == 'l' && vtok.string[1] == 'u' && vtok.string[2] == 'a' && vtok.string[3] == ':' ) {
					char chunkName[ 192 ];
					Com_sprintf( chunkName, sizeof( chunkName ),
						"%s.visible", item->name[0] ? item->name : "<anon>" );
					item->luaVisibleChunk = WiredUI_CompositorCompileChunkForMenu( menu, vtok.string + 4, chunkName );
					/* Default visible=qtrue at chunk-prep; runtime eval
					 * overrides per-frame. */
					item->visible = qtrue;
				} else {
					/* Legacy int form. Push back + use the existing int
					 * parse pattern. */
					WiredPC_UnreadToken( &vtok );
					{
						int v = 0;
						WiredPC_Int( handle, &v );
						item->visible = (qboolean) ( v != 0 );
					}
				}
			}
		}
		else if ( !Q_stricmp( token.string, "repeat" ) ) {
			/* repeat { source / countbind / as / itemDef }
			 * — per-frame expansion in cl_wired_clay.c. */
			pc_token_t  subTok;
			if ( WiredPC_ReadToken( handle, &subTok ) && !Q_stricmp( subTok.string, "{" ) ) {
				wiredRepeatBlock_t *rb;
				rb = (wiredRepeatBlock_t *) WiredUI_Alloc( sizeof( *rb ) );
				if ( !rb ) {
					WiredPC_SkipBracedBlock( handle );
					continue;
				}
				memset( rb, 0, sizeof( *rb ) );
				Q_strncpyz( rb->asName, "row", sizeof( rb->asName ) );  /* default */

				while ( WiredPC_ReadToken( handle, &subTok ) ) {
					if ( !Q_stricmp( subTok.string, "}" ) ) break;

					if ( !Q_stricmp( subTok.string, "source" ) ) {
						if ( WiredPC_String( handle, &str ) ) {
							Q_strncpyz( rb->source, str, sizeof( rb->source ) );
							if ( str[0] == 'l' && str[1] == 'u' && str[2] == 'a' && str[3] == ':' ) {
								char chunkName[ 192 ];
								rb->sourceIsLua = qtrue;
								Com_sprintf( chunkName, sizeof( chunkName ),
									"%s.repeat.source", item->name[0] ? item->name : "<anon>" );
								rb->sourceLuaChunk = WiredUI_CompositorCompileChunkForMenu( menu, str + 4, chunkName );
							}
						}
					}
					else if ( !Q_stricmp( subTok.string, "countbind" ) ) {
						if ( WiredPC_String( handle, &str ) ) {
							Q_strncpyz( rb->countBind, str, sizeof( rb->countBind ) );
						}
					}
					else if ( !Q_stricmp( subTok.string, "as" ) ) {
						if ( WiredPC_String( handle, &str ) ) {
							Q_strncpyz( rb->asName, str, sizeof( rb->asName ) );
						}
					}
					else if ( !Q_stricmp( subTok.string, "itemDef" ) ) {
						wiredItemDef_t *tmpl = (wiredItemDef_t *) WiredUI_Alloc( sizeof( *tmpl ) );
						if ( tmpl ) {
							memset( tmpl, 0, sizeof( *tmpl ) );
							tmpl->visible = qtrue;
							tmpl->forecolor[0] = tmpl->forecolor[1] = tmpl->forecolor[2] = tmpl->forecolor[3] = 1.0f;
							tmpl->textscale = 0.3f;
							tmpl->textalign = -1;
							tmpl->alignV = -1;
							tmpl->direction = -1;
							tmpl->fadeAlphaItem = 1.0f;
							tmpl->anchor = ANCHOR_NONE;
							tmpl->flexChild.shrink = 1.0f;
							if ( WiredUI_ParseItemProperties( handle, menu, tmpl ) ) {
								rb->templateItem = tmpl;
							}
						} else {
							/* template alloc failed — skip the body. */
							if ( WiredPC_Expect( handle, "{" ) ) {
								WiredPC_SkipBracedBlock( handle );
							}
						}
					}
					else {
						/* unknown — smart skip. */
						pc_token_t vv;
						if ( WiredPC_ReadToken( handle, &vv ) ) {
							if ( vv.string[0] == '{' ) WiredPC_SkipBracedBlock( handle );
						}
					}
				}

				/* M-18 warning: focusable template root must have a name
				 * (per-row IDs otherwise collide → broken hit-test). */
				if ( rb->templateItem
				  && !rb->templateItem->notselectable
				  && !rb->templateItem->decoration
				  && rb->templateItem->name[0] == '\0' ) {
					Com_Log( SEV_WARN, LOG_CH(ch_ui),
						"WiredUI: repeat template is focusable but lacks a 'name' keyword "
						"(per-row IDs will collide; M-18 warning)\n" );
				}

				item->repeatBlock = rb;
			}
		}
		else if ( !Q_stricmp( token.string, "if" ) ) {
			/* if { test "lua:..." / itemDef ... } — full
			 * semantics. Parser captures children + compiles test chunk;
			 * per-frame eval prunes or emits children in cl_wired_clay.c. */
			pc_token_t subTok;
			if ( WiredPC_ReadToken( handle, &subTok ) && !Q_stricmp( subTok.string, "{" ) ) {
				wiredIfBlock_t *ib;
				ib = (wiredIfBlock_t *) WiredUI_Alloc( sizeof( *ib ) );
				if ( !ib ) {
					WiredPC_SkipBracedBlock( handle );
					continue;
				}
				memset( ib, 0, sizeof( *ib ) );

				while ( WiredPC_ReadToken( handle, &subTok ) ) {
					if ( !Q_stricmp( subTok.string, "}" ) ) break;

					if ( !Q_stricmp( subTok.string, "test" ) ) {
						if ( WiredPC_String( handle, &str ) ) {
							Q_strncpyz( ib->testExpr, str, sizeof( ib->testExpr ) );
							if ( str[0] == 'l' && str[1] == 'u' && str[2] == 'a' && str[3] == ':' ) {
								char chunkName[ 192 ];
								Com_sprintf( chunkName, sizeof( chunkName ),
									"%s.if.test", item->name[0] ? item->name : "<anon>" );
								ib->testLuaChunk = WiredUI_CompositorCompileChunkForMenu( menu, str + 4, chunkName );
							}
						}
					}
					else if ( !Q_stricmp( subTok.string, "itemDef" ) ) {
						/* capture the child for per-frame
						 * conditional emit. Capacity cap = WIRED_MAX_IF_CHILDREN. */
						if ( ib->childCount < WIRED_MAX_IF_CHILDREN ) {
							wiredItemDef_t *child = (wiredItemDef_t *) WiredUI_Alloc( sizeof( *child ) );
							if ( child ) {
								memset( child, 0, sizeof( *child ) );
								child->visible = qtrue;
								child->forecolor[0] = child->forecolor[1] = child->forecolor[2] = child->forecolor[3] = 1.0f;
								child->textscale = 0.3f;
								child->textalign = -1;
								child->alignV = -1;
								child->direction = -1;
								child->fadeAlphaItem = 1.0f;
								child->anchor = ANCHOR_NONE;
								child->flexChild.shrink = 1.0f;
								if ( WiredUI_ParseItemProperties( handle, menu, child ) ) {
									ib->children[ ib->childCount++ ] = child;
								}
							} else if ( WiredPC_Expect( handle, "{" ) ) {
								WiredPC_SkipBracedBlock( handle );
							}
						} else {
							/* exceeded cap — skip body, log once */
							Com_Log( SEV_WARN, LOG_CH(ch_ui),
								"WiredUI: if block has more than %d itemDef children — extras skipped\n",
								WIRED_MAX_IF_CHILDREN );
							if ( WiredPC_Expect( handle, "{" ) ) {
								WiredPC_SkipBracedBlock( handle );
							}
						}
					}
					else {
						pc_token_t vv;
						if ( WiredPC_ReadToken( handle, &vv ) ) {
							if ( vv.string[0] == '{' ) WiredPC_SkipBracedBlock( handle );
						}
					}
				}

				item->ifBlock = ib;
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
			/* `direction` is shared between two
			 * semantics: legacy HUD widget direction (R/L/T/B for statusbar
			 * bar fill direction, see modfiles/ui/default.wui:145-156) and
			 * the modern flex container axis (row/column, see WiredPC_ParseFlexProps).
			 * The legacy handler used to fire first and silently swallowed
			 * `direction column`, leaving flexContainer.direction at NONE —
			 * which then degraded every nested flex tree to default ROW
			 * layout. Peek the value and dispatch: row/column → flex; else
			 * legacy enum. */
			pc_token_t peek;
			if ( WiredPC_ReadTokenEval( handle, &peek ) ) {
				if ( !Q_stricmp( peek.string, "row" ) ) {
					item->flexContainer.direction = WUI_LAYOUT_ROW;
					item->isFlexContainer = qtrue;
				}
				else if ( !Q_stricmp( peek.string, "column" ) ) {
					item->flexContainer.direction = WUI_LAYOUT_COLUMN;
					item->isFlexContainer = qtrue;
				}
				else {
					int d = WiredPC_LookupEnum( s_directionMap, peek.string, WUI_ENUM_UNKNOWN );
					item->direction = ( d != WUI_ENUM_UNKNOWN ) ? d : atoi( peek.string );
				}
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
				item->execKeyCode = (byte)keyStr[0];
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
			/* consumed by flex-container helper */
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
		// ── Dispatch 5.5: CSS-style per-side offsets ─────────────────
		// Single value per keyword; fraction relative to parent rect on
		// the matching axis. Auto-promotes the item to POSITION_ABSOLUTE
		// during Clay-emit when any side is set.
		else if ( !Q_stricmp( token.string, "top" ) ) {
			item->wuiOffset.top = WiredPC_ParseValue( handle ); item->wuiOffset.hasTop = qtrue;
		}
		else if ( !Q_stricmp( token.string, "left" ) ) {
			item->wuiOffset.left = WiredPC_ParseValue( handle ); item->wuiOffset.hasLeft = qtrue;
		}
		else if ( !Q_stricmp( token.string, "right" ) ) {
			item->wuiOffset.right = WiredPC_ParseValue( handle ); item->wuiOffset.hasRight = qtrue;
		}
		else if ( !Q_stricmp( token.string, "bottom" ) ) {
			item->wuiOffset.bottom = WiredPC_ParseValue( handle ); item->wuiOffset.hasBottom = qtrue;
		}
		// ── Dispatch 5.5: CSS-style margins (flex-child gap contributor) ──
		else if ( !Q_stricmp( token.string, "marginTop" ) ) {
			item->wuiMargin.top = WiredPC_ParseValue( handle ); item->wuiMargin.hasAny = qtrue;
		}
		else if ( !Q_stricmp( token.string, "marginRight" ) ) {
			item->wuiMargin.right = WiredPC_ParseValue( handle ); item->wuiMargin.hasAny = qtrue;
		}
		else if ( !Q_stricmp( token.string, "marginBottom" ) ) {
			item->wuiMargin.bottom = WiredPC_ParseValue( handle ); item->wuiMargin.hasAny = qtrue;
		}
		else if ( !Q_stricmp( token.string, "marginLeft" ) ) {
			item->wuiMargin.left = WiredPC_ParseValue( handle ); item->wuiMargin.hasAny = qtrue;
		}
		else if ( !Q_stricmp( token.string, "margin" ) ) {
			/* 1-4 value form mirroring `padding`: 1=all, 2=v+h, 3=t+h+b, 4=t+r+b+l. */
			pc_token_t peek;
			item->wuiMargin.top = WiredPC_ParseValue( handle );
			item->wuiMargin.hasAny = qtrue;
			if ( WiredPC_ReadTokenEval( handle, &peek ) ) {
				qboolean isNum1 = ( peek.type == TT_NUMBER
				                 || ( peek.string[0] >= '0' && peek.string[0] <= '9' )
				                 || peek.string[0] == '-' || peek.string[0] == '.' );
				if ( isNum1 ) {
					WiredPC_UnreadToken( &peek );
					item->wuiMargin.right = WiredPC_ParseValue( handle );
					if ( WiredPC_ReadTokenEval( handle, &peek ) ) {
						qboolean isNum2 = ( peek.type == TT_NUMBER
						                 || ( peek.string[0] >= '0' && peek.string[0] <= '9' )
						                 || peek.string[0] == '-' || peek.string[0] == '.' );
						if ( isNum2 ) {
							WiredPC_UnreadToken( &peek );
							item->wuiMargin.bottom = WiredPC_ParseValue( handle );
							item->wuiMargin.left = WiredPC_ParseValue( handle );
						} else {
							WiredPC_UnreadToken( &peek );
							item->wuiMargin.bottom = item->wuiMargin.top;
							item->wuiMargin.left = item->wuiMargin.right;
						}
					}
				} else {
					WiredPC_UnreadToken( &peek );
					item->wuiMargin.right = item->wuiMargin.bottom = item->wuiMargin.left = item->wuiMargin.top;
				}
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
		/* WUI Flexbox Authoring Migration spec §4 —
		 * `width <MODE> [v]` / `height <MODE> [v]`. Modes map onto the
		 * existing wuiRect_t + flexChild.grow storage so the Clay emit
		 * (cl_wired_clay.c:2050+) needs no plumbing changes:
		 *   FIT          → unit=UNIT_AUTO,  value=0    (Clay falls through to
		 *                                               resolvedRect FIXED, OK
		 *                                               for the minimum)
		 *   GROW         → flexChild.grow=1.0          (Clay → CLAY_SIZING_GROW)
		 *   PERCENT v    → unit=UNIT_NORM,  value=v    (Clay → SIZING_PERCENT)
		 *   FIXED v      → unit=UNIT_NORM,  value=v    (Clay → SIZING_PERCENT)
		 *   FIXED Npx    → unit=UNIT_PX,    value=N    (Clay → SIZING_FIXED) */
		else if ( !Q_stricmp( token.string, "width" ) || !Q_stricmp( token.string, "height" ) ) {
			qboolean isWidth = !Q_stricmp( token.string, "width" );
			pc_token_t mode;
			if ( WiredPC_ReadTokenEval( handle, &mode ) ) {
				if ( !Q_stricmp( mode.string, "FIT" ) ) {
					/* Dispatch 5.12 S2: FIT is a no-op when the rect axis has
					 * already been declared with an explicit unit (VH/VW/PX/NORM
					 * from a preceding `rect` keyword). The pre-declared value
					 * + unit IS the size; FIT keyword preserved for authoring
					 * intent but the layout pass resolves rect normally. This
					 * supersedes the dispatch 5.7 S1 UNIT_AUTO + hint mechanism
					 * which mis-resolved VH-unit values as NORM-multiplier (e.g.
					 * card rect 0 0 1 15vh → dispatch 5.7 hint = 15 × parent.h
					 * = 11340 px, catastrophic). When axis unit is already
					 * something other than the default UNIT_NORM-with-value-0,
					 * trust the rect declaration. */
					if ( isWidth && item->wuiRect.w.value == 0.0f
					              && item->wuiRect.w.unit == UNIT_NORM ) {
						item->wuiRect.w.unit = UNIT_AUTO;
					} else if ( !isWidth && item->wuiRect.h.value == 0.0f
					                     && item->wuiRect.h.unit == UNIT_NORM ) {
						item->wuiRect.h.unit = UNIT_AUTO;
					}
				}
				else if ( !Q_stricmp( mode.string, "GROW" ) ) {
					if ( isWidth ) {
						item->flexChild.grow = 1.0f;
						item->wuiRect.w.unit = UNIT_AUTO; item->wuiRect.w.value = 0;
					} else {
						/* Clay derives GROW from flexChild.grow on the main axis;
						 * for non-grow-axis we fall back to AUTO. The clay emit
						 * doesn't distinguish width-grow from height-grow today
						 * — single grow scalar — but storing the AUTO unit on the
						 * other axis preserves the intent for the later emit. */
						item->flexChild.grow = 1.0f;
						item->wuiRect.h.unit = UNIT_AUTO; item->wuiRect.h.value = 0;
					}
				}
				else if ( !Q_stricmp( mode.string, "PERCENT" ) || !Q_stricmp( mode.string, "FIXED" ) ) {
					wuiValue_t v = WiredPC_ParseValue( handle );
					if ( v.unit == UNIT_NORM && !Q_stricmp( mode.string, "FIXED" ) ) {
						/* `FIXED 0.5` (no px suffix) — normalized fraction; same
						 * storage as PERCENT. `FIXED 8px` → UNIT_PX. */
					}
					if ( isWidth ) item->wuiRect.w = v;
					else           item->wuiRect.h = v;
				}
				else {
					/* Dispatch 5.15 S1: accept `width <value>[unit]` shorthand
					 * (no MODE keyword). Numeric tokens (digit / dot / sign)
					 * are unread and parsed as a value+optional-unit pair. */
					char c = mode.string[ 0 ];
					if ( ( c >= '0' && c <= '9' ) || c == '.' || c == '-' ) {
						WiredPC_UnreadToken( &mode );
						wuiValue_t v = WiredPC_ParseValue( handle );
						if ( isWidth ) item->wuiRect.w = v;
						else           item->wuiRect.h = v;
					} else {
						Com_Log( SEV_WARN, LOG_CH(ch_ui),
							"WiredUI: unknown %s mode '%s' (FIT/GROW/PERCENT/FIXED) — '%s'\n",
							isWidth ? "width" : "height", mode.string,
							s_wui_parse_file ? s_wui_parse_file : "?" );
					}
				}
			}
		}
		/* visual keywords. `background`/`border`/`radius` map
		 * onto existing backcolor/bordercolor/bordersize state so the Clay
		 * emit consumes them through the same pathway as legacy itemDefs.
		 * `radius` lands on a new field — parser-only until the Clay
		 * hookup lands. */
		else if ( !Q_stricmp( token.string, "background" ) ) {
			/* extension — three forms in priority
			 * order:
			 *   1. `background "layered" effects "<flags>"` → bgLayerFlags
			 *      set via WUI_BackgroundParseFlags (cl_wired_bg.c). The
			 *      compositor's emit branch invokes WUI_DrawBackgroundLayered.
			 *   2. `background <color|$ref>` → spec-canonical color path
			 *      (#hex / rgba / $ref / bare float-quad).
			 *   3. `background "<shader>"` → legacy shader-name string. */
			pc_token_t peek;
			if ( WiredPC_ReadTokenEval( handle, &peek ) ) {
				qboolean isHex   = ( peek.string[ 0 ] == '#' );
				qboolean isFunc  = ( !Q_stricmpn( peek.string, "rgba", 4 )
				                  || !Q_stricmpn( peek.string, "rgb",  3 ) );
				qboolean isNum   = ( ( peek.string[ 0 ] >= '0' && peek.string[ 0 ] <= '9' )
				                  || peek.string[ 0 ] == '.'
				                  || peek.string[ 0 ] == '-' );
				qboolean isLayered = !Q_stricmp( peek.string, "layered" );
				if ( isLayered ) {
					/* Expect optional `effects "<flags>"` payload. The
					 * full-bitmask shortcut `background "layered:full"`
					 * is recognised in the WUI_BackgroundParseFlags
					 * single-token form too, but the spec primary form
					 * is two tokens. */
					pc_token_t eff;
					int flags = WUI_BG_LAYER_ALL;  /* default if no effects spec */
					if ( WiredPC_ReadToken( handle, &eff ) ) {
						if ( !Q_stricmp( eff.string, "effects" ) ) {
							pc_token_t spec;
							if ( WiredPC_ReadTokenEval( handle, &spec ) ) {
								flags = WUI_BackgroundParseFlags( spec.string );
							}
						} else {
							WiredPC_UnreadToken( &eff );
						}
					}
					item->bgLayerFlags = flags;
				} else if ( isHex || isFunc ) {
					WiredPC_DecodeColorString( peek.string, item->backcolor );
				} else if ( isNum ) {
					item->backcolor[ 0 ] = peek.floatvalue;
					pc_token_t more;
					int        ci;
					for ( ci = 1; ci < 4; ci++ ) {
						if ( WiredPC_ReadTokenEval( handle, &more ) ) {
							item->backcolor[ ci ] = more.floatvalue;
						}
					}
				} else {
					/* Legacy shader-name path — string already in peek. */
					Q_strncpyz( item->background, peek.string, sizeof( item->background ) );
				}
			}
		}
		else if ( !Q_stricmp( token.string, "border" ) ) {
			/* Discriminate legacy `border <int>` (WINDOW_BORDER_* flag)
			 * from spec shorthand `border <w> <color>`. Read width via
			 * ParseValue (consumes `Npx`/`vw`/`vh` suffix when present),
			 * then peek the next token to decide:
			 *   - color-like (`#hex`/`rgba(...)`/`rgb(...)`/digit/dot/
			 *     dash/`$ref`) → shorthand, decode color
			 *   - anything else → legacy single-int, push back. */
			wuiValue_t bw = WiredPC_ParseValue( handle );
			pc_token_t arg2;
			if ( !WiredPC_ReadTokenEval( handle, &arg2 ) ) {
				item->border = (int) bw.value;
				continue;
			}
			qboolean arg2_color = ( arg2.string[ 0 ] == '#'
			                     || !Q_stricmpn( arg2.string, "rgba", 4 )
			                     || !Q_stricmpn( arg2.string, "rgb",  3 )
			                     || ( arg2.string[ 0 ] >= '0' && arg2.string[ 0 ] <= '9' )
			                     || arg2.string[ 0 ] == '.'
			                     || arg2.string[ 0 ] == '-' );
			if ( arg2_color ) {
				float pxW = ( bw.unit == UNIT_NORM )
					? bw.value * (float) cls.glconfig.vidHeight
					: bw.value;
				item->bordersize     = pxW;
				item->bordersize4[0] = pxW;
				item->bordersize4[1] = pxW;
				item->bordersize4[2] = pxW;
				item->bordersize4[3] = pxW;
				item->border         = 1;
				if ( arg2.string[ 0 ] == '#'
				  || !Q_stricmpn( arg2.string, "rgba", 4 )
				  || !Q_stricmpn( arg2.string, "rgb",  3 ) ) {
					WiredPC_DecodeColorString( arg2.string, item->bordercolor );
				} else {
					item->bordercolor[ 0 ] = arg2.floatvalue;
					pc_token_t more;
					int        ci;
					for ( ci = 1; ci < 4; ci++ ) {
						if ( WiredPC_ReadTokenEval( handle, &more ) ) {
							item->bordercolor[ ci ] = more.floatvalue;
						}
					}
				}
			} else {
				/* Legacy single-int — push back arg2, set border flag. */
				WiredPC_UnreadToken( &arg2 );
				item->border = (int) bw.value;
			}
		}
		else if ( !Q_stricmp( token.string, "borderX" ) ) {
			/* Per-axis border: borderX → left + right only. */
			wuiValue_t bw = WiredPC_ParseValue( handle );
			float      pxW = ( bw.unit == UNIT_NORM )
				? bw.value * (float) cls.glconfig.vidHeight
				: bw.value;
			item->bordersize     = pxW;
			item->bordersize4[0] = pxW;  /* left */
			item->bordersize4[1] = pxW;  /* right */
			item->border         = 1;
			WiredPC_ReadColorOrFloats( handle, item->bordercolor );
		}
		else if ( !Q_stricmp( token.string, "borderY" ) ) {
			/* borderY → top + bottom only. */
			wuiValue_t bw = WiredPC_ParseValue( handle );
			float      pxW = ( bw.unit == UNIT_NORM )
				? bw.value * (float) cls.glconfig.vidHeight
				: bw.value;
			item->bordersize     = pxW;
			item->bordersize4[2] = pxW;  /* top */
			item->bordersize4[3] = pxW;  /* bottom */
			item->border         = 1;
			WiredPC_ReadColorOrFloats( handle, item->bordercolor );
		}
		else if ( !Q_stricmp( token.string, "borderLeft" )
		       || !Q_stricmp( token.string, "borderRight" )
		       || !Q_stricmp( token.string, "borderTop" )
		       || !Q_stricmp( token.string, "borderBottom" ) ) {
			/* Dispatch 5.15 S2: per-side border keywords. */
			int side = !Q_stricmp( token.string, "borderLeft"  ) ? 0
			         : !Q_stricmp( token.string, "borderRight" ) ? 1
			         : !Q_stricmp( token.string, "borderTop"   ) ? 2 : 3;
			wuiValue_t bw  = WiredPC_ParseValue( handle );
			float      pxW = ( bw.unit == UNIT_NORM )
				? bw.value * (float) cls.glconfig.vidHeight
				: bw.value;
			item->bordersize4[ side ] = pxW;
			item->border              = 1;
			WiredPC_ReadColorOrFloats( handle, item->bordercolor );
		}
		else if ( !Q_stricmp( token.string, "radius" ) ) {
			wuiValue_t v = WiredPC_ParseValue( handle );
			float px = ( v.unit == UNIT_NORM )
				? v.value * (float) cls.glconfig.vidHeight
				: v.value;
			item->cornerRadius = px;
			item->cornerRadius4[0] = item->cornerRadius4[1] =
			item->cornerRadius4[2] = item->cornerRadius4[3] = px;
		}
		else if ( !Q_stricmp( token.string, "radius4" ) ) {
			int i;
			for ( i = 0; i < 4; i++ ) {
				wuiValue_t v = WiredPC_ParseValue( handle );
				float px = ( v.unit == UNIT_NORM )
					? v.value * (float) cls.glconfig.vidHeight
					: v.value;
				item->cornerRadius4[ i ] = px;
			}
			item->cornerRadius = item->cornerRadius4[ 0 ];
		}
		/* `iconText "<name>"` keyword resolves a
		 * canonical icon name to its PUA codepoint, UTF-8-encodes it into
		 * item->text, and binds the wui_icons MSDF atlas as the font
		 * face. The canonical name→codepoint table is mirrored from
		 * tools/msdf/build_wui_icons.py CANONICAL_ICONS + the modder-
		 * facing chart in docs/wui-authoring-guide.md §5.3 — adding a
		 * new icon requires touching all three places (the build script
		 * is authoritative since it must agree with the atlas contents).
		 * Unknown name → SEV_ERROR with the parsed file's path; the item
		 * stays text-less (parse continues for other keywords). */
		else if ( !Q_stricmp( token.string, "iconText" ) ) {
			static const struct {
				const char *name;
				int         codepoint;
			} icons[] = {
				{ "card_featured",     0xE0 },
				{ "card_recent",       0xE1 },
				{ "card_status",       0xE2 },
				{ "status_connection", 0xE3 },
				{ "nav_play",          0xE4 },
				{ "nav_multi",         0xE5 },
				{ "nav_settings",      0xE6 },
				{ "nav_quit",          0xE7 },
				{ "arrow_right",       0xE8 },
				/* v2 design primitive glyphs — mirrored from
				 * tools/msdf/build_wui_icons.py CANONICAL_ICONS. */
				{ "qw_sigil",          0xE9 },
				{ "rune_0",            0xEA },
				{ "rune_1",            0xEB },
				{ "rune_2",            0xEC },
				{ "rune_3",            0xED },
				{ "rune_4",            0xEE },
				{ "rune_5",            0xEF },
				{ "rune_6",            0xF0 },
				{ "rune_7",            0xF1 },
				{ "rune_8",            0xF2 },
				{ "rune_9",            0xF3 },
				{ "helmet",            0xF4 },
				{ "weapon_gnt",        0xF5 },
				{ "weapon_mg",         0xF6 },
				{ "weapon_sg",         0xF7 },
				{ "weapon_gl",         0xF8 },
				{ "weapon_rl",         0xF9 },
				{ "weapon_lg",         0xFA },
				{ "weapon_rg",         0xFB },
				{ "weapon_pg",         0xFC },
				{ "weapon_bfg",        0xFD },
				{ "diamond",           0xFE },
				{ NULL, 0 }
			};
			pc_token_t nameTok;
			if ( WiredPC_ReadTokenEval( handle, &nameTok ) ) {
				int cp = 0;
				for ( int i = 0; icons[i].name; i++ ) {
					if ( !Q_stricmp( nameTok.string, icons[i].name ) ) {
						cp = icons[i].codepoint;
						break;
					}
				}
				if ( cp == 0 ) {
					Com_Log( SEV_ERROR, LOG_CH(ch_ui),
						"WiredUI: iconText '%s' is not a canonical icon name "
						"— '%s'\n",
						nameTok.string,
						s_wui_parse_file ? s_wui_parse_file : "?" );
				} else {
					/* Single-byte codepoint — the wired engine's MSDF text
					 * path consumes one byte at a time
					 * (cl_wired_msdf.c:736) and has no UTF-8 decoder. The
					 * canonical chart uses 0xE0-0xE7 (single-byte Latin-1
					 * supplement) so the glyph lookup just hands the byte
					 * value to MSDF_FindGlyph as the unicode int. */
					item->text[0] = (char)( cp & 0xFF );
					item->text[1] = '\0';
					/* Force the icon font face regardless of any prior
					 * `font` keyword — iconText resolution implies the
					 * wui_icons atlas. Modders can change the atlas slot
					 * by overriding the wui_icons family registration in
					 * cl_wired_fonts.c. */
					Q_strncpyz( item->fontName, "wui_icons", sizeof( item->fontName ) );
				}
			}
		}
		/* `animation "<name>" speed <ms>` or
		 * `animation "<name>" duration <ms> curve <curveName> [loop]`.
		 * Stores the binding on the itemDef; the compositor lazily
		 * creates the wuiAnim on first emit and reads its eased value
		 * back per frame. */
		else if ( !Q_stricmp( token.string, "animation" ) ) {
			pc_token_t nameTok;
			if ( WiredPC_ReadTokenEval( handle, &nameTok ) ) {
				Q_strncpyz( item->animationName, nameTok.string, sizeof( item->animationName ) );
			}
			while ( 1 ) {
				pc_token_t kw;
				if ( !WiredPC_ReadToken( handle, &kw ) ) break;
				if ( !Q_stricmp( kw.string, "speed" ) || !Q_stricmp( kw.string, "duration" ) ) {
					pc_token_t v;
					if ( WiredPC_ReadTokenEval( handle, &v ) ) {
						/* `speed <s>` semantic: s is a fraction of one
						 * second (smaller = faster). Map to durationMs
						 * by inverting against a 12s base period.
						 * `duration <ms>` is a direct millisecond value. */
						if ( !Q_stricmp( kw.string, "speed" ) ) {
							item->animationSpeed     = v.intvalue;
							/* speed scalar: 1.0 = built-in default duration */
							item->animationDurationMs = (int)( 12000.0f * ( v.floatvalue > 0 ? v.floatvalue : 1.0f ) );
						} else {
							item->animationDurationMs = v.intvalue;
						}
					}
				}
				else if ( !Q_stricmp( kw.string, "curve" ) ) {
					pc_token_t v;
					if ( WiredPC_ReadTokenEval( handle, &v ) ) {
						Q_strncpyz( item->animationCurve, v.string, sizeof( item->animationCurve ) );
					}
				}
				else if ( !Q_stricmp( kw.string, "loop" ) ) {
					item->animationLoop = qtrue;
				}
				else {
					/* Unknown modifier — push back so the outer keyword
					 * loop picks it up as its own item-level keyword. */
					WiredPC_UnreadToken( &kw );
					break;
				}
			}
		}
		/* `active <cvar> <value>` itemDef-level
		 * binding. Per-frame, the compositor compares the cvar's string
		 * against `value`; on match, .active-suffixed properties win. */
		else if ( !Q_stricmp( token.string, "active" ) ) {
			pc_token_t cvarTok, valTok;
			if ( WiredPC_ReadTokenEval( handle, &cvarTok ) ) {
				Q_strncpyz( item->activeCvar, cvarTok.string, sizeof( item->activeCvar ) );
			}
			if ( WiredPC_ReadTokenEval( handle, &valTok ) ) {
				Q_strncpyz( item->activeValue, valTok.string, sizeof( item->activeValue ) );
			}
		}
		/* `.active` dot-suffix per-property override variants.
		 * Each writes the same payload as the base keyword but into the
		 * *Active mirror field; the per-property hasActive* flag is set
		 * so the compositor knows to compare and switch. */
		else if ( !Q_stricmp( token.string, "forecolor.active" ) ) {
			/* Same disambiguation as background.active: accept `#hex`,
			 * `rgba(...)`, `$ref`, or bare float-quad. Legacy
			 * `forecolor 4-float` would have used WiredPC_Color, but the
			 * .active variant must follow the
			 * spec syntax. */
			pc_token_t peek;
			if ( WiredPC_ReadTokenEval( handle, &peek ) ) {
				qboolean isHex   = ( peek.string[ 0 ] == '#' );
				qboolean isFunc  = ( !Q_stricmpn( peek.string, "rgba", 4 )
				                  || !Q_stricmpn( peek.string, "rgb",  3 ) );
				qboolean isNum   = ( ( peek.string[ 0 ] >= '0' && peek.string[ 0 ] <= '9' )
				                  || peek.string[ 0 ] == '.'
				                  || peek.string[ 0 ] == '-' );
				if ( isHex || isFunc ) {
					WiredPC_DecodeColorString( peek.string, item->forecolorActive );
				} else if ( isNum ) {
					item->forecolorActive[ 0 ] = peek.floatvalue;
					pc_token_t more;
					int        ci;
					for ( ci = 1; ci < 4; ci++ ) {
						if ( WiredPC_ReadTokenEval( handle, &more ) ) {
							item->forecolorActive[ ci ] = more.floatvalue;
						}
					}
				}
			}
			item->hasActiveForecolor = qtrue;
		}
		else if ( !Q_stricmp( token.string, "backcolor.active" )
		       || !Q_stricmp( token.string, "background.active" ) ) {
			pc_token_t peek;
			if ( WiredPC_ReadTokenEval( handle, &peek ) ) {
				qboolean isHex   = ( peek.string[ 0 ] == '#' );
				qboolean isFunc  = ( !Q_stricmpn( peek.string, "rgba", 4 )
				                  || !Q_stricmpn( peek.string, "rgb",  3 ) );
				qboolean isNum   = ( ( peek.string[ 0 ] >= '0' && peek.string[ 0 ] <= '9' )
				                  || peek.string[ 0 ] == '.'
				                  || peek.string[ 0 ] == '-' );
				if ( isHex || isFunc ) {
					WiredPC_DecodeColorString( peek.string, item->backcolorActive );
				} else if ( isNum ) {
					item->backcolorActive[ 0 ] = peek.floatvalue;
					pc_token_t more;
					int        ci;
					for ( ci = 1; ci < 4; ci++ ) {
						if ( WiredPC_ReadTokenEval( handle, &more ) ) {
							item->backcolorActive[ ci ] = more.floatvalue;
						}
					}
				}
				/* String "shader name" on backcolor.active is rejected —
				 * the active variant is colour-only; modder must use the
				 * base `background` keyword for shader paths. */
			}
			item->hasActiveBackcolor = qtrue;
		}
		else if ( !Q_stricmp( token.string, "bordercolor.active" ) ) {
			pc_token_t peek;
			if ( WiredPC_ReadTokenEval( handle, &peek ) ) {
				qboolean isHex   = ( peek.string[ 0 ] == '#' );
				qboolean isFunc  = ( !Q_stricmpn( peek.string, "rgba", 4 )
				                  || !Q_stricmpn( peek.string, "rgb",  3 ) );
				qboolean isNum   = ( ( peek.string[ 0 ] >= '0' && peek.string[ 0 ] <= '9' )
				                  || peek.string[ 0 ] == '.'
				                  || peek.string[ 0 ] == '-' );
				if ( isHex || isFunc ) {
					WiredPC_DecodeColorString( peek.string, item->bordercolorActive );
				} else if ( isNum ) {
					item->bordercolorActive[ 0 ] = peek.floatvalue;
					pc_token_t more;
					int        ci;
					for ( ci = 1; ci < 4; ci++ ) {
						if ( WiredPC_ReadTokenEval( handle, &more ) ) {
							item->bordercolorActive[ ci ] = more.floatvalue;
						}
					}
				}
			}
			item->hasActiveBordercolor = qtrue;
		}
		else if ( !Q_stricmp( token.string, "fontsize.active" )
		       || !Q_stricmp( token.string, "fontPointSize.active" ) ) {
			pc_token_t v;
			if ( WiredPC_ReadTokenEval( handle, &v ) ) {
				item->fontPointSizeActive = v.floatvalue;
				item->hasActiveFontSize   = qtrue;
			}
		}
		else if ( !Q_stricmp( token.string, "radius.active" ) ) {
			wuiValue_t v = WiredPC_ParseValue( handle );
			item->cornerRadiusActive = ( v.unit == UNIT_NORM )
				? v.value * (float) cls.glconfig.vidHeight
				: v.value;
			item->hasActiveCornerRadius = qtrue;
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
					if ( WiredUI_ParseItemProperties( handle, menu, child ) ) {
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
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: unknown item keyword '%s'\n", token.string );
			if ( WiredPC_ReadToken( handle, &token ) ) {
				if ( !Q_stricmp( token.string, "{" ) ) {
					WiredPC_SkipBracedBlock( handle );
				}
				/* else: consumed one token (the value) -- good enough */
			}
		}
	}

	/* post-parse sigil rewrite for hudElement.
	 * WP_S table writes item->hudElement at parse time (line 618 above);
	 * mirror that name into the unified registry slot with the "hud:"
	 * prefix unless an explicit `custom "..."` already populated it. */
	if ( item->hudElement[ 0 ] && item->customDrawName[ 0 ] == '\0' ) {
		Com_sprintf( item->customDrawName, sizeof( item->customDrawName ),
			"hud:%s", item->hudElement );
	}

	/* strict registry validation. hudElement names
	 * must resolve against the unified custom-draw registry; in production
	 * mode (wui_strict_validation 1) a miss fails the parse so modders
	 * find typos early. Dev mode (0) downgrades to SEV_WARN + skip element
	 * so .wui files can author against not-yet-registered elements. */
	if ( item->hudElement[ 0 ] && item->customDrawName[ 0 ] == 'h'
	     && item->customDrawName[ 1 ] == 'u' && item->customDrawName[ 2 ] == 'd'
	     && item->customDrawName[ 3 ] == ':' )
	{
		if ( !WiredUI_FindCustomDraw( item->customDrawName, NULL ) ) {
			static cvar_t *strict = NULL;
			if ( !strict ) {
				static const cvarDesc_t d = CVAR_BOOL( "wui_strict_validation", "1",
					CVAR_ARCHIVE | CVAR_LATCH,
					"Reject .wui files whose hudElement names aren't registered "
					"in the unified custom-draw registry. 0: SEV_WARN + skip "
					"(modder dev mode). 1: SEV_ERROR + fail parse (default)." );
				strict = Cvar_Register( &d );
			}
			if ( strict && strict->integer ) {
				Com_Log( SEV_ERROR, LOG_CH(ch_ui),
					"WiredUI: unknown hudElement '%s' on item '%s' (menu '%s') "
					"— no registry binding for '%s'.\n",
					item->hudElement, item->name[ 0 ] ? item->name : "<anon>",
					menu ? menu->name : "<unknown>", item->customDrawName );
				return qfalse;
			}
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WiredUI: unknown hudElement '%s' on item '%s' "
				"(wui_strict_validation 0 — skipping element).\n",
				item->hudElement, item->name[ 0 ] ? item->name : "<anon>" );
			item->hudElement[ 0 ]      = '\0';
			item->customDrawName[ 0 ]  = '\0';
		}
	}

#ifdef _DEBUG
	Com_Log( SEV_TRACE, LOG_CH(ch_ui),
		"WUI_TRACE PARSE item='%s' flex=%d pos=%d grow=%.2f "
		"rect=(%.3f:%d,%.3f:%d,%.3f:%d,%.3f:%d) "
		"offT=(%d,%.3f:%d) offL=(%d,%.3f:%d) offR=(%d,%.3f:%d) offB=(%d,%.3f:%d)\n",
		item->name[ 0 ] ? item->name : "<anon>", (int) item->isFlexContainer,
		(int) item->position, item->flexChild.grow,
		item->wuiRect.x.value, item->wuiRect.x.unit,
		item->wuiRect.y.value, item->wuiRect.y.unit,
		item->wuiRect.w.value, item->wuiRect.w.unit,
		item->wuiRect.h.value, item->wuiRect.h.unit,
		(int) item->wuiOffset.hasTop,    item->wuiOffset.top.value,    item->wuiOffset.top.unit,
		(int) item->wuiOffset.hasLeft,   item->wuiOffset.left.value,   item->wuiOffset.left.unit,
		(int) item->wuiOffset.hasRight,  item->wuiOffset.right.value,  item->wuiOffset.right.unit,
		(int) item->wuiOffset.hasBottom, item->wuiOffset.bottom.value, item->wuiOffset.bottom.unit );
#endif

	return qtrue;
}

// ── layer keyword helper ────────────────────────────────────────
//
// Case-insensitive string → wuiLayer_t mapping. Returns WUI_LAYER_COUNT
// when the name doesn't match any known layer (caller decides fall-back).
// `menu_stack` retained as a legacy alias of `menu` for one-cycle
// transition; a later change renames the 39 .wui files to use `menu`.
wuiLayer_t WiredUI_ParseLayerName( const char *str ) {
	if ( !str || !*str ) return WUI_LAYER_COUNT;
	if ( !Q_stricmp( str, "bg_dark"        ) ) return WUI_LAYER_BG_DARK;
	if ( !Q_stricmp( str, "bg_animated"    ) ) return WUI_LAYER_BG_ANIMATED;
	if ( !Q_stricmp( str, "bg_attract"     ) ) return WUI_LAYER_BG_ATTRACT;
	if ( !Q_stricmp( str, "loading"        ) ) return WUI_LAYER_LOADING;
	if ( !Q_stricmp( str, "world_viewport" ) ) return WUI_LAYER_WORLD_VIEWPORT;
	if ( !Q_stricmp( str, "hud"            ) ) return WUI_LAYER_HUD;
	if ( !Q_stricmp( str, "menu"           ) ) return WUI_LAYER_MENU;
	if ( !Q_stricmp( str, "menu_stack"     ) ) return WUI_LAYER_MENU;
	if ( !Q_stricmp( str, "popup"          ) ) return WUI_LAYER_POPUP;
	if ( !Q_stricmp( str, "debug_overlay"  ) ) return WUI_LAYER_DEBUG_OVERLAY;
	if ( !Q_stricmp( str, "overlay"        ) ) return WUI_LAYER_OVERLAY;
	if ( !Q_stricmp( str, "console"        ) ) return WUI_LAYER_CONSOLE;
	return WUI_LAYER_COUNT;
}

// ── menu parser ───────────────────────────────────────────────────────

static qboolean WiredUI_ParseMenu( int handle ) {
	pc_token_t       token;
	const char      *str;
	const char      *prevMenu;
#ifdef _DEBUG
	const int        prePool = wui_menuPoolUsed;
#endif

	wiredMenuDef_t  *menu = (wiredMenuDef_t *)WiredUI_Alloc( sizeof( wiredMenuDef_t ) );
	if ( !menu ) {
		return qfalse;
	}

	/* Path-identity: stamp the relative source path+ext this menu is parsed
	 * from (s_wui_parse_file, set by WiredUI_LoadMenuFile). This is the menu's
	 * identity for state→named-UI binding (WiredUI_FindMenuByPath), distinct
	 * from the short `name` keyword. Empty when parsed outside a file load. */
	if ( s_wui_parse_file ) {
		Q_strncpyz( menu->sourcePath, s_wui_parse_file, sizeof( menu->sourcePath ) );
	}

	/* source-attribution: point at the menu's name buffer.
	 * Empty until `name "..."` keyword fires inside the parse loop;
	 * once written, attribution surfaces automatically. */
	prevMenu         = s_wui_parse_menu;
	s_wui_parse_menu = menu->name;

	// defaults
	menu->visible = qtrue;
	menu->anchor = ANCHOR_NONE;
	menu->cinematicHandle = -1;
	/* Default focus-gradient tint = the live theme accent. A .wui menu
	 * overrides per-theme with `focuscolor $<token>`.
	 *
	 * FIX 2026-08-17: this read the token named "primary_cyan", and its comment
	 * claimed the default "follows ui_palette_accent". It did not. The v2 accent
	 * overlays (ui/themes/<accent>/_tokens.wui) only override `accent` /
	 * `accentDim` / `accentSoft` / `accentWash`; `primary_cyan` is a v1 token no
	 * overlay touches, so it stayed #00b4d8 under every accent and the focus
	 * wash rendered cyan even with ui_palette_accent amber (observed on the
	 * settings category rail). Read `accent` — the token the overlay chain
	 * actually rewrites. The fallback literal is the amber boot default
	 * (_tokens.wui `token accent "#f4a03a"`), so a missing token degrades to the
	 * shipped default instead of back to v1 cyan. */
	menu->focuscolor[0] = 0.957f;
	menu->focuscolor[1] = 0.627f;
	menu->focuscolor[2] = 0.227f;
	menu->focuscolor[3] = 1.0f;
	{
		const char *focusTok = WiredToken_Find( "accent" );
		if ( focusTok ) WiredPC_DecodeColorString( focusTok, menu->focuscolor );
	}
	/* default layer is MENU — backward-compatible with
	 * older panels that don't author the new keyword.
	 *
	 * A planned parse-end check will SEV_ERROR on missing layer
	 * keyword once all 39 .wui files explicitly declare their layer. For
	 * now, the parser logs SEV_WARN per-menu (see end of WiredUI_ParseMenu)
	 * but falls back to this default so existing .wmenu files keep loading. */
	menu->layer = WUI_LAYER_MENU;

	// expect opening brace
	if ( !WiredPC_Expect( handle, "{" ) ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: expected '{' for menuDef\n" );
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
		else if ( !Q_stricmp( token.string, "composite" ) ) {
			WiredPC_ParseCompositeInto( handle, &menu->compositeMode );
		}
		else if ( !Q_stricmp( token.string, "backdrop" ) ) {
			/* What this menu wants behind it: invisible | animated | dim |
			 * none. A preset, never a layer name — the preset->layer mapping
			 * lives in one table (policy/wui_bg_preset.c), so inserting a
			 * layer later changes behaviour without touching any .wui.
			 *
			 * Spelled `backdrop`, not `background`: menuDef already has a
			 * `background` string field (s_menuProps) that the field table
			 * claims before this branch is reached, and itemDefs use
			 * `background "layered" effects` for authored chrome. Rather than
			 * overload a name three ways, the layer request gets its own. */
			if ( WiredPC_String( handle, &str ) ) {
				wuiBgPreset_t preset;
				if ( WUI_BgPresetParse( str, &preset ) ) {
					menu->bgPreset = preset;
				} else {
					Com_Log( SEV_WARN, LOG_CH(ch_ui),
						"WiredUI: unknown background preset '%s' on menu '%s'"
						" — defaulting to animated\n", str, menu->name );
					menu->bgPreset = WUI_BG_PRESET_ANIMATED;
				}
			}
		}
		else if ( !Q_stricmp( token.string, "layer" ) ) {
			/* panel layer assignment. String → wuiLayer_t.
			 * Mapping moved to WiredUI_ParseLayerName +
			 * `menu_stack` keyword renamed to `menu` + `world_viewport` /
			 * `console` keywords added. The legacy `menu_stack` string is
			 * still accepted with SEV_WARN for the transition cycle;
			 * a later change tightens the 39 .wui files to use `menu`. */
			if ( WiredPC_String( handle, &str ) ) {
				wuiLayer_t resolved = WiredUI_ParseLayerName( str );
				if ( resolved == WUI_LAYER_COUNT ) {
					Com_Log( SEV_WARN, LOG_CH(ch_ui),
						"WiredUI: unknown layer '%s' on menu '%s' — defaulting to menu\n",
						str, menu->name );
					resolved = WUI_LAYER_MENU;
				}
				menu->layer       = resolved;
				menu->layerSeen   = qtrue;
			}
		}
		else if ( !Q_stricmp( token.string, "font" ) ) {
			if ( WiredPC_String( handle, &str ) )
				Q_strncpyz( menu->font, str, sizeof( menu->font ) );
			// optional point size follows
			if ( WiredPC_ReadToken( handle, &token ) ) {
				if ( token.string[0] >= '0' && token.string[0] <= '9' ) {
					// consumed point size
				} else {
					// not a number — push it back so the next keyword isn't
					// swallowed (WiredPC_ReadToken pops the pushback stack first;
					// the old "botlib doesn't support pushback" comment was stale).
					WiredPC_UnreadToken( &token );
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
		else if ( !Q_stricmp( token.string, "hudOverlay" ) ) {
			/* retired keyword. The compositor's HUD
			 * layer is selected via `layer "hud"` instead. Consume the
			 * legacy integer argument so the parser stays in sync. */
			int dummy; WiredPC_Int( handle, &dummy );
			Com_Log( SEV_ERROR, LOG_CH(ch_ui),
				"WiredUI: `hudOverlay` keyword on menu '%s' is retired; "
				"use `layer \"hud\"` instead.\n",
				menu->name );
		}
		// ── flex container keywords ──────────────────────────────────
		else if ( WiredPC_ParseFlexProps( handle, token.string, &menu->flexContainer, &menu->isFlexContainer ) ) {
			/* consumed */
		}
		else if ( !Q_stricmp( token.string, "itemDef" ) ) {
			if ( !WiredUI_ParseItem( handle, menu ) ) {
				COM_WARN( LOG_CH(ch_ui), "WiredUI: failed to parse itemDef in menu '%s'\n", menu->name );
			}
		}
		else {
			// unknown keyword — smart skip to avoid poisoning subsequent parsing
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: unknown menu keyword '%s'\n", token.string );
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
		float maxBottom = 0;
		for ( int ci = 0; ci < menu->itemCount; ci++ ) {
			float bottom = menu->items[ci]->rect.y + menu->items[ci]->rect.h;
			if ( bottom > maxBottom ) maxBottom = bottom;
		}
		menu->contentHeight = maxBottom;
		menu->scrollOffset = 0;
		menu->scrollVelocity = 0;
		menu->scrollBarFadeTime = 0;
	}

	/* classify Path A vs Path B by scanning the parsed
	 * tree for polyfill-target features. All shipping panels in base/
	 * + q3now/ classify as Path A today (the inventory
	 * confirms zero usage of wrap/shrink>1/space-between); the
	 * flag is set up so a future author who reaches for those features
	 * gets the CLAY_FLOATING + resolvedRect fallback emit path while
	 * the polyfill is in flight. */
	{
		int ci;
		menu->pathBKind = qfalse;

		/* Panel-level flex container */
		if ( menu->isFlexContainer ) {
			if ( menu->flexContainer.wrap ) menu->pathBKind = qtrue;
			if ( menu->flexContainer.justify == WUI_JUSTIFY_SPACE_BETWEEN ) menu->pathBKind = qtrue;
		}

		/* Per-item scan */
		for ( ci = 0; ci < menu->itemCount && !menu->pathBKind; ci++ ) {
			const wiredItemDef_t *it = menu->items[ ci ];
			if ( !it ) continue;
			if ( it->isFlexContainer ) {
				if ( it->flexContainer.wrap ) { menu->pathBKind = qtrue; break; }
				if ( it->flexContainer.justify == WUI_JUSTIFY_SPACE_BETWEEN ) { menu->pathBKind = qtrue; break; }
			}
			/* shrink > 1.0 is "explicitly above default"; the
			 * polyfill applies when content overflows AND children have
			 * non-default shrink weights. The default 1.0 is the implicit
			 * "shrink uniformly if needed" — Clay handles that natively
			 * via FIT sizing, so it stays in Path A. */
			if ( it->flexChild.shrink > 1.0f ) { menu->pathBKind = qtrue; break; }
		}
	}

	/* layer keyword is mandatory. All 39 .wui files declare their
	 * layer explicitly; missing layer is a parse failure and the
	 * menu is NOT registered. */
	if ( !menu->layerSeen ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_ui),
			"WiredUI: menu '%s' must declare a `layer` keyword (one of "
			"bg_attract / loading / world_viewport / hud / menu / popup / "
			"debug_overlay / overlay / console).\n",
			menu->name );
		s_wui_parse_menu = prevMenu;
		return qfalse;
	}

	if ( wui_menuCount < WIRED_MAX_MENUS ) {
		wui_menus[wui_menuCount++] = menu;
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI: loaded menu '%s' (%d items, path %s)\n",
			menu->name, menu->itemCount, menu->pathBKind ? "B" : "A" );
#ifdef _DEBUG
		{
			const int delta     = wui_menuPoolUsed - prePool;
			const int perItem32 = ( menu->itemCount > 0 )
			                       ? ( delta / menu->itemCount )
			                       : delta;
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
				"WiredUI/pool: menu '%s' delta=%d B (%d items, ~%d B/item, head=%d/%d)\n",
				menu->name, delta, menu->itemCount, perItem32,
				wui_menuPoolUsed, WIRED_MENU_POOL_SIZE );
		}
#endif
	}

	s_wui_parse_menu = prevMenu;
	return qtrue;
}

// ── file loader ───────────────────────────────────────────────────────

qboolean WiredUI_LoadMenuFile( const char *filename ) {
	pc_token_t  token;
	const char *ext;

	/* .wui is the unified extension; .wmenu / .whud are retired. */
	ext = filename ? strrchr( filename, '.' ) : NULL;
	if ( ext && ( !Q_stricmp( ext, ".wmenu" ) || !Q_stricmp( ext, ".whud" ) ) ) {
		Com_Log( SEV_ERROR, LOG_CH(ch_ui),
			"WiredUI: '%s' extension is retired (file '%s'); rename to .wui.\n",
			ext, filename );
		return qfalse;
	}

	/* implicit `_tokens.wui` mandatory include.
	 * Every `.wui` parse pulls in the base token table once per WiredUI
	 * init cycle, so design-token references (`forecolor $primary_cyan`,
	 * `padding $spacing_md`, etc.) resolve to the canonical palette /
	 * spacing / typography literals declared in `ui/_tokens.wui`. The
	 * function below short-circuits on second-and-after calls. */
	WiredUI_LoadTokensIfNeeded( filename );

	int handle = WiredPC_LoadSource( filename );
	if ( !handle ) {
		COM_WARN( LOG_CH(ch_ui), "WiredUI: could not load '%s'\n", filename );
		return qfalse;
	}

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: loading native format '%s'\n", filename );

	/* source-attribution: set parse context for the duration
	 * of this load. CL_ForwardCommandToServer reads these to annotate
	 * leaked-keyword "Unknown command" log lines. */
	s_wui_parse_file   = filename;
	s_wui_parse_handle = handle;
	s_wui_parse_menu   = NULL;
	s_wui_parse_item   = NULL;

	while ( 1 ) {
		if ( !WiredPC_ReadToken( handle, &token ) ) {
			break;
		}

		if ( !Q_stricmp( token.string, "menuDef" ) ) {
			WiredUI_ParseMenu( handle );
		}
		else if ( !Q_stricmp( token.string, "token" ) ) {
			/* top-level `token <name> <value>`
			 * declaration. Name is a bare identifier; value is a single
			 * literal — color hex `#rrggbb`, `rgba(r,g,b,a)`, float,
			 * or string with `px`/`vw`/`vh` suffix recognised at use
			 * time. The raw text is stored as-is; substitution happens
			 * via WiredPC_ResolveTokenRef when a `$name` reference is
			 * read elsewhere. */
			pc_token_t  nameTok;
			pc_token_t  valTok;
			char        vbuf[ WIRED_TOKEN_VALUE_LEN ];
			if ( !WiredPC_ReadToken( handle, &nameTok ) ) break;
			if ( !WiredPC_ReadToken( handle, &valTok  ) ) break;
			/* `rgba(...)` arrives as multiple tokens from botlib's PC
			 * splitter. Two paths because the lexer may or may not glue
			 * the identifier to the `(`:
			 *   (a) one token already contains `(` with no closing `)`
			 *       — accumulate further tokens until balanced
			 *   (b) identifier alone (`rgba`/`rgb`/`hsla`/`hsl`) — peek
			 *       next token; if it's `(`, switch to depth-counting
			 *       accumulation. */
			Q_strncpyz( vbuf, valTok.string, sizeof( vbuf ) );
			{
				qstring_t   vbuf_qs    = QS_WrapExisting( vbuf, sizeof( vbuf ) );
				qboolean    isColorFn  = ( !Q_stricmp( valTok.string, "rgba" )
				                        || !Q_stricmp( valTok.string, "rgb"  )
				                        || !Q_stricmp( valTok.string, "hsla" )
				                        || !Q_stricmp( valTok.string, "hsl"  ) );

				if ( strchr( valTok.string, '(' ) && !strchr( valTok.string, ')' ) ) {
					pc_token_t  more;
					int depth = 1;
					while ( depth > 0 && WiredPC_ReadToken( handle, &more ) ) {
						QS_Append( &vbuf_qs, more.string );
						if ( strchr( more.string, '(' ) ) depth++;
						if ( strchr( more.string, ')' ) ) depth--;
					}
				}
				else if ( isColorFn ) {
					pc_token_t  more;
					int depth = 0;
					qboolean    sawOpen = qfalse;
					while ( WiredPC_ReadToken( handle, &more ) ) {
						QS_Append( &vbuf_qs, more.string );
						if ( strchr( more.string, '(' ) ) { depth++; sawOpen = qtrue; }
						if ( strchr( more.string, ')' ) ) depth--;
						if ( sawOpen && depth <= 0 ) break;
					}
				}
			}
			/* Multi-token unit suffix (e.g. `1` `px`): peek + concat
			 * if the next token is `px`/`vw`/`vh` AND the value is
			 * unmistakably numeric. */
			if ( vbuf[0] >= '0' && vbuf[0] <= '9'
			  && !strchr( vbuf, '(' ) ) {
				pc_token_t  unitPeek;
				if ( WiredPC_ReadToken( handle, &unitPeek ) ) {
					if ( !Q_stricmp( unitPeek.string, "px" )
					  || !Q_stricmp( unitPeek.string, "vw" )
					  || !Q_stricmp( unitPeek.string, "vh" ) )
					{
						qstring_t vbuf_qs2 = QS_WrapExisting( vbuf, sizeof( vbuf ) );
						QS_Append( &vbuf_qs2, unitPeek.string );
					} else {
						WiredPC_UnreadToken( &unitPeek );
					}
				}
			}
			/* mod-override gate. First _tokens.wui
			 * load registers the canonical set (allowNew=qtrue). Any
			 * subsequent load (mod overlay via `\wui_load_menu_loose
			 * <mod>/ui/_tokens.wui`, or a future engine-driven mod
			 * second-pass) sees `s_wuiTokensBaseLoaded` already true and
			 * runs with allowNew=qfalse — spec §3 "mod yeni token YASAK".
			 * The detection is load-order based, not FS-source based:
			 * the q3-derived FS layer's mod-precedence behaviour gives us
			 * only ONE _tokens.wui per layered lookup, so distinguishing
			 * "this file came from mod" vs "this file came from base"
			 * via FS introspection would require deeper plumbing. The
			 * load-order proxy is sufficient because: (a) the FIRST load
			 * is always the canonical set in practice, and (b) any
			 * SECOND load is by definition an explicit overlay. */
			WiredToken_Set( nameTok.string, vbuf, !s_wuiTokensBaseLoaded );
		}
		else if ( !Q_stricmp( token.string, "assetGlobalDef" ) ) {
			wiredAssetGlobals_t *ag = WiredUI_GetAssetGlobals();
			if ( !WiredPC_Expect( handle, "{" ) ) {
				COM_WARN( LOG_CH(ch_ui), "WiredUI: expected '{' for assetGlobalDef\n" );
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
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: parsed assetGlobalDef (cursor=%s)\n", ag->cursor );
		}
		else if ( !Q_stricmp( token.string, "{" ) || !Q_stricmp( token.string, "}" ) ) {
			// top-level braces — Q3:TA/QL files wrap content in { }. Just ignore.
		}
		else {
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "WiredUI: unexpected token '%s' in '%s'\n", token.string, filename );
		}
	}

	WiredPC_FreeSource( handle );

	/* source-attribution: clear parse context. CL_ForwardCommand
	 * now stops appending parser-source info to "Unknown command" lines. */
	s_wui_parse_file   = NULL;
	s_wui_parse_handle = 0;
	s_wui_parse_menu   = NULL;
	s_wui_parse_item   = NULL;

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
	for ( int i = 0; i < wui_menuCount; i++ ) {
		if ( !Q_stricmp( wui_menus[i]->name, name ) ) {
			return wui_menus[i];
		}
	}
	return NULL;
}

/* Path-identity lookup: resolve a menu by its relative source path+ext
 * (e.g. "ui/loading_screen.wui"), the value stamped on menu->sourcePath at
 * parse time. Companion to the name-keyed WiredUI_FindMenu (which the
 * MENU/POPUP layers use) — added alongside, not a repurpose. Used by the
 * LOADING layer's state→named-UI emit. NULL if no loaded menu matches. */
wiredMenuDef_t *WiredUI_FindMenuByPath( const char *path ) {
	if ( !path || !*path ) {
		return NULL;
	}
	for ( int i = 0; i < wui_menuCount; i++ ) {
		if ( !Q_stricmp( wui_menus[i]->sourcePath, path ) ) {
			return wui_menus[i];
		}
	}
	return NULL;
}

/* release Lua chunk refs before the menu pool's bulk memset.
 * Dispatches through the menu's VM (System or User) so vm "user" panels
 * release into the User VM registry that compiled them — symmetric with
 * the parser refactor that routes parse-time compiles via the
 * dispatcher. */
static void WiredUI_PurgeMenuChunks( void ) {
	int m, i;

	for ( m = 0; m < wui_menuCount; m++ ) {
		wiredMenuDef_t *menu = wui_menus[ m ];
		if ( !menu ) continue;

		for ( i = 0; i < menu->itemCount; i++ ) {
			wiredItemDef_t *item = menu->items[ i ];
			if ( !item ) continue;

			if ( item->luaBindChunk != WIRED_CHUNK_NOREF ) {
				WiredUI_CompositorReleaseChunkForMenu( menu, item->luaBindChunk );
				item->luaBindChunk = WIRED_CHUNK_NOREF;
			}
			if ( item->luaVisibleChunk != WIRED_CHUNK_NOREF ) {
				WiredUI_CompositorReleaseChunkForMenu( menu, item->luaVisibleChunk );
				item->luaVisibleChunk = WIRED_CHUNK_NOREF;
			}
			if ( item->repeatBlock && item->repeatBlock->sourceLuaChunk != WIRED_CHUNK_NOREF ) {
				WiredUI_CompositorReleaseChunkForMenu( menu, item->repeatBlock->sourceLuaChunk );
				item->repeatBlock->sourceLuaChunk = WIRED_CHUNK_NOREF;
			}
			if ( item->ifBlock && item->ifBlock->testLuaChunk != WIRED_CHUNK_NOREF ) {
				WiredUI_CompositorReleaseChunkForMenu( menu, item->ifBlock->testLuaChunk );
				item->ifBlock->testLuaChunk = WIRED_CHUNK_NOREF;
			}
		}
	}
}

void WiredUI_ClearMenus( void ) {
	/* Stop every active tween BEFORE the pool is wiped/reused. Loop anims
	 * (WUI_ANIM_FLAG_LOOP: scroll-x / pulse / blink) never self-terminate, and
	 * their targetRef points at an item's animOffset / animAlphaMul field inside
	 * wui_menuPool. Reparsing lays different items at the same byte offsets, so a
	 * surviving slot would keep writing eased values into an unrelated widget
	 * (phantom scroll/flicker) and leak slots until the pool fills. Stopping here,
	 * the single choke point every reload passes through, clears the dangling
	 * refs. Declared in cl_wired_anim.h. */
	WUI_AnimStopAll();
	WiredUI_PurgeMenuChunks();
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
	// allocate backup on first use (WIRED_MENU_POOL_SIZE-scaled —
	// way too large for stack regardless of capacity tier)
	if ( !wired_backup ) {
		wired_backup = Z_Malloc( sizeof( wiredMenuBackup_t ) );
	}

	// phase 1: save current state
	memcpy( wired_backup->pool, wui_menuPool, wui_menuPoolUsed );
	wired_backup->poolUsed = wui_menuPoolUsed;
	// sizeof(pointer) is the point: this copies the menu POINTER table, not
	// the menu definitions the pointers reference.
	// NOLINTNEXTLINE(bugprone-sizeof-expression)
	memcpy( wired_backup->menus, wui_menus, sizeof( wui_menus[0] ) * wui_menuCount );
	wired_backup->menuCount = wui_menuCount;
	wired_backup->assetGlobals = *WiredUI_GetAssetGlobals();

	// phase 2: clear and reparse from menus.lua
	WiredUI_ResetAssetGlobalsDefaults();
	WiredUI_ClearMenus();
	WiredUI_LoadMenusFromLua();
	// Re-run the explicit post-manifest loads (loading_screen + overlay) that
	// live outside menus.lua — otherwise this reload permanently drops them
	// from the registry, so the LOADING by-path lookup (13-30) returns NULL
	// and the loading screen / cursor-tooltip go blank. ClearMenus above
	// already wiped the prior copies, so this is a clean rebuild, not a
	// duplicate append. Counted in the `ok` success test below.
	WiredUI_LoadExplicitMenus();
	qboolean ok = ( wui_menuCount > 0 );

	if ( !ok ) {
		// parse failed — restore old menus
		COM_WARN( LOG_CH(ch_ui), "Menu reload failed — keeping old menus.\n" );
		memcpy( wui_menuPool, wired_backup->pool, wired_backup->poolUsed );
		wui_menuPoolUsed = wired_backup->poolUsed;
		// NOLINTNEXTLINE(bugprone-sizeof-expression) — pointer-table copy, same as the backup above
		memcpy( wui_menus, wired_backup->menus, sizeof( wui_menus[0] ) * wired_backup->menuCount );
		wui_menuCount = wired_backup->menuCount;
		*WiredUI_GetAssetGlobals() = wired_backup->assetGlobals;

		// the backup pool was snapshotted BEFORE phase-2 teardown, so
		// every restored itemDef still caches resources that the teardown has
		// since reclaimed:
		//   • customDrawContext → memory inside s_hudArena, which the caller's
		//     WiredHud_DestroyAllElements reset (Arena_Reset) before we ran.
		//     The compositor's lazy-create guard keys on this being NULL, so a
		//     stale non-NULL pointer is never regenerated → use-after-free on
		//     the next draw.
		//   • luaBindChunk / luaVisibleChunk / repeatBlock.sourceLuaChunk /
		//     ifBlock.testLuaChunk → registry refs that WiredUI_ClearMenus
		//     (phase 2) already released; the integer ref is now dangling.
		// Invalidate them on the restored items so the lazy-create path
		// rebuilds the custom-draw context against the fresh arena and the
		// dispatcher skips the released chunk refs (NOREF). The kept-old-menus
		// fallback loses Lua bind/visible eval until the next successful
		// reload — an acceptable degradation versus a crash.
		for ( int m = 0; m < wui_menuCount; m++ ) {
			wiredMenuDef_t *menu = wui_menus[ m ];
			if ( !menu ) continue;
			for ( int i = 0; i < menu->itemCount; i++ ) {
				wiredItemDef_t *item = menu->items[ i ];
				if ( !item ) continue;
				item->customDrawContext = NULL;
				item->luaBindChunk      = WIRED_CHUNK_NOREF;
				item->luaVisibleChunk   = WIRED_CHUNK_NOREF;
				if ( item->repeatBlock )
					item->repeatBlock->sourceLuaChunk = WIRED_CHUNK_NOREF;
				if ( item->ifBlock )
					item->ifBlock->testLuaChunk = WIRED_CHUNK_NOREF;
			}
		}
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
#include "../../../qcommon/wired/core/scripting/wired_scripting.h"

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

	int totalItems = 0;
	for ( int i = 0; i < wui_menuCount; i++ )
		totalItems += wui_menus[i] ? wui_menus[i]->itemCount : 0;
	Com_Log( SEV_INFO, LOG_CH(ch_ui), "WiredUI: loaded %d menus (%d items total)\n", wui_menuCount, totalItems );
}

#endif // FEAT_WIRED_UI
