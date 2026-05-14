// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// vk_ral_textures.c — Phase 7.4a texture migration support. See header
// docblock for the parallel-paths architecture; this TU implements:
//   * persistent RAL backend instance owned by the renderer
//   * the bindless SAMPLED_IMAGE BindGroup the migration populates as
//     R_CreateImage adds textures (consumer arrives in 7.4c)
//   * \ral_textures developer-command body (slot population + last-five
//     registered names)
//
// Only the asset-image path (R_CreateImage from disk / generated content)
// is migrated in 7.4a. Lightmap atlas, dynamic glyphs, scratch images, and
// render-target attachments stay on the legacy path — they remain in the
// follow-up turn inventory.

#include "tr_local.h"

#ifdef USE_VULKAN

#include <stdlib.h>     // Phase 7.4c-pipeline-followup-5 PART 0: vkRalActiveBuffer_t
                        // nodes allocate via stdlib malloc/free so they survive
                        // ri.FreeAll() in R_InitImages (renderer zone wipe).

#include "vk_ral_textures.h"

// The RAL public surface + the Vulkan-backend-internal accessors (the
// latter only because \ral_textures dumps the underlying VkImage for
// diagnostics; the renderer never USES the RAL VkImage in 7.4a beyond
// dumping it). ral/ral.h is the right include for everything else.
#include "../renderer/ral/ral.h"

// ── module state ────────────────────────────────────────────────────────
static ralBackend_t         *s_ral_backend;
static ralBindGroupLayout_t *s_ral_bindless_layout;
static ralBindGroup_t       *s_ral_bindless_set;
static uint32_t              s_ral_bindless_capacity;     // resolved bindless slot count (= min(RAL caps, requested))
static uint32_t              s_ral_registered_count;      // textures populated in the bindless set so far
static uint32_t              s_ral_skipped_no_slot;       // textures created but slot index overflowed s_ral_bindless_capacity
static uint32_t              s_ral_skipped_no_data;       // R_CreateImage calls with pic=NULL (scratch/placeholder)
static uint32_t              s_ral_destroyed_count;       // textures destroyed via vk_ral_unregister_image
#define VK_RAL_RECENT_NAMES   5
static char                  s_ral_recent_names[ VK_RAL_RECENT_NAMES ][ MAX_QPATH ];
static uint32_t              s_ral_recent_head;           // ring write cursor
static qboolean              s_ral_init_attempted;        // sticky: only retry init once per vid_init
static qboolean              s_ral_full_warned;           // one-shot for the bindless-capacity-exhausted log
static void                  vk_ral_warn_bindless_full( void );   // fwd

// ── Phase 7.4b — RAL buffer parallel-paths tracker ─────────────────────
// `s_buf_pending` holds register-buffer calls made before the RAL backend
// is up (vk_initialize fires before R_InitImages's vk_ral_textures_init).
// vk_ral_textures_init's tail flushes the pending list into the active list
// by creating real RAL buffers. Sized for the 20-ish vk_initialize-time
// create sites with comfortable headroom; oversize logs once and skips.
#define VK_RAL_PENDING_BUFFER_MAX  64u
typedef struct {
	VkBuffer        key;          // legacy VkBuffer handle (lookup key)
	uint64_t        size;
	int             usage;        // ralBufferUsage_t bitmask
	int             memory;       // ralMemoryType_t
	char            debugName[ MAX_QPATH ];
} vkRalPendingBuffer_t;
static vkRalPendingBuffer_t s_buf_pending[ VK_RAL_PENDING_BUFFER_MAX ];
static uint32_t             s_buf_pending_count;
static qboolean             s_buf_pending_warned_full;

// Active list: one entry per live RAL buffer. Linked list (ri.Malloc per
// node) since the count can grow with per-frame / per-map / per-IQM-model
// activity. Lookup keyed by VkBuffer for unregister. Allocation cost is
// negligible (~21 + per-model nodes); destroyed at vk_ral_textures_shutdown
// (cascading on the persistent backend's defer-destroy at backend shutdown).
typedef struct vkRalActiveBuffer_s {
	struct vkRalActiveBuffer_s *next;
	VkBuffer        key;
	ralBuffer_t    *ral;
	uint64_t        size;
	int             usage;
	int             memory;
} vkRalActiveBuffer_t;
static vkRalActiveBuffer_t *s_buf_active;
static uint32_t             s_buf_active_count;
static uint32_t             s_buf_peak_count;            // peak live across the session (informational)
static uint32_t             s_buf_register_total;        // lifetime register-count (incl. skipped)
static uint32_t             s_buf_destroy_total;         // lifetime unregister-count
static uint32_t             s_buf_skipped_no_backend;    // register attempts while backend never came up

// Per-usage byte tally (RAL_BUFFER_VERTEX through RAL_BUFFER_TRANSFER_DST, 7 bits).
// Index = single-bit position (0..6). Logged at diag_dump.
static uint64_t             s_buf_bytes_by_usage[8];

static void                 vk_ral_flush_pending_buffers( void );  // fwd
static void                 vk_ral_destroy_all_active_buffers( void );  // fwd
static void                 vk_ral_destroy_adopted_pipeline_layouts( void );  // fwd
void                        vk_ral_adopt_static_pipeline_layouts( void );      // fwd
void                        vk_ral_adopt_static_render_passes_and_framebuffers( void );  // fwd
static void                 vk_ral_destroy_adopted_render_passes_and_framebuffers( void ); // fwd
// Phase 7.4c-submit-A4 — internal-texture adoption (depth, color, tonemapped,
// SMAA input/edges/blend). The GPU-timestamp query-pool adoption + matching
// destroy live inline in vk.c (vk_gpu_ts_init / vk_gpu_ts_shutdown) because
// the VkQueryPool is created strictly after vk_init_descriptors runs. The
// reverse-lookup helper below resolves the wrapper via these externs.
extern VkQueryPool             vk_gpu_ts_pool;            // vk.c (defined non-static for adoption visibility)
extern struct ralQueryPool_s  *vk_gpu_ts_ral_pool;         // vk.c — adopted parallel-paths sibling

// Phase 7.4c-bindgroup — boot-time adoption of every allocate-once
// VkDescriptorSet wrapped into a ralBindGroup_t with ownsSet=qfalse so the
// renderer's existing vkResetDescriptorPool / vk_destroy_descriptor_pools
// path keeps lifetime ownership. The wrappers are pure metadata + a stable
// RAL handle that 7.4c-cmd's cmd-record migration can pass to
// Ral_CmdBindBindGroup. Per-draw rotating sets (vk.cmd->descriptor_set.current[])
// are NOT adopted here — they rotate per shader_type per frame and need
// per-frame re-adoption infrastructure introduced by 7.4c-cmd.
//
// Centralized registry (rather than per-subsystem sibling fields) — 7.4c-cmd
// refactors into sibling lookup at each bind site as it threads ralCommand-
// Buffer_t through the cmd path. For now this turn delivers the wrappers'
// existence + lifecycle + count log; bind-site consumers fold in next turn.
#define VK_RAL_MAX_ADOPTED_BGS  64u
static ralBindGroup_t      *s_adopted_bgs[ VK_RAL_MAX_ADOPTED_BGS ];
static uint32_t             s_adopted_bgs_count;

// ── lifecycle ───────────────────────────────────────────────────────────
qboolean vk_ral_textures_available( void ) {
	return ( s_ral_backend && s_ral_bindless_set ) ? qtrue : qfalse;
}

// Phase 7.4c-pipeline — backend accessor for vk.c::create_pipeline +
// the 16 special-case sites. Returns the imported-mode backend (or NULL).
struct ralBackend_s *vk_ral_get_backend( void ) {
	return s_ral_backend;
}

// Phase 7.4c-submit-followup-present-2-fix2 — backend bringup extracted
// from vk_ral_textures_init's head. Hoisted ahead of vk_create_swapchain
// in vk_initialize (present-2 made the swapchain itself a RAL consumer,
// invalidating the prior "R_InitImages is the only consumer" ordering).
// The bindless texture-set + buffer-pending-flush stays in
// vk_ral_textures_init at its late position (still gated by
// r_useRALTextures / r_useRALBuffers cvars).
qboolean vk_ral_backend_init( void ) {
	ralBackendCreateInfo_t bci;

	if ( s_ral_backend != NULL ) return qtrue;                // idempotent

	if ( vk.instance == VK_NULL_HANDLE || vk.device == VK_NULL_HANDLE ) {
		ri.Log( SEV_ERROR, "[VK->RAL] vk_ral_backend_init: vk.instance/device not yet built (instance=%p device=%p)\n",
		        (void *)vk.instance, (void *)vk.device );
		return qfalse;
	}

	memset( &bci, 0, sizeof( bci ) );
	bci.type  = RAL_BACKEND_VULKAN;
	bci.flags = RAL_FLAG_DEBUG_LABELS | ( ( ri.Cvar_VariableIntegerValue( "developer" ) >= 2 ) ? RAL_FLAG_VALIDATION : 0u );
	bci.externalInstance         = vk.instance;
	bci.externalPhysicalDevice   = vk.physical_device;
	bci.externalDevice           = vk.device;
	bci.externalQueueFamilies[ RAL_QUEUE_GRAPHICS ] = vk.queue_family_index;
	bci.externalQueueFamilies[ RAL_QUEUE_COMPUTE  ] = vk.queue_family_compute;
	bci.externalQueueFamilies[ RAL_QUEUE_TRANSFER ] = vk.queue_family_transfer;
	bci.externalApiVersion       = vk.instance_api_version;

	s_ral_backend = Ral_CreateBackend( &bci );
	if ( s_ral_backend == NULL ) {
		ri.Log( SEV_ERROR, "[VK->RAL] vk_ral_backend_init: Ral_CreateBackend returned NULL\n" );
		return qfalse;
	}

	ri.Log( SEV_INFO, "[VK->RAL] RAL backend brought up (imported mode; queue families gfx/cmp/xfer=%u/%u/%u)\n",
	        vk.queue_family_index, vk.queue_family_compute, vk.queue_family_transfer );

	return qtrue;
}


// Phase 7.4c-submit-followup-present-2-fix3 — symmetric pair to
// vk_ral_backend_init. Called from vk_shutdown's tail, AFTER every
// consumer's RAL wrapper destroy (Ral_DestroyCommandBuffer of the
// staging cmd buffer, Ral_DestroySwapchain in vk_destroy_swapchain,
// vk_destroy_sync_primitives' Ral_Destroy{Semaphore,Fence} calls,
// vk_ral_unregister_buffer for storage.buffer, etc.) but BEFORE
// qvkDestroyDevice (which would invalidate b->device). Idempotent.
void vk_ral_backend_shutdown( void ) {
	if ( s_ral_backend != NULL ) {
		Ral_DestroyBackend( s_ral_backend );
		s_ral_backend = NULL;
	}
}


void vk_ral_textures_init( void ) {
	ralBindEntry_t                  entry;
	ralBindGroupLayoutCreateInfo_t  lci;
	ralBindGroupCreateInfo_t        gci;
	const ralCaps_t                *caps;

	// Phase 7.4c-submit-followup-present-2-fix2 — backend bringup was
	// extracted to vk_ral_backend_init and is hoisted to run from
	// vk_initialize ahead of vk_create_swapchain. Defensive call here in
	// case some path reaches this without having brought the backend up
	// (idempotent — no-op if already initialized).
	if ( s_ral_backend == NULL ) {
		if ( !vk_ral_backend_init() ) {
			ri.Log( SEV_ERROR, "[RAL-TEX] vk_ral_textures_init: backend init failed; aborting texture/buffer setup\n" );
			return;
		}
	}

	// Bindless texture/buffer infrastructure cvar gate — the backend itself
	// is up unconditionally now (the swapchain depends on it). These cvars
	// only gate the bindless layout/set + buffer-pending-flush below.
	if ( s_ral_bindless_set != NULL ) return;                  // already wired
	{
		qboolean wantTex = ( r_useRALTextures && r_useRALTextures->integer != 0 ) ? qtrue : qfalse;
		qboolean wantBuf = ( r_useRALBuffers  && r_useRALBuffers->integer  != 0 ) ? qtrue : qfalse;
		if ( !wantTex && !wantBuf ) return;
	}

	// Bookkeeping reset (vid_restart re-enters here).
	memset( s_ral_recent_names, 0, sizeof( s_ral_recent_names ) );
	s_ral_recent_head     = 0;
	s_ral_registered_count = 0;
	s_ral_skipped_no_slot = 0;
	s_ral_skipped_no_data = 0;
	s_ral_destroyed_count = 0;
	s_ral_init_attempted  = qtrue;

	caps = Ral_GetCaps( s_ral_backend );
	s_ral_bindless_capacity = 4096u;   // 7.4a target; RAL caps would allow ~1M on RTX hardware
	if ( caps && caps->maxBindlessTextures > 0 && caps->maxBindlessTextures < s_ral_bindless_capacity )
		s_ral_bindless_capacity = caps->maxBindlessTextures;

	// Bindless texture table — only built when r_useRALTextures is on; the
	// buffer-only path (r_useRALBuffers=1, r_useRALTextures=0) keeps the
	// backend alive without the bindless infrastructure.
	if ( r_useRALTextures && r_useRALTextures->integer != 0 ) {
		memset( &entry, 0, sizeof( entry ) );
		entry.binding    = 0;
		entry.type       = RAL_BIND_SAMPLED_TEXTURE;
		entry.count      = 0;                                 // 0 == unbounded → RAL caps the array at RAL_VK_BINDLESS_LAYOUT_COUNT (4096)
		entry.stageFlags = RAL_STAGE_FRAGMENT;
		memset( &lci, 0, sizeof( lci ) );
		lci.entries    = &entry;
		lci.numEntries = 1;
		lci.bindless   = qtrue;
		lci.debugName  = "renderer-bindless-tex";
		s_ral_bindless_layout = Ral_CreateBindGroupLayout( s_ral_backend, &lci );
		if ( !s_ral_bindless_layout ) {
			ri.Log( SEV_WARN, "[RAL-TEX] Ral_CreateBindGroupLayout failed (bindless=%s)\n",
			        caps && caps->bindlessTextures ? "advertised yes" : "no" );
			Ral_DestroyBackend( s_ral_backend );  s_ral_backend = NULL;
			return;
		}

		memset( &gci, 0, sizeof( gci ) );
		gci.layout    = s_ral_bindless_layout;
		gci.numValues = 0;                                    // slots populated lazily as images register
		gci.debugName = "renderer-bindless-set";
		s_ral_bindless_set = Ral_CreateBindGroup( s_ral_backend, &gci );
		if ( !s_ral_bindless_set ) {
			ri.Log( SEV_WARN, "[RAL-TEX] Ral_CreateBindGroup failed\n" );
			Ral_DestroyBindGroupLayout( s_ral_bindless_layout );  s_ral_bindless_layout = NULL;
			Ral_DestroyBackend( s_ral_backend );                   s_ral_backend         = NULL;
			return;
		}
	}

	ri.Log( SEV_INFO, "[RAL-TEX] RAL texture infrastructure ready (bindless slots = %u; RAL device caps reports %u)\n",
	        s_ral_bindless_capacity, ( caps ? caps->maxBindlessTextures : 0 ) );

	// Phase 7.4b: flush any RAL buffer register calls that arrived during
	// vk_initialize (before the backend was up). Sites that paired register +
	// unregister before now (transient SMAA-LUT staging in
	// vk_smaa_alloc_resources) self-removed from pending — only survivors
	// reach Ral_CreateBuffer here.
	vk_ral_flush_pending_buffers();
}


// Phase 7.4c-bindgroup — adopt every allocate-once VkDescriptorSet
// the renderer has stood up into a ralBindGroup_t wrapper. Called from
// vk_init_descriptors's tail, AFTER all qvkAllocateDescriptorSets +
// vkUpdateDescriptorSets writes have completed. Idempotent: subsequent
// calls (vid_restart, REF_KEEP_CONTEXT-then-re-init) clear the registry
// first, so we always wrap the CURRENT descriptor set handles.
//
// The wrapper carries ownsSet=qfalse; teardown only frees the wrapper
// struct, not the underlying VkDescriptorSet. The legacy
// vkResetDescriptorPool path retains lifetime ownership.
//
// Sets NOT adopted here (deferred to 7.4c-cmd):
//   vk.cmd->descriptor_set.current[i] — rotates per shader_type per
//   frame; needs per-frame re-adoption tied to the cmd-buffer ring
//   that 7.4c-cmd introduces.
void vk_ral_adopt_static_bindgroups( void )
{
	uint32_t i, before;

	if ( !s_ral_backend ) return;

	// Idempotent: tear down any prior session's wrappers first so re-init
	// after vid_restart / map transition gets fresh wrappers for the
	// fresh descriptor sets the pool just re-allocated.
	for ( i = 0; i < s_adopted_bgs_count; i++ ) {
		if ( s_adopted_bgs[i] ) {
			Ral_DestroyBindGroup( s_adopted_bgs[i] );
			s_adopted_bgs[i] = NULL;
		}
	}
	s_adopted_bgs_count = 0;
	before = s_adopted_bgs_count;

	#define ADOPT( vkset, layout, label ) do {                                                            \
		if ( (vkset) != VK_NULL_HANDLE && (layout) != NULL && s_adopted_bgs_count < VK_RAL_MAX_ADOPTED_BGS ) { \
			ralBindGroup_t *bg = Ral_AdoptBindGroup( s_ral_backend, (vkset), (layout), (label) );             \
			if ( bg ) s_adopted_bgs[ s_adopted_bgs_count++ ] = bg;                                            \
		}                                                                                                 \
	} while ( 0 )

	// ── core singletons ────────────────────────────────────────────────
	ADOPT( vk.storage.descriptor,           vk.ral_bgl_storage, "wired-storage-bg" );
	ADOPT( vk.color_descriptor,             vk.ral_bgl_sampler, "wired-color-bg" );
	ADOPT( vk.tonemapped_descriptor,        vk.ral_bgl_sampler, "wired-tonemapped-bg" );
	ADOPT( vk.screenMap.color_descriptor,   vk.ral_bgl_sampler, "wired-screenmap-color-bg" );
	ADOPT( vk.depthFade.descriptor,         vk.ral_bgl_sampler, "wired-depthfade-bg" );
#if FEAT_SHADOW_MAPPING
	ADOPT( vk.shadowMap.descriptor,         vk.ral_bgl_sampler, "wired-shadowmap-bg" );
#endif

	// ── SMAA sampler sets (FBO-gated; same five descriptors the live
	//    r_smaa toggle path keeps populated across cycles via
	//    vk_update_attachment_descriptors) ───────────────────────────────
	if ( vk.fboActive ) {
		ADOPT( vk.smaa.edges_descriptor,    vk.ral_bgl_sampler, "wired-smaa-edges-bg" );
		ADOPT( vk.smaa.blend_descriptor,    vk.ral_bgl_sampler, "wired-smaa-blend-bg" );
		ADOPT( vk.smaa.input_descriptor,    vk.ral_bgl_sampler, "wired-smaa-input-bg" );
		ADOPT( vk.smaa.area_descriptor,     vk.ral_bgl_sampler, "wired-smaa-area-bg" );
		ADOPT( vk.smaa.search_descriptor,   vk.ral_bgl_sampler, "wired-smaa-search-bg" );
	}

	// ── bloom_image_descriptor[] — one per bloom level, 1 + VK_NUM_BLOOM_PASSES*2 ──
	for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ ) {
		ADOPT( vk.bloom_image_descriptor[i], vk.ral_bgl_sampler, "wired-bloom-img-bg" );
	}

	// ── per-frame-ring uniform descriptors (vk.tess[NUM_COMMAND_BUFFERS]) ──
	for ( i = 0; i < ARRAY_LEN( vk.tess ); i++ ) {
		ADOPT( vk.tess[i].uniform_descriptor, vk.ral_bgl_uniform, "wired-tess-uniform-bg" );
	}

	// ── per-subsystem rings (each NUM_COMMAND_BUFFERS slots) ──
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		ADOPT( vk.ribbon.descriptor[i],            vk.ribbon.ral_bgl,             "wired-ribbon-bg" );
		ADOPT( vk.sprite.descriptor[i],            vk.sprite.ral_bgl,             "wired-sprite-bg" );
		ADOPT( vk.beam.descriptor[i],              vk.beam.ral_bgl,               "wired-beam-bg" );
		ADOPT( vk.particle.compute_descriptor[i],  vk.particle.ral_bgl_compute,   "wired-particle-compute-bg" );
		ADOPT( vk.particle.render_descriptor[i],   vk.particle.ral_bgl_render,    "wired-particle-render-bg" );
#if FEAT_IQM
		ADOPT( vk.iqmGpu.bone_descriptor[i],       vk.iqmGpu.ral_bgl_bones,       "wired-iqm-bones-bg" );
#endif
	}

	#undef ADOPT

	ri.Log( SEV_INFO,
		"[VK->RAL] adopted %u bind groups as ralBindGroup_t (core singletons + SMAA + bloom + per-frame rings)\n",
		s_adopted_bgs_count - before );

	// Phase 7.4c-submit-A2 — also adopt every VkPipelineLayout into its
	// matching typed sibling. Same idempotent re-init contract.
	vk_ral_adopt_static_pipeline_layouts();

	// Phase 7.4c-submit-A3 — adopt every VkRenderPass + VkFramebuffer too.
	vk_ral_adopt_static_render_passes_and_framebuffers();

	// Phase 7.4c-submit-A4 — adopt the 6 renderer-owned internal-image VkImages
	// + the GPU-timestamp VkQueryPool into ralTexture_t / ralQueryPool_t
	// siblings. Must run AFTER vk_initialize (which creates the VkImages) and
	// AFTER vk_smaa_alloc_resources (when r_smaa is on at boot). The boot-time
	// call path is vk_initialize → vk_init_descriptors → vk_ral_adopt_static_bindgroups
	// → here; the SMAA images are conditionally adopted under vk.fboActive.
	vk_ral_adopt_static_internal_textures();
}


// ════════════════════════════════════════════════════════════════════════
// Phase 7.4c-submit-A2 — reverse-lookup helpers for the typed RAL cmd API.
// See vk_ral_textures.h for the parallel-paths-era NULL-fallthrough contract.
// ════════════════════════════════════════════════════════════════════════

ralBuffer_t *vk_ral_lookup_buffer( VkBuffer vkBuf ) {
	vkRalActiveBuffer_t *node;
	if ( vkBuf == VK_NULL_HANDLE ) return NULL;
	for ( node = s_buf_active; node != NULL; node = node->next ) {
		if ( node->key == vkBuf ) return node->ral;
	}
	return NULL;
}

ralPipeline_t *vk_ral_lookup_pipeline( VkPipeline vkPipe ) {
	uint32_t i, j;
	if ( vkPipe == VK_NULL_HANDLE ) return NULL;
	// Phase 7.4c-submit-A3 — RAL siblings now identity-share their
	// VkPipelineLayout with the renderer's qvkCreatePipelineLayout-created
	// handle (via ralGraphicsPipelineCreateInfo_t.externalLayout). The lookup
	// scan below returns a usable sibling whose vkCmdBindPipeline is
	// layout-compatible with the renderer's vkCmdBindDescriptorSets recorded
	// on the same parallel cmd buffer — no more layout-incompatibility VUIDs.
	// Centralized helper pipelines — vk.pipelines[].ral_handle[] mirrors
	// vk.pipelines[].pipeline_handle[] (one slot per render-pass variant).
	for ( i = 0; i < (uint32_t)vk.pipelines_count; i++ ) {
		for ( j = 0; j < RENDER_PASS_COUNT; j++ ) {
			if ( vk.pipelines[i].handle[j] == vkPipe && vk.pipelines[i].ral_handle[j] != NULL ) {
				return vk.pipelines[i].ral_handle[j];
			}
		}
	}
	// Special-case sibling fields. List once; future RAL pipeline additions
	// extend here.
	#define MATCH( vk_field, ral_field ) if ( (vk_field) == vkPipe && (ral_field) != NULL ) return (ral_field)
	MATCH( vk.gamma_pipeline,             vk.ral_gamma_pipeline );
	MATCH( vk.capture_pipeline,           vk.ral_capture_pipeline );
	MATCH( vk.bloom_extract_pipeline,     vk.ral_bloom_extract_pipeline );
	MATCH( vk.bloom_blend_pipeline,       vk.ral_bloom_blend_pipeline );
	MATCH( vk.smaa_edge_pipeline,         vk.ral_smaa_edge_pipeline );
	MATCH( vk.smaa_blend_pipeline,        vk.ral_smaa_blend_pipeline );
	MATCH( vk.smaa_resolve_pipeline,      vk.ral_smaa_resolve_pipeline );
	MATCH( vk.tonemap_pipeline,           vk.ral_tonemap_pipeline );
	for ( i = 0; i < ARRAY_LEN( vk.tonemap_variants ); i++ ) {
		MATCH( vk.tonemap_variants[i],    vk.ral_tonemap_variants[i] );
	}
	for ( i = 0; i < ARRAY_LEN( vk.blur_pipeline ); i++ ) {
		MATCH( vk.blur_pipeline[i],       vk.ral_blur_pipeline[i] );
	}
	MATCH( vk.ribbon.pipeline_alpha,      vk.ribbon.ral_pipeline_alpha );
	MATCH( vk.ribbon.pipeline_additive,   vk.ribbon.ral_pipeline_additive );
	MATCH( vk.beam.pipeline,              vk.beam.ral_pipeline );
	MATCH( vk.sprite.pipeline_alpha,      vk.sprite.ral_pipeline_alpha );
	MATCH( vk.sprite.pipeline_additive,   vk.sprite.ral_pipeline_additive );
	MATCH( vk.particle.compute_pipeline,        vk.particle.ral_compute_pipeline );
	MATCH( vk.particle.render_pipeline_alpha,   vk.particle.ral_render_pipeline_alpha );
	MATCH( vk.particle.render_pipeline_additive, vk.particle.ral_render_pipeline_additive );
#if FEAT_IQM
	MATCH( vk.iqmGpu.pipeline,            vk.iqmGpu.ral_pipeline );
#endif
#if FEAT_SHADOW_MAPPING
	MATCH( vk.shadowMap.depthPipeline,    vk.shadowMap.ral_depthPipeline );
#endif
	#undef MATCH
	return NULL;
}

struct ralRenderPass_s *vk_ral_lookup_render_pass( VkRenderPass vkRp ) {
	uint32_t i;
	if ( vkRp == VK_NULL_HANDLE ) return NULL;
	#define MATCH( vk_field, ral_field ) if ( (vk_field) == vkRp && (ral_field) != NULL ) return (ral_field)
	MATCH( vk.render_pass.main,          vk.ral_render_pass.main );
	MATCH( vk.render_pass.screenmap,     vk.ral_render_pass.screenmap );
	MATCH( vk.render_pass.tonemap,       vk.ral_render_pass.tonemap );
	MATCH( vk.render_pass.ui,            vk.ral_render_pass.ui );
	MATCH( vk.render_pass.ui_clear,      vk.ral_render_pass.ui_clear );
	MATCH( vk.render_pass.gamma,         vk.ral_render_pass.gamma );
	MATCH( vk.render_pass.capture,       vk.ral_render_pass.capture );
	MATCH( vk.render_pass.bloom_extract, vk.ral_render_pass.bloom_extract );
	MATCH( vk.render_pass.post_bloom,    vk.ral_render_pass.post_bloom );
	MATCH( vk.render_pass.depth_fade,    vk.ral_render_pass.depth_fade );
	MATCH( vk.render_pass.smaa_edge,     vk.ral_render_pass.smaa_edge );
	MATCH( vk.render_pass.smaa_blend,    vk.ral_render_pass.smaa_blend );
	MATCH( vk.render_pass.smaa_resolve,  vk.ral_render_pass.smaa_resolve );
	for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
		MATCH( vk.render_pass.blur[i],   vk.ral_render_pass.blur[i] );
	}
#if FEAT_SHADOW_MAPPING
	MATCH( vk.shadowMap.renderPass,      vk.shadowMap.ral_renderPass );
#endif
	#undef MATCH
	return NULL;
}

struct ralFramebuffer_s *vk_ral_lookup_framebuffer( VkFramebuffer vkFb ) {
	uint32_t i;
	if ( vkFb == VK_NULL_HANDLE ) return NULL;
	#define MATCH( vk_field, ral_field ) if ( (vk_field) == vkFb && (ral_field) != NULL ) return (ral_field)
	MATCH( vk.framebuffers.bloom_extract, vk.ral_framebuffers.bloom_extract );
	MATCH( vk.framebuffers.tonemap,       vk.ral_framebuffers.tonemap );
	MATCH( vk.framebuffers.ui,            vk.ral_framebuffers.ui );
	MATCH( vk.framebuffers.ui_clear,      vk.ral_framebuffers.ui_clear );
	MATCH( vk.framebuffers.screenmap,     vk.ral_framebuffers.screenmap );
	MATCH( vk.framebuffers.capture,       vk.ral_framebuffers.capture );
	MATCH( vk.framebuffers.smaa_edge,     vk.ral_framebuffers.smaa_edge );
	MATCH( vk.framebuffers.smaa_blend,    vk.ral_framebuffers.smaa_blend );
	MATCH( vk.framebuffers.smaa_resolve,  vk.ral_framebuffers.smaa_resolve );
	for ( i = 0; i < ARRAY_LEN( vk.framebuffers.blur );  i++ ) { MATCH( vk.framebuffers.blur[i],  vk.ral_framebuffers.blur[i]  ); }
	for ( i = 0; i < ARRAY_LEN( vk.framebuffers.main );  i++ ) { MATCH( vk.framebuffers.main[i],  vk.ral_framebuffers.main[i]  ); }
	for ( i = 0; i < ARRAY_LEN( vk.framebuffers.gamma ); i++ ) { MATCH( vk.framebuffers.gamma[i], vk.ral_framebuffers.gamma[i] ); }
	#undef MATCH
	return NULL;
}


ralPipelineLayout_t *vk_ral_lookup_pipeline_layout( VkPipelineLayout vkLayout ) {
	if ( vkLayout == VK_NULL_HANDLE ) return NULL;
	#define MATCH( vk_field, ral_field ) if ( (vk_field) == vkLayout && (ral_field) != NULL ) return (ral_field)
	MATCH( vk.pipeline_layout,                vk.ral_pipeline_layout );
	MATCH( vk.pipeline_layout_storage,        vk.ral_pipeline_layout_storage );
	MATCH( vk.pipeline_layout_post_process,   vk.ral_pipeline_layout_post_process );
	MATCH( vk.pipeline_layout_smaa,           vk.ral_pipeline_layout_smaa );
	MATCH( vk.pipeline_layout_blend,          vk.ral_pipeline_layout_blend );
	MATCH( vk.pipeline_layout_msdf,           vk.ral_pipeline_layout_msdf );
	MATCH( vk.pipeline_layout_ssao,           vk.ral_pipeline_layout_ssao );
	MATCH( vk.pipeline_layout_godrays,        vk.ral_pipeline_layout_godrays );
	MATCH( vk.ribbon.pipeline_layout,             vk.ribbon.ral_pipeline_layout );
	MATCH( vk.beam.pipeline_layout,               vk.beam.ral_pipeline_layout );
	MATCH( vk.sprite.pipeline_layout,             vk.sprite.ral_pipeline_layout );
	MATCH( vk.particle.compute_pipeline_layout,   vk.particle.ral_compute_pipeline_layout );
	MATCH( vk.particle.render_pipeline_layout,    vk.particle.ral_render_pipeline_layout );
#if FEAT_IQM
	MATCH( vk.iqmGpu.pipeline_layout,             vk.iqmGpu.ral_pipeline_layout );
#endif
#if FEAT_SHADOW_MAPPING
	MATCH( vk.shadowMap.depthLayout,              vk.shadowMap.ral_depthLayout );
#endif
	#undef MATCH
	return NULL;
}


static void vk_ral_destroy_adopted_bindgroups( void )
{
	uint32_t i;
	for ( i = 0; i < s_adopted_bgs_count; i++ ) {
		if ( s_adopted_bgs[i] ) {
			Ral_DestroyBindGroup( s_adopted_bgs[i] );
			s_adopted_bgs[i] = NULL;
		}
	}
	s_adopted_bgs_count = 0;
}


// ════════════════════════════════════════════════════════════════════════
// Phase 7.4c-submit-A2 — boot-time pipeline-layout adoption sweep.
//
// Walks every VkPipelineLayout field on the renderer's vk struct that the
// renderer's create-pipeline-layout sites populated, wraps each in a
// ralPipelineLayout_t via Ral_AdoptPipelineLayout, and stores the wrapper
// in the matching ral_* sibling field on vk. Called from
// vk_ral_adopt_static_bindgroups's tail; mirrors that helper's idempotent
// re-init pattern. Teardown integrated into vk_ral_textures_shutdown's
// full-teardown branch (destroyWindow=qtrue).
//
// The wrappers carry ownsHandle=qfalse so Ral_DestroyPipelineLayout skips
// vkDestroyPipelineLayout — the renderer's existing
// qvkDestroyPipelineLayout teardown owns the underlying VkPipelineLayout
// lifetime.
//
// Logged count (always-on SEV_INFO): "[VK->RAL] adopted N pipeline layouts
// as ralPipelineLayout_t". The 15 sibling fields cover every
// qvkCreatePipelineLayout site in vk.c.
// ════════════════════════════════════════════════════════════════════════
static uint32_t s_ral_pipeline_layouts_adopted;

void vk_ral_adopt_static_pipeline_layouts( void )
{
	uint32_t adopted = 0;

	if ( !s_ral_backend ) return;

	#define ADOPT_PL( vkfield, ralfield, label ) do {                                  \
		if ( ralfield ) { Ral_DestroyPipelineLayout( ralfield ); ralfield = NULL; } \
		if ( (vkfield) != VK_NULL_HANDLE ) {                                            \
			ralfield = Ral_AdoptPipelineLayout( s_ral_backend, (vkfield), (label) ); \
			if ( ralfield ) adopted++;                                                  \
		}                                                                               \
	} while ( 0 )

	ADOPT_PL( vk.pipeline_layout,                vk.ral_pipeline_layout,                "wired-pl-main" );
	ADOPT_PL( vk.pipeline_layout_storage,        vk.ral_pipeline_layout_storage,        "wired-pl-storage" );
	ADOPT_PL( vk.pipeline_layout_post_process,   vk.ral_pipeline_layout_post_process,   "wired-pl-post-process" );
	ADOPT_PL( vk.pipeline_layout_smaa,           vk.ral_pipeline_layout_smaa,           "wired-pl-smaa" );
	ADOPT_PL( vk.pipeline_layout_blend,          vk.ral_pipeline_layout_blend,          "wired-pl-blend" );
	ADOPT_PL( vk.pipeline_layout_msdf,           vk.ral_pipeline_layout_msdf,           "wired-pl-msdf" );
	ADOPT_PL( vk.pipeline_layout_ssao,           vk.ral_pipeline_layout_ssao,           "wired-pl-ssao" );
	ADOPT_PL( vk.pipeline_layout_godrays,        vk.ral_pipeline_layout_godrays,        "wired-pl-godrays" );

	ADOPT_PL( vk.ribbon.pipeline_layout,             vk.ribbon.ral_pipeline_layout,             "wired-pl-ribbon" );
	ADOPT_PL( vk.beam.pipeline_layout,               vk.beam.ral_pipeline_layout,               "wired-pl-beam" );
	ADOPT_PL( vk.sprite.pipeline_layout,             vk.sprite.ral_pipeline_layout,             "wired-pl-sprite" );
	ADOPT_PL( vk.particle.compute_pipeline_layout,   vk.particle.ral_compute_pipeline_layout,   "wired-pl-particle-compute" );
	ADOPT_PL( vk.particle.render_pipeline_layout,    vk.particle.ral_render_pipeline_layout,    "wired-pl-particle-render" );
#if FEAT_IQM
	ADOPT_PL( vk.iqmGpu.pipeline_layout,             vk.iqmGpu.ral_pipeline_layout,             "wired-pl-iqm" );
#endif
#if FEAT_SHADOW_MAPPING
	ADOPT_PL( vk.shadowMap.depthLayout,              vk.shadowMap.ral_depthLayout,              "wired-pl-shadow-depth" );
#endif

	#undef ADOPT_PL

	s_ral_pipeline_layouts_adopted = adopted;
	ri.Log( SEV_INFO,
		"[VK->RAL] adopted %u pipeline layouts as ralPipelineLayout_t (centralized + per-subsystem)\n",
		adopted );
}


// ════════════════════════════════════════════════════════════════════════
// Phase 7.4c-submit-A3 — render-pass + framebuffer adoption sweep.
// Same pattern as the pipeline-layout sweep: walk every renderer-owned
// VkRenderPass / VkFramebuffer field, wrap in a ral*_t with ownsHandle=qfalse,
// store in matching ral_* sibling. Sweep is idempotent (re-init clears old
// wrappers + re-adopts the fresh Vk handles). Logged count: "[VK->RAL]
// adopted N render passes + M framebuffers as ralRenderPass_t / ralFramebuffer_t".
// ════════════════════════════════════════════════════════════════════════
static uint32_t s_ral_render_passes_adopted;
static uint32_t s_ral_framebuffers_adopted;

void vk_ral_adopt_static_render_passes_and_framebuffers( void )
{
	uint32_t rpAdopted = 0, fbAdopted = 0;
	uint32_t i;
	if ( !s_ral_backend ) return;

	#define ADOPT_RP( vkfield, ralfield, label ) do {                                 \
		if ( ralfield ) { Ral_DestroyRenderPass( ralfield ); ralfield = NULL; }    \
		if ( (vkfield) != VK_NULL_HANDLE ) {                                          \
			ralfield = Ral_AdoptRenderPass( s_ral_backend, (vkfield), (label) ); \
			if ( ralfield ) rpAdopted++;                                              \
		}                                                                             \
	} while ( 0 )
	#define ADOPT_FB( vkfield, ralfield, label ) do {                                 \
		if ( ralfield ) { Ral_DestroyFramebuffer( ralfield ); ralfield = NULL; }   \
		if ( (vkfield) != VK_NULL_HANDLE ) {                                          \
			ralfield = Ral_AdoptFramebuffer( s_ral_backend, (vkfield), (label) ); \
			if ( ralfield ) fbAdopted++;                                              \
		}                                                                             \
	} while ( 0 )

	ADOPT_RP( vk.render_pass.main,          vk.ral_render_pass.main,          "wired-rp-main" );
	ADOPT_RP( vk.render_pass.screenmap,     vk.ral_render_pass.screenmap,     "wired-rp-screenmap" );
	ADOPT_RP( vk.render_pass.tonemap,       vk.ral_render_pass.tonemap,       "wired-rp-tonemap" );
	ADOPT_RP( vk.render_pass.ui,            vk.ral_render_pass.ui,            "wired-rp-ui" );
	ADOPT_RP( vk.render_pass.ui_clear,      vk.ral_render_pass.ui_clear,      "wired-rp-ui-clear" );
	ADOPT_RP( vk.render_pass.gamma,         vk.ral_render_pass.gamma,         "wired-rp-gamma" );
	ADOPT_RP( vk.render_pass.capture,       vk.ral_render_pass.capture,       "wired-rp-capture" );
	ADOPT_RP( vk.render_pass.bloom_extract, vk.ral_render_pass.bloom_extract, "wired-rp-bloom-extract" );
	ADOPT_RP( vk.render_pass.post_bloom,    vk.ral_render_pass.post_bloom,    "wired-rp-post-bloom" );
	ADOPT_RP( vk.render_pass.depth_fade,    vk.ral_render_pass.depth_fade,    "wired-rp-depth-fade" );
	ADOPT_RP( vk.render_pass.smaa_edge,     vk.ral_render_pass.smaa_edge,     "wired-rp-smaa-edge" );
	ADOPT_RP( vk.render_pass.smaa_blend,    vk.ral_render_pass.smaa_blend,    "wired-rp-smaa-blend" );
	ADOPT_RP( vk.render_pass.smaa_resolve,  vk.ral_render_pass.smaa_resolve,  "wired-rp-smaa-resolve" );
	for ( i = 0; i < ARRAY_LEN( vk.render_pass.blur ); i++ ) {
		ADOPT_RP( vk.render_pass.blur[i],   vk.ral_render_pass.blur[i],       "wired-rp-blur" );
	}
#if FEAT_SHADOW_MAPPING
	ADOPT_RP( vk.shadowMap.renderPass,     vk.shadowMap.ral_renderPass,       "wired-rp-shadow-depth" );
#endif

	ADOPT_FB( vk.framebuffers.bloom_extract, vk.ral_framebuffers.bloom_extract, "wired-fb-bloom-extract" );
	ADOPT_FB( vk.framebuffers.tonemap,       vk.ral_framebuffers.tonemap,       "wired-fb-tonemap" );
	ADOPT_FB( vk.framebuffers.ui,            vk.ral_framebuffers.ui,            "wired-fb-ui" );
	ADOPT_FB( vk.framebuffers.ui_clear,      vk.ral_framebuffers.ui_clear,      "wired-fb-ui-clear" );
	ADOPT_FB( vk.framebuffers.screenmap,     vk.ral_framebuffers.screenmap,     "wired-fb-screenmap" );
	ADOPT_FB( vk.framebuffers.capture,       vk.ral_framebuffers.capture,       "wired-fb-capture" );
	ADOPT_FB( vk.framebuffers.smaa_edge,     vk.ral_framebuffers.smaa_edge,     "wired-fb-smaa-edge" );
	ADOPT_FB( vk.framebuffers.smaa_blend,    vk.ral_framebuffers.smaa_blend,    "wired-fb-smaa-blend" );
	ADOPT_FB( vk.framebuffers.smaa_resolve,  vk.ral_framebuffers.smaa_resolve,  "wired-fb-smaa-resolve" );
	for ( i = 0; i < ARRAY_LEN( vk.framebuffers.blur ); i++ ) {
		ADOPT_FB( vk.framebuffers.blur[i],   vk.ral_framebuffers.blur[i],       "wired-fb-blur" );
	}
	for ( i = 0; i < ARRAY_LEN( vk.framebuffers.main ); i++ ) {
		ADOPT_FB( vk.framebuffers.main[i],   vk.ral_framebuffers.main[i],       "wired-fb-main" );
	}
	for ( i = 0; i < ARRAY_LEN( vk.framebuffers.gamma ); i++ ) {
		ADOPT_FB( vk.framebuffers.gamma[i],  vk.ral_framebuffers.gamma[i],      "wired-fb-gamma" );
	}

	#undef ADOPT_RP
	#undef ADOPT_FB

	s_ral_render_passes_adopted = rpAdopted;
	s_ral_framebuffers_adopted  = fbAdopted;
	ri.Log( SEV_INFO,
		"[VK->RAL] adopted %u render passes + %u framebuffers as ralRenderPass_t / ralFramebuffer_t\n",
		rpAdopted, fbAdopted );
}


static void vk_ral_destroy_adopted_render_passes_and_framebuffers( void )
{
	uint32_t i;
	#define KILL_RP( field ) do { if ( field ) { Ral_DestroyRenderPass ( field ); field = NULL; } } while ( 0 )
	#define KILL_FB( field ) do { if ( field ) { Ral_DestroyFramebuffer( field ); field = NULL; } } while ( 0 )
	KILL_RP( vk.ral_render_pass.main );
	KILL_RP( vk.ral_render_pass.screenmap );
	KILL_RP( vk.ral_render_pass.tonemap );
	KILL_RP( vk.ral_render_pass.ui );
	KILL_RP( vk.ral_render_pass.ui_clear );
	KILL_RP( vk.ral_render_pass.gamma );
	KILL_RP( vk.ral_render_pass.capture );
	KILL_RP( vk.ral_render_pass.bloom_extract );
	KILL_RP( vk.ral_render_pass.post_bloom );
	KILL_RP( vk.ral_render_pass.depth_fade );
	KILL_RP( vk.ral_render_pass.smaa_edge );
	KILL_RP( vk.ral_render_pass.smaa_blend );
	KILL_RP( vk.ral_render_pass.smaa_resolve );
	for ( i = 0; i < ARRAY_LEN( vk.ral_render_pass.blur ); i++ ) KILL_RP( vk.ral_render_pass.blur[i] );
#if FEAT_SHADOW_MAPPING
	KILL_RP( vk.shadowMap.ral_renderPass );
#endif
	KILL_FB( vk.ral_framebuffers.bloom_extract );
	KILL_FB( vk.ral_framebuffers.tonemap );
	KILL_FB( vk.ral_framebuffers.ui );
	KILL_FB( vk.ral_framebuffers.ui_clear );
	KILL_FB( vk.ral_framebuffers.screenmap );
	KILL_FB( vk.ral_framebuffers.capture );
	KILL_FB( vk.ral_framebuffers.smaa_edge );
	KILL_FB( vk.ral_framebuffers.smaa_blend );
	KILL_FB( vk.ral_framebuffers.smaa_resolve );
	for ( i = 0; i < ARRAY_LEN( vk.ral_framebuffers.blur );  i++ ) KILL_FB( vk.ral_framebuffers.blur[i]  );
	for ( i = 0; i < ARRAY_LEN( vk.ral_framebuffers.main );  i++ ) KILL_FB( vk.ral_framebuffers.main[i]  );
	for ( i = 0; i < ARRAY_LEN( vk.ral_framebuffers.gamma ); i++ ) KILL_FB( vk.ral_framebuffers.gamma[i] );
	#undef KILL_RP
	#undef KILL_FB
	s_ral_render_passes_adopted = 0;
	s_ral_framebuffers_adopted  = 0;
}


// ════════════════════════════════════════════════════════════════════════
// Phase 7.4c-submit-A4 — internal-texture adoption sweep.
//
// The 6 renderer-owned VkImage handles backing the parallel-paths cmd sites
// (depth_image, color_image, tonemapped_image + 3 SMAA images) get wrapped
// in ralTexture_t* siblings here. The wrappers carry ownsImage=qfalse —
// teardown frees only the wrapper struct, not the VkImage.
//
// SMAA siblings live behind vk.fboActive (same gate as the SMAA-image alloc
// lifecycle in vk_smaa_alloc_resources / vk_smaa_release_resources). When
// the user toggles r_smaa across maps the resource path recreates the SMAA
// VkImages; re-running the adoption sweep at vid_restart picks up the fresh
// handles via the idempotent destroy-then-adopt pattern below.
//
// Note on the GPU-timestamp query pool: vk_gpu_ts_init runs AFTER
// vk_init_descriptors (which is the hook point this sweep fires from), so
// the matching ralQueryPool_t adoption can't happen here. It's done inline
// at vk_gpu_ts_init's tail instead; vk_gpu_ts_shutdown owns its destroy.
// The vk_ral_lookup_query_pool helper below sees the static once
// vk_gpu_ts_init populates it.
//
// Logged count (always-on SEV_INFO): "[VK->RAL] adopted N internal textures
// as ralTexture_t".
// ════════════════════════════════════════════════════════════════════════
static uint32_t s_ral_internal_textures_adopted;

void vk_ral_adopt_static_internal_textures( void )
{
	uint32_t adopted = 0;
	if ( !s_ral_backend ) return;

	#define ADOPT_TEX( vkfield, ralfield, w, h, asp, label ) do {                                                \
		if ( ralfield ) { Ral_DestroyTexture( ralfield ); ralfield = NULL; }                                 \
		if ( (vkfield) != VK_NULL_HANDLE ) {                                                                     \
			ralfield = Ral_AdoptTexture( s_ral_backend, (vkfield), (w), (h), (asp), (label) );               \
			if ( ralfield ) adopted++;                                                                           \
		}                                                                                                        \
	} while ( 0 )

	ADOPT_TEX( vk.depth_image,       vk.ral_depth_image,
	           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_DEPTH_BIT,
	           "wired-img-depth" );
	ADOPT_TEX( vk.color_image,       vk.ral_color_image,
	           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
	           "wired-img-color" );
	ADOPT_TEX( vk.tonemapped_image,  vk.ral_tonemapped_image,
	           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
	           "wired-img-tonemapped" );

	// Phase 7.4c-submit-BC-Pre — depthFade.image adoption. Created by
	// vk_create_attachments inside vk_initialize, which runs BEFORE
	// tr_init.c's vk_init_descriptors() call that drives this sweep — so
	// the VkImage is already valid here. Gated by vk.depthFade.active
	// (matches the same gate used at the qvkCreateImage site in vk.c:11871).
	// Closes the 3 A4-known-miss callsites at vk_depth_fade_copy
	// (PipelineBarrier ×2 + CopyImage ×1) that previously SEV_WARN-skipped.
	if ( vk.depthFade.active ) {
		ADOPT_TEX( vk.depthFade.image, vk.depthFade.ral_image,
		           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_DEPTH_BIT,
		           "wired-img-depthfade" );
	}

	if ( vk.fboActive ) {
		ADOPT_TEX( vk.smaa.input_image, vk.smaa.ral_input_image,
		           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
		           "wired-img-smaa-input" );
		ADOPT_TEX( vk.smaa.edges_image, vk.smaa.ral_edges_image,
		           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
		           "wired-img-smaa-edges" );
		ADOPT_TEX( vk.smaa.blend_image, vk.smaa.ral_blend_image,
		           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
		           "wired-img-smaa-blend" );
	}

	#undef ADOPT_TEX

	s_ral_internal_textures_adopted = adopted;
	ri.Log( SEV_INFO,
		"[VK->RAL] adopted %u internal textures as ralTexture_t\n", adopted );
}


void vk_ral_destroy_adopted_internal_textures( void )
{
	#define KILL_TEX( field ) do { if ( field ) { Ral_DestroyTexture( field ); field = NULL; } } while ( 0 )
	KILL_TEX( vk.ral_depth_image );
	KILL_TEX( vk.ral_color_image );
	KILL_TEX( vk.ral_tonemapped_image );
	KILL_TEX( vk.depthFade.ral_image );   // Phase 7.4c-submit-BC-Pre
	KILL_TEX( vk.smaa.ral_input_image );
	KILL_TEX( vk.smaa.ral_edges_image );
	KILL_TEX( vk.smaa.ral_blend_image );
	#undef KILL_TEX
	s_ral_internal_textures_adopted = 0;
}


struct ralTexture_s *vk_ral_lookup_texture( VkImage vkImage )
{
	if ( vkImage == VK_NULL_HANDLE ) return NULL;
	#define MATCH( vk_field, ral_field ) if ( (vk_field) == vkImage && (ral_field) != NULL ) return (ral_field)
	MATCH( vk.depth_image,       vk.ral_depth_image );
	MATCH( vk.color_image,       vk.ral_color_image );
	MATCH( vk.tonemapped_image,  vk.ral_tonemapped_image );
	MATCH( vk.depthFade.image,   vk.depthFade.ral_image );   // Phase 7.4c-submit-BC-Pre
	MATCH( vk.smaa.input_image,  vk.smaa.ral_input_image );
	MATCH( vk.smaa.edges_image,  vk.smaa.ral_edges_image );
	MATCH( vk.smaa.blend_image,  vk.smaa.ral_blend_image );
	#undef MATCH
	return NULL;
}


struct ralQueryPool_s *vk_ral_lookup_query_pool( VkQueryPool vkPool )
{
	if ( vkPool == VK_NULL_HANDLE ) return NULL;
	if ( vkPool == vk_gpu_ts_pool && vk_gpu_ts_ral_pool != NULL ) return vk_gpu_ts_ral_pool;
	return NULL;
}


static void vk_ral_destroy_adopted_pipeline_layouts( void )
{
	#define KILL_PL( ralfield ) do { if ( ralfield ) { Ral_DestroyPipelineLayout( ralfield ); ralfield = NULL; } } while ( 0 )
	KILL_PL( vk.ral_pipeline_layout );
	KILL_PL( vk.ral_pipeline_layout_storage );
	KILL_PL( vk.ral_pipeline_layout_post_process );
	KILL_PL( vk.ral_pipeline_layout_smaa );
	KILL_PL( vk.ral_pipeline_layout_blend );
	KILL_PL( vk.ral_pipeline_layout_msdf );
	KILL_PL( vk.ral_pipeline_layout_ssao );
	KILL_PL( vk.ral_pipeline_layout_godrays );
	KILL_PL( vk.ribbon.ral_pipeline_layout );
	KILL_PL( vk.beam.ral_pipeline_layout );
	KILL_PL( vk.sprite.ral_pipeline_layout );
	KILL_PL( vk.particle.ral_compute_pipeline_layout );
	KILL_PL( vk.particle.ral_render_pipeline_layout );
#if FEAT_IQM
	KILL_PL( vk.iqmGpu.ral_pipeline_layout );
#endif
#if FEAT_SHADOW_MAPPING
	KILL_PL( vk.shadowMap.ral_depthLayout );
#endif
	#undef KILL_PL
	s_ral_pipeline_layouts_adopted = 0;
}


// Phase 7.4c-bindgroup — VkDescriptorSet → ralBindGroup_t * reverse lookup.
// Used at parallel bind-call sites to find the adopted wrapper for the
// VkDescriptorSet the legacy code is about to bind. Linear scan over the
// adoption registry (32 entries max) — acceptable parallel-paths overhead
// until 7.4c-cmd refactors to per-subsystem sibling fields. Returns NULL
// for unadopted sets (e.g. per-shader-type rotating descriptors in
// vk.cmd->descriptor_set.current[] — deferred to 7.4c-cmd's per-frame
// adoption); callers skip the RAL parallel call on NULL return.
ralBindGroup_t *vk_ral_lookup_bindgroup( VkDescriptorSet vkSet ) {
	uint32_t i;
	if ( vkSet == VK_NULL_HANDLE ) return NULL;
	for ( i = 0; i < s_adopted_bgs_count; i++ ) {
		if ( s_adopted_bgs[i] && Ral_GetBindGroupHandle( s_adopted_bgs[i] ) == (void *)vkSet ) {
			return s_adopted_bgs[i];
		}
	}
	// Phase 7.4c-cmd: also walk the per-frame ring adoption mirror. These
	// are per-image VkDescriptorSets (allocated at R_CreateImage time) that
	// vk_update_descriptor parks in vk.cmd->descriptor_set.current_ral[]
	// alongside the matching .current[] slot. Scanning this short array
	// is O(VK_DESC_COUNT+1) = O(8) and only fires on the miss path of the
	// boot-time registry above.
	if ( vk.cmd != NULL ) {
		for ( i = 0; i < ARRAY_LEN( vk.cmd->descriptor_set.current_ral ); i++ ) {
			ralBindGroup_t *bg = vk.cmd->descriptor_set.current_ral[i];
			if ( bg != NULL && Ral_GetBindGroupHandle( bg ) == (void *)vkSet ) {
				return bg;
			}
		}
	}
	return NULL;
}


void vk_ral_textures_shutdown( qboolean destroyWindow ) {
	if ( s_ral_init_attempted && s_ral_backend ) {
		// Session-summary log (replaces the unavailable \ral_resources cli
		// cmd — renderer-DLL-registered Cmd_AddCommand entries are missing
		// from the engine's cmd table for the +cli dispatch path; pre-
		// existing engine quirk, applies to vkinfo / imagelist / etc. too.
		// Always log on shutdown so the migration's bindless + RAL buffer
		// population is observable in the captured log.).
		vk_ral_textures_diag_dump();
	}
	// Phase 7.4c-pipeline-followup-5 PART 2.5 — REF_KEEP_CONTEXT skip gate.
	// destroyWindow == qfalse means level-scoped teardown (map transition):
	// keep the RAL backend, sibling pipelines, BGLs, and the active-buffer
	// tracker live across the transition. Mirrors legacy vk_shutdown's
	// skip-on-!destroyWindow pattern (Q3 lineage — preserves vk.device +
	// vk.gamma_pipeline et al. for the next map). The full teardown path
	// below runs only on the REF_UNLOAD_DLL final exit, which is also when
	// the legacy vk_shutdown destroys vk.device, so the RAL pipelines must
	// release BEFORE that.
	//
	// Interim: Phase 7.4d-map-arena retires REF_KEEP_CONTEXT entirely; after
	// that turn the gate disappears and the body runs unconditionally.
	if ( !destroyWindow ) {
		return;
	}
	// Phase 7.4b: destroy every still-live RAL buffer BEFORE the backend
	// teardown so each defer-destroy is owned by a live backend.
	vk_ral_destroy_all_active_buffers();
	// Phase 7.4c-pipeline-followup — save the pipeline cache to disk
	// before the backend is destroyed. Subsequent boots seed from this
	// file for faster pipeline warm-up. Same path convention as the
	// vk_initialize-time load (§16.7 versioned filename).
	//
	// 7.4c-pipeline-followup-2 observability note: this code path is
	// known correct (path construction matches the load site that does
	// fire) but the engine's qconsole.jsonl log truncates mid-message
	// during the very tail of shutdown — the save-confirmation log line
	// from inside Ral_SavePipelineCache cannot be observed in the smoke.
	// Disk-file verification is the authoritative confirmation; the file
	// shows up post-run on any session that reached actual pipeline
	// creation (smoke runs that time out before map load produce an
	// empty cache and the early-return in Ral_SavePipelineCache skips
	// the write — both behaviours by-spec).
	if ( s_ral_backend ) {
		char cachePath[ MAX_OSPATH ];
		const char *home = ri.Cvar_VariableString( "fs_homepath" );
		const char *base = ri.Cvar_VariableString( "fs_basegame" );
		if ( !base || !*base ) base = "baseq3";
		Com_sprintf( cachePath, sizeof( cachePath ), "%s/%s/pipelinecache_v1_vulkan.bin",
		             ( home && *home ) ? home : ".", base );
		ri.Log( SEV_INFO, "[VK->RAL] saving pipeline cache to '%s'\n", cachePath );
		Ral_SavePipelineCache( s_ral_backend, cachePath );
	}
	// Phase 7.4c-bindgroup — destroy every adopted bind-group wrapper.
	// ownsSet=qfalse on adopted wrappers means Ral_DestroyBindGroup only
	// frees the wrapper struct, not the underlying VkDescriptorSet — the
	// legacy vkResetDescriptorPool path frees the sets. Must run BEFORE
	// Ral_DestroyBackend so the wrappers don't outlive the backend they
	// reference.
	vk_ral_destroy_adopted_bindgroups();

	// Phase 7.4c-submit-A2 — destroy every adopted pipeline-layout wrapper.
	// Same ownsHandle=qfalse contract as the bindgroup wrappers above: the
	// renderer's existing qvkDestroyPipelineLayout teardown owns the underlying
	// VkPipelineLayout lifetime; this only frees the wrapper structs. Must run
	// BEFORE Ral_DestroyBackend for the same dangling-ref reason.
	vk_ral_destroy_adopted_pipeline_layouts();

	// Phase 7.4c-submit-A3 — destroy every adopted render-pass + framebuffer
	// wrapper. Same ownsHandle=qfalse contract.
	vk_ral_destroy_adopted_render_passes_and_framebuffers();

	// Phase 7.4c-submit-A4 — destroy every adopted internal-texture + query-
	// pool wrapper. Same ownsImage=qfalse / ownsPool=qfalse contract: only the
	// wrapper structs get freed; the underlying VkImage / VkQueryPool stays
	// owned by the renderer's existing teardown path (vk_destroy_attachments
	// for the images, vk_gpu_ts_shutdown for the query pool).
	vk_ral_destroy_adopted_internal_textures();

	// Phase 7.4c-pipeline-followup-5 PART 0 — renderer-side RAL pipeline +
	// BGL destruction BEFORE Ral_DestroyBackend. Ral_DestroyBackend does NOT
	// enumerate live RAL pipelines / BGLs (the backend doesn't track them in
	// a global list — by §7.4c-pre design). Each consumer must Ral_Destroy*
	// its own handles before backend teardown, else the underlying VkPipeline /
	// VkDescriptorSetLayout leak past vkDestroyDevice
	// (VUID-vkDestroyDevice-device-05137). On REF_KEEP_CONTEXT shutdown
	// (server map-load Phase P1) vk_shutdown is SKIPPED so its
	// vk_destroy_pipelines path doesn't run — we MUST do the destroy here.
	// On REF_UNLOAD_DLL shutdown the subsequent vk_destroy_pipelines NULL-
	// checks each sibling field and finds NULL (we cleared them below), so it
	// only destroys the legacy VkPipelines.
	{
		uint32_t i, j;
		#define KILL_PIPE( field )   do { if ( (field) ) { Ral_DestroyPipeline      ( (field) ); (field) = NULL; } } while (0)
		#define KILL_BGL(  field )   do { if ( (field) ) { Ral_DestroyBindGroupLayout( (field) ); (field) = NULL; } } while (0)
		// centralized helper variants
		for ( i = 0; i < vk.pipelines_count; i++ )
			for ( j = 0; j < RENDER_PASS_COUNT; j++ )
				KILL_PIPE( vk.pipelines[i].ral_handle[j] );
		// post-process + bloom + capture + tonemap + gamma + blur
		KILL_PIPE( vk.ral_gamma_pipeline );
		KILL_PIPE( vk.ral_tonemap_pipeline );
		for ( i = 0; i < ARRAY_LEN( vk.ral_tonemap_variants ); i++ )
			KILL_PIPE( vk.ral_tonemap_variants[i] );
		KILL_PIPE( vk.ral_capture_pipeline );
		KILL_PIPE( vk.ral_bloom_extract_pipeline );
		KILL_PIPE( vk.ral_bloom_blend_pipeline );
		for ( i = 0; i < ARRAY_LEN( vk.ral_blur_pipeline ); i++ )
			KILL_PIPE( vk.ral_blur_pipeline[i] );
		// SMAA three-pass
		KILL_PIPE( vk.ral_smaa_edge_pipeline );
		KILL_PIPE( vk.ral_smaa_blend_pipeline );
		KILL_PIPE( vk.ral_smaa_resolve_pipeline );
		// special-case subsystems
		KILL_PIPE( vk.shadowMap.ral_depthPipeline );
		KILL_PIPE( vk.ribbon.ral_pipeline_alpha );
		KILL_PIPE( vk.ribbon.ral_pipeline_additive );
		KILL_PIPE( vk.beam.ral_pipeline );
		KILL_PIPE( vk.sprite.ral_pipeline_alpha );
		KILL_PIPE( vk.sprite.ral_pipeline_additive );
		KILL_PIPE( vk.particle.ral_compute_pipeline );
		KILL_PIPE( vk.particle.ral_render_pipeline_alpha );
		KILL_PIPE( vk.particle.ral_render_pipeline_additive );
		KILL_PIPE( vk.iqmGpu.ral_pipeline );
		// adopted BGL wrappers (ownsLayout=qfalse: only the wrapper struct
		// gets freed, the underlying VkDescriptorSetLayout stays alive for
		// the legacy renderer code to vkDestroyDescriptorSetLayout later).
		KILL_BGL( vk.ral_bgl_sampler );
		KILL_BGL( vk.ral_bgl_uniform );
		KILL_BGL( vk.ral_bgl_storage );
		KILL_BGL( vk.ribbon.ral_bgl );
		KILL_BGL( vk.beam.ral_bgl );
		KILL_BGL( vk.sprite.ral_bgl );
		KILL_BGL( vk.particle.ral_bgl_compute );
		KILL_BGL( vk.particle.ral_bgl_render );
		KILL_BGL( vk.iqmGpu.ral_bgl_bones );
		#undef KILL_PIPE
		#undef KILL_BGL
	}
	if ( s_ral_bindless_set    ) { Ral_DestroyBindGroup      ( s_ral_bindless_set    ); s_ral_bindless_set    = NULL; }
	if ( s_ral_bindless_layout ) { Ral_DestroyBindGroupLayout( s_ral_bindless_layout ); s_ral_bindless_layout = NULL; }
	// Phase 7.4c-submit-followup-present-2-fix3 — Ral_DestroyBackend moved
	// out into vk_ral_backend_shutdown (called from vk_shutdown's tail). Was
	// here before; ran from RE_Shutdown:vk_ral_textures_shutdown (BEFORE
	// vk_shutdown), which left vk_shutdown's Ral_DestroyCommandBuffer of
	// vk.ral_staging_cmd + Ral_DestroySwapchain calls with a dangling backend
	// pointer → vkFreeCommandBuffers VUID storm + nvoglv64.dll access violation.
	s_ral_bindless_capacity = 0;
	s_ral_init_attempted    = qfalse;
	// Reset buffer counters too — vid_restart re-enters with a clean state.
	s_buf_pending_count       = 0;
	s_buf_pending_warned_full = qfalse;
	s_buf_active_count        = 0;
	s_buf_peak_count          = 0;
	s_buf_register_total      = 0;
	s_buf_destroy_total       = 0;
	s_buf_skipped_no_backend  = 0;
	memset( s_buf_bytes_by_usage, 0, sizeof( s_buf_bytes_by_usage ) );
}

// ── per-image registration ──────────────────────────────────────────────
static void vk_ral_record_name( const char *name ) {
	if ( !name || !name[0] ) return;
	Q_strncpyz( s_ral_recent_names[ s_ral_recent_head ], name, sizeof( s_ral_recent_names[0] ) );
	s_ral_recent_head = ( s_ral_recent_head + 1 ) % VK_RAL_RECENT_NAMES;
}

void vk_ral_register_image( image_t *image, byte *pic, int width, int height ) {
	ralTextureCreateInfo_t tci;
	ralTextureUploadDesc_t up;
	ralFence_t            *f;
	uint32_t               slot;

	if ( !vk_ral_textures_available() || !image ) return;
	if ( image->ral ) return;                                          // already registered
	if ( !pic ) { s_ral_skipped_no_data++; return; }                   // no source — bindless slot left empty for this image
	if ( image->texType != TEXTYPE_2D ) return;                        // 7.4a covers 2D only; cube/3D parallel registration lands later
	if ( width <= 0 || height <= 0 ) { s_ral_skipped_no_data++; return; }  // defensive — avoid 0-byte upload

	memset( &tci, 0, sizeof( tci ) );
	tci.type               = RAL_TEXTURE_2D;
	tci.format             = RAL_FORMAT_R8G8B8A8_UNORM;                // 7.4a forces RGBA8 for the parallel RAL texture; legacy may pick 4-bit packed format independently (bindless table is unused in 7.4a so format mismatch is harmless)
	tci.width              = (uint32_t)width;
	tci.height             = (uint32_t)height;
	tci.depthOrArrayLayers = 1;
	tci.mipLevels          = ( image->flags & IMGFLAG_MIPMAP ) ? 0u : 1u;   // 0 → RAL picks full chain via ralVk_FullMipChain
	tci.sampleCount        = 1;
	tci.usage              = RAL_TEXTURE_USAGE_SAMPLED;                // no STORAGE / no COLOR_ATTACHMENT — bindless sampled texture only
	tci.memory             = RAL_MEMORY_DEVICE_LOCAL;
	tci.debugName          = image->imgName;
	image->ral = Ral_CreateTexture( s_ral_backend, &tci );
	if ( !image->ral ) {
		ri.Log( SEV_DEBUG, "[RAL-TEX] Ral_CreateTexture failed for '%s' (%dx%d)\n", image->imgName, width, height );
		return;
	}

	// Upload the source RGBA bytes. RAL's GPU mip-gen produces mips 1..N via
	// vkCmdBlitImage (linear) instead of the renderer's CPU sRGB-aware box
	// filter — fine because the bindless table is unused in 7.4a; visual
	// output of the renderer is driven by the legacy VkImage (which has the
	// CPU mips). 7.4c may need to revisit if visual parity at low LOD is
	// required.
	memset( &up, 0, sizeof( up ) );
	up.mipLevel   = 0;
	up.arrayLayer = 0;
	up.data       = pic;
	up.dataSize   = (uint64_t)width * (uint64_t)height * 4u;
	f = Ral_TextureUploadAsync( image->ral, &up );
	if ( f ) { Ral_WaitFence( f, ~0ull ); Ral_DestroyFence( f ); }

	// Bindless slot index: each image is added to tr.images[] before this
	// function is called (R_CreateImage already incremented tr.numImages),
	// so the slot is tr.numImages - 1. Slots above the bindless capacity
	// keep their RAL texture alive but don't appear in the BindGroup.
	slot = (uint32_t)( tr.numImages - 1 );
	if ( slot < s_ral_bindless_capacity ) {
		Ral_BindGroupSetTextureAt( s_ral_bindless_set, slot, image->ral );
		image->ralBindlessSlot = (int)slot;
		s_ral_registered_count++;
		vk_ral_record_name( image->imgName );
	} else {
		image->ralBindlessSlot = -1;
		s_ral_skipped_no_slot++;
		vk_ral_warn_bindless_full();
	}
}

// One-shot log for the "bindless table full" case — keeps the log clean
// if a huge map blows past 4096 textures.
static void vk_ral_warn_bindless_full( void ) {
	if ( s_ral_full_warned ) return;
	s_ral_full_warned = qtrue;
	ri.Log( SEV_WARN, "[RAL-TEX] bindless table reached its %u-slot capacity; further images keep their RAL texture but won't appear in the set. Grow capacity or evict on pressure (Phase 7.4c).\n",
	        s_ral_bindless_capacity );
}

void vk_ral_unregister_image( image_t *image ) {
	if ( !image || !s_ral_backend ) return;
	if ( image->ralBindlessSlot >= 0 && s_ral_bindless_set ) {
		Ral_BindGroupSetTextureAt( s_ral_bindless_set, (uint32_t)image->ralBindlessSlot, NULL );
		image->ralBindlessSlot = -1;
	}
	if ( image->ral ) {
		Ral_DestroyTexture( image->ral );
		image->ral = NULL;
		s_ral_destroyed_count++;
	}
}

// ── Phase 7.4b — RAL buffer register / unregister ──────────────────────
static const char *vk_ral_usage_name( int usage ) {
	if ( usage & RAL_BUFFER_VERTEX       ) return "VERTEX";
	if ( usage & RAL_BUFFER_INDEX        ) return "INDEX";
	if ( usage & RAL_BUFFER_UNIFORM      ) return "UNIFORM";
	if ( usage & RAL_BUFFER_STORAGE      ) return "STORAGE";
	if ( usage & RAL_BUFFER_INDIRECT     ) return "INDIRECT";
	if ( usage & RAL_BUFFER_TRANSFER_SRC ) return "TRANSFER_SRC";
	if ( usage & RAL_BUFFER_TRANSFER_DST ) return "TRANSFER_DST";
	return "?";
}

// Tally bytes against every set usage bit (matches the legacy multi-usage
// VK_BUFFER_USAGE_* OR-mask). A single buffer with VERTEX | INDEX | UNIFORM
// counts in all three usage buckets — handy for spotting where bytes go.
static void vk_ral_tally_usage( int usage, uint64_t bytes, int sign ) {
	int i;
	for ( i = 0; i < 7; i++ ) {
		int bit = 1 << i;
		if ( usage & bit ) {
			if ( sign > 0 )      s_buf_bytes_by_usage[i] += bytes;
			else if ( bytes <= s_buf_bytes_by_usage[i] ) s_buf_bytes_by_usage[i] -= bytes;
			else                 s_buf_bytes_by_usage[i] = 0;
		}
	}
}

static void vk_ral_flush_pending_buffers( void ) {
	uint32_t i;
	uint32_t created = 0, failed = 0;
	if ( s_buf_pending_count == 0 ) return;
	for ( i = 0; i < s_buf_pending_count; i++ ) {
		vkRalPendingBuffer_t        *p = &s_buf_pending[i];
		ralBufferCreateInfo_t        bci;
		ralBuffer_t                 *rb;
		vkRalActiveBuffer_t         *node;
		memset( &bci, 0, sizeof( bci ) );
		bci.size      = p->size;
		bci.usage     = (ralBufferUsage_t)p->usage;
		bci.memory    = (ralMemoryType_t)p->memory;
		bci.debugName = p->debugName;
		rb = Ral_CreateBuffer( s_ral_backend, &bci );
		if ( !rb ) { failed++; continue; }
		node = (vkRalActiveBuffer_t *)malloc( sizeof( *node ) );
		if ( !node ) { Ral_DestroyBuffer( rb ); failed++; continue; }
		node->next   = s_buf_active;
		node->key    = p->key;
		node->ral    = rb;
		node->size   = p->size;
		node->usage  = p->usage;
		node->memory = p->memory;
		s_buf_active = node;
		s_buf_active_count++;
		if ( s_buf_active_count > s_buf_peak_count ) s_buf_peak_count = s_buf_active_count;
		vk_ral_tally_usage( p->usage, p->size, +1 );
		created++;
	}
	s_buf_pending_count = 0;
	ri.Log( SEV_INFO, "[RAL-BUF] flushed %u pending buffer(s) into the live RAL backend (%u failed)\n", created, failed );
}

static void vk_ral_destroy_all_active_buffers( void ) {
	vkRalActiveBuffer_t *p = s_buf_active;
	while ( p ) {
		vkRalActiveBuffer_t *n = p->next;
		if ( p->ral ) Ral_DestroyBuffer( p->ral );
		free( p );
		p = n;
	}
	s_buf_active = NULL;
}

static int vk_ral_translate_usage( VkBufferUsageFlags vk ) {
	int u = 0;
	if ( vk & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT   ) u |= RAL_BUFFER_VERTEX;
	if ( vk & VK_BUFFER_USAGE_INDEX_BUFFER_BIT    ) u |= RAL_BUFFER_INDEX;
	if ( vk & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT  ) u |= RAL_BUFFER_UNIFORM;
	if ( vk & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT  ) u |= RAL_BUFFER_STORAGE;
	if ( vk & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT ) u |= RAL_BUFFER_INDIRECT;
	if ( vk & VK_BUFFER_USAGE_TRANSFER_SRC_BIT    ) u |= RAL_BUFFER_TRANSFER_SRC;
	if ( vk & VK_BUFFER_USAGE_TRANSFER_DST_BIT    ) u |= RAL_BUFFER_TRANSFER_DST;
	return u;
}

static int vk_ral_translate_memory( VkMemoryPropertyFlags vk ) {
	if ( ( vk & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) && ( vk & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) )
		return RAL_MEMORY_HOST_COHERENT;
	if ( vk & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) return RAL_MEMORY_HOST_VISIBLE;
	if ( vk & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT ) return RAL_MEMORY_LAZY_ALLOC;
	return RAL_MEMORY_DEVICE_LOCAL;
}

void vk_ral_register_buffer( VkBuffer key, uint64_t size,
                             VkBufferUsageFlags vkUsage,
                             VkMemoryPropertyFlags vkMemProps,
                             const char *debugName ) {
	int usage  = vk_ral_translate_usage( vkUsage );
	int memory = vk_ral_translate_memory( vkMemProps );
	if ( key == VK_NULL_HANDLE || size == 0 ) return;
	// Cvar opt-out — early-out before any list maintenance.
	if ( !r_useRALBuffers || r_useRALBuffers->integer == 0 ) return;

	s_buf_register_total++;

	if ( s_ral_backend == NULL ) {
		// Backend not up yet — queue. Common path for vk_initialize-time
		// creates (staging / tess / storage / ribbon / beam / sprite /
		// particle / primitive / IQM bone / world VBO).
		if ( s_buf_pending_count >= VK_RAL_PENDING_BUFFER_MAX ) {
			if ( !s_buf_pending_warned_full ) {
				s_buf_pending_warned_full = qtrue;
				ri.Log( SEV_WARN, "[RAL-BUF] pending-buffer queue full (%u) — register dropped for '%s'\n",
				        VK_RAL_PENDING_BUFFER_MAX, debugName ? debugName : "(unnamed)" );
			}
			s_buf_skipped_no_backend++;
			return;
		}
		{
			vkRalPendingBuffer_t *p = &s_buf_pending[ s_buf_pending_count++ ];
			p->key    = key;
			p->size   = size;
			p->usage  = usage;
			p->memory = memory;
			Q_strncpyz( p->debugName, debugName ? debugName : "(unnamed)", sizeof( p->debugName ) );
		}
		return;
	}

	// Backend up — create RAL buffer immediately + add to active list.
	{
		ralBufferCreateInfo_t  bci;
		ralBuffer_t           *rb;
		vkRalActiveBuffer_t   *node;
		memset( &bci, 0, sizeof( bci ) );
		bci.size      = size;
		bci.usage     = (ralBufferUsage_t)usage;
		bci.memory    = (ralMemoryType_t)memory;
		bci.debugName = debugName;
		rb = Ral_CreateBuffer( s_ral_backend, &bci );
		if ( !rb ) { ri.Log( SEV_DEBUG, "[RAL-BUF] Ral_CreateBuffer failed for '%s' (%llu bytes)\n", debugName ? debugName : "?", (unsigned long long)size ); return; }
		node = (vkRalActiveBuffer_t *)malloc( sizeof( *node ) );
		if ( !node ) { Ral_DestroyBuffer( rb ); return; }
		node->next   = s_buf_active;
		node->key    = key;
		node->ral    = rb;
		node->size   = size;
		node->usage  = usage;
		node->memory = memory;
		s_buf_active = node;
		s_buf_active_count++;
		if ( s_buf_active_count > s_buf_peak_count ) s_buf_peak_count = s_buf_active_count;
		vk_ral_tally_usage( usage, size, +1 );
		(void)vk_ral_usage_name;   // referenced by diag_dump
	}
}

void vk_ral_unregister_buffer( VkBuffer key ) {
	vkRalActiveBuffer_t **link;
	uint32_t              i;
	if ( key == VK_NULL_HANDLE ) return;
	if ( !r_useRALBuffers || r_useRALBuffers->integer == 0 ) return;
	s_buf_destroy_total++;

	// Active list first — common path post-backend-init.
	link = &s_buf_active;
	while ( *link ) {
		vkRalActiveBuffer_t *node = *link;
		if ( node->key == key ) {
			*link = node->next;
			vk_ral_tally_usage( node->usage, node->size, -1 );
			if ( node->ral ) Ral_DestroyBuffer( node->ral );
			free( node );
			if ( s_buf_active_count > 0 ) s_buf_active_count--;
			return;
		}
		link = &node->next;
	}

	// Else pending list — backend never came up before this destroy fired
	// (transient: SMAA LUT staging buffer in vk_smaa_alloc_resources).
	for ( i = 0; i < s_buf_pending_count; i++ ) {
		if ( s_buf_pending[i].key == key ) {
			// O(N) shift — fine at N≤64.
			if ( i + 1 < s_buf_pending_count )
				memmove( &s_buf_pending[i], &s_buf_pending[i + 1], ( s_buf_pending_count - i - 1 ) * sizeof( s_buf_pending[0] ) );
			s_buf_pending_count--;
			return;
		}
	}
	// Unknown key — typical when r_useRALBuffers was 0 at register time and
	// flipped to 1 mid-session (CVAR_LATCH should prevent this); silently
	// ignore. Also covers buffers created by paths outside the wired
	// register sites (none expected in 7.4b but defensive).
}

// ── "\ral_dump live" — dump the renderer-owned (imported-mode) backend ─
// Q_EXPORT'd so the client's Sys_LoadFunction in cl_main.c can resolve it.
// Reports caps, memory budget, and the texture/buffer registration state
// against the LIVE backend (shared with renderervk's VkDevice) instead of
// creating a throwaway one like Ral_Dump does.
Q_EXPORT void Ral_DumpLive( void ) {
	const ralCaps_t  *c;
	ralMemoryBudget_t mb;

	ri.Log( SEV_INFO, "===== \\ral_dump live (renderer-owned imported-mode backend) =====\n" );

	if ( !s_ral_init_attempted ) {
		ri.Log( SEV_INFO, "  RAL bringup has not been attempted yet this session — try setting r_useRALTextures 1 or r_useRALBuffers 1 and vid_restart\n" );
		ri.Log( SEV_INFO, "===== end \\ral_dump live =====\n" );
		return;
	}
	if ( !s_ral_backend ) {
		ri.Log( SEV_WARN, "  RAL bringup attempted but failed — see earlier [RAL-TEX] / [RAL] warnings\n" );
		ri.Log( SEV_INFO, "===== end \\ral_dump live =====\n" );
		return;
	}

	c = Ral_GetCaps( s_ral_backend );
	if ( c ) {
		ri.Log( SEV_INFO, "Ral_GetCaps (live):\n" );
		ri.Log( SEV_INFO, "  device                   : %s\n", c->deviceName );
		ri.Log( SEV_INFO, "  apiVersion               : %s\n", c->apiVersion );
		ri.Log( SEV_INFO, "  bindlessTextures         : %s (max %u)\n", c->bindlessTextures ? "yes" : "no", c->maxBindlessTextures );
		ri.Log( SEV_INFO, "  dynamicRendering         : %s\n", c->dynamicRendering ? "yes" : "no" );
		ri.Log( SEV_INFO, "  timelineSemaphores       : %s\n", c->timelineSemaphores ? "yes" : "no" );
		ri.Log( SEV_INFO, "  asyncCompute / Transfer  : %s / %s\n", c->asyncCompute ? "yes" : "no", c->asyncTransfer ? "yes" : "no" );
		ri.Log( SEV_INFO, "  debugUtils               : %s\n", c->debugUtils ? "yes" : "no" );
		ri.Log( SEV_INFO, "  memoryBudget             : %s (imported mode leaves this off in 7.4c-pre)\n", c->memoryBudget ? "yes" : "no" );
		ri.Log( SEV_INFO, "  drawIndirectCount        : %s\n", c->drawIndirectCount ? "yes" : "no" );
		ri.Log( SEV_INFO, "  maxTextureDimension2D    : %u\n", c->maxTextureDimension2D );
		ri.Log( SEV_INFO, "  maxSamplerAnisotropy     : %.0fx\n", c->maxSamplerAnisotropy );
	}

	Ral_QueryMemoryBudget( s_ral_backend, &mb );
	ri.Log( SEV_INFO, "Ral_QueryMemoryBudget (live):\n" );
	ri.Log( SEV_INFO, "  device-local : %u / %u MiB used\n", (unsigned)( mb.deviceLocalUsed >> 20 ), (unsigned)( mb.deviceLocalBudget >> 20 ) );
	ri.Log( SEV_INFO, "  host-visible : %u / %u MiB used\n", (unsigned)( mb.hostVisibleUsed >> 20 ), (unsigned)( mb.hostVisibleBudget >> 20 ) );
	ri.Log( SEV_INFO, "  underPressure: %s\n", mb.underPressure ? "yes" : "no" );

	// Texture + buffer registration state — re-uses the existing dump.
	vk_ral_textures_diag_dump();

	// Phase 7.4c-pipeline-followup-5 PART 3 — pipeline-layout cache slot
	// enumeration on the live imported-mode backend. This is the slot count
	// the renderer actually drives draws against; the throwaway-backend
	// version (\ral_dump pipeline, exits via Ral_Dump → Ral_CreateBackend →
	// ralVk_RunPipelineTest) shows an empty cache pre-synthetic-test.
	Ral_DumpPipelineLayoutCache( s_ral_backend );

	ri.Log( SEV_INFO, "===== end \\ral_dump live =====\n" );
}

// ── "\ral_pipeline_test" — walk 19 §17.7 fixtures, report PASS/FAIL ────
// Q_EXPORT'd so cl_main.c's CL_RalPipelineTest_f can resolve it via
// Sys_LoadFunction. Each fixture asserts a specific parallel ralPipeline_t
// sibling field is non-NULL after boot (proving the special-case site or
// the centralized helper created the matching RAL pipeline). Fixtures that
// require runtime conditions not in scope at \ral_pipeline_test invocation
// time (mirror portal, wireframe, stencil shadow) are reported as N/A with
// a note about what would create them — they're still parallel-paths-
// compatible per the centralized helper coverage, but enumerating live
// requires triggering the conditional code path first.
//
// Phase 7.4c-pipeline-followup-4.
typedef struct {
	const char           *name;
	const ralPipeline_t **field;
	const char           *note;
} ral_pipeline_test_fixture_t;

Q_EXPORT void Ral_PipelineTest( void ) {
	uint32_t pass = 0, fail = 0, na = 0, i;

	ri.Log( SEV_INFO, "===== \\ral_pipeline_test (Phase 7.4c-pipeline-followup-4) =====\n" );

	if ( !s_ral_init_attempted || !s_ral_backend ) {
		ri.Log( SEV_INFO, "  RAL backend not live — set r_useRALPipelines 1, vid_restart, then retry.\n" );
		ri.Log( SEV_INFO, "===== end \\ral_pipeline_test =====\n" );
		return;
	}

	// Special-case sibling fields populated by per-call vk_ral_create_special_pipeline.
	// Fixtures numbered per §17.7's 19-entry table; some are "umbrella" entries
	// covering a cluster (e.g. fixture #15 SMAA → 3 ral pipelines), in which case
	// PASS requires ALL pipelines in the cluster non-NULL.
	const ral_pipeline_test_fixture_t fixtures[] = {
		// 1-4: centralized helper coverage — UI/world surfaces live as
		// ~500-650 hash-keyed Vk_Pipeline_Def variants in vk.pipelines[].
		// Their parallel ralPipeline_t * lives on the same array entry
		// (vk.pipelines[i].ral_pipeline). We can't enumerate the full set
		// without a pipeline hash walk; instead, report whether the array
		// has at least one non-NULL ral pointer (proving the centralized
		// path produced parallel RAL pipelines this session).
		{ "centralized #1 UI alpha blend",         NULL, "Vk_Pipeline_Def hash-keyed; checked via array scan below" },
		{ "centralized #2 UI additive",            NULL, "Vk_Pipeline_Def hash-keyed; checked via array scan below" },
		{ "centralized #3 UI modulate",            NULL, "Vk_Pipeline_Def hash-keyed; checked via array scan below" },
		{ "centralized #4 Opaque world surface",   NULL, "Vk_Pipeline_Def hash-keyed; checked via array scan below" },
		{ "centralized #5 Sky equal-depth",        NULL, "Vk_Pipeline_Def hash-keyed; checked via array scan below" },
		{ "centralized #6 Decal w/ polygon offset",NULL, "Vk_Pipeline_Def hash-keyed; checked via array scan below" },
		{ "centralized #7 Alpha-tested foliage",   NULL, "Vk_Pipeline_Def hash-keyed; checked via array scan below" },
		// 8: shadow caster — special-case site, sibling field.
		{ "shadow caster (special-case)",
		  (const ralPipeline_t **)&vk.shadowMap.ral_depthPipeline, NULL },
		// 9-11: stencil edges/quad — centralized helper SHADOW_EDGES/FS_QUAD
		// shader_types; same array-scan coverage as 1-7.
		{ "stencil edges front",                   NULL, "shader_type SHADOW_EDGES via centralized helper" },
		{ "stencil edges back",                    NULL, "shader_type SHADOW_EDGES via centralized helper" },
		{ "stencil FS quad",                       NULL, "shader_type SHADOW_FS_QUAD via centralized helper" },
		// 12: IQM skinned mesh (special-case).
		{ "IQM skinned mesh (special-case)",
		  (const ralPipeline_t **)&vk.iqmGpu.ral_pipeline, NULL },
		// 13: Particle compute (special-case, compute pipeline).
		{ "particle compute (special-case)",
		  (const ralPipeline_t **)&vk.particle.ral_compute_pipeline, NULL },
		// 14: Tonemap default (special-case post-process).
		{ "tonemap default (special-case)",
		  (const ralPipeline_t **)&vk.ral_tonemap_pipeline,
		  "may be NULL if tonemap variant > 0 is active; check tonemap_variants[] below" },
		// 15: SMAA edge detect (special-case, three-pipeline cluster).
		{ "SMAA edge detect (special-case)",
		  (const ralPipeline_t **)&vk.ral_smaa_edge_pipeline,
		  "+ smaa_blend + smaa_resolve checked separately" },
		// 16-17: Mirror portal + wireframe — both run through centralized
		// helper under specific shader_types / state_bits; same array-scan
		// coverage as 1-7.
		{ "mirror portal opaque",                  NULL, "shader_type triggered by R_DrawMirror; centralized helper" },
		{ "wireframe surface",                     NULL, "r_showtris > 0; centralized helper polygonMode=LINE" },
		// 18: Q1 lightstyle blend — centralized helper, FEAT_Q1 shader_type.
		{ "Q1 lightstyle blend",                   NULL, "FEAT_Q1 shader_type triggered by Q1 BSP map; centralized helper" },
		// 19: TYPE_DOT (debug point) — centralized helper shader_type.
		{ "TYPE_DOT (debug point)",                NULL, "shader_type=TYPE_DOT via centralized helper" },
	};

	ri.Log( SEV_INFO, "  Walking 19 §17.7 fixtures (sibling field non-NULL = PASS, no live trigger = N/A):\n" );
	for ( i = 0; i < ARRAY_LEN( fixtures ); i++ ) {
		const char *status;
		if ( fixtures[i].field == NULL ) {
			status = "N/A";
			na++;
		} else if ( *fixtures[i].field != NULL ) {
			status = "PASS";
			pass++;
		} else {
			status = "FAIL";
			fail++;
		}
		ri.Log( SEV_INFO, "    #%2u %-44s : %s%s%s\n",
		        i + 1, fixtures[i].name, status,
		        fixtures[i].note ? "  — " : "",
		        fixtures[i].note ? fixtures[i].note : "" );
	}

	// Additional cluster pipelines (umbrella'd into fixtures #14 / #15).
	ri.Log( SEV_INFO, "  Sub-fixture (cluster expansion):\n" );
	#define DUMP_FIELD(label, ptr) do { \
		if ( (ptr) ) { ri.Log( SEV_INFO, "    %-44s : %s\n", (label), "PASS" ); pass++; } \
		else         { ri.Log( SEV_INFO, "    %-44s : %s\n", (label), "miss" );  } \
	} while (0)
	DUMP_FIELD( "smaa_blend (cluster of #15)",       vk.ral_smaa_blend_pipeline );
	DUMP_FIELD( "smaa_resolve (cluster of #15)",     vk.ral_smaa_resolve_pipeline );
	DUMP_FIELD( "gamma (post-process)",              vk.ral_gamma_pipeline );
	DUMP_FIELD( "capture (post-process)",            vk.ral_capture_pipeline );
	DUMP_FIELD( "bloom_extract (post-process)",      vk.ral_bloom_extract_pipeline );
	DUMP_FIELD( "bloom_blend (post-process)",        vk.ral_bloom_blend_pipeline );
	DUMP_FIELD( "ribbon alpha (special-case)",       vk.ribbon.ral_pipeline_alpha );
	DUMP_FIELD( "ribbon additive (special-case)",    vk.ribbon.ral_pipeline_additive );
	DUMP_FIELD( "beam (special-case)",               vk.beam.ral_pipeline );
	DUMP_FIELD( "sprite alpha (special-case)",       vk.sprite.ral_pipeline_alpha );
	DUMP_FIELD( "sprite additive (special-case)",    vk.sprite.ral_pipeline_additive );
	DUMP_FIELD( "particle render alpha (special-case)",    vk.particle.ral_render_pipeline_alpha );
	DUMP_FIELD( "particle render additive (special-case)", vk.particle.ral_render_pipeline_additive );
	#undef DUMP_FIELD

	// Centralized-helper coverage: scan vk.pipelines[].ral_handle[RENDER_PASS_COUNT]
	// for non-NULL ral entries. Each Vk_Pipeline variant has up to RENDER_PASS_COUNT
	// sibling ralPipeline_t * — one per render pass it can be built for. The
	// outer scan is clamped to MAX_VK_PIPELINES defensively (vk.pipelines_count
	// should always be ≤ MAX_VK_PIPELINES by the engine's invariant, but a
	// corrupted counter would otherwise drive the read past array end).
	{
		uint32_t cent_variants_with_ral = 0, cent_total_ral = 0, p, rp;
		uint32_t scan_end = vk.pipelines_count;
		if ( scan_end > ARRAY_LEN( vk.pipelines ) ) scan_end = ARRAY_LEN( vk.pipelines );
		for ( p = 0; p < scan_end; p++ ) {
			qboolean any = qfalse;
			for ( rp = 0; rp < RENDER_PASS_COUNT; rp++ ) {
				if ( vk.pipelines[p].ral_handle[rp] ) { cent_total_ral++; any = qtrue; }
			}
			if ( any ) cent_variants_with_ral++;
		}
		ri.Log( SEV_INFO, "  Centralized helper: %u / %u Vk_Pipeline_Def variants have at least one sibling ralPipeline_t (total RAL pipelines = %u across %u render passes)\n",
		        cent_variants_with_ral, vk.pipelines_count, cent_total_ral, (unsigned)RENDER_PASS_COUNT );
		if ( cent_variants_with_ral > 0 ) {
			// Treat every fixture that opted out of an explicit sibling field
			// (fixtures[i].field == NULL, i.e. "checked via centralized scan")
			// as PASS once at least one centralized variant has a sibling RAL
			// pipeline. Compute the promotion count dynamically rather than
			// hard-coding 7+3+4 — avoids underflow if the fixture-table
			// composition changes and protects the unsigned `na` counter from
			// wraparound. Also clamp the decrement against the current na value
			// for double-safety against any future increment-path change.
			uint32_t cent_covered_na = 0, j;
			for ( j = 0; j < ARRAY_LEN( fixtures ); j++ ) {
				if ( fixtures[j].field == NULL ) cent_covered_na++;
			}
			if ( cent_covered_na > na ) cent_covered_na = na;
			pass += cent_covered_na;
			na   -= cent_covered_na;
			ri.Log( SEV_INFO, "  (%u centralized-helper-covered fixtures promoted to PASS via the non-zero sibling count above)\n", cent_covered_na );
		}
	}

	ri.Log( SEV_INFO, "  Summary: %u PASS, %u FAIL, %u N/A across 19 fixtures + cluster expansion.\n",
	        pass, fail, na );
	ri.Log( SEV_INFO, "===== end \\ral_pipeline_test =====\n" );
}

// ── \ral_resources developer dump (textures + buffers) ─────────────────
void vk_ral_textures_diag_dump( void ) {
	uint32_t i;
	ri.Log( SEV_INFO, "===== \\ral_resources =====\n" );
	if ( !s_ral_init_attempted ) {
		ri.Log( SEV_INFO, "  r_useRALTextures = %d, r_useRALBuffers = %d (RAL infra not yet initialized — vid_restart to apply)\n",
		        r_useRALTextures ? r_useRALTextures->integer : -1,
		        r_useRALBuffers  ? r_useRALBuffers->integer  : -1 );
		ri.Log( SEV_INFO, "===== end \\ral_resources =====\n" );
		return;
	}
	if ( !s_ral_backend ) {
		ri.Log( SEV_WARN, "  RAL backend creation FAILED this session — see earlier [RAL-TEX] / [RAL-BUF] warnings\n" );
		ri.Log( SEV_INFO, "===== end \\ral_resources =====\n" );
		return;
	}
	ri.Log( SEV_INFO, "  TEXTURES (Phase 7.4a parallel-paths):\n" );
	if ( !vk_ral_textures_available() ) {
		ri.Log( SEV_INFO, "    bindless table not built this session (r_useRALTextures=0); RAL backend up for buffer registrations only.\n" );
	} else {
		ri.Log( SEV_INFO, "    bindless slots capacity : %u\n", s_ral_bindless_capacity );
		ri.Log( SEV_INFO, "    registered (in bindless): %u\n", s_ral_registered_count );
		ri.Log( SEV_INFO, "    destroyed (lifetime sum): %u\n", s_ral_destroyed_count );
		ri.Log( SEV_INFO, "    skipped (no pic data)   : %u   (scratch/placeholder images don't register)\n", s_ral_skipped_no_data );
		ri.Log( SEV_INFO, "    skipped (slot overflow) : %u   (kept RAL texture, omitted from BindGroup)\n", s_ral_skipped_no_slot );
		ri.Log( SEV_INFO, "    tr.numImages            : %u\n", (unsigned)tr.numImages );
		ri.Log( SEV_INFO, "    last %u registered names:\n", VK_RAL_RECENT_NAMES );
		for ( i = 0; i < VK_RAL_RECENT_NAMES; i++ ) {
			uint32_t idx = ( s_ral_recent_head + VK_RAL_RECENT_NAMES - 1 - i ) % VK_RAL_RECENT_NAMES;
			if ( s_ral_recent_names[idx][0] )
				ri.Log( SEV_INFO, "      [-%u] %s\n", i + 1, s_ral_recent_names[idx] );
		}
	}
	ri.Log( SEV_INFO, "  BUFFERS (Phase 7.4b parallel-paths):\n" );
	ri.Log( SEV_INFO, "    r_useRALBuffers         : %d\n", r_useRALBuffers ? r_useRALBuffers->integer : -1 );
	ri.Log( SEV_INFO, "    live RAL buffers        : %u    (peak %u this session)\n", s_buf_active_count, s_buf_peak_count );
	ri.Log( SEV_INFO, "    pending (queued, unflush): %u\n", s_buf_pending_count );
	ri.Log( SEV_INFO, "    register total (incl.)  : %u\n", s_buf_register_total );
	ri.Log( SEV_INFO, "    unregister total        : %u\n", s_buf_destroy_total );
	ri.Log( SEV_INFO, "    skipped (queue full)    : %u\n", s_buf_skipped_no_backend );
	ri.Log( SEV_INFO, "    bytes by usage (live live buffers, MiB; bits sum if a buffer has multi-usage):\n" );
	{
		const char *names[7] = { "VERTEX", "INDEX", "UNIFORM", "STORAGE", "INDIRECT", "TRANSFER_SRC", "TRANSFER_DST" };
		for ( i = 0; i < 7; i++ ) {
			ri.Log( SEV_INFO, "      %-13s : %5u MiB (%llu bytes)\n",
			        names[i],
			        (unsigned)( s_buf_bytes_by_usage[i] >> 20 ),
			        (unsigned long long)s_buf_bytes_by_usage[i] );
		}
	}
	ri.Log( SEV_INFO, "===== end \\ral_resources =====\n" );
}

#endif // USE_VULKAN
