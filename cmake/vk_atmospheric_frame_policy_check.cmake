CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
IF(NOT DEFINED SOURCE_ROOT)
	MESSAGE(FATAL_ERROR "SOURCE_ROOT required")
ENDIF()

SET(_core_path "${SOURCE_ROOT}/code/renderervk/vk_atmospheric_frame.c")
SET(_header_path "${SOURCE_ROOT}/code/renderervk/vk_atmospheric_frame.h")
SET(_generic_core_path "${SOURCE_ROOT}/code/renderervk/vk_ral_frame_uniform.c")
SET(_generic_header_path "${SOURCE_ROOT}/code/renderervk/vk_ral_frame_uniform.h")
SET(_test_path "${SOURCE_ROOT}/tests/vk_atmospheric_frame_test.c")
SET(_vk_path "${SOURCE_ROOT}/code/renderervk/vk.c")
SET(_vk_h_path "${SOURCE_ROOT}/code/renderervk/vk.h")
SET(_smoke_path "${SOURCE_ROOT}/tests/ral-effects-dynamic-bind-smoke.sh")
FOREACH(_path IN ITEMS ${_core_path} ${_header_path} ${_generic_core_path}
		${_generic_header_path} ${_test_path}
		${_vk_path} ${_vk_h_path} ${_smoke_path}
		"${SOURCE_ROOT}/CMakeLists.txt")
	IF(NOT EXISTS "${_path}")
		MESSAGE(FATAL_ERROR "missing atmospheric frame source: ${_path}")
	ENDIF()
ENDFOREACH()
FILE(READ "${_core_path}" _core)
FILE(READ "${_header_path}" _header)
FILE(READ "${_generic_core_path}" _generic_core)
FILE(READ "${_generic_header_path}" _generic_header)
FILE(READ "${_test_path}" _test)
FILE(READ "${_vk_path}" _vk)
FILE(READ "${_vk_h_path}" _vk_h)
FILE(READ "${_smoke_path}" _smoke)
FILE(READ "${SOURCE_ROOT}/CMakeLists.txt" _cmake)

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
FUNCTION(require_call_count body regex expected label)
	STRING(REGEX MATCHALL "${regex}" _hits "${body}")
	LIST(LENGTH _hits _count)
	IF(NOT _count EQUAL expected)
		MESSAGE(FATAL_ERROR "${label}: expected ${expected}, found ${_count}")
	ENDIF()
ENDFUNCTION()
FUNCTION(slice_between out body begin_marker end_marker)
	STRING(FIND "${body}" "${begin_marker}" _begin)
	IF(_begin EQUAL -1)
		MESSAGE(FATAL_ERROR "slice begin missing: ${begin_marker}")
	ENDIF()
	STRING(SUBSTRING "${body}" ${_begin} -1 _tail)
	STRING(FIND "${_tail}" "${end_marker}" _end)
	IF(_end EQUAL -1)
		MESSAGE(FATAL_ERROR "slice end missing: ${end_marker}")
	ENDIF()
	STRING(SUBSTRING "${_tail}" 0 ${_end} _slice)
	SET(${out} "${_slice}" PARENT_SCOPE)
ENDFUNCTION()

slice_between(_descriptor "${_vk}" "void vk_atmospheric_write_descriptors( void )"
	"static void vk_init_atmospheric_compute_ral_pipeline( void );")
slice_between(_init "${_vk}" "void vk_init_atmospheric( void )"
	"void vk_shutdown_atmospheric( void )")
slice_between(_shutdown "${_vk}" "void vk_shutdown_atmospheric( void )"
	"void RE_SetAtmosphere( const atmosphericDesc_t *desc )")
slice_between(_compute "${_vk}" "void RB_RunAtmosphericCompute( void )"
	"void RB_DrawAtmospheric( void )")
slice_between(_render "${_vk}" "void RB_DrawAtmospheric( void )"
	"static qboolean vk_mat4_inverse")
slice_between(_begin_frame "${_vk}" "void vk_begin_frame( const temporalBatchRequest_t *temporalRequest )"
	"void vk_end_frame( void )")
slice_between(_atm_struct "${_vk_h}" "\t// ── GPU-resident atmospheric weather"
	"#if FEAT_IQM")

# Atmospheric is a thin semantic adapter over the shared native-free owner.
# WebGPU can lower its immediate-write surface without seeing a mapped pointer
# or Vulkan handle.
FOREACH(_needle IN ITEMS "Vk" "qvk" "ral_vulkan")
	forbid_text("${_core}" "${_needle}" "frame adapter native-free core")
	forbid_text("${_header}" "${_needle}" "frame adapter native-free header")
ENDFOREACH()
FOREACH(_needle IN ITEMS
		"VK_ATMOSPHERIC_FRAME_SLOT_COUNT VK_RAL_FRAME_UNIFORM_SLOT_COUNT"
		"VK_ATMOSPHERIC_FRAME_BYTE_SIZE 208u"
		"VK_ATMOSPHERIC_FRAME_COMPUTE_OFFSET 96u"
		"VK_ATMOSPHERIC_FRAME_COMPUTE_SIZE 100u"
		"typedef vkRalFrameUniformResourcesReceipt_t"
		"typedef vkRalFrameUniformReceipt_t vkAtmosphericFrameReceipt_t;"
		"typedef vkRalFrameUniformOwner_t vkAtmosphericFrameOwner_t;")
	require_text("${_header}" "${_needle}" "frame receipt shape")
ENDFOREACH()
FOREACH(_needle IN ITEMS
		"VK_ATMOSPHERIC_FRAME_BYTE_SIZE,"
		"VK_ATMOSPHERIC_FRAME_COMPUTE_OFFSET,"
		"VK_ATMOSPHERIC_FRAME_COMPUTE_SIZE,"
		"qtrue," "\"wired-atmospheric-frame\""
		"VK_RalFrameUniformEnsure( owner, backend, &atmosphericConfig )"
		"VK_RalFrameUniformPublishPartial( owner, commandSlot )"
		"VK_RalFrameUniformPublishFinal( owner, commandSlot, outReceipt )"
		"VK_RalFrameUniformGetReceipt( owner, commandSlot, outReceipt )")
	require_text("${_core}" "${_needle}" "frame adapter transaction")
ENDFOREACH()
require_text("${_generic_core}" "candidate.buffers[i] == candidate.buffers[0]"
	"buffer alias rejection")
require_text("${_generic_core}" "owner->shadows[0] == owner->shadows[1]"
	"shadow alias rejection")

# Shipping init/descriptors/teardown own no raw UBO allocation or persistent map.
require_call_count("${_init}" "VK_AtmosphericFrameInit[(]" 1 "frame init")
require_call_count("${_init}" "VK_AtmosphericFrameEnsure[(]" 1 "frame ensure")
FOREACH(_needle IN ITEMS "qvkCreateBuffer" "qvkGetBufferMemoryRequirements"
		"qvkAllocateMemory" "qvkMapMemory" "qvkBindBufferMemory"
		"vk.atm.frame_buffer" "vk.atm.frame_memory" "vk.atm.frame_ptr"
		"vk_ral_register_buffer")
	forbid_text("${_init}" "${_needle}" "raw atmospheric frame init purge")
	forbid_text("${_shutdown}" "${_needle}" "raw atmospheric frame shutdown purge")
ENDFOREACH()
FOREACH(_needle IN ITEMS "frame_buffer [NUM_COMMAND_BUFFERS]"
		"frame_memory [NUM_COMMAND_BUFFERS]" "*frame_ptr   [NUM_COMMAND_BUFFERS]")
	forbid_text("${_atm_struct}" "${_needle}" "legacy atmospheric frame fields")
ENDFOREACH()

require_call_count("${_descriptor}" "VK_AtmosphericFrameGetResources[(]" 1
	"descriptor frame receipt")
require_call_count("${_descriptor}" "VK_AtmosphericFrameResourcesReceiptExact[(]" 1
	"descriptor frame exactness")
require_text("${_descriptor}" "frameReceipt.byteSize != frameBytes"
	"descriptor byte-size join")
FOREACH(_needle IN ITEMS
		"bufferIdentities[0] = frameReceipt.buffers[0];"
		"bufferIdentities[1] = frameReceipt.buffers[1];"
		"Ral_GetBufferSize( frameReceipt.buffers[i] ) != frameBytes"
		"values[0].buffer = frameReceipt.buffers[i];"
		"values[0].bufferRange = frameBytes;")
	require_text("${_descriptor}" "${_needle}" "direct atmospheric frame group")
ENDFOREACH()
FOREACH(_needle IN ITEMS qvkAllocateDescriptorSets qvkUpdateDescriptorSets
		Ral_AdoptBindGroup VkDescriptorBufferInfo VkWriteDescriptorSet)
	forbid_text("${_descriptor}" "${_needle}"
		"atmospheric frame retained raw descriptor authority")
ENDFOREACH()
require_call_count("${_shutdown}" "VK_AtmosphericFrameRelease[(]" 1
	"frame owner release")
STRING(FIND "${_shutdown}" "Ral_DestroyBindGroup( vk.atm.ral_render_descriptor[i] )" _child)
STRING(FIND "${_shutdown}" "VK_AtmosphericFrameRelease( &vk_atmospheric_frame );" _parent)
IF(_child EQUAL -1 OR _parent EQUAL -1 OR NOT _child LESS _parent)
	MESSAGE(FATAL_ERROR "atmospheric bind-group children must precede frame parent")
ENDIF()

# Slot authority is captured before the renderer clears waitForFence, then the
# exact owner begins only after the completed-slot/deferred-drain boundary.
require_text("${_begin_frame}" "slotFenceRequired = qtrue;"
	"submitted-slot authority capture")
STRING(FIND "${_begin_frame}" "Ral_DrainDeferred( vk_ral_get_backend() );" _drain)
STRING(FIND "${_begin_frame}" "VK_AtmosphericFrameBeginSlot( &vk_atmospheric_frame," _slot_begin)
IF(_drain EQUAL -1 OR _slot_begin EQUAL -1 OR NOT _drain LESS _slot_begin)
	MESSAGE(FATAL_ERROR "atmospheric slot begin is not post-fence/drain")
ENDIF()
require_text("${_begin_frame}" "(uint32_t)vk.cmd_index, slotFenceRequired,"
	"slot identity and fence join")
require_text("${_begin_frame}" "slotFenceCompleted )"
	"slot completion join")

# Compute and render write only through the owner, before their portable command
# units. Render's whole-shadow write finalizes both disjoint CPU owners.
require_call_count("${_compute}" "VK_AtmosphericFrameGetShadow[(]" 1
	"compute shadow")
require_call_count("${_compute}" "VK_AtmosphericFramePublishCompute[(]" 1
	"compute publication")
STRING(FIND "${_compute}" "VK_AtmosphericFramePublishCompute" _compute_write)
STRING(FIND "${_compute}" "VK_RalComputeExecute" _compute_execute)
IF(_compute_write EQUAL -1 OR _compute_execute EQUAL -1
		OR NOT _compute_write LESS _compute_execute)
	MESSAGE(FATAL_ERROR "atmospheric compute write is not finalized before dispatch")
ENDIF()
require_call_count("${_render}" "VK_AtmosphericFrameGetShadow[(]" 1
	"render shadow")
require_call_count("${_render}" "VK_AtmosphericFramePublishFinal[(]" 1
	"render final publication")
STRING(FIND "${_render}" "VK_AtmosphericFramePublishFinal" _render_write)
STRING(FIND "${_render}" "VK_RalPoolRenderExecute" _render_execute)
IF(_render_write EQUAL -1 OR _render_execute EQUAL -1
		OR NOT _render_write LESS _render_execute)
	MESSAGE(FATAL_ERROR "atmospheric final write is not finalized before draw")
ENDIF()
require_text("${_render}" "vk_atmospheric_frame_smoke_receipt( &frameReceipt );"
	"native frame receipt after draw")
STRING(FIND "${_render}" "vk_atmospheric_frame_smoke_receipt( &frameReceipt );" _frame_smoke)
STRING(FIND "${_render}" "vk_effects_smoke_receipt( VK_EFFECT_SMOKE_ATMOSPHERIC" _effect_smoke)
IF(_frame_smoke EQUAL -1 OR _effect_smoke EQUAL -1
		OR NOT _render_execute LESS _frame_smoke
		OR NOT _frame_smoke LESS _effect_smoke)
	MESSAGE(FATAL_ERROR
		"native atmospheric frame receipt must publish after draw and before the effect receipt")
ENDIF()
FOREACH(_slice IN ITEMS _compute _render)
	FOREACH(_needle IN ITEMS "qvkMapMemory" "memcpy( vk.atm.frame_ptr"
			"vk.atm.frame_buffer")
		forbid_text("${${_slice}}" "${_needle}" "raw atmospheric frame write purge")
	ENDFOREACH()
ENDFOREACH()

FOREACH(_needle IN ITEMS
		"ral-atmospheric-frame schema=1 map=%s slot=%u owner-generation=%llu"
		"compute-write=%llu final-write=%llu content-hash=%llu"
		"VK_AtmosphericFrameReceiptExact( receipt, receipt )")
	require_text("${_vk}" "${_needle}" "native frame receipt serialization")
ENDFOREACH()
FOREACH(_needle IN ITEMS
		"atmospheric_frame_pat=re.compile"
		"if len(atmospheric_frame)!=1"
		"atmospheric_frame[0][5]<=atmospheric_frame[0][4]"
		"atmospheric_compute[0][0]<atmospheric_frame[0][0]<atmospheric[0]"
		"bad-atmospheric-frame.jsonl"
		"FAIL effects smoke accepted stale atmospheric frame write")
	require_text("${_smoke}" "${_needle}" "native frame analyzer coverage")
ENDFOREACH()

# Host pins candidate failure, exact receipt mutation, first-use/no-fence,
# submitted reuse, both write ranges, content revalidation and persistent gens.
FOREACH(_needle IN ITEMS
		"phase <= 5u" "failCreate = phase" "failAllocation = phase - 2u"
		"aliasSecond = 1u" "memcmp( &owner, &before, sizeof( owner ) )"
		"VK_AtmosphericFrameBeginSlot( &owner, 0u, qfalse, qfalse )"
		"!VK_AtmosphericFrameBeginSlot( &owner, 0u, qtrue, qfalse )"
		"VK_AtmosphericFrameBeginSlot( &owner, 0u, qtrue, qtrue )"
		"writeOffsets[0] == VK_ATMOSPHERIC_FRAME_COMPUTE_OFFSET"
		"writeSizes[1] == VK_ATMOSPHERIC_FRAME_BYTE_SIZE"
		"VK_AtmosphericFramePublishFinal( &owner, 0u, &receipt2 )"
		"!VK_AtmosphericFrameGetReceipt( &owner, 0u, &receipt2 )"
		"failWrite = writes + 1u" "shadow[96] ^= 1u"
		"badReceipt.allocation.allocationGeneration++"
		"badReceipt.finalWrite.graphicsVisibilityGeneration++"
		"badReceipt.contentHash++" "badReceipt.fenceRequired = qtrue"
		"owner.slotGenerations[1] = UINT64_MAX - 1u"
		"destroyOrder[0] == '1'" "resources2.ownerGeneration == 2u")
	require_text("${_test}" "${_needle}" "frame host mutation coverage")
ENDFOREACH()

# Only the owner and vk.c may consume this product transaction.
FILE(GLOB_RECURSE _shipping_sources "${SOURCE_ROOT}/code/*.c" "${SOURCE_ROOT}/code/*.h")
FOREACH(_path IN LISTS _shipping_sources)
	IF(_path STREQUAL _core_path OR _path STREQUAL _header_path
			OR _path STREQUAL _vk_path)
		CONTINUE()
	ENDIF()
	FILE(READ "${_path}" _body)
	forbid_text("${_body}" "VK_AtmosphericFrame"
		"atmospheric frame API escaped owner: ${_path}")
ENDFOREACH()

require_text("${_cmake}" "ADD_EXECUTABLE(vk_atmospheric_frame_test"
	"frame host target")
require_text("${_cmake}" "vk_atmospheric_frame_source_policy_contract"
	"frame source-policy target")
MESSAGE(STATUS "atmospheric frame uniforms are RAL-owned and slot-fence exact")
