// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

static uint32_t ralVk_TransientFullMipChain( uint32_t w, uint32_t h, uint32_t d ) {
	uint32_t levels = 1u;
	while ( w > 1u || h > 1u || d > 1u ) {
		if ( w > 1u ) w >>= 1u;
		if ( h > 1u ) h >>= 1u;
		if ( d > 1u ) d >>= 1u;
		levels++;
	}
	return levels;
}

static VkImageAspectFlags ralVk_TransientAspect( ralFormat_t format ) {
	switch ( format ) {
	case RAL_FORMAT_D16_UNORM:
	case RAL_FORMAT_X8_D24_UNORM:
	case RAL_FORMAT_D32_SFLOAT: return VK_IMAGE_ASPECT_DEPTH_BIT;
	case RAL_FORMAT_D24_UNORM_S8_UINT:
	case RAL_FORMAT_D16_UNORM_S8_UINT:
	case RAL_FORMAT_D32_SFLOAT_S8_UINT:
		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	default: return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

static VkImageType ralVk_TransientImageType( ralTextureType_t type ) {
	return type == RAL_TEXTURE_1D ? VK_IMAGE_TYPE_1D
		: type == RAL_TEXTURE_3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
}

static VkImageViewType ralVk_TransientViewType( ralTextureType_t type ) {
	switch ( type ) {
	case RAL_TEXTURE_1D: return VK_IMAGE_VIEW_TYPE_1D;
	case RAL_TEXTURE_3D: return VK_IMAGE_VIEW_TYPE_3D;
	case RAL_TEXTURE_CUBE: return VK_IMAGE_VIEW_TYPE_CUBE;
	case RAL_TEXTURE_2D_ARRAY: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	case RAL_TEXTURE_CUBE_ARRAY: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	case RAL_TEXTURE_2D:
	default: return VK_IMAGE_VIEW_TYPE_2D;
	}
}

static qboolean ralVk_TransientKeyExact( const ralVkTransientImageKey_t *a,
		const ralVkTransientImageKey_t *b ) {
	uint32_t i;
	if ( !a || !b || a->imageType != b->imageType || a->format != b->format
			|| a->extent.width != b->extent.width || a->extent.height != b->extent.height
			|| a->extent.depth != b->extent.depth || a->mipLevels != b->mipLevels
			|| a->arrayLayers != b->arrayLayers || a->samples != b->samples
			|| a->usage != b->usage || a->sharingMode != b->sharingMode
			|| a->queueFamilyIndexCount != b->queueFamilyIndexCount
			|| a->memoryTypeBits != b->memoryTypeBits ) return qfalse;
	for ( i = 0u; i < a->queueFamilyIndexCount; ++i )
		if ( a->queueFamilyIndices[i] != b->queueFamilyIndices[i] ) return qfalse;
	return qtrue;
}

static uint64_t ralVk_TransientHashU64( uint64_t hash, uint64_t value ) {
	uint32_t i;
	for ( i = 0u; i < 8u; ++i ) {
		hash ^= ( value >> ( i * 8u ) ) & 0xffu;
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static uint64_t ralVk_TransientKeyHash( const ralVkTransientImageKey_t *key ) {
	uint64_t hash = UINT64_C(1469598103934665603);
	uint32_t i;
	hash = ralVk_TransientHashU64( hash, key->imageType );
	hash = ralVk_TransientHashU64( hash, key->format );
	hash = ralVk_TransientHashU64( hash, key->extent.width );
	hash = ralVk_TransientHashU64( hash, key->extent.height );
	hash = ralVk_TransientHashU64( hash, key->extent.depth );
	hash = ralVk_TransientHashU64( hash, key->mipLevels );
	hash = ralVk_TransientHashU64( hash, key->arrayLayers );
	hash = ralVk_TransientHashU64( hash, key->samples );
	hash = ralVk_TransientHashU64( hash, key->usage );
	hash = ralVk_TransientHashU64( hash, key->sharingMode );
	hash = ralVk_TransientHashU64( hash, key->memoryTypeBits );
	for ( i = 0u; i < key->queueFamilyIndexCount; ++i )
		hash = ralVk_TransientHashU64( hash, key->queueFamilyIndices[i] );
	return hash ? hash : UINT64_C(1);
}

static qboolean ralVk_TransientCreateUnbound( void *context,
		const ralTransientTextureRequest_t *request, uint32_t requestIndex,
		ralTexture_t **outTexture, ralTransientTextureCandidateFacts_t *outFacts ) {
	ralBackend_t *b = (ralBackend_t *)context;
	const ralTextureCreateInfo_t *ci = &request->texture;
	VkImageCreateInfo ici;
	ralTexture_t *tex;
	uint32_t qfamCount = 0u, layers, depth, mips;
	(void)requestIndex;
	if ( !b || !outTexture || !outFacts ) return qfalse;
	tex = (ralTexture_t *)malloc( sizeof( *tex ) );
	if ( !tex ) return qfalse;
	RAL_ZERO( *tex );
	tex->header.refCount = 1;
	tex->backend = b;
	tex->vkFormat = ralVk_TranslateFormat( ci->format );
	tex->ralFormat = ci->format;
	tex->type = ci->type;
	tex->usage = ci->usage;
	tex->width = ci->width;
	tex->height = ci->height;
	tex->depthOrArrayLayers = ci->depthOrArrayLayers;
	tex->sampleCount = ci->sampleCount ? ci->sampleCount : 1u;
	tex->aspect = ralVk_TransientAspect( ci->format );
	tex->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	tex->ownsImage = qtrue;
	tex->transientCohortOwned = qtrue;
	tex->portableStateKnown = qtrue;
	tex->portableState.usage = RAL_RESOURCE_USAGE_UNDEFINED;
	tex->portableOwnerQueue = RAL_QUEUE_GRAPHICS;
	Ral_QueueTransferLifecycleInit( &tex->queueTransfer );
	if ( tex->vkFormat == VK_FORMAT_UNDEFINED ) { free( tex ); return qfalse; }
	switch ( ci->type ) {
	case RAL_TEXTURE_CUBE: layers = 6u; depth = 1u; break;
	case RAL_TEXTURE_CUBE_ARRAY: layers = 6u * ci->depthOrArrayLayers; depth = 1u; break;
	case RAL_TEXTURE_2D_ARRAY: layers = ci->depthOrArrayLayers; depth = 1u; break;
	case RAL_TEXTURE_3D: layers = 1u; depth = ci->depthOrArrayLayers; break;
	default: layers = 1u; depth = 1u; break;
	}
	mips = ci->mipLevels ? ci->mipLevels
		: ralVk_TransientFullMipChain( ci->width, ci->height, depth );
	tex->mipLevels = mips;
	tex->arrayLayers = layers;
	RAL_ZERO( ici );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.flags = VK_IMAGE_CREATE_ALIAS_BIT;
	if ( ci->type == RAL_TEXTURE_CUBE || ci->type == RAL_TEXTURE_CUBE_ARRAY )
		ici.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	ici.imageType = ralVk_TransientImageType( ci->type );
	ici.format = tex->vkFormat;
	ici.extent.width = ci->width;
	ici.extent.height = ci->height;
	ici.extent.depth = depth;
	ici.mipLevels = mips;
	ici.arrayLayers = layers;
	ici.samples = (VkSampleCountFlagBits)tex->sampleCount;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = ralVk_TextureUsage( ci->usage );
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if ( ( ci->concurrentGraphicsCompute && b->computeFamily != b->graphicsFamily )
			|| ( ci->concurrentGraphicsTransfer && b->transferFamily != b->graphicsFamily ) ) {
		tex->transientKey.queueFamilyIndices[qfamCount++] = b->graphicsFamily;
		if ( ci->concurrentGraphicsCompute && b->computeFamily != b->graphicsFamily )
			tex->transientKey.queueFamilyIndices[qfamCount++] = b->computeFamily;
		if ( ci->concurrentGraphicsTransfer && b->transferFamily != b->graphicsFamily
				&& b->transferFamily != b->computeFamily )
			tex->transientKey.queueFamilyIndices[qfamCount++] = b->transferFamily;
	}
	if ( qfamCount >= 2u ) {
		ici.sharingMode = VK_SHARING_MODE_CONCURRENT;
		ici.queueFamilyIndexCount = qfamCount;
		ici.pQueueFamilyIndices = tex->transientKey.queueFamilyIndices;
	}
	tex->concurrentTransfer = ci->concurrentGraphicsTransfer
		&& b->transferFamily != b->graphicsFamily
		&& ici.sharingMode == VK_SHARING_MODE_CONCURRENT ? qtrue : qfalse;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( b->vk.CreateImage( b->device, &ici, NULL, &tex->image ) != VK_SUCCESS ) {
		free( tex ); return qfalse;
	}
	b->vk.GetImageMemoryRequirements( b->device, tex->image, &tex->transientRequirements );
	tex->transientKey.imageType = ici.imageType;
	tex->transientKey.format = ici.format;
	tex->transientKey.extent = ici.extent;
	tex->transientKey.mipLevels = ici.mipLevels;
	tex->transientKey.arrayLayers = ici.arrayLayers;
	tex->transientKey.samples = ici.samples;
	tex->transientKey.usage = ici.usage;
	tex->transientKey.sharingMode = ici.sharingMode;
	tex->transientKey.queueFamilyIndexCount = ici.queueFamilyIndexCount;
	tex->transientKey.memoryTypeBits = tex->transientRequirements.memoryTypeBits;
	outFacts->size = tex->transientRequirements.size;
	outFacts->alignment = tex->transientRequirements.alignment;
	outFacts->compatibilityKey = ralVk_TransientKeyHash( &tex->transientKey );
	*outTexture = tex;
	return qtrue;
}

static qboolean ralVk_TransientAllocateSlot( void *context,
		const ralTexture_t *representative, uint32_t slotIndex,
		const ralTransientSlot_t *slot, void **outAllocation ) {
	ralBackend_t *b = (ralBackend_t *)context;
	VkMemoryRequirements requirements;
	ralVkAllocation_t *allocation;
	(void)slotIndex;
	if ( !b || !representative || !slot || !outAllocation
			|| representative->backend != b || !representative->transientCohortOwned ) return qfalse;
	requirements = representative->transientRequirements;
	requirements.size = slot->committedSize;
	requirements.alignment = slot->alignment;
	allocation = ralVk_Alloc( b, requirements, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,
		RAL_ALLOCATION_TRANSIENT, RAL_ALLOCATION_RESIDENCY_TRANSIENT,
		(uintptr_t)representative, RAL_VK_ALLOC_RESOURCE_TRANSIENT );
	if ( !allocation ) return qfalse;
	allocation->transientAlias = qtrue;
	allocation->transientKey = representative->transientKey;
	*outAllocation = allocation;
	return qtrue;
}

static qboolean ralVk_TransientBindComplete( void *context, ralTexture_t *texture,
		void *opaqueAllocation, uint32_t requestIndex, uint32_t slotIndex ) {
	ralBackend_t *b = (ralBackend_t *)context;
	ralVkAllocation_t *allocation = (ralVkAllocation_t *)opaqueAllocation;
	VkImageViewCreateInfo vci;
	(void)requestIndex; (void)slotIndex;
	if ( !b || !texture || !allocation || texture->backend != b || allocation->backend != b
			|| !texture->transientCohortOwned || !allocation->transientAlias
			|| !ralVk_TransientKeyExact( &texture->transientKey, &allocation->transientKey )
			|| texture->transientRequirements.size > allocation->size
			|| ( texture->transientRequirements.memoryTypeBits
				& ( 1u << allocation->memoryTypeIndex ) ) == 0u ) return qfalse;
	if ( b->vk.BindImageMemory( b->device, texture->image, allocation->memory, 0u ) != VK_SUCCESS )
		return qfalse;
	RAL_ZERO( vci );
	vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image = texture->image;
	vci.viewType = ralVk_TransientViewType( texture->type );
	vci.format = texture->vkFormat;
	vci.subresourceRange.aspectMask = texture->aspect;
	vci.subresourceRange.levelCount = texture->mipLevels;
	vci.subresourceRange.layerCount = texture->arrayLayers;
	if ( b->vk.CreateImageView( b->device, &vci, NULL, &texture->defaultView ) != VK_SUCCESS )
		return qfalse;
	texture->alloc = allocation;
	texture->resourceGeneration = allocation->receipt.allocationGeneration;
	return qtrue;
}

static qboolean ralVk_TransientCandidateAllowed( void *context,
		ralTransientPhysicalRole_t role, uintptr_t identity ) {
	ralBackend_t *b = (ralBackend_t *)context;
	(void)role;
	return b && identity != (uintptr_t)0 && identity != (uintptr_t)b ? qtrue : qfalse;
}

static qboolean ralVk_TransientGetAllocationReceipt( void *context,
		const void *opaqueAllocation, ralAllocationReceipt_t *out ) {
	const ralBackend_t *b = (const ralBackend_t *)context;
	const ralVkAllocation_t *allocation = (const ralVkAllocation_t *)opaqueAllocation;
	ralAllocationReceipt_t candidate;
	if ( !b || !allocation || !out || allocation->backend != b
			|| !allocation->transientAlias
			|| !Ral_AllocationReceiptExact( &allocation->receipt, &allocation->receipt ) ) return qfalse;
	candidate = allocation->receipt;
	*out = candidate;
	return qtrue;
}

static void ralVk_TransientDestroyTextureCandidate( void *context, ralTexture_t *texture ) {
	ralBackend_t *b = (ralBackend_t *)context;
	if ( !b || !texture ) return;
	if ( texture->defaultView ) b->vk.DestroyImageView( b->device, texture->defaultView, NULL );
	if ( texture->image ) b->vk.DestroyImage( b->device, texture->image, NULL );
	free( texture );
}

static void ralVk_TransientDestroyAllocationCandidate( void *context, void *allocation ) {
	if ( context && allocation ) ralVk_Free( (ralBackend_t *)context, (ralVkAllocation_t *)allocation );
}

static void ralVk_TransientRetireTexture( void *context, ralTexture_t *texture ) {
	ralBackend_t *b = (ralBackend_t *)context;
	if ( !b || !texture ) return;
	ralVk_DeferDestroy( b, RAL_RES_IMAGE_AND_VIEW, RAL_VK_H2U( texture->image ),
		RAL_VK_H2U( texture->defaultView ), NULL );
	free( texture );
}

static void ralVk_TransientRetireAllocation( void *context, void *allocation ) {
	if ( context && allocation )
		ralVk_DeferDestroy( (ralBackend_t *)context, RAL_RES_ALLOCATION_ONLY, 0u, 0u,
			(ralVkAllocation_t *)allocation );
}

ralTransientTextureCohort_t *Ral_CreateTransientTextureCohort(
		ralBackend_t *backend, const ralTransientTextureCohortCreateInfo_t *ci ) {
	ralTransientTextureOps_t ops;
	ralTransientTextureCohort_t *cohort = NULL;
	if ( !backend || !ci || backend->type != RAL_BACKEND_VULKAN
			|| ci->policy.backendType != RAL_BACKEND_VULKAN
			|| ci->policy.explicitAliasing != qtrue ) return NULL;
	RAL_ZERO( ops );
	ops.createUnbound = ralVk_TransientCreateUnbound;
	ops.allocateSlot = ralVk_TransientAllocateSlot;
	ops.bindComplete = ralVk_TransientBindComplete;
	ops.candidateAllowed = ralVk_TransientCandidateAllowed;
	ops.getAllocationReceipt = ralVk_TransientGetAllocationReceipt;
	ops.destroyTextureCandidate = ralVk_TransientDestroyTextureCandidate;
	ops.destroyAllocationCandidate = ralVk_TransientDestroyAllocationCandidate;
	ops.retireTexture = ralVk_TransientRetireTexture;
	ops.retireAllocation = ralVk_TransientRetireAllocation;
	return Ral_TransientTextureCohortCreateWithOps( ci, &ops, backend, &cohort )
		? cohort : NULL;
}
