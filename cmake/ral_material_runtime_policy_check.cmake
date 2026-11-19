# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
file(READ "${ROOT}/code/render/ral/core/ral_material_runtime.h" HEADER)
file(READ "${ROOT}/code/render/ral/core/ral_material_runtime.c" CORE)
file(READ "${ROOT}/tests/ral_material_runtime_test.c" TEST)
foreach(TEXT IN ITEMS "${HEADER}" "${CORE}" "${TEST}")
	foreach(FORBIDDEN IN ITEMS "Vk" "WGPU" "MTL" "SDL_")
		string(FIND "${TEXT}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR "material runtime leaked native type: ${FORBIDDEN}")
		endif()
	endforeach()
endforeach()
foreach(NEEDLE IN ITEMS "RAL_MATERIAL_RUNTIME_SCHEMA_VERSION" "ralMaterialRuntimeBinding_t"
	"ralMaterialRuntimeResource_t" "ralMaterialRuntimeBuildDiagnostic_t"
	"Ral_MaterialRuntimePlanBuildCooked" "ralMaterialDependencyEvent_t"
	"ralMaterialRuntimeSkySource_t"
	"Ral_MaterialRuntimePlanResolveDependencyTexture"
	"Ral_MaterialRuntimeInvalidate")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material runtime contract lost: ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "ResourceMatches" "bindingSlot" "cacheKeyHash"
	"Ral_MaterialCookReceiptValid" "RAL_MATERIAL_RUNTIME_BUILD_MISSING_RESOURCE"
	"RAL_MATERIAL_RUNTIME_BUILD_DUPLICATE_RESOURCE"
	"RAL_MATERIAL_RUNTIME_INVALIDATED" "resources[j].textureIdentity"
	"expectedResourceCount" "candidate.sky.authored"
	"dependency->textureResource.textureIdentity")
	string(FIND "${CORE}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material runtime invariant lost: ${NEEDLE}")
	endif()
endforeach()
	foreach(NEEDLE IN ITEMS "RAL_BACKEND_VULKAN" "RAL_BACKEND_OPENGL"
	"RAL_BACKEND_METAL" "RAL_BACKEND_WEBGPU" "bad[1].textureIdentity"
	"badKeyed[1].cacheKeyHash" "RAL_MATERIAL_RUNTIME_BUILD_INVALID_COOKED"
	"event.textureIdentity=(uintptr_t)0x9999u" "!memcmp(&cohort,&cohortBefore"
	"SkyFixture" "plan.sky.authored.outerDependencies"
	"Ral_MaterialRuntimePlanResolveDependencyTexture")
	string(FIND "${TEST}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material runtime hostile fixture lost: ${NEEDLE}")
	endif()
endforeach()
foreach(MODULE IN ITEMS "metal/ral_metal_module.mm" "opengl/ral_opengl_module.c"
		"webgpu/ral_webgpu_renderer_module.c")
	file(READ "${ROOT}/code/render/ral/backends/${MODULE}" TEXT)
	string(FIND "${TEXT}" "wired_render_pbr_register( &ri, &s_module.pbrDefault )" REGISTER)
	string(FIND "${TEXT}" "static void BeginFrame(" BEGIN)
	if(REGISTER EQUAL -1 OR BEGIN EQUAL -1)
		message(FATAL_ERROR "missing shared PBR registration/frame boundary: ${MODULE}")
	endif()
	string(SUBSTRING "${TEXT}" ${BEGIN} 1100 FRAME)
	string(FIND "${FRAME}" "s_module.frontend.pbrEnabled = s_module.pbr && s_module.pbr->integer != 0" SAMPLE)
	if(SAMPLE EQUAL -1)
		message(FATAL_ERROR "PBR control must be sampled at BeginFrame: ${MODULE}")
	endif()
endforeach()
message(STATUS "RAL material runtime policy: PASS")
