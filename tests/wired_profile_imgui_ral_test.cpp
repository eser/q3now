// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "imgui.h"
#include "ral.h"
#include "wired_profile_imgui_ral.h"
#include "shaders/wired_profile_imgui_spv.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#define CHECK(expression) do { \
	if ( !(expression) ) { \
		std::fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression ); \
		return 1; \
	} \
} while ( 0 )

struct ralBackend_s { int tag = 1; };
struct ralBuffer_s {
	uint32_t id = 0;
	ralBufferUsage_t usage = RAL_BUFFER_VERTEX;
	std::vector<unsigned char> bytes;
};
struct ralTexture_s { uint32_t id = 0, width = 0, height = 0; };
struct ralTextureView_s { uint32_t id = 0; };
struct ralSampler_s { uint32_t id = 0; };
struct ralBindGroupLayout_s { uint32_t id = 0; };
struct ralBindGroup_s {
	uint32_t id = 0;
	const ralTextureView_t *view = nullptr;
	const ralSampler_t *sampler = nullptr;
};
struct ralPipeline_s { uint32_t id = 0; ralFormat_t format = RAL_FORMAT_UNDEFINED; };
struct ralCommandBuffer_s { int tag = 1; };
struct ralFence_s { bool waited = false; };

namespace {

struct DrawCall {
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
};

struct FakeState {
	uint32_t nextId = 1;
	uint32_t bufferCreates = 0;
	uint32_t bufferDestroys = 0;
	uint32_t textureCreates = 0;
	uint32_t textureDestroys = 0;
	uint32_t bindGroupCreates = 0;
	uint32_t bindGroupDestroys = 0;
	uint32_t pipelineCreates = 0;
	uint32_t pipelineDestroys = 0;
	uint32_t pipelineBinds = 0;
	uint32_t vertexBinds = 0;
	uint32_t indexBinds = 0;
	uint32_t viewportSets = 0;
	uint32_t pushConstants = 0;
	uint32_t fenceWaits = 0;
	uint32_t fenceDestroys = 0;
	uint32_t fontWidth = 0;
	uint32_t fontHeight = 0;
	uint64_t uploadedBytes = 0;
	bool uploadedRgba = false;
	bool pipelineAbiValid = false;
	ralFormat_t lastPipelineFormat = RAL_FORMAT_UNDEFINED;
	ralBuffer_t *boundVertexBuffer = nullptr;
	ralBuffer_t *boundIndexBuffer = nullptr;
	std::vector<float> transforms;
	std::vector<uint32_t> boundGroups;
	std::vector<ralRect_t> scissors;
	std::vector<DrawCall> draws;
} fake;

uint32_t CommandMutationCount() {
	return fake.pipelineBinds + fake.vertexBinds + fake.indexBinds
		+ fake.viewportSets + fake.pushConstants
		+ static_cast<uint32_t>( fake.boundGroups.size() )
		+ static_cast<uint32_t>( fake.scissors.size() )
		+ static_cast<uint32_t>( fake.draws.size() );
}

ImDrawList *MakeList( int vertexCount, int indexCount ) {
	ImDrawList *list = new ImDrawList( ImGui::GetDrawListSharedData() );
	list->VtxBuffer.resize( vertexCount );
	list->IdxBuffer.resize( indexCount );
	for ( int i = 0; i < vertexCount; ++i ) {
		list->VtxBuffer[i].pos = ImVec2( static_cast<float>( i ), static_cast<float>( i ) );
		list->VtxBuffer[i].uv = ImVec2( 0.5f, 0.5f );
		list->VtxBuffer[i].col = IM_COL32_WHITE;
	}
	for ( int i = 0; i < indexCount; ++i ) list->IdxBuffer[i] = 0;
	return list;
}

ImDrawCmd DrawCommand( uint64_t textureId, unsigned int elemCount,
		unsigned int indexOffset, unsigned int vertexOffset, ImVec4 clip ) {
	ImDrawCmd command;
	command.TexRef = ImTextureRef( static_cast<ImTextureID>( textureId ) );
	command.ElemCount = elemCount;
	command.IdxOffset = indexOffset;
	command.VtxOffset = vertexOffset;
	command.ClipRect = clip;
	return command;
}

void SetIndices( ImDrawList *list, int offset,
		std::initializer_list<ImDrawIdx> values ) {
	int index = offset;
	for ( ImDrawIdx value : values ) list->IdxBuffer[index++] = value;
}

ImDrawData MakeDrawData( std::initializer_list<ImDrawList *> lists ) {
	ImDrawData data;
	data.Valid = true;
	data.DisplayPos = ImVec2( 0.0f, 0.0f );
	data.DisplaySize = ImVec2( 100.0f, 50.0f );
	data.FramebufferScale = ImVec2( 2.0f, 2.0f );
	for ( ImDrawList *list : lists ) {
		data.CmdLists.push_back( list );
		data.TotalVtxCount += list->VtxBuffer.Size;
		data.TotalIdxCount += list->IdxBuffer.Size;
	}
	data.CmdListsCount = data.CmdLists.Size;
	return data;
}

void ArbitraryCallback( const ImDrawList *, const ImDrawCmd * ) {}

bool RejectsAtomically( wiredProfileImGuiRalRenderer_t *renderer,
		const ImDrawData *data, ralCommandBuffer_t *commandBuffer,
		uint32_t width, uint32_t height ) {
	wiredProfileImGuiRalReceipt_t receipt;
	std::memset( &receipt, 0xa5, sizeof( receipt ) );
	const wiredProfileImGuiRalReceipt_t before = receipt;
	const uint32_t commandsBefore = CommandMutationCount();
	const uint32_t buffersBefore = fake.bufferCreates;
	const int result = WiredProfileImGuiRal_Record(
		renderer, data, commandBuffer, width, height, &receipt );
	return result == 0
		&& std::memcmp( &receipt, &before, sizeof( receipt ) ) == 0
		&& CommandMutationCount() == commandsBefore
		&& fake.bufferCreates == buffersBefore;
}

} // namespace

ralBuffer_t *Ral_CreateBuffer( ralBackend_t *, const ralBufferCreateInfo_t *ci ) {
	if ( !ci || ci->size == 0 || ci->memory != RAL_MEMORY_HOST_COHERENT ) return nullptr;
	ralBuffer_t *buffer = new ralBuffer_t;
	buffer->id = fake.nextId++;
	buffer->usage = ci->usage;
	buffer->bytes.resize( static_cast<size_t>( ci->size ) );
	fake.bufferCreates++;
	return buffer;
}

void Ral_DestroyBuffer( ralBuffer_t *buffer ) {
	if ( !buffer ) return;
	fake.bufferDestroys++;
	delete buffer;
}

void *Ral_MapBuffer( ralBuffer_t *buffer ) {
	return buffer && !buffer->bytes.empty() ? buffer->bytes.data() : nullptr;
}

void Ral_UnmapBuffer( ralBuffer_t * ) {}
void Ral_FlushBuffer( ralBuffer_t *, uint64_t, uint64_t ) {}

ralTexture_t *Ral_CreateTexture( ralBackend_t *, const ralTextureCreateInfo_t *ci ) {
	if ( !ci || ci->format != RAL_FORMAT_R8G8B8A8_UNORM || ci->width == 0 || ci->height == 0 ) return nullptr;
	ralTexture_t *texture = new ralTexture_t;
	texture->id = fake.nextId++;
	texture->width = ci->width;
	texture->height = ci->height;
	fake.fontWidth = ci->width;
	fake.fontHeight = ci->height;
	fake.textureCreates++;
	return texture;
}

void Ral_DestroyTexture( ralTexture_t *texture ) {
	if ( !texture ) return;
	fake.textureDestroys++;
	delete texture;
}

uint32_t Ral_GetTextureMipLevelCount( const ralTexture_t * ) { return 1; }

ralFence_t *Ral_TextureUploadAsync( ralTexture_t *texture,
		const ralTextureUploadDesc_t *upload ) {
	if ( !texture || !upload || !upload->data || upload->dataSize == 0
	  || !upload->suppressMipGeneration ) return nullptr;
	fake.uploadedBytes = upload->dataSize;
	fake.uploadedRgba = upload->dataSize
		== static_cast<uint64_t>( texture->width ) * texture->height * 4u;
	return new ralFence_t;
}

ralTextureView_t *Ral_CreateTextureView( ralBackend_t *,
		const ralTextureViewCreateInfo_t *ci ) {
	if ( !ci || !ci->texture ) return nullptr;
	ralTextureView_t *view = new ralTextureView_t;
	view->id = fake.nextId++;
	return view;
}

void Ral_DestroyTextureView( ralTextureView_t *view ) { delete view; }

ralSampler_t *Ral_CreateSampler( ralBackend_t *, const ralSamplerCreateInfo_t *ci ) {
	if ( !ci || ci->minFilter != RAL_FILTER_LINEAR || ci->magFilter != RAL_FILTER_LINEAR ) return nullptr;
	ralSampler_t *sampler = new ralSampler_t;
	sampler->id = fake.nextId++;
	return sampler;
}

void Ral_DestroySampler( ralSampler_t *sampler ) { delete sampler; }

ralBindGroupLayout_t *Ral_CreateBindGroupLayout( ralBackend_t *,
		const ralBindGroupLayoutCreateInfo_t *ci ) {
	if ( !ci || ci->numEntries != 1 || !ci->entries
	  || ci->entries[0].type != RAL_BIND_COMBINED_TEXTURE_SAMPLER ) return nullptr;
	ralBindGroupLayout_t *layout = new ralBindGroupLayout_t;
	layout->id = fake.nextId++;
	return layout;
}

void Ral_DestroyBindGroupLayout( ralBindGroupLayout_t *layout ) { delete layout; }

ralBindGroup_t *Ral_CreateBindGroup( ralBackend_t *, const ralBindGroupCreateInfo_t *ci ) {
	if ( !ci || !ci->layout || ci->numValues != 1 || !ci->values
	  || ci->values[0].type != RAL_BIND_COMBINED_TEXTURE_SAMPLER
	  || !ci->values[0].textureView || !ci->values[0].sampler ) return nullptr;
	ralBindGroup_t *group = new ralBindGroup_t;
	group->id = fake.nextId++;
	group->view = ci->values[0].textureView;
	group->sampler = ci->values[0].sampler;
	fake.bindGroupCreates++;
	return group;
}

void Ral_DestroyBindGroup( ralBindGroup_t *group ) {
	if ( !group ) return;
	fake.bindGroupDestroys++;
	delete group;
}

ralPipeline_t *Ral_CreateGraphicsPipeline( ralBackend_t *,
		const ralGraphicsPipelineCreateInfo_t *ci ) {
	#if IM_COL32_R_SHIFT == 0
	const ralFormat_t packedColorFormat = RAL_FORMAT_R8G8B8A8_UNORM;
	#else
	const ralFormat_t packedColorFormat = RAL_FORMAT_B8G8R8A8_UNORM;
	#endif
	if ( !ci || !ci->vertexSpirv || !ci->fragmentSpirv ) return nullptr;
	fake.pipelineAbiValid =
		ci->vertexSpirvSize == wired_profile_imgui_vert_spv_size
		&& ci->fragmentSpirvSize == wired_profile_imgui_frag_spv_size
		&& std::memcmp( ci->vertexSpirv, wired_profile_imgui_vert_spv,
			wired_profile_imgui_vert_spv_size ) == 0
		&& std::memcmp( ci->fragmentSpirv, wired_profile_imgui_frag_spv,
			wired_profile_imgui_frag_spv_size ) == 0
		&& ci->numVertexBindings == 1 && ci->vertexBindings
		&& ci->vertexBindings[0].binding == 0
		&& ci->vertexBindings[0].stride == sizeof( ImDrawVert )
		&& ci->vertexBindings[0].inputRate == RAL_VERTEX_INPUT_PER_VERTEX
		&& ci->numVertexAttributes == 3 && ci->vertexAttributes
		&& ci->vertexAttributes[0].location == 0 && ci->vertexAttributes[0].binding == 0
		&& ci->vertexAttributes[0].format == RAL_FORMAT_R32G32_SFLOAT
		&& ci->vertexAttributes[0].offset == offsetof( ImDrawVert, pos )
		&& ci->vertexAttributes[1].location == 1 && ci->vertexAttributes[1].binding == 0
		&& ci->vertexAttributes[1].format == RAL_FORMAT_R32G32_SFLOAT
		&& ci->vertexAttributes[1].offset == offsetof( ImDrawVert, uv )
		&& ci->vertexAttributes[2].location == 2 && ci->vertexAttributes[2].binding == 0
		&& ci->vertexAttributes[2].format == packedColorFormat
		&& ci->vertexAttributes[2].offset == offsetof( ImDrawVert, col )
		&& ci->topology == RAL_TOPOLOGY_TRIANGLE_LIST
		&& ci->raster.polygonMode == RAL_POLYGON_FILL
		&& ci->raster.cullMode == RAL_CULL_NONE
		&& ci->raster.frontFace == RAL_FRONT_FACE_CCW
		&& ci->raster.lineWidth == 1.0f
		&& !ci->depthStencil.depthTestEnable && !ci->depthStencil.depthWriteEnable
		&& ci->numColorBlends == 1 && ci->colorBlends
		&& ci->colorBlends[0].blendEnable
		&& ci->colorBlends[0].srcColor == RAL_BLEND_SRC_ALPHA
		&& ci->colorBlends[0].dstColor == RAL_BLEND_ONE_MINUS_SRC_ALPHA
		&& ci->colorBlends[0].colorOp == RAL_BLEND_OP_ADD
		&& ci->colorBlends[0].srcAlpha == RAL_BLEND_ONE
		&& ci->colorBlends[0].dstAlpha == RAL_BLEND_ONE_MINUS_SRC_ALPHA
		&& ci->colorBlends[0].alphaOp == RAL_BLEND_OP_ADD
		&& ci->colorBlends[0].writeMask == RAL_COLOR_WRITE_ALL
		&& ci->numColorFormats == 1 && ci->depthFormat == RAL_FORMAT_UNDEFINED
		&& ci->sampleCount == 1 && ci->numBindGroupLayouts == 1
		&& ci->bindGroupLayouts && ci->bindGroupLayouts[0]
		&& ci->pushConstantSize == 16 && ci->pushConstantStages == RAL_STAGE_VERTEX;
	if ( !fake.pipelineAbiValid ) return nullptr;
	ralPipeline_t *pipeline = new ralPipeline_t;
	pipeline->id = fake.nextId++;
	pipeline->format = ci->colorFormats[0];
	fake.lastPipelineFormat = ci->colorFormats[0];
	fake.pipelineCreates++;
	return pipeline;
}

void Ral_DestroyPipeline( ralPipeline_t *pipeline ) {
	if ( !pipeline ) return;
	fake.pipelineDestroys++;
	delete pipeline;
}

void Ral_WaitFence( ralFence_t *fence, uint64_t timeout ) {
	if ( fence && timeout == RAL_TIMEOUT_INFINITE ) {
		fence->waited = true;
		fake.fenceWaits++;
	}
}

void Ral_DestroyFence( ralFence_t *fence ) {
	if ( !fence ) return;
	fake.fenceDestroys++;
	delete fence;
}

void Ral_CmdBindPipeline( ralCommandBuffer_t *, ralPipeline_t *pipeline ) {
	if ( pipeline ) fake.pipelineBinds++;
}

void Ral_CmdBindBindGroup( ralCommandBuffer_t *, uint32_t setIndex, ralBindGroup_t *group ) {
	if ( setIndex == 0 && group ) fake.boundGroups.push_back( group->id );
}

void Ral_CmdBindVertexBuffer( ralCommandBuffer_t *, uint32_t binding,
		ralBuffer_t *buffer, uint64_t offset ) {
	if ( binding == 0 && buffer && offset == 0 && buffer->usage == RAL_BUFFER_VERTEX ) {
		fake.vertexBinds++;
		fake.boundVertexBuffer = buffer;
	}
}

void Ral_CmdBindIndexBuffer( ralCommandBuffer_t *, ralBuffer_t *buffer,
		uint64_t offset, ralIndexType_t type ) {
	if ( buffer && offset == 0 && buffer->usage == RAL_BUFFER_INDEX
	  && type == ( sizeof( ImDrawIdx ) == 2 ? RAL_INDEX_UINT16 : RAL_INDEX_UINT32 ) ) {
		fake.indexBinds++;
		fake.boundIndexBuffer = buffer;
	}
}

void Ral_CmdSetViewport( ralCommandBuffer_t *, const ralViewport_t *viewport ) {
	if ( viewport && viewport->width == 200.0f && viewport->height == 100.0f ) fake.viewportSets++;
}

void Ral_CmdPushConstants( ralCommandBuffer_t *, uint32_t stages,
		uint32_t offset, uint32_t size, const void *data ) {
	if ( stages == RAL_STAGE_VERTEX && offset == 0 && size == 16 && data ) {
		fake.pushConstants++;
		const float *values = static_cast<const float *>( data );
		fake.transforms.insert( fake.transforms.end(), values, values + 4 );
	}
}

void Ral_CmdSetScissor( ralCommandBuffer_t *, const ralRect_t *rect ) {
	if ( rect ) fake.scissors.push_back( *rect );
}

void Ral_CmdDrawIndexed( ralCommandBuffer_t *, uint32_t indexCount,
		uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
		uint32_t firstInstance ) {
	if ( instanceCount == 1 && firstInstance == 0 )
		fake.draws.push_back( { indexCount, firstIndex, vertexOffset } );
}

int main() {
	ralBackend_t backend;
	ralCommandBuffer_t commandBuffer;
	wiredProfileImGuiRalRenderer_t *renderer = reinterpret_cast<wiredProfileImGuiRalRenderer_t *>( 0x1 );
	wiredProfileImGuiRalReceipt_t receipt = {};
	uint64_t secondTextureId = 0;
	ImDrawList *first = nullptr, *second = nullptr, *large = nullptr;
	ImGuiContext *context;

	IMGUI_CHECKVERSION();
	context = ImGui::CreateContext();
	CHECK( context != nullptr );
	ImGuiIO &io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.Fonts->AddFontDefault();
	CHECK( WiredProfileImGuiRal_Create( &backend, RAL_FORMAT_B8G8R8A8_UNORM,
		context, &renderer ) == 1 );
	CHECK( renderer != nullptr && renderer != reinterpret_cast<wiredProfileImGuiRalRenderer_t *>( 0x1 ) );
	CHECK( io.BackendRendererUserData == renderer );
	CHECK( ( io.BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset ) != 0 );
	CHECK( ( io.BackendFlags & ImGuiBackendFlags_RendererHasTextures ) == 0 );
	CHECK( io.Fonts->TexRef.GetTexID() != ImTextureID_Invalid );
	CHECK( fake.fontWidth > 0 && fake.fontHeight > 0 && fake.uploadedRgba );
	CHECK( fake.uploadedBytes == static_cast<uint64_t>( fake.fontWidth ) * fake.fontHeight * 4u );
	CHECK( fake.fenceWaits == 1 && fake.fenceDestroys == 1 );
	CHECK( fake.pipelineAbiValid && fake.lastPipelineFormat == RAL_FORMAT_B8G8R8A8_UNORM );
	const uint32_t pipelinesAfterCreate = fake.pipelineCreates;
	CHECK( WiredProfileImGuiRal_EnsureColorFormat(
		renderer, RAL_FORMAT_B8G8R8A8_UNORM ) == 1 );
	CHECK( fake.pipelineCreates == pipelinesAfterCreate && fake.pipelineDestroys == 0 );
	CHECK( WiredProfileImGuiRal_EnsureColorFormat(
		renderer, RAL_FORMAT_R8G8B8A8_UNORM ) == 1 );
	CHECK( fake.lastPipelineFormat == RAL_FORMAT_R8G8B8A8_UNORM
	  && fake.pipelineCreates == pipelinesAfterCreate + 1 && fake.pipelineDestroys == 1 );
	CHECK( WiredProfileImGuiRal_EnsureColorFormat(
		renderer, RAL_FORMAT_B8G8R8A8_UNORM ) == 1 );
	CHECK( fake.lastPipelineFormat == RAL_FORMAT_B8G8R8A8_UNORM
	  && fake.pipelineCreates == pipelinesAfterCreate + 2 && fake.pipelineDestroys == 2 );

	ralTextureView_t callerView{ fake.nextId++ };
	ralSampler_t callerSampler{ fake.nextId++ };
	CHECK( WiredProfileImGuiRal_RegisterTexture( renderer, &callerView,
		&callerSampler, &secondTextureId ) == 1 );
	CHECK( secondTextureId != 0
	  && secondTextureId != static_cast<uint64_t>( io.Fonts->TexRef.GetTexID() ) );
	CHECK( fake.bindGroupCreates == 2 );

	const uint64_t fontTextureId = static_cast<uint64_t>( io.Fonts->TexRef.GetTexID() );
	first = MakeList( 8, 9 );
	SetIndices( first, 0, { 0, 1, 2, 0, 1, 2, 0, 1, 2 } );
	first->CmdBuffer.push_back( DrawCommand( fontTextureId, 3, 0, 1,
		ImVec4( -10.0f, -20.0f, 10.1f, 15.2f ) ) );
	first->CmdBuffer.push_back( DrawCommand( secondTextureId, 3, 3, 2,
		ImVec4( 20.0f, 10.0f, 40.0f, 30.0f ) ) );
	first->CmdBuffer.push_back( DrawCommand( fontTextureId, 3, 6, 0,
		ImVec4( 50.0f, 20.0f, 60.0f, 30.0f ) ) );
	{
		ImDrawCmd reset;
		reset.UserCallback = ImDrawCallback_ResetRenderState;
		reset.ClipRect = ImVec4( 0, 0, 1, 1 );
		first->CmdBuffer.push_back( reset );
	}
	second = MakeList( 6, 6 );
	SetIndices( second, 0, { 0, 1, 2, 0, 1, 2 } );
	second->CmdBuffer.push_back( DrawCommand( fontTextureId, 3, 0, 1,
		ImVec4( 80.0f, 40.0f, 120.0f, 70.0f ) ) );
	second->CmdBuffer.push_back( DrawCommand( secondTextureId, 3, 3, 2,
		ImVec4( 1.0f, 1.0f, 3.0f, 3.0f ) ) );
	ImDrawData data = MakeDrawData( { first, second } );

	CHECK( WiredProfileImGuiRal_Record( renderer, &data, &commandBuffer,
		200, 100, &receipt ) == 1 );
	CHECK( receipt.commandLists == 2 && receipt.vertexCount == 14 && receipt.indexCount == 15 );
	CHECK( receipt.drawCalls == 5 && receipt.scissors == 5 && receipt.textureBinds == 5 );
	CHECK( receipt.fontTextureId == fontTextureId
	  && receipt.fontWidth == fake.fontWidth && receipt.fontHeight == fake.fontHeight );
	CHECK( receipt.vertexCapacityBytes >= 14u * sizeof( ImDrawVert )
	  && receipt.indexCapacityBytes >= 15u * sizeof( ImDrawIdx ) );
	CHECK( fake.bufferCreates == 2 );
	CHECK( fake.pipelineBinds == 2 && fake.vertexBinds == 2
	  && fake.indexBinds == 2 && fake.viewportSets == 2 && fake.pushConstants == 2 );
	CHECK( fake.boundGroups.size() == 5 );
	CHECK( fake.boundGroups[0] != fake.boundGroups[1]
	  && fake.boundGroups[0] == fake.boundGroups[2]
	  && fake.boundGroups[2] == fake.boundGroups[3]
	  && fake.boundGroups[1] == fake.boundGroups[4] );
	CHECK( fake.scissors[0].x == 0 && fake.scissors[0].y == 0
	  && fake.scissors[0].width == 21 && fake.scissors[0].height == 31 );
	CHECK( fake.scissors[3].x == 160 && fake.scissors[3].y == 80
	  && fake.scissors[3].width == 40 && fake.scissors[3].height == 20 );
	CHECK( fake.draws[0].firstIndex == 0 && fake.draws[0].vertexOffset == 1 );
	CHECK( fake.draws[1].firstIndex == 3 && fake.draws[1].vertexOffset == 2 );
	CHECK( fake.draws[2].firstIndex == 6 && fake.draws[2].vertexOffset == 0 );
	CHECK( fake.draws[3].firstIndex == 9 && fake.draws[3].vertexOffset == 9 );
	CHECK( fake.draws[4].firstIndex == 12 && fake.draws[4].vertexOffset == 10 );
	CHECK( fake.transforms.size() == 8 );
	for ( size_t i = 0; i < fake.transforms.size(); i += 4 ) {
		CHECK( fake.transforms[i + 0] == 0.02f && fake.transforms[i + 1] == 0.04f
		  && fake.transforms[i + 2] == -1.0f && fake.transforms[i + 3] == -1.0f );
	}
	CHECK( fake.boundVertexBuffer && fake.boundIndexBuffer );
	std::vector<unsigned char> expectedVertices( 14u * sizeof( ImDrawVert ) );
	std::memcpy( expectedVertices.data(), first->VtxBuffer.Data,
		static_cast<size_t>( first->VtxBuffer.Size ) * sizeof( ImDrawVert ) );
	std::memcpy( expectedVertices.data() + static_cast<size_t>( first->VtxBuffer.Size ) * sizeof( ImDrawVert ),
		second->VtxBuffer.Data, static_cast<size_t>( second->VtxBuffer.Size ) * sizeof( ImDrawVert ) );
	std::vector<unsigned char> expectedIndices( 15u * sizeof( ImDrawIdx ) );
	std::memcpy( expectedIndices.data(), first->IdxBuffer.Data,
		static_cast<size_t>( first->IdxBuffer.Size ) * sizeof( ImDrawIdx ) );
	std::memcpy( expectedIndices.data() + static_cast<size_t>( first->IdxBuffer.Size ) * sizeof( ImDrawIdx ),
		second->IdxBuffer.Data, static_cast<size_t>( second->IdxBuffer.Size ) * sizeof( ImDrawIdx ) );
	CHECK( std::memcmp( fake.boundVertexBuffer->bytes.data(), expectedVertices.data(),
		expectedVertices.size() ) == 0 );
	CHECK( std::memcmp( fake.boundIndexBuffer->bytes.data(), expectedIndices.data(),
		expectedIndices.size() ) == 0 );

	const uint32_t buffersAfterFirst = fake.bufferCreates;
	CHECK( WiredProfileImGuiRal_Record( renderer, &data, &commandBuffer,
		200, 100, &receipt ) == 1 );
	CHECK( fake.bufferCreates == buffersAfterFirst );

	large = MakeList( 4000, 9000 );
	large->CmdBuffer.push_back( DrawCommand( fontTextureId, 9000, 0, 0,
		ImVec4( 0, 0, 100, 50 ) ) );
	ImDrawData largeData = MakeDrawData( { large } );
	CHECK( WiredProfileImGuiRal_Record( renderer, &largeData, &commandBuffer,
		200, 100, &receipt ) == 1 );
	CHECK( receipt.vertexCapacityBytes >= 4000u * sizeof( ImDrawVert )
	  && receipt.indexCapacityBytes >= 9000u * sizeof( ImDrawIdx ) );
	CHECK( fake.bufferCreates == buffersAfterFirst + 2 );
	CHECK( fake.bufferDestroys == 2 );

	// Every rejection below must happen before allocation, mapping or commands.
	const ImTextureRef savedTexture = first->CmdBuffer[0].TexRef;
	first->CmdBuffer[0].TexRef = ImTextureRef( static_cast<ImTextureID>( 0xfedcba98u ) );
	CHECK( RejectsAtomically( renderer, &data, &commandBuffer, 200, 100 ) );
	first->CmdBuffer[0].TexRef = savedTexture;

	const ImDrawCallback savedCallback = first->CmdBuffer[0].UserCallback;
	first->CmdBuffer[0].UserCallback = ArbitraryCallback;
	CHECK( RejectsAtomically( renderer, &data, &commandBuffer, 200, 100 ) );
	first->CmdBuffer[0].UserCallback = savedCallback;

	const ImDrawIdx savedIndex = first->IdxBuffer[0];
	first->IdxBuffer[0] = static_cast<ImDrawIdx>( first->VtxBuffer.Size );
	CHECK( RejectsAtomically( renderer, &data, &commandBuffer, 200, 100 ) );
	first->IdxBuffer[0] = savedIndex;

	const float savedClip = first->CmdBuffer[0].ClipRect.x;
	first->CmdBuffer[0].ClipRect.x = std::numeric_limits<float>::quiet_NaN();
	CHECK( RejectsAtomically( renderer, &data, &commandBuffer, 200, 100 ) );
	first->CmdBuffer[0].ClipRect.x = savedClip;
	CHECK( RejectsAtomically( renderer, &data, &commandBuffer, 202, 100 ) );

	CHECK( WiredProfileImGuiRal_UnregisterTexture( renderer, secondTextureId ) == 1 );
	CHECK( fake.bindGroupDestroys == 1 );
	CHECK( RejectsAtomically( renderer, &data, &commandBuffer, 200, 100 ) );
	CHECK( WiredProfileImGuiRal_UnregisterTexture( renderer, secondTextureId ) == 0 );

	delete large;
	delete second;
	delete first;
	WiredProfileImGuiRal_Destroy( renderer );
	CHECK( io.BackendRendererUserData == nullptr
	  && io.Fonts->TexRef.GetTexID() == ImTextureID_Invalid );
	CHECK( ( io.BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset ) == 0 );
	CHECK( fake.textureCreates == 1 && fake.textureDestroys == 1 );
	CHECK( fake.bindGroupCreates == 2 && fake.bindGroupDestroys == 2 );
	CHECK( fake.pipelineCreates == pipelinesAfterCreate + 2
	  && fake.pipelineDestroys == pipelinesAfterCreate + 2 );
	CHECK( fake.bufferCreates == fake.bufferDestroys );
	ImGui::DestroyContext( context );
	std::puts( "wired profile ImGui RAL fake-backend contract: PASS" );
	return 0;
}
