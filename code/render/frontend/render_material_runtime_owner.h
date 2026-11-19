// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_MATERIAL_RUNTIME_OWNER_H
#define WIRED_RENDER_MATERIAL_RUNTIME_OWNER_H

#include "render_material_cache_vfs.h"
#include "render_material_package.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	renderMaterialCacheVfs_t cache;
	renderMaterialRuntimeCatalog_t worlds[MAX_RENDER_WORLDS];
	qboolean ready;
} renderMaterialRuntimeOwner_t;

qboolean RenderMaterialRuntimeOwner_Init( renderMaterialRuntimeOwner_t *owner,
	const refimport_t *imports );
qboolean RenderMaterialRuntimeOwner_PublishWorld(
	renderMaterialRuntimeOwner_t *owner, const mapFile_t *map, int worldIndex,
	uint64_t runtimeGeneration,
	renderMaterialRuntimeResolveResourceFn resolveResource,
	void *resolveResourceContext,
	renderMaterialPackageDiagnostic_t *outDiagnostic );
void RenderMaterialRuntimeOwner_UnloadWorld(
	renderMaterialRuntimeOwner_t *owner, int worldIndex );
void RenderMaterialRuntimeOwner_Destroy( renderMaterialRuntimeOwner_t *owner );
const renderMaterialRuntimeCatalog_t *RenderMaterialRuntimeOwner_World(
	const renderMaterialRuntimeOwner_t *owner, int worldIndex );

#ifdef __cplusplus
}
#endif

#endif
