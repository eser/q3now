// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_types.h — shared enums, plain structs and opaque-handle forward
// declarations for the Wired Renderer Abstraction Layer (RAL).
//
// This is the v1 surface frozen by docs/phase-7-ral-design.md §3. Backend
// implementations (Vulkan first; Metal / GL 4.3 / WebGPU / WebGL2 later) live
// under code/renderer/ral_<backend>/ and provide the bodies. Renderer code
// only ever touches the Ral_* functions and these types — never a VkFoo.
//
// Naming: Ral_* for functions, ral*_t for types, RAL_* for enum constants and
// flag macros.

#ifndef WIRED_RAL_TYPES_H
#define WIRED_RAL_TYPES_H

#include <stdint.h>
#include "../../qcommon/q_shared.h"   // qboolean, byte, vec4_t, Q_EXPORT, ...

#ifdef __cplusplus
extern "C" {
#endif

// ── result codes ────────────────────────────────────────────────────────
// Every Ral_* call that can fail without returning a handle reports via
// ralResult_t. Stubbed-but-not-yet-implemented paths return ralUnsupported.
typedef enum {
	ralSuccess = 0,
	ralUnsupported,             // valid call, backend doesn't implement it (yet)
	ralErrorOutOfMemory,
	ralErrorInvalidArgument,
	ralErrorDeviceLost,
	ralErrorInitFailed,
	ralOutOfDate,               // Phase 7.4c-submit-followup-present-1 — swapchain out of date (surface size/format mismatch); recreate needed. Returned by Ral_AcquireNextImage / Ral_Present.
	ralSuboptimal,              // Phase 7.4c-submit-followup-present-1 — swapchain still functional but no longer optimal; renderer SHOULD recreate at frame boundary. Returned by Ral_Present.
	ralErrorUnknown
} ralResult_t;

// ── opaque handles ──────────────────────────────────────────────────────
// Full definitions live in each backend's ral_<backend>_internal.h. Renderer
// code holds pointers, never dereferences.
typedef struct ralBackend_s          ralBackend_t;
typedef struct ralBuffer_s           ralBuffer_t;
typedef struct ralTexture_s          ralTexture_t;
typedef struct ralTextureView_s      ralTextureView_t;
typedef struct ralSampler_s          ralSampler_t;
typedef struct ralBindGroupLayout_s  ralBindGroupLayout_t;
typedef struct ralBindGroup_s        ralBindGroup_t;
typedef struct ralPipeline_s         ralPipeline_t;
typedef struct ralPipelineLayout_s   ralPipelineLayout_t;   // Phase 7.4c-submit-A2 — typed cmd API
typedef struct ralRenderPass_s       ralRenderPass_t;       // Phase 7.4c-submit-A2 — typed cmd API
typedef struct ralFramebuffer_s      ralFramebuffer_t;      // Phase 7.4c-submit-A2 — typed cmd API
typedef struct ralCommandBuffer_s    ralCommandBuffer_t;
typedef struct ralFence_s            ralFence_t;
typedef struct ralSemaphore_s        ralSemaphore_t;
typedef struct ralSwapchain_s        ralSwapchain_t;
typedef struct ralQueryPool_s        ralQueryPool_t;

// ── backend identity ────────────────────────────────────────────────────
typedef enum {
	RAL_BACKEND_VULKAN,
	RAL_BACKEND_METAL,
	RAL_BACKEND_GL43,
	RAL_BACKEND_WEBGPU,
	RAL_BACKEND_WEBGL2
} ralBackendType_t;

// ralBackendCreateInfo_t::flags
#define RAL_FLAG_VALIDATION    (1u << 0)   // enable backend validation layers / API checking
#define RAL_FLAG_DEBUG_LABELS  (1u << 1)   // enable debug labels / object names (VK_EXT_debug_utils etc.)

// ── queues ──────────────────────────────────────────────────────────────
typedef enum {
	RAL_QUEUE_GRAPHICS,
	RAL_QUEUE_COMPUTE,          // async compute
	RAL_QUEUE_TRANSFER          // async transfer
} ralQueueType_t;

// ── shader stage flags (bitmask) ────────────────────────────────────────
#define RAL_STAGE_VERTEX         (1u << 0)
#define RAL_STAGE_FRAGMENT       (1u << 1)
#define RAL_STAGE_COMPUTE        (1u << 2)
#define RAL_STAGE_ALL_GRAPHICS   (RAL_STAGE_VERTEX | RAL_STAGE_FRAGMENT)
#define RAL_STAGE_ALL            (RAL_STAGE_ALL_GRAPHICS | RAL_STAGE_COMPUTE)

// ── formats ─────────────────────────────────────────────────────────────
// v1 set: covers the renderer's current needs plus the HDR / BC / mobile
// formats §3.3 requires. Backends map to their native enum.
typedef enum {
	RAL_FORMAT_UNDEFINED = 0,

	// uncompressed colour
	RAL_FORMAT_R8_UNORM,
	RAL_FORMAT_R8G8_UNORM,
	RAL_FORMAT_R8G8B8A8_UNORM,
	RAL_FORMAT_R8G8B8A8_SRGB,
	RAL_FORMAT_B8G8R8A8_UNORM,
	RAL_FORMAT_B8G8R8A8_SRGB,
	RAL_FORMAT_A2B10G10R10_UNORM,        // 10:10:10:2 — HDR10 swapchain candidate
	RAL_FORMAT_R16_UNORM,
	RAL_FORMAT_R16_SFLOAT,
	RAL_FORMAT_R16G16_SFLOAT,
	RAL_FORMAT_R16G16B16A16_SFLOAT,      // FP16 — scRGB swapchain / HDR offscreen
	RAL_FORMAT_R11G11B10_UFLOAT,         // packed HDR offscreen
	RAL_FORMAT_R32_SFLOAT,
	RAL_FORMAT_R32G32B32_SFLOAT,         // vec3 position vertex attribute — 7.4 BSP/MD3/MDL surface vertex layouts use this
	RAL_FORMAT_R32G32B32A32_SFLOAT,

	// depth / stencil
	RAL_FORMAT_D16_UNORM,
	RAL_FORMAT_D24_UNORM_S8_UINT,
	RAL_FORMAT_D32_SFLOAT,
	RAL_FORMAT_D32_SFLOAT_S8_UINT,

	// BC (desktop)
	RAL_FORMAT_BC1_RGBA_UNORM,
	RAL_FORMAT_BC1_RGBA_SRGB,
	RAL_FORMAT_BC3_UNORM,
	RAL_FORMAT_BC3_SRGB,
	RAL_FORMAT_BC4_UNORM,
	RAL_FORMAT_BC5_UNORM,
	RAL_FORMAT_BC6H_UFLOAT,
	RAL_FORMAT_BC7_UNORM,
	RAL_FORMAT_BC7_SRGB,

	// ASTC / ETC2 (mobile)
	RAL_FORMAT_ASTC_4x4_UNORM,
	RAL_FORMAT_ASTC_4x4_SRGB,
	RAL_FORMAT_ETC2_R8G8B8A8_UNORM,
	RAL_FORMAT_ETC2_R8G8B8A8_SRGB,

	RAL_FORMAT_COUNT
} ralFormat_t;

// ── swapchain colour spaces ─────────────────────────────────────────────
typedef enum {
	RAL_COLORSPACE_SRGB_NONLINEAR,        // SDR, default
	RAL_COLORSPACE_EXTENDED_SRGB_LINEAR,  // scRGB, FP16 HDR (Windows)
	RAL_COLORSPACE_HDR10_ST2084,          // BT.2020 + PQ
	RAL_COLORSPACE_HDR10_HLG,             // BT.2020 + HLG
	RAL_COLORSPACE_DISPLAY_P3             // macOS HDR
} ralColorSpace_t;

// ── present modes ───────────────────────────────────────────────────────
typedef enum {
	RAL_PRESENT_FIFO,           // vsync, always available
	RAL_PRESENT_MAILBOX,        // low-latency triple buffer
	RAL_PRESENT_IMMEDIATE       // no vsync (tearing)
} ralPresentMode_t;

// ── attachment load / store ─────────────────────────────────────────────
typedef enum {
	RAL_LOAD_OP_LOAD,
	RAL_LOAD_OP_CLEAR,
	RAL_LOAD_OP_DONT_CARE
} ralLoadOp_t;

typedef enum {
	RAL_STORE_OP_STORE,
	RAL_STORE_OP_DONT_CARE
} ralStoreOp_t;

// ── compare op (shared: sampler compare, depth test) ────────────────────
typedef enum {
	RAL_COMPARE_NEVER,
	RAL_COMPARE_LESS,
	RAL_COMPARE_EQUAL,
	RAL_COMPARE_LESS_EQUAL,
	RAL_COMPARE_GREATER,
	RAL_COMPARE_NOT_EQUAL,
	RAL_COMPARE_GREATER_EQUAL,
	RAL_COMPARE_ALWAYS
} ralCompareOp_t;

// ── small geometry helpers ──────────────────────────────────────────────
#define RAL_MAX_COLOR_ATTACHMENTS 8

typedef struct {
	int32_t  x, y;
	uint32_t width, height;
} ralRect_t;

typedef struct {
	float x, y, width, height;
	float minDepth, maxDepth;
} ralViewport_t;

typedef struct {
	uint32_t width, height, depthOrLayers;
} ralExtent3D_t;

// Either a colour clear (rgba float) or a depth/stencil clear — caller picks
// the active member to match the attachment.
typedef union {
	float color[4];
	struct { float depth; uint32_t stencil; } depthStencil;
} ralClearValue_t;

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_TYPES_H
