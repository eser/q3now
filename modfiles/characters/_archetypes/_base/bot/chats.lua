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

function chats.kill(ctx)
  local w = ctx.weapon or ""
  local v = victim(ctx)

  if w == "rg" then
    return pick {
      "Rail shot. Clean.",
      "You didn't see that coming, " .. v .. ".",
      "One shot.",
    }
  elseif w == "g" then
    return pick {
      "Gauntlet. Embarrassing for you, " .. v .. ".",
      "Nothing like getting up close.",
      "Touch of death.",
    }
  elseif w == "mg" then
    return pick {
      "Stay still next time, " .. v .. ".",
      "Machinegun does the job.",
      "Hosed you down.",
    }
  elseif w == "sg" then
    return pick {
      "Didn't have time to run, " .. v .. ".",
      "Shotgun. Close and final.",
      "You were too close.",
    }
  elseif w == "gl" then
    return pick {
      "Watch the corners, " .. v .. ".",
      "Grenade does what grenades do.",
      "Didn't dodge in time.",
    }
  elseif w == "rl" then
    return pick {
      "Rockets. Classic.",
      "That blast had your name on it, " .. v .. ".",
      "Splash damage works.",
    }
  elseif w == "lg" then
    return pick {
      "Held the beam. You went down.",
      "Lightning finds the target.",
      "Arc on, " .. v .. " out.",
    }
  elseif w == "pr" then
    return pick {
      "Plasma bolts add up, " .. v .. ".",
      "Full burst. You're done.",
      "Plasma gets the job done.",
    }
  elseif w == "telefrag" then
    return pick {
      "Watch the teleporter next time.",
      "Wrong place, wrong time.",
    }
  else
    return pick {
      v .. " is down.",
      "Target acquired. Target down.",
      "One less, " .. v .. ".",
      "Eliminated.",
    }
  end
end

function chats.death(ctx)
  local w = ctx.weapon or ""
  local k = killer(ctx)

  if w == "telefrag" then
    return pick { "Teleporter ambush.", "I walked into that." }
  elseif w == "lava" then
    return pick { "Map gets me again.", "Hot." }
  elseif w == "slime" or w == "drown" or w == "fall" then
    return pick { "Map hazard.", "My fault." }
  elseif w == "suicide" or w == "crush" then
    return pick { "Self-inflicted.", "That was on me." }
  elseif w == "g" then
    return pick {
      "Gauntlet. Really, " .. k .. ".",
      "You came that close?",
      "Next time I'll hear you coming.",
    }
  elseif w == "rg" then
    return pick {
      "Good shot, " .. k .. ".",
      "Clean rail. I'll give you that.",
      "One shot. Well aimed.",
    }
  elseif w == "mg" then
    return pick {
      "Had the angle on me.",
      "That was a lot of bullets, " .. k .. ".",
      "Suppressed and eliminated.",
    }
  elseif w == "sg" then
    return pick {
      "Too close.",
      "Didn't see you coming, " .. k .. ".",
      "Shotgun at that range. No chance.",
    }
  elseif w == "gl" then
    return pick {
      "Didn't clear the corner fast enough.",
      "That grenade had good timing.",
      "Bounced right into me.",
    }
  elseif w == "rl" then
    return pick {
      "Rockets. Yeah.",
      "Caught the splash.",
      "Good shot, " .. k .. ".",
    }
  elseif w == "lg" then
    return pick {
      "Held the beam too long.",
      "Couldn't shake the lightning.",
      "Good beam control, " .. k .. ".",
    }
  elseif w == "pr" then
    return pick {
      "Plasma gets there fast.",
      "Full burst. Fair enough.",
      "Couldn't dodge all of it.",
    }
  elseif w == "bfg" then
    return pick { "BFG. Of course.", "Hard to dodge that." }
  else
    return pick {
      "Got me.",
      "I'll do better.",
      "Good fight, " .. k .. ".",
      "Round goes to " .. k .. ".",
    }
  end
end

function chats.team_need_health(_ctx)
  return pick { "Need health!", "Low health.", nil }
end

function chats.team_need_weapon(_ctx)
  return pick { "Need ammo.", nil }
end

function chats.team_cover_me(_ctx)
  return pick { "Cover me!", "Help!", nil }
end

function chats.team_follow_me(_ctx)
  return pick { "Follow me.", nil }
end

function chats.team_enemy_base_attack(_ctx)
  return pick { "Attacking their base.", nil }
end

function chats.team_defending_base(_ctx)
  return pick { "Defending base.", nil }
end

function chats.team_got_flag_need_support(_ctx)
  return pick { "I have the flag! Help!", "Flag! Cover me!", nil }
end

function chats.powerup_quad(_ctx)
  return pick { "Quad damage!", "Quad acquired.", nil }
end

function chats.powerup_haste(_ctx)
  return pick { "Haste!", nil }
end

function chats.powerup_invis(_ctx)
  return pick { "Invisible now.", nil }
end

function chats.powerup_regen(_ctx)
  return pick { "Regeneration.", nil }
end

function chats.powerup_battlesuit(_ctx)
  return pick { "Battlesuit on.", nil }
end

function chats.powerup_enemy_quad(_ctx)
  return pick { "Enemy has quad!", "Watch out, quad!", nil }
end

function chats.powerup_enemy_any(_ctx)
  return pick { "Enemy powerup.", nil }
end

function chats.kill_double(_ctx)
  return pick { "Double kill.", nil }
end

function chats.kill_streak_5(ctx)
  return pick { tostring(ctx.count or 5) .. " in a row.", nil }
end

function chats.kill_streak_10(ctx)
  return pick { tostring(ctx.count or 10) .. " straight.", "On a roll.", nil }
end

function chats.kill_rampage(ctx)
  return pick { "Rampage. " .. tostring(ctx.count or 15) .. " frags.", nil }
end

function chats.score_first_place(_ctx)
  return pick { "First place.", "I'm on top.", nil }
end

function chats.score_falling_back(_ctx)
  return pick { "Falling back.", "Someone got ahead.", nil }
end

function chats.score_last_place(_ctx)
  return pick { "Last place.", "I'll catch up.", nil }
end

function chats.score_frag_milestone(ctx)
  local c = ctx.count or ctx.score or 0
  return pick { tostring(c) .. " frags.", nil }
end

function chats.ctf_got_flag(_ctx)
  return pick { "Got the flag.", "Flag secured.", "I have it." }
end

function chats.ctf_enemy_got_flag(_ctx)
  return pick { "They have our flag.", "Get the flag back.", nil }
end

function chats.ctf_returning_flag(_ctx)
  return pick { "Flag returned.", "Got it back.", nil }
end

function chats.ctf_capture(_ctx)
  return pick { "Captured.", "Flag in.", "Score." }
end

function chats.ctf_flag_dropped(_ctx)
  return pick { "Flag's loose.", "Someone pick it up.", nil }
end

function chats.ctf_attack(_ctx)
  return pick { "Going for the flag.", "Attacking.", nil }
end

function chats.ctf_defend(_ctx)
  return pick { "Defending.", "Holding base.", nil }
end

function chats.random(_ctx)
  return pick {
    "Stay sharp.",
    "Keep moving.",
  }
end

function chats.message(ctx)
  local text = string.lower(ctx.text or "")
  if text:match("hi") or text:match("hello") or text:match("hey") or text:match("sup") or text:match("what.?s up") then
    return pick { "Hey.", "Hi.", nil }
  end
  if text:match("gg") or text:match("good game") then
    return pick { "GG.", "Good game.", nil }
  end
  if text:match("nice") or text:match("good shot") or text:match("well played") or text:match("wp") then
    return pick { "Thanks.", "Appreciated.", nil }
  end
  if text:match("noob") or text:match("suck") or text:match("bad") or text:match("loser") or text:match("trash") then
    return pick { "Talk later.", "Focus.", nil }
  end
  if text:match("%?") then
    return pick { "Yes.", "No.", "Dunno.", nil }
  end
  if ctx.team and ctx.team ~= 0 then
    return pick { "Copy.", "Got it.", nil }
  end
  return pick { "...", "Noted.", nil }
end

function chats.reply(ctx)
  local text = string.lower(ctx.text or "")
  if text:match("gg") or text:match("good game") or
     text:match("well played") or text:match("wp") then
    return pick { "GG.", "Good game.", nil }
  end
  if text:match("noob") or text:match("suck") or
     text:match("bad") or text:match("trash") then
    return pick { "Talk after.", "Sure.", nil }
  end
  if text:match("nice") or text:match("good shot") then
    return pick { "Thanks.", nil }
  end
  if text:match("%?") then
    return pick { "Yes.", "No.", nil }
  end
  return pick { "Noted.", nil }
end

return chats
