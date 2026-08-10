// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_palette.c — Wired UI palette (mode × accent) overlay driver.
*/

#include "../../client.h"
#include "cl_wired_palette.h"

#if FEAT_WIRED_UI

#include "../../../qcommon/qcommon.h"

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

extern qboolean WiredUI_LoadMenuFile( const char *filename );
extern void     WiredUI_ReloadMenus( void );

/* Set by WiredPalette_Init once the palette cvars are registered, polled by
 * WiredUI_LoadTokensIfNeeded right after the base `_tokens.wui` parse so
 * any menu being parsed sees overlay-resolved tokens before its $-refs
 * land in itemDef vec4 fields. NULL before init; safe to call as a no-op. */
static qboolean s_palette_init_complete = qfalse;

/* Dispatch 5.6 S3: pending-reload flag. Set when a cvar accent/mode change
 * fires while cls.uiStarted=qfalse — the immediate WiredUI_ReloadMenus call
 * would be a no-op. WiredPalette_TickPending (polled from the UI per-frame
 * tick) fires the deferred reload once uiStarted transitions to qtrue. */
static qboolean s_palette_reload_pending = qfalse;

static cvar_t *ui_palette_mode_cvar;
static cvar_t *ui_palette_accent_cvar;

/* NULL-terminated enum lists for CVT_ENUM validation. Cvar_Register stores
 * the pointer; both arrays are file-static so they outlive the register
 * call without copy. */
static const char *s_palette_modes[]   = { "dark", "light", NULL };
static const char *s_palette_accents[] = { "amber", "blood", "toxic", "cyan", "violet", NULL };

static void wired_palette_on_change( cvar_t *self ) {
	(void) self;
	WiredPalette_Reload();
}

/* Inner overlay-only routine — loads mode + accent token files without
 * triggering menu reload. Called both from the public Reload entry point
 * (after manual cvar change) and from WiredUI_LoadTokensIfNeeded right
 * after the base `_tokens.wui` parse, so the in-progress menu parse sees
 * overlay-resolved tokens when its $-refs reach itemDef vec4 fields. */
void WiredPalette_ApplyOverlays( void ) {
	char path[ 64 ];
	const char *mode;
	const char *accent;

	if ( !s_palette_init_complete ) return;

	mode   = ui_palette_mode_cvar   ? ui_palette_mode_cvar->string   : "dark";
	accent = ui_palette_accent_cvar ? ui_palette_accent_cvar->string : "amber";

	Com_sprintf( path, sizeof( path ), "ui/themes/%s/_tokens.wui", mode );
	if ( !WiredUI_LoadMenuFile( path ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredPalette: mode overlay '%s' not found — palette mode swap skipped\n",
			path );
	}
	Com_sprintf( path, sizeof( path ), "ui/themes/%s/_tokens.wui", accent );
	if ( !WiredUI_LoadMenuFile( path ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredPalette: accent overlay '%s' not found — palette accent swap skipped\n",
			path );
	}
}

void WiredPalette_Reload( void ) {
	const char *mode   = ui_palette_mode_cvar   ? ui_palette_mode_cvar->string   : "dark";
	const char *accent = ui_palette_accent_cvar ? ui_palette_accent_cvar->string : "amber";

	WiredPalette_ApplyOverlays();

	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"WiredPalette: applied mode='%s' accent='%s' (uiStarted=%d)\n",
		mode, accent, cls.uiStarted );

	/* itemDef forecolor / backcolor / bordercolor fields resolve $token
	 * references at parse time. To make a runtime palette swap propagate
	 * to those resolved-vec4 fields, force a full menu reparse. The
	 * reparse path enters WiredUI_LoadTokensIfNeeded which calls
	 * WiredPalette_ApplyOverlays() AFTER base tokens — so the menus
	 * parse against overlay-resolved tokens.
	 *
	 * Dispatch 5.5 S5: the dispatch 5.4 symptom (accent change ignored
	 * when an extra override pak was mounted) traced to the dispatch-
	 * 4-era pak load order: pax21 already mounted at engine init, so the
	 * theme overlays at `ui/themes/<accent>/_tokens.wui` resolved from
	 * pax21. After dispatch 5.4 introduced zz_dispatch5_4_override.sw3z
	 * containing only `ui/main.wui`, the menu reload chain reloaded the
	 * override-pak main.wui (now sorted last) but the overlay tokens
	 * still resolved from pax21 — the chain itself worked, but the
	 * accent change fired BEFORE attract_restart's first frame had
	 * cls.uiStarted set in some boot orderings, silently dropping the
	 * menu reload. Pak cleanup (override removed; pax21 rebuilt with
	 * canonical widget) eliminates the trigger; the uiStarted guard is
	 * retained because unconditional reload during early init breaks the
	 * boot ordering invariants. */
	if ( cls.uiStarted ) {
		WiredUI_ReloadMenus();
		s_palette_reload_pending = qfalse;
	} else {
		s_palette_reload_pending = qtrue;
	}
}

/* Dispatch 5.6 S3: poll for deferred palette reload. Called from the UI
 * per-frame tick (see WiredUI_FrameTick). Fires WiredUI_ReloadMenus once
 * uiStarted has become qtrue after a prior call attempted reload with
 * uiStarted=qfalse. */
void WiredPalette_TickPending( void ) {
	if ( !s_palette_reload_pending ) return;
	if ( !cls.uiStarted ) return;
	WiredUI_ReloadMenus();
	s_palette_reload_pending = qfalse;
	Com_Log( SEV_INFO, LOG_CH(ch_ui),
		"WiredPalette: pending reload fired (uiStarted transition)\n" );
}

void WiredPalette_Init( void ) {
	static const cvarDesc_t d_mode = CVAR_ENUM_CB(
		"ui_palette_mode", "dark", CVAR_ARCHIVE,
		"UI palette mode: dark or light", s_palette_modes,
		wired_palette_on_change );
	static const cvarDesc_t d_accent = CVAR_ENUM_CB(
		"ui_palette_accent", "amber", CVAR_ARCHIVE,
		"UI palette accent: amber, blood, toxic, cyan, violet", s_palette_accents,
		wired_palette_on_change );

	ui_palette_mode_cvar   = Cvar_Register( &d_mode );
	ui_palette_accent_cvar = Cvar_Register( &d_accent );

	s_palette_init_complete = qtrue;

	/* Seed the initial state. If the archived cvar values pin a non-default
	 * mode/accent (set last session) this applies them before any menu
	 * parses a $-token reference. */
	WiredPalette_Reload();
}

const char *WiredPalette_CurrentMode( void ) {
	return ui_palette_mode_cvar ? ui_palette_mode_cvar->string : "dark";
}

const char *WiredPalette_CurrentAccent( void ) {
	return ui_palette_accent_cvar ? ui_palette_accent_cvar->string : "amber";
}

#endif /* FEAT_WIRED_UI */
