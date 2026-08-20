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
    ${SOURCE_DIR}/cgame/cg_lensflare.c
    ${SOURCE_DIR}/cgame/cg_utils.c
    ${SOURCE_DIR}/game/bg_tracemap.c
    ${SOURCE_DIR}/cgame/cg_consolecmds.c
    ${SOURCE_DIR}/cgame/cg_draw.c
    ${SOURCE_DIR}/cgame/cg_effects.c
    ${SOURCE_DIR}/cgame/cg_ents.c
    ${SOURCE_DIR}/cgame/cg_event.c
    ${SOURCE_DIR}/cgame/cg_localents.c
    ${SOURCE_DIR}/cgame/cg_marks.c
    ${SOURCE_DIR}/cgame/cg_monster.c
    ${SOURCE_DIR}/cgame/cg_creature.c
    ${SOURCE_DIR}/cgame/cg_particles.c
    ${SOURCE_DIR}/cgame/cg_q1_particles.c
    ${SOURCE_DIR}/cgame/cg_playlist.c
    ${SOURCE_DIR}/cgame/cg_players.c
    ${SOURCE_DIR}/cgame/cg_characters.c
    ${SOURCE_DIR}/cgame/cg_playerstate.c
    ${SOURCE_DIR}/cgame/cg_predict.c
    ${SOURCE_DIR}/cgame/cg_servercmds.c
    ${SOURCE_DIR}/cgame/cg_snapshot.c
    ${SOURCE_DIR}/cgame/cg_temporal_identity.c
    ${SOURCE_DIR}/cgame/cg_view.c
    ${SOURCE_DIR}/cgame/cg_weapons.c
    ${SOURCE_DIR}/cgame/cg_znudge.c
    ${SOURCE_DIR}/cgame/wired/cg_wired_bridge.c
    ${SOURCE_DIR}/cgame/wired/cg_wired_particles.c
    ${SOURCE_DIR}/cgame/wired/cg_wired_store.c
    # cinematic-scene evaluator + spline math run cgame-side (pure plain-C, no
    # engine/RAL/VM dep); the scene POD ships in via trap, cgame runs Eval locally.
    ${SOURCE_DIR}/qcommon/wired/scene/wired_scene_eval.c
    ${SOURCE_DIR}/qcommon/wired/math/wired_curve.c
    # particle curve table + parm evaluator: pure plain-C, split out of the
    # cgame registry so a contract test can compile the exact production
    # code without stubbing cg_local.h.
    ${SOURCE_DIR}/qcommon/wired/render/particle_curve.c
)

set(CGAME_BINARY_SOURCES ${SOURCE_DIR}/cgame/cg_syscalls.c)

set(GAME_SOURCES
    ${SOURCE_DIR}/game/g_main.c
    ${SOURCE_DIR}/game/g_objectives.c
    ${SOURCE_DIR}/game/g_save.c
    ${SOURCE_DIR}/game/g_save_registry.c
    ${SOURCE_DIR}/game/g_save_codec.c
    ${SOURCE_DIR}/game/g_save_file.c
    ${SOURCE_DIR}/game/g_save_serialize.c
    ${SOURCE_DIR}/game/g_save_world.c
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
    ${SOURCE_DIR}/game/g_bot_nav.c
    ${SOURCE_DIR}/game/g_behavior.c
    ${SOURCE_DIR}/game/g_script_verbs.c
    ${SOURCE_DIR}/game/wired/bots/g_bot_scripts.c
    ${SOURCE_DIR}/game/wired/bots/g_wiredintel.c
    ${SOURCE_DIR}/game/wired/bots/g_belief.c
    ${SOURCE_DIR}/game/wired/bots/g_sequenced_goal.c
    ${SOURCE_DIR}/game/g_character.c
    ${SOURCE_DIR}/game/g_client.c
    ${SOURCE_DIR}/game/g_cmds.c
    ${SOURCE_DIR}/game/g_lightstyle.c
    ${SOURCE_DIR}/game/g_combat.c
    ${SOURCE_DIR}/game/g_items.c
    ${SOURCE_DIR}/game/g_mem.c
    ${SOURCE_DIR}/game/entities/g_misc_q3.c
    ${SOURCE_DIR}/game/entities/g_misc_q1.c
    ${SOURCE_DIR}/game/entities/g_trigger_q1.c
    ${SOURCE_DIR}/game/g_missile.c
    ${SOURCE_DIR}/game/entities/g_mover_q3.c
    ${SOURCE_DIR}/game/entities/g_mover_q1.c
    ${SOURCE_DIR}/game/g_session.c
    ${SOURCE_DIR}/game/entities/g_spawn.c
    ${SOURCE_DIR}/game/bg_tracemap.c
    ${SOURCE_DIR}/game/g_stats.c
    ${SOURCE_DIR}/game/g_svcmds.c
    ${SOURCE_DIR}/game/entities/g_target_q3.c
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

set(GAME_MODULE_SHARED_SOURCES
    ${SOURCE_DIR}/qcommon/q_color.c
    ${SOURCE_DIR}/qcommon/q_math.c
    ${SOURCE_DIR}/qcommon/q_shared.c
    ${SOURCE_DIR}/qcommon/q_string.c
    # util/ functions extracted from q_shared.c — must ship with every game module
    ${SOURCE_DIR}/qcommon/util/math.c
    ${SOURCE_DIR}/qcommon/util/hash.c
    ${SOURCE_DIR}/qcommon/util/string.c
)

set(CGAME_SOURCES_BASEGAME ${CGAME_SOURCES} ${GAME_MODULE_SHARED_SOURCES})
set(GAME_SOURCES_BASEGAME ${GAME_SOURCES} ${GAME_MODULE_SHARED_SOURCES})

if(BUILD_GAME_LIBRARIES)
    set(GAMECL_MODULE_BINARY ${GAMECL_MODULE})
    set(GAMESV_MODULE_BINARY ${GAMESV_MODULE})

    set(GAMECL_MODULE_BINARY_BASEGAME ${GAMECL_MODULE_BINARY}_${BASEGAME})
    set(GAMESV_MODULE_BINARY_BASEGAME ${GAMESV_MODULE_BINARY}_${BASEGAME})

    # Derive ARCH_STRING (as used in q_platform.h / vm.c dylib filename lookup)
    # from RENDEXT (e.g. "_aarch64") by stripping the leading underscore.
    string(SUBSTRING "${RENDEXT}" 1 -1 _GAME_ARCH)

    add_library(                ${GAMECL_MODULE_BINARY_BASEGAME} SHARED ${CGAME_SOURCES_BASEGAME} ${CGAME_BINARY_SOURCES})
    target_compile_definitions( ${GAMECL_MODULE_BINARY_BASEGAME} PRIVATE CGAME)
    # WIRED_BUILD_ID/DATE reach this lib via the generated header (wired_build_stamp.h).
    # (Q3NOW_GAMEDATE is intentionally NOT injected here: the existing
    # `="${Q3NOW_GAMEDATE}"` form double-quotes the spaced string on the Ninja/Windows
    # toolchain — verified in build.ninja, it mis-escapes even on the engine libs, so
    # `gamedate` is "unknown" engine-wide. Closing that is a separate quote-safe fix.)
    target_include_directories( ${GAMECL_MODULE_BINARY_BASEGAME} PRIVATE ${SOURCE_DIR}/cgame ${SOURCE_DIR}/qcommon)
    target_link_libraries(      ${GAMECL_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${GAMECL_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME "${GAMECL_MODULE_BINARY}${_GAME_ARCH}" PREFIX "")
    set_output_dirs(            ${GAMECL_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

    add_library(                ${GAMESV_MODULE_BINARY_BASEGAME} SHARED ${GAME_SOURCES_BASEGAME} ${GAME_BINARY_SOURCES})
    target_compile_definitions( ${GAMESV_MODULE_BINARY_BASEGAME} PRIVATE QAGAME)
    # WIRED_BUILD_* via generated header; Q3NOW_GAMEDATE left as-is (see gamecl note).
    target_include_directories( ${GAMESV_MODULE_BINARY_BASEGAME} PRIVATE ${SOURCE_DIR}/game ${SOURCE_DIR}/botlib ${SOURCE_DIR}/qcommon)
    target_link_libraries(      ${GAMESV_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${GAMESV_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME "${GAMESV_MODULE_BINARY}${_GAME_ARCH}" PREFIX "")
    set_output_dirs(            ${GAMESV_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

endif()

if(USE_WASM)
    include(utils/wasm_tools)

    # WIRED_BUILD_ID/DATE reach the wasm modules via the generated header
    # (wired_build_stamp.h is included by g_main.c / cg_main.c, and qcommon is on
    # the wasm INCLUDE_DIRECTORIES). DEFINITIONS unchanged from upstream.
    add_wasm(${GAMESV_MODULE}_wasm
        OUTPUT_NAME ${GAMESV_MODULE}
        DEFINITIONS -DQAGAME
        OUTPUT_DIRECTORY ${BASEGAME}/vm
        INCLUDE_DIRECTORIES ${SOURCE_DIR}/game ${SOURCE_DIR}/botlib ${SOURCE_DIR}/qcommon
        SOURCES ${GAME_SOURCES_BASEGAME} ${GAME_BINARY_SOURCES})

    add_wasm(${GAMECL_MODULE}_wasm
        OUTPUT_NAME ${GAMECL_MODULE}
        DEFINITIONS -DCGAME
        OUTPUT_DIRECTORY ${BASEGAME}/vm
        INCLUDE_DIRECTORIES ${SOURCE_DIR}/cgame ${SOURCE_DIR}/game ${SOURCE_DIR}/qcommon
        SOURCES ${CGAME_SOURCES_BASEGAME} ${CGAME_BINARY_SOURCES})

endif()
