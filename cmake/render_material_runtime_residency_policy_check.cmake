# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/render/ral/core/ral_resource.h" RESOURCE_HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_material_runtime.c" RUNTIME_SOURCE)
file(READ "${ROOT}/code/render/renderer/tr_local.h" VK_IMAGE_HEADER)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_image.c" VK_IMAGE_SOURCE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_init.c" VK_FRONTEND_SOURCE)
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_product.c" GL_SOURCE)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_present.mm" METAL_SOURCE)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_product.c" WEBGPU_SOURCE)
file(READ "${ROOT}/code/render/frontend/render_material_runtime_owner.c" OWNER_SOURCE)
file(READ "${ROOT}/code/render/frontend/render_material_runtime_owner.h" OWNER_HEADER)
file(READ "${ROOT}/code/render/frontend/render_material_package.c" PACKAGE_SOURCE)
file(READ "${ROOT}/code/render/frontend/render_material_runtime_catalog.c" CATALOG_SOURCE)
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_module.c" GL_MODULE)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm" METAL_MODULE)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c" WEBGPU_MODULE)

foreach(NEEDLE IN ITEMS
	"RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION 2u"
	"RAL_TEXTURE_SAMPLE_ENCODING_MANUAL_SRGB"
	"sampleEncoding")
	string(FIND "${RESOURCE_HEADER}${RUNTIME_SOURCE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material residency receipt contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"ralTextureCacheKey_t ralTextureCacheKey"
	"ralMaterialRuntimeResource_t ralMaterialRuntimeResource"
	"R_PublishMaterialTextureIdentity"
	"R_RefreshMaterialRuntimeResource"
	"Ral_MaterialRuntimeTextureKeyBuild"
	"Ral_MaterialRuntimeResourceBuild")
	string(FIND "${VK_IMAGE_HEADER}${VK_IMAGE_SOURCE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "Vulkan resident material identity lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"FrontendMaterialBaseImage"
	"RenderSubmission_SetMaterialTextureCacheKey"
	"material-texture-identity")
	string(FIND "${VK_FRONTEND_SOURCE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "Vulkan frontend identity publication lost: ${NEEDLE}")
	endif()
endforeach()
foreach(BACKEND_SOURCE IN ITEMS GL_SOURCE METAL_SOURCE WEBGPU_SOURCE)
	string(FIND "${${BACKEND_SOURCE}}" "Ral_MaterialRuntimeResourceBuild" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "${BACKEND_SOURCE} lost resident material receipt")
	endif()
	string(FIND "${${BACKEND_SOURCE}}" "RAL_TEXTURE_SAMPLE_ENCODING_MANUAL_SRGB" POS)
	if(NOT POS EQUAL -1)
		message(FATAL_ERROR "${BACKEND_SOURCE} falsely claims Vulkan manual sRGB decode")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"renderMaterialRuntimeCatalog_t worlds[MAX_RENDER_WORLDS]"
	"RenderMaterialRuntimeOwner_PublishWorld"
	"RenderMaterialRuntimeOwner_UnloadWorld"
	"RenderMaterialRuntimeOwner_Destroy"
	"RenderMaterialRuntimeCatalog_Destroy( &owner->worlds[worldIndex] )")
	string(FIND "${OWNER_HEADER}${OWNER_SOURCE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "per-world material runtime ownership lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"RalOpenGl_ProductResolveMaterialRuntimeResource"
	"RalMetal_PresentResolveMaterialRuntimeResource"
	"RalWebGpu_ProductResolveMaterialRuntimeResource")
	string(FIND "${GL_SOURCE}${METAL_SOURCE}${WEBGPU_SOURCE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "backend resident material resolver lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS
	"RenderMaterialRuntimeCatalog_ResolveTexture"
	"RalOpenGl_ProductSetMaterialRuntimeCatalog"
	"RalMetal_PresentSetMaterialRuntimeCatalog"
	"RalWebGpu_ProductSetMaterialRuntimeCatalog"
	"RalWebGpu_ProductPrepareMaterialResources")
	string(FIND "${PACKAGE_SOURCE}${CATALOG_SOURCE}${GL_SOURCE}${METAL_SOURCE}${WEBGPU_SOURCE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "production cooked material selection lost: ${NEEDLE}")
	endif()
endforeach()
foreach(MODULE_SOURCE IN ITEMS GL_MODULE METAL_MODULE WEBGPU_MODULE)
	string(FIND "${${MODULE_SOURCE}}" "RenderMaterialRuntimeOwner_World" CATALOG_POS)
	string(FIND "${${MODULE_SOURCE}}" "SetMaterialRuntimeCatalog" SET_POS)
	string(FIND "${${MODULE_SOURCE}}" "RenderMaterialRuntimeOwner_UnloadWorld" UNLOAD_POS)
	if(CATALOG_POS EQUAL -1 OR SET_POS EQUAL -1 OR UNLOAD_POS EQUAL -1)
		message(FATAL_ERROR "${MODULE_SOURCE} lost publish/select/unload cutover")
	endif()
endforeach()
message(STATUS "Wired material runtime residency policy: PASS")
