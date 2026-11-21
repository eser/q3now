// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_DYNAMIC_LIGHT_CONTROL_H
#define WIRED_RENDER_DYNAMIC_LIGHT_CONTROL_H

#include "tr_public.h"

/* One definition of r_dynamiclight for every RAL renderer module (Vulkan, Metal,
 * OpenGL, WebGPU): 0 = dynamic lights off, 1 = on (per-pixel, GPU, world and models).
 * The old CPU "vertex-lit" tier is gone, so there is no third value. Each module
 * registers the cvar through this helper so name, default, range and description
 * cannot drift; a module that submits dynamic lights checks the cached value and
 * treats 0 as a successful no-op (a refused light would read as a submission failure).
 * An archived "2" from before the unification clamps to 1, which is the same meaning. */
static inline cvar_t *wired_render_dynamic_light_register( const refimport_t *imports,
		cvar_t *standaloneDefault ) {
	if ( !imports->Cvar_Get || !imports->Cvar_CheckRange ) {
		standaloneDefault->integer = 1;
		standaloneDefault->value = 1.0f;
		return standaloneDefault;
	}
	cvar_t *control = imports->Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE );
	if ( !control ) return NULL;
	imports->Cvar_CheckRange( control, "0", "1", CV_INTEGER );
	if ( imports->Cvar_SetDescription ) imports->Cvar_SetDescription( control,
		"Dynamic lights (GPU, per-pixel, world and models): 0 off, 1 on." );
	return control;
}

#endif
