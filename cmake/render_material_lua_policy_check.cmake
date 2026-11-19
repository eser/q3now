# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/frontend/render_material_lua.h" HEADER)
file(READ "${ROOT}/code/render/frontend/render_material_lua.c" SOURCE)
file(READ "${ROOT}/code/render/ral/core/ral_material_source.c" CORE)
foreach(NEEDLE IN ITEMS "RenderMaterialLua_ReadTable"
	"RenderMaterialLua_Load" "RenderMaterialLua_LoadSourceReceipt"
	"Ral_MaterialSourceCompile" "renderMaterialLuaSourceOps_t"
	"renderMaterialLuaResolveDependencyFn" "RAL_MATERIAL_PROVENANCE_WIRED_LUA"
	"pbrTextures" "creation_options" "luaL_loadbuffer" "lua_pcall"
	"sourceOps->release" "*outSource = candidate")
	string(FIND "${HEADER}${SOURCE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "Wired material Lua frontend contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "Vk" "WGPU" "MTL" "SDL_" "FS_FOpenFileRead")
	string(FIND "${SOURCE}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "material Lua frontend crossed ownership boundary: ${FORBIDDEN}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "lua_" "luaL_")
	string(FIND "${CORE}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "Lua leaked into canonical material core: ${FORBIDDEN}")
	endif()
endforeach()
message(STATUS "Wired material Lua frontend policy: PASS")
