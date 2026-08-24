// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_OPENGL_CORE_H
#define WIRED_RAL_OPENGL_CORE_H

#include "ral_backend_conformance.h"
#include "ral_memory_recovery.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAL_OPENGL_CORE_SCHEMA_VERSION 1u
#define RAL_OPENGL_REQUIRED_MAJOR 4u
#define RAL_OPENGL_REQUIRED_MINOR 6u

typedef struct ralOpenGlCore_s ralOpenGlCore_t;
typedef void ( *ralOpenGlProc_t )( void );
typedef ralOpenGlProc_t ( *ralOpenGlResolveProcFn )( const char *name,
	void *userData );

// The resolver and context token are supplied by the later platform owner.
// This backend-local seam never exposes GL types or objects to RAL core or the
// renderer frontend.
typedef struct {
	uint64_t generation;
	uintptr_t contextIdentity;
	ralOpenGlResolveProcFn resolveProc;
	void *resolveUserData;
} ralOpenGlCoreCreateInfo_t;

typedef struct {
	uint32_t schemaVersion;
	ralBackendType_t backendType;
	uint64_t generation;
	uintptr_t backendIdentity;
	uintptr_t contextIdentity;
	uintptr_t queueIdentity;
	uint32_t versionMajor;
	uint32_t versionMinor;
	qboolean coreProfile;
	ralCaps_t caps;
	ralCapabilityProfile_t capabilityProfile;
	qboolean ready;
} ralOpenGlCoreReceipt_t;

qboolean RalOpenGl_CoreCreate( const ralOpenGlCoreCreateInfo_t *createInfo,
	ralOpenGlCore_t **outCore, ralOpenGlCoreReceipt_t *outReceipt );
void RalOpenGl_CoreDestroy( ralOpenGlCore_t *core );
qboolean RalOpenGl_CoreReceiptExact( const ralOpenGlCoreReceipt_t *a,
	const ralOpenGlCoreReceipt_t *b );
qboolean RalOpenGl_OffscreenConformance( ralOpenGlCore_t *core,
	uint64_t byteCount, ralBackendConformanceReceipt_t *outReceipt );
qboolean RalOpenGl_CorePublishDeviceLoss( ralOpenGlCore_t *core,
	const ralMemoryFailureEvent_t *event, ralMemoryFailureReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif

#endif
