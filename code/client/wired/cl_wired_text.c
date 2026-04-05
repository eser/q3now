/*
===========================================================================
cl_wired_text.c — Unified text rendering implementation

Maps fontId to loaded MSDF font handles and delegates to
MSDF_DrawString / MSDF_DrawChar / MSDF_MeasureString.

Alignment and drop shadow are handled here so callers don't need to.
===========================================================================
*/

#include "../client.h"
#include "cl_wired_text.h"
#include "cl_wired_msdf.h"
#include "cl_wired_fonts.h"

#if FEAT_WIRED_UI

/* ── Text_Init ─────────────────────────────────────────────────────── */
/*
 * Bootstrap the MSDF font subsystem.  Called from WiredUI_Init()
 * so fonts are ready before HUD init.  Also handles vid_restart
 * re-registration.
 */
void Text_Init( void )
{
	MSDF_ReregisterShaders();
	WiredFonts_InitMSDF();
}

/* ── font handle resolution ────────────────────────────────────────── */

static msdfFont_t *Text_GetFont( int fontId )
{
	switch ( fontId ) {
	case FONT_DISPLAY:        return WiredFonts_GetMSDF( "sansman" );
	case FONT_DISPLAY_ITALIC: return WiredFonts_GetMSDF( "sansman-italic" );
	case FONT_UI:             return WiredFonts_GetMSDF( "oxanium" );
	case FONT_UI_MEDIUM:      return WiredFonts_GetMSDF( "oxanium-medium" );
	case FONT_MONO:           return WiredFonts_GetMSDF( "sharetechmono" );
	default:                  return WiredFonts_GetMSDF( "oxanium" );
	}
}

/* ── Text_Draw ─────────────────────────────────────────────────────── */

void Text_Draw( const char *text, float x, float y, int fontId,
                float size, const vec4_t color, int alignment, int flags )
{
	msdfFont_t *font;
	float drawX, drawY;

	if ( !text || !text[0] ) return;

	font = Text_GetFont( fontId );
	if ( !font ) return;

	drawX = x;
	drawY = y;

	/* alignment */
	if ( alignment == TEXT_ALIGN_CENTER ) {
		drawX -= MSDF_MeasureString( font, size, text, -1 ) * 0.5f;
	} else if ( alignment == TEXT_ALIGN_RIGHT ) {
		drawX -= MSDF_MeasureString( font, size, text, -1 );
	}

	/* drop shadow */
	if ( flags & TEXT_DROPSHADOW ) {
		vec4_t shadowColor = { 0, 0, 0, 0.8f };
		float offset = size * 0.06f;
		if ( offset < 1.0f ) offset = 1.0f;
		if ( color ) {
			shadowColor[3] = color[3] * 0.8f;
		}
		MSDF_DrawString( font, drawX + offset, drawY + offset,
		                 size, shadowColor, text, -1 );
	}

	MSDF_DrawString( font, drawX, drawY, size, color, text, -1 );
}

/* ── Text_Measure ──────────────────────────────────────────────────── */

float Text_Measure( const char *text, int fontId, float size )
{
	msdfFont_t *font;

	if ( !text || !text[0] ) return 0.0f;

	font = Text_GetFont( fontId );
	if ( !font ) return 0.0f;

	return MSDF_MeasureString( font, size, text, -1 );
}

/* ── Text_DrawChar ─────────────────────────────────────────────────── */

void Text_DrawChar( int ch, float x, float y, int fontId,
                    float size, const vec4_t color )
{
	msdfFont_t *font;

	if ( ch <= 0 ) return;

	font = Text_GetFont( fontId );
	if ( !font ) return;

	MSDF_DrawChar( font, x, y, size, color, ch );
}

#endif /* FEAT_WIRED_UI */
