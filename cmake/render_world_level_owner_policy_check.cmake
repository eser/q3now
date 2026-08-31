# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/code/render/frontend/tr_public.h" public_source)
file(READ "${SOURCE_ROOT}/code/client/cl_main.c" client_source)

function(require_in source_text needle label)
  string(FIND "${source_text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Renderer world-level owner contract missing ${label}: ${needle}")
  endif()
endfunction()

require_in("${public_source}" "REF_API_VERSION\t\t31" "versioned append-only ABI")
require_in("${public_source}" "WorldLevelAlloc)( int worldIndex, size_t size, size_t alignment )" "explicit owner import")
require_in("${public_source}" "WorldLevelUsed)( int worldIndex )" "owner-local accounting import")
require_in("${client_source}" "CL_RefWorldLevelAlloc( int worldIndex" "client allocation bridge")
require_in("${client_source}" "Level_Alloc( &clientApps[worldIndex].memory" "per-app Level arena routing")
require_in("${client_source}" "rimp.WorldLevelAlloc = CL_RefWorldLevelAlloc" "renderer import wiring")
require_in("${client_source}" "Arena_Used( clientApps[worldIndex].memory.levelArena )" "owner-local accounting")
require_in("${client_source}" "rimp.WorldLevelUsed = CL_RefWorldLevelUsed" "accounting import wiring")

message(STATUS "Renderer world-level allocation owner source policy: PASS")
