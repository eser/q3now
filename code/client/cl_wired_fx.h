// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../qcommon/wired/render/effect_profile.h"
#include "../qcommon/wired/render/particle_class.h"
#include "../render/frontend/tr_types.h"

void CL_WiredFx_BeginRegistration( void );
void CL_WiredFx_RegisterParticleClass( particleClassHandle_t handle,
	const particleClass_t *particleClass, const char *name );
void CL_WiredFx_LoadProfiles( void );
qboolean CL_WiredFx_SubmitEvent( const wiredFxEvent_t *event );
void CL_WiredFx_ServiceScene( refdef_t *refdef );
void CL_WiredFx_Stats_f( void );
