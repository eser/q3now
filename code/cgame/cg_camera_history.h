// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#pragma once

/* Game-side camera identity, not a renderer position/angle heuristic.
 * Stored in cg_t so level teardown resets it with the owning game module. */
typedef struct {
	int timeMs, clientNum, teleportBit, spawnCount;
	int thirdPerson, intermission, cinematic, sceneStartMs;
	int sceneCutRevision;
} wiredCameraSample_t;

typedef struct {
	wiredCameraSample_t previous;
	int valid, cut;
} wiredCameraHistory_t;

static inline int wired_camera_history_step( wiredCameraHistory_t *history,
		wiredCameraSample_t sample ) {
	const wiredCameraSample_t *previous = &history->previous;
	int cut = !history->valid || sample.timeMs < previous->timeMs
		|| sample.clientNum != previous->clientNum
		|| sample.teleportBit != previous->teleportBit
		|| sample.spawnCount != previous->spawnCount
		|| sample.thirdPerson != previous->thirdPerson
		|| sample.intermission != previous->intermission
		|| sample.cinematic != previous->cinematic
		|| sample.sceneStartMs != previous->sceneStartMs
		|| sample.sceneCutRevision != previous->sceneCutRevision;
	/* A repeated/stereo draw at the same scene time sees the same event. */
	if ( history->valid && sample.timeMs == previous->timeMs ) cut |= history->cut;
	history->previous = sample;
	history->valid = 1;
	history->cut = cut;
	return cut;
}
