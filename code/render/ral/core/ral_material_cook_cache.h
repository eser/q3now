// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_COOK_CACHE_H
#define WIRED_RAL_MATERIAL_COOK_CACHE_H

#include "ral_material_cook.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY 32u

typedef struct {
	qboolean (*writeAtomic)( void *context, uint64_t artifactHash,
		const char *keyText, const void *bytes, uint64_t byteLength );
	qboolean (*read)( void *context, uint64_t artifactHash,
		const char *keyText, void *bytes, uint64_t capacity,
		uint64_t *outByteLength );
} ralMaterialCookCacheOps_t;

qboolean Ral_MaterialCookCacheKeyText( uint64_t artifactHash,
	char *outText, uint32_t capacity );
qboolean Ral_MaterialCookCacheStore( const ralMaterialCookCacheOps_t *ops,
	void *context, const ralMaterialCookReceipt_t *receipt );
qboolean Ral_MaterialCookCacheLoad( const ralMaterialCookCacheOps_t *ops,
	void *context, uint64_t expectedArtifactHash,
	ralMaterialCookReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif
#endif
