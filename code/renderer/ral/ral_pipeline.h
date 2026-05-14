// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// ral_pipeline.h — graphics & compute pipelines, pipeline cache.
// Part of the Wired RAL v1 surface (docs/phase-7-ral-design.md §3.6, §6.3, §7.4, §8).
//
// Pipelines are compiled from SPIR-V — the canonical IR. Backends translate
// SPIR-V → MSL / WGSL / GLSL at pipeline creation (§8.2). Render passes /
// framebuffers are NOT part of pipeline state: Wired uses dynamic rendering
// (§6), so colour-attachment formats + depth format live in the create info.

#ifndef WIRED_RAL_PIPELINE_H
#define WIRED_RAL_PIPELINE_H

#include "ral_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── fixed-function state ────────────────────────────────────────────────
typedef enum {
	RAL_TOPOLOGY_POINT_LIST,
	RAL_TOPOLOGY_LINE_LIST,
	RAL_TOPOLOGY_LINE_STRIP,
	RAL_TOPOLOGY_TRIANGLE_LIST,
	RAL_TOPOLOGY_TRIANGLE_STRIP
} ralPrimitiveTopology_t;

typedef enum {
	RAL_POLYGON_FILL,
	RAL_POLYGON_LINE,
	RAL_POLYGON_POINT   // Phase 7.4c-pipeline (§17.8 gap #3) — Q3's TYPE_DOT shader. WebGPU lacks polygon-mode point; backends without it should fall back to RAL_TOPOLOGY_POINT_LIST.
} ralPolygonMode_t;
typedef enum { RAL_CULL_NONE, RAL_CULL_FRONT, RAL_CULL_BACK } ralCullMode_t;
typedef enum { RAL_FRONT_FACE_CCW, RAL_FRONT_FACE_CW } ralFrontFace_t;

typedef enum {
	RAL_BLEND_ZERO,
	RAL_BLEND_ONE,
	RAL_BLEND_SRC_COLOR,
	RAL_BLEND_ONE_MINUS_SRC_COLOR,
	RAL_BLEND_DST_COLOR,
	RAL_BLEND_ONE_MINUS_DST_COLOR,
	RAL_BLEND_SRC_ALPHA,
	RAL_BLEND_ONE_MINUS_SRC_ALPHA,
	RAL_BLEND_DST_ALPHA,
	RAL_BLEND_ONE_MINUS_DST_ALPHA,
	RAL_BLEND_SRC_ALPHA_SATURATE   // Phase 7.4c-pipeline (§17.8 gap #2) — Q3 GLS_SRCBLEND_ALPHA_SATURATE → particle anti-aliased point sprites.
} ralBlendFactor_t;

typedef enum {
	RAL_BLEND_OP_ADD,
	RAL_BLEND_OP_SUBTRACT,
	RAL_BLEND_OP_REVERSE_SUBTRACT,
	RAL_BLEND_OP_MIN,
	RAL_BLEND_OP_MAX
} ralBlendOp_t;

typedef enum { RAL_VERTEX_INPUT_PER_VERTEX, RAL_VERTEX_INPUT_PER_INSTANCE } ralVertexInputRate_t;

// colour write-mask bits for ralColorBlendAttachment_t::writeMask
#define RAL_COLOR_WRITE_R (1u << 0)
#define RAL_COLOR_WRITE_G (1u << 1)
#define RAL_COLOR_WRITE_B (1u << 2)
#define RAL_COLOR_WRITE_A (1u << 3)
#define RAL_COLOR_WRITE_ALL (RAL_COLOR_WRITE_R | RAL_COLOR_WRITE_G | RAL_COLOR_WRITE_B | RAL_COLOR_WRITE_A)

typedef struct {
	uint32_t             location;
	uint32_t             binding;
	ralFormat_t          format;
	uint32_t             offset;
} ralVertexAttribute_t;

typedef struct {
	uint32_t             binding;
	uint32_t             stride;
	ralVertexInputRate_t inputRate;
} ralVertexBinding_t;

typedef struct {
	qboolean         blendEnable;
	ralBlendFactor_t srcColor, dstColor;
	ralBlendOp_t     colorOp;
	ralBlendFactor_t srcAlpha, dstAlpha;
	ralBlendOp_t     alphaOp;
	uint32_t         writeMask;        // RAL_COLOR_WRITE_*
} ralColorBlendAttachment_t;

// Phase 7.4c-pipeline (§17.8 gap #1) — stencil op detail. v1 deferred this;
// q3now's stencil-shadow-volume passes (SHADOW_EDGES + SHADOW_FS_QUAD per
// §17.6.d) need full front/back-face state. Backends map directly:
// Vulkan VkStencilOp / Metal MTLStencilOperation / WebGPU GPUStencilOperation
// share the same eight values. GL 4.3 glStencilOpSeparate covers it.
typedef enum {
	RAL_STENCIL_OP_KEEP,
	RAL_STENCIL_OP_ZERO,
	RAL_STENCIL_OP_REPLACE,
	RAL_STENCIL_OP_INCREMENT_AND_CLAMP,
	RAL_STENCIL_OP_DECREMENT_AND_CLAMP,
	RAL_STENCIL_OP_INVERT,
	RAL_STENCIL_OP_INCREMENT_AND_WRAP,
	RAL_STENCIL_OP_DECREMENT_AND_WRAP
} ralStencilOp_t;

typedef struct {
	ralStencilOp_t failOp;
	ralStencilOp_t passOp;
	ralStencilOp_t depthFailOp;
	ralCompareOp_t compareOp;
	uint32_t       compareMask;
	uint32_t       writeMask;
	uint32_t       reference;
} ralStencilOpState_t;

typedef struct {
	qboolean             depthTestEnable;
	qboolean             depthWriteEnable;
	ralCompareOp_t       depthCompareOp;
	qboolean             stencilTestEnable;
	ralStencilOpState_t  stencilFront;       // Phase 7.4c-pipeline — only consulted when stencilTestEnable == qtrue.
	ralStencilOpState_t  stencilBack;        // Phase 7.4c-pipeline — set equal to stencilFront for two-sided same-op.
} ralDepthStencilState_t;

typedef struct {
	ralPolygonMode_t polygonMode;
	ralCullMode_t    cullMode;
	ralFrontFace_t   frontFace;
	qboolean         depthBiasEnable;
	float            depthBiasConstant;
	float            depthBiasSlope;
	float            depthBiasClamp;
	qboolean         depthClampEnable;
	float            lineWidth;              // Phase 7.4c-pipeline (§17.8 gap #4) — 0.0f → 1.0f; > 1.0 requires backend wideLines support (Vulkan) and is silently clamped to 1.0 on WebGPU.
} ralRasterState_t;

// Specialization constant override (§8.4). v1 treats every constant as a
// 32-bit value (uint/int/float reinterpreted by the shader).
typedef struct {
	uint32_t constantId;
	uint32_t value;
} ralSpecConstant_t;

// ── graphics pipeline ───────────────────────────────────────────────────
typedef struct {
	// programmable stages
	const uint32_t *vertexSpirv;
	uint32_t        vertexSpirvSize;       // bytes
	const uint32_t *fragmentSpirv;
	uint32_t        fragmentSpirvSize;     // bytes
	const char     *vertexEntry;           // NULL → "main"
	const char     *fragmentEntry;         // NULL → "main"

	// vertex input
	const ralVertexBinding_t   *vertexBindings;
	uint32_t                    numVertexBindings;
	const ralVertexAttribute_t *vertexAttributes;
	uint32_t                    numVertexAttributes;

	// fixed function
	ralPrimitiveTopology_t      topology;
	ralRasterState_t            raster;
	ralDepthStencilState_t      depthStencil;
	const ralColorBlendAttachment_t *colorBlends;   // one per colour attachment
	uint32_t                    numColorBlends;

	// dynamic-rendering attachment formats (§6.3) — no VkRenderPass needed
	ralFormat_t                 colorFormats[RAL_MAX_COLOR_ATTACHMENTS];
	uint32_t                    numColorFormats;
	ralFormat_t                 depthFormat;        // RAL_FORMAT_UNDEFINED → no depth attachment
	uint32_t                    sampleCount;        // 1 = no MSAA

	// resources
	const ralBindGroupLayout_t *const *bindGroupLayouts;
	uint32_t                    numBindGroupLayouts;
	uint32_t                    pushConstantSize;   // bytes
	uint32_t                    pushConstantStages; // RAL_STAGE_*

	const ralSpecConstant_t    *specConstants;
	uint32_t                    numSpecConstants;

	// Phase 7.4c-submit-A3 — optional caller-provided pipeline layout. When
	// non-NULL, Ral_CreateGraphicsPipeline reuses this layout (via
	// `externalLayout->vkHandle`) instead of building a fresh one through the
	// backend's layoutCache from bindGroupLayouts + pushConstantSize/Stages.
	// The created pipeline carries layoutCacheIndex = sentinel so
	// Ral_DestroyPipeline's release path no-ops on the layout — caller retains
	// lifetime ownership of the external layout. Used by the renderer's
	// parallel-paths era to make the RAL sibling pipeline identity-share its
	// VkPipelineLayout with the renderer's existing vkCmdBindDescriptorSets
	// calls (so binding the sibling on the parallel buffer is layout-
	// compatible with descriptor-set binds recorded on that buffer).
	ralPipelineLayout_t        *externalLayout;

	// Phase 7.4c-submit-A3 — optional caller-provided VkRenderPass. When
	// non-NULL, the pipeline is created with a legacy VkRenderPass + subpass
	// instead of VkPipelineRenderingCreateInfo (dynamic rendering). Required
	// when the consumer binds the pipeline inside a vkCmdBeginRenderPass
	// instance (validation: pipeline's renderPass must equal the bound
	// renderPass, or null for dynamic rendering — there's no mixing). The
	// caller retains lifetime ownership of the underlying VkRenderPass. Passed
	// as a typed `ralRenderPass_t *` wrapper; backend extracts vkHandle.
	ralRenderPass_t            *externalRenderPass;
	uint32_t                    externalSubpass;        // 0 by default

	const char                 *debugName;
} ralGraphicsPipelineCreateInfo_t;

ralPipeline_t *Ral_CreateGraphicsPipeline( ralBackend_t *b, const ralGraphicsPipelineCreateInfo_t *ci );

// ── compute pipeline ────────────────────────────────────────────────────
typedef struct {
	const uint32_t *computeSpirv;
	uint32_t        computeSpirvSize;      // bytes
	const char     *computeEntry;          // NULL → "main"

	const ralBindGroupLayout_t *const *bindGroupLayouts;
	uint32_t                    numBindGroupLayouts;
	uint32_t                    pushConstantSize;   // bytes

	const ralSpecConstant_t    *specConstants;
	uint32_t                    numSpecConstants;

	ralPipelineLayout_t        *externalLayout;     // Phase 7.4c-submit-A3 — see graphics variant docblock above

	const char                 *debugName;
} ralComputePipelineCreateInfo_t;

ralPipeline_t *Ral_CreateComputePipeline( ralBackend_t *b, const ralComputePipelineCreateInfo_t *ci );

void Ral_DestroyPipeline( ralPipeline_t *p );

// ── pipeline cache (§7.4) ───────────────────────────────────────────────
// Backend-internal on-disk cache (Vulkan VkPipelineCache, Metal
// MTLBinaryArchive; WebGPU N/A). RAL just surfaces save/load.
void Ral_SavePipelineCache( ralBackend_t *b, const char *path );
void Ral_LoadPipelineCache( ralBackend_t *b, const char *path );

// Phase 7.4c-pipeline-followup-5 PART 3 — pipeline-layout cache observability.
// SlotCount: read-only accessor for the high-water slot index. Consumers can
// branch on >0 / read in summary lines.
// DumpToLog: prints the per-slot state via ri.Log (SEV_INFO). Backend stays
// the slot-table owner — this function reaches into it on the consumer's
// behalf so renderer-side dump commands don't need backend-private headers.
uint32_t Ral_GetPipelineLayoutCacheSlotCount( ralBackend_t *b );
void     Ral_DumpPipelineLayoutCache       ( ralBackend_t *b );

#ifdef __cplusplus
}
#endif

#endif // WIRED_RAL_PIPELINE_H
