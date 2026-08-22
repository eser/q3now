// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

static uint32_t copyCalls;
static VkBuffer capturedSrc, capturedDst;
static VkBufferCopy capturedCopy;

static VKAPI_ATTR void VKAPI_CALL CaptureCopyBuffer( VkCommandBuffer command,
		VkBuffer src, VkBuffer dst, uint32_t count,
		const VkBufferCopy *regions ) {
	(void)command;
	if ( count != 1u || !regions ) return;
	copyCalls++;
	capturedSrc = src;
	capturedDst = dst;
	capturedCopy = regions[0];
}

static void InitBuffer( ralBuffer_t *buffer, ralBackend_t *backend,
		uintptr_t native, uint64_t size, ralBufferUsage_t usage ) {
	memset( buffer, 0, sizeof( *buffer ) );
	buffer->backend = backend;
	buffer->buffer = (VkBuffer)native;
	buffer->size = size;
	buffer->usage = usage;
	Ral_BufferMapLifecycleInit( &buffer->mapLifecycle );
	Ral_QueueTransferLifecycleInit( &buffer->queueTransfer );
}

int main( void ) {
	ralBackend_t backend, otherBackend;
	ralCommandBuffer_t command;
	ralBuffer_t src, dst;
	ralBufferCopy_t copy, bad;

	memset( &backend, 0, sizeof( backend ) );
	memset( &otherBackend, 0, sizeof( otherBackend ) );
	backend.vk.CmdCopyBuffer = CaptureCopyBuffer;
	memset( &command, 0, sizeof( command ) );
	command.backend = &backend;
	command.cb = (VkCommandBuffer)(uintptr_t)0x10u;
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	InitBuffer( &src, &backend, 0x20u, 256u,
		RAL_BUFFER_TRANSFER_SRC | RAL_BUFFER_VERTEX );
	InitBuffer( &dst, &backend, 0x30u, 512u,
		RAL_BUFFER_TRANSFER_DST | RAL_BUFFER_INDEX );
	memset( &copy, 0, sizeof( copy ) );
	copy.srcOffset = 32u;
	copy.dstOffset = 96u;
	copy.size = 128u;

	CHECK( Ral_CmdCopyBufferExact( &command, &src, &dst, &copy ) );
	CHECK( copyCalls == 1u && capturedSrc == src.buffer
		&& capturedDst == dst.buffer && capturedCopy.srcOffset == 32u
		&& capturedCopy.dstOffset == 96u && capturedCopy.size == 128u );

#define REJECT(statement) do { bad = copy; statement; copyCalls = 0u; \
	CHECK( !Ral_CmdCopyBufferExact( &command, &src, &dst, &bad ) \
		&& copyCalls == 0u ); \
} while ( 0 )
	REJECT( bad.size = 0u );
	REJECT( bad.srcOffset = 200u );
	REJECT( bad.dstOffset = 400u );
	src.usage &= ~RAL_BUFFER_TRANSFER_SRC; REJECT( (void)0 );
	src.usage |= RAL_BUFFER_TRANSFER_SRC;
	dst.usage &= ~RAL_BUFFER_TRANSFER_DST; REJECT( (void)0 );
	dst.usage |= RAL_BUFFER_TRANSFER_DST;
	dst.backend = &otherBackend; REJECT( (void)0 ); dst.backend = &backend;
	src.legacyMapped = qtrue; REJECT( (void)0 ); src.legacyMapped = qfalse;
	command.lifecycle.state = RAL_COMMAND_IDLE; REJECT( (void)0 );
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	bad.srcOffset = 0u; bad.dstOffset = 64u; bad.size = 128u;
	copyCalls = 0u;
	CHECK( !Ral_CmdCopyBufferExact( &command, &src, &src, &bad )
		&& copyCalls == 0u );
#undef REJECT

	// Same-buffer copies remain legal when their ranges do not overlap.
	bad.srcOffset = 0u; bad.dstOffset = 128u; bad.size = 64u;
	src.usage |= RAL_BUFFER_TRANSFER_DST;
	CHECK( Ral_CmdCopyBufferExact( &command, &src, &src, &bad )
		&& copyCalls == 1u && capturedSrc == capturedDst );

	puts( "PASS Vulkan exact portable buffer copy" );
	return 0;
}
