// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#pragma once

#include "../include/vulkan/vulkan.h"
#include "tr_common.h"
#include "tr_temporal_batch_request.h"
#include "../../../../../qcommon/q_feats.h"
#include "../../../core/ral_types.h"   // ralFormat_t (renderer attachment-format helpers)
#include "../../../core/ral_presentation_policy.h"
#include "../../../core/ral_color_output.h"
#include "../../../core/ral_atmosphere.h"

// Vulkan validation layer toggle. Gates both the renderer's request bit
// (vk_ral_textures.c: bci.enableValidation) and any in-file #ifdef in
// vk.c — they MUST agree across TUs, so the define lives here. Was
// TU-local in vk.c, which silently disabled validation in every release-
// or non-bindless-debug build (validation messenger never created, VUIDs
// lost).
#if defined(_DEBUG) && defined(_WIN32)
#define USE_VK_VALIDATION
#endif

#define MAX_SWAPCHAIN_IMAGES 8

#define MAX_VK_SAMPLERS 32
#define MAX_VK_PIPELINES ((1024 + 128)*2)

#define SHADOWMAP_MAX_CASCADES 4   // CSM cascade count — also sizes the lighting-UBO cascadeMVP[] array

// Alpha-tested shadow caster vertex = 8 floats (32 B): position xyzw, diffuse uv,
// a bit-cast uint (bindless diffuse pack | alpha-test func), and one pad float.
// Keeps the caster fill, the pipeline vertex stride, and the shader attribute
// offsets in agreement.
#define SHADOW_ATEST_VERT_FLOATS 8

// IBL probe cube dimensions. The source is a small analytic sky; the irradiance
// probe is small (diffuse is low-frequency); the radiance probe is mipped, one
// mip per prefiltered-roughness level. These size the per-face/per-mip view
// arrays on the vk struct and the compute dispatch grids.
#define VK_PROBE_SOURCE_SIZE     64   // analytic sky source cube face size
#define VK_PROBE_IRRADIANCE_SIZE 32   // diffuse irradiance cube face size (1 mip)
#define VK_PROBE_RADIANCE_SIZE   128  // prefiltered radiance cube face size (mip 0)
#define VK_PROBE_RADIANCE_MIPS   6    // roughness levels: 128,64,32,16,8,4

#if FEAT_SHADOW_MAPPING
// Per-inline-brush-model slice into the shared bmodel shadow-caster
// buffer (absolute indices into that buffer's vertex region, so vertexOffset == 0
// at draw time). Index k matches tr.world->bmodels[k] (slot 0 = worldspawn, unused —
// the worldspawn casters live in vk.shadowMap.casterBuf).
typedef struct {
	uint32_t firstIndex;   // first index of this bmodel's geometry in the shared index region
	uint32_t indexCount;   // index count (0 = no opaque casters / not a real bmodel slot)
} vkBmodelCasterRange_t;
#endif

#define VERTEX_BUFFER_SIZE     (4 * 1024 * 1024)  /* by default */
#define VERTEX_BUFFER_SIZE_HI  (8 * 1024 * 1024)

#define STAGING_BUFFER_SIZE    (2 * 1024 * 1024)  /* by default */
#define STAGING_BUFFER_SIZE_HI (24 * 1024 * 1024) /* enough for max.texture size upload with all mip levels at once */

#define NUM_COMMAND_BUFFERS 2	// double-buffered: paces CPU with GPU per-frame, prevents race-ahead bursting (matches MoltenVK's 3-drawable cap)

qboolean vk_publish_menubg_shadow( uint32_t commandSlot );

#define DLIGHT_SHADOW_K_MAX 4	// max simultaneous shadow-casting dynamic lights (top-K brightest); atlas = tileSize*6*K wide. 6*K_MAX mat4 = 1.5KB at K=4, well under the UBO limit (params kept a UBO).

#define USE_REVERSED_DEPTH

#define USE_UPLOAD_QUEUE

#define VK_NUM_BLOOM_PASSES 4

// HDR auto-exposure log-luminance histogram: bin count must match the
// local_size_x of hdr_histogram.comp (one thread clears one bin in the
// zeroing branch) and the SSBO declared there.
#define VK_HDR_HISTOGRAM_BINS 256

// Width of the shared set_layouts[] scratch array used to build every
// pipeline layout in the vk_initialize pipeline-layout block. It must be
// >= the largest setLayoutCount of any layout built there. The widest
// layouts are 4-set: the entity-matrices (main), MSDF, SSAO, sunrays, and
// SMAA layouts (set 0..3). 4 covers them; bump this if a wider layout is
// ever added. (The retired bloom-blend layout was also 4 sets, but its
// removal did not lower this — the layouts above still need 4.)
#define VK_MAX_PIPELINE_LAYOUT_SETS 4

// USE_DEDICATED_ALLOCATION retired (0 consumers; the dead #ifndef branch
// at vk.c was deleted).
//#define MIN_IMAGE_ALIGN (128*1024)

// Sets 1-6 (TEXTURE0/1/2 / FOG_COLLAPSE /
// DEPTH_FADE / NORMALMAP and their aliases) retired alongside the legacy
// 2D-role rotating-set machinery. Only set 0 (uniform/storage) survives in
// the rotating ring; set 7 (bindless) is folded directly in
// vk_bind_descriptor_sets via Ral_GetBindGroupHandle. Use VK_DESC_UNIFORM
// (aliased to VK_DESC_STORAGE — both indexed at 0) for the uniform write.
#define VK_DESC_STORAGE      0
#define VK_DESC_UNIFORM      0

// Bindless main shader packing:
//   - set 1 (binding 0) = SAMPLED_IMAGE[WIRED_BINDLESS_TEX_SLOTS] table
//   - set 1 (binding 1) = SAMPLER[WIRED_BINDLESS_SAMPLER_SLOTS] table
//   - set 0 vkUniform_t::packed_indices carries WIRED_BINDLESS_INDEX_COUNT
//     packed uint32 values per draw.
// Sampler-index widening (2026-05-17): sampler index was 4-bit (16-slot
// ceiling). MAX_VK_SAMPLERS is 32, so content producing 17+ distinct
// samplers silently overflowed the 4-bit field and corrupted the
// packed word. Widened to 8-bit (256-slot ceiling); SAMPLER array now
// sized to MAX_VK_SAMPLERS so the two cannot diverge; per-role packed
// type went uint16→uint32 (block 12 B→24 B; FS push range follows).
// Texture slot stays 12-bit (4096-slot cap unchanged).
// Set indices renumbered after the legacy ring deletion.
// Old layout: 0=uniform, 1-6=ring stubs, 7=bindless, 8=engine-resources.
// New layout: 0=uniform, 1=bindless, 2=engine-resources. setLayoutCount=3.
// Constants below are the single source of truth — set_layouts[] sizing,
// vk.cmd->descriptor_set.current[] sizing, and the fold logic all index
// through these, so the renumber is the only point of change C-side.
#define WIRED_BINDLESS_SET             1
#define WIRED_BINDLESS_TEX_BITS        12
#define WIRED_BINDLESS_SAMPLER_BITS     8
#define WIRED_BINDLESS_TEX_SLOTS       (1u << WIRED_BINDLESS_TEX_BITS)     /* 4096 */
#define WIRED_BINDLESS_SAMPLER_SLOTS   MAX_VK_SAMPLERS                     /* 32 — pool cap; bound here so the two cannot diverge */
// The top
// WIRED_BINDLESS_RESERVED_TEX_SLOTS slots of the 2D bindless table are
// reserved for engine render-targets / sentinels. Content images
// (R_CreateImage path) cap at slot
// WIRED_BINDLESS_TEX_SLOTS - WIRED_BINDLESS_RESERVED_TEX_SLOTS - 1.
//   slot 4095 (TEX_SLOTS-1) = screenmap render attachment
//   slot 4094 (TEX_SLOTS-2) = black sun-mask decline sentinel
//   slot 4093 (TEX_SLOTS-3) = white unused-texture-role sentinel
//   slot 4092 (TEX_SLOTS-4) = depth-fade scene-depth copy (soft particles)
#define WIRED_BINDLESS_RESERVED_TEX_SLOTS 4u
#define WIRED_BINDLESS_SCREENMAP_SLOT     (WIRED_BINDLESS_TEX_SLOTS - 1u)  /* 4095 — never used by content */
#define WIRED_BINDLESS_BLACK_TEX_SENTINEL (WIRED_BINDLESS_TEX_SLOTS - 2u)  /* 4094 — 1×1 R8 zero; sun-mask role default */
#define WIRED_BINDLESS_WHITE_TEX_SENTINEL (WIRED_BINDLESS_TEX_SLOTS - 3u)  /* 4093 — white; unused-texture-role default */
#define WIRED_BINDLESS_SCENEDEPTH_SLOT     (WIRED_BINDLESS_TEX_SLOTS - 4u)  /* 4092 — depth-fade scene-depth copy, role 4 on dfade draws */
// Role 6 (screenmap); role 7 (sun-mask). Both pushed
// by vk_push_bindless_indices. Role 6 is a constant (screenmap dedup slot);
// role 7 is per-draw (vk_bindless_track, mirrors role 1 lightmap) and defaults
// to the black sentinel WIRED_BINDLESS_BLACK_TEX_SENTINEL (samples 0 → A''
// graceful-decline: mix(1.0, shadow, 0) = 1.0). Shaders ignore roles they
// do not declare.
// Role 8 added (base-pass IBL pbrMap ORM, tracked from tr_shade.c): the count
// must INCLUDE role 8 so vk_bindless_track accepts it and the packing loop
// fills packed_indices[8] (a value of 8 would reject role 8 → it stays
// PACK(0,0) and gen_frag's USE_IBL term samples bindless slot 0 garbage).
// Roles 9..11 remain RESERVED capacity in the 12-slot UBO table (untracked,
// zero-initialised by the packing local) — not counted here until tracked.
#define WIRED_BINDLESS_INDEX_COUNT     9
// Role 4 normally carries lightmap2 (lm2, tracked from tr_shade.c on the Q1
// lightstyle paths). For soft-particle depth-fade draws — single-texture
// blended surfaces under r_depthFade — the packing loop's role-4 value is
// overridden post-loop with the scene-depth copy at WIRED_BINDLESS_SCENEDEPTH_SLOT
// so the dfade fragment can read scene depth; gated on the per-draw
// vk.cmd->depthFadeDraw flag so lm2 is never clobbered on other draws.
#define WIRED_BINDLESS_SCENEDEPTH_ROLE  4u
#define WIRED_BINDLESS_SCREENMAP_ROLE  6u
#define WIRED_BINDLESS_SUNMASK_ROLE    7u
// Main renderer pipeline layouts are push-free. MVP, bindless indices and the
// enhanced-fog state all travel through the set-0 per-draw UBO so the same ABI
// lowers directly to Vulkan, Metal and WebGPU.
#define WIRED_ADVANCED_FOG_UBO_OFFSET 608u
#define WIRED_ADVANCED_FOG_UBO_SIZE    32u
#define WIRED_BINDLESS_BIND_IMAGES        0
#define WIRED_BINDLESS_BIND_SAMPLERS      1
// Parallel SAMPLED_IMAGE binding for 2DArray content textures
// (q1_ls_array.frag's animArray). The 2D table at binding 0 holds
// VK_IMAGE_VIEW_TYPE_2D views; binding 2 holds VK_IMAGE_VIEW_TYPE_2D_ARRAY
// views — the dimension is enforced by the SPIR-V binding declaration
// (texture2D[] vs texture2DArray[]), so the same VkDescriptorType
// (SAMPLED_IMAGE) is used for both. Q1 maps typically carry under 50
// q1AnimArray shaders, so the bounded 256-slot capacity gives generous
// headroom while keeping pool consumption modest (256 SAMPLED_IMAGE
// descriptors versus 4096 for the 2D table).
#define WIRED_BINDLESS_BIND_ARRAY_IMAGES  2
#define WIRED_BINDLESS_ARRAY_TEX_SLOTS    256u

// Engine-resources descriptor set. Single-render-target,
// per-frame-stable engine state that consumes a sampler-style descriptor
// (NOT bindless): the CSM shadow-map 2DArray view (light_frag's
// USE_SHADOWMAP variants). Replaces the legacy-ring set=3 shadowMap
// binding. Single COMBINED_IMAGE_SAMPLER FRAGMENT binding — shape mirrors
// vk.set_layout_sampler but with a stricter view-dimension expectation
// on binding 0 (sampler2DArray). Bound per-draw via the same fold pattern
// set 7 uses (stable handle, pipeline-layout-compat-disturbance rebind).
// The screenmap binding (formerly WIRED_ENGINE_RES_BIND_SCREENMAP=1)
// was retired: water.frag now samples the screenmap through the bindless
// 2D table at WIRED_BINDLESS_SCREENMAP_SLOT, role WIRED_BINDLESS_SCREENMAP_ROLE.
#define WIRED_ENGINE_RES_SET                 2  /* renumbered from 8 */
#define WIRED_ENGINE_RES_BIND_SHADOWMAP      0
// IBL global scene resources share the engine-resources set (they are global,
// not per-draw — same rationale as the shadowMap). Declared/written but not yet
// sampled (the IBL term in light_frag lands later); inert + layout-compatible.
#define WIRED_ENGINE_RES_BIND_BRDF_LUT       1   // 2D RG16F split-sum BRDF LUT
#define WIRED_ENGINE_RES_BIND_IRRADIANCE     2   // diffuse irradiance cube
#define WIRED_ENGINE_RES_BIND_RADIANCE       3   // prefiltered radiance cube
#define WIRED_ENGINE_RES_BIND_GTAO           4   // denoised screen-space GTAO visibility (R8); gen_frag modulates the IBL-specular term by it
// Per-entity model/MVP matrices for the main opaque path live on their own
// dedicated set (set 3 of vk.pipeline_layout), NOT here — the engine-resources
// set is written once at FBO setup (stable views), whereas the entity matrices
// rotate per command-buffer slot, so they need a per-slot descriptor like the
// shadow depth pass's per-caster matrix SSBO. See WIRED_ENTITY_MAT_SET below.
// Bytes per per-entity slot in entMatBuf. The main vertex shaders read the full
// MVP (mat4); under FEAT_SHADOW_MAPPING the lit vertex shader also reads the
// model->world matrix for the fragment's shadow lookup, so the slot carries both
// (std430 mat4 = 64 B, tightly packed — no inter-element padding). The host write
// and the shader struct { mat4 mvp; [mat4 modelMatrix;] } must agree on this.
#if FEAT_SHADOW_MAPPING
#define ENTITY_MATRIX_SLOT_BYTES 128u
#else
#define ENTITY_MATRIX_SLOT_BYTES 64u
#endif
// MSDF text per-draw UBO set. The MSDF pipeline's per-draw data (MVP + outline/
// glow/shadow params + atlas bindless slot) lives in a dedicated per-draw
// UNIFORM_BUFFER_DYNAMIC ring bound here, on vk.pipeline_layout_msdf only. Set 3
// is the 4th and final slot (VK_MAX_PIPELINE_LAYOUT_SETS); a separate buffer from
// the set-0 main vkUniform_t ring. Like WIRED_BINDLESS_SET/WIRED_ENGINE_RES_SET
// above, this is a single source of truth for descriptor_set.current[]/offset[]
// sizing and the bind-fold logic.
#define WIRED_MSDF_SET                       3
// Per-entity matrix storage buffer set on vk.pipeline_layout (the MAIN opaque
// layout). Numerically set 3 like WIRED_MSDF_SET, but a DIFFERENT pipeline
// layout — MSDF's set 3 is on vk.pipeline_layout_msdf, this is on
// vk.pipeline_layout, so the index reuse is not a conflict. Both fit
// VK_MAX_PIPELINE_LAYOUT_SETS=4 and the shared descriptor_set.current[]/offset[]
// arrays (sized WIRED_MSDF_SET+1=4) already cover slot 3.
#define WIRED_ENTITY_MAT_SET                 3
#define WIRED_BINDLESS_TEX_MASK        (WIRED_BINDLESS_TEX_SLOTS - 1u)
#define WIRED_BINDLESS_SAMPLER_MASK    ((1u << WIRED_BINDLESS_SAMPLER_BITS) - 1u)  /* 0xFF — covers the 256-entry sampler field; SAMPLER_SLOTS caps the actual array */
// Slot 4095 of the 2D bindless table is permanently bound to
// vk.screenMap.color_image_view; slot 4094 holds the 1×1 R8
// black sun-mask sentinel; slot 4093 holds the white unused-
// texture-role sentinel. Content images cap at slot 4092
// (WIRED_BINDLESS_TEX_SLOTS - WIRED_BINDLESS_RESERVED_TEX_SLOTS - 1). The
// reserved slots are written with a direct raw-vkUpdateDescriptorSets write
// (no image_t shim); the screenmap also tracks a sampler-slot
// (vk.bindless_screenmap_sampler_slot) consumed by vk_push_bindless_indices
// to assemble the constant role-6 packed value.
#define WIRED_BINDLESS_PACK(tex, smp)  ((uint32_t)( ((uint32_t)((smp) & WIRED_BINDLESS_SAMPLER_MASK) << WIRED_BINDLESS_TEX_BITS) \
                                                  | (uint32_t)((tex) & WIRED_BINDLESS_TEX_MASK) ))
// Concern-1 guard: the sampler index field must hold every entry of the
// dedup pool. If MAX_VK_SAMPLERS is ever raised past 1u<<SAMPLER_BITS the
// field silently overflows again — fail the build instead.
_Static_assert( MAX_VK_SAMPLERS <= (1u << WIRED_BINDLESS_SAMPLER_BITS),
                "MAX_VK_SAMPLERS exceeds WIRED_BINDLESS_SAMPLER_BITS — widen SAMPLER_BITS or shrink MAX_VK_SAMPLERS" );

typedef enum {
	TYPE_COLOR_BLACK,
	TYPE_COLOR_WHITE,
	TYPE_COLOR_GREEN,
	TYPE_COLOR_RED,
	TYPE_FOG_ONLY,
	TYPE_MSDF,

	TYPE_SINGLE_TEXTURE_LIGHTING,
	TYPE_SINGLE_TEXTURE_LIGHTING_LINEAR,
#if FEAT_PARALLAX_MAPPING
	TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX,
	TYPE_SINGLE_TEXTURE_LIGHTING_PARALLAX_LINEAR,
#endif
#if FEAT_ADVANCED_WATER
	TYPE_WATER,
#endif
#if FEAT_SHADOW_MAPPING
	TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW,
	TYPE_SINGLE_TEXTURE_LIGHTING_SHADOW_LINEAR,
	TYPE_SHADOW_DEPTH,
	// World-lightmap sun-shadow receiver variants of the gen
	// pipelines (gen_frag/gen_vert USE_SHADOWMAP). Created on-demand via
	// vk_find_pipeline_ext like the PBR / water swaps — deliberately outside
	// the TYPE_GENERIC_BEGIN..END range (no env pairing).
	TYPE_MULTI_TEXTURE_MUL2_SHADOW,
	TYPE_MULTI_TEXTURE_MUL3_SHADOW,
	TYPE_BLEND2_MUL_SHADOW,
	TYPE_BLEND3_MUL_SHADOW,
	// IDENTITY / FIXED_COLOR colour-mode sun-shadow
	// receivers. The _IDENTITY / _FIXED_COLOR gen optimisation drops
	// TESS_RGBA0 (no per-vertex colour input), so these need dedicated
	// shadow modules — they cannot reuse MUL2_SHADOW, whose shader
	// consumes frag_color0In. MUL2 only (MUL3 / BLEND deferred).
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY_SHADOW,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_SHADOW,
#endif
#if FEAT_PBR
	TYPE_SINGLE_TEXTURE_LIGHTING_PBR,
	TYPE_SINGLE_TEXTURE_LIGHTING_PBR_LINEAR,
	// Base-pass IBL receiver variants of the gen world-lightmap pipelines
	// (gen_frag/gen_vert USE_IBL). Created on-demand via vk_find_pipeline_ext
	// like the SHADOW / PBR swaps — deliberately outside the
	// TYPE_GENERIC_BEGIN..END range (no env pairing). Worldspawn lightmap-modulate
	// only; mirrors the four core sun-shadow receiver types.
	TYPE_MULTI_TEXTURE_MUL2_IBL,
	TYPE_BLEND2_MUL_IBL,
	TYPE_MULTI_TEXTURE_MUL3_IBL,
	TYPE_BLEND3_MUL_IBL,
#endif

	TYPE_SINGLE_TEXTURE_DF,

	TYPE_LIGHTSTYLES,		// Q1 4-style lightmap blend, animChain lerp via set=6
	TYPE_LIGHTSTYLES_ARRAY,	// Q1 4-style lightmap blend, GPU time-driven texture array

	TYPE_GENERIC_BEGIN, // start of non-env/env shader pairs
	TYPE_SIGNLE_TEXTURE = TYPE_GENERIC_BEGIN,
	TYPE_SINGLE_TEXTURE_ENV,

	TYPE_SINGLE_TEXTURE_IDENTITY,
	TYPE_SINGLE_TEXTURE_IDENTITY_ENV,

	TYPE_SINGLE_TEXTURE_FIXED_COLOR,
	TYPE_SINGLE_TEXTURE_FIXED_COLOR_ENV,

	TYPE_SINGLE_TEXTURE_ENT_COLOR,
	TYPE_SINGLE_TEXTURE_ENT_COLOR_ENV,

	// Shadow-Unification Part 1 — single-texture (tx0) sun-shadow receiver types.
	// The separate-lightmap-pass inset's lone lightmap stage is a single-texture
	// pass; these route it through the USE_SHADOWMAP gen_frag variant so its lightmap
	// operand receives the CSM sun shadow (the operand-keyed apply). Three colour
	// modes mirror the single-texture types the lightmap stage can take (plain
	// vertex colour, IDENTITY, FIXED_COLOR).
	TYPE_SINGLE_TEXTURE_SHADOW,
	TYPE_SINGLE_TEXTURE_IDENTITY_SHADOW,
	TYPE_SINGLE_TEXTURE_FIXED_COLOR_SHADOW,

	TYPE_MULTI_TEXTURE_ADD2_IDENTITY,
	TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY,
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV,

	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR,
	TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV,

	TYPE_MULTI_TEXTURE_MUL2,
	TYPE_MULTI_TEXTURE_MUL2_ENV,
	TYPE_MULTI_TEXTURE_ADD2_1_1,
	TYPE_MULTI_TEXTURE_ADD2_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD2,
	TYPE_MULTI_TEXTURE_ADD2_ENV,

	TYPE_MULTI_TEXTURE_MUL3,
	TYPE_MULTI_TEXTURE_MUL3_ENV,
	TYPE_MULTI_TEXTURE_ADD3_1_1,
	TYPE_MULTI_TEXTURE_ADD3_1_1_ENV,
	TYPE_MULTI_TEXTURE_ADD3,
	TYPE_MULTI_TEXTURE_ADD3_ENV,

	TYPE_BLEND2_ADD,
	TYPE_BLEND2_ADD_ENV,
	TYPE_BLEND2_MUL,
	TYPE_BLEND2_MUL_ENV,
	TYPE_BLEND2_ALPHA,
	TYPE_BLEND2_ALPHA_ENV,
	TYPE_BLEND2_ONE_MINUS_ALPHA,
	TYPE_BLEND2_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND2_MIX_ALPHA,
	TYPE_BLEND2_MIX_ALPHA_ENV,

	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND2_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_BLEND3_ADD,
	TYPE_BLEND3_ADD_ENV,
	TYPE_BLEND3_MUL,
	TYPE_BLEND3_MUL_ENV,
	TYPE_BLEND3_ALPHA,
	TYPE_BLEND3_ALPHA_ENV,
	TYPE_BLEND3_ONE_MINUS_ALPHA,
	TYPE_BLEND3_ONE_MINUS_ALPHA_ENV,
	TYPE_BLEND3_MIX_ALPHA,
	TYPE_BLEND3_MIX_ALPHA_ENV,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA,
	TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV,

	TYPE_BLEND3_DST_COLOR_SRC_ALPHA,
	TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV,

	TYPE_GENERIC_END = TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV

} Vk_Shader_Type;

// Pipeline phase for the legacy stencil shadow-volume path (front/back edge pass +
// fullscreen darkening quad). Selects the stencil test/op state in create_pipeline.
typedef enum {
	SHADOW_DISABLED,
	SHADOW_EDGES,
	SHADOW_FS_QUAD,
} Vk_Shadow_Phase;

typedef enum {
	TRIANGLE_LIST = 0,
	TRIANGLE_STRIP,
	LINE_LIST,
	POINT_LIST
} Vk_Primitive_Topology;

typedef enum {
	DEPTH_RANGE_NORMAL,		// [0..1]
	DEPTH_RANGE_ZERO,		// [0..0]
	DEPTH_RANGE_ONE,		// [1..1]
	DEPTH_RANGE_WEAPON,		// [0..0.3]
	DEPTH_RANGE_COUNT
}  Vk_Depth_Range;

typedef struct {
	VkSamplerAddressMode address_mode; // clamp/repeat texture addressing mode
	int gl_mag_filter;		// GL_XXX mag filter
	int gl_min_filter;		// GL_XXX min filter
	qboolean max_lod_1_0;	// fixed 1.0 lod
	qboolean noAnisotropy;
} Vk_Sampler_Def;

typedef enum {
	RENDER_PASS_MAIN = 0,
	RENDER_PASS_SCREENMAP,
	RENDER_PASS_POST_BLOOM,
	RENDER_PASS_COUNT
} renderPass_t;

// Inert structural slots for the future temporal-MAIN pipeline cohort.  These
// never alias ral_handle[RENDER_PASS_MAIN]: ordinary MAIN/SCREENMAP selection
// remains authoritative until a later runtime leaf explicitly opts in.
typedef enum {
	VK_TEMPORAL_PIPELINE_PRESERVE = 0,
	VK_TEMPORAL_PIPELINE_WRITE,
	VK_TEMPORAL_PIPELINE_INVALIDATE,
	VK_TEMPORAL_PIPELINE_COHORT_COUNT
} vkTemporalPipelineCohortSlot_t;

typedef struct {
	Vk_Shader_Type shader_type;
	unsigned int state_bits; // GLS_XXX flags
	cullType_t face_culling;
	qboolean polygon_offset;
	qboolean mirror;
	Vk_Shadow_Phase shadow_phase;
	Vk_Primitive_Topology primitives;
	int line_width;
	int fog_stage; // off, fog-in / fog-out
	int abs_light;
	// Base-pass IBL ambient gate (gen_frag's ibl_enabled spec constant, id 14).
	// Set at the base-pass IBL pipeline-swap site for a pbrMap worldspawn surface
	// under r_pbr; selects the USE_IBL gen pipeline variant. Ignored by every
	// non-IBL shader (they do not declare id 14).
	int ibl_enabled;
	int allow_discard;
	int acff; // none, rgb, rgba, alpha
	// The `srgb` cache-key field is
	// retired — colour-texel sRGB->linear decode is unconditional in
	// gen_frag.tmpl / light_frag.tmpl now, so there is no per-pipeline
	// variant.
	// Per-slot colour-domain bitmask (bit N = texture slot N is
	// CD_LINEAR → raw fetch in gen_frag.tmpl / light_frag.tmpl). Filled from
	// each stage's bundle images at the def-build site; wired to fragment
	// spec constant id 4 (gen_frag.tmpl / light_frag.tmpl / water.frag all
	// declare `tex_domain` as constant_id 4).
	int tex_domain;
	// Which colour-texture slot holds the lightmap: 0 = none, else bundle index + 1
	// (1 = slot0, 2 = slot1, 3 = slot2). Filled from bundle[N].lightmap !=
	// LIGHTMAP_INDEX_NONE at the def-build site; wired to gen_frag.tmpl fragment spec
	// constant id 26 (lightmap_slot). gen_frag applies LIGHTMAP_BOOST to exactly this
	// operand (in wired_bl_sample_domain), so the world overbright follows the lightmap
	// through every combine branch + the single-texture pass — not just modulate.
	int lightmap_slot;
	// Tangent-space normal-map encoding for the PARALLAX
	// lighting variants. 0 = RGB (BC1/BC3/uncompressed RGBA — all 3 channels,
	// unpack *2-1); 1 = BC5 UNORM (ATI2N/3Dc — R=X G=Y, reconstruct
	// Z = sqrt(1-x²-y²)); 2 = BC5 SNORM (R=X G=Y already signed, reconstruct
	// Z). Set at the parallax pipeline-swap site from bundle[2].image[0]->
	// internalFormat; wired to light_frag.tmpl fragment spec constant id 15
	// (a hole left by the retired `srgb` constant). Ignored by every
	// non-parallax shader (they don't declare id 15).
	int normal_format;
	struct {
		byte rgb;
		byte alpha;
	} color;
} Vk_Pipeline_Def;

typedef struct VK_Pipeline {
	Vk_Pipeline_Def def;
	// qtrue when this pipeline was built with a soft-particle depth-fade
	// fragment module (the dfade_* swap in create_pipeline under r_depthFade +
	// vk.sceneDepth.active + a blended single-texture shader_type). Baked at
	// creation; vk_bind_pipeline copies it to vk.cmd->depthFadeDraw so the
	// bindless role-4 override fires only for these draws.
	qboolean depthFade;
	// Dynamic-rendering pipeline built for the world-pass slots —
	// RENDER_PASS_MAIN (3D world + UI piggyback it) and RENDER_PASS_SCREENMAP
	// (mirror/portal); both share the color/depth formats. POST_BLOOM owns a
	// dedicated RAL pipeline and never enters this generic cache.
	struct ralPipeline_s *ral_handle[ RENDER_PASS_COUNT ];
	// Exact three-attachment {scene, RG16F velocity, R8 validity} siblings.
	// Prerequisite A only provides the unreachable PRESERVE constructor; WRITE
	// and INVALIDATE remain NULL structural ownership slots for the next leaf.
	struct ralPipeline_s *ral_temporal_handle[ VK_TEMPORAL_PIPELINE_COHORT_COUNT ];
} VK_Pipeline_t;

// this structure must be in sync with shader uniforms!
typedef struct vkUniform_s {
	// light/env parameters:
	vec4_t eyePos;				// vertex
	union {
		struct {
			vec4_t pos;			// vertex: light origin
			vec4_t color;		// fragment: rgb + 1/(r*r)
			vec4_t vector;		// fragment: linear dynamic light
		} light;
		struct {
			vec4_t color[3];	// ent.color[3]
		} ent;
	};
	// fog parameters:
	vec4_t fogDistanceVector;	// vertex
	vec4_t fogDepthVector;		// vertex
	vec4_t fogEyeT;				// vertex
	vec4_t fogColor;			// fragment
	// lightstyle per-surface blend weights (x=slot0, y=slot1, z=slot2, w=slot3)
	vec4_t q1StyleIntensities;
#if FEAT_SHADOW_MAPPING
	// CSM cascade matrices + split distances. Appended at the end
	// (std140 offset 144) so existing field offsets are untouched — the lit
	// shader's USE_SHADOWMAP UBO declares these at explicit layout(offset=)s
	// (144 / 400 / 416). Replaces the earlier single-sunMVP-aliased-over-fog hack.
	float  cascadeMVP[SHADOWMAP_MAX_CASCADES][16];   // offset 144, 256 B — per-cascade light view-proj (column-major)
	vec4_t cascadeSplits;                            // offset 400, 16 B  — view-space Z for splits 1..4 (split 0 = near is implicit)
	// Caster/receiver model->world transform — identity for
	// worldspawn surfaces, the entity's [axis|origin] for entity/brush-model
	// surfaces. light_vert.tmpl maps in_position through it so the fragment's
	// shadow-map lookup uses the WORLD position (in_position is object-space
	// for entity surfaces / rest-space for moved brush models).
	float  modelMatrix[16];                          // offset 416, 64 B  — column-major
#endif
	// Model-view-projection — moved off the push-constant block (which sat at the
	// 128 B Vulkan ceiling) into this UBO. Appended at the END so every field above
	// keeps its offset (and the shaders' std140 _pad arrays are untouched): lands at
	// offset 480 with FEAT_SHADOW_MAPPING, 144 without — both 16-aligned (mat4). All
	// EVERY vertex shader reads mvp from here (gen/light/color/fog/q1_ls); no vertex
	// shader takes MVP from a push constant anymore (the last one, the flare-test
	// dot.vert, was retired with the dot-probe occlusion machinery).
	// Column-major (byte-identical to the old pushed value).
	float  mvp[16];                                  // offset 480 (shadow) / 144 (no-shadow), 64 B
	// Bindless per-draw index table — migrating off the FS push block. Each
	// uint32 packs (tex 12-bit | sampler<<12 8-bit) for one role; the fragment
	// shaders read it as `uvec4 packed_indices[3]` (std140: a uint[] would 16-byte-
	// stride each element to 128 B — the uvec4[3] form is 48 B, no waste). Roles
	// 0..7 are filled by the packing loop (WIRED_BINDLESS_INDEX_COUNT); roles
	// 8..11 are RESERVED capacity (8 = pbrMap for the base-pass IBL term — do NOT
	// repurpose). 48 B at offset 544 (shadow) / 208 (no-shadow), 16-aligned.
	uint32_t packed_indices[12];                     // offset 544 (shadow) / 208 (no-shadow), 48 B
	// World/material parameters, set once per frame (same value in every ring item).
	// .x = r_lightmapBoost (the base-pass linear-domain overbright multiplier the
	// lightmap modulate branches apply); .yzw = global wetness/frost/snow response.
	// Melt is folded into wetness host-side. Appended at the very END so every field
	// above keeps its offset; gen_frag declares it at the matching trailing offset.
	// gen_frag pads packed_indices to 544 in BOTH its shadow/no-shadow UBO variants,
	// so this lands at 592 there; with FEAT_SHADOW_MAPPING (always 1) the host layout
	// agrees. 16 B, 16-aligned.
	float    worldLightParams[4];                    // offset 592, 16 B
	// Enhanced fog state is a permanent part of the pipeline-layout and descriptor
	// ABI. Two vec4s keep the tail std140-
	// native on Vulkan/Metal/WebGPU: rgb+density, then type+farClip+enabled+pad.
	vec4_t advancedFogColorDensity;                   // offset 608, 16 B
	vec4_t advancedFogTypeFarEnabled;                 // offset 624, 16 B
	vec4_t emissionRadiance;                          // offset 640, 16 B
} vkUniform_t;
_Static_assert( sizeof( vkUniform_t ) == 656,
	"ordinary draw UBO ABI must remain 656 bytes" );

#define TESS_XYZ   (1)
#define TESS_RGBA0 (2)
#define TESS_RGBA1 (4)
#define TESS_RGBA2 (8)
#define TESS_ST0   (16)
#define TESS_ST1   (32)
#define TESS_ST2   (64)
#define TESS_NNN   (128)
#define TESS_VPOS  (256)  // uniform with eyePos
#define TESS_ENV   (512)  // mark shader stage with environment mapping
#define TESS_ENT0  (1024) // uniform with ent.color[0]
#define TESS_ENT1  (2048) // uniform with ent.color[1]
#define TESS_ENT2  (4096) // uniform with ent.color[2]
//
// Initialization.
//

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
// Returns qtrue on success; qfalse on a RECOVERABLE init failure (e.g.
// device lacks bindless capability) — the caller must NOT proceed with
// further init and the renderer DLL flags re.initFailed so cl_main
// advances to the next renderer in the fallback list.
qboolean vk_initialize( void );

// Called after initialization or renderer restart
void vk_init_descriptors( void );

// HDR auto-exposure histogram compute bring-up. Called from the tail of
// vk_ral_refresh_internal_texture_dependents (vk_ral_textures.c) once the color
// image has been adopted into vk.ral_color_image; lives in vk.c because it
// references the embedded histogram SPIR-V. Idempotent across vid_restart.
void vk_hdr_histogram_init( struct ralBackend_s *backend );
// Symmetric teardown, called from vk_ral_release_internal_texture_dependents so
// the RAL-owned histogram handles are freed before the device is destroyed.
void vk_hdr_histogram_shutdown( void );

// HDR auto-exposure reduce compute bring-up/teardown. Consumes the histogram +
// the exposure UBO ring; called from the same adopt/destroy sweep right after the
// histogram. Idempotent across vid_restart.
void vk_hdr_exposure_reduce_init( struct ralBackend_s *backend );
void vk_hdr_exposure_reduce_shutdown( void );

// BRDF integration LUT compute bring-up/teardown. The split-sum IBL view-term,
// a 256x256 RG16F storage image computed once at boot. Created (not adopted) in
// the same adopt/destroy sweep; the actual one-shot dispatch is recorded later
// in vk_begin_frame's no-render-pass seam (guarded by brdfLutDispatched). The
// init has no color-image dependency, but rides the adopt sweep for lifecycle
// symmetry with the other RAL compute resources. Idempotent across vid_restart.
void vk_brdf_lut_init( struct ralBackend_s *backend );
void vk_brdf_lut_shutdown( void );

// GTAO ambient-occlusion compute bring-up/teardown. Two compute passes (horizon
// search + depth-aware denoise) writing R8 storage textures the tonemap pass
// samples. Depth-dependent, so gated on the depth copy existing (depthFade
// active = r_ssao on); rides the same adopt/destroy sweep. Idempotent.
void vk_gtao_init( struct ralBackend_s *backend );
void vk_gtao_shutdown( void );
// Per-frame GTAO dispatch (main + denoise). Records into `cb`: the graphics cmd
// buffer (vk.cmd->ral_cmd) for the serialized fallback, or a RAL_QUEUE_COMPUTE cmd
// buffer for the async path. No-op unless r_ssao is on, the pipelines
// are up, and depth is available.
void vk_gtao_dispatch( struct ralCommandBuffer_s *cb );
// Async-compute: at the frame-start seam, if r_asyncCompute is set, the
// device has a dedicated compute queue, and a prior-frame depth snapshot exists,
// record GTAO into a RAL_QUEUE_COMPUTE cmd buffer reading that snapshot and submit
// it (sets vk.gtaoAsyncActive so the graphics submit waits its semaphore + the
// snapshot-point serial dispatch is skipped). No-op otherwise — the serialized
// graphics-queue GTAO at the snapshot point runs instead (byte-identical fallback).
void vk_gtao_async_begin( void );
// Whether the async GTAO ran this frame (the graphics submit must wait its sem).
qboolean vk_gtao_async_is_active( void );
// The per-frame async-GTAO semaphore the graphics submit waits on (NULL if none).
struct ralSemaphore_s *vk_gtao_async_wait_sem( void );
// The sync point just before tonemap samples the AO (graphics-queue barrier on the
// serial path; the async path's visibility comes from the graphics-submit sem wait).
void vk_gtao_sync_before_tonemap( void );

// Lens-glow occlusion oracle bring-up/teardown + per-frame dispatch. Mirrors the
// GTAO lifecycle: vk_lens_init mints a NEAREST sampling view over the depth copy +
// per-frame lens SSBOs + the N-tap compute pipeline (no-op when the flare system is
// off or no depth copy); vk_lens_shutdown frees them; vk_lens_dispatch records the
// per-source visibility compute into `cb` in the GTAO seam. No-op unless r_lens is on
// and there are lens sources this frame.
void vk_lens_init( struct ralBackend_s *backend );
void vk_lens_shutdown( void );
void vk_lens_dispatch( struct ralCommandBuffer_s *cb );

// IBL probe infrastructure bring-up/teardown. Creates the analytic-sky source
// cube (+ its compute pipeline) and the irradiance + radiance probe cubes with
// their per-face / per-(face,mip) storage views and full-cube sampled views.
// All inert this phase (the convolve + IBL term land later). Rides the same
// adopt/destroy sweep as the BRDF-LUT; the one-shot source fill is recorded in
// vk_begin_frame's no-render-pass seam. Idempotent across vid_restart (shutdown
// frees every view/image/pipeline + re-arms the one-shot guard).
void vk_ibl_probes_init( struct ralBackend_s *backend );
void vk_ibl_probes_shutdown( void );

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown( refShutdownCode_t code );

// Releases vulkan resources allocated during program execution.
// This effectively puts vulkan subsystem into initial state (the state we have after vk_initialize call).
void vk_release_resources( void );

void vk_wait_idle( void );
void vk_queue_wait_idle( void );

#if FEAT_SHADOW_MAPPING
// Sun-driven shadow map (4-layer cascade array; cascade 0
// is populated with world casters). vk_render_shadow_map() runs once
// per RB_LightingPass; vk_build_shadow_caster() builds the world caster
// geometry once per map load (also lazily re-entrant from vk_render_shadow_map).
void vk_render_shadow_map( void );
void vk_render_dlight_shadow( void );        // point-light omni shadow (one light)
void vk_dlight_shadow_capture_light( void ); // capture the budgeted light from the live backend view
void vk_build_shadow_caster( void );
// Hooked from RB_SurfaceMesh / RB_IQMSurfaceAnim (CPU
// path) right after the main pass deforms a mesh surface into
// tess.xyz[firstVert..numVerts] / tess.indexes[firstIdx..numIdx]; snapshots the
// model-space verts (w=1) + 0-based indices + the current entity's [axis|origin]
// into the shadowMeshSnap* CPU arrays for next frame's shadow pass to draw.
// Cheap early-out when shadow mapping is off / not an opaque caster / viewmodel
// / depth-hacked / RF_NOSHADOW. (GPU-skinned IQM never reaches this — it returns
// before tess.xyz is touched; it is handled by the skinned caster path.)
void vk_shadow_capture_mesh( int firstVert, int numVerts, int firstIdx, int numIdx );
// GPU-skinned IQM shadow caster capture: records the model's static VBO/IBO + draw
// range, the per-entity bone matrices (reused from the main draw — same pose), and
// the model->world transform, so the skinned shadow VS can skin it next frame.
void vk_shadow_capture_iqm( struct ralBuffer_s *vertBuffer,
	struct ralBuffer_s *idxBuffer,
	int firstIndex, int numIndexes, const float *boneMats, int numBones,
	const float *modelMatrix, const float *modelBounds );
#endif

//
// Resources allocation.
//
void vk_create_image( image_t *image, int width, int height, int mip_levels );
void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int miplevels, byte *pixels, int size, qboolean update, uint32_t baseArrayLayer );

// BC* compressed mip-chain upload. Caller passes the full
// concatenated mip data (as returned by R_LoadDDS) and the format /
// extents declared by the DDS header; this helper computes per-mip
// block byte sizes and dispatches vkCmdCopyBufferToImage with one
// region per mip level. image->internalFormat must be set to the
// matching VK_FORMAT_BC* before calling vk_create_image.
// When image->texType == TEXTYPE_CUBE the data is six
// face-major mip chains (+X,-X,+Y,-Y,+Z,-Z) and the upload targets
// all 6 array layers.
void vk_upload_image_data_compressed( image_t *image, int width, int height, int mipLevels, byte *data, int dataSize );

// Uncompressed volume (3D) mip-chain upload — one z-slab
// per mip, depth halving each level. Used for VK_FORMAT_R8G8B8A8_(UNORM|SRGB)
// .dds volume textures. image->texType must be TEXTYPE_3D and image->depth
// the base slice count before vk_create_image is called.
void vk_upload_image_data_3d( image_t *image, int width, int height, int depth, int mipLevels, byte *data, int dataSize );

// Dispatch a DDS image to the right upload path based on its
// format and texType (BC* / uncompressed 2D / volume / cubemap). Call after
// vk_create_image with image->internalFormat / texType / depth / layerCount
// already populated from the .dds header.
void vk_upload_dds_image_data( image_t *image, int width, int height, int depth, int mipLevels, byte *data, int dataSize );

qboolean vk_bc_format_supported( VkFormat format );
// Is `format` something R_CreateImageDDS can actually upload?
// True for any supported BC* block format, plus the uncompressed carriers
// used by cube/volume DDS assets (RGBA8 UNORM/SRGB, RGBA16F).
qboolean vk_dds_format_uploadable( VkFormat format );
void vk_update_descriptor_set( image_t *image, qboolean mipmap );
void vk_update_attachment_descriptors( void );
// Releases the portable sampling-view + arena bind-group cohort before an
// attachment generation or descriptor-arena generation is retired.
void vk_ral_release_attachment_sampler_cohorts( void );
// Releases the five SMAA sampling groups, views and adopted texture wrappers
// before either their arena generation or raw Vulkan image parents retire.
void vk_ral_release_smaa_sampler_cohorts( void );
void vk_register_black_sentinel( void );	/* sun-mask decline sentinel into bindless slot 4094 */
void vk_register_white_sentinel( void );	/* unused-texture-role sentinel into bindless slot 4093 */
void vk_destroy_samplers( void );

uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );
void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def );

// Unreachable prerequisite-A constructor. It authors one exact three-target
// PRESERVE sibling from an already assembled ordinary world pipeline and
// publishes it output-atomically. No draw/pass path calls it yet.
qboolean vk_temporal_build_preserve_pipeline(
	const VkGraphicsPipelineCreateInfo *base,
	struct ralPipelineLayout_s *layout, ralFormat_t sceneFormat,
	const char *debugName, struct ralPipeline_s **outPipeline );

void vk_create_post_process_pipeline( int program_index, uint32_t width, uint32_t height );
void vk_create_pipelines( void );

//
// Rendering setup.
//

void vk_clear_color( const vec4_t color );
void vk_clear_depth( qboolean clear_stencil );
void vk_begin_frame( const temporalBatchRequest_t *temporalRequest );
void vk_temporal_motion_release_before_ral_shutdown( void );
void vk_temporal_scene_color_attachment_published( void );
void vk_temporal_motion_readback_arm( void );
void vk_temporal_history_consume_arm( void );
void vk_temporal_resolve_readback_arm( void );
void vk_end_frame( void );
void vk_profile_markers_arm( void );
void vk_gpu_profile_dump( void );
qboolean vk_gpu_profile_sample( refGpuProfileSample_t *out );
void vk_request_swapchain_recreate( void );
void vk_present_frame( void );

void vk_end_render_pass( void );
void vk_begin_main_render_pass( void );
void vk_scene_depth_copy( void );
void vk_scene_depth_copy_final( void );
void vk_temporal_recursive_record( void );
qboolean vk_temporal_motion_seal_primary( void );
qboolean vk_temporal_resolve_prepare_authority( void );
void vk_temporal_resolved_hdr_record_copy( void );
void vk_temporal_history_store_shutdown( void );
// True when an active scene-depth consumer draws BEFORE the SS_FOG copy point,
// so the backend must force the copy earlier than the natural SS_FOG boundary.
qboolean vk_scene_depth_early_produce( void );
void vk_smaa( void );

void vk_bind_pipeline( uint32_t pipeline );
void vk_bind_index( void );
void vk_bind_index_ext( const int numIndexes, const uint32_t*indexes );
void vk_bind_geometry( uint32_t flags );
void vk_bind_lighting( int stage, int bundle );
// Pushes a zeroed per-draw UBO ring item for draw paths that carry no light/fog/
// shadow inputs and read MVP from the push constant (sky, beam, legacy projected
// dlight, debug overlays, shadow volumes). Gives each such draw its own ring item
// so vk_push_bindless_indices' index-table write does not clobber a still-in-flight
// bindless draw's item. Defined in tr_shade.c alongside VK_PushUniform.
uint32_t VK_PushUniformScratch( void );
void vk_draw_geometry( Vk_Depth_Range depth_range, qboolean indexed );
void vk_draw_forwardplus( Vk_Depth_Range depth_range );

void vk_read_pixels( byte* buffer, uint32_t width, uint32_t height ); // screenshots
qboolean vk_bloom( void );

// Tonemap and the UI compositing pass are fully
// decoupled. vk_tonemap() knows nothing about UI; vk_open_ui_pass()
// knows nothing about tonemap. The 3D→2D transition (RB_*, tr_backend.c)
// orchestrates them.
//
// vk_tonemap()        — ends the open dynamic scene pass (MAIN or post_bloom),
//                       runs the tonemap pass reading
//                       img 264 → writing img 265, then ends
//                       render_pass.tonemap. On return: no render pass
//                       open; img 265 is in SHADER_READ_ONLY_OPTIMAL.
//                       No-op if !fboActive or during the screenmap pass.
//
// vk_open_ui_pass(clear) — opens one RAL dynamic-rendering pass on img 265.
//                       qtrue CLEARs the pure-2D target after closing any open
//                       scene pass; qfalse LOADs the tonemap/SMAA result. On
//                       return the UI pass is open and renderPassIndex remains
//                       RENDER_PASS_MAIN so world/UI dynamic pipeline formats
//                       share the same typed cache cohort.
//
// vk_smaa()           — self-gated on r_smaa; precondition and
//                       postcondition: no render pass open. Reads/writes
//                       img 265. Dispatched only from the transition
//                       gameplay branch, between vk_tonemap() and
//                       vk_open_ui_pass(qfalse).
void vk_tonemap( void );
qboolean vk_atmosphere_full_execute( void );
void vk_atmosphere_fixture_smoke_receipt( void );
void vk_atmosphere_full_init( struct ralBackend_s *backend );
void vk_atmosphere_full_shutdown( void );
void RE_SetAtmosphereMediaVolumes( const atmosphereMediaVolume_t *volumes,
	uint32_t count, uint64_t digest );
void RE_SetAtmosphereSurfaceTiles( const ralAtmosphereSurfaceTile_t *tiles,
	uint32_t count, uint64_t generation );
void vk_open_ui_pass( qboolean clear );

qboolean vk_alloc_vbo( const byte *vbo_data, int vbo_size );
void vk_update_mvp( const float *m );
// Stash enhanced-fog state for the set-0 per-draw UBO. VK_PushUniform stamps
// this 32-byte portable tail into every bounded ring item.
void vk_update_fog_uniform( const vec4_t color, int fogType, float density, float farClip, qboolean enabled );
// Set a 2D scissor rect on the current command buffer. Pass NULL to restore
// the fullscreen scissor (equivalent to "no clip region").
void vk_set_2d_scissor( const int *rect );
void vk_update_msdf_outline( float outlineWidth, const float *outlineColor,
                              float glowWidth, const float *glowColor,
                              const float *shadowOffset, const float *shadowColor );

uint32_t vk_tess_index( uint32_t numIndexes, const void *src );
qboolean vk_tess_publish_shadow_range( uint32_t offset, uint32_t size );
void vk_bind_index_buffer( struct ralBuffer_s *buffer, uint32_t offset );
#ifdef USE_VBO
void vk_draw_indexed( uint32_t indexCount, uint32_t firstIndex, uint32_t firstInstance );
qboolean vk_entmat_active( void );
void vk_entmat_ensure_buffer( uint32_t requiredSlots );
void vk_entmat_begin_frame( void );
#endif
qboolean vk_temporal_motion_begin_primary_command( void );
void vk_temporal_motion_end_primary_command( qboolean admitted );
void vk_reset_descriptor( int index );
void vk_update_descriptor( int index, VkDescriptorSet descriptor );

// Bindless main shader — host per-draw packed-index write.
// vk_bindless_track records the image_t* a particular role would have been
// bound to (slot 0..5; matches the legacy set N+1 numbering), so
// vk_push_bindless_indices can resolve the (ralBindlessSlot, bindlessSamplerSlot)
// pair at draw time and push the 24 B packed payload to the FS push block at
// vkUniform_t::packed_indices. Both run
// unconditionally now (the legacy main path that gated them is gone).
void vk_bindless_track( int role, struct image_s *image );
qboolean vk_push_bindless_indices( void );

// Content 2D-array textures (R_CreateImageArray-built image_t,
// e.g. q1_ls_array.frag's animArray) get a slot in the parallel SAMPLED_IMAGE
// binding at WIRED_BINDLESS_BIND_ARRAY_IMAGES. R_CreateImageArray calls this
// right after vk_create_image so vk_bindless_track / vk_push_bindless_indices
// can pack image_t::ralBindlessSlot into the FS push range and the shader's
// WIRED_BINDLESS_TEX_ARRAY macro can sample the 2DArray view. Idempotent.
void vk_ral_register_image_array( struct image_s *image );
// Non-fatal VkFormat -> ralFormat_t for renderer attachment formats (returns
// RAL_FORMAT_UNDEFINED for any format the RAL surface cannot represent).
ralFormat_t vk_attachment_format_to_ral( VkFormat f );
void vk_update_descriptor_offset( int index, uint32_t offset );

void vk_update_post_process_pipelines( void );

const char *vk_format_string( VkFormat format );

// Print the HDR pipeline state snapshot captured during
// setup_surface_formats(). Called from GfxInfo (tr_init.c) so the
// existing `gfxinfo` console command shows current HDR plumbing
// without per-frame logging.
void vk_hdr_state_print( void );

void VBO_PrepareQueues( void );
void VBO_PrepareSubqueue( int start, int count );
int  VBO_GetQueueCount( void );
uint32_t VBO_GetQueueItemStylesPacked( int pos );
void VBO_RenderIBOItems( uint32_t firstInstance );
void VBO_ClearQueue( void );
void VBO_BeginView( void );
struct msurface_s;
void VBO_BuildSurfaceMap( const struct msurface_s *surf, int surfCount, int numStaticSurfaces );

// GPU world-surface cull. build: map-load AABB SSBO + pipeline.
// capture: front-end snapshot of frustum/reached/CPU-ref (R_AddWorldSurfaces).
// dispatch: per-frame compute in the no-render-pass seam. shutdown: teardown.
struct msurface_s;
void vk_cull_build_world_aabbs( const struct msurface_s *surfaces, int numSurfaces );
void vk_cull_init( ralBackend_t *backend );
void vk_cull_capture_world( const void *viewParms, const void *world, int viewCount );
void vk_cull_dispatch( void );
void vk_cull_shutdown( void );
// Forward+ tiled-lighting tile-classification compute (the dlight binning).
void vk_forwardplus_init( ralBackend_t *backend );
void vk_forwardplus_capture_dlights( void ); // snapshot the live dlights at RB_DrawSurfs (seam-order workaround)
void vk_forwardplus_dispatch( void );
void vk_forwardplus_shutdown( void );
void vk_forwardplus_depth_copy( void );   // frame-end: copy depth → the fp SAMPLED depth copy (next-frame reduce source)
// Forward+ lit consumer — the world-space lit pass that reads the tile light list.
void vk_forwardplus_lit_init( ralBackend_t *backend );
void vk_forwardplus_lit_shutdown( void );
// Host frame-current reproduction of the GPU world cull (per-surface AABB-frustum +
// backface over the host-coherent AABB SSBO + this-frame frustum/viewOrigin). Drives the
// r_gpuBatchDecomp world draw frame-current (the GPU compute stays verification-only).
// qfalse if no AABB SSBO.
qboolean vk_cull_host_derive_visible( const cplane_t frustum[4], const vec3_t viewOrigin,
                                      const byte *reached, byte *visibleOut, int n );

#if FEAT_IQM
// IQM GPU skinning
#define IQM_GPU_MAX_JOINTS 128
#define IQM_BONE_UBO_SIZE (IQM_GPU_MAX_JOINTS * 3 * sizeof(vec4_t))
#define IQM_MVP_UBO_OFFSET IQM_BONE_UBO_SIZE
#define IQM_UBO_TOTAL_SIZE (IQM_BONE_UBO_SIZE + 64)
struct drawSurf_s;
void vk_init_iqm_gpu_skinning( void );
void vk_shutdown_iqm_gpu_skinning( void );
qboolean vk_create_iqm_vbo( struct ralBuffer_s **outVertBuf,
	struct ralBuffer_s **outIdxBuf,
	const byte *vertData, int vertSize,
	const byte *idxData, int idxSize );
void vk_destroy_iqm_vbo( struct ralBuffer_s **vertBuf,
	struct ralBuffer_s **idxBuf );
void vk_draw_iqm_gpu( struct ralBuffer_s *vertBuffer,
	struct ralBuffer_s *idxBuffer,
	int firstIndex, int numIndexes,
	const float *boneMats, int numBones,
	struct ralBindGroup_s *textureGroup,
	const float *mvp );
qboolean vk_temporal_iqm_prescan_primary_command(
	const struct drawSurf_s *drawSurfs, int numDrawSurfs );
qboolean vk_temporal_iqm_bind_primary_command( void );
void vk_temporal_iqm_publish_drawsurf_ordinal( uint32_t ordinal );
void vk_temporal_iqm_reset_drawsurf_ordinal( void );
#endif

typedef struct vk_tess_s {
	VkCommandBuffer command_buffer;
	// Per-slot persistent RAL command
	// buffer acquired once at vk_initialize via Ral_AcquireCommandBuffer.
	// command_buffer above is an ALIAS to Ral_GetCommandBufferHandle(ral_cmd)
	// for residual unmigrated recording commands only. Begin/End/Recycle and
	// submit use the generation-bound RAL lifecycle; no external-lifecycle
	// bypass or per-frame wrapper allocation remains.
	struct ralCommandBuffer_s *ral_cmd;

	VkSemaphore image_acquired;
	struct ralSemaphore_s *ral_image_acquired;  // adopted sibling for typed Ral_AcquireNextImage's signalSem arg
	uint32_t	swapchain_image_index;
	struct ralTexture_s *swapchain_image;        // borrowed canonical image returned by Ral_AcquireNextImage; valid until swapchain recreation
	qboolean	swapchain_image_acquired;
	qboolean	swapchain_image_prepared;
	qboolean	swapchain_recreate_after_present;
	uint64_t	swapchain_generation;
	// Which bloom_image index the currently-open dynamic bloom pass writes (set at
	// the extract/blur begin, read by vk_end_render_pass's BLOOM_EXTRACT/BLUR
	// handlers to barrier the just-written image to SHADER_READ). Extract → 0;
	// blur loop index k → k+1. Mirrors how swapchain_image_index threads the
	// gamma present-pass target across begin/end.
	uint32_t	bloom_pass_target;
#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished2;
	struct ralSemaphore_s *ral_rendering_finished2;  // adopted sibling fed into ralSubmitInfo_t at vk_flush_staging_buffer's submit
#endif
	VkFence rendering_finished_fence;
	struct ralFence_s *ral_rendering_finished_fence;   // adopted sibling fed into ralSubmitInfo_t.signalFence at the per-frame Ral_Submit call site (qvkQueueSubmit retired)
	qboolean waitForFence;

	struct ralBuffer_s *ral_vertex_buffer;
	byte *vertex_buffer_ptr; // CPU shadow; bounded ranges publish through RAL
	uint32_t vertex_buffer_offset; // VkDeviceSize

	VkDescriptorSet uniform_descriptor;
	struct ralBindGroup_s *ral_uniform_descriptor;
	uint32_t		uniform_read_offset;
	VkDeviceSize	buf_offset[8];
	VkDeviceSize	vbo_offset[8];

	struct ralBuffer_s *curr_index_buffer;
	uint32_t		curr_index_offset;

	struct {
		uint32_t		start, end;
		// Slot 7 is the bindless-main-shader descriptor set
		// (set WIRED_BINDLESS_SET). Stored alongside the rotating sets so
		// vk_bind_descriptor_sets folds it into the per-draw exact RAL set
		// transaction — Vulkan's pipeline-layout-compat
		// rule disturbs set 7 whenever an incompatible layout is bound
		// (particle compute, MSDF, SMAA, post-process, shadow CSM), so a
		// once-per-frame bind is insufficient; the rebind must follow the
		// per-draw rotating-set discipline.
		VkDescriptorSet	current[WIRED_MSDF_SET + 1]; // 0:uniform, 1:bindless-main, 2:engine-resources, 3:MSDF per-draw UBO (set 3 used by the MSDF layout only)
		uint32_t		offset[WIRED_MSDF_SET + 1]; // dynamic offsets keyed by set index: [0]=set-0 uniform ring, [WIRED_MSDF_SET]=set-3 MSDF ring (slots 1/2 unused — those sets are non-dynamic)
		// Per-frame parallel ring
		// bind-group mirror deleted alongside the parallel-bind helper.
	} descriptor_set;

	// Bindless main shader — per-draw image-pointer mirror
	// shaped by WIRED_BINDLESS_INDEX_COUNT roles (7 once screenmap was added).
	// Populated by vk_bindless_track at every image-bind site (GL_Bind +
	// tr_shade.c's special-case binds); read by vk_push_bindless_indices
	// right before the draw to pack (tex:12, sampler:8) uint32 indices into
	// the FS push block. NULL slot = "use sentinel" (whiteImage / sampler 0).
	// Always populated (the legacy gate is
	// gone). Role WIRED_BINDLESS_SCREENMAP_ROLE (6) is special-cased in
	// vk_push_bindless_indices — overridden from vk.bindless_screenmap_sampler_slot
	// rather than this image_t mirror; the slot here at index 6 is ignored.
	struct image_s *bindless_slot_image[WIRED_BINDLESS_INDEX_COUNT];

	Vk_Depth_Range		depth_range;
	// Which dynamic-rendering pass (if any) is currently open. The main 3D pass,
	// the UI pass, and the screenmap (mirror/portal) pass are dynamic-rendering
	// (Ral_BeginRendering); they are closed through the shared vk_end_render_pass(),
	// which uses this to pick Ral_EndRendering + the right attachment hand-off
	// barrier (different target image per pass). Raw VkRenderPass command recording
	// is retired; 0 means no pass is open and ending it is a fail-closed error.
	enum { VK_DYN_PASS_NONE = 0, VK_DYN_PASS_MAIN, VK_DYN_PASS_TEMPORAL_MAIN, VK_DYN_PASS_TEMPORAL_POST_BLOOM, VK_DYN_PASS_UI, VK_DYN_PASS_SCREENMAP, VK_DYN_PASS_CAPTURE, VK_DYN_PASS_GAMMA, VK_DYN_PASS_BLOOM_EXTRACT, VK_DYN_PASS_BLUR } open_dynamic_pass;
	/* The generic RAL pipeline (MAIN/SCREENMAP) most-recently bound this pass.
	 * Reset at pass-begin / special-pipeline cleanup sites so the first world draw
	 * of each pass re-binds after Ral_BeginRendering. */
	struct ralPipeline_s *last_ral_pipeline;
	/* Layout the most-recently-bound pipeline was created with. Used by
	 * vk_bind_descriptor_sets so the BindDescriptorSets call passes a layout
	 * compatible with the bound pipeline (matters for the MSDF pipeline,
	 * which uses vk.pipeline_layout_msdf — different push-constant ranges
	 * from the main vk.pipeline_layout).
	 * VK_NULL_HANDLE = no pipeline bound; bind path falls back to
	 * vk.pipeline_layout (the historical hardcoded default). */
	VkPipelineLayout	last_pipeline_layout;

	/* qtrue when the most-recently-bound pipeline is a soft-particle depth-fade
	 * variant (the dfade_* fragment module selected at create_pipeline). Read by
	 * vk_push_bindless_indices to override bindless role 4 with the scene-depth
	 * copy ONLY for these draws (so lm2 — role 4 elsewhere — is never clobbered).
	 * Set from the exact RAL pipeline slot at every generic bind. */
	qboolean			depthFadeDraw;

	uint32_t num_indexes; // value from most recent vk_bind_index() call

	VkRect2D scissor_rect;

	// Per-command-buffer-slot host-coherent storage buffer of per-entity
	// transforms (one slot per main draw, written at the draw from this draw's
	// uniform-ring item), indexed in the vertex shader by gl_InstanceIndex ==
	// firstInstance. Active only on the r_entitySSBO path; a storage-buffer clone
	// of shadowEntMat* below. Each slot is the main-path transform pair (mvp, and
	// modelMatrix under FEAT_SHADOW_MAPPING) — see ENTITY_MATRIX_SLOT_BYTES.
	// Lazily (re)created on growth; the descriptor re-allocated on grow OR after a
	// pool reset. entMatSlot counts draws written this frame (reset at frame begin).
	struct ralBuffer_s *ral_entMatBuf;
	void           *entMatMapped; // CPU shadow; never a persistent GPU mapping
	VkDeviceSize    entMatSize;
	VkDescriptorSet entMatDesc;
	struct ralBindGroup_s *ral_entMatDesc;
	uint32_t        entMatSlot;
	uint32_t        entMatLastUniformOffset;
	uint32_t        entMatAllocationGeneration;

#if FEAT_SHADOW_MAPPING
	// Per-command-buffer-slot host-coherent buffer that
	// the previous frame's deformed-mesh shadow-caster snapshot is staged into by
	// vk_render_shadow_map() (verts then 0-based indices); the per-cascade
	// animated-mesh draw loop reads it (not yet drawn). Lazily
	// (re)created on growth; destroyed in vk_shutdown.
	struct ralBuffer_s *ral_shadowSnapBuf;
	void           *shadowSnapMapped;
	VkDeviceSize    shadowSnapSize;
	// Per-frame host-coherent SSBO of caster model->world matrices (mat4[],
	// std430). vk_render_shadow_map fills it once/frame (casc==0) and indexes
	// per-draw via firstInstance==gl_InstanceIndex (the per-entity push retired).
	// Lazily (re)created on growth; the descriptor is re-allocated on a grow OR
	// after a descriptor-pool reset (handle nulled). Destroyed in
	// vk_shutdown_shadow_snap. Replaces the per-(entity x cascade) model-matrix push.
	struct ralBuffer_s *ral_shadowEntMatBuf;
	void           *shadowEntMatMapped;
	VkDeviceSize    shadowEntMatSize;
	VkDescriptorSet shadowEntMatDesc;
	struct ralBindGroup_s *ral_shadowEntMatDesc;
#endif
} vk_tess_t;


// ── shared primitive shader image registry ──────────────────────────
//
// Global registry of resolved image_t* per primitive shader, used by
// wired primitive pipelines that need texturing. **Slot index ≠ qhandle.**
// The two are decoupled: the qhandle space (MAX_SHADERS=16384) is
// monotonic across all RE_RegisterShader calls (world textures, models,
// UI, primitive); the registry has only PRIMITIVE_SHADER_IMAGE_MAX = 64
// slots. RE_RegisterPrimitiveShader allocates a slot per shader via
// vk_alloc_primitive_shader_image_slot and records the mapping in
// vk.qhandle_to_prim_slot[]. SSBO write sites translate cgame-supplied
// qhandles to slots via vk_qhandle_to_prim_slot at submit time.
//
// Slot 0 is reserved for tr.whiteImage. Slots 1..63 are dynamically
// assigned to registered primitive shaders. Out-of-range or
// unregistered qhandles map to slot 0, rendering "untextured" (white
// texel × vertex color = vertex color) instead of producing OOB SSBO
// reads.
//
// Consumers: ribbon and rail-ribbon (texture binding 2, sampler binding 3),
// beam (texture binding 1, sampler binding 4). Particle has its own per-class
// registry because it indexes by particleClassHandle_t, not qhandle_t.
//
// vk_register_primitive_shader_image idempotently publishes both the host
// array slot and every dependent direct RAL bind group.
#define PRIMITIVE_SHADER_IMAGE_MAX 64

// Upper bound on the qhandle space used to size the
// qhandle→primitive-slot indirection table. Must equal MAX_SHADERS
// in tr_local.h (1<<SHADERNUM_BITS = 16384). vk.h is included by
// tr_local.h before MAX_SHADERS is defined, so we need a parallel
// constant here; a _Static_assert in vk.c verifies they agree.
#define VK_PRIM_QHANDLE_MAX 16384

extern struct image_s *vk_primitive_shader_images[PRIMITIVE_SHADER_IMAGE_MAX];

void vk_init_primitive_shader_images( void );
// Takes a primitive registry slot index (NOT a qhandle).
// Slot range [1, PRIMITIVE_SHADER_IMAGE_MAX); slot 0 is reserved for
// tr.whiteImage. Callers from RE_RegisterPrimitiveShader should go
// through vk_alloc_primitive_shader_image_slot instead, which is the
// stable allocator that maintains the qhandle→slot lookup table.
void vk_register_primitive_shader_image( int slot, struct image_s *image );

// Allocate (or reuse) a registry slot for a stage>0 image
// whose qhandle slot is already taken by stage 0's image. Linear
// search: returns existing slot if `image` is already registered;
// otherwise writes into the first slot still holding tr.whiteImage
// and broadcasts the descriptor write. Returns -1 on exhaustion.
int  vk_alloc_primitive_shader_image_slot( struct image_s *image );
void vk_shutdown_primitive_stages( void );

// Translate a cgame qhandle (whatever value
// RE_RegisterShader returned for the primitive shader) into the
// engine-internal primitive registry slot (0..PRIMITIVE_SHADER_IMAGE_MAX-1).
// Used at SSBO write time to pack a small slot index into the GPU
// header where the previous design had relied on (qhandle == slot).
// Out-of-range or unregistered qhandles return slot 0 (whiteImage),
// rendering as untextured rather than producing OOB SSBO reads.
unsigned int vk_qhandle_to_prim_slot( qhandle_t h );

// Per-stage rendering parameters for multi-stage primitive
// shaders, packed for the GPU SSBO. std430, 32 bytes per entry,
// indexed [shaderHandle * PRIMITIVE_STAGE_MAX + stageNumber].
//
// Trailing entries (stageNumber >= stageCount) are zero-initialized
// and should not be drawn — caller checks
// vk.primitive_shader_stage_counts[handle] before per-stage draws.
//
// blendPacked encodes (srcBlend << 16) | dstBlend so the 32B target
// fits without padding. GLSL unpacks via shifts.
#define VK_PRIMITIVE_STAGE_BYTES 32u

typedef struct {
	uint32_t imageSlot;          // bytes  0..3
	uint32_t blendPacked;        // bytes  4..7   (src << 16) | dst
	uint32_t rgbGen;             // bytes  8..11  primRgbGen_t
	uint32_t alphaGen;           // bytes 12..15  primAlphaGen_t
	float    uvScale[2];         // bytes 16..23
	float    uvScroll[2];        // bytes 24..31
} VkPrimitiveStageGPU;

#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
_Static_assert( sizeof( VkPrimitiveStageGPU ) == VK_PRIMITIVE_STAGE_BYTES,
	"VkPrimitiveStageGPU must be 32 bytes (std430-aligned, vec4-stride friendly)" );
#endif

// ── primitive particle (host-side type definitions) ────────────────
//
// These live outside Vk_Instance because C does not allow nested
// typedefs inside a struct definition. The particle subsystem state
// itself (buffers / pipelines / descriptor sets / cursors) lives as
// the `vk.particle` member inside Vk_Instance below.
//
// PARTICLE_BYTES (64) and PARTICLE_CLASS_GPU_BYTES (736) are the
// std430 sizes computed for the matching GLSL structs in
// particle_integrate.comp / particle.vert. PARTICLES_PER_POOL is the
// fixed pool capacity. _Static_assert in vk_init_particle catches
// any drift between this C layout and the std430 stride.
#define PARTICLES_PER_POOL          16384u
#define PARTICLE_BYTES                 64u  // sizeof(GPU Particle), std430
#define PARTICLE_SPAWN_REQUEST_MAX   1024u
#define PARTICLE_SPAWN_REQUEST_BYTES   96u
#define PARTICLE_CLASS_GPU_BYTES      736u  // sizeof(ParticleClassGPU), std430
#define PARTICLE_ATMOSPHERE_PROFILE_BYTES 592u
#define PARTICLE_CHILD_EVENT_MAX      1024u
#define PARTICLE_CHILD_EVENT_BYTES      64u
#define PARTICLE_CHILD_SPAWN_MAX        64u
#define PARTICLE_CHILD_PARTICLE_MAX    4096u
#define PARTICLE_CHILD_STAGE_COUNTERS \
	( ATMOSPHERE_EFFECT_MAX_PROFILES * ATMOSPHERE_EFFECT_MAX_STAGES )
#define PARTICLE_CHILD_PROFILE_COUNTERS ATMOSPHERE_EFFECT_MAX_PROFILES
#define PARTICLE_CHILD_COUNTER_BYTES \
	( ( PARTICLE_CHILD_STAGE_COUNTERS + PARTICLE_CHILD_PROFILE_COUNTERS ) \
		* sizeof( uint32_t ) )
#define PARTICLE_CHILD_COUNTER_ELEMENTS \
	( PARTICLE_CHILD_COUNTER_BYTES / PARTICLE_CHILD_EVENT_BYTES )

// Host-side mirror of GLSL std430 ParticleClassGPU. Field order +
// trailing pads MUST exactly match particle_integrate.comp /
// particle.vert. Padding is required because std430 rounds the
// struct's stride up to its largest member alignment (vec4 = 16);
// the natural C packing would leave 8 trailing bytes off, causing
// silent misreads from element 1 onward in the classes[] SSBO.
//
// shaderBlendIsAdditive: derived at class
// registration time from `cls->shader`'s stateBits (additive vs
// alpha-blend). Replaces the cgame-supplied PRIM_FLAG_ADDITIVE bit
// for blend-variant filtering — particle pipeline now honors the
// underlying shader script's blendFunc, matching CPU rendering
// semantics. The cgame `renderFlags` field still ships through but
// is informational only; see primitives.h.
// Host-side mirror of GLSL std430 ParticleParm. 32 B / 2 vec4 — the trailing
// pads exist to make that exact, so the struct can be embedded in an array
// without std430 introducing stride surprises.
//
// Mirrors particleParm_t in qcommon/wired/render/particle_class.h field for
// field; the two are copied member-wise in RE_RegisterParticleClass rather
// than memcpy'd, because the host type uses int/float and this one must be
// explicit about which lanes the shader reads.
typedef struct {
	int32_t  calc;       // particleParmCalc_t
	int32_t  hasCurve;   // 0 = samples[] unused; 1 = samples[] carries a shape
	float    val0;
	float    val1;
	float    variance;
	float    parmPad0;
	float    parmPad1;
	float    parmPad2;
	// The curve RESOLVED at registration, not an index into a second buffer.
	//
	// Curves are shared on the HOST — CG_RegisterParticleCurve dedups by
	// name, so editing one reaches every class built on it — but the GPU
	// receives a flattened copy. That trade is deliberate: an index would
	// need a second SSBO binding and an indirection per evaluation, on the
	// hot path, to save 2 KB. Resolving at upload costs 32 B per parm and
	// nothing per particle.
	float    samples[8]; // PARTICLE_CURVE_SAMPLES
} particleParmGPU_t;

typedef struct {
	uint32_t shader;
	uint32_t renderFlags;
	uint32_t emitMode;
	uint32_t scatterShape;
	float    scatterMagnitude;
	uint32_t velocityShape;
	float    axialSpeed;
	float    cubeJitter;
	float    coneHalfAngle;
	float    lifetimeMean;
	float    lifetimeJitter;
	int32_t  paletteCount;
	vec4_t   colorPalette[16];   // PARTICLE_CLASS_MAX_PALETTE
	vec4_t   colorEndMult;
	float    sizeStart;
	float    sizeEnd;
	float    gravityScale;
	float    drag;
	uint32_t shaderBlendIsAdditive;  // was pad0; 0=alpha, 1=additive
	uint32_t pad1;
	uint32_t pad2;
	uint32_t pad3;
	// Expressivity extension. Appended at the end so previously-set
	// offsets (palette, colorEndMult, size/gravity/drag,
	// shaderBlendIsAdditive) stay byte-identical. .w lanes on the two
	// vec4s are unused — only .xyz carry data — but storing as vec4
	// avoids std430 vec3-stride traps and keeps offsets 16-aligned.
	vec4_t   velocityBias;           // 352..367; .xyz = constant added to vel post-shape
	vec4_t   velocityBiasJitter;     // 368..383; .xyz = symmetric crandom() per-axis jitter
	float    speedJitter;            // 384..387; axialSpeed scatter, picked at emit
	float    sizeJitter;             // 388..391; sizeStart scatter, picked at emit
	uint32_t colorDomain;            // 392..395; was pad4 — CD_SRGB(0) | CD_LINEAR(1)
	                                 //           of the class's resolved stage-0 image; the vertex
	                                 //           shader packs it into bit 31 of particleClassHandle.
	uint32_t pad5;                   // 396..399; ends the original 400 B (= 25 * vec4) block
	// Sprite-frame (flipbook) animation. Appended at the end so all prior
	// offsets stay byte-identical. frameSlots[i] is the resolved sampler-
	// array slot (64..95, the frame pool) for frame i; frameCount 0/1 →
	// unused (single `shader` path). frameBlend 1 → vert forwards adjacent
	// frame slots + a blend factor for sub-frame interpolation.
	uint32_t frameSlots[16];         // 400..463; PARTICLE_CLASS_MAX_FRAMES (4 * vec4)
	uint32_t frameCount;             // 464..467
	uint32_t frameBlend;             // 468..471
	uint32_t framePad0;              // 472..475
	uint32_t framePad1;              // 476..479; ends the 480 B (= 30 * vec4) block
	// Curve-valued parameters. Appended at the end like every extension
	// before them, so all prior offsets stay byte-identical. Each mirrors a
	// particleParm_t and is 32 B (2 * vec4) by construction — the host struct
	// carries explicit padding for exactly this reason, so no per-field
	// std430 alignment rules are needed here.
	//
	// A parm whose calc is PARM_CONSTANT with val0 == 0 was never authored;
	// the shader falls back to the scalar field it overrides. That is what
	// keeps every pre-curve class rendering bit-identically.
	particleParmGPU_t sizeParm;      // 480..543
	particleParmGPU_t alphaParm;     // 544..607
	particleParmGPU_t dragParm;      // 608..671
	particleParmGPU_t gravityParm;   // 672..735; total stride 736 B (= 46 * vec4)
} particleClassGPU_t;

// Host-side mirror of GLSL std430 Particle (per-particle pool slot).
// Layout MUST exactly match the Particle struct in
// particle_integrate.comp / particle.vert. Size is fixed at 64 B
// (PARTICLE_BYTES); a sizeof check in vk_init_particle catches drift.
//
// In std430 a `vec3` followed by a `float` packs into a single 16 B
// slot (offset 0..11 for vec3, 12..15 for float), so two such pairs
// occupy 32 B. The next 16 B carries classHandle + paletteIndex +
// sizeJitterPick + pad1, and the trailing 16 B is reserved (pad2..5)
// to round the per-element stride up to 64 B (= 4 * vec4); std430
// rounds the array element stride up to the largest member's
// alignment (vec3 → 16 B), so sizes between 16-multiples would still
// pad implicitly — making the extra fields explicit avoids surprise
// at host-side memcpy time.
typedef struct {
	vec3_t   pos;            //  0..11
	float    age;            // 12..15
	vec3_t   vel;            // 16..27
	float    lifetimeInv;    // 28..31
	uint32_t classHandle;    // 32..35  (1..MAX_PARTICLE_CLASSES, 0 = dead)
	uint32_t paletteIndex;   // 36..39  index into class colorPalette[]; rand() % paletteCount at emit
	float    sizeJitterPick; // 40..43  per-particle sizeStart offset = crandom() * cls->sizeJitter
	uint32_t pad1;           // 44..47
	uint32_t pad2;           // 48..51
	uint32_t pad3;           // 52..55
	uint32_t pad4;           // 56..59
	uint32_t pad5;           // 60..63  total stride 64 B
} particleGPU_t;

// One CPU-authored emitter request expanded by the compute shader. Request
// count is bounded independently from particle count, so submission cost is
// O(emitters/stages), never O(particles). The target slot range is reserved by
// the host's monotonic ring cursor; deterministic hashes derive all variation.
typedef struct {
	vec4_t origin;           // xyz emission origin, w unused
	vec4_t axis;             // xyz direction, w unused
	vec4_t end;              // xyz path end, w unused
	vec4_t colorTint;        // linear RGBA multiplier
	uint32_t classHandle;
	uint32_t count;
	uint32_t firstSlot;
	uint32_t seed;
	uint32_t profileHandle;  // 0 for generic particles
	uint32_t stageIndex;     // valid when profileHandle != 0
	uint32_t stageFlags;     // ATMOSPHERE_STAGE_INHERIT_*
	uint32_t reserved;
} particleSpawnGPU_t;

// Header occupies event-storage element zero. GPU atomics update the first
// four lanes; the host rewrites the complete header before the current
// frame-slot dispatch. Events begin at byte 64, preserving a simple std430
// array and keeping every range bounded/WebGPU-portable.
typedef struct {
	uint32_t dispatchX;       // VkDispatchIndirectCommand-compatible prefix
	uint32_t dispatchY;
	uint32_t dispatchZ;
	uint32_t dispatchPad;
	uint32_t eventCount;
	uint32_t particleCursor;
	uint32_t droppedEvents;
	uint32_t droppedParticles;
	uint32_t eventBaseSlot;
	uint32_t particleBudget;
	uint32_t eventCapacity;
	uint32_t collisionEnabled;
	float worldMins[2];
	float worldMaxs[2];
} particleChildEventHeaderGPU_t;

typedef struct {
	vec4_t origin;
	vec4_t velocity;
	vec4_t colorTint;
	uint32_t profileHandle;
	uint32_t stageIndex;
	uint32_t count;
	uint32_t seed;
} particleChildEventGPU_t;

// ── GPU-resident atmospheric precipitation ─────────────────────────────
//
// A dedicated GPU pool, separate from the 64-class particle path. The
// cgame emits one weather descriptor on weather change; the compute shader
// continuously self-spawns / integrates / collides (against a heightgrid
// texture) / distance-culls, and the render pass draws each live slot as a
// rain streak or snow billboard. Mirrors the particle ping-pong + frame UBO
// idioms; WebGPU-portable (host-coherent SSBO + UBO + a single sampler2D,
// bounded pool, no bindless).
//
// ATM_PARTICLE_BYTES (32) is the std430 per-slot stride; a _Static_assert in
// vk_init_atmospheric catches drift. ATM_HEIGHTGRID_SIZE matches the cgame
// tracemap edge (TRACEMAP_SIZE = 256); the heightgrid is an R32F 256×256
// data texture the compute shader samples for ground collision.
#define ATM_PARTICLES_PER_POOL      8192u
#define ATM_PARTICLE_BYTES            32u  // sizeof(atmParticleGPU_t), std430
#define ATM_HEIGHTGRID_SIZE          256u  // R32F ground-height texture edge

// Host-side mirror of the GLSL std430 atmospheric particle slot. 32 B
// (two vec3+float pairs): pos.xyz + seed, vel.xyz + flags. `flags` carries
// the active/type state (0 = inactive → respawn; else falling). `seed` is a
// per-slot pseudo-random seed for the GPU respawn hash.
typedef struct {
	vec3_t   pos;            //  0..11
	float    seed;           // 12..15
	vec3_t   vel;            // 16..27
	float    flags;          // 28..31  0 = inactive (respawn), 1 = falling
} atmParticleGPU_t;

// Host-side mirror of GLSL std140 AtmFrame. 1152 B (= 72 * vec4). Two
// disjoint write owners, like particleFrame_t:
//   render region  (bytes   0..95): mvp, viewLeft, viewUp. Filled in
//                                   RB_DrawAtmospheric (backEnd.viewParms valid).
//   compute region (bytes 96..351): eyeWorld, simulation/weather fields and
//                                    the complete climate/surface/sky/media
//                                    snapshot. Filled in
//                                    RB_RunAtmosphericCompute (vk_begin_frame,
//                                    before the main render pass). eyeWorld is
//                                    compute-owned (distance-cull) but also read
//                                    by the vertex shader.
// Do NOT memcpy the whole struct from either site — that races the other's
// fields. Field-by-field writes keep the regions disjoint.
typedef struct {
	// ── render region (filled in RB_DrawAtmospheric) ────────────
	float    mvp[16];        //   0..63
	float    viewLeft[4];    //  64..79
	float    viewUp[4];      //  80..95
	// ── compute region (filled in RB_RunAtmosphericCompute) ─────
	float    eyeWorld[4];    //  96..111  eye for distance-cull (also read by VS)
	float    dt;             // 112..115
	float    time;           // 116..119  scene floatTime (snow wind sway + respawn hash)
	uint32_t poolSize;       // 120..123
	uint32_t pingPongRead;   // 124..127
	float    boundsMin[4];   // 128..143  world spawn volume min (xyz)
	float    boundsMax[4];   // 144..159  world spawn volume max (xyz)
	float    worldMins[2];   // 160..167  heightgrid xy origin
	float    worldMaxs[2];   // 168..175  heightgrid xy extent
	float    invGridStep[2]; // 176..183  1 / cell-size (world units → grid texels)
	uint32_t gridSize;       // 184..187  heightgrid edge (256)
	uint32_t type;           // 188..191  0 = none, 1 = rain, 2 = snow
	float    distance;       // 192..195  eye-relative cull radius
	float    computePad[3];  // 196..207  std140 alignment for the climate vectors
	float    windGust[4];    // 208..223  xyz wind, w gust strength
	float    precipitation[4];//224..239 rain, snow, sleet, hail weights
	float    dustAsh;        // 240..243 dust / ash precipitation weight
	float    indoorExposure; // 244..247 world-space precipitation exposure multiplier
	uint32_t seed;           // 248..251 full-width deterministic climate seed
	uint32_t climatePad;     // 252..255 reserved
	float    climate[4];     // 256..271 temperature, humidity, visibility, transition
	float    surfaceClimate[4];//272..287 wetness, frost, snow, melt
	float    sun[4];         // 288..303 direction.xyz, intensity
	float    moon[4];        // 304..319 direction.xyz, intensity
	float    ambientCloud[4];// 320..335 ambient.rgb, cloud cover
	float    cloudMedia[4];  // 336..351 cloud shadow, lightning, media density/falloff
	float    effectMeta[4]; // precipitation count, semantic workload count
	float    effectWorkloads[48][4]; // Vulkan weather keeps these zero; its
	                                // generic effect graph owns semantic FX.
	// Render-only soft-particle depth-fade params. Final publication fills the
	// last 16 bytes after the contiguous compute-owned region.
	float    renderParams[4];
} atmFrame_t;

// GPU decal ring. DECALS_PER_POOL is the fixed pool capacity: marks are
// lower-churn than particles (a few per impact + a contact blob per player,
// vs thousands of particles), so 4096 live decals is generous headroom while
// keeping the host-coherent SSBO small (4096 * 64 B = 256 KB). DECAL_BYTES is
// the std430 per-element stride a future projector vertex/fragment shader
// reads; a _Static_assert in vk_init_decal catches drift.
#define DECALS_PER_POOL             4096u
#define DECAL_BYTES                   64u  // sizeof(decalGPU_t), std430
// Bounded sampler-array size for the projector fragment shader (binding 2).
// Matches MAX_PARTICLE_CLASSES (64) — WebGPU-portable bounded array, no
// bindless. The qhandle→slot registry (vk.decal.images[]) is this wide.
#define MAX_DECAL_TEXTURES            64u

// Host-side mirror of the GLSL std430 Decal a later projector pass will read
// (renderer-private — distinct from the cgame-supplied decalDesc_t, like
// particleGPU_t vs the emit desc). Packed as vec4s so the std430 element
// stride stays 16-aligned with no vec3-stride surprises. The .w lanes carry
// scalars folded next to their vec3 so the projector reads one vec4 per pull:
//   originRadius.xyz = world origin, .w = projection radius
//   normalOrient.xyz = surface normal, .w = tangent-frame roll (radians)
//   rgba             = colour * alpha at spawn
//   textureIndex     = resolved decal texture slot (P2 fills the projector's
//                      sampler array; the qhandle is resolved at spawn time)
//   spawnTime        = tr.refdef.floatTime at emit (GPU fade reads age = now - spawn)
//   lifetimeInv      = 1/lifetime for the GPU fade ramp (0 = no auto-fade yet)
// Total stride 64 B (= 4 * vec4); explicit pads make the std430 rounding
// visible at host memcpy time. WebGPU-portable: a plain storage-buffer struct,
// no push-constants / input-attachments.
typedef struct {
	vec4_t   originRadius;   //  0..15  xyz = origin, w = radius
	vec4_t   normalOrient;   // 16..31  xyz = normal, w = orientation (radians)
	vec4_t   rgba;           // 32..47  colour * alpha
	uint32_t textureIndex;   // 48..51  resolved decal texture slot
	float    spawnTime;      // 52..55  tr.refdef.floatTime at emit
	float    lifetimeInv;    // 56..59  1/lifetime for GPU fade (0 = none yet)
	uint32_t blendMode;      // 60..63  per-decal blend mode (0=alpha,1=additive,
	                         //         2=colour); selects the matching projector
	                         //         pipeline via the vertex-stage degenerate-cull.
	                         //         Total stride 64 B.
} decalGPU_t;

// Host-side mirror of GLSL std140 DecalFrame (the projector pass per-frame
// UBO). 80 B; std140 rounds the mat4 to 16-B alignment and the trailing vec4
// keeps the block a vec4 multiple. RB_DrawDecals fills this each frame before
// recording the draw, one slot per cmd_index (ring like particleFrame_t).
//   mvp        = world→clip (projection y-flipped for Vulkan, world.modelMatrix)
//   timeCount  = .x scene floatTime (GPU fade reads age = now - spawnTime),
//                .y decal pool size (degenerate-cull bound in the vertex shader),
//                .zw reserved pad.
//   invMvp     = inverse of the SAME y-flipped mvp above (clip→world), so the
//                fragment shader reconstructs the surface world position from a
//                sampled scene-depth value to box-project the decal onto it.
//   reconParams= .xy 1/renderWidth, 1/renderHeight (screen UV from gl_FragCoord),
//                .z depthValid (1.0 when the scene-depth copy is fresh this frame;
//                0.0 → fragment falls back to the flat-quad path, no discard),
//                .w reserved pad.
// A _Static_assert in vk_init_decal catches std140 drift.
typedef struct {
	float    mvp[16];          //   0..63
	float    timeCount[4];     //  64..79  x=floatTime, y=poolSize, zw=pad
	float    invMvp[16];       //  80..143 clip→world (inverse of the y-flipped mvp)
	float    reconParams[4];   // 144..159 xy=1/render{W,H}, z=depthValid, w=pad
} decalFrame_t;

// Host-side mirror of GLSL std140 ParticleFrame. 144 B; std140
// rounds vec4 / mat4 members to 16 B alignment, scalars to 4 B.
//
// Three write owners during a frame:
//   render region  (bytes   0..111, 112 B): mvp, viewLeft, viewUp,
//                                           eyeWorld. Filled in
//                                           RB_DrawParticles, where
//                                           backEnd.viewParms is valid.
//   compute region (bytes 112..127,  16 B): dt, poolSize, numClasses,
//                                           pingPongRead. Filled in
//                                           RB_RunParticleCompute,
//                                           which now runs from
//                                           vk_begin_frame BEFORE the
//                                           main render pass opens.
//   shared region  (bytes 128..143,  16 B): reserved padding. Word 0
//                                           (offset 128) was the legacy
//                                           `identityLight` halving
//                                           factor — dropped (the linear
//                                           pipeline has no overbright
//                                           halve) and the field removed
//                                           entirely; now
//                                           just std140 vec4-stride pad
//                                           keeping the 144 B (9×vec4)
//                                           stride byte-stable.
// Regions are contiguous and disjoint; do NOT memcpy the entire struct
// from either site — that races / overwrites the other site's fields.
typedef struct {
	// ── render region (filled in RB_DrawParticles) ──────────────
	float    mvp[16];        //   0..63
	float    viewLeft[4];    //  64..79
	float    viewUp[4];      //  80..95
	float    eyeWorld[4];    //  96..111
	// ── compute region (filled in RB_RunParticleCompute) ────────
	float    dt;             // 112..115
	uint32_t poolSize;       // 116..119
	uint32_t numClasses;     // 120..123
	uint32_t pingPongRead;   // 124..127
	// ── render region cont.: soft-particle depth-fade params (filled in
	//    RB_DrawParticles alongside mvp, repurposing the former pad lanes) ─
	float    invResX;        // 128..131  1/renderWidth  (screen UV from gl_FragCoord)
	float    invResY;        // 132..135  1/renderHeight
	float    depthValid;     // 136..139  1.0 when vk.sceneDepth is fresh this frame, else 0.0
	float    exposureBias;   // 140..143  auto-exposure bias the tonemap re-multiplies; the
	                         //           additive flipbook divides by it for exposure-invariant
	                         //           intensity (flare-hdr-retune F1 convention). Stride 144 B.
} particleFrame_t;


// Host-side mirror of tonemap.frag's set-2 ExposureBlock UBO (std140). The
// renderer writes this each frame into the per-frame exposure buffer.
// exposure_bias is the pre-tonemap multiplier tonemap.frag reads. With auto-
// exposure ON the exposure-reduce compute OWNS exposure_bias (it writes the
// histogram-derived, temporally-adapted value into the buffer as an SSBO);
// the CPU then writes only the tuning fields + brightness (the manual trim the
// compute multiplies on top). With auto OFF the CPU writes exposure_bias
// = r_brightness directly and the compute is skipped. std140: scalars are tightly
// packed; 9 floats + 1 int = 40 bytes, the block is rounded to a vec4 multiple
// (48 bytes) by the buffer allocation. exposure_bias stays first so its offset
// never moves. Field order/offsets mirror the ExposureBlock in tonemap.frag and
// the storage-block mirror in exposure.comp. The final two fields carry the
// backend-neutral low-luminance visibility curve; 16 scalars close at 64 bytes.
typedef struct {
	float exposure_bias;
	float key;
	float pctLow;
	float pctHigh;
	float rateUp;
	float rateDown;
	float minExp;
	float maxExp;
	int   autoEnabled;
	float brightness;   // r_brightness — read by the reduce compute for the manual-trim multiply
	// Sunray sun-screen params. Per-frame, written in vk_tonemap when the SUNRAYS
	// tonemap variant is active. tonemap.frag reads these only under USE_SUNRAYS.
	// std140: these 4 trailing floats land at offsets 40..55, closing the block at
	// 56 bytes (14 scalars).
	float sunScreenX;   // sun screen UV x (from the tr.sunDirection projection)
	float sunScreenY;   // sun screen UV y (from the tr.sunDirection projection)
	float sunrayIntensity;
	float sunrayDecay;
	float shadowExponent; // chroma-preserving toe exponent; 1.0 is exact identity
	float shadowPivot;    // scene-linear luminance pivot; values above are unchanged
	// (SSAO ssaoZNear/ssaoZFar removed: the legacy per-pixel tonemap SSAO path is
	// fully retired — GTAO is the sole AO path.)
} vk_exposure_block_t;

// Host-side mirror of menubg.frag's set-2 MenuBgBlock UBO (std140). 8 floats = 32 B.
typedef struct {
	float time; float mouseX; float mouseY; float transition;
	float resX; float resY; float pad0; float pad1;
} vk_menubg_block_t;


// Host-side mirror of msdf.{vert,frag}'s set-3 MSDF UBO (std140), one item per
// text draw. Carries what the retired 132-byte VS|FS push constant did: mvp@0,
// outlineWidth@64, glowWidth@68, shadowOffset@72, outlineColor@80, glowColor@96,
// shadowColor@112, bindless_packed_slot@128. Colours are stored already sRGB->
// linear decoded (the decode stays host-side — the same values the push carried).
// std140 rounds the 132-byte payload up to a vec4 multiple => 144 bytes; the
// _pad tail makes sizeof() == 144 so the C struct and the std140 block agree.
typedef struct {
	float    mvp[16];
	float    outlineWidth;
	float    glowWidth;
	float    shadowOffset[2];
	float    outlineColor[4];
	float    glowColor[4];
	float    shadowColor[4];
	uint32_t bindless_packed_slot;
	uint32_t _pad[3];              // pad 132 -> 144 (std140 vec4 multiple)
} vk_msdf_ubo_t;


// Host-side mirror of the SHARED effects per-draw UBO (std140), one item per
// effect draw (ribbon/sprite per RB_Draw* batch; beam per stage-loop iteration).
// Generic-slot union — all three primitives map their fields to matching offsets
// so a single std140 block + descriptor serves all three:
//   mvp @0   (all three)
//   v0  @64  ribbon/beam = eyeWorld{xyz,0} ; sprite = viewLeft{xyz,0}
//   v1  @80  ribbon/beam = frameParams{0,floatTime,0,0} ; sprite = viewUp{xyz,0}
//   v2  @96  beam = stageParams{stageIdx,0,0,0} ; sprite = frameParams ; ribbon = 0
// 112 B — already a 16-byte (vec4) multiple, no trailing pad. Replaces the retired
// ribbon 96 B / beam 112 B / sprite 112 B push constant ranges.
typedef struct {
	float mvp[16];
	float v0[4];
	float v1[4];
	float v2[4];
} vk_effects_ubo_t;


// Vk_Instance contains engine-specific vulkan resources that persist entire renderer lifetime.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
typedef struct {
	VkInstance instance;                   // mirror of the file-static vk_instance, exposed for RAL imported-mode bringup
	VkPhysicalDevice physical_device;
	VkSurfaceFormatKHR base_format;
	VkSurfaceFormatKHR present_format;

	// queue_family_index / queue_family_compute /
	// queue_family_transfer retired. Read via Ral_GetQueueFamily(b, RAL_QUEUE_*).
	// instance_api_version retired (sole
	// reader was vk_ral_adopt_device, since deleted).
	VkDevice device;
	// VkQueue field retired. Per-
	// frame submit is via RAL; shutdown queue-wait is via
	// Ral_WaitQueueIdle. No remaining renderer-side
	// consumer.

	// VkSwapchainKHR alias retired.
	// Read via (VkSwapchainKHR)Ral_GetSwapchainHandle( vk.ral_swapchain ).
	struct ralSwapchain_s *ral_swapchain;  // RAL-owned swapchain wrapper.
	uint32_t swapchain_image_count;
	// Native-image aliases retained only for the still-unmigrated screenshot /
	// destroy guards. RAL owns enumeration, image views and canonical texture
	// wrappers; render/present code consumes vk_tess_s::swapchain_image directly.
	VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
	VkSemaphore swapchain_rendering_finished[MAX_SWAPCHAIN_IMAGES];
	struct ralSemaphore_s *ral_swapchain_rendering_finished[MAX_SWAPCHAIN_IMAGES];  // adopted per-swapchain-image siblings for typed ralPresentInfo_t.waitSemaphores[]
	//uint32_t swapchain_image_index;

	// VkCommandPool command_pool field
	// retired. All renderer command buffers (per-frame ring + staging) now
	// live in RAL's b->cmdPools[GRAPHICS] (functional equivalent — same
	// flags, same queue family).
#ifdef USE_UPLOAD_QUEUE
	VkCommandBuffer staging_command_buffer;        // borrowed alias for residual copy/barrier commands; lifecycle is exact RAL Begin/End/Recycle/Submit
	struct ralCommandBuffer_s *ral_staging_cmd;     // persistent acquire-once-at-init staging cb (RAL-owned via Ral_AcquireCommandBuffer; ownsBuffer=qtrue); submitted via Ral_Submit at vk_flush_staging_buffer
#endif


	ralBindGroupArena_t *ral_descriptor_arena;
	ralBindGroupArenaReceipt_t ral_descriptor_arena_receipt;
	VkDescriptorPool descriptor_pool; // borrowed native mirror for legacy set allocation
	VkDescriptorSetLayout set_layout_sampler;	// combined image sampler
	VkDescriptorSetLayout set_layout_uniform;	// dynamic uniform buffer
	// Per-entity model/MVP matrix storage buffer layout — 1 STORAGE_BUFFER
	// binding, VERTEX stage. Set 3 of vk.pipeline_layout on the r_entitySSBO
	// path (a free set slot — MSDF's set 3 is on its own layout). A clone of
	// vk.shadowMap.set_layout_entmat. Each command slot owns a direct RAL group;
	// vk_tess_t.entMatDesc is only its native mirror and is bound once per frame.
	VkDescriptorSetLayout set_layout_entmat;
	// Engine-resources set 2: shadow array + BRDF/irradiance/radiance IBL +
	// GTAO combined samplers. The screenmap binding moved to the central
	// bindless table. RAL owns the arena group; descriptor is only its mirror.
	VkDescriptorSetLayout set_layout_engine_resources;
	struct {
		VkDescriptorSet descriptor;
		struct ralBindGroup_s *ral_descriptor;
		// Typed sampling view over the shadow-map array. It exists only while binding 0
		// participates and never destroys the renderer-owned native view.
		struct ralTextureView_s *ral_shadow_view;
	} engineResources;

	// Dedup-pool slot of the VkSampler used to read the screenmap
	// via the bindless 2D table. Resolved at vk_update_attachment_descriptors
	// time when the screenmap view + sampler are written into the bindless
	// set. -1 means the slot is not yet known (no screenmap, or the bindless
	// set was just (re-)allocated and the persistent re-write has not run
	// yet). vk_push_bindless_indices reads this to assemble the constant
	// role-6 packed word; if -1, role 6 falls back to the white sentinel.
	int bindless_screenmap_sampler_slot;

	// RAL wrappers over the two descriptor set
	// layouts above, populated via Ral_AdoptBindGroupLayout when the RAL
	// backend is live + r_useRALPipelines is on. NULL otherwise. Consumed by
	// vk_ral_create_pipeline_from_def so RAL pipelines declare matching
	// VkPipelineLayouts. The underlying VkDescriptorSetLayouts are still
	// owned + destroyed by the legacy path; the adopted wrappers are freed
	// by Ral_DestroyBindGroupLayout at vk_shutdown.
	struct ralBindGroupLayout_s *ral_bgl_sampler;
	struct ralBindGroupLayout_s *ral_bgl_uniform;
	// Exact non-dynamic fragment UBO wrapper for exposureSetLayout. Exposure and
	// menu-background sets must not borrow ral_bgl_uniform: that native layout is
	// UNIFORM_BUFFER_DYNAMIC and is the per-draw set-0 authority.
	struct ralBindGroupLayout_s *ral_bgl_exposure;
	// Exact adopted wrapper for set_layout_effects_ubo. Unlike
	// ral_bgl_uniform this carries one dynamic-offset uniform binding and is
	// therefore the portable layout authority for ribbon/rail/beam/sprite.
	struct ralBindGroupLayout_s *ral_bgl_effects_ubo;
	// Exact non-dynamic VS|FS UBO layout used by SMAA set 3. Kept
	// separate from ral_bgl_uniform (dynamic UBO) so the portable layout
	// shape matches set_layout_smaa_rtmetrics exactly.
	struct ralBindGroupLayout_s *ral_bgl_smaa_rtmetrics;
	// Exact set-layout identities consumed by main/MSDF world pipelines.
	struct ralBindGroupLayout_s *ral_bgl_engine_resources;
	struct ralBindGroupLayout_s *ral_bgl_entmat;
	struct ralBindGroupLayout_s *ral_bgl_msdf;

	VkPipelineLayout pipeline_layout;			// default shaders
	VkPipelineLayout pipeline_layout_post_process;	// post-processing
	VkPipelineLayout pipeline_layout_smaa;		// SMAA (push constants + 3 samplers)
	VkPipelineLayout pipeline_layout_msdf;		// MSDF text (112-byte push constant range)
	// Typed RAL owners. Their native mirrors above are obtained from the direct
	// RAL-created layouts and consumed by the legacy Vulkan command surface.
	struct ralPipelineLayout_s *ral_pipeline_layout;
	struct ralPipelineLayout_s *ral_pipeline_layout_post_process;
	struct ralPipelineLayout_s *ral_pipeline_layout_smaa;
	struct ralPipelineLayout_s *ral_pipeline_layout_msdf;

	// ── primitive ribbon infrastructure ──────────────────────────────
	//
	// Self-contained pipeline for the ribbon primitive. Cgame submits
	// world-space control points + a header per ribbon via
	// RE_AddRibbonToScene; the renderer accumulates them into per-frame
	// host-coherent SSBOs and issues one vkCmdDraw per ribbon at the
	// translucent draw site in RB_DrawSurfs.
	//
	// Per-frame slots are indexed by vk.cmd_index; each frame in flight
	// has its own buffer pair so CPU writes don't race GPU reads.
	#define RIBBON_POINTS_PER_FRAME    16384u  // worst case: 8 ribbons × 2048 points
	#define RIBBON_HEADERS_PER_FRAME      256u // hard cap on submissions per frame
	#define RIBBON_POINT_BYTES             48u // sizeof(GPU RibbonPoint), std430
	                                           // (3 * vec4: posW, rgba, normal+pad)
	#define RIBBON_HEADER_BYTES            24u // sizeof(GPU RibbonHeader), std430
	                                           // 4 uints (pointOffset, pointCount, shaderHandle, flags) +
	                                           // vec2 uvScroll = 24 B. Struct alignment is 8 B (vec2);
	                                           // 24 is a multiple of 8 so no trailing pad. The dormant
	                                           // spawnTime + _pad pair was dropped (no shader
	                                           // ever read it; ribbon is transient-only and uvScroll
	                                           // references the absolute frame clock).
	struct {
		// pipeline + layouts + shader-bound state
		VkDescriptorSetLayout	set_layout;          // 2 SSBOs + texture[64] + one CLAMP sampler
		struct ralBindGroupLayout_s *ral_bgl;        // adopted wrapper for set_layout
		VkPipelineLayout		pipeline_layout;     // set0 primitive resources + set1 effects UBO
		struct ralPipelineLayout_s *ral_pipeline_layout;  // typed sibling
		struct ralPipeline_s	*ral_pipeline_alpha;     // dynamic-rendering pipeline (SRC_ALPHA / ONE_MINUS_SRC_ALPHA)
		struct ralPipeline_s	*ral_pipeline_additive;  // dynamic-rendering pipeline (SRC_ALPHA / ONE)
		// per-frame staging (host-coherent, mapped)
		struct ralBuffer_s	*ral_points_buffer[NUM_COMMAND_BUFFERS];
		byte					*points_ptr    [NUM_COMMAND_BUFFERS];
		struct ralBuffer_s	*ral_headers_buffer[NUM_COMMAND_BUFFERS];
		byte					*headers_ptr   [NUM_COMMAND_BUFFERS];
		// Direct generation-bound RAL set-0 owner. descriptor[] is only the
		// transitional Vulkan mirror used by the native pipeline layout.
		VkDescriptorSet			descriptor     [NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];
		// per-frame write cursors (CPU-side, reset in RE_BeginFrame
		// at frontend frame begin, before cgame submissions)
		uint32_t				numPointsThisFrame;
		uint32_t				numHeadersThisFrame;
		qboolean				available;            // false if init failed
	} ribbon;

	// ── primitive beam ───────────────────────────────────────────────
	//
	// Camera-facing two-endpoint quad with optional axial-copy
	// expansion (cross pattern). Engine-managed pool with mixed
	// transient (one-frame) and persistent (lifetime + fade) entries.
	//
	// Pool layout: vk.beam.active[i] tracks slot occupancy. Persistent
	// metadata (spawnTime, duration, fadeIn, fadeOut) lives alongside
	// the descriptor copy; transient slots have duration == 0 and
	// expire at the end of each frame's RB_DrawBeams pass.
	//
	// SSBO write pattern: at draw time, RB_DrawBeams walks the pool,
	// resolves entity-attached endpoints + fade alpha, and writes
	// drawCount consecutive beamHeaderGPU_t entries to the per-frame
	// SSBO. Then issues a single vkCmdDraw with instance count =
	// drawCount and vertex count = 6 * BEAM_AXIAL_MAX. The vertex
	// shader expands each instance to (axialCopies × 6) vertices,
	// emitting degenerate clip-space-behind triangles for unused
	// axial copies (when axialCopies < BEAM_AXIAL_MAX).
	#define BEAM_POOL_MAX       128u   // max concurrent beams (transient + persistent combined). The 64→128 bump gives headroom for 16-player matches and trail beam consumers (jumppad, rocket trails) on the roadmap.
	#define BEAM_AXIAL_MAX        8u   // max axialCopies per beam (vertex-shader fixed loop bound)
	#define BEAM_HEADER_BYTES    96u   // sizeof(GPU BeamHeader), std430:
	                                   //   4 vec4 (start, end, startColor, endColor)  = 64 B
	                                   //   vec2 uvScroll + 2 float widths + float spawnTime
	                                   //                                       + 3 uint  = 32 B
	                                   // total 96 B, naturally 16-aligned for vec4
	                                   // array stride; no manual padding needed.
	struct {
		// pipeline + layouts + shader-bound state
		VkDescriptorSetLayout	set_layout;            // 5 bindings: 3 SSBOs + texture[64] + one REPEAT sampler
		struct ralBindGroupLayout_s *ral_bgl;
		VkPipelineLayout		pipeline_layout;       // set0 primitive resources + set1 effects UBO
		struct ralPipelineLayout_s *ral_pipeline_layout;  // typed sibling
		struct ralPipeline_s	*ral_pipeline;         // dynamic-rendering pipeline (single ONE/ONE additive)
		// Dedicated REPEAT-mode sampler for beam binding 1.
		// Beam UV scrolling produces large out-of-range UVs; REPEAT
		// wraps natively. Ribbon/sprite/particle continue to share
		// vk.particle.sampler (CLAMP_TO_EDGE).
		VkSampler				sampler_repeat;
		struct ralSampler_s	*ral_sampler_repeat;
		// per-frame staging (host-coherent, mapped)
		struct ralBuffer_s	*ral_header_buffer[NUM_COMMAND_BUFFERS];
		byte					*header_ptr    [NUM_COMMAND_BUFFERS];
		VkDescriptorSet			descriptor     [NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];
		// host-side pool state. Slots 0..BEAM_POOL_MAX-1.
		qboolean				active         [BEAM_POOL_MAX];
		float					spawnTime      [BEAM_POOL_MAX];
		float					duration       [BEAM_POOL_MAX];
		float					fadeIn         [BEAM_POOL_MAX];
		float					fadeOut        [BEAM_POOL_MAX];
		beamDesc_t				desc           [BEAM_POOL_MAX];
		uint32_t				drawCount;             // beams written to SSBO this frame (compacted from active slots)
		qboolean				available;             // false if init failed
	} beam;

	// ── parametric rail-ribbon (GPU-resident helix) ─────────────────
	//
	// A persistent pool of helix ribbons whose geometry is REGENERATED
	// every frame by the vertex shader from spawn-fixed spiral params +
	// (currentTime - spawnTime). Unlike the transient ribbon path (which
	// re-uploads a CPU-built point array each frame), cgame submits the
	// spawn params ONCE at fire; the pool tracks the lifetime and the
	// ribbon_spiral vertex shader analytically rebuilds the evolving
	// spiral (expanding radius, unwinding spacing, per-point fade, and a
	// per-frame-varying point count) — the emit-and-forget pattern for
	// animated geometry.
	//
	// Slot management mirrors vk.beam (active/spawnTime/duration/desc);
	// the per-frame SSBO carries the packed spawn params for the live
	// slots. RB_DrawRailRibbons walks the pool, writes the SSBO, and
	// issues one instanced vkCmdDraw with a worst-case vertex count of
	// 6 * RAIL_RIBBON_MAX_SEGMENTS — the shader degenerate-culls segments
	// past the live count.
	#define RAIL_RIBBON_POOL_MAX   8u    // == MAX_RAIL_TRAILS (≤8 concurrent trails)
	// GPU RailRibbonHeader std430 (must match ribbon_spiral.vert):
	//   vec4 startBeamLen   (.xyz start, .w beamLen)
	//   vec4 beamAxisSpawn  (.xyz beamAxis, .w spawnTime)
	//   vec4 colorDuration  (.xyz rgb, .w duration)   + separate alpha in ring pad
	//   vec4 misc           (.x baseAlpha, .yzw pad)
	//   vec4 perpAxis[36]   (.xyz ring dir, .w pad)
	// = 4 headers vec4 (64B) + 36 vec4 (576B) = 640B.
	#define RAIL_RIBBON_HEADER_BYTES  640u
	struct {
		VkDescriptorSetLayout	set_layout;            // 1 SSBO + texture[64] + one CLAMP sampler
		struct ralBindGroupLayout_s *ral_bgl;
		VkPipelineLayout		pipeline_layout;       // set0 (SSBO) + set1 (effects UBO)
		struct ralPipelineLayout_s *ral_pipeline_layout;
		struct ralPipeline_s	*ral_pipeline_alpha;   // dynamic-rendering pipeline (SRC_ALPHA / ONE_MINUS_SRC_ALPHA)
		// per-frame staging (host-coherent, mapped)
		struct ralBuffer_s	*ral_header_buffer[NUM_COMMAND_BUFFERS];
		byte					*header_ptr    [NUM_COMMAND_BUFFERS];
		VkDescriptorSet			descriptor     [NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];
		// host-side pool state. Slots 0..RAIL_RIBBON_POOL_MAX-1.
		qboolean				active         [RAIL_RIBBON_POOL_MAX];
		float					spawnTime      [RAIL_RIBBON_POOL_MAX];
		float					duration       [RAIL_RIBBON_POOL_MAX];
		railRibbonDesc_t		desc           [RAIL_RIBBON_POOL_MAX];
		uint32_t				drawCount;             // slots written to SSBO this frame
		qboolean				available;             // false if init failed
	} railRibbon;

	// ── primitive shader stage SSBO ─────────────────────────
	//
	// Per-stage rendering parameters for multi-stage primitive
	// shaders. Indexed [shaderHandle * PRIMITIVE_STAGE_MAX +
	// stageNumber]. Host-mapped, written at shader registration
	// time; consumers (beam.vert / RB_DrawBeams) read this to
	// drive multi-stage draws.
	//
	// Allocated lazily (first call to vk_init_primitive_shader_images
	// after init) and persisted across vid_restart — buffer survives
	// because it lives outside the descriptor pool. Contents are
	// re-zeroed on every vk_init_primitive_shader_images call so
	// vid_restart sees a clean slate before cgame re-registers
	// shaders.
	struct ralBuffer_s *ral_primitive_stages_buffer;
	void          *primitive_stages_mapped;
	// Indexed by primitive registry SLOT (0..PRIMITIVE_SHADER_IMAGE_MAX-1),
	// NOT by qhandle. The two were decoupled when the qhandle space
	// (MAX_SHADERS=16384) outgrew the registry capacity.
	int            primitive_shader_stage_counts[PRIMITIVE_SHADER_IMAGE_MAX];

	// qhandle → primitive registry slot indirection.
	// Primitive registry has only PRIMITIVE_SHADER_IMAGE_MAX slots (64),
	// but qhandle is monotonic across all RE_RegisterShader calls (world
	// textures, models, UI, primitive). Map registered primitive shaders
	// into the compact slot space.
	//
	// Sentinel PRIMITIVE_SLOT_INVALID (0xFF) means "not registered as
	// primitive shader". Slot 0 is reserved for tr.whiteImage fallback
	// but is a legal allocation outcome — distinguish via the sentinel,
	// not the slot value.
	//
	// Sized via VK_PRIM_QHANDLE_MAX (declared above) which matches
	// MAX_SHADERS in tr_local.h. The mismatch is detected by a
	// _Static_assert in vk.c at first compile.
	//
	// Initialized to all 0xFF in vk_init_primitive_shader_stages
	// (memset 0xFF), repopulated by RE_RegisterPrimitiveShader on each
	// successful registration.
	uint8_t        qhandle_to_prim_slot[VK_PRIM_QHANDLE_MAX];

	// Per-shader stage count SSBO. Tiny (PRIMITIVE_SHADER_IMAGE_MAX
	// uint32_t = 256 B), uploaded once per RB_DrawBeams frame. Bound at
	// beam descriptor set binding 3 so the vertex shader can cull
	// per-stage draws for shaders with fewer stages than the current
	// loop index.
	struct ralBuffer_s *ral_primitive_stage_counts_buffer;
	void          *primitive_stage_counts_mapped;

	// ── primitive sprite (billboard quad) ────────────────────────────
	//
	// Self-contained pipeline for the sprite primitive. Cgame submits
	// world-space billboard sprites via RE_AddSpriteToScene; the
	// renderer accumulates them into a per-frame host-coherent SSBO
	// of SpriteHeaders and issues one direct vkCmdDraw per blend
	// variant (alpha / additive) at the translucent draw site in
	// RB_DrawSurfs, with vertexCount=6 and instanceCount=N.
	//
	// Per-frame slots are indexed by vk.cmd_index; each frame in
	// flight has its own buffer so CPU writes don't race GPU reads.
	#define SPRITES_PER_FRAME    4096u  // hard cap on submissions per frame
	#define SPRITE_HEADER_BYTES    48u  // sizeof(GPU SpriteHeader), std430
	struct {
		// pipeline + layouts + shader-bound state
		VkDescriptorSetLayout	set_layout;          // 1 SSBO: headers
		struct ralBindGroupLayout_s *ral_bgl;
		VkPipelineLayout		pipeline_layout;     // push(mvp+viewLeft+viewUp) + set0(SSBO)
		struct ralPipelineLayout_s *ral_pipeline_layout;  // typed sibling
		struct ralPipeline_s	*ral_pipeline_alpha;     // dynamic-rendering pipeline (SRC_ALPHA / ONE_MINUS_SRC_ALPHA)
		struct ralPipeline_s	*ral_pipeline_additive;  // dynamic-rendering pipeline (SRC_ALPHA / ONE)
		// per-frame staging (host-coherent, mapped)
		struct ralBuffer_s	*ral_headers_buffer[NUM_COMMAND_BUFFERS];
		byte					*headers_ptr   [NUM_COMMAND_BUFFERS];
		VkDescriptorSet			descriptor     [NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];
		// per-frame write cursor (CPU-side, reset in RE_BeginFrame
		// at frontend frame begin, before cgame submissions)
		uint32_t				numThisFrame;
		qboolean				available;            // false if init failed
	} sprite;

	// ── primitive particle (compute-integrated GPU pool) ────────────
	//
	// Self-contained compute + render pipeline for the particle
	// primitive. Particles live in a fixed-size GPU pool (ping-pong'd
	// between two SSBOs); each frame's compute pass reads from one
	// pool, integrates physics + age, and writes to the other. The
	// render pass reads from the just-written pool and emits a
	// billboard quad per live particle.
	//
	// Particle classes (the data-driven recipe registered by cgame
	// via CG_RegisterParticleClass) are mirrored into a separate
	// classes SSBO that both compute and render shaders read.
	//
	// The Particle struct in std430 has hard-fixed size 48 B
	// (vec3+float pairs round to 16 B each, plus 16 B of trailing
	// uints for classHandle + pad). Pool memory: 16384 × 48 B =
	// 768 KB per buffer × 2 buffers = 1.5 MB total.
	//
	// The ParticleClassGPU host-side mirror has hard-fixed size 352 B
	// to match GLSL std430's computed stride (vec4-aligned struct).
	// A static_assert in vk_init_particle catches any drift.
	//
	// Type definitions (particleClassGPU_t, particleFrame_t) and the
	// PARTICLES_PER_POOL / *_BYTES constants live above this struct,
	// outside the Vk_Instance typedef — C disallows nested typedefs
	// inside a struct.
	struct {
		// pipeline state
		VkDescriptorSetLayout	compute_set_layout;  // 8 bindings: UBO + pools/classes + spawn/profile/event/heightgrid
		struct ralBindGroupLayout_s *ral_bgl_compute;
		VkDescriptorSetLayout	render_set_layout;   // 3 bindings: UBO + 2 SSBOs
		struct ralBindGroupLayout_s *ral_bgl_render;
		VkPipelineLayout		compute_pipeline_layout;
		VkPipelineLayout		render_pipeline_layout;
		// typed siblings.
		struct ralPipelineLayout_s *ral_compute_pipeline_layout;
		struct ralPipelineLayout_s *ral_render_pipeline_layout;
		struct ralPipeline_s	*ral_compute_pipeline;
		struct ralPipeline_s	*ral_spawn_pipeline;
		struct ralPipeline_s	*ral_child_spawn_pipeline;
		struct ralPipeline_s	*ral_child_finalize_pipeline;
		struct ralPipeline_s	*ral_render_pipeline_alpha;     // dynamic-rendering pipeline (alpha)
		struct ralPipeline_s	*ral_render_pipeline_additive;  // dynamic-rendering pipeline (additive)

		// Ping-pong particle pool ownership lives in the backend-neutral
		// vk_ral_shadow_storage owner. Indexed by ping-pong bit, NOT by
		// cmd_index — semantically distinct, even though they happen
		// to advance in lockstep at NUM_COMMAND_BUFFERS == 2.

		// Class SSBO ownership also lives in vk_ral_shadow_storage;
		// registration updates one exact element in its CPU shadow.
		uint32_t				numClasses;          // current registry count
		uint32_t				spawnRequestCount;   // frame-local bounded GPU expansion queue
		uint32_t				spawnMaxGroups;      // max ceil(request.count / 64)
		uint32_t				spawnParticleCount;  // frame-local reserved slots; never exceeds pool
		uint32_t				spawnSeed;           // deterministic monotonic request seed
		qboolean				atmosphereGraphSmokePending;
		uint32_t				atmosphereGraphSmokeRequests;
		uint32_t				atmosphereGraphSmokeParticles;
		struct ralBuffer_s		*childTelemetryReadback[NUM_COMMAND_BUFFERS];
		qboolean				childTelemetryReady[NUM_COMMAND_BUFFERS];

		// Texturing: per-class image cache + shared sampler.
		// At RE_RegisterParticleClass time the resolved image_t
		// (shader→stages[0]→bundle[0]→image[0] with three-tier
		// fallback) is cached here so the current-slot RAL bind-group
		// replacement can rebuild the complete immutable texture array
		// after registry changes or an arena reset. The shared
		// sampler is created once at vk_init_particle (linear,
		// clamp-to-edge, no anisotropy) and used for every slot of
		// the array — billboard particles don't need per-class
		// filter/wrap variations.
		struct image_s			*classImages[64];    // MAX_PARTICLE_CLASSES; NULL = unregistered slot, fall back to tr.whiteImage
		// Sprite-frame (flipbook) texture pool. The fragment sampler
		// array (binding 3) is PARTICLE_SAMPLER_COUNT (96) wide: the
		// per-class slots [0..63] above, plus FRAME_POOL_SIZE (32)
		// flipbook-frame slots [64..95] cached here. frameImages[i]
		// maps to sampler-array slot (MAX_PARTICLE_CLASSES + i). A class
		// with frameCount > 1 allocates frameCount slots at registration;
		// frameNextSlot is the bump cursor. Still a fixed bounded array
		// (WebGPU-portable; no bindless, no atlas).
		struct image_s			*frameImages[32];    // FRAME_POOL_SIZE
		uint32_t				frameNextSlot;       // bump allocator cursor [0..FRAME_POOL_SIZE]
		VkSampler				sampler;
		struct ralSampler_s *ral_sampler;          // borrowed from the renderer sampler pool
		uint64_t              textureGeneration;    // registry generation, never zero while initialized
		uint64_t              renderGroupTextureGeneration[NUM_COMMAND_BUFFERS][2];

		// Per-frame uniform ownership is backend-neutral and lives in the
		// shared vk_ral_frame_uniform owner. These descriptor sets borrow only
		// the exact per-slot buffer identities published by its receipt.

		// Descriptor groups. Command-buffer frame ownership and particle pool
		// ping-pong ownership are independent axes. The first index always selects
		// the current frame UBO slot; the second selects the pool. Compute groups
		// bind pool[pool] for read and pool[1-pool] for write. Render groups bind
		// pool[pool] directly. Keeping the axes separate prevents a particle draw
		// from projecting world positions through another frame's camera matrix.
		VkDescriptorSet			compute_descriptor[NUM_COMMAND_BUFFERS][2];
		VkDescriptorSet			render_descriptor [NUM_COMMAND_BUFFERS][2];
		struct ralBindGroup_s *ral_compute_descriptor[NUM_COMMAND_BUFFERS][2];
		struct ralBindGroup_s *ral_render_descriptor[NUM_COMMAND_BUFFERS][2];
		struct ralTextureView_s *ral_collision_heightgrid_view;

		// frame-to-frame state
		uint32_t				pingPongRead;        // 0 or 1, flipped each frame
		float					prevSceneTime;       // backEnd.refdef.floatTime at last
		                                             // RB_RunParticleCompute call

		// Request-time ring reservation. CPU advances once per bounded emitter;
		// the specialized GPU spawn pass expands individual particle slots.
		uint32_t				nextSlot;

		qboolean				available;           // false if init failed
	} particle;

	// GPU decal ring + projector render pass. Mirrors the particle render
	// pipeline: the front-end writes decalGPU_t into the host-coherent pool SSBO
	// at emit (round-robin), and RB_DrawDecals draws the whole pool as surface-
	// aligned instanced quads in the main pass (6 verts/decal, depth-tested,
	// alpha-blended, depth-write off). Dead/empty slots emit degenerate
	// triangles in the vertex shader. RAL-native (pool registered via
	// vk_ral_register_buffer, pipeline via vk_ral_create_pipeline_from_gpinfo);
	// WebGPU-portable (host-coherent SSBO + UBO + bounded sampler array, no
	// push-constants / input-attachments / bindless).
	struct {
		// Pool ownership and element-dirty CPU shadow live in the
		// backend-neutral vk_ral_shadow_storage owner.
		uint32_t				nextSlot;            // round-robin emit cursor; wrap overwrites oldest

		// render pipeline state
		VkDescriptorSetLayout	render_set_layout;   // 5 bindings: UBO + pool + textures + depth + climate tiles
		struct ralBindGroupLayout_s *ral_bgl_render;
		VkPipelineLayout		render_pipeline_layout;
		struct ralPipelineLayout_s *ral_render_pipeline_layout;
		// One projector pipeline per blend mode: [0]=alpha, [1]=additive,
		// [2]=colour. Each carries the matching blend state plus a vertex-stage
		// DECAL_BLEND_MODE spec-constant so it degenerate-culls decals whose
		// blendMode does not match (mirrors the particle alpha/additive split).
		struct ralPipeline_s	*ral_render_pipeline[3];

		// Per-frame uniform ownership is backend-neutral and lives in the
		// shared vk_ral_frame_uniform owner. Descriptor sets borrow its exact
		// per-slot receipt identities.

		// per-cmd-index render descriptor sets
		VkDescriptorSet			render_descriptor[NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_render_descriptor[NUM_COMMAND_BUFFERS];

		// Texturing: qhandle→image registry + shared sampler. At emit
		// (RE_AddDecalToScene) the decal shader's resolved image_t
		// (shader→stages[0]→bundle[0]→image[0], three-tier fallback) is
		// find-or-added here; the registry SLOT is stored in
		// decalGPU_t.textureIndex so the fragment shader samples
		// decalTextures[textureIndex]. The shared sampler is created once
		// at vk_init_decal (linear, clamp-to-edge). vk_init_decal_textures
		// re-walks this registry to (re)populate the sampler-array binding
		// after a descriptor-pool reset (vid_restart / FBO toggle).
		struct image_s			*images[64];         // MAX_DECAL_TEXTURES; NULL = unused slot, falls back to tr.whiteImage
		uint32_t				numImages;           // current registry count
		VkSampler				sampler;
		struct ralSampler_s *ral_sampler;           // borrowed from renderer sampler pool
		uint64_t				textureGeneration;     // registry generation, never zero while initialized
		uint64_t				renderGroupTextureGeneration[NUM_COMMAND_BUFFERS];

		// Sorted backend-neutral surface-climate table, one retained storage
		// buffer per command slot. The fragment shader performs at most eight
		// comparisons; no material/decal pixel scans the 256-entry source list.
		struct ralBuffer_s		*surfaceClimateBuffer[NUM_COMMAND_BUFFERS];
		ralAtmosphereSurfaceTile_t surfaceClimateTable[RAL_ATMOSPHERE_MAX_SURFACE_TILES];
		uint64_t				surfaceClimateGeneration;
		uint64_t				surfaceClimatePublished[NUM_COMMAND_BUFFERS];
		uint32_t				surfaceClimateCount;

		qboolean				available;           // false if init failed
	} decal;

	// ── GPU-resident atmospheric precipitation ────────────────────────
	// Dedicated compute + draw pipeline, separate from the particle path.
	// Mirrors vk.particle: RAL-owned device-local ping-pong pool SSBOs, a per-frame
	// UBO ring, a compute pipeline that self-spawns / integrates / collides
	// against a heightgrid texture, and an instanced draw. The cgame emits
	// one weather descriptor on weather change (RE_SetAtmosphere) plus the
	// collision heightgrid (RE_SetAtmosphereHeightgrid); the pool runs every
	// frame. Gated by r_atmosphericGPU (0 = skip). Pool, frame, texture-view,
	// bind-group and command/pipeline authority are retained RAL resources.
	struct {
		// pipeline state
		VkDescriptorSetLayout	compute_set_layout;  // 4 bindings: UBO + read SSBO + write SSBO + heightgrid sampler
		VkDescriptorSetLayout	render_set_layout;   // 3 bindings: UBO + pool SSBO + scene depth
		struct ralBindGroupLayout_s *ral_bgl_compute;
		struct ralBindGroupLayout_s *ral_bgl_render;
		VkPipelineLayout		compute_pipeline_layout;
		VkPipelineLayout		render_pipeline_layout;
		struct ralPipelineLayout_s *ral_compute_pipeline_layout;
		struct ralPipelineLayout_s *ral_render_pipeline_layout;
		struct ralPipeline_s	*ral_compute_pipeline;
		struct ralPipeline_s	*ral_render_pipeline;   // dynamic-rendering pipeline (alpha-blend)

		// Ping-pong pool ownership lives in vk_atmospheric_pool.c. The direct
		// RAL bind groups below borrow the exact per-generation buffer receipts.

		// Per-frame uniform ownership lives in vk_atmospheric_frame.c. The
		// renderer keeps only generation-bound bind-group children here.

		// descriptor sets. compute_descriptor[i] reads pool[i], writes
		// pool[1-i]; render_descriptor[i] reads pool[1-i] (the post-compute
		// output). Frame N selects index pingPongRead.
		VkDescriptorSet			compute_descriptor[NUM_COMMAND_BUFFERS];
		VkDescriptorSet			render_descriptor [NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s	*ral_compute_descriptor[NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s	*ral_render_descriptor [NUM_COMMAND_BUFFERS];

		// Heightgrid texture/sampler ownership lives in the retained RAL owner.
		// This view is a bind-group child and is replaced atomically with the
		// four current-arena atmospheric groups after every descriptor reset.
		struct ralTextureView_s *ral_heightgrid_view;

		// frame-to-frame state
		uint32_t				pingPongRead;        // 0 or 1, flipped each frame
		float					prevSceneTime;       // floatTime at last compute

		// live weather descriptor (set by RE_SetAtmosphere). type == 0 →
		// pool inert (compute + draw skipped).
		int						type;                // 0 = none, 1 = rain, 2 = snow
		float					bounds[6];           // world spawn volume (xyz min, xyz max)
		float					distance;            // eye-relative cull radius
		vec2_t					worldMins;           // heightgrid xy bounds
		vec2_t					worldMaxs;
		int						gridSize;            // heightgrid edge (256)
		uint32_t				flags;
		uint32_t				qualityTier;
		uint32_t				seed;
		vec3_t					wind;
		float					gustStrength;
		float					precipitation[5];
		float					indoorExposure;
		float					timelineSeconds;       // app-authored deterministic effect clock
		atmosphereFrameState_t state;                 // complete normalized climate authority
		float					surfaceTargets[4];     // derived wetness/frost/snow/melt targets
		ralAtmosphereRuntimeState_t runtimeState;      // backend-neutral lifecycle authority
		ralAtmosphereRuntimeReceipt_t runtimeReceipt;  // latest exact lifecycle telemetry
		uint64_t				runtimeFrameGeneration;

		qboolean				available;           // false if init failed
	} atm;

#if FEAT_IQM
	// ── IQM GPU skinning infrastructure ──────────────────────────────
	//
	// Self-contained pipeline for skeletal IQM models.
	// Per-model VBOs (vertex+index) are stored in iqmData_t.
	// Per-frame bone matrices are uploaded to a host-visible UBO
	// and bound via a dedicated descriptor set + pipeline layout.
	//
	struct {
		// set 0 is a PER-DRAW ring (UNIFORM_BUFFER_DYNAMIC) of bones+mvp items (one per
		// IQM surface draw, written + bound with a dynamic offset). A single per-frame
		// slot would clobber when >1 GPU-skinned IQM entity draws in one frame (both
		// bones AND mvp would show the last entity); the ring gives each draw its own
		// item. Item = PAD(IQM_UBO_TOTAL_SIZE, vk.uniform_alignment); offset[] resets
		// per frame. Mirrors the effects ring (vk.effectsUbo).
		VkDescriptorSetLayout	set_layout_bones;	// bone+mvp UBO (set 0), UNIFORM_BUFFER_DYNAMIC
		struct ralBindGroupLayout_s *ral_bgl_bones;
		VkPipelineLayout		pipeline_layout;	// set0(bones+mvp ring) + set1(texture) — 0 push
		struct ralPipelineLayout_s *ral_pipeline_layout;  // typed sibling
		struct ralPipeline_s	*ral_pipeline;		// dynamic-rendering pipeline (IQM skinning)
		struct ralBuffer_s	*ral_bone_buffer[NUM_COMMAND_BUFFERS]; // per-frame bones+mvp ring
		byte					*bone_ptr[NUM_COMMAND_BUFFERS];   // mapped pointers
		VkDescriptorSet			bone_descriptor[NUM_COMMAND_BUFFERS]; // UNIFORM_BUFFER_DYNAMIC, base offset 0
		struct ralBindGroup_s	*ral_bone_descriptor[NUM_COMMAND_BUFFERS];
		uint64_t				ring_size;			// per-frame ring buffer size in bytes
		uint32_t				offset[NUM_COMMAND_BUFFERS]; // running per-draw alloc cursor; reset at frame begin
		qboolean				available;			// false if init failed
	} iqmGpu;
#endif

	VkDescriptorSet color_descriptor;
	// Arena-owned RAL bind group for the scene sampling view. The raw set above
	// is only its transitional Vulkan mirror.
	struct ralBindGroup_s *ral_color_descriptor;

	// Per-frame scene-exposure channel. tonemap.frag reads its pre-tonemap
	// exposure multiplier (and carries auto-exposure tuning fields, see
	// vk_exposure_block_t) from this host-coherent uniform buffer instead of a
	// baked specialization constant, so the values can change every frame
	// without rebuilding the post-process pipelines. The CPU writes the block
	// into exposure.ptr[cmd_index] before the tonemap draw; a future GPU
	// producer can write the same buffer in place. exposureSetLayout is set 2
	// of every post-process pipeline layout (set 0 = scene sampler, set 1 = the
	// depth sampler in the SSAO/SUNRAYS variants), so one buffer reaches all
	// variants. One buffer + descriptor per in-flight frame. Lifecycle follows
	// color_image (created/destroyed in vk_init_descriptors under fboActive).
	VkDescriptorSetLayout exposureSetLayout;
	struct {
		void           *ptr[NUM_COMMAND_BUFFERS];
		VkDescriptorSet descriptor[NUM_COMMAND_BUFFERS];
		// RAL owns the persistent mapped buffer; descriptor[] is only the native
		// mirror of the current generation's arena-owned bind group.
		struct ralBuffer_s    *ral_buffer[NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];
	} exposure;

	// Per-frame WiredUI SCENE backdrop UBO (menubg.frag set 2). Same host-coherent
	// ring shape as exposure; written each frame by RE_DrawMenuBackdrop. Reuses
	// exposureSetLayout (binding 0 UNIFORM_BUFFER FRAGMENT — layout-compatible).
	struct {
		void            *ptr[NUM_COMMAND_BUFFERS];
		VkDescriptorSet  descriptor[NUM_COMMAND_BUFFERS];
		struct ralBuffer_s    *ral_buffer[NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];
	} menubg;

	// MSDF text per-draw UBO ring. One host-coherent buffer per in-flight frame,
	// each text draw sub-allocating a vk_msdf_ubo_t item at a running offset (PAD'd
	// to vk.uniform_alignment) and binding it at WIRED_MSDF_SET via a per-draw
	// dynamic offset. Separate from the set-0 vkUniform_t ring; replaces the MSDF
	// 132-byte push constant. offset[] resets per frame alongside
	// vk.cmd->uniform_read_offset. set_layout_msdf is a single UNIFORM_BUFFER_DYNAMIC
	// binding (VS|FS), set 3 of vk.pipeline_layout_msdf. The descriptor sets are
	// retained as an exact RAL bind group; the dynamic offset is validated against
	// the adopted buffer before the backend emits the set-3 bind.
	VkDescriptorSetLayout set_layout_msdf;
	struct {
		void           *ptr[NUM_COMMAND_BUFFERS];
		VkDescriptorSet descriptor[NUM_COMMAND_BUFFERS]; // UNIFORM_BUFFER_DYNAMIC, base offset 0
		struct ralBuffer_s *ral_buffer[NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];
		VkDeviceSize    size;                            // per-frame buffer size in bytes
		uint32_t        offset[NUM_COMMAND_BUFFERS];     // running per-draw alloc cursor; reset at frame begin
	} msdf;

	// Shared effects per-draw UBO ring. ONE host-coherent buffer per in-flight
	// frame, sub-allocated per draw at a running offset (PAD'd to
	// vk.uniform_alignment) and bound at set index 1 of each effect pipeline layout
	// (ribbon/beam/sprite) via a per-draw dynamic offset. Modeled on the vk.msdf
	// ring above but bound through Ral_CmdBindBindGroupDynamic at the draw site
	// (setIndex=1) — the effects never use the descriptor_set.current[] fold, so no
	// WIRED_*_SET slot is consumed. One dynamic UBO descriptor per frame points at
	// the buffer (base 0, range = one item). offset[] resets per frame alongside
	// vk.msdf.offset[]. set_layout_effects_ubo is a single UNIFORM_BUFFER_DYNAMIC
	// binding (VS|FS), set 1 of each effect layout (set 0 stays the effect's own set).
	VkDescriptorSetLayout set_layout_effects_ubo;
	struct {
		void           *ptr[NUM_COMMAND_BUFFERS];
		VkDescriptorSet descriptor[NUM_COMMAND_BUFFERS]; // UNIFORM_BUFFER_DYNAMIC, base offset 0
		// Persistent RAL-owned buffers supply exact base/range and mapping authority.
		struct ralBuffer_s    *ral_buffer[NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];
		VkDeviceSize    size;                            // per-frame buffer size in bytes
		uint32_t        offset[NUM_COMMAND_BUFFERS];     // running per-draw alloc cursor; reset at frame begin
	} effectsUbo;

	// SMAA rtMetrics per-frame UBO. One host-coherent slot per in-flight
	// frame (vec4 {1/w,1/h,w,h}, written once per frame), bound at set 3 of
	// vk.pipeline_layout_smaa for all 3 passes. Replaces the retired rtMetrics push.
	// Non-dynamic UNIFORM_BUFFER (no per-draw offset) — exposure-style, not a ring.
	VkDescriptorSetLayout set_layout_smaa_rtmetrics;
	struct {
		void           *ptr[NUM_COMMAND_BUFFERS];
		VkDescriptorSet descriptor[NUM_COMMAND_BUFFERS];
		struct ralBuffer_s *ral_buffer[NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_descriptor[NUM_COMMAND_BUFFERS];
	} smaaRt;

	// LDR-linear scene image, written by tonemap pass,
	// sampled by the gamma/capture passes downstream. Format is
	// R8G8B8A8_UNORM (sRGB encoding lives in gamma.frag; a later change may
	// switch to an sRGB swapchain and drop manual encoding).
	VkDescriptorSet tonemapped_descriptor;
	// Portable sampling view + arena-owned set 0. The raw descriptor is only
	// the transitional Vulkan mirror used by the native pipeline layout.
	struct ralTextureView_s *ral_tonemapped_view;
	struct ralBindGroup_s *ral_tonemapped_descriptor;
	struct ralTexture_s *ral_tonemapped_image;

	// Native-presentation UI composite. The scene remains in
	// ral_tonemapped_image at the independently selected render extent; it is
	// upscaled once into this image before HUD/menu/console draws. Gamma and
	// capture sample this image so 2D never inherits the scene render scale.
	VkDescriptorSet ui_descriptor;
	struct ralTextureView_s *ral_ui_view;
	struct ralBindGroup_s *ral_ui_descriptor;
	struct ralTexture_s *ral_ui_image;
	/* Presentation-resolution depth for painter-ordered worldless UI
	 * subviews. Scene depth follows the independently scaled 3D extent and
	 * cannot legally back the native-resolution UI pass. */
	struct ralTexture_s *ral_ui_depth_image;
	qboolean ral_ui_image_initialized;

	struct ralTexture_s *ral_color_image;
	struct ralTextureView_s *ral_color_view;    // portable sampling view

	// HDR auto-exposure: a RAL compute pass reads color_image (the HDR scene)
	// and builds a 256-bin log2-luminance histogram into a device-local SSBO.
	// This is the renderer's first RAL compute consumption — created once at
	// vk_initialize (gated on the backend coming up), dispatched in the mid-
	// tonemap seam under r_hdrAutoExposure. The histogram is the input the
	// future reduce/adaption step turns into an exposure value; for now it is
	// produced and (optionally) read back for verification, not consumed by
	// tonemap.frag. All handles are RAL-owned except color_view, which is a
	// sampling view minted over the engine-owned color_image.
	struct ralBuffer_s         *ral_histogram_buffer;     // 256 * uint32, device-local STORAGE
	struct ralBindGroupLayout_s *ral_histogram_bgl;       // set 0: sampled color + sampler + SSBO
	struct ralBindGroup_s      *ral_histogram_descriptor; // the bound compute set
	struct ralPipeline_s       *ral_histogram_pipeline;   // the compute pipeline
	struct ralTextureView_s    *ral_histogram_color_view; // sampling view over color_image
	struct ralSampler_s        *ral_histogram_sampler;    // nearest/clamp (texelFetch ignores it)
	// Debug-only readback (r_hdrHistogramDebug): one MAP_READ-capable mirror per
	// command slot. A slot is mapped only after its fence has completed, then
	// unmapped before that slot is reused as COPY_DESTINATION.
	struct ralBuffer_s         *ral_histogram_readback[NUM_COMMAND_BUFFERS];
	qboolean                    histogramReadbackReady[NUM_COMMAND_BUFFERS];
	// Debug-only readback of the GPU-written exposure_bias. The exposure UBO is
	// CPU-shadow-backed, so its host shadow is not a valid GPU readback source.
	struct ralBuffer_s         *ral_exposure_readback[NUM_COMMAND_BUFFERS];
	qboolean                    exposureReadbackReady[NUM_COMMAND_BUFFERS];

	// Ground-truth ambient occlusion (GTAO). Two RAL compute passes dispatched in
	// the no-render-pass seam in vk_tonemap (the histogram pattern): the main pass
	// reconstructs view-space position + normal from the depth copy and runs the
	// horizon-search visibility integral into ral_gtao_ao_image; the denoise pass
	// depth-aware-blurs it into ral_gtao_denoised_image, which the tonemap pass
	// samples to modulate scene radiance. Both single-channel R8. Depth-dependent,
	// so the whole set is created only when the depth copy exists (depthFade
	// active = r_ssao on); a NULL pipeline disables the dispatch, like histogram.
	struct ralTexture_s         *ral_gtao_ao_image;        // R8 raw AO (storage)
	struct ralTextureView_s     *ral_gtao_ao_view;         // storage+sampled view
	struct ralTexture_s         *ral_gtao_denoised_image;  // R8 denoised AO (storage)
	struct ralTextureView_s     *ral_gtao_denoised_view;   // storage+sampled view
	struct ralTextureView_s     *ral_gtao_depth_view;      // sampling view over the depth copy
	struct ralSampler_s         *ral_gtao_sampler;         // linear/clamp depth+AO sampler
	struct ralBindGroupLayout_s *ral_gtao_main_bgl;        // set 0: depth tex+sampler + AO storage
	struct ralBindGroup_s       *ral_gtao_main_descriptor;
	struct ralPipeline_s        *ral_gtao_main_pipeline;
	struct ralBindGroupLayout_s *ral_gtao_denoise_bgl;     // set 0: AO in + depth tex+sampler + AO out
	struct ralBindGroup_s       *ral_gtao_denoise_descriptor;
	struct ralPipeline_s        *ral_gtao_denoise_pipeline;
	struct ralBindGroup_s       *ral_gtao_composite_descriptor; // sampled denoised-AO set for tonemap
	qboolean                     gtaoLayoutGeneral;        // AO images already moved UNDEFINED→GENERAL once

	// Async-compute: cross-frame async GTAO. When r_asyncCompute is set
	// AND the device exposes a dedicated compute queue family (caps.asyncCompute),
	// GTAO runs on RAL_QUEUE_COMPUTE at frame start, reading the PREVIOUS frame's
	// depth snapshot (1-frame-lag, Eser-ratified), overlapping this frame's graphics.
	// A per-frame binary semaphore the compute submit signals + the graphics submit
	// waits ensures the denoised AO is visible before tonemap samples it. On the
	// no-dedicated-queue / r_asyncCompute-off path, gtaoAsyncActive stays false and
	// GTAO runs serialized on the graphics queue at the snapshot point (byte-identical).
	struct ralSemaphore_s       *ral_gtao_async_done[ NUM_COMMAND_BUFFERS ]; // compute→graphics handoff, per frame slot
	struct ralCommandBuffer_s   *ral_gtao_async_cmd[ NUM_COMMAND_BUFFERS ];  // compute cmd buffer per frame slot
	qboolean                     gtaoAsyncActive;          // this frame: async GTAO submitted, graphics must wait the sem
	qboolean                     gtaoSnapshotValid;        // a depth snapshot from a PRIOR frame exists (cross-frame source)

	// HDR auto-exposure reduce: a second compute pass reads the histogram, reduces
	// it to a target exposure (percentile-clipped weighted average → middle-grey
	// key), smooths it temporally into a persistent accumulator, and writes
	// exposure_bias into the exposure UBO (bound here as STORAGE). Dispatched right
	// after the histogram pass, before tonemap. The accumulator is a SINGLE
	// persistent device-local float (NOT a per-frame ring) — the temporal smoothing
	// reads last frame's value; seeded to 1.0 once at init, never zeroed per-frame.
	// The reduce bind-group is a per-frame ring matching the exposure UBO ring so it
	// targets the same UBO slot the fragment tonemap binds (cmd_index).
	struct ralBuffer_s          *ral_exposure_accumulator;            // float[1], persistent
	struct ralBindGroupLayout_s *ral_exposure_reduce_bgl;             // set 0: hist + accum + exposure-ubo (all STORAGE)
	struct ralBindGroup_s       *ral_exposure_reduce_descriptor[NUM_COMMAND_BUFFERS];
	struct ralPipeline_s        *ral_exposure_reduce_pipeline;

	// GPU world-surface cull (cull.comp). A per-frame compute
	// pass reproduces the frontend per-surface visibility decision (PVS-reached
	// flag ∩ per-surface frustum ∩ ¬backface) over the FULL static world surface
	// set and compacts the survivors into a visible-id list. Verification-only:
	// the CPU draw is unchanged; the GPU set is read back (r_gpuCullDebug,
	// _DEBUG) and compared to the independently-produced CPU reference. Dispatched
	// in the no-render-pass seam (before the main pass opens), alongside the
	// histogram/IBL pre-passes. The AABB SSBO is built ONCE at map load (static
	// geometry); the per-frame inputs (frustum planes + viewOrigin + the PVS-reached
	// bitset) are uploaded each frame. cullSurfaceCount == 0 disables the dispatch.
	int                          cullSurfaceCount;        // # static world surfaces (== tr.world->numsurfaces at build)
	struct ralBuffer_s          *ral_cull_aabb;           // per-surface {mins,maxs,plane,meta}, device-local, map-load
	void                         *cullAabbCpu;             // CPU authority for host verification; uploaded through queue semantics
	uint32_t                     *cullReachedCpu;          // reusable CPU bitset; uploaded through queue semantics
	struct ralBuffer_s          *ral_cull_reached[NUM_COMMAND_BUFFERS]; // per-frame PVS-reached bitset (device-local queue upload)
	struct ralBuffer_s          *ral_cull_visible;        // per-frame output: visible flags + compacted ids + count
	struct ralBindGroupLayout_s *ral_cull_bgl;            // set 0: aabb + reached + visible (all STORAGE, COMPUTE)
	struct ralBindGroup_s       *ral_cull_descriptor[NUM_COMMAND_BUFFERS]; // bound set per frame (reached ring)
	struct ralPipeline_s        *ral_cull_pipeline;       // the compute pipeline

	// Forward+ tiled lighting (forwardplus_tile.comp). A cull.comp sibling: divides
	// the screen into TILE_SIZE-px tiles and, per tile, culls the dynamic dlight list
	// (each light = a world-space sphere) against the tile's view frustum, writing a
	// per-tile light-index list. The lit fragment reads its tile's list and iterates
	// only those lights (same per-light intens + BRDF math as PMLIGHT; only the
	// gather changes). r_forwardPlus gates the whole path; default-0 = the existing
	// PMLIGHT per-light-pass, byte-identical. Dispatched in the no-render-pass seam
	// alongside cull. Buffers are per-frame (the light list + tile depth bounds change
	// every frame); sized to the tile grid for the current render extent.
	struct ralBuffer_s          *ral_fp_lights[NUM_COMMAND_BUFFERS];     // per-frame dlight list (host-coherent upload)
	struct ralBuffer_s          *ral_fp_tiledepth[NUM_COMMAND_BUFFERS];  // per-frame per-tile depth min/max (host or prepass-reduced)
	struct ralBuffer_s          *ral_fp_tilelights[NUM_COMMAND_BUFFERS]; // per-frame output: per-tile light-index list (device-local)
	struct ralBindGroupLayout_s *ral_fp_bgl;             // set 0: lights + tiledepth + tilelights (STORAGE, COMPUTE)
	struct ralBindGroup_s       *ral_fp_descriptor[NUM_COMMAND_BUFFERS]; // bound set per frame
	struct ralPipeline_s        *ral_fp_pipeline;        // the tile-classification compute pipeline
	void                        *fpLightsPtr[NUM_COMMAND_BUFFERS];       // persistent mapping of ral_fp_lights (host writes dlights)
	void                        *fpTileDepthPtr[NUM_COMMAND_BUFFERS];    // persistent mapping of ral_fp_tiledepth
	int                          fpTilesX, fpTilesY;     // current tile grid dims (render extent / TILE_SIZE)
	int                          fpTileCapacity;         // tiles the buffers were sized for (grow on resize)
	qboolean                     fpActive;               // r_forwardPlus && pipeline built && a light list this frame
	int                          fpLightCount;           // dlights uploaded this frame

	// Forward+ dlight capture — the seam-ordering workaround. vk_forwardplus_dispatch
	// runs in the vk_begin_frame seam, BEFORE RB_DrawSurfs assigns backEnd.viewParms,
	// so it reads this frame's dlights one frame too early (num_dlights==0 there —
	// backEnd.viewParms.num_dlights is zeroed at the end of the prior frame's PMLIGHT
	// pass). Mirror the shadow producer (vk_dlight_shadow_capture_light): snapshot the
	// dlight list at RB_DrawSurfs (viewParms live) into fpCapture, and the seam consumes
	// the PREVIOUS frame's snapshot — the same 1-frame lag the CSM cascade fit accepts.
	// The seam's backEnd.viewParms MATRICES are still that same prior frame's (only
	// num_dlights was zeroed, not the view/proj), so the captured lights and the tile
	// unprojection agree. Only the fields the fp light-upload loop reads are stored.
	struct {
		int   num;
		struct {
			float origin[3];
			float radius;
			float color[3];
			float origin2[3];
			int   linear;
		} lights[ 256 /* FP_MAX_LIGHTS */ ];
	} fpCapture;

	// Forward+ depth source for the per-tile depth-cull tightening (depthValid=1).
	// The live depth attachment (vk.depth_image) is attachment-only (no SAMPLED
	// usage) so it cannot be sampled in the reduce compute. Mirror the depthFade
	// idiom: a dedicated SAMPLED depth copy (TRANSFER_DST|SAMPLED), filled at frame
	// END from the just-written depth, then sampled NEXT frame in the no-render-pass
	// seam by the reduce (a 1-frame lag — conservative: a stale-by-one-frame bound
	// only loosens which lights a tile considers, never drops a valid light).
	// r_forwardPlus-gated (no allocation when off → byte-identical OFF path).
	struct ralSampler_s         *ral_fpDepthSampler;    // nearest, clamp (reduce reads exact texels)
	struct ralTexture_s         *ral_fpDepthImage;      // direct copy dst + transition owner
	struct ralTextureView_s     *ral_fpDepthView;       // RAL sampling view for the reduce compute
	struct ralBindGroupLayout_s *ral_fpReduceBgl;       // set 0: sceneDepth(0)+sampler(1)+tileDepth(2)
	struct ralBindGroup_s       *ral_fpReduceDescriptor[NUM_COMMAND_BUFFERS];
	struct ralPipeline_s        *ral_fpReducePipeline;  // the per-tile depth min/max reduce compute
	qboolean                     fpDepthRetained;        // a depth copy from last frame is valid (reduce may read it)

	// Forward+ lit CONSUMER — the world-space lit pass that reads the per-tile light
	// list + dlight params (the producer's ral_fp_tilelights / ral_fp_lights) and
	// sums each tile's lights per fragment (same per-light intens + BRDF as PMLIGHT;
	// only the gather changes). A DEDICATED pipeline layout + descriptor set (set 2 =
	// {tileLights@4, dlightParams@5, tileParams@6}) so the shared main layout + the
	// default pipelines are untouched (zero blast radius; byte-identical when off).
	// Bound INSTEAD of the per-light PMLIGHT passes when r_forwardPlus 1.
	VkPipelineLayout             fpLitLayout;            // set0 per-draw UBO + set1 bindless + set2 fp-SSBOs
	struct ralPipelineLayout_s  *ral_fpLitLayout;       // typed sibling
	VkDescriptorSetLayout        fpLitSetLayout;         // set 2 — tileLights(4)+dlightParams(5)+tileParams(6)
	struct ralBindGroupLayout_s *ral_fpLitSetLayout;    // direct RAL-owned set-2 layout authority
	struct ralPipeline_s        *ral_fpLitPipeline;     // the additive world-space lit pipeline
	struct ralBuffer_s          *ral_fpTileParams[NUM_COMMAND_BUFFERS]; // per-frame {screenW,H,tilesX,tilesY}
	void                        *fpTileParamsPtr[NUM_COMMAND_BUFFERS];
	VkDescriptorSet              fpLitSet[NUM_COMMAND_BUFFERS]; // set 2 bound per frame (the 3 fp-SSBOs)
	struct ralBindGroup_s       *ral_fpLitSet[NUM_COMMAND_BUFFERS]; // direct current-arena owners
	struct ralTextureView_s     *ral_fpLitShadowView[NUM_COMMAND_BUFFERS]; // per-group borrowed native shadow-view wrappers

	// r_unbakeStaticLights world-cluster grid (set-2 bindings 10/11). SINGLE instance
	// (view-independent, constant per map — NOT a per-frame ring). The SSBO holds the
	// flat per-cell static-light-index lists; the params UBO holds grid origin/cellSize/
	// dims. When the cvar is off (or no static lights), a 1-cell empty fallback is bound
	// so the fragment's cluster loop reads count=0 → zero iterations → byte-identical.
	struct ralBuffer_s          *ral_fp_clustergrid;    // flat cell lists (host-coherent, built once at load)
	void                        *fpClusterGridPtr;      // persistent mapping (CPU memcpy of world->clusterFlat)
	struct ralBuffer_s          *ral_fp_clusterfallback;// 1-cell empty (count=0) for the OFF path
	struct ralBuffer_s          *ral_fpClusterParams;   // grid-params UBO {origin.xyz, cellSize; dims.xyz}
	void                        *fpClusterParamsPtr;

	// Lens-glow occlusion oracle — the unified visibility backbone (halo is the
	// first consumer). A per-frame host-coherent SSBO holds one record per registered
	// lens source (screen position + reversed-Z compare depth + a visibility output
	// slot); a compute pass dispatched in the GTAO seam (depth copy SHADER_READ_ONLY,
	// render pass closed) samples vk.sceneDepth with an N-tap disc per source and writes
	// visibility 0..1 back into the record. The registry mirrors the fp-lights SSBO
	// lifecycle (create/map/bind/destroy/reset); the oracle compute mirrors the gtao
	// bind-group idiom (its OWN bgl sampling vk.sceneDepth.ral_image — never touches the
	// graphics set-1 the SSAO/sunray tonemap variants depend on). Multi-backend: a
	// depth-sample + SSBO write-back, no readback, no queries, no VS push.
// Lens registry record geometry (shared host/shader; must match lens_occlusion.comp).
// Slot map (disjoint ranges, never alias): [0..255] flares (slot = flare index),
// [256] the sun, [257..576] cgame-registered lens sources (slot = base + id%count).
// The cgame strip is sub-banded BY THE CALLER so the three source kinds never alias:
// cgame passes a pre-banded id (map-flares [0,256), missiles [256,288), powerups
// [288,320)); the renderer stays kind-agnostic — it just does base + id%count.
#define LENS_MAX_SOURCES   256               // flare slots 0..255 (== MAX_FLARES, tr_local.h)
#define LENS_SLOT_SUN      LENS_MAX_SOURCES  // 256: dedicated reserved slot for the sun source
#define LENS_SLOT_CGSOURCES (LENS_SLOT_SUN + 1) // 257: base of the cgame lens-source strip
// LENS_MAX_CGSOURCES (the cgame strip size = sum of the kind-bands) + the band
// constants are defined in the shared ABI header (primitives.h) so cgame and the
// renderer agree on the slot map. = 352 (map[0,256)+missile[256,288)+powerup[288,320)
// +halo[320,352)).
#define LENS_TOTAL_SOURCES (LENS_SLOT_CGSOURCES + LENS_MAX_CGSOURCES) // flares + sun + cgame = 609
#define LENS_SOURCE_VEC4S  2                 // 2 vec4 per record (32 B): rec0 pos/depth, rec1 vis
	// WebGPU-valid split: CPU owns a shadow, each command slot owns a device-local
	// storage buffer plus COPY_DEST/MAP_READ readback. A slot is reused only after
	// its frame fence, so no mapped resource is simultaneously GPU-visible.
	struct ralBuffer_s          *ral_lens_sources[NUM_COMMAND_BUFFERS];
	struct ralBuffer_s          *ral_lens_readback[NUM_COMMAND_BUFFERS];
	qboolean                     lensReadbackReady[NUM_COMMAND_BUFFERS];
	void                        *lensSourcesPtr;        // CPU shadow; never a GPU mapping
	int                          lensSourceCount;        // active slot high-water this frame
	struct ralTextureView_s     *ral_lens_depth_view;   // sampling view over vk.sceneDepth.ral_image (NEAREST)
	struct ralSampler_s         *ral_lens_sampler;      // NEAREST + clamp portable depth sampling
	struct ralBindGroupLayout_s *ral_lens_bgl;          // depth tex(0) + sampler(1) + lens SSBO(2), COMPUTE
	struct ralBindGroup_s       *ral_lens_descriptor[NUM_COMMAND_BUFFERS];
	struct ralPipeline_s        *ral_lens_pipeline;     // the N-tap occlusion compute pipeline

	// Static surfaceIndex↔vboItemIndex map + per-vboItem static
	// batch-key data, baked ONCE at world VBO build (same map-load lifetime as the
	// cull AABB SSBO). The GPU-decomposition chain needs this bridge because the
	// cull emits world surfaceIndex (0-based into s_worldData.surfaces[]) while
	// the batch decomposition keys on vboItemIndex (1-based, assigned in shader-sorted
	// order). PURE DATA — consumed by nobody yet (a later pass starts reading it); for
	// now this only builds + self-verifies. Host tables (the batch build is host-side);
	// promote to a RAL SSBO only if a compute stage reads them. cullSurfaceCount/cullVboItemCount
	// == 0 means no map (non-VBO world / no static surfaces).
	uint32_t                    *cullSurfToItem;      // [cullSurfaceCount] surfaceIndex → vboItemIndex (0 = not a VBO item)
	uint32_t                    *cullItemToSurf;      // [cullVboItemCount+1] vboItemIndex → surfaceIndex (index 0 unused)
	uint32_t                    *cullItemSortKey;     // [cullVboItemCount+1] per-vboItem static (sortedIndex<<16)|fogIndex
	int                          cullVboItemCount;    // # static VBO items (== numStaticSurfaces at build)

	// BRDF integration LUT: the view-dependent term of the split-sum IBL
	// approximation (Karis 2013), a 256x256 RG16F storage image computed ONCE at
	// boot by a compute pass (scene-independent constant math) and held for the
	// life of the renderer. Nothing samples it yet — the IBL term that consumes
	// it lands later; for now it is produced and (optionally) read back for
	// verification. All handles are RAL-owned (created, not adopted). The one-shot
	// dispatch fires in the first frame's no-render-pass seam and self-disables via
	// brdfLutDispatched, so it costs nothing after frame 0.
	struct ralTexture_s         *ral_brdf_lut_image;     // 256x256 RG16F, device-local STORAGE+SAMPLED
	struct ralTextureView_s     *ral_brdf_lut_view;      // storage view bound for imageStore
	struct ralBindGroupLayout_s *ral_brdf_lut_bgl;       // set 0: storage image (binding 0)
	struct ralBindGroup_s       *ral_brdf_lut_descriptor;// the bound compute set
	struct ralPipeline_s        *ral_brdf_lut_pipeline;  // the compute pipeline
	qboolean                     brdfLutDispatched;       // false until the one-shot boot dispatch is recorded
	// Debug-only readback (r_brdfLutDebug): COPY_DEST → HOST_READ, mapped only
	// after the one-shot copy's in-flight depth has completed.
	struct ralBuffer_s          *ral_brdf_lut_readback;
	qboolean                     brdfLutReadbackReady;
	qboolean                     brdfLutReadbackLogged;   // false until the one-time debug log fires
	// Dedicated LINEAR + CLAMP_TO_EDGE + LINEAR-mipmap sampler the engine-resources
	// descriptor writes bind alongside the BRDF LUT + probe cube views (set 2,
	// bindings 1/2/3). Created unconditionally with the BRDF LUT (NOT borrowed from
	// SMAA — that sampler is NULL when SMAA/FBO is off, which silently dropped the
	// IBL bindings; and its maxLod=0 clamped the radiance cube to mip 0). LINEAR
	// mipmap + full LOD range so the radiance specular reads its roughness mips.
	struct ralSampler_s         *ral_ibl_sampler;

	// IBL probe infrastructure (the ambient half of PBR). THREE cube textures, all
	// RAL-owned and inert this phase (nothing samples them yet — the convolve fills
	// the probes later, the IBL term consumes them later):
	//  - probe_source: a 6-face analytic sky cube, filled ONCE at boot by the
	//    source_sky compute (image2DArray storage write, cube sampled view). This is
	//    the SWAP-READY source — a future real-HDR DDS load replaces only the fill.
	//  - probe_irradiance: a small cube for the diffuse irradiance (allocated EMPTY;
	//    the convolve fills it later).
	//  - probe_radiance: a MIPPED cube for the prefiltered specular, one mip per
	//    roughness level (allocated EMPTY). Its per-(face,mip) 2D storage views are
	//    the plumbing the convolve will imageStore into.
	// Each cube carries: the texture, a full-cube SAMPLED view (bound into the
	// engine-resources set for later sampling), and — for the storage targets — the
	// per-face (source) or per-(face,mip) (radiance) 2D STORAGE views. The source
	// also has a compute pipeline/bgl/descriptor + a one-shot dispatch guard,
	// mirroring the BRDF-LUT. vid_restart re-arms the guard and recreates all views.
	struct ralTexture_s         *ral_probe_source;        // 64x64x6 RGBA16F cube (analytic sky source)
	struct ralTextureView_s     *ral_probe_source_cube_view;             // VK_IMAGE_VIEW_TYPE_CUBE, sampled
	// One 2D-ARRAY storage view over all 6 layers — matches source_sky.comp's
	// image2DArray declaration (the shader writes layer = gl_GlobalInvocationID.z).
	// A per-face VK_IMAGE_VIEW_TYPE_2D view would mismatch the arrayed OpTypeImage
	// (VUID-vkCmdDispatch-viewType-07752).
	struct ralTextureView_s     *ral_probe_source_array_view;
	struct ralBindGroupLayout_s *ral_probe_source_bgl;    // set 0: storage image (binding 0)
	struct ralBindGroup_s       *ral_probe_source_descriptor;            // binds the 2D-array storage view
	struct ralPipeline_s        *ral_probe_source_pipeline;
	qboolean                     probeSourceDispatched;   // false until the one-shot source fill is recorded

	struct ralTexture_s         *ral_probe_irradiance;    // 32x32x6 RGBA16F cube, 1 mip (diffuse irradiance)
	struct ralTextureView_s     *ral_probe_irradiance_cube_view;         // VK_IMAGE_VIEW_TYPE_CUBE, sampled
	// One 2D-ARRAY storage view over all 6 layers (mip 0) — the convolve writes the
	// whole cube in one dispatch (image2DArray, layer = gl_GlobalInvocationID.z).
	struct ralTextureView_s     *ral_probe_irradiance_array_view;

	struct ralTexture_s         *ral_probe_radiance;      // 128x128x6 RGBA16F cube, N mips (prefiltered specular)
	struct ralTextureView_s     *ral_probe_radiance_cube_view;           // VK_IMAGE_VIEW_TYPE_CUBE, all mips, sampled
	// Per-MIP 2D-ARRAY storage views (all 6 faces at one mip): index [mip]. The
	// convolve dispatches once per mip (z=6 faces) into these — a 2D-array view (not
	// a single-face 2D view) matches the prefilter shader's image2DArray output
	// (a per-face 2D view trips VUID-vkCmdDispatch-viewType-07752).
	struct ralTextureView_s     *ral_probe_radiance_mip_view[VK_PROBE_RADIANCE_MIPS];

	// Convolve compute: source sampled cube (binding 0/1) + the per-dispatch storage
	// array view (binding 2). One pipeline + bgl each for irradiance / radiance; the
	// irradiance bind-group is fixed (one storage view), the radiance bind-group is
	// rebuilt per mip (the storage view changes) — held in an array indexed by mip.
	struct ralSampler_s         *ral_probe_conv_sampler;  // linear-clamp cube sampler for the source read
	struct ralBindGroupLayout_s *ral_probe_conv_bgl;      // set 0: sampled cube + sampler + storage array (shared)
	struct ralBindGroup_s       *ral_probe_irradiance_bg; // source + irradiance array view
	struct ralPipeline_s        *ral_probe_irradiance_pipeline;
	qboolean                     probeIrradianceDispatched;
	struct ralBindGroup_s       *ral_probe_radiance_bg[VK_PROBE_RADIANCE_MIPS];  // source + the mip's array view
	struct ralPipeline_s        *ral_probe_radiance_pipeline;
	qboolean                     probeRadianceDispatched;

	// Debug-only readback (r_probeSourceDebug / r_probeRadianceDebug): host-coherent
	// mirrors the one-shot dispatches copy the cubes into, so the CPU can confirm a
	// plausible directional source sky + a converging convolve (mip0≈source, high-mip
	// blurred, irradiance smooth). Each is bounded MAP_READ after completion.
	struct ralBuffer_s          *ral_probe_source_readback;
	qboolean                     probeSourceReadbackReady;
	qboolean                     probeSourceReadbackLogged;
	struct ralBuffer_s          *ral_probe_radiance_readback;   // mirror of radiance mip0 + an inner mip face
	qboolean                     probeRadianceReadbackReady;
	qboolean                     probeRadianceReadbackLogged;
	struct ralBuffer_s          *ral_probe_irradiance_readback;
	qboolean                     probeIrradianceReadbackReady;

	// Dual-filtering bloom pyramid: one image per mip level. Slot 0 is the
	// extract output (full res); slot k+1 is the level-k mip at captureW/2^(k+1).
	// The downsample writes each mip, the upsample additively reconstructs back
	// down the chain, and the composite blends slot 1 into the scene.
	// Direct dynamic-rendering owners for the bloom extract/downsample/upsample
	// chain. Index 0 is the extract target; index k (k>=1) is the mip written by
	// downsample level k-1. Adopted (gated fboActive && r_bloom) in
	// attachment creation; bound as Ral_BeginRendering color
	// attachments by the extract/mip begins.
	struct ralTexture_s *ral_bloom_image[1+VK_NUM_BLOOM_PASSES];

	VkDescriptorSet bloom_image_descriptor[1+VK_NUM_BLOOM_PASSES];
	// Portable sampling views and arena-owned set-0 groups for the bloom chain.
	// The raw descriptor array is only the Vulkan mirror.
	struct ralTextureView_s *ral_bloom_image_view[1+VK_NUM_BLOOM_PASSES];
	struct ralBindGroup_s *ral_bloom_image_descriptor[1+VK_NUM_BLOOM_PASSES];

	struct ralTexture_s *ral_depth_image;

	// Scene-depth copy: a sampled snapshot of the opaque-scene depth, shared by
	// every consumer that needs to read depth in a later pass — soft-particle
	// depth fade (r_depthFade), SSAO/GTAO, sunrays, and the lens-occlusion oracle.
	struct {
		VkSampler       sampler;
		struct ralSampler_s *ral_sampler;
		VkDescriptorSet descriptor;
		// Direct portable texture/view ownership + current-arena bind group. The
		// raw sampler/set are Vulkan pipeline-layout mirrors only.
		struct ralTexture_s *ral_image;
		struct ralTextureView_s *ral_view;
		struct ralBindGroup_s *ral_descriptor;
		qboolean        active;
		qboolean        copied;		// depth was copied this frame
		qboolean        pendingRebuild;	// a live consumer toggle (e.g. r_drawSunRays) requested an attachment/render-pass rebuild; consumed at the next safe frame boundary in vk_begin_frame
		int             bindlessSamplerSlot;  // dedup-pool slot of the NEAREST depth sampler in the bindless sampler array; -1 until resolved (gates the role-4 override)
	} sceneDepth;

#if FEAT_SHADOW_MAPPING
	// Shadow mapping. The depth target is a 4-layer 2D array
	// (one layer per CSM cascade). The pass renders the world casters into every
	// cascade using a per-cascade light view-proj fit (Practical Split + stable
	// texel snap). The sampling view is 2D_ARRAY; each framebuffer wraps a
	// single-layer 2D view of one cascade. (SHADOWMAP_MAX_CASCADES is defined
	// at file scope above — it also sizes vkUniform_t.cascadeMVP[].)
	struct {
		// Direct 4-layer depth array. RAL owns the full-array sampling view and one
		// per-layer attachment view selected by depthAttachmentLayerIndex.
		struct ralTexture_s *ral_image;
		VkSampler        sampler;
		struct ralSampler_s *ral_sampler;
		// renderPass / framebuffer[] / depthPipeline are gone — the cascaded shadow
		// depth pass is dynamic-rendering only.
		struct ralPipeline_s *ral_depthPipeline;  // dynamic-rendering pipeline (gpInfo-derived, depth-only)
		VkPipelineLayout depthLayout;     // set 0 = entity-matrix SSBO + set 1 = cascadeMVP UBO; ZERO push
		struct ralPipelineLayout_s *ral_depthLayout;  // typed sibling
		VkDescriptorSetLayout set_layout_entmat;  // set 0 of depthLayout — 1 STORAGE_BUFFER binding, VS stage
		struct ralBindGroupLayout_s *ral_bgl_entmat;
		// Per-cascade cascadeMVP UBO — set 1 of depthLayout (1 UNIFORM_BUFFER_DYNAMIC
		// binding, VS). Carries cascadeMVP (no longer a push). One host-coherent buffer
		// per in-flight frame holding SHADOWMAP_MAX_CASCADES PAD-aligned mat4 slots;
		// written + bound per cascade with the dynamic offset casc*PAD(64,uniform_alignment).
		// The shadow depthLayout has ZERO push ranges.
		VkDescriptorSetLayout set_layout_cascademvp;
		struct ralBindGroupLayout_s *ral_bgl_cascademvp;
		struct ralBuffer_s *ral_cascadeMvpBuf[NUM_COMMAND_BUFFERS];
		void            *cascadeMvpPtr[NUM_COMMAND_BUFFERS];
		VkDescriptorSet  cascadeMvpDesc[NUM_COMMAND_BUFFERS]; // UNIFORM_BUFFER_DYNAMIC, base offset 0
		struct ralBindGroup_s *ral_cascadeMvpDesc[NUM_COMMAND_BUFFERS];
		qboolean         active;
		uint32_t         size;            // shadow map resolution per cascade (default 2048)
		float            cascadeMVP[SHADOWMAP_MAX_CASCADES][16]; // per-cascade light view-proj (column-major), recomputed per frame
		float            cascadeSplits[4];                        // view-space Z for splits 1..4 (split 0 = near is implicit)
		// world caster geometry (worldspawn / model 0 only) — position-only,
		// built lazily once per map load
		struct ralBuffer_s *ral_casterBuf; // [vec4 positions][uint32 indices], device-local
		uint32_t         casterVtxBytes;  // byte offset where the index data begins
		uint32_t         casterIndexCount;
		const void      *casterBuiltSurfaces; // tr.world->surfaces value the buffer was built for (NULL = none)
		// Alpha-tested (cut-out) worldspawn casters — a strictly additive second
		// caster sub-group, built from the same surface walk but admitting surfaces
		// whose stage[0] carries GLS_ATEST_BITS (opaque casters above stay
		// position-only and byte-identical). Each vertex is
		// [ vec4 position ][ vec2 diffuse-uv ][ uint packed ], where packed =
		// (WIRED_BINDLESS_PACK(diffuse) in the low 24 bits | alpha-test func in the
		// high 8 bits) — so the alpha-test depth shader resolves the diffuse through
		// the same bindless table the main pass uses and discards holed fragments.
		struct ralBuffer_s *ral_casterAtestBuf; // [stride-32 verts][uint32 indices], device-local
		uint32_t         casterAtestVtxBytes;  // byte offset where the index data begins
		uint32_t         casterAtestIndexCount;
		// Inline-brush-model casters. One shared device-local
		// buffer holding every bmodel's opaque geometry concatenated
		// ([vec4 positions][uint32 indices]); per-bmodel slices in bmodelRanges[k]
		// (k matches tr.world->bmodels). Drawn per visible MOD_BRUSH entity in the
		// shadow pre-pass with that entity's [axis|origin] model matrix. Built /
		// freed alongside casterBuf (same casterBuiltSurfaces gate — bmodels are
		// static for a map's lifetime).
		struct ralBuffer_s      *ral_casterBmodelBuf; // shared bmodel geometry
		uint32_t                casterBmodelVtxBytes;// byte offset where the index region begins
		vkBmodelCasterRange_t  *bmodelRanges;        // ri.Malloc'd, length = numBmodelRanges (== tr.world->numBModels); NULL if none
		int                     numBmodelRanges;

		// GPU-skinned IQM casters. The static IQM model VBO (positions + bone
		// weights/indices) stays on the GPU; the skinned shadow VS skins it in the
		// shadow pass from this caster's bone matrices (set 2, same packing as the
		// main IQM pass — reused at the capture seam, same pose). depthLayoutSkinned
		// adds set 2 (bone UBO) on top of depthLayout's set 0 (entity SSBO) + set 1
		// (cascadeMVP). One bone ring per in-flight frame (host-coherent, mirroring
		// vk.iqmGpu.ral_bone_buffer); each captured caster sub-allocates a PAD-aligned
		// IQM_BONE_UBO item and binds it with a dynamic offset.
		struct ralPipeline_s   *ral_depthPipelineSkinned; // 3-set skinned-IQM caster pipeline
		VkPipelineLayout        depthLayoutSkinned;        // set0 entSSBO + set1 cascadeMVP + set2 bone UBO
		struct ralPipelineLayout_s *ral_depthLayoutSkinned;// typed sibling

		// Alpha-tested caster pipeline. Same depth-only state as the static caster
		// pipeline, but the VS carries diffuse UV + the packed bindless/atest value,
		// and the FS samples the diffuse alpha and discards holed fragments. The
		// layout adds set 2 = the bindless 2D table (image array + sampler array),
		// on top of depthLayout's set 0 (entity SSBO) + set 1 (cascadeMVP).
		struct ralPipeline_s   *ral_depthPipelineAtest;   // 3-set alpha-test caster pipeline
		VkPipelineLayout        depthLayoutAtest;          // set0 entSSBO + set1 cascadeMVP + set2 bindless
		struct ralPipelineLayout_s *ral_depthLayoutAtest;  // typed sibling
		VkDescriptorSetLayout   set_layout_bones;          // set 2 — 1 UNIFORM_BUFFER_DYNAMIC binding, VS
		struct ralBindGroupLayout_s *ral_bgl_bones;
		struct ralBuffer_s      *ral_boneBuf[NUM_COMMAND_BUFFERS]; // per-frame bone ring
		void                   *bonePtr[NUM_COMMAND_BUFFERS];
		uint32_t                boneRingSize;                   // bytes per ring
		uint32_t                boneOffset[NUM_COMMAND_BUFFERS];// running sub-alloc offset (reset per frame)
		VkDescriptorSet         boneDesc[NUM_COMMAND_BUFFERS];  // UNIFORM_BUFFER_DYNAMIC, base offset 0
		struct ralBindGroup_s   *ral_boneDesc[NUM_COMMAND_BUFFERS];
	} shadowMap;

	// Point-light (omni dlight) shadows — up to DLIGHT_SHADOW_K_MAX shadow-casting
	// lights, the top-K brightest visible dlights. A 2D ATLAS (WebGPU-portable, NOT a
	// cubemap-array) holding each light's 6 cube faces as a horizontal strip of tiles:
	// light L occupies columns [L*6 .. L*6+6); each face is a 90° perspective depth
	// render from the light origin (reuses the CSM depth-only pipeline + worldspawn
	// caster buffer + shadow_depth.vert). The forwardplus_lit fragment reconstructs the
	// face from the world-space light→fragment vector (manual cube-face-select), looks up
	// the matching light's column + UV, and depth-compares for occlusion. r_dlightShadows-
	// gated → no allocation / no render / no sample when off (byte-identical). K=1 is
	// byte-identical to a single-light budget (columns 0-5, one sample, /6.0 UV divisor).
	// Runtime dlights only — the BSP static lights stay baked (no extraction).
	struct {
		struct ralTexture_s *ral_image;// direct 2D depth atlas (6*K face tiles wide)
		VkSampler        sampler;      // depth-compare sampler (or nearest + manual compare)
		struct ralSampler_s *ral_sampler;
		VkDescriptorSet  descriptor;   // bound to the lit pass for sampling
		// Per-light per-face view-proj (column-major) — 90° perspective from each light
		// origin toward ±X/±Y/±Z, recomputed per frame for the budgeted lights. Written to
		// the per-frame faceMvp UBO (set 1 of the SAME depthLayout shadow_depth.vert reads).
		float            faceMVP[DLIGHT_SHADOW_K_MAX][6][16];
		struct ralBuffer_s *ral_faceMvpBuf[NUM_COMMAND_BUFFERS];
		void            *faceMvpPtr[NUM_COMMAND_BUFFERS];
		VkDescriptorSet  faceMvpDesc[NUM_COMMAND_BUFFERS]; // UNIFORM_BUFFER_DYNAMIC, base 0 (per-face dyn offset)
		struct ralBindGroup_s *ral_faceMvpDesc[NUM_COMMAND_BUFFERS];
		struct ralBuffer_s *ral_entMatBuf[NUM_COMMAND_BUFFERS]; // 1-slot identity model→world
		void            *entMatPtr[NUM_COMMAND_BUFFERS];
		VkDescriptorSet  entMatDesc[NUM_COMMAND_BUFFERS];
		struct ralBindGroup_s *ral_entMatDesc[NUM_COMMAND_BUFFERS];
		uint32_t         tileSize;     // per-face tile resolution (atlas = tileSize*6*K wide, tileSize tall)
		uint32_t         atlasK;       // K the atlas was allocated for (UV divisor = 6*atlasK); =1 when off
		// The lit-pass shadow UBO (set2 binding 9): 6*K_MAX face MVPs + K_MAX light slots
		// (lightIndex, valid, bias) + numShadowLights + the column count (6*atlasK). Per
		// in-flight frame, host-coherent, written each frame the lit pass runs
		// (numShadowLights=0 / valid=0 when no shadow light → the sample is gated off).
		struct ralBuffer_s *ral_paramsBuf[NUM_COMMAND_BUFFERS];
		void            *paramsPtr[NUM_COMMAND_BUFFERS];
		int              lightIndex[DLIGHT_SHADOW_K_MAX]; // budgeted lights' dlights[] indices this frame (-1 = none)
		int              numShadowLights;                 // how many lights were selected this frame (0..K)
		qboolean         active;       // r_dlightShadows && fboActive && a shadow-casting light this frame
		qboolean         haveLight;    // at least one budgeted shadow-casting light was selected this frame
		qboolean         pendingRebuild; // a live r_dlightShadows / r_dlightShadowK toggle requested an atlas re-alloc; consumed at the next safe frame boundary in vk_begin_frame
		float            lightOrigin[DLIGHT_SHADOW_K_MAX][3];
		float            lightFar[DLIGHT_SHADOW_K_MAX]; // each light's radius (far plane for the depth compare)
	} dlightShadow;
#endif

	// SMAA anti-aliasing
	struct {
		qboolean        active;
		int             quality;        // 1-4

		// LUT textures (static within the active SMAA cohort)
		VkDescriptorSet area_descriptor;
		struct ralBindGroup_s *ral_area_descriptor;
		struct ralTexture_s *ral_area_image;
		struct ralTextureView_s *ral_area_view;

		VkDescriptorSet search_descriptor;
		struct ralBindGroup_s *ral_search_descriptor;
		struct ralTexture_s *ral_search_image;
		struct ralTextureView_s *ral_search_view;

		// Direct RAL intermediate textures (resolution-dependent). Their independent
		// ownership is load-bearing for live r_smaa disable/re-enable.
		VkDescriptorSet edges_descriptor;
		struct ralBindGroup_s *ral_edges_descriptor;
		struct ralTexture_s *ral_edges_image;  // R8G8
		struct ralTextureView_s *ral_edges_view;

		VkDescriptorSet blend_descriptor;
		struct ralBindGroup_s *ral_blend_descriptor;
		struct ralTexture_s *ral_blend_image;  // RGBA8
		struct ralTextureView_s *ral_blend_view;

		VkDescriptorSet input_descriptor;
		struct ralBindGroup_s *ral_input_descriptor;
		struct ralTexture_s *ral_input_image;  // color_format copy of color_image
		struct ralTextureView_s *ral_input_view;

		VkSampler       point_sampler;
		VkSampler       linear_sampler;
		struct ralSampler_s *ral_point_sampler;
		struct ralSampler_s *ral_linear_sampler;
	} smaa;

	// Blue-noise dither tile (gamma.frag ditherMode 2). The complete texture/view/
	// sampler/bind-group cohort is RAL-owned so the same lifetime and binding shape
	// lowers to Vulkan, Metal and WebGPU. `descriptor` is only the transitional
	// Vulkan mirror used by the still-native gamma pipeline layout.
	struct {
		struct ralTexture_s *ral_texture;
		struct ralTextureView_s *ral_view;
		struct ralSampler_s *ral_sampler;
		VkDescriptorSet descriptor;
		struct ralBindGroup_s *ral_descriptor;
	} blueNoise;

	// screenMap
	struct {
		VkDescriptorSet color_descriptor;
		struct ralTextureView_s *ral_color_view;
		struct ralBindGroup_s *ral_color_descriptor;  // arena-owned set 0; raw field is its mirror
		struct ralTexture_s *ral_color_image;
		struct ralTexture_s *ral_depth_image;

	} screenMap;

	struct {
		struct ralTexture_s *ral_image;
	} capture;

#ifdef USE_UPLOAD_QUEUE
	VkSemaphore rendering_finished;	// reference to vk.cmd->rendering_finished2
	struct ralSemaphore_s *ral_rendering_finished;  // parallel-RAL alias of vk.cmd->ral_rendering_finished2; flips NULL/non-NULL alongside the legacy alias
	VkSemaphore image_uploaded2;
	struct ralSemaphore_s *ral_image_uploaded2;     // adopted sibling of image_uploaded2
	VkSemaphore image_uploaded;		// reference to vk.image_uploaded2
	struct ralSemaphore_s *ral_image_uploaded;      // parallel-RAL alias of vk.ral_image_uploaded2; flips NULL/non-NULL alongside the legacy alias
#endif

	vk_tess_t tess[ NUM_COMMAND_BUFFERS ], *cmd;
	int cmd_index;

	uint32_t uniform_item_size;
	uint32_t uniform_alignment;

	float    timestampPeriodNs;     // ns per timestamp tick, from VkPhysicalDeviceLimits
	qboolean timestampSupported;    // device supports CmdWriteTimestamp on graphics queue

	struct {
		struct ralBuffer_s *ral_vertex_buffer;
	} vbo;

	// Per-frame host-visible RAL buffers hold vertex, index and uniform data.
	VkDeviceSize geometry_buffer_size;
	VkDeviceSize geometry_buffer_size_new;

	// statistics
	struct {
		VkDeviceSize vertex_buffer_max;
		uint32_t push_size;
		uint32_t push_size_max;
	} stats;

	//
	// Shader modules.
	//
	struct {
		struct {
			VkShaderModule gen[3][2][2][2]; // tx[0,1,2], cl[0,1] env0[0,1] fog[0,1]
			VkShaderModule ident1[2][2][2]; // tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule fixed[2][2][2];  // tx[0,1], env0[0,1] fog[0,1]
			VkShaderModule light[2];        // fog[0,1]
#if FEAT_PARALLAX_MAPPING
			VkShaderModule light_parallax[2]; // fog[0,1]
#endif
		} vert;
		struct {
			VkShaderModule gen0_df;
			VkShaderModule gen[3][2][2]; // tx[0,1,2] cl[0,1] fog[0,1]
			VkShaderModule ident1[2][2]; // tx[0,1], fog[0,1]
			VkShaderModule fixed[2][2];  // tx[0,1], fog[0,1]
			VkShaderModule ent[1][2];    // tx[0], fog[0,1]
			VkShaderModule light[2][2];  // linear[0,1] fog[0,1]
#if FEAT_PARALLAX_MAPPING
			VkShaderModule light_parallax[2][2]; // linear[0,1] fog[0,1]
#endif
			// depth fade variants (single-texture only)
			VkShaderModule dfade_gen[1][2];    // tx[0], fog[0,1]
			VkShaderModule dfade_ident1[1][2]; // tx[0], fog[0,1]
			VkShaderModule dfade_fixed[1][2];  // tx[0], fog[0,1]
			VkShaderModule dfade_ent[1][2];    // tx[0], fog[0,1]
		} frag;

		VkShaderModule msdf_fs;
		VkShaderModule msdf_vs;

		VkShaderModule color_fs;
		VkShaderModule color_vs;

		VkShaderModule bloom_fs;
		// dual-filtering bloom: 13-tap Karis downsample + 9-tap tent
		// progressive-additive upsample, then a single-sampler composite of the
		// top mip into the scene.
		VkShaderModule downsample_fs;
		VkShaderModule upsample_fs;
		VkShaderModule bloom_composite_fs;

		VkShaderModule gamma_fs;
		VkShaderModule gamma_vs;

		VkShaderModule menubg_fs;    // WiredUI SCENE procedural backdrop (blended fullscreen)

		VkShaderModule overlay_fs;   // post-gamma HUD overlay (display-space alpha blend)
		VkShaderModule overlay_vs;

		VkShaderModule forwardplus_lit_vs;  // Forward+ world-space lit consumer (tile light gather)
		VkShaderModule forwardplus_lit_fs;

		// Scene-radiance post-process pass. tonemap.frag
		// owns exposure bias, SSAO, sunrays, tonemap operator, colour
		// grading, saturation. Reuses gamma_vs (generic fullscreen
		// quad) — no separate tonemap_vs.
		VkShaderModule tonemap_fs;

		VkShaderModule fog_fs;
		VkShaderModule fog_vs;

#if FEAT_ADVANCED_WATER
		VkShaderModule water_fs;
#endif

#if FEAT_SHADOW_MAPPING
		VkShaderModule shadow_depth_vs;
		VkShaderModule shadow_depth_skinned_vs;  // GPU-skinned IQM caster VS (4-bone blend)
		VkShaderModule shadow_depth_fs;
		VkShaderModule shadow_depth_atest_vs;    // alpha-tested caster VS (position + uv + packed bindless/atest)
		VkShaderModule shadow_depth_atest_fs;    // alpha-tested caster FS (samples diffuse alpha, discards holed fragments)
		VkShaderModule light_shadow[2];      // vert: fog[0,1]
		VkShaderModule light_shadow_frag[2][2]; // frag: linear[0,1] fog[0,1]
		// World-lightmap sun-shadow receiver modules. Indexed
		// [tx 1|2][colour-mode][fog 0|1]; the tx-0 slot is unused (no
		// lightmap). Colour-mode: 0=mul 1=cl 2=ident 3=fixed — slots 2/3
		// are populated for tx=1 (MUL2) only.
		VkShaderModule gen_shadow_vert[3][4][2];
		VkShaderModule gen_shadow_frag[3][4][2];
#endif
#if FEAT_PBR
		VkShaderModule light_pbr_frag[2][2]; // frag: linear[0,1] fog[0,1]
		// Base-pass IBL receiver modules — same [tx 1|2][colour-mode 0=mul/1=cl]
		// [fog 0|1] indexing as gen_shadow_*; only the four core lightmap-modulate
		// slots are populated (mul2 / blend2-mul / mul3 / blend3-mul).
		VkShaderModule gen_ibl_vert[3][4][2];
		VkShaderModule gen_ibl_frag[3][4][2];
#endif

		VkShaderModule smaa_edge_vs;
		VkShaderModule smaa_edge_fs;
		VkShaderModule smaa_blend_vs;
		VkShaderModule smaa_blend_fs;
		VkShaderModule smaa_resolve_vs;
		VkShaderModule smaa_resolve_fs;

		// Q1 4-style lightmap blend
		VkShaderModule q1_ls_vs;
		VkShaderModule q1_ls_fs;
		VkShaderModule q1_ls_array_fs;   // array variant; reuses q1_ls_vs

		// primitive ribbon
		VkShaderModule ribbon_vs;
		VkShaderModule ribbon_fs;
		VkShaderModule ribbon_spiral_vs;   // parametric rail-helix generator (reuses ribbon_fs)

		// primitive sprite (billboarded quad, view-axis aligned)
		VkShaderModule sprite_vs;
		VkShaderModule sprite_fs;

		// primitive beam (two-endpoint camera-facing quad with axial-copy expansion)
		VkShaderModule beam_vs;
		VkShaderModule beam_fs;

		// primitive particle (compute-driven pool, billboard render)
		VkShaderModule particle_vs;
		VkShaderModule particle_fs;

		// GPU decal projector (surface-aligned instanced quad render)
		VkShaderModule decal_vs;
		VkShaderModule decal_fs;

		// GPU-resident atmospheric weather (rain/snow): self-spawning compute
		// pool + instanced streak/billboard render
		VkShaderModule atmospheric_vs;
		VkShaderModule atmospheric_fs;

#if FEAT_IQM
		// IQM GPU skinning
		VkShaderModule iqm_skinning_vs;
		VkShaderModule iqm_skinning_fs;
#endif
	} modules;

	VK_Pipeline_t pipelines[ MAX_VK_PIPELINES ];
	uint32_t pipelines_count;
	uint32_t pipelines_world_base;

	// pipeline statistics
	int32_t pipeline_create_count;

	//
	// Standard pipelines.
	//
	uint32_t skybox_pipeline;

	// dim 0: 0 - front side, 1 - back size
	// dim 1: 0 - normal view, 1 - mirror view
	uint32_t shadow_volume_pipelines[2][2];
	uint32_t shadow_finish_pipeline;

	// dim 0 is based on fogPass_t: 0 - corresponds to FP_EQUAL, 1 - corresponds to FP_LE.
	// dim 1 is directly a cullType_t enum value.
	// dim 2 is a polygon offset value (0 - off, 1 - on).
	uint32_t fog_pipelines[2][3][2];

	// cullType[3], polygonOffset[2], fogStage[2], absLight[2]
#ifdef USE_PMLIGHT
	uint32_t dlight_pipelines_x[3][2][2][2];
	uint32_t dlight1_pipelines_x[3][2][2][2];
#endif

	// debug visualization pipelines
	uint32_t tris_debug_pipeline;
	uint32_t tris_mirror_debug_pipeline;
	uint32_t tris_debug_green_pipeline;
	uint32_t tris_mirror_debug_green_pipeline;
	uint32_t tris_debug_red_pipeline;
	uint32_t tris_mirror_debug_red_pipeline;

	uint32_t normals_debug_pipeline;
	uint32_t surface_debug_pipeline_solid;
	uint32_t surface_debug_pipeline_outline;
	uint32_t images_debug_pipeline;
	uint32_t images_debug_pipeline2;
	uint32_t surface_beam_pipeline;
	uint32_t surface_axis_pipeline;

	uint32_t msdf_pipeline;
	uint32_t q1ls_pipeline;		// Q1 4-style lightmap blend, animChain lerp
	uint32_t q1ls_array_pipeline;	// Q1 4-style lightmap blend, texture array animation

	// gamma is dynamic-rendering only (ral_gamma_pipeline) — no legacy VkPipeline.

	// Tonemap pass is the new home of scene-radiance
	// post-process effects. gamma_pipeline is now a thin display-encode
	// (sRGB + dither) pass with no variants. tonemap_pipeline is the
	// passthrough/default tonemap (no feature defines set); tonemap_variants[]
	// holds the feature-combinatorial pipelines indexed by TONEMAP_VAR_*
	// bits.
	// FXAA (was bit 3) was removed; SMAA is the AA path
	// going forward. SUNRAYS renumbered down from bit 4 to bit 3 to
	// keep the bitmap contiguous and shrink the variant array by half.
	// GAMMA_VAR_* renamed to TONEMAP_VAR_* to
	// reflect that the variant bits gate features in tonemap.frag, not
	// gamma.frag. GAMMA_VAR_TONEMAP became TONEMAP_VAR_BASE per the
	// rename spec (represents the base USE_TONEMAP variant).
	// Bit 0 = SSAO (RETIRED), Bit 1 = TONEMAP (BASE), Bit 2 = COLOR_GRADING, Bit 3 = SUNRAYS.
	// Bit 0 is reserved for the diagnostic-only denoised-GTAO isolation view. It
	// does not restore the retired per-pixel tonemap SSAO path and is never combined
	// with normal scene-radiance variants.
#define TONEMAP_VAR_SSAO    1
#define TONEMAP_VAR_BASE    2
#define TONEMAP_VAR_CG      4
#define TONEMAP_VAR_SUNRAYS 8
#define TONEMAP_VAR_COUNT   16
	// tonemap is dynamic-rendering only (ral_tonemap_pipeline / ral_tonemap_variants[]).
	// The variant shader modules stay — the RAL siblings are built from them.
	VkShaderModule tonemap_variant_fs[TONEMAP_VAR_COUNT];
	// Dynamic-rendering tonemap pipelines: RAL siblings of tonemap_pipeline
	// (no-feature default) and tonemap_variants[TONEMAP_VAR_BASE] (the
	// USE_TONEMAP path bound under stock cvars). Carry the same cvar-baked
	// fragment spec constants as the legacy pipelines and are recreated
	// alongside them on every renderer-cvar rebake, so the tonemap operator
	// stays in sync. The bind path uses these instead of the legacy VkPipeline.
	struct ralPipeline_s *ral_tonemap_pipeline;        // varIdx == 0 sibling
	// Dynamic-rendering sibling per built tonemap variant, indexed by the FEAT
	// bitmask (SSAO|BASE|CG|SUNRAYS). Built in lockstep with tonemap_variants[]
	// behind the same FEAT fences; NULL for any combo whose FEAT is off (never
	// selected, mirroring the legacy array). The render path binds these.
	struct ralPipeline_s *ral_tonemap_variants[TONEMAP_VAR_COUNT];
	struct ralPipeline_s *ral_capture_pipeline;        // dynamic-rendering sibling of capture (screenshot pass)
	struct ralPipeline_s *ral_gamma_pipeline;          // dynamic-rendering sibling of gamma (swapchain present pass)
	struct ralPipeline_s *ral_menubg_pipeline;         // WiredUI SCENE procedural backdrop (blended fullscreen into UI pass, RPFMT_UI)
	struct ralPipeline_s *ral_overlay_pipeline;        // post-gamma HUD overlay (2D quads on swapchain, display-space alpha blend)
	struct ralPipeline_s *ral_overlay_capture_pipeline; // overlay sibling for the supersample capture pass (RPFMT_CAPTURE format)
	struct ralPipeline_s *ral_bloom_extract_pipeline;  // dynamic-rendering sibling of bloom_extract
	// Dual-filtering bloom pipelines, one per mip level (the sole bloom path).
	// Downsample: CLEAR/STORE, no blend, 13-tap Karis. Upsample: LOAD + ONE/ONE
	// additive, 9-tap tent. Built whenever fboActive && r_bloom.
	struct ralPipeline_s *ral_downsample_pipeline[VK_NUM_BLOOM_PASSES];
	struct ralPipeline_s *ral_upsample_pipeline[VK_NUM_BLOOM_PASSES];
	// Final dual-filtering composite: top bloom mip -> scene (POST_BLOOM format,
	// ONE/ONE additive), intensity baked from r_bloomIntensity.
	struct ralPipeline_s *ral_bloom_dual_composite_pipeline;
	VkPipelineLayout pipeline_layout_ssao;    // 2 samplers: color + depth (SSAO without sunrays)
	VkPipelineLayout pipeline_layout_sunrays; // color + depth samplers + set-2 exposure UBO (sun-screen params folded into ExposureBlock — 0 push)
	// typed RAL siblings.
	struct ralPipelineLayout_s *ral_pipeline_layout_ssao;
	struct ralPipelineLayout_s *ral_pipeline_layout_sunrays;
	// capture / bloom_extract / blur / bloom_blend are dynamic-rendering only (ral_* siblings).

	// Dynamic-rendering SMAA pipelines. Recreated on r_smaa / r_smaa_threshold
	// rebake so their cvar-baked specialization constants stay in sync.
	struct ralPipeline_s *ral_smaa_edge_pipeline;
	struct ralPipeline_s *ral_smaa_blend_pipeline;
	struct ralPipeline_s *ral_smaa_resolve_pipeline;

	uint32_t frame_count;
	qboolean active;
	qboolean wideLines;
	qboolean samplerAnisotropy;
	qboolean fragmentStores;
	qboolean depthClampSupported;   // RAL caps: device VkPhysicalDeviceFeatures.depthClamp enabled.
	                                // qfalse → no native rasterizer depth-clamp → the
	                                // R_SetupProjectionZ near-plane-shrink fallback runs instead.
	qboolean dedicatedAllocation;

	float maxAnisotropy;
	float maxLod;

	VkFormat color_format;
	VkFormat capture_format;
	VkFormat depth_format;
	VkFormat bloom_format;

	// Per-GPU BCn texture format support. Populated at device-init time through
	// exact sampled+filterable RAL format-feature queries. DDS
	// loader queries this to fail fast when an asset uses a format
	// the local GPU rejects (BC1-BC5 are near-universal on desktop;
	// BC7 + BC6H wider on modern hardware; ARM/MoltenVK variable).
	struct {
		qboolean bc1_unorm;
		qboolean bc1_srgb;
		qboolean bc2_unorm;
		qboolean bc2_srgb;
		qboolean bc3_unorm;
		qboolean bc3_srgb;
		qboolean bc4_unorm;
		qboolean bc4_snorm;
		qboolean bc5_unorm;
		qboolean bc5_snorm;
		qboolean bc6h_ufloat;
		qboolean bc6h_sfloat;
		qboolean bc7_unorm;
		qboolean bc7_srgb;
	} bc_formats_supported;

	VkImageLayout initSwapchainLayout;

	qboolean clearAttachment;		// requires VK_IMAGE_USAGE_TRANSFER_DST_BIT for swapchains
	qboolean fboActive;
	qboolean msaaActive;

	qboolean offscreenRender;

	qboolean windowAdjusted;
	int		blitX0;
	int		blitY0;
	int		blitFilter;

	uint32_t renderWidth;
	uint32_t renderHeight;

	float renderScaleX;
	float renderScaleY;

	renderPass_t renderPassIndex;

	uint32_t screenMapWidth;
	uint32_t screenMapHeight;
	uint32_t screenMapSamples;

	uint32_t maxBoundDescriptorSets;

#ifdef USE_UPLOAD_QUEUE
	VkFence aux_fence;
	struct ralFence_s *ral_aux_fence;  // adopted sibling fed into ralSubmitInfo_t.signalFence at vk_flush_staging_buffer's submit + waited via Ral_WaitFence in vk_wait_staging_buffer
	qboolean aux_fence_wait;
#endif

	struct staging_buffer_s {
		struct ralBuffer_s *ral_buffer;
		VkDeviceSize size;
#ifdef USE_UPLOAD_QUEUE
		VkDeviceSize offset;
#endif
	} staging_buffer;

	struct samplers_s {
		int count;
		Vk_Sampler_Def def[MAX_VK_SAMPLERS];
		VkSampler handle[MAX_VK_SAMPLERS];
		struct ralSampler_s *ral_handle[MAX_VK_SAMPLERS];
		int filter_min;
		int filter_max;
	} samplers;

	// useBindlessMainPath field retired.
	// Bindless is the sole renderervk main path post-retire — caps are checked
	// at vk_initialize and a recoverable init failure declines the renderer
	// (R_DeclineInit) so cl_main advances cl_renderer to the next fallback.
	// Past the cap-decline gate, every former useBindlessMainPath==true branch
	// runs unconditionally.

	struct defaults_t {
		VkDeviceSize staging_size;
		VkDeviceSize geometry_size;
	} defaults;

	char driverNote[200];

} Vk_Instance;

// Vk_World contains vulkan resources/state requested by the game code.
// It is reinitialized on a map change.
typedef struct {
	//
	// State.
	//

	// Descriptor sets corresponding to bound texture images.
	//VkDescriptorSet current_descriptor_sets[ MAX_TEXTURE_UNITS ];

	// This flag is used to decide whether framebuffer's depth attachment should be cleared
	// with vmCmdClearAttachment (dirty_depth_attachment != 0), or it have just been
	// cleared by render pass instance clear op (dirty_depth_attachment == 0).
	int dirty_depth_attachment;

	float modelview_transform[16];

	// Latest MVP as computed/pushed by vk_update_mvp — stashed so the per-draw
	// UBO ring fill (VK_PushUniform) can copy it into vkUniform_t.mvp for the
	// main-path vertex shaders that read MVP from the UBO instead of push. Same
	// value as the pushed constant (byte-for-byte); the push stays for the other
	// vertex shaders until they migrate.
	float mvp[16];
	// Always present and zero after renderer re-init. Feature-OFF builds still
	// stamp these zeros into every draw so shader-side runtime gating is
	// deterministic and the cross-backend UBO ABI never changes.
	vec4_t advancedFogColorDensity;
	vec4_t advancedFogTypeFarEnabled;
} Vk_World;

extern Vk_Instance	vk;				// shouldn't be cleared during ref re-init
extern Vk_World		vk_world;		// this data is cleared during ref re-init

void vk_request_presentation_change( const refPresentationChange_t *change );
void vk_apply_pending_presentation_change( void );
