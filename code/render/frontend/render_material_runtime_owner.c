// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_runtime_owner.h"
#include "maps/map_format_registry.h"

#include <string.h>

qboolean RenderMaterialRuntimeOwner_Init( renderMaterialRuntimeOwner_t *owner,
		const refimport_t *imports ) {
	renderMaterialRuntimeOwner_t candidate;
	if ( !owner || !imports ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	if ( !RenderMaterialCacheVfs_Init( &candidate.cache, imports,
			"materials/cooked" ) ) return qfalse;
	candidate.ready = qtrue;
	*owner = candidate;
	return qtrue;
}

qboolean RenderMaterialRuntimeOwner_PublishWorld(
		renderMaterialRuntimeOwner_t *owner, const mapFile_t *map,
		int worldIndex, uint64_t runtimeGeneration,
		renderMaterialRuntimeResolveResourceFn resolveResource,
		void *resolveResourceContext,
		renderMaterialPackageDiagnostic_t *outDiagnostic ) {
	renderMaterialRuntimeCatalog_t candidate;
	if ( !owner || owner->ready != qtrue || !map || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS ) return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	if ( !RenderMaterialPackage_LoadProjectRuntimeCatalog( map->name,
			(uint32_t)map->checksum,
			RenderMaterialCacheVfs_ProjectReferenceOps(), &owner->cache,
			RenderMaterialCacheVfs_PackageOps(), &owner->cache,
			RenderMaterialCacheVfs_MaterialOps(), &owner->cache,
			resolveResource, resolveResourceContext, runtimeGeneration,
			&candidate, outDiagnostic ) ) return qfalse;
	RenderMaterialRuntimeCatalog_Destroy( &owner->worlds[worldIndex] );
	owner->worlds[worldIndex] = candidate;
	return qtrue;
}

void RenderMaterialRuntimeOwner_UnloadWorld(
		renderMaterialRuntimeOwner_t *owner, int worldIndex ) {
	if ( !owner || worldIndex < 0 || worldIndex >= MAX_RENDER_WORLDS ) return;
	RenderMaterialRuntimeCatalog_Destroy( &owner->worlds[worldIndex] );
}

void RenderMaterialRuntimeOwner_Destroy( renderMaterialRuntimeOwner_t *owner ) {
	if ( !owner ) return;
	for ( int worldIndex = 0; worldIndex < MAX_RENDER_WORLDS; ++worldIndex )
		RenderMaterialRuntimeCatalog_Destroy( &owner->worlds[worldIndex] );
	memset( owner, 0, sizeof( *owner ) );
}

const renderMaterialRuntimeCatalog_t *RenderMaterialRuntimeOwner_World(
		const renderMaterialRuntimeOwner_t *owner, int worldIndex ) {
	const renderMaterialRuntimeCatalog_t *catalog;
	if ( !owner || owner->ready != qtrue || worldIndex < 0
			|| worldIndex >= MAX_RENDER_WORLDS ) return NULL;
	catalog = &owner->worlds[worldIndex];
	return catalog->ready == qtrue ? catalog : NULL;
}
