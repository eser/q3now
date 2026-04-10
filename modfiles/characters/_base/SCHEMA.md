# Character Schema Reference

All float values are `[0, 1]` unless the Range column says otherwise.
`{a, b}` means skill-lerped: `a` at skill 0, `b` at skill 5.
C index = `CHARACTERISTIC_*` integer passed to the botlib.

---

## Top-level fields

| Key | Type | Required | Notes |
|-----|------|----------|-------|
| `name` | string | yes | Internal identifier; must match folder name |
| `display_name` | string | no | Shown in menus |
| `bio` | string | no | Flavour text |
| `role` | string | no | Cosmetic label (`attacker`, `defender`, `skirmisher`, …) |
| `model` | string | yes | Path to player body model |
| `head` | string | no | Path to player head model |
| `skin` | string | no | Skin name (`default`) |
| `icons` | table | no | `{ small = "…", large = "…" }` |
| `sounds` | table | no | See sound keys below |
| `stats` | table | no | `{ base_health, base_armor, max_health, max_armor, speed }` |
| `chat_name` | string | no | Name used in chat messages; defaults to `name` |
| `traits` | table | no | Personality dials (see below) |
| `aim` | table | no | Aiming dials (see below) |
| `chat` | table | no | Chat frequency dials (see below) |

---

## traits.*

C index refers to `CHARACTERISTIC_*` in `code/game/chars.h`.

| Key | C index | Range | Default | Notes |
|-----|---------|-------|---------|-------|
| `attack_skill` | 2 | [0, 1] | skill | Movement during combat: strafe, circle at high values |
| `view_factor` | 4 | (0, 1] | skill | How quickly view angle tracks target direction |
| `view_maxchange` | 5 | [1, 360] (raw°/frame) | skill×360 | Maximum view angle change per frame |
| `reaction_time` | 6 | [0, 5] (raw seconds) | 1-skill | Delay before noticing an enemy; `{1.0, 0.0}` is typical |
| `aggression` | 41 | [0, 1] | skill | Willingness to initiate combat vs retreat |
| `selfpreservation` | 42 | [0, 1] | skill | Flee when damaged; 0 = suicidal, 1 = always retreats |
| `vengefulness` | 43 | [0, 1] | skill | Prioritises last attacker as target |
| `camper` | 44 | [0, 1] | 0.5 | Tendency to hold a position instead of roaming |
| `easy_fragger` | 45 | [0, 1] | skill | Prefers weak/damaged targets |
| `alertness` | 46 | [0, 1] | skill | Effective view distance / noticeability of enemies |
| `firethrottle` | 47 | [0, 1] | skill | Rate of fire; 1 = fires as fast as possible |
| `croucher` | 36 | [0, 1] | 0.5 | Tendency to crouch while shooting |
| `jumper` | 37 | [0, 1] | 0.5 | Tendency to jump while moving |
| `weaponjumping` | 38 | [0, 1] | 0.5 | Use splash damage for jump boosts |
| `grapple_user` | 39 | [0, 1] | 0.5 | Use grapple hook when available |
| `walker` | 48 | [0, 1] | 1-skill | Tendency to walk instead of run |

---

## aim.*

All aim values are `[0, 1]`. Defaults are `skill` (low accuracy at skill 0, high at skill 5).

| Key | C index | Notes |
|-----|---------|-------|
| `accuracy` | 7 | Base aim accuracy — deviation from target centre |
| `skill` | 16 | Base aim skill — enemy leading, prediction shots |
| `accuracy_machinegun` | 8 | Overrides `accuracy` for machinegun |
| `accuracy_shotgun` | 9 | Overrides `accuracy` for shotgun |
| `accuracy_rocketlauncher` | 10 | Overrides `accuracy` for rocket launcher |
| `accuracy_grenadelauncher` | 11 | Overrides `accuracy` for grenade launcher |
| `accuracy_lightning` | 12 | Overrides `accuracy` for lightning gun |
| `accuracy_plasmagun` | 13 | Overrides `accuracy` for plasma gun |
| `accuracy_railgun` | 14 | Overrides `accuracy` for railgun |
| `accuracy_bfg10k` | 15 | Overrides `accuracy` for BFG10K |
| `skill_rocketlauncher` | 17 | Overrides `skill` for rocket launcher |
| `skill_grenadelauncher` | 18 | Overrides `skill` for grenade launcher |
| `skill_plasmagun` | 19 | Overrides `skill` for plasma gun |
| `skill_bfg10k` | 20 | Overrides `skill` for BFG10K |
| `attack_skill` | 2 | Also settable in `traits`; controls combat movement |

---

## chat.*

All values are `[0, 1]`. `0` = never chats; `1` = always chats on this event.
Default is `1 - skill` (chatty at low skill, quiet at high skill).

| Key | C index | When fired |
|-----|---------|-----------|
| `insult` | 24 | After fragging an enemy |
| `misc` | 25 | Random miscellaneous |
| `startendlevel` | 26 | Level start or end |
| `enterexitgame` | 27 | Player joins or leaves |
| `kill` | 28 | Bot gets a kill |
| `death` | 29 | Bot dies |
| `enemysuicide` | 30 | Enemy suicides |
| `hittalking` | 31 | Bot is hit while chatting |
| `hitnodeath` | 32 | Bot is hit but survives |
| `hitnokill` | 33 | Bot hits enemy but doesn't kill |
| `random` | 34 | Random interval |
| `reply` | 35 | Replying to another bot's chat |

Set all chat values to `0.0` for a silent bot (e.g. tao, taa).

---

## sounds.*

| Key | Example path |
|-----|-------------|
| `pain25` | `sound/player/visor/pain25.wav` |
| `pain50` | `sound/player/visor/pain50.wav` |
| `pain75` | `sound/player/visor/pain75.wav` |
| `pain100` | `sound/player/visor/pain100.wav` |
| `death1` | `sound/player/visor/death1.wav` |
| `death2` | `sound/player/visor/death2.wav` |
| `death3` | `sound/player/visor/death3.wav` |
| `jump` | `sound/player/visor/jump1.wav` |
| `taunt` | `sound/player/visor/taunt.wav` |
| `falling` | `sound/player/visor/falling1.wav` |
| `gasp` | `sound/player/visor/gasp.wav` |
| `drown` | `sound/player/visor/drown.wav` |

---

## Adding a new characteristic

1. Add `#define CHARACTERISTIC_FOO 50` to `code/game/chars.h`.
2. Add `{ "foo", CHARACTERISTIC_FOO },` to `s_characteristicNames[]` in `code/server/sv_lua.c`.
3. Add a row to this table.
4. Use `foo = value` under `traits` (or `aim` / `chat` if it belongs there) in any character file.

Everything else — the validator, the `_base/main.lua` key resolution, the C precompute — picks it up automatically.
