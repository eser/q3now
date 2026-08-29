// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_msdf.c -- MSDF font loading and rendering
*/

#include "../../client.h"
#include "cl_wired_msdf.h"
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI || defined(WIRED_WEB_UI_TEXT)

/* ── font pool ──────────────────────────────────────────────────────── */

/* wui_fonts moved from BSS into a named arena
 * (~165 KB of MSDF glyph metadata).  s_fontArena is an Engine-layer arena:
 * created once via lazy-init the first time any reader touches wui_fonts,
 * survives REF_LEVEL_ONLY map transitions, never destroyed (OS reclaims at
 * process exit, mirroring WiredScript_Arena / Console_Arena).
 *
 * GPU font atlas lifecycle is unchanged — the atlasShader qhandle_t
 * inside each msdfFont_t is invalidated by R_DeleteTextures every map
 * and re-registered by MSDF_ReregisterShaders (cl_wired_text.c:51).
 * Font_Arena only houses CPU-side metadata (glyph table, atlas
 * dimensions/metrics, font name). */
#define FONT_ARENA_SIZE  ( MAX_MSDF_FONTS * sizeof( msdfFont_t ) + 4096 )

static arena_t    *s_fontArena   = NULL;
static msdfFont_t *wui_fonts     = NULL;   /* arena-backed; lazy-init below */
static int         wui_fontCount = 0;

static void MSDF_EnsureArena( void )
{
	if ( s_fontArena ) return;   /* idempotent (re-entry from any reader) */
	s_fontArena = Arena_Create( "Font", FONT_ARENA_SIZE );
	wui_fonts   = Arena_AllocArray( s_fontArena, msdfFont_t, MAX_MSDF_FONTS );
	memset( wui_fonts, 0, MAX_MSDF_FONTS * sizeof( msdfFont_t ) );
}

/* ── outline / glow state ──────────────────────────────────────────── */

static float wui_outlineWidth = 0.0f;
static float wui_outlineColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static float wui_glowWidth = 0.0f;
static float wui_glowColor[4] = { 1.0f, 1.0f, 1.0f, 0.3f };

/* ── per-frame string measurement cache (B.8) ──────────────────────── */

#define MSDF_MEAS_CACHE_SIZE 16

typedef struct {
	const char *str;
	msdfFont_t *font;
	float       size;
	float       letterSpacing;
	int         maxChars;      /* glyph budget, or byte length when byteBounded */
	qboolean    byteBounded;   /* qtrue: maxChars is a byte length (end-bounded) */
	float       result;
	int         frame;
} msdf_meas_entry_t;

static msdf_meas_entry_t wui_meas_cache[MSDF_MEAS_CACHE_SIZE];
static unsigned           wui_meas_cache_idx = 0;   // unsigned: defined wrap, no negative %

void MSDF_SetOutline( float outlineWidth, const float *outlineColor,
                       float glowWidth, const float *glowColor )
{
	wui_outlineWidth = outlineWidth;
	if ( outlineColor )
		memcpy( wui_outlineColor, outlineColor, sizeof( wui_outlineColor ) );
	wui_glowWidth = glowWidth;
	if ( glowColor )
		memcpy( wui_glowColor, glowColor, sizeof( wui_glowColor ) );
}

/* ── minimal JSON tokeniser ─────────────────────────────────────────── */
/*
 * msdf-atlas-gen outputs well-formed JSON.  We only need:
 *   - atlas.width, atlas.height, atlas.size, atlas.distanceRange
 *   - glyphs[].unicode, advance, planeBounds.{left,bottom,right,top},
 *     atlasBounds.{left,bottom,right,top}
 *
 * Strategy: walk the text character by character, skip whitespace,
 * identify strings (quoted), numbers, structural chars ({, }, [, ], :, ,).
 * No need for a general-purpose JSON DOM -- just streaming key matching.
 */

typedef enum {
	JTOK_NONE,
	JTOK_STRING,
	JTOK_NUMBER,
	JTOK_LBRACE,       /* { */
	JTOK_RBRACE,       /* } */
	JTOK_LBRACKET,     /* [ */
	JTOK_RBRACKET,     /* ] */
	JTOK_COLON,
	JTOK_COMMA,
	JTOK_TRUE,
	JTOK_FALSE,
	JTOK_NULL,
	JTOK_EOF
} jsonTokenType_t;

#define JSON_TOK_MAX 256

typedef struct {
	const char          *p;         /* current read position */
	jsonTokenType_t     type;
	char                str[JSON_TOK_MAX];
	float               num;
} jsonParser_t;

static void JSON_SkipWhitespace( jsonParser_t *jp )
{
	while ( *jp->p && ( *jp->p == ' ' || *jp->p == '\t' ||
	        *jp->p == '\r' || *jp->p == '\n' ) ) {
		jp->p++;
	}
}

static void JSON_NextToken( jsonParser_t *jp )
{
	int i;

	JSON_SkipWhitespace( jp );

	if ( *jp->p == '\0' ) {
		jp->type = JTOK_EOF;
		jp->str[0] = '\0';
		return;
	}

	switch ( *jp->p ) {
	case '{': jp->type = JTOK_LBRACE;   jp->str[0] = '{'; jp->str[1] = '\0'; jp->p++; return;
	case '}': jp->type = JTOK_RBRACE;   jp->str[0] = '}'; jp->str[1] = '\0'; jp->p++; return;
	case '[': jp->type = JTOK_LBRACKET; jp->str[0] = '['; jp->str[1] = '\0'; jp->p++; return;
	case ']': jp->type = JTOK_RBRACKET; jp->str[0] = ']'; jp->str[1] = '\0'; jp->p++; return;
	case ':': jp->type = JTOK_COLON;    jp->str[0] = ':'; jp->str[1] = '\0'; jp->p++; return;
	case ',': jp->type = JTOK_COMMA;    jp->str[0] = ','; jp->str[1] = '\0'; jp->p++; return;
	default: break;
	}

	/* quoted string */
	if ( *jp->p == '"' ) {
		jp->p++;  /* skip opening quote */
		i = 0;
		while ( *jp->p && *jp->p != '"' && i < JSON_TOK_MAX - 1 ) {
			if ( *jp->p == '\\' && *(jp->p + 1) ) {
				jp->p++;  /* skip backslash, take next char literally */
			}
			jp->str[i++] = *jp->p;
			jp->p++;
		}
		jp->str[i] = '\0';
		if ( *jp->p == '"' ) {
			jp->p++;  /* skip closing quote */
		}
		jp->type = JTOK_STRING;
		jp->num = (float)atof( jp->str );
		return;
	}

	/* number (possibly negative, with decimal point) */
	if ( *jp->p == '-' || ( *jp->p >= '0' && *jp->p <= '9' ) ) {
		i = 0;
		if ( *jp->p == '-' ) {
			jp->str[i++] = *jp->p;
			jp->p++;
		}
		while ( *jp->p && ( ( *jp->p >= '0' && *jp->p <= '9' ) ||
		        *jp->p == '.' || *jp->p == 'e' || *jp->p == 'E' ||
		        *jp->p == '+' || *jp->p == '-' ) && i < JSON_TOK_MAX - 1 ) {
			/* only allow +/- after e/E */
			if ( ( *jp->p == '+' || *jp->p == '-' ) && i > 0 &&
			     jp->str[i-1] != 'e' && jp->str[i-1] != 'E' ) {
				break;
			}
			jp->str[i++] = *jp->p;
			jp->p++;
		}
		jp->str[i] = '\0';
		jp->num = (float)atof( jp->str );
		jp->type = JTOK_NUMBER;
		return;
	}

	/* literals: true, false, null */
	if ( jp->p[0] == 't' && jp->p[1] == 'r' && jp->p[2] == 'u' && jp->p[3] == 'e' ) {
		jp->type = JTOK_TRUE; Q_strncpyz( jp->str, "true", sizeof(jp->str) ); jp->p += 4; return;
	}
	if ( jp->p[0] == 'f' && jp->p[1] == 'a' && jp->p[2] == 'l' && jp->p[3] == 's' && jp->p[4] == 'e' ) {
		jp->type = JTOK_FALSE; Q_strncpyz( jp->str, "false", sizeof(jp->str) ); jp->p += 5; return;
	}
	if ( jp->p[0] == 'n' && jp->p[1] == 'u' && jp->p[2] == 'l' && jp->p[3] == 'l' ) {
		jp->type = JTOK_NULL; Q_strncpyz( jp->str, "null", sizeof(jp->str) ); jp->p += 4; return;
	}

	/* unknown character -- skip it */
	COM_WARN( LOG_CH(ch_ui), "MSDF JSON: unexpected char '%c'\n", *jp->p );
	jp->p++;
	jp->type = JTOK_NONE;
}

/*
 * JSON_SkipValue -- skip a complete JSON value (object, array, or primitive).
 * Used to skip over keys/values we don't care about.
 */
static void JSON_SkipValue( jsonParser_t *jp )
{
	if ( jp->type == JTOK_LBRACE ) {
		int depth = 1;
		while ( depth > 0 ) {
			JSON_NextToken( jp );
			if ( jp->type == JTOK_LBRACE )    depth++;
			else if ( jp->type == JTOK_RBRACE ) depth--;
			else if ( jp->type == JTOK_EOF )   return;
		}
		return;
	}

	if ( jp->type == JTOK_LBRACKET ) {
		int depth = 1;
		while ( depth > 0 ) {
			JSON_NextToken( jp );
			if ( jp->type == JTOK_LBRACKET )    depth++;
			else if ( jp->type == JTOK_RBRACKET ) depth--;
			else if ( jp->type == JTOK_EOF )     return;
		}
		return;
	}

	/* primitive -- already consumed by NextToken, nothing more to skip */
}

/*
 * JSON_Expect -- consume the next token and verify its type.
 * Returns qfalse on mismatch or EOF.
 */
static qboolean JSON_Expect( jsonParser_t *jp, jsonTokenType_t expected )
{
	JSON_NextToken( jp );
	return ( jp->type == expected ) ? qtrue : qfalse;
}

/* Advance to the next key-value pair in a JSON object.
   Fills key and positions jp at the value token. Returns qfalse at '}' or EOF. */
static qboolean JSON_NextMember( jsonParser_t *jp, char *key, int keySize )
{
	while ( jp->type != JTOK_RBRACE && jp->type != JTOK_EOF ) {
		JSON_NextToken( jp );
		if ( jp->type == JTOK_RBRACE ) return qfalse;
		if ( jp->type == JTOK_STRING ) {
			Q_strncpyz( key, jp->str, keySize );
			if ( !JSON_Expect( jp, JTOK_COLON ) ) return qfalse;
			JSON_NextToken( jp );
			return qtrue;
		}
	}
	return qfalse;
}

/* ── JSON glyph/atlas parsing ───────────────────────────────────────── */

/*
 * Parse a bounds object: { "left": N, "bottom": N, "right": N, "top": N }
 * Assumes the opening '{' has already been consumed.
 */
static qboolean MSDF_ParseBounds( jsonParser_t *jp,
                                  float *left, float *bottom,
                                  float *right, float *top )
{
	char key[64];
	*left = *bottom = *right = *top = 0.0f;
	while ( JSON_NextMember( jp, key, sizeof(key) ) ) {
		if      ( !Q_stricmp(key, "left")   ) *left   = jp->num;
		else if ( !Q_stricmp(key, "bottom") ) *bottom = jp->num;
		else if ( !Q_stricmp(key, "right")  ) *right  = jp->num;
		else if ( !Q_stricmp(key, "top")    ) *top    = jp->num;
	}
	return qtrue;
}

/*
 * Parse a single glyph object.
 * Assumes the opening '{' has already been consumed.
 */
static qboolean MSDF_ParseGlyph( jsonParser_t *jp, msdfFont_t *font )
{
	int         unicode  = -1;
	float       advance  = 0.0f;
	float       pL = 0, pB = 0, pR = 0, pT = 0;
	float       aL = 0, aB = 0, aR = 0, aT = 0;
	qboolean    hasPlaneBounds = qfalse;
	qboolean    hasAtlasBounds = qfalse;
	msdfGlyph_t *g;

	while ( jp->type != JTOK_RBRACE && jp->type != JTOK_EOF ) {
		JSON_NextToken( jp );

		if ( jp->type == JTOK_RBRACE ) {
			break;
		}

		if ( jp->type == JTOK_STRING ) {
			char key[64];
			Q_strncpyz( key, jp->str, sizeof(key) );

			if ( !JSON_Expect( jp, JTOK_COLON ) ) return qfalse;
			JSON_NextToken( jp );   /* value or '{' or '[' */

			if ( Q_stricmp( key, "unicode" ) == 0 ) {
				unicode = (int)jp->num;
			} else if ( Q_stricmp( key, "advance" ) == 0 ) {
				advance = jp->num;
			} else if ( Q_stricmp( key, "planeBounds" ) == 0 ) {
				if ( jp->type == JTOK_LBRACE ) {
					MSDF_ParseBounds( jp, &pL, &pB, &pR, &pT );
					hasPlaneBounds = qtrue;
					/* ParseBounds leaves jp->type == JTOK_RBRACE from inner '}'.
					 * Advance past it so the glyph loop doesn't exit early. */
					JSON_NextToken( jp );  /* reads ',' or outer '}' */
					if ( jp->type == JTOK_RBRACE ) break; /* glyph object closed */
					continue; /* comma — loop back to read next key */
				}
				JSON_SkipValue( jp );

			} else if ( Q_stricmp( key, "atlasBounds" ) == 0 ) {
				if ( jp->type == JTOK_LBRACE ) {
					MSDF_ParseBounds( jp, &aL, &aB, &aR, &aT );
					hasAtlasBounds = qtrue;
					JSON_NextToken( jp );
					if ( jp->type == JTOK_RBRACE ) break;
					continue;
				}
				JSON_SkipValue( jp );

			} else {
				JSON_SkipValue( jp );
			}
		}
		/* skip commas */
	}

	if ( unicode < 0 ) {
		return qtrue;   /* silently ignore invalid codepoints */
	}

	if ( font->glyphCount >= MAX_MSDF_GLYPH_COUNT ) {
		return qtrue;   /* silently ignore excess glyphs */
	}

	g = &font->glyphs[font->glyphCount];
	g->unicode      = unicode;
	g->advance      = advance;

	if ( hasPlaneBounds ) {
		g->planeLeft    = pL;
		g->planeBottom  = pB;
		g->planeRight   = pR;
		g->planeTop     = pT;
	}

	if ( hasAtlasBounds ) {
		g->atlasLeft    = aL;
		g->atlasBottom  = aB;
		g->atlasRight   = aR;
		g->atlasTop     = aT;
	}

	font->glyphCount++;
	return qtrue;
}

/*
 * Parse the "glyphs" array.
 * Assumes the opening '[' has already been consumed.
 */
static qboolean MSDF_ParseGlyphsArray( jsonParser_t *jp, msdfFont_t *font )
{
	while ( jp->type != JTOK_EOF ) {
		JSON_NextToken( jp );

		if ( jp->type == JTOK_RBRACKET ) {
			break;
		}
		if ( jp->type == JTOK_COMMA ) {
			continue;
		}
		if ( jp->type == JTOK_LBRACE ) {
			if ( !MSDF_ParseGlyph( jp, font ) ) {
				return qfalse;
			}
		}
	}
	return qtrue;
}

/*
 * Parse the "atlas" metadata object.
 * Assumes the opening '{' has already been consumed.
 */
static qboolean MSDF_ParseAtlasObject( jsonParser_t *jp, msdfFont_t *font )
{
	char key[64];
	while ( JSON_NextMember( jp, key, sizeof(key) ) ) {
		if      ( !Q_stricmp(key, "width")         ) font->atlasWidth    = (int)jp->num;
		else if ( !Q_stricmp(key, "height")        ) font->atlasHeight   = (int)jp->num;
		else if ( !Q_stricmp(key, "size")          ) font->atlasSize     = jp->num;
		else if ( !Q_stricmp(key, "distanceRange") ) font->distanceRange = jp->num;
		else                                          JSON_SkipValue( jp );
	}
	return qtrue;
}

/*
 * Parse "metrics" object: ascender, descender, lineHeight.
 */
static qboolean MSDF_ParseMetricsObject( jsonParser_t *jp, msdfFont_t *font )
{
	char key[64];
	while ( JSON_NextMember( jp, key, sizeof(key) ) ) {
		if      ( !Q_stricmp(key, "ascender")   ) font->ascender   = jp->num;
		else if ( !Q_stricmp(key, "descender")  ) font->descender  = jp->num;
		else if ( !Q_stricmp(key, "lineHeight") ) font->lineHeight = jp->num;
		else                                       JSON_SkipValue( jp );
	}
	return qtrue;
}

/*
 * MSDF_ParseJSON -- parse the top-level JSON file.
 * Expects: { "atlas": { ... }, "metrics": { ... }, "glyphs": [ ... ] }
 */
static qboolean MSDF_ParseJSON( const char *text, msdfFont_t *font )
{
	jsonParser_t jp;

	memset( &jp, 0, sizeof(jp) );
	jp.p = text;

	/* expect opening '{' */
	JSON_NextToken( &jp );
	if ( jp.type != JTOK_LBRACE ) {
		COM_ERROR( LOG_CH(ch_ui), "MSDF_ParseJSON: expected '{' at start\n" );
		return qfalse;
	}

	/* walk top-level keys */
	while ( jp.type != JTOK_EOF ) {
		JSON_NextToken( &jp );

		if ( jp.type == JTOK_RBRACE || jp.type == JTOK_EOF ) {
			break;
		}

		if ( jp.type == JTOK_STRING ) {
			char key[64];
			Q_strncpyz( key, jp.str, sizeof(key) );

			if ( !JSON_Expect( &jp, JTOK_COLON ) ) return qfalse;
			JSON_NextToken( &jp );   /* value */

			if ( Q_stricmp( key, "atlas" ) == 0 && jp.type == JTOK_LBRACE ) {
				if ( !MSDF_ParseAtlasObject( &jp, font ) ) return qfalse;
			} else if ( Q_stricmp( key, "metrics" ) == 0 && jp.type == JTOK_LBRACE ) {
				if ( !MSDF_ParseMetricsObject( &jp, font ) ) return qfalse;
			} else if ( Q_stricmp( key, "glyphs" ) == 0 && jp.type == JTOK_LBRACKET ) {
				if ( !MSDF_ParseGlyphsArray( &jp, font ) ) return qfalse;
			} else {
				JSON_SkipValue( &jp );
			}
		}
		/* skip commas */
	}

	return qtrue;
}

/* ── glyph sort comparator ──────────────────────────────────────────── */

static int MSDF_GlyphCompare( const void *a, const void *b )
{
	const msdfGlyph_t *ga = (const msdfGlyph_t *)a;
	const msdfGlyph_t *gb = (const msdfGlyph_t *)b;
	return ga->unicode - gb->unicode;
}

/* ── glyph fallback chain ───────────────────────────────────────────── */

/* Primary lookup with cross-font fallback. When the requested font lacks
 * the codepoint (typical for atlases authored against a narrow charset)
 * we iterate the live font registry and pick the first font that ships
 * the glyph. Returns the (font, glyph) pair through the out-params so
 * the renderer can switch atlas shaders when the fallback font wins.
 * Both out-params are NULL when no font has the glyph. */
static void MSDF_FindGlyphFallback( msdfFont_t *primary, int unicode,
                                     msdfFont_t **outFont, msdfGlyph_t **outGlyph )
{
	msdfGlyph_t *g;
	int          i;

	*outFont  = NULL;
	*outGlyph = NULL;

	if ( primary ) {
		g = MSDF_FindGlyph( primary, unicode );
		if ( g ) { *outFont = primary; *outGlyph = g; return; }
	}

	/* Fallback: walk every loaded font. The first hit wins. The text path
	 * usually requests ASCII which the primary covers, so this loop only
	 * runs for the rare BMP punctuation case (smart quotes, dashes,
	 * dingbats, etc.). Order matches the registration order, which is
	 * roughly "general-purpose first, icon-only atlases last". */
	for ( i = 0; i < wui_fontCount; i++ ) {
		msdfFont_t *f = &wui_fonts[ i ];
		if ( !f->loaded || f == primary ) continue;
		g = MSDF_FindGlyph( f, unicode );
		if ( g ) { *outFont = f; *outGlyph = g; return; }
	}
}

/* ── binary search for glyph by unicode ─────────────────────────────── */

msdfGlyph_t *MSDF_FindGlyph( msdfFont_t *font, int unicode )
{
	int lo = 0, hi = font->glyphCount - 1;

	while ( lo <= hi ) {
		int mid = (lo + hi) / 2;
		if ( font->glyphs[mid].unicode == unicode )
			return &font->glyphs[mid];
		if ( font->glyphs[mid].unicode < unicode )
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

/* ── font loading ───────────────────────────────────────────────────── */

msdfFont_t *MSDF_LoadFont( const char *fontName )
{
	char         jsonPath[MAX_QPATH];
	char         shaderPath[MAX_QPATH];

	if ( !fontName || !fontName[0] ) {
		COM_ERROR( LOG_CH(ch_ui), "MSDF_LoadFont: NULL font name\n" );
		return NULL;
	}

	MSDF_EnsureArena();

	/* check if already loaded */
	for ( int i = 0; i < wui_fontCount; i++ ) {
		if ( wui_fonts[i].loaded && Q_stricmp( wui_fonts[i].name, fontName ) == 0 ) {
			return &wui_fonts[i];
		}
	}

	/* find a free slot */
	if ( wui_fontCount >= MAX_MSDF_FONTS ) {
		COM_ERROR( LOG_CH(ch_ui), "MSDF_LoadFont: too many fonts (max %d)\n", MAX_MSDF_FONTS );
		return NULL;
	}
	msdfFont_t *font = &wui_fonts[wui_fontCount];

	/* zero out the struct */
	memset( font, 0, sizeof(*font) );
	Q_strncpyz( font->name, fontName, sizeof(font->name) );

	/* load JSON metrics */
	Com_sprintf( jsonPath, sizeof(jsonPath), "fonts/%s.json", fontName );
	void *buf;
	int   len = FS_ReadFile( jsonPath, &buf );
	if ( len <= 0 || !buf ) {
		COM_WARN( LOG_CH(ch_ui), "MSDF_LoadFont: could not read '%s'\n", jsonPath );
		return NULL;
	}

	if ( !MSDF_ParseJSON( (const char *)buf, font ) ) {
		COM_ERROR( LOG_CH(ch_ui), "MSDF_LoadFont: parse error in '%s'\n", jsonPath );
		FS_FreeFile( buf );
		return NULL;
	}
	FS_FreeFile( buf );

	/* sort glyphs by unicode for binary search */
	if ( font->glyphCount > 1 ) {
		qsort( font->glyphs, font->glyphCount, sizeof(msdfGlyph_t), MSDF_GlyphCompare );
	}

	/* validate atlas metadata */
	if ( font->atlasWidth <= 0 || font->atlasHeight <= 0 ) {
		COM_ERROR( LOG_CH(ch_ui), "MSDF_LoadFont: invalid atlas dimensions in '%s'\n", jsonPath );
		return NULL;
	}

	/* Register atlas as an MSDF shader — the renderer will use its MSDF
	 * fragment program (median-of-three + smoothstep) for antialiased text. */
	Com_sprintf( shaderPath, sizeof(shaderPath), "fonts/%s_atlas", fontName );
	font->atlasShader = re.RegisterMSDFShader( shaderPath,
		font->distanceRange, font->atlasWidth, font->atlasHeight );
	if ( font->atlasShader == 0 ) {
		COM_WARN( LOG_CH(ch_ui), "MSDF_LoadFont: could not register atlas shader '%s'\n", shaderPath );
		/* not fatal -- font can still be used for measurement */
	}
	// Phase 7.15.4-a class-B pin: the font atlas is a UI-critical, cached handle
	// bound every text draw without re-checking residency (and SL-4c lazy fonts are
	// block-until-resident) — the texture-LRU must never evict it. Dark in 7.15.4-a.
	else if ( re.PinShaderImages ) re.PinShaderImages( font->atlasShader );

	font->loaded = qtrue;
	wui_fontCount++;

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "MSDF_LoadFont: loaded '%s' (%dx%d atlas, %.0f px, %d glyphs)\n",
	            fontName, font->atlasWidth, font->atlasHeight, font->atlasSize,
	            font->glyphCount );

	return font;
}

/* ── shader re-registration ────────────────────────────────────────── */
/*
 * Re-register all atlas shaders for loaded fonts.
 * Must be called after renderer reinit (vid_restart, map change) because
 * all shader handles become stale when the renderer shuts down.
 */
void MSDF_ReregisterShaders( void )
{
	char shaderPath[MAX_QPATH];

	MSDF_EnsureArena();

	for ( int i = 0; i < wui_fontCount; i++ ) {
		msdfFont_t *f = &wui_fonts[i];
		if ( !f->loaded || f->name[0] == '\0' ) continue;

		Com_sprintf( shaderPath, sizeof(shaderPath), "fonts/%s_atlas", f->name );
		f->atlasShader = re.RegisterMSDFShader( shaderPath,
			f->distanceRange, f->atlasWidth, f->atlasHeight );

		if ( f->atlasShader == 0 ) {
			COM_WARN( LOG_CH(ch_ui), "MSDF_ReregisterShaders: failed for '%s'\n", f->name );
		}
		// Phase 7.15.4-a class-B pin (re-register path, e.g. vid_restart): re-pin
		// the freshly-resolved atlas — IMGFLAG_PINNED must survive the new image_t.
		else if ( re.PinShaderImages ) re.PinShaderImages( f->atlasShader );
	}

	if ( wui_fontCount > 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "MSDF_ReregisterShaders: re-registered %d font(s)\n", wui_fontCount );
	}
}

int MSDF_GetFontCount( void ) { return wui_fontCount; }

int MSDF_GetRenderableFontCount( void )
{
	int count = 0;
	for ( int i = 0; i < wui_fontCount; ++i )
		if ( wui_fonts[i].loaded && wui_fonts[i].atlasShader ) ++count;
	return count;
}

/* ── character drawing ──────────────────────────────────────────────── */

void MSDF_DrawChar( msdfFont_t *font, float x, float y,
                    float size, const float *color, int ch )
{
	msdfFont_t  *resolvedFont = NULL;
	msdfGlyph_t *g            = NULL;
	float        pixelSize;
	float        s0, t0, s1, t1;
	float        xOff, yOff, w, h;
	float        drawX, drawY, drawW, drawH;

	if ( !font || !font->loaded ) return;
	if ( ch < 0 ) return;

	/* Fallback chain — if the requested font lacks this codepoint, pick
	 * up the first font in the registry that ships it. The (font,glyph)
	 * pair travels together so we sample the right atlas + shader. */
	MSDF_FindGlyphFallback( font, ch, &resolvedFont, &g );
	if ( !g || !resolvedFont ) return;
	font = resolvedFont;

	/* nothing to draw if there are no atlas bounds (e.g. space character) */
	if ( g->atlasRight <= g->atlasLeft || g->atlasTop <= g->atlasBottom ) {
		return;
	}

	/* First-sample inventory probe (wui_fontProbe cheat): log the first draw of
	 * each font atlas with a frame marker so the load-burst vs in-frame first-use
	 * classification can be measured. cls.framecount==0 means the draw happens in
	 * the synchronous boot burst before any frame boundary. One line per font. */
	{
		static qboolean fontFirstDrawn[ MAX_MSDF_FONTS ];
		static cvar_t  *probe = NULL;
		int             fidx = (int)( font - wui_fonts );
		if ( !probe ) probe = Cvar_Get( "wui_fontProbe", "0", CVAR_CHEAT );
		if ( probe && probe->integer && fidx >= 0 && fidx < MAX_MSDF_FONTS && !fontFirstDrawn[ fidx ] ) {
			fontFirstDrawn[ fidx ] = qtrue;
			Com_Log( SEV_WARN, LOG_CH(ch_ui), "UIPROBE font-first-draw name='%s' frame=%d\n",
			            font->name, (int)cls.framecount );
		}
	}

	pixelSize = size;  /* 1 em = pixelSize virtual pixels */

	/*
	 * UV coordinates: atlas bounds (pixels) -> normalised 0..1.
	 *
	 * msdf-atlas-gen uses bottom-left origin for atlasBounds, but Q3
	 * textures use top-left origin.  Flip T axis vertically:
	 *   t0 = 1 - (atlasTop    / atlasHeight)   -- top of glyph in atlas
	 *   t1 = 1 - (atlasBottom / atlasHeight)   -- bottom of glyph in atlas
	 * Since atlasTop > atlasBottom, t0 < t1,
	 * which is what DrawStretchPic expects (top < bottom).
	 */
	s0 = g->atlasLeft   / (float)font->atlasWidth;
	s1 = g->atlasRight  / (float)font->atlasWidth;
	t0 = 1.0f - ( g->atlasTop    / (float)font->atlasHeight );
	t1 = 1.0f - ( g->atlasBottom / (float)font->atlasHeight );

	/* glyph screen-space quad from planeBounds (em-space).
	 * y input is the TOP of the text line (bitmap convention).
	 * Subtract ascender so glyphs align to the top, not the baseline. */
	xOff = g->planeLeft * pixelSize;
	yOff = ( font->ascender - g->planeTop ) * pixelSize;
	w    = ( g->planeRight - g->planeLeft ) * pixelSize;
	h    = ( g->planeTop   - g->planeBottom ) * pixelSize;

	drawX = x + xOff;
	drawY = y + yOff;
	drawW = w;
	drawH = h;

	/* apply color */
	re.SetColor( color );

	/* push outline/glow state into the render command stream */
	re.SetMSDFOutline( wui_outlineWidth, wui_outlineColor,
	                    wui_glowWidth, wui_glowColor );

	/* coordinates are already real screen pixels */
	re.DrawStretchPic( drawX, drawY, drawW, drawH, s0, t0, s1, t1, font->atlasShader );
}

/* ── Q3 color code helpers ──────────────────────────────────────────── */

/*
 * Check if the string at `p` starts a Q3 color code.
 * Returns the number of chars consumed (0 if not a color code),
 * and writes the new color to `outColor` if it's a standard ^0-^9 code.
 */
static int MSDF_HandleColorCode( const char *p, float *outColor )
{
	if ( p[0] != Q_COLOR_ESCAPE || p[1] == '\0' ) {
		return 0;
	}

	/* ^^ produces a literal '^' -- not a color code, returns 0 */
	if ( p[1] == Q_COLOR_ESCAPE ) {
		return 0;
	}

	/* ^0 through ^9 (and a-z for extended palette) */
	if ( ( p[1] >= '0' && p[1] <= '9' ) ||
	     ( p[1] >= 'a' && p[1] <= 'z' ) ||
	     ( p[1] >= 'A' && p[1] <= 'Z' ) ) {
		int idx = ColorIndexFromChar( p[1] );
		if ( outColor ) {
			outColor[0] = g_color_table[idx][0];
			outColor[1] = g_color_table[idx][1];
			outColor[2] = g_color_table[idx][2];
			/* preserve caller's alpha */
		}
		return 2;   /* consumed ^X */
	}

	/* unrecognised escape -- skip the ^ and the next char */
	return 2;
}

/* ── glyph advance helper ───────────────────────────────────────────── */

static float MSDF_GlyphAdvancePx( msdfFont_t *font, int ch,
                                   float pixelSize, float letterSpacing ) {
	/* Mirror the DrawChar fallback so layout width stays consistent with
	 * the chosen draw font when a glyph lives in a secondary atlas. */
	msdfFont_t  *resolvedFont = NULL;
	msdfGlyph_t *g            = NULL;
	MSDF_FindGlyphFallback( font, ch, &resolvedFont, &g );
	return ( g ? g->advance : 0.5f ) * pixelSize + letterSpacing;
}

/* ── character iterator ─────────────────────────────────────────────── */

#define MSDF_CHAR_COLORCODE (-1)  /* color code consumed */
#define MSDF_CHAR_NEWLINE   (-2)  /* newline consumed    */

/* Consume one escape sequence or character from *pp and return a codepoint.
   colorOut: if non-NULL, color codes update it; if NULL, they are silently skipped.
   Returns: character codepoint (>0), MSDF_CHAR_COLORCODE, MSDF_CHAR_NEWLINE, or 0 (end).

   UTF-8 multibyte sequences are decoded into a single Unicode codepoint
   that the caller feeds straight into the atlas glyph lookup. Atlases
   that ship supplementary characters (JBMono covers Latin-1 supplement
   + selected punctuation through U+25CF for v2) resolve directly;
   atlases that don't fall back to the missing-glyph slot — same
   behaviour as for unknown ASCII codepoints. */
static int MSDF_NextRenderableChar( const char **pp, float *colorOut )
{
	const char *p = *pp;
	int           skip;
	unsigned char b0;
	if ( !*p ) return 0;
	if ( *p == '\n' ) { *pp = p + 1; return MSDF_CHAR_NEWLINE; }
	if ( p[0] == Q_COLOR_ESCAPE && p[1] == Q_COLOR_ESCAPE ) { *pp = p + 2; return Q_COLOR_ESCAPE; }
	skip = MSDF_HandleColorCode( p, colorOut );
	if ( skip > 0 ) { *pp = p + skip; return MSDF_CHAR_COLORCODE; }

	b0 = (unsigned char) p[ 0 ];
	if ( b0 < 0x80 ) {
		/* ASCII fast path. */
		*pp = p + 1;
		return b0;
	}
	/* the icon-font path (cl_wired_parse.c iconText) stores RAW single
	 * bytes in the 0xE0..0xFE range — glyphs registered at codepoints
	 * 224..254 — which are NOT valid UTF-8 lead bytes followed by
	 * continuation bytes. Decode defensively: a multi-byte lead is only
	 * consumed as multi-byte when its continuation byte(s) are actually
	 * present and well-formed (0b10xxxxxx) AND do not run past the NUL
	 * terminator. Otherwise the lead byte is treated as a single codepoint
	 * (so 0xE0..0xFE map straight to glyphs 224..254 as before) and the
	 * pointer advances by exactly one byte — never past the terminator. */
	if ( ( b0 & 0xE0 ) == 0xC0 ) {
		/* 2-byte UTF-8: 110xxxxx 10xxxxxx -> 11 bits (U+0080..U+07FF). */
		unsigned char b1 = (unsigned char) p[ 1 ];
		if ( ( b1 & 0xC0 ) != 0x80 ) { *pp = p + 1; return b0; }
		*pp = p + 2;
		return ( ( b0 & 0x1F ) << 6 ) | ( b1 & 0x3F );
	}
	if ( ( b0 & 0xF0 ) == 0xE0 ) {
		/* 3-byte UTF-8: 1110xxxx 10xxxxxx 10xxxxxx -> 16 bits.
		 * Covers BMP punctuation the v2 design uses (diamond U+25C6,
		 * circle U+25CF, en/em-dash, smart quotes, etc.). A NUL in b1
		 * short-circuits the b2 read (b1 fails the 0x80 test first). */
		unsigned char b1 = (unsigned char) p[ 1 ];
		unsigned char b2 = ( b1 != 0 ) ? (unsigned char) p[ 2 ] : 0;
		if ( ( b1 & 0xC0 ) != 0x80 || ( b2 & 0xC0 ) != 0x80 ) { *pp = p + 1; return b0; }
		*pp = p + 3;
		return ( ( b0 & 0x0F ) << 12 ) | ( ( b1 & 0x3F ) << 6 ) | ( b2 & 0x3F );
	}
	if ( ( b0 & 0xF8 ) == 0xF0 ) {
		/* 4-byte UTF-8 (supplementary planes; emoji). No atlas ships
		 * glyphs above U+FFFF today; resolve as the missing glyph but
		 * only advance past the full sequence after validating all three
		 * continuation bytes — a short/raw sequence (e.g. an icon byte
		 * 0xF0..0xF7 at end of string) must NOT walk past the NUL.
		 * Stop reading the moment a NUL or bad continuation byte appears. */
		unsigned char b1 = (unsigned char) p[ 1 ];
		unsigned char b2 = ( b1 != 0 ) ? (unsigned char) p[ 2 ] : 0;
		unsigned char b3 = ( b2 != 0 ) ? (unsigned char) p[ 3 ] : 0;
		if ( ( b1 & 0xC0 ) != 0x80 || ( b2 & 0xC0 ) != 0x80 || ( b3 & 0xC0 ) != 0x80 ) {
			*pp = p + 1;
			return b0;
		}
		*pp = p + 4;
		return 0xFFFD;
	}
	/* 0xF8..0xFE lead bytes are not valid UTF-8 leads at all (and 0xFE/0xFF
	 * never appear in UTF-8). Treat as a single-byte codepoint so raw icon
	 * bytes (e.g. 0xFE diamond) resolve to glyph 254, advancing one byte. */
	*pp = p + 1;
	return b0;
}

/* ── string drawing ─────────────────────────────────────────────────── */

/* Core string draw shared by the glyph-budget entry point (MSDF_DrawString)
 * and the byte-bounded entry point (MSDF_DrawStringBytes). Two independent
 * stop conditions, both honouring the renderable-glyph model:
 *   maxChars >= 0 : stop after that many RENDERABLE glyphs are drawn (colour
 *                   codes never count) — the ellipsis-prefix contract used by
 *                   Text_DrawClipped via MSDF_ClampToWidth.
 *   end != NULL   : stop when the source pointer reaches `end` BYTES into the
 *                   string — used by the Clay compositor so a non-NUL-terminated
 *                   word/run slice measures and draws exactly its own bytes
 *                   (colour codes inside those bytes are skipped for width but
 *                   consume their bytes). This keeps measure == draw for any
 *                   colour-coded slice: both walk the identical byte window and
 *                   skip the identical ^N codes. */
static void MSDF_DrawStringCore( msdfFont_t *font, float x, float y,
                                 float size, const float *color,
                                 const char *str, int maxChars,
                                 const char *end, float letterSpacing,
                                 qboolean forceColor )
{
	float       curColor[4];

	if ( !font || !font->loaded || !str || !str[0] ) return;

	/* copy the initial color so we can modify RGB via color codes */
	curColor[0] = color ? color[0] : 1.0f;
	curColor[1] = color ? color[1] : 1.0f;
	curColor[2] = color ? color[2] : 1.0f;
	curColor[3] = color ? color[3] : 1.0f;

	float       pixelSize = size;
	float       curX      = x;
	int         drawn     = 0;
	const char *p;

	for ( p = str; *p; ) {
		int ch;
		if ( maxChars >= 0 && drawn >= maxChars ) break;
		if ( end && p >= end ) break;
		ch = MSDF_NextRenderableChar( &p, forceColor ? NULL : curColor );
		if ( ch == MSDF_CHAR_COLORCODE ) continue;
		if ( ch == MSDF_CHAR_NEWLINE ) { curX = x; y += font->lineHeight * pixelSize; continue; }
		MSDF_DrawChar( font, curX, y, size, curColor, ch );
		curX += MSDF_GlyphAdvancePx( font, ch, pixelSize, letterSpacing );
		drawn++;
	}

	/* reset color to avoid bleeding into subsequent draws */
	re.SetColor( NULL );
}

void MSDF_DrawString( msdfFont_t *font, float x, float y,
                      float size, const float *color,
                      const char *str, int maxChars, float letterSpacing,
                      qboolean forceColor )
{
	MSDF_DrawStringCore( font, x, y, size, color, str, maxChars, NULL,
	                     letterSpacing, forceColor );
}

/* Byte-bounded draw: render exactly `byteLen` bytes of `str` (a possibly
 * non-NUL-terminated slice into a larger buffer), skipping colour codes.
 * byteLen < 0 falls back to run-to-NUL. Companion of MSDF_MeasureStringBytes;
 * the Clay compositor uses this pair so the metric it lays out with is the
 * metric it renders with, for any embedded ^N codes. */
void MSDF_DrawStringBytes( msdfFont_t *font, float x, float y,
                           float size, const float *color,
                           const char *str, int byteLen, float letterSpacing,
                           qboolean forceColor )
{
	const char *end = ( byteLen >= 0 && str ) ? str + byteLen : NULL;
	MSDF_DrawStringCore( font, x, y, size, color, str, -1, end,
	                     letterSpacing, forceColor );
}

/* ── string measurement ─────────────────────────────────────────────── */

/* Core measure shared by the glyph-budget entry point (MSDF_MeasureString)
 * and the byte-bounded entry point (MSDF_MeasureStringBytes). The stop
 * conditions mirror MSDF_DrawStringCore exactly (same maxChars glyph budget,
 * same `end` byte bound, same MSDF_NextRenderableChar colour-code skipping),
 * which is what keeps measured width == drawn width for any colour-coded slice.
 * The per-frame cache key folds in `end` so byte-bounded and unbounded queries
 * on the same `str` pointer never alias. */
static float MSDF_MeasureStringCore( msdfFont_t *font, float size,
                                     const char *str, int maxChars,
                                     const char *end, float letterSpacing )
{
	if ( !font || !font->loaded || !str || !str[0] ) return 0.0f;

	/* per-frame cache: avoid re-iterating the same string within one frame.
	 * `maxChars` doubles as the byte-bound discriminator: unbounded calls use
	 * the caller's maxChars, byte-bounded calls store the byte length, so the
	 * two never collide on the same (str,font,size,ls) tuple. */
	int cacheKey = end ? (int)( end - str ) : maxChars;
	for ( int i = 0; i < MSDF_MEAS_CACHE_SIZE; i++ ) {
		if ( wui_meas_cache[i].frame == cls.realtime &&
		     wui_meas_cache[i].str == str &&
		     wui_meas_cache[i].font == font &&
		     wui_meas_cache[i].size == size &&
		     wui_meas_cache[i].letterSpacing == letterSpacing &&
		     wui_meas_cache[i].byteBounded == ( end != NULL ) &&
		     wui_meas_cache[i].maxChars == cacheKey ) {
			return wui_meas_cache[i].result;
		}
	}

	float       pixelSize = size;
	float       lineWidth = 0.0f;   /* accumulator for the current line */
	float       maxWidth  = 0.0f;   /* maximum width seen across all lines */
	int         counted   = 0;
	const char *p;

	for ( p = str; *p; ) {
		int ch;
		if ( maxChars >= 0 && counted >= maxChars ) break;
		if ( end && p >= end ) break;
		ch = MSDF_NextRenderableChar( &p, NULL );
		if ( ch == MSDF_CHAR_COLORCODE ) continue;
		if ( ch == MSDF_CHAR_NEWLINE ) { if ( lineWidth > maxWidth ) maxWidth = lineWidth; lineWidth = 0.0f; continue; }
		lineWidth += MSDF_GlyphAdvancePx( font, ch, pixelSize, letterSpacing );
		counted++;
	}

	/* commit the final line (no trailing \n required) */
	if ( lineWidth > maxWidth ) maxWidth = lineWidth;

	/* store result in per-frame ring cache */
	{
		unsigned idx = wui_meas_cache_idx & ( MSDF_MEAS_CACHE_SIZE - 1 );  // size is a power of two
		wui_meas_cache[idx].str          = str;
		wui_meas_cache[idx].font         = font;
		wui_meas_cache[idx].size         = size;
		wui_meas_cache[idx].letterSpacing = letterSpacing;
		wui_meas_cache[idx].maxChars     = cacheKey;
		wui_meas_cache[idx].byteBounded  = ( end != NULL );
		wui_meas_cache[idx].result       = maxWidth;
		wui_meas_cache[idx].frame        = cls.realtime;
		wui_meas_cache_idx++;
	}

	return maxWidth;
}

float MSDF_MeasureString( msdfFont_t *font, float size,
                          const char *str, int maxChars, float letterSpacing )
{
	return MSDF_MeasureStringCore( font, size, str, maxChars, NULL, letterSpacing );
}

/* Byte-bounded measure: width of exactly `byteLen` bytes of `str` (a possibly
 * non-NUL-terminated slice), skipping colour codes. byteLen < 0 falls back to
 * run-to-NUL. This is the fix for the Clay word-summation skew: Clay hands the
 * measure callback byte slices into a shared, non-terminated buffer, so a
 * glyph-budget stop would over-read past the slice whenever the slice held an
 * embedded ^N code (each 2-byte code let one extra glyph slip in). Bounding on
 * the byte window stops exactly where the draw run stops, so measure == draw. */
float MSDF_MeasureStringBytes( msdfFont_t *font, float size,
                               const char *str, int byteLen, float letterSpacing )
{
	const char *end = ( byteLen >= 0 && str ) ? str + byteLen : NULL;
	return MSDF_MeasureStringCore( font, size, str, -1, end, letterSpacing );
}

/* ── clamped char count ─────────────────────────────────────────────── */

int MSDF_ClampToWidth( msdfFont_t *font, float size,
                       const char *str, float maxPixels,
                       float letterSpacing, float *totalWidthOut )
{
	float       pixelSize = size;
	float       curWidth  = 0.0f;
	int         counted   = 0;
	const char *p;

	if ( totalWidthOut ) *totalWidthOut = 0.0f;

	if ( !font || !font->loaded || !str || !str[0] || maxPixels <= 0.0f )
		return 0;

	for ( p = str; *p; ) {
		float       advance;
		const char *q  = p;
		int         ch = MSDF_NextRenderableChar( &q, NULL );

		if ( ch == 0 ) break;
		if ( ch == MSDF_CHAR_COLORCODE ) { p = q; continue; }
		if ( ch == MSDF_CHAR_NEWLINE )   { p = q; continue; }

		advance = MSDF_GlyphAdvancePx( font, ch, pixelSize, letterSpacing );
		if ( curWidth + advance > maxPixels ) break;
		curWidth += advance;
		counted++;
		p = q;   /* advance past the full (UTF-8) sequence */
	}

	if ( totalWidthOut ) *totalWidthOut = curWidth;
	return counted;
}

#endif /* FEAT_WIRED_UI || WIRED_WEB_UI_TEXT */
