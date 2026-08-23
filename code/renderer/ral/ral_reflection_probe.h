// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_REFLECTION_PROBE_H
#define WIRED_RAL_REFLECTION_PROBE_H

#include "ral_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_REFLECTION_PROBE_SCHEMA_VERSION 1u
#define RAL_REFLECTION_PROBE_QUERY_SCHEMA_VERSION 1u
#define RAL_REFLECTION_PROBE_RECEIPT_SCHEMA_VERSION 1u
#define RAL_REFLECTION_PROBE_MAX_LOCAL 8u
#define RAL_REFLECTION_PROBE_MAX_SELECTED 2u
#define RAL_REFLECTION_PROBE_Q16_ONE 65536

typedef struct { int32_t x, y, z; } ralProbeVec3Q16_t;

typedef struct {
	uint64_t probeId;
	uint64_t probeGeneration;
	uint32_t priority;
	ralProbeVec3Q16_t boundsMin;
	ralProbeVec3Q16_t boundsMax;
	ralProbeVec3Q16_t capturePosition;
	int32_t blendDistanceQ16;
	ralTextureResourceReceipt_t cubemap;
} ralLocalReflectionProbe_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t catalogGeneration;
	ralTextureResourceReceipt_t globalCubemap;
	uint32_t localCount;
	ralLocalReflectionProbe_t locals[RAL_REFLECTION_PROBE_MAX_LOCAL];
	qboolean ready;
} ralReflectionProbeCatalog_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t catalogGeneration;
	uint64_t queryGeneration;
	ralProbeVec3Q16_t position;
	ralProbeVec3Q16_t reflectionDirection;
} ralReflectionProbeQuery_t;

typedef struct {
	uint64_t probeId;
	uint64_t probeGeneration;
	uint32_t weightQ16;
	ralProbeVec3Q16_t parallaxDirection;
	ralTextureResourceReceipt_t cubemap;
} ralReflectionProbeSelection_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t catalogGeneration;
	uint64_t queryGeneration;
	ralTextureResourceReceipt_t globalCubemap;
	uint32_t globalWeightQ16;
	uint32_t selectedCount;
	ralReflectionProbeSelection_t selected[RAL_REFLECTION_PROBE_MAX_SELECTED];
	qboolean ready;
} ralReflectionProbeReceipt_t;

qboolean Ral_ReflectionProbeCatalogBuild( const ralTextureResourceReceipt_t *globalCubemap,
	const ralLocalReflectionProbe_t *locals, uint32_t localCount,
	uint64_t catalogGeneration, ralReflectionProbeCatalog_t *outCatalog );
qboolean Ral_ReflectionProbeCatalogValid( const ralReflectionProbeCatalog_t *catalog );
qboolean Ral_ReflectionProbeResolve( const ralReflectionProbeCatalog_t *catalog,
	const ralReflectionProbeQuery_t *query, uint32_t outputCapacity,
	ralReflectionProbeReceipt_t *outReceipt );
qboolean Ral_ReflectionProbeReceiptValid( const ralReflectionProbeReceipt_t *receipt );
qboolean Ral_ReflectionProbeReceiptExact( const ralReflectionProbeReceipt_t *a,
	const ralReflectionProbeReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
