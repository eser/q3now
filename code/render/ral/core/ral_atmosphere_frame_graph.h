// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_ATMOSPHERE_FRAME_GRAPH_H
#define WIRED_RAL_ATMOSPHERE_FRAME_GRAPH_H

#include "ral_atmosphere.h"
#include "frame_graph/ral_frame_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_ATMOSPHERE_FRAME_GRAPH_SCHEMA_VERSION 1u

#define RAL_ATMOSPHERE_PASS_ID_MEDIA_INJECT 100u
#define RAL_ATMOSPHERE_PASS_ID_LIGHT_INJECT 200u
#define RAL_ATMOSPHERE_PASS_ID_CLOUD_INJECT 250u
#define RAL_ATMOSPHERE_PASS_ID_INTEGRATE    300u
#define RAL_ATMOSPHERE_PASS_ID_COMPOSITE    400u

#define RAL_ATMOSPHERE_RESOURCE_SCENE_HDR        1u
#define RAL_ATMOSPHERE_RESOURCE_SCENE_DEPTH      2u
#define RAL_ATMOSPHERE_RESOURCE_MEDIA            3u
#define RAL_ATMOSPHERE_RESOURCE_LIT_MEDIA        4u
#define RAL_ATMOSPHERE_RESOURCE_INTEGRATED       5u
#define RAL_ATMOSPHERE_RESOURCE_HISTORY_PREVIOUS 6u
#define RAL_ATMOSPHERE_RESOURCE_HISTORY_NEXT     7u
#define RAL_ATMOSPHERE_RESOURCE_COMPOSED_HDR     8u
#define RAL_ATMOSPHERE_RESOURCE_CLOUD_MEDIA      9u

typedef struct {
	uint32_t schemaVersion;
	uint64_t generation;
	uintptr_t resourceIdentityBase;
	uint64_t transientBudgetBytes;
	ralAtmospherePlanReceipt_t atmosphere;
} ralAtmosphereFrameGraphRequest_t;

qboolean Ral_AtmosphereFrameGraphBuild(
	const ralAtmosphereFrameGraphRequest_t *request,
	ralFrameGraphPlan_t *outPlan );

#ifdef __cplusplus
}
#endif

#endif
