// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_ENTITY_METADATA_H
#define WIRED_ENTITY_METADATA_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRED_METADATA_SCHEMA_VERSION 1u
#define WIRED_METADATA_REGISTRY_SCHEMA_VERSION 1u

typedef enum {
	WIRED_METADATA_INT = 1,
	WIRED_METADATA_FLOAT,
	WIRED_METADATA_STRING,
	WIRED_METADATA_VEC3,
	WIRED_METADATA_ANGLE_YAW
} wiredMetadataValueType_t;

typedef enum {
	WIRED_METADATA_PARSE_STRICT = 1,
	WIRED_METADATA_PARSE_LEGACY_PREFIX
} wiredMetadataParsePolicy_t;

typedef enum {
	WIRED_METADATA_PERSIST_NONE = 0,
	WIRED_METADATA_PERSIST_SAVE,
	WIRED_METADATA_PERSIST_CROSS_LEVEL
} wiredMetadataPersistence_t;

enum {
	WIRED_METADATA_INSPECT = 1u << 0,
	WIRED_METADATA_EVENT_READ = 1u << 1,
	WIRED_METADATA_EVENT_WRITE = 1u << 2
};

typedef struct {
	uint32_t schemaVersion;
	uint32_t stableId;
	const char *name;
	const char *alias;
	wiredMetadataValueType_t valueType;
	wiredMetadataParsePolicy_t parsePolicy;
	wiredMetadataPersistence_t persistence;
	uint32_t policyFlags;
	size_t offset;
	size_t byteSize;
	const char *defaultValue;
} wiredMetadataField_t;

typedef struct {
	uint32_t schemaVersion;
	const wiredMetadataField_t *fields;
	uint32_t fieldCount;
	uint64_t identityHash;
} wiredMetadataRegistry_t;

typedef char *( *wiredMetadataInternFn )( const char *value );

qboolean WiredMetadata_RegistryBuild( const wiredMetadataField_t *fields,
	uint32_t fieldCount, wiredMetadataRegistry_t *outRegistry );
qboolean WiredMetadata_RegistryValid( const wiredMetadataRegistry_t *registry );
const wiredMetadataField_t *WiredMetadata_Find(
	const wiredMetadataRegistry_t *registry, const char *name );
qboolean WiredMetadata_Parse( const wiredMetadataField_t *field, void *base,
	const char *value, wiredMetadataInternFn intern );
qboolean WiredMetadata_Inspect( const wiredMetadataField_t *field,
	const void *base, char *destination, size_t capacity );

#ifdef __cplusplus
}
#endif

#endif
