# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/frontend/render_material_vfs.c" SOURCE)
foreach(NEEDLE IN ITEMS "RenderMaterialVfs_LoadLuaSourceReceipt"
	"FS_ResolveResource" "FS_ReadFile" "FS_FreeFile"
	"sourceId != verifiedSourceId" "generation != verifiedGeneration"
	"RenderMaterialLua_LoadSourceReceipt")
	string(FIND "${SOURCE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material VFS bridge contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "Vk" "WGPU" "MTL" "SDL_")
	string(FIND "${SOURCE}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "material VFS bridge crossed backend boundary: ${FORBIDDEN}")
	endif()
endforeach()
message(STATUS "Wired material VFS bridge policy: PASS")
