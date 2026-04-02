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

#if FEAT_WIRED_UI

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

// ── text rendering ────────────────────────────────────────────────────
// These are the migrated CG_ModernDrawString* functions, renamed for clarity.
// The original cgame functions still exist for backward compat until full migration.

// full-featured: proportional fonts, shadows, color codes, borders, backgrounds
void    CG_ModernDrawStringNew( float x, float y, const char *str,
            const vec4_t color, vec4_t shadowColor,
            float charW, float charH, int maxWidth, int flags,
            vec4_t bgColor, vec4_t border, vec4_t borderColor );

// simpler variant without shadow/border
void    CG_ModernDrawString( float x, float y, const char *str,
            const vec4_t color, float charW, float charH,
            int maxWidth, int flags, vec4_t bgColor );

// measure text width in pixels
int     CG_ModernDrawStringLenPix( const char *str, float charW, int flags, int toWidth );

// hex color parsing
qboolean CG_Hex16GetColor( const char *str, float *color );

// text command compiler (used internally and by SuperHUD elements)
// note: text_command_t is defined in cl_wired_fonts.c — opaque to callers

// font loading (internal, called from WiredFont_Init)
void CG_LoadFonts( void );
void CG_FontSelect( int index );
int  CG_FontIndexFromName( const char *name );

// convenience aliases
#define WiredFont_DrawString     CG_ModernDrawStringNew
#define WiredFont_DrawSimple     CG_ModernDrawString
#define WiredFont_StringWidth    CG_ModernDrawStringLenPix

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
