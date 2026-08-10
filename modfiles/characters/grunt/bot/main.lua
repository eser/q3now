-- grunt bot data — the AGGRESSIVE RUSHER of the Rule-3 proof pair.
-- Shallow-merged onto _base/bot/main.lua. The decide() below SELECTS shared
-- native states (the C states EXECUTE them, F2-hybrid). Grunt's policy: close
-- and fight at almost any cost — it barely takes cover, escalates instantly on
-- any contact, and chases relentlessly.
--
-- ctx fields: health, enemy_visible, enemy_dist, enemy_health, sensed_noise,
-- noise_dist, ...
return {
    traits = {
        aggression       = { 0.95, 0.95 },
        selfpreservation = { 0.10, 0.10 },
    },

    decide = function(self, ctx)
        -- Rusher only breaks off when nearly dead, and even then just briefly.
        if ctx.health <= 15 then
            return "takecover"
        end
        -- Any contact → attack. Sees it → battle; only hears it → alert-search
        -- straight in (skip the cautious "query" investigate).
        if ctx.enemy_visible then
            return "battle"
        end
        if ctx.sensed_noise then
            return "alert"
        end
        -- No contact but a known target → hunt it down aggressively.
        return "hunt"
    end,
}
