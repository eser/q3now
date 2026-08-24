// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef WIRED_VK_TEMPORAL_GENERIC_CATALOG_H
#define WIRED_VK_TEMPORAL_GENERIC_CATALOG_H

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "../include/vulkan/vulkan.h"
#include "vk_temporal_shader_cohort.h"

typedef enum {
	VK_TEMPORAL_GENERIC_PLAIN = 0,
	VK_TEMPORAL_GENERIC_IDENT = 1,
	VK_TEMPORAL_GENERIC_FIXED = 2,
	VK_TEMPORAL_GENERIC_ENT = 3,
	VK_TEMPORAL_GENERIC_CL = 4
} vkTemporalGenericFamily_t;

typedef struct {
	uint32_t textureCount;
	vkTemporalGenericFamily_t family;
	qboolean environment;
	qboolean shaderFog;
} vkTemporalGenericKey_t;

typedef struct {
	vkTemporalGenericKey_t key;
	vkTemporalShaderBlob_t ordinaryVertex;
	vkTemporalShaderBlob_t ordinaryFragment;
	vkTemporalShaderBlob_t temporalVertex;
	vkTemporalShaderBlob_t temporalWriteFragment;
	vkTemporalShaderBlob_t temporalInvalidateFragment;
} vkTemporalGenericCatalogEntry_t;

enum {
	VK_TEMPORAL_GENERIC_CATALOG_COUNT = 40,
	VK_TEMPORAL_GENERIC_CATALOG_GENERATION = 1
};

typedef struct {
	VkShaderModule module;
	vkTemporalShaderBlob_t blob;
} vkTemporalShaderModuleRecord_t;

typedef struct {
	const vkTemporalShaderModuleRecord_t *records;
	uint32_t count;
	uint32_t generation;
	qboolean ready;
	qboolean complete;
} vkTemporalShaderModuleRegistryView_t;

// Pure definition-only selector over the generated exact 40-pair catalog.
// Failure leaves out byte-identical.
qboolean VK_TemporalGenericCatalogSelect( const vkTemporalGenericKey_t *key,
	vkTemporalGenericCatalogEntry_t *out );

// Reverse lookup over the generated SSOT. The ordinary VS/FS pair must name
// exactly one catalog entry by pointer+size; failure leaves both outputs
// byte-identical. Catalog ids are 1-based so zero remains the invalid/unset
// receipt sentinel. No blob pointer is retained by the returned key/id.
qboolean VK_TemporalGenericCatalogIdentify( vkTemporalShaderBlob_t ordinaryVertex,
	vkTemporalShaderBlob_t ordinaryFragment, vkTemporalGenericKey_t *outKey,
	uint32_t *outCatalogId );
qboolean VK_TemporalGenericCatalogKeyId( const vkTemporalGenericKey_t *key,
	uint32_t *outCatalogId );

// Resolve the ordinary stages for one catalog entry against the complete,
// current renderer module registry. Matching is canonical blob pointer+size,
// not byte content; both stages must have exactly one distinct live module.
// Failure leaves every output byte-identical.
qboolean VK_TemporalGenericCatalogResolveOrdinaryModules(
	const vkTemporalGenericCatalogEntry_t *entry,
	const vkTemporalShaderModuleRegistryView_t *registry,
	VkShaderModule *outVertex, VkShaderModule *outFragment,
	uint32_t *outRegistryGeneration );

#endif
