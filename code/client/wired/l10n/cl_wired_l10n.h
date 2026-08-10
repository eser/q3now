// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
===========================================================================
cl_wired_l10n.h -- localization table: l10n key -> localized text.

The authored content is Lua: scripts/l10n/<lang>.lua returns a flat table
{ ["<key>"] = "<text>", ... } where <key> is the sound-name-as-subtitle-key
(the same key space WiredScene's WSCENE_EV_CAPTION carries). At load time the
table is compiled on the System VM and read into a plain-C key->text hash; the
Lua VM is used ONLY at load time — WiredL10n_Get is a plain-C string lookup,
no per-call Lua.

Mirrors the landed scene/crosshair loaders (System VM, client-side, load-time
only). Client-side because the caption consumer (cgame CG_SceneView) runs in
the WASM VM, which has no lua_State — the l10n lookup lives engine-side and a
later phase bridges the cgame captionKey through WiredL10n_Get.
===========================================================================
*/

#ifndef CL_WIRED_L10N_H
#define CL_WIRED_L10N_H

/* Register the cl_language cvar + the l10n_reload command and queue the initial
 * table load (runs at WiredScript_PostInit, same registrar window as the
 * crosshair/scene loaders). Call once from CL_Init. */
void WiredL10n_Init( void );

/* Free the plain-C table. Call from CL_Shutdown. */
void WiredL10n_Shutdown( void );

/* Reload scripts/l10n/<cl_language>.lua, replacing the current table. Called on
 * the l10n_reload command after a language change, and once at PostInit for the
 * initial load. Frees the old table first (no leak across reloads). */
void WiredL10n_Reload( void );

/* Look up the localized text for `key`. Returns the stored text when present,
 * or `key` itself as a graceful fallback when the key is missing (so a missing
 * translation shows the raw key — never NULL, never blank). The returned
 * pointer is owned by the l10n table (or is `key` on fallback) and is valid
 * until the next reload; copy it if you need to hold it across a reload. */
const char *WiredL10n_Get( const char *key );

#endif /* CL_WIRED_L10N_H */
