cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SOURCE_ROOT)
	message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(CORE_PATH "${SOURCE_ROOT}/code/renderervk/vk_ral_shadow_storage.c")
set(HEADER_PATH "${SOURCE_ROOT}/code/renderervk/vk_ral_shadow_storage.h")
set(TEST_PATH "${SOURCE_ROOT}/tests/vk_ral_shadow_storage_test.c")
set(CMAKE_PATH "${SOURCE_ROOT}/CMakeLists.txt")
set(VK_PATH "${SOURCE_ROOT}/code/renderervk/vk.c")
set(VK_HEADER_PATH "${SOURCE_ROOT}/code/renderervk/vk.h")
set(SCENE_PATH "${SOURCE_ROOT}/code/renderervk/tr_scene.c")
set(SMOKE_PATH "${SOURCE_ROOT}/tests/ral-effects-dynamic-bind-smoke.sh")
foreach(PATH IN ITEMS "${CORE_PATH}" "${HEADER_PATH}" "${TEST_PATH}" "${CMAKE_PATH}"
		"${VK_PATH}" "${VK_HEADER_PATH}" "${SCENE_PATH}" "${SMOKE_PATH}")
	if(NOT EXISTS "${PATH}")
		message(FATAL_ERROR "missing shadow-storage contract file: ${PATH}")
	endif()
endforeach()

file(READ "${CORE_PATH}" CORE)
file(READ "${HEADER_PATH}" HEADER)
file(READ "${TEST_PATH}" HOST)
file(READ "${CMAKE_PATH}" BUILD)
file(READ "${VK_PATH}" VK)
file(READ "${VK_HEADER_PATH}" VK_HEADER)
file(READ "${SCENE_PATH}" SCENE)
file(READ "${SMOKE_PATH}" SMOKE)

function(require_text HAYSTACK NEEDLE LABEL)
	string(FIND "${HAYSTACK}" "${NEEDLE}" POSITION)
	if(POSITION EQUAL -1)
		message(FATAL_ERROR "${LABEL}")
	endif()
endfunction()

require_text("${CORE}" "createInfo.usage = RAL_BUFFER_STORAGE | RAL_BUFFER_TRANSFER_DST;"
	"shadow storage lost WebGPU-portable storage/upload usage")
require_text("${CORE}" "createInfo.memory = RAL_MEMORY_HOST_COHERENT;"
	"shadow storage lost portable upload allocation class")
require_text("${CORE}" "Ral_BufferWriteImmediate( owner->buffers[bufferIndex], offset,"
	"dirty runs no longer use the backend-neutral immediate upload")
require_text("${CORE}" "while ( runEnd < owner->elementCount"
	"dirty writes no longer coalesce contiguous elements")
require_text("${CORE}" "if ( writeCount ) memset( owner->dirty[bufferIndex], 0, owner->elementCount );"
	"dirty publication is no longer all-runs-success atomic")
require_text("${CORE}" "owner->shadows[bufferIndex]"
	"shadow storage lost per-buffer CPU ownership")
require_text("${HEADER}" "VK_RAL_SHADOW_STORAGE_MAX_BUFFERS 2u"
	"shadow storage buffer cardinality changed")
require_text("${HEADER}" "VK_RAL_SHADOW_STORAGE_MAX_BYTES ( 4u * 1024u * 1024u )"
	"shadow storage byte ceiling changed")
require_text("${HEADER}" "vkRalShadowStorageFlushReceipt_t"
	"shadow storage lost generation-bound flush receipts")
require_text("${HOST}" "receipt.dirtyElementCount == 3u && receipt.writeCount == 2u"
	"host no longer proves contiguous-run batching")
require_text("${HOST}" "writeOffsets[writesBefore] == 16u"
	"host no longer pins exact dirty upload offsets")
require_text("${HOST}" "memcmp( gpuBytes[0] + 48u, untouched"
	"host no longer proves clean gaps are preserved")
require_text("${HOST}" "failWrite = writes + 2u;"
	"host no longer covers partial multi-run upload failure")
require_text("${HOST}" "receipt.wrote == qfalse && receipt.writeCount == 0u"
	"host no longer covers generation-bound clean flush")
require_text("${BUILD}" "ADD_EXECUTABLE(vk_ral_shadow_storage_test"
	"shadow storage host is not registered")
require_text("${BUILD}" "vk_ral_shadow_storage_source_policy_contract"
	"shadow storage policy is not registered")
require_text("${VK}" "vkRalShadowStorageOwner_t vk_particle_pool_storage;"
	"particle ping-pong owner is not RAL shadow storage")
require_text("${VK}" "vkRalShadowStorageOwner_t vk_particle_class_storage;"
	"particle class owner is not RAL shadow storage")
require_text("${VK}" "vkRalShadowStorageOwner_t vk_decal_pool_storage;"
	"decal pool owner is not RAL shadow storage")
require_text("${VK}" "VK_RalShadowStorageFlush( &vk_particle_pool_storage, pingRead,"
	"particle emissions are not flushed only into the current ping-read pool")
require_text("${VK}" "VK_RalShadowStorageFlush( &vk_particle_class_storage, 0u,"
	"particle class dirty slots are not flushed before compute")
require_text("${VK}" "VK_RalShadowStorageFlush( &vk_decal_pool_storage, 0u,"
	"decal dirty slots are not flushed before draw")
require_text("${SCENE}" "vk_particle_shadow_write_emission( pingRead, slot, &p )"
	"particle emission bypasses the exact element shadow writer")
require_text("${SCENE}" "vk_particle_shadow_write_class( (uint32_t)handle - 1u, dst )"
	"particle class registration bypasses the exact element shadow writer")
require_text("${SCENE}" "vk_decal_shadow_write( slot, &d )"
	"decal emission bypasses the exact element shadow writer")
require_text("${VK}" "ral-shadow-storage schema=1 map=%s family=%s"
	"native smoke lost generation-bound shadow-storage receipts")
require_text("${SMOKE}" "families=(\"ribbon\",\"rail-ribbon\",\"beam\",\"sprite\",\"particle\",\"decal\",\"atmospheric\")"
	"native smoke lost complete effects family inventory")
require_text("${SMOKE}" "family=(particle-pool|particle-classes|decal-pool)"
	"native smoke analyzer lost shadow-storage family receipts")
require_text("${SMOKE}" "+set r_customwidth 1280 +set r_customheight 720"
	"native smoke is not explicitly constrained to canonical 16:9")

foreach(FORBIDDEN IN ITEMS
	"particle.pool_buffer" "particle.pool_memory" "particle.pool_ptr"
	"particle.classes_buffer" "particle.classes_memory" "particle.classes_ptr"
	"decal.pool_buffer" "decal.pool_memory" "decal.pool_ptr")
	string(FIND "${VK}${VK_HEADER}${SCENE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "raw particle/decal storage ownership escaped migration: ${FORBIDDEN}")
	endif()
endforeach()

foreach(FORBIDDEN IN ITEMS
	"VkBuffer" "VkDeviceMemory" "vkMapMemory" "vkCmdUpdateBuffer"
	"wgpu" "WGPU" "RAL_BACKEND_WEBGPU")
	string(FIND "${CORE}" "${FORBIDDEN}" POSITION)
	if(NOT POSITION EQUAL -1)
		message(FATAL_ERROR "shadow storage gained backend-specific token: ${FORBIDDEN}")
	endif()
endforeach()

message(STATUS "RAL shadow-storage source policy PASS")
