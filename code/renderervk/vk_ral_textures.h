// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// vk_ral_textures.h — texture migration support. Owns the
// renderer-side RAL backend instance + the bindless SAMPLED_IMAGE
// BindGroup that R_CreateImage populates as textures are registered.
//
// PARALLEL-PATHS MIGRATION MODEL:
// The RAL backend has its own VkInstance / VkDevice (per the RAL design),
// SEPARATE from code/renderervk/'s qvk* instance/device. RAL VkImage handles
// therefore cannot be used in qvk* descriptor writes (cross-VkDevice handle
// use is invalid per the Vulkan spec). When r_useRALTextures=1, the
// renderer keeps its legacy VkImage on the qvk* VkDevice (driving descriptor
// binding / blits / screenshots / readbacks) AND additionally creates a
// parallel RAL texture on the RAL VkDevice. The parallel RAL texture is
// registered into a renderer-owned bindless BindGroup that sits unused at
// first — a later migration moves descriptor binding onto RAL and consumes the
// bindless table at that point.
//
// Memory cost: ~2x texture allocation across the migration window; arena1
// observed footprint <200 MiB, doubled <400 MiB — trivial on modern HW.

#ifndef WIRED_VK_RAL_TEXTURES_H
#define WIRED_VK_RAL_TEXTURES_H

#ifdef USE_VULKAN

#include "../renderer/ral_vulkan/ral_vulkan_bridge.h"
#include "vk_bindless_publication.h"
#include "vk_bindless_cohort.h"

struct image_s;     // forward — from tr_local.h
struct ralTexture_s;
struct ralBackend_s;
struct ralBindGroup_s;

// Lifecycle — called from vk_initialize / vk_shutdown.
//
// vk_ral_textures_shutdown takes a destroyWindow flag mirroring vk_shutdown's
// signature. When qfalse (REF_LEVEL_ONLY, map-scoped teardown for map
// transitions) the function is a strict no-op past the diag dump — RAL
// backend, sibling pipelines, BGLs, and the active-buffer tracker survive
// across maps. When qtrue (REF_KEEP_WINDOW / REF_DESTROY_WINDOW /
// REF_UNLOAD_DLL) the full invalidate-then-NULL + Ral_DestroyBackend path
// runs.
void     vk_ral_textures_init    ( void );
void     vk_ral_textures_shutdown( qboolean destroyWindow );
qboolean vk_ral_textures_available( void );

// GPU memory budget for the /meminfo GPU section. Fills the byte counts + a
// pressure level (0/1/2); returns qtrue when the numbers are real (else the
// RAL-tracked-footprint estimate). Out-params may be NULL.
qboolean vk_ral_query_memory_budget( uint64_t *dlUsed, uint64_t *dlBudget,
                                     uint64_t *hvUsed, uint64_t *hvBudget,
                                     int *pressureLevel );

// Single-phase RAL bringup. Creates instance +
// messenger + surface + physDev pick + queue family resolve + device
// extension allow-list + feature enable + VkDevice in one Ral_CreateBackend
// call. Runs at vk_initialize entry. The earlier two-phase contract (a
// separate vk_ral_adopt_device) is retired.
qboolean vk_ral_boot_backend( void );

// Symmetric pair to
// vk_ral_backend_init. Destroys the imported-mode RAL backend AFTER every
// consumer has cleaned up its wrappers. Called from vk_shutdown's tail,
// just before qvkDestroyDevice. vk_ral_textures_shutdown (which still
// runs first, from RE_Shutdown) destroys the RAL bindless layout/set +
// the renderer-side RAL pipelines/BGLs but no longer touches the backend
// pointer itself — that final step moved here so the legacy
// Ral_DestroyCommandBuffer(vk.ral_staging_cmd) + Ral_DestroySwapchain
// path in vk_shutdown can still see a live backend.
void vk_ral_backend_shutdown( void );

// Boot-time adoption of every allocate-once
// VkDescriptorSet into a ralBindGroup_t with ownsSet=qfalse. Called from
// vk_init_descriptors's tail after all qvkAllocateDescriptorSets +
// vkUpdateDescriptorSets writes complete. Idempotent: re-init clears the
// registry then re-adopts.
void     vk_ral_adopt_static_bindgroups( void );
// Release every wrapper whose underlying VkDescriptorSet belongs to the
// renderer descriptor pool. Must run before qvkReset/DestroyDescriptorPool.
void     vk_ral_release_static_bindgroups( void );
// Per-slot exact wrapper for the main dynamic UBO descriptor. Geometry-buffer
// replacement destroys it before the registered buffer and refreshes it after
// the native descriptor is rewritten.
qboolean vk_ral_refresh_tess_uniform_bindgroup( uint32_t slot );
void     vk_ral_release_tess_uniform_bindgroup( uint32_t slot );
qboolean vk_ral_refresh_entmat_bindgroup( uint32_t slot );
void     vk_ral_release_entmat_bindgroup( uint32_t slot );

// Per-image legacy sampler-set interop. The native descriptor remains owned by
// the renderer pool; the wrapper provides typed command authority only.
qboolean vk_ral_adopt_image_descriptor( image_t *image );
void     vk_ral_release_image_descriptor( image_t *image );

// VkDescriptorSet → ralBindGroup_t * reverse lookup
// over the adoption registry. Returns NULL when `vkSet` isn't adopted
// (e.g. per-shader-type rotating descriptors deferred to the later
// per-frame adoption). Callers at the ~25 parallel bind-call sites
// guard on non-NULL before binding through the typed singular RAL command.
struct ralBindGroup_s;
struct ralBindGroup_s *vk_ral_lookup_bindgroup( VkDescriptorSet vkSet );

// Reverse-lookup helpers for the typed RAL cmd API.
// Each scans the renderer's existing sibling-field / active-buffer
// registry / vk.pipelines[].ral_handle[] arrays and returns NULL when no
// wrapper exists for the given Vk handle. The typed Ral_Cmd* surface
// null-guards every typed-pointer arg so a NULL return cleanly skips the
// underlying vkCmd* call (parallel-paths-era invariant — legacy qvkCmd*
// stays authoritative until a later migration retires it).
struct ralBuffer_s;
struct ralPipeline_s;
struct ralPipelineLayout_s;
struct ralTexture_s;
struct ralBuffer_s         *vk_ral_lookup_buffer         ( VkBuffer         vkBuf    );
struct ralPipeline_s       *vk_ral_lookup_pipeline       ( VkPipeline       vkPipe   );

// VkImage → adopted-wrapper reverse lookup for the typed image command
// migration. Query pools are native RAL resources and need no reverse lookup.
struct ralTexture_s        *vk_ral_lookup_texture        ( VkImage          vkImage  );

// Boot-time adoption of the renderer's 6 internal-image
// VkImage handles (depth_image / color_image / tonemapped_image + 3 inside
// vk.smaa.{input,edges,blend}_image) into ralTexture_t* sibling fields. Called
// from vk_ral_adopt_static_bindgroups's tail AFTER the render-pass + framebuffer
// adoption sweep. Idempotent on re-init.
//
// The SMAA siblings are gated on vk.fboActive (matching the smaa-image
// alloc lifecycle in vk_smaa_alloc_resources). The full-teardown destroy
// helper mirrors vk_ral_destroy_adopted_render_passes_and_framebuffers.
void vk_ral_adopt_static_internal_textures   ( void );
void vk_ral_destroy_adopted_internal_textures( void );

// Accessor for vk.c::create_pipeline + the 16 special-
// case pipeline create sites. Returns the imported-mode RAL backend pointer
// (shared VkDevice with vk.device) or NULL if RAL bringup didn't fire
// (r_useRALTextures = 0 && r_useRALBuffers = 0 && r_useRALPipelines = 0, or
// imported-mode init failed).
struct ralBackend_s *vk_ral_get_backend( void );

// On-demand single pipeline-layout adoption. The boot-time
// vk_ral_adopt_static_pipeline_layouts() sweep runs before the primitive
// subsystems create their VkPipelineLayouts, so it skips them (NULL guard) and
// their RAL siblings stay NULL. Each primitive calls this from its own init/
// recreate, after its legacy layout exists and before it builds its RAL
// dynamic-rendering pipeline, so externalLayout is a valid, matching layout
// (not NULL → an empty backend-built layout → VUID-…-07987/07988/08600).
// Idempotent: destroys any prior sibling and re-adopts. NULL vkLayout no-ops.
void vk_ral_adopt_one_pipeline_layout( VkPipelineLayout vkLayout,
		struct ralPipelineLayout_s **ralField, const char *label );

// On-demand single texture adoption — for an attachment image created/re-created
// outside the boot-time static-internal-texture sweep (e.g. the supersample-gated
// capture image, re-created on swapchain/r_hdr/r_fbo rebuilds). Adopt at creation,
// KILL the sibling before the VkImage destroy. Idempotent; NULL vkImage no-ops.
void vk_ral_adopt_one_texture( VkImage vkImage, VkImageView vkView, VkFormat fmt,
		struct ralTexture_s **ralField, uint32_t w, uint32_t h, uint32_t aspect,
		ralTextureUsage_t usage, const char *label );

// bindless-ral-consolidate — accessors for the RAL-owned bindless layout + set.
// The layout's binding shape is image-array (binding=0, unbounded) + sampler
// dedup-pool array (binding=1, MAX_VK_SAMPLERS). The renderer slots the layout
// handle into vk.pipeline_layout's set_layouts[WIRED_BINDLESS_SET] and binds
// the set per-draw at the same slot. Both return NULL when the bindless infra
// did not come up (caps not advertised / Ral_CreateBindGroup failure).
struct ralBindGroupLayout_s *vk_ral_get_bindless_layout( void );
struct ralBindGroup_s       *vk_ral_get_bindless_set   ( void );
qboolean vk_ral_bindless_get_cohort(
	vkRalBindlessCohortReceipt_t *outReceipt );

qboolean vk_ral_bindless_publish_texture_view( struct image_s *image,
	uint32_t slot, struct ralTextureView_s *view,
	vkBindlessPublicationKind_t kind );
qboolean vk_ral_bindless_publish_texture_views( struct image_s *const *images,
	const uint32_t *slots, struct ralTextureView_s *const *views,
	const vkBindlessPublicationKind_t *kinds, uint32_t count );
qboolean vk_ral_bindless_publish_texture( struct image_s *image,
	uint32_t slot, struct ralTexture_s *texture,
	vkBindlessPublicationKind_t kind );
qboolean vk_ral_bindless_publish_sampler( uint32_t slot,
	struct ralSampler_s *sampler, const Vk_Sampler_Def *definition );
qboolean vk_ral_bindless_record_legacy_exact( struct image_s *image,
	uint32_t slot, VkImageView view );
qboolean vk_ral_bindless_record_raw_image( struct image_s *image,
	uint32_t slot, VkImageView view, vkBindlessPublicationKind_t kind );
qboolean vk_ral_bindless_record_reserved( uint32_t slot, VkImageView view,
	const void *ownerIdentity, const void *descriptorIdentity );
qboolean vk_ral_bindless_tombstone( uint32_t slot );
qboolean vk_ral_bindless_query_ordinary( const struct image_s *image,
	vkBindlessOrdinaryReceipt_t *outReceipt );
void vk_ral_bindless_sampler_pool_invalidate( void );

// Per-image registration. `pic` is the original RGBA8 source (after any
// up-front resampling done by R_CreateImageArray's caller); a NULL pic
// causes the RAL parallel texture to be skipped for that image (typical for
// scratch / placeholder / dynamic update textures, which this migration
// doesn't cover). `width`/`height` are the renderer's pre-upload_vk_image
// dimensions — RAL receives original dimensions and runs its own GPU
// mip-gen (visual output of the renderer is unaffected because rendering
// still samples the legacy VkImage).
void vk_ral_register_image  ( struct image_s *image, byte *pic, int width, int height );
void vk_ral_unregister_image( struct image_s *image );

// Phase 7.15.4-b reversibility leg: restore an evicted-but-still-registered image
// (image->ral == NULL) by re-decoding its source from disk (image->imgName) and
// re-creating BOTH the RAL bindless texture and the legacy VkImage, mirroring
// R_CreateImage's pairing. Idempotent (no-op if already resident); skips + warns on
// a pinned image (must never be evicted); graceful on decode-fail (left non-resident
// → bind sentinel-declines). Defined in tr_image.c (where the static R_LoadImage +
// upload_vk_image live). Called by the step-b forced-evict round-trip test and,
// later, step-c eviction / 7.15.3 bind-miss.
void vk_ral_reregister_image( struct image_s *image );

// Assign a bindless slot to a DDS (BC*/packed) image that cannot go through
// vk_ral_register_image's RGBA8 path. Uses the SAME free-list allocator + the
// SAME over-capacity bookkeeping as the main path, so there is genuinely ONE
// slot-index source (eviction slot-recycle in 7.15.4 will not collide with a
// raw tr.numImages-1 index). DDS keeps its own vk_create_image +
// vk_upload_dds_image_data path; only the slot SOURCE is shared — the actual
// descriptor write happens later via vk_update_descriptor_set with the real
// DDS-format view. Call BEFORE vk_create_image (the descriptor write depends on
// image->ralBindlessSlot already being set). Phase 7.15.2-fix.
void vk_ral_assign_dds_slot( struct image_s *image );

// Reset the 2D bindless slot allocator (free-list + high-water) to empty/0.
// Called by R_DeleteTextures after the bulk unregister loop: when tr.numImages
// is zeroed and the whole texture set is torn down, all slots are conceptually
// freed and the next registration pass must restart at slot 0 — matching the
// old tr.numImages-1 source. Without this the bulk unregister fills the
// free-list and re-registration would pop recycled slots (reordering which
// texture lands in which slot). Phase 7.15.2.
void vk_ral_reset_bindless_slots( void );

// ── Phase 7.15.4-c automatic pressure-driven eviction (Option-A threading) ──
// Tunables (modder-code-level #defines, not user cvars). Derived from the poller's
// thresholds (ralVk_LevelOf: WARNING 75 % / CRITICAL 90 %): on CRITICAL the render
// thread evicts oldest-unpinned textures down to TARGET ‰ of the device-local
// budget (700 ‰ = 70 %, a margin below the 75 % WARNING line so the next poll does
// not immediately re-fire — anti-thrash), capping the per-frame batch so a large
// over-budget never stalls one frame (the flag re-arms on the next CRITICAL
// transition if more is needed).
#define WIRED_TEX_EVICT_TARGET_PERMILLE   700
#define WIRED_TEX_EVICT_MAX_PER_FRAME     64

// Phase 7.15.3 bind-miss handler: max sampled-evicted textures auto-re-streamed per
// frame (spreads a mass-resample over frames; a still-pending texture stays
// white-sentinel until its turn). Modder-code-level tunable, not a user cvar.
#define WIRED_TEX_REREGISTER_MAX_PER_FRAME 64
#define WIRED_TEX_REREGISTER_MAX_BYTES_PER_FRAME ( 32u * 1024u * 1024u )

// Option-A cross-thread flag accessors. Poll thread (single producer) sets the
// flag on CRITICAL via vk_ral_on_memory_pressure; render thread (single consumer)
// reads + clears it in vk_ral_drain_evictions. The flag is the ONLY cross-thread
// shared state for eviction — the poll thread never touches RAL / tr.images[] /
// the free-list (that would be the unsafe Option-B).
qboolean vk_ral_evict_requested( void );
void     vk_ral_clear_evict_request( void );
void     vk_ral_request_eviction( void );   // render-thread test hook (r_texEvictForce)

// Default-inert parent-view integration gate. `restore=qfalse` deterministically
// selects a live mipmapped image and holds baseMip=1 in its existing bindless
// slot; `restore=qtrue` promotes the same page identity back to its full view.
qboolean vk_ral_residency_mip_test( qboolean restore );
const char *vk_ral_residency_mip_test_source( int *width, int *height );
qboolean vk_ral_residency_mip_upload( const byte *pic, int width, int height );
qboolean vk_ral_residency_material_test( qboolean restore );
const char *vk_ral_residency_material_source( int plane, int *width, int *height );
qboolean vk_ral_residency_material_upload( byte *const pics[2], const int widths[2], const int heights[2] );

// Synthetic-pressure test override (default 0 = OFF). Set by r_texEvictPressureTest
// so the automatic drain can be exercised without real CRITICAL pressure (which
// never occurs at ~4 % usage). When non-zero, the next vk_ral_drain_evictions uses
// target = (used - N MiB) instead of the real 70 % threshold, then clears it.
void     vk_ral_set_evict_test_drop_mib( unsigned mib );
uint64_t vk_ral_get_evict_test_drop_bytes( void );
void     vk_ral_clear_evict_test_drop( void );

// Render-thread per-frame eviction drain — the DESTROY half of Option-A. Called at
// the per-frame safe boundary (after vk_ral_drain_pending_uploads). No-op unless the
// poll thread raised the flag. Defined in tr_image.c (where the victim-scan +
// dual-free + tr.images[] live). Phase 7.15.4-c.
void vk_ral_drain_evictions( void );

// Render-thread per-frame bind-miss recovery (Phase 7.15.3). Called at the per-frame
// safe boundary right after vk_ral_drain_evictions. Re-streams every evicted texture
// marked IMGFLAG_REREGISTER_PENDING (set in vk_bindless_track when sampled), via
// vk_ral_reregister_image, capped per frame. No-op when nothing is pending. Defined
// in tr_image.c (where vk_ral_reregister_image + tr.images[] live).
void vk_ral_drain_reregisters( void );

// Per-frame residency drain for async texture uploads: swaps completed uploads from
// the placeholder to the real texture in the bindless set. Call once per frame.
void     vk_ral_drain_pending_uploads( void );
qboolean vk_ral_pending_uploads_active( void );

// Cumulative count of texture uploads by path (synchronous resident-on-return vs async
// deferred-residency) since init. Used by the map-load timer to report the load's
// sync/async split. Either out-pointer may be NULL.
void     vk_ral_upload_counts( uint32_t *syncOut, uint32_t *asyncOut );

// Slot allocator for the parallel SAMPLED_IMAGE binding at
// WIRED_BINDLESS_BIND_ARRAY_IMAGES (set 7, binding=2). Returns a slot in
// the disjoint 2DArray slot-index space, or -1 when the bounded capacity
// (WIRED_BINDLESS_ARRAY_TEX_SLOTS) is exhausted. The actual descriptor
// write lives in vk.c::vk_ral_register_image_array — the raw
// qvkUpdateDescriptorSets handle is static there. Callers store the
// returned slot in image_t::ralBindlessSlot; the field is shared with the
// 2D path, but the disjoint binding indices keep the spaces separate, and
// the consuming shader's macro (WIRED_BINDLESS_TEX vs
// WIRED_BINDLESS_TEX_ARRAY) picks which to read.
int vk_ral_alloc_array_bindless_slot( const char *imgName );

// Exact native-buffer adoption bridge. Each legacy vkCreateBuffer
// site in vk.c calls vk_ral_register_buffer right after qvkBindBufferMemory;
// the matching qvkDestroyBuffer is preceded by vk_ral_unregister_buffer.
// `key` is both the lookup key and the exact native identity borrowed by the
// non-owning RAL wrapper; no second GPU buffer is allocated and raw parent
// destruction remains renderer-owned.
//
// Backend-availability handling: register calls made before
// vk_ral_textures_init has brought up the persistent RAL backend (which
// runs from R_InitImages after ri.FreeAll) are queued in a static pending
// list and flushed when the backend comes up. This covers the ~13
// vk_initialize-time create sites (staging, tess, storage, ribbon, beam,
// sprite, particle, primitive, IQM bone, world VBO) which all create
// buffers before R_InitImages fires. Per-map / per-frame / lazy-growth
// sites are registered after the backend is live and skip the queue.
//
// Helpers translate the legacy VkBufferUsageFlags + VkMemoryPropertyFlags
// internally and retain those exact portable capabilities in the wrapper.
void vk_ral_register_buffer  ( VkBuffer key, uint64_t size,
                               VkBufferUsageFlags vkUsage,
                               VkMemoryPropertyFlags vkMemProps,
                               const char *debugName );
void vk_ral_unregister_buffer( VkBuffer key );

// Diagnostic — used by the \ral_resources developer command. Auto-fires
// at vk_ral_textures_shutdown so the bindless texture + RAL buffer state
// is captured in the log even when the cli `+cmd` dispatch path doesn't
// surface renderer-DLL-registered commands (engine quirk).
void vk_ral_textures_diag_dump( void );

#endif // USE_VULKAN

#endif // WIRED_VK_RAL_TEXTURES_H
