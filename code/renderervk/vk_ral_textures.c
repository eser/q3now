// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// vk_ral_textures.c — texture migration support. See header
//
// rilog-channel-mechanism — routed onto `renderer.ral`.
// docblock for the parallel-paths architecture; this TU implements:
//   * persistent RAL backend instance owned by the renderer
//   * the bindless SAMPLED_IMAGE BindGroup the migration populates as
//     R_CreateImage adds textures (consumer arrives later)
//   * \ral_textures developer-command body (slot population + last-five
//     registered names)
//
// Only the asset-image path (R_CreateImage from disk / generated content)
// is migrated here. Lightmap atlas, dynamic glyphs, scratch images, and
// render-target attachments stay on the legacy path — they remain in the
// follow-up turn inventory.

#include "tr_local.h"
#include "../renderercommon/r_log.h"  // rilog-channel-mechanism — renderer.ral

R_LOG_DECLARE_CHANNEL( rch_ral,         "renderer.ral"         );
R_LOG_DECLARE_CHANNEL( rch_ral_texture, "renderer.ral.texture" );
R_LOG_DECLARE_CHANNEL( rch_ral_buffer,  "renderer.ral.buffer"  );

#ifdef USE_VULKAN

#include <stdlib.h>     // vkRalActiveBuffer_t nodes allocate via stdlib malloc/free
                        // so they survive ri.FreeAll() in R_InitImages (renderer
                        // zone wipe).

#include "vk_ral_textures.h"

// The RAL public surface + the Vulkan-backend-internal accessors (the
// latter only because \ral_textures dumps the underlying VkImage for
// diagnostics; the renderer never USES the RAL VkImage beyond
// dumping it). ral/ral.h is the right include for everything else.
#include "../renderer/ral/ral.h"

// ── module state ────────────────────────────────────────────────────────
static ralBackend_t         *s_ral_backend;
static ralBindGroupLayout_t *s_ral_bindless_layout;
static ralBindGroup_t       *s_ral_bindless_set;
static uint32_t              s_ral_bindless_capacity;     // resolved bindless slot count (= min(RAL caps, requested))
static vkBindlessPublicationLedger_t s_bindless_publication;
static qboolean              s_bindless_publication_initialized;
static uint64_t              s_bindless_owner_generation;
// 2D bindless slot allocator (Phase 7.15.2). Replaces the old tr.numImages-1
// monotonic source so slots can be recycled once eviction (7.15.4) frees them.
// s_ral_bindless_next is the high-water mark for never-yet-used slots; the
// free-list holds slots returned by vk_ral_unregister_image. alloc() pops a
// recycled slot first, else hands out the next high-water slot. Both reset to
// empty/0 on map reload / vid_restart (the descriptor set is recreated there, so
// prior slots are invalid anyway), so first image still gets slot 0 — byte
// identical to the tr.numImages-1 path while the free-list stays empty (no
// eviction caller exists in 7.15.2). Sized to the full slot count: at most one
// freed slot can exist per content slot.
static uint32_t              s_ral_bindless_next;         // high-water: next never-used slot
static uint32_t              s_ral_bindless_free[ WIRED_BINDLESS_TEX_SLOTS ];
static uint32_t              s_ral_bindless_free_count;   // recycled slots available to reuse
static uint32_t              s_ral_registered_count;      // textures populated in the bindless set so far
static uint32_t              s_ral_upload_sync_count;     // uploads that took the synchronous (resident-on-return) path
static uint32_t              s_ral_upload_async_count;    // uploads that took the async transfer path (deferred residency)
static uint32_t              s_ral_skipped_no_slot;       // textures created but slot index overflowed s_ral_bindless_capacity
static uint32_t              s_ral_skipped_no_data;       // R_CreateImage calls with pic=NULL (scratch/placeholder)
static uint32_t              s_ral_destroyed_count;       // textures destroyed via vk_ral_unregister_image
static image_t              *s_ral_mip_test_image;        // default-inert parent-view hold owner
static int                   s_ral_mip_test_resource = -1;
static int                   s_ral_mip_test_start_frame;
static int                   s_ral_mip_test_upload_frame;
static uint64_t              s_ral_mip_test_upload_bytes;
static ralUploadTicket_t     s_ral_mip_test_ticket;
#define VK_RAL_MATERIAL_PLANES 2
static image_t              *s_ral_material_images[VK_RAL_MATERIAL_PLANES];
static ralTextureView_t     *s_ral_material_coarse[VK_RAL_MATERIAL_PLANES];
static ralUploadTicket_t     s_ral_material_tickets[VK_RAL_MATERIAL_PLANES];
static ralResidencyPageRecord_t s_ral_material_group;
static uint8_t               s_ral_material_completed;
static int                   s_ral_material_start_frame;
static void                  vk_ral_material_reset( qboolean restoreViews );

// Phase 7.15.4-c automatic eviction — Option-A cross-thread hand-off. The 1 Hz
// poll thread (ralVk_PollThreadProc) calls vk_ral_on_memory_pressure on level
// transitions; on CRITICAL it ONLY raises this flag (a single-word write — same
// single-producer/single-consumer volatile-int idiom as the backend's
// pollThreadStop). It does NOT touch RAL / tr.images[] / the free-list — those
// would race the render thread (that is the unsafe Option-B the grounding ruled
// out). The render thread reads + clears the flag in vk_ral_drain_evictions at the
// per-frame safe boundary and does ALL the actual eviction there. The flag is the
// ONLY cross-thread shared state for eviction.
static volatile int          s_ral_evict_requested;       // poll-thread→render-thread mark (CRITICAL pressure)
// parallel slot counter for the 2DArray bindless binding
// (set 7, binding=WIRED_BINDLESS_BIND_ARRAY_IMAGES). Independent index
// space from the 2D table at binding 0; allocated by
// vk_ral_register_image_array as R_CreateImageArray creates content
// 2D-array textures (q1_ls_array.frag's animArrays today). No recycling
// — slots live for the map's lifetime, same as 2D content.
static uint32_t              s_ral_bindless_array_count;
static uint32_t              s_ral_skipped_no_array_slot; // R_CreateImageArray calls past WIRED_BINDLESS_ARRAY_TEX_SLOTS
#define VK_RAL_RECENT_NAMES   5
static char                  s_ral_recent_names[ VK_RAL_RECENT_NAMES ][ MAX_QPATH ];
static uint32_t              s_ral_recent_head;           // ring write cursor
static qboolean              s_ral_init_attempted;        // sticky: only retry init once per vid_init
static qboolean              s_ral_full_warned;           // one-shot for the bindless-capacity-exhausted log
static void                  vk_ral_warn_bindless_full( void );   // fwd
static uint32_t              vk_ral_alloc_bindless_slot( const image_t *image );   // fwd

// Pending async-upload residency list: an entry per texture whose async transfer
// upload had not completed when it was registered. Each frame, vk_ral_drain_pending_
// uploads() polls the fence; when the copy is done it swaps the real texture into the
// bindless slot (replacing the *default placeholder) only after a batched
// graphics acquire waits the ticket's transfer-ready semaphore for its exact
// mip/layer range. The slot holds the resident placeholder until then — never
// black, never stale.
typedef struct {
	ralUploadTicket_t ticket;
	uint32_t          slot;
	image_t          *image;
} vk_ral_pending_upload_t;
#define VK_RAL_MAX_PENDING_UPLOADS 4096
static vk_ral_pending_upload_t s_ral_pending_uploads[ VK_RAL_MAX_PENDING_UPLOADS ];
static uint32_t                s_ral_pending_upload_count;
static uint32_t                s_ral_pending_peak;
static void                  vk_ral_on_memory_pressure( struct ralBackend_s *b, ralPressureLevel_t level, const ralMemoryBudget_t *budget, void *user );   // fwd

static void vk_ral_release_upload_ticket( ralUploadTicket_t *ticket ) {
	if ( !ticket ) return;
	// Teardown/unregister may arrive before the per-frame residency poll. A
	// submitted copy owns its staging/fence/semaphore cohort until the fence
	// completes; block here before releasing those children or the texture parent.
	if ( ticket->transfer.state == RAL_TRANSFER_SUBMITTED && ticket->fence
			&& !Ral_TextureUploadTicketComplete( ticket ) ) {
		Ral_WaitFence( ticket->fence, RAL_TIMEOUT_INFINITE );
	}
	(void)Ral_TextureUploadTicketComplete( ticket );
	if ( ticket->fence ) Ral_DestroyFence( ticket->fence );
	if ( ticket->readySemaphore ) Ral_DestroySemaphore( ticket->readySemaphore );
	memset( ticket, 0, sizeof( *ticket ) );
}

static qboolean vk_ral_upload_ticket_complete( ralUploadTicket_t *ticket ) {
	return ticket && ticket->fence && Ral_FenceSignaled( ticket->fence )
		&& Ral_TextureUploadTicketComplete( ticket );
}

// ── RAL buffer parallel-paths tracker ─────────────────────
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
// internal-texture adoption (depth, color, tonemapped,
// SMAA input/edges/blend). GPU timestamp queries are native RAL resources
// owned directly by vk_gpu_ts_init/vk_gpu_ts_shutdown in vk.c.

// boot-time adoption of every allocate-once
// VkDescriptorSet wrapped into a ralBindGroup_t with ownsSet=qfalse so the
// renderer's existing vkResetDescriptorPool / vk_destroy_descriptor_pools
// path keeps lifetime ownership. The wrappers are pure metadata + a stable
// RAL handle that the cmd-record migration can pass to
// Ral_CmdBindBindGroup. Per-draw rotating sets (vk.cmd->descriptor_set.current[])
// are NOT adopted here — they rotate per shader_type per frame and need
// per-frame re-adoption infrastructure introduced by the cmd-record migration.
//
// Centralized registry (rather than per-subsystem sibling fields) — the
// cmd-record migration refactors into sibling lookup at each bind site as it
// threads ralCommandBuffer_t through the cmd path. For now this turn delivers
// the wrappers' existence + lifecycle + count log; bind-site consumers fold
// in next turn.
#define VK_RAL_MAX_ADOPTED_BGS  64u
static ralBindGroup_t      *s_adopted_bgs[ VK_RAL_MAX_ADOPTED_BGS ];
static uint32_t             s_adopted_bgs_count;

static qboolean vk_ral_bindless_ledger_activate( void );

// ── lifecycle ───────────────────────────────────────────────────────────
qboolean vk_ral_textures_available( void ) {
	return ( s_ral_backend && s_ral_bindless_set ) ? qtrue : qfalse;
}

qboolean vk_ral_bindless_get_cohort(
		vkRalBindlessCohortReceipt_t *outReceipt ) {
	if ( !outReceipt || !s_ral_backend || !s_ral_bindless_layout
			|| !s_ral_bindless_set || !vk_ral_bindless_ledger_activate() )
		return qfalse;
	return VK_BindlessCohortBuild( s_ral_backend, s_ral_bindless_layout,
		s_ral_bindless_set, Ral_GetBindGroupLayoutHandle( s_ral_bindless_layout ),
		Ral_GetBindGroupHandle( s_ral_bindless_set ),
		s_bindless_publication_initialized, &s_bindless_publication, outReceipt );
}

static uint64_t vk_ral_bindless_sampler_digest( const Vk_Sampler_Def *definition ) {
	uint64_t h = 1469598103934665603ull;
	uint32_t fields[5];
	uint32_t i, j;
	if ( !definition ) return 0;
	fields[0] = (uint32_t)definition->address_mode;
	fields[1] = (uint32_t)definition->gl_mag_filter;
	fields[2] = (uint32_t)definition->gl_min_filter;
	fields[3] = (uint32_t)definition->max_lod_1_0;
	fields[4] = (uint32_t)definition->noAnisotropy;
	for ( i = 0; i < 5; ++i ) for ( j = 0; j < 4; ++j ) {
		h ^= ( fields[i] >> ( j * 8u ) ) & 0xffu;
		h *= 1099511628211ull;
	}
	return h ? h : 1u;
}

static qboolean vk_ral_bindless_ledger_activate( void ) {
	if ( !s_ral_bindless_set ) return qfalse;
	if ( !s_bindless_publication_initialized ) {
		VK_BindlessPublicationInit( &s_bindless_publication );
		s_bindless_publication_initialized = qtrue;
	}
	if ( s_bindless_publication.samplerPoolIdentity != (uintptr_t)&vk.samplers
			&& !VK_BindlessPublicationActivateSamplerPool(
				&s_bindless_publication, &vk.samplers ) ) return qfalse;
	if ( s_bindless_publication.setIdentity != (uintptr_t)s_ral_bindless_set
			&& !VK_BindlessPublicationActivateSet(
				&s_bindless_publication, s_ral_bindless_set ) ) return qfalse;
	return qtrue;
}

static qboolean vk_ral_bindless_image_owner( image_t *image ) {
	if ( !image ) return qfalse;
	if ( image->bindlessOwnerGeneration ) return qtrue;
	if ( s_bindless_owner_generation == UINT64_MAX ) return qfalse;
	image->bindlessOwnerGeneration = ++s_bindless_owner_generation;
	return image->bindlessOwnerGeneration ? qtrue : qfalse;
}

static void vk_ral_bindless_poison_active( void ) {
	if ( s_bindless_publication_initialized && s_ral_bindless_set
			&& s_bindless_publication.setIdentity == (uintptr_t)s_ral_bindless_set )
		(void)VK_BindlessPublicationPoisonSetAfterWrite(
			&s_bindless_publication, s_ral_bindless_set );
}

static qboolean vk_ral_bindless_record_views( image_t *const *images,
		const uint32_t *slots, const void *const *nativeViews,
		const vkBindlessPublicationKind_t *kinds, uint32_t count ) {
	const void *owners[VK_RAL_MATERIAL_PLANES];
	const void *descriptors[VK_RAL_MATERIAL_PLANES];
	uint64_t generations[VK_RAL_MATERIAL_PLANES];
	uint32_t i;
	if ( !images || !slots || !nativeViews || !kinds || !count
			|| count > VK_RAL_MATERIAL_PLANES || !vk_ral_bindless_ledger_activate() ) {
		vk_ral_bindless_poison_active(); return qfalse;
	}
	for ( i = 0; i < count; ++i ) {
		if ( !vk_ral_bindless_image_owner( images[i] ) ) {
			vk_ral_bindless_poison_active(); return qfalse;
		}
		owners[i] = images[i];
		descriptors[i] = images[i]->descriptor
			? (const void *)images[i]->descriptor : (const void *)images[i];
		generations[i] = images[i]->bindlessOwnerGeneration;
	}
	if ( VK_BindlessPublicationViews( &s_bindless_publication,
			s_ral_bindless_set, slots, nativeViews, owners, descriptors,
			generations, kinds, count ) ) return qtrue;
	vk_ral_bindless_poison_active();
	return qfalse;
}

qboolean vk_ral_bindless_publish_texture_view( image_t *image,
		uint32_t slot, ralTextureView_t *view,
		vkBindlessPublicationKind_t kind ) {
	const void *nativeView;
	image_t *images[1] = { image };
	if ( !s_ral_bindless_set || !view ) return qfalse;
	nativeView = Ral_GetTextureViewHandle( view );
	if ( !nativeView ) return qfalse;
	Ral_BindGroupSetTextureViewAt( s_ral_bindless_set, slot, view );
	return vk_ral_bindless_record_views( images, &slot, &nativeView, &kind, 1 );
}

qboolean vk_ral_bindless_publish_texture_views( image_t *const *images,
		const uint32_t *slots, ralTextureView_t *const *views,
		const vkBindlessPublicationKind_t *kinds, uint32_t count ) {
	const void *nativeViews[VK_RAL_MATERIAL_PLANES];
	uint32_t i;
	if ( !images || !slots || !views || !kinds || !count
			|| count > VK_RAL_MATERIAL_PLANES || !s_ral_bindless_set ) return qfalse;
	for ( i = 0; i < count; ++i ) {
		nativeViews[i] = Ral_GetTextureViewHandle( views[i] );
		if ( !nativeViews[i] ) return qfalse;
	}
	if ( !Ral_BindGroupSetTextureViewsAt( s_ral_bindless_set, slots, views, count ) )
		return qfalse;
	return vk_ral_bindless_record_views( images, slots, nativeViews, kinds, count );
}

qboolean vk_ral_bindless_publish_texture( image_t *image, uint32_t slot,
		ralTexture_t *texture, vkBindlessPublicationKind_t kind ) {
	const void *nativeView;
	image_t *images[1] = { image };
	if ( !s_ral_bindless_set || !texture ) return qfalse;
	nativeView = Ral_GetTextureDefaultViewHandle( texture );
	if ( !nativeView ) return qfalse;
	Ral_BindGroupSetTextureAt( s_ral_bindless_set, slot, texture );
	return vk_ral_bindless_record_views( images, &slot, &nativeView, &kind, 1 );
}

qboolean vk_ral_bindless_publish_sampler( uint32_t slot, VkSampler sampler,
		const Vk_Sampler_Def *definition ) {
	ralSampler_t *adopted;
	const void *identity = (const void *)sampler;
	uint64_t digest;
	if ( !sampler || !definition || !s_ral_backend || !s_ral_bindless_set ) return qfalse;
	digest = vk_ral_bindless_sampler_digest( definition );
	adopted = Ral_AdoptSampler( s_ral_backend, (void *)identity,
		"vk-bindless-sampler-ledger" );
	if ( !adopted ) return qfalse;
	Ral_BindGroupSetSamplerAt( s_ral_bindless_set, slot, adopted );
	Ral_DestroySampler( adopted );
	if ( !vk_ral_bindless_ledger_activate()
			|| !VK_BindlessPublicationSamplers( &s_bindless_publication,
				s_ral_bindless_set, &vk.samplers, &slot, &identity, &digest, 1 ) ) {
		vk_ral_bindless_poison_active();
		return qfalse;
	}
	return qtrue;
}

qboolean vk_ral_bindless_record_raw_image( image_t *image, uint32_t slot,
		VkImageView view, vkBindlessPublicationKind_t kind ) {
	const void *nativeView = (const void *)view;
	image_t *images[1] = { image };
	if ( !view ) { vk_ral_bindless_poison_active(); return qfalse; }
	return vk_ral_bindless_record_views( images, &slot, &nativeView, &kind, 1 );
}

qboolean vk_ral_bindless_record_legacy_exact( image_t *image,
		uint32_t slot, VkImageView view ) {
	if ( !image || !image->descriptor ) {
		vk_ral_bindless_poison_active(); return qfalse;
	}
	return vk_ral_bindless_record_raw_image( image, slot, view,
		VK_BINDLESS_PUBLICATION_LEGACY_EXACT );
}

qboolean vk_ral_bindless_record_reserved( uint32_t slot, VkImageView view,
		const void *ownerIdentity, const void *descriptorIdentity ) {
	const void *nativeView = (const void *)view;
	uint64_t generation;
	vkBindlessPublicationKind_t kind = VK_BINDLESS_PUBLICATION_RESERVED;
	if ( !view || !ownerIdentity || !descriptorIdentity
			|| s_bindless_owner_generation == UINT64_MAX
			|| !vk_ral_bindless_ledger_activate() ) {
		vk_ral_bindless_poison_active(); return qfalse;
	}
	generation = ++s_bindless_owner_generation;
	if ( VK_BindlessPublicationViews( &s_bindless_publication,
			s_ral_bindless_set, &slot, &nativeView, &ownerIdentity,
			&descriptorIdentity, &generation, &kind, 1 ) ) return qtrue;
	vk_ral_bindless_poison_active();
	return qfalse;
}

qboolean vk_ral_bindless_tombstone( uint32_t slot ) {
	if ( !vk_ral_bindless_ledger_activate() ) return qfalse;
	Ral_BindGroupSetTextureAt( s_ral_bindless_set, slot, NULL );
	if ( VK_BindlessPublicationTombstoneImage(
			&s_bindless_publication, s_ral_bindless_set, slot ) ) return qtrue;
	(void)VK_BindlessPublicationPoisonSetAfterWrite(
		&s_bindless_publication, s_ral_bindless_set );
	return qfalse;
}

qboolean vk_ral_bindless_query_ordinary( const image_t *image,
		vkBindlessOrdinaryReceipt_t *outReceipt ) {
	int samplerSlot;
	if ( !image || !outReceipt || image->ralBindlessSlot < 0
			|| image->bindlessSamplerSlot < 0 || !image->bindlessOwnerGeneration
			|| image->view == VK_NULL_HANDLE || image->descriptor == VK_NULL_HANDLE )
		return qfalse;
	samplerSlot = image->bindlessSamplerSlot;
	if ( samplerSlot >= vk.samplers.count ) return qfalse;
	return VK_BindlessPublicationQueryOrdinary( &s_bindless_publication,
		s_ral_bindless_set, &vk.samplers, (uint32_t)image->ralBindlessSlot,
		(const void *)image->view, image, (const void *)image->descriptor,
		image->bindlessOwnerGeneration, (uint32_t)samplerSlot,
		(const void *)vk.samplers.handle[samplerSlot],
		vk_ral_bindless_sampler_digest( &vk.samplers.def[samplerSlot] ), outReceipt );
}

void vk_ral_bindless_sampler_pool_invalidate( void ) {
	if ( s_bindless_publication_initialized
			&& s_bindless_publication.samplerPoolIdentity == (uintptr_t)&vk.samplers )
		(void)VK_BindlessPublicationInvalidateSamplerPool(
			&s_bindless_publication, &vk.samplers );
}

// backend accessor for vk.c::create_pipeline +
// the 16 special-case sites. Returns the imported-mode backend (or NULL).
struct ralBackend_s *vk_ral_get_backend( void ) {
	return s_ral_backend;
}

// GPU memory budget for the /meminfo GPU section (re.GetMemoryBudget). Pulls the
// abstract ralMemoryBudget_t through the RAL boundary (no Vulkan-specific budget
// semantics here — this consumes the portable struct, so it works as-is on Metal
// / WebGPU once those backends fill it). The pressure level mirrors the RAL poll
// thread's classification (warning ≥75 %, critical ≥90 % of the tighter heap) so
// the meminfo level and the pressure-callback level agree. Returns qtrue when the
// backend reports real numbers (memoryBudget cap), qfalse when they are the
// RAL-tracked-footprint estimate; out-params may be NULL.
qboolean vk_ral_query_memory_budget( uint64_t *dlUsed, uint64_t *dlBudget,
                                     uint64_t *hvUsed, uint64_t *hvBudget,
                                     int *pressureLevel ) {
	ralMemoryBudget_t mb;
	const ralCaps_t  *caps;
	uint32_t          dlPm, hvPm, pm;
	int               level;

	if ( dlUsed )   *dlUsed   = 0;
	if ( dlBudget ) *dlBudget = 0;
	if ( hvUsed )   *hvUsed   = 0;
	if ( hvBudget ) *hvBudget = 0;
	if ( pressureLevel ) *pressureLevel = 0;

	if ( !s_ral_backend )
		return qfalse;

	Ral_QueryMemoryBudget( s_ral_backend, &mb );
	if ( dlUsed )   *dlUsed   = mb.deviceLocalUsed;
	if ( dlBudget ) *dlBudget = mb.deviceLocalBudget;
	if ( hvUsed )   *hvUsed   = mb.hostVisibleUsed;
	if ( hvBudget ) *hvBudget = mb.hostVisibleBudget;

	// Per-mille usage of the tighter heap → 0/1/2 (same thresholds as the poller).
	dlPm = ( mb.deviceLocalBudget > 0 ) ? (uint32_t)( ( mb.deviceLocalUsed * 1000ull ) / mb.deviceLocalBudget ) : 0;
	hvPm = ( mb.hostVisibleBudget > 0 ) ? (uint32_t)( ( mb.hostVisibleUsed * 1000ull ) / mb.hostVisibleBudget ) : 0;
	pm   = ( dlPm > hvPm ) ? dlPm : hvPm;
	level = ( pm >= 900 ) ? 2 : ( pm >= 750 ) ? 1 : 0;
	if ( pressureLevel ) *pressureLevel = level;

	caps = Ral_GetCaps( s_ral_backend );
	return ( caps && caps->memoryBudget ) ? qtrue : qfalse;
}

// bindless-ral-consolidate — accessors for the RAL-owned bindless layout + set.
struct ralBindGroupLayout_s *vk_ral_get_bindless_layout( void ) {
	return s_ral_bindless_layout;
}
struct ralBindGroup_s *vk_ral_get_bindless_set( void ) {
	return s_ral_bindless_set;
}

// single-phase RAL bringup. RAL owns instance +
// messenger + surface + physical device + queue families + features +
// VkDevice in one shot, gated by bci.letBackendOwnInstance=qtrue +
// bci.letBackendOwnDevice=qtrue. The earlier two-phase contract
// (vk_ral_adopt_device + Ral_AdoptDeviceAndQueues) is retired.
//
// The renderer reads vk.instance / vk.physical_device / vk.device back
// via Ral_Get*Handle accessors after Ral_CreateBackend returns. Side-
// effect state (vk.wideLines / vk.fragmentStores / vk.samplerAnisotropy /
// vk_fse_ext_enabled / vk.debugMarkers / vk.dedicatedAllocation) gets
// populated by init_vulkan_library's post-boot iteration over
// Ral_GetCaps + Ral_GetEnabledDeviceExtensions.
#if defined(_WIN32)
#  ifndef VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME
#    define VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME "VK_EXT_full_screen_exclusive"
#  endif
#endif

static const char *const kPlatformDeviceExts[] = {
	VK_EXT_HDR_METADATA_EXTENSION_NAME,
	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
	VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
	// Variable-rate shading (pipeline rate). Requested-if-available: the RAL backend
	// intersects this with device support and enables the feature only when present, so
	// it is a no-op on hardware that lacks it (the variableRateShading cap stays false →
	// consumers fall back to 1x1). create_renderpass2 (its formal dependency) is core on
	// the 1.3-class device this engine requires.
	VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
#if defined(_WIN32)
	VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,
#endif
};

static void *vk_ral_host_get_proc( void *userData, void *nativeInstance,
	                               const char *name ) {
	(void)userData;
	return ri.VK_GetInstanceProcAddr
	     ? ri.VK_GetInstanceProcAddr( (VkInstance)nativeInstance, name ) : NULL;
}

static qboolean vk_ral_host_create_surface( void *userData, void *platformHandle,
	                                         void *nativeInstance,
	                                         uint64_t *outNativeSurface ) {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	(void)userData;
	// The engine adapter intentionally preserves the existing main-window
	// callback.  A standalone SDL host supplies its own callback and consumes
	// platformHandle directly; RAL itself never reaches the engine-global window.
	(void)platformHandle;
	if ( outNativeSurface ) *outNativeSurface = 0;
	if ( !outNativeSurface || !ri.VK_CreateSurface
	  || !ri.VK_CreateSurface( (VkInstance)nativeInstance, &surface )
	  || surface == VK_NULL_HANDLE ) return qfalse;
	*outNativeSurface = (uint64_t)(uintptr_t)surface;
	return qtrue;
}

static void vk_ral_host_log( void *userData, ralLogSeverity_t severity,
	                         const char *message ) {
	log_severity_t engineSeverity;
	(void)userData;
	switch ( severity ) {
		case RAL_LOG_TRACE:
		case RAL_LOG_DEBUG: engineSeverity = SEV_DEBUG; break;
		case RAL_LOG_INFO:  engineSeverity = SEV_INFO;  break;
		case RAL_LOG_WARN:  engineSeverity = SEV_WARN;  break;
		case RAL_LOG_ERROR: engineSeverity = SEV_ERROR; break;
		case RAL_LOG_FATAL: engineSeverity = SEV_FATAL; break;
		default:            engineSeverity = SEV_ERROR; break;
	}
	if ( message ) R_LOG( rch_ral, engineSeverity, "%s", message );
}

qboolean vk_ral_boot_backend( void ) {
	ralBackendCreateInfo_t bci;
	cvar_t                *r_device, *r_anisotropic;
	uint32_t               platformInstanceExtensionCount = 0;
	const char *const     *platformInstanceExtensions = NULL;

	if ( s_ral_backend != NULL ) return qtrue;                // idempotent (vid_restart re-entry)

	memset( &bci, 0, sizeof( bci ) );
	bci.type  = RAL_BACKEND_VULKAN;
	bci.flags = RAL_FLAG_DEBUG_LABELS;
	bci.host.userData       = NULL;
	bci.host.getProcAddress = vk_ral_host_get_proc;
	bci.host.createSurface  = vk_ral_host_create_surface;
	bci.host.log            = vk_ral_host_log;
	bci.letBackendOwnInstance = qtrue;
	bci.letBackendOwnDevice   = qtrue;
	if ( !ri.VK_GetInstanceExtensions ) {
		R_LOG( rch_ral, SEV_ERROR, "vk_ral_boot_backend: platform instance-extension provider unavailable\n" );
		return qfalse;
	}
	platformInstanceExtensions = ri.VK_GetInstanceExtensions( &platformInstanceExtensionCount );
	if ( !platformInstanceExtensions || platformInstanceExtensionCount == 0 ) {
		R_LOG( rch_ral, SEV_ERROR, "vk_ral_boot_backend: platform instance-extension list is empty\n" );
		return qfalse;
	}
	bci.platformInstanceExtensions     = platformInstanceExtensions;
	bci.platformInstanceExtensionCount = platformInstanceExtensionCount;
	r_device = ri.Cvar_Get ? ri.Cvar_Get( "r_device", "-1", 0 ) : NULL;
	bci.preferredDeviceIndex  = r_device ? r_device->integer : -1;
	// Vulkan validation layers + the debug-utils messenger are gated on the
	// r_vkValidate diagnostic cvar (CVAR_LATCH: the value is fixed before this
	// backend/instance is created, so `+set r_vkValidate 1` on the command line is
	// honored here). A diagnostic-only switch — it gates the validation layer and
	// the messenger, never render behavior. Read directly (latched) rather than
	// caching a handle, since the backend may be (re)created before tr_init's cvar
	// registration on some paths; Cvar_Get is idempotent and returns the latched
	// value. Default 0 (validation is a debug run-mode, not shipped-on).
	{
		cvar_t *vkValidate = ri.Cvar_Get ? ri.Cvar_Get( "r_vkValidate", "0", CVAR_ARCHIVE | CVAR_LATCH ) : NULL;
		bci.enableValidation = ( vkValidate && vkValidate->integer ) ? qtrue : qfalse;
	}
	{
		cvar_t *asyncUpload = ri.Cvar_Get ? ri.Cvar_Get( "r_asyncTextureUpload", "1", CVAR_ARCHIVE ) : NULL;
		bci.allowAsyncTextureUploads = ( asyncUpload && asyncUpload->integer ) ? qtrue : qfalse;
	}

	// Renderer-requested device features. Single intent per bundle —
	// RAL owns the paired-bundle AND-gate logic (K14). HARD-REQUIRED
	// features (fillModeNonSolid, sync2, timelineSemaphore,
	// dynamicRendering) are RAL-internal and not expressed here (K15).
	bci.requestFeatures.wantShaderInt64           = qtrue;
	bci.requestFeatures.wantWideLines             = qtrue;
	bci.requestFeatures.wantVertexFragmentStores  = qtrue;
	bci.requestFeatures.wantDescriptorIndexing    = qtrue;
	bci.requestFeatures.wantHostQueryReset        = qtrue;
	bci.requestFeatures.wantDrawIndirectCount     = qtrue;
	bci.requestFeatures.wantVulkanMemoryModel     = qtrue;
	bci.requestFeatures.wantBufferDeviceAddress   = qtrue;
	bci.requestFeatures.want8BitStorage           = qtrue;
	bci.requestFeatures.wantFragmentShadingRate   = qtrue;   // pipeline-rate VRS; enabled only if the device supports it (cap stays false otherwise)
	bci.requestFeatures.wantDepthClamp            = qtrue;   // near/far depth-clamp raster state (free); enabled only if the device supports it (cap stays false → projection-tweak fallback). r_depthClamp gates runtime use, not the device request.
	bci.requestFeatures.wantIndependentBlend      = qtrue;   // per-attachment MRT state; runtime users still cap-gate before creating heterogeneous pipelines.
	r_anisotropic = ri.Cvar_Get ? ri.Cvar_Get( "r_ext_texture_filter_anisotropic", "1", 0 ) : NULL;
	bci.requestFeatures.wantSamplerAnisotropy     = ( r_anisotropic && r_anisotropic->integer != 0 ) ? qtrue : qfalse;

	bci.platformDeviceExtensions      = kPlatformDeviceExts;
	bci.platformDeviceExtensionCount  = (uint32_t)ARRAY_LEN( kPlatformDeviceExts );

	s_ral_backend = Ral_CreateBackend( &bci );
	if ( s_ral_backend == NULL ) {
		R_LOG( rch_ral, SEV_ERROR, "vk_ral_boot_backend: Ral_CreateBackend returned NULL\n" );
		return qfalse;
	}

	// Read back instance + physicalDevice + device into renderer-side
	// aliases. 537 vk.device readers + 21 vk.instance + 15 vk.physical_
	// device readers preserve transparently via alias.
	vk.instance        = (VkInstance)       Ral_GetInstanceHandle      ( s_ral_backend );
	vk.physical_device = (VkPhysicalDevice) Ral_GetPhysicalDeviceHandle( s_ral_backend );
	vk.device          = (VkDevice)         Ral_GetDeviceHandle        ( s_ral_backend );

	R_LOG( rch_ral, SEV_INFO, "RAL backend brought up (single-phase owned-instance + owned-device)\n" );

	// Register a pressure listener so the 1 Hz budget poller is not inert: it now
	// starts and reports level transitions. This is the consuming end of the
	// pressure signal (a telemetry sink) — eviction/upload-gating consumers hook
	// the same callback when they land. Registering a non-NULL callback is what
	// starts the poll thread (Ral_SetPressureCallback); de-registered at teardown.
	Ral_SetPressureCallback( s_ral_backend, vk_ral_on_memory_pressure, NULL );

	return qtrue;
}

// Pressure-signal listener (the consuming end — closes the producer-with-no-
// consumer gap). Fires on level transitions only (the poller dedupes), off the
// render hot path. Logs WARNING/CRITICAL so memory pressure is observable; a
// future texture-LRU eviction / upload gate hooks here without re-plumbing the
// producer. The budget is the abstract ralMemoryBudget_t — backend-agnostic.
static void vk_ral_on_memory_pressure( struct ralBackend_s *b, ralPressureLevel_t level,
                                const ralMemoryBudget_t *budget, void *user ) {
	(void)b; (void)user;
	if ( level == RAL_PRESSURE_CRITICAL ) {
		R_LOG( rch_ral, SEV_WARN, "GPU memory CRITICAL: device-local %u / %u MiB — evict/drop recommended\n",
			(unsigned)( budget->deviceLocalUsed >> 20 ), (unsigned)( budget->deviceLocalBudget >> 20 ) );
		// Phase 7.15.4-c Option-A: poll-thread MARKS ONLY. The render thread reads
		// this in vk_ral_drain_evictions and does the actual eviction at the
		// per-frame safe boundary. Raising a single volatile int is the entire
		// poll-thread side — no RAL call, no tr.images[] scan, no free-list op here
		// (those would race the render thread). The callback fires only on the
		// transition INTO critical, which gives clean re-arm (it won't re-fire every
		// poll while still critical).
		s_ral_evict_requested = 1;
	} else if ( level == RAL_PRESSURE_WARNING ) {
		R_LOG( rch_ral, SEV_WARN, "GPU memory warning: device-local %u / %u MiB — pause non-essential uploads\n",
			(unsigned)( budget->deviceLocalUsed >> 20 ), (unsigned)( budget->deviceLocalBudget >> 20 ) );
	} else {
		R_LOG( rch_ral, SEV_INFO, "GPU memory back to normal: device-local %u / %u MiB\n",
			(unsigned)( budget->deviceLocalUsed >> 20 ), (unsigned)( budget->deviceLocalBudget >> 20 ) );
	}
}

// Phase 7.15.4-c Option-A flag accessors. The poll thread is the single producer
// (sets via vk_ral_on_memory_pressure); the render thread is the single consumer
// (reads + clears in vk_ral_drain_evictions). A volatile int single-word
// read/write is the established cross-thread-flag idiom here (cf. the backend's
// pollThreadStop). Defined in this TU so the static flag stays file-private; the
// render-thread drain (tr_image.c) reaches it through these accessors.
qboolean vk_ral_evict_requested( void ) {
	return s_ral_evict_requested ? qtrue : qfalse;
}

void vk_ral_clear_evict_request( void ) {
	s_ral_evict_requested = 0;
}

// Manual test-harness hook: let r_texEvictForce also exercise the automatic
// pressure path by raising the flag (so vk_ral_drain_evictions runs its hysteresis
// loop next frame). Render-thread caller (command handler) — safe.
void vk_ral_request_eviction( void ) {
	s_ral_evict_requested = 1;
}

// Phase 7.15.4-c synthetic-pressure test override (default 0 = OFF — SL-3 pattern).
// Real device-local pressure is ~4 % on current content, so CRITICAL (90 %) never
// fires and the automatic path is otherwise unfalsifiable. When set to N>0 MiB, the
// next vk_ral_drain_evictions treats the target as (currentUsed - N MiB) so it
// evicts ~N MiB worth of oldest-unpinned textures through the REAL code path
// (flag → render-thread drain → victim-scan → dual-free → count-based reclaim toward
// target). Set by the r_texEvictPressureTest command; consumed once then cleared.
static uint64_t s_ral_evict_test_target_drop_bytes;

void vk_ral_set_evict_test_drop_mib( unsigned mib ) {
	s_ral_evict_test_target_drop_bytes = (uint64_t)mib << 20;
}

uint64_t vk_ral_get_evict_test_drop_bytes( void ) {
	return s_ral_evict_test_target_drop_bytes;
}

void vk_ral_clear_evict_test_drop( void ) {
	s_ral_evict_test_target_drop_bytes = 0;
}


// symmetric pair to
// vk_ral_backend_init. Called from vk_shutdown's tail, AFTER every
// consumer's RAL wrapper destroy (Ral_DestroyCommandBuffer of the
// staging cmd buffer, Ral_DestroySwapchain in vk_destroy_swapchain,
// vk_destroy_sync_primitives' Ral_Destroy{Semaphore,Fence} calls,
// vk_ral_unregister_buffer for storage.buffer, etc.) but BEFORE
// qvkDestroyDevice (which would invalidate b->device). Idempotent.
void vk_ral_backend_shutdown( void ) {
	if ( s_ral_backend != NULL ) {
		// Stop the budget poller (joins its thread) before the backend is freed.
		Ral_SetPressureCallback( s_ral_backend, NULL, NULL );
		Ral_DestroyBackend( s_ral_backend );
		s_ral_backend = NULL;
	}
}


void vk_ral_textures_init( void ) {
	ralBindGroupLayoutCreateInfo_t  lci;
	ralBindGroupCreateInfo_t        gci;
	const ralCaps_t                *caps;

	// defensive guard. Backend boot is the
	// single-phase vk_ral_boot_backend called from vk_initialize ahead
	// of vk_create_swapchain. If somehow we reach here without a
	// backend, log + bail (vid_restart re-entry is idempotent on
	// s_ral_backend != NULL).
	if ( s_ral_backend == NULL ) {
		R_LOG( rch_ral_texture, SEV_ERROR, "vk_ral_textures_init: backend not booted; aborting texture/buffer setup\n" );
		return;
	}

	// Bindless texture/buffer infrastructure cvar gate — the backend itself
	// is up unconditionally now (the swapchain depends on it). These cvars
	// only gate the bindless layout/set + buffer-pending-flush below.
	if ( s_ral_bindless_set != NULL ) return;                  // already wired
	{
		qboolean wantTex = qtrue ? qtrue : qfalse;
		qboolean wantBuf = qtrue ? qtrue : qfalse;
		if ( !wantTex && !wantBuf ) return;
	}

	// Bookkeeping reset (vid_restart re-enters here).
	memset( s_ral_recent_names, 0, sizeof( s_ral_recent_names ) );
	s_ral_recent_head     = 0;
	s_ral_registered_count = 0;
	s_ral_skipped_no_slot = 0;
	s_ral_skipped_no_data = 0;
	s_ral_destroyed_count = 0;
	s_ral_mip_test_image = NULL;
	s_ral_mip_test_resource = -1;
	s_ral_mip_test_start_frame = 0;
	s_ral_mip_test_upload_frame = 0;
	s_ral_mip_test_upload_bytes = 0;
	memset( &s_ral_mip_test_ticket, 0, sizeof( s_ral_mip_test_ticket ) );
	// 2D bindless slot allocator resets here too (Phase 7.15.2) for the
	// first-boot / vid_restart-with-fresh-set path. NOTE: this init early-returns
	// when s_ral_bindless_set already exists (the persisting-set vid_restart
	// path), so this is NOT the reset that keeps byte-identity across a normal
	// map transition — that one lives in R_DeleteTextures (which always runs and
	// zeroes tr.numImages). Both call the same helper.
	vk_ral_reset_bindless_slots();
	// 2DArray bindless slot counter resets on every map reload;
	// the bindless descriptor set is recreated when the layout is rebuilt
	// (vid_restart path), so prior slots are invalid anyway.
	s_ral_bindless_array_count  = 0;
	s_ral_skipped_no_array_slot = 0;
	s_ral_init_attempted  = qtrue;

	caps = Ral_GetCaps( s_ral_backend );
	s_ral_bindless_capacity = (uint32_t)WIRED_BINDLESS_TEX_SLOTS;   // target slot count; RAL caps would allow ~1M on RTX hardware
	if ( caps && caps->maxBindlessTextures > 0 && caps->maxBindlessTextures < s_ral_bindless_capacity )
		s_ral_bindless_capacity = caps->maxBindlessTextures;
	// the top WIRED_BINDLESS_RESERVED_TEX_SLOTS slots are off-limits
	// to content image registration. Currently this carves slot 4095 out
	// for the screenmap render attachment (WIRED_BINDLESS_SCREENMAP_SLOT,
	// driven by vk_ral_register_screenmap_view). Content images
	// (R_CreateImage → vk_ral_register_image) cap at slot
	// (s_ral_bindless_capacity - 1) — 4094 on RAL hardware that advertises
	// the full table.
	if ( s_ral_bindless_capacity > WIRED_BINDLESS_RESERVED_TEX_SLOTS )
		s_ral_bindless_capacity -= WIRED_BINDLESS_RESERVED_TEX_SLOTS;

	// Bindless texture table — only built when r_useRALTextures is on; the
	// buffer-only path (r_useRALBuffers=1, r_useRALTextures=0) keeps the
	// backend alive without the bindless infrastructure.
	// bindless-ral-consolidate: layout carries the renderer-set-7 binding
	// shape — image array (binding=0, unbounded → 4096) + sampler dedup-pool
	// array (binding=1, MAX_VK_SAMPLERS=32). A parallel
	// 2DArray content-texture array sits at binding=2 (bounded — 256 slots, plenty
	// for the per-Q1-map q1AnimArray inventory). All bindings flagged
	// PARTIALLY_BOUND + UPDATE_AFTER_BIND by Ral_CreateBindGroupLayout for
	// bindless layouts. The 2D vs 2DArray dimension is enforced at the SPIR-V
	// side (texture2D[] vs texture2DArray[] declarations); both bindings use
	// the same VkDescriptorType=SAMPLED_IMAGE here.
	if ( qtrue ) {
		ralBindEntry_t entries[ 3 ];
		memset( entries, 0, sizeof( entries ) );
		entries[0].binding    = WIRED_BINDLESS_BIND_IMAGES;
		entries[0].type       = RAL_BIND_SAMPLED_TEXTURE;
		entries[0].count      = 0;                                 // 0 == unbounded → RAL caps the array at RAL_VK_BINDLESS_LAYOUT_COUNT (4096)
		entries[0].stageFlags = RAL_STAGE_FRAGMENT;
		entries[1].binding    = WIRED_BINDLESS_BIND_SAMPLERS;
		entries[1].type       = RAL_BIND_SAMPLER;
		entries[1].count      = WIRED_BINDLESS_SAMPLER_SLOTS;      // bounded (MAX_VK_SAMPLERS = 32)
		entries[1].stageFlags = RAL_STAGE_FRAGMENT;
		entries[2].binding    = WIRED_BINDLESS_BIND_ARRAY_IMAGES;
		entries[2].type       = RAL_BIND_SAMPLED_TEXTURE;
		entries[2].count      = WIRED_BINDLESS_ARRAY_TEX_SLOTS;    // bounded (256; q1_ls_array's animArray + future 2DArray content)
		entries[2].stageFlags = RAL_STAGE_FRAGMENT;
		memset( &lci, 0, sizeof( lci ) );
		lci.entries    = entries;
		lci.numEntries = 3;
		lci.bindless   = qtrue;
		lci.debugName  = "renderer-bindless-tex";
		s_ral_bindless_layout = Ral_CreateBindGroupLayout( s_ral_backend, &lci );
		if ( !s_ral_bindless_layout ) {
			// release ONLY what this function allocated and decline.
			// The backend is owned by vk_ral_boot_backend and torn down by
			// vk_ral_backend_shutdown (vk_shutdown's decline path); destroying
			// it here would vkDestroyDevice/Instance while the renderer still
			// holds vk.device/vk.instance aliases, the RAL swapchain, and the
			// per-frame RAL command buffers → use-after-free. Nothing of ours
			// is allocated yet (layout came back NULL), so just decline.
			R_LOG( rch_ral_texture, SEV_WARN, "Ral_CreateBindGroupLayout failed (bindless=%s); declining RAL texture infrastructure\n",
			        caps && caps->bindlessTextures ? "advertised yes" : "no" );
			return;
		}

		memset( &gci, 0, sizeof( gci ) );
		gci.layout    = s_ral_bindless_layout;
		gci.numValues = 0;                                    // slots populated lazily as images register
		gci.debugName = "renderer-bindless-set";
		s_ral_bindless_set = Ral_CreateBindGroup( s_ral_backend, &gci );
		if ( !s_ral_bindless_set ) {
			// release ONLY the bind-group layout this function just
			// created; leave the shared owned backend alone (see above).
			R_LOG( rch_ral_texture, SEV_WARN, "Ral_CreateBindGroup failed; declining RAL texture infrastructure\n" );
			Ral_DestroyBindGroupLayout( s_ral_bindless_layout );  s_ral_bindless_layout = NULL;
			return;
		}
		if ( !vk_ral_bindless_ledger_activate() ) {
			R_LOG( rch_ral_texture, SEV_WARN, "bindless publication ledger activation failed; declining RAL texture infrastructure\n" );
			Ral_DestroyBindGroup( s_ral_bindless_set ); s_ral_bindless_set = NULL;
			Ral_DestroyBindGroupLayout( s_ral_bindless_layout ); s_ral_bindless_layout = NULL;
			return;
		}
	}

	R_LOG( rch_ral_texture, SEV_INFO, "RAL texture infrastructure ready (bindless slots = %u; RAL device caps reports %u)\n",
	        s_ral_bindless_capacity, ( caps ? caps->maxBindlessTextures : 0 ) );

	// flush any RAL buffer register calls that arrived during
	// vk_initialize (before the backend was up). Sites that paired register +
	// unregister before now (transient SMAA-LUT staging in
	// vk_smaa_alloc_resources) self-removed from pending — only survivors
	// reach Ral_CreateBuffer here.
	vk_ral_flush_pending_buffers();
}


// adopt every allocate-once VkDescriptorSet
// the renderer has stood up into a ralBindGroup_t wrapper. Called from
// vk_init_descriptors's tail, AFTER all qvkAllocateDescriptorSets +
// vkUpdateDescriptorSets writes have completed. Idempotent: subsequent
// calls (vid_restart, REF_LEVEL_ONLY-then-re-init) clear the registry
// first, so we always wrap the CURRENT descriptor set handles.
//
// The wrapper carries ownsSet=qfalse; teardown only frees the wrapper
// struct, not the underlying VkDescriptorSet. The legacy
// vkResetDescriptorPool path retains lifetime ownership.
//
// Sets NOT adopted here (deferred to the cmd-record migration):
//   vk.cmd->descriptor_set.current[i] — rotates per shader_type per
//   frame; needs per-frame re-adoption tied to the cmd-buffer ring
//   that the cmd-record migration introduces.
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

	// RETAIN_ADOPT — like ADOPT, but stores the wrapper in a named field so the
	// post-process render path can bind it via Ral_CmdBindBindGroup. The field
	// is freed idempotently (re-init frees the prior wrapper first). Not tracked
	// in s_adopted_bgs (the named field owns the wrapper lifetime).
	#define RETAIN_ADOPT( field, vkset, layout, label ) do {                                              \
		if ( (field) ) { Ral_DestroyBindGroup( (field) ); (field) = NULL; }                               \
		if ( (vkset) != VK_NULL_HANDLE && (layout) != NULL ) {                                            \
			(field) = Ral_AdoptBindGroup( s_ral_backend, (vkset), (layout), (label) );                    \
		}                                                                                                 \
	} while ( 0 )

	// ── core singletons ────────────────────────────────────────────────
	RETAIN_ADOPT( vk.ral_color_descriptor,           vk.color_descriptor,           vk.ral_bgl_sampler, "wired-color-bg" );
	RETAIN_ADOPT( vk.ral_tonemapped_descriptor,      vk.tonemapped_descriptor,      vk.ral_bgl_sampler, "wired-tonemapped-bg" );
	RETAIN_ADOPT( vk.screenMap.ral_color_descriptor, vk.screenMap.color_descriptor, vk.ral_bgl_sampler, "wired-screenmap-color-bg" );
	// Retained (not anonymous-ADOPT'd) so vk_tonemap can bind it as set 1 — the
	// SSAO/sunrays tonemap variants sample this depth copy. sceneDepth.descriptor
	// is written with the depth view + SHADER_READ_ONLY layout (vk_update_attachment_descriptors).
	RETAIN_ADOPT( vk.sceneDepth.ral_descriptor, vk.sceneDepth.descriptor, vk.ral_bgl_sampler, "wired-scenedepth-bg" );
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

	// ── bloom_image_descriptor[] — one per bloom mip, 1 + VK_NUM_BLOOM_PASSES.
	//    Retained so the bloom extract/downsample/upsample/composite passes bind
	//    them via RAL. ──
	for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ ) {
		RETAIN_ADOPT( vk.ral_bloom_image_descriptor[i], vk.bloom_image_descriptor[i], vk.ral_bgl_sampler, "wired-bloom-img-bg" );
	}

	// ── per-frame-ring uniform descriptors (vk.tess[NUM_COMMAND_BUFFERS]) ──
	for ( i = 0; i < ARRAY_LEN( vk.tess ); i++ ) {
		ADOPT( vk.tess[i].uniform_descriptor, vk.ral_bgl_uniform, "wired-tess-uniform-bg" );
	}

	// ── per-frame scene-exposure UBO ring: retain the wrappers (unlike the
	//    teardown-only ADOPT pool above) so the post-process passes can bind
	//    the exposure set via Ral_CmdBindBindGroup. The engine still owns the
	//    VkBuffer/memory and writes ptr[] each frame; these are adopt-in-place.
	//    Idempotent: free any prior session's wrappers before re-adopting the
	//    fresh ring (mirrors the s_adopted_bgs teardown at the top). ──
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.exposure.ral_descriptor[i] ) { Ral_DestroyBindGroup( vk.exposure.ral_descriptor[i] ); vk.exposure.ral_descriptor[i] = NULL; }
		if ( vk.exposure.ral_buffer[i] )     { Ral_DestroyBuffer( vk.exposure.ral_buffer[i] );        vk.exposure.ral_buffer[i] = NULL; }
		if ( vk.exposure.buffer[i] != VK_NULL_HANDLE ) {
			vk.exposure.ral_buffer[i] = Ral_AdoptBuffer( s_ral_backend, (void *)vk.exposure.buffer[i],
				sizeof( vk_exposure_block_t ), "wired-exposure-ubo" );
		}
		if ( vk.exposure.descriptor[i] != VK_NULL_HANDLE && vk.ral_bgl_uniform ) {
			vk.exposure.ral_descriptor[i] = Ral_AdoptBindGroup( s_ral_backend,
				vk.exposure.descriptor[i], vk.ral_bgl_uniform, "wired-exposure-bg" );
		}
		// ── per-frame WiredUI SCENE backdrop UBO ring (menubg.frag set 2). Same
		//    exposureSetLayout shape (binding 0 UNIFORM_BUFFER) → same ral_bgl_uniform.
		//    The engine owns the VkBuffer/memory and writes ptr[] each frame; this
		//    wrapper lets RB_MenuBackdrop bind the set via Ral_CmdBindBindGroup.
		//    Idempotent: free any prior session's wrapper before re-adopting. ──
		if ( vk.menubg.ral_descriptor[i] ) { Ral_DestroyBindGroup( vk.menubg.ral_descriptor[i] ); vk.menubg.ral_descriptor[i] = NULL; }
		if ( vk.menubg.descriptor[i] != VK_NULL_HANDLE && vk.ral_bgl_uniform ) {
			vk.menubg.ral_descriptor[i] = Ral_AdoptBindGroup( s_ral_backend,
				vk.menubg.descriptor[i], vk.ral_bgl_uniform, "wired-menubg-bg" );
		}
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
	#undef RETAIN_ADOPT

	R_LOG( rch_ral, SEV_INFO,
		"adopted %u bind groups as ralBindGroup_t (core singletons + SMAA + bloom + per-frame rings)\n",
		s_adopted_bgs_count - before );

	// also adopt every VkPipelineLayout into its
	// matching typed sibling. Same idempotent re-init contract.
	vk_ral_adopt_static_pipeline_layouts();

	// adopt the 6 renderer-owned internal-image VkImages
	// + the GPU-timestamp VkQueryPool into ralTexture_t / ralQueryPool_t
	// siblings. Must run AFTER vk_initialize (which creates the VkImages) and
	// AFTER vk_smaa_alloc_resources (when r_smaa is on at boot). The boot-time
	// call path is vk_initialize → vk_init_descriptors → vk_ral_adopt_static_bindgroups
	// → here; the SMAA images are conditionally adopted under vk.fboActive.
	vk_ral_adopt_static_internal_textures();
}


// ════════════════════════════════════════════════════════════════════════
// reverse-lookup helpers for the typed RAL cmd API.
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
// boot-time pipeline-layout adoption sweep.
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
// Logged count (always-on SEV_INFO): "adopted N pipeline layouts
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
	ADOPT_PL( vk.pipeline_layout_post_process,   vk.ral_pipeline_layout_post_process,   "wired-pl-post-process" );
	ADOPT_PL( vk.pipeline_layout_smaa,           vk.ral_pipeline_layout_smaa,           "wired-pl-smaa" );
	ADOPT_PL( vk.pipeline_layout_msdf,           vk.ral_pipeline_layout_msdf,           "wired-pl-msdf" );
	ADOPT_PL( vk.pipeline_layout_ssao,           vk.ral_pipeline_layout_ssao,           "wired-pl-ssao" );
	ADOPT_PL( vk.pipeline_layout_sunrays,        vk.ral_pipeline_layout_sunrays,        "wired-pl-sunrays" );

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
	R_LOG( rch_ral, SEV_INFO,
		"adopted %u pipeline layouts as ralPipelineLayout_t (centralized + per-subsystem)\n",
		adopted );
}


void vk_ral_adopt_one_pipeline_layout( VkPipelineLayout vkLayout,
		struct ralPipelineLayout_s **ralField, const char *label )
{
	if ( !s_ral_backend || !ralField ) return;
	if ( *ralField ) { Ral_DestroyPipelineLayout( *ralField ); *ralField = NULL; }
	if ( vkLayout != VK_NULL_HANDLE )
		*ralField = Ral_AdoptPipelineLayout( s_ral_backend, vkLayout, label );
}


// On-demand single texture adoption — for an attachment image created/re-created
// OUTSIDE the boot-time vk_ral_adopt_static_internal_textures sweep (e.g. the
// capture image, which is supersample-gated and re-created on every swapchain /
// r_hdr / r_fbo rebuild without re-running the sweep). Call at creation + KILL the
// sibling before destroying the VkImage. Idempotent: destroys any prior sibling.
// NULL vkImage no-ops. fmt is a VkFormat (translated via vk_attachment_format_to_ral).
void vk_ral_adopt_one_texture( VkImage vkImage, VkImageView vkView, VkFormat fmt,
		struct ralTexture_s **ralField, uint32_t w, uint32_t h, uint32_t aspect, const char *label )
{
	if ( !s_ral_backend || !ralField ) return;
	if ( *ralField ) { Ral_DestroyTexture( *ralField ); *ralField = NULL; }
	if ( vkImage != VK_NULL_HANDLE )
		*ralField = Ral_AdoptTexture( s_ral_backend, vkImage, vkView,
		                              vk_attachment_format_to_ral( fmt ), w, h, aspect, label );
}



// ════════════════════════════════════════════════════════════════════════
// internal-texture adoption sweep.
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
// Logged count (always-on SEV_INFO): "adopted N internal textures
// as ralTexture_t".
// ════════════════════════════════════════════════════════════════════════
static uint32_t s_ral_internal_textures_adopted;

void vk_ral_adopt_static_internal_textures( void )
{
	uint32_t adopted = 0;
	if ( !s_ral_backend ) return;

	// Each adopted texture carries its real VkImageView + format so it can serve
	// as a dynamic-rendering attachment (Ral_BeginRendering reads defaultView).
	#define ADOPT_TEX( vkfield, vkview, vkfmt, ralfield, w, h, asp, label ) do {                                 \
		if ( ralfield ) { Ral_DestroyTexture( ralfield ); ralfield = NULL; }                                 \
		if ( (vkfield) != VK_NULL_HANDLE ) {                                                                     \
			ralfield = Ral_AdoptTexture( s_ral_backend, (vkfield), (vkview),                                  \
			                             vk_attachment_format_to_ral( (vkfmt) ), (w), (h), (asp), (label) ); \
			if ( ralfield ) adopted++;                                                                           \
		}                                                                                                        \
	} while ( 0 )

	// Adopt the depth image with the SAME aspect it was created with: DEPTH, plus
	// STENCIL when the depth buffer carries a stencil aspect (combined D24S8 when
	// glConfig.stencilBits > 0). The stencil aspect lets Ral_BeginRendering
	// auto-bind the stencil attachment for a depth+stencil dynamic-rendering pass;
	// it is consulted only there, so it is inert for depth-sampling / legacy
	// depth-attachment use until that pass migrates.
	ADOPT_TEX( vk.depth_image,       vk.depth_image_view,      vk.depth_format,  vk.ral_depth_image,
	           glConfig.vidWidth, glConfig.vidHeight,
	           ( glConfig.stencilBits > 0 ) ? ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT ) : VK_IMAGE_ASPECT_DEPTH_BIT,
	           "wired-img-depth" );
	ADOPT_TEX( vk.color_image,       vk.color_image_view,      vk.color_format,  vk.ral_color_image,
	           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
	           "wired-img-color" );
	ADOPT_TEX( vk.tonemapped_image,  vk.tonemapped_image_view, vk.color_format,  vk.ral_tonemapped_image,
	           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
	           "wired-img-tonemapped" );

	// screenMap color/depth — the mirror/portal pass's separate targets. Same
	// formats as the main pass (vk.color_format / vk.depth_format); depth carries
	// the stencil aspect under the same gate as the main depth so Ral_BeginRendering
	// can auto-bind the screenmap stencil attachment.
	ADOPT_TEX( vk.screenMap.color_image, vk.screenMap.color_image_view, vk.color_format, vk.screenMap.ral_color_image,
	           vk.screenMapWidth, vk.screenMapHeight, VK_IMAGE_ASPECT_COLOR_BIT,
	           "wired-img-screenmap-color" );
	ADOPT_TEX( vk.screenMap.depth_image, vk.screenMap.depth_image_view, vk.depth_format, vk.screenMap.ral_depth_image,
	           vk.screenMapWidth, vk.screenMapHeight,
	           ( glConfig.stencilBits > 0 ) ? ( VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT ) : VK_IMAGE_ASPECT_DEPTH_BIT,
	           "wired-img-screenmap-depth" );

	// sceneDepth.image adoption. Created by
	// vk_create_attachments inside vk_initialize, which runs BEFORE
	// tr_init.c's vk_init_descriptors() call that drives this sweep — so
	// the VkImage is already valid here. Gated by vk.sceneDepth.active
	// (matches the same gate used at the qvkCreateImage site in vk.c:11871).
	// Closes the 3 previously-missed callsites at vk_scene_depth_copy
	// (PipelineBarrier x2 + CopyImage x1) that previously SEV_WARN-skipped.
	if ( vk.sceneDepth.active ) {
		ADOPT_TEX( vk.sceneDepth.image, vk.sceneDepth.view, vk.depth_format, vk.sceneDepth.ral_image,
		           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_DEPTH_BIT,
		           "wired-img-scenedepth" );
	}

	// NOTE: the cascaded shadow map (vk.shadowMap.image) is NOT adopted here — it
	// does not exist at boot (r_shadows defaults 0) and is (re)created on the
	// live r_shadows toggle / vid_restart, which this static boot sweep does
	// not re-run. It is adopted at its creation site in vk_shadow_alloc_resources
	// (and destroyed in vk_shadow_release_resources) — the same adopt-at-creation
	// lifecycle as the supersample-gated capture image.

	if ( vk.fboActive ) {
		ADOPT_TEX( vk.smaa.input_image, vk.smaa.input_view, vk.color_format, vk.smaa.ral_input_image,
		           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
		           "wired-img-smaa-input" );
		ADOPT_TEX( vk.smaa.edges_image, vk.smaa.edges_view, VK_FORMAT_R8G8_UNORM, vk.smaa.ral_edges_image,
		           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
		           "wired-img-smaa-edges" );
		ADOPT_TEX( vk.smaa.blend_image, vk.smaa.blend_view, VK_FORMAT_R8G8B8A8_UNORM, vk.smaa.ral_blend_image,
		           glConfig.vidWidth, glConfig.vidHeight, VK_IMAGE_ASPECT_COLOR_BIT,
		           "wired-img-smaa-blend" );

		// bloom extract/blur chain — the 9 bloom_image attachments (idx 0 extract
		// target, idx k>=1 blur output). Created in vk_create_attachments under the
		// same fboActive && r_bloom gate (vk.c bloom block); each is a single-layer
		// color attachment in vk.bloom_format. Dimensions per index: idx 0 is full
		// capture size, then halving per blur-pair level (idx 1,2 = /2; 3,4 = /4;
		// ...) matching create_color_attachment's loop. Used as Ral_BeginRendering
		// color targets + sampled (post each pass's own SHADER_READ barrier).
		if ( r_bloom->integer ) {
			uint32_t bi;
			uint32_t bw = gls.captureWidth;
			uint32_t bh = gls.captureHeight;
			ADOPT_TEX( vk.bloom_image[0], vk.bloom_image_view[0], vk.bloom_format, vk.ral_bloom_image[0],
			           bw, bh, VK_IMAGE_ASPECT_COLOR_BIT, "wired-img-bloom-0" );
			for ( bi = 1; bi < ARRAY_LEN( vk.bloom_image ); bi += 2 ) {
				bw /= 2;
				bh /= 2;
				ADOPT_TEX( vk.bloom_image[bi+0], vk.bloom_image_view[bi+0], vk.bloom_format, vk.ral_bloom_image[bi+0],
				           bw, bh, VK_IMAGE_ASPECT_COLOR_BIT, va( "wired-img-bloom-%u", bi+0 ) );
				ADOPT_TEX( vk.bloom_image[bi+1], vk.bloom_image_view[bi+1], vk.bloom_format, vk.ral_bloom_image[bi+1],
				           bw, bh, VK_IMAGE_ASPECT_COLOR_BIT, va( "wired-img-bloom-%u", bi+1 ) );
			}
		}
	}

	#undef ADOPT_TEX

	s_ral_internal_textures_adopted = adopted;
	R_LOG( rch_ral, SEV_INFO,
		"adopted %u internal textures as ralTexture_t\n", adopted );

	// HDR auto-exposure histogram compute resources depend on the freshly-
	// adopted vk.ral_color_image (the sampling view is minted over it), so bring
	// them up here rather than in vk_initialize (which runs before this sweep).
	// Idempotent: re-running this sweep on vid_restart re-mints the view.
	vk_hdr_histogram_init( s_ral_backend );

	// The exposure-reduce compute consumes the histogram (above) and the exposure
	// UBO ring (adopted earlier in vk_ral_adopt_static_bindgroups), so it comes up
	// after the histogram. Idempotent across vid_restart.
	vk_hdr_exposure_reduce_init( s_ral_backend );

	// The BRDF integration LUT has no color-image dependency, but rides this sweep
	// for lifecycle symmetry with the other RAL compute resources (created here,
	// destroyed in the symmetric teardown). Idempotent: re-arms its one-shot guard
	// so the recreated image is recomputed on the next frame.
	vk_brdf_lut_init( s_ral_backend );

	// IBL probe infrastructure (source sky cube + irradiance/radiance probe cubes +
	// per-face/per-mip views). Same sweep, same lifecycle; the source one-shot fill
	// is recorded in vk_begin_frame. Inert this phase (nothing samples the probes).
	vk_ibl_probes_init( s_ral_backend );

	// GTAO ambient occlusion. Depends on the depth copy (sceneDepth.ral_image)
	// adopted earlier in this sweep, so it comes up after that adoption. No-ops
	// when SSAO is off (no depth copy); a vid_restart with r_ssao on re-runs this
	// with the depth copy present. Idempotent across vid_restart.
	vk_gtao_init( s_ral_backend );

	// Lens-glow occlusion oracle. Like GTAO, depends on the depth copy
	// (sceneDepth.ral_image) adopted earlier in this sweep; the sampling view + the
	// per-frame lens SSBOs + the N-tap compute pipeline come up here. No-ops when the
	// flare system is off (no fragmentStores) or the depth copy is absent; a vid_restart
	// re-mints the depth view over the re-adopted copy. Idempotent across vid_restart.
	vk_lens_init( s_ral_backend );

	// Forward+ tiled-lighting tile-classification compute. Screen-extent-derived
	// tile grid + per-frame light/tile SSBOs + the compute pipeline. Independent of
	// the color/depth images (it bins lights, reads no attachment), but rides this
	// sweep for lifecycle symmetry (created here, torn down in the symmetric teardown
	// + re-created on vid_restart so the tile grid tracks the render extent). Inert
	// unless r_forwardPlus is set; the dispatch self-gates.
	vk_forwardplus_init( s_ral_backend );

	// Forward+ lit consumer pipeline (the world-space tile-light reader). Dedicated
	// layout + set 2 + the additive lit pipeline; reuses the producer's tile/light
	// SSBOs. Rides the sweep for the same lifecycle (re-created on vid_restart so the
	// pipeline tracks the color/depth formats). Inert unless r_forwardPlus is set.
	vk_forwardplus_lit_init( s_ral_backend );

	// The engine-resources set's IBL bindings (set 2, bindings 1/2/3 — the BRDF LUT
	// + probe cubes) are written by vk_update_attachment_descriptors, which gates
	// each write on its view existing. That function ran earlier in vk_init_descriptors
	// — BEFORE this sweep created the views — so re-run it now that the IBL images,
	// views, and sampler exist, otherwise the bindings stay unwritten until a later
	// descriptor refresh and the base-pass IBL term reads zero on a fresh boot.
	vk_update_attachment_descriptors();
	// H2a scene identity is usable only after the raw color attachment, adopted
	// texture, postprocess group and histogram group belong to this same sweep.
	// Publishing earlier would let pointer reuse satisfy a stale target receipt.
	vk_temporal_scene_color_attachment_published();
}


void vk_ral_destroy_adopted_internal_textures( void )
{
	// HDR histogram + exposure-reduce resources are brought up at the tail of the
	// adopt sweep and reference the adopted color image / exposure UBO ring; tear
	// them down symmetrically here, before the color image, so nothing outlives the
	// device. Reduce before histogram (reduce's bind-group references the histogram
	// buffer; free the dependent first).
	vk_hdr_exposure_reduce_shutdown();
	vk_hdr_histogram_shutdown();

	// GPU cull resources are RAL-owned (AABB/reached/visible SSBOs +
	// pipeline); free them with the device-lifetime compute siblings.
	vk_cull_shutdown();

	// BRDF LUT compute resources are RAL-owned (created, not adopted); free them
	// here too so they don't outlive the device.
	vk_brdf_lut_shutdown();

	// IBL probe cubes + all per-face/per-mip views are RAL-owned; free them here so
	// no view/image outlives the device.
	vk_ibl_probes_shutdown();

	// GTAO AO textures + the sampling view over the depth copy are RAL-owned; free
	// them here (before the depth copy below — the view must not outlive its image).
	vk_gtao_shutdown();

	// Lens-glow oracle: the sampling view over the depth copy + the lens SSBOs +
	// pipeline are RAL-owned; free them here (before the depth copy below — the view
	// must not outlive its image), symmetric with the GTAO teardown.
	vk_lens_shutdown();

	// Forward+ tile-classification compute resources are RAL-owned; free them with
	// the other device-lifetime compute siblings (re-created on the next bring-up).
	vk_forwardplus_shutdown();
	vk_forwardplus_lit_shutdown();

	#define KILL_TEX( field ) do { if ( field ) { Ral_DestroyTexture( field ); field = NULL; } } while ( 0 )
	KILL_TEX( vk.ral_depth_image );
	KILL_TEX( vk.ral_color_image );
	KILL_TEX( vk.ral_tonemapped_image );
	KILL_TEX( vk.screenMap.ral_color_image );
	KILL_TEX( vk.screenMap.ral_depth_image );
	KILL_TEX( vk.sceneDepth.ral_image );
	KILL_TEX( vk.smaa.ral_input_image );
	KILL_TEX( vk.smaa.ral_edges_image );
	KILL_TEX( vk.smaa.ral_blend_image );
	{
		uint32_t bi;
		for ( bi = 0; bi < ARRAY_LEN( vk.ral_bloom_image ); bi++ )
			KILL_TEX( vk.ral_bloom_image[bi] );
	}
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
	MATCH( vk.sceneDepth.image,   vk.sceneDepth.ral_image );
	MATCH( vk.smaa.input_image,  vk.smaa.ral_input_image );
	MATCH( vk.smaa.edges_image,  vk.smaa.ral_edges_image );
	MATCH( vk.smaa.blend_image,  vk.smaa.ral_blend_image );
	{
		uint32_t bi;
		for ( bi = 0; bi < ARRAY_LEN( vk.bloom_image ); bi++ )
			MATCH( vk.bloom_image[bi], vk.ral_bloom_image[bi] );
	}
	#undef MATCH
	return NULL;
}


static void vk_ral_destroy_adopted_pipeline_layouts( void )
{
	#define KILL_PL( ralfield ) do { if ( ralfield ) { Ral_DestroyPipelineLayout( ralfield ); ralfield = NULL; } } while ( 0 )
	KILL_PL( vk.ral_pipeline_layout );
	KILL_PL( vk.ral_pipeline_layout_post_process );
	KILL_PL( vk.ral_pipeline_layout_smaa );
	KILL_PL( vk.ral_pipeline_layout_msdf );
	KILL_PL( vk.ral_pipeline_layout_ssao );
	KILL_PL( vk.ral_pipeline_layout_sunrays );
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


// VkDescriptorSet → ralBindGroup_t * reverse lookup.
// Used at parallel bind-call sites to find the adopted wrapper for the
// VkDescriptorSet the legacy code is about to bind. Linear scan over the
// adoption registry (32 entries max) — acceptable parallel-paths overhead
// until the cmd-record migration refactors to per-subsystem sibling fields.
// Returns NULL for unadopted sets (e.g. per-shader-type rotating descriptors in
// vk.cmd->descriptor_set.current[] — deferred to the cmd-record migration's
// per-frame adoption); callers skip the RAL parallel call on NULL return.
ralBindGroup_t *vk_ral_lookup_bindgroup( VkDescriptorSet vkSet ) {
	uint32_t i;
	if ( vkSet == VK_NULL_HANDLE ) return NULL;
	for ( i = 0; i < s_adopted_bgs_count; i++ ) {
		if ( s_adopted_bgs[i] && Ral_GetBindGroupHandle( s_adopted_bgs[i] ) == (void *)vkSet ) {
			return s_adopted_bgs[i];
		}
	}
	// per-frame ring adoption mirror
	// (vk.cmd->descriptor_set.current_ral[]) was deleted alongside the
	// parallel bind helper; the boot-time s_adopted_bgs registry above is
	// the only lookup source now.
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
	// REF_LEVEL_ONLY skip gate.
	// destroyWindow == qfalse means map-scoped teardown (map transition):
	// keep the RAL backend, sibling pipelines, BGLs, and the active-buffer
	// tracker live across the transition. Mirrors vk_shutdown's skip-on-
	// !destroyWindow pattern. The full teardown path below runs only on
	// REF_KEEP_WINDOW / REF_DESTROY_WINDOW / REF_UNLOAD_DLL — when the
	// device is being released, RAL pipelines must release BEFORE that.
	if ( !destroyWindow ) {
		return;
	}
	vk_temporal_history_store_shutdown();
	R_TemporalHistoryShutdown();
	// legacy-mainpath-retire STEP 1 half-init safety. When vk_initialize
	// declined (caps decline), vk.active stays qfalse and most of the state
	// the full-teardown branch touches (the bindless layout/set, the
	// adopted bindgroups + pipeline layouts, the renderer-side RAL pipelines)
	// was never created. The KILL_* macros below all have NULL guards, but
	// the pipeline-cache save + active-buffer iteration paths are safer
	// short-circuited when vk_initialize never reached vk.active=qtrue.
	// vk_shutdown's own half-init branch destroys the RAL backend itself.
	if ( !vk.active ) {
		R_LOG( rch_ral, SEV_INFO, "vk_ral_textures_shutdown: half-init (decline) path — skipping full teardown\n" );
		s_ral_init_attempted = qfalse;
		return;
	}
	// destroy every still-live RAL buffer BEFORE the backend
	// teardown so each defer-destroy is owned by a live backend.
	vk_ral_destroy_all_active_buffers();
	// save the pipeline cache to disk
	// before the backend is destroyed. Subsequent boots seed from this
	// file for faster pipeline warm-up. Same path convention as the
	// vk_initialize-time load (versioned filename).
	//
	// observability note: this code path is
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
		if ( !base || !*base ) base = BASEGAME;
		Com_sprintf( cachePath, sizeof( cachePath ), "%s/%s/pipelinecache_v1_vulkan.bin",
		             ( home && *home ) ? home : ".", base );
		R_LOG( rch_ral, SEV_INFO, "saving pipeline cache to '%s'\n", cachePath );
		Ral_SavePipelineCache( s_ral_backend, cachePath );
	}
	// destroy every adopted bind-group wrapper.
	// ownsSet=qfalse on adopted wrappers means Ral_DestroyBindGroup only
	// frees the wrapper struct, not the underlying VkDescriptorSet — the
	// legacy vkResetDescriptorPool path frees the sets. Must run BEFORE
	// Ral_DestroyBackend so the wrappers don't outlive the backend they
	// reference.
	vk_ral_destroy_adopted_bindgroups();

	// destroy every adopted pipeline-layout wrapper.
	// Same ownsHandle=qfalse contract as the bindgroup wrappers above: the
	// renderer's existing qvkDestroyPipelineLayout teardown owns the underlying
	// VkPipelineLayout lifetime; this only frees the wrapper structs. Must run
	// BEFORE Ral_DestroyBackend for the same dangling-ref reason.
	vk_ral_destroy_adopted_pipeline_layouts();

	// Destroy every adopted internal-texture wrapper. ownsImage=qfalse means
	// only wrappers are freed; underlying VkImages remain renderer-owned.
	vk_ral_destroy_adopted_internal_textures();

	// renderer-side RAL pipeline +
	// BGL destruction BEFORE Ral_DestroyBackend. Ral_DestroyBackend does NOT
	// enumerate live RAL pipelines / BGLs (the backend doesn't track them in
	// a global list — by design). Each consumer must Ral_Destroy*
	// its own handles before backend teardown, else the underlying VkPipeline /
	// VkDescriptorSetLayout leak past vkDestroyDevice
	// (VUID-vkDestroyDevice-device-05137). On REF_LEVEL_ONLY shutdown
	// (server map-load) vk_shutdown is SKIPPED so its
	// vk_destroy_pipelines path doesn't run — we MUST do the destroy here.
	// On REF_KEEP_WINDOW / REF_DESTROY_WINDOW / REF_UNLOAD_DLL shutdown
	// the subsequent vk_destroy_pipelines NULL-checks each sibling field
	// and finds NULL (we cleared them below), so it only destroys the
	// legacy VkPipelines.
	{
		uint32_t i;
		// all 21 sibling pipeline KILL_PIPE
		// entries deleted (fields removed from vk.h). BGL teardown below stays.
		#define KILL_BGL(  field )   do { if ( (field) ) { Ral_DestroyBindGroupLayout( (field) ); (field) = NULL; } } while (0)
		// adopted BGL wrappers (ownsLayout=qfalse: only the wrapper struct
		// gets freed, the underlying VkDescriptorSetLayout stays alive for
		// the legacy renderer code to vkDestroyDescriptorSetLayout later).
		KILL_BGL( vk.ral_bgl_sampler );
		KILL_BGL( vk.ral_bgl_uniform );
		KILL_BGL( vk.ribbon.ral_bgl );
		KILL_BGL( vk.beam.ral_bgl );
		KILL_BGL( vk.sprite.ral_bgl );
		KILL_BGL( vk.particle.ral_bgl_compute );
		KILL_BGL( vk.particle.ral_bgl_render );
		KILL_BGL( vk.iqmGpu.ral_bgl_bones );
		#undef KILL_PIPE
		#undef KILL_BGL
	}
	if ( s_ral_bindless_set    ) {
		if ( s_bindless_publication_initialized )
			(void)VK_BindlessPublicationInvalidateSet(
				&s_bindless_publication, s_ral_bindless_set );
		Ral_DestroyBindGroup( s_ral_bindless_set ); s_ral_bindless_set = NULL;
	}
	if ( s_ral_bindless_layout ) { Ral_DestroyBindGroupLayout( s_ral_bindless_layout ); s_ral_bindless_layout = NULL; }
	// Ral_DestroyBackend moved
	// out into vk_ral_backend_shutdown (called from vk_shutdown's tail). Was
	// here before; ran from RE_Shutdown:vk_ral_textures_shutdown (BEFORE
	// vk_shutdown), which left vk_shutdown's Ral_DestroyCommandBuffer of
	// vk.ral_staging_cmd + Ral_DestroySwapchain calls with a dangling backend
	// pointer → vkFreeCommandBuffers VUID storm + nvoglv64.dll access violation.
	s_ral_bindless_capacity = 0;
	s_ral_init_attempted    = qfalse;
	// Flush any in-flight async-upload tickets (vid_restart / shutdown): destroy their
	// fences and clear the list. The device is idle by here, so the copies are done.
	{
		uint32_t i;
		for ( i = 0; i < s_ral_pending_upload_count; i++ )
			vk_ral_release_upload_ticket( &s_ral_pending_uploads[i].ticket );
		s_ral_pending_upload_count = 0;
		s_ral_pending_peak         = 0;
	}
	vk_ral_release_upload_ticket( &s_ral_mip_test_ticket );
	vk_ral_material_reset( qfalse );
	s_ral_mip_test_upload_frame = 0;
	s_ral_mip_test_upload_bytes = 0;
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

static qboolean vk_ral_whole_texture_promotion_ready( const image_t *image ) {
	ralResidencyCandidate_t candidate;
	if ( !image ) return qfalse;
	if ( image->ralResidencyMipCount > 0 ) {
		uint32_t i;
		for ( i = 0; i < image->ralResidencyMipCount; ++i ) {
			if ( image->ralMipResidency[i].state != RAL_RESIDENCY_RESIDENT )
				return qfalse;
		}
		return qtrue;
	}
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.id.classId = RAL_RESIDENCY_CLASS_TEXTURE;
	candidate.id.planeMask = 1; // current whole-texture adapter is one atomic plane
	candidate.state = RAL_RESIDENCY_IN_FLIGHT;
	// level 0 has no parent dependency; completed mask 1 proves the only plane.
	return Ral_ResidencyPromotionReady( &candidate, 1, 0 ) ? qtrue : qfalse;
}

static qboolean vk_ral_init_mip_records( image_t *image, uint32_t resource,
		uint32_t mipLevels, ralResidencyState_t initialState,
		uint32_t serial ) {
	uint32_t i;
	if ( !image || mipLevels == 0 || mipLevels > MAX_IMAGE_RESIDENCY_MIPS ) return qfalse;
	memset( image->ralMipResidency, 0, sizeof( image->ralMipResidency ) );
	for ( i = 0; i < mipLevels; ++i ) {
		ralResidencyPageId_t id;
		memset( &id, 0, sizeof( id ) );
		id.classId = RAL_RESIDENCY_CLASS_TEXTURE;
		id.resource = resource;
		id.level = (uint16_t)i;
		id.planeMask = 1;
		if ( !Ral_ResidencyPageRecordInit( &image->ralMipResidency[i], &id,
		                                  initialState, serial ) ) {
			image->ralResidencyMipCount = 0;
			return qfalse;
		}
	}
	image->ralResidencyMipCount = mipLevels;
	return qtrue;
}

static qboolean vk_ral_mip_transition( image_t *image, uint32_t level,
		ralResidencyState_t state, uint8_t completedPlaneMask,
		qboolean parentReady, uint32_t serial ) {
	if ( !image || level >= image->ralResidencyMipCount ) return qfalse;
	return Ral_ResidencyPageRecordTransition( &image->ralMipResidency[level],
		state, completedPlaneMask, parentReady ? 1 : 0, serial ) ? qtrue : qfalse;
}

static qboolean vk_ral_mip_mark_resident( image_t *image, uint32_t level,
		uint32_t serial ) {
	ralResidencyPageRecord_t *record;
	qboolean parentReady;
	if ( !image || level >= image->ralResidencyMipCount ) return qfalse;
	record = &image->ralMipResidency[level];
	parentReady = level == 0 ||
		( level - 1u < image->ralResidencyMipCount &&
		  image->ralMipResidency[level - 1u].state == RAL_RESIDENCY_RESIDENT );
	if ( record->state == RAL_RESIDENCY_ABSENT &&
	     !vk_ral_mip_transition( image, level, RAL_RESIDENCY_REQUESTED,
	                             0, parentReady, serial ) ) return qfalse;
	if ( record->state == RAL_RESIDENCY_REQUESTED &&
	     !vk_ral_mip_transition( image, level, RAL_RESIDENCY_IN_FLIGHT,
	                             0, parentReady, serial ) ) return qfalse;
	if ( record->state == RAL_RESIDENCY_IN_FLIGHT &&
	     !vk_ral_mip_transition( image, level, RAL_RESIDENCY_RESIDENT,
	                             record->id.planeMask, parentReady, serial ) ) return qfalse;
	return record->state == RAL_RESIDENCY_RESIDENT ? qtrue : qfalse;
}

static qboolean vk_ral_mip_promotion_ready( const image_t *image,
		uint32_t level ) {
	ralResidencyCandidate_t candidate;
	const ralResidencyPageRecord_t *record;
	if ( !image || level >= image->ralResidencyMipCount ) return qfalse;
	record = &image->ralMipResidency[level];
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.id = record->id;
	candidate.state = record->state;
	return Ral_ResidencyPromotionReady( &candidate, candidate.id.planeMask,
	                                   level == 0 ? 0 : record->parentReady )
		? qtrue : qfalse;
}

static void vk_ral_log_mip_record( const image_t *image, uint32_t level,
		const char *action ) {
	const ralResidencyPageRecord_t *record;
	const char *state;
	if ( !image || level >= image->ralResidencyMipCount || !action ) return;
	record = &image->ralMipResidency[level];
	switch ( record->state ) {
	case RAL_RESIDENCY_STALE: state = "stale"; break;
	case RAL_RESIDENCY_IN_FLIGHT: state = "in-flight"; break;
	case RAL_RESIDENCY_RESIDENT: state = "resident"; break;
	default: state = "unexpected"; break;
	}
	R_LOG( rch_ral_texture, SEV_WARN,
	       "RAL residency page state: action=%s class=texture resource=%u level=%u x=%u y=%u planeMask=%u state=%s completedMask=%u parentReady=%u serial=%u name=%s\n",
	       action, record->id.resource, (unsigned)record->id.level,
	       record->id.x, record->id.y, record->id.planeMask, state,
	       record->completedPlaneMask, record->parentReady,
	       record->transitionSerial, image->imgName );
}

static void vk_ral_material_reset( qboolean restoreViews ) {
	uint32_t i;
	for ( i = 0; i < VK_RAL_MATERIAL_PLANES; ++i ) {
		if ( s_ral_material_tickets[i].fence ) {
			Ral_WaitFence( s_ral_material_tickets[i].fence, ~0ull );
			vk_ral_release_upload_ticket( &s_ral_material_tickets[i] );
		}
		if ( restoreViews && s_ral_material_images[i] && s_ral_material_images[i]->ralResidencyView &&
		     s_ral_material_images[i]->ralBindlessSlot >= 0 )
			(void)vk_ral_bindless_publish_texture_view( s_ral_material_images[i],
				(uint32_t)s_ral_material_images[i]->ralBindlessSlot,
				s_ral_material_images[i]->ralResidencyView,
				VK_BINDLESS_PUBLICATION_RESIDENT );
		if ( s_ral_material_coarse[i] ) Ral_DestroyTextureView( s_ral_material_coarse[i] );
		s_ral_material_images[i] = NULL;
		s_ral_material_coarse[i] = NULL;
	}
	memset( &s_ral_material_group, 0, sizeof( s_ral_material_group ) );
	s_ral_material_completed = 0;
	s_ral_material_start_frame = 0;
}

qboolean vk_ral_residency_material_test( qboolean restore ) {
	image_t *images[VK_RAL_MATERIAL_PLANES];
	const uint8_t planeBits[VK_RAL_MATERIAL_PLANES] = { 1u, 4u };
	ralResidencyPageId_t id;
	uint32_t i;
	if ( restore ) { vk_ral_material_reset( qtrue ); return qtrue; }
	if ( s_ral_material_images[0] || s_ral_material_images[1] ) {
		R_LOG( rch_ral_texture, SEV_WARN,
		       "RAL residency material: unavailable reason=already-active\n" );
		return qfalse;
	}
	// Exact committed PBR fixture: these are the base and packed ORM planes of
	// textures/pbr_test/pbr_test. Load through the same image owner/flags used by
	// shader parsing so the diagnostic exercises real material resources even on
	// maps that do not otherwise reference the fixture shader.
	images[0] = R_FindImageFile( "textures/pbr_test/albedo.png", IMGFLAG_MIPMAP );
	images[1] = R_FindImageFile( "textures/pbr_test/orm.png",
		IMGFLAG_MIPMAP | IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_DOMAIN_LINEAR );
	for ( i = 0; i < VK_RAL_MATERIAL_PLANES; ++i ) {
		ralTextureViewCreateInfo_t vci;
		if ( !images[i] || !images[i]->ral || !images[i]->ralResidencyView ||
		     images[i]->ralBindlessSlot < 0 || images[i]->ralResidencyMipCount < 2 ) {
			R_LOG( rch_ral_texture, SEV_WARN,
			       "RAL residency material: unavailable plane=%u image=%u texture=%u view=%u slot=%d mipRecords=%u name=%s\n",
			       i, images[i] ? 1u : 0u,
			       images[i] && images[i]->ral ? 1u : 0u,
			       images[i] && images[i]->ralResidencyView ? 1u : 0u,
			       images[i] ? images[i]->ralBindlessSlot : -1,
			       images[i] ? images[i]->ralResidencyMipCount : 0u,
			       images[i] ? images[i]->imgName : "none" );
			vk_ral_material_reset( qtrue ); return qfalse;
		}
		memset( &vci, 0, sizeof( vci ) );
		vci.texture = images[i]->ral; vci.viewType = RAL_TEXTURE_2D;
		vci.baseMipLevel = 1; vci.mipLevelCount = 0; vci.arrayLayerCount = 1;
		s_ral_material_coarse[i] = Ral_CreateTextureView( s_ral_backend, &vci );
		if ( !s_ral_material_coarse[i] ) {
			R_LOG( rch_ral_texture, SEV_WARN,
			       "RAL residency material: unavailable reason=coarse-view plane=%u\n", i );
			vk_ral_material_reset( qtrue ); return qfalse;
		}
		s_ral_material_images[i] = images[i];
		images[i]->ralMipResidency[0].id.planeMask = planeBits[i];
		if ( !vk_ral_mip_transition( images[i], 0, RAL_RESIDENCY_STALE, 0,
		                             qtrue, (uint32_t)tr.frameCount ) ) {
			R_LOG( rch_ral_texture, SEV_WARN,
			       "RAL residency material: unavailable reason=page-transition plane=%u state=%u mask=%u serial=%u frame=%u\n",
			       i, images[i]->ralMipResidency[0].state,
			       images[i]->ralMipResidency[0].id.planeMask,
			       images[i]->ralMipResidency[0].transitionSerial,
			       (uint32_t)tr.frameCount );
			vk_ral_material_reset( qtrue ); return qfalse;
		}
	}
	memset( &id, 0, sizeof( id ) );
	id.classId = RAL_RESIDENCY_CLASS_TEXTURE;
	id.resource = images[0]->ralMipResidency[0].id.resource;
	id.level = 0; id.planeMask = 5;
	if ( !Ral_ResidencyPageRecordInit( &s_ral_material_group, &id,
	                                  RAL_RESIDENCY_RESIDENT, (uint32_t)tr.frameCount ) ||
	     !Ral_ResidencyPageRecordTransition( &s_ral_material_group,
	                                        RAL_RESIDENCY_STALE, 0, 1,
	                                        (uint32_t)tr.frameCount ) ) {
		R_LOG( rch_ral_texture, SEV_WARN,
		       "RAL residency material: unavailable reason=group-transition\n" );
		vk_ral_material_reset( qtrue ); return qfalse;
	}
	{
		uint32_t slots[2] = { (uint32_t)images[0]->ralBindlessSlot, (uint32_t)images[1]->ralBindlessSlot };
		ralTextureView_t *views[2] = { s_ral_material_coarse[0], s_ral_material_coarse[1] };
		const vkBindlessPublicationKind_t kinds[2] = {
			VK_BINDLESS_PUBLICATION_COARSE, VK_BINDLESS_PUBLICATION_COARSE };
		if ( !vk_ral_bindless_publish_texture_views( images, slots, views, kinds, 2 ) ) {
			R_LOG( rch_ral_texture, SEV_WARN,
			       "RAL residency material: unavailable reason=atomic-parent-bind\n" );
			vk_ral_material_reset( qtrue ); return qfalse;
		}
	}
	s_ral_material_start_frame = tr.frameCount;
	R_LOG( rch_ral_texture, SEV_WARN,
	       "RAL residency material: action=hold material=%u level=0 planeMask=5 state=stale completedMask=0 baseResource=%u baseSlot=%d ormResource=%u ormSlot=%d baseMip=1 atomic=1\n",
	       id.resource, images[0]->ralMipResidency[0].id.resource, images[0]->ralBindlessSlot,
	       images[1]->ralMipResidency[0].id.resource, images[1]->ralBindlessSlot );
	return qtrue;
}

const char *vk_ral_residency_material_source( int plane, int *width, int *height ) {
	image_t *image = plane >= 0 && plane < VK_RAL_MATERIAL_PLANES ? s_ral_material_images[plane] : NULL;
	if ( width ) *width = image ? image->width : 0;
	if ( height ) *height = image ? image->height : 0;
	return image ? image->imgName : NULL;
}

qboolean vk_ral_residency_material_upload( byte *const pics[2], const int widths[2], const int heights[2] ) {
	uint32_t i;
	if ( !pics || !widths || !heights || s_ral_material_group.state != RAL_RESIDENCY_STALE ) return qfalse;
	for ( i = 0; i < VK_RAL_MATERIAL_PLANES; ++i ) {
		ralTextureUploadDesc_t up;
		image_t *image = s_ral_material_images[i];
		if ( !image || !pics[i] || widths[i] != image->width || heights[i] != image->height ) return qfalse;
		memset( &up, 0, sizeof( up ) ); up.data = pics[i]; up.dataSize = (uint64_t)widths[i] * heights[i] * 4u;
		up.suppressMipGeneration = qtrue;
		s_ral_material_tickets[i] = Ral_TextureUploadBegin( image->ral, &up );
		if ( !s_ral_material_tickets[i].fence ) { vk_ral_material_reset( qtrue ); return qfalse; }
		if ( !vk_ral_mip_transition( image, 0, RAL_RESIDENCY_IN_FLIGHT, 0,
		                             qtrue, (uint32_t)tr.frameCount ) ) {
			vk_ral_material_reset( qtrue ); return qfalse;
		}
	}
	if ( !Ral_ResidencyPageRecordTransition( &s_ral_material_group,
	                                        RAL_RESIDENCY_IN_FLIGHT, 0, 1,
	                                        (uint32_t)tr.frameCount ) ) {
		vk_ral_material_reset( qtrue ); return qfalse;
	}
	R_LOG( rch_ral_texture, SEV_WARN,
	       "RAL residency material: action=upload-start material=%u level=0 planeMask=5 state=in-flight completedMask=0 parentBound=1 heldFrames=%d atomic=1\n",
	       s_ral_material_group.id.resource, tr.frameCount - s_ral_material_start_frame );
	return qtrue;
}

void vk_ral_register_image( image_t *image, byte *pic, int width, int height ) {
	ralTextureCreateInfo_t tci;
	ralTextureViewCreateInfo_t vci;
	uint32_t               slot;
	uint32_t               resource = ~0u;
	uint32_t               mipLevels;

	if ( !vk_ral_textures_available() || !image ) return;
	if ( image->ral ) return;                                          // already registered
	if ( image->texType != TEXTYPE_2D ) return;                        // 2D only for now; cube/3D parallel registration lands later
	if ( width <= 0 || height <= 0 ) { s_ral_skipped_no_data++; return; }  // defensive — avoid 0-byte upload
	// pic==NULL is legitimate now that the bindless main path is the sole
	// renderervk main path: the merged-lightmap atlas (tr_map.c:463) creates
	// the image_t up front and fills its pixels later via per-tile sub-region
	// uploads through vk_upload_image_data; the bindless sampler reads the
	// RAL texture, so the empty alloc must land.
	// legacy-mainpath-retire STEP 4: the `!vk.useBindlessMainPath` half of
	// the pre-retire skip is gone (no legacy path remains).

	memset( &tci, 0, sizeof( tci ) );
	tci.type               = RAL_TEXTURE_2D;
	tci.format             = RAL_FORMAT_R8G8B8A8_UNORM;                // forces RGBA8 for the parallel RAL texture; legacy may pick 4-bit packed format independently (bindless table is unused on this path so format mismatch is harmless)
	tci.width              = (uint32_t)width;
	tci.height             = (uint32_t)height;
	tci.depthOrArrayLayers = 1;
	tci.mipLevels          = ( image->flags & IMGFLAG_MIPMAP ) ? 0u : 1u;   // 0 → RAL picks full chain via ralVk_FullMipChain
	tci.sampleCount        = 1;
	tci.usage              = RAL_TEXTURE_USAGE_SAMPLED;                // no STORAGE / no COLOR_ATTACHMENT — bindless sampled texture only. TRANSFER_DST is always enabled by ralVk_TextureUsage so sub-region mirrors from vk_upload_image_data land fine.
	tci.memory             = RAL_MEMORY_DEVICE_LOCAL;
	tci.debugName          = image->imgName;
	// Create the image with graphics+transfer concurrent sharing when an async
	// transfer upload could take it. Only the non-mipmap (mipLevels==1) uploads are
	// transfer-eligible (mipmap textures need GPU mip-gen on graphics). Built-in images
	// (imgName starting with '*': *white/*black/*default/*dlight/*fog/…) are EXCLUDED:
	// they are bound directly by the renderer (tr.whiteImage etc.) outside the bindless
	// table, where the placeholder-swap can't stand in, and they are tiny — so they keep
	// the synchronous graphics path, whose SHADER_READ_ONLY transition is visible to
	// sampling without a separate cross-queue wait. The async path is the disk-texture
	// path (sampled only through the bindless slot).
	//
	// The async path is ALSO restricted to textures registered INSIDE an active render
	// frame (vk.frame_count != 0). The per-frame residency drain + graphics acquire that
	// make an async upload visible run at the start of the NEXT vk_begin_frame; a texture
	// registered during a synchronous load burst (boot UI, map-load fonts / HUD / model
	// textures) can be sampled before any such boundary, so it must upload synchronously
	// (resident on return). Inside a frame, the next frame's drain covers it. The flag is
	// inert without a dedicated transfer queue or with r_asyncTextureUpload off
	// (Ral_TextureUploadBegin re-checks).
	if ( tci.mipLevels == 1u && r_asyncTextureUpload->integer && vk.frame_count != 0
	     && image->imgName[0] != '*'
	     && tr.defaultImage && tr.defaultImage->ral ) {
		const ralCaps_t *caps = Ral_GetCaps( s_ral_backend );
		if ( caps && caps->asyncTransfer ) tci.concurrentGraphicsTransfer = qtrue;
	}
	image->ral = Ral_CreateTexture( s_ral_backend, &tci );
	if ( !image->ral ) {
		R_LOG( rch_ral_texture, SEV_WARN, "Ral_CreateTexture failed for '%s' (%dx%d)\n", image->imgName, width, height );
		return;
	}
	mipLevels = Ral_GetTextureMipLevelCount( image->ral );
	if ( tr.numImages > 0 && tr.images[tr.numImages - 1] == image )
		resource = (uint32_t)( tr.numImages - 1 );
	// Empty create-then-fill images retain the legacy lifecycle until their
	// sub-region producer gains page records.  Every ordinary decoded image gets
	// an exact persistent address/state table before its first upload.
	if ( pic && resource != ~0u &&
	     !vk_ral_init_mip_records( image, resource, mipLevels,
	                               RAL_RESIDENCY_ABSENT,
	                               (uint32_t)tr.frameCount ) ) {
		R_LOG( rch_ral_texture, SEV_WARN,
		       "RAL residency page record init refused for '%s' (resource=%u mipLevels=%u capacity=%u)\n",
		       image->imgName, resource, mipLevels,
		       (unsigned)MAX_IMAGE_RESIDENCY_MIPS );
	}

	// Bind through an explicit portable view even while it spans the full chain.
	// This is byte/visual-equivalent to texture->defaultView, but makes the
	// residency boundary capable of holding a coarse parent mip independently of
	// the child upload without exposing a Vulkan image view outside the backend.
	memset( &vci, 0, sizeof( vci ) );
	vci.texture         = image->ral;
	vci.viewType        = RAL_TEXTURE_2D;
	vci.format          = RAL_FORMAT_UNDEFINED;
	vci.baseMipLevel    = 0;
	vci.mipLevelCount   = 0; // full remaining chain
	vci.baseArrayLayer  = 0;
	vci.arrayLayerCount = 1;
	image->ralResidencyView = Ral_CreateTextureView( s_ral_backend, &vci );
	if ( !image->ralResidencyView ) {
		R_LOG( rch_ral_texture, SEV_WARN, "Ral_CreateTextureView failed for residency view '%s'\n", image->imgName );
		Ral_DestroyTexture( image->ral );
		image->ral = NULL;
		return;
	}

	// Allocate the bindless slot up front, then bind the *default checkerboard
	// placeholder into it before the upload, and swap the real texture in once
	// the upload has landed. The upload below is synchronous (it waits inline),
	// so the placeholder is replaced within this call and is never sampled — the
	// slot holds the real texture before the function returns, identical to a
	// direct bind. Binding the slot first is the ordering the residency swap
	// relies on once the upload becomes asynchronous; the slot allocator is a
	// single choke point so the index source can change without touching this
	// flow. Over-capacity images get no slot (and no placeholder bind).
	qboolean placeholderBound = qfalse;
	slot = vk_ral_alloc_bindless_slot( image );
	if ( slot < s_ral_bindless_capacity ) {
		// The placeholder must be a resident texture. tr.defaultImage is the
		// first image created, so while it is itself being registered (or any
		// image that registers before it is set) there is no placeholder yet —
		// those skip the placeholder bind and rely on the real-texture bind
		// below, which is unchanged behaviour.
		if ( tr.defaultImage && tr.defaultImage != image && tr.defaultImage->ral ) {
			(void)vk_ral_bindless_publish_texture( image, slot,
				tr.defaultImage->ral, VK_BINDLESS_PUBLICATION_PLACEHOLDER );
			placeholderBound = qtrue;
		}
	}

	// Upload the initial mip0 only when source bytes are present, through the
	// residency-ticket path: begin the upload, and once the texture is resident swap
	// it into the slot (replacing the placeholder). A synchronous upload is resident on
	// return and swaps inline; an async (transfer-queue) upload may still be in flight —
	// the texture is recorded on the pending list and swapped by vk_ral_drain_pending_
	// uploads() once the copy completes, the placeholder standing in until then. For the
	// pic==NULL case (merged lightmap and any other create-empty-then-fill callers) the
	// RAL texture content is undefined until the first vk_upload_image_data mirror lands
	// — which fires automatically once image->ral is non-NULL; no upload is begun here.
	qboolean resident = qtrue;
	if ( pic ) {
		ralTextureUploadDesc_t up;
		ralUploadTicket_t      ticket;
		memset( &up, 0, sizeof( up ) );
		up.mipLevel   = 0;
		up.arrayLayer = 0;
		up.data       = pic;
		up.dataSize   = (uint64_t)width * (uint64_t)height * 4u;
		ticket = Ral_TextureUploadBegin( image->ral, &up );
		// A synchronous upload is resident on return and is swapped inline (its layout is
		// already visible to graphics). An async upload — even if its fence happens to be
		// signaled already — goes through the pending list so the per-frame drain issues
		// the graphics-queue acquire that makes its layout visible to sampling; swapping
		// it inline here would skip that acquire and leave it UNDEFINED-from-graphics.
		// Deferring is only safe when a placeholder is bound (else the slot would be
		// UNDEFINED until the swap); the early built-ins that register before
		// tr.defaultImage exists have no placeholder and are excluded from async upstream,
		// but guard here too and fall back to a blocking wait + inline swap if needed.
		qboolean canDefer = ( placeholderBound && slot < s_ral_bindless_capacity
		                      && s_ral_pending_upload_count < VK_RAL_MAX_PENDING_UPLOADS ) ? qtrue : qfalse;
		if ( ticket.synchronous ) {
			uint32_t level;
			resident = qtrue;
			s_ral_upload_sync_count++;
			vk_ral_release_upload_ticket( &ticket );
			for ( level = 0; level < image->ralResidencyMipCount; ++level )
				if ( !vk_ral_mip_mark_resident( image, level,
				                                (uint32_t)tr.frameCount ) ) resident = qfalse;
		} else if ( canDefer ) {
			// In flight: keep the placeholder bound; the drain swaps the real texture in
			// and issues the graphics acquire once the fence signals.
			resident = qfalse;
			s_ral_upload_async_count++;
			vk_ral_pending_upload_t *p = &s_ral_pending_uploads[ s_ral_pending_upload_count++ ];
			p->ticket = ticket;
			p->slot   = slot;
			p->image  = image;
			if ( image->ralResidencyMipCount > 0 ) {
				if ( !vk_ral_mip_transition( image, 0, RAL_RESIDENCY_REQUESTED,
				                             0, qtrue, (uint32_t)tr.frameCount ) ||
				     !vk_ral_mip_transition( image, 0, RAL_RESIDENCY_IN_FLIGHT,
				                             0, qtrue, (uint32_t)tr.frameCount ) )
					R_LOG( rch_ral_texture, SEV_WARN,
					       "RAL residency page transition refused for '%s' initial async upload\n",
					       image->imgName );
			}
			if ( s_ral_pending_upload_count > s_ral_pending_peak ) s_ral_pending_peak = s_ral_pending_upload_count;
		} else {
			// Can't defer (no placeholder / list full): the async path can't issue its
			// graphics acquire through the drain, so fall back to the synchronous upload
			// for visibility. Wait the async copy, then re-upload synchronously so the
			// graphics-visible layout transition is recorded; correctness over pipelining.
			if ( ticket.fence ) Ral_WaitFence( ticket.fence, ~0ull );
			vk_ral_release_upload_ticket( &ticket );
			ralFence_t *sf = Ral_TextureUploadAsync( image->ral, &up );
			if ( sf ) { Ral_WaitFence( sf, ~0ull ); Ral_DestroyFence( sf ); }
			resident = qtrue;
			{
				uint32_t level;
				for ( level = 0; level < image->ralResidencyMipCount; ++level )
					if ( !vk_ral_mip_mark_resident( image, level,
					                                (uint32_t)tr.frameCount ) ) resident = qfalse;
			}
		}
	}

	// Slots above the bindless capacity keep their RAL texture alive but don't
	// appear in the BindGroup. Within capacity, swap the real texture into the
	// slot once it is resident (the placeholder stays bound until the drain swaps).
	if ( slot < s_ral_bindless_capacity ) {
		if ( resident ) {
			if ( vk_ral_whole_texture_promotion_ready( image ) )
				(void)vk_ral_bindless_publish_texture_view( image, slot,
					image->ralResidencyView, VK_BINDLESS_PUBLICATION_RESIDENT );
			else
				R_LOG( rch_ral_texture, SEV_WARN, "RAL residency promotion refused for '%s' (incomplete coherence group or parent fallback)\n", image->imgName );
		}
		image->ralBindlessSlot = (int)slot;
		s_ral_registered_count++;
		vk_ral_record_name( image->imgName );
	} else {
		image->ralBindlessSlot = -1;
		s_ral_skipped_no_slot++;
		vk_ral_warn_bindless_full();
	}
	image->flags &= ~IMGFLAG_RESIDENCY_EVICTED;
}

// Swap any async-uploaded textures whose transfer copy has completed from the *default
// placeholder to the real texture, then make the batch visible to graphics. The transfer
// copy left each image in SHADER_READ_ONLY on the transfer queue, but the graphics queue
// has no record of the layout; a single graphics-queue acquire barrier per batch
// establishes it, so every subsequent graphics submit (the main frame, the legacy
// upload-queue path, startup UI/font/console draws) samples a valid layout — no
// dependence on any one submit carrying a wait. Called once per frame by the renderer;
// entries not yet resident stay pending for a later frame.
void vk_ral_drain_pending_uploads( void ) {
	ralUploadTicket_t acquire[ VK_RAL_MAX_PENDING_UPLOADS ];
	uint32_t      acquireCount = 0;
	uint32_t      i = 0;
	if ( !vk_ral_textures_available() ) return;
	// First gather a stable copy of every signaled ticket. The exact-range
	// graphics acquire must be submitted before any descriptor publishes the
	// uploaded image; if command allocation fails, leave the entries pending and
	// retry next frame without consuming their binary semaphores.
	for ( i = 0; i < s_ral_pending_upload_count; i++ ) {
		vk_ral_pending_upload_t *p = &s_ral_pending_uploads[i];
		if ( vk_ral_upload_ticket_complete( &p->ticket ) &&
		     p->slot < s_ral_bindless_capacity && p->image && p->image->ral && p->image->ralResidencyView &&
		     ( p->image->ralResidencyMipCount == 0 ||
		       vk_ral_mip_promotion_ready( p->image, 0 ) ) ) {
			acquire[ acquireCount++ ] = p->ticket;
		}
	}
	if ( acquireCount > 0 && !Ral_TextureAcquireBatchToGraphics( s_ral_backend, acquire, acquireCount ) ) return;
	i = 0;
	while ( i < s_ral_pending_upload_count ) {
		vk_ral_pending_upload_t *p = &s_ral_pending_uploads[i];
		if ( vk_ral_upload_ticket_complete( &p->ticket ) ) {
			if ( p->slot < s_ral_bindless_capacity && p->image && p->image->ral && p->image->ralResidencyView &&
			     ( p->image->ralResidencyMipCount == 0 ||
			       vk_ral_mip_mark_resident( p->image, 0, (uint32_t)tr.frameCount ) ) &&
			     vk_ral_whole_texture_promotion_ready( p->image ) ) {
				(void)vk_ral_bindless_publish_texture_view( p->image, p->slot,
					p->image->ralResidencyView, VK_BINDLESS_PUBLICATION_RESIDENT );
			}
			vk_ral_release_upload_ticket( &p->ticket );
			// Remove by swapping the last entry into this slot (order doesn't matter).
			*p = s_ral_pending_uploads[ --s_ral_pending_upload_count ];
		} else {
			i++;
		}
	}
	// The page-level mip test uses a graphics-queue no-wait copy.  Its parent
	// view stays bound across frames until this poll observes the real fence;
	// only then may the exact same slot expose child mip 0 again.
	if ( vk_ral_upload_ticket_complete( &s_ral_mip_test_ticket ) ) {
		image_t *image = s_ral_mip_test_image;
		if ( image && image->ral && image->ralResidencyView && image->ralCoarseResidencyView &&
		     image->ralBindlessSlot >= 0 &&
		     vk_ral_mip_transition( image, 0, RAL_RESIDENCY_RESIDENT, 1,
		                            qtrue, (uint32_t)tr.frameCount ) ) {
			uint32_t mipLevels = Ral_GetTextureMipLevelCount( image->ral );
			int heldFrames = tr.frameCount - s_ral_mip_test_start_frame;
			int uploadFrames = tr.frameCount - s_ral_mip_test_upload_frame;
			int sampleAge = tr.frameCount - image->frameUsed;
			(void)vk_ral_bindless_publish_texture_view( image,
			                                   (uint32_t)image->ralBindlessSlot,
			                                   image->ralResidencyView,
			                                   VK_BINDLESS_PUBLICATION_RESIDENT );
			R_LOG( rch_ral_texture, SEV_WARN,
			       "RAL residency mip test: action=upload-promote class=texture resource=%d slot=%d childLevel=0 parentLevel=1 baseMip=0 levelCount=%u bytes=%llu synchronous=%d fenceSignaled=1 heldFrames=%d uploadFrames=%d sampleAge=%d fallback=parent source=decoded name=%s\n",
			       s_ral_mip_test_resource, image->ralBindlessSlot, mipLevels,
			       (unsigned long long)s_ral_mip_test_upload_bytes,
			       s_ral_mip_test_ticket.synchronous ? 1 : 0,
			       heldFrames, uploadFrames, sampleAge, image->imgName );
			vk_ral_log_mip_record( image, 0, "upload-promote" );
			Ral_DestroyTextureView( image->ralCoarseResidencyView );
			image->ralCoarseResidencyView = NULL;
		}
		vk_ral_release_upload_ticket( &s_ral_mip_test_ticket );
		s_ral_mip_test_image = NULL;
		s_ral_mip_test_resource = -1;
		s_ral_mip_test_start_frame = 0;
		s_ral_mip_test_upload_frame = 0;
		s_ral_mip_test_upload_bytes = 0;
	}
	// Material coherence diagnostic: independently completed base/ORM copies
	// retain both parent views. Only the complete planeMask=5 group may advance
	// and one batched descriptor update publishes both child views together.
	{
		const uint8_t planeBits[VK_RAL_MATERIAL_PLANES] = { 1u, 4u };
		uint32_t j;
		for ( j = 0; j < VK_RAL_MATERIAL_PLANES; ++j ) {
			if ( vk_ral_upload_ticket_complete( &s_ral_material_tickets[j] ) ) {
				s_ral_material_completed |= planeBits[j];
				vk_ral_release_upload_ticket( &s_ral_material_tickets[j] );
				R_LOG( rch_ral_texture, SEV_WARN,
				       "RAL residency material plane: action=complete material=%u plane=%s bit=%u resource=%u slot=%d completedMask=%u\n",
				       s_ral_material_group.id.resource, j == 0 ? "base" : "orm",
				       planeBits[j], s_ral_material_images[j]->ralMipResidency[0].id.resource,
				       s_ral_material_images[j]->ralBindlessSlot, s_ral_material_completed );
			}
		}
		if ( s_ral_material_group.state == RAL_RESIDENCY_IN_FLIGHT &&
		     s_ral_material_completed == s_ral_material_group.id.planeMask ) {
			ralResidencyPageRecord_t nextRecords[VK_RAL_MATERIAL_PLANES];
			ralResidencyPageRecord_t nextGroup = s_ral_material_group;
			uint32_t slots[2]; ralTextureView_t *views[2];
			const vkBindlessPublicationKind_t kinds[2] = {
				VK_BINDLESS_PUBLICATION_RESIDENT, VK_BINDLESS_PUBLICATION_RESIDENT };
			qboolean ready = qtrue;
			for ( j = 0; j < VK_RAL_MATERIAL_PLANES; ++j ) {
				image_t *image = s_ral_material_images[j];
				if ( !image ) { ready = qfalse; continue; }
				nextRecords[j] = image->ralMipResidency[0];
				if ( !Ral_ResidencyPageRecordTransition( &nextRecords[j],
					RAL_RESIDENCY_RESIDENT, planeBits[j], 1,
					(uint32_t)tr.frameCount ) ) ready = qfalse;
				slots[j] = (uint32_t)image->ralBindlessSlot;
				views[j] = image->ralResidencyView;
			}
			if ( ready && Ral_ResidencyPageRecordTransition( &nextGroup,
				RAL_RESIDENCY_RESIDENT, s_ral_material_completed, 1,
				(uint32_t)tr.frameCount ) &&
			     vk_ral_bindless_publish_texture_views( s_ral_material_images,
				     slots, views, kinds, 2 ) ) {
				for ( j = 0; j < VK_RAL_MATERIAL_PLANES; ++j )
					s_ral_material_images[j]->ralMipResidency[0] = nextRecords[j];
				s_ral_material_group = nextGroup;
				R_LOG( rch_ral_texture, SEV_WARN,
				       "RAL residency material: action=promote material=%u level=0 planeMask=5 state=resident completedMask=5 baseSlot=%u ormSlot=%u heldFrames=%d atomic=1\n",
				       s_ral_material_group.id.resource, slots[0], slots[1],
				       tr.frameCount - s_ral_material_start_frame );
				vk_ral_material_reset( qfalse );
			}
		}
	}
}

qboolean vk_ral_residency_mip_test( qboolean restore ) {
	image_t *image = s_ral_mip_test_image;
	uint32_t mipLevels;
	int i;

	if ( !vk_ral_textures_available() ) return qfalse;
	if ( restore ) {
		if ( s_ral_mip_test_ticket.fence ) {
			R_LOG( rch_ral_texture, SEV_WARN, "RAL residency mip test: restore refused (child upload in flight)\n" );
			return qfalse;
		}
		if ( !image || !image->ral || !image->ralResidencyView ||
		     image->ralBindlessSlot < 0 || image->ralCoarseResidencyView == NULL ) {
			R_LOG( rch_ral_texture, SEV_WARN, "RAL residency mip test: restore refused (no held parent view)\n" );
			return qfalse;
		}
		if ( !vk_ral_mip_transition( image, 0, RAL_RESIDENCY_RESIDENT, 1,
		                             qtrue, (uint32_t)tr.frameCount ) ) {
			R_LOG( rch_ral_texture, SEV_WARN,
			       "RAL residency mip test: restore refused (page state transition failed)\n" );
			return qfalse;
		}
		(void)vk_ral_bindless_publish_texture_view( image,
			(uint32_t)image->ralBindlessSlot, image->ralResidencyView,
			VK_BINDLESS_PUBLICATION_RESIDENT );
		mipLevels = Ral_GetTextureMipLevelCount( image->ral );
		R_LOG( rch_ral_texture, SEV_WARN,
		       "RAL residency mip test: action=promote class=texture resource=%d slot=%d childLevel=0 parentLevel=1 baseMip=0 levelCount=%u heldFrames=%d sampleAge=%d fallback=parent name=%s\n",
		       s_ral_mip_test_resource, image->ralBindlessSlot, mipLevels,
		       tr.frameCount - s_ral_mip_test_start_frame,
		       tr.frameCount - image->frameUsed, image->imgName );
		vk_ral_log_mip_record( image, 0, "promote" );
		Ral_DestroyTextureView( image->ralCoarseResidencyView );
		image->ralCoarseResidencyView = NULL;
		s_ral_mip_test_image = NULL;
		s_ral_mip_test_resource = -1;
		s_ral_mip_test_start_frame = 0;
		s_ral_mip_test_upload_frame = 0;
		s_ral_mip_test_upload_bytes = 0;
		return qtrue;
	}

	if ( s_ral_mip_test_image != NULL ) {
		R_LOG( rch_ral_texture, SEV_WARN, "RAL residency mip test: hold refused (parent view already held)\n" );
		return qfalse;
	}
	// Highest frameUsed wins; resource index breaks ties. This selects a texture
	// the current map actually sampled rather than a registration-order fixture.
	for ( i = 0; i < tr.numImages; ++i ) {
		image_t *candidate = tr.images[i];
		uint32_t candidateMips;
		if ( !candidate || !candidate->ral || !candidate->ralResidencyView ||
		     candidate->ralBindlessSlot < 0 || candidate->imgName[0] == '*' ||
		     !( candidate->flags & IMGFLAG_MIPMAP ) || R_ImageIsPinned( candidate ) ||
		     candidate->ralResidencyMipCount == 0 ||
		     candidate->ralMipResidency[0].id.planeMask != 1u ) continue;
		candidateMips = Ral_GetTextureMipLevelCount( candidate->ral );
		if ( candidateMips < 2u ) continue;
		if ( !image || candidate->frameUsed > image->frameUsed ||
		     ( candidate->frameUsed == image->frameUsed && i < s_ral_mip_test_resource ) ) {
			image = candidate;
			s_ral_mip_test_resource = i;
		}
	}
	if ( !image ) {
		R_LOG( rch_ral_texture, SEV_WARN, "RAL residency mip test: hold refused (no live mipmapped texture)\n" );
		return qfalse;
	}
	{
		ralTextureViewCreateInfo_t vci;
		memset( &vci, 0, sizeof( vci ) );
		vci.texture = image->ral; vci.viewType = RAL_TEXTURE_2D;
		vci.baseMipLevel = 1u; vci.mipLevelCount = 0u;
		vci.baseArrayLayer = 0u; vci.arrayLayerCount = 1u;
		image->ralCoarseResidencyView = Ral_CreateTextureView( s_ral_backend, &vci );
	}
	if ( !image->ralCoarseResidencyView ) {
		R_LOG( rch_ral_texture, SEV_WARN, "RAL residency mip test: hold refused (coarse view creation failed)\n" );
		return qfalse;
	}
	mipLevels = Ral_GetTextureMipLevelCount( image->ral );
	if ( image->ralResidencyMipCount != mipLevels ||
	     image->ralMipResidency[0].id.resource != (uint32_t)s_ral_mip_test_resource ||
	     !vk_ral_mip_transition( image, 0, RAL_RESIDENCY_STALE, 0,
	                            qtrue, (uint32_t)tr.frameCount ) ) {
		Ral_DestroyTextureView( image->ralCoarseResidencyView );
		image->ralCoarseResidencyView = NULL;
		R_LOG( rch_ral_texture, SEV_WARN,
		       "RAL residency mip test: hold refused (persistent page state/address mismatch)\n" );
		return qfalse;
	}
	(void)vk_ral_bindless_publish_texture_view( image,
		(uint32_t)image->ralBindlessSlot, image->ralCoarseResidencyView,
		VK_BINDLESS_PUBLICATION_COARSE );
	s_ral_mip_test_image = image;
	s_ral_mip_test_start_frame = tr.frameCount;
	R_LOG( rch_ral_texture, SEV_WARN,
	       "RAL residency mip test: action=hold class=texture resource=%d slot=%d childLevel=0 parentLevel=1 baseMip=1 levelCount=%u sampleAge=%d fallback=parent name=%s\n",
	       s_ral_mip_test_resource, image->ralBindlessSlot, mipLevels - 1u,
	       tr.frameCount - image->frameUsed, image->imgName );
	vk_ral_log_mip_record( image, 0, "hold" );
	return qtrue;
}

const char *vk_ral_residency_mip_test_source( int *width, int *height ) {
	image_t *image = s_ral_mip_test_image;
	if ( width ) *width = image ? image->width : 0;
	if ( height ) *height = image ? image->height : 0;
	return ( image && image->imgName[0] ) ? image->imgName : NULL;
}

qboolean vk_ral_residency_mip_upload( const byte *pic, int width, int height ) {
	image_t               *image = s_ral_mip_test_image;
	ralTextureUploadDesc_t upload;
	ralUploadTicket_t      ticket;
	uint64_t               bytes;
	int                    heldFrames, sampleAge;

	if ( !image || !image->ral || !image->ralResidencyView ||
	     image->ralBindlessSlot < 0 || image->ralCoarseResidencyView == NULL ) {
		R_LOG( rch_ral_texture, SEV_WARN, "RAL residency mip test: upload refused (no held parent view)\n" );
		return qfalse;
	}
	if ( s_ral_mip_test_ticket.fence ) {
		R_LOG( rch_ral_texture, SEV_WARN, "RAL residency mip test: upload refused (child upload already in flight)\n" );
		return qfalse;
	}
	if ( !pic || width != image->width || height != image->height || width <= 0 || height <= 0 ) {
		R_LOG( rch_ral_texture, SEV_WARN,
		       "RAL residency mip test: upload refused (decoded source mismatch for %s: %dx%d expected %dx%d)\n",
		       image->imgName, width, height, image->width, image->height );
		return qfalse;
	}
	heldFrames = tr.frameCount - s_ral_mip_test_start_frame;
	sampleAge = tr.frameCount - image->frameUsed;
	if ( heldFrames < 20 || sampleAge > 1 ) {
		R_LOG( rch_ral_texture, SEV_WARN,
		       "RAL residency mip test: upload refused (parent hold/sample authority heldFrames=%d sampleAge=%d)\n",
		       heldFrames, sampleAge );
		return qfalse;
	}

	// The explicit no-mipgen flag overwrites only child mip 0 and preserves the
	// already-visible parent chain.  Ral_TextureUploadBegin submits this page copy
	// without waiting; the parent view remains bound until the per-frame drain
	// observes its completion fence and promotes the full-chain view.
	bytes = (uint64_t)(uint32_t)width * (uint64_t)(uint32_t)height * 4u;
	memset( &upload, 0, sizeof( upload ) );
	upload.mipLevel = 0;
	upload.arrayLayer = 0;
	upload.data = pic;
	upload.dataSize = bytes;
	upload.suppressMipGeneration = qtrue;
	ticket = Ral_TextureUploadBegin( image->ral, &upload );
	if ( !ticket.fence ) {
		R_LOG( rch_ral_texture, SEV_WARN, "RAL residency mip test: upload refused (child upload did not return a fence)\n" );
		return qfalse;
	}
	if ( !vk_ral_mip_transition( image, 0, RAL_RESIDENCY_IN_FLIGHT, 0,
	                            qtrue, (uint32_t)tr.frameCount ) ) {
		vk_ral_release_upload_ticket( &ticket );
		R_LOG( rch_ral_texture, SEV_WARN,
		       "RAL residency mip test: upload refused (page state transition failed)\n" );
		return qfalse;
	}
	s_ral_mip_test_ticket = ticket;
	s_ral_mip_test_upload_frame = tr.frameCount;
	s_ral_mip_test_upload_bytes = bytes;
	R_LOG( rch_ral_texture, SEV_WARN,
	       "RAL residency mip test: action=upload-start class=texture resource=%d slot=%d childLevel=0 parentLevel=1 baseMip=1 bytes=%llu synchronous=%d parentBound=1 heldFrames=%d sampleAge=%d fallback=parent source=decoded name=%s\n",
	       s_ral_mip_test_resource, image->ralBindlessSlot,
	       (unsigned long long)bytes, ticket.synchronous ? 1 : 0,
	       heldFrames, sampleAge, image->imgName );
	vk_ral_log_mip_record( image, 0, "upload-start" );
	return qtrue;
}

// True while any async upload is still in flight (its placeholder not yet swapped).
qboolean vk_ral_pending_uploads_active( void ) {
	return ( s_ral_pending_upload_count > 0 || s_ral_mip_test_ticket.fence != NULL ) ? qtrue : qfalse;
}

void vk_ral_upload_counts( uint32_t *syncOut, uint32_t *asyncOut ) {
	if ( syncOut )  *syncOut  = s_ral_upload_sync_count;
	if ( asyncOut ) *asyncOut = s_ral_upload_async_count;
}

// Allocate the bindless-set slot for an image (Phase 7.15.2 free-list path).
// Recycled slots (returned by vk_ral_unregister_image on eviction, 7.15.4) are
// reused first; otherwise the next never-used high-water slot is handed out. A
// value at or above s_ral_bindless_capacity means no slot is available (the
// caller keeps the RAL texture but skips the BindGroup). This is the single
// point that chooses a slot index. While no eviction caller exists, the
// free-list stays empty and this hands out 0, 1, 2, … in registration order —
// byte-identical to the old tr.numImages-1 source (each image was added to
// tr.images[] just before registration, so tr.numImages-1 == the sequential
// slot count). The counter now owns the index source so slot index ≠ image
// count once slots recycle.
// Reset the slot allocator to its post-init state (free-list empty, high-water 0).
// See header for why R_DeleteTextures must call this — the bulk unregister fills
// the free-list and only a reset restores the byte-identical sequential handout.
void vk_ral_reset_bindless_slots( void ) {
	s_ral_bindless_next       = 0;
	s_ral_bindless_free_count = 0;
}

static uint32_t vk_ral_alloc_bindless_slot( const image_t *image ) {
	(void)image;
	if ( s_ral_bindless_free_count > 0 ) {
		return s_ral_bindless_free[ --s_ral_bindless_free_count ];
	}
	if ( s_ral_bindless_next < s_ral_bindless_capacity ) {
		return s_ral_bindless_next++;
	}
	// Exhausted: return a slot at capacity so the caller takes the over-capacity
	// branch (skip BindGroup + one-shot warn), matching the old behaviour when
	// tr.numImages-1 reached the cap.
	return s_ral_bindless_capacity;
}

// One-shot log for the "bindless table full" case — keeps the log clean
// if a huge map blows past 4096 textures.
static void vk_ral_warn_bindless_full( void ) {
	if ( s_ral_full_warned ) return;
	s_ral_full_warned = qtrue;
	R_LOG( rch_ral_texture, SEV_WARN, "bindless table reached its %u-slot capacity; further images keep their RAL texture but won't appear in the set. Grow capacity or evict on pressure (Phase 7.4c).\n",
	        s_ral_bindless_capacity );
}

// DDS (BC*/packed) slot assignment — see header. Routes the DDS path through the
// SAME free-list allocator + over-capacity bookkeeping as vk_ral_register_image,
// so there is genuinely ONE slot-index source (was previously a raw
// tr.numImages-1 bypass in R_CreateImageDDS — a latent collision once eviction
// recycles slots). Unlike the main path this does NOT call Ral_BindGroupSetTexture
// At here: DDS writes its real BC*/packed view into the slot later, via
// vk_create_image -> vk_update_descriptor_set. Only the slot SOURCE is shared.
void vk_ral_assign_dds_slot( image_t *image ) {
	if ( !vk_ral_textures_available() || !image ) return;
	uint32_t slot = vk_ral_alloc_bindless_slot( image );
	if ( slot < s_ral_bindless_capacity ) {
		image->ralBindlessSlot = (int)slot;
		s_ral_registered_count++;
		vk_ral_record_name( image->imgName );
	} else {
		image->ralBindlessSlot = -1;
		s_ral_skipped_no_slot++;
		vk_ral_warn_bindless_full();
	}
}

void vk_ral_unregister_image( image_t *image ) {
	if ( !image || !s_ral_backend ) return;
	if ( s_ral_material_images[0] == image || s_ral_material_images[1] == image )
		vk_ral_material_reset( qfalse );
	if ( s_ral_mip_test_image == image ) {
		vk_ral_release_upload_ticket( &s_ral_mip_test_ticket );
		s_ral_mip_test_image = NULL;
		s_ral_mip_test_resource = -1;
		s_ral_mip_test_start_frame = 0;
		s_ral_mip_test_upload_frame = 0;
		s_ral_mip_test_upload_bytes = 0;
	}
	// Scrub any in-flight async-upload entries that reference this image BEFORE
	// it is freed. R_DeleteTextures calls us for every tr.images[] entry on a
	// map transition without draining s_ral_pending_uploads; once Hunk_ClearLevel
	// reuses the image_t storage, a stale p->image here becomes a dangling
	// pointer that vk_ral_drain_pending_uploads would later dereference
	// (use-after-free). Swap-remove each match (and free its fence).
	{
		uint32_t i = 0;
		while ( i < s_ral_pending_upload_count ) {
			vk_ral_pending_upload_t *p = &s_ral_pending_uploads[i];
			if ( p->image == image ) {
				vk_ral_release_upload_ticket( &p->ticket );
				*p = s_ral_pending_uploads[ --s_ral_pending_upload_count ];
				// don't advance i — the swapped-in entry must be re-checked
			} else {
				i++;
			}
		}
	}
	if ( image->ralBindlessSlot >= 0 && s_ral_bindless_set ) {
		(void)vk_ral_bindless_tombstone( (uint32_t)image->ralBindlessSlot );
		// Return the slot to the free-list so a later registration can reuse it
		// (Phase 7.15.2 release plumbing for the 7.15.4 eviction path). This is
		// DARK in 7.15.2: R_DeleteTextures unregisters every image on a map
		// transition and the bookkeeping reset (vk_ral_textures_init) clears the
		// free-list, so it never meaningfully fills during steady-state — no
		// eviction caller exists yet. The bound is structural (free-count can
		// never exceed the number of distinct slots handed out, which is
		// <= capacity <= array size), so no overflow guard is needed; assert the
		// invariant defensively.
		if ( s_ral_bindless_free_count < WIRED_BINDLESS_TEX_SLOTS ) {
			s_ral_bindless_free[ s_ral_bindless_free_count++ ] = (uint32_t)image->ralBindlessSlot;
		}
		image->ralBindlessSlot = -1;
	}
	if ( image->ralCoarseResidencyView ) {
		Ral_DestroyTextureView( image->ralCoarseResidencyView );
		image->ralCoarseResidencyView = NULL;
	}
	if ( image->ralResidencyView ) {
		Ral_DestroyTextureView( image->ralResidencyView );
		image->ralResidencyView = NULL;
	}
	if ( image->ral ) {
		Ral_DestroyTexture( image->ral );
		image->ral = NULL;
		s_ral_destroyed_count++;
	}
	image->ralResidencyMipCount = 0;
	memset( image->ralMipResidency, 0, sizeof( image->ralMipResidency ) );
}


// slot allocator for the parallel 2DArray SAMPLED_IMAGE binding.
// Used by vk_ral_register_image_array (defined in vk.c — the raw-write site
// needs the static-to-vk.c qvkUpdateDescriptorSets handle). Bumps the slot
// counter when a slot is available, returns -1 when the bindless array
// capacity is exhausted. The skipped counter feeds the diag dump.
int vk_ral_alloc_array_bindless_slot( const char *imgName ) {
	if ( !s_ral_backend || !s_ral_bindless_set ) return -1;
	if ( s_ral_bindless_array_count >= WIRED_BINDLESS_ARRAY_TEX_SLOTS ) {
		s_ral_skipped_no_array_slot++;
		R_LOG( rch_ral_texture, SEV_WARN, "2DArray bindless capacity (%u) reached; '%s' degrades to whiteImage sentinel\n",
		        (unsigned)WIRED_BINDLESS_ARRAY_TEX_SLOTS, imgName ? imgName : "<unnamed>" );
		return -1;
	}
	vk_ral_record_name( imgName );
	return (int)( s_ral_bindless_array_count++ );
}

// ── RAL buffer register / unregister ──────────────────────
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
	R_LOG( rch_ral_buffer, SEV_INFO, "flushed %u pending buffer(s) into the live RAL backend (%u failed)\n", created, failed );
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

	s_buf_register_total++;

	if ( s_ral_backend == NULL ) {
		// Backend not up yet — queue. Common path for vk_initialize-time
		// creates (staging / tess / storage / ribbon / beam / sprite /
		// particle / primitive / IQM bone / world VBO).
		if ( s_buf_pending_count >= VK_RAL_PENDING_BUFFER_MAX ) {
			if ( !s_buf_pending_warned_full ) {
				s_buf_pending_warned_full = qtrue;
				R_LOG( rch_ral_buffer, SEV_WARN, "pending-buffer queue full (%u) — register dropped for '%s'\n",
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
		if ( !rb ) { R_LOG( rch_ral_buffer, SEV_WARN, "Ral_CreateBuffer failed for '%s' (%llu bytes)\n", debugName ? debugName : "?", (unsigned long long)size ); return; }
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
	// register sites (none expected but defensive).
}

static void vk_ral_fill_diagnostic_create_info( ralBackendCreateInfo_t *ci ) {
	cvar_t *vkValidate;
	cvar_t *asyncUpload;
	memset( ci, 0, sizeof( *ci ) );
	ci->type                = RAL_BACKEND_VULKAN;
	ci->flags               = RAL_FLAG_DEBUG_LABELS;
	ci->host.userData       = NULL;
	ci->host.getProcAddress = vk_ral_host_get_proc;
	ci->host.createSurface  = vk_ral_host_create_surface;
	ci->host.log            = vk_ral_host_log;
	vkValidate = ri.Cvar_Get ? ri.Cvar_Get( "r_vkValidate", "0", CVAR_ARCHIVE | CVAR_LATCH ) : NULL;
	asyncUpload = ri.Cvar_Get ? ri.Cvar_Get( "r_asyncTextureUpload", "1", CVAR_ARCHIVE ) : NULL;
	ci->enableValidation = ( vkValidate && vkValidate->integer ) ? qtrue : qfalse;
	ci->allowAsyncTextureUploads = ( asyncUpload && asyncUpload->integer ) ? qtrue : qfalse;
}

// Engine-facing console wrappers stay renderer-owned.  The RAL archive sees
// only explicit arguments/imports and therefore remains linkable by a future
// standalone SDL3 tool without Cmd_Argv/Cvar/global-ri stubs.
Q_EXPORT void Ral_Dump( void ) {
	ralBackendCreateInfo_t ci;
	const char *sub = ( ri.Cmd_Argc && ri.Cmd_Argc() > 1 ) ? ri.Cmd_Argv( 1 ) : NULL;
	vk_ral_fill_diagnostic_create_info( &ci );
	Ral_RunDiagnostic( &ci, sub );
}

void Ral_RunPipelineDiagnostic( void ) {
	ralBackendCreateInfo_t ci;
	vk_ral_fill_diagnostic_create_info( &ci );
	Ral_RunDiagnostic( &ci, "pipeline" );
}

// ── "\ral_dump live" — dump the renderer-owned (imported-mode) backend ─
// Q_EXPORT'd so the client's Sys_LoadFunction in cl_main.c can resolve it.
// Reports caps, memory budget, and the texture/buffer registration state
// against the LIVE backend (shared with renderervk's VkDevice) instead of
// creating a throwaway one like Ral_Dump does.
Q_EXPORT void Ral_DumpLive( void ) {
	const ralCaps_t  *c;
	ralMemoryBudget_t mb;

	// `ral_dump live markers` is the exact renderer-DLL-owned one-frame arm.
	// The client command already resolved this live export; keep the profiler
	// receipt on the real imported backend without adding a second command ABI.
	if ( ri.Cmd_Argc() > 2 && Q_stricmp( ri.Cmd_Argv( 2 ), "markers" ) == 0 ) {
		vk_profile_markers_arm();
		return;
	}
	if ( ri.Cmd_Argc() > 2 && Q_stricmp( ri.Cmd_Argv( 2 ), "profile" ) == 0 ) {
		vk_gpu_profile_dump();
		return;
	}
	if ( ri.Cmd_Argc() > 2 && Q_stricmp( ri.Cmd_Argv( 2 ), "swapchain" ) == 0 ) {
		vk_request_swapchain_recreate();
		return;
	}
	if ( ri.Cmd_Argc() > 2 && Q_stricmp( ri.Cmd_Argv( 2 ), "temporal" ) == 0 ) {
		R_TemporalProjectionDump();
		return;
	}
	if ( ri.Cmd_Argc() > 2 && Q_stricmp( ri.Cmd_Argv( 2 ), "temporal-motion-arm" ) == 0 ) {
		vk_temporal_motion_readback_arm();
		return;
	}
	if ( ri.Cmd_Argc() > 2 && Q_stricmp( ri.Cmd_Argv( 2 ), "temporal-history-arm" ) == 0 ) {
		vk_temporal_history_consume_arm();
		return;
	}
	if ( ri.Cmd_Argc() > 2 && Q_stricmp( ri.Cmd_Argv( 2 ), "temporal-resolve-arm" ) == 0 ) {
		vk_temporal_resolve_readback_arm();
		return;
	}

	R_LOG( rch_ral, SEV_INFO, "===== \\ral_dump live (renderer-owned imported-mode backend) =====\n" );

	if ( !s_ral_init_attempted ) {
		R_LOG( rch_ral, SEV_INFO, "  RAL bringup has not been attempted yet this session — vid_restart to retry\n" );
		R_LOG( rch_ral, SEV_INFO, "===== end \\ral_dump live =====\n" );
		return;
	}
	if ( !s_ral_backend ) {
		R_LOG( rch_ral, SEV_WARN, "  RAL bringup attempted but failed — see earlier [RAL-TEX] / [RAL] warnings\n" );
		R_LOG( rch_ral, SEV_INFO, "===== end \\ral_dump live =====\n" );
		return;
	}

	c = Ral_GetCaps( s_ral_backend );
	if ( c ) {
		R_LOG( rch_ral, SEV_INFO, "Ral_GetCaps (live):\n" );
		R_LOG( rch_ral, SEV_INFO, "  device                   : %s\n", c->deviceName );
		R_LOG( rch_ral, SEV_INFO, "  apiVersion               : %s\n", c->apiVersion );
		R_LOG( rch_ral, SEV_INFO, "  bindlessTextures         : %s (max %u)\n", c->bindlessTextures ? "yes" : "no", c->maxBindlessTextures );
		R_LOG( rch_ral, SEV_INFO, "  dynamicRendering         : %s\n", c->dynamicRendering ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  timelineSemaphores       : %s\n", c->timelineSemaphores ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  asyncCompute / Transfer  : %s / %s\n", c->asyncCompute ? "yes" : "no", c->asyncTransfer ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  debugUtils               : %s\n", c->debugUtils ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  memoryBudget             : %s (imported mode leaves this off in 7.4c-pre)\n", c->memoryBudget ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  drawIndirectCount        : %s\n", c->drawIndirectCount ? "yes" : "no" );
		R_LOG( rch_ral, SEV_INFO, "  maxTextureDimension2D    : %u\n", c->maxTextureDimension2D );
		R_LOG( rch_ral, SEV_INFO, "  maxSamplerAnisotropy     : %.0fx\n", c->maxSamplerAnisotropy );
	}

	Ral_QueryMemoryBudget( s_ral_backend, &mb );
	R_LOG( rch_ral, SEV_INFO, "Ral_QueryMemoryBudget (live):\n" );
	R_LOG( rch_ral, SEV_INFO, "  device-local : %u / %u MiB used\n", (unsigned)( mb.deviceLocalUsed >> 20 ), (unsigned)( mb.deviceLocalBudget >> 20 ) );
	R_LOG( rch_ral, SEV_INFO, "  host-visible : %u / %u MiB used\n", (unsigned)( mb.hostVisibleUsed >> 20 ), (unsigned)( mb.hostVisibleBudget >> 20 ) );
	R_LOG( rch_ral, SEV_INFO, "  underPressure: %s\n", mb.underPressure ? "yes" : "no" );

	// Texture + buffer registration state — re-uses the existing dump.
	vk_ral_textures_diag_dump();

	// pipeline-layout cache slot
	// enumeration on the live imported-mode backend. This is the slot count
	// the renderer actually drives draws against; the throwaway-backend
	// version (\ral_dump pipeline, exits via Ral_Dump → Ral_CreateBackend →
	// ralVk_RunPipelineTest) shows an empty cache pre-synthetic-test.
	Ral_DumpPipelineLayoutCache( s_ral_backend );

	R_LOG( rch_ral, SEV_INFO, "===== end \\ral_dump live =====\n" );
}

// ── "\ral_pipeline_test" — exact offscreen pipeline exercise ─────────
// Q_EXPORT'd so cl_main.c can resolve it through Sys_LoadFunction. The old
// 19-fixture sibling-field walk retired with that scaffolding; this command
// now delegates to the same production RAL draw/readback + compute + cache
// exercise as "\ral_dump pipeline".
typedef struct {
	const char           *name;
	const ralPipeline_t **field;
	const char           *note;
} ral_pipeline_test_fixture_t;

Q_EXPORT void Ral_PipelineTest( void ) {
	R_LOG( rch_ral, SEV_INFO,
	       "ral_pipeline_test: running exact offscreen RAL pipeline exercise\n" );
	Ral_RunPipelineDiagnostic();
}

#if 0  /* retired Ral_PipelineTest body — kept for archival reference only */
static void Ral_PipelineTest_retired( void ) {
	uint32_t pass = 0, fail = 0, na = 0, i;

	R_LOG( rch_ral, SEV_INFO, "===== \\ral_pipeline_test (Phase 7.4c-pipeline-followup-4) =====\n" );

	if ( !s_ral_init_attempted || !s_ral_backend ) {
		R_LOG( rch_ral, SEV_INFO, "  RAL backend not live — vid_restart and retry.\n" );
		R_LOG( rch_ral, SEV_INFO, "===== end \\ral_pipeline_test =====\n" );
		return;
	}

	// Special-case sibling fields populated by per-call vk_ral_create_special_pipeline.
	// Fixtures numbered per the 19-entry fixture table; some are "umbrella" entries
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
	};

	R_LOG( rch_ral, SEV_INFO, "  Walking %u §17.7 fixtures (sibling field non-NULL = PASS, no live trigger = N/A):\n", (unsigned)ARRAY_LEN( fixtures ) );
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
		R_LOG( rch_ral, SEV_INFO, "    #%2u %-44s : %s%s%s\n",
		        i + 1, fixtures[i].name, status,
		        fixtures[i].note ? "  — " : "",
		        fixtures[i].note ? fixtures[i].note : "" );
	}

	// Additional cluster pipelines (umbrella'd into fixtures #14 / #15).
	R_LOG( rch_ral, SEV_INFO, "  Sub-fixture (cluster expansion):\n" );
	#define DUMP_FIELD(label, ptr) do { \
		if ( (ptr) ) { R_LOG( rch_ral, SEV_INFO, "    %-44s : %s\n", (label), "PASS" ); pass++; } \
		else         { R_LOG( rch_ral, SEV_INFO, "    %-44s : %s\n", (label), "miss" );  } \
	} while (0)
	DUMP_FIELD( "smaa_blend (cluster of #15)",       vk.ral_smaa_blend_pipeline );
	DUMP_FIELD( "smaa_resolve (cluster of #15)",     vk.ral_smaa_resolve_pipeline );
	DUMP_FIELD( "gamma (post-process)",              vk.ral_gamma_pipeline );
	DUMP_FIELD( "capture (post-process)",            vk.ral_capture_pipeline );
	DUMP_FIELD( "bloom_extract (post-process)",      vk.ral_bloom_extract_pipeline );
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
		R_LOG( rch_ral, SEV_INFO, "  Centralized helper: %u / %u Vk_Pipeline_Def variants have at least one sibling ralPipeline_t (total RAL pipelines = %u across %u render passes)\n",
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
			R_LOG( rch_ral, SEV_INFO, "  (%u centralized-helper-covered fixtures promoted to PASS via the non-zero sibling count above)\n", cent_covered_na );
		}
	}

	R_LOG( rch_ral, SEV_INFO, "  Summary: %u PASS, %u FAIL, %u N/A across 19 fixtures + cluster expansion.\n",
	        pass, fail, na );
	R_LOG( rch_ral, SEV_INFO, "===== end \\ral_pipeline_test =====\n" );
}
#endif  /* retired Ral_PipelineTest body */

// ── \ral_resources developer dump (textures + buffers) ─────────────────
void vk_ral_textures_diag_dump( void ) {
	uint32_t i;
	R_LOG( rch_ral, SEV_INFO, "===== \\ral_resources =====\n" );
	if ( !s_ral_init_attempted ) {
		R_LOG( rch_ral, SEV_INFO, "  RAL infra not yet initialized — vid_restart to apply\n" );
		R_LOG( rch_ral, SEV_INFO, "===== end \\ral_resources =====\n" );
		return;
	}
	if ( !s_ral_backend ) {
		R_LOG( rch_ral, SEV_WARN, "  RAL backend creation FAILED this session — see earlier [RAL-TEX] / [RAL-BUF] warnings\n" );
		R_LOG( rch_ral, SEV_INFO, "===== end \\ral_resources =====\n" );
		return;
	}
	R_LOG( rch_ral, SEV_INFO, "  TEXTURES (Phase 7.4a parallel-paths):\n" );
	if ( !vk_ral_textures_available() ) {
		R_LOG( rch_ral, SEV_INFO, "    bindless table not built this session; RAL backend up for buffer registrations only.\n" );
	} else {
		R_LOG( rch_ral, SEV_INFO, "    bindless slots capacity : %u\n", s_ral_bindless_capacity );
		R_LOG( rch_ral, SEV_INFO, "    registered (in bindless): %u\n", s_ral_registered_count );
		R_LOG( rch_ral, SEV_INFO, "    destroyed (lifetime sum): %u\n", s_ral_destroyed_count );
		R_LOG( rch_ral, SEV_INFO, "    skipped (no pic data)   : %u   (scratch/placeholder images don't register)\n", s_ral_skipped_no_data );
		R_LOG( rch_ral, SEV_INFO, "    skipped (slot overflow) : %u   (kept RAL texture, omitted from BindGroup)\n", s_ral_skipped_no_slot );
		R_LOG( rch_ral, SEV_INFO, "    tr.numImages            : %u\n", (unsigned)tr.numImages );
		R_LOG( rch_ral, SEV_INFO, "    last %u registered names:\n", VK_RAL_RECENT_NAMES );
		for ( i = 0; i < VK_RAL_RECENT_NAMES; i++ ) {
			uint32_t idx = ( s_ral_recent_head + VK_RAL_RECENT_NAMES - 1 - i ) % VK_RAL_RECENT_NAMES;
			if ( s_ral_recent_names[idx][0] )
				R_LOG( rch_ral, SEV_INFO, "      [-%u] %s\n", i + 1, s_ral_recent_names[idx] );
		}
	}
	R_LOG( rch_ral, SEV_INFO, "  BUFFERS (Phase 7.4b parallel-paths):\n" );
	R_LOG( rch_ral, SEV_INFO, "    live RAL buffers        : %u    (peak %u this session)\n", s_buf_active_count, s_buf_peak_count );
	R_LOG( rch_ral, SEV_INFO, "    pending (queued, unflush): %u\n", s_buf_pending_count );
	R_LOG( rch_ral, SEV_INFO, "    register total (incl.)  : %u\n", s_buf_register_total );
	R_LOG( rch_ral, SEV_INFO, "    unregister total        : %u\n", s_buf_destroy_total );
	R_LOG( rch_ral, SEV_INFO, "    skipped (queue full)    : %u\n", s_buf_skipped_no_backend );
	R_LOG( rch_ral, SEV_INFO, "    bytes by usage (live live buffers, MiB; bits sum if a buffer has multi-usage):\n" );
	{
		const char *names[7] = { "VERTEX", "INDEX", "UNIFORM", "STORAGE", "INDIRECT", "TRANSFER_SRC", "TRANSFER_DST" };
		for ( i = 0; i < 7; i++ ) {
			R_LOG( rch_ral, SEV_INFO, "      %-13s : %5u MiB (%llu bytes)\n",
			        names[i],
			        (unsigned)( s_buf_bytes_by_usage[i] >> 20 ),
			        (unsigned long long)s_buf_bytes_by_usage[i] );
		}
	}
	R_LOG( rch_ral, SEV_INFO, "===== end \\ral_resources =====\n" );
}

#endif // USE_VULKAN
