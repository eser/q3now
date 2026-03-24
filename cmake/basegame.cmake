if(NOT BUILD_GAME_LIBRARIES AND NOT BUILD_GAME_QVMS)
    return()
endif()

include(utils/qvm_tools)
include(utils/set_output_dirs)

set(CGAME_SOURCES
    ${SOURCE_DIR}/cgame/cg_main.c
    ${SOURCE_DIR}/game/bg_misc.c
    ${SOURCE_DIR}/game/bg_pmove.c
    ${SOURCE_DIR}/game/bg_slidemove.c
    ${SOURCE_DIR}/game/bg_promode.c
    ${SOURCE_DIR}/game/bg_lib.c
    ${SOURCE_DIR}/cgame/cg_alloc.c
    ${SOURCE_DIR}/cgame/cg_atmospheric.c
    ${SOURCE_DIR}/cgame/cg_chatfilter.c
    ${SOURCE_DIR}/cgame/cg_colorparse.c
    ${SOURCE_DIR}/cgame/cg_lensflare.c
    ${SOURCE_DIR}/cgame/cg_ospcompat.c
    ${SOURCE_DIR}/cgame/cg_osptext.c
    ${SOURCE_DIR}/cgame/cg_window.c
    ${SOURCE_DIR}/cgame/cg_superhud.c
    ${SOURCE_DIR}/cgame/cg_superhud_configparser.c
    ${SOURCE_DIR}/cgame/cg_superhud_private.c
    ${SOURCE_DIR}/cgame/cg_superhud_util.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_ammomessage.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_chat.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_flagstatus.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_followmessage.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_fps.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_fragmessage.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_gametime.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_gametype.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_grid.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_itempickup.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_itempickupicon.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_localtime.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_location.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_name.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_ng.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_ngp.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_obituaries.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_player_name.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_player_stats.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_powerup.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_pred.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_rankmessage.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_reward.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_sbab.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_sbac.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_sbai.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_sbamb.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_sbamc.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_sbami.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_sbhb.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_sbhc.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_sbhi.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_score.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_specmessage.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_spectators.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_speed.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_target_name.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_target_status.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_team.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_teamcount.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_tempAcc.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_vmw.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_weapon_stats.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_warmupinfo.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_weaponlist.c
    ${SOURCE_DIR}/game/bg_tracemap.c
    ${SOURCE_DIR}/cgame/cg_consolecmds.c
    ${SOURCE_DIR}/cgame/cg_draw.c
    ${SOURCE_DIR}/cgame/cg_drawtools.c
    ${SOURCE_DIR}/cgame/cg_effects.c
    ${SOURCE_DIR}/cgame/cg_ents.c
    ${SOURCE_DIR}/cgame/cg_event.c
    ${SOURCE_DIR}/cgame/cg_info.c
    ${SOURCE_DIR}/cgame/cg_localents.c
    ${SOURCE_DIR}/cgame/cg_marks.c
    ${SOURCE_DIR}/cgame/cg_particles.c
    ${SOURCE_DIR}/cgame/cg_players.c
    ${SOURCE_DIR}/cgame/cg_playerstate.c
    ${SOURCE_DIR}/cgame/cg_predict.c
    ${SOURCE_DIR}/cgame/cg_scoreboard.c
    ${SOURCE_DIR}/cgame/cg_servercmds.c
    ${SOURCE_DIR}/cgame/cg_snapshot.c
    ${SOURCE_DIR}/cgame/cg_view.c
    ${SOURCE_DIR}/cgame/cg_weapons.c
    ${SOURCE_DIR}/cgame/cg_znudge.c
    ${SOURCE_DIR}/cgame/cg_superhud_element_netstats.c
)

set(CGAME_BINARY_SOURCES ${SOURCE_DIR}/cgame/cg_syscalls.c)
set(CGAME_QVM_SOURCES ${SOURCE_DIR}/cgame/cg_syscalls.asm)

set(GAME_SOURCES
    ${SOURCE_DIR}/game/g_main.c
    ${SOURCE_DIR}/game/ai_chat.c
    ${SOURCE_DIR}/game/ai_cmd.c
    ${SOURCE_DIR}/game/ai_dmnet.c
    ${SOURCE_DIR}/game/ai_dmq3.c
    ${SOURCE_DIR}/game/ai_main.c
    ${SOURCE_DIR}/game/ai_team.c
    ${SOURCE_DIR}/game/ai_vcmd.c
    ${SOURCE_DIR}/game/bg_misc.c
    ${SOURCE_DIR}/game/bg_pmove.c
    ${SOURCE_DIR}/game/bg_slidemove.c
    ${SOURCE_DIR}/game/bg_promode.c
    ${SOURCE_DIR}/game/bg_lib.c
    ${SOURCE_DIR}/game/g_active.c
    ${SOURCE_DIR}/game/g_arenas.c
    ${SOURCE_DIR}/game/g_bot.c
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
    ${SOURCE_DIR}/game/g_unlagged.c
)

set(GAME_BINARY_SOURCES ${SOURCE_DIR}/game/g_syscalls.c)
set(GAME_QVM_SOURCES ${SOURCE_DIR}/game/g_syscalls.asm)

set(UI_SOURCES
    ${SOURCE_DIR}/q3_ui/ui_main.c
    ${SOURCE_DIR}/game/bg_misc.c
    ${SOURCE_DIR}/game/bg_lib.c
    ${SOURCE_DIR}/q3_ui/ui_addbots.c
    ${SOURCE_DIR}/q3_ui/ui_atoms.c
    ${SOURCE_DIR}/q3_ui/ui_callvote.c
    ${SOURCE_DIR}/q3_ui/ui_cdkey.c
    ${SOURCE_DIR}/q3_ui/ui_dynamicmenu.c
    ${SOURCE_DIR}/q3_ui/ui_confirm.c
    ${SOURCE_DIR}/q3_ui/ui_connect.c
    ${SOURCE_DIR}/q3_ui/ui_controls2.c
    ${SOURCE_DIR}/q3_ui/ui_demo2.c
    ${SOURCE_DIR}/q3_ui/ui_display.c
    ${SOURCE_DIR}/q3_ui/ui_gameinfo.c
    ${SOURCE_DIR}/q3_ui/ui_ingame.c
    ${SOURCE_DIR}/q3_ui/ui_loadconfig.c
    ${SOURCE_DIR}/q3_ui/ui_mapcache.c
    ${SOURCE_DIR}/q3_ui/ui_menu.c
    ${SOURCE_DIR}/q3_ui/ui_mfield.c
    ${SOURCE_DIR}/q3_ui/ui_mods.c
    ${SOURCE_DIR}/q3_ui/ui_network.c
    ${SOURCE_DIR}/q3_ui/ui_options.c
    ${SOURCE_DIR}/q3_ui/ui_playermodel.c
    ${SOURCE_DIR}/q3_ui/ui_players.c
    ${SOURCE_DIR}/q3_ui/ui_playersettings.c
    ${SOURCE_DIR}/q3_ui/ui_preferences.c
    ${SOURCE_DIR}/q3_ui/ui_qmenu.c
    ${SOURCE_DIR}/q3_ui/ui_removebots.c
    ${SOURCE_DIR}/q3_ui/ui_saveconfig.c
    ${SOURCE_DIR}/q3_ui/ui_serverinfo.c
    ${SOURCE_DIR}/q3_ui/ui_servers2.c
    ${SOURCE_DIR}/q3_ui/ui_setup.c
    ${SOURCE_DIR}/q3_ui/ui_sound.c
    ${SOURCE_DIR}/q3_ui/ui_sparena.c
    ${SOURCE_DIR}/q3_ui/ui_specifyserver.c
    ${SOURCE_DIR}/q3_ui/ui_splevel.c
    ${SOURCE_DIR}/q3_ui/ui_sppostgame.c
    ${SOURCE_DIR}/q3_ui/ui_spskill.c
    ${SOURCE_DIR}/q3_ui/ui_startserver.c
    ${SOURCE_DIR}/q3_ui/ui_team.c
    ${SOURCE_DIR}/q3_ui/ui_teamorders.c
    ${SOURCE_DIR}/q3_ui/ui_video.c
    ${SOURCE_DIR}/q3_ui/ui_script.c
)

set(UI_BINARY_SOURCES ${SOURCE_DIR}/ui/ui_syscalls.c)
set(UI_QVM_SOURCES ${SOURCE_DIR}/ui/ui_syscalls.asm)

set(GAME_MODULE_SHARED_SOURCES
    ${SOURCE_DIR}/qcommon/q_math.c
    ${SOURCE_DIR}/qcommon/q_shared.c
)

set(CGAME_SOURCES_BASEGAME ${CGAME_SOURCES} ${GAME_MODULE_SHARED_SOURCES})
set(GAME_SOURCES_BASEGAME ${GAME_SOURCES} ${GAME_MODULE_SHARED_SOURCES})
set(UI_SOURCES_BASEGAME ${UI_SOURCES} ${GAME_MODULE_SHARED_SOURCES})

if(BUILD_GAME_LIBRARIES)
    set(CGAME_MODULE_BINARY ${CGAME_MODULE})
    set(GAME_MODULE_BINARY ${GAME_MODULE})
    set(UI_MODULE_BINARY ${UI_MODULE})

    set(CGAME_MODULE_BINARY_BASEGAME ${CGAME_MODULE_BINARY}_${BASEGAME})
    set(GAME_MODULE_BINARY_BASEGAME ${GAME_MODULE_BINARY}_${BASEGAME})
    set(UI_MODULE_BINARY_BASEGAME ${UI_MODULE_BINARY}_${BASEGAME})

    # Derive ARCH_STRING (as used in q_platform.h / vm.c dylib filename lookup)
    # from RENDEXT (e.g. "_aarch64") by stripping the leading underscore.
    string(SUBSTRING "${RENDEXT}" 1 -1 _GAME_ARCH)

    add_library(                ${CGAME_MODULE_BINARY_BASEGAME} SHARED ${CGAME_SOURCES_BASEGAME} ${CGAME_BINARY_SOURCES})
    target_compile_definitions( ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE CGAME)
    target_link_libraries(      ${CGAME_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${CGAME_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME "${CGAME_MODULE_BINARY}${_GAME_ARCH}" PREFIX "")
    set_output_dirs(            ${CGAME_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

    add_library(                ${GAME_MODULE_BINARY_BASEGAME} SHARED ${GAME_SOURCES_BASEGAME} ${GAME_BINARY_SOURCES})
    target_compile_definitions( ${GAME_MODULE_BINARY_BASEGAME} PRIVATE QAGAME)
    target_link_libraries(      ${GAME_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${GAME_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME "${GAME_MODULE_BINARY}${_GAME_ARCH}" PREFIX "")
    set_output_dirs(            ${GAME_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})

    add_library(                ${UI_MODULE_BINARY_BASEGAME} SHARED ${UI_SOURCES_BASEGAME} ${UI_BINARY_SOURCES})
    target_compile_definitions( ${UI_MODULE_BINARY_BASEGAME} PRIVATE UI)
    target_link_libraries(      ${UI_MODULE_BINARY_BASEGAME} PRIVATE ${COMMON_LIBRARIES})
    set_target_properties(      ${UI_MODULE_BINARY_BASEGAME} PROPERTIES OUTPUT_NAME "${UI_MODULE_BINARY}${_GAME_ARCH}" PREFIX "")
    set_output_dirs(            ${UI_MODULE_BINARY_BASEGAME} SUBDIRECTORY ${BASEGAME})
endif()

if(BUILD_GAME_QVMS)
    set(CGAME_MODULE_QVM_BASEGAME ${CGAME_MODULE}qvm_${BASEGAME})
    set(GAME_MODULE_QVM_BASEGAME ${GAME_MODULE}qvm_${BASEGAME})
    set(UI_MODULE_QVM_BASEGAME ${UI_MODULE}qvm_${BASEGAME})

    add_qvm(${CGAME_MODULE_QVM_BASEGAME}
        DEFINITIONS CGAME
        OUTPUT_NAME ${CGAME_MODULE}
        OUTPUT_DIRECTORY ${BASEGAME}/vm
        SOURCES ${CGAME_SOURCES_BASEGAME} ${CGAME_QVM_SOURCES})

    add_qvm(${GAME_MODULE_QVM_BASEGAME}
        DEFINITIONS QAGAME
        OUTPUT_NAME ${GAME_MODULE}
        OUTPUT_DIRECTORY ${BASEGAME}/vm
        SOURCES ${GAME_SOURCES_BASEGAME} ${GAME_QVM_SOURCES})

    add_qvm(${UI_MODULE_QVM_BASEGAME}
        DEFINITIONS UI
        OUTPUT_NAME ${UI_MODULE}
        OUTPUT_DIRECTORY ${BASEGAME}/vm
        SOURCES ${UI_SOURCES_BASEGAME} ${UI_QVM_SOURCES})
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

    add_wasm(${UI_MODULE}_wasm
        OUTPUT_NAME ${UI_MODULE}
        DEFINITIONS -DUI
        OUTPUT_DIRECTORY ${BASEGAME}/vm
        INCLUDE_DIRECTORIES ${SOURCE_DIR}/q3_ui ${SOURCE_DIR}/game ${SOURCE_DIR}/qcommon
        SOURCES ${UI_SOURCES_BASEGAME} ${UI_BINARY_SOURCES})
endif()
