// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_EFFECT_PROFILE_H
#define WIRED_RENDER_FRONTEND_EFFECT_PROFILE_H

#include "../../qcommon/wired/render/effect_profile.h"

typedef enum {
	WIRED_FX_VALID = 0,
	WIRED_FX_INVALID_ARGUMENT,
	WIRED_FX_INVALID_SCHEMA,
	WIRED_FX_INVALID_PROFILE_BUDGET,
	WIRED_FX_INVALID_PROFILE_RANGE,
	WIRED_FX_INVALID_INHERITANCE,
	WIRED_FX_INVALID_ACTION_TYPE,
	WIRED_FX_INVALID_ACTION_ID,
	WIRED_FX_INVALID_ACTION_FLAGS,
	WIRED_FX_INVALID_ACTION_RANGE,
	WIRED_FX_INVALID_ACTION_BUDGET,
	WIRED_FX_INVALID_ACTION_RESOURCE,
	WIRED_FX_INVALID_ACTION_GRAPH,
	WIRED_FX_INVALID_EVENT
} wiredFxValidationError_t;

typedef struct {
	wiredFxProfile_t profiles[WIRED_FX_MAX_PROFILES];
	qboolean registered[WIRED_FX_MAX_PROFILES];
	uint64_t generation;
	uint32_t count;
} wiredFxRegistry_t;

#ifdef __cplusplus
extern "C" {
#endif

wiredFxValidationError_t WiredFx_ValidateProfile( const wiredFxProfile_t *profile );
const char *WiredFx_ActionTypeName( uint32_t type );
void WiredFx_InitRegistry( wiredFxRegistry_t *registry );
qboolean WiredFx_RegisterProfile( wiredFxRegistry_t *registry, uint32_t handle,
	const wiredFxProfile_t *profile );
qboolean WiredFx_GetProfile( const wiredFxRegistry_t *registry, uint32_t handle,
	wiredFxProfile_t *outProfile );
wiredFxValidationError_t WiredFx_ValidateEvent( const wiredFxRegistry_t *registry,
	const wiredFxEvent_t *event );

#ifdef __cplusplus
}
#endif

#endif
