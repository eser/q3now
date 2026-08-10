-- Türkçe (tr) localization table.
--
-- Second language fixture: proves the cl_language cvar + l10n_reload path swaps
-- the whole table (set cl_language "tr" ; l10n_reload). Same key space as en.lua
-- (the sound-name-as-subtitle-key); only the text differs. A key present in en
-- but absent here falls back to the raw key at lookup.

return {
  ["scene/cinematic/intro"] = "Tesis artık sessiz. Burada ne bekliyorsa, geldiğimizi biliyor.",
  ["scene/intro/line1"]     = "Uyan. Arena aktif.",
  ["scene/arena1/greet"]    = "Deneme sahasına hoş geldin.",

  -- Görev hedefleri (aynı anahtar uzayı; hedefler HUD'u için çözümlenir).
  ["objective/reach_exit"]  = "Çıkış portalına ulaş",
  ["objective/find_key"]    = "Güvenlik anahtarını kurtar",
  ["objective/eliminate"]   = "Tüm düşmanları yok et",
  ["objective/kill_warden"] = "Muhafızı öldür",

  -- Muhafız karşılaşması (arena1 kompozit set-piece).
  ["scene/warden/intro"]    = "Muhafız seni buldu. Onu geçmekten başka çıkış yok.",
}
