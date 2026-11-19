// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RAL_MATERIAL_PACKAGE_H
#define WIRED_RAL_MATERIAL_PACKAGE_H

#include "ral_material_cook.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_MATERIAL_PACKAGE_SCHEMA_VERSION 1u
#define RAL_MATERIAL_PACKAGE_MAX_ENTRIES 512u
#define RAL_MATERIAL_PACKAGE_KEY_TEXT_CAPACITY 48u
#define RAL_MATERIAL_PROJECT_REFERENCE_SCHEMA_VERSION 1u
#define RAL_MATERIAL_PROJECT_REFERENCE_KEY_TEXT_CAPACITY 64u

typedef struct {
	char semanticName[MAX_QPATH];
	uint64_t semanticHash;
	uint64_t dependencyHash;
	uint64_t sourceArtifactHash;
	uint64_t cookedArtifactHash;
	ralMaterialProvenance_t provenance;
	uint32_t reserved;
} ralMaterialPackageEntry_t;

typedef struct {
	uint32_t schemaVersion;
	uint32_t entryCount;
	uint64_t generation;
	ralMaterialPackageEntry_t entries[RAL_MATERIAL_PACKAGE_MAX_ENTRIES];
	uint64_t artifactHash;
	qboolean ready;
} ralMaterialPackageReceipt_t;

typedef struct {
	qboolean (*writeAtomic)( void *context, uint64_t artifactHash,
		const char *keyText, const void *bytes, uint64_t byteLength );
	qboolean (*read)( void *context, uint64_t artifactHash,
		const char *keyText, void *bytes, uint64_t capacity,
		uint64_t *outByteLength );
} ralMaterialPackageCacheOps_t;

typedef struct {
	uint32_t schemaVersion;
	uint32_t mapChecksum;
	uint32_t materialCount;
	uint32_t reserved;
	uint64_t generation;
	uint64_t manifestArtifactHash;
	uint64_t corpusHash;
	uint64_t mapNameHash;
	char mapName[MAX_QPATH];
	uint64_t artifactHash;
	qboolean ready;
} ralMaterialProjectReferenceReceipt_t;

typedef struct {
	qboolean (*writeAtomic)( void *context, const char *keyText,
		const void *bytes, uint64_t byteLength );
	qboolean (*read)( void *context, const char *keyText, void *bytes,
		uint64_t capacity, uint64_t *outByteLength );
} ralMaterialProjectReferenceOps_t;

qboolean Ral_MaterialPackageEntryFromCook(
	const ralMaterialCookReceipt_t *cooked,
	ralMaterialPackageEntry_t *outEntry );
qboolean Ral_MaterialPackageSeal( uint64_t generation,
	const ralMaterialPackageEntry_t *entries, uint32_t entryCount,
	ralMaterialPackageReceipt_t *outReceipt );
qboolean Ral_MaterialPackageReceiptValid(
	const ralMaterialPackageReceipt_t *receipt );
qboolean Ral_MaterialPackageReceiptExact(
	const ralMaterialPackageReceipt_t *a,
	const ralMaterialPackageReceipt_t *b );
qboolean Ral_MaterialPackageLookup(
	const ralMaterialPackageReceipt_t *receipt, const char *semanticName,
	ralMaterialPackageEntry_t *outEntry );
qboolean Ral_MaterialPackageCacheKeyText( uint64_t artifactHash,
	char *outText, uint32_t capacity );
qboolean Ral_MaterialPackageCacheStore(
	const ralMaterialPackageCacheOps_t *ops, void *context,
	const ralMaterialPackageReceipt_t *receipt );
qboolean Ral_MaterialPackageCacheLoad(
	const ralMaterialPackageCacheOps_t *ops, void *context,
	uint64_t expectedArtifactHash, ralMaterialPackageReceipt_t *outReceipt );

qboolean Ral_MaterialProjectReferenceSeal( const char *mapName,
	uint32_t mapChecksum, uint64_t generation, uint32_t materialCount,
	uint64_t corpusHash, uint64_t manifestArtifactHash,
	ralMaterialProjectReferenceReceipt_t *outReceipt );
qboolean Ral_MaterialProjectReferenceReceiptValid(
	const ralMaterialProjectReferenceReceipt_t *receipt );
qboolean Ral_MaterialProjectReferenceKeyText( const char *mapName,
	uint32_t mapChecksum, char *outText, uint32_t capacity );
qboolean Ral_MaterialProjectReferenceStore(
	const ralMaterialProjectReferenceOps_t *ops, void *context,
	const ralMaterialProjectReferenceReceipt_t *receipt );
qboolean Ral_MaterialProjectReferenceLoad(
	const ralMaterialProjectReferenceOps_t *ops, void *context,
	const char *expectedMapName, uint32_t expectedMapChecksum,
	ralMaterialProjectReferenceReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif
#endif
