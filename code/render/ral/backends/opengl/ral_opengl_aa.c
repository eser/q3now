// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#include "ral_opengl_aa.h"
#include "ral_opengl_internal.h"
#include "../vulkan/renderer/smaa_area_texture.h"
#include "../vulkan/renderer/smaa_search_texture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#define RAL_GL_APIENTRY __stdcall
#else
#define RAL_GL_APIENTRY
#endif

typedef unsigned int aaName_t;
typedef unsigned int aaEnum_t;
typedef int aaInt_t;
typedef int aaSize_t;
typedef intptr_t aaSizePtr_t;
typedef intptr_t aaIntPtr_t;

enum {
	AA_GL_NO_ERROR = 0, AA_GL_TRIANGLES = 0x0004, AA_GL_CULL_FACE = 0x0B44, AA_GL_DEPTH_TEST = 0x0B71, AA_GL_BLEND = 0x0BE2,
	AA_GL_SCISSOR_TEST = 0x0C11, AA_GL_TEXTURE_2D = 0x0DE1, AA_GL_UNSIGNED_BYTE = 0x1401, AA_GL_COLOR = 0x1800,
	AA_GL_RED = 0x1903, AA_GL_NEAREST = 0x2600, AA_GL_LINEAR = 0x2601, AA_GL_TEXTURE_MAG_FILTER = 0x2800,
	AA_GL_TEXTURE_MIN_FILTER = 0x2801, AA_GL_TEXTURE_WRAP_S = 0x2802, AA_GL_TEXTURE_WRAP_T = 0x2803, AA_GL_RGBA8 = 0x8058,
	AA_GL_CLAMP_TO_EDGE = 0x812F, AA_GL_RG = 0x8227, AA_GL_R8 = 0x8229, AA_GL_RG8 = 0x822B, AA_GL_R32F = 0x822E,
	AA_GL_RGBA16F = 0x881A, AA_GL_WRITE_ONLY = 0x88B9, AA_GL_UNIFORM_BUFFER = 0x8A11, AA_GL_FRAGMENT_SHADER = 0x8B30,
	AA_GL_VERTEX_SHADER = 0x8B31, AA_GL_COMPILE_STATUS = 0x8B81, AA_GL_LINK_STATUS = 0x8B82, AA_GL_FRAMEBUFFER_COMPLETE = 0x8CD5,
	AA_GL_COLOR_ATTACHMENT0 = 0x8CE0, AA_GL_COLOR_ATTACHMENT1 = 0x8CE1, AA_GL_FRAMEBUFFER = 0x8D40, AA_GL_COMPUTE_SHADER = 0x91B9,
	AA_GL_DYNAMIC_STORAGE_BIT = 0x0100, AA_GL_TEXTURE_FETCH_BARRIER_BIT = 0x0008, AA_GL_SHADER_IMAGE_ACCESS_BARRIER_BIT = 0x0020
};

typedef struct {
	aaName_t ( RAL_GL_APIENTRY *CreateShader )( aaEnum_t );
	void ( RAL_GL_APIENTRY *ShaderSource )( aaName_t, aaSize_t, const char *const *, const aaInt_t * );
	void ( RAL_GL_APIENTRY *CompileShader )( aaName_t );
	void ( RAL_GL_APIENTRY *GetShaderiv )( aaName_t, aaEnum_t, aaInt_t * );
	void ( RAL_GL_APIENTRY *GetShaderInfoLog )( aaName_t, aaSize_t, aaSize_t *, char * );
	void ( RAL_GL_APIENTRY *DeleteShader )( aaName_t );
	aaName_t ( RAL_GL_APIENTRY *CreateProgram )( void );
	void ( RAL_GL_APIENTRY *AttachShader )( aaName_t, aaName_t );
	void ( RAL_GL_APIENTRY *LinkProgram )( aaName_t );
	void ( RAL_GL_APIENTRY *GetProgramiv )( aaName_t, aaEnum_t, aaInt_t * );
	void ( RAL_GL_APIENTRY *DeleteProgram )( aaName_t );
	void ( RAL_GL_APIENTRY *UseProgram )( aaName_t );
	void ( RAL_GL_APIENTRY *CreateTextures )( aaEnum_t, aaSize_t, aaName_t * );
	void ( RAL_GL_APIENTRY *TextureStorage2D )( aaName_t, aaSize_t, aaEnum_t, aaSize_t, aaSize_t );
	void ( RAL_GL_APIENTRY *TextureSubImage2D )( aaName_t, aaInt_t, aaInt_t, aaInt_t, aaSize_t, aaSize_t, aaEnum_t, aaEnum_t,
		const void * );
	void ( RAL_GL_APIENTRY *TextureParameteri )( aaName_t, aaEnum_t, aaInt_t );
	void ( RAL_GL_APIENTRY *DeleteTextures )( aaSize_t, const aaName_t * );
	void ( RAL_GL_APIENTRY *CreateBuffers )( aaSize_t, aaName_t * );
	void ( RAL_GL_APIENTRY *NamedBufferStorage )( aaName_t, aaSizePtr_t, const void *, unsigned int );
	void ( RAL_GL_APIENTRY *NamedBufferSubData )( aaName_t, aaIntPtr_t, aaSizePtr_t, const void * );
	void ( RAL_GL_APIENTRY *DeleteBuffers )( aaSize_t, const aaName_t * );
	void ( RAL_GL_APIENTRY *BindBufferBase )( aaEnum_t, unsigned int, aaName_t );
	void ( RAL_GL_APIENTRY *BindTextureUnit )( unsigned int, aaName_t );
	void ( RAL_GL_APIENTRY *BindImageTexture )( unsigned int, aaName_t, aaInt_t, unsigned char, aaInt_t, aaEnum_t, aaEnum_t );
	void ( RAL_GL_APIENTRY *DispatchCompute )( unsigned int, unsigned int, unsigned int );
	void ( RAL_GL_APIENTRY *MemoryBarrier )( unsigned int );
	void ( RAL_GL_APIENTRY *CreateSamplers )( aaSize_t, aaName_t * );
	void ( RAL_GL_APIENTRY *SamplerParameteri )( aaName_t, aaEnum_t, aaInt_t );
	void ( RAL_GL_APIENTRY *BindSampler )( unsigned int, aaName_t );
	void ( RAL_GL_APIENTRY *DeleteSamplers )( aaSize_t, const aaName_t * );
	void ( RAL_GL_APIENTRY *CreateFramebuffers )( aaSize_t, aaName_t * );
	void ( RAL_GL_APIENTRY *NamedFramebufferTexture )( aaName_t, aaEnum_t, aaName_t, aaInt_t );
	void ( RAL_GL_APIENTRY *NamedFramebufferDrawBuffers )( aaName_t, aaSize_t, const aaEnum_t * );
	aaEnum_t ( RAL_GL_APIENTRY *CheckNamedFramebufferStatus )( aaName_t, aaEnum_t );
	void ( RAL_GL_APIENTRY *DeleteFramebuffers )( aaSize_t, const aaName_t * );
	void ( RAL_GL_APIENTRY *BindFramebuffer )( aaEnum_t, aaName_t );
	void ( RAL_GL_APIENTRY *ClearNamedFramebufferfv )( aaName_t, aaEnum_t, aaInt_t, const float * );
	void ( RAL_GL_APIENTRY *CreateVertexArrays )( aaSize_t, aaName_t * );
	void ( RAL_GL_APIENTRY *BindVertexArray )( aaName_t );
	void ( RAL_GL_APIENTRY *DeleteVertexArrays )( aaSize_t, const aaName_t * );
	void ( RAL_GL_APIENTRY *DrawArrays )( aaEnum_t, aaInt_t, aaSize_t );
	void ( RAL_GL_APIENTRY *Viewport )( aaInt_t, aaInt_t, aaSize_t, aaSize_t );
	void ( RAL_GL_APIENTRY *Enable )( aaEnum_t );
	void ( RAL_GL_APIENTRY *Disable )( aaEnum_t );
	aaEnum_t ( RAL_GL_APIENTRY *GetError )( void );
} aaDispatch_t;

/* Same constants as the Vulkan, Metal and WebGPU renderers (vk_temporal_resolve.c, ral_metal_taa.mm, ral_webgpu_taa.c). */
#define WIRED_TAA_DEPTH_ABSOLUTE 0.125f
#define WIRED_TAA_DEPTH_RELATIVE 0.02f
#define WIRED_TAA_HISTORY_WEIGHT 0.875f
#define WIRED_TAA_QUALITY_FLAGS 7u
/* The catalog resolve linearises its device depth as zNear * zFar / (zNear + rd * (zFar - zNear)); the motion pass writes rd for
 * this far plane, so the pair reproduces the product's view distance exactly (the product projection itself has no far plane). */
#define WIRED_TAA_FAR 65536.0f

/* SMAA presets (index = quality): identical to the Vulkan, Metal and WebGPU tables. */
static const float kSmaaThreshold[] = { 0.0f, 0.15f, 0.10f, 0.10f, 0.05f };
static const int kSmaaSearchSteps[] = { 0, 4, 8, 16, 32 };
static const int kSmaaSearchStepsDiag[] = { 0, 0, 0, 8, 16 };
static const int kSmaaCornerRounding[] = { 0, 0, 0, 25, 25 };

enum { TEX_VELOCITY, TEX_VALIDITY, TEX_REVERSE_DEPTH, TEX_OUTPUT, TEX_HISTORY_COLOR0, TEX_HISTORY_COLOR1, TEX_HISTORY_DEPTH0,
	TEX_HISTORY_DEPTH1, TEX_COVERAGE, TAA_TEX_COUNT };
enum { SMAA_EDGES, SMAA_BLEND, SMAA_OUTPUT, SMAA_TARGET_COUNT };
enum { UBO_MOTION, UBO_BOOTSTRAP, UBO_RESOLVE, UBO_METRICS, UBO_COUNT };
enum { PROGRAM_MOTION, PROGRAM_BOOTSTRAP, PROGRAM_RESOLVE, PROGRAM_SMAA_EDGE, PROGRAM_SMAA_BLEND, PROGRAM_SMAA_RESOLVE, PROGRAM_COUNT };

typedef struct { float co[4], cf[4], cl[4], cu[4], cp[4], po[4], pf[4], pl[4], pu[4], pp[4], jitter[4]; uint32_t extent[4]; } motionParams_t;
typedef struct { uint32_t extent[2]; float zNear, zFar; } bootstrapParams_t;
typedef struct {
	uint32_t extent[2]; float currentJitterUv[2], previousJitterUv[2];
	float zNear, zFar, depthAbsolute, depthRelative, historyWeight;
	uint32_t historyReadIndex, qualityFlags, pad;
} resolveParams_t;
typedef char motionParamsStd140[sizeof( motionParams_t ) == 192u ? 1 : -1];
/* The resolve block's last member sits at offset 48 (std140 block size 64; the buffer is allocated rounded up to 16). */
typedef char resolveParamsStd140[sizeof( resolveParams_t ) == 56u ? 1 : -1];

/* Camera motion from the scene depth. Texel row 0 is the bottom of the GL image (NDC y = -1); every AA texture uses that same
 * orientation, so the velocity is a GL texture-space uv offset (current - previous) like the other backends' in theirs. The product
 * clip z is forward - near over w = forward with the default [-1, 1] clip range, so the window depth is d = 1 - near / (2 forward).
 * The jitter is removed before reconstruction: the velocity is the unjittered camera motion the resolve expects. */
static const char s_motionSource[] =
	"#version 460\n"
	"layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n"
	"layout(std140, binding = 0) uniform MotionParams { vec4 co, cf, cl, cu, cp, po, pf, pl, pu, pp, jitter; uvec4 extent; } p;\n"
	"layout(binding = 0) uniform sampler2D sceneDepth;\n"
	"layout(binding = 1) uniform sampler2D coverage;\n"
	"layout(binding = 0, rgba16f) uniform writeonly image2D velocity;\n"
	"layout(binding = 1, rgba16f) uniform writeonly image2D validity;\n"
	"layout(binding = 2, r32f) uniform writeonly image2D reverseDepth;\n"
	"void main() {\n"
	"	uvec2 gid = gl_GlobalInvocationID.xy;\n"
	"	if (gid.x >= p.extent.x || gid.y >= p.extent.y) return;\n"
	"	ivec2 texel = ivec2(gid);\n"
	"	float n = p.cp.z, f = p.cp.w;\n"
	"	float d = texelFetch(sceneDepth, texel, 0).r;\n"
	"	float forward = min(n / (2.0 * max(1.0 - d, 1.0e-7)), f);\n"
	"	float rd = clamp((n * f / forward - n) / (f - n), 0.0, 1.0);\n"
	"	vec2 ndc = (vec2(gid) + 0.5) / vec2(p.extent.xy) * 2.0 - 1.0 - p.jitter.xy;\n"
	"	vec3 world = p.co.xyz + p.cf.xyz * forward - p.cl.xyz * (ndc.x * forward / p.cp.x) + p.cu.xyz * (ndc.y * forward / p.cp.y);\n"
	"	vec3 delta = world - p.po.xyz;\n"
	"	float previousForward = dot(delta, p.pf.xyz);\n"
	"	bool reprojected = previousForward > 1.0e-3;\n"
	"	vec2 previous = reprojected ? vec2(-dot(delta, p.pl.xyz) * p.pp.x, dot(delta, p.pu.xyz) * p.pp.y) / previousForward : ndc;\n"
	"	float trust = clamp(texelFetch(coverage, texel, 0).r, 0.0, 1.0);\n"
	"	imageStore(velocity, texel, vec4((ndc - previous) * 0.5, 0.0, 0.0));\n"
	"	imageStore(validity, texel, vec4(reprojected ? trust : 0.0, 0.0, 0.0, 0.0));\n"
	"	imageStore(reverseDepth, texel, vec4(rd, 0.0, 0.0, 0.0));\n"
	"}\n";

struct ralOpenGlAa_s {
	aaDispatch_t gl;
	ralOpenGlAaSources_t sources;
	aaName_t programs[PROGRAM_COUNT];
	aaName_t taaTextures[TAA_TEX_COUNT];
	uint32_t taaWidth, taaHeight;
	aaName_t smaaTargets[SMAA_TARGET_COUNT], smaaFramebuffers[SMAA_TARGET_COUNT], smaaArea, smaaSearch;
	uint32_t smaaWidth, smaaHeight;
	int smaaQuality;
	aaName_t buffers[UBO_COUNT];
	aaName_t linearSampler, pointSampler, emptyVao;
};

static qboolean LoadDispatch( const ralOpenGlCore_t *core, aaDispatch_t *gl ) {
#define AA_LOAD( member, symbol )                                                    \
	do {                                                                         \
		ralOpenGlProc_t proc = RalOpenGl_CoreResolve( core, symbol );          \
		if ( !proc ) return qfalse;                                            \
		memcpy( &gl->member, &proc, sizeof( proc ) );                          \
	} while ( 0 )
	memset( gl, 0, sizeof( *gl ) );
	AA_LOAD( CreateShader, "glCreateShader" ); AA_LOAD( ShaderSource, "glShaderSource" );
	AA_LOAD( CompileShader, "glCompileShader" ); AA_LOAD( GetShaderiv, "glGetShaderiv" );
	AA_LOAD( GetShaderInfoLog, "glGetShaderInfoLog" ); AA_LOAD( DeleteShader, "glDeleteShader" );
	AA_LOAD( CreateProgram, "glCreateProgram" ); AA_LOAD( AttachShader, "glAttachShader" );
	AA_LOAD( LinkProgram, "glLinkProgram" ); AA_LOAD( GetProgramiv, "glGetProgramiv" );
	AA_LOAD( DeleteProgram, "glDeleteProgram" ); AA_LOAD( UseProgram, "glUseProgram" );
	AA_LOAD( CreateTextures, "glCreateTextures" ); AA_LOAD( TextureStorage2D, "glTextureStorage2D" );
	AA_LOAD( TextureSubImage2D, "glTextureSubImage2D" ); AA_LOAD( TextureParameteri, "glTextureParameteri" );
	AA_LOAD( DeleteTextures, "glDeleteTextures" ); AA_LOAD( CreateBuffers, "glCreateBuffers" );
	AA_LOAD( NamedBufferStorage, "glNamedBufferStorage" ); AA_LOAD( NamedBufferSubData, "glNamedBufferSubData" );
	AA_LOAD( DeleteBuffers, "glDeleteBuffers" ); AA_LOAD( BindBufferBase, "glBindBufferBase" );
	AA_LOAD( BindTextureUnit, "glBindTextureUnit" ); AA_LOAD( BindImageTexture, "glBindImageTexture" );
	AA_LOAD( DispatchCompute, "glDispatchCompute" ); AA_LOAD( MemoryBarrier, "glMemoryBarrier" );
	AA_LOAD( CreateSamplers, "glCreateSamplers" ); AA_LOAD( SamplerParameteri, "glSamplerParameteri" );
	AA_LOAD( BindSampler, "glBindSampler" ); AA_LOAD( DeleteSamplers, "glDeleteSamplers" );
	AA_LOAD( CreateFramebuffers, "glCreateFramebuffers" ); AA_LOAD( NamedFramebufferTexture, "glNamedFramebufferTexture" );
	AA_LOAD( NamedFramebufferDrawBuffers, "glNamedFramebufferDrawBuffers" );
	AA_LOAD( CheckNamedFramebufferStatus, "glCheckNamedFramebufferStatus" );
	AA_LOAD( DeleteFramebuffers, "glDeleteFramebuffers" ); AA_LOAD( BindFramebuffer, "glBindFramebuffer" );
	AA_LOAD( ClearNamedFramebufferfv, "glClearNamedFramebufferfv" ); AA_LOAD( CreateVertexArrays, "glCreateVertexArrays" );
	AA_LOAD( BindVertexArray, "glBindVertexArray" ); AA_LOAD( DeleteVertexArrays, "glDeleteVertexArrays" );
	AA_LOAD( DrawArrays, "glDrawArrays" ); AA_LOAD( Viewport, "glViewport" );
	AA_LOAD( Enable, "glEnable" ); AA_LOAD( Disable, "glDisable" ); AA_LOAD( GetError, "glGetError" );
#undef AA_LOAD
	return qtrue;
}

/* Compiles one stage; `defines` (specialization values) go right after the #version line, as the tonemap program does. */
static aaName_t CompileStage( const aaDispatch_t *gl, aaEnum_t stage, const char *source, uint32_t byteCount, const char *defines ) {
	const char *parts[3];
	aaInt_t lengths[3], ok = 0;
	const size_t versionBytes = strcspn( source, "\n" ) + 1u;
	aaName_t shader;
	if ( !source || !byteCount || versionBytes > byteCount ) return 0u;
	shader = gl->CreateShader( stage );
	if ( !shader ) return 0u;
	parts[0] = source; lengths[0] = (aaInt_t)versionBytes;
	parts[1] = defines ? defines : ""; lengths[1] = (aaInt_t)strlen( parts[1] );
	parts[2] = source + versionBytes; lengths[2] = (aaInt_t)( byteCount - versionBytes );
	gl->ShaderSource( shader, 3, parts, lengths );
	gl->CompileShader( shader );
	gl->GetShaderiv( shader, AA_GL_COMPILE_STATUS, &ok );
	if ( !ok ) {
		char log[1024];
		log[0] = '\0';
		gl->GetShaderInfoLog( shader, (aaSize_t)sizeof( log ), NULL, log );
		fprintf( stderr, "Wired native OpenGL RAL: anti-aliasing shader rejected: %s\n", log );
		gl->DeleteShader( shader );
		return 0u;
	}
	return shader;
}

static aaName_t LinkProgram( const aaDispatch_t *gl, aaName_t first, aaName_t second ) {
	aaName_t program = 0u;
	aaInt_t linked = 0;
	if ( first && ( program = gl->CreateProgram() ) != 0u ) {
		gl->AttachShader( program, first );
		if ( second ) gl->AttachShader( program, second );
		gl->LinkProgram( program );
		gl->GetProgramiv( program, AA_GL_LINK_STATUS, &linked );
		if ( !linked ) { gl->DeleteProgram( program ); program = 0u; }
	}
	if ( first ) gl->DeleteShader( first );
	if ( second ) gl->DeleteShader( second );
	return program;
}

static aaName_t ComputeProgram( const aaDispatch_t *gl, const char *source, uint32_t byteCount ) {
	return LinkProgram( gl, CompileStage( gl, AA_GL_COMPUTE_SHADER, source, byteCount, NULL ), 0u );
}

static aaName_t GraphicsProgram( const aaDispatch_t *gl, const ralOpenGlAaSource_t *vertex, const char *vertexDefines,
		const ralOpenGlAaSource_t *fragment, const char *fragmentDefines ) {
	aaName_t vs = CompileStage( gl, AA_GL_VERTEX_SHADER, (const char *)vertex->source, vertex->byteCount, vertexDefines );
	aaName_t fs = vs ? CompileStage( gl, AA_GL_FRAGMENT_SHADER, (const char *)fragment->source, fragment->byteCount,
		fragmentDefines ) : 0u;
	if ( !fs ) { if ( vs ) gl->DeleteShader( vs ); return 0u; }
	return LinkProgram( gl, vs, fs );
}

static aaName_t Texture( const aaDispatch_t *gl, aaEnum_t format, uint32_t width, uint32_t height, aaEnum_t filter ) {
	aaName_t texture = 0u;
	gl->CreateTextures( AA_GL_TEXTURE_2D, 1, &texture );
	if ( !texture ) return 0u;
	gl->TextureStorage2D( texture, 1, format, (aaSize_t)width, (aaSize_t)height );
	gl->TextureParameteri( texture, AA_GL_TEXTURE_MIN_FILTER, (aaInt_t)filter );
	gl->TextureParameteri( texture, AA_GL_TEXTURE_MAG_FILTER, (aaInt_t)filter );
	gl->TextureParameteri( texture, AA_GL_TEXTURE_WRAP_S, AA_GL_CLAMP_TO_EDGE );
	gl->TextureParameteri( texture, AA_GL_TEXTURE_WRAP_T, AA_GL_CLAMP_TO_EDGE );
	return texture;
}

static aaName_t Sampler( const aaDispatch_t *gl, aaEnum_t filter ) {
	aaName_t sampler = 0u;
	gl->CreateSamplers( 1, &sampler );
	if ( !sampler ) return 0u;
	gl->SamplerParameteri( sampler, AA_GL_TEXTURE_MIN_FILTER, (aaInt_t)filter );
	gl->SamplerParameteri( sampler, AA_GL_TEXTURE_MAG_FILTER, (aaInt_t)filter );
	gl->SamplerParameteri( sampler, AA_GL_TEXTURE_WRAP_S, AA_GL_CLAMP_TO_EDGE );
	gl->SamplerParameteri( sampler, AA_GL_TEXTURE_WRAP_T, AA_GL_CLAMP_TO_EDGE );
	return sampler;
}

static void DestroyTaaTextures( ralOpenGlAa_t *aa ) {
	for ( int i = 0; i < TAA_TEX_COUNT; ++i )
		if ( aa->taaTextures[i] ) aa->gl.DeleteTextures( 1, &aa->taaTextures[i] );
	memset( aa->taaTextures, 0, sizeof( aa->taaTextures ) );
	aa->taaWidth = aa->taaHeight = 0u;
}

static void DestroySmaaTargets( ralOpenGlAa_t *aa ) {
	for ( int i = 0; i < SMAA_TARGET_COUNT; ++i ) {
		if ( aa->smaaFramebuffers[i] ) aa->gl.DeleteFramebuffers( 1, &aa->smaaFramebuffers[i] );
		if ( aa->smaaTargets[i] ) aa->gl.DeleteTextures( 1, &aa->smaaTargets[i] );
	}
	memset( aa->smaaFramebuffers, 0, sizeof( aa->smaaFramebuffers ) );
	memset( aa->smaaTargets, 0, sizeof( aa->smaaTargets ) );
	aa->smaaWidth = aa->smaaHeight = 0u;
}

static qboolean PrepareTaa( ralOpenGlAa_t *aa, uint32_t width, uint32_t height ) {
	static const aaEnum_t formats[TAA_TEX_COUNT] = { AA_GL_RGBA16F, AA_GL_RGBA16F, AA_GL_R32F, AA_GL_RGBA16F, AA_GL_RGBA16F,
		AA_GL_RGBA16F, AA_GL_R32F, AA_GL_R32F, AA_GL_R8 };
	if ( !width || !height || width > 16384u || height > 16384u ) return qfalse;
	if ( aa->taaWidth == width && aa->taaHeight == height && aa->taaTextures[TEX_COVERAGE] ) return qtrue;
	DestroyTaaTextures( aa );
	if ( !aa->programs[PROGRAM_MOTION] ) {
		aa->programs[PROGRAM_MOTION] = ComputeProgram( &aa->gl, s_motionSource, (uint32_t)sizeof( s_motionSource ) - 1u );
		aa->programs[PROGRAM_BOOTSTRAP] = ComputeProgram( &aa->gl, (const char *)aa->sources.temporalBootstrap.source,
			aa->sources.temporalBootstrap.byteCount );
		aa->programs[PROGRAM_RESOLVE] = ComputeProgram( &aa->gl, (const char *)aa->sources.temporalResolve.source,
			aa->sources.temporalResolve.byteCount );
		if ( !aa->programs[PROGRAM_MOTION] || !aa->programs[PROGRAM_BOOTSTRAP] || !aa->programs[PROGRAM_RESOLVE] ) return qfalse;
	}
	for ( int i = 0; i < TAA_TEX_COUNT; ++i ) {
		aa->taaTextures[i] = Texture( &aa->gl, formats[i], width, height, AA_GL_LINEAR );
		if ( !aa->taaTextures[i] ) { DestroyTaaTextures( aa ); return qfalse; }
	}
	if ( aa->gl.GetError() != AA_GL_NO_ERROR ) { DestroyTaaTextures( aa ); return qfalse; }
	aa->taaWidth = width; aa->taaHeight = height;
	return qtrue;
}

static void Camera( float origin[4], float forward[4], float left[4], float up[4], float projection[4],
		const ralOpenGlAaCamera_t *camera ) {
	for ( int i = 0; i < 3; ++i ) {
		origin[i] = camera->origin[i]; forward[i] = camera->forward[i]; left[i] = camera->left[i]; up[i] = camera->up[i];
	}
	origin[3] = forward[3] = left[3] = up[3] = 0.0f;
	projection[0] = camera->projectionX; projection[1] = camera->projectionY;
	projection[2] = RAL_OPENGL_AA_NEAR; projection[3] = WIRED_TAA_FAR;
}

qboolean RalOpenGl_AaCreate( const ralOpenGlCore_t *core, const ralOpenGlAaSources_t *sources, ralOpenGlAa_t **out ) {
	static const aaSizePtr_t sizes[UBO_COUNT] = { sizeof( motionParams_t ), sizeof( bootstrapParams_t ),
		sizeof( resolveParams_t ), 4u * sizeof( float ) };
	ralOpenGlAa_t *aa;
	if ( !core || !sources || !out || *out ) return qfalse;
	aa = calloc( 1, sizeof( *aa ) );
	if ( !aa ) return qfalse;
	aa->sources = *sources;
	if ( !LoadDispatch( core, &aa->gl ) ) { free( aa ); return qfalse; }
	aa->gl.CreateBuffers( UBO_COUNT, aa->buffers );
	for ( int i = 0; i < UBO_COUNT; ++i ) {
		if ( !aa->buffers[i] ) goto fail;
		aa->gl.NamedBufferStorage( aa->buffers[i], ( sizes[i] + 15 ) & ~(aaSizePtr_t)15, NULL, AA_GL_DYNAMIC_STORAGE_BIT );
	}
	aa->linearSampler = Sampler( &aa->gl, AA_GL_LINEAR );   /* bicubic history taps and SMAA colour/blend/area fetches */
	aa->pointSampler = Sampler( &aa->gl, AA_GL_NEAREST );   /* SMAA edges and search */
	aa->gl.CreateVertexArrays( 1, &aa->emptyVao );          /* the SMAA vertex stages derive the triangle from gl_VertexID */
	if ( !aa->linearSampler || !aa->pointSampler || !aa->emptyVao || aa->gl.GetError() != AA_GL_NO_ERROR ) goto fail;
	*out = aa;
	return qtrue;
fail:
	RalOpenGl_AaDestroy( aa );
	return qfalse;
}

void RalOpenGl_AaDestroy( ralOpenGlAa_t *aa ) {
	if ( !aa ) return;
	DestroyTaaTextures( aa );
	DestroySmaaTargets( aa );
	if ( aa->smaaArea ) aa->gl.DeleteTextures( 1, &aa->smaaArea );
	if ( aa->smaaSearch ) aa->gl.DeleteTextures( 1, &aa->smaaSearch );
	for ( int i = 0; i < PROGRAM_COUNT; ++i ) if ( aa->programs[i] ) aa->gl.DeleteProgram( aa->programs[i] );
	for ( int i = 0; i < UBO_COUNT; ++i ) if ( aa->buffers[i] ) aa->gl.DeleteBuffers( 1, &aa->buffers[i] );
	if ( aa->linearSampler ) aa->gl.DeleteSamplers( 1, &aa->linearSampler );
	if ( aa->pointSampler ) aa->gl.DeleteSamplers( 1, &aa->pointSampler );
	if ( aa->emptyVao ) aa->gl.DeleteVertexArrays( 1, &aa->emptyVao );
	free( aa );
}

uint32_t RalOpenGl_AaCoverageTexture( ralOpenGlAa_t *aa, uint32_t width, uint32_t height ) {
	return aa && PrepareTaa( aa, width, height ) ? aa->taaTextures[TEX_COVERAGE] : 0u;
}

qboolean RalOpenGl_AaAttachCoverage( ralOpenGlAa_t *aa, uint32_t framebuffer, uint32_t coverage ) {
	static const aaEnum_t both[2] = { AA_GL_COLOR_ATTACHMENT0, AA_GL_COLOR_ATTACHMENT1 };
	if ( !aa || !framebuffer || ( coverage && coverage != aa->taaTextures[TEX_COVERAGE] ) ) return qfalse;
	aa->gl.NamedFramebufferTexture( framebuffer, AA_GL_COLOR_ATTACHMENT1, coverage, 0 );
	aa->gl.NamedFramebufferDrawBuffers( framebuffer, coverage ? 2 : 1, both );
	if ( coverage && aa->gl.CheckNamedFramebufferStatus( framebuffer, AA_GL_FRAMEBUFFER ) != AA_GL_FRAMEBUFFER_COMPLETE )
		return qfalse;
	return aa->gl.GetError() == AA_GL_NO_ERROR;
}

qboolean RalOpenGl_AaTaa( ralOpenGlAa_t *aa, uint32_t sceneColor, uint32_t sceneDepth, const ralOpenGlAaTaaFrame_t *frame,
		uint32_t *outColor ) {
	const aaDispatch_t *gl;
	const unsigned int groupsX = frame ? ( frame->width + 7u ) / 8u : 0u, groupsY = frame ? ( frame->height + 7u ) / 8u : 0u;
	uint32_t read, write;
	if ( !aa || !sceneColor || !sceneDepth || !frame || !outColor || frame->readIndex > 1u || frame->writeIndex > 1u
			|| !PrepareTaa( aa, frame->width, frame->height ) ) return qfalse;
	gl = &aa->gl;
	read = frame->readIndex; write = frame->writeIndex;
	{
		motionParams_t m;
		memset( &m, 0, sizeof( m ) );
		Camera( m.co, m.cf, m.cl, m.cu, m.cp, &frame->current );
		Camera( m.po, m.pf, m.pl, m.pu, m.pp, frame->historyValid ? &frame->previous : &frame->current );
		m.jitter[0] = frame->jitterNdc[0]; m.jitter[1] = frame->jitterNdc[1];
		m.extent[0] = frame->width; m.extent[1] = frame->height;
		gl->NamedBufferSubData( aa->buffers[UBO_MOTION], 0, (aaSizePtr_t)sizeof( m ), &m );
	}
	gl->UseProgram( aa->programs[PROGRAM_MOTION] );
	gl->BindBufferBase( AA_GL_UNIFORM_BUFFER, 0u, aa->buffers[UBO_MOTION] );
	gl->BindTextureUnit( 0u, sceneDepth );
	gl->BindTextureUnit( 1u, aa->taaTextures[TEX_COVERAGE] );
	gl->BindImageTexture( 0u, aa->taaTextures[TEX_VELOCITY], 0, 0u, 0, AA_GL_WRITE_ONLY, AA_GL_RGBA16F );
	gl->BindImageTexture( 1u, aa->taaTextures[TEX_VALIDITY], 0, 0u, 0, AA_GL_WRITE_ONLY, AA_GL_RGBA16F );
	gl->BindImageTexture( 2u, aa->taaTextures[TEX_REVERSE_DEPTH], 0, 0u, 0, AA_GL_WRITE_ONLY, AA_GL_R32F );
	gl->DispatchCompute( groupsX, groupsY, 1u );
	gl->MemoryBarrier( AA_GL_TEXTURE_FETCH_BARRIER_BIT | AA_GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	if ( frame->historyValid ) {
		resolveParams_t r;
		memset( &r, 0, sizeof( r ) );
		r.extent[0] = frame->width; r.extent[1] = frame->height;
		r.currentJitterUv[0] = frame->currentJitterUv[0]; r.currentJitterUv[1] = frame->currentJitterUv[1];
		r.previousJitterUv[0] = frame->previousJitterUv[0]; r.previousJitterUv[1] = frame->previousJitterUv[1];
		r.zNear = RAL_OPENGL_AA_NEAR; r.zFar = WIRED_TAA_FAR;
		r.depthAbsolute = WIRED_TAA_DEPTH_ABSOLUTE; r.depthRelative = WIRED_TAA_DEPTH_RELATIVE;
		r.historyWeight = WIRED_TAA_HISTORY_WEIGHT; r.historyReadIndex = read; r.qualityFlags = WIRED_TAA_QUALITY_FLAGS;
		gl->NamedBufferSubData( aa->buffers[UBO_RESOLVE], 0, (aaSizePtr_t)sizeof( r ), &r );
		gl->UseProgram( aa->programs[PROGRAM_RESOLVE] );
		gl->BindBufferBase( AA_GL_UNIFORM_BUFFER, 10u, aa->buffers[UBO_RESOLVE] );
		gl->BindTextureUnit( 0u, sceneColor );
		gl->BindTextureUnit( 1u, aa->taaTextures[TEX_REVERSE_DEPTH] );
		gl->BindTextureUnit( 2u, aa->taaTextures[TEX_HISTORY_COLOR0 + read] );
		gl->BindTextureUnit( 3u, aa->taaTextures[TEX_HISTORY_DEPTH0 + read] );
		gl->BindTextureUnit( 4u, aa->taaTextures[TEX_VELOCITY] );
		gl->BindTextureUnit( 5u, aa->taaTextures[TEX_VALIDITY] );
		for ( unsigned int unit = 0u; unit < 6u; ++unit ) gl->BindSampler( unit, aa->linearSampler );
	} else {
		const bootstrapParams_t b = { { frame->width, frame->height }, RAL_OPENGL_AA_NEAR, WIRED_TAA_FAR };
		gl->NamedBufferSubData( aa->buffers[UBO_BOOTSTRAP], 0, (aaSizePtr_t)sizeof( b ), &b );
		gl->UseProgram( aa->programs[PROGRAM_BOOTSTRAP] );
		gl->BindBufferBase( AA_GL_UNIFORM_BUFFER, 6u, aa->buffers[UBO_BOOTSTRAP] );
		gl->BindTextureUnit( 0u, sceneColor );
		gl->BindTextureUnit( 1u, aa->taaTextures[TEX_REVERSE_DEPTH] );
		for ( unsigned int unit = 0u; unit < 2u; ++unit ) gl->BindSampler( unit, aa->linearSampler );
	}
	gl->BindImageTexture( 0u, aa->taaTextures[TEX_OUTPUT], 0, 0u, 0, AA_GL_WRITE_ONLY, AA_GL_RGBA16F );
	gl->BindImageTexture( 1u, aa->taaTextures[TEX_HISTORY_COLOR0 + write], 0, 0u, 0, AA_GL_WRITE_ONLY, AA_GL_RGBA16F );
	gl->BindImageTexture( 2u, aa->taaTextures[TEX_HISTORY_DEPTH0 + write], 0, 0u, 0, AA_GL_WRITE_ONLY, AA_GL_R32F );
	gl->DispatchCompute( groupsX, groupsY, 1u );
	/* The exposure histogram and the tonemap sample the output; the next frame's resolve samples the history. */
	gl->MemoryBarrier( AA_GL_TEXTURE_FETCH_BARRIER_BIT | AA_GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	/* Units 0 and 1 carry the product's base texture and lightmap, whose wrap mode lives on the texture: never leave a sampler bound. */
	for ( unsigned int unit = 0u; unit < 6u; ++unit ) gl->BindSampler( unit, 0u );
	if ( gl->GetError() != AA_GL_NO_ERROR ) return qfalse;
	*outColor = aa->taaTextures[TEX_OUTPUT];
	return qtrue;
}

static qboolean PrepareSmaa( ralOpenGlAa_t *aa, uint32_t width, uint32_t height, int quality ) {
	const aaDispatch_t *gl = &aa->gl;
	if ( !width || !height || width > 16384u || height > 16384u ) return qfalse;
	if ( !aa->smaaArea ) {
		aa->smaaArea = Texture( gl, AA_GL_RG8, AREATEX_WIDTH, AREATEX_HEIGHT, AA_GL_LINEAR );
		aa->smaaSearch = Texture( gl, AA_GL_R8, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, AA_GL_NEAREST );
		if ( !aa->smaaArea || !aa->smaaSearch ) return qfalse;
		/* Uploaded unflipped: every SMAA pass renders and samples in GL texture space (row 0 at NDC y = -1). Both row pitches
		 * (320 and 64 bytes) satisfy the default 4-byte unpack alignment. */
		gl->TextureSubImage2D( aa->smaaArea, 0, 0, 0, AREATEX_WIDTH, AREATEX_HEIGHT, AA_GL_RG, AA_GL_UNSIGNED_BYTE, areaTexBytes );
		gl->TextureSubImage2D( aa->smaaSearch, 0, 0, 0, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, AA_GL_RED, AA_GL_UNSIGNED_BYTE,
			searchTexBytes );
	}
	if ( aa->smaaQuality != quality || !aa->programs[PROGRAM_SMAA_EDGE] ) {
		char edgeFragment[96], blendVertex[96], blendFragment[192];
		for ( int i = PROGRAM_SMAA_EDGE; i <= PROGRAM_SMAA_RESOLVE; ++i )
			if ( aa->programs[i] ) { gl->DeleteProgram( aa->programs[i] ); aa->programs[i] = 0u; }
		/* Specialization values per stage (WIRED_SPEC_N from the GLSL lowering): the edge fragment's id 0 is the float threshold, the
		 * blend stages' ids are the integer search steps, diagonal steps and corner rounding. */
		snprintf( edgeFragment, sizeof( edgeFragment ), "#define WIRED_SPEC_0 %.9g\n", (double)kSmaaThreshold[quality] );
		snprintf( blendVertex, sizeof( blendVertex ), "#define WIRED_SPEC_0 %d\n", kSmaaSearchSteps[quality] );
		snprintf( blendFragment, sizeof( blendFragment ), "#define WIRED_SPEC_0 %d\n#define WIRED_SPEC_1 %d\n#define WIRED_SPEC_2 %d\n",
			kSmaaSearchSteps[quality], kSmaaSearchStepsDiag[quality], kSmaaCornerRounding[quality] );
		aa->programs[PROGRAM_SMAA_EDGE] = GraphicsProgram( gl, &aa->sources.smaaEdgeVertex, NULL,
			&aa->sources.smaaEdgeFragment, edgeFragment );
		aa->programs[PROGRAM_SMAA_BLEND] = GraphicsProgram( gl, &aa->sources.smaaBlendVertex, blendVertex,
			&aa->sources.smaaBlendFragment, blendFragment );
		aa->programs[PROGRAM_SMAA_RESOLVE] = GraphicsProgram( gl, &aa->sources.smaaResolveVertex, NULL,
			&aa->sources.smaaResolveFragment, NULL );
		if ( !aa->programs[PROGRAM_SMAA_EDGE] || !aa->programs[PROGRAM_SMAA_BLEND] || !aa->programs[PROGRAM_SMAA_RESOLVE] )
			return qfalse;
		aa->smaaQuality = quality;
	}
	if ( aa->smaaWidth != width || aa->smaaHeight != height || !aa->smaaFramebuffers[SMAA_OUTPUT] ) {
		static const aaEnum_t formats[SMAA_TARGET_COUNT] = { AA_GL_RG8, AA_GL_RGBA8, AA_GL_RGBA16F };
		const float metrics[4] = { 1.0f / (float)width, 1.0f / (float)height, (float)width, (float)height };
		DestroySmaaTargets( aa );
		gl->CreateFramebuffers( SMAA_TARGET_COUNT, aa->smaaFramebuffers );
		for ( int i = 0; i < SMAA_TARGET_COUNT; ++i ) {
			aa->smaaTargets[i] = Texture( gl, formats[i], width, height, AA_GL_LINEAR );
			if ( !aa->smaaTargets[i] || !aa->smaaFramebuffers[i] ) { DestroySmaaTargets( aa ); return qfalse; }
			gl->NamedFramebufferTexture( aa->smaaFramebuffers[i], AA_GL_COLOR_ATTACHMENT0, aa->smaaTargets[i], 0 );
			if ( gl->CheckNamedFramebufferStatus( aa->smaaFramebuffers[i], AA_GL_FRAMEBUFFER ) != AA_GL_FRAMEBUFFER_COMPLETE ) {
				DestroySmaaTargets( aa ); return qfalse;
			}
		}
		gl->NamedBufferSubData( aa->buffers[UBO_METRICS], 0, (aaSizePtr_t)sizeof( metrics ), metrics );
		aa->smaaWidth = width; aa->smaaHeight = height;
	}
	return gl->GetError() == AA_GL_NO_ERROR;
}

static void SmaaPass( const ralOpenGlAa_t *aa, int target, int program, const aaName_t *textures, const aaName_t *samplers,
		unsigned int count, qboolean clear ) {
	static const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	const aaDispatch_t *gl = &aa->gl;
	gl->BindFramebuffer( AA_GL_FRAMEBUFFER, aa->smaaFramebuffers[target] );
	/* Edge and blend fragments discard where there is nothing to do: clear every frame so nothing stale survives. */
	if ( clear ) gl->ClearNamedFramebufferfv( aa->smaaFramebuffers[target], AA_GL_COLOR, 0, zero );
	gl->UseProgram( aa->programs[program] );
	for ( unsigned int unit = 0u; unit < count; ++unit ) {
		gl->BindTextureUnit( unit, textures[unit] );
		gl->BindSampler( unit, samplers[unit] );
	}
	gl->DrawArrays( AA_GL_TRIANGLES, 0, 3 );
}

static qboolean Smaa( ralOpenGlAa_t *aa, uint32_t sceneColor, uint32_t width, uint32_t height, int quality,
		qboolean intoFramebuffer, uint32_t framebuffer, uint32_t *outColor ) {
	const aaDispatch_t *gl;
	if ( !aa || !sceneColor || ( !intoFramebuffer && !outColor ) ) return qfalse;
	quality = quality >= 1 && quality <= 4 ? quality : 3;
	if ( !PrepareSmaa( aa, width, height, quality ) ) return qfalse;
	gl = &aa->gl;
	gl->Disable( AA_GL_BLEND ); gl->Disable( AA_GL_DEPTH_TEST ); gl->Disable( AA_GL_CULL_FACE ); gl->Disable( AA_GL_SCISSOR_TEST );
	gl->Viewport( 0, 0, (aaSize_t)width, (aaSize_t)height );
	gl->BindVertexArray( aa->emptyVao );
	gl->BindBufferBase( AA_GL_UNIFORM_BUFFER, 0u, aa->buffers[UBO_METRICS] );
	{
		/* Samplers as in the Vulkan, Metal and WebGPU renderers: colour linear, edges point, blend linear, area linear,
		 * search point. */
		const aaName_t edgeTextures[1] = { sceneColor }, edgeSamplers[1] = { aa->linearSampler };
		const aaName_t blendTextures[3] = { aa->smaaTargets[SMAA_EDGES], aa->smaaArea, aa->smaaSearch };
		const aaName_t blendSamplers[3] = { aa->pointSampler, aa->linearSampler, aa->pointSampler };
		const aaName_t resolveTextures[2] = { sceneColor, aa->smaaTargets[SMAA_BLEND] };
		const aaName_t resolveSamplers[2] = { aa->linearSampler, aa->linearSampler };
		SmaaPass( aa, SMAA_EDGES, PROGRAM_SMAA_EDGE, edgeTextures, edgeSamplers, 1u, qtrue );
		SmaaPass( aa, SMAA_BLEND, PROGRAM_SMAA_BLEND, blendTextures, blendSamplers, 3u, qtrue );
		if ( intoFramebuffer ) {
			/* The neighbourhood blend draws straight into the caller's framebuffer (the output attachment). */
			gl->BindFramebuffer( AA_GL_FRAMEBUFFER, framebuffer );
			gl->UseProgram( aa->programs[PROGRAM_SMAA_RESOLVE] );
			for ( unsigned int unit = 0u; unit < 2u; ++unit ) {
				gl->BindTextureUnit( unit, resolveTextures[unit] );
				gl->BindSampler( unit, resolveSamplers[unit] );
			}
			gl->DrawArrays( AA_GL_TRIANGLES, 0, 3 );
		} else
			SmaaPass( aa, SMAA_OUTPUT, PROGRAM_SMAA_RESOLVE, resolveTextures, resolveSamplers, 2u, qfalse );
	}
	for ( unsigned int unit = 0u; unit < 3u; ++unit ) gl->BindSampler( unit, 0u );
	gl->BindVertexArray( 0u );
	if ( gl->GetError() != AA_GL_NO_ERROR ) return qfalse;
	if ( outColor ) *outColor = intoFramebuffer ? 0u : aa->smaaTargets[SMAA_OUTPUT];
	return qtrue;
}

qboolean RalOpenGl_AaSmaa( ralOpenGlAa_t *aa, uint32_t sceneColor, uint32_t width, uint32_t height, int quality,
		uint32_t *outColor ) {
	return Smaa( aa, sceneColor, width, height, quality, qfalse, 0u, outColor );
}

qboolean RalOpenGl_AaSmaaInto( ralOpenGlAa_t *aa, uint32_t sceneColor, uint32_t width, uint32_t height, int quality,
		uint32_t framebuffer ) {
	return Smaa( aa, sceneColor, width, height, quality, qtrue, framebuffer, NULL );
}
