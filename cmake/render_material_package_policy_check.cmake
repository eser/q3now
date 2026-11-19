# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/frontend/render_material_package.h" HEADER)
file(READ "${ROOT}/code/render/frontend/render_material_package.c" SOURCE)
file(READ "${ROOT}/code/render/frontend/render_material_runtime_catalog.c" CATALOG_SOURCE)
file(READ "${ROOT}/code/render/ral/core/ral_material_package.h" CORE_HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_material_package.c" CORE_SOURCE)
set(ALL_TEXT "${HEADER}\n${SOURCE}\n${CATALOG_SOURCE}\n${CORE_HEADER}\n${CORE_SOURCE}")
foreach(NEEDLE IN ITEMS "RenderMaterialPackage_CookAndPublish"
	"RenderMaterialPackage_LoadCooked"
	"RenderMaterialPackage_LoadProjectRuntimePlan"
	"RenderMaterialPackage_LoadProjectRuntimeCatalog"
	"RenderMaterialRuntimeCatalog_Destroy"
	"RenderMaterialRuntimeCatalog_Lookup"
	"renderMaterialRuntimeResolveResourceFn"
	"Ral_MaterialRuntimePlanBuildCooked" "Ral_MaterialPackageCacheStore"
	"Ral_MaterialPackageCacheLoad" "wrmat-package-v1-"
	"RAL_MATERIAL_PACKAGE_MAX_ENTRIES" "expectedManifestHash")
	string(FIND "${ALL_TEXT}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "missing material package contract: ${NEEDLE}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "lua_State" "vulkan.h" "VkDevice" "id<")
	string(FIND "${ALL_TEXT}" "${FORBIDDEN}" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "material package leaked forbidden surface: ${FORBIDDEN}")
	endif()
endforeach()
