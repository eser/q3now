// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_opengl_core.h"
#include "ral_opengl_command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK( condition ) do { if ( !( condition ) ) { \
	fprintf( stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
		#condition ); return 1; } } while ( 0 )

typedef unsigned int fakeEnum_t;
typedef unsigned int fakeName_t;
typedef int fakeInt_t;
typedef int fakeSize_t;
typedef long fakeSizePtr_t;
typedef long fakeIntPtr_t;
typedef unsigned int fakeBits_t;
typedef unsigned char fakeBool_t;

enum {
	FAKE_CORE_PROFILE_BIT = 0x00000001,
	FAKE_NO_ERROR = 0,
	FAKE_MAX_OBJECTS = 32
};

typedef struct {
	fakeName_t name;
	unsigned char *bytes;
	size_t size;
} fakeBuffer_t;

static struct {
	int major;
	int minor;
	int profile;
	const char *missingProc;
	fakeEnum_t error;
	qboolean corruptCopy;
	qboolean failBarrier;
	qboolean failFence;
	fakeEnum_t waitResult;
	fakeBits_t lastBarrierBits;
	unsigned int barrierCount;
	unsigned int liveFenceCount;
	fakeName_t nextName;
	fakeBuffer_t buffers[FAKE_MAX_OBJECTS];
} s_gl;

static void ResetFake( void ) {
	unsigned int i;
	for ( i = 0u; i < FAKE_MAX_OBJECTS; ++i ) free( s_gl.buffers[i].bytes );
	memset( &s_gl, 0, sizeof( s_gl ) );
	s_gl.major = 4;
	s_gl.minor = 6;
	s_gl.profile = FAKE_CORE_PROFILE_BIT;
	s_gl.nextName = 10u;
	s_gl.waitResult = 0x911c;
}

static fakeBuffer_t *FindBuffer( fakeName_t name ) {
	unsigned int i;
	for ( i = 0u; i < FAKE_MAX_OBJECTS; ++i )
		if ( s_gl.buffers[i].name == name ) return &s_gl.buffers[i];
	return NULL;
}

static void FakeGetIntegerv( fakeEnum_t name, fakeInt_t *out ) {
	switch ( name ) {
	case 0x821B: *out = s_gl.major; break;
	case 0x821C: *out = s_gl.minor; break;
	case 0x9126: *out = s_gl.profile; break;
	case 0x8CDF: *out = 8; break;
	case 0x0D33: *out = 16384; break;
	case 0x8073: *out = 2048; break;
	case 0x88FF: *out = 2048; break;
	case 0x90EB: *out = 1024; break;
	case 0x8A34: *out = 256; break;
	case 0x90DF: *out = 16; break;
	case 0x8B4D: *out = 192; break;
	default: *out = 0; s_gl.error = 0x0500; break;
	}
}

static const unsigned char *FakeGetString( fakeEnum_t name ) {
	switch ( name ) {
	case 0x1F00: return (const unsigned char *)"Wired Fake Vendor";
	case 0x1F01: return (const unsigned char *)"Wired Fake OpenGL 4.6";
	case 0x1F02: return (const unsigned char *)"4.6 Core Fake";
	default: s_gl.error = 0x0500; return NULL;
	}
}

static fakeEnum_t FakeGetError( void ) {
	const fakeEnum_t error = s_gl.error;
	s_gl.error = FAKE_NO_ERROR;
	return error;
}

static void FakeCreateBuffers( fakeSize_t count, fakeName_t *names ) {
	int i;
	for ( i = 0; i < count; ++i ) {
		unsigned int slot;
		for ( slot = 0u; slot < FAKE_MAX_OBJECTS; ++slot ) {
			if ( s_gl.buffers[slot].name == 0u ) {
				s_gl.buffers[slot].name = s_gl.nextName++;
				names[i] = s_gl.buffers[slot].name;
				break;
			}
		}
		if ( slot == FAKE_MAX_OBJECTS ) { names[i] = 0u; s_gl.error = 0x0505; }
	}
}

static void FakeNamedBufferStorage( fakeName_t name, fakeSizePtr_t size,
		const void *data, fakeBits_t flags ) {
	fakeBuffer_t *buffer = FindBuffer( name );
	(void)flags;
	if ( !buffer || size <= 0 ) { s_gl.error = 0x0501; return; }
	buffer->bytes = (unsigned char *)calloc( 1u, (size_t)size );
	if ( !buffer->bytes ) { s_gl.error = 0x0505; return; }
	buffer->size = (size_t)size;
	if ( data ) memcpy( buffer->bytes, data, buffer->size );
}

static void *FakeMapNamedBufferRange( fakeName_t name, fakeIntPtr_t offset,
		fakeSizePtr_t size, fakeBits_t flags ) {
	fakeBuffer_t *buffer = FindBuffer( name );
	(void)flags;
	if ( !buffer || offset < 0 || size <= 0
			|| (size_t)offset + (size_t)size > buffer->size ) {
		s_gl.error = 0x0501; return NULL;
	}
	return buffer->bytes + offset;
}

static fakeBool_t FakeUnmapNamedBuffer( fakeName_t name ) {
	return FindBuffer( name ) ? 1u : 0u;
}

static void FakeCopyNamedBufferSubData( fakeName_t source, fakeName_t destination,
		fakeIntPtr_t sourceOffset, fakeIntPtr_t destinationOffset,
		fakeSizePtr_t size ) {
	fakeBuffer_t *src = FindBuffer( source );
	fakeBuffer_t *dst = FindBuffer( destination );
	if ( !src || !dst || sourceOffset < 0 || destinationOffset < 0 || size <= 0
			|| (size_t)sourceOffset + (size_t)size > src->size
			|| (size_t)destinationOffset + (size_t)size > dst->size ) {
		s_gl.error = 0x0501; return;
	}
	memcpy( dst->bytes + destinationOffset, src->bytes + sourceOffset, (size_t)size );
	if ( s_gl.corruptCopy ) dst->bytes[destinationOffset] ^= 0x80u;
}

static void FakeDeleteBuffers( fakeSize_t count, const fakeName_t *names ) {
	int i;
	for ( i = 0; i < count; ++i ) {
		fakeBuffer_t *buffer = FindBuffer( names[i] );
		if ( buffer ) { free( buffer->bytes ); memset( buffer, 0, sizeof( *buffer ) ); }
	}
}

static void FakeCreateTextures( fakeEnum_t target, fakeSize_t count,
		fakeName_t *names ) {
	int i;
	(void)target;
	for ( i = 0; i < count; ++i ) names[i] = s_gl.nextName++;
}

static void FakeTextureStorage2D( fakeName_t texture, fakeSize_t levels,
		fakeEnum_t format, fakeSize_t width, fakeSize_t height ) {
	if ( !texture || levels != 1 || format != 0x8058 || width != 4 || height != 4 )
		s_gl.error = 0x0501;
}

static void FakeDeleteTextures( fakeSize_t count, const fakeName_t *names ) {
	(void)count; (void)names;
}

static void FakeCreateSamplers( fakeSize_t count, fakeName_t *names ) {
	int i;
	for ( i = 0; i < count; ++i ) names[i] = s_gl.nextName++;
}

static void FakeSamplerParameteri( fakeName_t sampler, fakeEnum_t name,
		fakeInt_t value ) {
	if ( !sampler || ( name != 0x2801 && name != 0x2800 ) || value != 0x2601 )
		s_gl.error = 0x0501;
}

static void FakeDeleteSamplers( fakeSize_t count, const fakeName_t *names ) {
	(void)count; (void)names;
}

static void FakeFinish( void ) {}

static void FakeMemoryBarrier( fakeBits_t bits ) {
	s_gl.lastBarrierBits = bits;
	s_gl.barrierCount++;
	if ( s_gl.failBarrier ) s_gl.error = 0x0502;
}

static void *FakeFenceSync( fakeEnum_t condition, fakeBits_t flags ) {
	if ( s_gl.failFence || condition != 0x9117 || flags != 0u ) {
		s_gl.error = 0x0502;
		return NULL;
	}
	s_gl.liveFenceCount++;
	return (void *)(uintptr_t)( UINT64_C(0xf000) + s_gl.liveFenceCount );
}

static fakeEnum_t FakeClientWaitSync( void *sync, fakeBits_t flags,
		uint64_t timeout ) {
	(void)timeout;
	if ( !sync || flags != 1u || s_gl.liveFenceCount == 0u ) return 0x911d;
	return s_gl.waitResult;
}

static void FakeDeleteSync( void *sync ) {
	if ( sync && s_gl.liveFenceCount ) s_gl.liveFenceCount--;
}

#define RETURN_PROC( functionName ) return (ralOpenGlProc_t)( functionName )
static ralOpenGlProc_t FakeResolve( const char *name, void *userData ) {
	(void)userData;
	if ( s_gl.missingProc && !strcmp( name, s_gl.missingProc ) ) return NULL;
	if ( !strcmp( name, "glGetIntegerv" ) ) RETURN_PROC( FakeGetIntegerv );
	if ( !strcmp( name, "glGetString" ) ) RETURN_PROC( FakeGetString );
	if ( !strcmp( name, "glGetError" ) ) RETURN_PROC( FakeGetError );
	if ( !strcmp( name, "glCreateBuffers" ) ) RETURN_PROC( FakeCreateBuffers );
	if ( !strcmp( name, "glNamedBufferStorage" ) ) RETURN_PROC( FakeNamedBufferStorage );
	if ( !strcmp( name, "glMapNamedBufferRange" ) ) RETURN_PROC( FakeMapNamedBufferRange );
	if ( !strcmp( name, "glUnmapNamedBuffer" ) ) RETURN_PROC( FakeUnmapNamedBuffer );
	if ( !strcmp( name, "glCopyNamedBufferSubData" ) ) RETURN_PROC( FakeCopyNamedBufferSubData );
	if ( !strcmp( name, "glDeleteBuffers" ) ) RETURN_PROC( FakeDeleteBuffers );
	if ( !strcmp( name, "glCreateTextures" ) ) RETURN_PROC( FakeCreateTextures );
	if ( !strcmp( name, "glTextureStorage2D" ) ) RETURN_PROC( FakeTextureStorage2D );
	if ( !strcmp( name, "glDeleteTextures" ) ) RETURN_PROC( FakeDeleteTextures );
	if ( !strcmp( name, "glCreateSamplers" ) ) RETURN_PROC( FakeCreateSamplers );
	if ( !strcmp( name, "glSamplerParameteri" ) ) RETURN_PROC( FakeSamplerParameteri );
	if ( !strcmp( name, "glDeleteSamplers" ) ) RETURN_PROC( FakeDeleteSamplers );
	if ( !strcmp( name, "glFinish" ) ) RETURN_PROC( FakeFinish );
	if ( !strcmp( name, "glMemoryBarrier" ) ) RETURN_PROC( FakeMemoryBarrier );
	if ( !strcmp( name, "glFenceSync" ) ) RETURN_PROC( FakeFenceSync );
	if ( !strcmp( name, "glClientWaitSync" ) ) RETURN_PROC( FakeClientWaitSync );
	if ( !strcmp( name, "glDeleteSync" ) ) RETURN_PROC( FakeDeleteSync );
	return NULL;
}
#undef RETURN_PROC

static ralMemoryFailureEvent_t DeviceLoss( void ) {
	ralMemoryFailureEvent_t event;
	memset( &event, 0, sizeof( event ) );
	event.backendType = RAL_BACKEND_OPENGL;
	event.cause = RAL_MEMORY_FAILURE_DEVICE_LOST;
	event.memoryClass = RAL_ALLOCATION_DEVICE_LOCAL;
	event.residency = RAL_ALLOCATION_RESIDENCY_PERMANENT;
	event.criticality = RAL_MEMORY_CRITICALITY_REQUIRED;
	event.requestedBytes = 4096u;
	event.attempt = 1u;
	event.maxAttempts = 1u;
	event.liveParent = qtrue;
	return event;
}

static ralOpenGlCoreCreateInfo_t CreateInfo( uint64_t generation ) {
	ralOpenGlCoreCreateInfo_t info;
	memset( &info, 0, sizeof( info ) );
	info.generation = generation;
	info.contextIdentity = (uintptr_t)( UINT64_C(0xc0460000) + generation );
	info.resolveProc = FakeResolve;
	return info;
}

int main( void ) {
	ralOpenGlCore_t *core = NULL, *replacementCore = NULL;
	ralOpenGlCoreCreateInfo_t info;
	ralOpenGlCoreReceipt_t coreReceipt, beforeCore, replacementReceipt;
	ralBackendConformanceReceipt_t conformance, beforeConformance, replacement;
	ralBackendRecreateReceipt_t recreate;
	ralMemoryFailureEvent_t lossEvent;
	ralMemoryFailureReceipt_t loss;
	ralOpenGlCommand_t *command = NULL;
	ralOpenGlCommandReceipt_t recording, barrier, executable, stale;
	ralOpenGlSubmissionReceipt_t submitted, beforeSubmitted;
	ralOpenGlCompletionReceipt_t completion, beforeCompletion;

	ResetFake();
	memset( &coreReceipt, 0x5a, sizeof( coreReceipt ) );
	beforeCore = coreReceipt;
	info = CreateInfo( 0u );
	CHECK( !RalOpenGl_CoreCreate( &info, &core, &coreReceipt ) );
	CHECK( core == NULL && !memcmp( &coreReceipt, &beforeCore, sizeof( coreReceipt ) ) );

	info = CreateInfo( 7u );
	s_gl.minor = 5;
	CHECK( !RalOpenGl_CoreCreate( &info, &core, &coreReceipt ) );
	s_gl.minor = 6;
	s_gl.profile = 0;
	CHECK( !RalOpenGl_CoreCreate( &info, &core, &coreReceipt ) );
	s_gl.profile = FAKE_CORE_PROFILE_BIT;
	s_gl.missingProc = "glCreateBuffers";
	CHECK( !RalOpenGl_CoreCreate( &info, &core, &coreReceipt ) );
	s_gl.missingProc = NULL;

	CHECK( RalOpenGl_CoreCreate( &info, &core, &coreReceipt ) );
	CHECK( core && coreReceipt.backendType == RAL_BACKEND_OPENGL );
	CHECK( coreReceipt.versionMajor == 4u && coreReceipt.versionMinor == 6u );
	CHECK( coreReceipt.coreProfile == qtrue );
	CHECK( RalOpenGl_CoreReceiptExact( &coreReceipt, &coreReceipt ) );
	beforeCore = coreReceipt;
	beforeCore.versionMinor = 5u;
	CHECK( !RalOpenGl_CoreReceiptExact( &coreReceipt, &beforeCore ) );

	CHECK( RalOpenGl_CommandCreate( core, &coreReceipt, &command ) );
	CHECK( RalOpenGl_CommandBegin( command, &recording ) );
	stale = recording;
	s_gl.failBarrier = qtrue;
	CHECK( !RalOpenGl_CommandBarrier( command, &recording,
		RAL_BARRIER_COMPUTE_TO_GRAPHICS, &barrier ) );
	CHECK( s_gl.barrierCount == 1u );
	s_gl.failBarrier = qfalse;
	CHECK( RalOpenGl_CommandBarrier( command, &recording,
		RAL_BARRIER_COMPUTE_TO_GRAPHICS, &barrier ) );
	CHECK( barrier.barrierCount == 1u && s_gl.lastBarrierBits != 0u );
	CHECK( !RalOpenGl_CommandBarrier( command, &stale,
		RAL_BARRIER_ALL, &recording ) );
	CHECK( s_gl.barrierCount == 2u );
	CHECK( RalOpenGl_CommandEnd( command, &barrier, &executable ) );
	memset( &submitted, 0xa5, sizeof( submitted ) );
	beforeSubmitted = submitted;
	s_gl.failFence = qtrue;
	CHECK( !RalOpenGl_CommandSubmit( command, &executable, &submitted ) );
	CHECK( !memcmp( &submitted, &beforeSubmitted, sizeof( submitted ) ) );
	s_gl.failFence = qfalse;
	CHECK( RalOpenGl_CommandSubmit( command, &executable, &submitted ) );
	CHECK( submitted.submission.generation == 1u && s_gl.liveFenceCount == 1u );
	CHECK( !RalOpenGl_CommandBegin( command, &recording ) );
	memset( &completion, 0x5a, sizeof( completion ) );
	beforeCompletion = completion;
	s_gl.waitResult = 0x911b;
	CHECK( !RalOpenGl_CommandWait( command, &submitted, 1u, &completion ) );
	CHECK( !memcmp( &completion, &beforeCompletion, sizeof( completion ) )
		&& s_gl.liveFenceCount == 1u );
	s_gl.waitResult = 0x911c;
	CHECK( RalOpenGl_CommandWait( command, &submitted, UINT64_MAX, &completion ) );
	CHECK( completion.complete == qtrue && s_gl.liveFenceCount == 0u );
	CHECK( !RalOpenGl_CommandWait( command, &submitted, 0u, &completion ) );
	CHECK( RalOpenGl_CommandBegin( command, &recording ) );
	CHECK( recording.command.generation == 2u );
	CHECK( !RalOpenGl_CommandEnd( command, &stale, &executable ) );
	RalOpenGl_CommandDestroy( command );
	command = NULL;

	CHECK( RalOpenGl_OffscreenConformance( core, 4096u, &conformance ) );
	CHECK( conformance.backendType == RAL_BACKEND_OPENGL );
	CHECK( conformance.copiedByteCount == 4096u && conformance.copiedByteDigest );
	CHECK( Ral_BackendConformanceReceiptExact( &conformance, &conformance ) );
	beforeConformance = conformance;
	beforeConformance.copiedByteDigest++;
	CHECK( !Ral_BackendConformanceReceiptExact( &conformance, &beforeConformance ) );

	s_gl.corruptCopy = qtrue;
	memset( &beforeConformance, 0xa5, sizeof( beforeConformance ) );
	replacement = beforeConformance;
	CHECK( !RalOpenGl_OffscreenConformance( core, 1024u, &replacement ) );
	CHECK( !memcmp( &replacement, &beforeConformance, sizeof( replacement ) ) );
	s_gl.corruptCopy = qfalse;

	lossEvent = DeviceLoss();
	CHECK( RalOpenGl_CorePublishDeviceLoss( core, &lossEvent, &loss ) );
	CHECK( loss.action == RAL_MEMORY_RECOVERY_RECREATE_BACKEND );
	CHECK( !RalOpenGl_OffscreenConformance( core, 256u, &replacement ) );
	CHECK( !RalOpenGl_CorePublishDeviceLoss( core, &lossEvent, &loss ) );

	info = CreateInfo( 8u );
	CHECK( RalOpenGl_CoreCreate( &info, &replacementCore, &replacementReceipt ) );
	CHECK( RalOpenGl_OffscreenConformance( replacementCore, 4096u, &replacement ) );
	CHECK( Ral_BackendRecreateBuild( &conformance, &loss, &replacement, &recreate ) );
	CHECK( recreate.previous.backendGeneration == 7u );
	CHECK( recreate.replacement.backendGeneration == 8u );

	RalOpenGl_CoreDestroy( replacementCore );
	RalOpenGl_CoreDestroy( core );
	ResetFake();
	puts( "ral opengl core contract: ok" );
	return 0;
}
