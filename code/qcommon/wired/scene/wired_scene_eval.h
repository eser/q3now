// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

/*
===========================================================================
wired_scene_eval.h -- cinematic-scene evaluator (sample -> view) (plain-C)

Turns a loaded wiredScene_t (see wired_scene.h) into a per-frame view: given
a clock time, produce {origin, angles, fov}. This is a PURE SAMPLER — it reads
the immutable scene definition + a small runtime cursor and computes the view;
it does not touch the renderer, the refdef, the view code, or the script VM.
Nothing consumes its output yet; a later step feeds {origin, angles, fov} into
the view.

Two calls, mirroring the data-vs-eval split of wired_curve / wired_scene:

  WiredScenePlayback_Start( pb, def, startMs )   -- one-time, at play start.
      Binds the definition, arc-length-reparameterizes the eye path so it is
      sampled directly in seconds by the playback clock, time-orders the events,
      and resets the runtime cursor (active target, current fov, signals).
      NOTE: this MUTATES def->eyePath.times[] (the constant-speed dolly reparam
      is destructive) — call it once per playback, never per frame.

  WiredScene_Eval( pb, nowMs, origin, angles, fov )   -- per frame, pure.
      elapsed = nowMs - startMs, warped by any active WAIT hold, samples the eye
      path, computes facing (eye tangent, or look-at toward the active target),
      resolves fov (authored baseline lerp, overridden by a fired FOV event),
      advances the active target from TARGET events, and surfaces any pending
      FADEOUT / FADEIN / SCENE / STOP as signals it does NOT act on. Returns
      whether the cutscene is still running.

The clock is PULLED IN (nowMs), never read from a global: the evaluator is pure
and the playback state is per-instance, so several scenes (e.g. multiple app
instances) can run independently and the whole thing is unit-testable with no
engine. The angles output is the vectoangles form the view seam consumes
(AnglesToAxis(angles) -> viewaxis); there is no authored roll (ROLL = 0).

Pure CPU math: NO renderer, NO RAL, NO VM. Uses only the wired_curve sampler +
q_math's vectoangles. Standalone-guardable (-DWIRED_SCENE_STANDALONE) for the
leaf unit test, exactly like wired_scene.c.
===========================================================================
*/
#ifndef WIRED_SCENE_EVAL_H
#define WIRED_SCENE_EVAL_H

#include "wired_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host-facing signals the evaluator RAISES but does not act on. A later step
   (the view/host layer) reads these and performs the screen fade / scene
   chain. Bitmask so several can be pending in one frame. */
#define WIRED_SCENE_SIGNAL_NONE     0x00
#define WIRED_SCENE_SIGNAL_FADEOUT  0x01   /* start a fade to black (seconds in fadeSeconds) */
#define WIRED_SCENE_SIGNAL_FADEIN   0x02   /* start a fade up from black (seconds in fadeSeconds) */
#define WIRED_SCENE_SIGNAL_CHAIN    0x04   /* load+play the scene named chainName */
#define WIRED_SCENE_SIGNAL_STOP     0x08   /* authored end (WSCENE_EV_STOP) reached */

/* Runtime cursor for one playing scene. `def` is the immutable loaded
   definition (Start reparameterizes its eyePath once); every other field is
   the per-frame/per-instance evaluation state. Fixed-size, no malloc. */
typedef struct {
	const wiredScene_t *def;

	int   startMs;            /* wall-clock ms of playback start (Eval subtracts) */
	int   active;            /* nonzero once Start ran and before STOP/IsDone     */

	/* events time-ordered into evOrder[] (indices into def->events[]) so Eval can
	   walk them by increasing timeMs without mutating the definition. */
	int   evOrder[WIRED_MAX_SCENE_EVENTS];
	int   numEvents;

	/* active look-at target: index into def->targets[], or -1 for "no target"
	   (face along the eye-path tangent). RE-DERIVED from the TARGET events each
	   Eval (reset to -1 at the top of the pass), so rewinding the clock un-does
	   a later TARGET switch. */
	int   curTargetIndex;

	/* fov resolution — the fovEvent* group is RE-DERIVED each Eval (fovEventActive
	   reset to 0 at the top of the pass, then rebuilt from the FOV events whose
	   time has passed). When no FOV event has fired the output is the authored
	   baseline lerp (fovStart -> fovEnd over fovLenSec); once one fires these hold
	   its lerp (from the fov at fire time, to fovEventTarget, over fovEventLenSec,
	   from fovEventStartMs). */
	int   fovEventActive;     /* nonzero once a WSCENE_EV_FOV has fired             */
	float fovEventFrom;       /* fov value at the moment the event fired          */
	float fovEventTarget;     /* WSCENE_EV_FOV fparam (target degrees)              */
	float fovEventLenSec;     /* WSCENE_EV_FOV fparam2 (lerp seconds)               */
	int   fovEventStartMs;    /* the event's timeMs (elapsed ms at fire)          */
	float curFov;             /* last resolved fov, written each Eval (output cache) */

	/* WAIT time-warp bookkeeping: the eye path is sampled at (wall - heldTime),
	   where heldTime is the measure of the UNION of all passed WAIT hold windows
	   (a union, not a sum, so overlapping waits hold at most 1:1 and the clock
	   stays monotone). accumulatedWaitSec reports the FULLY-elapsed part of that
	   union — the permanent path-time shift after the current sample. */
	float accumulatedWaitSec;

	/* last valid facing, reused when the current facing degenerates — zero OR
	   non-finite (single knot, coincident-time knots giving a NaN/Inf tangent,
	   or a target on the eye) — so the view never snaps to a default or a NaN. */
	float lastForward[3];
	int   haveLastForward;

	/* host signals raised this run (bitmask of WIRED_SCENE_SIGNAL_*). Sticky:
	   a signal, once raised, stays set (edge-triggered latch — the host drains
	   it). fadeSeconds / chainName carry the latest fade/chain payload. */
	int   signals;
	float fadeSeconds;                        /* payload for FADEOUT / FADEIN     */
	char  chainName[WIRED_SCENE_NAME_LEN];      /* payload for CHAIN                */

	/* director state — RE-DERIVED each Eval (like the target/fov cursor) from the
	   THIRDPERSON / HUD / PLAYERFREEZE events whose time has passed. The host
	   (CG_SceneView) reads these and drives the render/input seams; all revert
	   automatically when the scene ends. */
	int   thirdPerson;      /* nonzero = force third-person (show player model)   */
	int   hudVisible;       /* nonzero = HUD shown (default 0 = hidden in a scene) */
	int   playerFrozen;     /* nonzero = suppress player input this frame          */

	/* caption edge: when a CAPTION event newly fires this Eval, captionKey holds
	   its l10n key and captionFired is set so the host logs/shows it once. */
	int   captionFired;
	char  captionKey[WIRED_SCENE_NAME_LEN];

	/* runtime actor binding: actorEntity[i] is a live entity number bound to look-
	   at target slot i, or -1 (unbound = the target uses its authored spline, the
	   byte-identical default). A bound target's look-at position is overridden per
	   frame with the live entity's origin (supplied by the host via the actorOrigins
	   param to EvalAnchored — the eval never references any cg/game state itself).
	   Set once by WiredScenePlayback_BindActors after Start; reset to all -1 by
	   Start so a scene played without binding is unchanged. */
	int   actorEntity[WIRED_MAX_SCENE_TARGETS];
} wiredScenePlayback_t;

/* ── one-time setup ───────────────────────────────────────────────────────
   Bind `def`, reparameterize its eye path to seconds via
   WiredCurve_SetConstantSpeed, rescale each look-at target's times[] onto the
   same 0..totalTimeSec seconds clock, time-order the events, and reset the
   cursor (curTargetIndex = -1, curFov = authored fovStart if a fov block is
   present else a sane default, no signals). `startMs` is the playback clock
   origin. `def` is NON-const because this MUTATES def->eyePath.times[] (and
   each target's times[]) — the constant-speed dolly reparam is destructive; do
   not call twice on the same def without reloading it. Eval only reads def. */
void WiredScenePlayback_Start( wiredScenePlayback_t *pb, wiredScene_t *def, int startMs );

/* ── per-frame sample ─────────────────────────────────────────────────────
   Sample the scene at wall-clock `nowMs`. Writes origin[3] (world position),
   angles[3] (PITCH,YAW,ROLL — ROLL always 0; feed AnglesToAxis for viewaxis),
   and *fov (horizontal degrees). Returns nonzero while the cutscene is running,
   0 once STOP fired or the eye path is done (WiredCurve_IsDone). Pure: reads pb
   + def, writes pb's cursor/signals/director-state and the out-params only. Safe
   to call at any nowMs, including before startMs (clamps to the path start) and
   past the end (clamps to the path end). out_angles / out_fov may not be NULL;
   origin may not be NULL.

   Anchored form: for a def->cameraSpace == WSCENE_SPACE_PLAYER scene, the eye
   path + look-at knots are OFFSETS from the player anchor — each sampled point's
   XY is rotated by anchorYaw (orbital) and translated by anchorOrigin, so the
   camera orbits the player over-the-shoulder. For WSCENE_SPACE_WORLD (default)
   the anchor is ignored and the knots are absolute world coords (byte-identical
   to the plain form). anchorOrigin may be NULL for world scenes. Pure: no engine
   dependency — the anchor is a bare float[3] + float, never a cg reference.

   Actor binding: actorOrigins, when non-NULL, is an array of WIRED_MAX_SCENE_TARGETS
   live entity origins in WORLD space (actorOrigins[i] is the current origin of the
   entity bound to target slot i via WiredScenePlayback_BindActors; unbound slots are
   ignored). When the active look-at target's slot is bound (pb->actorEntity[slot] >= 0),
   its look-at point is taken from actorOrigins[slot] instead of the authored spline, so
   the camera tracks the live entity. actorOrigins may be NULL (no binding) — then every
   target uses its spline, byte-identical to before. The eval stays pure: origins are bare
   floats supplied by the host, never a cg/entity reference. */
int WiredScene_EvalAnchored( wiredScenePlayback_t *pb, int nowMs,
                             const float anchorOrigin[3], float anchorYaw,
                             const float (*actorOrigins)[3], const int *actorValid,
                             float out_origin[3], float out_angles[3], float *out_fov );

/* World-space wrapper: WiredScene_EvalAnchored with no anchor and no actor binding.
   Kept for the existing callers + the world unit tests (identical behavior). */
int WiredScene_Eval( wiredScenePlayback_t *pb, int nowMs,
                      float out_origin[3], float out_angles[3], float *out_fov );

/* Bind live entities to the scene's look-at target slots (positional: actor arg i
   -> target slot i). Call once after WiredScenePlayback_Start. entityNums[i] < 0 (or
   i >= def->numTargets) leaves that slot unbound (spline). nActors is clamped to
   WIRED_MAX_SCENE_TARGETS. A bound slot's look-at follows the live entity origin the
   host feeds EvalAnchored. Never call before Start (Start resets the bindings). */
void WiredScenePlayback_BindActors( wiredScenePlayback_t *pb, const int *entityNums, int nActors );

#ifdef __cplusplus
}
#endif

#endif /* WIRED_SCENE_EVAL_H */
