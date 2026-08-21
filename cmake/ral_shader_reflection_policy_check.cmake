# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/tools/shader_xlate/main.cpp" TRANSLATOR)
file(READ "${ROOT}/code/renderervk/shaders/compile_reflect.mjs" DRIVER)
file(READ "${ROOT}/code/renderervk/shaders/shader_reflection_abi.mjs" NORMALIZER)
file(READ "${ROOT}/code/renderervk/shaders/spirv/ral_shader_reflection_catalog.json" CATALOG)
file(READ "${ROOT}/tests/ral_shader_reflection_abi_test.mjs" ABI_HOST)
file(READ "${ROOT}/tests/ral_shader_reflection_catalog_test.mjs" CATALOG_HOST)
set(REFLECTION_POLICY_TEXT "${ABI_HOST}${NORMALIZER}")

function(require_text HAYSTACK NEEDLE LABEL)
	string(FIND "${${HAYSTACK}}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "missing ${LABEL}: ${NEEDLE}")
	endif()
endfunction()

require_text(TRANSLATOR "CompilerReflection compiler( words )" "SPIRV-Cross reflection backend")
require_text(TRANSLATOR "--reflect-only" "reflection-only CLI")
require_text(DRIVER "validateSourceCorpus(rows, blobs)" "complete provenance join")
require_text(DRIVER "translatorIdentity(options.translator)" "pinned reflection toolchain identity")
require_text(DRIVER "reflection.stage !== item.stage" "stage exact join")
require_text(DRIVER "reflection catalog is stale" "catalog freshness equality")
foreach(NEEDLE
	"combined-sampler:"
	"runtime-array:"
	"vertex-format:"
	"inline-uniform-binding"
	"buffer-min-size:"
	"storage-access:"
)
	require_text(NORMALIZER "${NEEDLE}" "explicit unresolved lowering")
endforeach()
foreach(NEEDLE
	"RAL_SHADER_BIND_UNIFORM_BUFFER"
	"RAL_SHADER_BIND_STORAGE_BUFFER_READ"
	"RAL_SHADER_BIND_SAMPLED_TEXTURE"
	"RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE"
	"RAL_SHADER_BIND_FILTERING_SAMPLER"
)
	require_text(NORMALIZER "${NEEDLE}" "native-free binding mapping")
endforeach()
foreach(NATIVE "VkDescriptor" "VkPipeline" "WGPU" "MTL")
	string(FIND "${NORMALIZER}${DRIVER}${CATALOG}" "${NATIVE}" NATIVE_POS)
	if(NOT NATIVE_POS EQUAL -1)
		message(FATAL_ERROR "native handle escaped reflection ABI: ${NATIVE}")
	endif()
endforeach()

require_text(CATALOG "\"schemaVersion\":1,\"toolchainIdentity\":\"wired-shader-xlate/2" "pinned reflection corpus header")
require_text(CATALOG "\"sourceCount\":292" "full reflection corpus count")
string(REGEX MATCHALL "\"ordinal\":[0-9]+" CATALOG_ROWS "${CATALOG}")
list(LENGTH CATALOG_ROWS CATALOG_ROW_COUNT)
if(NOT CATALOG_ROW_COUNT EQUAL 292)
	message(FATAL_ERROR "reflection catalog row count drift: ${CATALOG_ROW_COUNT}")
endif()
foreach(NEEDLE
	"assert.equal(committed.sourceCount, 292)"
	"entry.spirv.digest, provenanceRow.artifactDigest"
	"color_vert_spv"
	"brdf_lut_comp_spv"
	"hdr_histogram_comp_spv"
)
	require_text(CATALOG_HOST "${NEEDLE}" "full/representative reflection host gate")
endforeach()
foreach(NEEDLE "duplicate reflected binding" "duplicate vertex location" "invalid push-data reflection"
	"buffer-min-size:0:0" "runtime-array:1:0")
	require_text(REFLECTION_POLICY_TEXT "${NEEDLE}" "reflection ABI mutation gate")
endforeach()

message(STATUS "RAL shader reflection policy: PASS")
