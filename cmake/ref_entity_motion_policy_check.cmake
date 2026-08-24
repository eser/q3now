# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()

function(require_text path needle)
  file(READ "${ROOT}/${path}" text)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${path}: missing required temporal identity seam: ${needle}")
  endif()
endfunction()

require_text("code/render/frontend/tr_public.h" "#define\tREF_API_VERSION\t\t22")
require_text("code/render/frontend/tr_public.h" "AddRefEntityToSceneTemporal")
require_text("code/cgame/cg_public.h" "CG_R_ADDREFENTITYTOSCENETEMPORAL = 232")
require_text("code/client/cl_cgame.c" "&& re.AddRefEntityToSceneTemporal")
require_text("code/client/cl_cgame.c" "VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], sizeof( refEntity_t ) )")
require_text("code/client/cl_cgame.c" "VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[2], sizeof( refEntityMotion_t ) )")
require_text("code/client/cl_cgame.c" "{ VARG_VMPTR, VARG_VMPTR }")
require_text("code/client/cl_cgame.c" "re.AddRefEntityToScene( t[0].p, qfalse )")

file(READ "${ROOT}/code/client/cl_cgame.c" client_source)
string(FIND "${client_source}" "VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[1], sizeof( refEntity_t ) )" entity_bounds)
string(FIND "${client_source}" "VM_CHECKBOUNDS( VM_ActiveNativeVM(), args[2], sizeof( refEntityMotion_t ) )" motion_bounds)
string(FIND "${client_source}" "motion = (const refEntityMotion_t *)t[1].p;" motion_assign)
string(FIND "${client_source}" "RefEntityMotion_IsValid( motion )" motion_validate)
if(entity_bounds LESS 0 OR motion_bounds LESS 0 OR motion_assign LESS 0
		OR motion_validate LESS 0 OR entity_bounds GREATER_EQUAL motion_bounds
		OR motion_bounds GREATER_EQUAL motion_assign
		OR motion_assign GREATER_EQUAL motion_validate)
  message(FATAL_ERROR "client temporal syscall must bounds-check both raw VM pointers before dereferencing motion")
endif()

foreach(renderer_dir IN ITEMS
    "code/renderer"
    "code/renderer2"
    "code/render/ral/backends/vulkan/renderer")
  require_text("${renderer_dir}/tr_scene.c" "RefEntityMotion_ClearOwned")
  require_text("${renderer_dir}/tr_scene.c" "if ( r_numentities == before + 1 )")
  require_text("${renderer_dir}/tr_scene.c" "RefEntityMotion_CopyOwned")
  require_text("${renderer_dir}/tr_init.c" "AddRefEntityToSceneTemporal = RE_AddRefEntityToSceneTemporal")
endforeach()

message(STATUS "temporal entity identity ABI/source policy: PASS")
