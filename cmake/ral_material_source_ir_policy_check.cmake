# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/core/ral_material_source.h" HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_material_source.c" CORE)
file(READ "${ROOT}/tests/ral_material_source_test.c" TEST)
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${TEST}")
	foreach(FORBIDDEN IN ITEMS "Vk" "WGPU" "MTL" "SDL_" "lua_")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "material source IR leaked backend/runtime parser type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_MATERIAL_SOURCE_IR_SCHEMA_VERSION"
	"RAL_MATERIAL_PROVENANCE_Q3_SHADER" "RAL_MATERIAL_PROVENANCE_WIRED_LUA"
	"RAL_MATERIAL_SOURCE_MAX_DEPENDENCIES" "ralMaterialSourceDependency_t"
	"ralMaterialSourceSpan_t" "ralMaterialSourceDiagnostic_t" "semanticHash"
	"dependencyHash" "artifactHash")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material source IR contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "CanonicalPathValid" "RAL_MATERIAL_SOURCE_DUPLICATE_DEPENDENCY"
	"HashSemantic" "HashDependencies" "*outReceipt = candidate")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material source compile invariant lost: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "malloc(" "calloc(" "realloc(" "free("
	"Z_Malloc" "Z_TagMalloc" "Z_Free" "Sys_Mutex")
	string(FIND "${CORE}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR
			"material source compiler lost heap-free/reentrant contract: ${FORBIDDEN}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "changed.dependencies[0].sourceId++"
	"RAL_MATERIAL_SOURCE_INVALID_DEPENDENCY" "RAL_MATERIAL_SOURCE_DUPLICATE_DEPENDENCY"
	"RAL_MATERIAL_SOURCE_NONFINITE_VALUE" "!memcmp( &receipt, &before"
	"pthread_create" "CreateThread" "CheckConcurrentCompile"
	"diagnostic.span.line == 12u")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material source hostile fixture lost: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL material source IR policy: PASS")
