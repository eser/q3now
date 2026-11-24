# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# r_aa is the only anti-aliasing setting and every renderer maps it through render_antialiasing.h. Each renderer used to derive the
# mapping by hand; Vulkan and Metal then let an r_smaa preset run SMAA with r_aa 0 while OpenGL and WebGPU did not. This check keeps
# the SMAA preset / threshold cvars gone and the four renderers on the shared resolve.

if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT is required")
endif()

set(RENDERERS
	"code/render/ral/backends/vulkan/renderer/vk_init.c"
	"code/render/ral/backends/metal/ral_metal_module.mm"
	"code/render/ral/backends/opengl/ral_opengl_module.c"
	"code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c")
foreach(path IN LISTS RENDERERS)
	file(READ "${ROOT}/${path}" TEXT)
	string(FIND "${TEXT}" "RenderAntialiasing_Resolve(" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "${path} does not map r_aa through RenderAntialiasing_Resolve (render_antialiasing.h)")
	endif()
endforeach()

file(GLOB_RECURSE SOURCES
	"${ROOT}/code/*.c" "${ROOT}/code/*.h" "${ROOT}/code/*.mm" "${ROOT}/code/*.m" "${ROOT}/code/*.cpp" "${ROOT}/code/*.mjs")
foreach(path IN LISTS SOURCES)
	file(READ "${path}" TEXT)
	foreach(name "\"r_smaa\"" "\"r_smaa_threshold\"")
		string(FIND "${TEXT}" "${name}" pos)
		if(NOT pos EQUAL -1)
			message(FATAL_ERROR "${path} registers or reads ${name}: r_aa is the only anti-aliasing setting")
		endif()
	endforeach()
endforeach()
