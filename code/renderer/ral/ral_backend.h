// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_backend.h — backend lifecycle, capability query, memory budget.
// Part of the Wired RAL v1 surface (docs/phase-7-ral-design.md §3.1, §5.2, §12).

#ifndef WIRED_RAL_BACKEND_H
#define WIRED_RAL_BACKEND_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── creation ────────────────────────────────────────────────────────────
// Two modes:
//   (a) Standalone — externalInstance == NULL. Ral_CreateBackend creates its
//       own VkInstance/VkPhysicalDevice/VkDevice. Used by the throwaway
//       \ral_dump probe and (historically) by the Phase 7.4a/b parallel-paths
//       texture/buffer migration.
//   (b) Imported  — externalInstance != NULL. Ral_CreateBackend adopts the
//       caller's already-created handles. Used by Phase 7.4c-pre (Option A):
//       renderervk creates its own 1.2-baseline VkInstance/VkDevice (see
//       vk.c::create_instance / vk_create_device) and hands them to RAL so
//       that resources allocated by Ral_* are usable in the same VkDevice
//       descriptor writes the renderer already issues. The caller retains
//       ownership: Ral_DestroyBackend will NOT destroy the imported handles.
//       Each external* field is the matching Vk* handle cast to void* so this
//       header stays free of vulkan/vulkan.h.
typedef struct {
	ralBackendType_t type;
	void            *platformHandle;            // HWND / NSWindow / canvas selector / etc. (NULL = offscreen)
	uint32_t         flags;                     // RAL_FLAG_VALIDATION | RAL_FLAG_DEBUG_LABELS

	// imported-mode fields (set all-or-none; NULL externalInstance → standalone)
	void            *externalInstance;          // VkInstance
	void            *externalPhysicalDevice;    // VkPhysicalDevice
	void            *externalDevice;            // VkDevice
	uint32_t         externalQueueFamilies[3];  // [GRAPHICS, COMPUTE, TRANSFER]
	uint32_t         externalApiVersion;        // VK_MAKE_API_VERSION(0, 1, 2, 0) or higher
} ralBackendCreateInfo_t;

ralBackend_t *Ral_CreateBackend ( const ralBackendCreateInfo_t *ci );
void          Ral_DestroyBackend( ralBackend_t *b );

// ── capabilities ────────────────────────────────────────────────────────
// Filled once at backend creation. Renderer reads via Ral_GetCaps() and
// caches. Adding fields is backward-compatible (renderer only reads what it
// knows); removing or reordering is not.
typedef struct {
	// feature flags
	qboolean bindlessTextures;          // §4 — large unbounded sampled-texture array
	qboolean dynamicRendering;          // §6 — no VkRenderPass/VkFramebuffer objects
	qboolean asyncCompute;              // dedicated compute queue family
	qboolean asyncTransfer;             // dedicated transfer queue family
	qboolean variableRateShading;       // 7.13
	qboolean timelineSemaphores;        // §3.8
	qboolean hdr10Swapchain;            // RAL_COLORSPACE_HDR10_ST2084 presentable
	qboolean scRGBSwapchain;            // RAL_COLORSPACE_EXTENDED_SRGB_LINEAR presentable
	qboolean debugUtils;                // debug labels / object names available
	qboolean memoryBudget;              // Ral_QueryMemoryBudget returns real numbers (else estimates)
	qboolean drawIndirectCount;         // *DrawIndirectCount available (§9.3 GPU-driven)

	// limits
	uint32_t maxBindlessTextures;       // size of the bindless sampled-texture table
	uint32_t maxColorAttachments;
	uint32_t maxComputeWorkgroupSize;   // max invocations per workgroup
	uint32_t maxTextureDimension2D;
	uint32_t maxTextureDimension3D;
	uint32_t maxTextureArrayLayers;
	uint32_t maxPushConstantSize;       // bytes
	uint64_t minUniformBufferAlignment;
	uint64_t minStorageBufferAlignment;
	float    timestampPeriodNs;         // ns per Ral_WriteTimestamp tick (0 = timestamps unsupported)
	float    maxSamplerAnisotropy;      // max anisotropy a sampler may request (1 = anisotropic filtering unavailable)

	// identity (informational)
	char     deviceName[256];
	char     apiVersion[32];            // e.g. "Vulkan 1.3.290"
} ralCaps_t;

const ralCaps_t *Ral_GetCaps( ralBackend_t *b );

// ── memory budget (7.14 — query ships in v1) ────────────────────────────
typedef struct {
	uint64_t deviceLocalUsed;
	uint64_t deviceLocalBudget;
	uint64_t hostVisibleUsed;
	uint64_t hostVisibleBudget;
	qboolean underPressure;             // backend heuristic: any tracked heap > 85% of its budget
} ralMemoryBudget_t;

void Ral_QueryMemoryBudget( ralBackend_t *b, ralMemoryBudget_t *out );

// Pressure callback (§12.4): backend polls the budget ~1 Hz and invokes this
// on level transitions. Consumer evicts caches / cancels uploads.
//   WARNING  at ~75% of budget — pause non-essential uploads
//   CRITICAL at ~90% of budget — evict (mip drop, LOD drop, pool shrink)
typedef enum {
	RAL_PRESSURE_NORMAL,
	RAL_PRESSURE_WARNING,
	RAL_PRESSURE_CRITICAL
} ralPressureLevel_t;

typedef void (*ralPressureCallback_t)( ralBackend_t *b,
                                       ralPressureLevel_t level,
                                       const ralMemoryBudget_t *budget,
                                       void *user );

void Ral_SetPressureCallback( ralBackend_t *b, ralPressureCallback_t cb, void *user );

// ── availability probe (§5.2) ───────────────────────────────────────────
// On engine boot the RAL probes each backend's availability; the result feeds
// the r_renderer cvar validation and the settings UI. The probe is cheap:
// instance + physical-device enumeration only — no device creation.
typedef struct {
	ralBackendType_t type;
	qboolean         available;
	const char      *name;       // "Vulkan 1.3", "Metal 3", ... (always set)
	const char      *deviceName; // GPU name reported by the backend (NULL if !available)
	const char      *reason;     // why it's unavailable (NULL if available)
} ralBackendAvailability_t;

uint32_t Ral_ProbeBackends( ralBackendAvailability_t *out, uint32_t maxOut );

// ── developer diagnostic entry point ────────────────────────────────────
// Exported from the renderer DLL; the client's "\ral_dump" command resolves
// it with Sys_LoadFunction. Probes, creates a throwaway backend, dumps caps
// and memory budget, destroys it. Not part of the rendering path.
Q_EXPORT void Ral_Dump( void );

// Phase 7.4c-pre: "\ral_dump live" dumps the *renderer-owned* backend (the
// one vk_ral_textures.c created via Ral_CreateBackend in imported mode)
// without creating or destroying anything. Tells you what the live shared
// VkDevice + bindless table + RAL buffer registrations actually look like.
// Returns silently if no live backend (renderer hasn't initialised RAL).
Q_EXPORT void Ral_DumpLive( void );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_BACKEND_H
