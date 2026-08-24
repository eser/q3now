# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/compile.mjs" COMPILER)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/shaders/shader_artifact_catalog.mjs" CATALOG)
file(READ "${ROOT}/tests/ral_shader_artifact_catalog_test.mjs" HOST)
file(READ "${ROOT}/tests/ral_shader_abi_test.c" C_HOST)

function(require_text HAYSTACK NEEDLE LABEL)
	string(FIND "${${HAYSTACK}}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "missing ${LABEL}: ${NEEDLE}")
	endif()
endfunction()

require_text(CATALOG "RAL_SHADER_SPIRV_ARTIFACT" "native-free generated row")
require_text(CATALOG "glslang:-V" "fixed compiler semantics")
require_text(CATALOG "entry.defines.length" "ordered define provenance")
require_text(CATALOG "sourceBytes.byteLength" "expanded source provenance")
require_text(CATALOG "row.ordinal !== i" "canonical row ordering")
require_text(CATALOG "symbols.has(row.output)" "duplicate symbol rejection")
require_text(HOST "new Array(4097)" "bounded catalog mutation")
require_text(COMPILER "buildShaderArtifactRow(entry, result.sourceBytes, result.spv, ordinal)" "per-entry provenance")
require_text(COMPILER "readFileSync(RAL_ARTIFACT_CATALOG_PATH, 'utf8') !== artifactCatalog" "freshness comparison")
require_text(COMPILER "writeFileSync(RAL_ARTIFACT_CATALOG_PATH, artifactCatalog)" "catalog regeneration")
require_text(C_HOST "artifactVector.lane0 == 0x9ace6ca4ab9ab8d3ull" "C/Node digest compatibility vector")
foreach(MUTATION "stage: 'frag'" "source: 'other.vert'" "defines: ['COUNT=2', 'USE_A']" "changedSource[0]++" "changedSpirv[4]++" "renderShaderArtifactCatalog([second, row])")
	require_text(HOST "${MUTATION}" "catalog mutation host")
endforeach()

message(STATUS "RAL shader artifact catalog policy: PASS")
