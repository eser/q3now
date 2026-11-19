# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

file(READ "${ROOT}/code/render/frontend/render_material_project_cook.h" HEADER)
file(READ "${ROOT}/code/render/frontend/render_material_project_cook.c" FRONTEND)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_init.c" VULKAN)
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_module.mm" METAL)
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_module.c" OPENGL)
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c" WEBGPU)
foreach(OWNER IN ITEMS FRONTEND VULKAN METAL OPENGL WEBGPU)
	string(FIND "${${OWNER}}" "RenderWorldMaterialUsage_Build(" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "${OWNER}: shared world material reachability missing")
	endif()
endforeach()

function(assert_command_only LABEL TEXT START_MARKER END_MARKER)
	string(FIND "${TEXT}" "${START_MARKER}" START)
	string(FIND "${TEXT}" "${END_MARKER}" END)
	if(START EQUAL -1 OR END EQUAL -1 OR END LESS_EQUAL START)
		message(FATAL_ERROR "${LABEL}: material cook command slice missing")
	endif()
	math(EXPR LENGTH "${END} - ${START}")
	string(SUBSTRING "${TEXT}" ${START} ${LENGTH} SLICE)
	foreach(NEEDLE IN ITEMS "RenderMaterialProjectCook_Execute"
			"RenderMaterialProjectCook_LogTelemetry")
		string(FIND "${SLICE}" "${NEEDLE}" POS)
		if(POS EQUAL -1)
			message(FATAL_ERROR "${LABEL}: command slice missing ${NEEDLE}")
		endif()
	endforeach()
	string(SUBSTRING "${TEXT}" 0 ${START} BEFORE)
	string(SUBSTRING "${TEXT}" ${END} -1 AFTER)
	set(OUTSIDE "${BEFORE}${AFTER}")
	foreach(FORBIDDEN IN ITEMS "RenderMaterialProjectCook_Execute"
			"RenderMaterialProjectCook_LogTelemetry")
		string(FIND "${OUTSIDE}" "${FORBIDDEN}" POS)
		if(NOT POS EQUAL -1)
			message(FATAL_ERROR
				"${LABEL}: ${FORBIDDEN} escaped explicit cook command path")
		endif()
	endforeach()
endfunction()

foreach(NEEDLE IN ITEMS "command-time telemetry"
		"RenderMaterialProjectCook_LogTelemetry")
	string(FIND "${HEADER}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cook telemetry contract missing ${NEEDLE}")
	endif()
endforeach()
foreach(NEEDLE IN ITEMS "RenderMaterialProjectCook_ReceiptValid"
		"material project cook: shaders=" "fallbacks=" "proxies="
		"manifest=%016llx" "corpus=%016llx")
	string(FIND "${FRONTEND}" "${NEEDLE}" POS)
	if(POS EQUAL -1)
		message(FATAL_ERROR "material cook telemetry missing ${NEEDLE}")
	endif()
endforeach()

assert_command_only("Vulkan" "${VULKAN}"
	"static qboolean FrontendCookMaterialProject"
	"static void FrontendBeginFrame")
assert_command_only("Metal" "${METAL}"
	"static qboolean CookMaterialProject"
	"static void NoopBytes")
assert_command_only("OpenGL" "${OPENGL}"
	"static qboolean CookMaterialProject"
	"static qboolean InitializeOwners")

message(STATUS "material project cook command/frame-loop policy: PASS")
