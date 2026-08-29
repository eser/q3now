// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_EFFECT_RUNTIME_H
#define WIRED_RENDER_FRONTEND_EFFECT_RUNTIME_H

#include "render_effect_profile.h"

#define WIRED_FX_MAX_ACTIVE_EVENTS 128u

typedef enum {
	WIRED_FX_DISPATCH_EXECUTED = 0,
	WIRED_FX_DISPATCH_UNSUPPORTED,
	WIRED_FX_DISPATCH_DROPPED
} wiredFxDispatchResult_t;

typedef wiredFxDispatchResult_t ( *wiredFxDispatchSink_t )(
	const wiredFxProfile_t *profile, const wiredFxAction_t *action,
	const wiredFxEvent_t *event, uint32_t actionIndex, uint32_t cycle,
	float scheduledTimeSeconds, void *userData );

typedef struct {
	wiredFxEvent_t event;
	uint32_t triggeredMask;
	uint32_t doneMask;
	uint32_t lastCycle[WIRED_FX_MAX_ACTIONS];
	float triggerTime[WIRED_FX_MAX_ACTIONS];
	qboolean active;
} wiredFxEventInstance_t;

typedef struct {
	wiredFxEventInstance_t events[WIRED_FX_MAX_ACTIVE_EVENTS];
	uint32_t activeCount;
	uint64_t admittedEvents;
	uint64_t droppedEvents;
	uint64_t duplicateEvents;
	uint64_t dispatchedActions;
	uint64_t unsupportedActions;
	uint64_t droppedActions;
	uint64_t conditionCulls;
	uint64_t lodCulls;
	uint64_t expiredEvents;
} wiredFxRuntime_t;

void WiredFx_InitRuntime( wiredFxRuntime_t *runtime );
qboolean WiredFx_SubmitEvent( wiredFxRuntime_t *runtime, const wiredFxRegistry_t *registry,
	const wiredFxEvent_t *event );
uint32_t WiredFx_Service( wiredFxRuntime_t *runtime, const wiredFxRegistry_t *registry,
	float nowSeconds, const float viewOrigin[3], wiredFxDispatchSink_t sink, void *userData );

#endif

