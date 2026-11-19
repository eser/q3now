// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_material_script.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
	uint32_t line;
	uint32_t column;
	char token[MAX_QPATH];
} materialParseDiagnostic_t;

typedef struct {
	const char *cursor;
	const char *end;
	uint32_t line;
	uint32_t column;
	uint32_t tokenLine;
	uint32_t tokenColumn;
	qboolean hasPushback;
	char pushbackToken[MAX_QPATH];
	uint32_t pushbackLine;
	uint32_t pushbackColumn;
	materialParseDiagnostic_t *diagnostic;
} materialTokenizer_t;

static void RememberToken( materialTokenizer_t *parser, const char *token ) {
	if ( !parser || !parser->diagnostic || !token ) return;
	parser->diagnostic->line = parser->tokenLine;
	parser->diagnostic->column = parser->tokenColumn;
	(void)snprintf( parser->diagnostic->token,
		sizeof( parser->diagnostic->token ), "%s", token );
}

static void AdvanceTokenizer( materialTokenizer_t *parser ) {
	if ( *parser->cursor == '\n' ) {
		parser->line++;
		parser->column = 1u;
	} else {
		parser->column++;
	}
	parser->cursor++;
}

static qboolean CopyToken( char *target, size_t size,
		const char *start, size_t length ) {
	if ( !target || !size || length >= size ) return qfalse;
	memcpy( target, start, length ); target[length] = '\0';
	return qtrue;
}

static qboolean NextToken( materialTokenizer_t *parser,
		char token[MAX_QPATH] ) {
	const char *start;
	if ( !parser || !token ) return qfalse;
	if ( parser->hasPushback ) {
		(void)snprintf( token, MAX_QPATH, "%s", parser->pushbackToken );
		parser->tokenLine = parser->pushbackLine;
		parser->tokenColumn = parser->pushbackColumn;
		parser->hasPushback = qfalse;
		RememberToken( parser, token );
		return qtrue;
	}
	for ( ;; ) {
		while ( parser->cursor < parser->end
				&& (unsigned char)*parser->cursor <= ' ' )
			AdvanceTokenizer( parser );
		if ( parser->cursor + 1 < parser->end
				&& parser->cursor[0] == '/' && parser->cursor[1] == '/' ) {
			AdvanceTokenizer( parser );
			AdvanceTokenizer( parser );
			while ( parser->cursor < parser->end
					&& *parser->cursor != '\n' ) AdvanceTokenizer( parser );
			continue;
		}
		if ( parser->cursor + 1 < parser->end
				&& parser->cursor[0] == '/' && parser->cursor[1] == '*' ) {
			AdvanceTokenizer( parser );
			AdvanceTokenizer( parser );
			while ( parser->cursor + 1 < parser->end
					&& !( parser->cursor[0] == '*' && parser->cursor[1] == '/' ) )
				AdvanceTokenizer( parser );
			if ( parser->cursor + 1 >= parser->end ) return qfalse;
			AdvanceTokenizer( parser );
			AdvanceTokenizer( parser );
			continue;
		}
		break;
	}
	if ( parser->cursor >= parser->end ) return qfalse;
	parser->tokenLine = parser->line;
	parser->tokenColumn = parser->column;
	if ( *parser->cursor == '{' || *parser->cursor == '}'
			|| *parser->cursor == '(' || *parser->cursor == ')' ) {
		token[0] = *parser->cursor;
		AdvanceTokenizer( parser );
		token[1] = '\0'; RememberToken( parser, token ); return qtrue;
	}
	if ( *parser->cursor == '"' ) {
		AdvanceTokenizer( parser );
		start = parser->cursor;
		while ( parser->cursor < parser->end && *parser->cursor != '"' )
			AdvanceTokenizer( parser );
		if ( parser->cursor >= parser->end
				|| !CopyToken( token, MAX_QPATH, start,
					(size_t)( parser->cursor - start ) ) ) return qfalse;
		AdvanceTokenizer( parser ); RememberToken( parser, token ); return qtrue;
	}
	start = parser->cursor;
	while ( parser->cursor < parser->end
			&& (unsigned char)*parser->cursor > ' '
			&& *parser->cursor != '{' && *parser->cursor != '}'
			&& *parser->cursor != '(' && *parser->cursor != ')' )
		AdvanceTokenizer( parser );
	if ( !CopyToken( token, MAX_QPATH, start,
			(size_t)( parser->cursor - start ) ) ) return qfalse;
	RememberToken( parser, token );
	return qtrue;
}

static qboolean PushToken( materialTokenizer_t *parser, const char *token ) {
	if ( !parser || !token || parser->hasPushback
			|| strlen( token ) >= sizeof( parser->pushbackToken ) ) return qfalse;
	(void)snprintf( parser->pushbackToken, sizeof( parser->pushbackToken ),
		"%s", token );
	parser->pushbackLine = parser->tokenLine;
	parser->pushbackColumn = parser->tokenColumn;
	parser->hasPushback = qtrue;
	return qtrue;
}

static qboolean UsableImage( const char *name ) {
	return name && name[0] && name[0] != '$' && strcmp( name, "-" )
		? qtrue : qfalse;
}

static qboolean ParseFloatToken( const char *text, float *out ) {
	char *end = NULL;
	float value;
	if ( !text || !out || !text[0] ) return qfalse;
	errno = 0; value = strtof( text, &end );
	if ( errno || !end || *end || !isfinite( value ) ) return qfalse;
	*out = value; return qtrue;
}

static qboolean ParseIntToken( const char *text, int *out ) {
	char *end = NULL;
	long value;
	if ( !text || !out || !text[0] ) return qfalse;
	errno = 0; value = strtol( text, &end, 10 );
	if ( errno || !end || *end || value < INT_MIN || value > INT_MAX ) return qfalse;
	*out = (int)value; return qtrue;
}

typedef struct {
	const char *name;
	qboolean clearSolid;
	uint32_t surfaceFlags;
	uint32_t contentFlags;
} materialSurfaceParm_t;

static void ApplySurfaceParm( renderMaterialScriptEntry_t *entry,
		const char *token ) {
	static const materialSurfaceParm_t parms[] = {
		{ "water", qtrue, 0u, CONTENTS_WATER },
		{ "slime", qtrue, 0u, CONTENTS_SLIME },
		{ "lava", qtrue, 0u, CONTENTS_LAVA },
		{ "playerclip", qtrue, 0u, CONTENTS_PLAYERCLIP },
		{ "monsterclip", qtrue, 0u, CONTENTS_MONSTERCLIP },
		{ "nodrop", qtrue, 0u, CONTENTS_NODROP },
		{ "nonsolid", qtrue, SURF_NONSOLID, 0u },
		{ "origin", qtrue, 0u, CONTENTS_ORIGIN },
		{ "trans", qfalse, 0u, CONTENTS_TRANSLUCENT },
		{ "detail", qfalse, 0u, CONTENTS_DETAIL },
		{ "structural", qfalse, 0u, CONTENTS_STRUCTURAL },
		{ "areaportal", qtrue, 0u, CONTENTS_AREAPORTAL },
		{ "clusterportal", qtrue, 0u, CONTENTS_CLUSTERPORTAL },
		{ "donotenter", qtrue, 0u, CONTENTS_DONOTENTER },
		{ "fog", qtrue, 0u, CONTENTS_FOG },
		{ "sky", qfalse, SURF_SKY, 0u },
		{ "lightfilter", qfalse, SURF_LIGHTFILTER, 0u },
		{ "alphashadow", qfalse, SURF_ALPHASHADOW, 0u },
		{ "hint", qfalse, SURF_HINT, 0u },
		{ "slick", qfalse, SURF_SLICK, 0u },
		{ "noimpact", qfalse, SURF_NOIMPACT, 0u },
		{ "nomarks", qfalse, SURF_NOMARKS, 0u },
		{ "ladder", qfalse, SURF_LADDER, 0u },
		{ "nodamage", qfalse, SURF_NODAMAGE, 0u },
		{ "metalsteps", qfalse, SURF_METALSTEPS, 0u },
		{ "flesh", qfalse, SURF_FLESH, 0u },
		{ "nosteps", qfalse, SURF_NOSTEPS, 0u },
		{ "nodraw", qfalse, SURF_NODRAW, 0u },
		{ "pointlight", qfalse, SURF_POINTLIGHT, 0u },
		{ "nolightmap", qfalse, SURF_NOLIGHTMAP, 0u },
		{ "nodlight", qfalse, SURF_NODLIGHT, 0u },
		{ "dust", qfalse, SURF_DUST, 0u }
	};
	if ( !entry || !token ) return;
	for ( uint32_t i = 0u; i < ARRAY_LEN( parms ); ++i ) {
		if ( strcasecmp( token, parms[i].name ) ) continue;
		entry->surfaceFlags |= parms[i].surfaceFlags;
		entry->contentFlags |= parms[i].contentFlags;
		entry->clearSolid = entry->clearSolid || parms[i].clearSolid;
		return;
	}
}

static ralMaterialWaveFunction_t WaveFunction( const char *token ) {
	if ( !token ) return RAL_MATERIAL_WAVE_NONE;
	if ( !strcasecmp( token, "sin" ) ) return RAL_MATERIAL_WAVE_SIN;
	if ( !strcasecmp( token, "square" ) ) return RAL_MATERIAL_WAVE_SQUARE;
	if ( !strcasecmp( token, "triangle" ) ) return RAL_MATERIAL_WAVE_TRIANGLE;
	if ( !strcasecmp( token, "sawtooth" ) ) return RAL_MATERIAL_WAVE_SAWTOOTH;
	if ( !strcasecmp( token, "inversesawtooth" ) )
		return RAL_MATERIAL_WAVE_INVERSE_SAWTOOTH;
	if ( !strcasecmp( token, "noise" ) ) return RAL_MATERIAL_WAVE_NOISE;
	return RAL_MATERIAL_WAVE_NONE;
}

static qboolean ParseWave( materialTokenizer_t *parser,
		ralMaterialSourceWave_t *wave ) {
	char token[MAX_QPATH];
	if ( !parser || !wave || !NextToken( parser, token ) ) return qfalse;
	wave->span.line = parser->tokenLine;
	wave->span.column = parser->tokenColumn;
	wave->function = WaveFunction( token );
	if ( wave->function == RAL_MATERIAL_WAVE_NONE ) return qfalse;
	return NextToken( parser, token ) && ParseFloatToken( token, &wave->base )
		&& NextToken( parser, token ) && ParseFloatToken( token, &wave->amplitude )
		&& NextToken( parser, token ) && ParseFloatToken( token, &wave->phase )
		&& NextToken( parser, token ) && ParseFloatToken( token, &wave->frequency );
}

static qboolean ParseVector( materialTokenizer_t *parser, float *target,
		uint32_t componentCount ) {
	char token[MAX_QPATH];
	qboolean parenthesized = qfalse;
	if ( !parser || !target || !componentCount || !NextToken( parser, token ) )
		return qfalse;
	if ( !strcmp( token, "(" ) ) {
		parenthesized = qtrue;
		if ( !NextToken( parser, token ) ) return qfalse;
	}
	for ( uint32_t i = 0u; i < componentCount; ++i ) {
		if ( i && !NextToken( parser, token ) ) return qfalse;
		if ( !ParseFloatToken( token, &target[i] ) ) return qfalse;
	}
	if ( parenthesized ) {
		if ( !NextToken( parser, token ) || strcmp( token, ")" ) ) return qfalse;
	}
	return qtrue;
}

static qboolean AppendSourceTcMod( renderMaterialScriptStage_t *stage,
		ralMaterialTcModType_t type, const float *parameters,
		uint32_t parameterCount, uint32_t line, uint32_t column ) {
	ralMaterialSourceTcMod_t *target;
	if ( !stage || !parameters
			|| stage->sourceTcModCount >= RAL_MATERIAL_SOURCE_MAX_TCMODS
			|| parameterCount > ARRAY_LEN( stage->sourceTcMods[0].parameters ) )
		return qfalse;
	target = &stage->sourceTcMods[stage->sourceTcModCount++];
	target->type = type;
	target->span.line = line;
	target->span.column = column;
	memcpy( target->parameters, parameters,
		parameterCount * sizeof( parameters[0] ) );
	return qtrue;
}

static qboolean ParseDeform( materialTokenizer_t *parser,
		renderMaterialScriptEntry_t *entry ) {
	ralMaterialSourceDeform_t *deform;
	ralMaterialSourceWave_t wave;
	char token[MAX_QPATH];
	uint32_t line, column;
	if ( !parser || !entry
			|| entry->deformCount >= RAL_MATERIAL_SOURCE_MAX_DEFORMS
			|| !NextToken( parser, token ) ) return qfalse;
	deform = &entry->deforms[entry->deformCount];
	line = parser->tokenLine;
	column = parser->tokenColumn;
	deform->span.line = line;
	deform->span.column = column;
	if ( !strcasecmp( token, "autosprite" ) )
		deform->type = RAL_MATERIAL_DEFORM_AUTOSPRITE;
	else if ( !strcasecmp( token, "autosprite2" ) )
		deform->type = RAL_MATERIAL_DEFORM_AUTOSPRITE2;
	else if ( !strcasecmp( token, "projectionshadow" ) )
		deform->type = RAL_MATERIAL_DEFORM_PROJECTION_SHADOW;
	else if ( !strncasecmp( token, "text", 4u ) ) {
		int index = token[4] >= '0' && token[4] <= '7' ? token[4] - '0' : 0;
		deform->type = (ralMaterialDeformType_t)( RAL_MATERIAL_DEFORM_TEXT0 + index );
	} else if ( !strcasecmp( token, "bulge" ) ) {
		deform->type = RAL_MATERIAL_DEFORM_BULGE;
		for ( uint32_t i = 0u; i < 3u; ++i )
			if ( !NextToken( parser, token )
					|| !ParseFloatToken( token, &deform->parameters[i] ) )
				return qfalse;
	} else if ( !strcasecmp( token, "normal" ) ) {
		deform->type = RAL_MATERIAL_DEFORM_NORMAL;
		for ( uint32_t i = 0u; i < 2u; ++i )
			if ( !NextToken( parser, token )
					|| !ParseFloatToken( token, &deform->parameters[i] ) )
				return qfalse;
	} else if ( !strcasecmp( token, "wave" ) ) {
		deform->type = RAL_MATERIAL_DEFORM_WAVE;
		if ( !NextToken( parser, token )
				|| !ParseFloatToken( token, &deform->parameters[0] )
				|| !ParseWave( parser, &wave ) ) return qfalse;
		deform->parameters[1] = (float)wave.function;
		deform->parameters[2] = wave.base;
		deform->parameters[3] = wave.amplitude;
		deform->parameters[4] = wave.phase;
		deform->parameters[5] = wave.frequency;
	} else if ( !strcasecmp( token, "move" ) ) {
		deform->type = RAL_MATERIAL_DEFORM_MOVE;
		if ( !ParseVector( parser, deform->parameters, 3u )
				|| !ParseWave( parser, &wave ) ) return qfalse;
		deform->parameters[3] = (float)wave.function;
		deform->parameters[4] = wave.base;
		deform->parameters[5] = wave.amplitude;
		deform->parameters[6] = wave.phase;
		deform->parameters[7] = wave.frequency;
	} else return qfalse;
	entry->deformCount++;
	return qtrue;
}

static qboolean ExplicitBlendFactor( const char *token ) {
	static const char *const factors[] = {
		"GL_ZERO", "GL_ONE", "GL_SRC_COLOR", "GL_ONE_MINUS_SRC_COLOR",
		"GL_DST_COLOR", "GL_ONE_MINUS_DST_COLOR", "GL_SRC_ALPHA",
		"GL_ONE_MINUS_SRC_ALPHA", "GL_DST_ALPHA", "GL_ONE_MINUS_DST_ALPHA",
		"GL_SRC_ALPHA_SATURATE"
	};
	if ( !token ) return qfalse;
	for ( uint32_t i = 0u; i < ARRAY_LEN( factors ); ++i )
		if ( !strcasecmp( token, factors[i] ) ) return qtrue;
	return qfalse;
}

static renderMaterialBlendFactor_t BlendFactor( const char *token ) {
	if ( !strcasecmp( token, "GL_ZERO" ) ) return RENDER_MATERIAL_BLEND_ZERO;
	if ( !strcasecmp( token, "GL_ONE" ) ) return RENDER_MATERIAL_BLEND_ONE;
	if ( !strcasecmp( token, "GL_SRC_COLOR" ) ) return RENDER_MATERIAL_BLEND_SRC_COLOR;
	if ( !strcasecmp( token, "GL_ONE_MINUS_SRC_COLOR" ) ) return RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_COLOR;
	if ( !strcasecmp( token, "GL_DST_COLOR" ) ) return RENDER_MATERIAL_BLEND_DST_COLOR;
	if ( !strcasecmp( token, "GL_ONE_MINUS_DST_COLOR" ) ) return RENDER_MATERIAL_BLEND_ONE_MINUS_DST_COLOR;
	if ( !strcasecmp( token, "GL_SRC_ALPHA" ) ) return RENDER_MATERIAL_BLEND_SRC_ALPHA;
	if ( !strcasecmp( token, "GL_ONE_MINUS_SRC_ALPHA" ) ) return RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_ALPHA;
	if ( !strcasecmp( token, "GL_DST_ALPHA" ) ) return RENDER_MATERIAL_BLEND_DST_ALPHA;
	if ( !strcasecmp( token, "GL_ONE_MINUS_DST_ALPHA" ) ) return RENDER_MATERIAL_BLEND_ONE_MINUS_DST_ALPHA;
	return RENDER_MATERIAL_BLEND_SRC_ALPHA_SATURATE;
}

static qboolean ParseLightingBlock( materialTokenizer_t *parser,
		renderMaterialScriptEntry_t *entry ) {
	char token[MAX_QPATH], value[MAX_QPATH];
	if ( !NextToken( parser, token ) || strcmp( token, "{" ) ) return qfalse;
	entry->hasLighting = qtrue;
	while ( NextToken( parser, token ) ) {
		if ( !strcmp( token, "}" ) ) return qtrue;
		if ( !strcasecmp( token, "diffuse" ) || !strcasecmp( token, "emissive" ) ) {
			vec3_t *target = !strcasecmp( token, "diffuse" )
				? &entry->lighting.diffuseReflectance : &entry->lighting.emissiveRadiance;
			for ( uint32_t axis = 0u; axis < 3u; ++axis )
				if ( !NextToken( parser, value ) || !ParseFloatToken( value, &( *target )[axis] ) ) return qfalse;
		} else if ( !strcasecmp( token, "mobility" ) ) {
			if ( !NextToken( parser, value ) ) return qfalse;
			if ( !strcasecmp( value, "static" ) ) entry->lighting.emissiveMobility = RAL_LIGHT_MOBILITY_STATIC;
			else if ( !strcasecmp( value, "stationary" ) ) entry->lighting.emissiveMobility = RAL_LIGHT_MOBILITY_STATIONARY;
			else if ( !strcasecmp( value, "dynamic" ) ) entry->lighting.emissiveMobility = RAL_LIGHT_MOBILITY_DYNAMIC;
			else return qfalse;
		} else if ( !strcasecmp( token, "range" ) ) {
			if ( !NextToken( parser, value ) || !ParseFloatToken( value, &entry->lighting.emissiveInfluenceRange ) )
				return qfalse;
		} else {
			int *target = NULL;
			if ( !strcasecmp( token, "shadowPriority" ) ) target = &entry->lighting.emissiveShadowPriority;
			else if ( !strcasecmp( token, "proxyCount" ) ) target = &entry->lighting.emissiveRequestedProxyCount;
			else if ( !strcasecmp( token, "staticBake" ) ) target = &entry->lighting.participatesInStaticBake;
			else if ( !strcasecmp( token, "explicitProxyAuthority" ) ) target = &entry->lighting.emissiveExplicitProxyAuthority;
			else if ( !strcasecmp( token, "injectAtmosphere" ) ) target = &entry->lighting.emissiveInjectsAtmosphere;
			else return qfalse;
			if ( !NextToken( parser, value ) || !ParseIntToken( value, target ) ) return qfalse;
		}
	}
	return qfalse;
}

static qboolean StoreEntry( renderMaterialScriptCatalog_t *catalog,
		const renderMaterialScriptEntry_t *entry ) {
	uint32_t i;
	/* Volume-only Q3 fog is a material declaration without an image stage.
	 * Keep its authored fog metadata; qer_editorimage is not a runtime asset. */
	if ( !entry->name[0] || ( !entry->imageName[0] && !entry->sky
			&& !entry->noDraw && !entry->hasLighting
			&& !( entry->contentFlags & CONTENTS_FOG )
			&& entry->fogDepth <= 0.0f ) ) return qtrue;
	for ( i = 0u; i < catalog->count; ++i ) {
		if ( !strcasecmp( catalog->entries[i].name, entry->name ) ) {
			/* FS_ListFiles returns unique script paths in search-path priority
			 * order.  Preserve the declaration from the first (highest-priority)
			 * file, exactly as the legacy Q3 shader hash does.  A repeated
			 * declaration inside that same file still follows Q3's last-one-wins
			 * rule. */
			if ( !strcasecmp( catalog->entries[i].declarationPath,
					entry->declarationPath ) ) catalog->entries[i] = *entry;
			return qtrue;
		}
	}
	if ( catalog->count >= RENDER_MATERIAL_SCRIPT_MAX_ENTRIES ) return qfalse;
	catalog->entries[catalog->count++] = *entry;
	return qtrue;
}

static qboolean ParseFile( renderMaterialScriptCatalog_t *catalog,
		const char *text, size_t size, const char *declarationPath,
		uint64_t declarationSourceId, uint64_t declarationSize,
		unsigned declarationGeneration,
		materialParseDiagnostic_t *diagnostic ) {
	materialTokenizer_t parser = {
		.cursor = text, .end = text + size, .line = 1u, .column = 1u,
		.diagnostic = diagnostic
	};
	char token[MAX_QPATH], value[MAX_QPATH];
	while ( NextToken( &parser, token ) ) {
		renderMaterialScriptEntry_t entry;
		int depth = 0;
		int selectedStage = -1;
		int currentStage = -1;
		char skyPrefix[MAX_QPATH] = "";
		memset( &entry, 0, sizeof( entry ) );
		(void)snprintf( entry.name, sizeof( entry.name ), "%s", token );
		entry.declarationLine = parser.tokenLine;
		entry.declarationColumn = parser.tokenColumn;
		(void)snprintf( entry.declarationPath,
			sizeof( entry.declarationPath ), "%s", declarationPath );
		entry.declarationSourceId = declarationSourceId;
		entry.declarationSize = declarationSize;
		entry.declarationGeneration = declarationGeneration;
		entry.alphaMode = RENDER_ALPHA_OPAQUE;
		entry.secondaryAlphaMode = RENDER_ALPHA_OPAQUE;
		entry.cullMode = RENDER_CULL_BACK;
		entry.alphaCutoff = 0.5f; entry.depthWrite = qtrue;
		entry.sort = RENDER_MATERIAL_SORT_OPAQUE;
		entry.skyCloudHeight = 512.0f;
		entry.skyScale[0] = entry.skyScale[1] = 1.0f;
		entry.secondarySkyScale[0] = entry.secondarySkyScale[1] = 1.0f;
		entry.lighting.schemaVersion = RENDER_EMISSIVE_MATERIAL_AUTHORING_SCHEMA_VERSION;
		entry.lighting.diffuseReflectance[0] = entry.lighting.diffuseReflectance[1] =
			entry.lighting.diffuseReflectance[2] = 1.0f;
		entry.lighting.emissiveMobility = RAL_LIGHT_MOBILITY_STATIC;
		entry.lighting.emissiveInfluenceRange = 256.0f;
		entry.lighting.emissiveRequestedProxyCount = 1;
		entry.lighting.participatesInStaticBake = 1;
		if ( !NextToken( &parser, token ) || strcmp( token, "{" ) ) continue;
		depth = 1;
		while ( depth && NextToken( &parser, token ) ) {
			if ( !strcmp( token, "{" ) ) {
				depth++;
				if ( depth == 2 ) {
					renderMaterialScriptStage_t *stage = NULL;
					selectedStage = -1; currentStage = -1;
					if ( entry.stageCount >= RENDER_MATERIAL_SCRIPT_MAX_STAGES )
						return qfalse;
					currentStage = (int)entry.stageCount++;
					stage = &entry.stages[currentStage];
					stage->sourceLine = parser.tokenLine;
					stage->sourceColumn = parser.tokenColumn;
					stage->imageSource = RENDER_MATERIAL_STAGE_IMAGE;
					stage->sourceBlend = RENDER_MATERIAL_BLEND_ONE;
					stage->destinationBlend = RENDER_MATERIAL_BLEND_ZERO;
					stage->alphaCutoff = 0.5f;
					stage->scale[0] = stage->scale[1] = 1.0f;
					stage->rgbGen = RAL_MATERIAL_COLOR_BAD;
					stage->alphaGen = RAL_MATERIAL_ALPHA_IDENTITY;
					stage->constantColor[0] = stage->constantColor[1] =
						stage->constantColor[2] = stage->constantColor[3] = 1.0f;
					stage->animationLooping = qtrue;
				}
				continue;
			}
			if ( !strcmp( token, "}" ) ) {
				if ( depth == 2 ) { selectedStage = -1; currentStage = -1; }
				depth--; continue;
			}
			if ( depth == 1 && !strcasecmp( token, "wiredLighting" ) ) {
				if ( !ParseLightingBlock( &parser, &entry ) ) return qfalse;
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "surfaceparm" ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				ApplySurfaceParm( &entry, value );
				if ( !strcasecmp( value, "sky" ) ) entry.sky = qtrue;
				if ( !strcasecmp( value, "nodraw" )
						|| !strcasecmp( value, "skip" ) ) entry.noDraw = qtrue;
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "nomipmaps" ) ) {
				entry.noMipMaps = entry.noPicMip = qtrue;
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "nopicmip" ) ) {
				entry.noPicMip = qtrue;
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "polygonoffset" ) ) {
				entry.polygonOffset = qtrue;
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "deformvertexes" ) ) {
				if ( !ParseDeform( &parser, &entry ) ) return qfalse;
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "fogparms" ) ) {
				uint32_t fogLine = parser.tokenLine;
				if ( !ParseVector( &parser, entry.fogColor, 3u )
						|| !NextToken( &parser, value )
						|| !ParseFloatToken( value, &entry.fogDepth ) ) return qfalse;
				if ( NextToken( &parser, value ) ) {
					if ( parser.tokenLine != fogLine ) {
						if ( !PushToken( &parser, value ) ) return qfalse;
					} else if ( !strcasecmp( value, "linear" )
							|| !strcasecmp( value, "exp" )
							|| !strcasecmp( value, "exp2" ) ) {
						entry.fogType = !strcasecmp( value, "linear" )
							? RAL_MATERIAL_FOG_LINEAR : !strcasecmp( value, "exp" )
							? RAL_MATERIAL_FOG_EXP : RAL_MATERIAL_FOG_EXP2;
						if ( !NextToken( &parser, value )
								|| !ParseFloatToken( value, &entry.fogDensity )
								|| !NextToken( &parser, value )
								|| !ParseFloatToken( value, &entry.fogFarClip ) )
							return qfalse;
					} else if ( !PushToken( &parser, value ) ) return qfalse;
				}
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "sort" ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( !strcasecmp( value, "portal" ) )
					entry.sort = RENDER_MATERIAL_SORT_PORTAL;
				else if ( !strcasecmp( value, "sky" ) )
					entry.sort = RENDER_MATERIAL_SORT_ENVIRONMENT;
				else if ( !strcasecmp( value, "opaque" ) )
					entry.sort = RENDER_MATERIAL_SORT_OPAQUE;
				else if ( !strcasecmp( value, "decal" ) )
					entry.sort = RENDER_MATERIAL_SORT_DECAL;
				else if ( !strcasecmp( value, "seethrough" ) )
					entry.sort = RENDER_MATERIAL_SORT_SEE_THROUGH;
				else if ( !strcasecmp( value, "banner" ) )
					entry.sort = RENDER_MATERIAL_SORT_BANNER;
				else if ( !strcasecmp( value, "underwater" ) )
					entry.sort = RENDER_MATERIAL_SORT_UNDERWATER;
				else if ( !strcasecmp( value, "additive" ) )
					entry.sort = RENDER_MATERIAL_SORT_ADDITIVE;
				else if ( !strcasecmp( value, "nearest" ) )
					entry.sort = RENDER_MATERIAL_SORT_NEAREST;
				else if ( !ParseFloatToken( value, &entry.sort )
						|| entry.sort <= 0.0f ) return qfalse;
				entry.sortExplicit = qtrue;
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "cull" ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( !strcasecmp( value, "none" )
						|| !strcasecmp( value, "disable" )
						|| !strcasecmp( value, "twosided" ) )
					entry.cullMode = RENDER_CULL_NONE;
				else if ( !strcasecmp( value, "front" )
						|| !strcasecmp( value, "frontsided" ) )
					entry.cullMode = RENDER_CULL_BACK;
				else if ( !strcasecmp( value, "back" )
						|| !strcasecmp( value, "backside" )
						|| !strcasecmp( value, "backsided" ) )
					entry.cullMode = RENDER_CULL_FRONT;
				else return qfalse;
				continue;
			}
			if ( depth == 1 && ( !strcasecmp( token, "q3map_sun" )
					|| !strcasecmp( token, "q3map_sunExt" ) ) ) {
				ralMaterialSourceSky_t *sky = &entry.skySource;
				float parameters[8] = { 0.0f };
				uint32_t line = parser.tokenLine;
				sky->sunExtended = !strcasecmp( token, "q3map_sunExt" );
				sky->sunSpan = (ralMaterialSourceSpan_t){
					declarationSourceId, line, parser.tokenColumn };
				for ( uint32_t i = 0u; i < ( sky->sunExtended ? 8u : 6u ); ++i ) {
					if ( !NextToken( &parser, value ) || parser.tokenLine != line
							|| !ParseFloatToken( value, &parameters[i] ) ) return qfalse;
				}
				if ( parameters[0] < 0.0f || parameters[1] < 0.0f
						|| parameters[2] < 0.0f || parameters[3] < 0.0f
						|| parameters[6] < 0.0f || parameters[7] < 0.0f
						|| parameters[7] >= 4294967296.0
						|| floorf( parameters[7] ) != parameters[7] ) return qfalse;
				memcpy( sky->sunColor, parameters, sizeof( sky->sunColor ) );
				sky->sunIntensity = parameters[3];
				sky->sunAzimuthDegrees = parameters[4];
				sky->sunElevationDegrees = parameters[5];
				sky->sunDevianceDegrees = parameters[6];
				sky->sunSamples = (uint32_t)parameters[7];
				sky->hasSun = qtrue;
				continue;
			}
			if ( depth == 1 && !strcasecmp( token, "skyparms" ) ) {
				char cloudHeight[MAX_QPATH], innerBox[MAX_QPATH];
				entry.skySource.parmsSpan = (ralMaterialSourceSpan_t){
					declarationSourceId, parser.tokenLine, parser.tokenColumn };
				if ( !NextToken( &parser, value )
						|| !NextToken( &parser, cloudHeight )
						|| !NextToken( &parser, innerBox ) ) return qfalse;
				(void)snprintf( skyPrefix, sizeof( skyPrefix ), "%s",
					UsableImage( value ) ? value : "" );
				(void)snprintf( entry.skyInnerBoxPrefix,
					sizeof( entry.skyInnerBoxPrefix ), "%s",
					UsableImage( innerBox ) ? innerBox : "" );
				/* Shipped Q3 scripts use '-' (mre/space skies), and old
				 * declarations use 'full' before a later numeric skyParms.
				 * Both historically select the zero/default 512 height. */
				if ( !strcmp( cloudHeight, "-" ) || !strcasecmp( cloudHeight, "full" ) )
					entry.skyCloudHeight = 0.0f;
				else if ( !ParseFloatToken( cloudHeight, &entry.skyCloudHeight )
						|| entry.skyCloudHeight < 0.0f ) return qfalse;
				if ( entry.skyCloudHeight == 0.0f )
					entry.skyCloudHeight = 512.0f;
				entry.sky = qtrue; continue;
			}
			if ( depth != 2 ) continue;
			if ( !strcasecmp( token, "map" )
					|| !strcasecmp( token, "clampmap" ) ) {
				qboolean clamp = !strcasecmp( token, "clampmap" );
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( currentStage >= 0 ) {
					renderMaterialScriptStage_t *stage = &entry.stages[currentStage];
					stage->clampToEdge = clamp;
					if ( !strcasecmp( value, "$lightmap" ) )
						stage->imageSource = RENDER_MATERIAL_STAGE_LIGHTMAP;
					else if ( !strcasecmp( value, "$whiteimage" ) )
						stage->imageSource = RENDER_MATERIAL_STAGE_WHITE;
					else if ( UsableImage( value ) )
						(void)snprintf( stage->imageName,
							sizeof( stage->imageName ), "%s", value );
				}
				if ( !entry.imageName[0] && UsableImage( value ) ) {
					(void)snprintf( entry.imageName, sizeof( entry.imageName ),
						"%s", value ); entry.clampToEdge = clamp;
					selectedStage = 0;
				} else if ( !entry.secondaryImageName[0] && UsableImage( value ) ) {
					(void)snprintf( entry.secondaryImageName,
						sizeof( entry.secondaryImageName ), "%s", value );
					entry.secondaryClampToEdge = clamp;
					selectedStage = 1;
				} else {
					selectedStage = -1;
				}
			} else if ( !strcasecmp( token, "animmap" )
					|| !strcasecmp( token, "animmapclamp" ) ) {
				uint32_t commandLine = parser.tokenLine;
				renderMaterialScriptStage_t *stage = currentStage >= 0
					? &entry.stages[currentStage] : NULL;
				qboolean looping = !strcasecmp( token, "animmap" );
				if ( !stage || !NextToken( &parser, value )
						|| !ParseFloatToken( value, &stage->animationFrequency ) )
					return qfalse;
				stage->animationLooping = looping;
				stage->clampToEdge = looping ? qfalse : qtrue;
				stage->animationFrameOffset = entry.animationPathCount;
				while ( NextToken( &parser, value ) ) {
					if ( parser.tokenLine != commandLine ) {
						if ( !PushToken( &parser, value ) ) return qfalse;
						break;
					}
					if ( !UsableImage( value )
							|| stage->animationFrameCount
								>= RENDER_MATERIAL_SCRIPT_MAX_ANIMATION_FRAMES
							|| entry.animationPathCount
								>= RENDER_MATERIAL_SCRIPT_MAX_ANIMATION_PATHS )
						return qfalse;
					(void)snprintf( entry.animationFrameNames[
						entry.animationPathCount++], MAX_QPATH, "%s", value );
					stage->animationFrameCount++;
				}
				if ( !stage->animationFrameCount ) return qfalse;
				(void)snprintf( stage->imageName, sizeof( stage->imageName ), "%s",
					entry.animationFrameNames[stage->animationFrameOffset] );
				if ( !entry.imageName[0] ) {
					(void)snprintf( entry.imageName, sizeof( entry.imageName ),
						"%s", stage->imageName );
					selectedStage = 0;
				} else if ( !entry.secondaryImageName[0] ) {
					(void)snprintf( entry.secondaryImageName,
						sizeof( entry.secondaryImageName ), "%s", stage->imageName );
					selectedStage = 1;
				} else selectedStage = -1;
			} else if ( currentStage >= 0 && !strcasecmp( token, "tcmod" ) ) {
				char operation[MAX_QPATH], first[MAX_QPATH], second[MAX_QPATH];
				uint32_t tcModLine = parser.tokenLine;
				uint32_t tcModColumn = parser.tokenColumn;
				if ( !NextToken( &parser, operation ) ) return qfalse;
				if ( !strcasecmp( operation, "scale" )
						|| !strcasecmp( operation, "scroll" ) ) {
					float parsed[2];
					if ( !NextToken( &parser, first ) || !NextToken( &parser, second )
							|| !ParseFloatToken( first, &parsed[0] )
							|| !ParseFloatToken( second, &parsed[1] ) ) return qfalse;
					float *stageTarget = !strcasecmp( operation, "scale" )
						? entry.stages[currentStage].scale
						: entry.stages[currentStage].scroll;
					stageTarget[0] = parsed[0]; stageTarget[1] = parsed[1];
					if ( !AppendSourceTcMod( &entry.stages[currentStage],
						!strcasecmp( operation, "scale" )
							? RAL_MATERIAL_TCMOD_SCALE : RAL_MATERIAL_TCMOD_SCROLL,
						parsed, 2u, tcModLine, tcModColumn ) ) return qfalse;
					if ( selectedStage >= 0 ) {
						float *legacyTarget = selectedStage == 0
							? ( !strcasecmp( operation, "scale" )
								? entry.skyScale : entry.skyScroll )
							: ( !strcasecmp( operation, "scale" )
								? entry.secondarySkyScale : entry.secondarySkyScroll );
						legacyTarget[0] = parsed[0]; legacyTarget[1] = parsed[1];
					}
				} else if ( !strcasecmp( operation, "rotate" ) ) {
					if ( !NextToken( &parser, value ) || currentStage < 0
							|| !ParseFloatToken( value,
								&entry.stages[currentStage].rotateDegrees ) ) return qfalse;
					if ( !AppendSourceTcMod( &entry.stages[currentStage],
						RAL_MATERIAL_TCMOD_ROTATE,
						&entry.stages[currentStage].rotateDegrees, 1u,
						tcModLine, tcModColumn ) ) return qfalse;
				} else if ( !strcasecmp( operation, "turb" ) ) {
					uint32_t component = 0u;
					if ( currentStage < 0 || !NextToken( &parser, value ) ) return qfalse;
					/* ioq3 content normally omits the waveform name for turb, but
					 * several shader packs spell the equivalent `turb sin ...`. */
					if ( ParseFloatToken( value,
							&entry.stages[currentStage].turbulence[0] ) ) component = 1u;
					for ( ; component < 4u; ++component )
						if ( !NextToken( &parser, value )
								|| !ParseFloatToken( value,
									&entry.stages[currentStage].turbulence[component] ) )
							return qfalse;
					entry.stages[currentStage].hasTurbulence = qtrue;
					if ( !AppendSourceTcMod( &entry.stages[currentStage],
						RAL_MATERIAL_TCMOD_TURBULENCE,
						entry.stages[currentStage].turbulence, 4u,
						tcModLine, tcModColumn ) ) return qfalse;
				} else if ( !strcasecmp( operation, "stretch" ) ) {
					float sourceParameters[5];
					if ( currentStage < 0 || !NextToken( &parser, value ) ) return qfalse;
					sourceParameters[0] = (float)WaveFunction( value );
					if ( sourceParameters[0] == (float)RAL_MATERIAL_WAVE_NONE )
						return qfalse;
					for ( uint32_t component = 0u; component < 4u; ++component )
						if ( !NextToken( &parser, value )
								|| !ParseFloatToken( value,
									&entry.stages[currentStage].stretch[component] ) )
							return qfalse;
					for ( uint32_t component = 0u; component < 4u; ++component )
						sourceParameters[component + 1u] =
							entry.stages[currentStage].stretch[component];
					entry.stages[currentStage].hasStretch = qtrue;
					if ( !AppendSourceTcMod( &entry.stages[currentStage],
						RAL_MATERIAL_TCMOD_STRETCH, sourceParameters, 5u,
						tcModLine, tcModColumn ) ) return qfalse;
				} else if ( !strcasecmp( operation, "transform" ) ) {
					float matrix[6];
					for ( uint32_t component = 0u; component < ARRAY_LEN( matrix ); ++component ) {
						if ( !NextToken( &parser, value )
								|| !ParseFloatToken( value, &matrix[component] ) ) return qfalse;
					}
					entry.stages[currentStage].hasTransform = qtrue;
					memcpy( entry.stages[currentStage].transform, matrix,
						sizeof( matrix ) );
					if ( !AppendSourceTcMod( &entry.stages[currentStage],
						RAL_MATERIAL_TCMOD_TRANSFORM, matrix, 6u,
						tcModLine, tcModColumn ) ) return qfalse;
					/* Keep the existing frontend compatibility projection for diagonal
					 * transforms while the source receipt preserves the complete matrix. */
					if ( selectedStage == 0 && fabsf( matrix[1] ) < 0.000001f
							&& fabsf( matrix[2] ) < 0.000001f ) {
						entry.skyScale[0] = matrix[0]; entry.skyScale[1] = matrix[3];
						entry.skyScroll[0] = matrix[4]; entry.skyScroll[1] = matrix[5];
						entry.hasTcTransform = qtrue;
					}
					if ( fabsf( matrix[1] ) < 0.000001f
							&& fabsf( matrix[2] ) < 0.000001f ) {
						entry.stages[currentStage].scale[0] = matrix[0];
						entry.stages[currentStage].scale[1] = matrix[3];
						entry.stages[currentStage].scroll[0] = matrix[4];
						entry.stages[currentStage].scroll[1] = matrix[5];
					}
				}
			} else if ( currentStage >= 0
					&& ( !strcasecmp( token, "tcgen" )
						|| !strcasecmp( token, "texgen" ) ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( !strcasecmp( value, "environment" ) )
					entry.stages[currentStage].tcGen =
						RENDER_MATERIAL_TCGEN_ENVIRONMENT;
				else if ( !strcasecmp( value, "lightmap" ) )
					entry.stages[currentStage].tcGen =
						RENDER_MATERIAL_TCGEN_LIGHTMAP;
				else if ( !strcasecmp( value, "texture" )
						|| !strcasecmp( value, "base" ) )
					entry.stages[currentStage].tcGen =
						RENDER_MATERIAL_TCGEN_TEXTURE;
				else if ( !strcasecmp( value, "vector" ) ) {
					if ( !ParseVector( &parser,
							entry.stages[currentStage].tcGenVectors[0], 3u )
							|| !ParseVector( &parser,
								entry.stages[currentStage].tcGenVectors[1], 3u ) )
						return qfalse;
					entry.stages[currentStage].tcGen = RENDER_MATERIAL_TCGEN_VECTOR;
				} else return qfalse;
			} else if ( currentStage >= 0
					&& !strcasecmp( token, "depthfunc" ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( !strcasecmp( value, "lequal" ) )
					entry.stages[currentStage].depthFunc = RAL_MATERIAL_DEPTH_LEQUAL;
				else if ( !strcasecmp( value, "equal" ) )
					entry.stages[currentStage].depthFunc = RAL_MATERIAL_DEPTH_EQUAL;
				else return qfalse;
			} else if ( currentStage >= 0 && !strcasecmp( token, "rgbgen" ) ) {
				renderMaterialScriptStage_t *stage = &entry.stages[currentStage];
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( !strcasecmp( value, "wave" ) ) {
					stage->rgbGen = RAL_MATERIAL_COLOR_WAVE;
					if ( !ParseWave( &parser, &stage->rgbWave ) ) return qfalse;
				} else if ( !strcasecmp( value, "const" ) ) {
					stage->rgbGen = RAL_MATERIAL_COLOR_CONST;
					if ( !ParseVector( &parser, stage->constantColor, 3u ) )
						return qfalse;
				} else if ( !strcasecmp( value, "identitylighting" )
						|| !strcasecmp( value, "identitylight" ) )
					stage->rgbGen = RAL_MATERIAL_COLOR_IDENTITY_LIGHTING;
				else if ( !strcasecmp( value, "identity" ) )
					stage->rgbGen = RAL_MATERIAL_COLOR_IDENTITY;
				else if ( !strcasecmp( value, "entity" ) )
					stage->rgbGen = RAL_MATERIAL_COLOR_ENTITY;
				else if ( !strcasecmp( value, "oneminusentity" ) )
					stage->rgbGen = RAL_MATERIAL_COLOR_ONE_MINUS_ENTITY;
				else if ( !strcasecmp( value, "exactvertex" ) )
					stage->rgbGen = RAL_MATERIAL_COLOR_EXACT_VERTEX;
				else if ( !strcasecmp( value, "vertex" ) )
					stage->rgbGen = RAL_MATERIAL_COLOR_VERTEX;
				else if ( !strcasecmp( value, "oneminusvertex" ) )
					stage->rgbGen = RAL_MATERIAL_COLOR_ONE_MINUS_VERTEX;
				else if ( !strcasecmp( value, "lightingdiffuse" ) )
					stage->rgbGen = RAL_MATERIAL_COLOR_LIGHTING_DIFFUSE;
				else if ( !strcasecmp( value, "fog" ) )
					stage->rgbGen = RAL_MATERIAL_COLOR_FOG;
				else return qfalse;
			} else if ( currentStage >= 0 && !strcasecmp( token, "alphagen" ) ) {
				renderMaterialScriptStage_t *stage = &entry.stages[currentStage];
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( !strcasecmp( value, "wave" ) ) {
					stage->alphaGen = RAL_MATERIAL_ALPHA_WAVE;
					if ( !ParseWave( &parser, &stage->alphaWave ) ) return qfalse;
				} else if ( !strcasecmp( value, "const" ) ) {
					stage->alphaGen = RAL_MATERIAL_ALPHA_CONST;
					if ( !NextToken( &parser, value )
							|| !ParseFloatToken( value, &stage->constantColor[3] ) )
						return qfalse;
				} else if ( !strcasecmp( value, "identity" ) )
					stage->alphaGen = RAL_MATERIAL_ALPHA_IDENTITY;
				else if ( !strcasecmp( value, "entity" ) )
					stage->alphaGen = RAL_MATERIAL_ALPHA_ENTITY;
				else if ( !strcasecmp( value, "oneminusentity" ) )
					stage->alphaGen = RAL_MATERIAL_ALPHA_ONE_MINUS_ENTITY;
				else if ( !strcasecmp( value, "vertex" ) )
					stage->alphaGen = RAL_MATERIAL_ALPHA_VERTEX;
				else if ( !strcasecmp( value, "oneminusvertex" ) )
					stage->alphaGen = RAL_MATERIAL_ALPHA_ONE_MINUS_VERTEX;
				else if ( !strcasecmp( value, "lightingspecular" ) )
					stage->alphaGen = RAL_MATERIAL_ALPHA_LIGHTING_SPECULAR;
				else if ( !strcasecmp( value, "portal" ) ) {
					uint32_t commandLine = parser.tokenLine;
					stage->alphaGen = RAL_MATERIAL_ALPHA_PORTAL;
					stage->portalRange = 256.0f;
					if ( NextToken( &parser, value ) ) {
						if ( parser.tokenLine != commandLine
								|| !ParseFloatToken( value, &stage->portalRange ) ) {
							if ( !PushToken( &parser, value ) ) return qfalse;
						}
					}
				} else return qfalse;
			} else if ( !strcasecmp( token, "alphafunc" ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( currentStage >= 0 ) {
					renderMaterialScriptStage_t *stage =
						&entry.stages[currentStage];
					if ( !strcasecmp( value, "gt0" ) ) {
						stage->alphaTest = RAL_MATERIAL_ALPHA_TEST_KEEP_NOT_EQUAL;
						stage->alphaCutoff = 0.0f;
					} else if ( !strcasecmp( value, "lt128" ) ) {
						stage->alphaTest = RAL_MATERIAL_ALPHA_TEST_KEEP_LESS;
						stage->alphaCutoff = 0.5f;
					} else if ( !strcasecmp( value, "ge128" ) ) {
						stage->alphaTest =
							RAL_MATERIAL_ALPHA_TEST_KEEP_GREATER_EQUAL;
						stage->alphaCutoff = 0.5f;
					} else return qfalse;
				}
				if ( selectedStage == 0 ) {
					entry.alphaMode = RENDER_ALPHA_MASK;
					entry.alphaCutoff = !strcasecmp( value, "ge128" ) ? 0.5f : 0.0f;
				} else if ( selectedStage == 1 ) {
					entry.secondaryAlphaMode = RENDER_ALPHA_MASK;
				}
			} else if ( !strcasecmp( token, "blendfunc" ) ) {
				renderAlphaMode_t stageAlphaMode;
				renderMaterialBlendFactor_t sourceBlend = RENDER_MATERIAL_BLEND_SRC_ALPHA;
				renderMaterialBlendFactor_t destinationBlend = RENDER_MATERIAL_BLEND_ONE_MINUS_SRC_ALPHA;
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( !strcasecmp( value, "filter" ) ) {
					stageAlphaMode = RENDER_ALPHA_OPAQUE;
					sourceBlend = RENDER_MATERIAL_BLEND_DST_COLOR;
					destinationBlend = RENDER_MATERIAL_BLEND_ZERO;
				}
				else if ( !strcasecmp( value, "add" )
						|| !strcasecmp( value, "GL_add" )
						|| !strcasecmp( value, "GL_ONE" ) ) {
					stageAlphaMode = RENDER_ALPHA_ADDITIVE;
					sourceBlend = destinationBlend = RENDER_MATERIAL_BLEND_ONE;
				}
				else stageAlphaMode = RENDER_ALPHA_BLEND;
				/* The long form supplies a source and destination factor.  Consume
				 * the destination token so it cannot be mistaken for a stage command.
				 * GL_ONE GL_ONE is the canonical spelling of additive blending. */
				if ( ExplicitBlendFactor( value ) ) {
					char destination[MAX_QPATH];
					if ( !NextToken( &parser, destination ) ) return qfalse;
					sourceBlend = BlendFactor( value );
					destinationBlend = BlendFactor( destination );
					if ( !strcasecmp( value, "GL_ONE" )
							&& !strcasecmp( destination, "GL_ONE" ) )
						stageAlphaMode = RENDER_ALPHA_ADDITIVE;
					else if ( !strcasecmp( value, "GL_SRC_ALPHA" )
							&& !strcasecmp( destination, "GL_ONE" ) )
						stageAlphaMode = RENDER_ALPHA_ALPHA_ADDITIVE;
					else if ( ( !strcasecmp( value, "GL_DST_COLOR" )
								&& !strcasecmp( destination, "GL_ZERO" ) )
							|| ( !strcasecmp( value, "GL_ZERO" )
								&& !strcasecmp( destination, "GL_SRC_COLOR" ) ) )
						/* The frontend binds BSP lightmaps separately.  The canonical
						 * Q3 filter stage therefore collapses to an opaque base sample,
						 * not framebuffer alpha blending. */
						stageAlphaMode = RENDER_ALPHA_OPAQUE;
				}
				if ( currentStage >= 0 ) {
					entry.stages[currentStage].sourceBlend = sourceBlend;
					entry.stages[currentStage].destinationBlend = destinationBlend;
				}
				if ( selectedStage == 0 ) {
					entry.alphaMode = stageAlphaMode;
					if ( stageAlphaMode != RENDER_ALPHA_OPAQUE )
						entry.depthWrite = qfalse;
				} else if ( selectedStage == 1 ) {
					entry.secondaryAlphaMode = stageAlphaMode;
				}
				} else if ( !strcasecmp( token, "depthwrite" ) ) {
					if ( currentStage >= 0 )
						entry.stages[currentStage].depthWrite = qtrue;
					if ( selectedStage == 0 ) entry.depthWrite = qtrue;
				}
		}
		if ( depth ) return qfalse;
		if ( entry.sky && skyPrefix[0] )
			(void)snprintf( entry.skyBoxPrefix, sizeof( entry.skyBoxPrefix ),
				"%s", skyPrefix );
		if ( entry.sky && skyPrefix[0] )
			(void)snprintf( entry.imageName, sizeof( entry.imageName ),
				"%s_up", skyPrefix );
		if ( entry.sky ) entry.sort = RENDER_MATERIAL_SORT_ENVIRONMENT;
		else if ( !entry.sortExplicit && !entry.depthWrite
				&& entry.alphaMode != RENDER_ALPHA_OPAQUE )
			entry.sort = ( entry.alphaMode == RENDER_ALPHA_ADDITIVE
					|| entry.alphaMode == RENDER_ALPHA_ALPHA_ADDITIVE )
				? RENDER_MATERIAL_SORT_ADDITIVE : RENDER_MATERIAL_SORT_BLEND;
		/* A fog volume with no raster stages contributes medium parameters,
		 * not a white polygon. Preserve those parameters in the catalog. */
		if ( !entry.sky && entry.stageCount == 0u
				&& ( ( entry.contentFlags & CONTENTS_FOG ) || entry.fogDepth > 0.0f ) )
			entry.noDraw = qtrue;
		if ( !StoreEntry( catalog, &entry ) ) return qfalse;
	}
	return qtrue;
}

qboolean RenderMaterialScript_Load( renderMaterialScriptCatalog_t *catalog,
		const refimport_t *imports ) {
	char **files;
	int count = 0;
	qboolean valid = qtrue;
	if ( !catalog || !imports ) return qfalse;
	memset( catalog, 0, sizeof( *catalog ) );
	/* A focused/offscreen host may intentionally provide no VFS. Direct image
	 * registration remains valid there; the product host supplies the complete
	 * cohort and gets the populated script catalog. */
	if ( !imports->FS_ListFiles || !imports->FS_FreeFileList
			|| !imports->FS_ReadFile || !imports->FS_FreeFile ) {
		catalog->ready = qtrue; return qtrue;
	}
	files = imports->FS_ListFiles( "scripts", ".shader", &count );
	for ( int i = 0; files && i < count; ++i ) {
		char path[MAX_QPATH]; void *text = NULL;
		char canonicalPath[MAX_QPATH];
		materialParseDiagnostic_t diagnostic;
		uint64_t sourceId = 0u, sourceSize = 0u;
		unsigned sourceGeneration = 0u;
		int bytes = snprintf( path, sizeof( path ), "scripts/%s", files[i] );
		if ( bytes <= 0 || bytes >= (int)sizeof( path ) ) continue;
		if ( !imports->FS_ResolveResource
				|| !imports->FS_ResolveResource( path, canonicalPath,
					sizeof( canonicalPath ), &sourceId, &sourceSize,
					&sourceGeneration ) ) {
			valid = qfalse; break;
		}
		bytes = imports->FS_ReadFile( path, &text );
		memset( &diagnostic, 0, sizeof( diagnostic ) );
		if ( bytes > 0 && text && !ParseFile( catalog,
			(const char *)text, (size_t)bytes, canonicalPath, sourceId,
			sourceSize, sourceGeneration, &diagnostic ) ) {
			valid = qfalse;
			if ( imports->GetLogChannel && imports->LogCh ) {
				int channel = imports->GetLogChannel( "renderer.material.import" );
				imports->LogCh( channel, SEV_ERROR,
					"material-import parse-failed path=%s line=%u column=%u token=%s source-id=%llu generation=%u\n",
					canonicalPath, diagnostic.line, diagnostic.column,
					diagnostic.token[0] ? diagnostic.token : "<eof>",
					(unsigned long long)sourceId,
					sourceGeneration );
			}
		}
		if ( text ) imports->FS_FreeFile( text );
		if ( !valid ) break;
	}
	if ( files ) imports->FS_FreeFileList( files );
	if ( !valid ) {
		memset( catalog, 0, sizeof( *catalog ) );
		return qfalse;
	}
	catalog->ready = qtrue;
	return qtrue;
}

qboolean RenderMaterialScript_Lookup(
		const renderMaterialScriptCatalog_t *catalog, const char *name,
		renderMaterialScriptEntry_t *outEntry ) {
	char canonical[MAX_QPATH];
	char *dot, *slash;
	if ( !catalog || !catalog->ready || !name || !outEntry ) return qfalse;
	for ( uint32_t i = 0u; i < catalog->count; ++i ) {
		if ( !strcasecmp( catalog->entries[i].name, name ) ) {
			*outEntry = catalog->entries[i]; return qtrue;
		}
	}
	/* MD3/IQM material slots commonly preserve an authored image extension
	 * while the matching Q3 shader declaration names the material without it.
	 * The legacy renderer normalizes that distinction before shader lookup;
	 * keep the same content contract in the backend-neutral catalog so every
	 * RAL adapter resolves the scripted material instead of a missing image. */
	if ( strlen( name ) >= sizeof( canonical ) ) return qfalse;
	(void)snprintf( canonical, sizeof( canonical ), "%s", name );
	dot = strrchr( canonical, '.' );
	slash = strrchr( canonical, '/' );
	if ( !dot || ( slash && dot < slash ) ) return qfalse;
	*dot = '\0';
	for ( uint32_t i = 0u; i < catalog->count; ++i ) {
		if ( !strcasecmp( catalog->entries[i].name, canonical ) ) {
			*outEntry = catalog->entries[i]; return qtrue;
		}
	}
	return qfalse;
}

static uint64_t MaterialDependencyOptions( const renderMaterialScriptStage_t *stage ) {
	uint64_t hash = UINT64_C( 14695981039346656037 );
	const unsigned char options[] = {
		(unsigned char)stage->clampToEdge,
		(unsigned char)stage->imageSource,
		(unsigned char)stage->tcGen
	};
	for ( uint32_t i = 0u; i < ARRAY_LEN( options ); ++i ) {
		hash ^= options[i]; hash *= UINT64_C( 1099511628211 );
	}
	return hash ? hash : 1u;
}

static qboolean ResolveMaterialDependency( ralMaterialSourceIr_t *source,
		const refimport_t *imports, const renderMaterialScriptStage_t *stage,
		const char *name, ralMaterialSourceSpan_t span,
		uint32_t *outDependencyIndex ) {
	char canonical[MAX_QPATH];
	uint64_t sourceId = 0u, byteSize = 0u;
	unsigned fsGeneration = 0u;
	uint64_t optionsHash;
	uint32_t dependencyIndex;
	if ( !source || !imports || !imports->FS_ResolveResource || !stage
			|| !name || !name[0] || !outDependencyIndex ) return qfalse;
	optionsHash = MaterialDependencyOptions( stage );
	if ( !imports->FS_ResolveResource( name, canonical, sizeof( canonical ),
			&sourceId, &byteSize, &fsGeneration ) ) return qfalse;
	for ( dependencyIndex = 0u; dependencyIndex < source->dependencyCount;
			++dependencyIndex ) {
		ralMaterialSourceDependency_t *dependency =
			&source->dependencies[dependencyIndex];
		if ( dependency->sourceId == sourceId
				&& dependency->creationOptionsHash == optionsHash ) break;
	}
	if ( dependencyIndex == source->dependencyCount ) {
		ralMaterialSourceDependency_t *dependency;
		if ( source->dependencyCount >= RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES )
			return qfalse;
		dependency = &source->dependencies[source->dependencyCount++];
		dependency->role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
		dependency->sourceId = sourceId;
		dependency->fsGeneration = fsGeneration;
		dependency->size = byteSize;
		dependency->creationOptionsHash = optionsHash;
		dependency->span = span;
		(void)snprintf( dependency->canonicalPath,
			sizeof( dependency->canonicalPath ), "%s", canonical );
	}
	*outDependencyIndex = dependencyIndex;
	return qtrue;
}

static qboolean AddTcMod( ralMaterialSourceStage_t *target,
		ralMaterialTcModType_t type, const float *parameters,
		uint32_t parameterCount, ralMaterialSourceSpan_t span ) {
	ralMaterialSourceTcMod_t *tcMod;
	if ( target->tcModCount >= RAL_MATERIAL_SOURCE_MAX_TCMODS
			|| parameterCount > ARRAY_LEN( target->tcMods[0].parameters ) )
		return qfalse;
	tcMod = &target->tcMods[target->tcModCount++];
	tcMod->type = type;
	tcMod->span = span;
	memcpy( tcMod->parameters, parameters, parameterCount * sizeof( float ) );
	return qtrue;
}

static qboolean ResolveSkyBox( ralMaterialSourceIr_t *source,
		const refimport_t *imports, const char *prefix, qboolean clamp,
		qboolean *present, uint32_t dependencies[RAL_MATERIAL_SKY_FACE_COUNT] ) {
	static const char *suffix[RAL_MATERIAL_SKY_FACE_COUNT] = {
		"rt", "bk", "lf", "ft", "up", "dn"
	};
	renderMaterialScriptStage_t image;
	*present = prefix[0] ? qtrue : qfalse;
	if ( !*present ) return qtrue;
	memset( &image, 0, sizeof( image ) );
	image.imageSource = RENDER_MATERIAL_STAGE_IMAGE;
	image.clampToEdge = clamp;
	for ( uint32_t face = 0u; face < RAL_MATERIAL_SKY_FACE_COUNT; ++face ) {
		char path[MAX_QPATH];
		/* Format-defined expansion, not extension probing. Static VFS aliases
		 * resolve these authored .tga names to the canonical asset. */
		if ( snprintf( path, sizeof( path ), "%s_%s.tga", prefix, suffix[face] )
				>= (int)sizeof( path )
				|| !ResolveMaterialDependency( source, imports, &image, path,
					source->skySource.parmsSpan, &dependencies[face] ) ) return qfalse;
	}
	return qtrue;
}

qboolean RenderMaterialScript_BuildStages(
		const renderMaterialScriptEntry_t *entry,
		renderMaterialStageResolver_t resolve, void *context,
		renderMaterialStageSnapshot_t outStages[RENDER_MATERIAL_MAX_STAGES] ) {
	renderMaterialStageSnapshot_t stages[RENDER_MATERIAL_MAX_STAGES];
	if ( !entry || !resolve || !outStages || !entry->stageCount
			|| entry->stageCount > RENDER_MATERIAL_MAX_STAGES
			|| entry->animationPathCount > RENDER_MATERIAL_SCRIPT_MAX_ANIMATION_PATHS )
		return qfalse;
	/* Validate all indices before any resolver call. */
	for ( uint32_t i = 0u; i < entry->stageCount; ++i ) {
		const renderMaterialScriptStage_t *source = &entry->stages[i];
		if ( source->imageSource < RENDER_MATERIAL_STAGE_IMAGE
				|| source->imageSource > RENDER_MATERIAL_STAGE_WHITE
				|| (uint32_t)source->depthFunc > RAL_MATERIAL_DEPTH_EQUAL
				|| ( source->depthWrite != qfalse && source->depthWrite != qtrue )
				|| source->sourceTcModCount > RAL_MATERIAL_SOURCE_MAX_TCMODS
				|| source->animationFrameCount > RENDER_MATERIAL_MAX_ANIMATION_FRAMES
				|| source->animationFrameOffset > entry->animationPathCount
				|| source->animationFrameCount > entry->animationPathCount - source->animationFrameOffset )
			return qfalse;
	}
	memset( stages, 0, sizeof( stages ) );
	for ( uint32_t i = 0u; i < entry->stageCount; ++i ) {
		const renderMaterialScriptStage_t *source = &entry->stages[i];
		renderMaterialStageSnapshot_t *target = &stages[i];
		target->imageSource = (uint32_t)source->imageSource;
		target->tcGen = (uint32_t)source->tcGen;
		memcpy( target->tcGenVectors, source->tcGenVectors, sizeof( target->tcGenVectors ) );
		target->sourceBlend = (uint32_t)source->sourceBlend;
		target->destinationBlend = (uint32_t)source->destinationBlend;
		target->alphaTest = (uint32_t)source->alphaTest;
		target->alphaCutoff = source->alphaCutoff;
		target->depthWrite = source->depthWrite || ( i == 0u && entry->depthWrite );
		target->depthFunc = (uint32_t)source->depthFunc;
		target->rgbGen = source->rgbGen == RAL_MATERIAL_COLOR_BAD
			? ( source->sourceBlend == RENDER_MATERIAL_BLEND_ONE
				|| source->sourceBlend == RENDER_MATERIAL_BLEND_SRC_ALPHA
				? RAL_MATERIAL_COLOR_IDENTITY_LIGHTING : RAL_MATERIAL_COLOR_IDENTITY )
			: (uint32_t)source->rgbGen;
		target->alphaGen = (uint32_t)source->alphaGen;
		memcpy( target->constantColor, source->constantColor, sizeof( target->constantColor ) );
		target->rgbWave = source->rgbWave;
		target->alphaWave = source->alphaWave;
		target->portalRange = source->portalRange;
		target->scaleScroll[0] = source->scale[0];
		target->scaleScroll[1] = source->scale[1];
		target->scaleScroll[2] = source->scroll[0];
		target->scaleScroll[3] = source->scroll[1];
		target->rotateDegrees = source->rotateDegrees;
		memcpy( target->turbulence, source->turbulence, sizeof( target->turbulence ) );
		memcpy( target->stretch, source->stretch, sizeof( target->stretch ) );
		target->hasTurbulence = source->hasTurbulence;
		target->hasStretch = source->hasStretch;
		target->tcModCount = source->sourceTcModCount;
		for ( uint32_t op = 0u; op < source->sourceTcModCount; ++op ) {
			target->tcMods[op].type = (uint32_t)source->sourceTcMods[op].type;
			memcpy( target->tcMods[op].parameters,
				source->sourceTcMods[op].parameters,
				sizeof( target->tcMods[op].parameters ) );
		}
		if ( source->imageSource != RENDER_MATERIAL_STAGE_IMAGE ) continue;
		if ( !source->imageName[0] ) return qfalse;
		target->material = resolve( context, source, source->imageName );
		if ( target->material <= 0 ) return qfalse;
		target->animationFrameCount = source->animationFrameCount;
		target->animationFrequency = source->animationFrequency;
		target->animationLooping = source->animationLooping;
		for ( uint32_t frame = 0u; frame < source->animationFrameCount; ++frame ) {
			const char *name = entry->animationFrameNames[source->animationFrameOffset + frame];
			if ( !name[0] ) return qfalse;
			target->animationMaterials[frame] = resolve( context, source, name );
			if ( target->animationMaterials[frame] <= 0 ) return qfalse;
		}
	}
	memcpy( outStages, stages, sizeof( stages ) );
	return qtrue;
}

qboolean RenderMaterialScript_CompileSource(
		const renderMaterialScriptEntry_t *entry, const refimport_t *imports,
		uint64_t materialGeneration, ralMaterialSourceReceipt_t *outReceipt,
		ralMaterialSourceDiagnostic_t *outDiagnostic ) {
	ralMaterialSourceIr_t source;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !entry || !imports || !imports->FS_ResolveResource || !outReceipt
			|| !materialGeneration || !entry->declarationSourceId
			|| !entry->declarationGeneration
			|| entry->stageCount > RENDER_MATERIAL_SCRIPT_MAX_STAGES
			|| entry->animationPathCount
				> RENDER_MATERIAL_SCRIPT_MAX_ANIMATION_PATHS
			|| entry->deformCount > RAL_MATERIAL_SOURCE_MAX_DEFORMS ) return qfalse;
	memset( &source, 0, sizeof( source ) );
	source.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	source.generation = materialGeneration;
	source.provenance = RAL_MATERIAL_PROVENANCE_Q3_SHADER;
	source.declaration.sourceId = entry->declarationSourceId;
	source.declaration.line = entry->declarationLine;
	source.declaration.column = entry->declarationColumn;
	(void)snprintf( source.semanticName, sizeof( source.semanticName ), "%s",
		entry->name );
	source.surfaceFlags = entry->surfaceFlags;
	source.contentFlags = entry->contentFlags;
	source.clearSolid = entry->clearSolid;
	source.cullMode = (uint32_t)entry->cullMode;
	source.sort = entry->sort;
	source.sky = entry->sky;
	source.noDraw = entry->noDraw;
	source.polygonOffset = entry->polygonOffset;
	source.noPicMip = entry->noPicMip;
	source.noMipMaps = entry->noMipMaps;
	source.skyCloudHeight = entry->skyCloudHeight;
	source.skySource = entry->skySource;
	if ( !ResolveSkyBox( &source, imports, entry->skyBoxPrefix, qtrue,
			&source.skySource.outerBox, source.skySource.outerDependencies )
			|| !ResolveSkyBox( &source, imports, entry->skyInnerBoxPrefix, qfalse,
				&source.skySource.innerBox, source.skySource.innerDependencies ) )
		return qfalse;
	memcpy( source.fogColor, entry->fogColor, sizeof( source.fogColor ) );
	source.fogDepth = entry->fogDepth;
	source.fogType = entry->fogType;
	source.fogDensity = entry->fogDensity;
	source.fogFarClip = entry->fogFarClip;
	memcpy( source.diffuseReflectance, entry->lighting.diffuseReflectance,
		sizeof( source.diffuseReflectance ) );
	memcpy( source.emissiveRadiance, entry->lighting.emissiveRadiance,
		sizeof( source.emissiveRadiance ) );
	source.emissiveRange = entry->lighting.emissiveInfluenceRange;
	source.lightingFlags = entry->hasLighting ? 1u : 0u;
	source.pbrFactors.baseColor[0] = source.pbrFactors.baseColor[1] =
		source.pbrFactors.baseColor[2] = source.pbrFactors.baseColor[3] = 1.0f;
	source.pbrFactors.roughness = 1.0f;
	source.pbrFactors.normalScale = 1.0f;
	source.pbrFactors.occlusionStrength = 1.0f;
	source.pbrRenderPolicy.depthTest = qtrue;
	source.pbrRenderPolicy.depthWrite = qtrue;
	for ( uint32_t i = 0u; i < entry->stageCount; ++i ) {
		const renderMaterialScriptStage_t *input = &entry->stages[i];
		if ( input->animationFrameCount
				> RENDER_MATERIAL_SCRIPT_MAX_ANIMATION_FRAMES
				|| input->animationFrameOffset > entry->animationPathCount
				|| input->animationFrameCount
					> entry->animationPathCount - input->animationFrameOffset
				|| input->sourceTcModCount > RAL_MATERIAL_SOURCE_MAX_TCMODS )
			return qfalse;
		ralMaterialSourceStage_t *stage = &source.stages[source.stageCount];
		ralMaterialSourceSpan_t stageSpan = {
			entry->declarationSourceId, input->sourceLine, input->sourceColumn
		};
		stage->span = stageSpan;
		stage->mapKind = input->imageSource == RENDER_MATERIAL_STAGE_LIGHTMAP
			? RAL_MATERIAL_MAP_LIGHTMAP : input->imageSource == RENDER_MATERIAL_STAGE_WHITE
			? RAL_MATERIAL_MAP_WHITE : RAL_MATERIAL_MAP_IMAGE;
		stage->dependencyIndex = UINT32_MAX;
		if ( stage->mapKind == RAL_MATERIAL_MAP_IMAGE ) {
			if ( !ResolveMaterialDependency( &source, imports, input,
					input->imageName, stageSpan, &stage->dependencyIndex ) )
				return qfalse;
		}
		stage->tcGen = (ralMaterialSourceTcGen_t)input->tcGen;
		memcpy( stage->tcGenVectors, input->tcGenVectors,
			sizeof( stage->tcGenVectors ) );
		stage->sourceBlend = (uint32_t)input->sourceBlend;
		stage->destinationBlend = (uint32_t)input->destinationBlend;
		stage->alphaTest = (uint32_t)input->alphaTest;
		stage->alphaCutoff = input->alphaCutoff;
		stage->depthWrite = input->depthWrite
			|| ( i == 0u && entry->depthWrite );
		stage->depthFunc = input->depthFunc;
		stage->rgbGen = input->rgbGen;
		if ( stage->rgbGen == RAL_MATERIAL_COLOR_BAD ) {
			stage->rgbGen = input->sourceBlend == RENDER_MATERIAL_BLEND_ONE
				|| input->sourceBlend == RENDER_MATERIAL_BLEND_SRC_ALPHA
				? RAL_MATERIAL_COLOR_IDENTITY_LIGHTING
				: RAL_MATERIAL_COLOR_IDENTITY;
		}
		stage->alphaGen = input->alphaGen;
		memcpy( stage->constantColor, input->constantColor,
			sizeof( stage->constantColor ) );
		stage->rgbWave = input->rgbWave;
		stage->alphaWave = input->alphaWave;
		stage->rgbWave.span.sourceId = entry->declarationSourceId;
		stage->alphaWave.span.sourceId = entry->declarationSourceId;
		stage->portalRange = input->portalRange;
		stage->animationFrequency = input->animationFrequency;
		stage->animationLooping = input->animationLooping;
		stage->animationFrameCount = input->animationFrameCount;
		for ( uint32_t frame = 0u; frame < input->animationFrameCount; ++frame ) {
			if ( !ResolveMaterialDependency( &source, imports, input,
					entry->animationFrameNames[input->animationFrameOffset + frame],
					stageSpan,
					&stage->animationDependencyIndices[frame] ) ) return qfalse;
		}
		if ( input->sourceTcModCount ) {
			stage->tcModCount = input->sourceTcModCount;
			memcpy( stage->tcMods, input->sourceTcMods,
				input->sourceTcModCount * sizeof( stage->tcMods[0] ) );
			for ( uint32_t tcMod = 0u; tcMod < stage->tcModCount; ++tcMod )
				stage->tcMods[tcMod].span.sourceId = entry->declarationSourceId;
		} else {
		if ( input->scale[0] != 1.0f || input->scale[1] != 1.0f )
			if ( !AddTcMod( stage, RAL_MATERIAL_TCMOD_SCALE,
				input->scale, 2u, stageSpan ) ) return qfalse;
		if ( input->scroll[0] != 0.0f || input->scroll[1] != 0.0f )
			if ( !AddTcMod( stage, RAL_MATERIAL_TCMOD_SCROLL,
				input->scroll, 2u, stageSpan ) ) return qfalse;
		if ( input->rotateDegrees != 0.0f )
			if ( !AddTcMod( stage, RAL_MATERIAL_TCMOD_ROTATE,
				&input->rotateDegrees, 1u, stageSpan ) ) return qfalse;
		if ( input->hasTurbulence )
			if ( !AddTcMod( stage, RAL_MATERIAL_TCMOD_TURBULENCE,
				input->turbulence, 4u, stageSpan ) ) return qfalse;
		if ( input->hasStretch )
			if ( !AddTcMod( stage, RAL_MATERIAL_TCMOD_STRETCH,
				input->stretch, 4u, stageSpan ) ) return qfalse;
		if ( input->hasTransform )
			if ( !AddTcMod( stage, RAL_MATERIAL_TCMOD_TRANSFORM,
				input->transform, 6u, stageSpan ) ) return qfalse;
		}
		source.stageCount++;
	}
	source.deformCount = entry->deformCount;
	memcpy( source.deforms, entry->deforms,
		entry->deformCount * sizeof( source.deforms[0] ) );
	for ( uint32_t i = 0u; i < source.deformCount; ++i )
		source.deforms[i].span.sourceId = entry->declarationSourceId;
	return Ral_MaterialSourceCompile( &source, outReceipt, outDiagnostic );
}
