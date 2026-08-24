// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_capability.h"

#include <limits.h>
#include <string.h>

typedef struct {
	ralCapabilityRequirement_t requirement;
	qboolean allowEmulation;
	uint64_t minimum;
} ralCapabilityPolicy_t;

static const ralCapabilityPolicy_t s_policy[RAL_CAP_COUNT] = {
	{ RAL_CAP_REQUIREMENT_REQUIRED, qfalse, 1u },    // dynamic rendering
	{ RAL_CAP_REQUIREMENT_REQUIRED, qfalse, 1u },    // graphics queue
	{ RAL_CAP_REQUIREMENT_REQUIRED, qfalse, 1u },    // bind groups
	{ RAL_CAP_REQUIREMENT_REQUIRED, qfalse, 1u },    // storage textures
	{ RAL_CAP_REQUIREMENT_REQUIRED, qfalse, 4u },    // MRT
	{ RAL_CAP_REQUIREMENT_REQUIRED, qfalse, 4096u }, // 2D texture size
	{ RAL_CAP_REQUIREMENT_REQUIRED, qfalse, 8u },    // bindings per group
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qtrue, 1u },     // binding arrays
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qtrue, 128u },   // push/uniform inline bytes
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qtrue, 1u },     // submission timeline
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qtrue, 1u },     // shared graphics fallback
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qtrue, 1u },     // shared graphics fallback
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qfalse, 1u },    // indirect count
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qfalse, 1u },    // VRS
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qfalse, 1u },    // HDR10
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qtrue, 1u },     // decode to RGBA
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qtrue, 1u },     // decode to RGBA
	{ RAL_CAP_REQUIREMENT_OPTIONAL, qtrue, 1u },     // decode to RGBA
};

static qboolean BoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

qboolean Ral_CapabilityProfileBuild( ralBackendType_t backendType,
		uint64_t generation, const ralCapabilityFact_t *facts, uint32_t factCount,
		ralCapabilityProfile_t *out ) {
	ralCapabilityProfile_t candidate;
	uint32_t i;
	if ( !out || !facts || factCount != RAL_CAP_COUNT
			|| backendType < RAL_BACKEND_VULKAN || backendType >= RAL_BACKEND_COUNT
			|| generation == 0 || generation == UINT64_MAX ) return qfalse;
	memset(&candidate,0,sizeof(candidate));
	candidate.schemaVersion = RAL_CAPABILITY_PROFILE_SCHEMA_VERSION;
	candidate.backendType = backendType;
	candidate.generation = generation;
	candidate.entryCount = RAL_CAP_COUNT;
	for (i=0u;i<RAL_CAP_COUNT;++i) {
		const ralCapabilityPolicy_t *policy = &s_policy[i];
		const ralCapabilityFact_t *fact = &facts[i];
		ralCapabilityEntry_t *entry = &candidate.entries[i];
		const qboolean nativeEnough = fact->nativeSupport && fact->nativeLimit >= policy->minimum;
		const qboolean emulatedEnough = fact->emulationSupport && fact->emulationLimit >= policy->minimum;
		if ( fact->id != (ralCapabilityId_t)i || !BoolValid(fact->nativeSupport)
				|| !BoolValid(fact->emulationSupport) ) return qfalse;
		entry->id = fact->id; entry->requirement = policy->requirement;
		if ( nativeEnough ) {
			entry->outcome = RAL_CAP_OUTCOME_NATIVE;
			entry->limit = fact->nativeLimit;
		} else if ( policy->allowEmulation && emulatedEnough ) {
			entry->outcome = RAL_CAP_OUTCOME_EMULATED;
			entry->limit = fact->emulationLimit;
		} else if ( policy->requirement == RAL_CAP_REQUIREMENT_OPTIONAL ) {
			entry->outcome = RAL_CAP_OUTCOME_DISABLED;
			entry->limit = 0u;
		}
		else return qfalse;
	}
	candidate.ready = qtrue;
	*out = candidate;
	return qtrue;
}

static void Fact( ralCapabilityFact_t *facts, ralCapabilityId_t id,
		qboolean nativeSupport, uint64_t nativeLimit,
		qboolean emulationSupport, uint64_t emulationLimit ) {
	facts[id].id=id; facts[id].nativeSupport=nativeSupport;
	facts[id].nativeLimit=nativeLimit; facts[id].emulationSupport=emulationSupport;
	facts[id].emulationLimit=emulationLimit;
}

qboolean Ral_CapabilityProfileFromCaps( ralBackendType_t backendType,
		const ralCaps_t *caps, uint64_t generation, ralCapabilityProfile_t *out ) {
	ralCapabilityFact_t facts[RAL_CAP_COUNT];
	if ( !caps ) return qfalse;
	memset(facts,0,sizeof(facts));
	Fact(facts,RAL_CAP_DYNAMIC_RENDERING,caps->dynamicRendering,1u,qfalse,0u);
	Fact(facts,RAL_CAP_GRAPHICS_QUEUE,qtrue,1u,qfalse,0u);
	Fact(facts,RAL_CAP_BIND_GROUPS,qtrue,1u,qfalse,0u);
	Fact(facts,RAL_CAP_STORAGE_TEXTURES,qtrue,1u,qfalse,0u);
	Fact(facts,RAL_CAP_MAX_COLOR_ATTACHMENTS,qtrue,caps->maxColorAttachments,qfalse,0u);
	Fact(facts,RAL_CAP_MAX_TEXTURE_2D,qtrue,caps->maxTextureDimension2D,qfalse,0u);
	Fact(facts,RAL_CAP_MAX_BINDINGS_PER_GROUP,qtrue,64u,qfalse,0u);
	Fact(facts,RAL_CAP_BINDING_ARRAYS,caps->bindlessTextures,caps->maxBindlessTextures,
		qtrue,64u);
	// Only Vulkan exposes native push constants through this legacy caps field.
	// Metal/WebGPU/GL lower the same portable inline-data intent to a
	// backend-owned constant or uniform buffer and must report EMULATED.
	Fact(facts,RAL_CAP_INLINE_DATA,
		backendType == RAL_BACKEND_VULKAN && caps->maxPushConstantSize>0u,
		backendType == RAL_BACKEND_VULKAN ? caps->maxPushConstantSize : 0u,
		qtrue,caps->maxPushConstantSize >= 128u ? caps->maxPushConstantSize : 256u);
	Fact(facts,RAL_CAP_SUBMISSION_TIMELINE,caps->timelineSemaphores,1u,qtrue,1u);
	Fact(facts,RAL_CAP_ASYNC_COMPUTE,caps->asyncCompute,1u,qtrue,1u);
	Fact(facts,RAL_CAP_ASYNC_TRANSFER,caps->asyncTransfer,1u,qtrue,1u);
	Fact(facts,RAL_CAP_DRAW_INDIRECT_COUNT,caps->drawIndirectCount,1u,qfalse,0u);
	Fact(facts,RAL_CAP_VARIABLE_RATE_SHADING,caps->variableRateShading,1u,qfalse,0u);
	Fact(facts,RAL_CAP_HDR10_PRESENTATION,caps->hdr10Swapchain,1u,qfalse,0u);
	Fact(facts,RAL_CAP_TEXTURE_COMPRESSION_BC,caps->textureCompressionBC,1u,qtrue,1u);
	Fact(facts,RAL_CAP_TEXTURE_COMPRESSION_ASTC,caps->textureCompressionASTC,1u,qtrue,1u);
	Fact(facts,RAL_CAP_TEXTURE_COMPRESSION_ETC2,caps->textureCompressionETC2,1u,qtrue,1u);
	return Ral_CapabilityProfileBuild(backendType,generation,facts,RAL_CAP_COUNT,out);
}

static qboolean ProfileValid( const ralCapabilityProfile_t *profile ) {
	uint32_t i;
	if ( !profile || profile->schemaVersion != RAL_CAPABILITY_PROFILE_SCHEMA_VERSION
			|| profile->backendType < RAL_BACKEND_VULKAN || profile->backendType >= RAL_BACKEND_COUNT
			|| profile->generation == 0 || profile->generation == UINT64_MAX
			|| profile->entryCount != RAL_CAP_COUNT || profile->ready != qtrue ) return qfalse;
	for (i=0u;i<RAL_CAP_COUNT;++i) {
		const ralCapabilityEntry_t *entry=&profile->entries[i];
		const ralCapabilityPolicy_t *policy=&s_policy[i];
		if ( entry->id != (ralCapabilityId_t)i || entry->requirement != policy->requirement
				|| entry->outcome < RAL_CAP_OUTCOME_NATIVE || entry->outcome > RAL_CAP_OUTCOME_DISABLED
				|| (entry->outcome == RAL_CAP_OUTCOME_EMULATED && !policy->allowEmulation)
				|| (entry->outcome != RAL_CAP_OUTCOME_DISABLED && entry->limit < policy->minimum)
				|| (entry->outcome == RAL_CAP_OUTCOME_DISABLED && entry->limit != 0u)
				|| (entry->outcome == RAL_CAP_OUTCOME_DISABLED
					&& policy->requirement == RAL_CAP_REQUIREMENT_REQUIRED) ) return qfalse;
	}
	return qtrue;
}

qboolean Ral_CapabilityProfileExact( const ralCapabilityProfile_t *a,
		const ralCapabilityProfile_t *b ) {
	uint32_t i;
	if ( !ProfileValid(a) || !ProfileValid(b) || a->backendType != b->backendType
			|| a->generation != b->generation ) return qfalse;
	for (i=0u;i<RAL_CAP_COUNT;++i)
		if ( a->entries[i].id != (ralCapabilityId_t)i
				|| b->entries[i].id != (ralCapabilityId_t)i
				|| a->entries[i].requirement != b->entries[i].requirement
				|| a->entries[i].outcome != b->entries[i].outcome
				|| a->entries[i].limit != b->entries[i].limit ) return qfalse;
	return qtrue;
}
