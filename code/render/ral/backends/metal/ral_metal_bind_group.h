// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_METAL_BIND_GROUP_H
#define WIRED_RAL_METAL_BIND_GROUP_H

#include "ral_metal_core.h"
#include "ral_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_METAL_BIND_GROUP_SCHEMA_VERSION 1u
#define RAL_METAL_BIND_MAX_ENTRIES 16u
#define RAL_METAL_BIND_MAX_RESOURCES 64u

typedef enum {
	RAL_METAL_ARGUMENT_BUFFER = 1,
	RAL_METAL_ARGUMENT_TEXTURE,
	RAL_METAL_ARGUMENT_SAMPLER
} ralMetalArgumentClass_t;

typedef struct {
	uint32_t binding;
	ralBindType_t type;
	uint32_t count;
	uint32_t stageFlags;
	ralBindTextureViewType_t textureViewType;
	uint32_t argumentIndex;
	ralMetalArgumentClass_t argumentClass;
} ralMetalBindLayoutEntryReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t layoutGeneration;
	uintptr_t layoutIdentity;
	uint32_t entryCount;
	uint32_t totalArgumentCount;
	uint64_t encodedLength;
	uint64_t alignment;
	ralMetalBindLayoutEntryReceipt_t entries[RAL_METAL_BIND_MAX_ENTRIES];
	qboolean ready;
} ralMetalBindLayoutReceipt_t;

typedef struct {
	uint32_t binding;
	uint32_t arrayElement;
	ralBindType_t type;
	uintptr_t resourceIdentity;
	uint64_t resourceGeneration;
	void *nativeResource;
	uint64_t bufferOffset;
	uint64_t bufferRange;
} ralMetalBindResource_t;

// Resources are borrowed: their exact native objects and generations must
// remain live until all command buffers using the group have completed.  The
// group owns only its Metal argument buffer; receipts expose opaque identities,
// never Objective-C/Metal types.

typedef struct {
	uint32_t binding;
	uint32_t arrayElement;
	ralBindType_t type;
	uintptr_t resourceIdentity;
	uint64_t resourceGeneration;
	uint64_t bufferOffset;
	uint64_t bufferRange;
} ralMetalBindResourceReceipt_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t coreGeneration;
	uint64_t layoutGeneration;
	uintptr_t layoutIdentity;
	uint64_t groupGeneration;
	uintptr_t groupIdentity;
	uintptr_t argumentBufferIdentity;
	uint64_t argumentBufferBytes;
	uint32_t resourceCount;
	ralMetalBindResourceReceipt_t resources[RAL_METAL_BIND_MAX_RESOURCES];
	qboolean ready;
} ralMetalBindGroupReceipt_t;

typedef struct ralMetalBindLayout_s ralMetalBindLayout_t;
typedef struct ralMetalBindGroup_s ralMetalBindGroup_t;

qboolean RalMetal_BindLayoutCreate( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt,
	const ralBindGroupLayoutCreateInfo_t *createInfo, uint64_t generation,
	ralMetalBindLayout_t **outLayout, ralMetalBindLayoutReceipt_t *outReceipt );
void RalMetal_BindLayoutDestroy( ralMetalBindLayout_t *layout );
qboolean RalMetal_BindLayoutReceiptExact( const ralMetalBindLayoutReceipt_t *a,
	const ralMetalBindLayoutReceipt_t *b );
qboolean RalMetal_BindLayoutMatchesReceipt( const ralMetalBindLayout_t *layout,
	const ralMetalBindLayoutReceipt_t *receipt );

qboolean RalMetal_BindGroupCreate( ralMetalCore_t *core,
	const ralMetalCoreReceipt_t *coreReceipt, ralMetalBindLayout_t *layout,
	const ralMetalBindLayoutReceipt_t *layoutReceipt,
	const ralMetalBindResource_t *resources, uint32_t resourceCount,
	uint64_t generation, ralMetalBindGroup_t **outGroup,
	ralMetalBindGroupReceipt_t *outReceipt );
void RalMetal_BindGroupDestroy( ralMetalBindGroup_t *group );
qboolean RalMetal_BindGroupReceiptExact( const ralMetalBindGroupReceipt_t *a,
	const ralMetalBindGroupReceipt_t *b );
qboolean RalMetal_BindGroupMatchesReceipt( const ralMetalBindGroup_t *group,
	const ralMetalBindGroupReceipt_t *receipt );

#ifdef __cplusplus
}
#endif

#endif
