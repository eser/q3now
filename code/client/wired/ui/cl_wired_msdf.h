/*
cl_wired_msdf.h — MSDF font loading and rendering API
*/

#ifndef CL_WIRED_MSDF_H
#define CL_WIRED_MSDF_H

#include "../../../qcommon/q_shared.h"
#include "../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

/* ── limits ─────────────────────────────────────────────────────────── */

#define MAX_MSDF_FONTS          8
#define MAX_MSDF_GLYPH_COUNT    512   /* max glyphs per font (Latin Extended + specials) */

/* ── glyph data (one per codepoint) ─────────────────────────────────── */

typedef struct {
	int         unicode;

	/* horizontal advance in em-space */
	float       advance;

	/* bounding box in em-space (relative to font size) */
	float       planeLeft;
	float       planeBottom;
	float       planeRight;
	float       planeTop;

	/* bounding box in atlas pixels */
	float       atlasLeft;
	float       atlasBottom;
	float       atlasRight;
	float       atlasTop;
} msdfGlyph_t;

/* ── font instance ──────────────────────────────────────────────────── */

typedef struct {
	char            name[MAX_QPATH];
	qboolean        loaded;

	/* atlas texture registered as a shader */
	qhandle_t       atlasShader;

	/* atlas metadata from JSON */
	int             atlasWidth;
	int             atlasHeight;
	float           atlasSize;          /* nominal font size in atlas (px) */
	float           distanceRange;      /* MSDF distance range in atlas px */

	/* font metrics from JSON (em-space, multiply by pixelSize for pixels) */
	float           ascender;           /* height above baseline (e.g. 0.9) */
	float           descender;          /* depth below baseline (e.g. -0.26) */
	float           lineHeight;         /* full line height (e.g. 1.16) */

	/* glyph table: sorted by unicode for binary search */
	msdfGlyph_t     glyphs[MAX_MSDF_GLYPH_COUNT];
	int             glyphCount;         /* number of loaded glyphs */
} msdfFont_t;

/* ── public API ─────────────────────────────────────────────────────── */

/*
 * MSDF_FindGlyph
 *   Binary search for a glyph by unicode codepoint.
 *   Returns pointer to glyph or NULL if not found.
 */
msdfGlyph_t *MSDF_FindGlyph( msdfFont_t *font, int unicode );

/*
 * MSDF_LoadFont
 *   Load font "<fontName>" from:
 *     fonts/<fontName>.json   -- glyph metrics (msdf-atlas-gen output)
 *     fonts/<fontName>_atlas  -- atlas PNG (registered as shader)
 *   Returns pointer to internal font slot, or NULL on failure.
 */
msdfFont_t *MSDF_LoadFont( const char *fontName );

/*
 * MSDF_DrawChar
 *   Draw a single character at (x, y) in real screen pixel coords.
 *   `size` is in points; converted to pixels via screenHeight / 72.
 *   `color` is an RGBA vec4.
 */
void MSDF_DrawChar( msdfFont_t *font, float x, float y,
                    float size, const float *color, int ch );

/*
 * MSDF_DrawString
 *   Draw a null-terminated string with Q3 color code support (^0-^7, ^^).
 *   `maxChars` limits visible characters drawn (-1 = no limit).
 *   `letterSpacing` adds extra pixels between each glyph advance (0.0 = normal).
 *   `forceColor` if qtrue, ignore inline color codes — use `color` for all glyphs.
 *   Coordinates are in real screen pixels.
 */
void MSDF_DrawString( msdfFont_t *font, float x, float y,
                      float size, const float *color,
                      const char *str, int maxChars, float letterSpacing,
                      qboolean forceColor );

/*
 * MSDF_MeasureString
 *   Return the total width in real screen pixels of the string,
 *   skipping Q3 color codes.  `maxChars` limits visible chars counted
 *   (-1 = no limit).  `letterSpacing` matches the draw call spacing.
 */
float MSDF_MeasureString( msdfFont_t *font, float size,
                          const char *str, int maxChars, float letterSpacing );

/*
 * MSDF_ClampToWidth
 *   Walk the string glyph-by-glyph and return the number of visible
 *   (non-color-code) chars that fit within maxPixels.  totalWidthOut
 *   (may be NULL) receives the accumulated pixel advance of those chars.
 *   Caller must separately account for the ellipsis width when maxPixels
 *   is meant to leave room for it.
 */
int MSDF_ClampToWidth( msdfFont_t *font, float size,
                       const char *str, float maxPixels,
                       float letterSpacing, float *totalWidthOut );

/*
 * MSDF_SetOutline
 *   Set outline and glow parameters for subsequent MSDF draws.
 *   outlineWidth/glowWidth are in SDF units (0.0 = disabled).
 *   outlineColor/glowColor are RGBA vec4; NULL leaves current value.
 */
void MSDF_SetOutline( float outlineWidth, const float *outlineColor,
                       float glowWidth, const float *glowColor );

/*
 * MSDF_ReregisterShaders
 *   Re-register all atlas shaders after renderer reinit (vid_restart,
 *   map change). Called from CL_InitRef or equivalent.
 */
void MSDF_ReregisterShaders( void );
int  MSDF_GetFontCount( void );

#endif /* FEAT_WIRED_UI */
#endif /* CL_WIRED_MSDF_H */
