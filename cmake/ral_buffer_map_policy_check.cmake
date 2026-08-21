# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/renderer/ral/ral_buffer_map.h" HEADER)
file(READ "${ROOT}/code/renderer/ral/ral_buffer_map.c" CORE)
file(READ "${ROOT}/code/renderer/ral/ral_resource.h" RESOURCE_HEADER)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_map.c" VULKAN)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_transition.c" TRANSITION)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_command.c" COMMAND)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_resource.c" RESOURCE)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_internal.h" VULKAN_INTERNAL)
file(READ "${ROOT}/code/renderer/ral_vulkan/ral_vulkan_pipeline.c" PIPELINE)
file(READ "${ROOT}/tests/ral_webgpu_buffer_map_contract_test.c" WEBGPU_TEST)

foreach(FORBIDDEN IN ITEMS "Vk" "WGPU" "MTL" "queueFamily" "MapMemory")
	string(FIND "${HEADER}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "typed map public surface leaked backend-native token: ${FORBIDDEN}")
	endif()
endforeach()

string(REGEX MATCHALL "ralVk_BufferGpuUseAllowed[(]" COMMAND_GUARDS "${COMMAND}")
list(LENGTH COMMAND_GUARDS COMMAND_GUARD_COUNT)
if(NOT COMMAND_GUARD_COUNT EQUAL 13)
	message(FATAL_ERROR "mapped-buffer command guard inventory drifted: expected 13 copy/bind/indirect/barrier operands, got ${COMMAND_GUARD_COUNT}")
endif()

string(REGEX MATCHALL "ralVk_CommandBoundBuffersGpuUseAllowed[(] cb [)]" DRAW_REVALIDATIONS "${COMMAND}")
list(LENGTH DRAW_REVALIDATIONS DRAW_REVALIDATION_COUNT)
if(NOT DRAW_REVALIDATION_COUNT EQUAL 6)
	message(FATAL_ERROR "draw/dispatch bound-buffer revalidation inventory drifted: expected 6, got ${DRAW_REVALIDATION_COUNT}")
endif()

foreach(REQUIRED IN ITEMS
	"bufferTrackingComplete = qtrue"
	"bg->buffers[bg->bufferCount++] = val->buffer"
	"cb->boundBindGroups[setIndex] = g"
	"cb->boundVertexBuffers[binding] = buf"
	"cb->boundIndexBuffer = buf"
	"ralVk_BindGroupBuffersGpuUseAllowed"
	"ralVk_CommandBoundBuffersGpuUseAllowed")
	string(FIND "${RESOURCE}${COMMAND}${VULKAN_INTERNAL}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "bind/draw mapped-buffer exclusion lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"MockBegin" "MockComplete" "MockPoll"
	"RAL_BUFFER_MAP_PENDING" "ralErrorDeviceLost"
	"Ral_BufferMapLifecycleCancel" "Ral_BufferMapLifecycleUnmap")
	string(FIND "${WEBGPU_TEST}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU-shaped async map conformance lost mutation seam: ${REQUIRED}")
	endif()
endforeach()

string(REGEX MATCHALL "ralVk_WriteStagingBuffer[(]" STAGING_CALLS "${RESOURCE}")
list(LENGTH STAGING_CALLS STAGING_COUNT)
if(NOT STAGING_COUNT EQUAL 5)
	message(FATAL_ERROR "typed MAP_WRITE staging inventory drifted: expected definition + 4 calls, got ${STAGING_COUNT}")
endif()

string(REGEX MATCHALL "RAL_BUFFER_TRANSFER_SRC [|] RAL_BUFFER_MAP_WRITE" MAP_WRITE_USAGES "${RESOURCE}")
list(LENGTH MAP_WRITE_USAGES MAP_WRITE_USAGE_COUNT)
if(NOT MAP_WRITE_USAGE_COUNT EQUAL 4)
	message(FATAL_ERROR "typed MAP_WRITE staging creation inventory drifted: expected 4, got ${MAP_WRITE_USAGE_COUNT}")
endif()

foreach(REQUIRED IN ITEMS
	"RAL_BUFFER_TRANSFER_DST | RAL_BUFFER_MAP_READ"
	"hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT"
	"VK_PIPELINE_STAGE_HOST_BIT"
	"rb->portableState.usage = RAL_RESOURCE_USAGE_HOST_READ"
	"Ral_BufferMapBegin( rb, &mapRequest, &mapTicket )"
	"Ral_BufferMapUnmap( rb, &mapTicket )")
	string(FIND "${RESOURCE}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "typed MAP_READ staging/readback migration lost seam: ${REQUIRED}")
	endif()
endforeach()

file(GLOB_RECURSE PRODUCT_C
	"${ROOT}/code/renderer/*.c"
	"${ROOT}/code/renderer2/*.c"
	"${ROOT}/code/renderervk/*.c")
set(LEGACY_MAP_COUNT 0)
foreach(FILE IN LISTS PRODUCT_C)
	file(READ "${FILE}" FILE_TEXT)
	string(REGEX MATCHALL "Ral_MapBuffer[(]" FILE_CALLS "${FILE_TEXT}")
	list(LENGTH FILE_CALLS FILE_COUNT)
	math(EXPR LEGACY_MAP_COUNT "${LEGACY_MAP_COUNT} + ${FILE_COUNT}")
endforeach()
if(NOT LEGACY_MAP_COUNT EQUAL 1)
	message(FATAL_ERROR "legacy Ral_MapBuffer inventory changed: expected only the compatibility definition, got ${LEGACY_MAP_COUNT}; migrate downward deliberately and update the pin")
endif()

file(READ "${ROOT}/code/renderervk/vk.c" PRODUCT_VK)
file(READ "${ROOT}/code/renderervk/vk.h" PRODUCT_VK_HEADER)
foreach(REQUIRED IN ITEMS
	"vk.cullAabbCpu = records"
	"recs = (const vkCullSurf_t *)vk.cullAabbCpu"
	"Ral_BufferUploadAsync( vk.ral_cull_aabb"
	"Ral_BufferUploadAsync( vk.ral_cull_reached[ vk.cmd_index ]")
	string(FIND "${PRODUCT_VK}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU-valid cull upload migration lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"vk_typed_readback_transition"
	"vk_typed_readback_map"
	"brdfLutReadbackReady"
	"probeSourceReadbackReady"
	"probeRadianceReadbackReady"
	"probeIrradianceReadbackReady"
	"Ral_BufferMapUnmap( vk.ral_brdf_lut_readback"
	"Ral_BufferMapUnmap( vk.ral_probe_source_readback"
	"Ral_BufferMapUnmap( vk.ral_probe_irradiance_readback"
	"Ral_BufferMapUnmap( vk.ral_probe_radiance_readback")
	string(FIND "${PRODUCT_VK}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		string(FIND "${PRODUCT_VK_HEADER}${PRODUCT_VK}" "${REQUIRED}" POSITION)
	endif()
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "bounded one-shot typed readback migration lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"ral_histogram_readback[NUM_COMMAND_BUFFERS]"
	"histogramReadbackReady[NUM_COMMAND_BUFFERS]"
	"vk_typed_readback_map("
	"vk.ral_histogram_readback[vk.cmd_index], &ticket"
	"Ral_BufferMapUnmap( vk.ral_histogram_readback[vk.cmd_index]"
	"RAL_RESOURCE_USAGE_HOST_READ")
	string(FIND "${PRODUCT_VK_HEADER}${PRODUCT_VK}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "slot-fenced histogram typed readback migration lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(FORBIDDEN IN ITEMS "histogramReadbackPtr" "ral_fp_readback" "fpReadbackPtr")
	string(FIND "${PRODUCT_VK_HEADER}${PRODUCT_VK}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "dead/persistent debug readback authority returned: ${FORBIDDEN}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"ral_lens_sources[NUM_COMMAND_BUFFERS]"
	"ral_lens_readback[NUM_COMMAND_BUFFERS]"
	"Ral_BufferUploadAsync( vk.ral_lens_sources[slot]"
	"Ral_BufferMapBegin( vk.ral_lens_readback[slot]"
	"Ral_CmdCopyBuffer( cb, vk.ral_lens_sources[slot], vk.ral_lens_readback[slot]"
	"transition.after.usage = RAL_RESOURCE_USAGE_HOST_READ")
	string(FIND "${RESOURCE_HEADER}${PRODUCT_VK}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		file(READ "${ROOT}/code/renderervk/vk.h" PRODUCT_VK_HEADER)
		string(FIND "${PRODUCT_VK_HEADER}${PRODUCT_VK}" "${REQUIRED}" POSITION)
	endif()
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU-valid lens split/readback migration lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"vk.fpLightsPtr[i] = calloc"
	"Ral_BufferUploadAsync( vk.ral_fp_lights[ vk.cmd_index ]"
	"Ral_BufferUploadAsync( vk.ral_fp_clusterfallback"
	"Ral_BufferUploadAsync( vk.ral_fp_clustergrid")
	string(FIND "${PRODUCT_VK}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU-valid Forward+ upload migration lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"ralVk_TransitionWholeBuffer"
	"ralVk_MapReadbackBuffer"
	"RAL_BUFFER_TRANSFER_DST | RAL_BUFFER_MAP_READ"
	"ralVk_MapReadbackBuffer( readback, &mapTicket )"
	"ralVk_MapReadbackBuffer( cReadback, &mapTicket )"
	"ralVk_MapReadbackBuffer( sampleReadback, &mapTicket )")
	string(FIND "${VULKAN_INTERNAL}${COMMAND}${PIPELINE}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "internal typed readback migration lost seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS "RAL_BUFFER_MAP_READ" "RAL_BUFFER_MAP_WRITE")
	string(FIND "${RESOURCE_HEADER}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "WebGPU map creation capability lost required bit: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"Ral_BufferMapLifecyclePublishBegin( &buf->mapLifecycle"
	"request, qtrue, mappedRange, outTicket"
	"ralVk_Invalidate( buf->alloc"
	"ralVk_Flush( buf->alloc"
	"return Ral_BufferMapLifecycleCancel"
	"buf->legacyMapped || buf->alloc->mapped"
	"buf->mapLifecycle.generation >= UINT64_MAX - 1u"
	"buf->usage & RAL_BUFFER_MAP_READ"
	"buf->usage & RAL_BUFFER_MAP_WRITE")
	string(FIND "${VULKAN}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "Vulkan typed map lowering lost required seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"!ralVk_BufferGpuUseAllowed( buffer )"
	"buf->portableStateKnown = qfalse"
	"buf->legacyMapped = qtrue")
	string(FIND "${TRANSITION}${RESOURCE}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "typed/legacy map GPU exclusion lost required seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"RAL_MAP_READ" "RAL_MAP_WRITE"
	"RAL_BUFFER_MAP_PENDING" "RAL_BUFFER_MAP_READY"
	"uint64_t generation" "void *mappedRange"
	"WebGPU can publish PENDING"
	"Ral_BufferMapLifecycleGpuUseAllowed"
	"Ral_BufferMapLifecyclePoll")
	string(FIND "${HEADER}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "typed map contract lost required seam: ${REQUIRED}")
	endif()
endforeach()

foreach(REQUIRED IN ITEMS
	"lifecycle->generation >= UINT64_MAX - 1u"
	"!Ral_BufferMapLifecycleGpuUseAllowed( lifecycle )"
	"lifecycle->bufferIdentity == ticket->bufferIdentity"
	"candidate.status = ready ? RAL_BUFFER_MAP_READY : RAL_BUFFER_MAP_PENDING"
	"ralBufferMapAuthorityMatches( lifecycle, authority )"
	"pendingTicket->status != RAL_BUFFER_MAP_PENDING"
	"readyTicket->status != RAL_BUFFER_MAP_READY")
	string(FIND "${CORE}" "${REQUIRED}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "typed map state machine lost fail-closed seam: ${REQUIRED}")
	endif()
endforeach()

message(STATUS "backend-neutral typed buffer-map policy: PASS")
