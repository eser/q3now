/*
===========================================================================
cl_wired_theme.h — Wired UI Theme: semantic state to color mapping

Phase 4: Maps state labels ("critical", "warning", "normal") to RGBA colors.
cgame writes state labels, UI resolves them to colors via the active theme.
===========================================================================
*/

#ifndef CL_WIRED_THEME_H
#define CL_WIRED_THEME_H

#include "../../qcommon/q_shared.h"

#if FEAT_WIRED_UI

/* ── maximum theme entries ─────────────────────────────────────────── */

#define WUI_THEME_MAX_STATES    32

/* ── theme entry ───────────────────────────────────────────────────── */

typedef struct {
    char        name[32];       /* state label (e.g. "critical", "warning") */
    vec4_t      color;          /* RGBA color for this state */
} wuiThemeEntry_t;

/* ── theme container ───────────────────────────────────────────────── */

typedef struct {
    char              name[64];                          /* theme name */
    wuiThemeEntry_t   states[WUI_THEME_MAX_STATES];     /* state-to-color mappings */
    int               numStates;                         /* active entry count */
} wuiTheme_t;

/* ── public API ────────────────────────────────────────────────────── */

void        WiredTheme_Init( void );
void        WiredTheme_Shutdown( void );

/* Look up a state label in the active theme. Returns qtrue and copies color
   if found, qfalse if the state label has no theme mapping. */
qboolean    WiredTheme_ResolveState( const char *state, vec4_t colorOut );

/* Console command: list all state-to-color mappings in the active theme */
/* Registered as "wui_theme_list" */

/* Console command: set a state color at runtime */
/* wui_theme_set <state> <r> <g> <b> <a> */

#endif /* FEAT_WIRED_UI */

#endif /* CL_WIRED_THEME_H */
