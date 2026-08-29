// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_LIGHTING_ARTIFACT_H
#define WIRED_RAL_LIGHTING_ARTIFACT_H

#include "ral_static_lighting.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_ARTIFACT_RECEIPT_SCHEMA_VERSION 1u
#define RAL_LIGHTING_ARTIFACT_WIRE_HEADER_BYTES 240u
#define RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS 5u
#define RAL_LIGHTING_ARTIFACT_MAX_BYTES UINT64_C(1073741824)

typedef enum {
	RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP = 1,
	RAL_LIGHTING_ARTIFACT_IRRADIANCE_VOLUME
} ralLightingArtifactKind_t;

typedef enum {
	RAL_LIGHTING_PAYLOAD_RADIANCE = 1,
	RAL_LIGHTING_PAYLOAD_DIRECTION,
	RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY,
	RAL_LIGHTING_PAYLOAD_SH_COEFFICIENTS,
	RAL_LIGHTING_PAYLOAD_PROBE_VALIDITY
} ralLightingPayloadRole_t;

enum {
	RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY = 1u << 0,
	RAL_LIGHTING_ARTIFACT_HAS_STATIONARY_VISIBILITY = 1u << 1
};

typedef struct {
	ralLightingPayloadRole_t role;
	const void *bytes;
	uint64_t byteLength;
} ralLightingPayloadView_t;

typedef struct {
	uint32_t schemaVersion;
	ralLightingArtifactKind_t kind;
	uint64_t artifactGeneration;
	uint64_t cacheKey;
	uint64_t producerVersion;
	uint32_t encoding;
	uint32_t flags;
	uint32_t dimensions[3];
	uint32_t payloadCount;
} ralLightingArtifactDefinition_t;

typedef struct {
	ralLightingPayloadRole_t role;
	uint64_t byteOffset;
	uint64_t byteLength;
	uint64_t payloadHash;
} ralLightingArtifactPayload_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t artifactGeneration;
	uint64_t cacheKey;
	uint64_t producerVersion;
	ralLightingArtifactKind_t kind;
	uint32_t encoding;
	uint32_t flags;
	uint32_t dimensions[3];
	uint32_t payloadCount;
	ralLightingArtifactPayload_t payloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS];
	uint64_t manifestHash;
	uint64_t byteLength;
	qboolean ready;
} ralLightingArtifactReceipt_t;

qboolean Ral_LightingArtifactMeasure( const ralLightingArtifactDefinition_t *definition,
	const ralLightingPayloadView_t *payloads, uint64_t *outByteLength );
qboolean Ral_LightingArtifactWrite( const ralLightingArtifactDefinition_t *definition,
	const ralLightingPayloadView_t *payloads, void *destination, uint64_t capacity,
	ralLightingArtifactReceipt_t *outReceipt );
qboolean Ral_LightingArtifactRead( const void *bytes, uint64_t byteLength,
	ralLightingArtifactReceipt_t *outReceipt,
	ralLightingPayloadView_t outPayloads[RAL_LIGHTING_ARTIFACT_MAX_PAYLOADS] );
qboolean Ral_LightingArtifactReceiptValid( const ralLightingArtifactReceipt_t *receipt );
qboolean Ral_LightingArtifactReceiptExact( const ralLightingArtifactReceipt_t *a,
	const ralLightingArtifactReceipt_t *b );

#ifdef __cplusplus
}
#endif
#endif
