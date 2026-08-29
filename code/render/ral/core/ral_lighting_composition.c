// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_composition.h"

#include <limits.h>
#include <string.h>

static uint32_t DebugTerm( ralLightingDebugView_t view ) {
	if ( view == RAL_LIGHTING_DEBUG_DYNAMIC_DIRECT )
		return RAL_LIGHTING_TERM_DYNAMIC_DIRECT;
	if ( view == RAL_LIGHTING_DEBUG_STATIC_INDIRECT )
		return RAL_LIGHTING_TERM_STATIC_INDIRECT;
	if ( view == RAL_LIGHTING_DEBUG_LOCAL_SH )
		return RAL_LIGHTING_TERM_LOCAL_SH;
	if ( view == RAL_LIGHTING_DEBUG_SPECULAR_IBL )
		return RAL_LIGHTING_TERM_SPECULAR_IBL;
	return 0u;
}

static qboolean AuthorityValid( const ralLightingCompositionRequest_t *r ) {
	if ( r->legacyLightmapBound && r->directionalStaticBound ) return qfalse;
	if ( r->diffuseAuthority == RAL_LIGHTING_DIFFUSE_AUTHORITY_LEGACY_LIGHTMAP )
		return r->legacyLightmapBound && !r->directionalStaticBound
			&& !r->localShBound && !r->staticProductDiffuseIndirectOnly;
	if ( r->diffuseAuthority == RAL_LIGHTING_DIFFUSE_AUTHORITY_DIRECTIONAL_STATIC )
		return !r->legacyLightmapBound && r->directionalStaticBound
			&& !r->localShBound && r->staticProductDiffuseIndirectOnly;
	if ( r->diffuseAuthority == RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH )
		return !r->legacyLightmapBound && !r->directionalStaticBound
			&& r->localShBound && !r->staticProductDiffuseIndirectOnly;
	return qfalse;
}

qboolean Ral_LightingCompositionBuild(
		const ralLightingCompositionRequest_t *request,
		ralLightingCompositionReceipt_t *outReceipt ) {
	ralLightingCompositionReceipt_t candidate;
	uint32_t requiredDiffuse, debugTerm, i;
	if ( !request || !outReceipt
			|| request->schemaVersion != RAL_LIGHTING_COMPOSITION_SCHEMA_VERSION
			|| !request->frameGeneration || request->frameGeneration == UINT64_MAX
			|| !request->surfaceId
			|| request->debugView > RAL_LIGHTING_DEBUG_SPECULAR_IBL
			|| !request->availableTermMask
			|| ( request->availableTermMask & ~RAL_LIGHTING_TERM_ALL )
			|| !AuthorityValid( request ) ) return qfalse;
	requiredDiffuse = request->diffuseAuthority
		== RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH
		? RAL_LIGHTING_TERM_LOCAL_SH : RAL_LIGHTING_TERM_STATIC_INDIRECT;
	if ( !( request->availableTermMask & requiredDiffuse )
			|| request->dynamicDirectBound
				!= !!( request->availableTermMask & RAL_LIGHTING_TERM_DYNAMIC_DIRECT )
			|| request->specularIblBound
				!= !!( request->availableTermMask & RAL_LIGHTING_TERM_SPECULAR_IBL )
			|| request->localShBound
				!= !!( request->availableTermMask & RAL_LIGHTING_TERM_LOCAL_SH ) )
		return qfalse;
	if ( request->diffuseAuthority != RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH
			&& ( request->availableTermMask & RAL_LIGHTING_TERM_LOCAL_SH ) )
		return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_LIGHTING_COMPOSITION_RECEIPT_SCHEMA_VERSION;
	candidate.frameGeneration = request->frameGeneration;
	candidate.surfaceId = request->surfaceId;
	candidate.diffuseAuthority = request->diffuseAuthority;
	candidate.debugView = request->debugView;
	candidate.activeTermMask = request->availableTermMask;
	debugTerm = DebugTerm( request->debugView );
	if ( debugTerm && !( candidate.activeTermMask & debugTerm ) ) return qfalse;
	candidate.visibleTermMask = debugTerm ? debugTerm : candidate.activeTermMask;
	for ( i = 0u; i < RAL_LIGHTING_COMPOSITION_TERM_COUNT; ++i )
		candidate.compositionWriteCount[i]
			= ( candidate.activeTermMask & ( 1u << i ) ) ? 1u : 0u;
	candidate.ready = qtrue;
	if ( !Ral_LightingCompositionReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean Ral_LightingCompositionReceiptValid(
		const ralLightingCompositionReceipt_t *receipt ) {
	uint32_t diffuseMask, i;
	if ( !receipt
			|| receipt->schemaVersion
				!= RAL_LIGHTING_COMPOSITION_RECEIPT_SCHEMA_VERSION
			|| !receipt->frameGeneration || receipt->frameGeneration == UINT64_MAX
			|| !receipt->surfaceId || !receipt->activeTermMask
			|| ( receipt->activeTermMask & ~RAL_LIGHTING_TERM_ALL )
			|| !receipt->visibleTermMask
			|| ( receipt->visibleTermMask & ~receipt->activeTermMask )
			|| receipt->debugView > RAL_LIGHTING_DEBUG_SPECULAR_IBL
			|| !receipt->ready ) return qfalse;
	diffuseMask = receipt->activeTermMask
		& ( RAL_LIGHTING_TERM_STATIC_INDIRECT | RAL_LIGHTING_TERM_LOCAL_SH );
	if ( receipt->diffuseAuthority == RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH ) {
		if ( diffuseMask != RAL_LIGHTING_TERM_LOCAL_SH ) return qfalse;
	} else if ( receipt->diffuseAuthority
			== RAL_LIGHTING_DIFFUSE_AUTHORITY_LEGACY_LIGHTMAP
			|| receipt->diffuseAuthority
				== RAL_LIGHTING_DIFFUSE_AUTHORITY_DIRECTIONAL_STATIC ) {
		if ( diffuseMask != RAL_LIGHTING_TERM_STATIC_INDIRECT ) return qfalse;
	} else return qfalse;
	if ( receipt->debugView == RAL_LIGHTING_DEBUG_FINAL ) {
		if ( receipt->visibleTermMask != receipt->activeTermMask ) return qfalse;
	} else if ( receipt->visibleTermMask != DebugTerm( receipt->debugView ) )
		return qfalse;
	for ( i = 0u; i < RAL_LIGHTING_COMPOSITION_TERM_COUNT; ++i ) {
		const uint8_t expected = ( receipt->activeTermMask & ( 1u << i ) ) ? 1u : 0u;
		if ( receipt->compositionWriteCount[i] != expected ) return qfalse;
	}
	return qtrue;
}

qboolean Ral_LightingCompositionEvaluateQ16(
		const ralLightingCompositionReceipt_t *receipt,
		const int32_t termRgbQ16[RAL_LIGHTING_COMPOSITION_TERM_COUNT][3],
		int32_t outRgbQ16[3] ) {
	int32_t candidate[3];
	uint32_t channel, term;
	if ( !Ral_LightingCompositionReceiptValid( receipt )
			|| !termRgbQ16 || !outRgbQ16 ) return qfalse;
	for ( channel = 0u; channel < 3u; ++channel ) {
		int64_t sum = 0;
		for ( term = 0u; term < RAL_LIGHTING_COMPOSITION_TERM_COUNT; ++term )
			if ( receipt->visibleTermMask & ( 1u << term ) )
				sum += termRgbQ16[term][channel];
		if ( sum < 0 || sum > INT32_MAX ) return qfalse;
		candidate[channel] = (int32_t)sum;
	}
	memcpy( outRgbQ16, candidate, sizeof( candidate ) );
	return qtrue;
}

static qboolean ModernPlanBindingValid( const ralLightingRuntimePlan_t *plan ) {
	uint32_t i;
	if ( !plan
			|| plan->schemaVersion != RAL_LIGHTING_RUNTIME_PLAN_SCHEMA_VERSION
			|| plan->backendType >= RAL_BACKEND_COUNT
			|| !plan->frameGeneration || plan->frameGeneration == UINT64_MAX
			|| !plan->artifactGeneration || !plan->artifactHash
			|| plan->artifactHash != plan->manifestHash
			|| ( plan->encoding != RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8
				&& plan->encoding != RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16 )
			|| plan->planeCount < 2u
			|| plan->planeCount > RAL_LIGHTING_RUNTIME_MAX_PLANES
			|| !plan->staticDiffuseIndirectOnly || plan->ready != qtrue )
		return qfalse;
	for ( i = 0u; i < plan->planeCount; ++i ) {
		const ralLightingRuntimePlanePlan_t *plane = &plan->planes[i];
		if ( plane->role < RAL_LIGHTING_PAYLOAD_RADIANCE
				|| plane->role > RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY
				|| plane->texture.type != RAL_TEXTURE_2D_ARRAY
				|| plane->texture.format <= RAL_FORMAT_UNDEFINED
				|| plane->texture.format >= RAL_FORMAT_COUNT
				|| !plane->texture.width || !plane->texture.height
				|| !plane->texture.depthOrArrayLayers
				|| plane->texture.mipLevels != 1u
				|| plane->texture.sampleCount != 1u
				|| !( plane->texture.usage & RAL_TEXTURE_USAGE_SAMPLED )
				|| !plane->byteLength || !plane->payloadHash ) return qfalse;
	}
	return qtrue;
}

static qboolean ModernPlaneCatalog( const ralLightingRuntimePlan_t *plan,
		uint32_t *outRadiance, uint32_t *outDirection,
		uint32_t *outVisibility, uint32_t *outLayers ) {
	uint32_t radiance = RAL_LIGHTING_SURFACE_BINDING_NO_PLANE;
	uint32_t direction = RAL_LIGHTING_SURFACE_BINDING_NO_PLANE;
	uint32_t visibility = RAL_LIGHTING_SURFACE_BINDING_NO_PLANE;
	uint32_t layers = 0u, i;
	if ( !plan || !outRadiance || !outDirection || !outVisibility || !outLayers
			|| !ModernPlanBindingValid( plan ) ) return qfalse;
	for ( i = 0u; i < plan->planeCount; ++i ) {
		const ralLightingRuntimePlanePlan_t *plane = &plan->planes[i];
		if ( !layers ) layers = plane->texture.depthOrArrayLayers;
		if ( plane->texture.depthOrArrayLayers != layers ) return qfalse;
		if ( plane->role == RAL_LIGHTING_PAYLOAD_RADIANCE ) {
			if ( radiance != RAL_LIGHTING_SURFACE_BINDING_NO_PLANE ) return qfalse;
			radiance = i;
		} else if ( plane->role == RAL_LIGHTING_PAYLOAD_DIRECTION ) {
			if ( direction != RAL_LIGHTING_SURFACE_BINDING_NO_PLANE ) return qfalse;
			direction = i;
		} else if ( plane->role == RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY ) {
			if ( visibility != RAL_LIGHTING_SURFACE_BINDING_NO_PLANE ) return qfalse;
			visibility = i;
		} else return qfalse;
	}
	if ( radiance == RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
			|| direction == RAL_LIGHTING_SURFACE_BINDING_NO_PLANE || !layers )
		return qfalse;
	*outRadiance = radiance;
	*outDirection = direction;
	*outVisibility = visibility;
	*outLayers = layers;
	return qtrue;
}

qboolean Ral_LightingSurfaceBindingBuild(
		const ralLightingSurfaceBindingRequest_t *request,
		const ralLightingRuntimePlan_t *modernPlan,
		ralLightingSurfaceBindingReceipt_t *outReceipt ) {
	ralLightingSurfaceBindingReceipt_t candidate;
	uint32_t layers = 0u;
	if ( !request || !outReceipt
			|| request->schemaVersion != RAL_LIGHTING_SURFACE_BINDING_SCHEMA_VERSION
			|| !request->frameGeneration || request->frameGeneration == UINT64_MAX
			|| !request->surfaceId || request->lightmapIndex < 0 ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_LIGHTING_SURFACE_BINDING_RECEIPT_SCHEMA_VERSION;
	candidate.frameGeneration = request->frameGeneration;
	candidate.surfaceId = request->surfaceId;
	candidate.visibilityPlane = RAL_LIGHTING_SURFACE_BINDING_NO_PLANE;
	if ( modernPlan ) {
		if ( !ModernPlaneCatalog( modernPlan, &candidate.radiancePlane,
					&candidate.directionPlane, &candidate.visibilityPlane, &layers )
				|| (uint32_t)request->lightmapIndex >= layers ) return qfalse;
		candidate.diffuseAuthority = RAL_LIGHTING_DIFFUSE_AUTHORITY_DIRECTIONAL_STATIC;
		candidate.arrayLayer = (uint32_t)request->lightmapIndex;
		candidate.directionalStaticBound = qtrue;
	} else {
		if ( !request->legacyLightmapAvailable ) return qfalse;
		candidate.diffuseAuthority = RAL_LIGHTING_DIFFUSE_AUTHORITY_LEGACY_LIGHTMAP;
		candidate.radiancePlane = RAL_LIGHTING_SURFACE_BINDING_NO_PLANE;
		candidate.directionPlane = RAL_LIGHTING_SURFACE_BINDING_NO_PLANE;
		candidate.legacyLightmapBound = qtrue;
	}
	candidate.ready = qtrue;
	if ( !Ral_LightingSurfaceBindingReceiptValid( &candidate ) ) return qfalse;
	*outReceipt = candidate;
	return qtrue;
}

qboolean Ral_LightingSurfaceBindingReceiptValid(
		const ralLightingSurfaceBindingReceipt_t *receipt ) {
	if ( !receipt
			|| receipt->schemaVersion != RAL_LIGHTING_SURFACE_BINDING_RECEIPT_SCHEMA_VERSION
			|| !receipt->frameGeneration || receipt->frameGeneration == UINT64_MAX
			|| !receipt->surfaceId || receipt->ready != qtrue
			|| receipt->legacyLightmapBound == receipt->directionalStaticBound )
		return qfalse;
	if ( receipt->diffuseAuthority == RAL_LIGHTING_DIFFUSE_AUTHORITY_LEGACY_LIGHTMAP )
		return receipt->legacyLightmapBound
			&& receipt->radiancePlane == RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
			&& receipt->directionPlane == RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
			&& receipt->visibilityPlane == RAL_LIGHTING_SURFACE_BINDING_NO_PLANE;
	if ( receipt->diffuseAuthority == RAL_LIGHTING_DIFFUSE_AUTHORITY_DIRECTIONAL_STATIC )
		return receipt->directionalStaticBound
			&& receipt->radiancePlane != RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
			&& receipt->directionPlane != RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
			&& receipt->radiancePlane != receipt->directionPlane
			&& ( receipt->visibilityPlane == RAL_LIGHTING_SURFACE_BINDING_NO_PLANE
				|| ( receipt->visibilityPlane != receipt->radiancePlane
					&& receipt->visibilityPlane != receipt->directionPlane ) );
	return qfalse;
}

qboolean Ral_LightingSurfaceBindingReceiptExact(
		const ralLightingSurfaceBindingReceipt_t *a,
		const ralLightingSurfaceBindingReceipt_t *b ) {
	return Ral_LightingSurfaceBindingReceiptValid( a )
		&& Ral_LightingSurfaceBindingReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}
