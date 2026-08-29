# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/render/ral/backends/metal/ral_metal_module.h")
set(S "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm")
set(T "${ROOT}/tests/ral_metal_module_test.mm")
set(C "${ROOT}/CMakeLists.txt")
set(P "${ROOT}/code/render/frontend/tr_public.h")
set(I "${ROOT}/code/render/ral/backends/metal/ral_metal_image_decode.mm")
set(M "${ROOT}/code/render/frontend/render_submission_model.h")
set(MS "${ROOT}/code/render/frontend/render_submission_model.c")
foreach(path IN ITEMS "${H}" "${S}" "${T}" "${C}" "${P}" "${I}" "${M}" "${MS}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing Metal renderer-module contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
file(READ "${C}" CMAKE_SOURCE)
file(READ "${P}" PUBLIC_ABI)
file(READ "${I}" IMAGE_SOURCE)
file(READ "${M}" MODEL_HEADER)
file(READ "${MS}" MODEL_SOURCE)
string(REGEX MATCH "#[ \t]*define[ \t]+REF_API_VERSION[ \t]+28([^0-9]|$)"
	ref_api_28 "${PUBLIC_ABI}")
if(NOT ref_api_28)
	message(FATAL_ERROR "Metal renderer module no longer targets REF_API_VERSION 28")
endif()

foreach(forbidden IN ITEMS "CAMetalLayer" "MTLDevice" "SDL_Window" "SDL_MetalView"
	"VkDevice" "VkSwapchain" "WGPUDevice")
	string(FIND "${HEADER}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal renderer module header leaked native identity: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"WIRED_METAL_MODULE_EXPORT refexport_t *QDECL GetRefAPI"
	"apiVersion != REF_API_VERSION" "Ral_PresentationHostImportsValid"
	"RalMetal_CoreCreate" "s_module.imports.PresentationHost.open"
	"s_module.imports.PresentationHost.borrow"
	"RalMetal_PresentAdoptBorrowedLayer" "Ral_FrameShellInit"
	"Ral_FrameShellBegin" "RefreshPresentation"
	"s_module.imports.PresentationHost.refresh"
	"RalMetal_PresentAcquire" "RalMetal_PresentClearAndSubmit"
	"RenderSubmission_LoadWorld" "RenderSubmission_AddEntity"
	"RenderSubmission_AddPoly" "RenderSubmission_RenderScene"
	"RenderSubmission_AddUiQuad" "RenderSubmission_EndFrame"
	"Cvar_Get( \"r_brightness\", \"1\""
	"Cvar_CheckRange( s_module.brightness, \"0\", \"32\", CV_FLOAT )"
	"Ral_DisplayVisibilityPlanBuild" "RalMetal_PresentSetDisplayVisibility"
"RenderImage_DecodeRgba8" "RenderSubmission_RegisterMaterialImage"
"RenderSubmission_SetMaterialNoDraw"
"zero-pass default shader" "placeholder cannot occlude valid geometry"
	"s_module.imports.MetaRemap_Lookup" "REMAP_KIND_SHADER"
	"PrepareWorldMaterials" "RenderSubmission_LightmapMaterialName"
	"RenderSubmission_RegisterModelData" "RenderSubmission_RegisterInlineModel"
	"RenderSubmission_SetModelBatchMaterial" "RenderSubmission_ModelSnapshot"
	"RENDER_ASSET_LIGHTMAP" "_atlas" ".png"
	"Ral_FrameShellComplete"
	"Ral_FrameShellCancel" "Ral_FrameShellShutdown"
	"RalMetal_PresentDestroy( s_module.presentation )"
	"s_module.imports.PresentationHost.close"
	"RalMetal_CoreDestroy( s_module.core )" "FillExports( &s_module.exports )")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal renderer module lost ABI/frame lifecycle seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "RENDER_SUBMISSION_MAX_MODELS"
	"RENDER_SUBMISSION_MAX_MODEL_VERTICES" "RENDER_SUBMISSION_MAX_MODEL_BYTES"
	"renderModelSnapshot_t" "renderEntityCommand_t" "RENDER_MODEL_MD3"
	"RENDER_MODEL_IQM" "DecodeMd3" "DecodeIqm" "ModelRange")
	string(FIND "${MODEL_HEADER}${MODEL_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "neutral model/entity payload lost bounded seam: ${needle}")
	endif()
endforeach()
foreach(forbidden IN ITEMS "<Metal/" "CAMetalLayer" "MTLBuffer" "VkBuffer"
	"WGPUBuffer" "renderer/tr_local.h" "renderer2/tr_local.h")
	string(FIND "${MODEL_HEADER}${MODEL_SOURCE}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "neutral model/entity payload leaked backend/legacy ownership: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS "R_LoadPNG" "R_LoadJPG" "R_LoadTGA" "R_LoadBMP"
	"straight RGBA8" "fully-zero legacy alpha plane"
	"RENDER_SUBMISSION_MAX_MATERIAL_BYTES")
	string(FIND "${IMAGE_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal VFS image adapter lost decode/bound seam: ${needle}")
	endif()
endforeach()
string(FIND "${SOURCE}" "RalMetal_PresentDestroy( s_module.presentation );" destroy_pos)
string(FIND "${SOURCE}" "s_module.imports.PresentationHost.close(" close_pos)
string(FIND "${SOURCE}" "RalMetal_CoreDestroy( s_module.core ); s_module.core = NULL;" core_pos)
if(destroy_pos EQUAL -1 OR close_pos EQUAL -1 OR core_pos EQUAL -1
		OR close_pos LESS destroy_pos OR core_pos LESS close_pos)
	message(FATAL_ERROR "Metal renderer shutdown lost child-before-host-before-core order")
endif()
foreach(needle IN ITEMS
	"dlopen( RAL_METAL_TEST_MODULE" "dlsym( library, \"GetRefAPI\" )"
	"getRefApi( REF_API_VERSION - 1, &imports ) == NULL"
	"getRefApi( REF_API_VERSION, &imports ) == NULL"
	"WiredSdlRalPresentationHost_Create"
	"getRefApi( REF_API_VERSION, &imports )" "BeginRegistration"
	"BeginFrame( STEREO_CENTER )" "EndFrame( &frontEnd, &backEnd )"
	"memcmp( &receipt, &before, sizeof( receipt ) ) == 0"
	"MUTATE( moduleGeneration )" "MUTATE( frameGeneration )"
	"MUTATE( host.surfaceGeneration )" "MUTATE( surface.surfaceIdentity )"
	"MUTATE( frontend.frameDigest )"
	"WiredSdlRalPresentationHost_RequestResize"
	"Shutdown( REF_LEVEL_ONLY )" "Shutdown( REF_KEEP_WINDOW )"
	"Shutdown( REF_UNLOAD_DLL )"
	"exact.host.ownerIdentity == receipt.host.ownerIdentity"
	"exports->EndFrame( NULL, NULL )" "exports->initFailed == qtrue")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal module host lost dynamic ABI/mutation coverage: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"ADD_LIBRARY(\${RENDERER_PREFIX}_metal\${RENDEXT} SHARED"
	"code/render/ral/backends/metal/ral_metal_module.mm"
	"code/render/ral/backends/metal/ral_metal_image_decode.mm"
	"code/render/frontend/render_submission.c"
	"code/render/frontend/render_submission_model.c"
	"code/render/frontend/tr_image_png.c"
	"code/render/frontend/tr_image_jpg.c"
	"code/render/frontend/tr_image_tga.c"
	"code/render/frontend/tr_image_bmp.c"
	"PROPERTIES LANGUAGE C" "PREFIX \"\""
	"ADD_EXECUTABLE(ral_metal_module_test tests/ral_metal_module_test.mm)"
	"ral_sdl_presentation_host"
	"RAL_METAL_TEST_MODULE=\"$<TARGET_FILE:\${RENDERER_PREFIX}_metal\${RENDEXT}>\"")
	string(FIND "${CMAKE_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal module lost narrow build/host seam: ${needle}")
	endif()
endforeach()
string(FIND "${CMAKE_SOURCE}" "ADD_LIBRARY(\${RENDERER_PREFIX}_metal\${RENDEXT} SHARED" target_begin)
string(FIND "${CMAKE_SOURCE}" "IF(USE_OPENGL)" target_end)
if(target_begin EQUAL -1 OR target_end EQUAL -1 OR target_end LESS target_begin)
	message(FATAL_ERROR "cannot isolate Metal renderer module target")
endif()
math(EXPR target_len "${target_end}-${target_begin}")
string(SUBSTRING "${CMAKE_SOURCE}" ${target_begin} ${target_len} target_span)
foreach(forbidden IN ITEMS "ral_vulkan" "MoltenVK"
	"Vulkan.framework" "renderervk" "SDL3::SDL3")
	string(FIND "${target_span}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "Metal renderer module gained compatibility backend/clone: ${forbidden}")
	endif()
endforeach()
message(STATUS "RAL Metal renderer-module policy: PASS")
