-- skulk bot data — the CAUTIOUS HOLDER of the Rule-3 proof pair.
-- Shallow-merged onto _base/bot/main.lua. The decide() below SELECTS shared
-- native states (the C states EXECUTE them, F2-hybrid). Skulk's policy: stay
-- safe — investigate noises warily before committing, break to cover early, and
-- only fight when the enemy is close and it is healthy.
--
-- ctx fields: health, enemy_visible, enemy_dist, enemy_health, sensed_noise,
-- noise_dist, ...
return {
    traits = {
        aggression       = { 0.25, 0.25 },
        selfpreservation = { 0.90, 0.90 },
    },

    decide = function(self, ctx)
        -- Breaks off much earlier than the rusher (half-health), and flees
        -- outright if badly hurt with the enemy in its face.
        if ctx.health <= 60 then
            if ctx.enemy_visible and ctx.enemy_dist < 300 then
                return "flee"      -- cornered + hurt → panic-run, no cover ritual
            end
            return "takecover"     -- hurt → retreat and recover
        end
        -- Healthy: only engage when the enemy is close AND visible; otherwise
        -- hold back cautiously.
        if ctx.enemy_visible then
            if ctx.enemy_dist < 400 then
                return "battle"    -- close enough to commit
            end
            return "alert"         -- seen but far → hold/search, don't charge
        end
        -- Heard but not seen → investigate warily (the QUERY the rusher skips).
        if ctx.sensed_noise then
            return "query"
        end
        -- Nothing sensed → patrol relaxed.
        return "relaxed"
    end,
}
