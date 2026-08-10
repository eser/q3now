// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
cl_wired_customdraw.h — unified custom-draw registry.

Collapses three legacy mechanisms (ownerdraw, hudElement, hudElement family
indexed) into one named registry. Parser sigils route legacy keywords into
prefixed names:

  ownerdraw "player_health"      → "od:player_health"      (stateless)
  hudElement "fps"               → "hud:fps"               (stateful)
  hudElement "chat8"             → "hud:chat" family + idx (stateful)
  custom "loading_wireframe"     → "custom:loading_wireframe" (either)

Discriminant union: each entry is either stateless (ownerdraw-style, simple
xywh+color callback) or stateful (hudElement-style, create/destroy/routine
lifecycle with per-item context). The `isStateful` flag tells the dispatch
site which arm of the union to invoke.

Lifecycle:
  - Registry storage: static arrays, process-lifetime, populated at
    WiredUI_Init by the permanent registration bootstrap helpers
    (WiredOwnerDraw_RegisterAll, WiredHud_RegisterElements).
  - Per-item stateful context: created at item bind time, lives in the
    s_hudArena (cl_wired_ui_hud_register.c). Reclaimed via Arena_Reset on
    menu teardown.

Transitional shape: legacy dispatch sites (cl_wired_ui.c:2189
ownerdraw, cl_wired_hud_registry.c WiredHud_RenderElements) STAY for the
double-dispatch cycle. Compositor's CLAY_RENDER_COMMAND_TYPE_CUSTOM is the
new primary dispatch. Both fire; visuals overdraw (deterministic) until
the legacy SCR + hud routine path is retired.
*/

#ifndef CL_WIRED_CUSTOMDRAW_H
#define CL_WIRED_CUSTOMDRAW_H

#include "../../../qcommon/q_feats.h"

#if FEAT_WIRED_UI

#include "../../../qcommon/q_shared.h"

/* Forward-declare to avoid pulling cl_wired_ui.h transitively. */
struct wiredItemDef_s;

/* ── runtime per-emit config ───────────────────────────────────────────
 * Passed to stateful create() at item bind time + threaded into the per-
 * frame compositor emit on every CUSTOM dispatch. Stateless entries
 * ignore everything except rect + forecolor (their callback signature
 * doesn't take it). */
typedef struct wuiCustomDrawConfig_s {
	float                          rect[4];          /* x, y, w, h pixels */
	vec4_t                         forecolor;
	int                            ownerdrawFlag;    /* CG_SHOW_* / UI_SHOW_* */
	int                            textstyle;
	int                            familyIndex;      /* -1 if not family-indexed */
	const char                    *unprefixedName;   /* "fps" from "hud:fps" — for adapter dispatch */
	const struct wiredItemDef_s   *item;             /* back-pointer for legacy-config adapters */
} wuiCustomDrawConfig_t;

/* ── direct-registry entry ─────────────────────────────────────────────
 * One per registered name. Discriminant union: when isStateful == qfalse,
 * create + destroy must be NULL and routine.stateless drives all dispatch;
 * when isStateful == qtrue, create returns a per-item context that
 * routine.stateful + destroy consume.
 *
 * `name` is an inline char buffer (not `const char *`) so callers can
 * compose sigil-prefixed names in stack/local scope without lifetime
 * concerns — the registry copies the bytes when WiredUI_RegisterCustomDraw
 * inserts the def. */
typedef struct wuiCustomDrawDef_s {
	char        name[ 64 ];                          /* including sigil, e.g. "od:player_health" */
	int         defaultVisibility;                   /* SE_IM/SE_SPECT/etc., 0 = always */
	qboolean    isStateful;
	void *    (*create)( const wuiCustomDrawConfig_t *cfg );
	void      (*destroy)( void *context );
	/* Non-const vec4_t color matches legacy ownerdraw callback signature
	 * (ownerDrawFunc_t in cl_wired_ownerdraw.c) so the existing function
	 * pointers register without a cast. Callbacks treat the color as
	 * read-only by convention; no callback mutates it. */
	union {
		void  (*stateless)( float x, float y, float w, float h, vec4_t color );
		void  (*stateful) ( void *context, float x, float y, float w, float h, vec4_t color );
	} routine;
} wuiCustomDrawDef_t;

/* ── family-indexed registry entry ─────────────────────────────────────
 * One per family prefix. Resolves at FindCustomDraw lookup time: names
 * matching "<prefix><N>" with N ∈ [minIndex, maxIndex] synthesize a
 * direct def using these callbacks + the parsed familyIndex in cfg. */
typedef struct wuiCustomDrawFamilyDef_s {
	char        prefix[ 64 ];                        /* including sigil, e.g. "hud:chat" */
	int         minIndex;
	int         maxIndex;
	int         defaultVisibility;
	void *    (*create)( const wuiCustomDrawConfig_t *cfg );
	void      (*destroy)( void *context );
	void      (*routine_stateful)( void *context, float x, float y, float w, float h, vec4_t color );
} wuiCustomDrawFamilyDef_t;

/* ── lifecycle + registration ──────────────────────────────────────────*/

void  WiredUI_CustomDraw_Init    ( void );  /* call from WiredUI_Init */
void  WiredUI_CustomDraw_Shutdown( void );

void  WiredUI_RegisterCustomDraw      ( const wuiCustomDrawDef_t       *def );
void  WiredUI_RegisterCustomDrawFamily( const wuiCustomDrawFamilyDef_t *def );

/* ── lookup ───────────────────────────────────────────────────────────
 * Direct hit first. If miss, parses name into <prefix><digits> + tries
 * family registry. On family hit, returns a static synthesized def
 * (caller-owned for the duration of one frame's dispatch — DO NOT cache
 * across frames). outFamilyIndex receives the parsed index on family hit
 * (set to -1 otherwise). NULL if neither matches. */
const wuiCustomDrawDef_t *WiredUI_FindCustomDraw( const char *name, int *outFamilyIndex );

/* ── indexed-name parser (shared) ─────────────────────────────────────
 * Splits an indexed family/element name like "powerup1_icon" into its
 * prefix+suffix ("powerup_icon") and the numeric index (1). Digits may be
 * followed by a role-suffix. Callers pass their own [minIndex..maxIndex]
 * bounds (e.g. hud families use [1..16], customdraw families [0..999]);
 * an out-of-range or digit-less name returns qfalse. Shared so the
 * customdraw lookup and the hud family registry use one implementation. */
qboolean WiredUI_ParseIndexedName( const char *name, char *prefixBuf,
                                   int prefixBufSize, int minIndex,
                                   int maxIndex, int *outIndex );

/* ── dev command registration (developer-1 gated) ─────────────────────
 * `wui_customdraw_list` dumps the registered defs sorted alphabetically.
 * Diagnostic helper. */
void  WiredUI_CustomDraw_RegisterDevCommands  ( void );
void  WiredUI_CustomDraw_UnregisterDevCommands( void );

#endif /* FEAT_WIRED_UI */
#endif /* CL_WIRED_CUSTOMDRAW_H */
