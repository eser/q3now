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
} materialTokenizer_t;

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
				&& (unsigned char)*parser->cursor <= ' ' ) parser->cursor++;
		if ( parser->cursor + 1 < parser->end
				&& parser->cursor[0] == '/' && parser->cursor[1] == '/' ) {
			parser->cursor += 2;
			while ( parser->cursor < parser->end
					&& *parser->cursor != '\n' ) parser->cursor++;
			continue;
		}
		if ( parser->cursor + 1 < parser->end
				&& parser->cursor[0] == '/' && parser->cursor[1] == '*' ) {
			parser->cursor += 2;
			while ( parser->cursor + 1 < parser->end
					&& !( parser->cursor[0] == '*' && parser->cursor[1] == '/' ) )
				parser->cursor++;
			if ( parser->cursor + 1 >= parser->end ) return qfalse;
			parser->cursor += 2; continue;
		}
		break;
	}
	if ( parser->cursor >= parser->end ) return qfalse;
	if ( *parser->cursor == '{' || *parser->cursor == '}' ) {
		token[0] = *parser->cursor++; token[1] = '\0'; return qtrue;
	}
	if ( *parser->cursor == '"' ) {
		start = ++parser->cursor;
		while ( parser->cursor < parser->end && *parser->cursor != '"' )
			parser->cursor++;
		if ( parser->cursor >= parser->end
				|| !CopyToken( token, MAX_QPATH, start,
					(size_t)( parser->cursor - start ) ) ) return qfalse;
		parser->cursor++; return qtrue;
	}
	start = parser->cursor;
	while ( parser->cursor < parser->end
			&& (unsigned char)*parser->cursor > ' '
			&& *parser->cursor != '{' && *parser->cursor != '}' )
		parser->cursor++;
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
		const char *text, size_t size ) {
	materialTokenizer_t parser = { text, text + size };
	char token[MAX_QPATH], value[MAX_QPATH];
	while ( NextToken( &parser, token ) ) {
		renderMaterialScriptEntry_t entry;
		int depth = 0;
		int selectedStage = -1;
		int currentStage = -1;
		char skyPrefix[MAX_QPATH] = "";
		memset( &entry, 0, sizeof( entry ) );
		(void)snprintf( entry.name, sizeof( entry.name ), "%s", token );
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
		int bytes = snprintf( path, sizeof( path ), "scripts/%s", files[i] );
		if ( bytes <= 0 || bytes >= (int)sizeof( path ) ) continue;
		bytes = imports->FS_ReadFile( path, &text );
		if ( bytes > 0 && text && !ParseFile( catalog,
			(const char *)text, (size_t)bytes ) ) valid = qfalse;
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
