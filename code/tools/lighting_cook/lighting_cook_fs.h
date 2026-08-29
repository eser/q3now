// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_LIGHTING_COOK_FS_H
#define WIRED_LIGHTING_COOK_FS_H

#include "ral_lighting_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRED_LIGHTING_COOK_FS_SCHEMA_VERSION 1u
#define WIRED_LIGHTING_COOK_FS_PATH_CAPACITY 1024u

typedef struct {
	uint32_t schemaVersion;
	char root[WIRED_LIGHTING_COOK_FS_PATH_CAPACITY];
	uint64_t temporarySerial;
	qboolean ready;
} wiredLightingCookFilesystem_t;

qboolean WiredLightingCookFilesystem_Init(
	wiredLightingCookFilesystem_t *filesystem, const char *derivedRoot );
const ralLightingCacheOps_t *WiredLightingCookFilesystem_Ops( void );
qboolean WiredLightingCookFilesystem_PublishSidecar(
	wiredLightingCookFilesystem_t *filesystem, const char *worldStem,
	ralLightingArtifactKind_t kind, const void *bytes, uint64_t byteLength );
qboolean WiredLightingCookFilesystem_ReadSidecar(
	const wiredLightingCookFilesystem_t *filesystem, const char *worldStem,
	ralLightingArtifactKind_t kind, void *bytes, uint64_t capacity,
	uint64_t *outByteLength );

#ifdef __cplusplus
}
#endif

#endif
