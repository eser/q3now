# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Wired Engine contributors

if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()

function(require_text path needle)
  file(READ "${ROOT}/${path}" text)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${path}: missing fixed-center third-person aim seam: ${needle}")
  endif()
endfunction()

function(forbid_text path needle)
  file(READ "${ROOT}/${path}" text)
  string(FIND "${text}" "${needle}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "${path}: forbidden dynamic-reticle seam remains: ${needle}")
  endif()
endfunction()

# Presentation stays centered; world convergence belongs to gameplay.
require_text("code/cgame/wired/cg_wired_bridge.c" "state.crosshair.x = 0.0f;")
require_text("code/cgame/wired/cg_wired_bridge.c" "state.crosshair.y = 0.0f;")
forbid_text("code/cgame/wired/cg_wired_bridge.c" "CG_WorldToScreenPixels")

# Cgame resolves the target from the post-collision camera and transports aim
# independently of viewangles.
require_text("code/cgame/cg_view.c" "CG_UpdateThirdPersonCenterAim")
require_text("code/cgame/cg_view.c" "cg.refdef.viewaxis[0], cameraEnd")
require_text("code/cgame/cg_view.c" "cg.predictedPlayerState.clientNum, MASK_SHOT")
require_text("code/cgame/cg_view.c" "trap_SetUserCmdAim( UCMD_AIM_THIRD_PERSON_CENTER")
require_text("code/cgame/cg_public.h" "CG_SETUSERCMDAIM               = 237")
require_text("code/client/cl_input.c" "cmd->aimMode = clientActiveApp->cl.cgameAimMode;")
require_text("code/client/cl_input.c" "cmd->angles[i] = ANGLE2SHORT(clientActiveApp->cl.viewangles[i]);")
require_text("code/client/cl_cgame.c" "Never let a staged")

# The distinct aim channel is part of the versioned wire contract and survives
# both changed and unchanged delta-usercmd paths.
require_text("code/qcommon/qcommon.h" "#define\tPROTOCOL_VERSION\t75")
require_text("code/qcommon/wired/protocol.h" "int\t\t\t\taimAngles[2]")
require_text("code/qcommon/msg.c" "to->aimMode = MSG_ReadDeltaKey")
require_text("code/qcommon/msg.c" "to->aimMode = from->aimMode;")

# Server-authoritative primary/alt/offhand dispatch shares the same validated
# direction and near-wall muzzle clamp. Player pose consumes that direction too.
require_text("code/game/g_weapon.c" "THIRD_PERSON_CENTER_AIM_MAX_DELTA")
require_text("code/game/g_weapon.c" "G_WeaponAimVectors( ent, forward, right, up );")
require_text("code/game/g_weapon.c" "G_ClampWeaponMuzzle( ent, ent->client->oldOrigin, muzzle );")
require_text("code/game/weapons/g_gauntlet.c" "G_WeaponAimVectors( ent, forward, right, up );")
require_text("code/game/weapons/g_grappling_hook.c" "G_WeaponAimVectors( ent, forward, right, up );")
require_text("code/game/g_active.c" "G_ResolveWeaponAimAngles( ent, weaponAimAngles )")

message(STATUS "fixed-center third-person camera-to-muzzle aim policy: PASS")
