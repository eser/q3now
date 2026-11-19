// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_lua.h"
#include "ral_material_proxy_lower.h"

#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static qboolean Resolve( void *opaque, const char *path, uint64_t options,
		ralMaterialSourceSpan_t span, ralMaterialSourceDependency_t *out ) {
	uint64_t *nextId = opaque;
	if ( !path || !strstr( path, "textures/" ) || !out ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
	out->span = span;
	out->sourceId = ( *nextId )++;
	out->fsGeneration = 7u;
	out->size = 4096u;
	out->creationOptionsHash = options;
	(void)snprintf( out->canonicalPath, sizeof( out->canonicalPath ), "%s", path );
	return qtrue;
}

static qboolean ResolveAlias( void *opaque, const char *path, uint64_t options,
		ralMaterialSourceSpan_t span, ralMaterialSourceDependency_t *out ) {
	(void)opaque;
	if ( !path || !out ) return qfalse;
	memset( out, 0, sizeof( *out ) );
	out->role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
	out->span = span;
	out->sourceId = 4401u;
	out->fsGeneration = 7u;
	out->size = 4096u;
	out->creationOptionsHash = options;
	(void)snprintf( out->canonicalPath, sizeof( out->canonicalPath ),
		"textures/shared.ktx2" );
	return qtrue;
}

static qboolean Load( lua_State *L, const char *text ) {
	lua_settop( L, 0 );
	return luaL_loadstring( L, text ) == 0 && lua_pcall( L, 0, 1, 0 ) == 0;
}

typedef struct {
	const char *text;
	unsigned reads;
	unsigned releases;
} sourceStore_t;

static qboolean ReadSource( void *opaque, const char *path,
		const void **outBytes, uint64_t *outByteLength, uint64_t *outSourceId,
		uint64_t *outSourceGeneration ) {
	sourceStore_t *store = opaque;
	if ( !store || strcmp( path, "materials/panel.lua" ) ) return qfalse;
	store->reads++;
	*outBytes = store->text;
	*outByteLength = strlen( store->text );
	*outSourceId = 701u;
	*outSourceGeneration = 9u;
	return qtrue;
}

static void ReleaseSource( void *opaque, const void *bytes ) {
	sourceStore_t *store = opaque;
	if ( store && bytes == store->text ) store->releases++;
}

static qboolean Material( ralMaterialReceipt_t *out ) {
	ralMaterialDescription_t description;
	memset( &description, 0, sizeof( description ) );
	description.schemaVersion = RAL_MATERIAL_SCHEMA_VERSION;
	description.materialGeneration = 9u;
	description.provenanceHash = 701u;
	description.source = RAL_MATERIAL_SOURCE_PBR;
	description.factors.baseColor[0] = description.factors.baseColor[1] =
		description.factors.baseColor[2] = description.factors.baseColor[3] = 1.0f;
	description.factors.roughness = 1.0f;
	description.factors.normalScale = 1.0f;
	description.factors.occlusionStrength = 1.0f;
	description.renderPolicy.depthTest = qtrue;
	return Ral_MaterialCompile( &description, out );
}

static const ralMaterialExpressionField_t *FindField(
		const ralMaterialProxyProgramReceipt_t *receipt, const char *name ) {
	for ( uint32_t i = 0u; i < receipt->program.expressionFieldCount; ++i )
		if ( !strcmp( receipt->program.expressionFields[i].name, name ) )
			return &receipt->program.expressionFields[i];
	return NULL;
}

int main( void ) {
	const char *sourceText =
		"return { name='materials/metal/panel',"
		" factors={base_color={0.8,0.7,0.6,1},metallic=.75,roughness=.3,"
		" normal_scale=1.2,occlusion_strength=.9,emissive={.1,.2,.3}},"
		" render={alpha_mode='opaque',double_sided=false,depth_test=true,"
		" depth_write=true}, textures={"
		" normal={path='textures/panel_n.ktx2',uv=0,creation_options=17},"
		" base_color={path='textures/panel_bc.ktx2'}} }";
	lua_State *L = luaL_newstate();
	ralMaterialSourceIr_t ir, before;
	ralMaterialSourceReceipt_t receipt, receiptBefore;
	renderMaterialLuaDiagnostic_t diagnostic;
	const ralMaterialSourceSpan_t span = { 701u, 1u, 1u };
	uint64_t nextId = 1001u;
	CHECK( L != NULL && Load( L, sourceText ) );
	memset( &ir, 0xA5, sizeof( ir ) );
	CHECK( RenderMaterialLua_ReadTable( L, -1, 9u, span, Resolve, &nextId,
		&ir, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_LUA_OK );
	CHECK( ir.provenance == RAL_MATERIAL_PROVENANCE_WIRED_LUA );
	CHECK( ir.pbrTextureCount == 2u && ir.dependencyCount == 2u );
	CHECK( ir.pbrTextures[0].role == RAL_MATERIAL_TEXTURE_BASE_COLOR );
	CHECK( ir.pbrTextures[1].role == RAL_MATERIAL_TEXTURE_NORMAL );
	CHECK( ir.pbrFactors.metallic == 0.75f && ir.pbrFactors.roughness == 0.3f );
	CHECK( ir.dependencies[1].creationOptionsHash == 17u );
	CHECK( Ral_MaterialSourceCompile( &ir, &receipt, NULL ) );
	before = ir;
	CHECK( Load( L, "return {name='bad',textures={normal={path=4}}}" ) );
	nextId = 2001u;
	CHECK( !RenderMaterialLua_ReadTable( L, -1, 10u, span, Resolve, &nextId,
		&ir, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_LUA_INVALID_TEXTURE );
	CHECK( !memcmp( &ir, &before, sizeof( ir ) ) );

	const renderMaterialLuaSourceOps_t sourceOps = { ReadSource, ReleaseSource };
	sourceStore_t store = { sourceText, 0u, 0u };
	nextId = 3001u;
	CHECK( RenderMaterialLua_Load( L, "materials/panel.lua", &sourceOps, &store,
		Resolve, &nextId, &ir, &diagnostic ) );
	CHECK( store.reads == 1u && store.releases == 1u );
	CHECK( lua_gettop( L ) == 1 );
	CHECK( Ral_MaterialSourceCompile( &ir, &receipt, NULL ) );
	CHECK( RenderMaterialLua_LoadSourceReceipt( L, "materials/panel.lua",
		&sourceOps, &store, Resolve, &nextId, &receipt, &diagnostic ) );
	CHECK( Ral_MaterialSourceReceiptValid( &receipt ) );
	CHECK( store.reads == 2u && store.releases == 2u );
	receiptBefore = receipt;
	store.text = "return {name='materials/invalid',factors={roughness=2}}";
	CHECK( !RenderMaterialLua_LoadSourceReceipt( L, "materials/panel.lua",
		&sourceOps, &store, Resolve, &nextId, &receipt, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_LUA_SOURCE_COMPILE_FAILED );
	CHECK( diagnostic.source.reason == RAL_MATERIAL_SOURCE_NONFINITE_VALUE );
	CHECK( !memcmp( &receipt, &receiptBefore, sizeof( receipt ) ) );
	store.text = "return function() end";
	before = ir;
	CHECK( !RenderMaterialLua_Load( L, "materials/panel.lua", &sourceOps,
		&store, Resolve, &nextId, &ir, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_LUA_EXPECTED_TABLE );
	CHECK( store.reads == 4u && store.releases == 4u );
	CHECK( !memcmp( &ir, &before, sizeof( ir ) ) );

	CHECK( Load( L, "return {name='materials/alias',textures={"
		"base_color={path='textures/old-name.png'},"
		"emissive={path='textures/new-name.ktx2'}}}" ) );
	CHECK( RenderMaterialLua_ReadTable( L, -1, 11u, span, ResolveAlias, NULL,
		&ir, &diagnostic ) );
	CHECK( ir.pbrTextureCount == 2u && ir.dependencyCount == 1u );
	CHECK( ir.pbrTextures[0].dependencyIndex == 0u
		&& ir.pbrTextures[1].dependencyIndex == 0u );
	CHECK( Ral_MaterialSourceCompile( &ir, &receipt, NULL ) );

	/* Wired-authored conditions compile at load/cook time into the same
	 * artifact-bound proxy-v2 VM used by imported Q3 runtime expressions. */
	const char *proxyText =
		"return {name='materials/proxy/conditional',proxy={"
		"tables={{name='pulse',samples={0,1,0},wrap=true}},fields={"
		"{name='emissive.visibility',components=1,expression={op='and',"
		"a={op='gt',a='scalar',b=.5},b={op='lt',a='time',b=10}}},"
		"{name='emissive.pulse',components=1,expression={op='table',"
		"table='pulse',a={op='mod',a='time',b=1}}}}}}";
	ralMaterialReceipt_t material;
	ralMaterialProxyProgramReceipt_t proxyReceipt;
	ralMaterialProxyEvaluationReceipt_t evaluation;
	ralMaterialProxyInputs_t inputs;
	qboolean hasProxy = qfalse;
	CHECK( Load( L, proxyText ) );
	CHECK( RenderMaterialLua_ReadTable( L, -1, 9u, span, Resolve, &nextId,
		&ir, &diagnostic ) );
	CHECK( ir.hasAuthoredProxy
		&& ir.authoredProxy.materialArtifactHash == 0u
		&& ir.authoredProxy.expressionFieldCount == 2u
		&& ir.authoredProxy.expressionTableCount == 1u );
	CHECK( Ral_MaterialSourceCompile( &ir, &receipt, NULL ) );
	const uint64_t conditionalHash = receipt.semanticHash;
	receiptBefore = receipt;
	ir.authoredProxy.materialArtifactHash = 1u;
	CHECK( !Ral_MaterialSourceCompile( &ir, &receipt, NULL )
		&& !memcmp( &receipt, &receiptBefore, sizeof( receipt ) ) );
	ir.authoredProxy.materialArtifactHash = 0u;
	receipt = receiptBefore;
	CHECK( Material( &material ) );
	CHECK( Ral_MaterialProxyLowerSource( &receipt, &material, &hasProxy,
		&proxyReceipt, NULL ) && hasProxy );
	memset( &inputs, 0, sizeof( inputs ) );
	inputs.evaluationGeneration = 1u;
	inputs.timeMilliseconds = 1000u;
	inputs.scalarQ16 = RAL_MATERIAL_PROXY_Q16_ONE;
	CHECK( Ral_MaterialProxyEvaluate( &material, &proxyReceipt, &inputs,
		RAL_MATERIAL_PROXY_DOMAIN_RUNTIME, RAL_MATERIAL_PROXY_VALUE_COUNT,
		&evaluation ) );
	const ralMaterialExpressionField_t *visibility =
		FindField( &proxyReceipt, "emissive.visibility" );
	CHECK( visibility && evaluation.valuesQ16[visibility->outputValueOffset]
		== RAL_MATERIAL_PROXY_Q16_ONE );
	inputs.timeMilliseconds = 11000u;
	inputs.evaluationGeneration++;
	CHECK( Ral_MaterialProxyEvaluate( &material, &proxyReceipt, &inputs,
		RAL_MATERIAL_PROXY_DOMAIN_RUNTIME, RAL_MATERIAL_PROXY_VALUE_COUNT,
		&evaluation ) );
	CHECK( evaluation.valuesQ16[visibility->outputValueOffset] == 0 );

	CHECK( Load( L,
		"return {name='materials/proxy/conditional',proxy={fields={{"
		"name='emissive.visibility',components=1,expression={op='lt',"
		"a='time',b=11}}}}}" ) );
	CHECK( RenderMaterialLua_ReadTable( L, -1, 9u, span, Resolve, &nextId,
		&ir, &diagnostic ) );
	CHECK( Ral_MaterialSourceCompile( &ir, &receipt, NULL )
		&& receipt.semanticHash != conditionalHash );

	/* Malformed and over-deep authored programs preserve the last valid IR. */
	before = ir;
	CHECK( Load( L,
		"return {name='materials/proxy/bad',proxy={fields={{name='x',"
		"components=1,expression={op='bogus',a=1,b=2}}}}}" ) );
	CHECK( !RenderMaterialLua_ReadTable( L, -1, 9u, span, Resolve, &nextId,
		&ir, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_LUA_INVALID_PROXY
		&& !memcmp( &ir, &before, sizeof( ir ) ) );
	CHECK( Load( L,
		"local e=1 for i=1,34 do e={op='add',a=e,b=1} end "
		"return {name='materials/proxy/deep',proxy={fields={{name='x',"
		"components=1,expression=e}}}}" ) );
	CHECK( !RenderMaterialLua_ReadTable( L, -1, 9u, span, Resolve, &nextId,
		&ir, &diagnostic ) );
	CHECK( diagnostic.reason == RENDER_MATERIAL_LUA_INVALID_PROXY
		&& !memcmp( &ir, &before, sizeof( ir ) ) );
	lua_close( L );
	puts( "render_material_lua_test: ok" );
	return 0;
}
