// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_metal_bind_group.h"
#include "ral_metal_internal.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct ralMetalBindLayout_s {
	id<MTLArgumentEncoder> encoder;
	ralMetalBindLayoutReceipt_t receipt;
};

struct ralMetalBindGroup_s {
	id<MTLBuffer> argumentBuffer;
	id nativeResources[RAL_METAL_BIND_MAX_RESOURCES];
	ralMetalArgumentClass_t argumentClasses[RAL_METAL_BIND_MAX_RESOURCES];
	ralMetalBindGroupReceipt_t receipt;
};

static qboolean IsTextureType( ralBindType_t type ) {
	return ( type == RAL_BIND_SAMPLED_TEXTURE || type == RAL_BIND_STORAGE_TEXTURE
		|| type == RAL_BIND_TEXTURE_ARRAY ) ? qtrue : qfalse;
}

static qboolean ViewTypeValid( ralBindTextureViewType_t type ) {
	return type >= RAL_BIND_TEXTURE_VIEW_1D
		&& type <= RAL_BIND_TEXTURE_VIEW_3D ? qtrue : qfalse;
}

static qboolean ClassForType( ralBindType_t type, ralMetalArgumentClass_t *out ) {
	switch ( type ) {
	case RAL_BIND_UNIFORM_BUFFER:
	case RAL_BIND_STORAGE_BUFFER: *out = RAL_METAL_ARGUMENT_BUFFER; return qtrue;
	case RAL_BIND_SAMPLED_TEXTURE:
	case RAL_BIND_STORAGE_TEXTURE:
	case RAL_BIND_TEXTURE_ARRAY: *out = RAL_METAL_ARGUMENT_TEXTURE; return qtrue;
	case RAL_BIND_SAMPLER: *out = RAL_METAL_ARGUMENT_SAMPLER; return qtrue;
	case RAL_BIND_COMBINED_TEXTURE_SAMPLER: return qfalse;
	default: return qfalse;
	}
}

static qboolean EntryValid( const ralMetalBindLayoutEntryReceipt_t *entry ) {
	ralMetalArgumentClass_t expected;
	if ( !entry || entry->count == 0u || entry->count > RAL_METAL_BIND_MAX_RESOURCES
			|| entry->stageFlags == 0u || ( entry->stageFlags & ~RAL_STAGE_ALL ) != 0u
			|| !ClassForType( entry->type, &expected )
			|| expected != entry->argumentClass ) return qfalse;
	if ( IsTextureType( entry->type ) != ViewTypeValid( entry->textureViewType ) )
		return qfalse;
	return qtrue;
}

static qboolean LayoutReceiptValid( const ralMetalBindLayoutReceipt_t *receipt ) {
	uint32_t i, next = 0u;
	if ( !receipt || receipt->schemaVersion != RAL_METAL_BIND_GROUP_SCHEMA_VERSION
			|| receipt->backendType != RAL_BACKEND_METAL
			|| receipt->coreGeneration == 0u || receipt->coreGeneration == UINT64_MAX
			|| receipt->layoutGeneration == 0u || receipt->layoutGeneration == UINT64_MAX
			|| receipt->layoutIdentity == (uintptr_t)0 || receipt->entryCount == 0u
			|| receipt->entryCount > RAL_METAL_BIND_MAX_ENTRIES
			|| receipt->totalArgumentCount == 0u
			|| receipt->totalArgumentCount > RAL_METAL_BIND_MAX_RESOURCES
			|| receipt->encodedLength == 0u || receipt->alignment == 0u
			|| receipt->ready != qtrue ) return qfalse;
	for ( i = 0u; i < receipt->entryCount; ++i ) {
		const ralMetalBindLayoutEntryReceipt_t *entry = &receipt->entries[i];
		if ( !EntryValid( entry ) || entry->argumentIndex != next
				|| ( i && receipt->entries[i - 1u].binding >= entry->binding ) ) return qfalse;
		next += entry->count;
	}
	return next == receipt->totalArgumentCount ? qtrue : qfalse;
}

static qboolean LayoutEntryExact( const ralMetalBindLayoutEntryReceipt_t *a,
		const ralMetalBindLayoutEntryReceipt_t *b ) {
	return ( a->binding == b->binding && a->type == b->type && a->count == b->count
		&& a->stageFlags == b->stageFlags && a->textureViewType == b->textureViewType
		&& a->argumentIndex == b->argumentIndex && a->argumentClass == b->argumentClass )
		? qtrue : qfalse;
}

qboolean RalMetal_BindLayoutReceiptExact( const ralMetalBindLayoutReceipt_t *a,
		const ralMetalBindLayoutReceipt_t *b ) {
	uint32_t i;
	if ( !LayoutReceiptValid( a ) || !LayoutReceiptValid( b )
			|| a->backendType != b->backendType || a->coreGeneration != b->coreGeneration
			|| a->layoutGeneration != b->layoutGeneration
			|| a->layoutIdentity != b->layoutIdentity || a->entryCount != b->entryCount
			|| a->totalArgumentCount != b->totalArgumentCount
			|| a->encodedLength != b->encodedLength || a->alignment != b->alignment ) return qfalse;
	for ( i = 0u; i < a->entryCount; ++i )
		if ( !LayoutEntryExact( &a->entries[i], &b->entries[i] ) ) return qfalse;
	return qtrue;
}

qboolean RalMetal_BindLayoutMatchesReceipt( const ralMetalBindLayout_t *layout,
		const ralMetalBindLayoutReceipt_t *receipt ) {
	return ( layout && receipt && receipt->layoutIdentity == (uintptr_t)layout
		&& RalMetal_BindLayoutReceiptExact( receipt, &layout->receipt ) ) ? qtrue : qfalse;
}

static MTLTextureType MetalTextureType( ralBindTextureViewType_t type ) {
	switch ( type ) {
	case RAL_BIND_TEXTURE_VIEW_1D: return MTLTextureType1D;
	case RAL_BIND_TEXTURE_VIEW_2D: return MTLTextureType2D;
	case RAL_BIND_TEXTURE_VIEW_2D_ARRAY: return MTLTextureType2DArray;
	case RAL_BIND_TEXTURE_VIEW_CUBE: return MTLTextureTypeCube;
	case RAL_BIND_TEXTURE_VIEW_CUBE_ARRAY: return MTLTextureTypeCubeArray;
	case RAL_BIND_TEXTURE_VIEW_3D: return MTLTextureType3D;
	default: return MTLTextureType2D;
	}
}

qboolean RalMetal_BindLayoutCreate( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt,
		const ralBindGroupLayoutCreateInfo_t *createInfo, uint64_t generation,
		ralMetalBindLayout_t **outLayout, ralMetalBindLayoutReceipt_t *outReceipt ) {
	ralMetalBindLayout_t *candidate = NULL;
	ralMetalBindLayoutReceipt_t receipt;
	id<MTLDevice> device;
	uint32_t i, next = 0u;
	if ( !core || !coreReceipt || !createInfo || !outLayout || !outReceipt
			|| !createInfo->entries || createInfo->numEntries == 0u
			|| createInfo->numEntries > RAL_METAL_BIND_MAX_ENTRIES
			|| createInfo->bindless != qfalse || generation == 0u || generation == UINT64_MAX
			|| !RalMetal_CoreMatchesReceipt( core, coreReceipt ) ) return qfalse;
	device = RalMetal_CoreNativeDevice( core );
	if ( !device || coreReceipt->backendIdentity != (uintptr_t)core
			|| coreReceipt->deviceIdentity != (uintptr_t)(void *)device ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_BIND_GROUP_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL;
	receipt.coreGeneration = coreReceipt->generation;
	receipt.layoutGeneration = generation;
	@autoreleasepool {
		NSMutableArray<MTLArgumentDescriptor *> *arguments = [NSMutableArray
			arrayWithCapacity:createInfo->numEntries];
		for ( i = 0u; i < createInfo->numEntries; ++i ) {
			const ralBindEntry_t *source = &createInfo->entries[i];
			ralMetalBindLayoutEntryReceipt_t *entry = &receipt.entries[i];
			MTLArgumentDescriptor *descriptor;
			if ( source->count == 0u || source->count > RAL_METAL_BIND_MAX_RESOURCES
					|| next > RAL_METAL_BIND_MAX_RESOURCES - source->count
					|| ( i && createInfo->entries[i - 1u].binding >= source->binding ) ) return qfalse;
			entry->binding = source->binding; entry->type = source->type;
			entry->count = source->count; entry->stageFlags = source->stageFlags;
			entry->textureViewType = source->textureViewType;
			entry->argumentIndex = next;
			if ( !ClassForType( source->type, &entry->argumentClass ) || !EntryValid( entry ) )
				return qfalse;
			descriptor = [MTLArgumentDescriptor argumentDescriptor];
			descriptor.index = entry->argumentIndex;
			descriptor.arrayLength = entry->count == 1u ? 0u : entry->count;
			switch ( entry->argumentClass ) {
			case RAL_METAL_ARGUMENT_BUFFER:
				descriptor.dataType = MTLDataTypePointer;
				descriptor.access = entry->type == RAL_BIND_UNIFORM_BUFFER
					? MTLBindingAccessReadOnly : MTLBindingAccessReadWrite;
				break;
			case RAL_METAL_ARGUMENT_TEXTURE:
				descriptor.dataType = MTLDataTypeTexture;
				descriptor.textureType = MetalTextureType( entry->textureViewType );
				descriptor.access = entry->type == RAL_BIND_STORAGE_TEXTURE
					? MTLBindingAccessReadWrite : MTLBindingAccessReadOnly;
				break;
			case RAL_METAL_ARGUMENT_SAMPLER:
				descriptor.dataType = MTLDataTypeSampler;
				descriptor.access = MTLBindingAccessReadOnly;
				break;
			}
			[arguments addObject:descriptor];
			next += entry->count;
		}
		candidate = (ralMetalBindLayout_t *)calloc( 1u, sizeof( *candidate ) );
		if ( !candidate ) return qfalse;
		candidate->encoder = [device newArgumentEncoderWithArguments:arguments];
		if ( !candidate->encoder ) { free( candidate ); return qfalse; }
		receipt.layoutIdentity = (uintptr_t)candidate;
		receipt.entryCount = createInfo->numEntries;
		receipt.totalArgumentCount = next;
		receipt.encodedLength = (uint64_t)candidate->encoder.encodedLength;
		receipt.alignment = (uint64_t)candidate->encoder.alignment;
		receipt.ready = qtrue;
		if ( !LayoutReceiptValid( &receipt ) ) {
			[candidate->encoder release]; free( candidate ); return qfalse;
		}
		candidate->receipt = receipt;
	}
	*outLayout = candidate;
	*outReceipt = receipt;
	return qtrue;
}

void RalMetal_BindLayoutDestroy( ralMetalBindLayout_t *layout ) {
	if ( !layout ) return;
	[layout->encoder release];
	memset( layout, 0, sizeof( *layout ) );
	free( layout );
}

static qboolean ResourceReceiptValid( const ralMetalBindResourceReceipt_t *resource ) {
	ralMetalArgumentClass_t argumentClass;
	return ( resource && ClassForType( resource->type, &argumentClass )
		&& resource->resourceIdentity != (uintptr_t)0
		&& resource->resourceGeneration != 0u && resource->resourceGeneration != UINT64_MAX
		&& ( ( argumentClass == RAL_METAL_ARGUMENT_BUFFER && resource->bufferRange != 0u )
			|| ( argumentClass != RAL_METAL_ARGUMENT_BUFFER
				&& resource->bufferOffset == 0u && resource->bufferRange == 0u ) ) )
		? qtrue : qfalse;
}

static qboolean NativeResourceMatches( ralMetalArgumentClass_t argumentClass,
		void *nativeResource ) {
	id resource = (id)nativeResource;
	if ( !resource ) return qfalse;
	switch ( argumentClass ) {
	case RAL_METAL_ARGUMENT_BUFFER:
		return [resource conformsToProtocol:@protocol(MTLBuffer)] ? qtrue : qfalse;
	case RAL_METAL_ARGUMENT_TEXTURE:
		return [resource conformsToProtocol:@protocol(MTLTexture)] ? qtrue : qfalse;
	case RAL_METAL_ARGUMENT_SAMPLER:
		return [resource conformsToProtocol:@protocol(MTLSamplerState)] ? qtrue : qfalse;
	default: return qfalse;
	}
}

static qboolean GroupReceiptValid( const ralMetalBindGroupReceipt_t *receipt ) {
	uint32_t i;
	if ( !receipt || receipt->schemaVersion != RAL_METAL_BIND_GROUP_SCHEMA_VERSION
			|| receipt->backendType != RAL_BACKEND_METAL
			|| receipt->coreGeneration == 0u || receipt->coreGeneration == UINT64_MAX
			|| receipt->layoutGeneration == 0u || receipt->layoutGeneration == UINT64_MAX
			|| receipt->layoutIdentity == (uintptr_t)0
			|| receipt->groupGeneration == 0u || receipt->groupGeneration == UINT64_MAX
			|| receipt->groupIdentity == (uintptr_t)0
			|| receipt->argumentBufferIdentity == (uintptr_t)0
			|| receipt->argumentBufferBytes == 0u || receipt->resourceCount == 0u
			|| receipt->resourceCount > RAL_METAL_BIND_MAX_RESOURCES
			|| receipt->groupIdentity == receipt->layoutIdentity
			|| receipt->groupIdentity == receipt->argumentBufferIdentity
			|| receipt->ready != qtrue ) return qfalse;
	for ( i = 0u; i < receipt->resourceCount; ++i ) {
		if ( !ResourceReceiptValid( &receipt->resources[i] )
				|| receipt->argumentBufferIdentity == receipt->resources[i].resourceIdentity
				|| ( i && ( receipt->resources[i - 1u].binding > receipt->resources[i].binding
				|| ( receipt->resources[i - 1u].binding == receipt->resources[i].binding
				&& receipt->resources[i - 1u].arrayElement >= receipt->resources[i].arrayElement ) ) ) )
			return qfalse;
	}
	return qtrue;
}

static qboolean ResourceReceiptExact( const ralMetalBindResourceReceipt_t *a,
		const ralMetalBindResourceReceipt_t *b ) {
	return ( a->binding == b->binding && a->arrayElement == b->arrayElement
		&& a->type == b->type && a->resourceIdentity == b->resourceIdentity
		&& a->resourceGeneration == b->resourceGeneration
		&& a->bufferOffset == b->bufferOffset && a->bufferRange == b->bufferRange )
		? qtrue : qfalse;
}

qboolean RalMetal_BindGroupReceiptExact( const ralMetalBindGroupReceipt_t *a,
		const ralMetalBindGroupReceipt_t *b ) {
	uint32_t i;
	if ( !GroupReceiptValid( a ) || !GroupReceiptValid( b )
			|| a->backendType != b->backendType || a->coreGeneration != b->coreGeneration
			|| a->layoutGeneration != b->layoutGeneration
			|| a->layoutIdentity != b->layoutIdentity || a->groupGeneration != b->groupGeneration
			|| a->groupIdentity != b->groupIdentity
			|| a->argumentBufferIdentity != b->argumentBufferIdentity
			|| a->argumentBufferBytes != b->argumentBufferBytes
			|| a->resourceCount != b->resourceCount ) return qfalse;
	for ( i = 0u; i < a->resourceCount; ++i )
		if ( !ResourceReceiptExact( &a->resources[i], &b->resources[i] ) ) return qfalse;
	return qtrue;
}

qboolean RalMetal_BindGroupMatchesReceipt( const ralMetalBindGroup_t *group,
		const ralMetalBindGroupReceipt_t *receipt ) {
	uint32_t i;
	if ( !group || !receipt || receipt->groupIdentity != (uintptr_t)group
			|| receipt->argumentBufferIdentity != (uintptr_t)(void *)group->argumentBuffer
			|| !RalMetal_BindGroupReceiptExact( receipt, &group->receipt ) ) return qfalse;
	for ( i = 0u; i < receipt->resourceCount; ++i )
		if ( receipt->resources[i].resourceIdentity
				!= (uintptr_t)(void *)group->nativeResources[i] ) return qfalse;
	return qtrue;
}

id<MTLBuffer> RalMetal_BindGroupNativeArgumentBuffer( ralMetalBindGroup_t *group ) {
	return group ? group->argumentBuffer : nil;
}

id RalMetal_BindGroupNativeResource( ralMetalBindGroup_t *group, uint32_t index ) {
	return group && index < group->receipt.resourceCount ? group->nativeResources[index] : nil;
}

void RalMetal_BindGroupUseFragmentResources( ralMetalBindGroup_t *group,
		id<MTLRenderCommandEncoder> encoder ) {
	uint32_t i;
	if ( !group || !encoder ) return;
	for ( i = 0u; i < group->receipt.resourceCount; ++i ) {
		MTLResourceUsage usage;
		if ( group->argumentClasses[i] == RAL_METAL_ARGUMENT_SAMPLER ) continue;
		usage = MTLResourceUsageRead;
		if ( group->receipt.resources[i].type == RAL_BIND_STORAGE_BUFFER
				|| group->receipt.resources[i].type == RAL_BIND_STORAGE_TEXTURE )
			usage |= MTLResourceUsageWrite;
		[encoder useResource:(id<MTLResource>)group->nativeResources[i]
			usage:usage stages:MTLRenderStageFragment];
	}
}

qboolean RalMetal_BindGroupCreate( ralMetalCore_t *core,
		const ralMetalCoreReceipt_t *coreReceipt, ralMetalBindLayout_t *layout,
		const ralMetalBindLayoutReceipt_t *layoutReceipt,
		const ralMetalBindResource_t *resources, uint32_t resourceCount,
		uint64_t generation, ralMetalBindGroup_t **outGroup,
		ralMetalBindGroupReceipt_t *outReceipt ) {
	ralMetalBindGroup_t *candidate;
	ralMetalBindGroupReceipt_t receipt;
	id<MTLDevice> device;
	uint32_t i, resourceCursor = 0u;
	if ( !core || !coreReceipt || !layout || !layoutReceipt || !resources
			|| !outGroup || !outReceipt || resourceCount == 0u
			|| resourceCount > RAL_METAL_BIND_MAX_RESOURCES
			|| generation == 0u || generation == UINT64_MAX
			|| !RalMetal_CoreMatchesReceipt( core, coreReceipt )
			|| !RalMetal_BindLayoutMatchesReceipt( layout, layoutReceipt )
			|| resourceCount != layoutReceipt->totalArgumentCount ) return qfalse;
	device = RalMetal_CoreNativeDevice( core );
	if ( !device || coreReceipt->backendIdentity != (uintptr_t)core
			|| coreReceipt->deviceIdentity != (uintptr_t)(void *)device
			|| coreReceipt->generation != layoutReceipt->coreGeneration ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_METAL_BIND_GROUP_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_METAL;
	receipt.coreGeneration = coreReceipt->generation;
	receipt.layoutGeneration = layoutReceipt->layoutGeneration;
	receipt.layoutIdentity = layoutReceipt->layoutIdentity;
	receipt.groupGeneration = generation;
	receipt.argumentBufferBytes = layoutReceipt->encodedLength;
	receipt.resourceCount = resourceCount;
	for ( i = 0u; i < layoutReceipt->entryCount; ++i ) {
		const ralMetalBindLayoutEntryReceipt_t *entry = &layoutReceipt->entries[i];
		uint32_t element;
		for ( element = 0u; element < entry->count; ++element, ++resourceCursor ) {
			const ralMetalBindResource_t *source = &resources[resourceCursor];
			ralMetalBindResourceReceipt_t *target = &receipt.resources[resourceCursor];
			if ( source->binding != entry->binding || source->arrayElement != element
					|| source->type != entry->type || !source->nativeResource
					|| source->resourceIdentity == (uintptr_t)0
					|| source->resourceIdentity != (uintptr_t)source->nativeResource
					|| source->resourceGeneration == 0u
					|| source->resourceGeneration == UINT64_MAX
					|| !NativeResourceMatches( entry->argumentClass,
						source->nativeResource ) ) return qfalse;
			if ( entry->argumentClass == RAL_METAL_ARGUMENT_BUFFER ) {
				id<MTLBuffer> buffer = (id<MTLBuffer>)source->nativeResource;
				if ( source->bufferRange == 0u || source->bufferOffset > (uint64_t)buffer.length
						|| source->bufferRange > (uint64_t)buffer.length - source->bufferOffset ) return qfalse;
			} else if ( source->bufferOffset != 0u || source->bufferRange != 0u ) return qfalse;
			target->binding = source->binding; target->arrayElement = source->arrayElement;
			target->type = source->type; target->resourceIdentity = source->resourceIdentity;
			target->resourceGeneration = source->resourceGeneration;
			target->bufferOffset = source->bufferOffset; target->bufferRange = source->bufferRange;
		}
	}
	@autoreleasepool {
		candidate = (ralMetalBindGroup_t *)calloc( 1u, sizeof( *candidate ) );
		if ( !candidate ) return qfalse;
		candidate->argumentBuffer = [device newBufferWithLength:(NSUInteger)receipt.argumentBufferBytes
			options:MTLResourceStorageModeShared];
		if ( !candidate->argumentBuffer ) { free( candidate ); return qfalse; }
		[layout->encoder setArgumentBuffer:candidate->argumentBuffer offset:0u];
		resourceCursor = 0u;
		for ( i = 0u; i < layoutReceipt->entryCount; ++i ) {
			const ralMetalBindLayoutEntryReceipt_t *entry = &layoutReceipt->entries[i];
			uint32_t element;
			for ( element = 0u; element < entry->count; ++element, ++resourceCursor ) {
				const ralMetalBindResource_t *source = &resources[resourceCursor];
				NSUInteger index = (NSUInteger)( entry->argumentIndex + element );
				candidate->nativeResources[resourceCursor] = (id)source->nativeResource;
				candidate->argumentClasses[resourceCursor] = entry->argumentClass;
				switch ( entry->argumentClass ) {
				case RAL_METAL_ARGUMENT_BUFFER:
					[layout->encoder setBuffer:(id<MTLBuffer>)source->nativeResource
						offset:(NSUInteger)source->bufferOffset atIndex:index]; break;
				case RAL_METAL_ARGUMENT_TEXTURE:
					[layout->encoder setTexture:(id<MTLTexture>)source->nativeResource atIndex:index]; break;
				case RAL_METAL_ARGUMENT_SAMPLER:
					[layout->encoder setSamplerState:(id<MTLSamplerState>)source->nativeResource atIndex:index]; break;
				}
			}
		}
		receipt.groupIdentity = (uintptr_t)candidate;
		receipt.argumentBufferIdentity = (uintptr_t)(void *)candidate->argumentBuffer;
		receipt.ready = qtrue;
		if ( !GroupReceiptValid( &receipt ) ) {
			[candidate->argumentBuffer release]; free( candidate ); return qfalse;
		}
		candidate->receipt = receipt;
	}
	*outGroup = candidate;
	*outReceipt = receipt;
	return qtrue;
}

void RalMetal_BindGroupDestroy( ralMetalBindGroup_t *group ) {
	if ( !group ) return;
	[group->argumentBuffer release];
	memset( group, 0, sizeof( *group ) );
	free( group );
}
