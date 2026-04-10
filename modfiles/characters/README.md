# Bot Characters

A character is a folder under `modfiles/characters/<name>/` containing two files:

| File | Purpose |
|------|---------|
| `main.lua` | Identity, stats, and personality dials (this doc) |
| `bot/main.lua` | Decision logic: pick weapon, choose targets, react (optional) |

`main.lua` is pure data — no Lua knowledge needed. `bot/main.lua` is real Lua code and
is optional; if it's absent the bot uses `_base/bot/main.lua` as its brain.

---

## 10-minute tutorial: add a new bot

```
# 1. Copy an existing character as a starting point
cp -r modfiles/characters/visor/  modfiles/characters/jamie/

# 2. Edit the identity fields in main.lua
#    Change name, display_name, model, head, icon, and sound paths.

# 3. Tune a few traits (see schema below)
#    For a more aggressive bot: raise aggression, lower selfpreservation.
#    For a camper: raise camper, lower jumper.

# 4. Launch the game and add the bot
addbot jamie 3
```

That's it. No `require`, no function calls — just a data file you fill in.

---

## File format

```lua
-- traits: personality dials, all 0..1 (use {low, high} for skill-lerped values)
-- aim: per-weapon accuracy dials, all 0..1
-- chat: per-event chat frequency, all 0..1
return {
  name         = "jamie",
  display_name = "Jamie",
  bio          = "One-line flavour description.",
  role         = "attacker",   -- cosmetic label only

  model = "models/players/visor/body",
  head  = "models/players/visor/head",
  skin  = "default",

  icons = {
    small = "gfx/characters/visor/icon_16",
    large = "gfx/characters/visor/icon_64",
  },

  sounds = {
    pain25  = "sound/player/visor/pain25.wav",
    pain50  = "sound/player/visor/pain50.wav",
    pain75  = "sound/player/visor/pain75.wav",
    pain100 = "sound/player/visor/pain100.wav",
    death1  = "sound/player/visor/death1.wav",
    death2  = "sound/player/visor/death2.wav",
    death3  = "sound/player/visor/death3.wav",
    jump    = "sound/player/visor/jump1.wav",
    taunt   = "sound/player/visor/taunt.wav",
    falling = "sound/player/visor/falling1.wav",
    gasp    = "sound/player/visor/gasp.wav",
    drown   = "sound/player/visor/drown.wav",
  },

  stats = { base_health = 125, base_armor = 0, max_health = 200, max_armor = 200, speed = 320 },

  traits = {
    aggression       = 0.80,           -- scalar: same at all skill levels
    selfpreservation = 0.25,
    reaction_time    = { 1.00, 0.00 }, -- {low_skill, high_skill}: lerped
    jumper           = { 0.30, 0.70 },
    -- ... (see schema for full key list)
  },

  aim = {
    accuracy         = { 0.25, 0.75 },
    skill            = { 0.35, 0.80 },
    accuracy_railgun = { 0.35, 0.80 },
    -- ... (see schema for full key list)
  },

  chat = {
    insult = 0.20,
    kill   = 0.25,
    -- ... (see schema for full key list)
  },
}
```

### Value formats

| Format | Meaning |
|--------|---------|
| `0.75` | Constant — same at all skill levels |
| `{0.2, 0.8}` | Skill-lerped: `0.2` at skill 0, `0.8` at skill 5 |

All values are `[0, 1]` unless stated otherwise in the schema. Values outside `[0, 1]`
are treated as raw engineering values and clamped to the botlib's `[min, max]` range
for that characteristic (e.g. `reaction_time` is `[0, 5]` seconds).

---

## What lives where

| You want to change... | Edit this file |
|-----------------------|---------------|
| Personality (aggression, camper, reaction time, ...) | `main.lua` → `traits` |
| Aiming accuracy / skill | `main.lua` → `aim` |
| Chat frequency | `main.lua` → `chat` |
| Which weapon the bot prefers | `bot/main.lua` → `pick_weapon` |
| How the bot values items (health, armor, ...) | `bot/main.lua` → `eval_item` |
| High-level combat decisions | `bot/main.lua` → `decide` |

If `bot/main.lua` is absent the bot uses `_base/bot/main.lua`, which gives reasonable
default behavior for any personality you define in `main.lua`.

---

## Personality quick-reference

These are the most impactful traits to tune first:

| Trait | Low | High | Tip |
|-------|-----|------|-----|
| `aggression` | defensive, retreats | rushes enemies | Start here for overall playstyle |
| `selfpreservation` | suicidal berserker | flees when low HP | Pair with aggression |
| `camper` | always moving | holds positions | Raise for defensive bots |
| `alertness` | slow to notice enemies | eagle-eyed | Affects effective "view range" |
| `reaction_time` | slow (1.0s) | instant (0.0s) | Use `{1.0, 0.0}` for normal bots |
| `jumper` | stays on ground | bunny-hops | Raise for agile/mobile feel |
| `firethrottle` | conserves ammo | full-auto | Raise for aggressive fire |

---

## Troubleshooting

**"My change didn't apply"** — Check the server console for `BotLua:` lines. A typo in
a key name produces a loud error with a "did you mean?" suggestion.

**"Bot behavior didn't change"** — Skill matters. `addbot jamie 1` (lowest) vs
`addbot jamie 5` (highest) can look very different if you used `{a, b}` values.

**"Unknown key 'aggresion' (did you mean 'aggression'?)"** — Typo. Fix the key name.

**"Bot is boring"** — Lower `camper` (→ 0.05), raise `aggression` (→ 0.85),
raise `firethrottle` (→ 0.90).

---

See `_base/SCHEMA.md` for the complete key reference with defaults and behavioral notes.
