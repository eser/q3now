/*
===========================================================================
ai_movement.h — Bot movement system

Structs and declarations for all five movement components.
Include this before bot_state_t is defined (via ai_main.h) so that
bot_doublejump_t and bot_walljump_t can be embedded in bot_state_t.
===========================================================================
*/
#ifndef AI_MOVEMENT_H
#define AI_MOVEMENT_H

/* Trajectory simulation parameters */
#define TRAJ_STEP_MS   8      /* simulate at 125 Hz (8 ms steps)       */
#define TRAJ_MAX_STEPS 250    /* max 2 seconds of prediction            */

/*
 * bot_trajectory_t — result of BotPredictTrajectory().
 * origin/velocity are the final state; landed is set when the bot
 * dropped below startOrigin[2] with negative z-velocity.
 */
typedef struct {
	vec3_t   origin;
	vec3_t   velocity;
	float    time;       /* total simulation time elapsed (ms)    */
	qboolean landed;     /* qtrue if landing was detected         */
} bot_trajectory_t;

/*
 * bot_doublejump_t — tracks the two-jump sequence for high platforms.
 * Embedded in bot_state_t.
 */
typedef struct {
	qboolean wantDoubleJump;  /* currently attempting a double jump    */
	qboolean firstJumpDone;   /* first jump has been issued this seq   */
	int      landTime;        /* level.time when bot landed after j1   */
} bot_doublejump_t;

/*
 * bot_walljump_t — tracks wall detection and jump-off execution.
 * Embedded in bot_state_t.
 */
typedef struct {
	qboolean wantWallJump;   /* currently attempting a wall jump      */
	vec3_t   wallNormal;     /* outward normal of the detected wall   */
	vec3_t   wallPoint;      /* trace endpos on the wall surface      */
} bot_walljump_t;

/* Forward declarations for function signatures — full definitions are  */
/* available to callers via their normal include chains.                 */
struct bot_state_s;
struct bot_moveresult_s;

/* ── Component 1: trajectory predictor ─────────────────────────────── */
bot_trajectory_t BotPredictTrajectory(
	const vec3_t startOrigin,
	const vec3_t startVelocity,
	const vec3_t wishdir,      /* normalized 2D forward direction      */
	float        forwardmove,  /* −127 to 127                           */
	float        rightmove,    /* −127 to 127                           */
	float        gravity );    /* typically 800                          */

/* ── Component 2: air steering ──────────────────────────────────────── */
void BotAirSteer( struct bot_state_s *bs );

/* ── Component 4: double jump ───────────────────────────────────────── */
qboolean BotShouldDoubleJump( struct bot_state_s *bs );
void     BotDoubleJumpThink(  struct bot_state_s *bs );

/* ── Component 5: wall jump ─────────────────────────────────────────── */
qboolean BotFindWallJumpOpportunity( struct bot_state_s *bs );
void     BotWallJumpThink(          struct bot_state_s *bs );

/* ── Master controller ──────────────────────────────────────────────── */
void BotMovementThink( struct bot_state_s *bs,
                       struct bot_moveresult_s *moveresult );

#endif /* AI_MOVEMENT_H */
