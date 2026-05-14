// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// bg_local.h -- local definitions for the bg (both games) files

#define	MIN_WALK_NORMAL	0.7f		// can't walk on very steep slopes

#define	STEPSIZE		18

#define	TIMER_LAND		130
#define	TIMER_GESTURE	(34*66+50)

#define	OVERCLIP		1.001f

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server
typedef struct {
	vec3_t		forward, right, up;
	float		frametime;

	int			msec;

	qboolean	walking;
	qboolean	groundPlane;
	trace_t		groundTrace;

	float		impactSpeed;

	vec3_t		previous_origin;
	vec3_t		previous_velocity;
	int			previous_waterlevel;
} pml_t;

extern	pmove_t		*pm;
extern	pml_t		pml;

// movement parameters
extern	float	pm_stopSpeed;
extern	float	pm_duckScale;
extern	float	pm_swimScale;

extern	float	pm_accelerate;
extern	float	pm_airAccelerate;
extern	float	pm_waterAccelerate;
extern	float	pm_flyAccelerate;

extern	float	pm_friction;
extern	float	pm_waterFriction;
extern	float	pm_flightFriction;

extern	float	pm_rampboost;
extern	float	pm_rampboostMin;
extern	float	pm_rampboostMax;
extern	float	pm_stairBunnyHopMin;
extern  int     pm_overbounceThreshold;

extern	int		c_pmove;

void PM_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce );
void PM_OneSidedClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce );
void PM_SlopeBoostClip( vec3_t in, vec3_t normal, vec3_t out, float boost, qboolean allowOverbounce );
void PM_AddTouchEnt( int entityNum );
void PM_AddEvent( int newEvent );

qboolean	PM_SlideMove( qboolean gravity );
void		PM_StepSlideMove( qboolean gravity );
