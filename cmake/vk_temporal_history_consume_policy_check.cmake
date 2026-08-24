if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT required")
endif()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_history_consume.c" CONSUMER)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_temporal_history_consume.h" HEADER)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VKC)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_temporal_input.c" INPUT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/temporal_history_consume.comp" SHADER)

foreach(needle
    "!plan->historyValid" "!view->committed.valid"
    "previousCommitted=view->committed"
    "PackedHalf2Finite(words[14])"
    "FloatWordFinitePositive(words[16])"
    "RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_SRC"
    "RAL_BUFFER_TRANSFER_DST | RAL_BUFFER_MAP_READ"
    "RAL_MEMORY_DEVICE_LOCAL"
    "RAL_RESOURCE_USAGE_STORAGE_WRITE, RAL_RESOURCE_USAGE_COPY_SOURCE"
    "RAL_RESOURCE_USAGE_COPY_DESTINATION, RAL_RESOURCE_USAGE_HOST_READ"
    "Ral_CmdCopyBuffer( commandBuffer, slot->gpuBuffer, slot->readbackBuffer"
    "Ral_BufferMapBegin( slot->readbackBuffer"
    "Ral_BufferMapUnmap( slot->readbackBuffer")
  string(FIND "${CONSUMER}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "history consumer missing exact contract: ${needle}")
  endif()
endforeach()
foreach(forbidden "Ral_MapBuffer(" "Ral_UnmapBuffer(" "void *mapped")
  string(FIND "${CONSUMER}${HEADER}" "${forbidden}" pos)
  if(NOT pos EQUAL -1)
    message(FATAL_ERROR "history consumer regained persistent map surface: ${forbidden}")
  endif()
endforeach()
foreach(forbidden "motion" "activation" "vk_temporal_main" "vk_temporal_motion")
  string(FIND "${CONSUMER}${HEADER}${SHADER}" "${forbidden}" pos)
  if(NOT pos EQUAL -1)
    message(FATAL_ERROR "H1 illegally imports temporal motion/activation: ${forbidden}")
  endif()
endforeach()
foreach(needle
    "VK_TemporalHistoryConsumeRecord"
    "vk_temporal_history_store_prepare"
    "VK_TemporalHistoryConsumeAcceptStore("
    "&vk_temporal_history_consume, (uint32_t)vk.cmd_index, staged")
  string(FIND "${VKC}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "product H1 seam missing: ${needle}")
  endif()
endforeach()
string(FIND "${VKC}" "static qboolean vk_temporal_history_store_prepare" function_pos)
string(FIND "${VKC}" "static qboolean vk_temporal_history_store_feedback" function_end)
if(function_pos EQUAL -1 OR function_end EQUAL -1 OR NOT function_pos LESS function_end)
  message(FATAL_ERROR "cannot isolate H4 history_prepare")
endif()
math(EXPR function_length "${function_end} - ${function_pos}")
string(SUBSTRING "${VKC}" ${function_pos} ${function_length} RECORD_BODY)
string(FIND "${RECORD_BODY}" "VK_TemporalHistoryConsumeRecord" record_pos)
if(record_pos EQUAL -1)
  message(FATAL_ERROR "history_prepare must retain the exact H1 raw witness")
endif()
string(FIND "${RECORD_BODY}" "if ( VK_TemporalHistoryConsumeIsArmed" armed_pos)
string(FIND "${RECORD_BODY}"
  "Ral_CmdTransitionTexture( vk.cmd->ral_cmd, vk.ral_color_image" color_acquire_pos)
string(FIND "${RECORD_BODY}" "vk_scene_depth_copy_final();" depth_acquire_pos)
if(armed_pos EQUAL -1 OR color_acquire_pos EQUAL -1 OR depth_acquire_pos EQUAL -1
    OR NOT color_acquire_pos LESS armed_pos OR NOT depth_acquire_pos LESS armed_pos)
  message(FATAL_ERROR
    "current color transition and portable scene-depth copy must precede the diagnostic arm branch")
endif()
string(FIND "${RECORD_BODY}"
  "RAL_PIPELINE_STAGE_TOP_OF_PIPE_BIT, RAL_PIPELINE_STAGE_COMPUTE_SHADER_BIT"
  top_of_pipe_write)
if(NOT top_of_pipe_write EQUAL -1)
  message(FATAL_ERROR "history record must not reuse TOP_OF_PIPE for compute-owned slots")
endif()
string(FIND "${INPUT}" "s_temporalCommittedHistory[ MAX_RENDER_WORLDS ][2]" receipts)
if(receipts EQUAL -1)
  message(FATAL_ERROR "physical history receipts must be per world and per slot")
endif()
foreach(needle
    "R_TemporalHistoryPublishable"
    "diagnostic->resourceGeneration != history->allocationGeneration"
    "diagnostic->width != history->width"
    "history->topologyEpoch != s_temporalTopologyEpochs[worldIndex]"
    "textures[i]==textures[j] || views[i]==views[j]")
  string(FIND "${INPUT}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "history submit publication missing live-resource validation: ${needle}")
  endif()
endforeach()
string(FIND "${VKC}" "if ( vk.geometry_buffer_size_new )" discard_pos)
string(FIND "${VKC}" "VK_TemporalHistoryConsumeResolveSubmit(\n\t\t\t&vk_temporal_history_consume, (uint32_t)vk.cmd_index,\n\t\t\tqfalse, NULL, NULL );" cancel_pos)
string(FIND "${VKC}" "vk_resize_geometry_buffer();" resize_pos)
if(discard_pos EQUAL -1 OR cancel_pos EQUAL -1 OR resize_pos EQUAL -1
    OR NOT discard_pos LESS cancel_pos OR NOT cancel_pos LESS resize_pos)
  message(FATAL_ERROR "discarded command buffer must cancel H1 before geometry resize")
endif()
string(FIND "${VKC}" "static void vk_temporal_history_store_release_after_idle( void ) {" shutdown_pos)
string(FIND "${VKC}" "static qboolean vk_temporal_history_store_build_key" shutdown_end)
if(shutdown_pos EQUAL -1 OR shutdown_end EQUAL -1 OR NOT shutdown_pos LESS shutdown_end)
  message(FATAL_ERROR "cannot isolate H1 shutdown")
endif()
math(EXPR shutdown_length "${shutdown_end} - ${shutdown_pos}")
string(SUBSTRING "${VKC}" ${shutdown_pos} ${shutdown_length} SHUTDOWN_BODY)
foreach(needle
    "VK_TemporalHistoryConsumeReleaseAfterIdle"
    "ri.Terminate( TERM_UNRECOVERABLE"
	"qboolean consumeLive = VK_TemporalHistoryConsumeHasLive("
    "VK_TemporalHistoryStoreReleaseAfterIdle")
  string(FIND "${SHUTDOWN_BODY}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "H1 shutdown missing fail-closed lifetime step: ${needle}")
  endif()
endforeach()
string(FIND "${SHUTDOWN_BODY}" "return;" silent_escape)
if(NOT silent_escape EQUAL -1)
  message(FATAL_ERROR "H4 idle release must not silently escape with live children")
endif()
foreach(needle "temporal_history_consume_comp_spv" "if(pc.historyValid!=0u)"
		       "textureLod(sampler2D(previousColor"
		       "textureLod(sampler2D(previousLinearDepth"
		       "witness.words[14]=packHalf2x16(previous.rg)"
		       "witness.words[15]=packHalf2x16(previous.ba)"
		       "witness.words[16]=floatBitsToUint(previousDepth)")
  string(FIND "${SHADER}${VKC}" "${needle}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "history shader/product binding missing: ${needle}")
  endif()
endforeach()
foreach(assignment
    "vec4 current=textureLod(sampler2D(currentColor,nearestSampler),uv,0.0)"
    "float currentDepth=linearizeDepth(textureLod(sampler2D(currentDeviceDepth,nearestSampler),uv,0.0).r)"
    "vec4 previous=textureLod(sampler2D(previousColor,nearestSampler),uv,0.0)"
    "float previousDepth=textureLod(sampler2D(previousLinearDepth,nearestSampler),uv,0.0).r")
  string(FIND "${SHADER}" "${assignment}" pos)
  if(pos EQUAL -1)
    message(FATAL_ERROR "history witness input must be sourced by exact texture sample: ${assignment}")
  endif()
endforeach()
foreach(variable current currentDepth previous previousDepth)
  string(REGEX MATCHALL "${variable}[ \t]*=" assignments "${SHADER}")
  list(LENGTH assignments assignment_count)
  if(NOT assignment_count EQUAL 1)
    message(FATAL_ERROR "history sampled value must have exactly one assignment: ${variable}")
  endif()
endforeach()
foreach(word 14 15 16)
  string(REGEX MATCHALL "witness[.]words\\[${word}\\][ \t]*=" witness_writes "${SHADER}")
  list(LENGTH witness_writes witness_write_count)
  if(NOT witness_write_count EQUAL 2)
    message(FATAL_ERROR "history witness word ${word} must have one sampled and one bootstrap write")
  endif()
endforeach()
message(STATUS "temporal history consume source policy: PASS")
