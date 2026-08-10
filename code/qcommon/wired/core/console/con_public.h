// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// con_public.h — WiredConsole core public API.
//
// Re-exports the engine-wide Con_* surface that ships with the WCE-tier
// console core (code/qcommon/wired/core/console/con_buffer.c). The functions
// below are presentation-agnostic — buffer state, scroll position, history
// navigation, command dispatch — and are safe to call from headless code
// paths.
//
// Turn 1 (V-01) scope: minimal re-export of the existing public Con_* surface
// that callers outside con_buffer.c already use (Con_Init / Con_Shutdown /
// Con_Toggle / Con_Clear / Con_PageUp / Con_PageDown / Con_Top / Con_Bottom /
// Con_Close / Con_SoftClose / Con_RunConsole / Con_ClearNotify, plus the
// engine-wide print entry CL_ConsolePrint). Turn 3 V-20 will add the
// read-only accessor surface that the elements/console.c UI projection
// needs (Con_GetBuffer / Con_GetLineCount / Con_GetHistory* /
// Con_GetCursor* / Con_IsScrolledBack).
//
// Code outside the console subsystem should include THIS header rather than
// reaching into console_private.h (which remains UI-tier private between
// con_buffer.c and elements/console.c through the transition cycle).

#ifndef WCE_CON_PUBLIC_H
#define WCE_CON_PUBLIC_H

#include "../../../q_shared.h"

/* ── lifecycle ──────────────────────────────────────────────────────── */

void Con_Init    ( void );
void Con_Shutdown( void );

/* ── buffer entry point ─────────────────────────────────────────────── */

/* Engine-wide print sink. Wired-style severity/channel routing happens
 * upstream (Com_Log → log_sink_console.c → CL_ConsolePrint here). The
 * presentation-agnostic name `Con_Print` aliases the existing entry to
 * align with the spec §7.3 surface; signature unchanged.
 *
 * Turn 1: forwards to CL_ConsolePrint (existing). Turn 3 V-20 may rename
 * the implementation in-place once UI extraction lands. */
void CL_ConsolePrint( const char *txt );

/* ── visibility + scroll ────────────────────────────────────────────── */

void Con_ToggleConsole_f( void );
void Con_Close   ( void );
void Con_SoftClose( void );
/* Register the UI presentation half of Con_Close (input-field clear +
 * KEYCATCH_CONSOLE drop). Set by the UI projection in Con_InitProjection();
 * NULL-safe (headless / pre-init core just collapses the buffer view). */
void Con_SetCloseHook( void (*hook)( void ) );
/* Scroll the console toward its destination height. Geometry-injected:
 * the UI passes whether the console key-catcher is active and the real
 * frame time in ms (core reads no Key_* / cls state). */
void Con_RunConsole( qboolean consoleKeyActive, int frameMsec );

void Con_PageUp  ( int lines );
void Con_PageDown( int lines );
void Con_Top     ( void );
void Con_Bottom  ( void );

/* ── notify lines (cleared per-frame) ───────────────────────────────── */

void Con_ClearNotify( void );

/* ── search + mark (selection) ──────────────────────────────────────── */

void     Con_SearchOpen ( void );
void     Con_SearchClose( void );
void     Con_SearchNext ( qboolean forward );
void     Con_SearchChar ( int ch );
qboolean Con_IsSearchActive( void );
int      Con_SearchLine ( void );

void     Con_MarkOpen   ( void );
void     Con_MarkClose  ( void );
qboolean Con_IsMarkActive( void );
qboolean Con_MarkCellIsSelected( int row, int col );

/* Mark-mode cursor operations (the UI tier translates keys → these). All
 * operate on core mark state; the UI never touches mark fields directly (it
 * reads them via Con_GetMarkState). extendSel=qtrue keeps the anchor
 * (Shift-select); qfalse collapses the anchor onto the cursor. toExtreme is
 * the Ctrl modifier for HOME/END (Ctrl+Home → buffer top, Ctrl+End → current
 * line); it is ignored for every other move. */
typedef enum {
	CON_MARK_LEFT, CON_MARK_RIGHT, CON_MARK_UP, CON_MARK_DOWN,
	CON_MARK_HOME, CON_MARK_END, CON_MARK_PGUP, CON_MARK_PGDN
} conMarkMove_t;
void     Con_MarkMove( conMarkMove_t move, qboolean extendSel, qboolean toExtreme );
void     Con_MarkCopyAndClose( void );   /* Con_MarkCopySelection + Con_MarkClose */

/* ── read-only buffer accessors (Turn 3 V-20) ───────────────────────────
 *
 * The UI projection (elements/console.c) reads buffer state ONLY through
 * these accessors — it never touches console_t or the `con` macro. Each
 * accessor copies out scalars / fills a POD view; none expose the internal
 * struct, glconfig, vec4_t colors, or cvars.
 *
 * log_severity_t is defined in wired/core/logging/log.h, pulled in
 * transitively by the q_shared.h include above. */

/* POD view structs — pure data, no vec4_t / cvar / glconfig. */
typedef struct { qboolean active; char pattern[256]; int matchCount; int line; } conSearchView_t;
typedef struct { qboolean active; int startLine, startCol, endLine, endCol; } conMarkView_t;

/* Console view mode. FULL = full-screen (the attract <-> full-console return
 * pair); HALF = upper-portion overlay that leaves the underlying state visible
 * below. The console-key handler picks the mode by the state it is opened from;
 * the renderer reads it to choose the target height. One console core. */
typedef enum {
	CON_VIEW_FULL = 0,
	CON_VIEW_HALF
} conViewMode_t;

void          Con_SetViewMode( conViewMode_t mode );
conViewMode_t Con_GetViewMode( void );

const short* Con_GetBuffer( void );
int   Con_GetLineWidth( void );
int   Con_GetTotalLines( void );
int   Con_GetCurrentLine( void );
int   Con_GetDisplayLine( void );
float Con_GetDisplayFrac( void );
int   Con_GetNotifyTime( int idx );
qboolean Con_GetSearchState( conSearchView_t *out );
qboolean Con_GetMarkState( conMarkView_t *out );
log_severity_t Con_GetLineSeverity( int line );

/* Core reflow (param-injected geometry — the UI computes targetLineWidth
 * and visPage from glconfig + con_scale, core stays glconfig-free). */
void Con_Reflow( int targetLineWidth, int visPage );

/* Severity-carrying print entry (severity-as-data). CL_ConsolePrint stays
 * unchanged for non-log callers (tags SEV_INFO = "no special severity"). */
void Con_PrintSeverity( log_severity_t sev, const char *txt );

#endif /* WCE_CON_PUBLIC_H */
