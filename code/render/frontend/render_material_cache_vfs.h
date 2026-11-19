// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_MATERIAL_CACHE_VFS_H
#define WIRED_RENDER_MATERIAL_CACHE_VFS_H

#include "../ral/core/ral_material_cook_cache.h"
#include "../ral/core/ral_material_package.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	const refimport_t *imports;
	char root[MAX_QPATH];
	qboolean ready;
} renderMaterialCacheVfs_t;

qboolean RenderMaterialCacheVfs_Init( renderMaterialCacheVfs_t *cache,
	const refimport_t *imports, const char *root );
const ralMaterialCookCacheOps_t *RenderMaterialCacheVfs_MaterialOps( void );
const ralMaterialPackageCacheOps_t *RenderMaterialCacheVfs_PackageOps( void );
const ralMaterialProjectReferenceOps_t *
	RenderMaterialCacheVfs_ProjectReferenceOps( void );

#ifdef __cplusplus
}
#endif

#endif
