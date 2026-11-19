// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_MATERIAL_LUA_H
#define WIRED_RENDER_MATERIAL_LUA_H

#include "../ral/core/ral_material_source.h"

typedef struct lua_State lua_State;

typedef enum {
	RENDER_MATERIAL_LUA_OK = 0,
	RENDER_MATERIAL_LUA_INVALID_ARGUMENT,
	RENDER_MATERIAL_LUA_EXPECTED_TABLE,
	RENDER_MATERIAL_LUA_INVALID_NAME,
	RENDER_MATERIAL_LUA_INVALID_FACTORS,
	RENDER_MATERIAL_LUA_INVALID_RENDER_POLICY,
	RENDER_MATERIAL_LUA_INVALID_TEXTURE,
	RENDER_MATERIAL_LUA_INVALID_PROXY,
	RENDER_MATERIAL_LUA_DEPENDENCY_REJECTED,
	RENDER_MATERIAL_LUA_SOURCE_READ_FAILED,
	RENDER_MATERIAL_LUA_SOURCE_LOAD_FAILED,
	RENDER_MATERIAL_LUA_SOURCE_EXEC_FAILED,
	RENDER_MATERIAL_LUA_SOURCE_COMPILE_FAILED
} renderMaterialLuaFailure_t;

typedef struct {
	renderMaterialLuaFailure_t reason;
	ralMaterialTextureRole_t textureRole;
	char field[32];
	ralMaterialSourceDiagnostic_t source;
} renderMaterialLuaDiagnostic_t;

typedef qboolean (*renderMaterialLuaResolveDependencyFn)( void *context,
	const char *requestedPath, uint64_t creationOptionsHash,
	ralMaterialSourceSpan_t span, ralMaterialSourceDependency_t *outDependency );

typedef struct {
	qboolean (*read)( void *context, const char *canonicalPath,
		const void **outBytes, uint64_t *outByteLength, uint64_t *outSourceId,
		uint64_t *outSourceGeneration );
	void (*release)( void *context, const void *bytes );
} renderMaterialLuaSourceOps_t;

qboolean RenderMaterialLua_ReadTable( lua_State *L, int tableIndex,
	uint64_t materialGeneration, ralMaterialSourceSpan_t declaration,
	renderMaterialLuaResolveDependencyFn resolveDependency, void *resolveContext,
	ralMaterialSourceIr_t *outSource,
	renderMaterialLuaDiagnostic_t *outDiagnostic );
qboolean RenderMaterialLua_Load( lua_State *L, const char *canonicalPath,
	const renderMaterialLuaSourceOps_t *sourceOps, void *sourceContext,
	renderMaterialLuaResolveDependencyFn resolveDependency, void *resolveContext,
	ralMaterialSourceIr_t *outSource,
	renderMaterialLuaDiagnostic_t *outDiagnostic );
qboolean RenderMaterialLua_LoadSourceReceipt( lua_State *L,
	const char *canonicalPath, const renderMaterialLuaSourceOps_t *sourceOps,
	void *sourceContext,
	renderMaterialLuaResolveDependencyFn resolveDependency, void *resolveContext,
	ralMaterialSourceReceipt_t *outReceipt,
	renderMaterialLuaDiagnostic_t *outDiagnostic );

#endif
