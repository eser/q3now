// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting_cache.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static qboolean ExpectedValid( ralLightingArtifactKind_t kind,
	uint64_t generation, uint64_t cacheKey )
{
	return kind >= RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP &&
		kind <= RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME && generation && cacheKey;
}

static qboolean ReceiptMatches( const ralLightingArtifactReceipt_t *receipt,
	ralLightingArtifactKind_t kind, uint64_t generation, uint64_t cacheKey )
{
	return Ral_LightingArtifactReceiptValid( receipt ) && receipt->kind == kind &&
		receipt->artifactGeneration == generation && receipt->cacheKey == cacheKey;
}

qboolean Ral_LightingArtifactCacheKeyText( ralLightingArtifactKind_t kind,
	uint64_t generation, uint64_t cacheKey, char *out, uint32_t capacity )
{
	char text[RAL_LIGHTING_CACHE_KEY_TEXT_CAPACITY];
	int length;
	if ( !out || !ExpectedValid( kind, generation, cacheKey ) )
		return qfalse;
	length = snprintf( text, sizeof( text ), "wrlight-v1-%u-%016" PRIx64 "-%016" PRIx64,
		(uint32_t)kind, generation, cacheKey );
	if ( length < 0 || (uint32_t)length >= sizeof( text ) || capacity <= (uint32_t)length )
		return qfalse;
	memcpy( out, text, (size_t)length + 1u );
	return qtrue;
}

qboolean Ral_LightingArtifactCacheStore( const ralLightingCacheOps_t *ops,
	void *context, const void *bytes, uint64_t length,
	ralLightingArtifactKind_t kind, uint64_t generation, uint64_t cacheKey,
	ralLightingArtifactReceipt_t *outReceipt )
{
	ralLightingArtifactReceipt_t receipt;
	ralLightingPayloadView_t payloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	char keyText[RAL_LIGHTING_CACHE_KEY_TEXT_CAPACITY];
	if ( !ops || !ops->writeAtomic || !bytes || !outReceipt ||
		!Ral_LightingArtifactCacheKeyText( kind, generation, cacheKey,
			keyText, sizeof( keyText ) ) ||
		!Ral_LightingArtifactRead( bytes, length, &receipt, payloads ) ||
		!ReceiptMatches( &receipt, kind, generation, cacheKey ) ||
		!ops->writeAtomic( context, keyText, bytes, length ) )
		return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

qboolean Ral_LightingArtifactCacheLoad( const ralLightingCacheOps_t *ops,
	void *context, ralLightingArtifactKind_t kind, uint64_t generation,
	uint64_t cacheKey, void *outBytes, uint64_t capacity,
	uint64_t *outLength, ralLightingArtifactReceipt_t *outReceipt )
{
	ralLightingArtifactReceipt_t receipt;
	ralLightingPayloadView_t payloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	char keyText[RAL_LIGHTING_CACHE_KEY_TEXT_CAPACITY];
	unsigned char *scratch;
	uint64_t length = 0u;
	qboolean ok = qfalse;
	if ( !ops || !ops->read || !outBytes || !outLength || !outReceipt || !capacity ||
		capacity > (uint64_t)SIZE_MAX ||
		!Ral_LightingArtifactCacheKeyText( kind, generation, cacheKey,
			keyText, sizeof( keyText ) ) )
		return qfalse;
	scratch = (unsigned char *)malloc( (size_t)capacity );
	if ( !scratch )
		return qfalse;
	if ( ops->read( context, keyText, scratch, capacity, &length ) && length &&
		length <= capacity && Ral_LightingArtifactRead( scratch, length, &receipt, payloads ) &&
		ReceiptMatches( &receipt, kind, generation, cacheKey ) ) {
		memcpy( outBytes, scratch, (size_t)length );
		*outLength = length;
		*outReceipt = receipt;
		ok = qtrue;
	}
	free( scratch );
	return ok;
}
