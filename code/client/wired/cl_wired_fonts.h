/*
===========================================================================
cl_wired_fonts.h — Wired UI font system API

Provides proportional font rendering with 11 fonts, color codes, shadows,
and alignment. Migrated from cg_moderntext.c to run in the client directly.
===========================================================================
*/

#ifndef CL_WIRED_FONTS_H
#define CL_WIRED_FONTS_H

#include "../../qcommon/q_shared.h"
#include "../../qcommon/q_feats.h"
#include "cl_wired_msdf.h"

#if FEAT_WIRED_UI

// ── font face / family types ─────────────────────────────────────────

#define MSDF_MAX_FACES_PER_FAMILY 6

typedef enum {
	FONT_WEIGHT_LIGHT      = 300,
	FONT_WEIGHT_REGULAR    = 400,
	FONT_WEIGHT_MEDIUM     = 500,
	FONT_WEIGHT_SEMIBOLD   = 600,
	FONT_WEIGHT_BOLD       = 700,
	FONT_WEIGHT_EXTRABOLD  = 800,
} fontWeight_t;

typedef enum {
	FONT_STYLE_NORMAL,
	FONT_STYLE_ITALIC,
} fontStyle_t;

// A single renderable face: one atlas
typedef struct {
	char            name[64];       // e.g. "sansman-regular"
	char            atlasName[64];  // e.g. "sansman" — references the MSDF atlas
	fontWeight_t    weight;         // semantic weight this face represents
	fontStyle_t     style;          // normal or italic
	// Populated at load time:
	msdfFont_t     *atlas;          // pointer to loaded MSDF atlas (shared across faces)
} fontFace_t;

// A font family: groups multiple faces sharing atlas(es)
typedef struct {
	char            familyName[64]; // e.g. "sansman"
	fontFace_t      faces[MSDF_MAX_FACES_PER_FAMILY];
	int             faceCount;
} fontFamily_t;

// ── draw style flags ──────────────────────────────────────────────────
#ifndef DS_HLEFT
#define DS_HLEFT        0x0000
#define DS_HCENTER      0x0001
#define DS_HRIGHT       0x0002
#define DS_VTOP         0x0000
#define DS_VCENTER      0x0004
#define DS_VBOTTOM      0x0008
#define DS_PROPORTIONAL 0x0010
#define DS_SHADOW       0x0020
#define DS_EMOJI        0x0040
#define DS_FORCE_COLOR  0x0080
#define DS_MAX_WIDTH_IS_CHARS 0x0100
#endif

// ── font init / selection ─────────────────────────────────────────────

void    WiredFont_Init( void );                         // load all fonts from .cfg files
void    WiredFont_Select( int index );                  // switch active font by index
int     WiredFont_IndexFromName( const char *name );    // "sansman" → 2, "elite" → 9, etc.

// ── MSDF font integration ────────────────────────────────────────────
// Load MSDF fonts (sansman, sansman-italic, oxanium, oxanium-medium, sharetechmono).
// Call once after CG_LoadFonts().
void            WiredFonts_InitMSDF( void );

// Look up an MSDF font by name. Returns NULL for unknown/bitmap-only fonts.
msdfFont_t *WiredFonts_GetMSDF( const char *fontName );

// Resolve a font face by family name, weight, and style (CSS-like matching).
const fontFace_t *WiredFont_Resolve(
	const char   *familyName,
	fontWeight_t  weight,
	fontStyle_t   style
);

// hex color parsing
qboolean CG_Hex16GetColor( const char *str, float *color );

// font loading (internal, called from WiredFont_Init)
void CG_LoadFonts( void );
int  CG_FontIndexFromName( const char *name );

// font-index-to-Text_Draw helpers
int  WiredFont_ToFontId( int fontIndex );
int  WiredFont_ToAlignment( int dsFlags );
int  WiredFont_ToTextFlags( int dsFlags );

// ── TA font system (fontInfo_t-based, for v6/TA menu compatibility) ──
//
// These load fonts via re.RegisterFont() using .dat metric files + .tga atlases.
// Three sizes: small (12pt), normal (16pt), big (20pt) — matching TA's assetGlobalDef.

typedef enum {
	TA_FONT_SMALL,     // 12pt — smallFont
	TA_FONT_NORMAL,    // 16pt — font
	TA_FONT_BIG,       // 20pt — bigFont
	TA_FONT_COUNT
} taFontSize_t;

void          WiredUI_LoadTAFonts( void );
fontInfo_t   *WiredUI_GetTAFont( taFontSize_t size );
void          WiredUI_DrawText_TA( float x, float y, float scale, const vec4_t color,
                  const char *text, int limit, int style, fontInfo_t *font );
float         WiredUI_TextWidth_TA( const char *text, float scale, int limit, fontInfo_t *font );
float         WiredUI_TextHeight_TA( const char *text, float scale, int limit, fontInfo_t *font );

#endif // FEAT_WIRED_UI
#endif // CL_WIRED_FONTS_H
