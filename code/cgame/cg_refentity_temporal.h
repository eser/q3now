// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef CG_REFENTITY_TEMPORAL_H
#define CG_REFENTITY_TEMPORAL_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../qcommon/q_shared.h"
#include "../render/frontend/tr_types.h"

#define CG_REFENTITY_TEMPORAL_UNKNOWN (-2)
#define CG_REFENTITY_TEMPORAL_ABSENT  (-1)

typedef qboolean (*cgTemporalGetValueFn)( char *value, int valueSize,
	const char *key );
typedef void (*cgTemporalEntityCallFn)( int trap, const refEntity_t *re,
	const refEntityMotion_t *motion );

typedef struct {
	int optionalTrap;
	uint32_t glconfigGeneration;
} cgTemporalEntityDispatch_t;

static ID_INLINE qboolean CG_TemporalEntityDispatch(
		cgTemporalEntityDispatch_t *state, const refEntity_t *re,
		const refEntityMotion_t *motion, int ordinaryTrap, int temporalTrap,
		uint32_t glconfigGeneration,
		cgTemporalGetValueFn getValue, cgTemporalEntityCallFn call ) {
	char value[16];
	char canonical[16];
	char *end;
	long parsed;

	if ( !state || !re || !RefEntityMotion_IsValid( motion ) || !getValue || !call )
		return qfalse;
	if ( state->glconfigGeneration != glconfigGeneration ) {
		state->optionalTrap = CG_REFENTITY_TEMPORAL_UNKNOWN;
		state->glconfigGeneration = glconfigGeneration;
	}
	if ( state->optionalTrap == CG_REFENTITY_TEMPORAL_UNKNOWN ) {
		memset( value, 0, sizeof( value ) );
		state->optionalTrap = CG_REFENTITY_TEMPORAL_ABSENT;
		if ( getValue( value, sizeof( value ),
				"trap_R_AddRefEntityToSceneTemporal" ) ) {
			parsed = strtol( value, &end, 10 );
			snprintf( canonical, sizeof( canonical ), "%d", temporalTrap );
			if ( end != value && *end == '\0' && parsed == temporalTrap
					&& !strcmp( value, canonical ) ) {
				state->optionalTrap = temporalTrap;
			}
		}
	}
	if ( state->optionalTrap == temporalTrap )
		call( temporalTrap, re, motion );
	else
		call( ordinaryTrap, re, NULL );
	return qtrue;
}

#endif
