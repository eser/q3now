-- scripts/char_framework.lua
-- Character manifest framework.
-- Loaded by sv_lua.c at init; installs q3now.load_character and q3now.load_bot.
--
-- q3now.load_character(name, skill_norm):
--   three-layer merge: _archetypes/_base/main.lua → _archetypes/{archetype}/main.lua
--   → characters/{name}/main.lua.  Top-level containers (model, sounds, stats, bot)
--   merge field-by-field; within bot, traits/aim/chats also merge field-by-field;
--   everything else (scalars, lists) replaces wholesale.
--
-- q3now.load_bot(name):
--   three-layer merge: _archetypes/_base/bot/main.lua → _archetypes/{archetype}/bot/main.lua
--   → characters/{name}/bot/main.lua.  traits/aim/chats are sub-containers.

local M = {}

-- ── Utilities ─────────────────────────────────────────────────────────────────

local function log(msg)
  if q3now and type(q3now.print) == "function" then q3now.print(msg)
  else print(msg) end
end

-- Levenshtein distance for "did you mean?" suggestions.
local function edit_dist(a, b)
  local la, lb = #a, #b
  if la == 0 then return lb end
  if lb == 0 then return la end
  local prev = {}
  for j = 0, lb do prev[j] = j end
  for i = 1, la do
    local curr = { [0] = i }
    local ai = a:sub(i, i)
    for j = 1, lb do
      if ai == b:sub(j, j) then
        curr[j] = prev[j - 1]
      else
        curr[j] = 1 + math.min(prev[j], curr[j - 1], prev[j - 1])
      end
    end
    prev = curr
  end
  return prev[lb]
end

local function suggest(key, valid_set)
  local best, best_d = nil, 3  -- accept edit distance ≤ 2
  for k in pairs(valid_set) do
    local d = edit_dist(key, k)
    if d < best_d then best, best_d = k, d end
  end
  return best
end

-- ── Utilities (identity) ──────────────────────────────────────────────────────

-- Strip Q3 color codes (^N sequences) from a string; ^^ collapses to ^.
local function strip_color(s)
  return (s:gsub("%^%^", "\0"):gsub("%^%d", ""):gsub("\0", "^"))
end

-- ── Validation ────────────────────────────────────────────────────────────────

local TOP_VALID = {
  name = true, display_name = true, nicknames = true,
  bio = true, role = true, archetype = true,
  model = true, sounds = true, stats = true,
}

local function validate(spec, char_name)
  -- name is optional; display_name is required.
  if spec.name ~= nil and (type(spec.name) ~= "string" or spec.name == "") then
    error(string.format(
      "characters/%s/main.lua: field 'name', if present, must be a non-empty string",
      char_name), 0)
  end
  if type(spec.display_name) ~= "string" or spec.display_name == "" then
    error(string.format(
      "characters/%s/main.lua: required field 'display_name' must be a non-empty string",
      char_name), 0)
  end
  if spec.model ~= nil and type(spec.model) ~= "table" then
    error(string.format(
      "characters/%s/main.lua: 'model' must be a table",
      char_name), 0)
  end
  -- Warn on unknown top-level keys in the character's own manifest (not the merged result).
  for k in pairs(spec) do
    if not TOP_VALID[k] then
      local s = suggest(k, TOP_VALID)
      log(string.format(
        "characters/%s/main.lua: unknown top-level key '%s'%s (ignored)",
        char_name, k, s and (" (did you mean '" .. s .. "'?)") or ""))
    end
  end
end

-- ── Two-level merge ───────────────────────────────────────────────────────────
-- Top-level containers (model, sounds, stats, bot) are merged field-by-field.
-- Inside bot, traits/aim/chats are also merged field-by-field.
-- Everything else (scalars, list fields like skins/parts/headoffset) replaces wholesale.

-- Top-level manifest keys that are containers.
local TOP_CONTAINERS = { model = true, sounds = true, stats = true, bot = true }

-- Second-level containers inside `bot`.
local BOT_CONTAINERS = { traits = true, aim = true, chats = true }

-- Deep-copy t up to depth levels (so level-2 sub-tables are independent fresh
-- copies — safe for in-place merges without affecting the original).
local function deep_copy(t, depth)
  local copy = {}
  for k, v in pairs(t) do
    if depth > 0 and type(v) == "table" then
      copy[k] = deep_copy(v, depth - 1)
    else
      copy[k] = v
    end
  end
  return copy
end

-- Leaf merge: assign every key from overlay into result wholesale.
local function merge_leaf(result, overlay)
  for k, v in pairs(overlay) do result[k] = v end
end

-- Two-level merge: result starts as a deep copy (depth 2) of base; each key
-- in overlay either merges field-by-field (containers) or replaces wholesale.
local function merge_manifests(base, overlay)
  local result = deep_copy(base, 2)
  for k, v in pairs(overlay) do
    if TOP_CONTAINERS[k] and type(v) == "table" and type(result[k]) == "table" then
      if k == "bot" then
        -- bot sub-containers (traits, aim, chats) merge one level deeper.
        for bk, bv in pairs(v) do
          if BOT_CONTAINERS[bk] and type(bv) == "table" and type(result[k][bk]) == "table" then
            merge_leaf(result[k][bk], bv)
          else
            result[k][bk] = bv
          end
        end
      else
        merge_leaf(result[k], v)
      end
    else
      result[k] = v
    end
  end
  return result
end

-- ── q3now.load_character(name, skill_norm) ────────────────────────────────────

function q3now.load_character(name, _skill_norm)
  local base = q3now.load("characters/_archetypes/_base/main")
  if type(base) ~= "table" then
    error("characters/_archetypes/_base/main.lua must return a table", 0)
  end

  local char = q3now.load("characters/" .. name .. "/main")
  if type(char) ~= "table" then
    error(string.format(
      "characters/%s/main.lua must return a table, got %s",
      name, type(char)), 0)
  end

  -- Validate char-specific manifest before merge (warn on author typos).
  validate(char, name)

  -- Determine archetype; default to "humanoid_male".
  local archetype_name = (type(char.archetype) == "string" and char.archetype ~= "")
    and char.archetype or "humanoid_male"

  -- Load archetype manifest and merge over _base (optional — missing archetype file is ok).
  local arch_path = "characters/_archetypes/" .. archetype_name .. "/main"
  local ok_arch, arch = pcall(function() return q3now.load(arch_path) end)
  if not (ok_arch and type(arch) == "table") then arch = {} end

  -- Three-layer merge: _base → archetype → character.
  local merged = merge_manifests(merge_manifests(base, arch), char)

  -- Derive name from display_name if not explicitly set.
  if merged.name == nil or merged.name == "" then
    merged.name = strip_color(merged.display_name)
  end

  -- Ensure name matches directory (case-insensitive: "Visor" is valid in characters/visor/).
  if merged.name:lower() ~= name:lower() then
    error(string.format(
      "characters/%s/main.lua: name field '%s' does not match directory",
      name, tostring(merged.name)), 0)
  end

  return merged
end

-- ── q3now.load_bot(name) ──────────────────────────────────────────────────────

function q3now.load_bot(name)
  local base = q3now.load("characters/_archetypes/_base/bot/main")
  if type(base) ~= "table" then
    error("characters/_archetypes/_base/bot/main.lua must return a table", 0)
  end

  -- Determine archetype from character manifest (best-effort; default to "humanoid_male").
  local archetype_name = "humanoid_male"
  local ok_char, main_char = pcall(function()
    return q3now.load("characters/" .. name .. "/main")
  end)
  if ok_char and type(main_char) == "table" then
    if type(main_char.archetype) == "string" and main_char.archetype ~= "" then
      archetype_name = main_char.archetype
    end
  end

  -- Load archetype bot/main (optional).
  local ok_arch, arch_bot = pcall(function()
    return q3now.load("characters/_archetypes/" .. archetype_name .. "/bot/main")
  end)
  if not (ok_arch and type(arch_bot) == "table") then arch_bot = {} end

  -- Load character bot/main (optional).
  local charPath = "characters/" .. name .. "/bot/main"
  local ok_c, charBot = pcall(function() return q3now.load(charPath) end)
  if not (ok_c and type(charBot) == "table") then charBot = {} end

  -- Three-layer merge: base → archetype bot → character bot.
  -- traits/aim/chats are sub-containers (field-by-field); everything else replaces.
  local function merge_bot(result, overlay)
    for k, v in pairs(overlay) do
      if BOT_CONTAINERS[k] and type(v) == "table" and type(result[k]) == "table" then
        merge_leaf(result[k], v)
      else
        result[k] = v
      end
    end
  end

  local result = deep_copy(base, 1)
  merge_bot(result, arch_bot)
  merge_bot(result, charBot)

  -- Load chats module (per-character if present, else _base).
  local chats_mod
  do
    local ok, loaded = pcall(q3now.load, "characters/" .. name .. "/bot/chats")
    if ok and type(loaded) == "table" then
      chats_mod = loaded
    else
      local ok2, base_chats = pcall(q3now.load, "characters/_archetypes/_base/bot/chats")
      if ok2 and type(base_chats) == "table" then chats_mod = base_chats end
    end
  end

  -- Map WiredBots event names to the chats{} rate keys in bot/main.lua.
  -- Keys not in this table fall through and use the event name directly
  -- (so new events like ctf_got_flag just need a matching key in chats{}).
  local EVENT_RATE_KEY = {
    game_enter     = "enterexitgame",
    game_exit      = "enterexitgame",
    level_start    = "startendlevel",
    level_end      = "startendlevel",
    kill           = "kill",
    death          = "death",
    enemy_suicide  = "enemysuicide",
    hit_talking    = "hittalking",
    hit_nodeath    = "hitnodeath",
    hit_nokill     = "hitnokill",
    random         = "random",
    message        = "reply",
  }

  if chats_mod then
    result.on_chat = function(self, eventName, ctx)
      local rateKey = EVENT_RATE_KEY[eventName] or eventName
      local rateVal = self.chats and self.chats[rateKey]
      local rate
      if type(rateVal) == "number" then
        rate = rateVal
      elseif type(rateVal) == "table" then
        rate = type(rateVal[1]) == "number" and rateVal[1] or 0
      else
        rate = 0
      end
      if math.random() > rate then return nil end
      local fn = chats_mod[eventName]
      if type(fn) ~= "function" then return nil end
      return fn(ctx)
    end
  end

  return result
end

return M
