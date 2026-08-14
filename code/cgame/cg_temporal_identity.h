// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef CG_TEMPORAL_IDENTITY_H
#define CG_TEMPORAL_IDENTITY_H

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_types.h"

typedef refEntityMotionRole_t cgTemporalRole_t;

typedef struct {
	uint32_t generation; // zero until first observation; zero again only after overflow
	uint32_t entityNumber;
	uint32_t eType;
	uint32_t modelindex;
	uint32_t modelindex2;
	uint32_t clientNum;
	uint32_t continuity;
	qboolean pendingDiscontinuity;
	qboolean disabled;
} cgTemporalIdentity_t;

typedef void (*cgTemporalOrdinarySubmitFn)( const refEntity_t *entity );
typedef void (*cgTemporalIdentitySubmitFn)( const refEntity_t *entity,
	const refEntityMotion_t *motion );

// Observes one authoritative snapshot state.  First sight seeds generation 1;
// discontinuities and semantic model changes advance it exactly once.  Overflow
// permanently disables temporal submission for this cgame lifetime rather than
// reusing a key.
qboolean CG_TemporalIdentityObserve( cgTemporalIdentity_t *identity,
	const entityState_t *state, uint32_t continuity, qboolean discontinuity );
void CG_TemporalIdentityMarkDiscontinuity( cgTemporalIdentity_t *identity );

// Output-atomic metadata materialization and exactly-one-submit adapter.  The
// adapter falls back to ordinary submission whenever metadata is unavailable.
qboolean CG_TemporalIdentityBuildMotion( const cgTemporalIdentity_t *identity,
	uint32_t entityNumber, refEntityMotionRole_t role,
	refEntityMotion_t *outMotion );
qboolean CG_TemporalIdentitySubmit( const cgTemporalIdentity_t *identity,
	uint32_t entityNumber, refEntityMotionRole_t role,
	const refEntity_t *entity,
	cgTemporalOrdinarySubmitFn ordinarySubmit,
	cgTemporalIdentitySubmitFn temporalSubmit );

#endif
