cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/renderervk/vk.c" VKC)
file(READ "${ROOT}/code/renderervk/vk_temporal_main_rendering.c" RENDERING)
file(READ "${ROOT}/code/renderervk/vk_temporal_main_activation.c" ACTIVATION)

function(require_text haystack needle why)
	string(FIND "${haystack}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "A2c4b policy missing ${why}: ${needle}")
	endif()
endfunction()

require_text("${RENDERING}" "info.colorLoadOps[0] = RAL_LOAD_OP_LOAD" "scene LOAD in exact/resume")
require_text("${RENDERING}" "clearAuxiliary ? RAL_LOAD_OP_CLEAR : RAL_LOAD_OP_LOAD" "auxiliary clear-once/load-later")
require_text("${RENDERING}" "info.depthStoreOp = RAL_STORE_OP_STORE" "depth preservation")
require_text("${RENDERING}" "info.stencilStoreOp = hasStencil ? RAL_STORE_OP_STORE" "stencil preservation")
require_text("${RENDERING}" "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT" "typed attachment barrier")
require_text("${RENDERING}" "VK_DEPENDENCY_BY_REGION_BIT" "tile-local barrier dependency")

string(FIND "${VKC}" "static qboolean vk_temporal_main_issue_exact_segment(" segment_pos)
if(segment_pos LESS 0)
	message(FATAL_ERROR "A2c4b policy missing exact segment product seam")
endif()
string(SUBSTRING "${VKC}" ${segment_pos} 9000 SEGMENT)
foreach(needle IN ITEMS
		"Ral_EndRendering( vk.cmd->ral_cmd );"
		"vk_temporal_main_attachment_barrier();"
		"VK_TemporalMainRenderingBuildExact3("
		"Ral_CmdBindPipeline( vk.cmd->ral_cmd, plan->pipeline );"
		"qvkCmdBindDescriptorSets("
		"vk_issue_geometry_draws( indexed, firstInstance );"
		"VK_TemporalMainRenderingBuildResume("
		"vk_temporal_main_reset_bind_cache();")
	require_text("${SEGMENT}" "${needle}" "bounded exact segment step")
endforeach()
string(FIND "${SEGMENT}" "Ral_EndRendering( vk.cmd->ral_cmd );" end_pos)
string(FIND "${SEGMENT}" "Ral_BeginRendering( vk.cmd->ral_cmd, &exactInfo );" exact_begin_pos)
string(FIND "${SEGMENT}" "vk_issue_geometry_draws( indexed, firstInstance );" draw_pos)
string(FIND "${SEGMENT}" "Ral_BeginRendering( vk.cmd->ral_cmd, &resumeInfo );" resume_pos)
if(end_pos LESS 0 OR exact_begin_pos LESS end_pos OR draw_pos LESS exact_begin_pos
		OR resume_pos LESS draw_pos)
	message(FATAL_ERROR "A2c4b policy exact segment order drift")
endif()

require_text("${VKC}" "vk.fboActive || vk.renderPassIndex != RENDER_PASS_MAIN" "FBO MAIN-only gate")
require_text("${VKC}" "VK_TemporalMotionMaterializationGetProductView(" "generation-bound target/layout view")
require_text("${VKC}" "VK_TemporalEntMatRuntimeGetFrameBinding(" "generation-bound composite set")
require_text("${VKC}" "VK_TemporalMotionRecordingPeekPendingDraw(" "same A2c3 pending outcome")
require_text("${VKC}" "VK_TemporalMainActivationResolveSubmit(" "submit-gated publication")
require_text("${VKC}" "submitResult == ralSuccess ? qtrue : qfalse" "checked submit result")
require_text("${ACTIVATION}" "if ( submitted ) receipt = owner->pendingReceipt" "success-only receipt promotion")

message(STATUS "vk temporal MAIN rendering policy: PASS")
