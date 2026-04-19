/*
===========================================================================
cl_wired_text.c — Unified text rendering implementation

Maps fontId to loaded MSDF font handles and delegates to
MSDF_DrawString / MSDF_DrawChar / MSDF_MeasureString.

Alignment and drop shadow are handled here so callers don't need to.
===========================================================================
*/

#include "../../client.h"
#include "cl_wired_text.h"
#include "cl_wired_ui.h"
#include "cl_wired_msdf.h"
#include "cl_wired_fonts.h"

#if FEAT_WIRED_UI

/* ── letter spacing state ──────────────────────────────────────────── */

static float text_letterSpacing = 0.0f;

static cvar_t *cl_wiredTextShadow = NULL;

void Text_SetLetterSpacing( float spacing )
{
	text_letterSpacing = spacing;
}

float Text_GetLetterSpacing( void )
{
	return text_letterSpacing;
}

/* ── Text_Init ─────────────────────────────────────────────────────── */
/*
 * Bootstrap the MSDF font subsystem.  Called from WiredUI_Init()
 * so fonts are ready before HUD init.  Also handles vid_restart
 * re-registration.
 */
void Text_Init( void )
{
	cl_wiredTextShadow = Cvar_Get( "cl_wiredTextShadow", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_wiredTextShadow, "Enable drop-shadow on Wired UI text. 0: off (default). 1: on." );
	MSDF_ReregisterShaders();
	WiredFonts_InitMSDF();
}

/* ── font face resolution (family-based) ──────────────────────────── */

static const fontFace_t *Text_ResolveFace( int fontId )
{
	wiredAssetGlobals_t *ag = WiredUI_GetAssetGlobals();
	const char *serif = ag->defaultSerifFontName[0] ? ag->defaultSerifFontName : "sansman";
	const char *serifItalic = ag->defaultSerifFontItalicName[0] ? ag->defaultSerifFontItalicName : "sansman-italic";
	const char *sans = ag->defaultSansFontName[0] ? ag->defaultSansFontName : "oxanium";
	const char *sansMedium = ag->defaultSansFontMediumName[0] ? ag->defaultSansFontMediumName : "oxanium-medium";
	const char *mono = ag->defaultMonoFontName[0] ? ag->defaultMonoFontName : "sharetechmono";
	const fontFace_t *face = NULL;

	switch ( fontId ) {
	case FONT_DISPLAY:
		face = WiredFont_ResolveByName( serif );
		if ( !face ) face = WiredFont_Resolve( "sansman", FONT_WEIGHT_REGULAR, FONT_STYLE_NORMAL );
		return face;
	case FONT_DISPLAY_ITALIC:
		face = WiredFont_ResolveByName( serifItalic );
		if ( !face ) face = WiredFont_Resolve( "sansman", FONT_WEIGHT_REGULAR, FONT_STYLE_ITALIC );
		return face;
	case FONT_DISPLAY_BOLD:
		face = WiredFont_ResolveByName( serif );
		if ( !face ) face = WiredFont_Resolve( "sansman", FONT_WEIGHT_BOLD, FONT_STYLE_NORMAL );
		return face;
	case FONT_UI:
		face = WiredFont_ResolveByName( sans );
		if ( !face ) face = WiredFont_Resolve( "oxanium", FONT_WEIGHT_REGULAR, FONT_STYLE_NORMAL );
		return face;
	case FONT_UI_MEDIUM:
		face = WiredFont_ResolveByName( sansMedium );
		if ( !face ) face = WiredFont_Resolve( "oxanium", FONT_WEIGHT_MEDIUM, FONT_STYLE_NORMAL );
		return face;
	case FONT_MONO:
		face = WiredFont_ResolveByName( mono );
		if ( !face ) face = WiredFont_Resolve( "sharetechmono", FONT_WEIGHT_REGULAR, FONT_STYLE_NORMAL );
		return face;
	default:
		face = WiredFont_ResolveByName( sans );
		if ( !face ) face = WiredFont_Resolve( "oxanium", FONT_WEIGHT_REGULAR, FONT_STYLE_NORMAL );
		return face;
	}
}

/* ── Text_Draw ─────────────────────────────────────────────────────── */

void Text_Draw( const char *text, float x, float y, int fontId,
                float size, const vec4_t color, int alignment, int flags )
{
	const fontFace_t *face;
	float drawX, drawY;
	float ls = text_letterSpacing;

	if ( !text || !text[0] ) return;

	face = Text_ResolveFace( fontId );
	if ( !face || !face->atlas ) return;

	drawX = x;
	drawY = y;

	/* alignment */
	if ( alignment == TEXT_ALIGN_CENTER ) {
		drawX -= MSDF_MeasureString( face->atlas, size, text, -1, ls ) * 0.5f;
	} else if ( alignment == TEXT_ALIGN_RIGHT ) {
		drawX -= MSDF_MeasureString( face->atlas, size, text, -1, ls );
	}

	/* drop shadow: single pass via in-shader offset sample */
	if ( (flags & TEXT_DROPSHADOW) && cl_wiredTextShadow && cl_wiredTextShadow->integer ) {
		vec4_t shadowColor = { 0, 0, 0, 0.8f };
		float offset = size * 0.06f;
		if ( offset < 1.0f ) offset = 1.0f;
		if ( color ) {
			shadowColor[3] = color[3] * 0.8f;
		}
		re.SetMSDFShadow( offset, offset, shadowColor );
	}

	MSDF_DrawString( face->atlas, drawX, drawY, size, color, text, -1, ls,
	                 ( flags & TEXT_FORCECOLOR ) ? qtrue : qfalse );

	if ( (flags & TEXT_DROPSHADOW) && cl_wiredTextShadow && cl_wiredTextShadow->integer ) {
		re.SetMSDFShadow( 0.0f, 0.0f, NULL );
	}
}

/* ── Text_DrawClipped ──────────────────────────────────────────────── */

void Text_DrawClipped( const char *text, float x, float y, float maxWidth,
                       int fontId, float size,
                       const vec4_t color, int alignment, int flags )
{
	const fontFace_t *face;
	float             ls, fullWidth, drawX;
	qboolean          shadow, forceColor;

	if ( !text || !text[0] ) return;

	face = Text_ResolveFace( fontId );
	if ( !face || !face->atlas ) return;

	ls        = text_letterSpacing;
	fullWidth = MSDF_MeasureString( face->atlas, size, text, -1, ls );

	if ( maxWidth <= 0.0f || fullWidth <= maxWidth ) {
		/* fits — anchor against full width, draw in one pass */
		drawX = x;
		if      ( alignment == TEXT_ALIGN_CENTER ) drawX -= fullWidth * 0.5f;
		else if ( alignment == TEXT_ALIGN_RIGHT  ) drawX -= fullWidth;

		shadow    = ( flags & TEXT_DROPSHADOW ) && cl_wiredTextShadow && cl_wiredTextShadow->integer;
		forceColor = ( flags & TEXT_FORCECOLOR ) ? qtrue : qfalse;

		if ( shadow ) {
			vec4_t sc    = { 0, 0, 0, color ? color[3] * 0.8f : 0.8f };
			float  off   = size * 0.06f < 1.0f ? 1.0f : size * 0.06f;
			re.SetMSDFShadow( off, off, sc );
		}
		MSDF_DrawString( face->atlas, drawX, y, size, color, text, -1, ls, forceColor );
		if ( shadow ) re.SetMSDFShadow( 0.0f, 0.0f, NULL );
		return;
	}

	/* clamped path — glyph-accurate prefix + ".." */
	{
		float ellipsisW   = MSDF_MeasureString( face->atlas, size, "..", -1, ls );
		float prefixWidth;
		int   prefixChars = MSDF_ClampToWidth( face->atlas, size, text,
		                                       maxWidth - ellipsisW, ls, &prefixWidth );
		float effectiveW  = prefixWidth + ellipsisW;

		drawX = x;
		if      ( alignment == TEXT_ALIGN_CENTER ) drawX -= effectiveW * 0.5f;
		else if ( alignment == TEXT_ALIGN_RIGHT  ) drawX -= effectiveW;

		shadow     = ( flags & TEXT_DROPSHADOW ) && cl_wiredTextShadow && cl_wiredTextShadow->integer;
		forceColor = ( flags & TEXT_FORCECOLOR ) ? qtrue : qfalse;

		if ( shadow ) {
			vec4_t sc  = { 0, 0, 0, color ? color[3] * 0.8f : 0.8f };
			float  off = size * 0.06f < 1.0f ? 1.0f : size * 0.06f;
			re.SetMSDFShadow( off, off, sc );
		}
		MSDF_DrawString( face->atlas, drawX,                y, size, color, text, prefixChars, ls, forceColor );
		MSDF_DrawString( face->atlas, drawX + prefixWidth,  y, size, color, "..", -1,          ls, forceColor );
		if ( shadow ) re.SetMSDFShadow( 0.0f, 0.0f, NULL );
	}
}

/* ── Text_Measure ──────────────────────────────────────────────────── */

float Text_Measure( const char *text, int fontId, float size )
{
	const fontFace_t *face;

	if ( !text || !text[0] ) return 0.0f;

	face = Text_ResolveFace( fontId );
	if ( !face || !face->atlas ) return 0.0f;

	return MSDF_MeasureString( face->atlas, size, text, -1, text_letterSpacing );
}

/* ── Text_DrawChar ─────────────────────────────────────────────────── */

void Text_DrawChar( int ch, float x, float y, int fontId,
                    float size, const vec4_t color )
{
	const fontFace_t *face;

	if ( ch <= 0 ) return;

	face = Text_ResolveFace( fontId );
	if ( !face || !face->atlas ) return;

	MSDF_DrawChar( face->atlas, x, y, size, color, ch );
}

#endif /* FEAT_WIRED_UI */
