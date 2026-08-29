// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_LIGHTING_METADATA_H
#define WIRED_RENDER_LIGHTING_METADATA_H

#include "wired/entity/metadata.h"
#include "ral_irradiance_runtime.h"
#include "ral_lighting.h"
#include "render_submission_material.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_LIGHTING_VOLUME_AUTHORING_SCHEMA_VERSION 1u
#define RENDER_LIGHT_AUTHORING_SCHEMA_VERSION 1u
#define RENDER_EMISSIVE_MATERIAL_AUTHORING_SCHEMA_VERSION 1u

typedef struct {
	uint32_t schemaVersion;
	vec3_t origin;
	vec3_t spacing;
	vec3_t boundsMin;
	vec3_t boundsMax;
	int dimensionsX;
	int dimensionsY;
	int dimensionsZ;
	int priority;
	float blendDistance;
	int fallback;
} renderLightingVolumeAuthoring_t;

typedef struct {
	uint32_t schemaVersion;
	int kind;
	int mobility;
	vec3_t position;
	vec3_t direction;
	vec3_t boundsMin;
	vec3_t boundsMax;
	vec3_t radiance;
	float range;
	float innerConeCos;
	float outerConeCos;
	int shadowPriority;
	int flags;
} renderLightAuthoring_t;

typedef struct {
	uint32_t schemaVersion;
	vec3_t diffuseReflectance;
	vec3_t emissiveRadiance;
	int emissiveMobility;
	float emissiveInfluenceRange;
	int emissiveShadowPriority;
	int emissiveRequestedProxyCount;
	int participatesInStaticBake;
	int emissiveExplicitProxyAuthority;
	int emissiveInjectsAtmosphere;
} renderEmissiveMaterialAuthoring_t;

const wiredMetadataRegistry_t *Render_LightingVolumeMetadataRegistry( void );
qboolean Render_LightingVolumePlacementBuild(
	const renderLightingVolumeAuthoring_t *authoring,
	uint64_t volumeId, uint64_t sourceGeneration, uint64_t provenanceHash,
	ralIrradianceVolumePlacement_t *outPlacement );
const wiredMetadataRegistry_t *Render_LightMetadataRegistry( void );
qboolean Render_LightDescriptionBuild( const renderLightAuthoring_t *authoring,
	uint64_t lightId, uint64_t sourceGeneration, uint64_t provenanceHash,
	ralLightDescription_t *outLight );
const wiredMetadataRegistry_t *Render_EmissiveMaterialMetadataRegistry( void );
qboolean Render_EmissiveMaterialLightingBuild(
	const renderEmissiveMaterialAuthoring_t *authoring,
	uint64_t sourceGeneration, uint64_t provenanceHash,
	renderMaterialLighting_t *outLighting );

#ifdef __cplusplus
}
#endif

#endif
