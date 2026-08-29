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

load_menu("ui/assets.wui")
load_menu("ui/main.wui")
load_menu("ui/ingame.wui")
load_menu("ui/video.wui")
load_menu("ui/display.wui")
load_menu("ui/sound.wui")
load_menu("ui/controls.wui")
load_menu("ui/network.wui")
load_menu("ui/preferences.wui")
load_menu("ui/playersettings.wui")
load_menu("ui/servers.wui")
load_menu("ui/startserver.wui")
load_menu("ui/campaign.wui")
load_menu("ui/demos.wui")
load_menu("ui/mods.wui")
load_menu("ui/confirm.wui")
load_menu("ui/team.wui")
load_menu("ui/specifyserver.wui")
load_menu("ui/callvote.wui")
load_menu("ui/connect.wui")
load_menu("ui/password.wui")
load_menu("ui/serverinfo.wui")
load_menu("ui/addbots.wui")
load_menu("ui/removebots.wui")
load_menu("ui/teamorders.wui")

-- ── Error dialog ─────────────────────────────────────────────────────
-- Must be loaded so CL_WiredUI_ShowError can push it.

load_menu("ui/error_popup.wui")

-- ── Phase 2 canary (flexbox-first popup) ─────────────────────────────
-- WUI Flexbox Authoring Migration §10 — first panel using container
-- itemDef + design token references. Triggered manually via
-- `/wuimenu_open popup_message` for verification.

load_menu("ui/popup_message.wui")

-- ── Top-layer overlay (cursor + tooltip) ──────────────────────────────
-- WUI_LAYER_OVERLAY — emitted whenever KEYCATCH_UI is on. Loaded
-- explicitly from WiredUI_Init alongside loading_screen.wui; not
-- listed here to avoid double-registration.

-- ── Theme overrides ───────────────────────────────────────────────────
-- Load theme-specific asset overrides after core menus so they can
-- shadow default assets without replacing the full menu set.

if theme and theme ~= "" then
    load_menu("ui/themes/" .. theme .. "/assets.wui")
end

-- ── World viewport (V-19 2026-05-25) ──────────────────────────────────
-- Default WUI_LAYER_WORLD_VIEWPORT panel. Carries a `type viewport` itemDef
-- bound to the provider id "main_scene" registered by CG_Init.

load_menu("ui/world_main.wui")

-- ── Console panel (V-20 2026-05-25) ───────────────────────────────────
-- Default WUI_LAYER_CONSOLE panel. The `type console_view` itemDef
-- dispatches Con_DrawConsole when KEYCATCH_CONSOLE is on (`~` toggle).

load_menu("ui/console_panel.wui")

-- ── HUD and scoreboards ───────────────────────────────────────────────
-- HUD file is loaded by WiredUI_Init via the 'hud' cvar (`hud classic`; legacy
-- `hud default` and `hud hud_default` values are compatibility aliases).

load_menu("ui/ingame_scoreboard_ffa.wui")
load_menu("ui/ingame_scoreboard_duel.wui")
load_menu("ui/ingame_scoreboard_tdm.wui")
load_menu("ui/ingame_scoreboard_ctf.wui")
load_menu("ui/end_scoreboard_ffa.wui")
load_menu("ui/end_scoreboard_duel.wui")
load_menu("ui/end_scoreboard_tdm.wui")
load_menu("ui/end_scoreboard_ctf.wui")

-- ── Attract panels ────────────────────────────────────────────────────

load_menu("ui/attract_brand.wui")
load_menu("ui/attract_demo_overlay.wui")
load_menu("ui/attract_leaderboard.wui")
load_menu("ui/attract_cinematic.wui")
