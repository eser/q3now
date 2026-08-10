// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
===========================================================================
wired_scene.h -- cinematic-scene definition + Lua-table loader (plain-C)

A scene definition is a .lua file that RETURNS a table: one eye path (the
scene position spline), zero or more NAMED look-at target splines, an
optional FOV lerp, and a time-ordered list of decoded events (wait / target
switch / fov lerp / fade / feather / stop / chain). This module loads that
table into a wiredScene_t; it does NOT sample or play the scene (the
runtime evaluator that turns a time t into {origin, angles, fov} is a
separate module).

Scene definitions are Lua because Wired content authoring is Lua across the
board (menus, attract, procedural crosshairs, bot/character defs). The load +
table-read idiom mirrors the crosshair loader (cl_wired_crosshair.c): compile
the chunk, pcall it, verify the result is a table, read fields via lua_*.

The event vocabulary is the live subset of RealRTCW's idCameraEvent (the
meaning only): wait / target / fov / fadeout / fadein / feather / stop /
scene. The eye path + each target are wiredCurve_t (code/qcommon/wired/math),
populated via WiredCurve_Init + WiredCurve_AddValue.

Lua table shape:

    return {
        time = 12.0,                                  -- totalTimeSec
        cameraSpace = "world",                        -- or "player" (orbital, knots are offsets)
        eyePath = {
            type = "catmullrom",                      -- or "tcb"
            boundary = "clamped",                     -- free|clamped|closed
            knots = {
                { t = 0.0, pos = { 100, 200, 64 } },
                { t = 1.0, pos = { 700, 100, 96 }, tcb = { 0.5, -0.25, 0.75 } },
            },
        },
        targets = { hero = { type = "catmullrom", knots = { ... } }, },
        fov = { start = 90, ["end"] = 60, length = 3.0 },  -- `end` is a Lua keyword
        events = {
            { verb = "feather",     time = 0 },
            { verb = "thirdperson", time = 0,    on = 1 },       -- show player model
            { verb = "hud",         time = 0,    on = 0 },       -- hide HUD
            { verb = "playerfreeze",time = 0,    on = 1 },       -- suppress input
            { verb = "fov",         time = 2000, fov = 60, length = 3 },
            { verb = "caption",     time = 2000, key = "scene/intro/line1" },
            { verb = "target",      time = 3000, name = "hero" },
            { verb = "wait",        time = 5000, seconds = 2.0 },
            { verb = "stop",        time = 10000 },
            { verb = "scene",       time = 11000, name = "next" },
        },
    }

Pure CPU + Lua load: NO renderer, NO RAL. The Lua VM is used ONLY at load —
the wiredScene_t it fills is plain-C, and the future evaluator samples it in
engine-C (no per-frame VM crossing).
===========================================================================
*/
#ifndef WIRED_SCENE_H
#define WIRED_SCENE_H

#include "../math/wired_curve.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward-declared so this header needs no lua.h (callers that use the raw
   lua_State* reader include lua.h themselves). */
struct lua_State;

/* String field width for names / chain paths. Matches q3now MAX_QPATH (64)
   without pulling q_shared.h into this header. */
#ifndef WIRED_SCENE_NAME_LEN
#define WIRED_SCENE_NAME_LEN 64
#endif

/* Fixed bounds — no malloc, matching wired_curve's fixed-array style. */
#ifndef WIRED_MAX_SCENE_TARGETS
#define WIRED_MAX_SCENE_TARGETS 8
#endif
#ifndef WIRED_MAX_SCENE_EVENTS
#define WIRED_MAX_SCENE_EVENTS 64
#endif

/* Live event verbs (RealRTCW idCameraEvent, live semantics only). The dead
   verbs (speed / targetwait / snaptarget / cmd / trigger) are intentionally
   absent — a q3now cutscene does not need them. */
typedef enum {
	WSCENE_EV_WAIT = 0,   /* hold position for fparam seconds                 */
	WSCENE_EV_TARGET,     /* switch active look-at to the target named sparam */
	WSCENE_EV_FOV,        /* lerp FOV to fparam degrees over fparam2 seconds  */
	WSCENE_EV_FADEOUT,    /* fade screen to black over fparam seconds         */
	WSCENE_EV_FADEIN,     /* fade up from black over fparam seconds           */
	WSCENE_EV_FEATHER,    /* ease-in/out velocity ramp at path start + end    */
	WSCENE_EV_STOP,       /* end the cutscene                                 */
	WSCENE_EV_SCENE,      /* chain to the .scene file named sparam           */
	/* director events — client-view-only state toggles (fparam = 1 on / 0 off) */
	WSCENE_EV_THIRDPERSON,/* fparam: 1 = force third-person, 0 = first-person  */
	WSCENE_EV_HUD,        /* fparam: 1 = show HUD, 0 = hide HUD                */
	WSCENE_EV_PLAYERFREEZE,/* fparam: 1 = suppress player input, 0 = restore   */
	WSCENE_EV_CAPTION     /* sparam: l10n key (sound-name-as-subtitle-key);
	                         fparam: optional display seconds (0 = default)    */
} wiredSceneEventType_t;

/* Camera space for the eye path + target knots. World = knots are absolute
   world coordinates (the default, unchanged). Player = knots are OFFSETS from
   the player anchor: the evaluator rotates each sampled offset's XY by the
   anchor yaw (orbital, over-the-shoulder) and adds the anchor origin. */
typedef enum {
	WSCENE_SPACE_WORLD = 0,   /* knots are absolute world coords (default)     */
	WSCENE_SPACE_PLAYER       /* knots are player-relative offsets (orbital)   */
} wiredSceneSpace_t;

/* A decoded event. timeMs is the offset from scene start (ms). fparam /
   fparam2 hold the numeric payload (fparam2 only for FOV's length); sparam
   holds the string payload (TARGET name, SCENE chain name). Unused fields
   are zero. */
typedef struct {
	int   type;                       /* wiredSceneEventType_t             */
	int   timeMs;
	float fparam;                     /* seconds / degrees                  */
	float fparam2;                    /* FOV lerp length (seconds)          */
	char  sparam[WIRED_SCENE_NAME_LEN]; /* TARGET / SCENE name               */
} wiredSceneEvent_t;

/* A named look-at target: its name + its position spline. */
typedef struct {
	char         name[WIRED_SCENE_NAME_LEN];
	wiredCurve_t path;
} wiredSceneTarget_t;

/* A loaded cinematic scene definition. */
typedef struct {
	float               totalTimeSec;   /* `time` — total path duration     */

	wiredCurve_t        eyePath;        /* the scene position spline       */

	wiredSceneTarget_t targets[WIRED_MAX_SCENE_TARGETS];
	int                 numTargets;

	int                 hasFov;         /* nonzero if a `fov` table present  */
	float               fovStart;       /* degrees                          */
	float               fovEnd;         /* degrees                          */
	float               fovLenSec;      /* lerp length (seconds)            */

	wiredSceneEvent_t  events[WIRED_MAX_SCENE_EVENTS];
	int                 numEvents;      /* in file order; a later step sorts */

	int                 cameraSpace;    /* wiredSceneSpace_t (0=world default) */
} wiredScene_t;

/* ── load ─────────────────────────────────────────────────────────────── */

/* Read a scene-definition table (already on top of L's stack) into `out`.
   `out` is zero-initialised internally. `name` is used only for error
   messages. The table is LEFT on the stack (caller pops). Pure lua_* reads —
   VM-agnostic (the caller may use any Lua state; the unit test uses a minimal
   luaL_newstate). Missing optional fields default cleanly; a structurally
   wrong value (e.g. eyePath not a table) logs and yields an empty curve but
   does not crash. Returns 1 on success, 0 if the top of stack is not a table. */
int WiredScene_ReadTable( wiredScene_t *out, struct lua_State *L, const char *name );

/* Load a scene .lua file through the engine's WiredScript VM: FS-read the
   file, compile + pcall the chunk (nargs=0, nresults=1), verify the returned
   value is a table, then WiredScene_ReadTable it. Returns 1 on success. Not
   built in the standalone unit-test configuration (no engine FS / WiredScript). */
#ifndef WIRED_SCENE_STANDALONE
int WiredScene_LoadFromFile( wiredScene_t *out, const char *path );
#endif

#ifdef __cplusplus
}
#endif

#endif /* WIRED_SCENE_H */
