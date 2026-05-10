/*
cl_wired_msdf.c -- MSDF font loading and rendering
*/

#include "../../client.h"
#include "cl_wired_msdf.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

/* ── font pool ──────────────────────────────────────────────────────── */

static msdfFont_t	wui_fonts[MAX_MSDF_FONTS];
static int			wui_fontCount = 0;

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
	int         maxChars;
	float       result;
	int         frame;
} msdf_meas_entry_t;

static msdf_meas_entry_t wui_meas_cache[MSDF_MEAS_CACHE_SIZE];
static int                wui_meas_cache_idx = 0;

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

	for ( int i = 0; i < wui_fontCount; i++ ) {
		msdfFont_t *f = &wui_fonts[i];
		if ( !f->loaded || f->name[0] == '\0' ) continue;

		Com_sprintf( shaderPath, sizeof(shaderPath), "fonts/%s_atlas", f->name );
		f->atlasShader = re.RegisterMSDFShader( shaderPath,
			f->distanceRange, f->atlasWidth, f->atlasHeight );

		if ( f->atlasShader == 0 ) {
			COM_WARN( LOG_CH(ch_ui), "MSDF_ReregisterShaders: failed for '%s'\n", f->name );
		}
	}

	if ( wui_fontCount > 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui), "MSDF_ReregisterShaders: re-registered %d font(s)\n", wui_fontCount );
	}
}

int MSDF_GetFontCount( void ) { return wui_fontCount; }

/* ── character drawing ──────────────────────────────────────────────── */

void MSDF_DrawChar( msdfFont_t *font, float x, float y,
                    float size, const float *color, int ch )
{
	msdfGlyph_t *g;
	float        pixelSize;
	float        s0, t0, s1, t1;
	float        xOff, yOff, w, h;
	float        drawX, drawY, drawW, drawH;

	if ( !font || !font->loaded ) return;
	if ( ch < 0 ) return;

	g = MSDF_FindGlyph( font, ch );



	if ( !g ) return;

	/* nothing to draw if there are no atlas bounds (e.g. space character) */
	if ( g->atlasRight <= g->atlasLeft || g->atlasTop <= g->atlasBottom ) {
		return;
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
	msdfGlyph_t *g = MSDF_FindGlyph( font, ch );
	return ( g ? g->advance : 0.5f ) * pixelSize + letterSpacing;
}

/* ── character iterator ─────────────────────────────────────────────── */

#define MSDF_CHAR_COLORCODE (-1)  /* color code consumed */
#define MSDF_CHAR_NEWLINE   (-2)  /* newline consumed    */

/* Consume one escape sequence or character from *pp and return a codepoint.
   colorOut: if non-NULL, color codes update it; if NULL, they are silently skipped.
   Returns: character codepoint (>0), MSDF_CHAR_COLORCODE, MSDF_CHAR_NEWLINE, or 0 (end). */
static int MSDF_NextRenderableChar( const char **pp, float *colorOut )
{
	const char *p = *pp;
	int skip;
	if ( !*p ) return 0;
	if ( *p == '\n' ) { *pp = p + 1; return MSDF_CHAR_NEWLINE; }
	if ( p[0] == Q_COLOR_ESCAPE && p[1] == Q_COLOR_ESCAPE ) { *pp = p + 2; return Q_COLOR_ESCAPE; }
	skip = MSDF_HandleColorCode( p, colorOut );
	if ( skip > 0 ) { *pp = p + skip; return MSDF_CHAR_COLORCODE; }
	*pp = p + 1;
	return (unsigned char)*p;
}

/* ── string drawing ─────────────────────────────────────────────────── */

void MSDF_DrawString( msdfFont_t *font, float x, float y,
                      float size, const float *color,
                      const char *str, int maxChars, float letterSpacing,
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

/* ── string measurement ─────────────────────────────────────────────── */

float MSDF_MeasureString( msdfFont_t *font, float size,
                          const char *str, int maxChars, float letterSpacing )
{
	if ( !font || !font->loaded || !str || !str[0] ) return 0.0f;

	/* per-frame cache: avoid re-iterating the same string within one frame */
	for ( int i = 0; i < MSDF_MEAS_CACHE_SIZE; i++ ) {
		if ( wui_meas_cache[i].frame == cls.realtime &&
		     wui_meas_cache[i].str == str &&
		     wui_meas_cache[i].font == font &&
		     wui_meas_cache[i].size == size &&
		     wui_meas_cache[i].letterSpacing == letterSpacing &&
		     wui_meas_cache[i].maxChars == maxChars ) {
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
		int idx = wui_meas_cache_idx % MSDF_MEAS_CACHE_SIZE;
		wui_meas_cache[idx].str          = str;
		wui_meas_cache[idx].font         = font;
		wui_meas_cache[idx].size         = size;
		wui_meas_cache[idx].letterSpacing = letterSpacing;
		wui_meas_cache[idx].maxChars     = maxChars;
		wui_meas_cache[idx].result       = maxWidth;
		wui_meas_cache[idx].frame        = cls.realtime;
		wui_meas_cache_idx++;
	}

	return maxWidth;
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
		float advance;
		int   skip;

		if ( p[0] == Q_COLOR_ESCAPE && p[1] == Q_COLOR_ESCAPE ) {
			advance = MSDF_GlyphAdvancePx( font, Q_COLOR_ESCAPE, pixelSize, letterSpacing );
			if ( curWidth + advance > maxPixels ) break;
			curWidth += advance;
			counted++;
			p += 2;
			continue;
		}

		skip = MSDF_HandleColorCode( p, NULL );
		if ( skip > 0 ) { p += skip; continue; }

		advance = MSDF_GlyphAdvancePx( font, (unsigned char)*p, pixelSize, letterSpacing );
		if ( curWidth + advance > maxPixels ) break;
		curWidth += advance;
		counted++;
		p++;
	}

	if ( totalWidthOut ) *totalWidthOut = curWidth;
	return counted;
}

#endif /* FEAT_WIRED_UI */
