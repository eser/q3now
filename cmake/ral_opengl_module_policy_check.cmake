# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()
set(H "${ROOT}/code/render/ral/backends/opengl/ral_opengl_module.h")
set(S "${ROOT}/code/render/ral/backends/opengl/ral_opengl_module.c")
set(T "${ROOT}/tests/ral_opengl_module_test.c")
set(C "${ROOT}/CMakeLists.txt")
set(I "${ROOT}/code/render/ral/backends/opengl/ral_opengl_image_decode.c")
foreach(path IN ITEMS "${H}" "${S}" "${T}" "${C}" "${I}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "missing OpenGL renderer-module contract file: ${path}")
	endif()
endforeach()
file(READ "${H}" HEADER)
file(READ "${S}" SOURCE)
file(READ "${T}" TEST)
file(READ "${C}" CMAKE_SOURCE)
file(READ "${I}" IMAGE_SOURCE)

foreach(forbidden IN ITEMS "GLuint" "GLsync" "SDL_Window" "VkDevice"
	"MTLDevice" "WGPUDevice")
	string(FIND "${HEADER}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "OpenGL renderer module header leaked native identity: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS "Q_EXPORT refexport_t *QDECL GetRefAPI"
	"apiVersion != REF_API_VERSION" "s_module.imports.GLimp_InitOpenGL46"
	"s_module.imports.GL_GetProcAddress" "RalOpenGl_CoreCreate"
	"RalOpenGl_ProductCreate" "RalOpenGl_ProductSetOutputExtent"
	"RenderSubmission_Init" "RenderMaterialScript_Load"
	"RenderImage_DecodeRgba8" "RenderSubmission_LightmapMaterialName"
	"R_ScreenshotRegisterCommands" "RalOpenGl_ProductReadbackRgb"
	"R_SaveScreenshotPNG" "\"opengl\"" "CompleteScreenshot"
	"RenderSubmission_LoadWorld" "RenderSubmission_AddEntity"
	"RenderSubmission_AddPoly" "RenderSubmission_RenderScene"
	"RenderSubmission_AddUiQuad" "RenderSubmission_EndFrame"
	"Cvar_Get( \"r_brightness\", \"1.4\""
	"Cvar_CheckRange( s_module.brightness, \"0\", \"32\", CV_FLOAT )"
	"Ral_DisplayVisibilityPlanBuild" "RalOpenGl_ProductSetDisplayVisibility"
	"RalOpenGl_ProductRender" "s_module.imports.GLimp_EndFrame"
	"s_module.currentFrameGeneration" "RalOpenGl_ProductDestroy"
	"RalOpenGl_CoreDestroy" "RenderSubmission_Reset"
	"FillExports( &s_module.exports )")
	string(FIND "${SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "OpenGL renderer module lost ABI/frame seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS "R_LoadPNG" "R_LoadJPG" "R_LoadTGA" "R_LoadBMP"
	"RENDER_SUBMISSION_MAX_IMAGE_DIMENSION" "RENDER_SUBMISSION_MAX_MATERIAL_BYTES"
	"ri.FS_ReadFile" "ri.Free")
	string(FIND "${IMAGE_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "OpenGL VFS image adapter lost decode/bound seam: ${needle}")
	endif()
endforeach()
foreach(forbidden IN ITEMS "renderer/tr_local.h" "renderer2/tr_local.h"
	"renderervk" "ral_vulkan" "ral_metal")
	string(FIND "${SOURCE}${HEADER}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "OpenGL renderer module gained legacy/backend ownership: ${forbidden}")
	endif()
endforeach()
foreach(needle IN ITEMS "dlopen( RAL_OPENGL_TEST_MODULE"
	"dlsym( library, \"GetRefAPI\" )"
	"getRefApi( REF_API_VERSION - 1, &imports ) == NULL"
	"getRefApi( REF_API_VERSION, &imports ) == NULL"
	"exports->BeginRegistration( &config )" "exports->Shutdown( REF_UNLOAD_DLL )"
	"s_shutdownCount == 1u" "memcmp( &receipt, &before, sizeof( receipt ) ) == 0")
	string(FIND "${TEST}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "OpenGL renderer module test lost ABI/fail-closed seam: ${needle}")
	endif()
endforeach()
foreach(needle IN ITEMS
	"ADD_LIBRARY(\${RENDERER_PREFIX}_opengl\${RENDEXT} SHARED"
	"code/render/ral/backends/opengl/ral_opengl_module.c"
	"code/render/ral/backends/opengl/ral_opengl_image_decode.c"
	"code/render/frontend/render_submission.c"
	"code/render/frontend/render_material_script.c"
	"TARGET_LINK_LIBRARIES(\${RENDERER_PREFIX}_opengl\${RENDEXT} PRIVATE ral_opengl)"
	"ADD_LIBRARY(\${RENDERER_PREFIX}_opengl_legacy\${RENDEXT} SHARED"
	"ADD_EXECUTABLE(ral_opengl_module_test tests/ral_opengl_module_test.c)")
	string(FIND "${CMAKE_SOURCE}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "OpenGL module lost canonical/legacy build seam: ${needle}")
	endif()
endforeach()
string(FIND "${CMAKE_SOURCE}"
	"ADD_LIBRARY(\${RENDERER_PREFIX}_opengl\${RENDEXT} SHARED" target_begin)
string(FIND "${CMAKE_SOURCE}" "ADD_EXECUTABLE(ral_opengl_core_test" target_end)
if(target_begin EQUAL -1 OR target_end EQUAL -1 OR target_end LESS target_begin)
	message(FATAL_ERROR "cannot isolate canonical OpenGL renderer target")
endif()
math(EXPR target_len "${target_end}-${target_begin}")
string(SUBSTRING "${CMAKE_SOURCE}" ${target_begin} ${target_len} target_span)
foreach(forbidden IN ITEMS "code/renderer/" "code/renderer2/" "renderervk"
	"ral_vulkan" "ral_metal" "SDL3::SDL3")
	string(FIND "${target_span}" "${forbidden}" pos)
	if(NOT pos EQUAL -1)
		message(FATAL_ERROR "canonical OpenGL module gained compatibility ownership: ${forbidden}")
	endif()
endforeach()
message(STATUS "RAL OpenGL renderer-module policy: PASS")
