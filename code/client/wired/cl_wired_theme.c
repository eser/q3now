/*
===========================================================================
cl_wired_theme.c — Wired UI Theme: semantic state to color mapping

Phase 4: Default theme provides colors for standard state labels.
Modders can override colors via console commands or theme files (v2).
===========================================================================
*/

#include "../client.h"
#include "cl_wired_theme.h"

#if FEAT_WIRED_UI

/* ── active theme ──────────────────────────────────────────────────── */

static wuiTheme_t wired_theme;

/* ── default theme colors ──────────────────────────────────────────── */

typedef struct {
    const char  *name;
    float       r, g, b, a;
} wuiDefaultState_t;

static const wuiDefaultState_t wired_defaultStates[] = {
    /* health states */
    { "critical",    1.0f, 0.2f, 0.2f, 1.0f },   /* red — below 25% */
    { "warning",     1.0f, 0.8f, 0.0f, 1.0f },   /* yellow — below 50% */
    { "normal",      1.0f, 1.0f, 1.0f, 1.0f },   /* white — default */
    { "overhealed",  0.0f, 0.8f, 1.0f, 1.0f },   /* cyan — above 100% */

    /* team/faction states */
    { "friendly",    0.2f, 0.8f, 1.0f, 1.0f },   /* blue — allies */
    { "enemy",       1.0f, 0.3f, 0.3f, 1.0f },   /* red — opponents */

    /* generic polarity states */
    { "positive",    0.2f, 1.0f, 0.4f, 1.0f },   /* green — good */
    { "negative",    1.0f, 0.2f, 0.2f, 1.0f },   /* red — bad */
    { "neutral",     0.7f, 0.7f, 0.7f, 1.0f },   /* gray — neither */

    { NULL, 0, 0, 0, 0 }
};

/* ── internal helpers ──────────────────────────────────────────────── */

static void WiredTheme_SetState( const char *name, float r, float g, float b, float a ) {
    int i;
    wuiThemeEntry_t *entry;

    /* check for existing entry to update */
    for ( i = 0; i < wired_theme.numStates; i++ ) {
        if ( !Q_stricmp( wired_theme.states[i].name, name ) ) {
            wired_theme.states[i].color[0] = r;
            wired_theme.states[i].color[1] = g;
            wired_theme.states[i].color[2] = b;
            wired_theme.states[i].color[3] = a;
            return;
        }
    }

    /* add new entry */
    if ( wired_theme.numStates >= WUI_THEME_MAX_STATES ) {
        Com_Printf( S_COLOR_YELLOW "WiredTheme: max states reached (%d), cannot add '%s'\n",
                     WUI_THEME_MAX_STATES, name );
        return;
    }

    entry = &wired_theme.states[wired_theme.numStates++];
    Q_strncpyz( entry->name, name, sizeof( entry->name ) );
    entry->color[0] = r;
    entry->color[1] = g;
    entry->color[2] = b;
    entry->color[3] = a;
}

static void WiredTheme_LoadDefaults( void ) {
    int i;
    for ( i = 0; wired_defaultStates[i].name; i++ ) {
        WiredTheme_SetState( wired_defaultStates[i].name,
                             wired_defaultStates[i].r,
                             wired_defaultStates[i].g,
                             wired_defaultStates[i].b,
                             wired_defaultStates[i].a );
    }
}

/* ── public API ────────────────────────────────────────────────────── */

qboolean WiredTheme_ResolveState( const char *state, vec4_t colorOut ) {
    int i;

    if ( !state || !state[0] ) {
        return qfalse;
    }

    for ( i = 0; i < wired_theme.numStates; i++ ) {
        if ( !Q_stricmp( wired_theme.states[i].name, state ) ) {
            Vector4Copy( wired_theme.states[i].color, colorOut );
            return qtrue;
        }
    }

    return qfalse;
}

/* ── console commands ──────────────────────────────────────────────── */

static void WiredTheme_Cmd_List( void ) {
    int i;

    Com_Printf( "WiredTheme: '%s' (%d states)\n", wired_theme.name, wired_theme.numStates );
    Com_Printf( "------------------------------------\n" );
    for ( i = 0; i < wired_theme.numStates; i++ ) {
        Com_Printf( "  %-16s  %.2f %.2f %.2f %.2f\n",
                     wired_theme.states[i].name,
                     wired_theme.states[i].color[0],
                     wired_theme.states[i].color[1],
                     wired_theme.states[i].color[2],
                     wired_theme.states[i].color[3] );
    }
}

static void WiredTheme_Cmd_Set( void ) {
    const char *state;
    float r, g, b, a;

    if ( Cmd_Argc() < 6 ) {
        Com_Printf( "Usage: wui_theme_set <state> <r> <g> <b> <a>\n" );
        Com_Printf( "  Example: wui_theme_set critical 1.0 0.0 0.0 1.0\n" );
        return;
    }

    state = Cmd_Argv( 1 );
    r = atof( Cmd_Argv( 2 ) );
    g = atof( Cmd_Argv( 3 ) );
    b = atof( Cmd_Argv( 4 ) );
    a = atof( Cmd_Argv( 5 ) );

    WiredTheme_SetState( state, r, g, b, a );
    Com_Printf( "WiredTheme: set '%s' to %.2f %.2f %.2f %.2f\n", state, r, g, b, a );
}

/* ── init / shutdown ───────────────────────────────────────────────── */

void WiredTheme_Init( void ) {
    Com_Memset( &wired_theme, 0, sizeof( wired_theme ) );
    Q_strncpyz( wired_theme.name, "default", sizeof( wired_theme.name ) );

    WiredTheme_LoadDefaults();

    Cmd_AddCommand( "wui_theme_list", WiredTheme_Cmd_List );
    Cmd_AddCommand( "wui_theme_set", WiredTheme_Cmd_Set );

    Com_Printf( "WiredTheme: initialized '%s' (%d states)\n",
                 wired_theme.name, wired_theme.numStates );
}

void WiredTheme_Shutdown( void ) {
    Cmd_RemoveCommand( "wui_theme_list" );
    Cmd_RemoveCommand( "wui_theme_set" );
    Com_Memset( &wired_theme, 0, sizeof( wired_theme ) );
}

#endif /* FEAT_WIRED_UI */
