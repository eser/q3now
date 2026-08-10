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

-- Brutalist-poster attract loop (Eser 2026-07-03): brand splash → leaderboard.
-- The old idlogo.roq cinematic was dropped.
--
-- The gameplay-demo item is TEMPORARILY OUT of the playlist: the dummy demo
-- recorded so far is a broken 2380-byte capture (the bot match never really
-- ran), so it completes instantly every loop and triggers a full teardown
-- (CA_LOADING → CL_Disconnect). Re-add once a healthy demo is recorded AND the
-- CL_Disconnect/WiredUI state-separation fix lands so demo cycling never
-- interrupts the console/attract layers.
--   attract.add("demo", "attract_demo", 0)

-- Brand splash: 6 seconds
attract.add("panel", "attract_brand", 6000)

-- Leaderboard: 10 seconds
attract.add("panel", "attract_leaderboard", 10000)

-- ── Playback settings ────────────────────────────────────────────────────

attract.set_loop(true)        -- wrap back to item 0 after the last item
attract.set_transition(500)   -- 500 ms total (250 ms fade-out + 250 ms fade-in)
