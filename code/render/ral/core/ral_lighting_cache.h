// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_LIGHTING_CACHE_H
#define WIRED_RAL_LIGHTING_CACHE_H

#include "ral_lighting_artifact.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_LIGHTING_CACHE_IO_SCHEMA_VERSION 1u
#define RAL_LIGHTING_CACHE_KEY_TEXT_CAPACITY 80u

typedef struct {
	qboolean (*writeAtomic)( void *context, const char *keyText,
		const void *bytes, uint64_t byteLength );
	qboolean (*read)( void *context, const char *keyText,
		void *bytes, uint64_t capacity, uint64_t *outByteLength );
} ralLightingCacheOps_t;

qboolean Ral_LightingArtifactCacheKeyText( ralLightingArtifactKind_t kind,
	uint64_t artifactGeneration, uint64_t cacheKey,
	char *outText, uint32_t capacity );
qboolean Ral_LightingArtifactCacheStore( const ralLightingCacheOps_t *ops,
	void *context, const void *artifactBytes, uint64_t artifactByteLength,
	ralLightingArtifactKind_t expectedKind, uint64_t expectedArtifactGeneration,
	uint64_t expectedCacheKey, ralLightingArtifactReceipt_t *outReceipt );
qboolean Ral_LightingArtifactCacheLoad( const ralLightingCacheOps_t *ops,
	void *context, ralLightingArtifactKind_t expectedKind,
	uint64_t expectedArtifactGeneration, uint64_t expectedCacheKey,
	void *outBytes, uint64_t outCapacity, uint64_t *outByteLength,
	ralLightingArtifactReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif

#endif
