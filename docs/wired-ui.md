# Wired UI System

## Overview

Wired UI is q3now's unified menu and HUD system, built around three core
components:

1. **Store** -- a game-agnostic key-value state bridge between cgame and client.
2. **UI bindings** -- `.wmenu` / `.whud` keywords that read store keys at render
   time and display them as text, colors, or icons.
3. **Scripting** -- a sandboxed LuaJIT console that can read/write the store and
   bridge to engine cvars.

The client never contains game logic. cgame computes game state, writes it
to the store, and the client renders whatever it finds there.

## Architecture

```
  cgame (mod-replaceable)             client (engine)
  +--------------------------+        +---------------------------+
  | cg_wired_bridge.c        |        | cl_wired_store.c          |
  |  WUI_Stage_SetString()   |------->|  wuiStore_t (hash table)  |
  |  WUI_Stage_SetFloat()    | batch  |                           |
  |  WUI_Stage_SetColor()    | flush  | cl_wired_parse.c          |
  |  WUI_Stage_SetIcon()     |        |  bind / bindcolor / ...   |
  |  WUI_Stage_SetState()    |        |  reads store at render    |
  +--------------------------+        +---------------------------+
```

**Data flow each frame:**

1. `CG_WiredHudPushState()` in cgame reads snapshot game state.
2. It calls `WUI_Stage_Set*()` to accumulate changes in a staging buffer.
3. `WUI_Stage_Flush()` sends all staged entries to the client store in a
   single batch syscall.
4. The client parser resolves `bind`, `bindcolor`, etc. keywords against the
   store and renders items.

---

## Store Key Reference

Every key below is written by `cg_wired_bridge.c` via `WUI_Stage_Set*()`.
Keys use dot-separated namespacing.

### Player Health

| Key | Type | Description |
|-----|------|-------------|
| `player.health.text` | string | Current HP as display string (e.g. `"87"`) |
| `player.health.value` | float | Raw HP value |
| `player.health.percent` | float | HP / 100, clamped 0.0 -- 1.0 |
| `player.health.color` | color | RGBA derived from effective health |
| `player.health.icon` | icon | Health icon shader handle |
| `player.health.state` | state | `"critical"` (<25), `"warning"` (<50), `"overhealed"` (>100), `"normal"` |

### Player Armor

| Key | Type | Description |
|-----|------|-------------|
| `player.armor.text` | string | Current AP as display string |
| `player.armor.value` | float | Raw AP value |
| `player.armor.percent` | float | AP / 200, clamped 0.0 -- 1.0 |
| `player.armor.color` | color | RGBA derived from AP value |
| `player.armor.icon` | icon | Armor icon (heavy / combat / jacket) |
| `player.armor.state` | state | `"neutral"` (0), `"warning"` (<50), `"normal"` (>=50) |

### Player Ammo

| Key | Type | Description |
|-----|------|-------------|
| `player.ammo.text` | string | Current ammo as display string |
| `player.ammo.value` | float | Raw ammo value |
| `player.ammo.percent` | float | Ammo / 200, clamped 0.0 -- 1.0 |
| `player.ammo.color` | color | Red when empty, white otherwise |
| `player.ammo.icon` | icon | Ammo icon for current weapon |
| `player.ammo.visible` | string | `"0"` for gauntlet/none, `"1"` otherwise |
| `player.ammo.state` | state | `"critical"` (empty), `"warning"` (low), `"normal"`, `"neutral"` (no weapon) |

### Player Weapon

| Key | Type | Description |
|-----|------|-------------|
| `player.weapon.current` | int | Weapon index (`WP_*` enum) |
| `player.weapon.name` | string | Weapon display name |
| `player.weapon.icon` | icon | Weapon icon shader handle |

### Match State

| Key | Type | Description |
|-----|------|-------------|
| `match.score.own` | int | Local player's score |
| `match.score.red` | int | Red team / leader score |
| `match.score.blue` | int | Blue team / second score |

### Warmup

| Key | Type | Description |
|-----|------|-------------|
| `game.warmup.message` | string | Warmup text (e.g. `"^BWaiting for Players"`) |

### Crosshair Target

| Key | Type | Description |
|-----|------|-------------|
| `crosshair.isTeammate` | int | 1 if crosshair target is on same team |
| `crosshair.showName` | int | 1 if crosshair target name should display |

### Spectators

| Key | Type | Description |
|-----|------|-------------|
| `game.spectators.list` | string | Comma-separated list of spectator names |

### Scoreboard

| Key | Type | Description |
|-----|------|-------------|
| `game.scores.count` | int | Number of score entries |
| `game.scores.gametype` | int | Gametype enum value |

Per-player row keys (N = 0-based index):

| Key pattern | Type | Description |
|-------------|------|-------------|
| `game.scores.N.name` | string | Player name |
| `game.scores.N.team` | int | Team index |
| `game.scores.N.client` | int | Client number |
| `game.scores.N.highlight` | float | 1.0 if local player, 0.0 otherwise |
| `game.scores.N.rank` | string | Display rank (1-based) |
| `game.scores.N.score` | string | Score value |
| `game.scores.N.kd` | string | Kill/death string (e.g. `"12/3"`) |
| `game.scores.N.kdcolor` | color | Green (positive K/D) to red (negative) |
| `game.scores.N.eff` | string | Efficiency percentage (e.g. `"75%"`) |
| `game.scores.N.effcolor` | color | Green (>=60%), yellow (>=40%), white |
| `game.scores.N.damage` | string | Total damage dealt (formatted with k suffix) |
| `game.scores.N.acc` | string | Accuracy percentage (e.g. `"42%"`) |
| `game.scores.N.acccolor` | color | Green (>=50%), yellow (>=30%), white |
| `game.scores.N.ping` | string | Ping in ms, or `"..."` if connecting |
| `game.scores.N.pingcolor` | color | Green (<40ms), yellow (<100ms), red |

---

## .wmenu / .whud Binding Syntax

### Data binding keywords

These keywords go inside an `itemDef { }` block and connect a store key to
a rendering property.

| Keyword | Stores into field | Purpose | Example |
|---------|-------------------|---------|---------|
| `bind` | `storeBind` | Override item text with store key's `.text` | `bind "player.health.text"` |
| `bindcolor` | `storeBindColor` | Override forecolor with store key's `.color` | `bindcolor "player.health.color"` |
| `bindicon` | `storeBindIcon` | Override icon with store key's `.icon` | `bindicon "player.health.icon"` |
| `bindvalue` | `storeBindValue` | Read numeric `.value` from store key | `bindvalue "player.health.percent"` |
| `showbind` | `showBind` | Show item only when store key text is truthy | `showbind "player.ammo.visible"` |
| `hidebind` | `hideBind` | Hide item when store key text is truthy | `hidebind "player.ammo.visible"` |

**How truthy works:** The store entry's `.text` field is checked. Empty string
or `"0"` is falsy; anything else is truthy.

### Minimal example

```
itemDef {
    name "health_number"
    rect 0.466 0.913 0 0
    anchor BOTTOM_CENTER
    font "sansman" 28
    textalign 2
    forecolor 1 1 1 1
    bind "player.health.text"
    bindcolor "player.health.color"
    visible 1
}
```

This item displays the player's health as a number, colored by the health
state (red when critical, white normally, cyan when overhealed).

### TABLE widget

The TABLE widget displays tabular data from the store, designed for
scoreboards and similar repeating-row displays.

**Item-level keywords:**

| Keyword | Purpose | Example |
|---------|---------|---------|
| `source` | Store key prefix for row data | `source "game.scores"` |
| `countbind` | Store key holding row count | `countbind "game.scores.count"` |
| `teamfilter` | Filter rows by team (-1=all, 1=red, 2=blue) | `teamfilter -1` |

**Column definition:**

Each `column { }` block defines one column with these sub-keywords:

| Sub-keyword | Purpose | Example |
|-------------|---------|---------|
| `field` | Store key suffix for cell text | `field "name"` |
| `header` | Column header label | `header "Player"` |
| `width` | Fraction of table width (0.0-1.0) | `width 0.25` |
| `align` | Text alignment (0=left, 1=center, 2=right) | `align 0` |
| `colorfield` | Store key suffix for per-cell color | `colorfield "kdcolor"` |
| `iconfield` | Store key suffix for per-cell icon | `iconfield "headicon"` |

Maximum 16 columns per table (`WUI_TABLE_MAX_COLUMNS`).

**How it works:** For each row index N from 0 to count-1, the widget reads
`{source}.{N}.{field}` from the store. For example, with
`source "game.scores"` and `field "name"`, row 3 reads
`game.scores.3.name`.

**Full TABLE example:**

```
itemDef {
    name "scoreboard"
    rect 0.1 0.1 0.8 0.8
    visible 1

    source "game.scores"
    countbind "game.scores.count"
    teamfilter -1

    column { field "rank"   header "#"      width 0.05  align 2 }
    column { field "name"   header "Player" width 0.30  align 0 }
    column { field "score"  header "Score"  width 0.10  align 2 }
    column { field "kd"     header "K/D"    width 0.12  align 2  colorfield "kdcolor" }
    column { field "eff"    header "Eff"    width 0.10  align 2  colorfield "effcolor" }
    column { field "damage" header "Dmg"    width 0.10  align 2 }
    column { field "acc"    header "Acc"    width 0.10  align 2  colorfield "acccolor" }
    column { field "ping"   header "Ping"   width 0.08  align 2  colorfield "pingcolor" }
}
```

---

## Theme System

The theme maps semantic state labels to RGBA colors. cgame writes state labels
(e.g. `"critical"`) to store entries. At render time, `bindcolor` resolves the
state through the theme to produce a concrete color.

### Default states

| State | R | G | B | A | Visual | Use case |
|-------|---|---|---|---|--------|----------|
| `critical` | 1.0 | 0.2 | 0.2 | 1.0 | Red | Below 25% health |
| `warning` | 1.0 | 0.8 | 0.0 | 1.0 | Yellow | Below 50% health |
| `normal` | 1.0 | 1.0 | 1.0 | 1.0 | White | Default/healthy |
| `overhealed` | 0.0 | 0.8 | 1.0 | 1.0 | Cyan | Above 100% health |
| `friendly` | 0.2 | 0.8 | 1.0 | 1.0 | Blue | Allies/teammates |
| `enemy` | 1.0 | 0.3 | 0.3 | 1.0 | Red | Opponents |
| `positive` | 0.2 | 1.0 | 0.4 | 1.0 | Green | Good values |
| `negative` | 1.0 | 0.2 | 0.2 | 1.0 | Red | Bad values |
| `neutral` | 0.7 | 0.7 | 0.7 | 1.0 | Gray | Inactive/irrelevant |

### How state resolution works

1. cgame writes a state label to a store entry:
   `WUI_Stage_SetState("player.health.state", "critical")`
2. The store entry's `.state` field is set to `"critical"`.
3. When a `bindcolor` reads this entry, `WiredTheme_ResolveState("critical", colorOut)`
   looks up the theme and returns `{1.0, 0.2, 0.2, 1.0}`.
4. If the state label is unknown, the raw `.color` from the store entry is
   used as a fallback.

### Console commands

```
wui_theme_list
```
Lists all state-to-color mappings in the active theme.

```
wui_theme_set <state> <r> <g> <b> <a>
```
Override or add a state color at runtime.

Example: `wui_theme_set critical 1.0 0.0 0.5 1.0` changes "critical" to magenta.

---

## Lua Scripting API

LuaJIT is integrated as a console scripting layer. It does NOT run in the
render path -- only on explicit console input or file execution.

### Store module

```lua
store.get(key)                -- returns string or nil
store.set(key, value)         -- writes text + value to store (string or number)
store.getvalue(key)           -- returns number or nil
store.getcolor(key)           -- returns r, g, b, a (4 numbers) or nil
```

**Examples:**

```lua
-- Read health text
print(store.get("player.health.text"))   -- "87"

-- Read health as number
print(store.getvalue("player.health.value"))  -- 87

-- Write a custom store key
store.set("my.custom.label", "Hello HUD")

-- Read color components
local r, g, b, a = store.getcolor("player.health.color")
```

### Cvar bridge

Any unknown global name is treated as an engine cvar. Reading returns the
cvar's value (as number if parseable, string otherwise). Writing sets the cvar.

```lua
-- Read a cvar
print(sensitivity)           -- prints current sensitivity value

-- Set a cvar
sensitivity = 3.5            -- equivalent to: /sensitivity 3.5
name = "Player1"             -- equivalent to: /name Player1
```

Functions and tables assigned to globals are stored normally (not as cvars).

### Engine command execution

```lua
cmd("map q3dm17")            -- execute any engine console command
```

### Console commands

| Command | Purpose |
|---------|---------|
| `lua_eval <expression>` | Evaluate a Lua expression and print the result |
| `lua_exec <filename.lua>` | Execute a Lua file from the game filesystem |

**lua_eval** tries the input as an expression first (prepends `return`),
falling back to statement execution. This makes it work as a REPL:

```
lua_eval 2 + 2               -- prints: 4
lua_eval store.get("player.health.text")  -- prints: 87
lua_eval sensitivity = 3.0   -- sets the cvar
```

### Startup

On init, the scripting system tries to execute `autoexec.lua` from the game
filesystem. Place your startup scripts there.

### Sandbox

**Blocked modules:** `os`, `io`, `debug`, `loadfile`, `dofile`

**Available:** `string`, `table`, `math`, `print` (redirected to engine
console), `store` (Wired Store API), `cmd` (engine command execution)

---

## Console Commands Reference

### Store

| Command | Purpose |
|---------|---------|
| `wui_store_get <key>` | Print all fields of a store entry (text, value, color, icon, state, flags) |
| `wui_store_dump` | Print all entries in bucket order (for debugging) |
| `wui_store_list` | Print all keys sorted alphabetically |
| `wui_store_stats` | Print hash table statistics (entry count, bucket distribution, load factor) |
| `wui_store_watch <key>` | Toggle watching a key (prints changes each frame when dirty) |

### Theme

| Command | Purpose |
|---------|---------|
| `wui_theme_list` | List all state-to-color mappings |
| `wui_theme_set <state> <r> <g> <b> <a>` | Set or add a state color |

### Scripting

| Command | Purpose |
|---------|---------|
| `lua_eval <expression>` | Evaluate Lua expression |
| `lua_exec <filename.lua>` | Execute Lua file |

### HUD / Menu reload

| Command | Purpose |
|---------|---------|
| `hud_reload` | Reload HUD overlay files |
| `menu_reload` | Reload menu files |

---

## Tutorial: Creating a Custom Health Display

This tutorial creates a health display from scratch using only `.wmenu`
syntax and store bindings. No C code required.

### Step 1: Create the file

Create `modfiles/ui/hud_myhealth.whud`:

```
#include "ui/wmenumacros.h"

menuDef {
    name "hud_myhealth"
    fullScreen 0
    visible 1
    hudOverlay 1

    // Background panel
    itemDef {
        name "health_bg"
        anchor BOTTOM_LEFT
        rect 0.02 0.90 0.15 0.06
        backcolor 0.05 0.05 0.10 0.7
        style 1
        visible 1
    }

    // Health icon
    itemDef {
        name "health_icon"
        anchor BOTTOM_LEFT
        rect 0.025 0.905 0.020 0.040
        bindicon "player.health.icon"
        visible 1
    }

    // Health number
    itemDef {
        name "health_text"
        anchor BOTTOM_LEFT
        rect 0.050 0.905 0 0
        font "sansman" 24
        textalign 0
        forecolor 1 1 1 1
        bind "player.health.text"
        bindcolor "player.health.color"
        visible 1
    }

    // Health bar (fills proportionally)
    itemDef {
        name "health_bar"
        anchor BOTTOM_LEFT
        rect 0.02 0.955 0.15 0.008
        backcolor 0.2 0.2 0.2 0.5
        style 1
        bindvalue "player.health.percent"
        bindcolor "player.health.color"
        visible 1
    }
}
```

### Step 2: Understand what happens

- `bind "player.health.text"` -- overrides the item's text with the health
  number from the store. cgame writes this every frame.
- `bindcolor "player.health.color"` -- colors the text based on effective
  health. When health drops below 25, the state becomes `"critical"` and
  the theme resolves that to red.
- `bindicon "player.health.icon"` -- pulls the health icon shader from the
  store entry.
- `bindvalue "player.health.percent"` -- reads the 0.0--1.0 progress value
  for bar rendering.

### Step 3: Add conditional visibility

Hide the health display when spectating:

```
itemDef {
    name "health_text"
    ...
    hidebind "player.ammo.visible"
    // This uses the ammo visible flag -- spectators have no weapon,
    // so player.ammo.visible is "0". For a direct spectator check,
    // you would need a dedicated store key.
}
```

### Step 4: Debug with console

Use the store console commands to verify data flow:

```
wui_store_list                        -- see all keys
wui_store_get player.health.text      -- check current value
wui_store_watch player.health.state   -- watch state changes live
lua_eval store.get("player.health.text")  -- check via Lua
```

---

## Migration Guide

### From elem_* pattern to store + bindings

**Old way (C code in `cl_wired_hud_elem_*.c`):**

Each HUD element is a C function that reads from `wiredHudState_t` and calls
renderer functions directly. Adding a new element means writing C, recompiling,
and deploying.

```c
// cl_wired_hud_elem_health.c (old pattern)
void WiredHud_DrawHealth(wiredHudState_t *state, ...) {
    int hp = state->health;
    vec4_t color;
    BG_GetColorForAmount(hp, color);
    // ... direct draw calls ...
}
```

**New way (store + bindings):**

cgame writes data to the store. The `.wmenu` / `.whud` file uses `bind` keywords
to display it. No C code needed for display logic.

```c
// cg_wired_bridge.c -- cgame side (already exists)
WUI_Stage_SetString("player.health.text", "87");
WUI_Stage_SetColor("player.health.color", color);
WUI_Stage_SetState("player.health.state", "warning");
```

```
// .whud file -- display side (no C needed)
itemDef {
    bind "player.health.text"
    bindcolor "player.health.color"
}
```

### Adding new game data to the store

To expose new game data for HUD display:

**1. Write data in cgame bridge (`cg_wired_bridge.c`):**

```c
// Inside CG_WiredHudPushState(), in the store-writing section:
WUI_Stage_SetString("player.location.name", locationName);
WUI_Stage_SetInt("player.location.index", locationIndex);
```

Use the appropriate setter:

| Function | When to use |
|----------|-------------|
| `WUI_Stage_SetString(key, text)` | Display text |
| `WUI_Stage_SetInt(key, value)` | Integer values (stored as float + text) |
| `WUI_Stage_SetFloat(key, value)` | Float values |
| `WUI_Stage_SetColor(key, rgba)` | RGBA color (vec4_t) |
| `WUI_Stage_SetIcon(key, handle)` | Shader handle for icons |
| `WUI_Stage_SetState(key, label)` | Semantic state label for theme resolution |

**2. Flush is automatic.** `WUI_Stage_Flush()` is already called once per frame
at the end of `CG_WiredHudPushState()`. Your staged entries will be included.

**3. Display in `.wmenu` / `.whud`:**

```
itemDef {
    name "location"
    bind "player.location.name"
    ...
}
```

**4. Verify with console:**

```
wui_store_get player.location.name
wui_store_watch player.location.name
```

### Store entry structure

Each store entry (defined in `cl_wired_store.h`) holds:

| Field | Type | Size | Description |
|-------|------|------|-------------|
| `key` | char[] | 128 | Dot-separated namespace key |
| `text` | char[] | 256 | String value |
| `color` | vec4_t | 16 | RGBA color |
| `icon` | qhandle_t | 4 | Shader handle |
| `value` | float | 4 | Numeric value |
| `state` | char[] | 32 | Semantic state label |
| `flags` | int | 4 | Dirty + watched flags |
| `generation` | int | 4 | Last-written frame counter |

The store supports up to 4096 entries across 512 hash buckets.
