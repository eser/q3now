# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/frontend/render_material_product.h" HEADER)
file(READ "${ROOT}/code/render/frontend/render_material_product.c" SOURCE)
foreach(NEEDLE IN ITEMS "RenderMaterialProduct_CookAndStore"
	"RenderMaterialProduct_LoadCooked" "Ral_MaterialCookCacheStore"
	"Ral_MaterialCookCacheLoad" "renderMaterialProductBindDependencyFn"
	"*outReceipt = *candidate" "free( candidate )")
	string(FIND "${HEADER}${SOURCE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material product contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "lua_" "luaL_" "FS_ReadFile" "FS_FOpenFileRead"
	"Vk" "WGPU" "MTL" "SDL_")
	string(FIND "${SOURCE}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "material product crossed ownership boundary: ${FORBIDDEN}")
	endif()
endforeach()
message(STATUS "Wired material product policy: PASS")
