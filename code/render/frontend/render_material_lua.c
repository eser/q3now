// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "render_material_lua.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>

typedef struct {
	const char *name;
	ralMaterialTextureRole_t role;
	ralMaterialChannelConvention_t channels;
} textureField_t;

static const textureField_t textureFields[] = {
	{ "base_color", RAL_MATERIAL_TEXTURE_BASE_COLOR,
		RAL_MATERIAL_CHANNEL_RGBA },
	{ "normal", RAL_MATERIAL_TEXTURE_NORMAL,
		RAL_MATERIAL_CHANNEL_XY_NORMAL },
	{ "orm", RAL_MATERIAL_TEXTURE_ORM, RAL_MATERIAL_CHANNEL_RGB_ORM },
	{ "occlusion", RAL_MATERIAL_TEXTURE_OCCLUSION,
		RAL_MATERIAL_CHANNEL_R_OCCLUSION },
	{ "metallic_roughness", RAL_MATERIAL_TEXTURE_METALLIC_ROUGHNESS,
		RAL_MATERIAL_CHANNEL_GB_ROUGHNESS_METALLIC },
	{ "emissive", RAL_MATERIAL_TEXTURE_EMISSIVE,
		RAL_MATERIAL_CHANNEL_RGB_EMISSIVE },
	{ "lightmap", RAL_MATERIAL_TEXTURE_LIGHTMAP, RAL_MATERIAL_CHANNEL_RGBA },
	{ "environment", RAL_MATERIAL_TEXTURE_ENVIRONMENT,
		RAL_MATERIAL_CHANNEL_RGBA }
};

#define MATERIAL_LUA_EXPRESSION_MAX_DEPTH 32u

typedef struct {
	ralMaterialProxyProgram_t *program;
	renderMaterialLuaDiagnostic_t *diagnostic;
	uint32_t nextRegister;
	uint32_t nextOutput;
	char tableNames[RAL_MATERIAL_PROXY_EXPRESSION_MAX_TABLES]
		[RAL_MATERIAL_PROXY_FIELD_NAME];
} authoredProxyBuilder_t;

static int AbsoluteIndex( lua_State *L, int index ) {
	return index > 0 ? index : lua_gettop( L ) + index + 1;
}

static void Fail( renderMaterialLuaDiagnostic_t *diagnostic,
		renderMaterialLuaFailure_t reason, const char *field,
		ralMaterialTextureRole_t role ) {
	if ( !diagnostic ) return;
	memset( diagnostic, 0, sizeof( *diagnostic ) );
	diagnostic->reason = reason;
	diagnostic->textureRole = role;
	if ( field ) (void)snprintf( diagnostic->field,
		sizeof( diagnostic->field ), "%s", field );
}

static qboolean OptionalFloat( lua_State *L, int table, const char *field,
		float *value ) {
	qboolean ok = qtrue;
	lua_getfield( L, table, field );
	if ( !lua_isnil( L, -1 ) ) {
		if ( !lua_isnumber( L, -1 ) ) ok = qfalse;
		else {
			*value = (float)lua_tonumber( L, -1 );
			if ( !isfinite( *value ) ) ok = qfalse;
		}
	}
	lua_pop( L, 1 );
	return ok;
}

static qboolean OptionalBool( lua_State *L, int table, const char *field,
		qboolean *value ) {
	qboolean ok = qtrue;
	lua_getfield( L, table, field );
	if ( !lua_isnil( L, -1 ) ) {
		if ( !lua_isboolean( L, -1 ) ) ok = qfalse;
		else *value = lua_toboolean( L, -1 ) ? qtrue : qfalse;
	}
	lua_pop( L, 1 );
	return ok;
}

static qboolean OptionalTuple( lua_State *L, int table, const char *field,
		float *values, uint32_t count ) {
	qboolean ok = qtrue;
	lua_getfield( L, table, field );
	if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); return qtrue; }
	if ( !lua_istable( L, -1 ) || lua_objlen( L, -1 ) != count ) ok = qfalse;
	for ( uint32_t i = 0u; ok && i < count; ++i ) {
		lua_rawgeti( L, -1, (int)i + 1 );
		if ( !lua_isnumber( L, -1 ) ) ok = qfalse;
		else {
			values[i] = (float)lua_tonumber( L, -1 );
			if ( !isfinite( values[i] ) ) ok = qfalse;
		}
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
	return ok;
}

static qboolean FloatQ16( float value, int32_t *out ) {
	double scaled;
	if ( !out || !isfinite( value ) ) return qfalse;
	scaled = (double)value * RAL_MATERIAL_PROXY_Q16_ONE;
	if ( scaled < INT32_MIN || scaled > INT32_MAX ) return qfalse;
	*out = (int32_t)( scaled < 0.0 ? scaled - 0.5 : scaled + 0.5 );
	return qtrue;
}

static qboolean ProxyConstant( authoredProxyBuilder_t *builder,
		const float lane[4], uint16_t *outRegister ) {
	ralMaterialExpressionValue_t value;
	uint32_t constantIndex, laneIndex;
	for ( laneIndex = 0u; laneIndex < 4u; ++laneIndex )
		if ( !FloatQ16( lane[laneIndex], &value.lane[laneIndex] ) ) return qfalse;
	for ( constantIndex = 0u;
			constantIndex < builder->program->expressionConstantCount;
			++constantIndex ) {
		if ( !memcmp( &builder->program->expressionConstants[constantIndex],
				&value, sizeof( value ) ) ) break;
	}
	if ( constantIndex == builder->program->expressionConstantCount ) {
		if ( constantIndex >= RAL_MATERIAL_PROXY_EXPRESSION_MAX_CONSTANTS )
			return qfalse;
		builder->program->expressionConstants[constantIndex] = value;
		builder->program->expressionConstantCount++;
	}
	if ( builder->program->expressionInstructionCount
			>= RAL_MATERIAL_PROXY_EXPRESSION_MAX_INSTRUCTIONS
			|| builder->nextRegister
				>= RAL_MATERIAL_PROXY_EXPRESSION_MAX_REGISTERS ) return qfalse;
	ralMaterialExpressionInstruction_t *instruction =
		&builder->program->expressionInstructions[
			builder->program->expressionInstructionCount++];
	instruction->opcode = RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE_CONSTANT;
	instruction->destination = (uint16_t)builder->nextRegister++;
	instruction->a = (uint16_t)constantIndex;
	instruction->b = 0u;
	*outRegister = instruction->destination;
	return qtrue;
}

static qboolean ProxyEmit( authoredProxyBuilder_t *builder,
		ralMaterialExpressionOpcode_t opcode, uint16_t a, uint16_t b,
		uint16_t *outRegister ) {
	ralMaterialExpressionInstruction_t *instruction;
	if ( builder->program->expressionInstructionCount
			>= RAL_MATERIAL_PROXY_EXPRESSION_MAX_INSTRUCTIONS
			|| builder->nextRegister
				>= RAL_MATERIAL_PROXY_EXPRESSION_MAX_REGISTERS ) return qfalse;
	instruction = &builder->program->expressionInstructions[
		builder->program->expressionInstructionCount++];
	instruction->opcode = (uint16_t)opcode;
	instruction->destination = (uint16_t)builder->nextRegister++;
	instruction->a = a;
	instruction->b = b;
	*outRegister = instruction->destination;
	return qtrue;
}

static qboolean ProxyInputRegister( const char *name, uint16_t *outRegister ) {
	static const char *const baseNames[RAL_MATERIAL_PROXY_BASE_VALUE_COUNT] = {
		"base_color_r", "base_color_g", "base_color_b", "base_color_a",
		"emissive_r", "emissive_g", "emissive_b", "metallic",
		"roughness", "normal_scale", "occlusion_strength", "alpha_cutoff"
	};
	if ( !name || !outRegister ) return qfalse;
	if ( !strcmp( name, "time" ) ) {
		*outRegister = RAL_MATERIAL_PROXY_REGISTER_TIME; return qtrue;
	}
	if ( !strcmp( name, "scalar" ) ) {
		*outRegister = RAL_MATERIAL_PROXY_REGISTER_SCALAR; return qtrue;
	}
	for ( uint32_t i = 0u; i < RAL_MATERIAL_PROXY_BASE_VALUE_COUNT; ++i ) {
		if ( !strcmp( name, baseNames[i] ) ) {
			*outRegister = (uint16_t)( RAL_MATERIAL_PROXY_REGISTER_BASE_VALUES + i );
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean ProxyTupleConstant( lua_State *L, int index,
		authoredProxyBuilder_t *builder, uint16_t *outRegister ) {
	float lane[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	uint32_t count;
	index = AbsoluteIndex( L, index );
	if ( lua_isnumber( L, index ) ) {
		lane[0] = lane[1] = lane[2] = lane[3] = (float)lua_tonumber( L, index );
		return ProxyConstant( builder, lane, outRegister );
	}
	if ( !lua_istable( L, index ) ) return qfalse;
	count = (uint32_t)lua_objlen( L, index );
	if ( !count || count > 4u ) return qfalse;
	for ( uint32_t i = 0u; i < count; ++i ) {
		lua_rawgeti( L, index, (int)i + 1 );
		if ( !lua_isnumber( L, -1 ) ) { lua_pop( L, 1 ); return qfalse; }
		lane[i] = (float)lua_tonumber( L, -1 );
		lua_pop( L, 1 );
		if ( !isfinite( lane[i] ) ) return qfalse;
	}
	if ( count == 1u ) lane[1] = lane[2] = lane[3] = lane[0];
	return ProxyConstant( builder, lane, outRegister );
}

static qboolean ProxyBinaryOpcode( const char *name,
		ralMaterialExpressionOpcode_t *outOpcode ) {
	static const struct { const char *name; ralMaterialExpressionOpcode_t opcode; }
		operations[] = {
			{ "add", RAL_MATERIAL_EXPRESSION_ADD },
			{ "subtract", RAL_MATERIAL_EXPRESSION_SUBTRACT },
			{ "multiply", RAL_MATERIAL_EXPRESSION_MULTIPLY },
			{ "dot3", RAL_MATERIAL_EXPRESSION_DOT3 },
			{ "dot4", RAL_MATERIAL_EXPRESSION_DOT4 },
			{ "divide", RAL_MATERIAL_EXPRESSION_DIVIDE },
			{ "mod", RAL_MATERIAL_EXPRESSION_MOD },
			{ "gt", RAL_MATERIAL_EXPRESSION_GT },
			{ "ge", RAL_MATERIAL_EXPRESSION_GE },
			{ "lt", RAL_MATERIAL_EXPRESSION_LT },
			{ "le", RAL_MATERIAL_EXPRESSION_LE },
			{ "eq", RAL_MATERIAL_EXPRESSION_EQ },
			{ "ne", RAL_MATERIAL_EXPRESSION_NE },
			{ "and", RAL_MATERIAL_EXPRESSION_AND },
			{ "or", RAL_MATERIAL_EXPRESSION_OR }
		};
	if ( !name || !outOpcode ) return qfalse;
	for ( uint32_t i = 0u; i < sizeof( operations ) / sizeof( operations[0] );
			++i ) if ( !strcmp( name, operations[i].name ) ) {
		*outOpcode = operations[i].opcode; return qtrue;
	}
	return qfalse;
}

static qboolean CompileProxyExpression( lua_State *L, int index,
		authoredProxyBuilder_t *builder, uint32_t depth,
		uint16_t *outRegister ) {
	const char *op;
	uint16_t a, b;
	index = AbsoluteIndex( L, index );
	if ( depth > MATERIAL_LUA_EXPRESSION_MAX_DEPTH ) return qfalse;
	if ( lua_isnumber( L, index ) )
		return ProxyTupleConstant( L, index, builder, outRegister );
	if ( lua_type( L, index ) == LUA_TSTRING )
		return ProxyInputRegister( lua_tostring( L, index ), outRegister );
	if ( !lua_istable( L, index ) ) return qfalse;
	lua_getfield( L, index, "op" );
	if ( lua_isnil( L, -1 ) ) {
		lua_pop( L, 1 );
		return ProxyTupleConstant( L, index, builder, outRegister );
	}
	op = lua_type( L, -1 ) == LUA_TSTRING ? lua_tostring( L, -1 ) : NULL;
	if ( !op ) { lua_pop( L, 1 ); return qfalse; }
	char operation[24];
	if ( strlen( op ) >= sizeof( operation ) ) { lua_pop( L, 1 ); return qfalse; }
	(void)snprintf( operation, sizeof( operation ), "%s", op );
	lua_pop( L, 1 );
	lua_getfield( L, index, "a" );
	if ( !CompileProxyExpression( L, -1, builder, depth + 1u, &a ) ) {
		lua_pop( L, 1 ); return qfalse;
	}
	lua_pop( L, 1 );
	if ( !strcmp( operation, "move" ) )
		return ProxyEmit( builder, RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE,
			a, 0u, outRegister );
	if ( !strcmp( operation, "mask" ) ) {
		uint16_t mask = 0u;
		lua_getfield( L, index, "mask" );
		const char *lanes = lua_type( L, -1 ) == LUA_TSTRING
			? lua_tostring( L, -1 ) : NULL;
		if ( lanes ) for ( const char *cursor = lanes; *cursor; ++cursor ) {
			if ( *cursor == 'x' ) mask |= 1u;
			else if ( *cursor == 'y' ) mask |= 2u;
			else if ( *cursor == 'z' ) mask |= 4u;
			else if ( *cursor == 'w' ) mask |= 8u;
			else { mask = 0u; break; }
		}
		lua_pop( L, 1 );
		return mask && ProxyEmit( builder, RAL_MATERIAL_EXPRESSION_MOVE,
			a, mask, outRegister );
	}
	if ( !strcmp( operation, "swizzle" ) ) {
		uint16_t encoding = 0u;
		const char *components = "xyzw";
		lua_getfield( L, index, "pattern" );
		const char *pattern = lua_type( L, -1 ) == LUA_TSTRING
			? lua_tostring( L, -1 ) : NULL;
		if ( !pattern || strlen( pattern ) != 4u ) {
			lua_pop( L, 1 ); return qfalse;
		}
		for ( uint32_t lane = 0u; lane < 4u; ++lane ) {
			const char *xyzw = strchr( components, pattern[lane] );
			if ( !xyzw ) { lua_pop( L, 1 ); return qfalse; }
			encoding |= (uint16_t)( ( xyzw - components ) << ( lane * 2u ) );
		}
		lua_pop( L, 1 );
		return ProxyEmit( builder, RAL_MATERIAL_EXPRESSION_SWIZZLE,
			a, encoding, outRegister );
	}
	if ( !strcmp( operation, "table" ) ) {
		uint16_t tableIndex = UINT16_MAX;
		lua_getfield( L, index, "table" );
		const char *tableName = lua_type( L, -1 ) == LUA_TSTRING
			? lua_tostring( L, -1 ) : NULL;
		if ( tableName ) for ( uint32_t i = 0u;
				i < builder->program->expressionTableCount; ++i )
			if ( !strcmp( tableName, builder->tableNames[i] ) ) {
				tableIndex = (uint16_t)i; break;
			}
		lua_pop( L, 1 );
		return tableIndex != UINT16_MAX
			&& ProxyEmit( builder, RAL_MATERIAL_EXPRESSION_TABLE,
				a, tableIndex, outRegister );
	}
	ralMaterialExpressionOpcode_t opcode;
	if ( !ProxyBinaryOpcode( operation, &opcode ) ) return qfalse;
	lua_getfield( L, index, "b" );
	if ( !CompileProxyExpression( L, -1, builder, depth + 1u, &b ) ) {
		lua_pop( L, 1 ); return qfalse;
	}
	lua_pop( L, 1 );
	return ProxyEmit( builder, opcode, a, b, outRegister );
}

static qboolean ReadFactors( lua_State *L, int root,
		ralMaterialSourceIr_t *source, renderMaterialLuaDiagnostic_t *diagnostic ) {
	lua_getfield( L, root, "factors" );
	if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); return qtrue; }
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 ); Fail( diagnostic,
			RENDER_MATERIAL_LUA_INVALID_FACTORS, "factors", 0 ); return qfalse;
	}
	const int factors = lua_gettop( L );
	if ( !OptionalTuple( L, factors, "base_color",
			source->pbrFactors.baseColor, 4u )
			|| !OptionalTuple( L, factors, "emissive",
				source->pbrFactors.emissive, 3u )
			|| !OptionalFloat( L, factors, "metallic",
				&source->pbrFactors.metallic )
			|| !OptionalFloat( L, factors, "roughness",
				&source->pbrFactors.roughness )
			|| !OptionalFloat( L, factors, "normal_scale",
				&source->pbrFactors.normalScale )
			|| !OptionalFloat( L, factors, "occlusion_strength",
				&source->pbrFactors.occlusionStrength ) ) {
		lua_pop( L, 1 ); Fail( diagnostic,
			RENDER_MATERIAL_LUA_INVALID_FACTORS, "factors", 0 ); return qfalse;
	}
	lua_pop( L, 1 );
	return qtrue;
}

static qboolean ReadRenderPolicy( lua_State *L, int root,
		ralMaterialSourceIr_t *source, renderMaterialLuaDiagnostic_t *diagnostic ) {
	lua_getfield( L, root, "render" );
	if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); return qtrue; }
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 ); Fail( diagnostic,
			RENDER_MATERIAL_LUA_INVALID_RENDER_POLICY, "render", 0 ); return qfalse;
	}
	const int render = lua_gettop( L );
	lua_getfield( L, render, "alpha_mode" );
	if ( !lua_isnil( L, -1 ) ) {
		const char *mode = lua_type( L, -1 ) == LUA_TSTRING
			? lua_tostring( L, -1 ) : NULL;
		if ( !mode ) source->pbrRenderPolicy.alphaMode = UINT32_MAX;
		else if ( !strcmp( mode, "opaque" ) )
			source->pbrRenderPolicy.alphaMode = RAL_MATERIAL_ALPHA_OPAQUE;
		else if ( !strcmp( mode, "mask" ) )
			source->pbrRenderPolicy.alphaMode = RAL_MATERIAL_ALPHA_MASK;
		else if ( !strcmp( mode, "blend" ) )
			source->pbrRenderPolicy.alphaMode = RAL_MATERIAL_ALPHA_BLEND;
		else if ( !strcmp( mode, "additive" ) )
			source->pbrRenderPolicy.alphaMode = RAL_MATERIAL_ALPHA_ADDITIVE;
		else source->pbrRenderPolicy.alphaMode = UINT32_MAX;
	}
	lua_pop( L, 1 );
	if ( source->pbrRenderPolicy.alphaMode > RAL_MATERIAL_ALPHA_ADDITIVE
			|| !OptionalFloat( L, render, "alpha_cutoff",
				&source->pbrRenderPolicy.alphaCutoff )
			|| !OptionalBool( L, render, "double_sided",
				&source->pbrRenderPolicy.doubleSided )
			|| !OptionalBool( L, render, "depth_test",
				&source->pbrRenderPolicy.depthTest )
			|| !OptionalBool( L, render, "depth_write",
				&source->pbrRenderPolicy.depthWrite ) ) {
		lua_pop( L, 1 ); Fail( diagnostic,
			RENDER_MATERIAL_LUA_INVALID_RENDER_POLICY, "render", 0 ); return qfalse;
	}
	lua_pop( L, 1 );
	return qtrue;
}

static qboolean ReadTextures( lua_State *L, int root,
		renderMaterialLuaResolveDependencyFn resolver, void *resolveContext,
		ralMaterialSourceIr_t *source, renderMaterialLuaDiagnostic_t *diagnostic ) {
	lua_getfield( L, root, "textures" );
	if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); return qtrue; }
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 ); Fail( diagnostic,
			RENDER_MATERIAL_LUA_INVALID_TEXTURE, "textures", 0 ); return qfalse;
	}
	const int textures = lua_gettop( L );
	for ( uint32_t i = 0u; i < sizeof( textureFields ) / sizeof( textureFields[0] );
			++i ) {
		const textureField_t *field = &textureFields[i];
		lua_getfield( L, textures, field->name );
		if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); continue; }
		if ( !lua_istable( L, -1 ) ) {
			lua_pop( L, 2 ); Fail( diagnostic,
				RENDER_MATERIAL_LUA_INVALID_TEXTURE, field->name, field->role );
			return qfalse;
		}
		const int entry = lua_gettop( L );
		const char *path;
		uint64_t options = 0u;
		uint32_t uvSet = 0u;
		lua_getfield( L, entry, "path" );
		path = lua_type( L, -1 ) == LUA_TSTRING
			? lua_tostring( L, -1 ) : NULL;
		if ( !path || !path[0] ) {
			lua_pop( L, 3 ); Fail( diagnostic,
				RENDER_MATERIAL_LUA_INVALID_TEXTURE, field->name, field->role );
			return qfalse;
		}
		char requestedPath[MAX_QPATH];
		if ( strlen( path ) >= sizeof( requestedPath ) ) {
			lua_pop( L, 3 ); Fail( diagnostic,
				RENDER_MATERIAL_LUA_INVALID_TEXTURE, field->name, field->role );
			return qfalse;
		}
		(void)snprintf( requestedPath, sizeof( requestedPath ), "%s", path );
		lua_pop( L, 1 );
		lua_getfield( L, entry, "uv" );
		if ( !lua_isnil( L, -1 ) ) {
			const lua_Number value = lua_isnumber( L, -1 )
				? lua_tonumber( L, -1 ) : -1.0;
			if ( value != 0.0 && value != 1.0 ) uvSet = UINT32_MAX;
			else uvSet = (uint32_t)value;
		}
		lua_pop( L, 1 );
		lua_getfield( L, entry, "creation_options" );
		if ( !lua_isnil( L, -1 ) ) {
			const lua_Number value = lua_isnumber( L, -1 )
				? lua_tonumber( L, -1 ) : -1.0;
			if ( value < 0.0 || value > 9007199254740991.0
					|| floor( value ) != value ) uvSet = UINT32_MAX;
			else options = (uint64_t)value;
		}
		lua_pop( L, 1 );
		if ( uvSet > 1u ) {
			lua_pop( L, 2 ); Fail( diagnostic,
				RENDER_MATERIAL_LUA_INVALID_TEXTURE, field->name, field->role );
			return qfalse;
		}
		ralMaterialSourceDependency_t resolved;
		uint32_t dependencyIndex;
		memset( &resolved, 0, sizeof( resolved ) );
		if ( !resolver( resolveContext, requestedPath, options,
				source->declaration, &resolved )
				|| resolved.role != RAL_MATERIAL_DEPENDENCY_TEXTURE ) {
			lua_pop( L, 2 ); Fail( diagnostic,
				RENDER_MATERIAL_LUA_DEPENDENCY_REJECTED, field->name, field->role );
			return qfalse;
		}
		/* TASK-237 identity is canonical source + creation semantics, not the
		 * spelling used by the declaration.  Reusing the existing dependency
		 * keeps aliases from becoming duplicate source-IR nodes while still
		 * allowing distinct creation options to produce distinct resources. */
		for ( dependencyIndex = 0u;
				dependencyIndex < source->dependencyCount; ++dependencyIndex ) {
			const ralMaterialSourceDependency_t *existing =
				&source->dependencies[dependencyIndex];
			if ( existing->role == resolved.role
					&& existing->sourceId == resolved.sourceId
					&& existing->creationOptionsHash
						== resolved.creationOptionsHash ) break;
		}
		if ( dependencyIndex == source->dependencyCount ) {
			if ( source->dependencyCount >= RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES ) {
				lua_pop( L, 2 ); Fail( diagnostic,
					RENDER_MATERIAL_LUA_DEPENDENCY_REJECTED,
					field->name, field->role );
				return qfalse;
			}
			source->dependencies[source->dependencyCount++] = resolved;
		}
		ralMaterialSourcePbrTexture_t *texture =
			&source->pbrTextures[source->pbrTextureCount++];
		texture->span = source->declaration;
		texture->dependencyIndex = dependencyIndex;
		texture->role = field->role;
		texture->channels = field->channels;
		texture->uvSet = uvSet;
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
	return qtrue;
}

static qboolean ReadProxyTables( lua_State *L, int proxy,
		authoredProxyBuilder_t *builder ) {
	lua_getfield( L, proxy, "tables" );
	if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); return qtrue; }
	if ( !lua_istable( L, -1 )
			|| lua_objlen( L, -1 ) > RAL_MATERIAL_PROXY_EXPRESSION_MAX_TABLES ) {
		lua_pop( L, 1 ); return qfalse;
	}
	const int tables = lua_gettop( L );
	const uint32_t tableCount = (uint32_t)lua_objlen( L, tables );
	for ( uint32_t i = 0u; i < tableCount; ++i ) {
		ralMaterialExpressionTable_t *table =
			&builder->program->expressionTables[i];
		qboolean wrap = qfalse, snap = qfalse;
		lua_rawgeti( L, tables, (int)i + 1 );
		if ( !lua_istable( L, -1 ) ) { lua_pop( L, 2 ); return qfalse; }
		const int entry = lua_gettop( L );
		lua_getfield( L, entry, "name" );
		const char *name = lua_type( L, -1 ) == LUA_TSTRING
			? lua_tostring( L, -1 ) : NULL;
		if ( !name || !name[0]
				|| strlen( name ) >= RAL_MATERIAL_PROXY_FIELD_NAME ) {
			lua_pop( L, 3 ); return qfalse;
		}
		for ( uint32_t previous = 0u; previous < i; ++previous )
			if ( !strcmp( name, builder->tableNames[previous] ) ) {
				lua_pop( L, 3 ); return qfalse;
			}
		(void)snprintf( builder->tableNames[i],
			sizeof( builder->tableNames[i] ), "%s", name );
		lua_pop( L, 1 );
		lua_getfield( L, entry, "samples" );
		if ( !lua_istable( L, -1 ) || lua_objlen( L, -1 ) < 2u
				|| lua_objlen( L, -1 )
					> RAL_MATERIAL_PROXY_EXPRESSION_TABLE_SAMPLES ) {
			lua_pop( L, 3 ); return qfalse;
		}
		table->sampleCount = (uint16_t)lua_objlen( L, -1 );
		for ( uint32_t sample = 0u; sample < table->sampleCount; ++sample ) {
			lua_rawgeti( L, -1, (int)sample + 1 );
			if ( !lua_isnumber( L, -1 ) || !FloatQ16(
					(float)lua_tonumber( L, -1 ), &table->samplesQ16[sample] ) ) {
				lua_pop( L, 4 ); return qfalse;
			}
			lua_pop( L, 1 );
		}
		lua_pop( L, 1 );
		if ( !OptionalBool( L, entry, "wrap", &wrap )
				|| !OptionalBool( L, entry, "snap", &snap ) ) {
			lua_pop( L, 2 ); return qfalse;
		}
		table->flags = (uint16_t)( ( wrap
			? RAL_MATERIAL_EXPRESSION_TABLE_WRAP : 0u ) | ( snap
			? RAL_MATERIAL_EXPRESSION_TABLE_SNAP : 0u ) );
		builder->program->expressionTableCount++;
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
	return qtrue;
}

static qboolean ReadProxyFields( lua_State *L, int proxy,
		authoredProxyBuilder_t *builder ) {
	lua_getfield( L, proxy, "fields" );
	if ( !lua_istable( L, -1 ) || !lua_objlen( L, -1 )
			|| lua_objlen( L, -1 ) > RAL_MATERIAL_PROXY_EXPRESSION_MAX_FIELDS ) {
		lua_pop( L, 1 ); return qfalse;
	}
	const int fields = lua_gettop( L );
	const uint32_t fieldCount = (uint32_t)lua_objlen( L, fields );
	for ( uint32_t i = 0u; i < fieldCount; ++i ) {
		uint16_t valueRegister;
		uint32_t componentCount;
		lua_rawgeti( L, fields, (int)i + 1 );
		if ( !lua_istable( L, -1 ) ) { lua_pop( L, 2 ); return qfalse; }
		const int entry = lua_gettop( L );
		lua_getfield( L, entry, "name" );
		const char *name = lua_type( L, -1 ) == LUA_TSTRING
			? lua_tostring( L, -1 ) : NULL;
		if ( !name || !name[0]
				|| strlen( name ) >= RAL_MATERIAL_PROXY_FIELD_NAME ) {
			lua_pop( L, 3 ); return qfalse;
		}
		for ( uint32_t previous = 0u; previous < i; ++previous )
			if ( !strcmp( name,
					builder->program->expressionFields[previous].name ) ) {
				lua_pop( L, 3 ); return qfalse;
			}
		char fieldName[RAL_MATERIAL_PROXY_FIELD_NAME];
		(void)snprintf( fieldName, sizeof( fieldName ), "%s", name );
		lua_pop( L, 1 );
		lua_getfield( L, entry, "components" );
		const lua_Number components = lua_isnumber( L, -1 )
			? lua_tonumber( L, -1 ) : 0.0;
		lua_pop( L, 1 );
		if ( components < 1.0 || components > 4.0
				|| floor( components ) != components ) {
			lua_pop( L, 2 ); return qfalse;
		}
		componentCount = (uint32_t)components;
		lua_getfield( L, entry, "expression" );
		if ( !CompileProxyExpression( L, -1, builder, 0u, &valueRegister ) ) {
			lua_pop( L, 3 ); return qfalse;
		}
		lua_pop( L, 1 );
		if ( valueRegister < RAL_MATERIAL_PROXY_REGISTER_BASE_VALUES
				+ RAL_MATERIAL_PROXY_BASE_VALUE_COUNT
				&& !ProxyEmit( builder, RAL_MATERIAL_EXPRESSION_UNMASKED_MOVE,
					valueRegister, 0u, &valueRegister ) ) {
			lua_pop( L, 2 ); return qfalse;
		}
		if ( builder->nextOutput + componentCount
				> RAL_MATERIAL_PROXY_VALUE_COUNT ) {
			lua_pop( L, 2 ); return qfalse;
		}
		ralMaterialExpressionField_t *field =
			&builder->program->expressionFields[
				builder->program->expressionFieldCount++];
		(void)snprintf( field->name, sizeof( field->name ), "%s", fieldName );
		field->registerIndex = valueRegister;
		field->inlineByteOffset = (uint16_t)( builder->nextOutput * 4u );
		field->componentCount = (uint8_t)componentCount;
		field->outputValueOffset = (uint8_t)builder->nextOutput;
		builder->nextOutput += componentCount;
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
	return qtrue;
}

static qboolean ReadProxy( lua_State *L, int root,
		ralMaterialSourceIr_t *source, renderMaterialLuaDiagnostic_t *diagnostic ) {
	authoredProxyBuilder_t builder;
	lua_getfield( L, root, "proxy" );
	if ( lua_isnil( L, -1 ) ) { lua_pop( L, 1 ); return qtrue; }
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		Fail( diagnostic, RENDER_MATERIAL_LUA_INVALID_PROXY, "proxy", 0 );
		return qfalse;
	}
	const int proxy = lua_gettop( L );
	memset( &builder, 0, sizeof( builder ) );
	builder.program = &source->authoredProxy;
	builder.diagnostic = diagnostic;
	builder.nextRegister = RAL_MATERIAL_PROXY_REGISTER_BASE_VALUES
		+ RAL_MATERIAL_PROXY_BASE_VALUE_COUNT;
	builder.nextOutput = RAL_MATERIAL_PROXY_BASE_VALUE_COUNT;
	builder.program->schemaVersion = RAL_MATERIAL_PROXY_SCHEMA_VERSION;
	if ( !ReadProxyTables( L, proxy, &builder )
			|| !ReadProxyFields( L, proxy, &builder ) ) {
		lua_pop( L, 1 );
		Fail( diagnostic, RENDER_MATERIAL_LUA_INVALID_PROXY, "proxy", 0 );
		return qfalse;
	}
	builder.program->expressionRegisterCount = builder.nextRegister;
	source->hasAuthoredProxy = qtrue;
	lua_pop( L, 1 );
	return qtrue;
}

qboolean RenderMaterialLua_ReadTable( lua_State *L, int tableIndex,
		uint64_t materialGeneration, ralMaterialSourceSpan_t declaration,
		renderMaterialLuaResolveDependencyFn resolveDependency,
		void *resolveContext, ralMaterialSourceIr_t *outSource,
		renderMaterialLuaDiagnostic_t *outDiagnostic ) {
	ralMaterialSourceIr_t candidate;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !L || !outSource || !materialGeneration || !declaration.sourceId
			|| !resolveDependency ) {
		Fail( outDiagnostic, RENDER_MATERIAL_LUA_INVALID_ARGUMENT, NULL, 0 );
		return qfalse;
	}
	tableIndex = AbsoluteIndex( L, tableIndex );
	if ( !lua_istable( L, tableIndex ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_LUA_EXPECTED_TABLE, NULL, 0 );
		return qfalse;
	}
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	candidate.generation = materialGeneration;
	candidate.provenance = RAL_MATERIAL_PROVENANCE_WIRED_LUA;
	candidate.declaration = declaration;
	candidate.diffuseReflectance[0] = candidate.diffuseReflectance[1] =
		candidate.diffuseReflectance[2] = 1.0f;
	candidate.pbrFactors.baseColor[0] = candidate.pbrFactors.baseColor[1] =
		candidate.pbrFactors.baseColor[2] = candidate.pbrFactors.baseColor[3] = 1.0f;
	candidate.pbrFactors.roughness = 1.0f;
	candidate.pbrFactors.normalScale = 1.0f;
	candidate.pbrFactors.occlusionStrength = 1.0f;
	candidate.pbrRenderPolicy.depthTest = qtrue;
	candidate.pbrRenderPolicy.depthWrite = qtrue;
	lua_getfield( L, tableIndex, "name" );
	const char *name = lua_type( L, -1 ) == LUA_TSTRING
		? lua_tostring( L, -1 ) : NULL;
	if ( !name || !name[0] || strlen( name ) >= sizeof( candidate.semanticName ) ) {
		lua_pop( L, 1 ); Fail( outDiagnostic,
			RENDER_MATERIAL_LUA_INVALID_NAME, "name", 0 ); return qfalse;
	}
	(void)snprintf( candidate.semanticName, sizeof( candidate.semanticName ),
		"%s", name );
	lua_pop( L, 1 );
	if ( !ReadFactors( L, tableIndex, &candidate, outDiagnostic )
			|| !ReadRenderPolicy( L, tableIndex, &candidate, outDiagnostic )
			|| !ReadTextures( L, tableIndex, resolveDependency, resolveContext,
				&candidate, outDiagnostic )
			|| !ReadProxy( L, tableIndex, &candidate, outDiagnostic ) ) return qfalse;
	*outSource = candidate;
	return qtrue;
}

qboolean RenderMaterialLua_Load( lua_State *L, const char *canonicalPath,
		const renderMaterialLuaSourceOps_t *sourceOps, void *sourceContext,
		renderMaterialLuaResolveDependencyFn resolveDependency,
		void *resolveContext, ralMaterialSourceIr_t *outSource,
		renderMaterialLuaDiagnostic_t *outDiagnostic ) {
	const void *bytes = NULL;
	uint64_t byteLength = 0u, sourceId = 0u, sourceGeneration = 0u;
	int base, status;
	qboolean ok;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !L || !canonicalPath || !canonicalPath[0] || !sourceOps
			|| !sourceOps->read || !sourceOps->release || !resolveDependency
			|| !outSource ) {
		Fail( outDiagnostic, RENDER_MATERIAL_LUA_INVALID_ARGUMENT, NULL, 0 );
		return qfalse;
	}
	base = lua_gettop( L );
	if ( !sourceOps->read( sourceContext, canonicalPath, &bytes, &byteLength,
			&sourceId, &sourceGeneration ) || !bytes || !byteLength || !sourceId
			|| !sourceGeneration || byteLength > SIZE_MAX ) {
		if ( bytes ) sourceOps->release( sourceContext, bytes );
		Fail( outDiagnostic, RENDER_MATERIAL_LUA_SOURCE_READ_FAILED,
			canonicalPath, 0 ); return qfalse;
	}
	status = luaL_loadbuffer( L, bytes, (size_t)byteLength, canonicalPath );
	sourceOps->release( sourceContext, bytes );
	if ( status ) {
		lua_settop( L, base );
		Fail( outDiagnostic, RENDER_MATERIAL_LUA_SOURCE_LOAD_FAILED,
			canonicalPath, 0 ); return qfalse;
	}
	status = lua_pcall( L, 0, 1, 0 );
	if ( status ) {
		lua_settop( L, base );
		Fail( outDiagnostic, RENDER_MATERIAL_LUA_SOURCE_EXEC_FAILED,
			canonicalPath, 0 ); return qfalse;
	}
	const ralMaterialSourceSpan_t declaration = { sourceId, 1u, 1u };
	ok = RenderMaterialLua_ReadTable( L, -1, sourceGeneration, declaration,
		resolveDependency, resolveContext, outSource, outDiagnostic );
	lua_settop( L, base );
	return ok;
}

qboolean RenderMaterialLua_LoadSourceReceipt( lua_State *L,
		const char *canonicalPath, const renderMaterialLuaSourceOps_t *sourceOps,
		void *sourceContext,
		renderMaterialLuaResolveDependencyFn resolveDependency,
		void *resolveContext, ralMaterialSourceReceipt_t *outReceipt,
		renderMaterialLuaDiagnostic_t *outDiagnostic ) {
	ralMaterialSourceIr_t source;
	ralMaterialSourceReceipt_t candidate;
	ralMaterialSourceDiagnostic_t sourceDiagnostic;
	if ( !outReceipt ) {
		Fail( outDiagnostic, RENDER_MATERIAL_LUA_INVALID_ARGUMENT, NULL, 0 );
		return qfalse;
	}
	if ( !RenderMaterialLua_Load( L, canonicalPath, sourceOps, sourceContext,
			resolveDependency, resolveContext, &source, outDiagnostic ) )
		return qfalse;
	memset( &sourceDiagnostic, 0, sizeof( sourceDiagnostic ) );
	if ( !Ral_MaterialSourceCompile( &source, &candidate, &sourceDiagnostic ) ) {
		Fail( outDiagnostic, RENDER_MATERIAL_LUA_SOURCE_COMPILE_FAILED,
			canonicalPath, 0 );
		if ( outDiagnostic ) outDiagnostic->source = sourceDiagnostic;
		return qfalse;
	}
	*outReceipt = candidate;
	return qtrue;
}
