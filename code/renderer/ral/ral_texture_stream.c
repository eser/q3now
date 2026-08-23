// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_texture_stream.h"

#include <string.h>

static uint32_t ResourceId( uint64_t hash ) {
	uint32_t id = (uint32_t)( hash ^ ( hash >> 32u ) );
	return id ? id : 1u;
}

static qboolean ResourceMatchesPlan( const ralTextureResourceReceipt_t *r,
		const ralTextureUploadPlan_t *p ) {
	uint32_t layers;
	if ( !r || !p || r->schemaVersion != RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION
			|| r->backendType < RAL_BACKEND_VULKAN
			|| r->backendType > RAL_BACKEND_WEBGL2 || !r->textureIdentity
			|| !r->resourceGeneration || r->ready != qtrue ) return qfalse;
	layers = p->texture.type == RAL_TEXTURE_3D ? 1u
		: p->texture.depthOrArrayLayers;
	return r->type == p->texture.type && r->format == p->texture.format
		&& r->usage == p->texture.usage && r->width == p->texture.width
		&& r->height == p->texture.height && r->mipLevels == p->levelCount
		&& r->arrayLayers == layers ? qtrue : qfalse;
}

static qboolean ReceiptMatches( const ralTextureStreamCohort_t *c,
		const ralTextureStreamUploadReceipt_t *r, ralResidencyState_t state ) {
	const ralTextureUploadLevelPlan_t *level;
	if ( !c || !r || r->schemaVersion != RAL_TEXTURE_STREAM_UPLOAD_SCHEMA_VERSION
			|| r->streamGeneration != c->streamGeneration
			|| r->cacheKeyHash != c->cacheKey.keyHash
			|| r->mipLevel >= c->mipCount || r->state != state
			|| memcmp( &r->resource, &c->resource, sizeof( r->resource ) ) )
		return qfalse;
	level = &c->uploadPlan.levels[r->mipLevel];
	return r->imageCount == level->imageCount
		&& r->payloadByteLength == level->artifactLength
		&& r->completedPlaneMask == ( state == RAL_RESIDENCY_RESIDENT ? 1u : 0u )
		? qtrue : qfalse;
}

static void BuildReceipt( const ralTextureStreamCohort_t *c, uint32_t mip,
		ralResidencyState_t state, ralTextureStreamUploadReceipt_t *out ) {
	memset( out, 0, sizeof( *out ) );
	out->schemaVersion = RAL_TEXTURE_STREAM_UPLOAD_SCHEMA_VERSION;
	out->streamGeneration = c->streamGeneration;
	out->cacheKeyHash = c->cacheKey.keyHash;
	out->resource = c->resource; out->mipLevel = mip;
	out->imageCount = c->uploadPlan.levels[mip].imageCount;
	out->payloadByteLength = c->uploadPlan.levels[mip].artifactLength;
	out->state = state;
	out->completedPlaneMask = state == RAL_RESIDENCY_RESIDENT ? 1u : 0u;
}

qboolean Ral_TextureStreamCohortValid( const ralTextureStreamCohort_t *c ) {
	uint32_t i, id;
	if ( !c || c->schemaVersion != RAL_TEXTURE_STREAM_SCHEMA_VERSION
			|| !c->streamGeneration || !Ral_TextureCacheKeyValid( &c->cacheKey )
			|| !Ral_TextureUploadPlanValid( &c->uploadPlan )
			|| !Ral_TextureAssetReceiptExact( &c->cacheKey.asset,
				&c->uploadPlan.asset ) || !c->mipCount
			|| c->mipCount != c->uploadPlan.levelCount
			|| c->mipCount > RAL_KTX2_MAX_LEVELS ) return qfalse;
	if ( c->resourceBound ) {
		if ( !ResourceMatchesPlan( &c->resource, &c->uploadPlan ) ) return qfalse;
	} else {
		ralTextureResourceReceipt_t zero;
		memset( &zero, 0, sizeof( zero ) );
		if ( memcmp( &c->resource, &zero, sizeof( zero ) ) ) return qfalse;
	}
	id = ResourceId( c->cacheKey.keyHash );
	for ( i = 0u; i < c->mipCount; i++ ) {
		const ralResidencyPageRecord_t *p = &c->mips[i];
		if ( p->id.resource != id || p->id.level != c->mipCount - 1u - i
				|| p->id.x || p->id.y || p->id.classId != RAL_RESIDENCY_CLASS_TEXTURE
				|| p->id.planeMask != 1u || p->state >= RAL_RESIDENCY_STATE_COUNT )
			return qfalse;
		if ( p->state == RAL_RESIDENCY_RESIDENT || p->state == RAL_RESIDENCY_STALE ) {
			if ( p->completedPlaneMask != 1u ) return qfalse;
		} else if ( p->completedPlaneMask != 0u ) return qfalse;
		if ( p->id.level == 0u && !p->parentReady ) return qfalse;
	}
	return qtrue;
}

qboolean Ral_TextureStreamCohortInit( ralTextureStreamCohort_t *out,
		const ralTextureCacheKey_t *key, const ralTextureUploadPlan_t *plan,
		uint64_t generation, uint32_t serial ) {
	ralTextureStreamCohort_t v;
	uint32_t i, id;
	if ( !out || !generation || !Ral_TextureCacheKeyValid( key )
			|| !Ral_TextureUploadPlanValid( plan )
			|| !Ral_TextureAssetReceiptExact( &key->asset, &plan->asset )
			|| plan->asset.residency != RAL_TEXTURE_RESIDENCY_STREAMED ) return qfalse;
	memset( &v, 0, sizeof( v ) );
	v.schemaVersion = RAL_TEXTURE_STREAM_SCHEMA_VERSION;
	v.streamGeneration = generation; v.cacheKey = *key; v.uploadPlan = *plan;
	v.mipCount = plan->levelCount; id = ResourceId( key->keyHash );
	for ( i = 0u; i < v.mipCount; i++ ) {
		ralResidencyPageId_t page;
		memset( &page, 0, sizeof( page ) ); page.resource = id;
		page.level = (uint16_t)( v.mipCount - 1u - i );
		page.classId = RAL_RESIDENCY_CLASS_TEXTURE; page.planeMask = 1u;
		if ( !Ral_ResidencyPageRecordInit( &v.mips[i], &page,
			RAL_RESIDENCY_REQUESTED, serial ) ) return qfalse;
	}
	if ( !Ral_TextureStreamCohortValid( &v ) ) return qfalse;
	*out = v; return qtrue;
}

qboolean Ral_TextureStreamBindResource( ralTextureStreamCohort_t *c,
		const ralTextureResourceReceipt_t *resource ) {
	ralTextureStreamCohort_t v;
	if ( !Ral_TextureStreamCohortValid( c ) || c->resourceBound
			|| !ResourceMatchesPlan( resource, &c->uploadPlan ) ) return qfalse;
	v = *c; v.resourceBound = qtrue; v.resource = *resource;
	if ( !Ral_TextureStreamCohortValid( &v ) ) return qfalse;
	*c = v; return qtrue;
}

qboolean Ral_TextureStreamRequestMip( ralTextureStreamCohort_t *c,
		uint32_t mip, uint32_t serial ) {
	ralTextureStreamCohort_t v;
	if ( !Ral_TextureStreamCohortValid( c ) || mip >= c->mipCount ) return qfalse;
	v = *c;
	if ( v.mips[mip].state != RAL_RESIDENCY_ABSENT
			&& v.mips[mip].state != RAL_RESIDENCY_STALE ) return qfalse;
	if ( !Ral_ResidencyPageRecordTransition( &v.mips[mip],
		RAL_RESIDENCY_REQUESTED, 0u, v.mips[mip].id.level == 0u, serial ) )
		return qfalse;
	*c = v; return qtrue;
}

qboolean Ral_TextureStreamUploadBegin( ralTextureStreamCohort_t *c,
		uint32_t mip, uint32_t serial, ralTextureStreamUploadReceipt_t *out ) {
	ralTextureStreamCohort_t v;
	ralTextureStreamUploadReceipt_t receipt;
	if ( !out || !Ral_TextureStreamCohortValid( c ) || !c->resourceBound
			|| mip >= c->mipCount
			|| c->mips[mip].state != RAL_RESIDENCY_REQUESTED ) return qfalse;
	v = *c;
	if ( !Ral_ResidencyPageRecordTransition( &v.mips[mip],
		RAL_RESIDENCY_IN_FLIGHT, 0u, v.mips[mip].id.level == 0u, serial ) )
		return qfalse;
	BuildReceipt( &v, mip, RAL_RESIDENCY_IN_FLIGHT, &receipt );
	*c = v; *out = receipt; return qtrue;
}

qboolean Ral_TextureStreamUploadComplete( ralTextureStreamCohort_t *c,
		const ralTextureStreamUploadReceipt_t *inFlight, uint8_t planes,
		uint32_t serial, ralTextureStreamUploadReceipt_t *out ) {
	ralTextureStreamCohort_t v;
	ralTextureStreamUploadReceipt_t receipt;
	uint32_t mip;
	int parentReady;
	if ( !out || !Ral_TextureStreamCohortValid( c ) || !c->resourceBound
			|| !ReceiptMatches( c, inFlight, RAL_RESIDENCY_IN_FLIGHT ) ) return qfalse;
	mip = inFlight->mipLevel;
	if ( c->mips[mip].state != RAL_RESIDENCY_IN_FLIGHT ) return qfalse;
	parentReady = mip + 1u == c->mipCount
		|| c->mips[mip + 1u].state == RAL_RESIDENCY_RESIDENT;
	v = *c;
	if ( !Ral_ResidencyPageRecordTransition( &v.mips[mip],
		RAL_RESIDENCY_RESIDENT, planes, parentReady, serial ) ) return qfalse;
	BuildReceipt( &v, mip, RAL_RESIDENCY_RESIDENT, &receipt );
	*c = v; *out = receipt; return qtrue;
}

qboolean Ral_TextureStreamUploadCancel( ralTextureStreamCohort_t *c,
		const ralTextureStreamUploadReceipt_t *inFlight, uint32_t serial ) {
	ralTextureStreamCohort_t v;
	uint32_t mip;
	if ( !Ral_TextureStreamCohortValid( c ) || !c->resourceBound
			|| !ReceiptMatches( c, inFlight, RAL_RESIDENCY_IN_FLIGHT ) ) return qfalse;
	mip = inFlight->mipLevel;
	if ( c->mips[mip].state != RAL_RESIDENCY_IN_FLIGHT ) return qfalse;
	v = *c;
	if ( !Ral_ResidencyPageRecordTransition( &v.mips[mip],
		RAL_RESIDENCY_REQUESTED, 0u, v.mips[mip].id.level == 0u, serial ) )
		return qfalse;
	*c = v; return qtrue;
}

qboolean Ral_TextureStreamApplyPressure( ralTextureStreamCohort_t *c,
		uint32_t retain, uint32_t serial, uint64_t *outBytes ) {
	ralTextureStreamCohort_t v;
	uint64_t bytes = 0u;
	uint32_t i, firstRetained;
	if ( !outBytes || !Ral_TextureStreamCohortValid( c ) || !c->resourceBound
			|| !retain || retain > c->mipCount ) return qfalse;
	firstRetained = c->mipCount - retain;
	for ( i = firstRetained; i < c->mipCount; i++ )
		if ( c->mips[i].state != RAL_RESIDENCY_RESIDENT ) return qfalse;
	v = *c;
	for ( i = 0u; i < firstRetained; i++ ) {
		ralResidencyPageRecord_t *p = &v.mips[i];
		if ( p->state == RAL_RESIDENCY_RESIDENT || p->state == RAL_RESIDENCY_STALE ) {
			bytes += v.uploadPlan.levels[i].artifactLength;
			if ( !Ral_ResidencyPageRecordTransition( p, RAL_RESIDENCY_EVICTING,
				p->completedPlaneMask, qtrue, serial )
					|| !Ral_ResidencyPageRecordTransition( p, RAL_RESIDENCY_ABSENT,
						0u, qfalse, serial ) ) return qfalse;
		} else if ( p->state == RAL_RESIDENCY_REQUESTED
				|| p->state == RAL_RESIDENCY_IN_FLIGHT ) {
			if ( !Ral_ResidencyPageRecordTransition( p, RAL_RESIDENCY_ABSENT,
				0u, qfalse, serial ) ) return qfalse;
		} else if ( p->state != RAL_RESIDENCY_ABSENT ) return qfalse;
	}
	if ( !Ral_TextureStreamCohortValid( &v ) ) return qfalse;
	*c = v; *outBytes = bytes; return qtrue;
}

qboolean Ral_TextureStreamInvalidateDevice( ralTextureStreamCohort_t *c,
		uint64_t nextGeneration, uint32_t serial ) {
	ralTextureStreamCohort_t v;
	uint32_t i;
	if ( !Ral_TextureStreamCohortValid( c ) || !c->resourceBound
			|| !nextGeneration || nextGeneration == c->streamGeneration ) return qfalse;
	v = *c;
	for ( i = 0u; i < v.mipCount; i++ ) {
		ralResidencyPageRecord_t *p = &v.mips[i];
		if ( p->state == RAL_RESIDENCY_RESIDENT )
			if ( !Ral_ResidencyPageRecordTransition( p, RAL_RESIDENCY_STALE,
				1u, p->id.level == 0u, serial ) ) return qfalse;
		if ( p->state == RAL_RESIDENCY_EVICTING )
			if ( !Ral_ResidencyPageRecordTransition( p, RAL_RESIDENCY_ABSENT,
				0u, p->id.level == 0u, serial ) ) return qfalse;
		if ( p->state != RAL_RESIDENCY_REQUESTED
				&& !Ral_ResidencyPageRecordTransition( p, RAL_RESIDENCY_REQUESTED,
					0u, p->id.level == 0u, serial ) ) return qfalse;
	}
	v.streamGeneration = nextGeneration; v.resourceBound = qfalse;
	memset( &v.resource, 0, sizeof( v.resource ) );
	if ( !Ral_TextureStreamCohortValid( &v ) ) return qfalse;
	*c = v; return qtrue;
}
