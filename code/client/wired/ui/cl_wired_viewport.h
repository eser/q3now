// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// cl_wired_viewport.h — WiredUI viewport-provider registry public API.
//
// Apps (cgame VM, demo viewer, attract-mode background) register render
// callbacks against a string id; .wui menus author `<viewport id="…"/>` items
// and the compositor's WUI_LAYER_WORLD_VIEWPORT walk (Turn 3 V-19) invokes
// the registered callback at emit time.
//
// Lifecycle policies (S7):
//   PROCESS — engine-lifetime; cleaned only on engine quit.
//   LEVEL   — bound to the current map; auto-unregistered on CL_ShutdownLevel.
//   FRAME   — transient (debug overlays, preview captures); auto-cleaned at
//             frame end.
//
// Input modes (S9): forwarded by the input dispatcher (Turn 3 V-22) when the
// cursor falls inside a registered viewport's rect.

#ifndef WUI_VIEWPORT_H
#define WUI_VIEWPORT_H

#include "../../../qcommon/q_shared.h"
#include "../../../qcommon/q_feats.h"
/* V-16 (2026-05-25): public viewport types relocated to qcommon/wired/ so
 * the engine and the cgame VM both see the same struct layout when the
 * CG_REGISTER_VIEWPORT_PROVIDER syscall derefs `provider`. */
#include "../../../qcommon/wired/ui_viewport_types.h"

#if FEAT_WIRED_UI

/* ── lifecycle ──────────────────────────────────────────────────────── */

/* Registers `provider` against `id`. id is the app-provided lookup key (copied
 * into the slot; kebab-case convention; case-sensitive). An app owns its id
 * namespace; distinct apps use distinct ids and occupy distinct slots. `owner`
 * is the scope-identity token for per-app cleanup — opaque to the kernel
 * (pointer identity only), DISTINCT from provider->userdata (the render
 * payload): pass the cgame VM handle (cgvm) for VM-routed providers, or an
 * app-provided token for host-side ones. NEVER an engine-read clientNum. Same-
 * id re-registration is last-write-wins + SEV_WARN (loud when the owner differs
 * — one app's viewport would be lost; apps must namespace their ids). */
void WiredUI_RegisterViewportProvider  ( const char *id, const wuiViewportProvider_t *provider, const void *owner );

/* Removes the provider with matching id. No-op + SEV_DEBUG if id not in
 * registry. */
void WiredUI_UnregisterViewportProvider( const char *id );

/* Returns the provider struct (registry-owned, do NOT free) or NULL if id
 * not registered. Callers in the render path treat NULL as a soft miss
 * (SEV_WARN + visible placeholder). */
const wuiViewportProvider_t *WiredUI_FindViewportProvider( const char *id );

/* The owner token a provider was registered with (the owning app's cgame VM
 * handle for a VM-routed provider). NULL if id not registered. Lets the render
 * path enter the OWNING app's VM rather than the focused one. */
const void *WiredUI_FindViewportOwner( const char *id );

/* Bulk cleanup: unregisters every provider with LIFETIME_LEVEL. Wired into
 * CL_ShutdownLevel (Turn 3 V-19). */
void WiredUI_UnregisterLevelViewportProviders( void );

/* Bulk cleanup: unregisters every provider with LIFETIME_FRAME. Wired into
 * the WiredUI tick (Turn 3 V-15) end-of-frame. */
void WiredUI_UnregisterFrameViewportProviders( void );

/* Bulk cleanup: unregisters every provider registered with the given
 * app-provided owner token (= the provider->userdata supplied at register).
 * Pointer identity only — the kernel never interprets the token. Lets one
 * app's providers be torn down without engine-side app-instance knowledge. */
void WiredUI_UnregisterViewportProvidersByOwner( const void *owner );

/* Engine-wide registry teardown — invoked at WiredUI_Shutdown. */
void WiredUI_ViewportRegistryShutdown( void );

/* Walk active providers, testing each owner token with the caller's predicate;
 * returns qtrue on the first accepted owner. Keeps the slot array encapsulated —
 * the caller supplies only the per-owner test (e.g. "is the app owning this
 * provider renderable?"). Single-threaded (compositor emit). */
qboolean WiredUI_AnyViewportOwner( qboolean (*pred)( const void *owner ) );

/* V-22 (2026-05-25): query helper for the input dispatcher. Returns the
 * registered provider's `input_mode` if a provider with matching id
 * exists; returns WUI_VIEWPORT_INPUT_MODAL_CGAME as a safe default if not
 * found (so callers fall back to the legacy CA_ACTIVE input path rather
 * than silently dropping events). The compositor input dispatcher reads
 * this to decide whether mouse / key events flow to the cgame VM
 * (MODAL_CGAME), get dropped (PASSIVE_DISPLAY), or route through normal
 * UI focus (INTERACTIVE_UI — future workstream). */
wuiViewportInputMode_t WiredUI_GetViewportInputMode( const char *id );

#ifdef _DEBUG
/* V-20 multi-viewport acceptance self-test (verify-only, K16). Registers two
 * dummy providers, dispatches both via the registry-lookup primitive the
 * compositor walk uses, and logs PASS/FAIL that each fired exactly once.
 * Called once from WiredUI_Init. Removed after acceptance. */
void WiredUI_ViewportMultiSelfTest( void );
#endif

#endif /* FEAT_WIRED_UI */
#endif /* WUI_VIEWPORT_H */
