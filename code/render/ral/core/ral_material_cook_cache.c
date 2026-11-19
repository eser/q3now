// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material_cook_cache.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

qboolean Ral_MaterialCookCacheKeyText( uint64_t artifactHash,
		char *outText, uint32_t capacity ) {
	char text[RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY];
	int length;
	if ( !artifactHash || !outText ) return qfalse;
	length = snprintf( text, sizeof( text ), "wrmat-v4-%016" PRIx64,
		artifactHash );
	if ( length != 25 || capacity <= (uint32_t)length ) return qfalse;
	memcpy( outText, text, (size_t)length + 1u );
	return qtrue;
}

qboolean Ral_MaterialCookCacheStore( const ralMaterialCookCacheOps_t *ops,
		void *context, const ralMaterialCookReceipt_t *receipt ) {
	char keyText[RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY];
	return ops && ops->writeAtomic
		&& Ral_MaterialCookReceiptValid( receipt )
		&& Ral_MaterialCookCacheKeyText( receipt->artifactHash, keyText,
			sizeof( keyText ) )
		&& ops->writeAtomic( context, receipt->artifactHash, keyText,
			receipt, sizeof( *receipt ) );
}

qboolean Ral_MaterialCookCacheLoad( const ralMaterialCookCacheOps_t *ops,
		void *context, uint64_t expectedArtifactHash,
		ralMaterialCookReceipt_t *outReceipt ) {
	ralMaterialCookReceipt_t *candidate;
	char keyText[RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY];
	uint64_t byteLength = 0u;
	if ( !ops || !ops->read || !outReceipt
			|| !Ral_MaterialCookCacheKeyText( expectedArtifactHash, keyText,
				sizeof( keyText ) ) ) return qfalse;
	candidate = malloc( sizeof( *candidate ) );
	if ( !candidate ) return qfalse;
	if ( !ops->read( context, expectedArtifactHash, keyText, candidate,
				sizeof( *candidate ), &byteLength )
			|| byteLength != sizeof( *candidate )
			|| candidate->artifactHash != expectedArtifactHash
			|| !Ral_MaterialCookReceiptValid( candidate ) ) {
		free( candidate );
		return qfalse;
	}
	*outReceipt = *candidate;
	free( candidate );
	return qtrue;
}
