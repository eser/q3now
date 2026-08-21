// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#ifndef WIRED_RAL_CAPABILITY_H
#define WIRED_RAL_CAPABILITY_H

#include "ral_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_CAPABILITY_PROFILE_SCHEMA_VERSION 1u

typedef enum {
	RAL_CAP_DYNAMIC_RENDERING = 0,
	RAL_CAP_GRAPHICS_QUEUE,
	RAL_CAP_BIND_GROUPS,
	RAL_CAP_STORAGE_TEXTURES,
	RAL_CAP_MAX_COLOR_ATTACHMENTS,
	RAL_CAP_MAX_TEXTURE_2D,
	RAL_CAP_MAX_BINDINGS_PER_GROUP,
	RAL_CAP_BINDING_ARRAYS,
	RAL_CAP_INLINE_DATA,
	RAL_CAP_SUBMISSION_TIMELINE,
	RAL_CAP_ASYNC_COMPUTE,
	RAL_CAP_ASYNC_TRANSFER,
	RAL_CAP_DRAW_INDIRECT_COUNT,
	RAL_CAP_VARIABLE_RATE_SHADING,
	RAL_CAP_HDR10_PRESENTATION,
	RAL_CAP_TEXTURE_COMPRESSION_BC,
	RAL_CAP_TEXTURE_COMPRESSION_ASTC,
	RAL_CAP_TEXTURE_COMPRESSION_ETC2,
	RAL_CAP_COUNT
} ralCapabilityId_t;

typedef enum {
	RAL_CAP_REQUIREMENT_REQUIRED = 1,
	RAL_CAP_REQUIREMENT_OPTIONAL = 2
} ralCapabilityRequirement_t;

typedef enum {
	RAL_CAP_OUTCOME_NATIVE = 1,
	RAL_CAP_OUTCOME_EMULATED,
	RAL_CAP_OUTCOME_DISABLED
} ralCapabilityOutcome_t;

typedef struct {
	ralCapabilityId_t id;
	qboolean nativeSupport;
	qboolean emulationSupport;
	uint64_t nativeLimit;
	uint64_t emulationLimit;
} ralCapabilityFact_t;

typedef struct {
	ralCapabilityId_t id;
	ralCapabilityRequirement_t requirement;
	ralCapabilityOutcome_t outcome;
	uint64_t limit;
} ralCapabilityEntry_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t generation;
	ralCapabilityEntry_t entries[RAL_CAP_COUNT];
	uint32_t entryCount;
	qboolean ready;
} ralCapabilityProfile_t;

qboolean Ral_CapabilityProfileBuild( ralBackendType_t backendType,
	uint64_t generation, const ralCapabilityFact_t *facts, uint32_t factCount,
	ralCapabilityProfile_t *out );
qboolean Ral_CapabilityProfileFromCaps( ralBackendType_t backendType,
	const ralCaps_t *caps, uint64_t generation, ralCapabilityProfile_t *out );
qboolean Ral_CapabilityProfileExact( const ralCapabilityProfile_t *a,
	const ralCapabilityProfile_t *b );

#ifdef __cplusplus
}
#endif

#endif
