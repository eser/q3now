// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_product.h"
#include "ral_opengl_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "ral_opengl_product_shader_catalog.inc"

#if defined(_WIN32)
#define RAL_GL_APIENTRY __stdcall
#else
#define RAL_GL_APIENTRY
#endif

typedef unsigned int glName_t;
typedef unsigned int glEnum_t;
typedef int glInt_t;
typedef int glSize_t;
typedef char glChar_t;
typedef glName_t ( RAL_GL_APIENTRY *createShaderFn )( glEnum_t );
typedef void ( RAL_GL_APIENTRY *shaderSourceFn )( glName_t, glSize_t,
	const glChar_t *const *, const glInt_t * );
typedef void ( RAL_GL_APIENTRY *compileShaderFn )( glName_t );
typedef void ( RAL_GL_APIENTRY *getShaderivFn )( glName_t, glEnum_t, glInt_t * );
typedef void ( RAL_GL_APIENTRY *deleteShaderFn )( glName_t );
typedef glName_t ( RAL_GL_APIENTRY *createProgramFn )( void );
typedef void ( RAL_GL_APIENTRY *attachShaderFn )( glName_t, glName_t );
typedef void ( RAL_GL_APIENTRY *linkProgramFn )( glName_t );
typedef void ( RAL_GL_APIENTRY *getProgramivFn )( glName_t, glEnum_t, glInt_t * );
typedef void ( RAL_GL_APIENTRY *deleteProgramFn )( glName_t );
typedef void ( RAL_GL_APIENTRY *pixelStoreiFn )( glEnum_t, glInt_t );
typedef void ( RAL_GL_APIENTRY *readPixelsFn )( glInt_t, glInt_t, glSize_t,
	glSize_t, glEnum_t, glEnum_t, void * );
typedef glEnum_t ( RAL_GL_APIENTRY *getErrorFn )( void );

enum {
	GL_NO_ERROR_VALUE = 0,
	GL_VERTEX_SHADER_VALUE = 0x8b31,
	GL_FRAGMENT_SHADER_VALUE = 0x8b30,
	GL_COMPILE_STATUS_VALUE = 0x8b81,
	GL_LINK_STATUS_VALUE = 0x8b82,
	GL_PACK_ALIGNMENT_VALUE = 0x0d05,
	GL_RGB_VALUE = 0x1907,
	GL_UNSIGNED_BYTE_VALUE = 0x1401
};

typedef struct {
	createShaderFn CreateShader;
	shaderSourceFn ShaderSource;
	compileShaderFn CompileShader;
	getShaderivFn GetShaderiv;
	deleteShaderFn DeleteShader;
	createProgramFn CreateProgram;
	attachShaderFn AttachShader;
	linkProgramFn LinkProgram;
	getProgramivFn GetProgramiv;
	deleteProgramFn DeleteProgram;
	pixelStoreiFn PixelStorei;
	readPixelsFn ReadPixels;
	getErrorFn GetError;
} productDispatch_t;

struct ralOpenGlProduct_s {
	ralOpenGlCore_t *core;
	ralOpenGlCoreReceipt_t coreReceipt;
	productDispatch_t gl;
	uint64_t generation;
	uint64_t lastFrameGeneration;
	uint32_t outputWidth;
	uint32_t outputHeight;
	glName_t program;
};

static qboolean LoadDispatch( const ralOpenGlCore_t *core,
		productDispatch_t *gl ) {
#define LOAD_GL( member, symbol, type ) do { \
	ralOpenGlProc_t proc = RalOpenGl_CoreResolve( core, symbol ); \
	if ( !proc ) return qfalse; gl->member = (type)proc; \
} while ( 0 )
	memset( gl, 0, sizeof( *gl ) );
	LOAD_GL( CreateShader, "glCreateShader", createShaderFn );
	LOAD_GL( ShaderSource, "glShaderSource", shaderSourceFn );
	LOAD_GL( CompileShader, "glCompileShader", compileShaderFn );
	LOAD_GL( GetShaderiv, "glGetShaderiv", getShaderivFn );
	LOAD_GL( DeleteShader, "glDeleteShader", deleteShaderFn );
	LOAD_GL( CreateProgram, "glCreateProgram", createProgramFn );
	LOAD_GL( AttachShader, "glAttachShader", attachShaderFn );
	LOAD_GL( LinkProgram, "glLinkProgram", linkProgramFn );
	LOAD_GL( GetProgramiv, "glGetProgramiv", getProgramivFn );
	LOAD_GL( DeleteProgram, "glDeleteProgram", deleteProgramFn );
	LOAD_GL( PixelStorei, "glPixelStorei", pixelStoreiFn );
	LOAD_GL( ReadPixels, "glReadPixels", readPixelsFn );
	LOAD_GL( GetError, "glGetError", getErrorFn );
#undef LOAD_GL
	return qtrue;
}

static glName_t Compile( const productDispatch_t *gl, glEnum_t stage,
		const unsigned char *source, unsigned int sourceByteCount ) {
	glName_t shader = gl->CreateShader( stage );
	glInt_t compiled = 0;
	glInt_t sourceLength;
	const glChar_t *sourceText = (const glChar_t *)source;
	if ( !shader || !source || !sourceByteCount || sourceByteCount > INT_MAX )
		return 0u;
	sourceLength = (glInt_t)sourceByteCount;
	gl->ShaderSource( shader, 1, &sourceText, &sourceLength );
	gl->CompileShader( shader );
	gl->GetShaderiv( shader, GL_COMPILE_STATUS_VALUE, &compiled );
	if ( !compiled || gl->GetError() != GL_NO_ERROR_VALUE ) {
		gl->DeleteShader( shader ); return 0u;
	}
	return shader;
}

static qboolean FrameValid( const ralOpenGlProductFrameReceipt_t *receipt ) {
	return receipt && receipt->schemaVersion == RAL_OPENGL_PRODUCT_SCHEMA_VERSION
		&& receipt->backendType == RAL_BACKEND_OPENGL
		&& receipt->productGeneration && receipt->frameGeneration
		&& receipt->vertexShaderByteCount == ralOpenGlProductVertexByteCount
		&& receipt->fragmentShaderByteCount == ralOpenGlProductFragmentByteCount
		&& receipt->vertexShaderDigestLane0 == ralOpenGlProductVertexDigestLane0
		&& receipt->vertexShaderDigestLane1 == ralOpenGlProductVertexDigestLane1
		&& receipt->fragmentShaderDigestLane0 == ralOpenGlProductFragmentDigestLane0
		&& receipt->fragmentShaderDigestLane1 == ralOpenGlProductFragmentDigestLane1
		&& RalOpenGl_FrontendPlanReceiptExact( &receipt->plan, &receipt->plan )
		&& RalOpenGl_WorldReceiptExact( &receipt->native, &receipt->native )
		&& receipt->plan.planGeneration == receipt->frameGeneration
		&& receipt->native.generation == receipt->frameGeneration
		&& receipt->native.loweringDigest == receipt->plan.loweringDigest
		&& !receipt->unresolvedCount && !receipt->fallbackCount
		&& !receipt->fatalCount && receipt->ready == qtrue;
}

qboolean RalOpenGl_ProductFrameReceiptExact(
		const ralOpenGlProductFrameReceipt_t *a,
		const ralOpenGlProductFrameReceipt_t *b ) {
	return FrameValid( a ) && FrameValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean RalOpenGl_ProductCreate( ralOpenGlCore_t *core,
		const ralOpenGlCoreReceipt_t *coreReceipt, uint64_t generation,
		ralOpenGlProduct_t **outProduct ) {
	ralOpenGlProduct_t *product;
	glName_t vertex = 0u, fragment = 0u;
	glInt_t linked = 0;
	if ( !core || !coreReceipt || !generation || generation == UINT64_MAX
			|| !outProduct || !RalOpenGl_CoreMatchesReceipt( core, coreReceipt ) )
		return qfalse;
	product = calloc( 1u, sizeof( *product ) );
	if ( !product ) return qfalse;
	product->core = core; product->coreReceipt = *coreReceipt;
	product->generation = generation;
	if ( !LoadDispatch( core, &product->gl ) ) goto fail;
	vertex = Compile( &product->gl, GL_VERTEX_SHADER_VALUE,
		ralOpenGlProductVertexSource, ralOpenGlProductVertexByteCount );
	fragment = Compile( &product->gl, GL_FRAGMENT_SHADER_VALUE,
		ralOpenGlProductFragmentSource, ralOpenGlProductFragmentByteCount );
	if ( !vertex || !fragment ) goto fail;
	product->program = product->gl.CreateProgram();
	if ( !product->program ) goto fail;
	product->gl.AttachShader( product->program, vertex );
	product->gl.AttachShader( product->program, fragment );
	product->gl.LinkProgram( product->program );
	product->gl.GetProgramiv( product->program, GL_LINK_STATUS_VALUE, &linked );
	if ( !linked || product->gl.GetError() != GL_NO_ERROR_VALUE ) goto fail;
	product->gl.DeleteShader( fragment ); product->gl.DeleteShader( vertex );
	*outProduct = product;
	return qtrue;
fail:
	if ( product->program ) product->gl.DeleteProgram( product->program );
	if ( fragment ) product->gl.DeleteShader( fragment );
	if ( vertex ) product->gl.DeleteShader( vertex );
	free( product );
	return qfalse;
}

void RalOpenGl_ProductDestroy( ralOpenGlProduct_t *product ) {
	if ( !product ) return;
	if ( product->program ) product->gl.DeleteProgram( product->program );
	memset( product, 0, sizeof( *product ) ); free( product );
}

qboolean RalOpenGl_ProductSetOutputExtent( ralOpenGlProduct_t *product,
		uint32_t width, uint32_t height ) {
	if ( !product || !width || !height || width > 16384u || height > 16384u )
		return qfalse;
	product->outputWidth = width;
	product->outputHeight = height;
	return qtrue;
}

qboolean RalOpenGl_ProductReadbackRgb( ralOpenGlProduct_t *product,
		byte *outPixels, uint32_t byteCount ) {
	uint64_t required;
	if ( !product || !outPixels || !product->outputWidth
			|| !product->outputHeight ) return qfalse;
	required = (uint64_t)product->outputWidth * product->outputHeight * 3u;
	if ( required != byteCount || required > INT_MAX ) return qfalse;
	product->gl.PixelStorei( GL_PACK_ALIGNMENT_VALUE, 1 );
	product->gl.ReadPixels( 0, 0, (glSize_t)product->outputWidth,
		(glSize_t)product->outputHeight, GL_RGB_VALUE,
		GL_UNSIGNED_BYTE_VALUE, outPixels );
	return product->gl.GetError() == GL_NO_ERROR_VALUE ? qtrue : qfalse;
}

qboolean RalOpenGl_ProductRender( ralOpenGlProduct_t *product,
		const renderSubmissionState_t *frontend,
		const renderSubmissionReceipt_t *submission, uint64_t frameGeneration,
		ralOpenGlProductFrameReceipt_t *outReceipt ) {
	ralOpenGlProductFrameReceipt_t receipt;
	ralOpenGlWorldLowerInfo_t lower;
	if ( !product || !frontend || !submission || !outReceipt
			|| !frameGeneration || frameGeneration == UINT64_MAX
			|| !product->outputWidth || !product->outputHeight
			|| frameGeneration <= product->lastFrameGeneration
			|| !RalOpenGl_CoreMatchesReceipt( product->core,
				&product->coreReceipt ) ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_OPENGL_PRODUCT_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_OPENGL;
	receipt.productGeneration = product->generation;
	receipt.frameGeneration = frameGeneration;
	receipt.vertexShaderByteCount = ralOpenGlProductVertexByteCount;
	receipt.fragmentShaderByteCount = ralOpenGlProductFragmentByteCount;
	receipt.vertexShaderDigestLane0 = ralOpenGlProductVertexDigestLane0;
	receipt.vertexShaderDigestLane1 = ralOpenGlProductVertexDigestLane1;
	receipt.fragmentShaderDigestLane0 = ralOpenGlProductFragmentDigestLane0;
	receipt.fragmentShaderDigestLane1 = ralOpenGlProductFragmentDigestLane1;
	if ( !RalOpenGl_FrontendPlanBuild( &product->coreReceipt, frontend,
			submission, frameGeneration, &receipt.plan ) ) return qfalse;
	memset( &lower, 0, sizeof( lower ) );
	lower.generation = frameGeneration; lower.programName = product->program;
	lower.outputWidth = product->outputWidth;
	lower.outputHeight = product->outputHeight;
	if ( !RalOpenGl_WorldLower( product->core, &product->coreReceipt, frontend,
			submission, &receipt.plan, &lower, &receipt.native ) ) return qfalse;
	receipt.unresolvedCount = receipt.plan.unresolvedCount
		+ receipt.native.unresolvedCount;
	receipt.fallbackCount = receipt.plan.fallbackCount + receipt.native.fallbackCount;
	receipt.fatalCount = receipt.plan.fatalCount + receipt.native.fatalCount;
	receipt.ready = qtrue;
	if ( !FrameValid( &receipt ) ) return qfalse;
	product->lastFrameGeneration = frameGeneration;
	*outReceipt = receipt;
	return qtrue;
}
