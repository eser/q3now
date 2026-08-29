// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

#pragma once

#include "../../render/effect_profile.h"

typedef struct lua_State lua_State;

typedef enum {
	WIRED_FX_RESOURCE_MATERIAL = 0,
	WIRED_FX_RESOURCE_PARTICLE_CLASS,
	WIRED_FX_RESOURCE_MODEL,
	WIRED_FX_RESOURCE_SOUND,
	WIRED_FX_RESOURCE_CURVE,
	WIRED_FX_RESOURCE_RENDER_PARM,
	WIRED_FX_RESOURCE_ENVIRONMENT,
	WIRED_FX_RESOURCE_FLARE,
	WIRED_FX_RESOURCE_RIBBON
} wiredFxResourceType_t;

typedef uint32_t ( *wiredFxResolveResourceFn )( wiredFxResourceType_t type,
	const char *name, void *userData );

typedef struct {
	wiredFxResolveResourceFn resolveResource;
	void *userData;
} wiredFxAuthoringResolver_t;

/*
Compile the Lua table at tableIndex into the pointer-free WiredFX ABI. String
resources are resolved exactly once here; numeric resource handles are useful
for generated content and focused tests. On failure the output is cleared and
error receives a stable field-oriented diagnostic when provided.
*/
qboolean WiredFxAuthoring_ReadProfile( wiredFxProfile_t *out, lua_State *L,
	int tableIndex, const wiredFxAuthoringResolver_t *resolver,
	char *error, uint32_t errorSize );
