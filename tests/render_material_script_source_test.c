// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_script.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x); return 1; } } while (0)

static qboolean Resolve( const char *name, char *canonical, size_t capacity,
		uint64_t *sourceId, uint64_t *size, unsigned *generation ) {
	if ( strcmp( name, "legacy/wall.tga" )
			&& strcmp( name, "textures/wall.ktx2" ) ) return qfalse;
	(void)snprintf( canonical, capacity, "textures/wall.ktx2" );
	*sourceId = 77u; *size = 4096u; *generation = 5u;
	return qtrue;
}

int main( void ) {
	renderMaterialScriptEntry_t entry;
	refimport_t imports;
	ralMaterialSourceReceipt_t receipt, exact, before;
	ralMaterialSourceDiagnostic_t diagnostic;
	memset( &entry, 0, sizeof( entry ) );
	memset( &imports, 0, sizeof( imports ) );
	(void)snprintf( entry.name, sizeof( entry.name ), "textures/test/wall" );
	(void)snprintf( entry.declarationPath, sizeof( entry.declarationPath ),
		"scripts/test.shader" );
	entry.declarationSourceId = 11u;
	entry.declarationLine = 4u;
	entry.declarationColumn = 2u;
	entry.declarationSize = 512u;
	entry.declarationGeneration = 5u;
	entry.sort = 3.0f;
	entry.cullMode = RENDER_CULL_BACK;
	entry.depthWrite = qtrue;
	entry.skyCloudHeight = 512.0f;
	entry.lighting.diffuseReflectance[0] =
		entry.lighting.diffuseReflectance[1] =
		entry.lighting.diffuseReflectance[2] = 1.0f;
	entry.stageCount = 2u;
	for ( uint32_t i = 0u; i < entry.stageCount; ++i ) {
		entry.stages[i].sourceLine = 6u + i;
		entry.stages[i].sourceColumn = 3u;
		entry.stages[i].imageSource = RENDER_MATERIAL_STAGE_IMAGE;
		(void)snprintf( entry.stages[i].imageName,
			sizeof( entry.stages[i].imageName ), "%s",
			i ? "textures/wall.ktx2" : "legacy/wall.tga" );
		entry.stages[i].sourceBlend = RENDER_MATERIAL_BLEND_ONE;
		entry.stages[i].scale[0] = entry.stages[i].scale[1] = 1.0f;
	}
	imports.FS_ResolveResource = Resolve;
	CHECK( RenderMaterialScript_CompileSource( &entry, &imports, 9u,
		&receipt, &diagnostic ) );
	CHECK( Ral_MaterialSourceReceiptValid( &receipt ) );
	CHECK( receipt.source.dependencyCount == 1u );
	CHECK( receipt.source.declaration.line == 4u );
	CHECK( receipt.source.stages[0].span.line == 6u );
	CHECK( receipt.source.dependencies[0].span.line == 6u );
	CHECK( receipt.source.stages[0].dependencyIndex == 0u );
	CHECK( receipt.source.stages[1].dependencyIndex == 0u );
	CHECK( !strcmp( receipt.source.dependencies[0].canonicalPath,
		"textures/wall.ktx2" ) );
	exact = receipt;
	CHECK( Ral_MaterialSourceReceiptExact( &receipt, &exact ) );
	before = receipt;
	entry.stages[0].imageName[0] = '\0';
	CHECK( !RenderMaterialScript_CompileSource( &entry, &imports, 10u,
		&receipt, &diagnostic ) );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
	puts( "render_material_script_source_test: ok" );
	return 0;
}
