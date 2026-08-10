-- testmonster bot data — dev-only. Shallow-merged onto _base/bot/main.lua.
-- The point of this file is the decide() function: it drives the monster
-- behavior layer's Lua-decide opt-in (the engine flags this character
-- hasDecideFn=true because this table defines "decide"), so the Lua path can be
-- exercised and measured against the pure-C FSM.
--
-- F2-hybrid contract: decide() only SELECTS which state; the native C states
-- (battle/hunt/takecover) still EXECUTE the movement and attack. It returns a
-- state-key string; an unknown/empty return makes the engine fall back to the C
-- FSM. ctx fields: health, enemy_visible, enemy_dist, enemy_health, ...
return {
    traits = {
        aggression       = { 0.90, 0.90 },
        selfpreservation = { 0.30, 0.30 },
    },

    -- The Lua decide override. Trivial, deterministic logic — enough to prove the
    -- Lua path selects a state and to measure its per-call cost; the real
    -- per-monster decision tables are a later phase.
    decide = function(self, ctx)
        if ctx.health <= 30 then
            return "takecover"
        end
        if ctx.enemy_visible then
            return "battle"
        end
        return "hunt"
    end,
}
