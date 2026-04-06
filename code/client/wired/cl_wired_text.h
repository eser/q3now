/*
===========================================================================
cl_wired_text.h — Unified text rendering API

Single entry point for all text drawing in the engine. Wraps the MSDF
font system. No bitmap font fallbacks — MSDF is the only path.

Callable from anywhere in the client. Owned by Wired UI.

Font IDs, alignment, and flag constants are defined in cg_public.h
(shared between client and cgame).
===========================================================================
*/

#ifndef CL_WIRED_TEXT_H
#define CL_WIRED_TEXT_H

#include "../../qcommon/q_shared.h"
#include "../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

/* Font IDs, alignment, and flags are in cg_public.h:
 *   FONT_DISPLAY, FONT_DISPLAY_ITALIC, FONT_UI, FONT_UI_MEDIUM, FONT_MONO
 *   TEXT_ALIGN_LEFT, TEXT_ALIGN_CENTER, TEXT_ALIGN_RIGHT
 *   TEXT_DROPSHADOW, TEXT_FORCECOLOR
 */

/* ── init ──────────────────────────────────────────────────────────── */

/* Bootstrap MSDF fonts.  Call from CL_InitRenderer(). */
void  Text_Init( void );

/* ── core API ──────────────────────────────────────────────────────── */

void  Text_Draw( const char *text, float x, float y, int fontId,
                 float size, const vec4_t color, int alignment, int flags );

float Text_Measure( const char *text, int fontId, float size );

void  Text_DrawChar( int ch, float x, float y, int fontId,
                     float size, const vec4_t color );

/* ── letter spacing ───────────────────────────────────────────────── */

/* Set extra pixels between glyphs for subsequent Text_Draw / Text_Measure calls.
 * Default is 0.0. Reset after use. */
void  Text_SetLetterSpacing( float spacing );
float Text_GetLetterSpacing( void );

#endif /* FEAT_WIRED_UI */
#endif /* CL_WIRED_TEXT_H */
