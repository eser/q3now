-- characters/_base/bot/chats.lua
-- Default chat lines. Characters without their own bot/chats.lua inherit these.
local chats = {}

local function pick(list)
  local valid = {}
  for _, v in ipairs(list) do
    if v ~= nil then valid[#valid + 1] = v end
  end
  if #valid == 0 then return nil end
  return valid[math.random(#valid)]
end

local function victim(ctx) return (ctx.victim ~= "" and ctx.victim) or "you" end
local function killer(ctx) return (ctx.killer ~= "" and ctx.killer) or "someone" end

function chats.game_enter(_ctx)
  return pick {
    "Let's go.",
    "Time to fight.",
    "Ready.",
  }
end

function chats.game_exit(_ctx)
  return pick {
    "Good fight.",
    "Until next time.",
    "Out.",
  }
end

function chats.level_start(_ctx)
  return pick {
    "This ends now.",
    "Let's do this.",
  }
end

function chats.level_end(ctx)
  if ctx.place == 1 then
    return pick { "Victory.", "First place." }
  end
  return pick { "Next time.", "I'll do better." }
end

function chats.level_end_eliminated(_ctx)
  return pick { "Eliminated.", "I'll be back." }
end

function chats.hit_talking(ctx)
  return pick {
    "Quit talking, " .. victim(ctx) .. ".",
    "No time to chat.",
  }
end

function chats.hit_nodeath(ctx)
  return pick {
    "That all you got, " .. victim(ctx) .. "?",
    "Barely felt it.",
  }
end

function chats.hit_nokill(ctx)
  return pick {
    "Missed.",
    "Next shot.",
  }
end

function chats.enemy_suicide(ctx)
  return pick {
    victim(ctx) .. " did the work for me.",
    "Saved me the effort.",
  }
end

function chats.kill_insult(ctx)
  return pick {
    victim(ctx) .. " is down.",
    "Target eliminated.",
  }
end

function chats.death_insult(ctx)
  return pick {
    "Got me, " .. killer(ctx) .. ".",
    "I'll remember that.",
  }
end

function chats.random(_ctx)
  return pick {
    "Stay sharp.",
    "Keep moving.",
  }
end

function chats.reply(_ctx)
  return pick {
    "...",
    "Noted.",
  }
end

return chats
