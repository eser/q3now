# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

file(READ "${ROOT}/code/renderervk/vk_temporal_resolved_hdr.h" HDR_H)
file(READ "${ROOT}/code/renderervk/vk_temporal_resolved_hdr.c" HDR)
file(READ "${ROOT}/code/renderervk/vk.c" VKC)
file(READ "${ROOT}/code/renderervk/vk_ral_textures.c" RAL_TEX)
file(READ "${ROOT}/code/renderervk/tr_backend.c" BACKEND)
file(READ "${ROOT}/code/renderervk/tr_init.c" TR_INIT)
file(READ "${ROOT}/tests/vk_temporal_resolved_hdr_test.c" TEST)
file(READ "${ROOT}/CMakeLists.txt" BUILD)

function(require_text var needle why)
	string(FIND "${${var}}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "H2a policy missing ${why}: ${needle}")
	endif()
endfunction()
function(forbid_text var needle why)
	string(FIND "${${var}}" "${needle}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "H2a policy forbidden ${why}: ${needle}")
	endif()
endfunction()
function(require_count var regex expected why)
	string(REGEX MATCHALL "${regex}" hits "${${var}}")
	list(LENGTH hits count)
	if(NOT count EQUAL expected)
		message(FATAL_ERROR "H2a policy ${why}: expected ${expected}, got ${count}")
	endif()
endfunction()
function(require_order_in_span var start_marker end_marker first second why)
	string(FIND "${${var}}" "${start_marker}" span_start)
	string(FIND "${${var}}" "${end_marker}" span_end)
	if(span_start EQUAL -1 OR span_end EQUAL -1 OR span_end LESS_EQUAL span_start)
		message(FATAL_ERROR "H2a policy cannot isolate ${why}")
	endif()
	math(EXPR span_length "${span_end} - ${span_start}")
	string(SUBSTRING "${${var}}" ${span_start} ${span_length} span)
	string(FIND "${span}" "${first}" first_pos)
	string(FIND "${span}" "${second}" second_pos)
	if(first_pos EQUAL -1 OR second_pos EQUAL -1 OR second_pos LESS_EQUAL first_pos)
		message(FATAL_ERROR "H2a policy order drift in ${why}: ${first} < ${second}")
	endif()
endfunction()

function(require_count_in_span var start_marker end_marker regex expected why)
	string(FIND "${${var}}" "${start_marker}" span_start)
	string(FIND "${${var}}" "${end_marker}" span_end)
	if(span_start EQUAL -1 OR span_end EQUAL -1 OR span_end LESS_EQUAL span_start)
		message(FATAL_ERROR "H2a policy cannot isolate ${why}")
	endif()
	math(EXPR span_length "${span_end} - ${span_start}")
	string(SUBSTRING "${${var}}" ${span_start} ${span_length} span)
	string(REGEX MATCHALL "${regex}" hits "${span}")
	list(LENGTH hits count)
	if(NOT count EQUAL expected)
		message(FATAL_ERROR "H2a policy ${why}: expected ${expected}, got ${count}")
	endif()
endfunction()

foreach(field IN ITEMS backend currentSceneColor currentPostprocessGroup
		currentHistogramGroup postprocessLayout histogramLayout histogramBuffer
		worldIndex width height topologyEpoch planGeneration
		sceneColorAttachmentGeneration sceneFormat filter)
	require_text(HDR_H "${field}" "exact input/receipt field ${field}")
endforeach()
foreach(field IN ITEMS batchToken frameId contentSerial commandSlot frameCount
		sourceSceneColor sourcePostprocessGroup sourceHistogramGroup
		targetAllocationGeneration)
	require_text(HDR_H "${field}" "future content authority ${field}")
endforeach()
require_text(HDR "input->sceneFormat != RAL_FORMAT_R16G16B16A16_SFLOAT"
	"exact HDR format admission")
foreach(bit IN ITEMS STORAGE SAMPLED COLOR_ATTACHMENT TRANSFER_DST TRANSFER_SRC)
	require_text(HDR "RAL_TEXTURE_USAGE_${bit}" "exact target usage ${bit}")
endforeach()
require_text(HDR "tci.depthOrArrayLayers = 1;" "one layer")
require_text(HDR "tci.mipLevels = 1;" "one mip")
require_text(HDR "tci.sampleCount = 1;" "one sample")
string(FIND "${HDR}" "if ( ExactKeyEqual( owner, input ) ) return qtrue;" exact_hit)
string(FIND "${HDR}" "caps = Ral_GetCaps( input->backend );" caps_query)
if(exact_hit LESS 0 OR caps_query LESS exact_hit)
	message(FATAL_ERROR "H2a policy: same-key reuse must precede caps/format queries")
endif()
require_text(HDR "candidate.ready = qtrue;\n\tDestroyHandles( owner );\n\t*owner = candidate;"
	"candidate-first publication")
require_text(HDR "DestroyCandidate( &candidate, owner, input );"
	"live/borrowed-safe rollback")
require_text(HDR "owner->allocationGeneration == UINT32_MAX" "generation saturation")
require_text(HDR "generation = owner->allocationGeneration;" "release generation preservation")

require_text(HDR "content->batchToken == current->batchToken" "batch match")
require_text(HDR "content->commandSlot == current->commandSlot" "slot match")
require_text(HDR "content->contentSerial" "content serial")
require_text(HDR "target->sceneColorAttachmentGeneration ==\n\t\t\tcurrent->sceneColorAttachmentGeneration"
	"current/target generation match")
require_text(HDR "target->backend == current->backend" "current/target backend match")
require_text(HDR "target->sceneFormat == current->sceneFormat" "current/target format match")
require_text(HDR "target->currentPostprocessGroup == current->postprocessGroup"
	"postprocess fallback identity")
require_text(HDR "target->currentHistogramGroup == current->histogramGroup"
	"histogram fallback identity")

foreach(token IN ITEMS Ral_Cmd Ral_BeginRendering Ral_EndRendering
		Ral_CreatePipeline Ral_CreateShader Cvar CmdBind Draw Dispatch CopyImage)
	forbid_text(HDR "${token}" "resolve/render authority")
endforeach()

require_text(VKC "static uint32_t vk_scene_color_attachment_generation_counter;"
	"monotonic scene generation authority")
require_text(VKC "static uint32_t vk_scene_color_attachment_generation;"
	"active scene generation publication")
require_count(VKC "\\+\\+vk_scene_color_attachment_generation_counter" 1
	"single adopted-cohort generation publication")
require_count(VKC "vk_scene_color_attachment_generation = 0" 3
	"publish-decline plus raw create/destroy invalidation authorities")
require_count_in_span(VKC
	"static void vk_create_attachments( void )\n{"
	"static void vk_create_framebuffers( void )\n{"
	"vk_scene_color_attachment_generation = 0" 1
	"single raw-create invalidation")
require_count_in_span(VKC
	"static void vk_destroy_attachments( void )\n{"
	"static void vk_destroy_render_passes( void )\n{"
	"vk_scene_color_attachment_generation = 0" 1
	"single raw-destroy invalidation")
require_text(VKC "!vk.color_image || !vk.color_image_view || !vk.ral_color_image"
	"raw/adopted scene identity gate")
require_text(VKC "!vk.ral_histogram_bgl || !vk.ral_histogram_buffer"
	"histogram cohort gate")
string(FIND "${RAL_TEX}" "vk_hdr_histogram_init( s_ral_backend );" histogram_init)
string(FIND "${RAL_TEX}" "vk_update_attachment_descriptors();" descriptors_ready)
string(FIND "${RAL_TEX}" "vk_temporal_scene_color_attachment_published();" generation_publish)
if(histogram_init LESS 0 OR descriptors_ready LESS histogram_init
		OR generation_publish LESS descriptors_ready)
	message(FATAL_ERROR "H2a policy: histogram/current groups must precede scene generation publication")
endif()

require_count(VKC "VK_TemporalResolvedHdrInit\\(" 1 "sole active initialization")
require_count(VKC "VK_TemporalResolvedHdrEnsureAfterFence\\(" 1 "sole product Ensure")
require_count(VKC "VK_TemporalResolvedHdrGetReceipt\\(" 3
	"diagnostic arm, prepare and H3 submit must each refetch the live target receipt")
require_count(VKC "VK_TemporalResolvedHdrRouteSource\\(" 4
	"prepare fallback, copy, H3 and Store exact route joins")
require_count(VKC "VK_TemporalResolvedHdrBuildContentReceipt\\(" 1
	"sole post-copy content author")
require_count(VKC "VK_TemporalResolvedHdrResolveContentSubmit\\(" 2
	"seed prepromotion plus non-history fallback-copy prepromotion")
require_text(VKC "vk.color_format == VK_FORMAT_R16G16B16A16_SFLOAT"
	"current scene format match")
require_text(VKC "resolvedInput.width = temporalRequest->width;" "exact request width")
require_text(VKC "resolvedInput.height = temporalRequest->height;" "exact request height")
require_text(VKC "resolvedInput.currentSceneColor = vk.ral_color_image;"
	"current texture identity")
require_text(VKC "resolvedInput.currentPostprocessGroup =" "current postprocess field")
require_text(VKC "vk.ral_color_descriptor;" "current postprocess group")
require_text(VKC "resolvedInput.currentHistogramGroup =" "current histogram field")
require_text(VKC "vk.ral_histogram_descriptor;" "current histogram group")
require_text(VKC "resolvedInput.filter = vk.blitFilter == GL_LINEAR"
	"product GL filter translation")

string(FIND "${VKC}" "void vk_begin_frame(" frame_start)
string(SUBSTRING "${VKC}" ${frame_start} 30000 FRAME)
string(FIND "${FRAME}" "R_TemporalBatchRequestValidateExact( temporalRequest )" exact_request)
string(FIND "${FRAME}" "temporalRequest->enabled && temporalCapacityRequested" enabled_gate)
string(FIND "${FRAME}" "VK_TemporalEntMatRuntimeEnsureAfterFence(" a2a)
string(FIND "${FRAME}" "VK_TemporalMotionMaterializationEnsureAfterFence(" a2b)
string(FIND "${FRAME}" "VK_TemporalResolvedHdrEnsureAfterFence(" h2a)
string(FIND "${FRAME}" "Ral_AcquireNextImage" acquire)
if(exact_request LESS 0 OR enabled_gate LESS exact_request OR a2a LESS enabled_gate
		OR a2b LESS a2a OR h2a LESS a2b OR acquire LESS h2a)
	message(FATAL_ERROR "H2a policy: exact enabled full target < A2a < A2b < H2a < acquire drift")
endif()

require_text(VKC "if ( !vk_temporal_resolved_hdr.initialized"
	"cold-OFF local release guard")
require_text(VKC "if ( !vk_temporal_resolved_hdr.initialized )"
	"Init active guard")
foreach(reason IN ITEMS HDR-attachment-rebuild FBO-attachment-rebuild
		swapchain-attachment-rebuild scene-depth-attachment-rebuild
		full-shutdown pre-RAL-shutdown disabled-exact-request)
	require_text(VKC "${reason}" "release authority ${reason}")
endforeach()
require_text(VKC "VK_TemporalResolvedHdrReleaseAfterIdle(" "idle-proven release")
require_text(VKC "Ral_WaitIdleAndDrainDeferred( vk_ral_get_backend() )"
	"deferred drain before borrowed/raw parents")

# Every raw scene-attachment replacement must prove global idle before the
# H2a child release, and must finish that release/drain before destroying the
# borrowed current-scene parent.  Pin the individual rebuild authorities so
# merely retaining the reason strings cannot make a reordered teardown pass.
require_order_in_span(VKC
	"static void vk_rebuild_fbo_for_hdr_change( void )"
	"static void vk_rebuild_for_fbo_change( void )"
	"vk_wait_idle();"
	"vk_temporal_resolved_hdr_release_after_idle( \"HDR-attachment-rebuild\" );"
	"HDR rebuild idle-before-release")
require_order_in_span(VKC
	"static void vk_rebuild_fbo_for_hdr_change( void )"
	"static void vk_rebuild_for_fbo_change( void )"
	"vk_temporal_resolved_hdr_release_after_idle( \"HDR-attachment-rebuild\" );"
	"vk_destroy_attachments();"
	"HDR rebuild child-before-parent")
require_order_in_span(VKC
	"static void vk_rebuild_for_fbo_change( void )"
	"static qboolean vk_show_ao_source_ready( void )"
	"vk_wait_idle();"
	"vk_temporal_resolved_hdr_release_after_idle( \"FBO-attachment-rebuild\" );"
	"FBO rebuild idle-before-release")
require_order_in_span(VKC
	"static void vk_rebuild_for_fbo_change( void )"
	"static qboolean vk_show_ao_source_ready( void )"
	"vk_temporal_resolved_hdr_release_after_idle( \"FBO-attachment-rebuild\" );"
	"vk_destroy_attachments();"
	"FBO rebuild child-before-parent")
require_order_in_span(VKC
	"static void vk_restart_swapchain( const char *funcname, VkResult res )"
	"static void vk_set_render_scale( void )"
	"vk_wait_idle();"
	"vk_temporal_resolved_hdr_release_after_idle(\n\t\t\"swapchain-attachment-rebuild\" );"
	"swapchain rebuild idle-before-release")
require_order_in_span(VKC
	"static void vk_restart_swapchain( const char *funcname, VkResult res )"
	"static void vk_set_render_scale( void )"
	"vk_temporal_resolved_hdr_release_after_idle(\n\t\t\"swapchain-attachment-rebuild\" );"
	"vk_destroy_attachments();"
	"swapchain rebuild child-before-parent")
require_order_in_span(VKC
	"static void vk_create_attachments( void )\n{"
	"static void vk_create_framebuffers( void )\n{"
	"vk_scene_color_attachment_generation = 0;"
	"vk_clear_attachment_pool();"
	"raw scene create invalidates publication first")
require_order_in_span(VKC
	"static void vk_destroy_attachments( void )\n{"
	"static void vk_destroy_render_passes( void )\n{"
	"vk_scene_color_attachment_generation = 0;"
	"if ( vk.bloom_image[0] )"
	"raw scene destroy invalidates publication first")
require_order_in_span(VKC
	"void vk_shutdown( refShutdownCode_t code )"
	"void vk_wait_idle( void )"
	"vk_wait_idle();"
	"vk_temporal_resolved_hdr_release_after_idle( \"full-shutdown\" );"
	"full shutdown idle-before-release")
require_order_in_span(VKC
	"void vk_shutdown( refShutdownCode_t code )"
	"void vk_wait_idle( void )"
	"vk_temporal_resolved_hdr_release_after_idle( \"full-shutdown\" );"
	"vk_destroy_attachments();"
	"full shutdown child-before-parent")
require_order_in_span(TR_INIT
	"void RE_Shutdown( refShutdownCode_t code )"
	"void RE_EndRegistration( void )"
	"vk_temporal_motion_release_before_ral_shutdown();"
	"vk_ral_textures_shutdown( code != REF_LEVEL_ONLY );"
	"pre-RAL H2a release-before-borrowed-parent teardown")

string(FIND "${FRAME}" "vk_wait_idle();" depth_idle)
string(FIND "${FRAME}" "vk_temporal_resolved_hdr_release_after_idle(\n\t\t\t\"scene-depth-attachment-rebuild\" );" depth_release)
string(FIND "${FRAME}" "vk_ral_destroy_adopted_internal_textures();" depth_static)
string(FIND "${FRAME}" "vk_destroy_attachments();" depth_parent)
if(depth_idle LESS 0 OR depth_release LESS depth_idle
		OR depth_static LESS depth_release
		OR depth_parent LESS depth_static)
	message(FATAL_ERROR "H2a policy: idle < H3/H1/Store/H2 release+drain < adopted children < raw parent drift")
endif()

# H2b chooses one command-local cohort and every downstream HDR consumer uses
# that same route.  Missing/stale content keeps the exact legacy members.
require_count(VKC "postRoute \\? postRoute->postprocessGroup : vk\\.ral_color_descriptor" 2
	"bloom-extract plus tonemap atomic source routing")
require_count(VKC "postRoute \\? postRoute->histogramGroup : vk\\.ral_histogram_descriptor" 1
	"histogram atomic source routing")
require_order_in_span(VKC
	"void vk_begin_post_bloom_render_pass( void )"
	"void vk_tonemap( void )"
	"ri.colorAttachments[0] = postRoute ? postRoute->attachment : vk.ral_color_image;"
	"Ral_BeginRendering( vk.cmd->ral_cmd, &ri );"
	"post-bloom atomic route attachment")
require_text(VKC "VK_DYN_PASS_TEMPORAL_POST_BLOOM" "resolved post-bloom end authority")
require_text(VKC "if ( r_vkApplePinkBarrier->integer && !postRoute )"
	"legacy-only Apple raw-scene barrier")
require_order_in_span(BACKEND
	"static void RB_TransitionToUI( void )"
	"static const void *RB_StretchPic"
	"vk_temporal_motion_seal_primary();"
	"vk_temporal_recursive_record();"
	"seal before shared H4 transaction")
require_order_in_span(BACKEND
	"static void RB_TransitionToUI( void )"
	"static const void *RB_StretchPic"
	"vk_temporal_recursive_record();"
	"vk_bloom();"
	"shared H4 producer/Store transaction before bloom")
require_order_in_span(BACKEND
	"static void RB_TransitionToUI( void )"
	"static const void *RB_StretchPic"
	"vk_temporal_recursive_record();"
	"vk_tonemap();"
	"shared H4 producer/Store transaction before tonemap")
require_order_in_span(VKC
	"void vk_temporal_resolved_hdr_record_copy( void )"
	"static qboolean vk_temporal_resolved_hdr_resolve_submit("
	"VK_TemporalResolvedHdrBuildContentReceipt("
	"Ral_CmdCopyImage("
	"validate-before-copy")
require_text(VKC "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL" "current transfer source")
require_text(VKC "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL" "resolved transfer destination")
require_count_in_span(VKC
	"void vk_temporal_resolved_hdr_record_copy( void )"
	"static qboolean vk_temporal_resolved_hdr_resolve_submit("
	"Ral_CmdCopyImage\\(" 1 "one full-extent copy")
require_text(VKC "vk_temporal_resolved_hdr_resolve_submit(\n\t\t\t\tsubmitResult == ralSuccess"
	"submit-bound content promotion")
require_text(VKC "qfalse, qfalse, NULL );"
	"discard/no-submit cancellation")
require_text(VKC "if ( vk_temporal_resolved_hdr_frame.prepared\n\t\t\t|| vk_temporal_resolved_hdr_frame.copied )\n\t\tvk_temporal_resolved_hdr_reset_frame();"
	"cold-frame reset guard")
require_text(VKC "(void)vk_temporal_resolved_hdr_resolve_submit(\n\t\t\t\tqfalse, qfalse, NULL );"
	"discard resolver guard")
foreach(member IN ITEMS target targetView postprocessGroup histogramGroup)
	forbid_text(VKC "vk_temporal_resolved_hdr.${member}"
		"direct owner-member consumer ${member}")
endforeach()

file(GLOB PRODUCT_C "${ROOT}/code/renderervk/*.c")
foreach(path IN LISTS PRODUCT_C)
	get_filename_component(name "${path}" NAME)
	file(READ "${path}" SRC)
	if(name STREQUAL "vk_temporal_resolved_hdr.c")
		require_count(SRC "VK_TemporalResolvedHdrEnsureAfterFence" 1 "owner Ensure definition")
		require_count(SRC "VK_TemporalResolvedHdrRouteSource" 1 "router definition")
	elseif(name STREQUAL "vk_temporal_resolve_authority.c")
		# H3's pure join borrows the H2 receipt type but owns no H2 lifecycle,
		# producer, routing or submit authority.
		require_count(SRC "vkTemporalResolvedHdrReceipt_t" 1
			"pure H3 target receipt borrow")
		forbid_text(SRC "VK_TemporalResolvedHdr" "H2 lifecycle/route authority in H3 join")
	elseif(name STREQUAL "vk_temporal_resolve.c")
		require_count(SRC "VK_TemporalResolvedHdrBuildContentReceiptForProducer" 1
			"sole H3 producer content author")
		require_count(SRC "VK_TemporalResolvedHdrRouteSource" 1
			"sole H3 pre-command route validation")
		require_count(SRC "VK_TemporalResolvedHdrResolveContentSubmit" 1
			"sole H3 submit promotion delegate")
		forbid_text(SRC "VK_TemporalResolvedHdrEnsureAfterFence"
			"H2 owner lifecycle authority in H3 producer")
	elseif(name STREQUAL "vk.c")
		require_count(SRC "VK_TemporalResolvedHdrEnsureAfterFence" 1 "sole product Ensure")
	else()
		forbid_text(SRC "VK_TemporalResolvedHdr" "H2a authority in ${name}")
		forbid_text(SRC "vkTemporalResolvedHdr" "H2a storage in ${name}")
	endif()
endforeach()

require_text(TEST "MUTATE_CONTENT" "content receipt mutation matrix")
require_text(TEST "MUTATE_SUBMIT" "submit receipt mutation matrix")
require_text(TEST "MUTATE_TARGET" "target receipt mutation matrix")
require_text(TEST "MUTATE_CURRENT" "valid current-cohort fallback matrix")
require_text(TEST "INVALID_CURRENT" "invalid current-cohort output atomicity matrix")
require_text(TEST "const void *roles[8]" "target/current cross-role alias matrix")
require_text(TEST "const void *currentRoles[4]" "current source cross-role alias matrix")
# The legacy-fallback publication assertion, which must keep asserting that a
# non-matching route republishes `current` unchanged.
#
# Spelled SourceEquals rather than memcmp since da317716: the struct is 96 bytes
# with only 92 bytes of fields, and C11 6.2.6.1p6 leaves the 4 tail-padding
# bytes unspecified, so a byte comparison was testing the optimiser rather than
# the routine — it passed under clang and failed under gcc at -O1/-O2/-O3.
# The guarantee is unchanged and in fact stricter; only its spelling moved.
require_text(TEST "SourceEquals( &routed, &current )"
	"exact legacy fallback publication")
# And the comparator itself must still cover every declared field, or the
# assertion above would silently weaken as the struct grows.
require_text(TEST "static qboolean SourceEquals("
	"field-wise source comparator")
# Pin the number of fallback-publication sites. require_text alone only proves
# ONE survives, so deleting three of the four would pass — a gap the previous
# memcmp spelling had too. Four: the plain fallback, the two matrix macros
# (MUTATE_CURRENT / INVALID_CURRENT) and the cross-role alias sweep.
require_count(TEST "SourceEquals\\( &routed, &current \\)" 4
	"fallback publication asserted at every route site")
require_text(TEST "memcmp( &owner, &before, sizeof( owner ) ) == 0"
	"candidate-first byte-stable failure")
require_text(TEST "VK_TemporalResolvedHdrBuildContentReceipt("
	"product-shaped content author contract")
require_text(TEST "VK_TemporalResolvedHdrResolveContentSubmit("
	"submit promotion/cancel contract")
require_text(BUILD "ADD_EXECUTABLE(vk_temporal_resolved_hdr_test"
	"host test registration")
require_text(BUILD "AUX_SOURCE_DIRECTORY(code/renderervk RENDERER_VK_SRCS)"
	"product source ownership")
message(STATUS "H2 resolved-HDR target/copy/router policy: PASS")
