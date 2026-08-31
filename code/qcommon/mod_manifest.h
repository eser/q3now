// SPDX-License-Identifier: GPL-2.0-or-later
// Runtime view of the canonical wired.sw3z.package/v1 manifest.

#ifndef WIRED_MOD_MANIFEST_H
#define WIRED_MOD_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WIRED_PACKAGE_SCHEMA_V1 "wired.sw3z.package/v1"
#define WIRED_PACKAGE_ID_MAX 64
#define WIRED_PACKAGE_VERSION_MAX 64
#define WIRED_PACKAGE_TARGET_MAX 32
#define WIRED_PACKAGE_ABI_MAX 64
#define WIRED_PACKAGE_PATH_MAX 256
#define WIRED_PACKAGE_DEPENDENCY_MAX 32
#define WIRED_PACKAGE_PROVIDE_MAX 32
#define WIRED_PACKAGE_TARGET_COUNT_MAX 8
#define WIRED_PACKAGE_FILE_MAX 64
#define WIRED_PACKAGE_SET_MAX 64
#define WIRED_PACKAGE_RECEIPT_BYTES_MAX (64 * 1024)

typedef enum {
	WIRED_PACKAGE_ROLE_INVALID = 0,
	WIRED_PACKAGE_ROLE_RUNTIME,
	WIRED_PACKAGE_ROLE_TOOLCHAIN,
	WIRED_PACKAGE_ROLE_SERVER,
	WIRED_PACKAGE_ROLE_CLIENT,
	WIRED_PACKAGE_ROLE_SHARED,
	WIRED_PACKAGE_ROLE_COSMETIC
} wiredPackageRole_t;

typedef struct {
	char id[WIRED_PACKAGE_ID_MAX];
	char version[WIRED_PACKAGE_VERSION_MAX];
	bool optional;
} wiredPackageDependency_t;

typedef struct {
	char path[WIRED_PACKAGE_PATH_MAX];
	char sha256[65];
	uint64_t size;
	bool shared;
} wiredPackageFile_t;

typedef struct {
	char schema[sizeof( WIRED_PACKAGE_SCHEMA_V1 )];
	char id[WIRED_PACKAGE_ID_MAX];
	char version[WIRED_PACKAGE_VERSION_MAX];
	char owner[WIRED_PACKAGE_ID_MAX];
	wiredPackageRole_t role;
	wiredPackageDependency_t depends[WIRED_PACKAGE_DEPENDENCY_MAX];
	size_t dependencyCount;
	char provides[WIRED_PACKAGE_PROVIDE_MAX][WIRED_PACKAGE_ID_MAX];
	size_t provideCount;
	char os[WIRED_PACKAGE_TARGET_COUNT_MAX][WIRED_PACKAGE_TARGET_MAX];
	size_t osCount;
	char arch[WIRED_PACKAGE_TARGET_COUNT_MAX][WIRED_PACKAGE_TARGET_MAX];
	size_t archCount;
	char abi[WIRED_PACKAGE_ABI_MAX];
	char engineMin[WIRED_PACKAGE_VERSION_MAX];
	char engineMax[WIRED_PACKAGE_VERSION_MAX];
	wiredPackageFile_t files[WIRED_PACKAGE_FILE_MAX];
	size_t fileCount;
} wiredPackageManifest_t;

bool WiredPackageManifest_Parse( const char *json, size_t jsonLength,
	wiredPackageManifest_t *out, char *error, size_t errorSize );
bool WiredPackageManifest_ValidateSet( const wiredPackageManifest_t *manifests,
	size_t manifestCount, char *error, size_t errorSize );
const char *WiredPackageRole_Name( wiredPackageRole_t role );

#endif
