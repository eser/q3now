-- English (en) localization table.
--
-- Maps l10n keys -> localized text. A key is the sound-name-as-subtitle-key:
-- the same string a WiredScene caption event carries (WSCENE_EV_CAPTION's
-- sparam), so one string is both the sound cue and the subtitle lookup. A later
-- phase passes a caption's key through this table to get the text to render.
--
-- Loaded by cl_wired_l10n.c on the System VM at load time into a plain-C
-- key->text hash. A missing key falls back to the raw key at lookup, so an
-- untranslated caption shows the key rather than blanking.
--
-- The keys below match the caption events in the shipped scene fixtures
-- (scripts/scene/cinematic.lua, arena1.lua) so the loader has real content.

return {
  ["scene/cinematic/intro"] = "The complex is silent now. Whatever waits here, it knows we came.",
  ["scene/intro/line1"]     = "Wake up. The arena is live.",
  ["scene/arena1/greet"]    = "Welcome to the proving grounds.",

  -- Mission objectives (same key space; resolved for the objectives HUD).
  ["objective/reach_exit"]  = "Reach the exit portal",
  ["objective/find_key"]    = "Recover the security key",
  ["objective/eliminate"]   = "Eliminate all hostiles",
  ["objective/kill_warden"] = "Kill the warden",

  -- Warden encounter (arena1 composite set-piece).
  ["scene/warden/intro"]    = "The warden has found you. There is no way out but through it.",
}
