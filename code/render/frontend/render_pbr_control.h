// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_PBR_CONTROL_H
#define WIRED_RENDER_PBR_CONTROL_H

#include "tr_public.h"

/* Cache the cvar pointer at registration, never look it up per material or
 * draw. The module samples its live value once when opening a new frame. */
static inline cvar_t *wired_render_pbr_register( const refimport_t *imports,
		cvar_t *standaloneDefault ) {
	if ( !imports->Cvar_Get || !imports->Cvar_CheckRange ) {
		standaloneDefault->integer = 0;
		standaloneDefault->value = 0.0f;
		return standaloneDefault;
	}
	cvar_t *control = imports->Cvar_Get( "r_pbr", "1",
		CVAR_ARCHIVE | CVAR_NODEFAULT );
	if ( !control ) return NULL;
	imports->Cvar_CheckRange( control, "0", "1", CV_INTEGER );
	if ( imports->Cvar_SetGroup ) imports->Cvar_SetGroup( control, CVG_RENDERER );
	if ( imports->Cvar_SetDescription ) imports->Cvar_SetDescription( control,
		"Live material PBR/environment response: 0 disabled, 1 enabled. Does not disable sky, FX, lightmaps or bloom." );
	return control;
}

#endif
