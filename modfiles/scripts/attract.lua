-- attract.lua — Wired Attract playlist
--
-- Runs once at WiredAttract_Init time. Adds items to the attract scheduler
-- and configures loop / transition settings.
--
-- attract.add(kind, source, duration_ms)
--   kind:       "cinematic" | "demo" | "panel"
--   source:     file path (demo/cinematic) or menu name (panel)
--   duration_ms: how long to show the item; 0 = wait for natural completion
--
-- Tips:
--   - attract_delay controls how many seconds of idle before attract starts.
--     Default is 30 s. Set to 5 in developer configs for faster iteration.
--   - attract_status in the console prints current state.
--   - attract_skip skips to the next item without stopping the scheduler.

-- ── Playlist ─────────────────────────────────────────────────────────────

-- Leaderboard panel: 10 seconds
attract.add("panel", "attract_leaderboard", 10000)

-- Cinematic: shows video/idlogo.roq for 8 seconds then cuts
-- (MVP: duration_ms controls length since natural cinematic end is not wired)
attract.add("cinematic", "video/idlogo.roq", 8000)

-- ── Playback settings ────────────────────────────────────────────────────

attract.set_loop(true)        -- wrap back to item 0 after the last item
attract.set_transition(500)   -- 500 ms total (250 ms fade-out + 250 ms fade-in)
