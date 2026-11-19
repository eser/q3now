# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/core/ral_material_cook.h" HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_material_cook.c" CORE)
file(READ "${ROOT}/code/render/ral/core/ral_material_cook_cache.h" CACHE_HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_material_cook_cache.c" CACHE_CORE)
file(READ "${ROOT}/tests/ral_material_cook_test.c" TEST)
file(READ "${ROOT}/tests/ral_material_cook_cache_test.c" CACHE_TEST)
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${CACHE_HEADER}" "${CACHE_CORE}"
	"${TEST}" "${CACHE_TEST}")
	foreach(FORBIDDEN IN ITEMS "Vk" "WGPU" "MTL" "SDL_" "lua_")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "material cook leaked backend/runtime type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_MATERIAL_COOK_CACHE_KEY_TEXT_CAPACITY"
	"writeAtomic" "expectedArtifactHash")
	string(FIND "${CACHE_HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cook cache contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "wrmat-v4-" "Ral_MaterialCookReceiptValid"
	"*outReceipt = *candidate" "free( candidate )")
	string(FIND "${CACHE_CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cook cache invariant lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_MATERIAL_COOK_SCHEMA_VERSION"
	"ralMaterialCookBinding_t" "ralMaterialSourceReceipt_t"
	"ralMaterialReceipt_t" "ralLegacyMaterialReceipt_t"
	"RAL_MATERIAL_COOK_STALE_BINDING" "artifactHash")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cook contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "ValidateBindings" "FindBinding"
	"Ral_MaterialSourceReceiptValid" "Ral_MaterialCompile"
	"Ral_LegacyMaterialResolve" "RAL_MATERIAL_PROVENANCE_WIRED_LUA"
	"RAL_MATERIAL_SOURCE_PBR" "pbrTextures" "calloc( 1u, sizeof( *candidate ) )"
	"*outReceipt = *candidate" "free( candidate )")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cook invariant lost: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "realloc(" "Z_Malloc" "Z_TagMalloc" "Z_Free"
	"Sys_Mutex" "static ralMaterialCookReceipt_t")
	string(FIND "${CORE}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "material cook lost bounded/reentrant contract: ${FORBIDDEN}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_MATERIAL_COOK_MISSING_BINDING"
	"RAL_MATERIAL_COOK_DUPLICATE_BINDING" "RAL_MATERIAL_COOK_STALE_BINDING"
	"!memcmp( &receipt, &before" "Ral_MaterialCookReceiptExact"
	"PbrSource()" "RAL_MATERIAL_SOURCE_PBR" "legacyStageCount == 0u")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cook hostile fixture lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "store.byteLength--" "store.bytes[sizeof"
	"!memcmp( &loaded, &before")
	string(FIND "${CACHE_TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cook cache hostile fixture lost: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "RAL material cook policy: PASS")
