// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// cl_wired_viewport.c — WiredUI viewport-provider registry.
//
// Storage: fixed array of WUI_VIEWPORT_MAX_PROVIDERS slots backed by the
// process-lifetime WiredUI arena (no per-frame churn — registry mutations
// happen on map change + cgame init/shutdown, not in the hot path).
//
// id matching: case-sensitive, kebab-case convention. The id is the
// app-provided lookup key — the compositor resolves a `.wui` itemDef's
// viewportId against it; an app owns its id namespace. Same-id re-registration
// is last-write-wins + SEV_WARN: callers must explicitly unregister first.
//
// Scope identity (owner): each slot records an owner token passed at register
// time, used ONLY to clean up one app's providers at teardown — the registry
// never interprets it (pointer identity only) nor derives it from engine state.
// It is DISTINCT from provider->userdata (which is the render-callback payload):
//   - VM-routed providers (cgame world scene): owner = the kernel's own cgame
//     VM handle (cgvm), supplied engine-side at the register call. A kernel
//     resource handle, NOT app-instance state — no clientNum is ever read.
//   - host-side providers (attract bg / demo viewer): owner = an app-provided
//     token passed explicitly by the caller.
// Distinct apps use distinct ids (their own namespace), so they occupy distinct
// slots and never silently overwrite one another.

#include "../../client.h"

#if FEAT_WIRED_UI

#include "cl_wired_viewport.h"

LOG_DECLARE_CHANNEL( ch_ui, "ui" );

#define WUI_VIEWPORT_MAX_PROVIDERS 32
#define WUI_VIEWPORT_ID_LEN        64

typedef struct {
	char                  id[ WUI_VIEWPORT_ID_LEN ];
	wuiViewportProvider_t provider;
	void                 *owner;   /* scope identity for per-app cleanup, opaque
	                                  to the kernel (pointer identity only).
	                                  DISTINCT from provider.userdata (render
	                                  payload): cgvm for VM-routed providers, an
	                                  app-provided token for host-side ones.
	                                  NEVER engine-read clientNum. */
	qboolean              active;
} wui_viewport_slot_t;

static wui_viewport_slot_t s_slots[ WUI_VIEWPORT_MAX_PROVIDERS ];
static int                 s_slotCount = 0;

/* ── registry lookup ────────────────────────────────────────────────── */

static int find_slot_by_id( const char *id ) {
	int i;
	if ( !id || !*id ) return -1;
	for ( i = 0; i < s_slotCount; i++ ) {
		if ( !s_slots[ i ].active ) continue;
		if ( strcmp( s_slots[ i ].id, id ) == 0 ) {
			return i;
		}
	}
	return -1;
}

static int find_free_slot( void ) {
	int i;
	for ( i = 0; i < s_slotCount; i++ ) {
		if ( !s_slots[ i ].active ) return i;
	}
	if ( s_slotCount < WUI_VIEWPORT_MAX_PROVIDERS ) {
		return s_slotCount++;
	}
	return -1;
}

/* ── public API ─────────────────────────────────────────────────────── */

void WiredUI_RegisterViewportProvider( const char *id, const wuiViewportProvider_t *provider, const void *owner ) {
	int slot;

	/* WN-BLACKFIX: a VM-routed provider (cgame world scene) carries
	 * render==NULL by design — the engine enters it via VM_Call, not a fn-ptr
	 * deref. Reject only when neither a render fn nor VM routing is supplied. */
	if ( !id || !*id || !provider ||
	     ( !provider->is_vm_routed && !provider->render ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredUI_RegisterViewportProvider: invalid args (id=%s provider=%p render=%p vm_routed=%d)\n",
			id ? id : "(null)", (void *) provider,
			provider ? (void *) provider->render : NULL,
			provider ? (int) provider->is_vm_routed : 0 );
		return;
	}

	slot = find_slot_by_id( id );
	if ( slot >= 0 ) {
		/* Same id already registered. The compositor resolves a viewportId to
		 * exactly one provider, so an id is single-occupancy by design and this
		 * is last-write-wins. Flag the owner-changing case loudly: two distinct
		 * apps colliding on the same id means one's viewport would be silently
		 * lost (black-screen-class) — apps must namespace their ids. */
		if ( s_slots[ slot ].owner != owner ) {
			Com_Log( SEV_WARN, LOG_CH(ch_ui),
				"WiredUI_RegisterViewportProvider: id '%s' (slot %d) re-registered by a "
				"different owner (%p -> %p); the previous provider is replaced — apps must "
				"use distinct viewport ids\n",
				id, slot, s_slots[ slot ].owner, owner );
		} else {
			Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
				"WiredUI_RegisterViewportProvider: id '%s' (slot %d) re-registered by same owner\n",
				id, slot );
		}
		s_slots[ slot ].provider = *provider;
		s_slots[ slot ].owner    = (void *) owner;
		return;
	}

	slot = find_free_slot();
	if ( slot < 0 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_ui),
			"WiredUI_RegisterViewportProvider: registry full (%d slots); dropping '%s'\n",
			WUI_VIEWPORT_MAX_PROVIDERS, id );
		return;
	}

	Q_strncpyz( s_slots[ slot ].id, id, sizeof( s_slots[ slot ].id ) );
	s_slots[ slot ].provider = *provider;
	s_slots[ slot ].owner    = (void *) owner;
	s_slots[ slot ].active   = qtrue;

	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI_RegisterViewportProvider: '%s' slot=%d lifetime=%d input_mode=%d owner=%p\n",
		id, slot, (int) provider->lifetime, (int) provider->input_mode, owner );
}

void WiredUI_UnregisterViewportProvider( const char *id ) {
	int slot;
	if ( !id || !*id ) return;
	slot = find_slot_by_id( id );
	if ( slot < 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI_UnregisterViewportProvider: '%s' not in registry; no-op\n", id );
		return;
	}
	s_slots[ slot ].active = qfalse;
	s_slots[ slot ].id[ 0 ] = '\0';
	s_slots[ slot ].owner   = NULL;
	memset( &s_slots[ slot ].provider, 0, sizeof( s_slots[ slot ].provider ) );
	Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
		"WiredUI_UnregisterViewportProvider: '%s' slot=%d freed\n", id, slot );
}

const wuiViewportProvider_t *WiredUI_FindViewportProvider( const char *id ) {
	int slot = find_slot_by_id( id );
	if ( slot < 0 ) return NULL;
	return &s_slots[ slot ].provider;
}

/* The scope-identity token a provider was registered with. For a VM-routed
 * (cgame) provider this is the owning app's cgame VM handle, captured at
 * register time — the engine renders that viewport by entering THAT VM, so a
 * non-focused app's viewport renders its own scene rather than the focused
 * app's. NULL when the id is unknown or the provider carries no owner. */
const void *WiredUI_FindViewportOwner( const char *id ) {
	int slot = find_slot_by_id( id );
	if ( slot < 0 ) return NULL;
	return s_slots[ slot ].owner;
}

/* ── lifecycle cleanup ──────────────────────────────────────────────── */

static void free_slot( int i ) {
	s_slots[ i ].active   = qfalse;
	s_slots[ i ].id[ 0 ]  = '\0';
	s_slots[ i ].owner    = NULL;
	memset( &s_slots[ i ].provider, 0, sizeof( s_slots[ i ].provider ) );
}

static void unregister_by_lifetime( wuiViewportLifetime_t lifetime ) {
	int i, removed = 0;
	for ( i = 0; i < s_slotCount; i++ ) {
		if ( !s_slots[ i ].active ) continue;
		if ( s_slots[ i ].provider.lifetime != lifetime ) continue;
		free_slot( i );
		removed++;
	}
	if ( removed > 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI viewport registry: lifetime=%d cleanup removed %d slot(s)\n",
			(int) lifetime, removed );
	}
}

void WiredUI_UnregisterLevelViewportProviders( void ) {
	unregister_by_lifetime( WUI_VIEWPORT_LIFETIME_LEVEL );
}

void WiredUI_UnregisterFrameViewportProviders( void ) {
	unregister_by_lifetime( WUI_VIEWPORT_LIFETIME_FRAME );
}

/* Tear down every provider an app registered, keyed on the app-provided owner
 * token. Lets one app's providers be cleaned up without the kernel knowing what
 * the token means (it never interprets it — pointer identity only). */
void WiredUI_UnregisterViewportProvidersByOwner( const void *owner ) {
	int i, removed = 0;
	for ( i = 0; i < s_slotCount; i++ ) {
		if ( !s_slots[ i ].active ) continue;
		if ( s_slots[ i ].owner != owner ) continue;
		free_slot( i );
		removed++;
	}
	if ( removed > 0 ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_ui),
			"WiredUI viewport registry: owner=%p cleanup removed %d slot(s)\n",
			owner, removed );
	}
}

void WiredUI_ViewportRegistryShutdown( void ) {
	memset( s_slots, 0, sizeof( s_slots ) );
	s_slotCount = 0;
}

/* Walk every active provider and test its owner token with the caller-supplied
 * predicate; return qtrue on the first owner the predicate accepts. The registry
 * keeps its slot array encapsulated — the caller never sees s_slots[] — and
 * supplies only the per-owner test. Runs in the single-threaded compositor emit;
 * no locking. Used to ask "does any app that owns a viewport satisfy <cond>?"
 * (e.g. is renderable) without the registry interpreting the owner token. */
qboolean WiredUI_AnyViewportOwner( qboolean (*pred)( const void *owner ) ) {
	int i;
	if ( !pred ) return qfalse;
	for ( i = 0; i < s_slotCount; i++ ) {
		if ( !s_slots[ i ].active ) continue;
		if ( pred( s_slots[ i ].owner ) ) return qtrue;
	}
	return qfalse;
}

/* V-22 (2026-05-25): see header. */
wuiViewportInputMode_t WiredUI_GetViewportInputMode( const char *id ) {
	int slot = find_slot_by_id( id );
	if ( slot < 0 ) return WUI_VIEWPORT_INPUT_MODAL_CGAME;
	return s_slots[ slot ].provider.input_mode;
}

#ifdef _DEBUG
/* ── V-20 multi-viewport acceptance self-test (verify-only, K16) ─────────
 *
 * Proves the compositor's per-itemDef dispatch handles >1 registered world-
 * viewport provider — the foundation server-client-decoupling's split-screen
 * builds on. Exercises the EXACT primitive the per-panel walk uses
 * (WiredUI_FindViewportProvider(id) → provider->render), for two distinct
 * ids, and asserts BOTH fired. Registry-only: no shipping .wui references
 * these test ids, so normal play is unaffected. Not split-screen UX —
 * dispatch-mechanism proof only. Remove after acceptance. */
static int s_dbg_vp_fire_a = 0;
static int s_dbg_vp_fire_b = 0;
static void dbg_vp_render_a( const wuiViewportRect_t *rect, void *userdata ) {
	(void) rect; (void) userdata; s_dbg_vp_fire_a++;
}
static void dbg_vp_render_b( const wuiViewportRect_t *rect, void *userdata ) {
	(void) rect; (void) userdata; s_dbg_vp_fire_b++;
}

void WiredUI_ViewportMultiSelfTest( void ) {
	const wuiViewportProvider_t pa = {
		.render = dbg_vp_render_a, .lifetime = WUI_VIEWPORT_LIFETIME_PROCESS,
		.input_mode = WUI_VIEWPORT_INPUT_PASSIVE_DISPLAY, .userdata = NULL };
	const wuiViewportProvider_t pb = {
		.render = dbg_vp_render_b, .lifetime = WUI_VIEWPORT_LIFETIME_PROCESS,
		.input_mode = WUI_VIEWPORT_INPUT_PASSIVE_DISPLAY, .userdata = NULL };
	const wuiViewportProvider_t *qa, *qb;
	wuiViewportRect_t rect = { 0.0f, 0.0f, 1.0f, 1.0f };

	s_dbg_vp_fire_a = s_dbg_vp_fire_b = 0;
	/* Host-side providers: each passes a distinct app-owned token (its own
	 * provider address). This exercises the owner-identity path — previously
	 * both carried userdata=NULL and would have been mis-seen as one owner. */
	WiredUI_RegisterViewportProvider( "dbg_vp_self_a", &pa, &pa );
	WiredUI_RegisterViewportProvider( "dbg_vp_self_b", &pb, &pb );

	/* Dispatch both via the registry lookup the walk uses. */
	qa = WiredUI_FindViewportProvider( "dbg_vp_self_a" );
	qb = WiredUI_FindViewportProvider( "dbg_vp_self_b" );
	if ( qa && qa->render ) qa->render( &rect, qa->userdata );
	if ( qb && qb->render ) qb->render( &rect, qb->userdata );

	Com_Log( ( s_dbg_vp_fire_a == 1 && s_dbg_vp_fire_b == 1 ) ? SEV_DEBUG : SEV_WARN,
		LOG_CH(ch_ui),
		"V-20 multi-viewport self-test: provider A fired=%d, B fired=%d (expect 1,1) — %s\n",
		s_dbg_vp_fire_a, s_dbg_vp_fire_b,
		( s_dbg_vp_fire_a == 1 && s_dbg_vp_fire_b == 1 ) ? "PASS" : "FAIL" );

	WiredUI_UnregisterViewportProvider( "dbg_vp_self_a" );
	WiredUI_UnregisterViewportProvider( "dbg_vp_self_b" );
}
#endif /* _DEBUG */

#endif /* FEAT_WIRED_UI */
