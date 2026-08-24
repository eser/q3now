# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_shader_catalog.mjs" CONSUMER)
file(READ "${ROOT}/tests/ral_metal_shader_catalog_test.mjs" HOST)

function(require_text HAYSTACK NEEDLE LABEL)
	string(FIND "${${HAYSTACK}}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "missing ${LABEL}: ${NEEDLE}")
	endif()
endfunction()

foreach(NEEDLE
	"item.target === 'msl'"
	"portableManifest.byteCount"
	"ralShaderArtifactDigest(bytes)"
	"bindings: portable.reflection.bindings"
	"vertexInputs: portable.reflection.vertexInputs"
	"inlineData: portable.reflection.inlineData"
	"unknown MSL module")
	require_text(CONSUMER "${NEEDLE}" "native-free Metal catalog consumer")
endforeach()
foreach(NATIVE "VkDescriptor" "VkPipeline" "vulkan.h" "spirv_cross" "child_process")
	string(FIND "${CONSUMER}" "${NATIVE}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "Metal catalog consumer leaked runtime/native concept: ${NATIVE}")
	endif()
endforeach()
foreach(NEEDLE "moduleCount, 294" "color_vert_spv" "hdr_histogram_comp_spv" "stale MSL artifact")
	require_text(HOST "${NEEDLE}" "Metal catalog mutation host")
endforeach()

message(STATUS "RAL Metal shader catalog policy: PASS")
