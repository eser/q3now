# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/tools/material_cook/material_cook_fs.c" FILE_CACHE)
file(READ "${ROOT}/code/render/frontend/render_material_cache_vfs.c" VFS_CACHE)
file(READ "${ROOT}/tests/render_material_cook_fs_test.c" TEST)

foreach(NEEDLE IN ITEMS "O_EXCL" "CREATE_NEW" "fsync" "FlushFileBuffers"
	"rename(" "MoveFileExA" "WiredMaterialCookFilesystem_MaterialOps"
	"WiredMaterialCookFilesystem_PackageOps"
	"WiredMaterialCookFilesystem_ProjectReferenceOps"
	"KeyKind( artifactHash, key )")
	string(FIND "${FILE_CACHE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material physical cache contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RenderMaterialCacheVfs_MaterialOps"
	"RenderMaterialCacheVfs_PackageOps" "FS_ResolveResource" "FS_ReadFile"
	"RenderMaterialCacheVfs_ProjectReferenceOps"
	"FS_FreeFile" "strcmp( requested, canonical )"
	"sourceId != verifiedSourceId" "generation != verifiedGeneration"
	"static const ralMaterialCookCacheOps_t ops = { NULL, Read }"
	"static const ralMaterialPackageCacheOps_t ops = { NULL, Read }"
	"NULL, ReadProjectReference")
	string(FIND "${VFS_CACHE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cooked VFS contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "FS_WriteFile" "lua_State" "lua_" "Vk" "WGPU"
	"MTL" "SDL_")
	string(FIND "${VFS_CACHE}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "material cooked VFS crossed boundary: ${FORBIDDEN}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "ConcurrentIdenticalPublish"
	"RenderMaterialPackage_LoadProjectCooked"
	"LoadOnBoundedWorkerStack" "pthread_attr_setstacksize"
	"vfsReads == 9u && vfsFrees == 9u && vfsResolves == 18u"
	"vfsMutateAfterRead = qtrue" "RENDER_MATERIAL_PACKAGE_MANIFEST_LOAD_REJECTED"
	"!memcmp( &vfsProject, &vfsProjectBefore"
	"!memcmp( &vfsLoaded, &vfsBefore")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cache IO hostile fixture lost: ${NEEDLE}")
	endif()
endforeach()
message(STATUS "Wired material cache IO policy: PASS")
