-- characters/_base/main.lua
-- Framework: turns a plain spec table (returned by a character file) into the
-- engine-ready shape that C expects.
--
-- C injects q3now.characteristics (snake_case name → CHARACTERISTIC_* int index)
-- at Lua init before any character is loaded. This module reads it — no local
-- copy, no drift hazard.
--
-- C calls q3now.load_character(name, skillNorm) to load a character.
-- The trampoline at the bottom of this file is registered as a side-effect of
-- loading this module.

local M = {}

-- ── Small utilities ───────────────────────────────────────────────────────────

local function lerp(a, b, t) return a + (b - a) * t end
local function clamp01(v)    return math.max(0.0, math.min(1.0, v)) end

local function log(msg)
  if q3now and type(q3now.print) == "function" then q3now.print(msg)
  else print(msg) end
end

-- Resolve a spec value to a float.
--   number     → returned as-is
--   {min, max} → lerp(min, max, skill)   (skill-lerped shorthand)
--   function   → f(skill)                (arbitrary curve, called once at load)
local function resolve(v, skill)
  local t = type(v)
  if t == "number" then
    return v
  elseif t == "table" and #v == 2 then
    return lerp(v[1], v[2], skill)
  elseif t == "function" then
    return v(skill)
  end
  return nil
end

-- ── Characteristic index table (injected by C at Lua init) ────────────────────

-- Populated at Lua init time by SV_Lua_Init before any character is loaded.
-- { "aggression" = 41, "reaction_time" = 6, "aim_accuracy_railgun" = 14, ... }
local INDEX = q3now.characteristics
assert(type(INDEX) == "table" and next(INDEX) ~= nil,
  "q3now.characteristics not injected — check SV_Lua_Init")

-- ── Key resolution ────────────────────────────────────────────────────────────
-- Used by bot/main.lua processing. Resolution algorithm:
--   1. Exact match: INDEX[k]
--   2. Group-prefixed: INDEX[group .. "_" .. k]
--   3. Neither found → invalid key
--
-- Examples (INDEX keys match s_characteristicNames[] in sv_lua.c):
--   resolve_key("aim",   "accuracy")             → INDEX["accuracy"]             (7)
--   resolve_key("aim",   "accuracy_machinegun")   → INDEX["accuracy_machinegun"]  (8)
--   resolve_key("aim",   "skill_rocketlauncher")  → INDEX["skill_rocketlauncher"] (17)
--   resolve_key("traits","aggression")            → INDEX["aggression"]           (41)
--   resolve_key("traits","reaction_time")         → INDEX["reaction_time"]        (6)
--   resolve_key("chats", "kill")                  → INDEX["kill"]                 (28)

local function resolve_key(group, key)
  local idx = INDEX[key]
  if idx then return idx end
  idx = INDEX[group .. "_" .. key]
  if idx then return idx end
  return nil
end

-- Build the set of all valid short keys for a group (for Levenshtein suggestions).
-- A key k is valid in group g iff resolve_key(g, k) returns non-nil, i.e.:
--   - INDEX[k] exists   (any direct characteristic name)
--   - INDEX[g.."_"..k] exists  (any characteristic with the group prefix, stripped)
local function build_valid_keys(group)
  local set = {}
  local prefix = group .. "_"
  local plen   = #prefix
  for name in pairs(INDEX) do
    set[name] = true                               -- step 1: direct match
    if name:sub(1, plen) == prefix then
      set[name:sub(plen + 1)] = true               -- step 2: strip group prefix
    end
  end
  return set
end

local TRAITS_VALID = build_valid_keys("traits")
local AIM_VALID    = build_valid_keys("aim")
local CHATS_VALID  = build_valid_keys("chats")

-- ── Levenshtein "did you mean?" ───────────────────────────────────────────────

local function edit_dist(a, b)
  local la, lb = #a, #b
  if la == 0 then return lb end
  if lb == 0 then return la end
  local prev = {}
  for j = 0, lb do prev[j] = j end
  for i = 1, la do
    local curr = { [0] = i }
    local ai   = a:sub(i, i)
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
  local best, best_d = nil, 3   -- accept edit distance ≤ 2
  for k in pairs(valid_set) do
    local d = edit_dist(key, k)
    if d < best_d then best, best_d = k, d end
  end
  return best
end

-- ── Group walker ──────────────────────────────────────────────────────────────
-- Used for all three groups (traits / aim / chat).
-- Uses the same resolve_key() algorithm for every key — no magic prefixing.

local function walk_group(group, spec_group, valid_set, skill, result, char_name)
  if not spec_group then return end
  for k, v in pairs(spec_group) do
    local idx = resolve_key(group, k)
    if not idx then
      local s = suggest(k, valid_set)
      error(string.format(
        "characters/%s/main.lua: unknown %s key '%s'%s",
        char_name, group, k, s and (" (did you mean '" .. s .. "'?)") or ""), 0)
    end
    local val = resolve(v, skill)
    if val == nil then
      error(string.format(
        "characters/%s/main.lua: %s.%s must be a number, {a,b}, or function",
        char_name, group, k), 0)
    end
    if val < 0.0 or val > 1.0 then
      log(string.format("BotLua: %s %s.%s = %g is outside [0,1]; treated as raw",
          char_name, group, k, val))
    end
    result[idx] = val
  end
end

-- ── Top-level validation ──────────────────────────────────────────────────────

local TOP_VALID = {
  name = true, display_name = true, bio = true, role = true,
  model = true, head = true, skin = true,
  skin_tint = true, icon = true, nicknames = true,
  sounds = true, stats = true,
}

-- Keys that used to live in main.lua but now belong in bot/main.lua.
local BOT_ONLY_KEYS = { traits = true, aim = true, chat = true,
                        icons = true, chat_name = true }

local function validate(spec, char_name)
  if type(spec.name) ~= "string" or spec.name == "" then
    error(string.format(
      "characters/%s/main.lua: required field 'name' must be a non-empty string",
      char_name), 0)
  end
  if type(spec.model) ~= "string" or spec.model == "" then
    error(string.format(
      "characters/%s/main.lua: required field 'model' must be a non-empty string",
      char_name), 0)
  end
  for k in pairs(spec) do
    if BOT_ONLY_KEYS[k] then
      error(string.format(
        "characters/%s/main.lua: '%s' belongs in bot/main.lua, not main.lua",
        char_name, k), 0)
    end
    if not TOP_VALID[k] then
      local s = suggest(k, TOP_VALID)
      error(string.format(
        "characters/%s/main.lua: unknown top-level key '%s'%s",
        char_name, k, s and (" (did you mean '" .. s .. "'?)") or ""), 0)
    end
  end
end

-- ── M.build(spec, skill, char_name) → char table ─────────────────────────────

function M.build(spec, skill, char_name)
  skill     = clamp01(type(skill) == "number" and skill or 0.5)
  char_name = char_name or (type(spec.name) == "string" and spec.name or "unknown")

  validate(spec, char_name)

  local char = {}

  for _, f in ipairs({ "name", "display_name", "bio", "role",
                       "model", "head", "skin", "skin_tint" }) do
    char[f] = spec[f]
  end
  char.icon      = spec.icon
  char.nicknames = spec.nicknames
  char.sounds    = spec.sounds
  char.stats     = spec.stats

  return char
end

-- ── Load trampoline (installed as side-effect of loading this module) ─────────
-- C calls q3now.load_character(name, skillNorm) rather than loading the character
-- module directly. The trampoline loads the raw table and runs M.build().
-- Character files never need to require() or call base.build().

function q3now.load_character(name, skill_norm)
  local raw = q3now.load("characters/" .. name .. "/main")
  if type(raw) ~= "table" then
    error(string.format(
      "characters/%s/main.lua must return a table, got %s",
      name, type(raw)), 0)
  end
  return M.build(raw, skill_norm, name)
end

return M
