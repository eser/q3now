// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_MATERIAL_COOK_FS_H
#define WIRED_MATERIAL_COOK_FS_H

#include "ral_material_cook_cache.h"
#include "ral_material_package.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRED_MATERIAL_COOK_FS_SCHEMA_VERSION 1u
#define WIRED_MATERIAL_COOK_FS_PATH_CAPACITY 1024u

typedef struct {
	uint32_t schemaVersion;
	char root[WIRED_MATERIAL_COOK_FS_PATH_CAPACITY];
	qboolean ready;
} wiredMaterialCookFilesystem_t;

qboolean WiredMaterialCookFilesystem_Init(
	wiredMaterialCookFilesystem_t *filesystem, const char *derivedRoot );
const ralMaterialCookCacheOps_t *WiredMaterialCookFilesystem_MaterialOps( void );
const ralMaterialPackageCacheOps_t *WiredMaterialCookFilesystem_PackageOps( void );
const ralMaterialProjectReferenceOps_t *
	WiredMaterialCookFilesystem_ProjectReferenceOps( void );

#ifdef __cplusplus
}
#endif

#endif
