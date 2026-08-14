// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "wired_profile_imgui_ral.h"

#include "imgui.h"
#include "ral.h"
#include "shaders/wired_profile_imgui_spv.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

struct TextureBinding {
	ImTextureID id = ImTextureID_Invalid;
	ralBindGroup_t *bindGroup = nullptr;
	bool fontOwned = false;
};

struct wiredProfileImGuiRalRenderer {
	ralBackend_t *backend = nullptr;
	ImGuiContext *context = nullptr;
	ralFormat_t colorFormat = RAL_FORMAT_UNDEFINED;
	ralPipeline_t *pipeline = nullptr;
	ralBindGroupLayout_t *bindGroupLayout = nullptr;
	ralBindGroup_t *fontBindGroup = nullptr;
	ralTexture_t *fontTexture = nullptr;
	ralTextureView_t *fontView = nullptr;
	ralSampler_t *fontSampler = nullptr;
	ralBuffer_t *vertexBuffer = nullptr;
	ralBuffer_t *indexBuffer = nullptr;
	uint64_t vertexCapacityBytes = 0;
	uint64_t indexCapacityBytes = 0;
	uint32_t fontWidth = 0;
	uint32_t fontHeight = 0;
	ImTextureID fontTextureId = ImTextureID_Invalid;
	std::vector<TextureBinding> textures;
};

namespace {

constexpr uint64_t kInitialVertexCapacity = 64u * 1024u;
constexpr uint64_t kInitialIndexCapacity = 16u * 1024u;
constexpr float kFramebufferExtentTolerance = 1.0f;
constexpr const char *kBackendName = "wired_profile_imgui_ral";
std::atomic<uint64_t> nextTextureId{ 1u };

struct DrawTransform {
	float scale[2];
	float translate[2];
};

struct ValidatedDrawData {
	uint32_t commandLists;
	uint32_t vertexCount;
	uint32_t indexCount;
	uint64_t vertexBytes;
	uint64_t indexBytes;
};

static_assert( sizeof( DrawTransform ) == 16, "ImGui push-constant ABI changed" );
static_assert( sizeof( ImDrawIdx ) == 2 || sizeof( ImDrawIdx ) == 4,
	"RAL supports only 16-bit and 32-bit ImGui indices" );
static_assert( sizeof( ImTextureID ) >= sizeof( uint64_t ),
	"ImTextureID cannot carry the renderer-owned numeric font token" );

bool Finite( float value ) {
	return std::isfinite( value );
}

bool CheckedMul( uint64_t lhs, uint64_t rhs, uint64_t *out ) {
	if ( !out || ( rhs != 0 && lhs > std::numeric_limits<uint64_t>::max() / rhs ) ) return false;
	*out = lhs * rhs;
	return true;
}

bool CheckedAdd( uint64_t lhs, uint64_t rhs, uint64_t *out ) {
	if ( !out || lhs > std::numeric_limits<uint64_t>::max() - rhs ) return false;
	*out = lhs + rhs;
	return true;
}

uint64_t TextureIdValue( ImTextureID id ) {
	return static_cast<uint64_t>( id );
}

bool CommandTextureId( const ImDrawCmd &command, ImTextureID *out ) {
	if ( !out || ( command.TexRef._TexData != nullptr
	  && command.TexRef._TexID != ImTextureID_Invalid ) ) return false;
	*out = command.TexRef._TexData
		? command.TexRef._TexData->TexID : command.TexRef._TexID;
	return true;
}

const TextureBinding *FindTexture( const wiredProfileImGuiRalRenderer_t *renderer,
		ImTextureID id ) {
	if ( !renderer || id == ImTextureID_Invalid ) return nullptr;
	for ( const TextureBinding &binding : renderer->textures )
		if ( binding.id == id ) return &binding;
	return nullptr;
}

ImTextureID NextTextureId() {
	for ( ;; ) {
		const uint64_t value = nextTextureId.fetch_add( 1u, std::memory_order_relaxed );
		if ( value != static_cast<uint64_t>( ImTextureID_Invalid ) )
			return static_cast<ImTextureID>( value );
	}
}

ralPipeline_t *CreatePipeline( wiredProfileImGuiRalRenderer_t *renderer,
		ralFormat_t colorFormat ) {
	#if IM_COL32_R_SHIFT == 0
	constexpr ralFormat_t packedColorFormat = RAL_FORMAT_R8G8B8A8_UNORM;
	#elif IM_COL32_B_SHIFT == 0
	constexpr ralFormat_t packedColorFormat = RAL_FORMAT_B8G8R8A8_UNORM;
	#else
	#error Unsupported ImDrawVert packed-color channel order
	#endif
	const ralVertexBinding_t vertexBinding = {
		0u, static_cast<uint32_t>( sizeof( ImDrawVert ) ), RAL_VERTEX_INPUT_PER_VERTEX
	};
	const ralVertexAttribute_t vertexAttributes[] = {
		{ 0u, 0u, RAL_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>( offsetof( ImDrawVert, pos ) ) },
		{ 1u, 0u, RAL_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>( offsetof( ImDrawVert, uv ) ) },
		{ 2u, 0u, packedColorFormat, static_cast<uint32_t>( offsetof( ImDrawVert, col ) ) }
	};
	const ralColorBlendAttachment_t blend = {
		qtrue,
		RAL_BLEND_SRC_ALPHA, RAL_BLEND_ONE_MINUS_SRC_ALPHA, RAL_BLEND_OP_ADD,
		RAL_BLEND_ONE, RAL_BLEND_ONE_MINUS_SRC_ALPHA, RAL_BLEND_OP_ADD,
		RAL_COLOR_WRITE_ALL
	};
	const ralBindGroupLayout_t *layouts[] = { renderer->bindGroupLayout };
	ralGraphicsPipelineCreateInfo_t ci = {};
	ci.vertexSpirv = wired_profile_imgui_vert_spv;
	ci.vertexSpirvSize = wired_profile_imgui_vert_spv_size;
	ci.fragmentSpirv = wired_profile_imgui_frag_spv;
	ci.fragmentSpirvSize = wired_profile_imgui_frag_spv_size;
	ci.vertexBindings = &vertexBinding;
	ci.numVertexBindings = 1;
	ci.vertexAttributes = vertexAttributes;
	ci.numVertexAttributes = static_cast<uint32_t>( sizeof( vertexAttributes ) / sizeof( vertexAttributes[0] ) );
	ci.topology = RAL_TOPOLOGY_TRIANGLE_LIST;
	ci.raster.polygonMode = RAL_POLYGON_FILL;
	ci.raster.cullMode = RAL_CULL_NONE;
	ci.raster.frontFace = RAL_FRONT_FACE_CCW;
	ci.raster.lineWidth = 1.0f;
	ci.depthStencil.depthTestEnable = qfalse;
	ci.depthStencil.depthWriteEnable = qfalse;
	ci.colorBlends = &blend;
	ci.numColorBlends = 1;
	ci.colorFormats[0] = colorFormat;
	ci.numColorFormats = 1;
	ci.depthFormat = RAL_FORMAT_UNDEFINED;
	ci.sampleCount = 1;
	ci.bindGroupLayouts = layouts;
	ci.numBindGroupLayouts = 1;
	ci.pushConstantSize = sizeof( DrawTransform );
	ci.pushConstantStages = RAL_STAGE_VERTEX;
	ci.debugName = "wired.profile-imgui.pipeline";
	return Ral_CreateGraphicsPipeline( renderer->backend, &ci );
}

void DestroyOwnedResources( wiredProfileImGuiRalRenderer_t *renderer ) {
	if ( !renderer ) return;
	Ral_DestroyBuffer( renderer->indexBuffer );
	Ral_DestroyBuffer( renderer->vertexBuffer );
	Ral_DestroyPipeline( renderer->pipeline );
	for ( const TextureBinding &binding : renderer->textures )
		Ral_DestroyBindGroup( binding.bindGroup );
	renderer->textures.clear();
	Ral_DestroySampler( renderer->fontSampler );
	Ral_DestroyTextureView( renderer->fontView );
	Ral_DestroyTexture( renderer->fontTexture );
	Ral_DestroyBindGroupLayout( renderer->bindGroupLayout );
	renderer->indexBuffer = nullptr;
	renderer->vertexBuffer = nullptr;
	renderer->pipeline = nullptr;
	renderer->fontBindGroup = nullptr;
	renderer->fontSampler = nullptr;
	renderer->fontView = nullptr;
	renderer->fontTexture = nullptr;
	renderer->bindGroupLayout = nullptr;
}

bool GrowBuffer( wiredProfileImGuiRalRenderer_t *renderer, uint64_t required,
		ralBufferUsage_t usage, uint64_t initialCapacity, const char *debugName,
		ralBuffer_t **buffer, uint64_t *capacity ) {
	if ( required <= *capacity ) return true;
	uint64_t next = *capacity > 0 ? *capacity : initialCapacity;
	while ( next < required ) {
		if ( next > std::numeric_limits<uint64_t>::max() / 2u ) {
			next = required;
			break;
		}
		next *= 2u;
	}
	ralBufferCreateInfo_t ci = {};
	ci.size = next;
	ci.usage = usage;
	ci.memory = RAL_MEMORY_HOST_COHERENT;
	ci.debugName = debugName;
	ralBuffer_t *replacement = Ral_CreateBuffer( renderer->backend, &ci );
	if ( !replacement ) return false;
	Ral_DestroyBuffer( *buffer );
	*buffer = replacement;
	*capacity = next;
	return true;
}

bool ValidateDrawData( const wiredProfileImGuiRalRenderer_t *renderer,
		const ImDrawData *drawData, uint32_t framebufferWidth,
		uint32_t framebufferHeight, ValidatedDrawData *validated ) {
	uint64_t vertices = 0, indices = 0;
	if ( !renderer || !drawData || !validated || !drawData->Valid || framebufferWidth == 0
	  || framebufferHeight == 0 || drawData->CmdListsCount < 0
	  || framebufferWidth > static_cast<uint32_t>( std::numeric_limits<int32_t>::max() )
	  || framebufferHeight > static_cast<uint32_t>( std::numeric_limits<int32_t>::max() )
	  || drawData->TotalVtxCount < 0 || drawData->TotalIdxCount < 0
	  || !Finite( drawData->DisplayPos.x ) || !Finite( drawData->DisplayPos.y )
	  || !Finite( drawData->DisplaySize.x ) || !Finite( drawData->DisplaySize.y )
	  || !Finite( drawData->FramebufferScale.x ) || !Finite( drawData->FramebufferScale.y )
	  || drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f
	  || drawData->FramebufferScale.x <= 0.0f || drawData->FramebufferScale.y <= 0.0f ) return false;
	const float expectedFramebufferWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
	const float expectedFramebufferHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;
	if ( !Finite( expectedFramebufferWidth ) || !Finite( expectedFramebufferHeight )
	  || std::fabs( expectedFramebufferWidth - static_cast<float>( framebufferWidth ) ) > kFramebufferExtentTolerance
	  || std::fabs( expectedFramebufferHeight - static_cast<float>( framebufferHeight ) ) > kFramebufferExtentTolerance ) return false;
	if ( drawData->CmdListsCount != drawData->CmdLists.Size
	  || ( drawData->CmdListsCount > 0 && !drawData->CmdLists.Data ) ) return false;

	for ( int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex ) {
		const ImDrawList *list = drawData->CmdLists[listIndex];
		if ( !list || list->VtxBuffer.Size < 0 || list->IdxBuffer.Size < 0
		  || list->CmdBuffer.Size < 0
		  || ( list->VtxBuffer.Size > 0 && !list->VtxBuffer.Data )
		  || ( list->IdxBuffer.Size > 0 && !list->IdxBuffer.Data )
		  || ( list->CmdBuffer.Size > 0 && !list->CmdBuffer.Data ) ) return false;
		if ( !CheckedAdd( vertices, static_cast<uint64_t>( list->VtxBuffer.Size ), &vertices )
		  || !CheckedAdd( indices, static_cast<uint64_t>( list->IdxBuffer.Size ), &indices ) ) return false;
		if ( vertices > static_cast<uint64_t>( std::numeric_limits<int32_t>::max() )
		  || indices > static_cast<uint64_t>( std::numeric_limits<uint32_t>::max() ) ) return false;

		for ( int commandIndex = 0; commandIndex < list->CmdBuffer.Size; ++commandIndex ) {
			const ImDrawCmd &command = list->CmdBuffer[commandIndex];
			if ( !Finite( command.ClipRect.x ) || !Finite( command.ClipRect.y )
			  || !Finite( command.ClipRect.z ) || !Finite( command.ClipRect.w ) ) return false;
			if ( command.UserCallback ) {
				if ( command.UserCallback != ImDrawCallback_ResetRenderState ) return false;
				continue;
			}
			ImTextureID commandTexture = ImTextureID_Invalid;
			if ( !CommandTextureId( command, &commandTexture )
		  || !FindTexture( renderer, commandTexture ) ) return false;
			if ( command.ElemCount % 3u != 0u ) return false;
			const uint64_t indexEnd = static_cast<uint64_t>( command.IdxOffset ) + command.ElemCount;
			if ( indexEnd > static_cast<uint64_t>( list->IdxBuffer.Size )
			  || command.VtxOffset > static_cast<unsigned int>( list->VtxBuffer.Size ) ) return false;
			for ( uint64_t element = command.IdxOffset; element < indexEnd; ++element ) {
				const uint64_t vertex = static_cast<uint64_t>( list->IdxBuffer.Data[element] ) + command.VtxOffset;
				if ( vertex >= static_cast<uint64_t>( list->VtxBuffer.Size ) ) return false;
			}
		}
	}
	if ( vertices != static_cast<uint64_t>( drawData->TotalVtxCount )
	  || indices != static_cast<uint64_t>( drawData->TotalIdxCount )
	  || vertices > std::numeric_limits<uint32_t>::max()
	  || !CheckedMul( vertices, sizeof( ImDrawVert ), &validated->vertexBytes )
	  || !CheckedMul( indices, sizeof( ImDrawIdx ), &validated->indexBytes )
	  || validated->vertexBytes > std::numeric_limits<size_t>::max()
	  || validated->indexBytes > std::numeric_limits<size_t>::max() ) return false;
	validated->commandLists = static_cast<uint32_t>( drawData->CmdListsCount );
	validated->vertexCount = static_cast<uint32_t>( vertices );
	validated->indexCount = static_cast<uint32_t>( indices );
	return true;
}

void SetupRenderState( wiredProfileImGuiRalRenderer_t *renderer,
		const ImDrawData *drawData, ralCommandBuffer_t *commandBuffer,
		uint32_t framebufferWidth, uint32_t framebufferHeight,
		wiredProfileImGuiRalReceipt_t *receipt ) {
	const DrawTransform transform = {
		{ 2.0f / drawData->DisplaySize.x, 2.0f / drawData->DisplaySize.y },
		{ -1.0f - drawData->DisplayPos.x * ( 2.0f / drawData->DisplaySize.x ),
		  -1.0f - drawData->DisplayPos.y * ( 2.0f / drawData->DisplaySize.y ) }
	};
	const ralViewport_t viewport = {
		0.0f, 0.0f, static_cast<float>( framebufferWidth ),
		static_cast<float>( framebufferHeight ), 0.0f, 1.0f
	};
	Ral_CmdBindPipeline( commandBuffer, renderer->pipeline );
	Ral_CmdBindVertexBuffer( commandBuffer, 0, renderer->vertexBuffer, 0 );
	Ral_CmdBindIndexBuffer( commandBuffer, renderer->indexBuffer, 0,
		sizeof( ImDrawIdx ) == 2 ? RAL_INDEX_UINT16 : RAL_INDEX_UINT32 );
	Ral_CmdSetViewport( commandBuffer, &viewport );
	Ral_CmdPushConstants( commandBuffer, RAL_STAGE_VERTEX, 0, sizeof( transform ), &transform );
}

} // namespace

int WiredProfileImGuiRal_Create( ralBackend_t *backend, ralFormat_t colorFormat,
		ImGuiContext *context, wiredProfileImGuiRalRenderer_t **out ) {
	unsigned char *fontPixels = nullptr;
	int fontWidth = 0, fontHeight = 0, bytesPerPixel = 0;
	if ( !backend || colorFormat == RAL_FORMAT_UNDEFINED || !context || !out ) return 0;
	ImGuiContext *previousContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext( context );
	ImGuiIO &io = ImGui::GetIO();
	if ( io.BackendRendererUserData || io.BackendRendererName
	  || io.Fonts->TexRef.GetTexID() != ImTextureID_Invalid ) {
		ImGui::SetCurrentContext( previousContext );
		return 0;
	}
	io.Fonts->GetTexDataAsRGBA32( &fontPixels, &fontWidth, &fontHeight, &bytesPerPixel );
	if ( !fontPixels || fontWidth <= 0 || fontHeight <= 0 || bytesPerPixel != 4 ) {
		ImGui::SetCurrentContext( previousContext );
		return 0;
	}
	uint64_t fontBytes = 0, fontPixelsCount = 0;
	if ( !CheckedMul( static_cast<uint64_t>( fontWidth ), static_cast<uint64_t>( fontHeight ), &fontPixelsCount )
	  || !CheckedMul( fontPixelsCount, 4u, &fontBytes )
	  || fontBytes > std::numeric_limits<size_t>::max() ) {
		ImGui::SetCurrentContext( previousContext );
		return 0;
	}

	wiredProfileImGuiRalRenderer_t *renderer = new ( std::nothrow ) wiredProfileImGuiRalRenderer_t();
	if ( !renderer ) { ImGui::SetCurrentContext( previousContext ); return 0; }
	renderer->backend = backend;
	renderer->context = context;
	renderer->fontWidth = static_cast<uint32_t>( fontWidth );
	renderer->fontHeight = static_cast<uint32_t>( fontHeight );
	renderer->fontTextureId = NextTextureId();

	{
		const ralBindEntry_t entry = { 0u, RAL_BIND_COMBINED_TEXTURE_SAMPLER, 1u, RAL_STAGE_FRAGMENT };
		ralBindGroupLayoutCreateInfo_t ci = {};
		ci.entries = &entry;
		ci.numEntries = 1;
		ci.debugName = "wired.profile-imgui.bind-layout";
		renderer->bindGroupLayout = Ral_CreateBindGroupLayout( backend, &ci );
		if ( !renderer->bindGroupLayout ) goto fail;
	}
	{
		ralTextureCreateInfo_t ci = {};
		ci.type = RAL_TEXTURE_2D;
		ci.format = RAL_FORMAT_R8G8B8A8_UNORM;
		ci.width = renderer->fontWidth;
		ci.height = renderer->fontHeight;
		ci.depthOrArrayLayers = 1;
		ci.mipLevels = 1;
		ci.sampleCount = 1;
		ci.usage = static_cast<ralTextureUsage_t>( RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_DST );
		ci.memory = RAL_MEMORY_DEVICE_LOCAL;
		ci.concurrentGraphicsTransfer = qfalse;
		ci.debugName = "wired.profile-imgui.font-texture";
		renderer->fontTexture = Ral_CreateTexture( backend, &ci );
		if ( !renderer->fontTexture ) goto fail;
	}
	{
		ralTextureUploadDesc_t upload = {};
		upload.data = fontPixels;
		upload.dataSize = fontBytes;
		// This makes the existing upload implementation choose the graphics
		// queue. It waits internally, so no transfer-family acquire is owed.
		upload.suppressMipGeneration = qtrue;
		ralFence_t *fence = Ral_TextureUploadAsync( renderer->fontTexture, &upload );
		if ( !fence ) goto fail;
		Ral_WaitFence( fence, RAL_TIMEOUT_INFINITE );
		Ral_DestroyFence( fence );
	}
	{
		ralTextureViewCreateInfo_t ci = {};
		ci.texture = renderer->fontTexture;
		ci.viewType = RAL_TEXTURE_2D;
		ci.format = RAL_FORMAT_R8G8B8A8_UNORM;
		ci.mipLevelCount = 1;
		ci.arrayLayerCount = 1;
		renderer->fontView = Ral_CreateTextureView( backend, &ci );
		if ( !renderer->fontView ) goto fail;
	}
	{
		ralSamplerCreateInfo_t ci = {};
		ci.minFilter = RAL_FILTER_LINEAR;
		ci.magFilter = RAL_FILTER_LINEAR;
		ci.mipmapMode = RAL_MIPMAP_NEAREST;
		ci.addressU = ci.addressV = ci.addressW = RAL_ADDRESS_CLAMP_TO_EDGE;
		ci.maxAnisotropy = 1.0f;
		ci.compareOp = RAL_COMPARE_ALWAYS;
		ci.minLod = 0.0f;
		ci.maxLod = 1.0f;
		ci.debugName = "wired.profile-imgui.font-sampler";
		renderer->fontSampler = Ral_CreateSampler( backend, &ci );
		if ( !renderer->fontSampler ) goto fail;
	}
	{
		const ralBindingValue_t value = {
			0u, RAL_BIND_COMBINED_TEXTURE_SAMPLER, nullptr, 0u, 0u,
			renderer->fontView, renderer->fontSampler, nullptr, 0u
		};
		ralBindGroupCreateInfo_t ci = {};
		ci.layout = renderer->bindGroupLayout;
		ci.values = &value;
		ci.numValues = 1;
		ci.debugName = "wired.profile-imgui.font-bind-group";
		renderer->fontBindGroup = Ral_CreateBindGroup( backend, &ci );
		if ( !renderer->fontBindGroup ) goto fail;
		try {
			renderer->textures.push_back( { renderer->fontTextureId,
				renderer->fontBindGroup, true } );
		} catch ( ... ) {
			Ral_DestroyBindGroup( renderer->fontBindGroup );
			renderer->fontBindGroup = nullptr;
			goto fail;
		}
	}
	renderer->pipeline = CreatePipeline( renderer, colorFormat );
	if ( !renderer->pipeline ) goto fail;
	renderer->colorFormat = colorFormat;

	io.Fonts->SetTexID( renderer->fontTextureId );
	io.BackendRendererUserData = renderer;
	io.BackendRendererName = kBackendName;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	*out = renderer;
	ImGui::SetCurrentContext( previousContext );
	return 1;

fail:
	DestroyOwnedResources( renderer );
	delete renderer;
	ImGui::SetCurrentContext( previousContext );
	return 0;
}

int WiredProfileImGuiRal_EnsureColorFormat(
		wiredProfileImGuiRalRenderer_t *renderer, ralFormat_t colorFormat ) {
	if ( !renderer || colorFormat == RAL_FORMAT_UNDEFINED ) return 0;
	if ( renderer->colorFormat == colorFormat ) return 1;
	ralPipeline_t *replacement = CreatePipeline( renderer, colorFormat );
	if ( !replacement ) return 0;
	Ral_DestroyPipeline( renderer->pipeline );
	renderer->pipeline = replacement;
	renderer->colorFormat = colorFormat;
	return 1;
}

int WiredProfileImGuiRal_RegisterTexture( wiredProfileImGuiRalRenderer_t *renderer,
		ralTextureView_t *view, ralSampler_t *sampler, uint64_t *outTextureId ) {
	if ( !renderer || !view || !sampler || !outTextureId ) return 0;
	const ImTextureID id = NextTextureId();
	const ralBindingValue_t value = {
		0u, RAL_BIND_COMBINED_TEXTURE_SAMPLER, nullptr, 0u, 0u,
		view, sampler, nullptr, 0u
	};
	ralBindGroupCreateInfo_t ci = {};
	ci.layout = renderer->bindGroupLayout;
	ci.values = &value;
	ci.numValues = 1;
	ci.debugName = "wired.profile-imgui.registered-texture";
	ralBindGroup_t *bindGroup = Ral_CreateBindGroup( renderer->backend, &ci );
	if ( !bindGroup ) return 0;
	try {
		renderer->textures.push_back( { id, bindGroup, false } );
	} catch ( ... ) {
		Ral_DestroyBindGroup( bindGroup );
		return 0;
	}
	*outTextureId = TextureIdValue( id );
	return 1;
}

int WiredProfileImGuiRal_UnregisterTexture( wiredProfileImGuiRalRenderer_t *renderer,
		uint64_t textureId ) {
	if ( !renderer || textureId == 0 ) return 0;
	for ( auto it = renderer->textures.begin(); it != renderer->textures.end(); ++it ) {
		if ( TextureIdValue( it->id ) != textureId ) continue;
		if ( it->fontOwned ) return 0;
		Ral_DestroyBindGroup( it->bindGroup );
		renderer->textures.erase( it );
		return 1;
	}
	return 0;
}

int WiredProfileImGuiRal_Record( wiredProfileImGuiRalRenderer_t *renderer,
		const ImDrawData *drawData, ralCommandBuffer_t *commandBuffer,
		uint32_t framebufferWidth, uint32_t framebufferHeight,
		wiredProfileImGuiRalReceipt_t *receipt ) {
	ValidatedDrawData validated = {};
	wiredProfileImGuiRalReceipt_t result = {};
	if ( !commandBuffer || !receipt
	  || !ValidateDrawData( renderer, drawData, framebufferWidth, framebufferHeight, &validated ) ) return 0;
	if ( validated.vertexBytes > 0
	  && !GrowBuffer( renderer, validated.vertexBytes, RAL_BUFFER_VERTEX,
		  kInitialVertexCapacity, "wired.profile-imgui.vertices",
		  &renderer->vertexBuffer, &renderer->vertexCapacityBytes ) ) return 0;
	if ( validated.indexBytes > 0
	  && !GrowBuffer( renderer, validated.indexBytes, RAL_BUFFER_INDEX,
		  kInitialIndexCapacity, "wired.profile-imgui.indices",
		  &renderer->indexBuffer, &renderer->indexCapacityBytes ) ) return 0;

	if ( validated.vertexBytes > 0 || validated.indexBytes > 0 ) {
		void *vertexMap = validated.vertexBytes > 0 ? Ral_MapBuffer( renderer->vertexBuffer ) : nullptr;
		void *indexMap = validated.indexBytes > 0 ? Ral_MapBuffer( renderer->indexBuffer ) : nullptr;
		if ( ( validated.vertexBytes > 0 && !vertexMap ) || ( validated.indexBytes > 0 && !indexMap ) ) {
			if ( vertexMap ) Ral_UnmapBuffer( renderer->vertexBuffer );
			if ( indexMap ) Ral_UnmapBuffer( renderer->indexBuffer );
			return 0;
		}
		size_t vertexOffset = 0, indexOffset = 0;
		for ( int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex ) {
			const ImDrawList *list = drawData->CmdLists[listIndex];
			const size_t vertexSize = static_cast<size_t>( list->VtxBuffer.Size ) * sizeof( ImDrawVert );
			const size_t indexSize = static_cast<size_t>( list->IdxBuffer.Size ) * sizeof( ImDrawIdx );
			if ( vertexSize ) std::memcpy( static_cast<unsigned char *>( vertexMap ) + vertexOffset,
				list->VtxBuffer.Data, vertexSize );
			if ( indexSize ) std::memcpy( static_cast<unsigned char *>( indexMap ) + indexOffset,
				list->IdxBuffer.Data, indexSize );
			vertexOffset += vertexSize;
			indexOffset += indexSize;
		}
		if ( vertexMap ) Ral_UnmapBuffer( renderer->vertexBuffer );
		if ( indexMap ) Ral_UnmapBuffer( renderer->indexBuffer );
	}

	result.commandLists = validated.commandLists;
	result.vertexCount = validated.vertexCount;
	result.indexCount = validated.indexCount;
	result.fontWidth = renderer->fontWidth;
	result.fontHeight = renderer->fontHeight;
	result.fontTextureId = TextureIdValue( renderer->fontTextureId );
	result.vertexCapacityBytes = renderer->vertexCapacityBytes;
	result.indexCapacityBytes = renderer->indexCapacityBytes;
	if ( validated.vertexCount == 0 || validated.indexCount == 0 ) {
		*receipt = result;
		return 1;
	}

	SetupRenderState( renderer, drawData, commandBuffer, framebufferWidth, framebufferHeight, &result );
	uint32_t globalVertexOffset = 0, globalIndexOffset = 0;
	ImTextureID boundTexture = ImTextureID_Invalid;
	for ( int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex ) {
		const ImDrawList *list = drawData->CmdLists[listIndex];
		for ( int commandIndex = 0; commandIndex < list->CmdBuffer.Size; ++commandIndex ) {
			const ImDrawCmd &command = list->CmdBuffer[commandIndex];
			if ( command.UserCallback ) {
				if ( command.UserCallback == ImDrawCallback_ResetRenderState ) {
					SetupRenderState( renderer, drawData, commandBuffer,
						framebufferWidth, framebufferHeight, &result );
					boundTexture = ImTextureID_Invalid;
				}
				continue;
			}
			float clipMinX = ( command.ClipRect.x - drawData->DisplayPos.x ) * drawData->FramebufferScale.x;
			float clipMinY = ( command.ClipRect.y - drawData->DisplayPos.y ) * drawData->FramebufferScale.y;
			float clipMaxX = ( command.ClipRect.z - drawData->DisplayPos.x ) * drawData->FramebufferScale.x;
			float clipMaxY = ( command.ClipRect.w - drawData->DisplayPos.y ) * drawData->FramebufferScale.y;
			if ( clipMinX < 0.0f ) clipMinX = 0.0f;
			if ( clipMinY < 0.0f ) clipMinY = 0.0f;
			if ( clipMaxX > framebufferWidth ) clipMaxX = static_cast<float>( framebufferWidth );
			if ( clipMaxY > framebufferHeight ) clipMaxY = static_cast<float>( framebufferHeight );
			if ( clipMaxX <= clipMinX || clipMaxY <= clipMinY || command.ElemCount == 0 ) continue;
			ImTextureID commandTexture = ImTextureID_Invalid;
			if ( !CommandTextureId( command, &commandTexture ) ) return 0;
			if ( commandTexture != boundTexture ) {
				const TextureBinding *binding = FindTexture( renderer, commandTexture );
				if ( !binding ) return 0;
				Ral_CmdBindBindGroup( commandBuffer, 0, binding->bindGroup );
				boundTexture = commandTexture;
				result.textureBinds++;
			}
			const int32_t x = static_cast<int32_t>( clipMinX );
			const int32_t y = static_cast<int32_t>( clipMinY );
			const uint32_t maxX = static_cast<uint32_t>( std::ceil( clipMaxX ) );
			const uint32_t maxY = static_cast<uint32_t>( std::ceil( clipMaxY ) );
			const ralRect_t scissor = {
				x, y, maxX - static_cast<uint32_t>( x ), maxY - static_cast<uint32_t>( y )
			};
			Ral_CmdSetScissor( commandBuffer, &scissor );
			Ral_CmdDrawIndexed( commandBuffer, command.ElemCount, 1,
				globalIndexOffset + command.IdxOffset,
				static_cast<int32_t>( globalVertexOffset + command.VtxOffset ), 0 );
			result.scissors++;
			result.drawCalls++;
		}
		globalIndexOffset += static_cast<uint32_t>( list->IdxBuffer.Size );
		globalVertexOffset += static_cast<uint32_t>( list->VtxBuffer.Size );
	}
	*receipt = result;
	return 1;
}

void WiredProfileImGuiRal_Destroy( wiredProfileImGuiRalRenderer_t *renderer ) {
	if ( !renderer ) return;
	ImGuiContext *previousContext = ImGui::GetCurrentContext();
	if ( renderer->context ) {
		ImGui::SetCurrentContext( renderer->context );
		ImGuiIO &io = ImGui::GetIO();
		if ( io.BackendRendererUserData == renderer ) {
			if ( io.Fonts->TexRef.GetTexID() == renderer->fontTextureId )
				io.Fonts->SetTexID( ImTextureID_Invalid );
			io.BackendRendererUserData = nullptr;
			if ( io.BackendRendererName == kBackendName ) io.BackendRendererName = nullptr;
			io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
		}
	}
	ImGui::SetCurrentContext( previousContext );
	DestroyOwnedResources( renderer );
	delete renderer;
}
