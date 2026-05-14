// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// vk_ral_textures.h — Phase 7.4a texture migration support. Owns the
// renderer-side RAL backend instance + the bindless SAMPLED_IMAGE
// BindGroup that R_CreateImage populates as textures are registered.
//
// PARALLEL-PATHS MIGRATION MODEL (7.4a):
// The RAL backend has its own VkInstance / VkDevice (per the RAL design),
// SEPARATE from code/renderervk/'s qvk* instance/device. RAL VkImage handles
// therefore cannot be used in qvk* descriptor writes (cross-VkDevice handle
// use is invalid per the Vulkan spec). In 7.4a, when r_useRALTextures=1, the
// renderer keeps its legacy VkImage on the qvk* VkDevice (driving descriptor
// binding / blits / screenshots / readbacks) AND additionally creates a
// parallel RAL texture on the RAL VkDevice. The parallel RAL texture is
// registered into a renderer-owned bindless BindGroup that sits unused in
// 7.4a — 7.4c will migrate descriptor binding onto RAL and consume the
// bindless table at that point.
//
// Memory cost: ~2x texture allocation across the migration window; arena1
// observed footprint <200 MiB, doubled <400 MiB — trivial on modern HW.

#ifndef WIRED_VK_RAL_TEXTURES_H
#define WIRED_VK_RAL_TEXTURES_H

#ifdef USE_VULKAN

struct image_s;     // forward — from tr_local.h
struct ralTexture_s;
struct ralBackend_s;
struct ralBindGroup_s;

// Lifecycle — called from vk_initialize / vk_shutdown.
//
// Phase 7.4c-pipeline-followup-5 PART 2.5 — vk_ral_textures_shutdown takes a
// destroyWindow flag mirroring legacy vk_shutdown's signature. When qfalse
// (REF_KEEP_CONTEXT, level-scoped teardown for map transitions) the function
// is a strict no-op past the diag dump — RAL backend, sibling pipelines, BGLs,
// and the active-buffer tracker survive across maps the same way legacy
// vk.device / vk.gamma_pipeline / etc. do. When qtrue (REF_UNLOAD_DLL / final
// exit) the full PART 0 invalidate-then-NULL + Ral_DestroyBackend path runs.
// Interim: Phase 7.4d-map-arena retires REF_KEEP_CONTEXT entirely; after that
// turn the flag becomes always-qtrue and the gate can be removed.
void     vk_ral_textures_init    ( void );
void     vk_ral_textures_shutdown( qboolean destroyWindow );
qboolean vk_ral_textures_available( void );

// Phase 7.4c-submit-followup-present-2-fix2 — bring up the imported-mode
// RAL backend ahead of any consumer. Hoisted out of vk_ral_textures_init's
// head so vk_create_swapchain (a NEW consumer added in present-2) can rely
// on vk_ral_get_backend() being non-NULL. Called from vk_initialize
// immediately after init_vulkan_library() returns + vk.instance/device/
// queue-family fields are populated. Returns qtrue on success or when the
// backend is already up (idempotent); qfalse + SEV_ERROR log on failure.
qboolean vk_ral_backend_init( void );

// Phase 7.4c-submit-followup-present-2-fix3 — symmetric pair to
// vk_ral_backend_init. Destroys the imported-mode RAL backend AFTER every
// consumer has cleaned up its wrappers. Called from vk_shutdown's tail,
// just before qvkDestroyDevice. vk_ral_textures_shutdown (which still
// runs first, from RE_Shutdown) destroys the RAL bindless layout/set +
// the renderer-side RAL pipelines/BGLs but no longer touches the backend
// pointer itself — that final step moved here so the legacy
// Ral_DestroyCommandBuffer(vk.ral_staging_cmd) + Ral_DestroySwapchain
// path in vk_shutdown can still see a live backend.
void vk_ral_backend_shutdown( void );

// Phase 7.4c-bindgroup — boot-time adoption of every allocate-once
// VkDescriptorSet into a ralBindGroup_t with ownsSet=qfalse. Called from
// vk_init_descriptors's tail after all qvkAllocateDescriptorSets +
// vkUpdateDescriptorSets writes complete. Idempotent: re-init clears the
// registry then re-adopts.
void     vk_ral_adopt_static_bindgroups( void );

// Phase 7.4c-bindgroup — VkDescriptorSet → ralBindGroup_t * reverse lookup
// over the adoption registry. Returns NULL when `vkSet` isn't adopted
// (e.g. per-shader-type rotating descriptors deferred to 7.4c-cmd's
// per-frame adoption). Callers at the ~25 parallel bind-call sites
// guard on non-NULL before calling Ral_CmdBindBindGroups.
struct ralBindGroup_s;
struct ralBindGroup_s *vk_ral_lookup_bindgroup( VkDescriptorSet vkSet );

// Phase 7.4c-submit-A2 — reverse-lookup helpers for the typed RAL cmd API.
// Each scans the renderer's existing sibling-field / 7.4b active-buffer
// registry / vk.pipelines[].ral_handle[] arrays and returns NULL when no
// wrapper exists for the given Vk handle. The typed Ral_Cmd* surface
// null-guards every typed-pointer arg so a NULL return cleanly skips the
// underlying vkCmd* call (parallel-paths-era invariant — legacy qvkCmd*
// stays authoritative until 7.4c-submit-BC retires it).
struct ralBuffer_s;
struct ralPipeline_s;
struct ralPipelineLayout_s;
struct ralTexture_s;
struct ralQueryPool_s;
struct ralBuffer_s         *vk_ral_lookup_buffer         ( VkBuffer         vkBuf    );
struct ralPipeline_s       *vk_ral_lookup_pipeline       ( VkPipeline       vkPipe   );
struct ralPipelineLayout_s *vk_ral_lookup_pipeline_layout( VkPipelineLayout vkLayout );
struct ralRenderPass_s     *vk_ral_lookup_render_pass    ( VkRenderPass     vkRp     );
struct ralFramebuffer_s    *vk_ral_lookup_framebuffer    ( VkFramebuffer    vkFb     );

// Phase 7.4c-submit-A4 — VkImage / VkQueryPool → adopted-wrapper reverse-lookup
// for the typed Ral_Cmd{PipelineBarrierFull,CopyImage,WriteTimestamp,ResetQueryPool}
// migration. Returns NULL when the lookup target wasn't adopted (e.g. transient
// images / a VkQueryPool other than the renderer's vk_gpu_ts_pool). Parallel-
// paths NULL-fallthrough contract: callers either short-circuit with a SEV_WARN
// log on miss, or skip the typed RAL call and rely on the legacy qvkCmd* path
// staying authoritative.
struct ralTexture_s        *vk_ral_lookup_texture        ( VkImage          vkImage  );
struct ralQueryPool_s      *vk_ral_lookup_query_pool     ( VkQueryPool      vkPool   );

// Phase 7.4c-submit-A4 — boot-time adoption of the renderer's 6 internal-image
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

// Phase 7.4c-pipeline: accessor for vk.c::create_pipeline + the 16 special-
// case pipeline create sites. Returns the imported-mode RAL backend pointer
// (shared VkDevice with vk.device) or NULL if RAL bringup didn't fire
// (r_useRALTextures = 0 && r_useRALBuffers = 0 && r_useRALPipelines = 0, or
// imported-mode init failed).
struct ralBackend_s *vk_ral_get_backend( void );

// Per-image registration. `pic` is the original RGBA8 source (after any
// up-front resampling done by R_CreateImageArray's caller); a NULL pic
// causes the RAL parallel texture to be skipped for that image (typical for
// scratch / placeholder / dynamic update textures, which 7.4a doesn't
// migrate). `width`/`height` are the renderer's pre-upload_vk_image
// dimensions — RAL receives original dimensions and runs its own GPU
// mip-gen (visual output of the renderer is unaffected because rendering
// still samples the legacy VkImage).
void vk_ral_register_image  ( struct image_s *image, byte *pic, int width, int height );
void vk_ral_unregister_image( struct image_s *image );

// Phase 7.4b — parallel-paths buffer migration. Each legacy vkCreateBuffer
// site in vk.c calls vk_ral_register_buffer right after qvkBindBufferMemory;
// the matching qvkDestroyBuffer is preceded by vk_ral_unregister_buffer.
// `key` is the legacy VkBuffer handle, used to index the parallel RAL
// buffer in the tracking table (so callers don't need a sibling field on
// every struct). Cvar-gated by r_useRALBuffers — when 0 the calls are
// no-ops, identical to pre-7.4b behaviour.
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
// internally — callers in vk.c pass exactly the values they hand to
// vkCreateBuffer / vkAllocateMemory and let the helper map to the RAL
// enums. Keeps the renderer .c files free of an ral_resource.h dependency.
void vk_ral_register_buffer  ( VkBuffer key, uint64_t size,
                               VkBufferUsageFlags vkUsage,
                               VkMemoryPropertyFlags vkMemProps,
                               const char *debugName );
void vk_ral_unregister_buffer( VkBuffer key );

// Diagnostic — used by the \ral_resources developer command. Auto-fires
// at vk_ral_textures_shutdown so the bindless texture + RAL buffer state
// is captured in the log even when the cli `+cmd` dispatch path doesn't
// surface renderer-DLL-registered commands (engine quirk; see 7.4a report).
void vk_ral_textures_diag_dump( void );

#endif // USE_VULKAN

#endif // WIRED_VK_RAL_TEXTURES_H
