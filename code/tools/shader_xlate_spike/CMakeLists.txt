# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024-present Wired Engine contributors
#
# shader_xlate_spike — Phase 7 pre-flight throwaway. Standalone CMake project
# (NOT wired into the engine's root CMakeLists.txt — keep the spike contained).
#
#   cmake -G Ninja -S code/tools/shader_xlate_spike -B build/shader_xlate_spike \
#         -DCMAKE_BUILD_TYPE=Release
#   ninja -C build/shader_xlate_spike
#   build/shader_xlate_spike/shader_xlate_spike <some.spv> [outdir]
#
# Builds SPIRV-Cross (vendored at code/libs/SPIRV-Cross/) as static libs and
# links shader_xlate_spike against spirv-cross-{core,glsl,msl}. CLI / tests /
# HLSL / C-API of SPIRV-Cross are turned off — only what the spike needs.

cmake_minimum_required(VERSION 3.16)
project(shader_xlate_spike LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# CMake 4.x: SPIRV-Cross still declares cmake_minimum_required(VERSION 3.10),
# which 4.x accepts but warns about; pin a floor so the build is quiet.
set(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "" FORCE)

set(SPIRV_CROSS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/SPIRV-Cross" CACHE PATH
	"Vendored SPIRV-Cross tree")
if(NOT EXISTS "${SPIRV_CROSS_DIR}/CMakeLists.txt")
	message(FATAL_ERROR
		"SPIRV-Cross not found at ${SPIRV_CROSS_DIR}. Vendor it there first "
		"(e.g. extract https://github.com/KhronosGroup/SPIRV-Cross tag "
		"vulkan-sdk-1.4.341.0). See README.md.")
endif()

# Trim SPIRV-Cross to the three backends the spike uses.
set(SPIRV_CROSS_CLI                OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS       OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL        OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP         OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT     OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API       OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL        OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_GLSL        ON  CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL         ON  CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL       ON  CACHE BOOL "" FORCE)
set(SPIRV_CROSS_FORCE_PIC          ON  CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED             OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_STATIC             ON  CACHE BOOL "" FORCE)

add_subdirectory("${SPIRV_CROSS_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/spirv-cross")

add_executable(shader_xlate_spike main.cpp)
target_link_libraries(shader_xlate_spike PRIVATE spirv-cross-msl spirv-cross-glsl spirv-cross-core)
# The vendored SPIRV-Cross headers live at the SPIRV-Cross tree root
# (spirv_glsl.hpp, spirv_msl.hpp, …); add_subdirectory already exports this via
# the targets' PUBLIC include dirs, but be explicit for older CMake.
target_include_directories(shader_xlate_spike PRIVATE "${SPIRV_CROSS_DIR}")
if(MSVC)
	target_compile_options(shader_xlate_spike PRIVATE /EHsc)
endif()
