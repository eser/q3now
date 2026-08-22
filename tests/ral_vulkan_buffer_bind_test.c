// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if ( !(expr) ) { \
	fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr ); return 1; \
} } while ( 0 )

static uint32_t vertexCalls, indexCalls;
static uint32_t capturedFirst, capturedCount;
static VkBuffer capturedVertex[2], capturedIndex;
static VkDeviceSize capturedVertexOffsets[2], capturedIndexOffset;
static VkIndexType capturedIndexType;

static VKAPI_ATTR void VKAPI_CALL CaptureVertex( VkCommandBuffer command,
		uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *buffers,
		const VkDeviceSize *offsets ) {
	uint32_t i;
	(void)command;
	vertexCalls++;
	capturedFirst = firstBinding;
	capturedCount = bindingCount;
	for ( i = 0u; i < bindingCount && i < 2u; ++i ) {
		capturedVertex[i] = buffers[i];
		capturedVertexOffsets[i] = offsets[i];
	}
}

static VKAPI_ATTR void VKAPI_CALL CaptureIndex( VkCommandBuffer command,
		VkBuffer buffer, VkDeviceSize offset, VkIndexType type ) {
	(void)command;
	indexCalls++;
	capturedIndex = buffer;
	capturedIndexOffset = offset;
	capturedIndexType = type;
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
	ralBuffer_t vertex0, vertex1, index, sentinel;
	ralBuffer_t *vertices[2];
	uint64_t offsets[2] = { 16u, 32u };

	memset( &backend, 0, sizeof( backend ) );
	memset( &otherBackend, 0, sizeof( otherBackend ) );
	backend.vk.CmdBindVertexBuffers = CaptureVertex;
	backend.vk.CmdBindIndexBuffer = CaptureIndex;
	memset( &command, 0, sizeof( command ) );
	command.backend = &backend;
	command.cb = (VkCommandBuffer)(uintptr_t)0x10u;
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	InitBuffer( &vertex0, &backend, 0x20u, 256u, RAL_BUFFER_VERTEX );
	InitBuffer( &vertex1, &backend, 0x30u, 512u, RAL_BUFFER_VERTEX );
	InitBuffer( &index, &backend, 0x40u, 256u, RAL_BUFFER_INDEX );
	InitBuffer( &sentinel, &backend, 0x50u, 64u,
		RAL_BUFFER_VERTEX | RAL_BUFFER_INDEX );
	vertices[0] = &vertex0;
	vertices[1] = &vertex1;

	CHECK( Ral_CmdBindVertexBuffersExact( &command, 2u, 2u, vertices, offsets ) );
	CHECK( vertexCalls == 1u && capturedFirst == 2u && capturedCount == 2u
		&& capturedVertex[0] == vertex0.buffer
		&& capturedVertex[1] == vertex1.buffer
		&& capturedVertexOffsets[0] == 16u && capturedVertexOffsets[1] == 32u
		&& command.boundVertexBuffers[2] == &vertex0
		&& command.boundVertexBuffers[3] == &vertex1 );
	CHECK( Ral_CmdBindIndexBufferExact( &command, &index, 12u,
		RAL_INDEX_UINT32 ) );
	CHECK( indexCalls == 1u && capturedIndex == index.buffer
		&& capturedIndexOffset == 12u
		&& capturedIndexType == VK_INDEX_TYPE_UINT32
		&& command.boundIndexBuffer == &index );

#define REJECT_VERTEX(statement) do { \
	command.boundVertexBuffers[2] = &sentinel; \
	command.boundVertexBuffers[3] = &sentinel; \
	vertexCalls = 0u; statement; \
	CHECK( !Ral_CmdBindVertexBuffersExact( &command, 2u, 2u, vertices, offsets ) \
		&& vertexCalls == 0u && command.boundVertexBuffers[2] == &sentinel \
		&& command.boundVertexBuffers[3] == &sentinel ); \
} while ( 0 )
	REJECT_VERTEX( vertex0.backend = &otherBackend );
	vertex0.backend = &backend;
	REJECT_VERTEX( vertex0.usage = RAL_BUFFER_INDEX );
	vertex0.usage = RAL_BUFFER_VERTEX;
	REJECT_VERTEX( offsets[0] = vertex0.size );
	offsets[0] = 16u;
	REJECT_VERTEX( offsets[0] = 2u );
	offsets[0] = 16u;
	REJECT_VERTEX( vertex0.legacyMapped = qtrue );
	vertex0.legacyMapped = qfalse;
	REJECT_VERTEX( command.state = RAL_VK_CMD_IDLE );
	command.state = RAL_VK_CMD_RECORDING;
	vertexCalls = 0u;
	CHECK( !Ral_CmdBindVertexBuffersExact( &command, 15u, 2u,
		vertices, offsets ) && vertexCalls == 0u );
	CHECK( !Ral_CmdBindVertexBuffersExact( &command, 0u, 0u,
		vertices, offsets ) && vertexCalls == 0u );
#undef REJECT_VERTEX

#define REJECT_INDEX(statement) do { \
	command.boundIndexBuffer = &sentinel; indexCalls = 0u; statement; \
	CHECK( !Ral_CmdBindIndexBufferExact( &command, &index, 12u, \
		RAL_INDEX_UINT32 ) && indexCalls == 0u \
		&& command.boundIndexBuffer == &sentinel ); \
} while ( 0 )
	REJECT_INDEX( index.backend = &otherBackend );
	index.backend = &backend;
	REJECT_INDEX( index.usage = RAL_BUFFER_VERTEX );
	index.usage = RAL_BUFFER_INDEX;
	REJECT_INDEX( index.legacyMapped = qtrue );
	index.legacyMapped = qfalse;
	REJECT_INDEX( command.lifecycle.state = RAL_COMMAND_IDLE );
	command.lifecycle.state = RAL_COMMAND_RECORDING;
#undef REJECT_INDEX
	indexCalls = 0u;
	CHECK( !Ral_CmdBindIndexBufferExact( &command, &index, 2u,
		RAL_INDEX_UINT32 ) && indexCalls == 0u );
	CHECK( !Ral_CmdBindIndexBufferExact( &command, &index, index.size,
		RAL_INDEX_UINT32 ) && indexCalls == 0u );
	CHECK( !Ral_CmdBindIndexBufferExact( &command, &index, 12u,
		(ralIndexType_t)2 ) && indexCalls == 0u );
	CHECK( Ral_CmdBindIndexBufferExact( &command, &index, 6u,
		RAL_INDEX_UINT16 ) && indexCalls == 1u
		&& capturedIndexType == VK_INDEX_TYPE_UINT16 );

	// RAL-owned command buffers require both native and portable RECORDING
	// authority for every binding command.
	command.state = RAL_VK_CMD_RECORDING;
	command.lifecycle.state = RAL_COMMAND_RECORDING;
	vertexCalls = 0u;
	CHECK( Ral_CmdBindVertexBufferExact( &command, 0u, &vertex0, 0u )
		&& vertexCalls == 1u );

	puts( "PASS Vulkan exact portable vertex/index binding" );
	return 0;
}
