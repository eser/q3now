# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
	message(FATAL_ERROR "SOURCE_ROOT must name the q3now source tree")
endif()

function(strip_c_comments input output)
	set(clean "${input}")
	# Remove line comments first so disabled-code sentinels such as `//*` and
	# `//*/` cannot masquerade as unterminated block comments.
	string(REGEX REPLACE "//[^\r\n]*" "" clean "${clean}")
	while(TRUE)
		string(FIND "${clean}" "/*" comment_begin)
		if(comment_begin EQUAL -1)
			break()
		endif()
		string(SUBSTRING "${clean}" ${comment_begin} -1 comment_tail)
		string(FIND "${comment_tail}" "*/" comment_end_relative)
		if(comment_end_relative EQUAL -1)
			# Historical id code intentionally comments disabled tails through EOF.
			string(SUBSTRING "${clean}" 0 ${comment_begin} clean)
			break()
		endif()
		math(EXPR comment_end "${comment_begin} + ${comment_end_relative}")
		string(SUBSTRING "${clean}" 0 ${comment_begin} before)
		math(EXPR after_begin "${comment_end} + 2")
		string(SUBSTRING "${clean}" ${after_begin} -1 after)
		set(clean "${before}${after}")
	endwhile()
	set(${output} "${clean}" PARENT_SCOPE)
endfunction()

function(require_text body needle why)
	string(FIND "${body}" "${needle}" hit)
	if(hit EQUAL -1)
		message(FATAL_ERROR "raw API boundary lost ${why}: ${needle}")
	endif()
endfunction()

function(forbid_regex body pattern why)
	string(REGEX MATCH "${pattern}" hit "${body}")
	if(hit)
		message(FATAL_ERROR "raw API boundary leaked ${why}: ${hit}")
	endif()
endfunction()

set(public_abi_path "${SOURCE_ROOT}/code/renderercommon/tr_public.h")
set(client_path "${SOURCE_ROOT}/code/client/client.h")
set(sdl_path "${SOURCE_ROOT}/code/sdl/sdl_glimp.c")
set(vk_boot_path "${SOURCE_ROOT}/code/renderervk/vk_ral_textures.c")
set(vk_path "${SOURCE_ROOT}/code/renderervk/vk.c")
foreach(path IN ITEMS "${public_abi_path}" "${client_path}" "${sdl_path}"
	"${vk_boot_path}" "${vk_path}")
	if(NOT EXISTS "${path}")
		message(FATAL_ERROR "raw API boundary input missing: ${path}")
	endif()
endforeach()

file(READ "${public_abi_path}" public_abi)
file(READ "${client_path}" client)
file(READ "${sdl_path}" sdl)
file(READ "${vk_boot_path}" vk_boot)
file(READ "${vk_path}" vk)
strip_c_comments("${public_abi}" public_code)
strip_c_comments("${client}" client_code)

require_text("${public_abi}" "#define\tREF_API_VERSION\t\t21" "renderer ABI generation")
require_text("${public_abi}" "(*VK_GetInstanceProcAddr)( void *nativeInstance, const char *name )" "opaque proc-loader callback")
require_text("${public_abi}" "(*VK_CreateSurface)( void *nativeInstance, uint64_t *outNativeSurface )" "fixed-width surface callback")
require_text("${client}" "VK_GetInstanceProcAddr( void *nativeInstance, const char *name )" "client opaque proc-loader declaration")
require_text("${client}" "VK_CreateSurface( void *nativeInstance, uint64_t *outNativeSurface )" "client fixed-width surface declaration")
foreach(common_code IN ITEMS "${public_code}" "${client_code}")
	forbid_regex("${common_code}" "(^|[^A-Za-z0-9_])Vk[A-Z][A-Za-z0-9_]*" "Vulkan handle type across renderer ABI")
	forbid_regex("${common_code}" "#[ \t]*include[ \t]*[<\"][^>\"]*vulkan" "Vulkan header across renderer ABI")
endforeach()

require_text("${sdl}" "VK_GetInstanceProcAddr( void *nativeInstance, const char *name )" "SDL opaque proc-loader adapter")
require_text("${sdl}" "qvkGetInstanceProcAddr( (VkInstance)nativeInstance, name )" "SDL-only instance conversion")
require_text("${sdl}" "VK_CreateSurface( void *nativeInstance, uint64_t *outNativeSurface )" "SDL fixed-width surface adapter")
require_text("${sdl}" "SDL_Vulkan_CreateSurface( SDL_window, (VkInstance)nativeInstance," "SDL-only surface conversion")
require_text("${sdl}" "*outNativeSurface = (uint64_t)(uintptr_t)surface;" "SDL surface publication")
require_text("${vk_boot}" "ri.VK_GetInstanceProcAddr( nativeInstance, name )" "native-free RAL host proc forwarding")
require_text("${vk_boot}" "ri.VK_CreateSurface( nativeInstance, outNativeSurface )" "native-free RAL host surface forwarding")
require_text("${vk}" "ri.VK_GetInstanceProcAddr((void *)vk.instance, #func)" "Vulkan migration-boundary proc call")

# Portable/common/game code may not gain native graphics types or SDK includes.
# Backend implementations, the two SDL platform adapters, vendored Vulkan SDK
# headers, legacy GL renderer modules and host-only tools are explicit owners.
# renderervk is the one bounded migration-debt owner; the aggregate caps below
# can only decrease as TASK-202.5 moves code behind RAL.
file(GLOB_RECURSE production_sources
	"${SOURCE_ROOT}/code/*.c" "${SOURCE_ROOT}/code/*.h"
	"${SOURCE_ROOT}/code/*.cpp" "${SOURCE_ROOT}/code/*.hpp"
	"${SOURCE_ROOT}/code/*.m" "${SOURCE_ROOT}/code/*.mm")
set(vk_type_count 0)
set(vk_macro_count 0)
set(qvk_call_count 0)
foreach(path IN LISTS production_sources)
	file(RELATIVE_PATH rel "${SOURCE_ROOT}" "${path}")
	file(READ "${path}" body)
	strip_c_comments("${body}" code)

	if(rel MATCHES "^code/renderervk/")
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])Vk[A-Z][A-Za-z0-9_]*" matches "${code}")
		list(LENGTH matches count)
		math(EXPR vk_type_count "${vk_type_count} + ${count}")
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])VK_[A-Z0-9_]+" matches "${code}")
		list(LENGTH matches count)
		math(EXPR vk_macro_count "${vk_macro_count} + ${count}")
		string(REGEX MATCHALL "(^|[^A-Za-z0-9_])qvk[A-Z][A-Za-z0-9_]*" matches "${code}")
		list(LENGTH matches count)
		math(EXPR qvk_call_count "${qvk_call_count} + ${count}")
		continue()
	endif()

	if(rel MATCHES "^code/renderer/ral_(vulkan|metal|webgpu)/"
		OR rel MATCHES "^code/renderercommon/vulkan/"
		OR rel MATCHES "^code/renderer2/"
		OR rel MATCHES "^code/renderer/[^/]+\\.(c|h|cpp|hpp|m|mm)$"
		OR rel STREQUAL "code/sdl/sdl_glimp.c"
		OR rel STREQUAL "code/sdl/sdl_ral_presentation.mm"
		OR rel MATCHES "^code/tools/")
		continue()
	endif()

	foreach(pattern IN ITEMS
		"#[ \t]*include[ \t]*[<\"][^>\"]*(vulkan|Metal/Metal|webgpu|SDL_opengl|OpenGL/)"
		"(^|[^A-Za-z0-9_])Vk[A-Z][A-Za-z0-9_]*"
		"(^|[^A-Za-z0-9_])qvk[A-Z][A-Za-z0-9_]*"
		"(^|[^A-Za-z0-9_])(MTL[A-Z][A-Za-z0-9_]*|WGPU[A-Z][A-Za-z0-9_]*)"
		"(^|[^A-Za-z0-9_])(GLenum|GLuint|GLint|GLsizei|GLboolean|GLbitfield|GLfloat|GLsync|GLchar|GLintptr|GLsizeiptr)([^A-Za-z0-9_]|$)")
		string(REGEX MATCH "${pattern}" escaped "${code}")
		if(escaped)
			message(FATAL_ERROR "raw graphics API escaped backend/platform ownership in ${rel}: ${escaped}")
		endif()
	endforeach()
endforeach()

set(vk_type_cap 1788)
set(vk_macro_cap 4315)
set(qvk_call_cap 1129)
if(vk_type_count GREATER vk_type_cap OR vk_macro_count GREATER vk_macro_cap
	OR qvk_call_count GREATER qvk_call_cap)
	message(FATAL_ERROR
		"renderervk raw debt grew: types=${vk_type_count}/${vk_type_cap}, "
		"macros=${vk_macro_count}/${vk_macro_cap}, calls=${qvk_call_count}/${qvk_call_cap}")
endif()

set(cmake_path "${SOURCE_ROOT}/CMakeLists.txt")
set(readme_path "${SOURCE_ROOT}/tests/README.md")
if(EXISTS "${cmake_path}" AND EXISTS "${readme_path}")
	file(READ "${cmake_path}" cmake_source)
	file(READ "${readme_path}" readme)
	require_text("${cmake_source}" "ral_raw_api_boundary_source_policy_contract" "registered positive policy")
	require_text("${cmake_source}" "ral_raw_api_boundary_reintroduction_rejected" "registered negative mutation")
	require_text("${readme}" "ral_raw_api_boundary_source_policy_contract" "test inventory row")
endif()

message(STATUS "RAL raw API boundary: native-free ABI 21; renderervk debt ${vk_type_count}/${vk_macro_count}/${qvk_call_count} within monotonic caps")
