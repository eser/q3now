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

// Renderer-requested device features. Single
// boolean per intent; RAL owns the AND-gate logic:
//   * PAIRED bundles (vertexFragmentStores, descriptorIndexing,
//     vulkanMemoryModel, 8BitStorage) enable ALL their bits or NONE — RAL
//     checks every device-support bit in the bundle and only flips them
//     when the full set is supported.
//   * Single-bit wants AND against the matching device-support bit; the
//     bit is enabled when both the renderer asked for it and the device
//     reports support.
//   * HARD-REQUIRED features (synchronization2, timelineSemaphore,
//     dynamicRendering, fillModeNonSolid) are NOT in this struct — RAL
//     enforces them internally and Ral_CreateBackend returns NULL with a
//     SEV_ERROR log if any are absent.
//   * wantSamplerAnisotropy is CVAR-GATED — renderer reads
//     r_ext_texture_filter_anisotropic pre-Ral_CreateBackend and only
//     sets the field qtrue when the cvar is non-zero.
typedef struct {
	qboolean wantShaderInt64;
	qboolean wantWideLines;
	qboolean wantVertexFragmentStores;       // PAIRED: vertexPipelineStores + fragmentStores
	qboolean wantDescriptorIndexing;         // PAIRED 4-bundle + 2 mirror bits
	qboolean wantHostQueryReset;
	qboolean wantDrawIndirectCount;
	qboolean wantVulkanMemoryModel;          // PAIRED: model + Scope
	qboolean wantBufferDeviceAddress;
	qboolean want8BitStorage;                // PAIRED: storage + uniformAndStorage
	qboolean wantSamplerAnisotropy;          // CVAR-GATED
	qboolean wantFragmentShadingRate;        // pipeline-rate VRS; enabled only when the
	                                         // extension is present AND the feature query
	                                         // reports pipelineFragmentShadingRate (else the
	                                         // variableRateShading cap stays false)
	qboolean wantDepthClamp;                 // rasterizer near/far depth-clamp (free raster
	                                         // state); enabled only when the device reports
	                                         // VkPhysicalDeviceFeatures.depthClamp (else the
	                                         // depthClamp cap stays false → projection-tweak
	                                         // fallback on backends without native support)
	qboolean wantIndependentBlend;           // per-colour-attachment blend/write-mask state;
	                                         // enabled only when the core device feature is supported
} ralRequestedFeatures_t;

// Host services consumed by a concrete RAL backend.  The public RAL surface
// deliberately keeps native API handles opaque: standalone tools can provide
// SDL/platform callbacks without importing the renderer DLL's global `ri`
// table, while the renderer supplies a thin adapter over its existing imports.
// The struct is copied by value when a backend is created; userData and the
// callback targets must remain valid until Ral_DestroyBackend returns.
typedef enum {
	RAL_LOG_TRACE = 1,
	RAL_LOG_DEBUG = 5,
	RAL_LOG_INFO  = 9,
	RAL_LOG_WARN  = 13,
	RAL_LOG_ERROR = 17,
	RAL_LOG_FATAL = 21
} ralLogSeverity_t;

typedef void *(*ralHostGetProcAddressFn)( void *userData,
	                                      void *nativeInstance,
	                                      const char *name );
typedef qboolean (*ralHostCreateSurfaceFn)( void *userData,
	                                        void *platformHandle,
	                                        void *nativeInstance,
	                                        uint64_t *outNativeSurface );
typedef void (*ralHostLogFn)( void *userData,
	                          ralLogSeverity_t severity,
	                          const char *message );

typedef struct {
	void                       *userData;
	ralHostGetProcAddressFn     getProcAddress;
	ralHostCreateSurfaceFn      createSurface;
	ralHostLogFn                log;
} ralHostImports_t;

// ── creation ────────────────────────────────────────────────────────────
// Three modes (gated by the letBackendOwn* flags + externalInstance):
//   (a) Standalone — externalInstance == NULL, both letBackendOwn* qfalse.
//       Legacy path retained for the throwaway \ral_dump probe (creates a
//       minimum-viable instance + device for caps enumeration).
//   (b) Imported  — externalInstance != NULL. Ral_CreateBackend adopts the
//       caller's already-created handles. Lifetime stays with the caller
//       (Ral_DestroyBackend does NOT destroy the imported handles). Used by
//       the historical ral_dump probe; the renderer no longer uses this
//       path.
//   (c) Owned — letBackendOwnInstance=qtrue
//       AND letBackendOwnDevice=qtrue. RAL creates everything (instance +
//       messenger + surface + picks physical device + queue family resolve
//       + device-extension allow-list + feature enable + VkDevice). Renderer
//       reads handles back via Ral_Get*Handle accessors. Ral_DestroyBackend
//       tears down everything when ownsInstance / ownsDevice are qtrue.
typedef struct {
	ralBackendType_t type;
	void            *platformHandle;            // HWND / NSWindow / canvas selector / etc. (NULL = offscreen)
	uint32_t         flags;                     // RAL_FLAG_DEBUG_LABELS
	ralHostImports_t host;                      // required host loader/surface/log services

	// imported-mode fields (set all-or-none; NULL externalInstance → standalone)
	void            *externalInstance;          // VkInstance
	void            *externalPhysicalDevice;    // VkPhysicalDevice
	void            *externalDevice;            // VkDevice
	uint32_t         externalQueueFamilies[3];  // [GRAPHICS, COMPUTE, TRANSFER]
	uint32_t         externalApiVersion;        // VK_MAKE_API_VERSION(0, 1, 2, 0) or higher

	// owned-instance bringup. When qtrue, RAL
	// creates the VkInstance + debug messenger + VkSurfaceKHR + picks the
	// physical device internally; externalInstance/externalPhysicalDevice
	// fields are ignored.
	qboolean         letBackendOwnInstance;
	// owned-device bringup. When qtrue (in
	// combination with letBackendOwnInstance=qtrue), RAL also resolves
	// queue families, builds the device-extension allow-list, queries
	// features, enables the requested-and-supported subset, and creates
	// the VkDevice internally. externalDevice / externalQueueFamilies
	// are ignored. requestFeatures + platformDeviceExtensions feed the
	// allow-list + feature enable. The renderer reads vk.device,
	// vk.queue_family_*, queue handles back via Ral_Get*Handle /
	// Ral_GetQueue* accessors.
	qboolean         letBackendOwnDevice;
	// Mirrors `r_device` cvar semantics when letBackendOwnInstance=qtrue:
	// -1 = first DISCRETE_GPU (default), -2 = first INTEGRATED_GPU,
	// N ≥ 0 = explicit index into vkEnumeratePhysicalDevices' output.
	int              preferredDeviceIndex;
	// Compile-time USE_VK_VALIDATION reach replacement. When qtrue + the
	// validation layer is available, RAL applies the LUNARG_standard_
	// validation → KHRONOS_validation → none fallback chain during
	// vkCreateInstance and creates a debug messenger.
	qboolean         enableValidation;
	// Host-selected upload policy.  The renderer maps its
	// r_asyncTextureUpload cvar here; standalone tools choose explicitly and
	// the RAL core never imports or names an engine cvar.
	qboolean         allowAsyncTextureUploads;
	// renderer-requested device features +
	// platform-specific device extensions. Both feed the owned-device
	// branch only; ignored when letBackendOwnDevice=qfalse.
	//
	// platformDeviceExtensions points at a caller-owned const char *const
	// array (string-literal lifetime, outlives the backend); RAL retains
	// the pointers without copying. RAL always adds VK_KHR_swapchain
	// (HARD-REQUIRED) and VK_EXT_memory_budget (RAL-internal) on top of
	// the supplied list.
	ralRequestedFeatures_t requestFeatures;
	// Exact platform instance extensions required by the surface provider.
	// Caller-owned immutable strings; the pointer array must outlive the
	// backend.  RAL validates/deduplicates these against the loader's inventory
	// instead of enabling every extension whose name happens to end in surface.
	const char *const *platformInstanceExtensions;
	uint32_t           platformInstanceExtensionCount;
	const char *const *platformDeviceExtensions;
	uint32_t           platformDeviceExtensionCount;
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
	qboolean variableRateShading;
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

	// per-feature enable flags that the renderer
	// reads back at boot to populate its sibling state (vk.wideLines,
	// vk.fragmentStores, vk.samplerAnisotropy). Each reflects what was
	// actually enabled on the VkDevice during the owned-device bringup
	// (request AND device-support).
	qboolean wideLines;
	qboolean vertexFragmentStores;
	qboolean samplerAnisotropyEnabled;
	qboolean depthClamp;                // rasterizer depthClampEnable usable: the device
	                                    // feature was requested AND supported AND enabled on
	                                    // the VkDevice. false → the backend has no native
	                                    // depth-clamp (WebGL2) → renderer takes the
	                                    // near-plane projection-tweak fallback.

	// identity (informational)
	char     deviceName[256];
	char     apiVersion[32];            // e.g. "Vulkan 1.3.290"
	qboolean independentBlend;          // append-only: per-colour-attachment blend/write-mask state enabled
	uint64_t maxStorageBufferRange;      // append-only: maximum legal storage-buffer descriptor range
	qboolean textureCompressionBC;      // append-only: sampled BC1/3/5/7 family available
	qboolean textureCompressionASTC;    // append-only: sampled ASTC 4x4 available
	qboolean textureCompressionETC2;    // append-only: sampled ETC2 RGBA8 available
} ralCaps_t;

const ralCaps_t *Ral_GetCaps( ralBackend_t *b );

// ── memory budget (query ships in v1) ────────────────────────────
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

uint32_t Ral_ProbeBackends( const ralHostImports_t *host,
	                        ralBackendAvailability_t *out, uint32_t maxOut );

// Backend-owned diagnostic body. Engine console/cvar parsing stays in the
// renderer adapter; standalone hosts can invoke the same exercise with their
// own imports and an explicit subcommand.
void Ral_RunDiagnostic( const ralBackendCreateInfo_t *ci,
	                    const char *subcommand );

// ── developer diagnostic entry point ────────────────────────────────────
// Exported from the renderer DLL; the client's "\ral_dump" command resolves
// it with Sys_LoadFunction. Probes, creates a throwaway backend, dumps caps
// and memory budget, destroys it. Not part of the rendering path.
Q_EXPORT void Ral_Dump( void );

// Exact compatibility target for the historical "\ral_pipeline_test"
// command. Runs the same offscreen draw/readback + compute + cache exercise
// as "\ral_dump pipeline" without depending on console argument state.
void Ral_RunPipelineDiagnostic( void );

// "\ral_dump live" dumps the *renderer-owned* backend (the
// one vk_ral_textures.c created via Ral_CreateBackend in imported mode)
// without creating or destroying anything. Tells you what the live shared
// VkDevice + bindless table + RAL buffer registrations actually look like.
// Returns silently if no live backend (renderer hasn't initialised RAL).
Q_EXPORT void Ral_DumpLive( void );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_BACKEND_H
