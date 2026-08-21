# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/renderervk/shaders/shader_manifest_compose.mjs" COMPOSER)
file(READ "${ROOT}/code/renderervk/shaders/spirv/ral_shader_portability_overrides.json" OVERRIDES)
file(READ "${ROOT}/tests/ral_shader_manifest_compose_test.mjs" HOST)
file(READ "${ROOT}/tests/ral_shader_manifest_schema_test.c" C_HOST)

function(require_text HAYSTACK NEEDLE LABEL)
	string(FIND "${${HAYSTACK}}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "missing ${LABEL}: ${NEEDLE}")
	endif()
endfunction()

foreach(NEEDLE
	"stale override:"
	"unresolved lowering for"
	"conflicting binding:"
	"invalid module cohort"
	"override rows must be unique and ordered"
	"inline-uniform-binding"
	"combined-sampler:"
	"combinedSamplers"
	"RAL_SHADER_BIND_SAMPLED_TEXTURE"
)
	require_text(COMPOSER "${NEEDLE}" "fail-closed manifest composition")
endforeach()
foreach(NEEDLE
	"color_vert_spv"
	"1255590707a965bd"
	"RAL_FORMAT_R32G32B32A32_SFLOAT"
	"hdr_histogram_comp_spv"
	"b3d978881ef424af"
	"RAL_SHADER_BIND_FILTERING_SAMPLER"
	"\"binding\": 3"
)
	require_text(OVERRIDES "${NEEDLE}" "provenance-bound production override")
endforeach()
foreach(NEEDLE
	"color_vert_spv', 'color_frag_spv"
	"brdf_lut_comp_spv"
	"hdr_histogram_comp_spv"
	"atmospheric_vert_spv', 'atmospheric_frag_spv"
	"binding === 34"
	"bloom_frag_spv"
	"stale override"
	"invalidFormat"
	"duplicateLocation"
	"syntheticOverrides"
	"overCapacity"
	"conflictingReflection"
)
	require_text(HOST "${NEEDLE}" "manifest composition mutation/production gate")
endforeach()
foreach(NATIVE "VkDescriptor" "VkPipeline" "WGPU" "MTL")
	string(FIND "${COMPOSER}${OVERRIDES}" "${NATIVE}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "native handle escaped portable manifest composer: ${NATIVE}")
	endif()
endforeach()
foreach(NEEDLE
	"RAL_SHADER_PIPELINE_GRAPHICS"
	"RAL_SHADER_PIPELINE_COMPUTE"
	"RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE"
	"inlineData.webgpuUniformBinding = 3u"
	"!Ral_ShaderAbiManifestValid"
)
	require_text(C_HOST "${NEEDLE}" "C schema-v1 composed manifest fixture")
endforeach()

message(STATUS "RAL shader manifest compose policy: PASS")
