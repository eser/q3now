/*
===========================================================================
cl_wired_fonts.h — Wired UI font system API

Provides proportional font rendering with 11 fonts, color codes, shadows,
and alignment. Migrated from cg_osptext.c to run in the client directly.
===========================================================================
*/

#ifndef CL_WIRED_FONTS_H
#define CL_WIRED_FONTS_H

#include "../../qcommon/q_shared.h"
#include "../../game/q_feats.h"

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
// These are the migrated CG_OSPDrawString* functions, renamed for clarity.
// The original cgame functions still exist for backward compat until full migration.

// full-featured: proportional fonts, shadows, color codes, borders, backgrounds
void    CG_OSPDrawStringNew( float x, float y, const char *str,
            const vec4_t color, vec4_t shadowColor,
            float charW, float charH, int maxWidth, int flags,
            vec4_t bgColor, vec4_t border, vec4_t borderColor );

// simpler variant without shadow/border
void    CG_OSPDrawString( float x, float y, const char *str,
            const vec4_t color, float charW, float charH,
            int maxWidth, int flags, vec4_t bgColor );

// measure text width in pixels
int     CG_OSPDrawStringLenPix( const char *str, float charW, int flags, int toWidth );

// hex color parsing
qboolean CG_Hex16GetColor( const char *str, float *color );

// text command compiler (used internally and by SuperHUD elements)
// note: text_command_t is defined in cl_wired_fonts.c — opaque to callers

// font loading (internal, called from WiredFont_Init)
void CG_LoadFonts( void );
void CG_FontSelect( int index );
int  CG_FontIndexFromName( const char *name );

// convenience aliases
#define WiredFont_DrawString     CG_OSPDrawStringNew
#define WiredFont_DrawSimple     CG_OSPDrawString
#define WiredFont_StringWidth    CG_OSPDrawStringLenPix

#endif // FEAT_WIRED_UI
#endif // CL_WIRED_FONTS_H
