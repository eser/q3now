// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_vulkan_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #x ); return 1; \
} } while ( 0 )

int main( void ) {
	ralBackend_t backend;
	ralTexture_t *first, *second;
	ralTexture_t *volume, *compressed, *rgb;
	ralTextureView_t *borrowedView;
	ralTextureResourceReceipt_t receipt, bad, before;
	ralTextureCreateInfo_t volumeInfo;
	memset( &backend, 0, sizeof( backend ) );
	backend.type = RAL_BACKEND_VULKAN;
	backend.nextTextureGeneration = 40u;
	first = Ral_AdoptTextureExact( &backend, (void *)(uintptr_t)0x100u, NULL,
		RAL_FORMAT_R8G8B8A8_UNORM, 17u, 9u, VK_IMAGE_ASPECT_COLOR_BIT,
		RAL_TEXTURE_USAGE_COLOR_ATTACHMENT | RAL_TEXTURE_USAGE_TRANSFER_SRC, NULL );
	CHECK( first && backend.nextTextureGeneration == 41u );
	CHECK( Ral_TextureGetResourceReceipt( first, &receipt ) );
	CHECK( receipt.ready && receipt.imported
		&& receipt.textureIdentity == (uintptr_t)first
		&& receipt.resourceGeneration == 41u
		&& receipt.width == 17u && receipt.height == 9u
		&& receipt.usage == ( RAL_TEXTURE_USAGE_COLOR_ATTACHMENT
			| RAL_TEXTURE_USAGE_TRANSFER_SRC ) );
	CHECK( Ral_TextureResourceReceiptExact( &receipt, &receipt ) );
#define MUTATE(statement) do { bad = receipt; statement; \
	CHECK( !Ral_TextureResourceReceiptExact( &receipt, &bad ) ); } while ( 0 )
	MUTATE( bad.backendType = RAL_BACKEND_WEBGPU );
	MUTATE( bad.textureIdentity++ );
	MUTATE( bad.resourceGeneration++ );
	MUTATE( bad.format = RAL_FORMAT_B8G8R8A8_UNORM );
	MUTATE( bad.usage ^= RAL_TEXTURE_USAGE_TRANSFER_SRC );
	MUTATE( bad.width++ ); MUTATE( bad.height++ );
	MUTATE( bad.mipLevels++ ); MUTATE( bad.arrayLayers++ );
	MUTATE( bad.imported = qfalse ); MUTATE( bad.ready = qfalse );
#undef MUTATE
	second = Ral_AdoptTextureExact( &backend, (void *)(uintptr_t)0x101u, NULL,
		RAL_FORMAT_R8G8B8A8_UNORM, 17u, 9u, VK_IMAGE_ASPECT_COLOR_BIT,
		RAL_TEXTURE_USAGE_TRANSFER_SRC, NULL );
	CHECK( second && second->resourceGeneration == 42u );
	memset( &volumeInfo, 0, sizeof( volumeInfo ) );
	volumeInfo.type = RAL_TEXTURE_3D;
	volumeInfo.format = RAL_FORMAT_R8G8B8A8_UNORM;
	volumeInfo.width = 16u; volumeInfo.height = 8u;
	volumeInfo.depthOrArrayLayers = 4u;
	volumeInfo.mipLevels = 3u; volumeInfo.sampleCount = 1u;
	volumeInfo.usage = RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_DST;
	volume = Ral_AdoptTextureResourceExact( &backend,
		(void *)(uintptr_t)0x103u, (void *)(uintptr_t)0x203u,
		VK_IMAGE_ASPECT_COLOR_BIT, &volumeInfo );
	CHECK( volume && volume->type == RAL_TEXTURE_3D
		&& volume->depthOrArrayLayers == 4u && volume->arrayLayers == 1u
		&& volume->mipLevels == 3u );
	CHECK( Ral_TextureGetResourceReceipt( volume, &bad )
		&& bad.type == RAL_TEXTURE_3D && bad.mipLevels == 3u
		&& bad.arrayLayers == 1u );
	volumeInfo.mipLevels = 6u;
	CHECK( !Ral_AdoptTextureResourceExact( &backend,
		(void *)(uintptr_t)0x104u, NULL, VK_IMAGE_ASPECT_COLOR_BIT,
		&volumeInfo ) );
	volumeInfo.mipLevels = 3u;
	CHECK( !Ral_AdoptTextureResourceExact( &backend,
		(void *)(uintptr_t)0x104u, NULL, VK_IMAGE_ASPECT_DEPTH_BIT,
		&volumeInfo ) );
	memset( &volumeInfo, 0, sizeof( volumeInfo ) );
	volumeInfo.type = RAL_TEXTURE_CUBE;
	volumeInfo.format = RAL_FORMAT_BC7_SRGB;
	volumeInfo.width = volumeInfo.height = 32u;
	volumeInfo.depthOrArrayLayers = 1u;
	volumeInfo.mipLevels = 6u; volumeInfo.sampleCount = 1u;
	volumeInfo.usage = RAL_TEXTURE_USAGE_SAMPLED | RAL_TEXTURE_USAGE_TRANSFER_DST;
	compressed = Ral_AdoptTextureResourceExact( &backend,
		(void *)(uintptr_t)0x105u, NULL, VK_IMAGE_ASPECT_COLOR_BIT,
		&volumeInfo );
	CHECK( compressed && compressed->vkFormat == VK_FORMAT_BC7_SRGB_BLOCK
		&& compressed->arrayLayers == 6u );
	volumeInfo.type = RAL_TEXTURE_2D;
	volumeInfo.format = RAL_FORMAT_R8G8B8_UNORM;
	volumeInfo.width = 9u; volumeInfo.height = 5u;
	volumeInfo.mipLevels = 1u;
	rgb = Ral_AdoptTextureResourceExact( &backend,
		(void *)(uintptr_t)0x106u, NULL, VK_IMAGE_ASPECT_COLOR_BIT,
		&volumeInfo );
	CHECK( rgb && rgb->vkFormat == VK_FORMAT_R8G8B8_UNORM );
	borrowedView = Ral_AdoptTextureViewExact( &backend, first,
		(void *)(uintptr_t)0x200u );
	CHECK( borrowedView
		&& Ral_GetTextureViewHandle( borrowedView ) == (void *)(uintptr_t)0x200u
		&& borrowedView->texture == first && borrowedView->ownsView == qfalse );
	CHECK( !Ral_AdoptTextureViewExact( NULL, first,
		(void *)(uintptr_t)0x201u ) );
	CHECK( !Ral_AdoptTextureViewExact( &backend, NULL,
		(void *)(uintptr_t)0x201u ) );
	CHECK( !Ral_AdoptTextureViewExact( &backend, first, NULL ) );
	Ral_DestroyTextureView( borrowedView );
	Ral_DestroyTexture( rgb );
	Ral_DestroyTexture( compressed );
	Ral_DestroyTexture( volume );
	memset( &before, 0x5a, sizeof( before ) ); bad = before;
	first->resourceGeneration = 0u;
	CHECK( !Ral_TextureGetResourceReceipt( first, &bad )
		&& !memcmp( &bad, &before, sizeof( bad ) ) );
	first->resourceGeneration = 41u;
	backend.nextTextureGeneration = UINT64_MAX - 1u;
	CHECK( !Ral_AdoptTextureExact( &backend, (void *)(uintptr_t)0x102u, NULL,
		RAL_FORMAT_R8G8B8A8_UNORM, 1u, 1u, VK_IMAGE_ASPECT_COLOR_BIT,
		RAL_TEXTURE_USAGE_TRANSFER_SRC, NULL ) );
	Ral_DestroyTexture( second );
	Ral_DestroyTexture( first );
	puts( "ral texture resource receipt: PASS" );
	return 0;
}
