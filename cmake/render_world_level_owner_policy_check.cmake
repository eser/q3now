# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/code/render/frontend/tr_public.h" public_source)
file(READ "${SOURCE_ROOT}/code/client/cl_main.c" client_source)
file(READ "${SOURCE_ROOT}/code/client/cl_cgame.c" cgame_source)
file(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_map.c" world_source)
file(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_curve.c" curve_source)
file(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/vk_vbo.c" vbo_source)
file(READ "${SOURCE_ROOT}/code/render/ral/backends/vulkan/renderer/tr_init.c" vulkan_module_source)
file(READ "${SOURCE_ROOT}/code/render/ral/backends/metal/ral_metal_module.mm" metal_module_source)
file(READ "${SOURCE_ROOT}/code/render/ral/backends/opengl/ral_opengl_module.c" opengl_module_source)
file(READ "${SOURCE_ROOT}/code/render/ral/backends/webgpu/ral_webgpu_renderer_module.c" webgpu_module_source)

function(require_in source_text needle label)
  string(FIND "${source_text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Renderer world-level owner contract missing ${label}: ${needle}")
  endif()
endfunction()

function(reject_in source_text needle label)
  string(FIND "${source_text}" "${needle}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "Renderer world-level owner contract retained ${label}: ${needle}")
  endif()
endfunction()

function(require_before source_text first second label)
  string(FIND "${source_text}" "${first}" first_pos)
  string(FIND "${source_text}" "${second}" second_pos)
  if(first_pos EQUAL -1 OR second_pos EQUAL -1 OR NOT first_pos LESS second_pos)
    message(FATAL_ERROR "Renderer world-level owner ordering contract failed ${label}: '${first}' before '${second}'")
  endif()
endfunction()

function(require_before_after source_text marker first second label)
  string(FIND "${source_text}" "${marker}" marker_pos)
  if(marker_pos EQUAL -1)
    message(FATAL_ERROR "Renderer world-level owner ordering contract missing ${label} marker: '${marker}'")
  endif()
  string(SUBSTRING "${source_text}" ${marker_pos} -1 scoped_source)
  require_before("${scoped_source}" "${first}" "${second}" "${label}")
endfunction()

require_in("${public_source}" "REF_API_VERSION\t\t32" "versioned append-only ABI")
require_in("${public_source}" "MAX_RENDER_APP_WORLDS       4" "bounded app world range")
require_in("${public_source}" "RENDER_DISTANT_WORLD_BASE   MAX_RENDER_APP_WORLDS" "paired distant-world range")
require_in("${public_source}" "MAX_RENDER_WORLDS           ( MAX_RENDER_APP_WORLDS * 2 )" "non-aliasing world-slot capacity")
require_in("${public_source}" "WorldContentIdentity)( int worldIndex, uint64_t *outIdentity )" "exact world identity export")
require_in("${public_source}" "WorldLevelAlloc)( int worldIndex, size_t size, size_t alignment )" "explicit owner import")
require_in("${public_source}" "WorldLevelUsed)( int worldIndex )" "owner-local accounting import")
require_in("${client_source}" "CL_RefWorldLevelAlloc( int worldIndex" "client allocation bridge")
require_in("${client_source}" "appIndex -= RENDER_DISTANT_WORLD_BASE" "paired slot-to-app owner mapping")
require_in("${client_source}" "Level_Alloc( &clientApps[appIndex].memory" "per-app Level arena routing")
require_in("${client_source}" "rimp.WorldLevelAlloc = CL_RefWorldLevelAlloc" "renderer import wiring")
require_in("${client_source}" "Arena_Used( clientApps[appIndex].memory.levelArena )" "owner-local accounting")
require_in("${client_source}" "rimp.WorldLevelUsed = CL_RefWorldLevelUsed" "accounting import wiring")
foreach(module_source IN ITEMS "${vulkan_module_source}" "${metal_module_source}"
    "${opengl_module_source}" "${webgpu_module_source}")
  require_in("${module_source}" "RenderSubmission_WorldContentIdentity" "backend-neutral identity query")
  require_in("${module_source}" "WorldContentIdentity" "renderer identity export wiring")
endforeach()

# Persistent BSP/patch/VBO metadata is owned by the app's Level arena. Temporary
# upload scratch may continue to use Hunk_AllocateTempMemory, but a process-wide
# persistent Hunk_Alloc would survive a targeted world unload and is forbidden.
require_in("${world_source}" "R_WorldLevelAlloc(" "BSP world allocation cutover")
require_in("${curve_source}" "R_WorldLevelAlloc( size )" "patch-grid allocation cutover")
require_in("${vbo_source}" "R_WorldLevelAlloc(" "VBO metadata allocation cutover")
reject_in("${world_source}" "ri.Hunk_Alloc(" "persistent BSP Hunk allocation")
reject_in("${curve_source}" "ri.Hunk_Alloc(" "persistent patch-grid Hunk allocation")
reject_in("${vbo_source}" "ri.Hunk_Alloc(" "persistent VBO Hunk allocation")

# A targeted unload first quiesces and retires GPU-owned state, then invalidates
# CPU slot metadata. Idempotent empty-slot handling prevents a second destroy.
require_in("${world_source}" "if ( !R_WorldSlotResident( worldIndex ) ) return qtrue;" "idempotent world unload")
require_before_after("${world_source}" "qboolean R_UnloadWorldSlot( int worldIndex )" "vk_cull_unload_world( worldIndex );" "memset( &s_worldDataSlots[worldIndex]" "cull teardown before world slot clear")
require_before_after("${world_source}" "qboolean R_UnloadWorldSlot( int worldIndex )" "VBO_UnloadWorld( worldIndex );" "memset( &s_worldDataSlots[worldIndex]" "GPU teardown before world slot clear")
require_in("${vbo_source}" "if ( vk.active ) vk_wait_idle();" "GPU quiescence before buffer destroy")
require_before_after("${vbo_source}" "void VBO_UnloadWorld( int worldIndex )" "if ( vk.active ) vk_wait_idle();" "Ral_DestroyBuffer( slot->gpu );" "quiescence before RAL buffer destroy")
require_before_after("${vbo_source}" "void VBO_UnloadWorld( int worldIndex )" "Ral_DestroyBuffer( slot->gpu );" "memset( slot, 0" "single destroy before slot invalidation")

# Client map backing storage may be freed only after the renderer relinquishes
# every borrowed pointer. Shared global renderer teardown is reserved for the
# final resident world so an app cannot destroy a sibling's GPU state.
require_before_after("${cgame_source}" "void CL_ShutdownCGame( clientApp_t *app )" "(void)re.UnloadWorld( (int)( app - clientApps ) );" "Map_Free( app->cgameBsp );" "renderer unload before BSP free")
require_before_after("${cgame_source}" "static void CL_UnloadDistantWorld( clientApp_t *app )" "(void)re.UnloadWorld( CL_DistantWorldIndex( app ) );" "Map_Free( app->distantWorldBsp );" "distant renderer unload before BSP free")
require_before_after("${cgame_source}" "void CL_ShutdownCGame( clientApp_t *app )" "CL_UnloadDistantWorld( app );" "Map_Free( app->cgameBsp );" "distant world retired before gameplay backing free")
require_in("${client_source}" "residentWorlds = re.ResidentWorldCount ? re.ResidentWorldCount() : 0;" "resident-world query")
require_in("${client_source}" "rendererWasStarted && residentWorlds == 0 && re.Shutdown" "final-world global shutdown gate")
require_before_after("${client_source}" "void CL_ShutdownLevel( void )" "re.Shutdown( REF_LEVEL_ONLY );" "Level_Reset( &clientActiveApp->memory );" "GPU/global teardown before Level arena reset")

message(STATUS "Renderer world-level allocation owner source policy: PASS")
