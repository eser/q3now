cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/renderervk/vk.c" VKC)
file(READ "${ROOT}/code/renderervk/vk_temporal_main_rendering.c" RENDERING)
file(READ "${ROOT}/code/renderervk/vk_temporal_main_activation.c" ACTIVATION)
file(READ "${ROOT}/code/renderer/ral/ral_command.h" COMMAND_HEADER)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_dynamic_bind.c" DYNAMIC_BIND)
file(READ "${ROOT}/tests/ral_vulkan_dynamic_bind_test.c" DYNAMIC_HOST)

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
		"Ral_CmdBindBindGroupDynamicExact( vk.cmd->ral_cmd, 0u,"
		"Ral_CmdBindBindGroupDynamicExact( vk.cmd->ral_cmd, 3u,"
		"vk_issue_geometry_draws( indexed, firstInstance );"
		"VK_TemporalMainRenderingBuildResume("
		"vk_temporal_main_reset_bind_cache();")
	require_text("${SEGMENT}" "${needle}" "bounded exact segment step")
endforeach()
string(FIND "${SEGMENT}" "qvkCmdBindDescriptorSets(" raw_descriptor_bind)
if(NOT raw_descriptor_bind EQUAL -1)
	message(FATAL_ERROR "A2c4b policy exact segment regained raw descriptor binding")
endif()
string(REGEX MATCHALL "Ral_CmdBindBindGroupDynamicExact[(]" exact_bind_calls "${SEGMENT}")
list(LENGTH exact_bind_calls exact_bind_count)
if(NOT exact_bind_count EQUAL 4)
	message(FATAL_ERROR "A2c4b policy expected four exact bind-group calls, got ${exact_bind_count}")
endif()
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
foreach(needle IN ITEMS
		"ralBindGroup_t *groups[4];"
		"resources.groups[0] = vk.cmd->ral_uniform_descriptor;"
		"resources.groups[1] = vk_ral_get_bindless_set();"
		"resources.groups[2] = vk.engineResources.ral_descriptor;"
		"resources.groups[3] = resources.frameBinding.compositeGroup;"
		"Ral_ValidateBindGroupDynamicExact( vk.cmd->ral_cmd,"
		"(VkDescriptorSet)Ral_GetBindGroupHandle( resources.groups[0] )"
		"(VkDescriptorSet)Ral_GetBindGroupHandle( resources.groups[2] )")
	require_text("${VKC}" "${needle}" "pre-END exact group authority")
endforeach()
string(REGEX MATCHALL "Ral_ValidateBindGroupDynamicExact[(]" preflight_calls "${VKC}")
list(LENGTH preflight_calls preflight_count)
if(NOT preflight_count EQUAL 4)
	message(FATAL_ERROR "A2c4b policy expected four non-emitting group preflights, got ${preflight_count}")
endif()
require_text("${COMMAND_HEADER}" "qboolean Ral_ValidateBindGroupDynamicExact(" "public non-emitting exact preflight")
require_text("${DYNAMIC_BIND}" "qboolean ralVk_ValidateBindGroupDynamicExact(" "Vulkan non-emitting exact preflight")
require_text("${DYNAMIC_BIND}" "return ralVk_BindGroupDynamicValid( cb, pipeline" "shared validate/emit predicate")
require_text("${DYNAMIC_HOST}" "bindCalls == 0u && command.boundBindGroups[3] == &sentinel" "preflight output atomicity")
require_text("${DYNAMIC_HOST}" "!Ral_ValidateBindGroupDynamicExact(" "preflight layout mutation")
foreach(needle IN ITEMS
		"exactPipeline.backend = &otherBackend;"
		"exactPipeline.layout = VK_NULL_HANDLE;"
		"command.lifecycle.state = RAL_COMMAND_IDLE;"
		"offsets[0] = 1u;"
		"uniformBuffer.size = 300u;")
	require_text("${DYNAMIC_HOST}" "${needle}" "preflight mutation matrix")
endforeach()
require_text("${VKC}" "VK_TemporalMotionRecordingPeekPendingDraw(" "same A2c3 pending outcome")
require_text("${VKC}" "VK_TemporalMainActivationResolveSubmit(" "submit-gated publication")
require_text("${VKC}" "submitResult == ralSuccess ? qtrue : qfalse" "checked submit result")
# Success-only receipt promotion. The guarantee: a receipt is published ONLY
# when the submit succeeded — a failed submit must leave the consumer with
# nothing rather than a stale or half-formed receipt.
#
# The spelling moved when the check was STRENGTHENED: promotion now also
# requires the pending receipt to validate, so the condition reads
# `submitted && ActivationReceiptValid(...)`. Pinning the old, weaker text
# would fail on code that is strictly more careful — which is exactly what
# happened, and is the recurring hazard of text-matching a guarantee rather
# than its meaning.
#
# Both halves are pinned so neither can be dropped alone.
require_text("${ACTIVATION}" "if ( submitted && ActivationReceiptValid( &owner->pendingReceipt ) )"
	"success-only receipt promotion")
require_text("${ACTIVATION}" "receipt = owner->pendingReceipt"
	"promotion actually publishes the pending receipt")

message(STATUS "vk temporal MAIN rendering policy: PASS")
