CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
IF(NOT DEFINED SOURCE_ROOT)
	MESSAGE(FATAL_ERROR "SOURCE_ROOT required")
ENDIF()

SET(_core_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_frame_uniform.c")
SET(_header_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_ral_frame_uniform.h")
SET(_test_path "${SOURCE_ROOT}/tests/vk_ral_frame_uniform_test.c")
SET(_wrapper_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_atmospheric_frame.c")
SET(_product_path "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk.c")
SET(_cmake_path "${SOURCE_ROOT}/CMakeLists.txt")
FOREACH(_path IN ITEMS ${_core_path} ${_header_path} ${_test_path}
		${_wrapper_path} ${_product_path} ${_cmake_path})
	IF(NOT EXISTS "${_path}")
		MESSAGE(FATAL_ERROR "missing shared frame-uniform source: ${_path}")
	ENDIF()
ENDFOREACH()
FILE(READ "${_core_path}" _core)
FILE(READ "${_header_path}" _header)
FILE(READ "${_test_path}" _test)
FILE(READ "${_cmake_path}" _cmake)

FUNCTION(require_text body needle label)
	STRING(FIND "${body}" "${needle}" _pos)
	IF(_pos EQUAL -1)
		MESSAGE(FATAL_ERROR "${label}: missing ${needle}")
	ENDIF()
ENDFUNCTION()
FUNCTION(forbid_text body needle label)
	STRING(FIND "${body}" "${needle}" _pos)
	IF(NOT _pos EQUAL -1)
		MESSAGE(FATAL_ERROR "${label}: forbidden ${needle}")
	ENDIF()
ENDFUNCTION()

FOREACH(_needle IN ITEMS "Vk" "qvk" "ral_vulkan" "vk_ral_textures")
	forbid_text("${_core}" "${_needle}" "generic owner native-free core")
	forbid_text("${_header}" "${_needle}" "generic owner native-free header")
ENDFOREACH()
FOREACH(_needle IN ITEMS
		"VK_RAL_FRAME_UNIFORM_SLOT_COUNT 2u"
		"VK_RAL_FRAME_UNIFORM_MAX_BYTES 4096u"
		"uint64_t partialOffset;" "uint64_t partialSize;"
		"qboolean partialRequired;"
		"ralBufferUploadReceipt_t partialWrite;"
		"ralBufferUploadReceipt_t finalWrite;"
		"uint64_t ownerGeneration;" "uint64_t slotGeneration;"
		"uint64_t partialHash;" "uint64_t contentHash;"
		"qboolean fenceRequired;" "qboolean fenceCompleted;")
	require_text("${_header}" "${_needle}" "generic receipt shape")
ENDFOREACH()
FOREACH(_needle IN ITEMS
		"config->byteSize > VK_RAL_FRAME_UNIFORM_MAX_BYTES"
		"config->partialSize <= config->byteSize - config->partialOffset"
		"createInfo.usage = RAL_BUFFER_UNIFORM | RAL_BUFFER_TRANSFER_DST;"
		"createInfo.memory = RAL_MEMORY_HOST_COHERENT;"
		"candidate.shadows[i] = (unsigned char *)calloc"
		"candidate.buffers[i] == candidate.buffers[0]"
		"candidate.ownerGeneration++;" "*owner = candidate;"
		"fenceRequired != fenceCompleted"
		"owner->slotGenerations[commandSlot] >= UINT64_MAX - 1u"
		"owner->partialOffset," "owner->partialSize, &write"
		"partialHash != owner->partialHashes[commandSlot]"
		"owner->shadows[commandSlot], owner->byteSize, &write"
		"candidateOwner.finalReady = qtrue;"
		"HashBytes( owner->shadows[commandSlot], owner->byteSize )")
	require_text("${_core}" "${_needle}" "generic owner transaction")
ENDFOREACH()

FOREACH(_needle IN ITEMS
		"144u, 112u, 16u, qtrue, \"wired-particle-frame\""
		"160u, 0u, 0u, qfalse, \"wired-decal-frame\""
		"phase <= 5u" "failCreate = phase" "failAllocation = phase - 2u"
		"aliasSecond = 1u" "VK_RAL_FRAME_UNIFORM_MAX_BYTES + 1u"
		"VK_RalFrameUniformBeginSlot( &owner, 0u, qfalse, qfalse )"
		"!VK_RalFrameUniformBeginSlot( &owner, 0u, qtrue, qfalse )"
		"writeOffsets[0] == 112u && writeSizes[0] == 16u"
		"writeOffsets[1] == 0u && writeSizes[1] == 144u"
		"!VK_RalFrameUniformPublishFinal( &owner, 0u, &receipt2 )"
		"badReceipt.partialOffset++" "badReceipt.partialHash++"
		"!VK_RalFrameUniformPublishPartial( &owner, 1u )"
		"receipt.finalWrite.transfer.request.byteSize == 160u"
		"owner.slotGenerations[1] = UINT64_MAX - 1u"
		"destroyOrder[0] == '1'")
	require_text("${_test}" "${_needle}" "generic host mutation coverage")
ENDFOREACH()

# Product authority is narrow: the atmospheric compatibility wrapper and the
# single renderer owner TU may consume the generic transaction.
FILE(GLOB_RECURSE _shipping_sources "${SOURCE_ROOT}/code/*.c" "${SOURCE_ROOT}/code/*.h")
FOREACH(_path IN LISTS _shipping_sources)
	IF(_path STREQUAL _core_path OR _path STREQUAL _header_path
			OR _path STREQUAL _wrapper_path OR _path STREQUAL _product_path)
		CONTINUE()
	ENDIF()
	FILE(READ "${_path}" _body)
	forbid_text("${_body}" "VK_RalFrameUniform"
		"shared frame-uniform API escaped owner: ${_path}")
ENDFOREACH()

require_text("${_cmake}" "ADD_EXECUTABLE(vk_ral_frame_uniform_test"
	"generic host target")
require_text("${_cmake}" "vk_ral_frame_uniform_source_policy_contract"
	"generic source-policy target")
MESSAGE(STATUS "shared frame-uniform owner is native-free and slot-fence exact")
