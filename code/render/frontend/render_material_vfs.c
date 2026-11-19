// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_vfs.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	const refimport_t *imports;
} materialVfsContext_t;

static qboolean Resolve( const refimport_t *imports, const char *path,
		char canonical[MAX_QPATH], uint64_t *sourceId, uint64_t *byteSize,
		unsigned *generation ) {
	return imports && imports->FS_ResolveResource && path && path[0]
		&& imports->FS_ResolveResource( path, canonical, MAX_QPATH, sourceId,
			byteSize, generation );
}

static qboolean ReadSource( void *opaque, const char *requestedPath,
		const void **outBytes, uint64_t *outByteLength, uint64_t *outSourceId,
		uint64_t *outSourceGeneration ) {
	materialVfsContext_t *context = opaque;
	const refimport_t *imports = context ? context->imports : NULL;
	char canonical[MAX_QPATH], verifiedCanonical[MAX_QPATH];
	uint64_t sourceId = 0u, byteSize = 0u, verifiedSourceId = 0u,
		verifiedByteSize = 0u;
	unsigned generation = 0u, verifiedGeneration = 0u;
	void *bytes = NULL;
	int readLength;
	if ( !outBytes || !outByteLength || !outSourceId || !outSourceGeneration
			|| !imports || !imports->FS_ReadFile || !imports->FS_FreeFile
			|| !Resolve( imports, requestedPath, canonical, &sourceId, &byteSize,
				&generation ) || !sourceId || !byteSize || !generation ) return qfalse;
	readLength = imports->FS_ReadFile( canonical, &bytes );
	if ( readLength <= 0 || !bytes || (uint64_t)readLength != byteSize
			|| !Resolve( imports, canonical, verifiedCanonical,
				&verifiedSourceId, &verifiedByteSize, &verifiedGeneration )
			|| strcmp( canonical, verifiedCanonical )
			|| sourceId != verifiedSourceId || byteSize != verifiedByteSize
			|| generation != verifiedGeneration ) {
		if ( bytes ) imports->FS_FreeFile( bytes );
		return qfalse;
	}
	*outBytes = bytes;
	*outByteLength = byteSize;
	*outSourceId = sourceId;
	*outSourceGeneration = generation;
	return qtrue;
}

static void ReleaseSource( void *opaque, const void *bytes ) {
	materialVfsContext_t *context = opaque;
	if ( context && context->imports && context->imports->FS_FreeFile && bytes )
		context->imports->FS_FreeFile( (void *)bytes );
}

static qboolean ResolveDependency( void *opaque, const char *requestedPath,
		uint64_t creationOptionsHash, ralMaterialSourceSpan_t span,
		ralMaterialSourceDependency_t *outDependency ) {
	materialVfsContext_t *context = opaque;
	char canonical[MAX_QPATH];
	uint64_t sourceId = 0u, byteSize = 0u;
	unsigned generation = 0u;
	if ( !outDependency || !context
			|| !Resolve( context->imports, requestedPath, canonical, &sourceId,
				&byteSize, &generation ) || !sourceId || !generation ) return qfalse;
	memset( outDependency, 0, sizeof( *outDependency ) );
	outDependency->role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
	outDependency->span = span;
	outDependency->sourceId = sourceId;
	outDependency->fsGeneration = generation;
	outDependency->size = byteSize;
	outDependency->creationOptionsHash = creationOptionsHash;
	(void)snprintf( outDependency->canonicalPath,
		sizeof( outDependency->canonicalPath ), "%s", canonical );
	return qtrue;
}

qboolean RenderMaterialVfs_LoadLuaSourceReceipt( lua_State *L,
		const refimport_t *imports, const char *requestedPath,
		ralMaterialSourceReceipt_t *outReceipt,
		renderMaterialLuaDiagnostic_t *outDiagnostic ) {
	const renderMaterialLuaSourceOps_t sourceOps = { ReadSource, ReleaseSource };
	materialVfsContext_t context;
	if ( !imports || !imports->FS_ResolveResource || !imports->FS_ReadFile
			|| !imports->FS_FreeFile ) return qfalse;
	context.imports = imports;
	return RenderMaterialLua_LoadSourceReceipt( L, requestedPath, &sourceOps,
		&context, ResolveDependency, &context, outReceipt, outDiagnostic );
}
