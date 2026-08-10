// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
cl_wired_scene_lua.h -- Wired Cinematic Scene: the `scene` Lua namespace

Registers the scene.play / scene.stop / scene.skip trigger surface on the System VM.
The bindings bridge to the cgame playback state via the sceneplay/scenestop/sceneskip
console commands (see cl_wired_scene_lua.c for the rationale). Client-only
(the whole Lua UI/scene surface is gated by FEAT_WIRED_UI); the header keeps a
no-op fallback so callers need no guard.
===========================================================================
*/

#ifndef CL_WIRED_SCENE_LUA_H
#define CL_WIRED_SCENE_LUA_H

/* Register the scene.* Lua bindings (via WiredScript_RegisterBindings) so they
   go live at the System VM's PostInit. Call once during client init, in the
   same registrar window as WiredCrosshair_Init. */
void WiredScene_LuaInit( void );

#endif /* CL_WIRED_SCENE_LUA_H */
