# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/renderervk/vk_temporal_iqm_exact3_factory.h" ABI)
file(READ "${ROOT}/code/renderervk/vk_temporal_iqm_exact3_factory.c" FACTORY)
file(READ "${ROOT}/code/renderervk/shaders/iqm_temporal_exact3.vert" VERT)
file(READ "${ROOT}/code/renderervk/shaders/iqm_temporal_exact3.frag" FRAG)
file(READ "${ROOT}/code/renderervk/shaders/iqm_skinning.frag" ORDINARY_FRAG)
file(READ "${ROOT}/code/renderervk/shaders/colorspace.glsl" COLORSPACE)
file(READ "${ROOT}/code/renderervk/shaders/shaders.manifest.mjs" MANIFEST)
file(READ "${ROOT}/code/renderervk/shaders/spirv/shader_data.c" BLOBS)
file(READ "${ROOT}/tests/vk_temporal_iqm_exact3_factory_test.c" HOST)
file(READ "${ROOT}/CMakeLists.txt" CMAKE_TEXT)

function(require_text haystack needle label)
	string(FIND "${haystack}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "temporal IQM exact3 policy missing ${label}: ${needle}")
	endif()
endfunction()

function(extract_shader_function text signature out_var)
	string(FIND "${text}" "${signature}" start)
	if(start EQUAL -1)
		message(FATAL_ERROR "missing IQM exact3 shader function: ${signature}")
	endif()
	string(SUBSTRING "${text}" ${start} -1 tail)
	string(FIND "${tail}" "\n}" finish)
	if(finish EQUAL -1)
		message(FATAL_ERROR "unterminated IQM exact3 shader function: ${signature}")
	endif()
	math(EXPR length "${finish} + 2")
	string(SUBSTRING "${tail}" 0 ${length} function_text)
	set(${out_var} "${function_text}" PARENT_SCOPE)
endfunction()

function(require_scoped_family text required forbidden expected label)
	string(REGEX MATCHALL "${required}" matches "${text}")
	list(LENGTH matches actual)
	if(NOT actual EQUAL expected)
		message(FATAL_ERROR "IQM exact3 ${label} requires ${expected} scoped '${required}' uses, got ${actual}")
	endif()
	if("${text}" MATCHES "${forbidden}")
		message(FATAL_ERROR "IQM exact3 ${label} crossed current/previous provenance: ${forbidden}")
	endif()
endfunction()

function(require_body_hash body expected label)
	string(SHA256 actual "${body}")
	if(NOT actual STREQUAL expected)
		message(FATAL_ERROR "IQM exact3 ${label} body drifted: ${actual}")
	endif()
endfunction()

foreach(needle IN ITEMS
	"vkTemporalIqmExact3Push_t"
	"offsetof( vkTemporalIqmExact3Push_t, imageSlot ) == 0u"
	"offsetof( vkTemporalIqmExact3Push_t, samplerSlot ) == 4u"
	"VK_TEMPORAL_IQM_EXACT3_PUSH_SIZE 8u"
	"VK_TEMPORAL_IQM_EXACT3_VERTEX_STRIDE 68u")
	require_text("${ABI}" "${needle}" "fixed shader ABI")
endforeach()
foreach(needle IN ITEMS
	"VK_TemporalIqmPayloadAcquireLayoutLease"
	"VK_TemporalIqmPayloadReleaseLayoutLease"
	"layoutInfo.bindGroupLayouts = ralSets"
	"layoutInfo.numBindGroupLayouts = 2u"
	"layoutInfo.pushConstantSize = VK_TEMPORAL_IQM_EXACT3_PUSH_SIZE"
	"layoutInfo.pushConstantStages = RAL_STAGE_FRAGMENT"
	"owner->adoptedLayout = ops->createLayout( input->backend, &layoutInfo )"
	"owner->rawLayout = (VkPipelineLayout)ops->getLayoutHandle("
	"ops->destroyLayout( owner->adoptedLayout )"
	"ci->pushConstantSize = VK_TEMPORAL_IQM_EXACT3_PUSH_SIZE"
	"ci->pushConstantStages = RAL_STAGE_FRAGMENT"
	"ci->numBindGroupLayouts = 2u"
	"ci->numColorFormats = 3u"
	"RAL_FORMAT_R16G16_SFLOAT"
	"RAL_FORMAT_R8_UNORM"
	"ci->sampleCount = 1u"
	"candidateAllowed"
	"owner->allocationGeneration >= UINT32_MAX - 1u"
	"parentsDrained"
	"payloadRawLayout == owner->bindlessRawLayout"
	"input->bindless.backend != input->backend"
	"parent->setIdentity != (const void *)parent->backend"
	"parent->setIdentity != (const void *)parent->layout"
	"const void *roles[8]"
	"roles[4]=owner->bindless.setIdentity"
	"roles[4]=receipt->bindless.setIdentity"
	"const void *borrowed[4] = { owner->backend, owner->payloadOwner,"
	"if ( borrowed[i] == borrowed[j] ) goto fail;"
	"if ( !OwnerValid( owner ) ) {"
	"FreshOwner( owner )")
	require_text("${FACTORY}" "${needle}" "factory/layout invariant")
endforeach()
foreach(forbidden IN ITEMS samplerPoolGeneration vk_ral_lookup_buffer
	Ral_Cmd vkCmdBind vkCmdDraw firstInstance instanceCount
	qvkCreatePipelineLayout qvkDestroyPipelineLayout VkPipelineLayoutCreateInfo)
	string(FIND "${FACTORY}" "${forbidden}" forbidden_pos)
	if(NOT forbidden_pos EQUAL -1)
		message(FATAL_ERROR "definition-only factory gained forbidden authority: ${forbidden}")
	endif()
endforeach()

foreach(needle IN ITEMS
	"layout(std430, set = 0, binding = 0) readonly buffer TemporalIqmPayload"
	"const uint TEMPORAL_IQM_MAX_RECORDS = 256u"
	"recordIndex >= TEMPORAL_IQM_MAX_RECORDS"
	"currentBones[128 * 3]"
	"previousBones[128 * 3]"
	"rasterMvp"
	"temporalCurrentMvp"
	"temporalPreviousMvp"
	"gl_Position = temporalIqmPayload.records[recordIndex].rasterMvp"
	"layout(set = 1, binding = 0) uniform texture2D wired_bindless_images[]"
	"layout(set = 1, binding = 1) uniform sampler wired_bindless_samplers[]"
	"uint imageSlot"
	"uint samplerSlot"
	"nonuniformEXT( surfacePush.imageSlot )"
	"nonuniformEXT( surfacePush.samplerSlot )"
	"wired_bindless_images[nonuniformEXT( surfacePush.imageSlot )]"
	"wired_bindless_samplers[nonuniformEXT( surfacePush.samplerSlot )]"
	"frag_tex_coord )"
	"out_color = vec4( sRGBToLinear( sampled.rgb ), sampled.a )"
	"vec2 currentUv = currentNdc * 0.5 + vec2( 0.5 )"
	"vec2 previousUv = previousNdc * 0.5 + vec2( 0.5 )"
	"vec2 velocity = currentUv - previousUv"
	"out_temporal_velocity = velocity"
	"out_temporal_validity = 1.0"
	"out_temporal_validity = 0.0")
	require_text("${VERT}\n${FRAG}" "${needle}" "shader dataflow/ABI")
endforeach()
foreach(needle IN ITEMS
	"if ( !wiredTemporalFinite4( temporalCurrentClip )\n\t\t\t|| !wiredTemporalFinite4( temporalPreviousClip )\n\t\t\t|| temporalCurrentClip.w <= 1.0e-6\n\t\t\t|| temporalPreviousClip.w <= 1.0e-6 ) return;"
	"vec2 currentNdc = temporalCurrentClip.xy / temporalCurrentClip.w;"
	"vec2 previousNdc = temporalPreviousClip.xy / temporalPreviousClip.w;"
	"if ( !wiredTemporalFinite2( currentNdc )\n\t\t\t|| !wiredTemporalFinite2( previousNdc ) ) return;"
	"if ( !wiredTemporalFinite2( velocity ) ) return;")
	require_text("${FRAG}" "${needle}" "exact finite/perspective motion guard")
endforeach()
if(FRAG MATCHES "gl_FragCoord|currentUv\\.y|previousUv\\.y")
	message(FATAL_ERROR "IQM exact3 motion gained raster/Y-flip authority")
endif()
foreach(declaration IN ITEMS "vec2 currentNdc" "vec2 previousNdc" "vec2 currentUv" "vec2 previousUv" "vec2 velocity")
	string(REGEX MATCHALL "${declaration}[ \t]*=" assignments "${FRAG}")
	list(LENGTH assignments assignment_count)
	if(NOT assignment_count EQUAL 1)
		message(FATAL_ERROR "IQM exact3 ${declaration} must have one source assignment")
	endif()
endforeach()
extract_shader_function("${VERT}" "vec4 currentBoneRow(" CURRENT_BONE_ROW)
extract_shader_function("${VERT}" "vec4 previousBoneRow(" PREVIOUS_BONE_ROW)
extract_shader_function("${VERT}" "vec3 transformCurrentPosition(" TRANSFORM_CURRENT)
extract_shader_function("${VERT}" "vec3 transformPreviousPosition(" TRANSFORM_PREVIOUS)
extract_shader_function("${VERT}" "vec3 transformDirection(" TRANSFORM_DIRECTION)
extract_shader_function("${VERT}" "vec3 skinCurrentPosition(" SKIN_CURRENT)
extract_shader_function("${VERT}" "vec3 skinPreviousPosition(" SKIN_PREVIOUS)
extract_shader_function("${VERT}" "void main()" VERT_MAIN)
extract_shader_function("${FRAG}" "bool wiredTemporalFinite4(" FINITE4)
extract_shader_function("${FRAG}" "bool wiredTemporalFinite2(" FINITE2)
require_body_hash("${CURRENT_BONE_ROW}" "100c1679fa0f653b56cc455a4c8f9eb5483d479825cbb6d6ef828c1c3dd6b1a3" "currentBoneRow")
require_body_hash("${PREVIOUS_BONE_ROW}" "6074bd3b8be37b3b25355b2cd61012988835832644193b4855f9493d2e96e1cf" "previousBoneRow")
require_body_hash("${TRANSFORM_CURRENT}" "8de9bbdb69d207f5d67eab435e9b595f5d270f67761ccca762df830748d4070c" "transformCurrentPosition")
require_body_hash("${TRANSFORM_PREVIOUS}" "4b61f7c566fbf2d122414bc3aca74b9376419c4a07e078f37cefdd91286bacb9" "transformPreviousPosition")
require_body_hash("${SKIN_CURRENT}" "7e923a8721bc3998bdf13ddfb5e598343d2618a0e1599a7f1fda91a3cdf2c34c" "skinCurrentPosition")
require_body_hash("${SKIN_PREVIOUS}" "a02f4d5cb03e059099abbd3059bd7d7630f1daf3d99ddc7e26c8eb2223a05452" "skinPreviousPosition")
require_body_hash("${FINITE4}" "bdfe0840b088ae6065727f6f91fef3e05c137a35e18944cf046369d4efe9511f" "wiredTemporalFinite4")
require_body_hash("${FINITE2}" "36a2363a628423e5fb755c882956d089c0ceab7cc3a1a73f2c7a99f09fc45c2c" "wiredTemporalFinite2")
require_scoped_family("${CURRENT_BONE_ROW}" "currentBones\\[offset\\]" "previousBones" 1 "currentBoneRow")
require_scoped_family("${PREVIOUS_BONE_ROW}" "previousBones\\[offset\\]" "currentBones" 1 "previousBoneRow")
require_scoped_family("${TRANSFORM_CURRENT}" "currentBoneRow\\(" "previousBoneRow\\(" 3 "transformCurrentPosition")
require_scoped_family("${TRANSFORM_PREVIOUS}" "previousBoneRow\\(" "currentBoneRow\\(" 3 "transformPreviousPosition")
require_scoped_family("${TRANSFORM_DIRECTION}" "currentBoneRow\\(" "previousBoneRow\\(" 3 "transformDirection")
require_scoped_family("${SKIN_CURRENT}" "transformCurrentPosition\\(" "transformPreviousPosition\\(" 4 "skinCurrentPosition")
require_scoped_family("${SKIN_PREVIOUS}" "transformPreviousPosition\\(" "transformCurrentPosition\\(" 4 "skinPreviousPosition")
foreach(span_name IN ITEMS CURRENT_BONE_ROW PREVIOUS_BONE_ROW)
	require_text("${${span_name}}" "uint offset = index * 3u + row;" "exact IQM palette row addressing")
endforeach()
foreach(pair IN ITEMS
	"TRANSFORM_CURRENT|currentBoneRow"
	"TRANSFORM_PREVIOUS|previousBoneRow")
	string(REPLACE "|" ";" fields "${pair}")
	list(GET fields 0 span_name)
	list(GET fields 1 helper)
	foreach(row IN ITEMS 0 1 2)
		require_text("${${span_name}}" "dot( ${helper}( recordIndex, index, ${row}u ), vec4( position, 1.0 ) )" "exact IQM position row/homogeneous source")
	endforeach()
endforeach()
foreach(row IN ITEMS 0 1 2)
	require_text("${TRANSFORM_DIRECTION}" "dot( currentBoneRow( recordIndex, index, ${row}u ).xyz, direction )" "exact IQM direction row source")
endforeach()
foreach(pair IN ITEMS
	"SKIN_CURRENT|transformCurrentPosition"
	"SKIN_PREVIOUS|transformPreviousPosition")
	string(REPLACE "|" ";" fields "${pair}")
	list(GET fields 0 span_name)
	list(GET fields 1 helper)
	require_text("${${span_name}}" "vec3 value = in_bone_weights.x\n\t\t* ${helper}( recordIndex, in_bone_indices.x, in_position );" "exact IQM x-lane skin source")
	foreach(lane IN ITEMS y z w)
		require_text("${${span_name}}" "if ( in_bone_weights.${lane} > 0.0 ) value += in_bone_weights.${lane}\n\t\t* ${helper}( recordIndex, in_bone_indices.${lane}, in_position );" "exact IQM guarded skin lane source")
	endforeach()
endforeach()
foreach(needle IN ITEMS
	"gl_Position = temporalIqmPayload.records[recordIndex].rasterMvp\n\t\t* vec4( currentPosition, 1.0 )"
	"temporalCurrentClip = temporalIqmPayload.records[recordIndex].temporalCurrentMvp\n\t\t* vec4( currentPosition, 1.0 )"
	"temporalPreviousClip = temporalIqmPayload.records[recordIndex].temporalPreviousMvp\n\t\t* vec4( previousPosition, 1.0 )")
	require_text("${VERT}" "${needle}" "exact current/previous clip source")
endforeach()
foreach(definition IN ITEMS
	"uint recordIndex = gl_InstanceIndex;"
	"vec3 currentPosition = skinCurrentPosition( recordIndex );"
	"vec3 previousPosition = skinPreviousPosition( recordIndex );"
	"vec3 currentNormal = skinDirection( recordIndex, in_normal );"
	"vec3 currentTangent = skinDirection( recordIndex, in_tangent.xyz );")
	require_text("${VERT_MAIN}" "${definition}" "canonical VS local provenance")
endforeach()
set(VERT_MUTATION_SCAN "${VERT_MAIN}")
foreach(allowed IN ITEMS
	"uint recordIndex = gl_InstanceIndex;"
	"vec3 currentPosition = skinCurrentPosition( recordIndex );"
	"vec3 previousPosition = skinPreviousPosition( recordIndex );"
	"vec3 currentNormal = skinDirection( recordIndex, in_normal );"
	"vec3 currentTangent = skinDirection( recordIndex, in_tangent.xyz );"
	"gl_Position = vec4( 0.0, 0.0, -2.0, 1.0 );"
	"temporalCurrentClip = vec4( 0.0 );"
	"temporalPreviousClip = vec4( 0.0 );"
	"gl_Position = temporalIqmPayload.records[recordIndex].rasterMvp\n\t\t* vec4( currentPosition, 1.0 );"
	"temporalCurrentClip = temporalIqmPayload.records[recordIndex].temporalCurrentMvp\n\t\t* vec4( currentPosition, 1.0 );"
	"temporalPreviousClip = temporalIqmPayload.records[recordIndex].temporalPreviousMvp\n\t\t* vec4( previousPosition, 1.0 );")
	string(REPLACE "${allowed}" "" VERT_MUTATION_SCAN "${VERT_MUTATION_SCAN}")
endforeach()
if(VERT_MUTATION_SCAN MATCHES "(^|[^A-Za-z0-9_])(recordIndex|currentPosition|previousPosition|currentNormal|currentTangent|gl_Position|temporalCurrentClip|temporalPreviousClip)([ \\t]*(\\.[A-Za-z_][A-Za-z0-9_]*|\\[[^]]+\\]))*[ \\t]*(=|\\+=|-=|\\*=|/=|\\+\\+|--)")
	message(FATAL_ERROR "IQM exact3 VS local/output provenance gained a second mutation")
endif()
if(VERT_MUTATION_SCAN MATCHES "(\\+\\+|--)[ \\t]*(recordIndex|currentPosition|previousPosition|currentNormal|currentTangent|gl_Position|temporalCurrentClip|temporalPreviousClip)([ \\t]*(\\.[A-Za-z_][A-Za-z0-9_]*|\\[[^]]+\\]))*")
	message(FATAL_ERROR "IQM exact3 VS local/output provenance gained a prefix mutation")
endif()
set(FRAG_MUTATION_SCAN "${FRAG}")
foreach(declaration IN ITEMS
	"vec4 sampled = texture( sampler2D(\n\t\twired_bindless_images[nonuniformEXT( surfacePush.imageSlot )],\n\t\twired_bindless_samplers[nonuniformEXT( surfacePush.samplerSlot )] ),\n\t\tfrag_tex_coord );"
	"vec2 currentNdc = temporalCurrentClip.xy / temporalCurrentClip.w;"
	"vec2 previousNdc = temporalPreviousClip.xy / temporalPreviousClip.w;"
	"vec2 currentUv = currentNdc * 0.5 + vec2( 0.5 );"
	"vec2 previousUv = previousNdc * 0.5 + vec2( 0.5 );"
	"vec2 velocity = currentUv - previousUv;"
	"out_color = vec4( sRGBToLinear( sampled.rgb ), sampled.a );"
	"out_temporal_velocity = vec2( 0.0 );"
	"out_temporal_validity = 0.0;"
	"out_temporal_velocity = velocity;"
	"out_temporal_validity = 1.0;")
	string(REPLACE "${declaration}" "" FRAG_MUTATION_SCAN "${FRAG_MUTATION_SCAN}")
endforeach()
if(FRAG_MUTATION_SCAN MATCHES "(^|[^A-Za-z0-9_])(sampled|currentNdc|previousNdc|currentUv|previousUv|velocity|out_color|out_temporal_velocity|out_temporal_validity)([ \\t]*(\\.[A-Za-z_][A-Za-z0-9_]*|\\[[^]]+\\]))*[ \\t]*(=|\\+=|-=|\\*=|/=|\\+\\+|--)")
	message(FATAL_ERROR "IQM exact3 sample/UV/velocity/output gained a second mutation")
endif()
if(FRAG_MUTATION_SCAN MATCHES "(\\+\\+|--)[ \\t]*(sampled|currentNdc|previousNdc|currentUv|previousUv|velocity|out_color|out_temporal_velocity|out_temporal_validity)([ \\t]*(\\.[A-Za-z_][A-Za-z0-9_]*|\\[[^]]+\\]))*")
	message(FATAL_ERROR "IQM exact3 sample/UV/velocity/output gained a prefix mutation")
endif()
if(FRAG MATCHES "previousUv[ \t]*-[ \t]*currentUv|out_temporal_velocity[ \t]*=[ \t]*-[ \t]*velocity")
	message(FATAL_ERROR "IQM exact3 velocity sign authority drifted")
endif()

function(extract_srgb_function text out_var)
	string(FIND "${text}" "vec3 sRGBToLinear( vec3 c ) {" start)
	if(start EQUAL -1)
		message(FATAL_ERROR "missing sRGBToLinear authority")
	endif()
	string(SUBSTRING "${text}" ${start} -1 tail)
	string(FIND "${tail}" "\n}" finish)
	if(finish EQUAL -1)
		message(FATAL_ERROR "unterminated sRGBToLinear authority")
	endif()
	math(EXPR length "${finish} + 2")
	string(SUBSTRING "${tail}" 0 ${length} function_text)
	set(${out_var} "${function_text}" PARENT_SCOPE)
endfunction()
extract_srgb_function("${COLORSPACE}" SHARED_SRGB)
extract_srgb_function("${ORDINARY_FRAG}" ORDINARY_SRGB)
if(NOT SHARED_SRGB STREQUAL ORDINARY_SRGB)
	message(FATAL_ERROR "exact3 shared sRGB decode drifted from ordinary IQM")
endif()
foreach(forbidden IN ITEMS "TemporalIqmRecord record =" "vec4 rows[" "bool previous")
	string(FIND "${VERT}" "${forbidden}" bad_shader)
	if(NOT bad_shader EQUAL -1)
		message(FATAL_ERROR "IQM exact3 VS gained bulk-copy/selector surface: ${forbidden}")
	endif()
endforeach()

foreach(output IN ITEMS iqm_temporal_exact3_vert_spv
	iqm_temporal_exact3_write_frag_spv
	iqm_temporal_exact3_invalidate_frag_spv)
	string(REGEX MATCHALL "output:[ \t]*'${output}'" manifest_rows "${MANIFEST}")
	list(LENGTH manifest_rows manifest_count)
	if(NOT manifest_count EQUAL 1)
		message(FATAL_ERROR "exact3 manifest output must appear once: ${output}")
	endif()
	string(REGEX MATCHALL "const unsigned char ${output}\\[" blob_rows "${BLOBS}")
	list(LENGTH blob_rows blob_count)
	if(NOT blob_count EQUAL 1)
		message(FATAL_ERROR "exact3 embedded blob must appear once: ${output}")
	endif()
endforeach()

# Batch 3B owns definitions only. Batch 4 will add the pre-scan, materialize,
# bind and draw authority; no shipping TU may instantiate/call this owner yet.
file(GLOB_RECURSE PRODUCT_SURFACE "${ROOT}/code/*.c" "${ROOT}/code/*.h")
foreach(source IN LISTS PRODUCT_SURFACE)
	if(source STREQUAL "${ROOT}/code/renderervk/vk_temporal_iqm_exact3_factory.c" OR
			source STREQUAL "${ROOT}/code/renderervk/vk_temporal_iqm_exact3_factory.h")
		continue()
	endif()
	file(READ "${source}" source_text)
	if(source STREQUAL "${ROOT}/code/renderervk/vk_temporal_main_activation.c" OR
			source STREQUAL "${ROOT}/code/renderervk/vk_temporal_main_activation.h")
		require_text("${source_text}" "vkTemporalIqmExact3FactoryReceipt_t"
			"central activation immutable factory receipt")
		if(source MATCHES "\\.c$")
			require_text("${source_text}" "VK_TemporalIqmExact3FactoryReceiptExact"
				"central activation exact factory receipt join")
		endif()
		foreach(forbidden IN ITEMS Init Ensure GetReceipt Release)
			string(FIND "${source_text}"
				"VK_TemporalIqmExact3Factory${forbidden}" forbidden_pos)
			if(NOT forbidden_pos EQUAL -1)
				message(FATAL_ERROR
					"central activation escaped factory receipt-only surface: ${forbidden}")
			endif()
		endforeach()
		string(FIND "${source_text}" "vkTemporalIqmExact3FactoryOwner_t" owner_pos)
		if(NOT owner_pos EQUAL -1)
			message(FATAL_ERROR "central activation gained exact3 factory owner storage")
		endif()
		continue()
	endif()
	if(source STREQUAL "${ROOT}/code/renderervk/vk_temporal_iqm_command.c" OR
			source STREQUAL "${ROOT}/code/renderervk/vk_temporal_iqm_command.h")
		if(source MATCHES "[.]c$")
			require_text("${source_text}" "VK_TemporalIqmExact3FactoryReceiptExact"
				"IQM command exact current factory join")
		else()
			require_text("${source_text}" "vkTemporalIqmExact3FactoryReceipt_t"
				"IQM command immutable factory receipt")
		endif()
		foreach(forbidden IN ITEMS Init Ensure GetReceipt Release)
			string(FIND "${source_text}"
				"VK_TemporalIqmExact3Factory${forbidden}" forbidden_pos)
			if(NOT forbidden_pos EQUAL -1)
				message(FATAL_ERROR
					"IQM command escaped factory receipt-only surface: ${forbidden}")
			endif()
		endforeach()
		continue()
	endif()
	if(source STREQUAL "${ROOT}/code/renderervk/vk.c")
		string(REGEX MATCHALL
			"vkTemporalIqmExact3FactoryOwner_t[ 	]+vk_temporal_iqm_exact3_factory"
			product_owners "${source_text}")
		list(LENGTH product_owners product_owner_count)
		if(NOT product_owner_count EQUAL 1)
			message(FATAL_ERROR "exact3 product owner inventory drifted")
		endif()
		foreach(api_count IN ITEMS "Init;1" "Ensure;1" "GetReceipt;5" "Release;5")
			list(GET api_count 0 api)
			list(GET api_count 1 expected)
			string(REGEX MATCHALL "VK_TemporalIqmExact3Factory${api}[(]" calls "${source_text}")
			list(LENGTH calls actual)
			if(NOT actual EQUAL expected)
				message(FATAL_ERROR "exact3 product resource API drifted ${api}: ${actual}")
			endif()
		endforeach()
		require_text("${source_text}" "vk_temporal_iqm_resources_prepare_after_fence"
			"exact3 post-fence resource materializer")
		require_text("${source_text}" "vk_temporal_iqm_resources_release_after_idle"
			"exact3 child-first product teardown")
		continue()
	endif()
	string(FIND "${source_text}" "VK_TemporalIqmExact3Factory" escaped_call)
	string(FIND "${source_text}" "vkTemporalIqmExact3FactoryOwner_t" escaped_owner)
	if(NOT escaped_call EQUAL -1 OR NOT escaped_owner EQUAL -1)
		message(FATAL_ERROR "exact3 factory escaped definition-only TU: ${source}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"vk_temporal_iqm_exact3_factory_test"
	"vk_temporal_iqm_exact3_factory.c"
	"vk_temporal_iqm_exact3_policy_check.cmake"
	"vk-temporal-iqm-exact3-shader-check.sh")
	require_text("${CMAKE_TEXT}" "${needle}" "CMake acceptance wiring")
endforeach()
foreach(needle IN ITEMS
	"RAL_STAGE_FRAGMENT"
	"CreateLayout"
	"GetPipelineLayoutHandle"
	"s_layoutPushOffset==0u"
	"s_rejectRole"
	"input.bindless.setIdentity"
	"SELF_BAD(m.bindless.setIdentity=m.backend)"
	"SELF_BAD(m.bindless.setIdentity=m.bindless.layout)"
	"VK_TemporalIqmExact3FactoryReceiptExact")
	require_text("${HOST}" "${needle}" "host mutation gate")
endforeach()

message(STATUS "temporal IQM exact3 definition-only policy PASS")
