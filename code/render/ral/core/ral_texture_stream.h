// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_TEXTURE_STREAM_H
#define WIRED_RAL_TEXTURE_STREAM_H

#include "ral_residency.h"
#include "ral_texture_cache.h"
#include "ral_texture_upload.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_TEXTURE_STREAM_SCHEMA_VERSION 1u
#define RAL_TEXTURE_STREAM_UPLOAD_SCHEMA_VERSION 1u

/* One receipt covers the complete mip coherence group: every array layer or
 * every depth slice described by the immutable upload plan. */
typedef struct {
	uint32_t schemaVersion;
	uint64_t streamGeneration;
	uint64_t cacheKeyHash;
	ralTextureResourceReceipt_t resource;
	uint32_t mipLevel;
	uint32_t imageCount;
	uint64_t payloadByteLength;
	ralResidencyState_t state;
	uint8_t completedPlaneMask;
} ralTextureStreamUploadReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	uint64_t streamGeneration;
	ralTextureCacheKey_t cacheKey;
	ralTextureUploadPlan_t uploadPlan;
	qboolean resourceBound;
	ralTextureResourceReceipt_t resource;
	uint32_t mipCount;
	ralResidencyPageRecord_t mips[RAL_KTX2_MAX_LEVELS];
} ralTextureStreamCohort_t;

qboolean Ral_TextureStreamCohortInit( ralTextureStreamCohort_t *outCohort,
	const ralTextureCacheKey_t *cacheKey, const ralTextureUploadPlan_t *uploadPlan,
	uint64_t streamGeneration, uint32_t transitionSerial );
qboolean Ral_TextureStreamCohortValid( const ralTextureStreamCohort_t *cohort );
qboolean Ral_TextureStreamBindResource( ralTextureStreamCohort_t *cohort,
	const ralTextureResourceReceipt_t *resource );
qboolean Ral_TextureStreamRequestMip( ralTextureStreamCohort_t *cohort,
	uint32_t mipLevel, uint32_t transitionSerial );
qboolean Ral_TextureStreamUploadBegin( ralTextureStreamCohort_t *cohort,
	uint32_t mipLevel, uint32_t transitionSerial,
	ralTextureStreamUploadReceipt_t *outReceipt );
qboolean Ral_TextureStreamUploadComplete( ralTextureStreamCohort_t *cohort,
	const ralTextureStreamUploadReceipt_t *inFlight, uint8_t completedPlaneMask,
	uint32_t transitionSerial, ralTextureStreamUploadReceipt_t *outReceipt );
qboolean Ral_TextureStreamUploadCancel( ralTextureStreamCohort_t *cohort,
	const ralTextureStreamUploadReceipt_t *inFlight, uint32_t transitionSerial );
qboolean Ral_TextureStreamApplyPressure( ralTextureStreamCohort_t *cohort,
	uint32_t retainCoarsestMipCount, uint32_t transitionSerial,
	uint64_t *outEvictedBytes );
qboolean Ral_TextureStreamInvalidateDevice( ralTextureStreamCohort_t *cohort,
	uint64_t nextStreamGeneration, uint32_t transitionSerial );

#ifdef __cplusplus
}
#endif
#endif
