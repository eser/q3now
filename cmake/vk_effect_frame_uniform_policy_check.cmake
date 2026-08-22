CMAKE_MINIMUM_REQUIRED(VERSION 3.16)
IF(NOT DEFINED SOURCE_ROOT)
	MESSAGE(FATAL_ERROR "SOURCE_ROOT required")
ENDIF()

SET(_vk_path "${SOURCE_ROOT}/code/renderervk/vk.c")
SET(_header_path "${SOURCE_ROOT}/code/renderervk/vk.h")
SET(_smoke_path "${SOURCE_ROOT}/tests/ral-effects-dynamic-bind-smoke.sh")
SET(_cmake_path "${SOURCE_ROOT}/CMakeLists.txt")
FOREACH(_path IN ITEMS ${_vk_path} ${_header_path} ${_smoke_path} ${_cmake_path})
	IF(NOT EXISTS "${_path}")
		MESSAGE(FATAL_ERROR "missing effect frame-uniform source: ${_path}")
	ENDIF()
ENDFOREACH()
FILE(READ "${_vk_path}" _vk)
FILE(READ "${_header_path}" _header)
FILE(READ "${_smoke_path}" _smoke)
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
FUNCTION(require_order body first second label)
	STRING(FIND "${body}" "${first}" _first)
	STRING(FIND "${body}" "${second}" _second)
	IF(_first EQUAL -1 OR _second EQUAL -1 OR NOT _first LESS _second)
		MESSAGE(FATAL_ERROR "${label}: expected ${first} before ${second}")
	ENDIF()
ENDFUNCTION()

slice_between(_particle_init "${_vk}" "void vk_init_particle( void )"
	"void vk_init_particle_textures( void )")
slice_between(_particle_shutdown "${_vk}" "void vk_shutdown_particle( void )"
	"void vk_init_decal( void )")
slice_between(_decal_init "${_vk}" "void vk_init_decal( void )"
	"void vk_init_decal_textures( void )")
slice_between(_decal_shutdown "${_vk}" "void vk_shutdown_decal( void )"
	"static void vk_atmospheric_create_heightgrid( void )")
slice_between(_decal_draw "${_vk}" "void RB_DrawDecals( void )"
	"void RB_RunParticleCompute( void )")
slice_between(_particle_compute "${_vk}" "void RB_RunParticleCompute( void )"
	"void RB_DrawParticles( void )")
slice_between(_particle_draw "${_vk}" "void RB_DrawParticles( void )"
	"#if FEAT_IQM")
slice_between(_begin_frame "${_vk}" "void vk_begin_frame( const temporalBatchRequest_t *temporalRequest )"
	"void vk_end_frame( void )")

# The product configs preserve the exact shader ABI: particle publishes its
# compute-owned 112..127 range before the final 144-byte image; decal is one
# final-only 160-byte image.
FOREACH(_needle IN ITEMS
		"static vkRalFrameUniformOwner_t vk_particle_frame;"
		"static vkRalFrameUniformOwner_t vk_decal_frame;"
		"sizeof( particleFrame_t ),"
		"offsetof( particleFrame_t, dt ),"
		"4u * sizeof( uint32_t ),"
		"\"wired-particle-frame\""
		"sizeof( decalFrame_t ),"
		"\"wired-decal-frame\"")
	require_text("${_vk}" "${_needle}" "effect frame config")
ENDFOREACH()

# Native handles may cross only at the descriptor authoring boundary, derived
# output-atomically from a complete RAL resource receipt.
FOREACH(_needle IN ITEMS
		"VK_RalFrameUniformGetResources( owner, &receipt )"
		"VK_RalFrameUniformResourcesReceiptExact( &receipt, &receipt )"
		"Ral_GetBufferSize( receipt.buffers[i] ) != config->byteSize"
		"candidate[i] = (VkBuffer)Ral_GetBufferHandle( receipt.buffers[i] );"
		"memcpy( outBuffers, candidate, sizeof( candidate ) );")
	require_text("${_vk}" "${_needle}" "descriptor receipt boundary")
ENDFOREACH()

FOREACH(_needle IN ITEMS
		"frame_buffer [NUM_COMMAND_BUFFERS]"
		"frame_memory [NUM_COMMAND_BUFFERS]"
		"*frame_ptr   [NUM_COMMAND_BUFFERS]"
		"vk.particle.frame_buffer" "vk.particle.frame_memory"
		"vk.particle.frame_ptr" "vk.decal.frame_buffer"
		"vk.decal.frame_memory" "vk.decal.frame_ptr")
	forbid_text("${_header}" "${_needle}" "legacy raw effect frame fields")
	forbid_text("${_vk}" "${_needle}" "legacy raw effect frame ownership")
ENDFOREACH()
FOREACH(_slice IN ITEMS _particle_init _particle_shutdown _decal_init _decal_shutdown
		_particle_compute _particle_draw _decal_draw)
	FOREACH(_needle IN ITEMS "vk.particle.frame" "vk.decal.frame")
		forbid_text("${${_slice}}" "${_needle}" "raw effect frame lifecycle purge")
	ENDFOREACH()
ENDFOREACH()

FOREACH(_slice IN ITEMS _particle_init _decal_init)
	require_call_count("${${_slice}}" "VK_RalFrameUniformInit[(]" 1
		"effect frame init")
	require_call_count("${${_slice}}" "VK_RalFrameUniformEnsure[(]" 1
		"effect frame ensure")
	require_call_count("${${_slice}}" "vk_frame_uniform_descriptor_buffers[(]" 1
		"effect descriptor receipt")
ENDFOREACH()
require_text("${_particle_init}" "bufInfos[0].buffer = frameBuffers[i];"
	"particle descriptor identity")
require_text("${_decal_init}" "bufInfos[0].buffer = frameBuffers[i];"
	"decal descriptor identity")

# Every active owner joins the exact post-fence slot boundary.
require_order("${_begin_frame}" "Ral_DrainDeferred( vk_ral_get_backend() );"
	"VK_RalFrameUniformBeginSlot( &vk_particle_frame,"
	"particle slot is post-drain")
require_order("${_begin_frame}" "VK_RalFrameUniformBeginSlot( &vk_particle_frame,"
	"VK_RalFrameUniformBeginSlot( &vk_decal_frame,"
	"effect slot ordering")
FOREACH(_needle IN ITEMS
		"(uint32_t)vk.cmd_index, slotFenceRequired,"
		"slotFenceCompleted )"
		"Particle RAL frame slot reuse lost exact fence authority"
		"Decal RAL frame slot reuse lost exact fence authority")
	require_text("${_begin_frame}" "${_needle}" "effect slot authority")
ENDFOREACH()

# Particle partial/final and decal final publication precede their command
# units; native smoke receipts are emitted only after successful execution.
require_order("${_particle_compute}" "VK_RalFrameUniformGetShadow( &vk_particle_frame,"
	"VK_RalFrameUniformPublishPartial( &vk_particle_frame,"
	"particle partial shadow/write")
require_order("${_particle_compute}" "VK_RalFrameUniformPublishPartial( &vk_particle_frame,"
	"VK_RalComputeExecute( &plan )" "particle partial before compute")
require_order("${_particle_draw}" "VK_RalFrameUniformGetShadow( &vk_particle_frame,"
	"VK_RalFrameUniformPublishFinal( &vk_particle_frame,"
	"particle final shadow/write")
require_order("${_particle_draw}" "VK_RalFrameUniformPublishFinal( &vk_particle_frame,"
	"VK_RalPoolRenderExecute( &plan )" "particle final before draw")
require_order("${_particle_draw}" "VK_RalPoolRenderExecute( &plan )"
	"vk_frame_uniform_smoke_receipt( 0u, \"particle\", &frameReceipt );"
	"particle receipt after draw")
require_order("${_decal_draw}" "VK_RalFrameUniformGetShadow( &vk_decal_frame,"
	"VK_RalFrameUniformPublishFinal( &vk_decal_frame,"
	"decal final shadow/write")
require_order("${_decal_draw}" "VK_RalFrameUniformPublishFinal( &vk_decal_frame,"
	"VK_RalPoolRenderExecute( &plan )" "decal final before draw")
require_order("${_decal_draw}" "VK_RalPoolRenderExecute( &plan )"
	"vk_frame_uniform_smoke_receipt( 1u, \"decal\", &frameReceipt );"
	"decal receipt after draw")

# Adopted descriptor children are gone before their RAL buffer parents.
require_order("${_particle_shutdown}" "Ral_DestroyBindGroup( vk.particle.ral_render_descriptor[i] );"
	"VK_RalFrameUniformRelease( &vk_particle_frame );"
	"particle child-before-parent")
require_order("${_decal_shutdown}" "Ral_DestroyBindGroup( vk.decal.ral_render_descriptor[i] );"
	"VK_RalFrameUniformRelease( &vk_decal_frame );"
	"decal child-before-parent")

FOREACH(_needle IN ITEMS
		"ral-frame-uniform schema=1 map=%s family=%s slot=%u"
		"VK_RalFrameUniformReceiptExact( receipt, receipt )"
		"frame_uniform_pat=re.compile"
		"particle_frame[0][5]<=particle_frame[0][4]"
		"decal_frame[0][4]!=0"
		"bad-particle-frame.jsonl" "bad-decal-frame.jsonl")
	require_text("${_vk}${_smoke}" "${_needle}" "effect frame native evidence")
ENDFOREACH()

require_text("${_cmake}" "vk_effect_frame_uniform_source_policy_contract"
	"effect frame source policy registration")
MESSAGE(STATUS "particle and decal frame uniforms are RAL-owned and slot-fence exact")
