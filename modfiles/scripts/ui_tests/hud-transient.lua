-- SPDX-License-Identifier: GPL-3.0-or-later

ui_test.store("test.lua_harness", "hud-transient")
ui_test.assert_store("test.lua_harness", "hud-transient")
ui_test.key(50)
ui_test.wait(8)
ui_test.assert_rect("classic", "hud_weapon_carousel")
ui_test.assert_rect("classic", "hud_msgqueue")
ui_test.screenshot("lua_hud_transient")
