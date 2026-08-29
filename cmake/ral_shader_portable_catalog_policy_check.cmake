# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/compile_portable_manifest.mjs" GENERATOR)
file(READ "${ROOT}/code/render/ral/core/ral_webgpu_shader_catalog.mjs" CONSUMER)
file(READ "${ROOT}/tests/ral_shader_portable_manifest_test.mjs" MANIFEST_HOST)
file(READ "${ROOT}/tests/ral_webgpu_shader_catalog_test.mjs" WEBGPU_HOST)

function(require_text HAYSTACK NEEDLE LABEL)
	string(FIND "${${HAYSTACK}}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "missing ${LABEL}: ${NEEDLE}")
	endif()
endfunction()

foreach(NEEDLE
	"reflection/translation identity mismatch"
	"reflection.toolchainIdentity !== translation.toolchainIdentity"
	"WGSL combined split missing"
	"WGSL inline uniform mismatch"
	"IQM_VEC3"
	"RAL_FORMAT_R32G32B32A32_SFLOAT"
	"arrayCount: reflected.bindingClass.includes('SAMPLER') ? 32 : 4096"
	"resolveShaderModule(entry, overrides)"
)
	require_text(GENERATOR "${NEEDLE}" "portable manifest resolution")
endforeach()
foreach(NEEDLE
	"portableManifest.byteCount"
	"ralShaderArtifactDigest(bytes)"
	"var<immediate>"
	"bindings: portable.reflection.bindings"
	"unknown WGSL module"
)
	require_text(CONSUMER "${NEEDLE}" "native-free WebGPU catalog consumer")
endforeach()
foreach(NATIVE "VkDescriptor" "VkPipeline" "vulkan.h" "spirv_cross")
	string(FIND "${CONSUMER}" "${NATIVE}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "WebGPU catalog consumer leaked native concept: ${NATIVE}")
	endif()
endforeach()
foreach(NEEDLE "299" "inlineUniform).length, 17" "combinedSamplers?.length" "arrayCounts?.length" "vertexFormats?.length")
	require_text(MANIFEST_HOST "${NEEDLE}" "full portable manifest host")
endforeach()
foreach(NEEDLE "moduleCount, 299" "color_vert_spv" "hdr_histogram_comp_spv" "stale WGSL artifact")
	require_text(WEBGPU_HOST "${NEEDLE}" "WebGPU catalog mutation host")
endforeach()

message(STATUS "RAL portable shader catalog policy: PASS")
