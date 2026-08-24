# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VK)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_backend.c" BACKEND)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_model_iqm.c" IQM_MODEL)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_temporal_iqm_motion.c" IQM_CORE)
file(READ "${ROOT}/tests/tr_temporal_iqm_motion_test.c" IQM_HOST)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_textures.c" TEXTURES)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_bindless_cohort.c" COHORT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_iqm_command.c" IQM_COMMAND)

function(require_text text needle label)
	string(FIND "${text}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "temporal IQM resource product policy missing ${label}: ${needle}")
	endif()
endfunction()
function(extract_span text begin end out)
	string(FIND "${text}" "${begin}" begin_pos)
	if(begin_pos EQUAL -1)
		message(FATAL_ERROR "cannot isolate temporal IQM resource span ${begin}")
	endif()
	string(SUBSTRING "${text}" ${begin_pos} -1 tail)
	string(FIND "${tail}" "${end}" span_len)
	if(span_len EQUAL -1)
		message(FATAL_ERROR "cannot isolate temporal IQM resource end ${end}")
	endif()
	string(SUBSTRING "${text}" ${begin_pos} ${span_len} span)
	set(${out} "${span}" PARENT_SCOPE)
endfunction()

extract_span("${VK}" "static qboolean vk_temporal_iqm_geometry_prepare_loaded_after_fence"
	"static qboolean vk_temporal_iqm_resources_prepare_after_fence" GEOMETRY_PREPARE)
extract_span("${VK}" "static qboolean vk_temporal_iqm_resources_prepare_after_fence"
	"static void vk_temporal_iqm_resources_release_after_idle(\n\t\tconst char *reason ) {" RESOURCE_PREPARE)
extract_span("${VK}" "static void vk_temporal_iqm_resources_release_after_idle(\n\t\tconst char *reason ) {"
	"static struct {" RESOURCE_RELEASE)
extract_span("${VK}" "void vk_temporal_motion_release_before_ral_shutdown"
	"static void vk_profile_rendering_marker" PRE_RAL_RELEASE)
extract_span("${VK}" "void vk_release_resources( void )"
	"void vk_create_image(" LEVEL_RELEASE)

foreach(needle IN ITEMS
	"vk_temporal_iqm_geometry_key( data, &key )"
	"VK_TemporalIqmGeometryNeedsIdle("
	"vk_temporal_iqm_geometry_get_receipt( data, &receipt )"
	"vk_temporal_iqm_geometry_ensure_after_idle("
	"data, qfalse )")
	require_text("${GEOMETRY_PREPARE}" "${needle}" "active-only geometry preflight")
endforeach()
foreach(needle IN ITEMS
	"R_TemporalBatchRequestValidateExact( request )"
	"!request->enabled"
	"vk_temporal_iqm_geometry_frame_ready"
	"vk_ral_bindless_get_cohort( &cohort )"
	"VK_TemporalIqmPayloadPrepareAfterFence("
	"VK_TemporalIqmExact3FactoryEnsure("
	"candidateContext.protectedRal[4] = cohort.set"
	"factoryReceipt.bindless.setGeneration == cohort.setGeneration")
	require_text("${RESOURCE_PREPARE}" "${needle}" "post-fence exact resource materializer")
endforeach()
foreach(forbidden IN ITEMS Ral_Cmd vkCmdBind vkCmdDraw Ral_CmdDrawIndexed
	firstInstance instanceCount)
	string(FIND "${GEOMETRY_PREPARE}${RESOURCE_PREPARE}${RESOURCE_RELEASE}"
		"${forbidden}" command_pos)
	if(NOT command_pos EQUAL -1)
		message(FATAL_ERROR "resource-only slice gained command authority: ${forbidden}")
	endif()
endforeach()
string(FIND "${RESOURCE_RELEASE}" "VK_TemporalIqmExact3FactoryRelease(" factory_pos)
string(FIND "${RESOURCE_RELEASE}" "VK_TemporalIqmPayloadReleaseAfterIdle(" payload_pos)
string(FIND "${RESOURCE_RELEASE}" "vk_temporal_iqm_geometry_release_after_idle(" geometry_pos)
string(FIND "${RESOURCE_RELEASE}" "Ral_WaitIdleAndDrainDeferred(" final_drain_pos)
if(factory_pos EQUAL -1 OR payload_pos EQUAL -1 OR geometry_pos EQUAL -1
		OR final_drain_pos EQUAL -1
		OR NOT factory_pos LESS payload_pos OR NOT payload_pos LESS geometry_pos)
	message(FATAL_ERROR "IQM release must be factory/drain -> payload -> geometry")
endif()
if(NOT geometry_pos LESS final_drain_pos)
	message(FATAL_ERROR "IQM wrapper release must finish before the final deferred drain")
endif()
string(FIND "${LEVEL_RELEASE}" "vk_wait_idle();" idle_pos)
string(FIND "${LEVEL_RELEASE}" "vk_temporal_iqm_resources_release_after_idle( \"release-resources\" )" child_pos)
string(FIND "${LEVEL_RELEASE}" "vk_destroy_iqm_vbo(" raw_pos)
if(idle_pos EQUAL -1 OR child_pos EQUAL -1 OR raw_pos EQUAL -1
		OR NOT idle_pos LESS child_pos OR NOT child_pos LESS raw_pos)
	message(FATAL_ERROR "REF_LEVEL_ONLY must idle/release IQM wrappers before raw parents")
endif()
string(FIND "${PRE_RAL_RELEASE}" "vk_wait_idle();" full_idle_pos)
string(FIND "${PRE_RAL_RELEASE}" "vk_temporal_iqm_resources_release_after_idle( \"pre-RAL-shutdown\" )" full_child_pos)
if(full_idle_pos EQUAL -1 OR full_child_pos EQUAL -1
		OR NOT full_idle_pos LESS full_child_pos)
	message(FATAL_ERROR "full shutdown must release IQM children before RAL parent")
endif()
foreach(needle IN ITEMS
	"vk_temporal_iqm_geometry_frame_ready = qfalse;"
	"R_TemporalBatchRequestValidateExact( temporalRequest )\n\t\t\t&& temporalRequest->enabled"
	"vk_temporal_iqm_geometry_prepare_loaded_after_fence();"
	"vk_temporal_iqm_resources_prepare_after_fence("
	"vk_temporal_iqm_geometry_frame_ready =\n\t\t\t\tvk_temporal_iqm_resources_prepare_after_fence("
	"vk_temporal_iqm_resources_release_after_idle(\n\t\t\t\t\"disabled-exact-request\" )")
	require_text("${VK}" "${needle}" "strict OFF/current-batch lifecycle gate")
endforeach()
foreach(needle IN ITEMS
	"return Ral_AdoptBuffer( backend, native, bytes, debugName );"
	"ops.destroy = Ral_DestroyBuffer;"
	"Ral_GetBufferHandle( candidate ) == native"
	"Ral_GetBufferSize( candidate ) == (uint64_t)bytes")
	require_text("${VK}" "${needle}" "direct adopted geometry wrapper")
endforeach()
foreach(needle IN ITEMS
	"vk_ral_bindless_ledger_activate()"
	"Ral_GetBindGroupLayoutHandle( s_ral_bindless_layout )"
	"Ral_GetBindGroupHandle( s_ral_bindless_set )"
	"VK_BindlessCohortBuild( s_ral_backend")
	require_text("${TEXTURES}" "${needle}" "authoritative bindless parent accessor")
endforeach()
require_text("${COHORT}" "*outReceipt = candidate" "output-atomic cohort publication")

extract_span("${VK}" "qboolean vk_temporal_iqm_prescan_primary_command("
	"static qboolean vk_temporal_iqm_revalidate_geometry(" IQM_SCAN)
foreach(needle IN ITEMS
	"R_TemporalIqmClassifyProductSurface( &admissionFacts )"
	"if ( admission == TEMPORAL_IQM_PRODUCT_NONE ) continue;"
	"if ( admission != TEMPORAL_IQM_PRODUCT_ADMIT )"
	"if ( ++iqmSurfaceCount > TEMPORAL_IQM_MAX_DRAWS )"
	"entityNum >= 0"
	"entityNum != REFENTITYNUM_WORLD"
	"entityNum < (int)TEMPORAL_IQM_ENTITY_INDEX_LIMIT"
	"vk_temporal_iqm_geometry_get_receipt( data, &geometry )"
	"vk_ral_bindless_query_ordinary( ordinaryImage, &bindless )"
	"R_TemporalIqmSequenceBuild( &authority"
	"VK_TemporalIqmPayloadAuthorBegin("
	"VK_TemporalIqmPayloadAuthorWrite("
	"VK_TemporalIqmPayloadAuthorSeal("
	"recordIndex < vk_temporal_iqm_primary.sequence.entityCount"
	"vk_temporal_iqm_primary.sequence.entries[i].recordIndex"
	"vk_temporal_iqm_primary.models[drawIndex]"
	"start + item * vk_temporal_iqm_primary.drawCount"
	"vk_temporal_iqm_primary.prepared = qtrue;")
	require_text("${IQM_SCAN}" "${needle}" "IQM pre-scan/author exact gate")
endforeach()
string(FIND "${IQM_SCAN}" "admissionFacts.gpuDirect = data && data->vk_gpu_skinning" gpu_direct_pos)
string(FIND "${IQM_SCAN}" "== TEMPORAL_IQM_PRODUCT_NONE ) continue;" cpu_continue_pos)
string(FIND "${IQM_SCAN}" "if ( admission != TEMPORAL_IQM_PRODUCT_ADMIT )" non_admit_pos)
string(FIND "${IQM_SCAN}" "if ( ++iqmSurfaceCount > TEMPORAL_IQM_MAX_DRAWS )" admitted_cap_pos)
string(FIND "${IQM_SCAN}" "facts = &vk_temporal_iqm_primary.draws[" draw_publish_pos)
string(FIND "${IQM_SCAN}" "vk_temporal_iqm_geometry_get_receipt( data, &geometry )" geometry_query_pos)
string(FIND "${IQM_SCAN}" "vk_ral_bindless_query_ordinary( ordinaryImage, &bindless )" bindless_query_pos)
if(gpu_direct_pos EQUAL -1 OR cpu_continue_pos EQUAL -1
		OR non_admit_pos EQUAL -1 OR admitted_cap_pos EQUAL -1
		OR draw_publish_pos EQUAL -1 OR geometry_query_pos EQUAL -1
		OR bindless_query_pos EQUAL -1
		OR NOT gpu_direct_pos LESS cpu_continue_pos
		OR NOT cpu_continue_pos LESS geometry_query_pos
		OR NOT cpu_continue_pos LESS bindless_query_pos
		OR NOT cpu_continue_pos LESS non_admit_pos
		OR NOT non_admit_pos LESS admitted_cap_pos
		OR NOT admitted_cap_pos LESS draw_publish_pos)
	message(FATAL_ERROR "CPU IQM must exit before H5 geometry/bindless product queries")
endif()
string(REGEX MATCHALL "[+][+]iqmSurfaceCount" admitted_cap_calls "${IQM_SCAN}")
list(LENGTH admitted_cap_calls admitted_cap_count)
if(NOT admitted_cap_count EQUAL 1)
	message(FATAL_ERROR "admitted GPU-direct count must increment exactly once after classification: ${admitted_cap_count}")
endif()
foreach(api IN ITEMS R_TemporalIqmSequenceBuild
	VK_TemporalIqmPayloadAuthorBegin VK_TemporalIqmPayloadAuthorWrite
	VK_TemporalIqmPayloadAuthorSeal vk_ral_bindless_query_ordinary)
	string(REGEX MATCHALL "${api}[(]" api_calls "${IQM_SCAN}")
	list(LENGTH api_calls api_count)
	if(NOT api_count EQUAL 1)
		message(FATAL_ERROR "IQM pre-scan exact call inventory drifted ${api}: ${api_count}")
	endif()
endforeach()
extract_span("${VK}" "static qboolean vk_temporal_iqm_primary_poison( void )"
	"static qboolean vk_temporal_iqm_opaque_stage0_exact(" IQM_POISON)
foreach(api IN ITEMS VK_TemporalMotionRecordingPoison VK_TemporalMainActivationPoison)
	string(REGEX MATCHALL "${api}[(]" poison_calls "${IQM_POISON}")
	list(LENGTH poison_calls poison_count)
	if(NOT poison_count EQUAL 1)
		message(FATAL_ERROR "primary IQM poison must poison each temporal owner exactly once: ${api}=${poison_count}")
	endif()
endforeach()
require_text("${IQM_POISON}"
	"VK_TemporalMotionRecordingPoison( &vk_temporal_motion_recording );"
	"primary IQM recording poison")
require_text("${IQM_POISON}"
	"VK_TemporalMainActivationPoison( &vk_temporal_main_activation );"
	"primary IQM activation poison")
foreach(needle IN ITEMS
	"shader->sort == SS_OPAQUE"
	"shader->numUnfoggedPasses == 1"
	"stage->numTexBundles == 1u"
	"stage->stateBits & GLS_DEPTHMASK_TRUE"
	"GLS_ATEST_BITS | GLS_BLEND_BITS"
	"shader->numDeforms == 0"
	"shader->polygonOffset == qfalse"
	"FinishShader assigns FP_EQUAL to every opaque shader"
	"fogNum == 0 && dlighted == 0"
	"RF_DEPTHHACK | RF_CROSSHAIR")
	require_text("${VK}" "${needle}" "opaque stage0 exact classification")
endforeach()
extract_span("${VK}" "static qboolean vk_temporal_iqm_opaque_stage0_exact("
	"qboolean vk_temporal_iqm_prescan_primary_command(" IQM_OPAQUE_EXACT)
if(IQM_OPAQUE_EXACT MATCHES "shader->fogPass")
	message(FATAL_ERROR "opaque IQM admission must use the actual fogNum draw fact, not shader fogPass")
endif()
foreach(forbidden IN ITEMS VK_TemporalMainActivationBindIqm
	VK_TemporalMainActivationPlanIqmDraw VK_TemporalMainActivationCommitIqmDraw
	VK_TemporalMainActivationFinishIqm Ral_Cmd vkCmd vkCmdDraw)
	string(FIND "${IQM_SCAN}" "${forbidden}" forbidden_pos)
	if(NOT forbidden_pos EQUAL -1)
		message(FATAL_ERROR "pre-scan checkpoint gained bind/command authority: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"RB_RecordTemporalEntityReceipts( cmd );"
	"temporalPrimaryCommand = vk_temporal_motion_begin_primary_command();"
	"vk_temporal_iqm_prescan_primary_command("
	"RB_BeginDrawingView();")
	require_text("${BACKEND}" "${needle}" "pre-pass primary scan seam")
endforeach()
string(FIND "${BACKEND}" "RB_RecordTemporalEntityReceipts( cmd );" receipt_pos)
string(FIND "${BACKEND}" "temporalPrimaryCommand = vk_temporal_motion_begin_primary_command();" primary_pos)
string(FIND "${BACKEND}" "vk_temporal_iqm_prescan_primary_command(" scan_pos)
string(FIND "${BACKEND}" "RB_BeginDrawingView();" begin_view_pos)
if(receipt_pos EQUAL -1 OR primary_pos EQUAL -1 OR scan_pos EQUAL -1
		OR begin_view_pos EQUAL -1 OR NOT receipt_pos LESS primary_pos
		OR NOT primary_pos LESS scan_pos OR NOT scan_pos LESS begin_view_pos)
	message(FATAL_ERROR "IQM pre-scan must follow receipts+primary and precede BeginDrawingView")
endif()
extract_span("${BACKEND}" "static void RB_InvokeDrawSurf("
	"static void RB_RenderDrawSurfList(" INVOKE_SURF)
foreach(needle IN ITEMS
	"vk_temporal_iqm_publish_drawsurf_ordinal( absoluteOrdinal );"
	"rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );"
	"vk_temporal_iqm_reset_drawsurf_ordinal();")
	require_text("${INVOKE_SURF}" "${needle}" "command-adjacent absolute ordinal wrapper")
endforeach()
string(REGEX MATCHALL "RB_InvokeDrawSurf[(] drawSurf, [(]uint32_t[)]i," invoke_calls "${BACKEND}")
list(LENGTH invoke_calls invoke_count)
if(NOT invoke_count EQUAL 2)
	message(FATAL_ERROR "both fast/normal draw-surface paths must use ordinal wrapper: ${invoke_count}")
endif()
require_text("${VK}" "_Static_assert( sizeof( vkTemporalIqmPrimaryContext_t ) <= 524288u" "bounded primary context")
require_text("${VK}" "vk_temporal_iqm_primary_clear();" "per-command candidate reset")
extract_span("${VK}" "static void vk_temporal_iqm_primary_clear( void )"
	"static qboolean vk_temporal_iqm_primary_poison( void )" IQM_CLEAR)
string(FIND "${IQM_CLEAR}" "memset( &vk_temporal_iqm_primary" bulk_clear)
if(NOT bulk_clear EQUAL -1)
	message(FATAL_ERROR "generic-only active path must not bulk-clear the fixed IQM context")
endif()
foreach(needle IN ITEMS
	"vk_temporal_iqm_primary.drawCount = 0;"
	"vk_temporal_iqm_primary.prepared = qfalse;"
	"vk_temporal_iqm_primary.currentDrawSurfOrdinal = UINT32_MAX;"
	"vk_temporal_iqm_primary.author.initialized = qfalse;")
	require_text("${IQM_CLEAR}" "${needle}" "narrow primary context reset")
endforeach()
extract_span("${VK}" "qboolean vk_temporal_motion_seal_primary( void )"
	"void vk_end_frame( void )" MOTION_SEAL)
require_text("${MOTION_SEAL}" "vk_temporal_iqm_primary_clear();" "candidate lifetime ends at generic seal")
require_text("${IQM_MODEL}" "R_IqmOrdinaryImageForShader( tess.shader )" "ordinary material helper consumer")
require_text("${IQM_SCAN}" "R_IqmOrdinaryImageForShader( shader )" "pre-scan material helper consumer")
extract_span("${IQM_MODEL}" "\t\tvk_draw_iqm_gpu("
	"\t\t\tmvp );" ORDINARY_IQM_DRAW)
require_text("${ORDINARY_IQM_DRAW}"
    "data->num_poses,\n\t\t\t\tordinaryImage->ralDescriptor,\n"
    "ordinary IQM draw exact texture bind-group argument")
require_text("${IQM_MODEL}" "R_IqmTemporalModelView( data, &temporalModel )" "ordinary structural model helper consumer")
require_text("${IQM_SCAN}" "R_IqmTemporalModelView( data, &modelView )" "pre-scan structural model helper consumer")
require_text("${IQM_CORE}" "if ( facts->gpuDirect != qtrue ) return TEMPORAL_IQM_PRODUCT_NONE;" "CPU IQM generic classification")
require_text("${IQM_HOST}" "temporalIqmProductAdmissionFacts_t mixed[2]" "CPU+GPU mixed-list classification mutation")
require_text("${IQM_HOST}" "CHECK( admitted == 1u )" "CPU entries excluded from direct bound")
require_text("${IQM_HOST}" "== TEMPORAL_IQM_PRODUCT_ADMIT" "GPU exact classification host")

# Reachable atomic command authority. Bind is command-preceding, Plan observes
# live facts, every fallible join precedes EndRendering, and exact success can
# never replay the ordinary IQM draw.
extract_span("${VK}" "qboolean vk_temporal_iqm_bind_primary_command( void )"
	"void vk_temporal_iqm_publish_drawsurf_ordinal(" IQM_BIND)
foreach(needle IN ITEMS
	"VK_TemporalIqmExact3FactoryGetReceipt("
	"VK_TemporalMainActivationBindIqm("
	"&vk_temporal_iqm_primary.sequence"
	"&vk_temporal_iqm_primary.content"
	"&vk_temporal_iqm_payload, &factory, &pass"
	"&vk_temporal_iqm_revalidate_ops"
	"vk_temporal_iqm_primary.bound = qtrue;")
	require_text("${IQM_BIND}" "${needle}" "pre-command IQM bind authority")
endforeach()
string(FIND "${IQM_BIND}" "VK_TemporalIqmExact3FactoryGetReceipt(" bind_factory_pos)
string(FIND "${IQM_BIND}" "VK_TemporalMainActivationBindIqm(" bind_call_pos)
string(FIND "${IQM_BIND}" "vk_temporal_iqm_primary.bound = qtrue;" bind_publish_pos)
if(bind_factory_pos EQUAL -1 OR bind_call_pos EQUAL -1 OR bind_publish_pos EQUAL -1
		OR NOT bind_factory_pos LESS bind_call_pos
		OR NOT bind_call_pos LESS bind_publish_pos)
	message(FATAL_ERROR "IQM factory receipt must precede BindIqm and bound publication")
endif()
string(FIND "${BACKEND}" "vk_temporal_iqm_prescan_primary_command(" prescan_pos)
string(FIND "${BACKEND}" "vk_temporal_iqm_bind_primary_command();" bind_pos)
string(FIND "${BACKEND}" "RB_BeginDrawingView();" begin_view_pos)
if(prescan_pos EQUAL -1 OR bind_pos EQUAL -1 OR begin_view_pos EQUAL -1
		OR NOT prescan_pos LESS bind_pos OR NOT bind_pos LESS begin_view_pos)
	message(FATAL_ERROR "IQM Bind must follow pre-scan and precede BeginDrawingView")
endif()

extract_span("${VK}" "static qboolean vk_temporal_iqm_build_observed_facts("
	"void vk_temporal_iqm_reject_current_draw( void )" IQM_OBSERVED)
foreach(needle IN ITEMS
	"currentDrawSurfOrdinal"
	"backEnd.currentEntity == &backEnd.refdef.entities[i]"
	"surface == &data->surfaces[i]"
	"entity->temporalReceipt.current"
	"modelView.contentDigest"
	"geometry.key.nativeVertexBuffer"
	"geometry.key.nativeIndexBuffer"
	"geometry.key.geometryGeneration"
	"geometry.allocationGeneration"
	"vk_ral_bindless_query_ordinary( ordinaryImage, &bindless )"
	"bindless.ordinaryDescriptorIdentity"
	"R_TemporalIqmDrawFactsValid( &facts )")
	require_text("${IQM_OBSERVED}" "${needle}" "live observed IQM facts")
endforeach()

extract_span("${VK}" "static qboolean vk_temporal_iqm_command_preflight("
	"static void vk_temporal_iqm_command_end_ordinary(" IQM_PREFLIGHT)
foreach(needle IN ITEMS
	"resources->commandBuffer != vk.cmd->ral_cmd"
	"vk.cmd->open_dynamic_pass != VK_DYN_PASS_MAIN"
	"vk_temporal_iqm_build_observed_facts("
	"R_TemporalIqmSequenceEntryEqual( &actualEntry, &plan->entry )"
	"VK_TemporalIqmPayloadGetReceipt("
	"VK_TemporalIqmPayloadReceiptExact("
	"VK_TemporalIqmPayloadContentRevalidate("
	"Ral_GetBufferHandle( resources->vertexBuffer )"
	"Ral_GetBufferHandle( resources->indexBuffer )"
	"Ral_GetBufferSize( resources->vertexBuffer )"
	"Ral_GetBufferSize( resources->indexBuffer )"
	"memcmp( record->currentBones, c->currentBones"
	"memcmp( record->rasterMvp, c->rasterMvp")
	require_text("${IQM_PREFLIGHT}" "${needle}" "final pre-emission IQM join")
endforeach()

extract_span("${VK}" "qboolean vk_temporal_iqm_draw_exact("
	"static void vk_update_depth_range(" IQM_DRAW)
string(FIND "${IQM_DRAW}" "RB_EndSurface();" flush_pos)
string(FIND "${IQM_DRAW}" "vk_temporal_iqm_build_observed_facts(" observed_pos)
string(FIND "${IQM_DRAW}" "VK_TemporalMainActivationPlanIqmDraw(" plan_pos)
string(FIND "${IQM_DRAW}" "vk_temporal_iqm_get_command_resources(" resources_pos)
string(FIND "${IQM_DRAW}" "VK_TemporalIqmCommandExecute(" execute_pos)
string(FIND "${IQM_DRAW}" "VK_TemporalMainActivationCommitIqmDraw(" commit_pos)
string(FIND "${IQM_DRAW}" "return qtrue;" emitted_true_pos)
if(flush_pos EQUAL -1 OR observed_pos EQUAL -1 OR plan_pos EQUAL -1
		OR resources_pos EQUAL -1 OR execute_pos EQUAL -1 OR commit_pos EQUAL -1
		OR emitted_true_pos EQUAL -1 OR NOT flush_pos LESS observed_pos
		OR NOT observed_pos LESS plan_pos OR NOT plan_pos LESS resources_pos
		OR NOT resources_pos LESS execute_pos OR NOT execute_pos LESS commit_pos
		OR NOT commit_pos LESS emitted_true_pos)
	message(FATAL_ERROR "IQM exact draw transaction order drifted")
endif()
foreach(needle IN ITEMS
	"VK_TemporalMainActivationPoison( &vk_temporal_main_activation );"
	"return qfalse;"
	"&plan, &resources, &context, &vk_temporal_iqm_command_ops"
	"must never replay the ordinary draw")
	require_text("${IQM_DRAW}" "${needle}" "IQM exact failure/no-replay boundary")
endforeach()
foreach(api IN ITEMS RB_EndSurface VK_TemporalMainActivationPlanIqmDraw
	vk_temporal_iqm_get_command_resources VK_TemporalIqmCommandExecute
	VK_TemporalMainActivationCommitIqmDraw)
	string(REGEX MATCHALL "${api}[(]" draw_calls "${IQM_DRAW}")
	list(LENGTH draw_calls draw_count)
	if(NOT draw_count EQUAL 1)
		message(FATAL_ERROR "IQM exact draw must call ${api} exactly once: ${draw_count}")
	endif()
endforeach()
string(REGEX MATCHALL "return qfalse" draw_false_returns "${IQM_DRAW}")
list(LENGTH draw_false_returns draw_false_count)
string(REGEX MATCHALL "return qtrue" draw_true_returns "${IQM_DRAW}")
list(LENGTH draw_true_returns draw_true_count)
if(NOT draw_false_count EQUAL 3 OR NOT draw_true_count EQUAL 1)
	message(FATAL_ERROR
		"IQM exact draw fallback/no-replay return inventory drifted: false=${draw_false_count} true=${draw_true_count}")
endif()
require_text("${IQM_DRAW}"
	"if ( !VK_TemporalMainActivationCommitIqmDraw(\n\t\t\t&vk_temporal_main_activation, &plan ) )\n\t\tVK_TemporalMainActivationPoison( &vk_temporal_main_activation );\n\t// The exact draw has already completed and MAIN is open again. Even a late\n\t// receipt failure must never replay the ordinary draw; poisoning prevents\n\t// this frame from becoming temporal history while preserving scene output.\n\treturn qtrue;"
	"post-emission Commit failure poison followed by unconditional no-replay success")

string(REGEX MATCHALL "vk_draw_iqm_gpu[(]" ordinary_draw_calls "${IQM_MODEL}")
list(LENGTH ordinary_draw_calls ordinary_draw_count)
if(NOT ordinary_draw_count EQUAL 1)
	message(FATAL_ERROR "ordinary IQM draw must remain a sole fallback: ${ordinary_draw_count}")
endif()
require_text("${IQM_MODEL}" "temporalExactDrawn = vk_temporal_iqm_draw_exact("
	"actual IQM exact interception")
require_text("${IQM_MODEL}" "if ( !temporalExactDrawn ) {\n\t\t\tvk_draw_iqm_gpu("
	"exact success skips ordinary replay")

extract_span("${VK}" "qboolean vk_temporal_motion_seal_primary( void )"
	"void vk_end_frame( void )" IQM_FINISH)
foreach(needle IN ITEMS
	"if ( vk_temporal_iqm_primary.bound )"
	"VK_TemporalIqmExact3FactoryGetReceipt("
	"VK_TemporalMainActivationFinishIqm("
	"&vk_temporal_iqm_primary.sequence"
	"&vk_temporal_iqm_payload"
	"factoryReady ? &factory : NULL"
	"vk_temporal_iqm_primary_clear();")
	require_text("${IQM_FINISH}" "${needle}" "IQM FinishIqm lifetime authority")
endforeach()
string(FIND "${IQM_FINISH}" "VK_TemporalIqmExact3FactoryGetReceipt(" finish_factory_pos)
string(FIND "${IQM_FINISH}" "VK_TemporalMainActivationFinishIqm(" finish_call_pos)
string(FIND "${IQM_FINISH}" "vk_temporal_iqm_primary_clear();" finish_clear_pos)
if(finish_factory_pos EQUAL -1 OR finish_call_pos EQUAL -1 OR finish_clear_pos EQUAL -1
		OR NOT finish_factory_pos LESS finish_call_pos
		OR NOT finish_call_pos LESS finish_clear_pos)
	message(FATAL_ERROR "IQM factory receipt and FinishIqm must precede primary context clear")
endif()
foreach(api IN ITEMS VK_TemporalIqmExact3FactoryGetReceipt
	VK_TemporalMainActivationFinishIqm vk_temporal_iqm_primary_clear)
	string(REGEX MATCHALL "${api}[(]" finish_calls "${IQM_FINISH}")
	list(LENGTH finish_calls finish_count)
	if(NOT finish_count EQUAL 1)
		message(FATAL_ERROR "IQM primary seal must call ${api} exactly once: ${finish_count}")
	endif()
endforeach()
message(STATUS "temporal IQM product resource lifecycle policy PASS")
