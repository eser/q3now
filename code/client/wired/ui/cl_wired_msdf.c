/*
===========================================================================
cl_wired_msdf.c -- MSDF font loading and rendering

Loads msdf-atlas-gen JSON metrics + atlas PNG for multi-channel signed
distance field fonts.  Renders via the standard Q3 renderer interface
(re.SetColor / re.DrawStretchPic).

Coordinates are in real screen pixels and are submitted directly
to the renderer without virtual-to-real conversion.

The MSDF fragment shader (task 2) replaces the default shader later.
Until then, the atlas renders with standard bilinear filtering -- UVs
and geometry will be correct, but glyph edges will appear blurred.
===========================================================================
*/

#include "../../client.h"
#include "cl_wired_msdf.h"

#if FEAT_WIRED_UI

/* ── font pool ──────────────────────────────────────────────────────── */

static msdfFont_t	msdfFonts[MAX_MSDF_FONTS];
static int			msdfFontCount = 0;

/* ── outline / glow state ──────────────────────────────────────────── */

static float msdf_outlineWidth = 0.0f;
static float msdf_outlineColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static float msdf_glowWidth = 0.0f;
static float msdf_glowColor[4] = { 1.0f, 1.0f, 1.0f, 0.3f };

void MSDF_SetOutline( float outlineWidth, const float *outlineColor,
                       float glowWidth, const float *glowColor )
{
	msdf_outlineWidth = outlineWidth;
	if ( outlineColor )
		Com_Memcpy( msdf_outlineColor, outlineColor, sizeof( msdf_outlineColor ) );
	msdf_glowWidth = glowWidth;
	if ( glowColor )
		Com_Memcpy( msdf_glowColor, glowColor, sizeof( msdf_glowColor ) );
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
	Com_Printf( S_COLOR_YELLOW "MSDF JSON: unexpected char '%c'\n", *jp->p );
	jp->p++;
	jp->type = JTOK_NONE;
}

/*
 * JSON_SkipValue -- skip a complete JSON value (object, array, or primitive).
 * Used to skip over keys/values we don't care about.
 */
static void JSON_SkipValue( jsonParser_t *jp )
{
	int depth;

	if ( jp->type == JTOK_LBRACE ) {
		depth = 1;
		while ( depth > 0 ) {
			JSON_NextToken( jp );
			if ( jp->type == JTOK_LBRACE )    depth++;
			else if ( jp->type == JTOK_RBRACE ) depth--;
			else if ( jp->type == JTOK_EOF )   return;
		}
		return;
	}

	if ( jp->type == JTOK_LBRACKET ) {
		depth = 1;
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

/* ── JSON glyph/atlas parsing ───────────────────────────────────────── */

/*
 * Parse a bounds object: { "left": N, "bottom": N, "right": N, "top": N }
 * Assumes the opening '{' has already been consumed.
 */
static qboolean MSDF_ParseBounds( jsonParser_t *jp,
                                  float *left, float *bottom,
                                  float *right, float *top )
{
	*left = *bottom = *right = *top = 0.0f;

	while ( jp->type != JTOK_RBRACE && jp->type != JTOK_EOF ) {
		JSON_NextToken( jp );

		if ( jp->type == JTOK_RBRACE ) {
			break;
		}

		if ( jp->type == JTOK_STRING ) {
			char key[64];
			Q_strncpyz( key, jp->str, sizeof(key) );

			if ( !JSON_Expect( jp, JTOK_COLON ) ) return qfalse;
			JSON_NextToken( jp );   /* value */

			if ( Q_stricmp( key, "left" ) == 0 )        *left   = jp->num;
			else if ( Q_stricmp( key, "bottom" ) == 0 )  *bottom = jp->num;
			else if ( Q_stricmp( key, "right" ) == 0 )   *right  = jp->num;
			else if ( Q_stricmp( key, "top" ) == 0 )     *top    = jp->num;
		}
		/* skip commas */
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
				} else {
					JSON_SkipValue( jp );
				}
			} else if ( Q_stricmp( key, "atlasBounds" ) == 0 ) {
				if ( jp->type == JTOK_LBRACE ) {
					MSDF_ParseBounds( jp, &aL, &aB, &aR, &aT );
					hasAtlasBounds = qtrue;
					JSON_NextToken( jp );
					if ( jp->type == JTOK_RBRACE ) break;
					continue;
				} else {
					JSON_SkipValue( jp );
				}
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
	while ( jp->type != JTOK_RBRACE && jp->type != JTOK_EOF ) {
		JSON_NextToken( jp );

		if ( jp->type == JTOK_RBRACE ) {
			break;
		}

		if ( jp->type == JTOK_STRING ) {
			char key[64];
			Q_strncpyz( key, jp->str, sizeof(key) );

			if ( !JSON_Expect( jp, JTOK_COLON ) ) return qfalse;
			JSON_NextToken( jp );   /* value */

			if ( Q_stricmp( key, "width" ) == 0 )              font->atlasWidth     = (int)jp->num;
			else if ( Q_stricmp( key, "height" ) == 0 )        font->atlasHeight    = (int)jp->num;
			else if ( Q_stricmp( key, "size" ) == 0 )          font->atlasSize      = jp->num;
			else if ( Q_stricmp( key, "distanceRange" ) == 0 ) font->distanceRange  = jp->num;
			else                                                JSON_SkipValue( jp );
		}
		/* skip commas */
	}
	return qtrue;
}

/*
 * Parse "metrics" object: ascender, descender, lineHeight.
 */
static qboolean MSDF_ParseMetricsObject( jsonParser_t *jp, msdfFont_t *font )
{
	while ( jp->type != JTOK_RBRACE && jp->type != JTOK_EOF ) {
		JSON_NextToken( jp );
		if ( jp->type == JTOK_RBRACE ) break;

		if ( jp->type == JTOK_STRING ) {
			char key[64];
			Q_strncpyz( key, jp->str, sizeof(key) );
			if ( !JSON_Expect( jp, JTOK_COLON ) ) return qfalse;
			JSON_NextToken( jp );

			if ( Q_stricmp( key, "ascender" ) == 0 )        font->ascender   = jp->num;
			else if ( Q_stricmp( key, "descender" ) == 0 )   font->descender  = jp->num;
			else if ( Q_stricmp( key, "lineHeight" ) == 0 )  font->lineHeight = jp->num;
			else                                              JSON_SkipValue( jp );
		}
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

	Com_Memset( &jp, 0, sizeof(jp) );
	jp.p = text;

	/* expect opening '{' */
	JSON_NextToken( &jp );
	if ( jp.type != JTOK_LBRACE ) {
		Com_Printf( S_COLOR_RED "MSDF_ParseJSON: expected '{' at start\n" );
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
		else if ( font->glyphs[mid].unicode < unicode )
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

/* ── font loading ───────────────────────────────────────────────────── */

msdfFont_t *MSDF_LoadFont( const char *fontName )
{
	msdfFont_t  *font;
	char         jsonPath[MAX_QPATH];
	char         shaderPath[MAX_QPATH];
	void        *buf;
	int          len;
	int          i;

	if ( !fontName || !fontName[0] ) {
		Com_Printf( S_COLOR_RED "MSDF_LoadFont: NULL font name\n" );
		return NULL;
	}

	/* check if already loaded */
	for ( i = 0; i < msdfFontCount; i++ ) {
		if ( msdfFonts[i].loaded && Q_stricmp( msdfFonts[i].name, fontName ) == 0 ) {
			return &msdfFonts[i];
		}
	}

	/* find a free slot */
	if ( msdfFontCount >= MAX_MSDF_FONTS ) {
		Com_Printf( S_COLOR_RED "MSDF_LoadFont: too many fonts (max %d)\n", MAX_MSDF_FONTS );
		return NULL;
	}
	font = &msdfFonts[msdfFontCount];

	/* zero out the struct */
	Com_Memset( font, 0, sizeof(*font) );
	Q_strncpyz( font->name, fontName, sizeof(font->name) );

	/* load JSON metrics */
	Com_sprintf( jsonPath, sizeof(jsonPath), "fonts/%s.json", fontName );
	len = FS_ReadFile( jsonPath, &buf );
	if ( len <= 0 || !buf ) {
		Com_Printf( S_COLOR_YELLOW "MSDF_LoadFont: could not read '%s'\n", jsonPath );
		return NULL;
	}

	if ( !MSDF_ParseJSON( (const char *)buf, font ) ) {
		Com_Printf( S_COLOR_RED "MSDF_LoadFont: parse error in '%s'\n", jsonPath );
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
		Com_Printf( S_COLOR_RED "MSDF_LoadFont: invalid atlas dimensions in '%s'\n", jsonPath );
		return NULL;
	}

	/* Register atlas as an MSDF shader — the renderer will use its MSDF
	 * fragment program (median-of-three + smoothstep) for antialiased text. */
	Com_sprintf( shaderPath, sizeof(shaderPath), "fonts/%s_atlas", fontName );
	font->atlasShader = re.RegisterMSDFShader( shaderPath,
		font->distanceRange, font->atlasWidth, font->atlasHeight );
	if ( font->atlasShader == 0 ) {
		Com_Printf( S_COLOR_YELLOW "MSDF_LoadFont: could not register atlas shader '%s'\n", shaderPath );
		/* not fatal -- font can still be used for measurement */
	}

	font->loaded = qtrue;
	msdfFontCount++;

	Com_Printf( "MSDF_LoadFont: loaded '%s' (%dx%d atlas, %.0f px, %d glyphs)\n",
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
	int i;
	char shaderPath[MAX_QPATH];

	for ( i = 0; i < msdfFontCount; i++ ) {
		msdfFont_t *f = &msdfFonts[i];
		if ( !f->loaded || f->name[0] == '\0' ) continue;

		Com_sprintf( shaderPath, sizeof(shaderPath), "fonts/%s_atlas", f->name );
		f->atlasShader = re.RegisterMSDFShader( shaderPath,
			f->distanceRange, f->atlasWidth, f->atlasHeight );

		if ( f->atlasShader == 0 ) {
			Com_Printf( S_COLOR_YELLOW "MSDF_ReregisterShaders: failed for '%s'\n", f->name );
		}
	}

	if ( msdfFontCount > 0 ) {
		Com_DPrintf( "MSDF_ReregisterShaders: re-registered %d font(s)\n", msdfFontCount );
	}
}

/* ── point-to-pixel conversion ──────────────────────────────────────── */
/*
 * Convert point size to screen pixels.
 *
 * Point sizes come from the parser shim (textscale * 48) or from native
 * .wmenu files (direct point values like 12, 14, 22).
 *
 * Coordinates are now in real screen pixels directly.
 */
static float MSDF_PointToPixels( float pointSize )
{
	return pointSize;
}

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

	pixelSize = MSDF_PointToPixels( size );  /* 1 em = pixelSize virtual pixels */

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
	re.SetMSDFOutline( msdf_outlineWidth, msdf_outlineColor,
	                    msdf_glowWidth, msdf_glowColor );

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

/* ── string drawing ─────────────────────────────────────────────────── */

void MSDF_DrawString( msdfFont_t *font, float x, float y,
                      float size, const float *color,
                      const char *str, int maxChars, float letterSpacing,
                      qboolean forceColor )
{
	float       curColor[4];
	float       pixelSize;
	float       curX;
	int         drawn;
	const char *p;

	if ( !font || !font->loaded || !str || !str[0] ) return;

	/* copy the initial color so we can modify RGB via color codes */
	curColor[0] = color ? color[0] : 1.0f;
	curColor[1] = color ? color[1] : 1.0f;
	curColor[2] = color ? color[2] : 1.0f;
	curColor[3] = color ? color[3] : 1.0f;

	pixelSize = MSDF_PointToPixels( size );
	curX = x;
	drawn = 0;

	for ( p = str; *p; ) {
		int skip;

		/* check character limit */
		if ( maxChars >= 0 && drawn >= maxChars ) {
			break;
		}

		/* handle newline — advance to next line */
		if ( *p == '\n' ) {
			curX = x;
			y += font->lineHeight * pixelSize;
			p++;
			continue;
		}

		/* handle ^^ (literal caret) */
		if ( p[0] == Q_COLOR_ESCAPE && p[1] == Q_COLOR_ESCAPE ) {
			msdfGlyph_t *caretGlyph;
			MSDF_DrawChar( font, curX, y, size, curColor, Q_COLOR_ESCAPE );
			caretGlyph = MSDF_FindGlyph( font, Q_COLOR_ESCAPE );
			curX += ( caretGlyph ? caretGlyph->advance : 0.5f ) * pixelSize + letterSpacing;
			drawn++;
			p += 2;
			continue;
		}

		/* handle color codes (^0 - ^9, ^a-^z, etc.) */
		skip = MSDF_HandleColorCode( p, forceColor ? NULL : curColor );
		if ( skip > 0 ) {
			p += skip;
			continue;   /* color codes don't count as drawn chars */
		}

		/* regular character */
		{
			msdfGlyph_t *charGlyph;
			MSDF_DrawChar( font, curX, y, size, curColor, (unsigned char)*p );

			/* advance cursor */
			charGlyph = MSDF_FindGlyph( font, (unsigned char)*p );
			if ( charGlyph ) {
				curX += charGlyph->advance * pixelSize + letterSpacing;
			} else {
				/* fallback: advance by half an em for unknown glyphs */
				curX += 0.5f * pixelSize + letterSpacing;
			}
		}

		drawn++;
		p++;
	}

	/* reset color to avoid bleeding into subsequent draws */
	re.SetColor( NULL );
}

/* ── string measurement ─────────────────────────────────────────────── */

float MSDF_MeasureString( msdfFont_t *font, float size,
                          const char *str, int maxChars, float letterSpacing )
{
	float       pixelSize;
	float       lineWidth;   /* accumulator for the current line */
	float       maxWidth;    /* maximum width seen across all lines */
	int         counted;
	const char *p;

	if ( !font || !font->loaded || !str || !str[0] ) return 0.0f;

	pixelSize = MSDF_PointToPixels( size );
	lineWidth = 0.0f;
	maxWidth  = 0.0f;
	counted   = 0;

	for ( p = str; *p; ) {
		int skip;

		if ( maxChars >= 0 && counted >= maxChars ) {
			break;
		}

		/* newline: \n itself is zero-width; commit current line and reset */
		if ( *p == '\n' ) {
			if ( lineWidth > maxWidth ) maxWidth = lineWidth;
			lineWidth = 0.0f;
			p++;
			continue;
		}

		/* ^^ literal caret */
		if ( p[0] == Q_COLOR_ESCAPE && p[1] == Q_COLOR_ESCAPE ) {
			msdfGlyph_t *caretGlyph = MSDF_FindGlyph( font, Q_COLOR_ESCAPE );
			if ( caretGlyph ) {
				lineWidth += caretGlyph->advance * pixelSize + letterSpacing;
			} else {
				lineWidth += 0.5f * pixelSize + letterSpacing;
			}
			counted++;
			p += 2;
			continue;
		}

		/* color codes are invisible */
		skip = MSDF_HandleColorCode( p, NULL );
		if ( skip > 0 ) {
			p += skip;
			continue;
		}

		/* regular character */
		{
			msdfGlyph_t *charGlyph = MSDF_FindGlyph( font, (unsigned char)*p );
			if ( charGlyph ) {
				lineWidth += charGlyph->advance * pixelSize + letterSpacing;
			} else {
				lineWidth += 0.5f * pixelSize + letterSpacing;
			}
		}

		counted++;
		p++;
	}

	/* commit the final line (no trailing \n required) */
	if ( lineWidth > maxWidth ) maxWidth = lineWidth;

	return maxWidth;
}

#endif /* FEAT_WIRED_UI */
