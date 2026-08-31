# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(MAP_SOURCE "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_map.c")
file(READ "${MAP_SOURCE}" map_source)

function(require_text needle label)
  string(FIND "${map_source}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Vulkan per-world state contract missing ${label}: ${needle}")
  endif()
endfunction()

function(reject_text needle label)
  string(FIND "${map_source}" "${needle}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "Vulkan per-world state contract contains ${label}: ${needle}")
  endif()
endfunction()

require_text("worldMapState_t s_worldMapStates[ MAX_RENDER_WORLDS ]" "bounded state slots")
require_text("R_SaveWorldMapState( s_activeWorldIndex )" "outgoing state publication")
require_text("R_ApplyWorldMapState( worldIndex )" "incoming state activation")
require_text("state->lightmaps = tr.lightmaps" "lightmap ownership")
require_text("state->lightstyleValues" "lightstyle ownership")
require_text("state->viewCluster = tr.viewCluster" "visibility ownership")
require_text("VectorCopy( tr.sunDirection, state->sunDirection )" "sun ownership")
require_text("state->globalFog = tr.globalFog" "fog ownership")
require_text("s_pendingWorldVisData = vis" "next-world visibility staging")
require_text("memset( &s_worldMapStates[worldIndex], 0" "targeted state retirement")
reject_text("state->sunShader" "renderer-global shader registry pointer")

message(STATUS "Vulkan per-world map state source policy: PASS")
