// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_runtime.h"

#include <limits.h>
#include <string.h>

static qboolean PlaneFormat( ralStaticLightingEncoding_t encoding,
		ralLightingPayloadRole_t role, ralFormat_t *outFormat,
		uint32_t *outBytesPerTexel ) {
	if ( !outFormat || !outBytesPerTexel ) return qfalse;
	switch ( role ) {
	case RAL_LIGHTING_PAYLOAD_RADIANCE:
		if ( encoding == RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8 ) {
			*outFormat = RAL_FORMAT_E5B9G9R9_UFLOAT; *outBytesPerTexel = 4u;
			return qtrue;
		}
		if ( encoding == RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16 ) {
			*outFormat = RAL_FORMAT_R16G16B16A16_SFLOAT; *outBytesPerTexel = 8u;
			return qtrue;
		}
		break;
	case RAL_LIGHTING_PAYLOAD_DIRECTION:
		if ( encoding == RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8 ) {
			*outFormat = RAL_FORMAT_R8G8_UNORM; *outBytesPerTexel = 2u;
			return qtrue;
		}
		if ( encoding == RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16 ) {
			*outFormat = RAL_FORMAT_R16G16_SNORM; *outBytesPerTexel = 4u;
			return qtrue;
		}
		break;
	case RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY:
		*outFormat = RAL_FORMAT_R8_UNORM; *outBytesPerTexel = 1u;
		return qtrue;
	default:
		break;
	}
	return qfalse;
}

static qboolean EncodingSupported( ralStaticLightingEncoding_t encoding,
		const ralStaticLightingCapabilities_t *capabilities ) {
	if ( !capabilities || !capabilities->textureArrays ) return qfalse;
	if ( encoding == RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8 )
		return capabilities->sampledRgb9e5;
	if ( encoding == RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16 )
		return capabilities->sampledRgba16Float && capabilities->sampledRg16Snorm;
	return qfalse;
}

qboolean Ral_LightingRuntimePlanBuild( ralBackendType_t backendType,
		uint64_t frameGeneration, const void *artifactBytes,
		uint64_t artifactByteLength,
		const ralLightingArtifactReceipt_t *expectedArtifact,
		const ralStaticLightingCapabilities_t *capabilities,
		ralLightingRuntimePlan_t *outPlan ) {
	ralLightingArtifactReceipt_t artifact;
	ralLightingPayloadView_t views[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	ralLightingRuntimePlan_t candidate;
	uint64_t texels;
	uint32_t i;
	if ( !outPlan || backendType >= RAL_BACKEND_COUNT || !frameGeneration
			|| frameGeneration == UINT64_MAX || !artifactBytes || !artifactByteLength
			|| !Ral_LightingArtifactRead( artifactBytes, artifactByteLength,
				&artifact, views )
			|| ( expectedArtifact && !Ral_LightingArtifactReceiptExact(
				expectedArtifact, &artifact ) )
			|| artifact.kind != RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP
			|| !EncodingSupported( (ralStaticLightingEncoding_t)artifact.encoding,
				capabilities )
			|| ( artifact.flags & RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY ) == 0u
			|| artifact.payloadCount > RAL_LIGHTING_RUNTIME_MAX_PLANES ) return qfalse;
	texels = (uint64_t)artifact.dimensions[0] * artifact.dimensions[1]
		* artifact.dimensions[2];
	if ( !texels || texels > UINT32_MAX ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_LIGHTING_RUNTIME_PLAN_SCHEMA_VERSION;
	candidate.backendType = backendType;
	candidate.frameGeneration = frameGeneration;
	candidate.artifactGeneration = artifact.artifactGeneration;
	candidate.artifactHash = artifact.manifestHash;
	candidate.manifestHash = artifact.manifestHash;
	candidate.encoding = (ralStaticLightingEncoding_t)artifact.encoding;
	candidate.planeCount = artifact.payloadCount;
	candidate.staticDiffuseIndirectOnly = qtrue;
	for ( i = 0u; i < artifact.payloadCount; ++i ) {
		ralLightingRuntimePlanePlan_t *plane = &candidate.planes[i];
		uint32_t bytesPerTexel;
		if ( !PlaneFormat( candidate.encoding, artifact.payloads[i].role,
				&plane->texture.format, &bytesPerTexel )
				|| artifact.payloads[i].byteLength != texels * bytesPerTexel
				|| artifact.dimensions[0] > UINT32_MAX / bytesPerTexel ) return qfalse;
		plane->role = artifact.payloads[i].role;
		/* Directional products always lower as arrays, including a single layer,
		 * so the draw ABI does not fork sampler/view types by map cardinality. */
		plane->texture.type = RAL_TEXTURE_2D_ARRAY;
		plane->texture.width = artifact.dimensions[0];
		plane->texture.height = artifact.dimensions[1];
		plane->texture.depthOrArrayLayers = artifact.dimensions[2];
		plane->texture.mipLevels = 1u;
		plane->texture.sampleCount = 1u;
		plane->texture.usage = RAL_TEXTURE_USAGE_SAMPLED
			| RAL_TEXTURE_USAGE_TRANSFER_DST;
		plane->texture.memory = RAL_MEMORY_DEVICE_LOCAL;
		plane->texture.concurrentGraphicsTransfer = qtrue;
		plane->artifactOffset = artifact.payloads[i].byteOffset;
		plane->byteLength = artifact.payloads[i].byteLength;
		plane->payloadHash = artifact.payloads[i].payloadHash;
		plane->tightBytesPerRow = artifact.dimensions[0] * bytesPerTexel;
		plane->rowsPerImage = artifact.dimensions[1];
	}
	candidate.ready = qtrue;
	if ( !Ral_LightingRuntimePlanValid( &candidate ) ) return qfalse;
	*outPlan = candidate;
	return qtrue;
}

qboolean Ral_LightingRuntimePlanValid( const ralLightingRuntimePlan_t *plan ) {
	uint32_t i;
	if ( !plan || plan->schemaVersion != RAL_LIGHTING_RUNTIME_PLAN_SCHEMA_VERSION
			|| plan->backendType >= RAL_BACKEND_COUNT || !plan->frameGeneration
			|| plan->frameGeneration == UINT64_MAX || !plan->artifactGeneration
			|| !plan->artifactHash || plan->artifactHash != plan->manifestHash
			|| ( plan->encoding != RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8
				&& plan->encoding != RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16 )
			|| plan->planeCount < 2u
			|| plan->planeCount > RAL_LIGHTING_RUNTIME_MAX_PLANES
			|| !plan->staticDiffuseIndirectOnly || !plan->ready ) return qfalse;
	for ( i = 0u; i < plan->planeCount; ++i ) {
		const ralLightingRuntimePlanePlan_t *plane = &plan->planes[i];
		if ( plane->role < RAL_LIGHTING_PAYLOAD_RADIANCE
				|| plane->role > RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY
				|| plane->texture.format <= RAL_FORMAT_UNDEFINED
				|| plane->texture.format >= RAL_FORMAT_COUNT
				|| !plane->texture.width || !plane->texture.height
				|| !plane->texture.depthOrArrayLayers
				|| plane->texture.mipLevels != 1u
				|| plane->texture.sampleCount != 1u
				|| plane->texture.usage != ( RAL_TEXTURE_USAGE_SAMPLED
					| RAL_TEXTURE_USAGE_TRANSFER_DST )
				|| plane->texture.memory != RAL_MEMORY_DEVICE_LOCAL
				|| !plane->texture.concurrentGraphicsTransfer
				|| !plane->byteLength || !plane->payloadHash
				|| !plane->tightBytesPerRow || !plane->rowsPerImage ) return qfalse;
	}
	return qtrue;
}

qboolean Ral_LightingRuntimePlanExact( const ralLightingRuntimePlan_t *a,
		const ralLightingRuntimePlan_t *b ) {
	return Ral_LightingRuntimePlanValid( a ) && Ral_LightingRuntimePlanValid( b )
		&& !memcmp( a, b, sizeof( *a ) ) ? qtrue : qfalse;
}
