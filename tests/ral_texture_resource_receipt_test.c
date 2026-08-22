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
	ralTextureResourceReceipt_t receipt, bad, before;
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
