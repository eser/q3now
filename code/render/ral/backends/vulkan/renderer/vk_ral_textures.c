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
// Asset images and BSP lightmap asset chunks are migrated here. Dynamic glyphs,
// scratch images and render-target attachments retain their dedicated owners.

#include "tr_local.h"
#include "../../../../frontend/r_log.h"  // rilog-channel-mechanism — renderer.ral

R_LOG_DECLARE_CHANNEL( rch_ral,         "renderer.ral"         );
R_LOG_DECLARE_CHANNEL( rch_ral_texture, "renderer.ral.texture" );

#ifdef USE_VULKAN

#include "vk_ral_textures.h"

// The RAL public surface + the Vulkan-backend-internal accessors (the
// latter only because \ral_textures dumps the underlying VkImage for
// diagnostics; the renderer never USES the RAL VkImage beyond
// dumping it). ral/ral.h is the right include for everything else.
#include "../../../core/ral.h"
#include "../../../core/frame_graph/ral_frame_graph_native.h"

// ── module state ────────────────────────────────────────────────────────
static ralBackend_t         *s_ral_backend;
static uint64_t              s_ral_frame_graph_diagnostic_generation;
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
#define VK_RAL_ASSET_CHUNK_PAGES_PER_FRAME 8u
#define VK_RAL_ASSET_CHUNK_BYTES_PER_FRAME (16u * 1024u * 1024u)
static vk_ral_pending_upload_t s_ral_pending_uploads[ VK_RAL_MAX_PENDING_UPLOADS ];
static uint32_t                s_ral_pending_upload_count;
static uint32_t                s_ral_pending_peak;
typedef struct {
	image_t  *image;
	byte     *data;
	uint64_t  dataSize;
	uint32_t  requestFrame;
} vk_ral_asset_chunk_request_t;
static vk_ral_asset_chunk_request_t s_ral_asset_chunk_requests[ VK_RAL_MAX_PENDING_UPLOADS ];
static ralResidencyCandidate_t s_ral_asset_chunk_candidates[ VK_RAL_MAX_PENDING_UPLOADS ];
static size_t                   s_ral_asset_chunk_selected[ VK_RAL_MAX_PENDING_UPLOADS ];
static uint8_t                  s_ral_asset_chunk_scratch[ VK_RAL_MAX_PENDING_UPLOADS ];
static uint8_t                  s_ral_asset_chunk_admit[ VK_RAL_MAX_PENDING_UPLOADS ];
static uint32_t                 s_ral_asset_chunk_request_count;
static uint32_t                 s_ral_asset_chunk_request_peak;
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

static void                 vk_ral_destroy_adopted_pipeline_layouts( void );  // fwd
void                        vk_ral_adopt_static_pipeline_layouts( void );      // fwd
// internal-texture adoption (depth, color, tonemapped,
// SMAA input/edges/blend). GPU timestamp queries are native RAL resources
// owned directly by vk_gpu_ts_init/vk_gpu_ts_shutdown in vk.c.

// boot-time adoption of every allocate-once
// VkDescriptorSet wrapped into a ralBindGroup_t with ownsSet=qfalse so the
// renderer's existing arena-reset / descriptor-owner teardown
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
	if ( !s_ral_bindless_set || !image || !view ) return qfalse;
	nativeView = Ral_GetTextureViewHandle( view );
	if ( !nativeView ) return qfalse;
	if ( kind == VK_BINDLESS_PUBLICATION_LEGACY_EXACT
			&& ( !image->descriptor
				|| image->ralDescriptorView != view ) ) return qfalse;
	if ( !Ral_BindGroupSetTextureViewAt( s_ral_bindless_set, slot, view ) )
		return qfalse;
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
	if ( !Ral_BindGroupSetTextureAt( s_ral_bindless_set, slot, texture ) )
		return qfalse;
	return vk_ral_bindless_record_views( images, &slot, &nativeView, &kind, 1 );
}

qboolean vk_ral_bindless_publish_sampler( uint32_t slot, ralSampler_t *sampler,
		const Vk_Sampler_Def *definition ) {
	const void *identity;
	uint64_t digest;
	if ( !sampler || !definition || !s_ral_backend || !s_ral_bindless_set ) return qfalse;
	identity = Ral_GetSamplerHandle( sampler );
	if ( !identity ) return qfalse;
	digest = vk_ral_bindless_sampler_digest( definition );
	if ( !Ral_BindGroupSetSamplerAt( s_ral_bindless_set, slot, sampler ) )
		return qfalse;
	if ( !vk_ral_bindless_ledger_activate()
			|| !VK_BindlessPublicationSamplers( &s_bindless_publication,
				s_ral_bindless_set, &vk.samplers, &slot, &identity, &digest, 1 ) ) {
		vk_ral_bindless_poison_active();
		return qfalse;
	}
	return qtrue;
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
	ralTextureView_t *fallback;
	if ( !vk_ral_bindless_ledger_activate() ) return qfalse;
	// PARTIALLY_BOUND does not make a descriptor that still names a destroyed
	// view safe.  Eviction therefore replaces the physical descriptor with the
	// pinned white image before the publication ledger forgets its old owner.
	// Bulk level teardown rebuilds the complete set after all image_t owners are
	// gone, so the fallback may itself be among that teardown's later victims.
	fallback = tr.whiteImage ? tr.whiteImage->ralDescriptorView : NULL;
	if ( !fallback || !Ral_GetTextureViewHandle( fallback )
			|| !Ral_BindGroupSetTextureViewAt( s_ral_bindless_set, slot,
				fallback ) )
		return qfalse;
	if ( VK_BindlessPublicationTombstoneImage(
			&s_bindless_publication, s_ral_bindless_set, slot ) ) return qtrue;
	(void)VK_BindlessPublicationPoisonSetAfterWrite(
		&s_bindless_publication, s_ral_bindless_set );
	return qfalse;
}

qboolean vk_ral_bindless_query_ordinary( const image_t *image,
		vkBindlessOrdinaryReceipt_t *outReceipt ) {
	const void *nativeView;
	int samplerSlot;
	if ( !image || !outReceipt || image->ralBindlessSlot < 0
			|| image->bindlessSamplerSlot < 0 || !image->bindlessOwnerGeneration
			|| !image->ralDescriptorView || image->descriptor == VK_NULL_HANDLE )
		return qfalse;
	nativeView = Ral_GetTextureViewHandle( image->ralDescriptorView );
	if ( !nativeView ) return qfalse;
	samplerSlot = image->bindlessSamplerSlot;
	if ( samplerSlot >= vk.samplers.count ) return qfalse;
	return VK_BindlessPublicationQueryOrdinary( &s_bindless_publication,
		s_ral_bindless_set, &vk.samplers, (uint32_t)image->ralBindlessSlot,
		nativeView, image, (const void *)image->descriptor,
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
// vk_fse_ext_enabled / vk.dedicatedAllocation) gets
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
	     ? ri.VK_GetInstanceProcAddr( nativeInstance, name ) : NULL;
}

static qboolean vk_ral_host_create_surface( void *userData, void *platformHandle,
	                                         void *nativeInstance,
	                                         uint64_t *outNativeSurface ) {
	(void)userData;
	// The engine adapter intentionally preserves the existing main-window
	// callback.  A standalone SDL host supplies its own callback and consumes
	// platformHandle directly; RAL itself never reaches the engine-global window.
	(void)platformHandle;
	if ( outNativeSurface ) *outNativeSurface = 0;
	if ( !outNativeSurface || !ri.VK_CreateSurface
	  || !ri.VK_CreateSurface( nativeInstance, outNativeSurface )
	  || *outNativeSurface == 0 ) return qfalse;
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
// vk_destroy_sync_primitives' Ral_Destroy{Semaphore,Fence} calls and direct
// RAL resource teardown, but BEFORE
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

	// Bindless texture table is part of unconditional RAL renderer bringup.
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

}


// Rebuild every named renderer bind-group owner after the descriptor arena and
// its current generation are available. Idempotent: subsequent calls
// (vid_restart, REF_LEVEL_ONLY-then-re-init) clear the registry first, so every
// owner and compatibility mirror belongs to the current arena cohort.
//
// Remaining adopted compatibility wrappers carry ownsSet=qfalse; teardown only
// frees the wrapper struct, while direct RAL groups own their arena allocation.
//
// The main rotating set cohort is also retained here through its named owners:
// set0 tess uniform, set2 engine resources and set3 MSDF/entMat. Set1 remains
// the central bindless owner in this TU.
static void vk_ral_destroy_smaa_bindgroups( void )
{
	uint32_t i;
	#define DESTROY_SMAA_BG( field ) do { \
		if ( (field) ) { Ral_DestroyBindGroup( (field) ); (field) = NULL; } \
	} while ( 0 )
	vk_ral_release_smaa_sampler_cohorts();
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		DESTROY_SMAA_BG( vk.smaaRt.ral_descriptor[i] );
	}
	#undef DESTROY_SMAA_BG
}

static qboolean vk_ral_smaa_bindgroups_ready( void )
{
	uint32_t i;
	if ( vk.smaa.active
			&& ( !vk.smaa.ral_edges_image || !vk.smaa.ral_edges_view
				|| !vk.smaa.ral_edges_descriptor
				|| !vk.smaa.ral_blend_image || !vk.smaa.ral_blend_view
				|| !vk.smaa.ral_blend_descriptor
				|| !vk.smaa.ral_input_image || !vk.smaa.ral_input_view
				|| !vk.smaa.ral_input_descriptor
				|| !vk.smaa.ral_area_image || !vk.smaa.ral_area_view
				|| !vk.smaa.ral_area_descriptor
				|| !vk.smaa.ral_search_image || !vk.smaa.ral_search_view
				|| !vk.smaa.ral_search_descriptor
				|| Ral_GetBindGroupHandle( vk.smaa.ral_edges_descriptor )
					!= (void *)vk.smaa.edges_descriptor
				|| Ral_GetBindGroupHandle( vk.smaa.ral_blend_descriptor )
					!= (void *)vk.smaa.blend_descriptor
				|| Ral_GetBindGroupHandle( vk.smaa.ral_input_descriptor )
					!= (void *)vk.smaa.input_descriptor
				|| Ral_GetBindGroupHandle( vk.smaa.ral_area_descriptor )
					!= (void *)vk.smaa.area_descriptor
				|| Ral_GetBindGroupHandle( vk.smaa.ral_search_descriptor )
					!= (void *)vk.smaa.search_descriptor ) ) return qfalse;
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
		if ( !vk.smaaRt.ral_descriptor[i] || !vk.smaaRt.ral_buffer[i]
		  || Ral_GetBindGroupHandle( vk.smaaRt.ral_descriptor[i] )
			!= (void *)vk.smaaRt.descriptor[i] ) return qfalse;
	return qtrue;
}

void vk_ral_release_tess_uniform_bindgroup( uint32_t slot )
{
	if ( slot >= ARRAY_LEN( vk.tess ) ) return;
	if ( vk.tess[slot].ral_uniform_descriptor ) {
		Ral_DestroyBindGroup( vk.tess[slot].ral_uniform_descriptor );
		vk.tess[slot].ral_uniform_descriptor = NULL;
	}
	vk.tess[slot].uniform_descriptor = VK_NULL_HANDLE;
}

qboolean vk_ral_refresh_tess_uniform_bindgroup( uint32_t slot )
{
	ralBindingValue_t value;
	ralBindGroupCreateInfo_t createInfo;
	if ( slot >= ARRAY_LEN( vk.tess ) ) return qfalse;
	if ( !s_ral_backend || !vk.ral_bgl_uniform
			|| !vk.ral_descriptor_arena
			|| !vk.tess[slot].ral_vertex_buffer ) return qfalse;
	if ( vk.tess[slot].ral_uniform_descriptor
	  && Ral_GetBindGroupHandle( vk.tess[slot].ral_uniform_descriptor )
		== (void *)vk.tess[slot].uniform_descriptor ) return qtrue;
	vk_ral_release_tess_uniform_bindgroup( slot );
	if ( Ral_GetBufferSize( vk.tess[slot].ral_vertex_buffer )
			< sizeof( vkUniform_t ) ) return qfalse;
	memset( &value, 0, sizeof( value ) );
	value.binding = 0u;
	value.type = RAL_BIND_UNIFORM_BUFFER;
	value.buffer = vk.tess[slot].ral_vertex_buffer;
	value.bufferRange = sizeof( vkUniform_t );
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.layout = vk.ral_bgl_uniform;
	createInfo.values = &value;
	createInfo.numValues = 1u;
	createInfo.debugName = "wired-tess-uniform-bg";
	createInfo.arena = vk.ral_descriptor_arena;
	createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;
	vk.tess[slot].ral_uniform_descriptor = Ral_CreateBindGroup(
		s_ral_backend, &createInfo );
	if ( !vk.tess[slot].ral_uniform_descriptor ) return qfalse;
	vk.tess[slot].uniform_descriptor = (VkDescriptorSet)Ral_GetBindGroupHandle(
		vk.tess[slot].ral_uniform_descriptor );
	return qtrue;
}

void vk_ral_release_iqm_bone_bindgroup( uint32_t slot )
{
#if FEAT_IQM
	if ( slot >= ARRAY_LEN( vk.iqmGpu.ral_bone_descriptor ) ) return;
	if ( vk.iqmGpu.ral_bone_descriptor[slot] ) {
		Ral_DestroyBindGroup( vk.iqmGpu.ral_bone_descriptor[slot] );
		vk.iqmGpu.ral_bone_descriptor[slot] = NULL;
	}
	vk.iqmGpu.bone_descriptor[slot] = VK_NULL_HANDLE;
#else
	(void)slot;
#endif
}

qboolean vk_ral_refresh_iqm_bone_bindgroup( uint32_t slot )
{
#if FEAT_IQM
	ralBuffer_t *buffer;
	ralBindingValue_t value;
	ralBindGroupCreateInfo_t createInfo;
	const uint64_t item = PAD( (uint32_t)IQM_UBO_TOTAL_SIZE,
		vk.uniform_alignment );
	if ( slot >= ARRAY_LEN( vk.iqmGpu.ral_bone_descriptor ) ) return qfalse;
	if ( !s_ral_backend || !vk.iqmGpu.ral_bgl_bones
			|| !vk.ral_descriptor_arena
			|| !vk.iqmGpu.ral_bone_buffer[slot]
			|| vk.iqmGpu.ring_size == 0u ) return qfalse;
	if ( vk.iqmGpu.ral_bone_descriptor[slot]
	  && Ral_GetBindGroupHandle( vk.iqmGpu.ral_bone_descriptor[slot] )
		== (void *)vk.iqmGpu.bone_descriptor[slot] ) return qtrue;
	vk_ral_release_iqm_bone_bindgroup( slot );
	buffer = vk.iqmGpu.ral_bone_buffer[slot];
	if ( Ral_GetBufferSize( buffer ) != vk.iqmGpu.ring_size
	  || item > vk.iqmGpu.ring_size ) return qfalse;
	memset( &value, 0, sizeof( value ) );
	value.binding = 0u;
	value.type = RAL_BIND_UNIFORM_BUFFER;
	value.buffer = buffer;
	value.bufferRange = item;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.layout = vk.iqmGpu.ral_bgl_bones;
	createInfo.values = &value;
	createInfo.numValues = 1u;
	createInfo.debugName = "wired-iqm-bones-bg";
	createInfo.arena = vk.ral_descriptor_arena;
	createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;
	vk.iqmGpu.ral_bone_descriptor[slot] = Ral_CreateBindGroup(
		s_ral_backend, &createInfo );
	if ( !vk.iqmGpu.ral_bone_descriptor[slot] ) return qfalse;
	vk.iqmGpu.bone_descriptor[slot] = (VkDescriptorSet)Ral_GetBindGroupHandle(
		vk.iqmGpu.ral_bone_descriptor[slot] );
	return qtrue;
#else
	(void)slot;
	return qfalse;
#endif
}

void vk_ral_release_entmat_bindgroup( uint32_t slot )
{
	if ( slot >= ARRAY_LEN( vk.tess ) ) return;
	if ( vk.tess[slot].ral_entMatDesc ) {
		Ral_DestroyBindGroup( vk.tess[slot].ral_entMatDesc );
		vk.tess[slot].ral_entMatDesc = NULL;
	}
	vk.tess[slot].entMatDesc = VK_NULL_HANDLE;
}

ralBindGroup_t *vk_ral_create_entmat_bindgroup_candidate(
		uint32_t slot, ralBuffer_t *buffer, uint64_t bufferSize,
		VkDescriptorSet *outNative )
{
	ralBindingValue_t value;
	ralBindGroupCreateInfo_t createInfo;
	ralBindGroup_t *candidate;
	VkDescriptorSet rawCandidate;
	uint32_t i;
	if ( outNative ) *outNative = VK_NULL_HANDLE;
	if ( slot >= ARRAY_LEN( vk.tess ) || !outNative ) return NULL;
	if ( !s_ral_backend || !vk.ral_bgl_entmat
			|| !vk.ral_descriptor_arena
			|| !Ral_BindGroupArenaReceiptValid(
				&vk.ral_descriptor_arena_receipt )
			|| vk.ral_descriptor_arena_receipt.backendIdentity
				!= s_ral_backend
			|| vk.ral_descriptor_arena_receipt.arenaIdentity
				!= vk.ral_descriptor_arena
			|| !buffer || !Ral_GetBufferHandle( buffer )
			|| !bufferSize || Ral_GetBufferSize( buffer ) != bufferSize ) return NULL;
	memset( &value, 0, sizeof( value ) );
	value.binding = 0u;
	value.type = RAL_BIND_STORAGE_BUFFER;
	value.buffer = buffer;
	value.bufferRange = bufferSize;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.layout = vk.ral_bgl_entmat;
	createInfo.values = &value;
	createInfo.numValues = 1u;
	createInfo.debugName = "wired-entmat-bg";
	createInfo.arena = vk.ral_descriptor_arena;
	createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;
	candidate = Ral_CreateBindGroup( s_ral_backend, &createInfo );
	if ( !candidate ) return NULL;
	rawCandidate = (VkDescriptorSet)Ral_GetBindGroupHandle( candidate );
	if ( rawCandidate == VK_NULL_HANDLE
			|| candidate == vk.tess[slot].ral_entMatDesc
			|| rawCandidate == vk.tess[slot].entMatDesc ) goto fail;
	for ( i = 0u; i < ARRAY_LEN( vk.tess ); ++i ) {
		if ( i == slot ) continue;
		if ( candidate == vk.tess[i].ral_entMatDesc
				|| rawCandidate == vk.tess[i].entMatDesc ) goto fail;
	}
	*outNative = rawCandidate;
	return candidate;

fail:
	Ral_DestroyBindGroup( candidate );
	return NULL;
}

qboolean vk_ral_refresh_entmat_bindgroup( uint32_t slot )
{
	ralBindGroup_t *candidate;
	ralBindGroup_t *retired;
	VkDescriptorSet rawCandidate;
	if ( slot >= ARRAY_LEN( vk.tess ) || !vk.tess[slot].ral_entMatBuf
			|| !vk.tess[slot].entMatSize ) return qfalse;
	if ( vk.tess[slot].ral_entMatDesc
	  && Ral_GetBindGroupHandle( vk.tess[slot].ral_entMatDesc )
		== (void *)vk.tess[slot].entMatDesc ) return qtrue;
	candidate = vk_ral_create_entmat_bindgroup_candidate( slot,
		vk.tess[slot].ral_entMatBuf, vk.tess[slot].entMatSize,
		&rawCandidate );
	if ( !candidate ) return qfalse;
	retired = vk.tess[slot].ral_entMatDesc;
	vk.tess[slot].ral_entMatDesc = candidate;
	vk.tess[slot].entMatDesc = rawCandidate;
	if ( retired ) Ral_DestroyBindGroup( retired );
	return qtrue;
}

void vk_ral_release_engine_resources_bindgroup( void )
{
	if ( vk.engineResources.ral_descriptor )
		Ral_DestroyBindGroup( vk.engineResources.ral_descriptor );
	if ( vk.engineResources.ral_shadow_view )
		Ral_DestroyTextureView( vk.engineResources.ral_shadow_view );
	vk.engineResources.ral_descriptor = NULL;
	vk.engineResources.ral_shadow_view = NULL;
	vk.engineResources.descriptor = VK_NULL_HANDLE;
}

qboolean vk_ral_refresh_engine_resources_bindgroup( void )
{
	ralBindingValue_t values[5];
	ralBindGroupCreateInfo_t createInfo;
	ralTextureView_t *shadowViewCandidate = NULL, *shadowViewRetired;
	ralBindGroup_t *groupCandidate = NULL, *groupRetired;
	VkDescriptorSet rawCandidate;
	qboolean shadowViewOwned = qtrue, groupOwned = qtrue;
	uint32_t valueCount = 0u, i, j;

	if ( !s_ral_backend || !vk.ral_bgl_engine_resources
			|| !vk.ral_descriptor_arena
			|| !Ral_BindGroupArenaReceiptValid(
				&vk.ral_descriptor_arena_receipt )
			|| vk.ral_descriptor_arena_receipt.backendIdentity
				!= s_ral_backend
			|| vk.ral_descriptor_arena_receipt.arenaIdentity
				!= vk.ral_descriptor_arena ) return qfalse;
	memset( values, 0, sizeof( values ) );

#define ADD_ENGINE_COMBINED(binding_, view_, sampler_) do { \
	const ralTextureView_t *addView_ = (view_); \
	const ralSampler_t *addSampler_ = (sampler_); \
	if ( !addView_ || !addSampler_ \
			|| !Ral_GetTextureViewHandle( addView_ ) \
			|| !Ral_GetSamplerHandle( addSampler_ ) \
			|| valueCount >= ARRAY_LEN( values ) ) goto fail; \
	values[valueCount].binding = (binding_); \
	values[valueCount].type = RAL_BIND_COMBINED_TEXTURE_SAMPLER; \
	values[valueCount].textureView = addView_; \
	values[valueCount].sampler = addSampler_; \
	++valueCount; \
} while ( 0 )

#if FEAT_SHADOW_MAPPING
	if ( vk.shadowMap.active || vk.shadowMap.ral_image
			|| vk.shadowMap.ral_sampler ) {
		ralTextureViewCreateInfo_t viewInfo;
		if ( !vk.shadowMap.active || !vk.shadowMap.ral_image
				|| !vk.shadowMap.ral_sampler ) goto fail;
		memset( &viewInfo, 0, sizeof( viewInfo ) );
		viewInfo.texture = vk.shadowMap.ral_image;
		viewInfo.viewType = RAL_TEXTURE_2D_ARRAY;
		viewInfo.format = RAL_FORMAT_UNDEFINED;
		viewInfo.aspect = RAL_TEXTURE_VIEW_ASPECT_DEPTH_ONLY;
		shadowViewCandidate = Ral_CreateTextureView( s_ral_backend, &viewInfo );
		if ( !shadowViewCandidate ) goto fail;
		ADD_ENGINE_COMBINED( WIRED_ENGINE_RES_BIND_SHADOWMAP,
			shadowViewCandidate, vk.shadowMap.ral_sampler );
	}
#endif

	if ( vk.ral_brdf_lut_view || vk.ral_probe_irradiance_cube_view
			|| vk.ral_probe_radiance_cube_view ) {
		if ( !vk.ral_ibl_sampler ) goto fail;
	}
	if ( vk.ral_brdf_lut_view ) {
		ADD_ENGINE_COMBINED( WIRED_ENGINE_RES_BIND_BRDF_LUT,
			vk.ral_brdf_lut_view, vk.ral_ibl_sampler );
	}
	if ( vk.ral_probe_irradiance_cube_view ) {
		ADD_ENGINE_COMBINED( WIRED_ENGINE_RES_BIND_IRRADIANCE,
			vk.ral_probe_irradiance_cube_view, vk.ral_ibl_sampler );
	}
	if ( vk.ral_probe_radiance_cube_view ) {
		ADD_ENGINE_COMBINED( WIRED_ENGINE_RES_BIND_RADIANCE,
			vk.ral_probe_radiance_cube_view, vk.ral_ibl_sampler );
	}

	if ( vk.ral_gtao_denoised_view || vk.ral_gtao_sampler ) {
		if ( !vk.ral_gtao_denoised_view || !vk.ral_gtao_sampler ) goto fail;
		ADD_ENGINE_COMBINED( WIRED_ENGINE_RES_BIND_GTAO,
			vk.ral_gtao_denoised_view, vk.ral_gtao_sampler );
	} else if ( tr.whiteImage ) {
		if ( !tr.whiteImage->ralDescriptorView
				|| !tr.whiteImage->ralDescriptorSampler ) goto fail;
		ADD_ENGINE_COMBINED( WIRED_ENGINE_RES_BIND_GTAO,
			tr.whiteImage->ralDescriptorView,
			tr.whiteImage->ralDescriptorSampler );
	}
#undef ADD_ENGINE_COMBINED

	for ( i = 0u; i < valueCount; ++i )
		for ( j = 0u; j < i; ++j )
			if ( values[i].textureView == values[j].textureView
					|| Ral_GetTextureViewHandle( values[i].textureView )
						== Ral_GetTextureViewHandle(
							values[j].textureView ) ) goto fail;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.layout = vk.ral_bgl_engine_resources;
	createInfo.values = values;
	createInfo.numValues = valueCount;
	createInfo.debugName = "wired-engine-resources-bg";
	createInfo.arena = vk.ral_descriptor_arena;
	createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;
	groupCandidate = Ral_CreateBindGroup( s_ral_backend, &createInfo );
	if ( !groupCandidate ) goto fail;
	rawCandidate = (VkDescriptorSet)Ral_GetBindGroupHandle( groupCandidate );
	if ( rawCandidate == VK_NULL_HANDLE ) goto fail;
	if ( groupCandidate == vk.engineResources.ral_descriptor ) {
		groupOwned = qfalse;
		goto fail;
	}
	if ( shadowViewCandidate == vk.engineResources.ral_shadow_view
			&& shadowViewCandidate ) {
		shadowViewOwned = qfalse;
		goto fail;
	}
	if ( rawCandidate == vk.engineResources.descriptor ) goto fail;

	groupRetired = vk.engineResources.ral_descriptor;
	shadowViewRetired = vk.engineResources.ral_shadow_view;
	vk.engineResources.ral_descriptor = groupCandidate;
	vk.engineResources.ral_shadow_view = shadowViewCandidate;
	vk.engineResources.descriptor = rawCandidate;
	if ( groupRetired ) Ral_DestroyBindGroup( groupRetired );
	if ( shadowViewRetired ) Ral_DestroyTextureView( shadowViewRetired );
	return qtrue;

fail:
	if ( groupCandidate && groupOwned ) Ral_DestroyBindGroup( groupCandidate );
	if ( shadowViewCandidate && shadowViewOwned )
		Ral_DestroyTextureView( shadowViewCandidate );
	return qfalse;
}

void vk_ral_release_sprite_bindgroup( uint32_t slot )
{
	if ( slot >= ARRAY_LEN( vk.sprite.ral_descriptor ) ) return;
	if ( vk.sprite.ral_descriptor[slot] ) {
		Ral_DestroyBindGroup( vk.sprite.ral_descriptor[slot] );
		vk.sprite.ral_descriptor[slot] = NULL;
	}
	vk.sprite.descriptor[slot] = VK_NULL_HANDLE;
}

qboolean vk_ral_refresh_sprite_bindgroup( uint32_t slot )
{
	ralBuffer_t *buffer;
	ralBindingValue_t value;
	ralBindGroupCreateInfo_t createInfo;
	const uint64_t bytes = (uint64_t)SPRITES_PER_FRAME * SPRITE_HEADER_BYTES;
	if ( slot >= ARRAY_LEN( vk.sprite.ral_descriptor ) ) return qfalse;
	if ( !s_ral_backend || !vk.sprite.ral_bgl || !vk.ral_descriptor_arena
			|| !vk.sprite.ral_headers_buffer[slot] ) return qfalse;
	if ( vk.sprite.ral_descriptor[slot]
	  && Ral_GetBindGroupHandle( vk.sprite.ral_descriptor[slot] )
		== (void *)vk.sprite.descriptor[slot] ) return qtrue;
	vk_ral_release_sprite_bindgroup( slot );
	buffer = vk.sprite.ral_headers_buffer[slot];
	if ( Ral_GetBufferSize( buffer ) != bytes ) return qfalse;
	memset( &value, 0, sizeof( value ) );
	value.binding = 0u;
	value.type = RAL_BIND_STORAGE_BUFFER;
	value.buffer = buffer;
	value.bufferRange = bytes;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.layout = vk.sprite.ral_bgl;
	createInfo.values = &value;
	createInfo.numValues = 1u;
	createInfo.debugName = "wired-sprite-bg";
	createInfo.arena = vk.ral_descriptor_arena;
	createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;
	vk.sprite.ral_descriptor[slot] = Ral_CreateBindGroup(
		s_ral_backend, &createInfo );
	if ( !vk.sprite.ral_descriptor[slot] ) return qfalse;
	vk.sprite.descriptor[slot] = (VkDescriptorSet)Ral_GetBindGroupHandle(
		vk.sprite.ral_descriptor[slot] );
	return vk.sprite.descriptor[slot] != VK_NULL_HANDLE;
}

static ralSampler_t *vk_ral_lookup_sampler( void *nativeSampler )
{
	int i;
	if ( !nativeSampler ) return NULL;
	for ( i = 0; i < vk.samplers.count; ++i ) {
		if ( (void *)vk.samplers.handle[i] == nativeSampler
				&& vk.samplers.ral_handle[i]
		  && Ral_GetSamplerHandle( vk.samplers.ral_handle[i] )
			== nativeSampler ) return vk.samplers.ral_handle[i];
	}
	return NULL;
}

void vk_ral_release_primitive_bindgroups( void )
{
	uint32_t i;
	for ( i = 0; i < NUM_COMMAND_BUFFERS; ++i ) {
		if ( vk.ribbon.ral_descriptor[i] )
			Ral_DestroyBindGroup( vk.ribbon.ral_descriptor[i] );
		if ( vk.railRibbon.ral_descriptor[i] )
			Ral_DestroyBindGroup( vk.railRibbon.ral_descriptor[i] );
		if ( vk.beam.ral_descriptor[i] )
			Ral_DestroyBindGroup( vk.beam.ral_descriptor[i] );
		vk.ribbon.ral_descriptor[i] = NULL;
		vk.railRibbon.ral_descriptor[i] = NULL;
		vk.beam.ral_descriptor[i] = NULL;
		vk.ribbon.descriptor[i] = VK_NULL_HANDLE;
		vk.railRibbon.descriptor[i] = VK_NULL_HANDLE;
		vk.beam.descriptor[i] = VK_NULL_HANDLE;
	}
}

qboolean vk_ral_refresh_primitive_bindgroups( void )
{
	ralBindGroup_t *ribbon[NUM_COMMAND_BUFFERS] = { NULL };
	ralBindGroup_t *rail[NUM_COMMAND_BUFFERS] = { NULL };
	ralBindGroup_t *beam[NUM_COMMAND_BUFFERS] = { NULL };
	ralTextureView_t *views[PRIMITIVE_SHADER_IMAGE_MAX];
	ralSampler_t *clampSampler, *repeatSampler;
	ralBuffer_t *stageBuffer, *stageCountBuffer;
	void *nativeGroups[NUM_COMMAND_BUFFERS * 3];
	uint32_t nativeCount = 0u, i, j;
	const uint64_t ribbonPointsBytes =
		(uint64_t)RIBBON_POINTS_PER_FRAME * RIBBON_POINT_BYTES;
	const uint64_t ribbonHeadersBytes =
		(uint64_t)RIBBON_HEADERS_PER_FRAME * RIBBON_HEADER_BYTES;
	const uint64_t railHeadersBytes =
		(uint64_t)RAIL_RIBBON_POOL_MAX * RAIL_RIBBON_HEADER_BYTES;
	const uint64_t beamHeadersBytes =
		(uint64_t)BEAM_POOL_MAX * BEAM_HEADER_BYTES;
	const uint64_t stageBytes = (uint64_t)PRIMITIVE_SHADER_IMAGE_MAX
		* PRIMITIVE_STAGE_MAX * VK_PRIMITIVE_STAGE_BYTES;
	const uint64_t stageCountBytes =
		(uint64_t)PRIMITIVE_SHADER_IMAGE_MAX * sizeof( uint32_t );

	if ( !s_ral_backend || !vk.ral_descriptor_arena
			|| !Ral_BindGroupArenaReceiptExact( &vk.ral_descriptor_arena_receipt,
				&vk.ral_descriptor_arena_receipt ) ) return qfalse;
	for ( i = 0; i < PRIMITIVE_SHADER_IMAGE_MAX; ++i ) {
		image_t *image = vk_primitive_shader_images[i];
		if ( !image || !image->ralResidencyView
		  || !Ral_GetTextureViewHandle( image->ralResidencyView ) ) return qfalse;
		views[i] = image->ralResidencyView;
	}
	clampSampler = vk_ral_lookup_sampler( vk.particle.sampler );
	repeatSampler = vk.beam.ral_sampler_repeat;
	if ( !clampSampler || !repeatSampler
	  || Ral_GetSamplerHandle( repeatSampler ) != (void *)vk.beam.sampler_repeat )
		return qfalse;
	stageBuffer = vk.ral_primitive_stages_buffer;
	stageCountBuffer = vk.ral_primitive_stage_counts_buffer;
	if ( vk.beam.available
	  && ( !stageBuffer || !stageCountBuffer
		|| Ral_GetBufferSize( stageBuffer ) != stageBytes
		|| Ral_GetBufferSize( stageCountBuffer ) != stageCountBytes ) ) return qfalse;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; ++i ) {
		ralBindingValue_t values[5];
		ralBindGroupCreateInfo_t createInfo;
		uint32_t count;
		memset( &createInfo, 0, sizeof( createInfo ) );
		createInfo.arena = vk.ral_descriptor_arena;
		createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;

		if ( vk.ribbon.available ) {
			ralBuffer_t *points = vk.ribbon.ral_points_buffer[i];
			ralBuffer_t *headers = vk.ribbon.ral_headers_buffer[i];
			if ( !vk.ribbon.ral_bgl || !points || !headers
			  || Ral_GetBufferSize( points ) != ribbonPointsBytes
			  || Ral_GetBufferSize( headers ) != ribbonHeadersBytes ) goto fail;
			memset( values, 0, sizeof( values ) );
			values[0] = (ralBindingValue_t){ .binding=0u, .type=RAL_BIND_STORAGE_BUFFER, .buffer=points, .bufferRange=ribbonPointsBytes };
			values[1] = (ralBindingValue_t){ .binding=1u, .type=RAL_BIND_STORAGE_BUFFER, .buffer=headers, .bufferRange=ribbonHeadersBytes };
			values[2] = (ralBindingValue_t){ .binding=2u, .type=RAL_BIND_TEXTURE_ARRAY, .textureArray=(const ralTextureView_t *const *)views, .textureArrayCount=PRIMITIVE_SHADER_IMAGE_MAX };
			values[3] = (ralBindingValue_t){ .binding=3u, .type=RAL_BIND_SAMPLER, .sampler=clampSampler };
			createInfo.layout = vk.ribbon.ral_bgl; createInfo.values = values;
			createInfo.numValues = 4u; createInfo.debugName = "wired-ribbon-bg";
			ribbon[i] = Ral_CreateBindGroup( s_ral_backend, &createInfo );
			if ( !ribbon[i] ) goto fail;
			nativeGroups[nativeCount++] = Ral_GetBindGroupHandle( ribbon[i] );
		}

		if ( vk.railRibbon.available ) {
			ralBuffer_t *headers = vk.railRibbon.ral_header_buffer[i];
			if ( !vk.railRibbon.ral_bgl || !headers
			  || Ral_GetBufferSize( headers ) != railHeadersBytes ) goto fail;
			memset( values, 0, sizeof( values ) );
			values[0] = (ralBindingValue_t){ .binding=0u, .type=RAL_BIND_STORAGE_BUFFER, .buffer=headers, .bufferRange=railHeadersBytes };
			values[1] = (ralBindingValue_t){ .binding=2u, .type=RAL_BIND_TEXTURE_ARRAY, .textureArray=(const ralTextureView_t *const *)views, .textureArrayCount=PRIMITIVE_SHADER_IMAGE_MAX };
			values[2] = (ralBindingValue_t){ .binding=3u, .type=RAL_BIND_SAMPLER, .sampler=clampSampler };
			createInfo.layout = vk.railRibbon.ral_bgl; createInfo.values = values;
			createInfo.numValues = 3u; createInfo.debugName = "wired-rail-ribbon-bg";
			rail[i] = Ral_CreateBindGroup( s_ral_backend, &createInfo );
			if ( !rail[i] ) goto fail;
			nativeGroups[nativeCount++] = Ral_GetBindGroupHandle( rail[i] );
		}

		if ( vk.beam.available ) {
			ralBuffer_t *headers = vk.beam.ral_header_buffer[i];
			if ( !vk.beam.ral_bgl || !headers
			  || Ral_GetBufferSize( headers ) != beamHeadersBytes ) goto fail;
			memset( values, 0, sizeof( values ) ); count = 0u;
			values[count++] = (ralBindingValue_t){ .binding=0u, .type=RAL_BIND_STORAGE_BUFFER, .buffer=headers, .bufferRange=beamHeadersBytes };
			values[count++] = (ralBindingValue_t){ .binding=1u, .type=RAL_BIND_TEXTURE_ARRAY, .textureArray=(const ralTextureView_t *const *)views, .textureArrayCount=PRIMITIVE_SHADER_IMAGE_MAX };
			values[count++] = (ralBindingValue_t){ .binding=2u, .type=RAL_BIND_STORAGE_BUFFER, .buffer=stageBuffer, .bufferRange=stageBytes };
			values[count++] = (ralBindingValue_t){ .binding=3u, .type=RAL_BIND_STORAGE_BUFFER, .buffer=stageCountBuffer, .bufferRange=stageCountBytes };
			values[count++] = (ralBindingValue_t){ .binding=4u, .type=RAL_BIND_SAMPLER, .sampler=repeatSampler };
			createInfo.layout = vk.beam.ral_bgl; createInfo.values = values;
			createInfo.numValues = count; createInfo.debugName = "wired-beam-bg";
			beam[i] = Ral_CreateBindGroup( s_ral_backend, &createInfo );
			if ( !beam[i] ) goto fail;
			nativeGroups[nativeCount++] = Ral_GetBindGroupHandle( beam[i] );
		}
	}
	for ( i = 0; i < nativeCount; ++i ) {
		if ( !nativeGroups[i] ) goto fail;
		for ( j = i + 1u; j < nativeCount; ++j )
			if ( nativeGroups[i] == nativeGroups[j] ) goto fail;
	}

	for ( i = 0; i < NUM_COMMAND_BUFFERS; ++i ) {
		ralBindGroup_t *oldRibbon = vk.ribbon.ral_descriptor[i];
		ralBindGroup_t *oldRail = vk.railRibbon.ral_descriptor[i];
		ralBindGroup_t *oldBeam = vk.beam.ral_descriptor[i];
		vk.ribbon.ral_descriptor[i] = ribbon[i]; ribbon[i] = NULL;
		vk.railRibbon.ral_descriptor[i] = rail[i]; rail[i] = NULL;
		vk.beam.ral_descriptor[i] = beam[i]; beam[i] = NULL;
		vk.ribbon.descriptor[i] = vk.ribbon.ral_descriptor[i]
			? (VkDescriptorSet)Ral_GetBindGroupHandle( vk.ribbon.ral_descriptor[i] ) : VK_NULL_HANDLE;
		vk.railRibbon.descriptor[i] = vk.railRibbon.ral_descriptor[i]
			? (VkDescriptorSet)Ral_GetBindGroupHandle( vk.railRibbon.ral_descriptor[i] ) : VK_NULL_HANDLE;
		vk.beam.descriptor[i] = vk.beam.ral_descriptor[i]
			? (VkDescriptorSet)Ral_GetBindGroupHandle( vk.beam.ral_descriptor[i] ) : VK_NULL_HANDLE;
		if ( oldRibbon ) Ral_DestroyBindGroup( oldRibbon );
		if ( oldRail ) Ral_DestroyBindGroup( oldRail );
		if ( oldBeam ) Ral_DestroyBindGroup( oldBeam );
	}
	return qtrue;

fail:
	for ( i = 0; i < NUM_COMMAND_BUFFERS; ++i ) {
		if ( ribbon[i] ) Ral_DestroyBindGroup( ribbon[i] );
		if ( rail[i] ) Ral_DestroyBindGroup( rail[i] );
		if ( beam[i] ) Ral_DestroyBindGroup( beam[i] );
	}
	return qfalse;
}

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
	// Engine resources is created directly from the generation-bound arena. The
	// raw descriptor field is only its compatibility mirror; an unsuccessful
	// refresh preserves the prior complete cohort and never adopts a second owner.
	if ( !vk_ral_refresh_engine_resources_bindgroup() )
		R_LOG( rch_ral, SEV_WARN,
			"engine-resources: RAL bind-group refresh declined\n" );
	// ── per-frame-ring uniform descriptors (vk.tess[NUM_COMMAND_BUFFERS]) ──
	for ( i = 0; i < ARRAY_LEN( vk.tess ); i++ ) {
		(void)vk_ral_refresh_tess_uniform_bindgroup( i );
		if ( vk.tess[i].ral_entMatBuf != NULL )
			(void)vk_ral_refresh_entmat_bindgroup( i );
	}

	// MSDF set 3 is created directly from the generation-bound RAL arena. This
	// sweep only verifies that the legacy raw field remains its exact mirror.
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( !vk.msdf.ral_descriptor[i] || !vk.msdf.ral_buffer[i]
		  || Ral_GetBindGroupHandle( vk.msdf.ral_descriptor[i] )
			!= (void *)vk.msdf.descriptor[i] )
			ri.Terminate( TERM_UNRECOVERABLE,
				"Vulkan: MSDF RAL bind-group cohort drifted at slot %u", i );
	}

	// Exposure and menu-backdrop bind groups are created directly from the
	// generation-bound arena. This sweep only verifies their raw mirrors and
	// persistent RAL-owned buffers; it must never adopt a replacement set.
	if ( vk.ral_color_image ) {
		for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
			if ( !vk.exposure.ral_descriptor[i] || !vk.exposure.ral_buffer[i]
			  || Ral_GetBindGroupHandle( vk.exposure.ral_descriptor[i] )
				!= (void *)vk.exposure.descriptor[i] )
				ri.Terminate( TERM_UNRECOVERABLE,
					"Vulkan: exposure RAL bind-group cohort drifted at slot %u", i );
			if ( !vk.menubg.ral_descriptor[i] || !vk.menubg.ral_buffer[i]
			  || Ral_GetBindGroupHandle( vk.menubg.ral_descriptor[i] )
				!= (void *)vk.menubg.descriptor[i] )
				ri.Terminate( TERM_UNRECOVERABLE,
					"Vulkan: menubg RAL bind-group cohort drifted at slot %u", i );
		}
	}

	// Particle compute is one candidate-first two-slot cohort; verify it once,
	// not once per slot like the remaining legacy ring adoption calls.
	if ( vk.particle.available
	  && !vk_ral_refresh_particle_compute_bindgroups() )
		ri.Terminate( TERM_UNRECOVERABLE,
			"Vulkan: particle compute RAL bind-group cohort drifted" );

	// ── per-subsystem rings (each NUM_COMMAND_BUFFERS slots) ──
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		if ( vk.sprite.available && !vk_ral_refresh_sprite_bindgroup( i ) )
			ri.Terminate( TERM_UNRECOVERABLE,
				"Vulkan: sprite RAL bind-group cohort drifted at slot %u", i );

		// Effects set 1 is created directly from the renderer's generation-bound
		// RAL arena. Keep that exact owner when this compatibility adoption sweep
		// visits the remaining legacy sets; the raw VkDescriptorSet is only its
		// borrowed mirror.
		if ( !vk.effectsUbo.ral_descriptor[i]
		  || !vk.effectsUbo.ral_buffer[i]
		  || Ral_GetBindGroupHandle( vk.effectsUbo.ral_descriptor[i] )
			!= (void *)vk.effectsUbo.descriptor[i] )
			ri.Terminate( TERM_UNRECOVERABLE,
				"Vulkan: effects RAL bind-group cohort drifted at slot %u", i );
#if FEAT_IQM
		if ( vk.iqmGpu.available
				&& !vk_ral_refresh_iqm_bone_bindgroup( i ) )
			ri.Terminate( TERM_UNRECOVERABLE,
				"Vulkan: IQM bone RAL bind-group cohort drifted at slot %u", i );
#endif
	}
	if ( !vk.fboActive || !vk_ral_smaa_bindgroups_ready() ) {
		if ( vk.fboActive )
			R_LOG( rch_ral, SEV_WARN,
				"SMAA direct RAL sampler cohort creation failed; disabling typed SMAA command path\n" );
		vk_ral_destroy_smaa_bindgroups();
	}

	#undef ADOPT
	#undef RETAIN_ADOPT

	R_LOG( rch_ral, SEV_INFO,
		"adopted %u compatibility bind groups as ralBindGroup_t (legacy singletons + per-frame rings)\n",
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
	vk_ral_refresh_internal_texture_dependents();
}

static qboolean vk_ral_image_texture_info( const image_t *image,
		ralTextureCreateInfo_t *out, uint32_t *outArrayLayers ) {
	ralTextureCreateInfo_t candidate;
	uint32_t layers = 1u;
	if ( !image || !out || !outArrayLayers || image->uploadWidth <= 0
			|| image->uploadHeight <= 0 || image->mipLevelCount == 0u )
		return qfalse;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.format = vk_attachment_format_to_ral(
		(VkFormat)image->internalFormat );
	if ( candidate.format == RAL_FORMAT_UNDEFINED ) return qfalse;
	candidate.width = (uint32_t)image->uploadWidth;
	candidate.height = (uint32_t)image->uploadHeight;
	candidate.mipLevels = image->mipLevelCount;
	candidate.sampleCount = 1u;
	candidate.usage = RAL_TEXTURE_USAGE_SAMPLED
		| RAL_TEXTURE_USAGE_TRANSFER_DST;
	candidate.memory = RAL_MEMORY_DEVICE_LOCAL;
	candidate.debugName = image->imgName;
	if ( image->texType == TEXTYPE_CUBE ) {
		candidate.type = RAL_TEXTURE_CUBE;
		candidate.depthOrArrayLayers = 1u;
		layers = 6u;
	} else if ( image->texType == TEXTYPE_3D ) {
		candidate.type = RAL_TEXTURE_3D;
		candidate.depthOrArrayLayers = image->depth > 0
			? (uint32_t)image->depth : 1u;
	} else if ( image->texType == TEXTYPE_CUBE_ARRAY ) {
		if ( image->layerCount == 0u || image->layerCount % 6u != 0u )
			return qfalse;
		candidate.type = RAL_TEXTURE_CUBE_ARRAY;
		candidate.depthOrArrayLayers = image->layerCount / 6u;
		layers = image->layerCount;
	} else if ( image->layerCount > 1u ) {
		candidate.type = RAL_TEXTURE_2D_ARRAY;
		candidate.depthOrArrayLayers = image->layerCount;
		layers = image->layerCount;
	} else {
		candidate.type = RAL_TEXTURE_2D;
		candidate.depthOrArrayLayers = 1u;
	}
	*out = candidate;
	*outArrayLayers = layers;
	return qtrue;
}

qboolean vk_ral_create_image_texture_candidate( image_t *image,
		ralTexture_t **outTexture ) {
	ralTextureCreateInfo_t createInfo;
	ralTextureResourceReceipt_t receipt;
	ralTexture_t *candidate;
	void *candidateImage, *candidateView;
	uint32_t i, expectedArrayLayers;
	if ( !image || !outTexture || *outTexture || !s_ral_backend
			|| !vk_ral_image_texture_info( image, &createInfo,
				&expectedArrayLayers ) ) return qfalse;
	candidate = Ral_CreateTexture( s_ral_backend, &createInfo );
	if ( !candidate ) return qfalse;
	if ( !Ral_TextureGetResourceReceipt( candidate, &receipt )
			|| receipt.imported || !receipt.ready
			|| receipt.type != createInfo.type
			|| receipt.format != createInfo.format
			|| receipt.usage != createInfo.usage
			|| receipt.width != createInfo.width
			|| receipt.height != createInfo.height
			|| receipt.mipLevels != createInfo.mipLevels
			|| receipt.arrayLayers != expectedArrayLayers ) {
		Ral_DestroyTexture( candidate );
		return qfalse;
	}
	candidateImage = Ral_GetTextureImageHandle( candidate );
	candidateView = Ral_GetTextureDefaultViewHandle( candidate );
	if ( !candidateImage || !candidateView ) {
		Ral_DestroyTexture( candidate );
		return qfalse;
	}
	for ( i = 0u; i < (uint32_t)tr.numImages; ++i ) {
		const image_t *live = tr.images[i];
		if ( !live || !live->ral ) continue;
		if ( candidate == live->ral
				|| candidateImage == Ral_GetTextureImageHandle( live->ral )
				|| candidateView == Ral_GetTextureDefaultViewHandle( live->ral ) ) {
			Ral_DestroyTexture( candidate );
			return qfalse;
		}
	}
	*outTexture = candidate;
	return qtrue;
}

qboolean vk_ral_refresh_image_descriptor( image_t *image,
		void *nativeSampler )
{
	ralTextureView_t *viewCandidate = NULL, *viewRetired;
	ralBindGroup_t *groupCandidate = NULL, *groupRetired;
	ralSampler_t *sampler;
	ralBindingValue_t value;
	ralBindGroupCreateInfo_t createInfo;
	ralTextureViewCreateInfo_t viewInfo;
	VkDescriptorSet rawCandidate;
	ralTextureCreateInfo_t textureInfo;
	ralTextureResourceReceipt_t textureReceipt;
	qboolean viewCandidateOwned = qtrue;
	qboolean groupCandidateOwned = qtrue;
	uint32_t i, expectedArrayLayers;

	if ( !image || !image->ral || !nativeSampler
			|| image->uploadWidth <= 0 || image->uploadHeight <= 0
			|| !s_ral_backend || !vk.ral_bgl_sampler
			|| !vk.ral_descriptor_arena
			|| !Ral_BindGroupArenaReceiptValid(
				&vk.ral_descriptor_arena_receipt )
			|| vk.ral_descriptor_arena_receipt.backendIdentity
				!= s_ral_backend
			|| vk.ral_descriptor_arena_receipt.arenaIdentity
				!= vk.ral_descriptor_arena ) return qfalse;
	if ( !vk_ral_image_texture_info( image, &textureInfo,
			&expectedArrayLayers ) ) return qfalse;
	if ( !Ral_TextureGetResourceReceipt( image->ral, &textureReceipt )
			|| textureReceipt.imported
			|| textureReceipt.type != textureInfo.type
			|| textureReceipt.format != textureInfo.format
			|| textureReceipt.usage != textureInfo.usage
			|| textureReceipt.width != textureInfo.width
			|| textureReceipt.height != textureInfo.height
			|| textureReceipt.mipLevels != textureInfo.mipLevels
			|| textureReceipt.arrayLayers != expectedArrayLayers ) return qfalse;
	sampler = vk_ral_lookup_sampler( nativeSampler );
	if ( !sampler ) return qfalse;
	if ( image->ralDescriptor && image->ralDescriptorView
			&& image->ralDescriptorSampler == sampler
			&& image->descriptor != VK_NULL_HANDLE
			&& Ral_GetBindGroupHandle( image->ralDescriptor )
				== (void *)image->descriptor
			&& Ral_GetTextureViewHandle( image->ralDescriptorView )
			&& Ral_GetSamplerHandle( sampler ) == nativeSampler
			)
		return qtrue;

	memset( &viewInfo, 0, sizeof( viewInfo ) );
	viewInfo.texture = image->ral;
	viewInfo.viewType = textureInfo.type;
	viewInfo.format = RAL_FORMAT_UNDEFINED;
	viewCandidate = Ral_CreateTextureView( s_ral_backend, &viewInfo );
	if ( !viewCandidate ) goto fail;
	memset( &value, 0, sizeof( value ) );
	value.binding = 0u;
	value.type = RAL_BIND_COMBINED_TEXTURE_SAMPLER;
	value.textureView = viewCandidate;
	value.sampler = sampler;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.layout = vk.ral_bgl_sampler;
	createInfo.values = &value;
	createInfo.numValues = 1u;
	createInfo.debugName = image->imgName;
	createInfo.arena = vk.ral_descriptor_arena;
	createInfo.arenaReceipt = &vk.ral_descriptor_arena_receipt;
	groupCandidate = Ral_CreateBindGroup( s_ral_backend, &createInfo );
	if ( !groupCandidate ) goto fail;
	rawCandidate = (VkDescriptorSet)Ral_GetBindGroupHandle( groupCandidate );
	if ( rawCandidate == VK_NULL_HANDLE
			|| !Ral_GetTextureImageHandle( image->ral )
			|| !Ral_GetTextureViewHandle( viewCandidate )
			|| Ral_GetSamplerHandle( sampler ) != nativeSampler ) goto fail;
	for ( i = 0u; i < (uint32_t)tr.numImages; ++i ) {
		const image_t *live = tr.images[i];
		if ( !live ) continue;
		if ( groupCandidate == live->ralDescriptor ) {
			groupCandidateOwned = qfalse;
			goto fail;
		}
		if ( viewCandidate == live->ralDescriptorView
				|| viewCandidate == live->ralResidencyView
				|| viewCandidate == live->ralCoarseResidencyView ) {
			viewCandidateOwned = qfalse;
			goto fail;
		}
		if ( rawCandidate == live->descriptor ) goto fail;
	}

	groupRetired = image->ralDescriptor;
	viewRetired = image->ralDescriptorView;
	image->ralDescriptor = groupCandidate;
	image->ralDescriptorView = viewCandidate;
	image->ralDescriptorSampler = sampler;
	image->descriptor = rawCandidate;
	if ( groupRetired ) Ral_DestroyBindGroup( groupRetired );
	if ( viewRetired ) Ral_DestroyTextureView( viewRetired );
	return qtrue;

fail:
	if ( groupCandidate && groupCandidateOwned )
		Ral_DestroyBindGroup( groupCandidate );
	if ( viewCandidate && viewCandidateOwned )
		Ral_DestroyTextureView( viewCandidate );
	return qfalse;
}

void vk_ral_release_image_descriptor( image_t *image )
{
	if ( !image ) return;
	if ( image->ralDescriptor ) Ral_DestroyBindGroup( image->ralDescriptor );
	if ( image->ralDescriptorView )
		Ral_DestroyTextureView( image->ralDescriptorView );
	image->ralDescriptor = NULL;
	image->ralDescriptorView = NULL;
	image->ralDescriptorSampler = NULL;
	image->descriptor = VK_NULL_HANDLE;
}


static void vk_ral_destroy_adopted_bindgroups( void )
{
	uint32_t i;
	#define DESTROY_RETAINED_BG( field ) do { \
		if ( (field) ) { Ral_DestroyBindGroup( (field) ); (field) = NULL; } \
	} while ( 0 )
	vk_ral_release_attachment_sampler_cohorts();
	vk_ral_release_engine_resources_bindgroup();
	DESTROY_RETAINED_BG( vk.blueNoise.ral_descriptor );
	vk.blueNoise.descriptor = VK_NULL_HANDLE;
	vk_ral_destroy_smaa_bindgroups();
	vk_ral_release_particle_compute_bindgroups();
	vk_ral_release_particle_render_bindgroups();
	vk_ral_release_decal_render_bindgroups();
	vk_ral_release_primitive_bindgroups();
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		DESTROY_RETAINED_BG( vk.tess[i].ral_uniform_descriptor );
		vk_ral_release_entmat_bindgroup( i );
		DESTROY_RETAINED_BG( vk.msdf.ral_descriptor[i] );
		DESTROY_RETAINED_BG( vk.exposure.ral_descriptor[i] );
		DESTROY_RETAINED_BG( vk.menubg.ral_descriptor[i] );
		vk_ral_release_sprite_bindgroup( i );
		DESTROY_RETAINED_BG( vk.atm.ral_compute_descriptor[i] );
		DESTROY_RETAINED_BG( vk.atm.ral_render_descriptor[i] );
		vk.atm.compute_descriptor[i] = VK_NULL_HANDLE;
		vk.atm.render_descriptor[i] = VK_NULL_HANDLE;
		DESTROY_RETAINED_BG( vk.effectsUbo.ral_descriptor[i] );
#if FEAT_IQM
		vk_ral_release_iqm_bone_bindgroup( i );
#endif
		// The five persistent UI/post-process buffer cohorts are RAL-owned and
		// survive descriptor-arena resets. Final renderer teardown releases them.
	}
	for ( i = 0; i < s_adopted_bgs_count; i++ ) {
		if ( s_adopted_bgs[i] ) {
			Ral_DestroyBindGroup( s_adopted_bgs[i] );
			s_adopted_bgs[i] = NULL;
		}
	}
	s_adopted_bgs_count = 0;
	#undef DESTROY_RETAINED_BG
}

void vk_ral_release_static_bindgroups( void )
{
	int i;
	for ( i = 0; i < tr.numImages; i++ )
		vk_ral_release_image_descriptor( tr.images[i] );
	vk_ral_destroy_adopted_bindgroups();
}


// ════════════════════════════════════════════════════════════════════════
// Boot-time direct pipeline-layout verification sweep.
//
// Confirms that every legacy native mirror points at its typed RAL owner.
// Called from vk_ral_adopt_static_bindgroups's tail; teardown remains in the
// full vk_ral_textures_shutdown branch. No native layout is adopted here.
// ════════════════════════════════════════════════════════════════════════
static uint32_t s_ral_pipeline_layouts_verified;

void vk_ral_adopt_static_pipeline_layouts( void )
{
	uint32_t direct = 0u;
	if ( !s_ral_backend ) return;
	#define VERIFY_PL( native, owner ) do { \
		if ( (owner) && Ral_GetPipelineLayoutHandle( (owner) ) == (void *)(native) ) direct++; \
	} while ( 0 )
	VERIFY_PL( vk.pipeline_layout, vk.ral_pipeline_layout );
	VERIFY_PL( vk.pipeline_layout_post_process, vk.ral_pipeline_layout_post_process );
	VERIFY_PL( vk.pipeline_layout_smaa, vk.ral_pipeline_layout_smaa );
	VERIFY_PL( vk.pipeline_layout_msdf, vk.ral_pipeline_layout_msdf );
	VERIFY_PL( vk.pipeline_layout_ssao, vk.ral_pipeline_layout_ssao );
	VERIFY_PL( vk.pipeline_layout_sunrays, vk.ral_pipeline_layout_sunrays );
	#undef VERIFY_PL
	s_ral_pipeline_layouts_verified = direct;
	R_LOG( rch_ral, SEV_INFO,
		"verified %u direct RAL core pipeline layouts\n", direct );
}


// Refresh every long-lived child that borrows a direct RAL attachment. Texture
// parents are created by vk_create_attachments; this function owns no adoption
// or replacement authority.
void vk_ral_refresh_internal_texture_dependents( void )
{
	if ( !s_ral_backend ) return;
	// Replacement remains child-before-parent: release every dependent view,
	// bind group and compute cohort before rebuilding them over the live parents.
	vk_ral_release_internal_texture_dependents();

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

	// GTAO ambient occlusion. Depends on the RAL-owned depth copy
	// (sceneDepth.ral_image), created with the attachments before this sweep. No-ops
	// when SSAO is off (no depth copy); a vid_restart with r_ssao on re-runs this
	// with the depth copy present. Idempotent across vid_restart.
	vk_gtao_init( s_ral_backend );

	// Lens-glow occlusion oracle. Like GTAO, depends on the depth copy
	// (sceneDepth.ral_image); the sampling view + the
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

	// Re-mint the shared attachment sampling views before any later consumer
	// borrows them. The dependency sweep above retired sceneDepth.ral_view;
	// atmosphere bind groups must never capture that stale/null child and the
	// refresh must not run again after those bind groups have been created.
	// This also publishes the IBL views created earlier in this sweep.
	vk_update_attachment_descriptors();

	// Full atmosphere consumes the adopted scene HDR/depth resources and borrows
	// the shipping Forward+ tile/light buffers, so it is the final compute cohort
	// in this dependency-ordered sweep.
	vk_atmosphere_full_init( s_ral_backend );
	// H2a scene identity is usable only after the raw color attachment, adopted
	// texture, postprocess group and histogram group belong to this same sweep.
	// Publishing earlier would let pointer reuse satisfy a stale target receipt.
	vk_temporal_scene_color_attachment_published();
}


void vk_ral_release_internal_texture_dependents( void )
{
	// Views/groups must not outlive their adopted texture wrappers or the raw
	// renderer-owned VkImages those wrappers reference.
	vk_ral_release_engine_resources_bindgroup();
	vk_ral_release_attachment_sampler_cohorts();
	// HDR histogram + exposure-reduce resources are brought up at the tail of the
	// adopt sweep and reference the adopted color image / exposure UBO ring; tear
	// them down symmetrically here, before the color image, so nothing outlives the
	// device. Reduce before histogram (reduce's bind-group references the histogram
	// buffer; free the dependent first).
	vk_hdr_exposure_reduce_shutdown();
	vk_hdr_histogram_shutdown();

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
	vk_atmosphere_full_shutdown();
	vk_forwardplus_lit_shutdown();
	vk_forwardplus_shutdown();

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
#if FEAT_SHADOW_MAPPING
	KILL_PL( vk.shadowMap.ral_depthLayout );
#endif
	#undef KILL_PL
	s_ral_pipeline_layouts_verified = 0;
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
		// Always log on shutdown so bindless + direct RAL resource
		// population is observable in the captured log.).
		vk_ral_textures_diag_dump();
	}
	// REF_LEVEL_ONLY skip gate.
	// destroyWindow == qfalse means map-scoped teardown (map transition):
	// keep the RAL backend, sibling pipelines, BGLs and direct resources live
	// across the transition. Mirrors vk_shutdown's skip-on-
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
	// the pipeline-cache save and renderer-owned resource paths are safer
	// short-circuited when vk_initialize never reached vk.active=qtrue.
	// vk_shutdown's own half-init branch destroys the RAL backend itself.
	if ( !vk.active ) {
		R_LOG( rch_ral, SEV_INFO, "vk_ral_textures_shutdown: half-init (decline) path — skipping full teardown\n" );
		s_ral_init_attempted = qfalse;
		return;
	}
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
	// The four portable effect families directly own their RAL pipelines,
	// pipeline layouts, bind groups, and bind-group layouts. Retire each
	// complete family before the generic adopted-wrapper sweeps below so the
	// dependency order remains pipeline -> pipeline layout -> BGL on every
	// full teardown path. vk_shutdown repeats these calls later, but each
	// family shutdown is intentionally NULL-safe.
	vk_shutdown_ribbon();
	vk_shutdown_railribbon();
	vk_shutdown_beam();
	vk_shutdown_sprite();
	vk_shutdown_particle();
	vk_shutdown_decal();
	vk_shutdown_atmospheric();
#if FEAT_IQM
	vk_shutdown_iqm_gpu_skinning();
#endif
	// destroy every adopted bind-group wrapper.
	// ownsSet=qfalse on adopted wrappers means Ral_DestroyBindGroup only
	// frees the wrapper struct, not the underlying VkDescriptorSet — the
	// legacy arena-reset path frees the sets. Must run BEFORE
	// Ral_DestroyBackend so the wrappers don't outlive the backend they
	// reference.
	vk_ral_destroy_adopted_bindgroups();

	// Destroy every direct core pipeline-layout owner. The legacy native fields
	// are mirrors only and are nulled by the renderer teardown that follows.
	vk_ral_destroy_adopted_pipeline_layouts();

	// Destroy every adopted internal-texture wrapper. ownsImage=qfalse means
	// only wrappers are freed; underlying VkImages remain renderer-owned.
	// GPU culling is world-owned rather than attachment-owned, so it is released
	// only on this full backend teardown path, not on live HDR/FBO rebuilds.
	vk_cull_shutdown();
	vk_ral_release_internal_texture_dependents();

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
		KILL_BGL( vk.ral_bgl_exposure );
		KILL_BGL( vk.ral_bgl_effects_ubo );
		KILL_BGL( vk.ral_bgl_smaa_rtmetrics );
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
		for ( i = 0; i < s_ral_asset_chunk_request_count; ++i )
			if ( s_ral_asset_chunk_requests[i].data )
				ri.Free( s_ral_asset_chunk_requests[i].data );
		s_ral_asset_chunk_request_count = 0;
		s_ral_asset_chunk_request_peak = 0;
	}
	vk_ral_release_upload_ticket( &s_ral_mip_test_ticket );
	vk_ral_material_reset( qfalse );
	s_ral_mip_test_upload_frame = 0;
	s_ral_mip_test_upload_bytes = 0;
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
		uint32_t mipLevels, ralResidencyClass_t classId,
		ralResidencyState_t initialState,
		uint32_t serial ) {
	uint32_t i;
	if ( !image || mipLevels == 0 || mipLevels > MAX_IMAGE_RESIDENCY_MIPS
			|| classId >= RAL_RESIDENCY_CLASS_COUNT ) return qfalse;
	memset( image->ralMipResidency, 0, sizeof( image->ralMipResidency ) );
	for ( i = 0; i < mipLevels; ++i ) {
		ralResidencyPageId_t id;
		memset( &id, 0, sizeof( id ) );
		id.classId = classId;
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

qboolean vk_ral_queue_asset_chunk_image( image_t *image, const byte *pic,
		int width, int height, uint64_t dataSize ) {
	ralTextureViewCreateInfo_t vci;
	vk_ral_asset_chunk_request_t *request;
	byte *owned;
	uint32_t mipLevels, resource, slot;

	if ( !vk_ral_textures_available() || !image || !image->ral || !pic
			|| width <= 0 || height <= 0 || dataSize == 0u
			|| dataSize > (uint64_t)INT_MAX || image->texType != TEXTYPE_2D
			|| image->layerCount > 1u || image->ralResidencyView
			|| image->ralBindlessSlot >= 0 || !tr.defaultImage
			|| !tr.defaultImage->ral || s_ral_asset_chunk_request_count >= VK_RAL_MAX_PENDING_UPLOADS
			|| s_ral_pending_upload_count >= VK_RAL_MAX_PENDING_UPLOADS ) return qfalse;
	if ( s_ral_bindless_free_count == 0
			&& s_ral_bindless_next >= s_ral_bindless_capacity ) return qfalse;
	if ( tr.numImages <= 0 || tr.images[tr.numImages - 1] != image ) return qfalse;

	mipLevels = Ral_GetTextureMipLevelCount( image->ral );
	if ( mipLevels != 1u ) return qfalse;
	resource = (uint32_t)( tr.numImages - 1 );
	owned = ri.Malloc( (int)dataSize );
	memcpy( owned, pic, (size_t)dataSize );

	if ( !vk_ral_init_mip_records( image, resource, mipLevels,
			RAL_RESIDENCY_CLASS_ASSET_CHUNK, RAL_RESIDENCY_REQUESTED,
			(uint32_t)tr.frameCount ) ) {
		ri.Free( owned );
		return qfalse;
	}
	memset( &vci, 0, sizeof( vci ) );
	vci.texture = image->ral;
	vci.viewType = RAL_TEXTURE_2D;
	vci.format = RAL_FORMAT_UNDEFINED;
	vci.mipLevelCount = 0;
	vci.arrayLayerCount = 1;
	image->ralResidencyView = Ral_CreateTextureView( s_ral_backend, &vci );
	if ( !image->ralResidencyView ) {
		image->ralResidencyMipCount = 0;
		ri.Free( owned );
		return qfalse;
	}

	slot = vk_ral_alloc_bindless_slot( image );
	if ( slot >= s_ral_bindless_capacity ) {
		Ral_DestroyTextureView( image->ralResidencyView );
		image->ralResidencyView = NULL;
		image->ralResidencyMipCount = 0;
		ri.Free( owned );
		return qfalse;
	}
	image->ralBindlessSlot = (int)slot;
	if ( !vk_ral_bindless_publish_texture( image, slot,
			tr.defaultImage->ral, VK_BINDLESS_PUBLICATION_PLACEHOLDER ) ) {
		image->ralBindlessSlot = -1;
		if ( s_ral_bindless_free_count < WIRED_BINDLESS_TEX_SLOTS )
			s_ral_bindless_free[s_ral_bindless_free_count++] = slot;
		Ral_DestroyTextureView( image->ralResidencyView );
		image->ralResidencyView = NULL;
		image->ralResidencyMipCount = 0;
		ri.Free( owned );
		return qfalse;
	}

	request = &s_ral_asset_chunk_requests[s_ral_asset_chunk_request_count++];
	request->image = image;
	request->data = owned;
	request->dataSize = dataSize;
	request->requestFrame = (uint32_t)tr.frameCount;
	if ( s_ral_asset_chunk_request_count > s_ral_asset_chunk_request_peak )
		s_ral_asset_chunk_request_peak = s_ral_asset_chunk_request_count;
	s_ral_registered_count++;
	vk_ral_record_name( image->imgName );
	image->flags &= ~IMGFLAG_RESIDENCY_EVICTED;
	R_LOG( rch_ral_texture, SEV_DEBUG,
		"RAL asset chunk: action=request class=asset-chunk resource=%u slot=%u bytes=%llu fallback=default queue=%u name=%s\n",
		resource, slot, (unsigned long long)dataSize,
		s_ral_asset_chunk_request_count, image->imgName );
	return qtrue;
}

static void vk_ral_admit_asset_chunks( void ) {
	ralResidencyBudget_t budget;
	uint32_t i;
	size_t selectedCount, selectedIndex;
	uint64_t largest = 0u;

	if ( s_ral_asset_chunk_request_count == 0
			|| s_ral_pending_upload_count >= VK_RAL_MAX_PENDING_UPLOADS ) return;
	memset( s_ral_asset_chunk_admit, 0, s_ral_asset_chunk_request_count );
	for ( i = 0; i < s_ral_asset_chunk_request_count; ++i ) {
		vk_ral_asset_chunk_request_t *request = &s_ral_asset_chunk_requests[i];
		ralResidencyCandidate_t *candidate = &s_ral_asset_chunk_candidates[i];
		memset( candidate, 0, sizeof( *candidate ) );
		candidate->id = request->image->ralMipResidency[0].id;
		candidate->state = request->image->ralMipResidency[0].state;
		candidate->tier = RAL_RESIDENCY_TIER_VISIBLE;
		candidate->ageFrames = (uint32_t)tr.frameCount - request->requestFrame;
		candidate->costBytes = request->dataSize;
		candidate->pinned = 1;
		candidate->fallbackReady = 1;
		if ( request->dataSize > largest ) largest = request->dataSize;
	}
	memset( &budget, 0, sizeof( budget ) );
	budget.maxPages = VK_RAL_ASSET_CHUNK_PAGES_PER_FRAME;
	budget.maxBytes = largest > VK_RAL_ASSET_CHUNK_BYTES_PER_FRAME
		? largest : VK_RAL_ASSET_CHUNK_BYTES_PER_FRAME;
	budget.minPerClass[RAL_RESIDENCY_CLASS_ASSET_CHUNK] = 1;
	selectedCount = Ral_ResidencySelectRequests( s_ral_asset_chunk_candidates,
		s_ral_asset_chunk_request_count, &ralResidencyDefaultPolicy, &budget,
		s_ral_asset_chunk_selected, VK_RAL_ASSET_CHUNK_PAGES_PER_FRAME,
		s_ral_asset_chunk_scratch );
	for ( selectedIndex = 0; selectedIndex < selectedCount; ++selectedIndex )
		s_ral_asset_chunk_admit[s_ral_asset_chunk_selected[selectedIndex]] = 1;

	i = 0;
	while ( i < s_ral_asset_chunk_request_count
			&& s_ral_pending_upload_count < VK_RAL_MAX_PENDING_UPLOADS ) {
		vk_ral_asset_chunk_request_t *request = &s_ral_asset_chunk_requests[i];
		ralTextureUploadDesc_t upload;
		ralUploadTicket_t ticket;
		vk_ral_pending_upload_t *pending;
		if ( !s_ral_asset_chunk_admit[i] ) {
			i++;
			continue;
		}
		memset( &upload, 0, sizeof( upload ) );
		upload.data = request->data;
		upload.dataSize = request->dataSize;
		upload.suppressMipGeneration = qtrue;
		ticket = Ral_TextureUploadBegin( request->image->ral, &upload );
		if ( !ticket.fence ) {
			i++;
			continue;
		}
		if ( !vk_ral_mip_transition( request->image, 0,
				RAL_RESIDENCY_IN_FLIGHT, 0, qtrue,
				(uint32_t)tr.frameCount ) ) {
			vk_ral_release_upload_ticket( &ticket );
			i++;
			continue;
		}
		pending = &s_ral_pending_uploads[s_ral_pending_upload_count++];
		pending->ticket = ticket;
		pending->slot = (uint32_t)request->image->ralBindlessSlot;
		pending->image = request->image;
		s_ral_upload_async_count += ticket.synchronous ? 0u : 1u;
		s_ral_upload_sync_count += ticket.synchronous ? 1u : 0u;
		R_LOG( rch_ral_texture, SEV_DEBUG,
			"RAL asset chunk: action=admit class=asset-chunk resource=%u slot=%u bytes=%llu synchronous=%d pending=%u name=%s\n",
			request->image->ralMipResidency[0].id.resource, pending->slot,
			(unsigned long long)request->dataSize, ticket.synchronous ? 1 : 0,
			s_ral_pending_upload_count, request->image->imgName );
		ri.Free( request->data );
		*request = s_ral_asset_chunk_requests[--s_ral_asset_chunk_request_count];
		if ( i < s_ral_asset_chunk_request_count )
			s_ral_asset_chunk_admit[i] = s_ral_asset_chunk_admit[s_ral_asset_chunk_request_count];
	}
	if ( s_ral_pending_upload_count > s_ral_pending_peak )
		s_ral_pending_peak = s_ral_pending_upload_count;
}

void vk_ral_register_image( image_t *image, byte *pic, int width, int height ) {
	ralTextureViewCreateInfo_t vci;
	uint32_t               slot;
	uint32_t               resource = ~0u;
	uint32_t               mipLevels;

	if ( !vk_ral_textures_available() || !image || !image->ral ) return;
	if ( image->texType != TEXTYPE_2D || image->layerCount > 1u ) return;
	if ( width <= 0 || height <= 0 ) { s_ral_skipped_no_data++; return; }
	if ( image->ralResidencyView || image->ralBindlessSlot >= 0 ) return;
	mipLevels = Ral_GetTextureMipLevelCount( image->ral );
	if ( tr.numImages > 0 && tr.images[tr.numImages - 1] == image )
		resource = (uint32_t)( tr.numImages - 1 );
	// Empty create-then-fill images retain the legacy lifecycle until their
	// sub-region producer gains page records.  Every ordinary decoded image gets
	// an exact persistent address/state table before its first upload.
	if ( pic && resource != ~0u &&
	     !vk_ral_init_mip_records( image, resource, mipLevels,
	                               RAL_RESIDENCY_CLASS_TEXTURE,
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
		return;
	}

	slot = vk_ral_alloc_bindless_slot( image );
	// The exact texture upload completed through vk_ral_stage_texture_copy before
	// registration. Publish that same resource identity into the residency slot.
	if ( slot < s_ral_bindless_capacity ) {
		uint32_t level;
		s_ral_upload_sync_count++;
		for ( level = 0; level < image->ralResidencyMipCount; ++level )
			(void)vk_ral_mip_mark_resident( image, level,
				(uint32_t)tr.frameCount );
		if ( vk_ral_whole_texture_promotion_ready( image ) )
			(void)vk_ral_bindless_publish_texture_view( image, slot,
				image->ralResidencyView, VK_BINDLESS_PUBLICATION_RESIDENT );
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
	vk_ral_admit_asset_chunks();
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
	return ( s_ral_pending_upload_count > 0
		|| s_ral_asset_chunk_request_count > 0
		|| s_ral_mip_test_ticket.fence != NULL ) ? qtrue : qfalse;
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

qboolean vk_ral_rebuild_bindless_set( void ) {
	ralBindGroupCreateInfo_t createInfo;
	ralBindGroup_t *candidate, *retired;
	if ( !s_ral_backend || !s_ral_bindless_layout ) return qfalse;
	memset( &createInfo, 0, sizeof( createInfo ) );
	createInfo.layout = s_ral_bindless_layout;
	createInfo.debugName = "renderer-bindless-set";
	candidate = Ral_CreateBindGroup( s_ral_backend, &createInfo );
	if ( !candidate || !Ral_GetBindGroupHandle( candidate ) ) {
		if ( candidate ) Ral_DestroyBindGroup( candidate );
		return qfalse;
	}
	retired = s_ral_bindless_set;
	if ( retired && s_bindless_publication_initialized )
		(void)VK_BindlessPublicationInvalidateSet(
			&s_bindless_publication, retired );
	s_ral_bindless_set = candidate;
	if ( !vk_ral_bindless_ledger_activate() ) {
		s_ral_bindless_set = retired;
		Ral_DestroyBindGroup( candidate );
		if ( retired ) (void)vk_ral_bindless_ledger_activate();
		return qfalse;
	}
	if ( retired ) Ral_DestroyBindGroup( retired );
	return qtrue;
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
		while ( i < s_ral_asset_chunk_request_count ) {
			vk_ral_asset_chunk_request_t *request = &s_ral_asset_chunk_requests[i];
			if ( request->image == image ) {
				if ( request->data ) ri.Free( request->data );
				*request = s_ral_asset_chunk_requests[--s_ral_asset_chunk_request_count];
			} else {
				i++;
			}
		}
		i = 0;
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
		if ( image->flags & IMGFLAG_ARRAY ) {
			// Array images use the disjoint array binding and its bulk-reset
			// allocator; never return that index to the ordinary-texture free list.
			image->ralBindlessSlot = -1;
		} else {
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


// Slot allocator for the direct 2DArray SAMPLED_IMAGE binding.
// Used by vk_ral_register_image_array (defined in vk.c, where image_t slot
// ownership lives). The binding-aware RAL writer consumes the result. Bumps the slot
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
	if ( ri.Cmd_Argc() > 2 && Q_stricmp( ri.Cmd_Argv( 2 ), "framegraph" ) == 0 ) {
		ralFrameGraphNativeReceipt_t receipt;
		uint64_t generation;
		if ( !s_ral_backend
				|| s_ral_frame_graph_diagnostic_generation>=UINT64_MAX-1u ) {
			R_LOG( rch_ral, SEV_WARN,
				"ral-frame-graph-native-failure reason=backend-or-generation\n" );
			return;
		}
		generation=++s_ral_frame_graph_diagnostic_generation;
		memset(&receipt,0,sizeof(receipt));
		if ( !RalFrameGraphNative_Run(s_ral_backend,generation,&receipt) ) {
			R_LOG( rch_ral, SEV_WARN,
				"ral-frame-graph-native-failure generation=%llu\n",
				(unsigned long long)generation );
			return;
		}
		R_LOG( rch_ral, SEV_INFO,
			"ral-frame-graph-native schema=%u generation=%llu graph=%llu material=%llu batch=%llu recording=%llu submission=%llu native-submission=%llu textures=%u allocations=%u passes=%u submits=%u disjoint=%llu physical=%llu saved=%llu permille=%u timeline=%llu:%llu completed=%d retired=%d ready=%d\n",
			receipt.schemaVersion,(unsigned long long)receipt.generation,
			(unsigned long long)receipt.graphGeneration,
			(unsigned long long)receipt.materializationGeneration,
			(unsigned long long)receipt.batchGeneration,
			(unsigned long long)receipt.recordingGeneration,
			(unsigned long long)receipt.submissionGeneration,
			(unsigned long long)receipt.nativeSubmissionGeneration,
			receipt.textureCount,receipt.allocationCount,receipt.passCount,
			receipt.submissionCount,
			(unsigned long long)receipt.disjointEquivalentCommittedBytes,
			(unsigned long long)receipt.physicalCommittedBytes,
			(unsigned long long)receipt.savedBytes,receipt.savedPermille,
			(unsigned long long)receipt.timelineBaseValue,
			(unsigned long long)receipt.timelineFinalValue,
			(int)receipt.timelineCompleted,(int)receipt.retired,(int)receipt.ready );
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
		R_LOG( rch_ral, SEV_INFO, "    bindless table not built this session; RAL backend remains available.\n" );
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
	R_LOG( rch_ral, SEV_INFO, "  BUFFERS:\n" );
	R_LOG( rch_ral, SEV_INFO,
		"    ownership              : direct Ral_CreateBuffer/Ral_DestroyBuffer\n" );
	R_LOG( rch_ral, SEV_INFO,
		"    native adoption registry: retired (no pending/active compatibility wrappers)\n" );
	R_LOG( rch_ral, SEV_INFO,
		"    accounting authority   : live backend memory budget above\n" );
	R_LOG( rch_ral, SEV_INFO, "===== end \\ral_resources =====\n" );
}

#endif // USE_VULKAN
