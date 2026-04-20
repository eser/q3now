if(NOT BUILD_GAME_LIBRARIES)
    return()
endif()

include(utils/set_output_dirs)

set(CGAME_SOURCES
    ${SOURCE_DIR}/cgame/cg_main.c
    ${SOURCE_DIR}/game/bg_misc.c
    ${SOURCE_DIR}/game/bg_weapons.c
    ${SOURCE_DIR}/game/bg_gametypes.c
    ${SOURCE_DIR}/game/bg_pmove.c
    ${SOURCE_DIR}/game/bg_slidemove.c
    ${SOURCE_DIR}/cgame/cg_alloc.c
    ${SOURCE_DIR}/cgame/cg_atmospheric.c
    ${SOURCE_DIR}/cgame/cg_chatfilter.c
    ${SOURCE_DIR}/cgame/cg_colorparse.c
    ${SOURCE_DIR}/cgame/cg_lensflare.c
    ${SOURCE_DIR}/cgame/cg_utils.c
    ${SOURCE_DIR}/cgame/cg_window.c
    ${SOURCE_DIR}/game/bg_tracemap.c
    ${SOURCE_DIR}/cgame/cg_consolecmds.c
    ${SOURCE_DIR}/cgame/cg_draw.c
    ${SOURCE_DIR}/cgame/cg_effects.c
    ${SOURCE_DIR}/cgame/cg_ents.c
    ${SOURCE_DIR}/cgame/cg_event.c
    ${SOURCE_DIR}/cgame/cg_localents.c
    ${SOURCE_DIR}/cgame/cg_marks.c
    ${SOURCE_DIR}/cgame/cg_particles.c
    ${SOURCE_DIR}/cgame/cg_players.c
    ${SOURCE_DIR}/cgame/cg_playerstate.c
    ${SOURCE_DIR}/cgame/cg_predict.c
    ${SOURCE_DIR}/cgame/cg_servercmds.c
    ${SOURCE_DIR}/cgame/cg_snapshot.c
    ${SOURCE_DIR}/cgame/cg_view.c
    ${SOURCE_DIR}/cgame/cg_weapons.c
    ${SOURCE_DIR}/cgame/cg_znudge.c
    ${SOURCE_DIR}/cgame/wired/cg_wired_bridge.c
    ${SOURCE_DIR}/cgame/wired/cg_wired_store.c
)

set(CGAME_BINARY_SOURCES ${SOURCE_DIR}/cgame/cg_syscalls.c)

set(GAME_SOURCES
    ${SOURCE_DIR}/game/g_main.c
    ${SOURCE_DIR}/game/ai_aware.c
    ${SOURCE_DIR}/game/ai_chat.c
    ${SOURCE_DIR}/game/ai_cmd.c
    ${SOURCE_DIR}/game/ai_dmnet.c
    ${SOURCE_DIR}/game/ai_dmq3.c
    ${SOURCE_DIR}/game/ai_dodge.c
    ${SOURCE_DIR}/game/ai_itemtime.c
    ${SOURCE_DIR}/game/ai_main.c
    ${SOURCE_DIR}/game/ai_movement.c
    ${SOURCE_DIR}/game/ai_team.c
    ${SOURCE_DIR}/game/ai_vcmd.c
    ${SOURCE_DIR}/game/ai_weapsel.c
    ${SOURCE_DIR}/game/bg_misc.c
    ${SOURCE_DIR}/game/bg_weapons.c
    ${SOURCE_DIR}/game/bg_gametypes.c
    ${SOURCE_DIR}/game/bg_pmove.c
    ${SOURCE_DIR}/game/bg_slidemove.c
    ${SOURCE_DIR}/game/g_active.c
    ${SOURCE_DIR}/game/g_arenas.c
    ${SOURCE_DIR}/game/g_bot.c
    ${SOURCE_DIR}/game/wired/bots/g_bot_scripts.c
    ${SOURCE_DIR}/game/wired/bots/g_wiredbots.c
    ${SOURCE_DIR}/game/g_character.c
    ${SOURCE_DIR}/game/g_client.c
    ${SOURCE_DIR}/game/g_cmds.c
    ${SOURCE_DIR}/game/g_combat.c
    ${SOURCE_DIR}/game/g_items.c
    ${SOURCE_DIR}/game/g_mem.c
    ${SOURCE_DIR}/game/g_misc.c
    ${SOURCE_DIR}/game/g_missile.c
    ${SOURCE_DIR}/game/g_mover.c
    ${SOURCE_DIR}/game/g_session.c
    ${SOURCE_DIR}/game/g_spawn.c
    ${SOURCE_DIR}/game/bg_tracemap.c
    ${SOURCE_DIR}/game/g_stats.c
    ${SOURCE_DIR}/game/g_svcmds.c
    ${SOURCE_DIR}/game/g_target.c
    ${SOURCE_DIR}/game/g_team.c
    ${SOURCE_DIR}/game/g_trigger.c
    ${SOURCE_DIR}/game/g_utils.c
    ${SOURCE_DIR}/game/g_weapon.c
    ${SOURCE_DIR}/game/weapons/g_gauntlet.c
    ${SOURCE_DIR}/game/weapons/g_machinegun.c
    ${SOURCE_DIR}/game/weapons/g_shotgun.c
    ${SOURCE_DIR}/game/weapons/g_grenade_launcher.c
    ${SOURCE_DIR}/game/weapons/g_rocket_launcher.c
    ${SOURCE_DIR}/game/weapons/g_plasma_rifle.c
    ${SOURCE_DIR}/game/weapons/g_railgun.c
    ${SOURCE_DIR}/game/weapons/g_lightning_gun.c
    ${SOURCE_DIR}/game/weapons/g_grappling_hook.c
    ${SOURCE_DIR}/game/g_unlagged.c
)

set(GAME_BINARY_SOURCES ${SOURCE_DIR}/game/g_syscalls.c)

# Legacy UI module (q3_ui) removed — Wired UI replaces it

set(GAME_MODULE_SHARED_SOURCES
    ${SOURCE_DIR}/qcommon/q_math.c
    ${SOURCE_DIR}/qcommon/q_shared.c
    ${SOURCE_DIR}/qcommon/q_string.c
)

set(CGAME_SOURCES_BASEGAME ${CGAME_SOURCES} ${GAME_MODULE_SHARED_SOURCES})
set(GAME_SOURCES_BASEGAME ${GAME_SOURCES} ${GAME_MODULE_SHARED_SOURCES})

if(BUILD_GAME_LIBRARIES)
    set(CGAME_MODULE_BINARY ${CGAME_MODULE})
    set(GAME_MODULE_BINARY ${GAME_MODULE})

    set(CGAME_MODULE_BINARY_BASEGAME ${CGAME_MODULE_BINARY}_${BASEGAME})
    set(GAME_MODULE_BINARY_BASEGAME ${GAME_MODULE_BINARY}_${BASEGAME})

    # Derive ARCH_STRING (as used in q_platform.h / vm.c dylib filename lookup)
    # from RENDEXT (e.g. "_aarch64") by stripping the leading underscore.
    string(SUBSTRING "${RENDEXT}" 1 -1 _GAME_ARCH)

    add_library(                ${CGAME_MODULE_BINARY_BASEGAME} SHARED ${CGAME_SOURCES_BASEGAME} ${CGAME_BINARY_SOURCES})
    target_compile_definitions( ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE CGAME)
    target_include_directories( ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE ${SOURCE_DIR}/cgame)
    target_link_libraries(      ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${CGAME_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME "${CGAME_MODULE_BINARY}${_GAME_ARCH}" PREFIX "")
    set_output_dirs(            ${CGAME_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

    add_library(                ${GAME_MODULE_BINARY_BASEGAME} SHARED ${GAME_SOURCES_BASEGAME} ${GAME_BINARY_SOURCES})
    target_compile_definitions( ${GAME_MODULE_BINARY_BASEGAME} PRIVATE QAGAME)
    target_include_directories( ${GAME_MODULE_BINARY_BASEGAME} PRIVATE ${SOURCE_DIR}/game ${SOURCE_DIR}/botlib)
    target_link_libraries(      ${GAME_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${GAME_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME "${GAME_MODULE_BINARY}${_GAME_ARCH}" PREFIX "")
    set_output_dirs(            ${GAME_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

endif()

if(USE_WASM)
    include(utils/wasm_tools)

    add_wasm(${GAME_MODULE}_wasm
        OUTPUT_NAME ${GAME_MODULE}
        DEFINITIONS -DQAGAME
        OUTPUT_DIRECTORY ${BASEGAME}/vm
        INCLUDE_DIRECTORIES ${SOURCE_DIR}/game ${SOURCE_DIR}/botlib ${SOURCE_DIR}/qcommon
        SOURCES ${GAME_SOURCES_BASEGAME} ${GAME_BINARY_SOURCES})

    add_wasm(${CGAME_MODULE}_wasm
        OUTPUT_NAME ${CGAME_MODULE}
        DEFINITIONS -DCGAME
        OUTPUT_DIRECTORY ${BASEGAME}/vm
        INCLUDE_DIRECTORIES ${SOURCE_DIR}/cgame ${SOURCE_DIR}/game ${SOURCE_DIR}/qcommon
        SOURCES ${CGAME_SOURCES_BASEGAME} ${CGAME_BINARY_SOURCES})

endif()
