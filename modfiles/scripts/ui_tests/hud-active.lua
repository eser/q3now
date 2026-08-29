-- SPDX-License-Identifier: GPL-3.0-or-later

ui_test.store("test.lua_harness", "hud-active")
ui_test.assert_store("test.lua_harness", "hud-active")
ui_test.wait(8)
ui_test.assert_rect("classic", "hud_active_crosshair")
ui_test.assert_rect("classic", "hud_active_health_panel")
ui_test.screenshot("lua_hud_active")
