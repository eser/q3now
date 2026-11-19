// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_runtime.h"

#include <math.h>
#include <string.h>

qboolean Ral_MaterialRuntimeTextureKeyBuild(
		uint64_t sourceId, uint64_t filesystemGeneration,
		const unsigned char *rgba8, uint32_t width, uint32_t height,
		qboolean srgb, qboolean noMipMaps,
		ralTextureAssetResidency_t residency, ralTextureCacheKey_t *outKey ) {
	ralTextureAssetRequest_t request;
	ralTextureAssetReceipt_t asset;
	uint64_t pixelCount;
	qboolean hasAlpha = qfalse;
	if ( !sourceId || !filesystemGeneration || !rgba8 || !width || !height
			|| ( srgb != qfalse && srgb != qtrue )
			|| ( noMipMaps != qfalse && noMipMaps != qtrue )
			|| residency < RAL_TEXTURE_RESIDENCY_PINNED
			|| residency > RAL_TEXTURE_RESIDENCY_TRANSIENT || !outKey ) return qfalse;
	pixelCount = (uint64_t)width * height;
	if ( pixelCount > UINT64_MAX / 4u ) return qfalse;
	for ( uint64_t pixel = 0u; pixel < pixelCount; ++pixel )
		if ( rgba8[pixel * 4u + 3u] != 255u ) { hasAlpha = qtrue; break; }
	memset( &request, 0, sizeof( request ) );
	request.schemaVersion = RAL_TEXTURE_ASSET_SCHEMA_VERSION;
	request.assetGeneration = filesystemGeneration;
	request.provenanceHash = sourceId;
	request.dimension = RAL_TEXTURE_ASSET_2D;
	request.width = width; request.height = height;
	request.depth = request.layers = request.sourceMipLevels = 1u;
	request.colorEncoding = srgb ? RAL_TEXTURE_ENCODING_SRGB
		: RAL_TEXTURE_ENCODING_LINEAR;
	request.channelSemantic = srgb ? RAL_TEXTURE_CHANNEL_COLOR
		: RAL_TEXTURE_CHANNEL_DATA;
	request.sourceEncoding = RAL_TEXTURE_SOURCE_UNCOMPRESSED;
	request.mipPolicy = noMipMaps ? RAL_TEXTURE_MIPS_NONE
		: RAL_TEXTURE_MIPS_GENERATE;
	request.residency = residency;
	request.hasAlpha = hasAlpha;
	request.preferenceCount = 1u;
	request.preferences[0] = RAL_TEXTURE_COMPRESSION_UNCOMPRESSED;
	request.allowUncompressedFallback = qtrue;
	return Ral_ResolveTextureAsset( &request, &asset )
		&& Ral_TextureCacheKeyBuild( &asset, outKey );
}

qboolean Ral_TextureResourceReceiptValid(
		const ralTextureResourceReceipt_t *receipt ) {
	return receipt
		&& receipt->schemaVersion == RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION
		&& receipt->backendType >= RAL_BACKEND_VULKAN
		&& receipt->backendType < RAL_BACKEND_COUNT
		&& receipt->textureIdentity != 0u
		&& receipt->resourceGeneration > 0u
		&& receipt->resourceGeneration < UINT64_MAX
		&& receipt->type >= RAL_TEXTURE_1D
		&& receipt->type <= RAL_TEXTURE_CUBE_ARRAY
		&& receipt->format > RAL_FORMAT_UNDEFINED
		&& receipt->format < RAL_FORMAT_COUNT
		&& receipt->usage != 0u
		&& receipt->width > 0u && receipt->height > 0u
		&& receipt->mipLevels > 0u && receipt->arrayLayers > 0u
		&& receipt->sampleEncoding >= RAL_TEXTURE_SAMPLE_ENCODING_NATIVE
		&& receipt->sampleEncoding <= RAL_TEXTURE_SAMPLE_ENCODING_MANUAL_SRGB
		&& ( receipt->sampleEncoding != RAL_TEXTURE_SAMPLE_ENCODING_MANUAL_SRGB
			|| receipt->format == RAL_FORMAT_R8G8B8A8_UNORM )
		&& ( receipt->imported == qfalse || receipt->imported == qtrue )
		&& receipt->ready == qtrue;
}

qboolean Ral_TextureResourceReceiptExact(
		const ralTextureResourceReceipt_t *a,
		const ralTextureResourceReceipt_t *b ) {
	return Ral_TextureResourceReceiptValid( a )
		&& Ral_TextureResourceReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

static qboolean ResourceMatches( const ralTextureResourceReceipt_t *r,
		const ralTextureAssetReceipt_t *a ) {
	ralTextureType_t type;
	uint32_t layers;
	qboolean formatMatches;
	if ( !r || !a || !Ral_TextureResourceReceiptValid( r )
			|| !( r->usage & RAL_TEXTURE_USAGE_SAMPLED ) ) return qfalse;
	switch ( a->dimension ) {
	case RAL_TEXTURE_ASSET_2D: type = RAL_TEXTURE_2D; break;
	case RAL_TEXTURE_ASSET_2D_ARRAY: type = RAL_TEXTURE_2D_ARRAY; break;
	case RAL_TEXTURE_ASSET_CUBE: type = a->layers == 6u ? RAL_TEXTURE_CUBE
		: RAL_TEXTURE_CUBE_ARRAY; break;
	case RAL_TEXTURE_ASSET_3D: type = RAL_TEXTURE_3D; break;
	default: return qfalse;
	}
	layers = type == RAL_TEXTURE_3D ? 1u : a->layers;
	formatMatches = r->format == a->targetFormat
		&& r->sampleEncoding == RAL_TEXTURE_SAMPLE_ENCODING_NATIVE;
	if ( !formatMatches && a->targetFormat == RAL_FORMAT_R8G8B8A8_SRGB
			&& r->format == RAL_FORMAT_R8G8B8A8_UNORM
			&& r->sampleEncoding == RAL_TEXTURE_SAMPLE_ENCODING_MANUAL_SRGB )
		formatMatches = qtrue;
	return r->type == type && formatMatches
		&& r->width == a->width && r->height == a->height
		&& r->mipLevels == a->targetMipLevels && r->arrayLayers == layers;
}

qboolean Ral_MaterialRuntimeResourceBuild(
		const ralTextureCacheKey_t *key,
		const ralTextureResourceReceipt_t *resource,
		ralMaterialRuntimeResource_t *outResource ) {
	ralMaterialRuntimeResource_t candidate;
	if ( !outResource || !Ral_TextureCacheKeyValid( key )
			|| !ResourceMatches( resource, &key->asset ) ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.cacheKeyHash = key->keyHash;
	candidate.resource = *resource;
	*outResource = candidate;
	return qtrue;
}

static qboolean Build( const ralMaterialReceipt_t *material,
		const ralTextureResourceReceipt_t *resources, uint32_t count,
		uint64_t generation, ralMaterialRuntimePlan_t *out ) {
	ralMaterialRuntimePlan_t v;
	uint32_t i, j;
	if ( !out || !generation || !Ral_MaterialReceiptValid( material )
			|| count != material->material.textureCount
			|| ( count && !resources ) || count > RAL_MATERIAL_MAX_TEXTURES ) return qfalse;
	memset( &v, 0, sizeof( v ) ); v.schemaVersion = RAL_MATERIAL_RUNTIME_SCHEMA_VERSION;
	v.runtimeGeneration = generation; v.material = *material; v.bindingCount = count;
	for ( i = 0u; i < count; i++ ) {
		const ralMaterialTexture_t *texture = &material->material.textures[i];
		if ( !ResourceMatches( &resources[i], &texture->texture.asset ) ) return qfalse;
		if ( i && resources[i].backendType != resources[0].backendType ) return qfalse;
		/* Binding slots are not allocation owners. Q3 stages may sample one
		 * image with distinct scroll/blend policies. Only an exact resource
		 * and cache identity may be shared; stale generations still reject. */
		for ( j = 0u; j < i; j++ )
			if ( resources[j].textureIdentity == resources[i].textureIdentity
					&& ( material->material.textures[j].texture.keyHash
						!= texture->texture.keyHash
						|| memcmp( &resources[j], &resources[i],
							sizeof( resources[i] ) ) ) ) return qfalse;
		v.bindings[i].bindingSlot = (uint32_t)texture->role - 1u;
		v.bindings[i].role = texture->role;
		v.bindings[i].cacheKeyHash = texture->texture.keyHash;
		v.bindings[i].resource = resources[i];
	}
	v.ready = qtrue; *out = v; return qtrue;
}

static qboolean DependenciesValid( const ralMaterialRuntimePlan_t *plan ) {
	uint32_t i;
	if ( !plan || plan->dependencyCount > RAL_MATERIAL_COOK_MAX_BINDINGS )
		return qfalse;
	for ( i = 0u; i < plan->dependencyCount; ++i ) {
		const ralMaterialRuntimeDependency_t *dependency = &plan->dependencies[i];
		if ( dependency->dependencyIndex != i
				|| dependency->role < RAL_MATERIAL_DEPENDENCY_TEXTURE
				|| dependency->role > RAL_MATERIAL_DEPENDENCY_RESIDENCY
				|| !dependency->sourceId || !dependency->fsGeneration
				|| !dependency->creationOptionsHash || !dependency->artifactHash
				|| ( dependency->hasTextureResource != qfalse
					&& dependency->hasTextureResource != qtrue ) )
			return qfalse;
		if ( dependency->role == RAL_MATERIAL_DEPENDENCY_TEXTURE ) {
			if ( dependency->hasTextureResource != qtrue
					|| dependency->cacheKeyHash != dependency->artifactHash
					|| !Ral_TextureResourceReceiptValid(
						&dependency->textureResource )
					|| !( dependency->textureResource.usage
						& RAL_TEXTURE_USAGE_SAMPLED ) ) return qfalse;
		} else if ( dependency->hasTextureResource != qfalse
				|| dependency->cacheKeyHash
				|| memcmp( &dependency->textureResource,
					&(ralTextureResourceReceipt_t){ 0 },
					sizeof( dependency->textureResource ) ) ) return qfalse;
	}
	for ( i = plan->dependencyCount; i < RAL_MATERIAL_COOK_MAX_BINDINGS; ++i )
		if ( memcmp( &plan->dependencies[i],
				&(ralMaterialRuntimeDependency_t){ 0 },
				sizeof( plan->dependencies[i] ) ) ) return qfalse;
	return qtrue;
}

static qboolean SkyDependencyTextureValid( const ralMaterialRuntimePlan_t *plan,
		uint32_t dependencyIndex ) {
	return dependencyIndex < plan->dependencyCount
		&& plan->dependencies[dependencyIndex].role
			== RAL_MATERIAL_DEPENDENCY_TEXTURE
		&& plan->dependencies[dependencyIndex].hasTextureResource == qtrue;
}

static qboolean RuntimeBoolValid( qboolean value ) {
	return value == qfalse || value == qtrue;
}

static qboolean SkyStageValid( const ralMaterialRuntimePlan_t *plan,
		const ralMaterialSourceStage_t *stage ) {
	uint32_t i, j;
	if ( stage->mapKind < RAL_MATERIAL_MAP_IMAGE
			|| stage->mapKind > RAL_MATERIAL_MAP_WHITE
			|| stage->tcGen > RAL_MATERIAL_TCGEN_VECTOR
			|| stage->depthFunc > RAL_MATERIAL_DEPTH_EQUAL
			|| stage->alphaTest > RAL_MATERIAL_ALPHA_TEST_KEEP_GREATER_EQUAL
			|| stage->rgbGen > RAL_MATERIAL_COLOR_CONST
			|| stage->alphaGen > RAL_MATERIAL_ALPHA_CONST
			|| !RuntimeBoolValid( stage->depthWrite )
			|| !RuntimeBoolValid( stage->animationLooping )
			|| stage->animationFrameCount
				> RAL_MATERIAL_SOURCE_MAX_ANIMATION_FRAMES
			|| stage->tcModCount > RAL_MATERIAL_SOURCE_MAX_TCMODS
			|| !isfinite( stage->alphaCutoff )
			|| !isfinite( stage->portalRange )
			|| !isfinite( stage->animationFrequency )
			|| stage->rgbWave.function > RAL_MATERIAL_WAVE_NOISE
			|| stage->alphaWave.function > RAL_MATERIAL_WAVE_NOISE
			|| !isfinite( stage->rgbWave.base )
			|| !isfinite( stage->rgbWave.amplitude )
			|| !isfinite( stage->rgbWave.phase )
			|| !isfinite( stage->rgbWave.frequency )
			|| !isfinite( stage->alphaWave.base )
			|| !isfinite( stage->alphaWave.amplitude )
			|| !isfinite( stage->alphaWave.phase )
			|| !isfinite( stage->alphaWave.frequency ) ) return qfalse;
	if ( stage->mapKind == RAL_MATERIAL_MAP_IMAGE
			&& !SkyDependencyTextureValid( plan, stage->dependencyIndex ) )
		return qfalse;
	for ( i = 0u; i < 2u; ++i )
		for ( j = 0u; j < 3u; ++j )
			if ( !isfinite( stage->tcGenVectors[i][j] ) ) return qfalse;
	for ( i = 0u; i < 4u; ++i )
		if ( !isfinite( stage->constantColor[i] ) ) return qfalse;
	for ( i = 0u; i < stage->animationFrameCount; ++i )
		if ( !SkyDependencyTextureValid( plan,
				stage->animationDependencyIndices[i] ) ) return qfalse;
	for ( i = 0u; i < stage->tcModCount; ++i ) {
		if ( stage->tcMods[i].type < RAL_MATERIAL_TCMOD_SCALE
				|| stage->tcMods[i].type > RAL_MATERIAL_TCMOD_TRANSFORM )
			return qfalse;
		for ( j = 0u; j < 6u; ++j )
			if ( !isfinite( stage->tcMods[i].parameters[j] ) ) return qfalse;
	}
	return qtrue;
}

static qboolean SkyValid( const ralMaterialRuntimePlan_t *plan ) {
	const ralMaterialRuntimeSkySource_t *runtime = &plan->sky;
	const ralMaterialSourceSky_t *sky = &runtime->authored;
	if ( runtime->present == qfalse )
		return !memcmp( runtime, &(ralMaterialRuntimeSkySource_t){ 0 },
			sizeof( *runtime ) );
	if ( runtime->present != qtrue || !isfinite( runtime->cloudHeight )
			|| runtime->cloudHeight < 0.0f
			|| runtime->stageCount > RAL_MATERIAL_SOURCE_MAX_STAGES
			|| !RuntimeBoolValid( sky->outerBox )
			|| !RuntimeBoolValid( sky->innerBox )
			|| !RuntimeBoolValid( sky->hasSun )
			|| !RuntimeBoolValid( sky->sunExtended )
			|| ( sky->sunExtended && !sky->hasSun )
			|| !isfinite( sky->sunIntensity ) || sky->sunIntensity < 0.0f
			|| !isfinite( sky->sunAzimuthDegrees )
			|| !isfinite( sky->sunElevationDegrees )
			|| !isfinite( sky->sunDevianceDegrees )
			|| sky->sunDevianceDegrees < 0.0f
			|| ( !sky->sunExtended
				&& ( sky->sunDevianceDegrees != 0.0f || sky->sunSamples ) )
			|| ( !sky->hasSun && ( sky->sunIntensity != 0.0f
				|| sky->sunAzimuthDegrees != 0.0f
				|| sky->sunElevationDegrees != 0.0f ) ) )
		return qfalse;
	for ( uint32_t color = 0u; color < 3u; ++color )
		if ( !isfinite( sky->sunColor[color] ) || sky->sunColor[color] < 0.0f
				|| ( !sky->hasSun && sky->sunColor[color] != 0.0f ) )
			return qfalse;
	for ( uint32_t box = 0u; box < 2u; ++box ) {
		const qboolean present = box ? sky->innerBox : sky->outerBox;
		const uint32_t *faces = box ? sky->innerDependencies
			: sky->outerDependencies;
		for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
			if ( present ) {
				if ( !SkyDependencyTextureValid( plan, faces[face] ) )
					return qfalse;
			} else if ( faces[face] ) return qfalse;
		}
	}
	for ( uint32_t stageIndex = 0u; stageIndex < runtime->stageCount;
			++stageIndex ) {
		const ralMaterialSourceStage_t *stage = &runtime->stages[stageIndex];
		if ( !SkyStageValid( plan, stage ) ) return qfalse;
	}
	for ( uint32_t stageIndex = runtime->stageCount;
			stageIndex < RAL_MATERIAL_SOURCE_MAX_STAGES; ++stageIndex )
		if ( memcmp( &runtime->stages[stageIndex],
				&(ralMaterialSourceStage_t){ 0 },
				sizeof( runtime->stages[stageIndex] ) ) ) return qfalse;
	return qtrue;
}

static void BuildFail( ralMaterialRuntimeBuildDiagnostic_t *diagnostic,
		ralMaterialRuntimeBuildFailure_t reason, uint32_t itemIndex,
		uint64_t cacheKeyHash ) {
	if ( !diagnostic ) return;
	memset( diagnostic, 0, sizeof( *diagnostic ) );
	diagnostic->reason = reason;
	diagnostic->itemIndex = itemIndex;
	diagnostic->cacheKeyHash = cacheKeyHash;
}

qboolean Ral_MaterialRuntimePlanBuild( const ralMaterialReceipt_t *material,
		const ralTextureResourceReceipt_t *resources, uint32_t count,
		uint64_t generation, ralMaterialRuntimePlan_t *out ) {
	ralMaterialRuntimePlan_t v;
	if ( !out || !Build( material, resources, count, generation, &v ) ) return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_MaterialRuntimePlanBuildCooked(
		const ralMaterialCookReceipt_t *cooked,
		const ralMaterialRuntimeResource_t *resources, uint32_t count,
		uint64_t generation, ralMaterialRuntimePlan_t *out,
		ralMaterialRuntimeBuildDiagnostic_t *diagnostic ) {
	ralTextureResourceReceipt_t ordered[RAL_MATERIAL_MAX_TEXTURES];
	ralMaterialRuntimePlan_t candidate;
	uint32_t i, j, expectedResourceCount = 0u;
	if ( diagnostic ) memset( diagnostic, 0, sizeof( *diagnostic ) );
	if ( !out || !generation ) {
		BuildFail( diagnostic, RAL_MATERIAL_RUNTIME_BUILD_INVALID_RESOURCE_SET,
			0u, 0u ); return qfalse;
	}
	if ( !Ral_MaterialCookReceiptValid( cooked ) ) {
		BuildFail( diagnostic, RAL_MATERIAL_RUNTIME_BUILD_INVALID_COOKED,
			0u, 0u ); return qfalse;
	}
	for ( i = 0u; i < cooked->bindingCount; ++i )
		if ( cooked->bindings[i].role == RAL_MATERIAL_DEPENDENCY_TEXTURE )
			expectedResourceCount++;
	if ( count != expectedResourceCount
			|| count > RAL_MATERIAL_COOK_MAX_BINDINGS
			|| ( count && !resources ) ) {
		BuildFail( diagnostic, RAL_MATERIAL_RUNTIME_BUILD_INVALID_RESOURCE_SET,
			count, 0u ); return qfalse;
	}
	for ( i = 0u; i < count; ++i ) {
		if ( !resources[i].cacheKeyHash
				|| !Ral_TextureResourceReceiptValid( &resources[i].resource )
				|| !( resources[i].resource.usage & RAL_TEXTURE_USAGE_SAMPLED ) ) {
			BuildFail( diagnostic, RAL_MATERIAL_RUNTIME_BUILD_INVALID_RESOURCE_SET,
				i, 0u ); return qfalse;
		}
		for ( j = 0u; j < i; ++j ) {
			if ( resources[j].cacheKeyHash == resources[i].cacheKeyHash ) {
				BuildFail( diagnostic,
					RAL_MATERIAL_RUNTIME_BUILD_DUPLICATE_RESOURCE, i,
					resources[i].cacheKeyHash ); return qfalse;
			}
			if ( resources[j].resource.textureIdentity
					== resources[i].resource.textureIdentity
					|| resources[j].resource.backendType
					!= resources[i].resource.backendType ) {
				BuildFail( diagnostic,
					RAL_MATERIAL_RUNTIME_BUILD_INVALID_RESOURCE_SET, i,
					resources[i].cacheKeyHash ); return qfalse;
			}
		}
	}
	for ( i = 0u; i < cooked->material.material.textureCount; ++i ) {
		const uint64_t key = cooked->material.material.textures[i].texture.keyHash;
		const ralMaterialRuntimeResource_t *match = NULL;
		for ( j = 0u; j < count; ++j ) {
			if ( resources[j].cacheKeyHash == key ) {
				match = &resources[j]; break;
			}
		}
		if ( !match ) {
			BuildFail( diagnostic, RAL_MATERIAL_RUNTIME_BUILD_MISSING_RESOURCE,
				i, key ); return qfalse;
		}
		if ( !ResourceMatches( &match->resource,
				&cooked->material.material.textures[i].texture.asset ) ) {
			BuildFail( diagnostic, RAL_MATERIAL_RUNTIME_BUILD_INVALID_RESOURCE_SET,
				i, key ); return qfalse;
		}
		ordered[i] = match->resource;
	}
	if ( !Build( &cooked->material, ordered,
			cooked->material.material.textureCount, generation, &candidate ) ) {
		BuildFail( diagnostic, RAL_MATERIAL_RUNTIME_BUILD_PLAN_REJECTED,
		0u, 0u ); return qfalse;
	}
	candidate.dependencyCount = cooked->bindingCount;
	for ( i = 0u; i < cooked->bindingCount; ++i ) {
		const ralMaterialCookBinding_t *binding = &cooked->bindings[i];
		ralMaterialRuntimeDependency_t *dependency = &candidate.dependencies[i];
		dependency->dependencyIndex = binding->dependencyIndex;
		dependency->role = binding->role;
		dependency->sourceId = binding->sourceId;
		dependency->fsGeneration = binding->fsGeneration;
		dependency->creationOptionsHash = binding->creationOptionsHash;
		dependency->artifactHash = binding->artifactHash;
		if ( binding->role == RAL_MATERIAL_DEPENDENCY_TEXTURE ) {
			const ralMaterialRuntimeResource_t *match = NULL;
			for ( j = 0u; j < count; ++j )
				if ( resources[j].cacheKeyHash == binding->texture.keyHash ) {
					match = &resources[j]; break;
				}
			if ( !match || !ResourceMatches( &match->resource,
					&binding->texture.asset ) ) {
				BuildFail( diagnostic,
					RAL_MATERIAL_RUNTIME_BUILD_MISSING_RESOURCE, i,
					binding->texture.keyHash ); return qfalse;
			}
			dependency->hasTextureResource = qtrue;
			dependency->cacheKeyHash = binding->texture.keyHash;
			dependency->textureResource = match->resource;
		}
	}
	if ( cooked->source.source.sky ) {
		candidate.sky.present = qtrue;
		candidate.sky.cloudHeight = cooked->source.source.skyCloudHeight;
		candidate.sky.authored = cooked->source.source.skySource;
		candidate.sky.stageCount = cooked->source.source.stageCount;
		memcpy( candidate.sky.stages, cooked->source.source.stages,
			(size_t)candidate.sky.stageCount * sizeof( candidate.sky.stages[0] ) );
	}
	if ( !DependenciesValid( &candidate ) || !SkyValid( &candidate ) ) {
		BuildFail( diagnostic, RAL_MATERIAL_RUNTIME_BUILD_PLAN_REJECTED,
			0u, 0u ); return qfalse;
	}
	*out = candidate;
	return qtrue;
}

qboolean Ral_MaterialRuntimePlanValid( const ralMaterialRuntimePlan_t *p ) {
	ralMaterialRuntimePlan_t expected;
	ralTextureResourceReceipt_t resources[RAL_MATERIAL_MAX_TEXTURES];
	uint32_t i;
	if ( !p || p->schemaVersion != RAL_MATERIAL_RUNTIME_SCHEMA_VERSION
			|| p->ready != qtrue || p->bindingCount > RAL_MATERIAL_MAX_TEXTURES
			|| !DependenciesValid( p ) || !SkyValid( p ) ) return qfalse;
	for ( i = 0u; i < p->bindingCount; i++ ) resources[i] = p->bindings[i].resource;
	if ( !Build( &p->material, resources, p->bindingCount,
			p->runtimeGeneration, &expected ) ) return qfalse;
	expected.dependencyCount = p->dependencyCount;
	memcpy( expected.dependencies, p->dependencies,
		(size_t)p->dependencyCount * sizeof( p->dependencies[0] ) );
	expected.sky = p->sky;
	return !memcmp( p, &expected, sizeof( *p ) );
}

qboolean Ral_MaterialRuntimePlanExact( const ralMaterialRuntimePlan_t *a,
		const ralMaterialRuntimePlan_t *b ) {
	return Ral_MaterialRuntimePlanValid( a ) && Ral_MaterialRuntimePlanValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean Ral_MaterialRuntimePlanResolveDependencyTexture(
		const ralMaterialRuntimePlan_t *plan, uint32_t dependencyIndex,
		ralTextureResourceReceipt_t *outResource ) {
	const ralMaterialRuntimeDependency_t *dependency;
	if ( !outResource || !Ral_MaterialRuntimePlanValid( plan )
			|| dependencyIndex >= plan->dependencyCount ) return qfalse;
	dependency = &plan->dependencies[dependencyIndex];
	if ( dependency->role != RAL_MATERIAL_DEPENDENCY_TEXTURE
			|| dependency->hasTextureResource != qtrue ) return qfalse;
	*outResource = dependency->textureResource;
	return qtrue;
}

qboolean Ral_MaterialRuntimeCohortValid( const ralMaterialRuntimeCohort_t *c ) {
	return c && c->schemaVersion == RAL_MATERIAL_RUNTIME_COHORT_SCHEMA_VERSION
		&& c->cohortGeneration && c->dependencyGeneration
		&& ( c->state == RAL_MATERIAL_RUNTIME_READY
			|| c->state == RAL_MATERIAL_RUNTIME_INVALIDATED )
		&& Ral_MaterialRuntimePlanValid( &c->plan );
}

qboolean Ral_MaterialRuntimeCohortInit( const ralMaterialRuntimePlan_t *plan,
		uint64_t cohortGeneration, uint64_t dependencyGeneration,
		ralMaterialRuntimeCohort_t *out ) {
	ralMaterialRuntimeCohort_t v;
	if ( !out || !cohortGeneration || !dependencyGeneration
			|| !Ral_MaterialRuntimePlanValid( plan ) ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = RAL_MATERIAL_RUNTIME_COHORT_SCHEMA_VERSION;
	v.cohortGeneration = cohortGeneration; v.dependencyGeneration = dependencyGeneration;
	v.state = RAL_MATERIAL_RUNTIME_READY; v.plan = *plan;
	*out = v; return qtrue;
}

qboolean Ral_MaterialRuntimeInvalidate( ralMaterialRuntimeCohort_t *c,
		const ralMaterialDependencyEvent_t *e, qboolean *outInvalidated ) {
	ralMaterialRuntimeCohort_t v;
	qboolean matches = qfalse;
	uint32_t i;
	if ( !outInvalidated || !Ral_MaterialRuntimeCohortValid( c ) || !e
			|| e->kind < RAL_MATERIAL_RUNTIME_DEPENDENCY_ARTIFACT
			|| e->kind > RAL_MATERIAL_RUNTIME_DEPENDENCY_SOURCE
			|| e->dependencyGeneration <= c->dependencyGeneration
			|| !e->previousGeneration || !e->nextGeneration
			|| e->previousGeneration == e->nextGeneration ) return qfalse;
	if ( e->kind == RAL_MATERIAL_RUNTIME_DEPENDENCY_ARTIFACT ) {
		if ( !e->artifactHash || e->textureIdentity ) return qfalse;
		matches = e->artifactHash == c->plan.material.artifactHash
			&& e->previousGeneration == c->plan.material.material.materialGeneration;
	} else if ( e->kind == RAL_MATERIAL_RUNTIME_DEPENDENCY_TEXTURE ) {
		if ( e->artifactHash || !e->textureIdentity ) return qfalse;
		for ( i = 0u; i < c->plan.bindingCount; i++ )
			if ( c->plan.bindings[i].resource.textureIdentity == e->textureIdentity
					&& c->plan.bindings[i].resource.resourceGeneration
						== e->previousGeneration ) matches = qtrue;
		for ( i = 0u; i < c->plan.dependencyCount; ++i ) {
			const ralMaterialRuntimeDependency_t *dependency =
				&c->plan.dependencies[i];
			if ( dependency->hasTextureResource == qtrue
					&& dependency->textureResource.textureIdentity
						== e->textureIdentity
					&& dependency->textureResource.resourceGeneration
						== e->previousGeneration ) matches = qtrue;
		}
	} else {
		if ( e->textureIdentity || !e->artifactHash || !e->sourceId
				|| !e->creationOptionsHash
				|| e->role < RAL_MATERIAL_DEPENDENCY_TEXTURE
				|| e->role > RAL_MATERIAL_DEPENDENCY_RESIDENCY ) return qfalse;
		for ( i = 0u; i < c->plan.dependencyCount; ++i ) {
			const ralMaterialRuntimeDependency_t *dependency =
				&c->plan.dependencies[i];
			if ( dependency->role == e->role
					&& dependency->sourceId == e->sourceId
					&& dependency->creationOptionsHash == e->creationOptionsHash
					&& dependency->artifactHash == e->artifactHash
					&& dependency->fsGeneration == e->previousGeneration ) {
				matches = qtrue;
				break;
			}
		}
	}
	if ( !matches ) { *outInvalidated = qfalse; return qtrue; }
	if ( c->state != RAL_MATERIAL_RUNTIME_READY ) return qfalse;
	v = *c; v.state = RAL_MATERIAL_RUNTIME_INVALIDATED;
	v.dependencyGeneration = e->dependencyGeneration;
	*c = v; *outInvalidated = qtrue; return qtrue;
}
