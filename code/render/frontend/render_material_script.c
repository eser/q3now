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
	const char *cursor;
	const char *end;
	uint32_t line;
	uint32_t column;
	uint32_t tokenLine;
	uint32_t tokenColumn;
} materialTokenizer_t;

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
	if ( *parser->cursor == '{' || *parser->cursor == '}' ) {
		token[0] = *parser->cursor;
		AdvanceTokenizer( parser );
		token[1] = '\0'; return qtrue;
	}
	if ( *parser->cursor == '"' ) {
		AdvanceTokenizer( parser );
		start = parser->cursor;
		while ( parser->cursor < parser->end && *parser->cursor != '"' )
			AdvanceTokenizer( parser );
		if ( parser->cursor >= parser->end
				|| !CopyToken( token, MAX_QPATH, start,
					(size_t)( parser->cursor - start ) ) ) return qfalse;
		AdvanceTokenizer( parser ); return qtrue;
	}
	start = parser->cursor;
	while ( parser->cursor < parser->end
			&& (unsigned char)*parser->cursor > ' '
			&& *parser->cursor != '{' && *parser->cursor != '}' )
		AdvanceTokenizer( parser );
	return CopyToken( token, MAX_QPATH, start,
		(size_t)( parser->cursor - start ) );
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
	if ( errno || !end || *end ) return qfalse;
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

static void StoreEntry( renderMaterialScriptCatalog_t *catalog,
		const renderMaterialScriptEntry_t *entry ) {
	uint32_t i;
	if ( !entry->name[0] || ( !entry->imageName[0] && !entry->sky
			&& !entry->noDraw && !entry->hasLighting ) ) return;
	for ( i = 0u; i < catalog->count; ++i ) {
		if ( !strcasecmp( catalog->entries[i].name, entry->name ) ) {
			catalog->entries[i] = *entry; return;
		}
	}
	if ( catalog->count < RENDER_MATERIAL_SCRIPT_MAX_ENTRIES )
		catalog->entries[catalog->count++] = *entry;
}

static qboolean ParseFile( renderMaterialScriptCatalog_t *catalog,
		const char *text, size_t size, const char *declarationPath,
		uint64_t declarationSourceId, uint64_t declarationSize,
		unsigned declarationGeneration ) {
	materialTokenizer_t parser = { text, text + size, 1u, 1u, 0u, 0u };
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
					if ( entry.stageCount < RENDER_MATERIAL_SCRIPT_MAX_STAGES ) {
						currentStage = (int)entry.stageCount++;
						stage = &entry.stages[currentStage];
						stage->sourceLine = parser.tokenLine;
						stage->sourceColumn = parser.tokenColumn;
						stage->imageSource = RENDER_MATERIAL_STAGE_IMAGE;
						stage->sourceBlend = RENDER_MATERIAL_BLEND_ONE;
						stage->destinationBlend = RENDER_MATERIAL_BLEND_ZERO;
						stage->alphaCutoff = 0.5f;
						stage->scale[0] = stage->scale[1] = 1.0f;
					}
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
				if ( !strcasecmp( value, "sky" ) ) entry.sky = qtrue;
				if ( !strcasecmp( value, "nodraw" )
						|| !strcasecmp( value, "skip" ) ) entry.noDraw = qtrue;
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
			if ( depth == 1 && !strcasecmp( token, "skyparms" ) ) {
				char cloudHeight[MAX_QPATH], ignored[MAX_QPATH];
				if ( !NextToken( &parser, value )
						|| !NextToken( &parser, cloudHeight )
						|| !NextToken( &parser, ignored ) ) return qfalse;
				if ( UsableImage( value ) )
					(void)snprintf( skyPrefix, sizeof( skyPrefix ), "%s", value );
				if ( !ParseFloatToken( cloudHeight, &entry.skyCloudHeight )
						|| entry.skyCloudHeight == 0.0f )
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
			} else if ( !strcasecmp( token, "animmap" ) ) {
				if ( !NextToken( &parser, value ) || !NextToken( &parser, value ) )
					return qfalse;
				if ( currentStage >= 0 && UsableImage( value ) )
					(void)snprintf( entry.stages[currentStage].imageName,
						sizeof( entry.stages[currentStage].imageName ), "%s", value );
				if ( !entry.imageName[0] && UsableImage( value ) ) {
					(void)snprintf( entry.imageName, sizeof( entry.imageName ),
						"%s", value );
					selectedStage = 0;
				} else if ( !entry.secondaryImageName[0] && UsableImage( value ) ) {
					(void)snprintf( entry.secondaryImageName,
						sizeof( entry.secondaryImageName ), "%s", value );
					selectedStage = 1;
				} else {
					selectedStage = -1;
				}
			} else if ( currentStage >= 0 && !strcasecmp( token, "tcmod" ) ) {
				char operation[MAX_QPATH], first[MAX_QPATH], second[MAX_QPATH];
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
				} else if ( !strcasecmp( operation, "stretch" ) ) {
					if ( currentStage < 0 || !NextToken( &parser, value ) ) return qfalse;
					for ( uint32_t component = 0u; component < 4u; ++component )
						if ( !NextToken( &parser, value )
								|| !ParseFloatToken( value,
									&entry.stages[currentStage].stretch[component] ) )
							return qfalse;
					entry.stages[currentStage].hasStretch = qtrue;
				} else if ( !strcasecmp( operation, "transform" ) ) {
					float matrix[6];
					for ( uint32_t component = 0u; component < ARRAY_LEN( matrix ); ++component ) {
						if ( !NextToken( &parser, value )
								|| !ParseFloatToken( value, &matrix[component] ) ) return qfalse;
					}
					/* The current portable material contract carries scale + offset.
					 * Preserve diagonal Q3 transforms exactly; leave shear/rotation for
					 * the forthcoming full stage-matrix contract instead of corrupting it. */
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
			} else if ( !strcasecmp( token, "alphafunc" ) ) {
				if ( !NextToken( &parser, value ) ) return qfalse;
				if ( currentStage >= 0 ) {
					entry.stages[currentStage].alphaTest = qtrue;
					entry.stages[currentStage].alphaCutoff =
						!strcasecmp( value, "ge128" ) ? 0.5f : 0.0f;
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
		StoreEntry( catalog, &entry );
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
		if ( bytes > 0 && text && !ParseFile( catalog,
			(const char *)text, (size_t)bytes, canonicalPath, sourceId,
			sourceSize, sourceGeneration ) ) valid = qfalse;
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

qboolean RenderMaterialScript_CompileSource(
		const renderMaterialScriptEntry_t *entry, const refimport_t *imports,
		uint64_t materialGeneration, ralMaterialSourceReceipt_t *outReceipt,
		ralMaterialSourceDiagnostic_t *outDiagnostic ) {
	ralMaterialSourceIr_t source;
	if ( outDiagnostic ) memset( outDiagnostic, 0, sizeof( *outDiagnostic ) );
	if ( !entry || !imports || !imports->FS_ResolveResource || !outReceipt
			|| !materialGeneration || !entry->declarationSourceId
			|| !entry->declarationGeneration ) return qfalse;
	memset( &source, 0, sizeof( source ) );
	source.schemaVersion = RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION;
	source.generation = materialGeneration;
	source.provenance = RAL_MATERIAL_PROVENANCE_Q3_SHADER;
	source.declaration.sourceId = entry->declarationSourceId;
	source.declaration.line = entry->declarationLine;
	source.declaration.column = entry->declarationColumn;
	(void)snprintf( source.semanticName, sizeof( source.semanticName ), "%s",
		entry->name );
	source.cullMode = (uint32_t)entry->cullMode;
	source.sort = entry->sort;
	source.sky = entry->sky;
	source.noDraw = entry->noDraw;
	source.skyCloudHeight = entry->skyCloudHeight;
	memcpy( source.diffuseReflectance, entry->lighting.diffuseReflectance,
		sizeof( source.diffuseReflectance ) );
	memcpy( source.emissiveRadiance, entry->lighting.emissiveRadiance,
		sizeof( source.emissiveRadiance ) );
	source.emissiveRange = entry->lighting.emissiveInfluenceRange;
	source.lightingFlags = entry->hasLighting ? 1u : 0u;
	for ( uint32_t i = 0u; i < entry->stageCount; ++i ) {
		const renderMaterialScriptStage_t *input = &entry->stages[i];
		ralMaterialSourceStage_t *stage = &source.stages[source.stageCount];
		ralMaterialSourceSpan_t stageSpan = {
			entry->declarationSourceId, input->sourceLine, input->sourceColumn
		};
		uint64_t optionsHash = MaterialDependencyOptions( input );
		stage->span = stageSpan;
		stage->mapKind = input->imageSource == RENDER_MATERIAL_STAGE_LIGHTMAP
			? RAL_MATERIAL_MAP_LIGHTMAP : input->imageSource == RENDER_MATERIAL_STAGE_WHITE
			? RAL_MATERIAL_MAP_WHITE : RAL_MATERIAL_MAP_IMAGE;
		stage->dependencyIndex = UINT32_MAX;
		if ( stage->mapKind == RAL_MATERIAL_MAP_IMAGE ) {
			char canonical[MAX_QPATH];
			uint64_t sourceId = 0u, byteSize = 0u;
			unsigned fsGeneration = 0u;
			uint32_t dependencyIndex;
			if ( !input->imageName[0] || !imports->FS_ResolveResource(
					input->imageName, canonical, sizeof( canonical ), &sourceId,
					&byteSize, &fsGeneration ) ) return qfalse;
			for ( dependencyIndex = 0u;
					dependencyIndex < source.dependencyCount; ++dependencyIndex ) {
				ralMaterialSourceDependency_t *dependency =
					&source.dependencies[dependencyIndex];
				if ( dependency->sourceId == sourceId
						&& dependency->creationOptionsHash == optionsHash ) break;
			}
			if ( dependencyIndex == source.dependencyCount ) {
				ralMaterialSourceDependency_t *dependency;
				if ( source.dependencyCount >= RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES )
					return qfalse;
				dependency = &source.dependencies[source.dependencyCount++];
				dependency->role = RAL_MATERIAL_DEPENDENCY_TEXTURE;
				dependency->sourceId = sourceId;
				dependency->fsGeneration = fsGeneration;
				dependency->size = byteSize;
				dependency->creationOptionsHash = optionsHash;
				dependency->span = stageSpan;
				(void)snprintf( dependency->canonicalPath,
					sizeof( dependency->canonicalPath ), "%s", canonical );
			}
			stage->dependencyIndex = dependencyIndex;
		}
		stage->tcGen = (ralMaterialSourceTcGen_t)input->tcGen;
		stage->sourceBlend = (uint32_t)input->sourceBlend;
		stage->destinationBlend = (uint32_t)input->destinationBlend;
		stage->alphaTest = (uint32_t)input->alphaTest;
		stage->alphaCutoff = input->alphaCutoff;
		stage->depthWrite = i == 0u ? entry->depthWrite : qfalse;
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
		source.stageCount++;
	}
	return Ral_MaterialSourceCompile( &source, outReceipt, outDiagnostic );
}
