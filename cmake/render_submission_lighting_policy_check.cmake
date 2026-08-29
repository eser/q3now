if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(HEADER "${ROOT}/code/render/frontend/render_submission_lighting.h")
set(SOURCE "${ROOT}/code/render/frontend/render_submission_lighting.c")
set(MATERIAL_SCRIPT_HEADER "${ROOT}/code/render/frontend/render_material_script.h")
set(MATERIAL_SCRIPT_SOURCE "${ROOT}/code/render/frontend/render_material_script.c")
set(MATERIAL_SCRIPT_LIGHTING "${ROOT}/code/render/frontend/render_material_script_lighting.c")
set(LIGHTING_SIDECAR_HEADER "${ROOT}/code/render/frontend/render_lighting_sidecar.h")
set(LIGHTING_SIDECAR_SOURCE "${ROOT}/code/render/frontend/render_lighting_sidecar.c")
file(READ "${ROOT}/code/render/frontend/render_submission_model.h" MODEL_HEADER_TEXT)
file(READ "${ROOT}/code/render/frontend/render_submission.c" SUBMISSION_TEXT)
file(READ "${ROOT}/code/render/frontend/render_submission_material.h" MATERIAL_HEADER_TEXT)
foreach(FILE IN ITEMS "${HEADER}" "${SOURCE}" "${MATERIAL_SCRIPT_HEADER}"
	"${MATERIAL_SCRIPT_SOURCE}" "${MATERIAL_SCRIPT_LIGHTING}"
	"${LIGHTING_SIDECAR_HEADER}" "${LIGHTING_SIDECAR_SOURCE}")
	if(NOT EXISTS "${FILE}")
		message(FATAL_ERROR "missing deterministic lighting extraction surface: ${FILE}")
	endif()
endforeach()
file(READ "${LIGHTING_SIDECAR_HEADER}" LIGHTING_SIDECAR_HEADER_TEXT)
file(READ "${LIGHTING_SIDECAR_SOURCE}" LIGHTING_SIDECAR_SOURCE_TEXT)
foreach(TOKEN IN ITEMS "RENDER_LIGHTING_SIDECAR_MODERN_LOADED"
	"RENDER_LIGHTING_SIDECAR_MISSING_COMPATIBILITY" ".wlight"
	"RenderSubmission_RegisterDirectionalLighting"
	"RenderSubmission_DirectionalLightingSnapshot"
	"RENDER_LIGHTING_SIDECAR_MAX_BYTES")
	string(FIND "${LIGHTING_SIDECAR_HEADER_TEXT}${LIGHTING_SIDECAR_SOURCE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "directional lighting sidecar contract missing ${TOKEN}")
	endif()
endforeach()
foreach(TOKEN IN ITEMS "RENDER_IRRADIANCE_SIDECAR_MODERN_LOADED"
	"RENDER_IRRADIANCE_SIDECAR_MISSING_COMPATIBILITY" ".wprobe"
	"RenderLightingSidecar_PackIrradiance" "RenderLightingSidecar_LoadIrradiance"
	"RenderSubmission_ReplaceIrradianceVolumes" "RENDER_IRRADIANCE_SIDECAR_MAX_BYTES")
	string(FIND "${LIGHTING_SIDECAR_HEADER_TEXT}${LIGHTING_SIDECAR_SOURCE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "irradiance lighting sidecar contract missing ${TOKEN}")
	endif()
endforeach()
file(READ "${MATERIAL_SCRIPT_HEADER}" MATERIAL_SCRIPT_HEADER_TEXT)
file(READ "${MATERIAL_SCRIPT_SOURCE}" MATERIAL_SCRIPT_SOURCE_TEXT)
file(READ "${MATERIAL_SCRIPT_LIGHTING}" MATERIAL_SCRIPT_LIGHTING_TEXT)
foreach(TOKEN IN ITEMS "wiredLighting" "diffuse" "emissive" "mobility"
	"shadowPriority" "proxyCount" "staticBake" "explicitProxyAuthority"
	"injectAtmosphere" "ParseFloatToken" "ParseIntToken" "if ( depth ) return qfalse"
	"if ( !valid )")
	string(FIND "${MATERIAL_SCRIPT_SOURCE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "material lighting authoring grammar missing ${TOKEN}")
	endif()
endforeach()
foreach(TOKEN IN ITEMS "RenderMaterialScript_ApplyLighting"
	"Render_EmissiveMaterialLightingBuild" "RenderSubmission_SetMaterialLighting")
	string(FIND "${MATERIAL_SCRIPT_HEADER_TEXT}${MATERIAL_SCRIPT_LIGHTING_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "material lighting ingestion seam missing ${TOKEN}")
	endif()
endforeach()
foreach(BACKEND IN ITEMS
	"${ROOT}/code/render/ral/backends/vulkan/renderer/tr_init.c"
	"${ROOT}/code/render/ral/backends/opengl/ral_opengl_module.c"
	"${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm"
	"${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c")
	file(READ "${BACKEND}" BACKEND_TEXT)
	string(FIND "${BACKEND_TEXT}" "RenderMaterialScript_ApplyLighting" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "backend material registration lost typed lighting ingestion: ${BACKEND}")
	endif()
	string(FIND "${BACKEND_TEXT}" "RenderLightingSidecar_LoadDirectional" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "backend world load lost directional sidecar ingestion: ${BACKEND}")
	endif()
	string(FIND "${BACKEND_TEXT}" "RenderLightingSidecar_LoadIrradiance" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "backend world load lost irradiance sidecar ingestion: ${BACKEND}")
	endif()
endforeach()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_init.c" VULKAN_MODULE_TEXT)
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_module.c" OPENGL_MODULE_TEXT)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm" METAL_MODULE_TEXT)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c" WEBGPU_MODULE_TEXT)
foreach(TOKEN IN ITEMS "RalVulkan_LightingUpload" "RalVulkan_LightingDestroy")
	string(FIND "${VULKAN_MODULE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "backend world load lost native directional lifecycle: ${TOKEN}")
	endif()
endforeach()
foreach(TOKEN IN ITEMS "RalOpenGl_LightingUpload" "RalOpenGl_LightingDestroy")
	string(FIND "${OPENGL_MODULE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "backend world load lost native directional lifecycle: ${TOKEN}")
	endif()
endforeach()
foreach(TOKEN IN ITEMS "RalMetal_LightingUpload" "RalMetal_LightingDestroy")
	string(FIND "${METAL_MODULE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "backend world load lost native directional lifecycle: ${TOKEN}")
	endif()
endforeach()
foreach(TOKEN IN ITEMS "RalWebGpu_LightingUpload" "RalWebGpu_LightingDestroy")
	string(FIND "${WEBGPU_MODULE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "backend world load lost native directional lifecycle: ${TOKEN}")
	endif()
endforeach()
foreach(TOKEN IN ITEMS "localIrradiance" "lightingComposition"
	"RenderSubmission_AttachEntityIrradiance" "Ral_IrradianceEntitySampleReceiptValid"
	"RenderSubmission_AttachConfiguredEntityIrradiance"
	"Ral_IrradianceVolumeSelect" "RAL_LIGHTING_DIFFUSE_AUTHORITY_LOCAL_SH")
	string(FIND "${MODEL_HEADER_TEXT}${SUBMISSION_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "lighting entity submission contract missing ${TOKEN}")
	endif()
endforeach()
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_scene.c" VULKAN_SCENE_TEXT)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_shade_calc.c" VULKAN_SHADE_TEXT)
foreach(TOKEN IN ITEMS "RE_SetRefEntityLocalIrradiance" "localShQ16"
	"hasLocalIrradiance" "coefficientHash" "r_showIrradianceProbes"
	"FrontendDrawIrradianceDebug" "Ral_IrradianceDebugBuild"
	"debug.selectedProbeCount" "debug.selected" "validityBytes")
	string(FIND "${VULKAN_MODULE_TEXT}${VULKAN_SCENE_TEXT}${VULKAN_SHADE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "Vulkan moving-entity local-SH consumer missing ${TOKEN}")
	endif()
endforeach()
file(READ "${HEADER}" HEADER_TEXT)
file(READ "${SOURCE}" SOURCE_TEXT)
foreach(TOKEN IN ITEMS
	"RenderSubmission_AttachLegacyLightGridEntityIrradiance"
	"LegacyLightGridSize" "LegacyLightGridShiftRgb"
	"RAL_IRRADIANCE_FALLBACK_LIGHTGRID")
	string(FIND "${HEADER_TEXT}${SOURCE_TEXT}${METAL_MODULE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "legacy BSP light-grid compatibility seam missing ${TOKEN}")
	endif()
endforeach()
foreach(TOKEN IN ITEMS
	"RENDER_LIGHTING_EXTRACTION_SCHEMA_VERSION"
	"RenderSubmission_ExtractLightingTriangles"
	"RenderSubmission_LightingExtractionReceiptValid"
	"RENDER_LIGHTING_VISIBILITY_SCHEMA_VERSION"
	"RenderSubmission_BuildLightingVisibility"
	"RenderSubmission_LightingVisibilityReceiptValid"
	"RENDER_EMISSIVE_ROUTING_SCHEMA_VERSION"
	"RenderSubmission_BuildEmissiveRoutes"
	"RenderSubmission_EmissiveRoutingReceiptValid")
	string(FIND "${HEADER_TEXT}${SOURCE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "lighting extraction contract missing ${TOKEN}")
	endif()
endforeach()
foreach(TOKEN IN ITEMS "emissiveMobility" "emissiveInfluenceRangeQ16"
	"emissiveShadowPriority" "emissiveRequestedProxyCount"
	"emissiveExplicitProxyAuthority" "emissiveInjectsAtmosphere")
	string(FIND "${MATERIAL_HEADER_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "typed emissive material authority missing ${TOKEN}")
	endif()
endforeach()
foreach(TOKEN IN ITEMS "RenderSubmission_RegisterIrradianceVolume"
	"RenderSubmission_ReplaceIrradianceVolumes"
	"RenderSubmission_ClearIrradianceVolumes"
	"RenderSubmission_AttachConfiguredEntityIrradiance"
	"RenderSubmission_IrradianceVolumeSnapshot"
	"RenderSubmission_RegisterDirectionalLighting"
	"RenderSubmission_ClearDirectionalLighting"
	"RenderSubmission_DirectionalLightingSnapshot"
	"Ral_IrradianceVolumeSelect" "Ral_IrradianceEntitySample")
	string(FIND "${HEADER_TEXT}${SOURCE_TEXT}" "${TOKEN}" FOUND)
	if(FOUND EQUAL -1)
		message(FATAL_ERROR "frontend irradiance-volume owner missing ${TOKEN}")
	endif()
endforeach()
foreach(FORBIDDEN IN ITEMS "Vk" "MTL" "WGPU" "glBind" "rgba8[")
	string(FIND "${SOURCE_TEXT}" "${FORBIDDEN}" FOUND)
	if(NOT FOUND EQUAL -1)
		message(FATAL_ERROR "lighting extraction must remain backend-neutral and pixel-inference-free: ${FORBIDDEN}")
	endif()
endforeach()
