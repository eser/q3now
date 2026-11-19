// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_vfs.h"

#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static const char materialText[] = "return {name='materials/vfs',textures={"
	"base_color={path='textures/legacy.png'},"
	"emissive={path='textures/canonical.ktx2'}}}";
static unsigned generation = 9u, reads, frees, resolves;
static qboolean mutateAfterRead;

static qboolean Resolve( const char *path, char *canonical, size_t capacity,
		uint64_t *sourceId, uint64_t *byteSize, unsigned *outGeneration ) {
	resolves++;
	if ( strstr( path, "materials/" ) ) {
		(void)snprintf( canonical, capacity, "materials/vfs.lua" );
		if ( sourceId ) *sourceId = 701u;
		if ( byteSize ) *byteSize = sizeof( materialText ) - 1u;
	} else if ( strstr( path, "textures/" ) ) {
		(void)snprintf( canonical, capacity, "textures/canonical.ktx2" );
		if ( sourceId ) *sourceId = 801u;
		if ( byteSize ) *byteSize = 4096u;
	} else return qfalse;
	if ( outGeneration ) *outGeneration = generation;
	return qtrue;
}

static int Read( const char *path, void **out ) {
	(void)path; reads++;
	*out = (void *)materialText;
	if ( mutateAfterRead ) generation++;
	return (int)( sizeof( materialText ) - 1u );
}

static void Free( void *bytes ) {
	if ( bytes == materialText ) frees++;
}

int main( void ) {
	lua_State *L = luaL_newstate();
	refimport_t imports;
	ralMaterialSourceReceipt_t receipt, before;
	renderMaterialLuaDiagnostic_t diagnostic;
	memset( &imports, 0, sizeof( imports ) );
	imports.FS_ResolveResource = Resolve;
	imports.FS_ReadFile = Read;
	imports.FS_FreeFile = Free;
	CHECK( L != NULL );
	CHECK( RenderMaterialVfs_LoadLuaSourceReceipt( L, &imports,
		"materials/alias.lua", &receipt, &diagnostic ) );
	CHECK( Ral_MaterialSourceReceiptValid( &receipt ) );
	CHECK( receipt.source.declaration.sourceId == 701u );
	CHECK( receipt.source.dependencyCount == 1u );
	CHECK( receipt.source.dependencies[0].sourceId == 801u );
	CHECK( reads == 1u && frees == 1u && resolves == 4u );
	before = receipt;
	mutateAfterRead = qtrue;
	CHECK( !RenderMaterialVfs_LoadLuaSourceReceipt( L, &imports,
		"materials/alias.lua", &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_LUA_SOURCE_READ_FAILED );
	CHECK( reads == 2u && frees == 2u );
	CHECK( !memcmp( &receipt, &before, sizeof( receipt ) ) );
	lua_close( L );
	puts( "render_material_vfs_test: ok" );
	return 0;
}
