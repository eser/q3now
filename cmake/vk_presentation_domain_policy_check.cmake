# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

cmake_minimum_required(VERSION 3.16)
if(NOT DEFINED ROOT)
	message(FATAL_ERROR "ROOT required")
endif()

file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/tr_scene.c" SCENE)
file(READ "${ROOT}/code/render/ral/backends/vulkan/renderer/vk.c" VK)

function(require_text variable needle label)
	string(FIND "${${variable}}" "${needle}" position)
	if(position EQUAL -1)
		message(FATAL_ERROR "Vulkan presentation-domain policy lost ${label}: ${needle}")
	endif()
endfunction()

function(forbid_text variable needle label)
	string(FIND "${${variable}}" "${needle}" position)
	if(NOT position EQUAL -1)
		message(FATAL_ERROR "Vulkan presentation-domain policy regained ${label}: ${needle}")
	endif()
endfunction()

# refdef rectangles originate in the presentation-pixel domain.  The Y-axis
# conversion must stay in that domain until the Vulkan render-pass boundary.
require_text(SCENE
	"parms.viewportY = gls.windowHeight\n\t\t- ( tr.refdef.y + tr.refdef.height );"
	"presentation-domain top-left conversion")
forbid_text(SCENE
	"parms.viewportY = glConfig.vidHeight - ( tr.refdef.y + tr.refdef.height );"
	"render-domain refdef conversion")

# A reduced scene attachment scales the complete presentation rectangle once.
require_text(VK
	"(float)vk.renderWidth / (float)gls.windowWidth"
	"horizontal presentation-to-render scaling")
require_text(VK
	"(float)vk.renderHeight / (float)gls.windowHeight"
	"vertical presentation-to-render scaling")
require_text(VK
	"static void vk_render_domain_viewport_scissor( ralViewport_t *viewport,"
	"shared procedural render-domain viewport helper")
require_text(VK
	"viewport->width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;"
	"procedural horizontal viewport scaling")
require_text(VK
	"viewport->height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;"
	"procedural vertical viewport scaling")
string(REGEX MATCHALL
	"vk_render_domain_viewport_scissor\\( &plan.viewport, &plan.scissor \\)"
	POOL_VIEWPORT_CALLS "${VK}")
list(LENGTH POOL_VIEWPORT_CALLS POOL_VIEWPORT_CALL_COUNT)
if(NOT POOL_VIEWPORT_CALL_COUNT EQUAL 3)
	message(FATAL_ERROR
		"Vulkan presentation-domain policy requires particle/decal/atmosphere pool viewport scaling; found ${POOL_VIEWPORT_CALL_COUNT} calls")
endif()

# Screen-space effect fragments address the scene-depth attachment with
# gl_FragCoord, which is expressed in the active render attachment domain.
# Feeding presentation dimensions here makes decals and soft particles sample
# unrelated depth texels whenever render and presentation extents differ.
require_text(VK
	"frameDst->reconParams[0] = ( vk.renderWidth  > 0 ) ? 1.0f / (float)vk.renderWidth"
	"decal render-domain depth reconstruction width")
require_text(VK
	"frameDst->reconParams[1] = ( vk.renderHeight > 0 ) ? 1.0f / (float)vk.renderHeight"
	"decal render-domain depth reconstruction height")
require_text(VK
	"frameDst->invResX     = ( vk.renderWidth  > 0 ) ? 1.0f / (float)vk.renderWidth"
	"particle render-domain depth sampling width")
require_text(VK
	"frameDst->invResY     = ( vk.renderHeight > 0 ) ? 1.0f / (float)vk.renderHeight"
	"particle render-domain depth sampling height")
forbid_text(VK
	"frameDst->reconParams[0] = ( glConfig.vidWidth"
	"presentation-sized decal reconstruction")
forbid_text(VK
	"frameDst->invResX     = ( glConfig.vidWidth"
	"presentation-sized particle depth sampling")

# MAIN is entered three ways: initial clear, scene-depth LOAD resume, and
# Forward+ LOAD resume.  All three must restore the same render domain; unit
# scale at either resume separates additive model parts from opaque geometry.
require_text(VK
	"static void vk_set_main_render_domain( void )"
	"shared MAIN render-domain helper")
string(REGEX MATCHALL "vk_set_main_render_domain\\(\\)" MAIN_DOMAIN_CALLS "${VK}")
list(LENGTH MAIN_DOMAIN_CALLS MAIN_DOMAIN_CALL_COUNT)
if(NOT MAIN_DOMAIN_CALL_COUNT EQUAL 3)
	message(FATAL_ERROR
		"Vulkan presentation-domain policy requires exactly three MAIN-domain entries; found ${MAIN_DOMAIN_CALL_COUNT}")
endif()
forbid_text(VK
	"vk.renderPassIndex = RENDER_PASS_MAIN; // still in \"main\" context for pipeline compatibility\n\t\tvk.renderWidth = glConfig.vidWidth;\n\t\tvk.renderHeight = glConfig.vidHeight;\n\t\tvk.renderScaleX = vk.renderScaleY = 1.0f;"
	"unit-scaled scene-depth resume")

message(STATUS "Vulkan presentation-domain policy: refdefs remain presentation-sized and scale once at the render pass")
