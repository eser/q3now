// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_LIGHTING_SIDECAR_H
#define WIRED_RENDER_FRONTEND_LIGHTING_SIDECAR_H

#include "render_submission_lighting.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_LIGHTING_SIDECAR_SCHEMA_VERSION 1u
#define RENDER_IRRADIANCE_SIDECAR_SCHEMA_VERSION 1u
#define RENDER_IRRADIANCE_SIDECAR_HEADER_BYTES 48u
#define RENDER_IRRADIANCE_SIDECAR_ENTRY_BYTES 128u

typedef enum {
	RENDER_LIGHTING_SIDECAR_MISSING_COMPATIBILITY = 0,
	RENDER_LIGHTING_SIDECAR_MODERN_LOADED = 1
} renderLightingSidecarStatus_t;

typedef struct {
	uint32_t schemaVersion;
	renderLightingSidecarStatus_t status;
	char path[MAX_QPATH];
	uint64_t byteLength;
	uint64_t artifactGeneration;
	uint64_t cacheKey;
	qboolean ready;
} renderLightingSidecarReceipt_t;

typedef enum {
	RENDER_IRRADIANCE_SIDECAR_MISSING_COMPATIBILITY = 0,
	RENDER_IRRADIANCE_SIDECAR_MODERN_LOADED = 1
} renderIrradianceSidecarStatus_t;

typedef struct {
	uint32_t schemaVersion;
	renderIrradianceSidecarStatus_t status;
	char path[MAX_QPATH];
	uint64_t byteLength;
	uint64_t manifestHash;
	uint64_t volumeDigest;
	uint32_t volumeCount;
	qboolean ready;
} renderIrradianceSidecarReceipt_t;

qboolean RenderLightingSidecar_LoadDirectional(
	renderSubmissionState_t *submission, const refimport_t *imports,
	const char *worldName, renderLightingSidecarReceipt_t *outReceipt );
qboolean RenderLightingSidecar_ReceiptValid(
	const renderLightingSidecarReceipt_t *receipt );
qboolean RenderLightingSidecar_PackIrradiance(
	const renderIrradianceVolumeSource_t *sources, uint32_t sourceCount,
	void *outBytes, uint64_t outputCapacity, uint64_t *outByteLength,
	uint64_t *outManifestHash );
qboolean RenderLightingSidecar_LoadIrradiance(
	renderSubmissionState_t *submission, const refimport_t *imports,
	const char *worldName, renderIrradianceSidecarReceipt_t *outReceipt );
qboolean RenderLightingSidecar_IrradianceReceiptValid(
	const renderIrradianceSidecarReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif
