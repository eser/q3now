-- menus.lua — Wired UI menu manifest
--
-- Replaces menus.txt. Loaded by WiredUI_Init via WiredScript.
-- load_menu(path) is registered before WiredScript_PostInit so it is
-- available when this file runs inside CL_StartHunkUsers.
--
-- Theme: ui_theme cvar is read via the WiredScript cvar metatable bridge.
-- If ui_theme is empty the default load order applies.

local theme = ui_theme  -- nil if cvar absent or empty string

-- ── Core menus ───────────────────────────────────────────────────────

load_menu("ui/assets.wmenu")
load_menu("ui/main.wmenu")
load_menu("ui/ingame.wmenu")
load_menu("ui/options.wmenu")
load_menu("ui/video.wmenu")
load_menu("ui/display.wmenu")
load_menu("ui/sound.wmenu")
load_menu("ui/controls.wmenu")
load_menu("ui/network.wmenu")
load_menu("ui/preferences.wmenu")
load_menu("ui/playersettings.wmenu")
load_menu("ui/servers.wmenu")
load_menu("ui/startserver.wmenu")
load_menu("ui/demos.wmenu")
load_menu("ui/mods.wmenu")
load_menu("ui/confirm.wmenu")
load_menu("ui/team.wmenu")
load_menu("ui/specifyserver.wmenu")
load_menu("ui/callvote.wmenu")
load_menu("ui/connect.wmenu")
load_menu("ui/password.wmenu")
load_menu("ui/serverinfo.wmenu")
load_menu("ui/addbots.wmenu")
load_menu("ui/removebots.wmenu")
load_menu("ui/teamorders.wmenu")

-- ── Error dialog ─────────────────────────────────────────────────────
-- Must be loaded so CL_WiredUI_ShowError can push it.

load_menu("ui/error_popup.wmenu")

-- ── Theme overrides ───────────────────────────────────────────────────
-- Load theme-specific asset overrides after core menus so they can
-- shadow default assets without replacing the full menu set.

if theme and theme ~= "" then
    load_menu("ui/themes/" .. theme .. "/assets.wmenu")
end

-- ── HUD and scoreboards ───────────────────────────────────────────────
-- HUD file is loaded by WiredUI_Init via the 'hud' cvar (e.g. \hud hud_default)

load_menu("ui/ingame_scoreboard_ffa.wmenu")
load_menu("ui/ingame_scoreboard_duel.wmenu")
load_menu("ui/ingame_scoreboard_tdm.wmenu")
load_menu("ui/ingame_scoreboard_ctf.wmenu")
load_menu("ui/end_scoreboard_ffa.wmenu")
load_menu("ui/end_scoreboard_duel.wmenu")
load_menu("ui/end_scoreboard_tdm.wmenu")
load_menu("ui/end_scoreboard_ctf.wmenu")

-- ── Attract panels ────────────────────────────────────────────────────

load_menu("ui/attract_cinematic.wmenu")
load_menu("ui/attract_leaderboard.wmenu")
