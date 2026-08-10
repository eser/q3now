// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_customdraw.c — unified custom-draw registry impl.

See cl_wired_customdraw.h for the design overview + the transitional
double-dispatch rationale.
*/

#include "../../client.h"
#include "cl_wired_customdraw.h"

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#if FEAT_WIRED_UI

/* ── storage ──────────────────────────────────────────────────────────
 * Process-lifetime static arrays. Capacities sized to ship-content +
 * generous headroom: 256 direct entries (currently bootstrapped 16
 * ownerdraw + ~50 hudElement = ~66, room for Lua / custom additions);
 * 16 family prefixes (currently 4 — chat, team, powerup_icon,
 * powerup_time). */

#define WUI_CUSTOMDRAW_MAX         256
#define WUI_CUSTOMDRAW_FAMILY_MAX  16

static wuiCustomDrawDef_t        s_directDefs   [ WUI_CUSTOMDRAW_MAX ];
static int                       s_directCount  = 0;
static qboolean                  s_directOverflowWarned = qfalse;

static wuiCustomDrawFamilyDef_t  s_familyDefs   [ WUI_CUSTOMDRAW_FAMILY_MAX ];
static int                       s_familyCount  = 0;
static qboolean                  s_familyOverflowWarned = qfalse;

/* Synthesis target for FindCustomDraw family-lookup returns. Caller must
 * not cache the returned pointer across frames — each family lookup
 * overwrites this slot. */
static wuiCustomDrawDef_t        s_synthesizedDef;

/* ── lifecycle ────────────────────────────────────────────────────────*/

void WiredUI_CustomDraw_Init( void )
{
	if ( s_directCount > 0 || s_familyCount > 0 ) {
		/* Re-entry (hot-reload). Reset and let bootstrap helpers
		 * re-register. */
		s_directCount = 0;
		s_familyCount = 0;
		s_directOverflowWarned = qfalse;
		s_familyOverflowWarned = qfalse;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI_CustomDraw: initialized (capacity direct=%d family=%d)\n",
		WUI_CUSTOMDRAW_MAX, WUI_CUSTOMDRAW_FAMILY_MAX );
}

void WiredUI_CustomDraw_Shutdown( void )
{
	s_directCount = 0;
	s_familyCount = 0;
	s_directOverflowWarned = qfalse;
	s_familyOverflowWarned = qfalse;
}

/* ── registration ─────────────────────────────────────────────────────*/

void WiredUI_RegisterCustomDraw( const wuiCustomDrawDef_t *def )
{
	if ( !def || !def->name ) return;

	/* Reject duplicates so the (registered, registered) double-bootstrap
	 * doesn't silently shadow legacy entries. */
	for ( int i = 0; i < s_directCount; i++ ) {
		if ( !Q_stricmp( s_directDefs[ i ].name, def->name ) ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WiredUI_CustomDraw: duplicate name '%s' — skipping re-register\n",
				def->name );
			return;
		}
	}

	if ( s_directCount >= WUI_CUSTOMDRAW_MAX ) {
		if ( !s_directOverflowWarned ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WiredUI_CustomDraw: direct registry full (cap=%d) — '%s' dropped\n",
				WUI_CUSTOMDRAW_MAX, def->name );
			s_directOverflowWarned = qtrue;
		}
		return;
	}

	s_directDefs[ s_directCount++ ] = *def;
	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"WiredUI_CustomDraw: registered %s (%s)\n",
		def->name, def->isStateful ? "stateful" : "stateless" );
}

void WiredUI_RegisterCustomDrawFamily( const wuiCustomDrawFamilyDef_t *def )
{
	if ( !def || !def->prefix ) return;

	for ( int i = 0; i < s_familyCount; i++ ) {
		if ( !Q_stricmp( s_familyDefs[ i ].prefix, def->prefix ) ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WiredUI_CustomDraw: duplicate family prefix '%s' — skipping\n",
				def->prefix );
			return;
		}
	}

	if ( s_familyCount >= WUI_CUSTOMDRAW_FAMILY_MAX ) {
		if ( !s_familyOverflowWarned ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WiredUI_CustomDraw: family registry full (cap=%d) — '%s' dropped\n",
				WUI_CUSTOMDRAW_FAMILY_MAX, def->prefix );
			s_familyOverflowWarned = qtrue;
		}
		return;
	}

	s_familyDefs[ s_familyCount++ ] = *def;
	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"WiredUI_CustomDraw: registered family %s [%d..%d]\n",
		def->prefix, def->minIndex, def->maxIndex );
}

/* ── lookup helpers ───────────────────────────────────────────────────*/

/* Shared indexed-name parser. Parses "hud:chat8" → prefix="hud:chat" + index=8;
 * preserves any sigil prefix and a role-suffix after the digits ("powerup1_icon"
 * → "powerup_icon" + 1). Bounds are caller-supplied so the customdraw lookup
 * ([0..999]) and the hud family registry ([1..16]) share one implementation.
 * Returns qfalse on a digit-less name or an index outside [minIndex, maxIndex]. */
qboolean WiredUI_ParseIndexedName( const char *name, char *prefixBuf,
                                   int prefixBufSize, int minIndex,
                                   int maxIndex, int *outIndex )
{
	const char *p = name;
	const char *digitStart;
	int         index = 0;

	if ( !name || !prefixBuf || prefixBufSize <= 0 || !outIndex ) return qfalse;

	/* Walk to the first digit run. */
	while ( *p && !( *p >= '0' && *p <= '9' ) ) p++;
	if ( !*p ) return qfalse;

	digitStart = p;
	while ( *p >= '0' && *p <= '9' ) {
		index = index * 10 + ( *p - '0' );
		p++;
	}

	if ( index < minIndex || index > maxIndex ) return qfalse;

	/* prefix = chars before digits + chars after digits (the role suffix). */
	Com_sprintf( prefixBuf, prefixBufSize, "%.*s%s",
		(int)( digitStart - name ), name, p );

	*outIndex = index;
	return qtrue;
}

const wuiCustomDrawDef_t *WiredUI_FindCustomDraw( const char *name, int *outFamilyIndex )
{
	if ( outFamilyIndex ) *outFamilyIndex = -1;

	if ( !name || !*name ) return NULL;

	/* Direct hit first. */
	for ( int i = 0; i < s_directCount; i++ ) {
		if ( !Q_stricmp( s_directDefs[ i ].name, name ) ) {
			return &s_directDefs[ i ];
		}
	}

	/* Family fallback. */
	{
		char prefix[ 128 ];
		int  idx;
		if ( !WiredUI_ParseIndexedName( name, prefix, sizeof( prefix ), 0, 999, &idx ) ) {
			return NULL;
		}
		for ( int i = 0; i < s_familyCount; i++ ) {
			const wuiCustomDrawFamilyDef_t *fam = &s_familyDefs[ i ];
			if ( Q_stricmp( fam->prefix, prefix ) != 0 ) continue;
			if ( idx < fam->minIndex || idx > fam->maxIndex ) continue;

			Q_strncpyz( s_synthesizedDef.name, name, sizeof( s_synthesizedDef.name ) );
			s_synthesizedDef.defaultVisibility = fam->defaultVisibility;
			s_synthesizedDef.isStateful        = qtrue;
			s_synthesizedDef.create            = fam->create;
			s_synthesizedDef.destroy           = fam->destroy;
			s_synthesizedDef.routine.stateful  = fam->routine_stateful;

			if ( outFamilyIndex ) *outFamilyIndex = idx;
			return &s_synthesizedDef;
		}
	}

	return NULL;
}

/* ── wui_customdraw_list dev command ──────────────────────────────────
 * Sorted dump of the direct registry + family registry. Developer-1
 * gated. Diagnostic helper retained. */

static int wui_customdraw_cmp_byname( const void *a, const void *b )
{
	const wuiCustomDrawDef_t *da = (const wuiCustomDrawDef_t *) a;
	const wuiCustomDrawDef_t *db = (const wuiCustomDrawDef_t *) b;
	return Q_stricmp( da->name ? da->name : "", db->name ? db->name : "" );
}

static void WiredUI_CustomDrawList_f( void )
{
	wuiCustomDrawDef_t sorted[ WUI_CUSTOMDRAW_MAX ];
	int                i;

	if ( !Cvar_VariableIntegerValue( "wired_ui_debug" ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"wui_customdraw_list: requires `wired_ui_debug 1` to fire\n" );
		return;
	}

	memcpy( sorted, s_directDefs, sizeof( sorted[ 0 ] ) * s_directCount );
	qsort( sorted, s_directCount, sizeof( sorted[ 0 ] ), wui_customdraw_cmp_byname );

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"wui_customdraw_list: %d direct entries, %d family entries\n",
		s_directCount, s_familyCount );

	for ( i = 0; i < s_directCount; i++ ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"  %s (%s, defaultVis=0x%X)\n",
			sorted[ i ].name,
			sorted[ i ].isStateful ? "stateful" : "stateless",
			sorted[ i ].defaultVisibility );
	}

	for ( i = 0; i < s_familyCount; i++ ) {
		Com_Log( SEV_INFO, LOG_CH(ch_ui),
			"  %s [%d..%d] (family, defaultVis=0x%X)\n",
			s_familyDefs[ i ].prefix,
			s_familyDefs[ i ].minIndex,
			s_familyDefs[ i ].maxIndex,
			s_familyDefs[ i ].defaultVisibility );
	}
}

void WiredUI_CustomDraw_RegisterDevCommands( void )
{
	Cmd_AddCommand( "wui_customdraw_list", WiredUI_CustomDrawList_f );
}

void WiredUI_CustomDraw_UnregisterDevCommands( void )
{
	Cmd_RemoveCommand( "wui_customdraw_list" );
}

#endif /* FEAT_WIRED_UI */
