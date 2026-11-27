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

# SMAA runs on the tone-mapped scene, before the UI (render_antialiasing.h). Vulkan: vk_smaa() after vk_tonemap() at the 3D->2D
# transition. Metal: the tone map renders into smaaColor and SMAA resolves it there, after exposure metering.
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk_backend.c" VK_BACKEND)
string(FIND "${VK_BACKEND}" "vk_tonemap();\n\tif ( R_AntialiasingSmaaQuality() > 0 )\n\t\tvk_smaa();" vk_order)
if(vk_order EQUAL -1)
	message(FATAL_ERROR "Vulkan: SMAA must follow the tone map (vk_tonemap(); then vk_smaa())")
endif()
file(READ "${ROOT}/code/render/ral/backends/metal/ral_metal_present.mm" METAL_PRESENT)
foreach(needle IN ITEMS
		"RalMetal_SmaaEncode( present->smaa, command, present->smaaColor, present->smaaQuality )"
		"[toneMapEncoder setRenderPipelineState:present->smaaToneMapPipeline];")
	string(FIND "${METAL_PRESENT}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "Metal: SMAA must run on the tone-mapped target: missing ${needle}")
	endif()
endforeach()
string(FIND "${METAL_PRESENT}" "RalMetal_SmaaEncode( present->smaa, command, present->sceneColorCache" metal_hdr)
if(NOT metal_hdr EQUAL -1)
	message(FATAL_ERROR "Metal: SMAA must not run on the HDR scene")
endif()
# WebGPU: meter and tone-map into the RGBA16F linear target, SMAA there, copy into the canvas. OpenGL: tone-map into the RGBA16F
# target, SMAA resolves it into the output attachment.
file(READ "${ROOT}/code/render/ral/backends/webgpu/ral_webgpu_product.c" WEBGPU_PRODUCT)
foreach(needle IN ITEMS
		"RalWebGpu_ExposureRecordLinear( product->exposure, command, recording )"
		"RalWebGpu_SmaaRecord( product->smaa, runtimeReceipt, command, recording, linear,"
		"RalWebGpu_ExposureRecordCopy( product->exposure, command, recording, target, resolved )")
	string(FIND "${WEBGPU_PRODUCT}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "WebGPU: SMAA must run on the tone-mapped target: missing ${needle}")
	endif()
endforeach()
string(FIND "${WEBGPU_PRODUCT}" "RalWebGpu_SmaaRecord( product->smaa, runtimeReceipt, command, recording, sceneTarget" webgpu_hdr)
if(NOT webgpu_hdr EQUAL -1)
	message(FATAL_ERROR "WebGPU: SMAA must not run on the HDR scene")
endif()
file(READ "${ROOT}/code/render/ral/backends/opengl/ral_opengl_product.c" GL_PRODUCT)
string(FIND "${GL_PRODUCT}" "RalOpenGl_AaSmaaInto( product->aa, product->smaaToneTexture," gl_tone)
if(gl_tone EQUAL -1)
	message(FATAL_ERROR "OpenGL: SMAA must resolve the tone-mapped target into the output attachment")
endif()
string(FIND "${GL_PRODUCT}" "RalOpenGl_AaSmaa( product->aa, product->hdrTextures[0]" gl_hdr)
if(NOT gl_hdr EQUAL -1)
	message(FATAL_ERROR "OpenGL: SMAA must not run on the HDR scene")
endif()

# Owner decision (2026-11-24): r_aa 0 off, 1 TAA (the default and the recommended mode), 2 SMAA (kept, discouraged: only for players
# whom TAA ghosting bothers). Every renderer registers r_aa with the default 1, and the video menu offers Off / TAA / SMAA with the
# tooltip that recommends TAA.
foreach(path IN LISTS RENDERERS)
	file(READ "${ROOT}/${path}" TEXT)
	string(FIND "${TEXT}" "\"r_aa\", \"1\"" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "${path}: r_aa must default to 1 (TAA)")
	endif()
	string(REGEX MATCH "\"r_aa\", \"[02]\"" other "${TEXT}")
	if(other)
		message(FATAL_ERROR "${path}: r_aa must not default to off or SMAA (${other})")
	endif()
endforeach()
file(READ "${ROOT}/modfiles/ui/video.wui" VIDEO_MENU)
foreach(needle IN ITEMS
		"cvar \"r_aa\""
		"cvarFloatList { \"Off\" 0 \"TAA\" 1 \"SMAA\" 2 }"
		"tooltip \"TAA is recommended. Use SMAA only if TAA ghosting trails bother you.\"")
	string(FIND "${VIDEO_MENU}" "${needle}" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "video.wui: the anti-aliasing row must offer Off / TAA / SMAA and discourage SMAA: missing ${needle}")
	endif()
endforeach()
# `help r_aa` says the same on every renderer: the description lives in render_antialiasing.h.
file(READ "${ROOT}/code/render/frontend/render_antialiasing.h" AA_HEADER)
string(FIND "${AA_HEADER}" "2 SMAA (use it only if TAA ghosting bothers you)" pos)
if(pos EQUAL -1)
	message(FATAL_ERROR "render_antialiasing.h: RENDER_AA_CVAR_DESCRIPTION must recommend TAA and keep SMAA for ghosting only")
endif()
foreach(path IN LISTS RENDERERS)
	file(READ "${ROOT}/${path}" TEXT)
	string(FIND "${TEXT}" "RENDER_AA_CVAR_DESCRIPTION" pos)
	if(pos EQUAL -1)
		message(FATAL_ERROR "${path}: r_aa must carry RENDER_AA_CVAR_DESCRIPTION (help r_aa)")
	endif()
endforeach()
