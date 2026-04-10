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

-- game_enter
function chats.game_enter(ctx)
  return pick {
    "Heh. Fresh Meat.",
    "Time to Rock 'n Roll.",
    "Oh yeah! " .. ctx.map .. " is where Visor lays down the law!",
    "My reputation precedes me.",
    "So, this is where it happens.",
    "Well, well, well.",
  }
end

-- game_exit
function chats.game_exit(_ctx)
  return pick {
    "You know I'll be back.",
    "I'm history. Later kids.",
    "Later.",
    "I'm out.",
  }
end

-- level_start
function chats.level_start(ctx)
  local m = ctx.map ~= "" and ctx.map or "here"
  return pick {
    "This place rocks!",
    m .. " rocks!",
    "I'm ready to win.",
    "Who needs a lesson today?",
    "Alright. Make me look good.",
    "Here we go again.",
    "I was born for this.",
  }
end

-- level_end: won=1 victory, won=-1 last, won=0 mid
function chats.level_end(ctx)
  if ctx.won == 1 then
    return pick {
      "I deserve this!",
      "This is what it's all about, kid!",
      "This is the reason you fighters are here. To make me look good.",
      "Victory is mine!",
      "Did someone call for an exterminator?",
    }
  elseif ctx.won == -1 then
    return pick {
      "Grrrr.",
      "Luck. Pure luck.",
      "Next time, your butts are mine.",
      "I'll be back.",
      "Next time.",
    }
  else
    return pick {
      "Perhaps luck won't favor you next time, eh?",
      "I won't make the same mistakes next time.",
      "Only winning matters here.",
    }
  end
end

-- death: ctx.weapon key dispatches sub-type
function chats.death(ctx)
  local w = ctx.weapon or ""

  if w == "telefrag" then
    return pick {
      "There's only room for one of us behind the mask.",
      "You're in my space, man.",
      "That had to hurt.",
    }
  elseif w == "lava" then
    return pick {
      "Lousy stuff.",
      "This stuff makes me mad.",
      "Whose idea was this?",
      "D'oh!",
      "Too hot to handle.",
    }
  elseif w == "slime" then
    return pick {
      "I never get used to that stuff.",
      "Get that slime in your cybronics and they're never the same again.",
      "D'oh!",
    }
  elseif w == "drown" or w == "fall" then
    return pick {
      "I knew I should have packed a snorkel for this trip.",
      "Wish this visor was fitted with a scuba hook up.",
      "I can swim. Really.",
    }
  elseif w == "suicide" or w == "crush" then
    return pick {
      "Don't laugh kid, you've probably done this a lot.",
      "I've gone and made a mess of myself again.",
      "D'oh!",
      "Self-inflicted.",
    }
  elseif w == "g" then
    return pick {
      "Will you stop touching me?",
      "Do that again and you are toast.",
      "That's just rude.",
      "Hands to yourself!",
    }
  elseif w == "rg" then
    return pick {
      "Heh. Nice shot kid.",
      "I hate campers. I really do.",
      "Did Xian show you how to do that?",
      "Nice shot, sniper.",
    }
  elseif w == "bfg" then
    return pick {
      "Meteor Shower from Hell!",
      "There goes the neighborhood.",
      "Big fragging gun. Nice.",
    }
  else
    -- generic: insult or praise
    if math.random() < 0.5 then
      return pick {
        "Got any ammo left, punk? You're gonna need it.",
        "Did you enjoy that?",
        "I'll bet you enjoyed that more than I did, " .. killer(ctx) .. ".",
        "Yeah, yeah.",
        "Amateur.",
      }
    else
      return pick {
        "Oh, yeah. You had me good there kid.",
        "Not bad kid.",
        "Work on your follow through, kid.",
        "You copied that move from me, didn't you, " .. killer(ctx) .. "?",
        "Okay, you got me fair and square.",
        "That was skilled.",
      }
    end
  end
end

-- kill: dispatches by weapon
function chats.kill(ctx)
  local w = ctx.weapon or ""
  local v = victim(ctx)

  if w == "rg" then
    return pick {
      "Yessssss!!",
      "Gotcha!",
      "You're it!",
      "Railguns rule my universe!",
      "Rails rule.",
      "Sniper? No, artist.",
    }
  elseif w == "g" then
    return pick {
      "Thanks, " .. v .. ". I love doing that!",
      "Humiliation is the name of the game, kid.",
      "Feel the power of the gauntlet!",
      "Shocking.",
    }
  elseif w == "telefrag" then
    return pick {
      "Stand aside, kid. Whoops. Too late.",
      "Nothing but memories left there.",
      "Next time, look both ways.",
      "Mind the teleporter.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "Pick up your toys and go home kid.",
        "I was told you were competition, " .. v .. ". Somebody got that wrong.",
        "And the crowd goes wild.",
        "Better luck next time.",
        "Impressive, not.",
        "Easy.",
      }
    else
      return pick {
        "Not bad, kid. Most don't last that long.",
        "Almost didn't get you there, kid.",
        "Nice frag.",
        "Good game.",
      }
    end
  end
end

-- enemy_suicide
function chats.enemy_suicide(ctx)
  local v = victim(ctx)
  return pick {
    "Saved me the trouble.",
    "Dang it, " .. v .. ". I don't need your help to win here.",
  }
end

-- hit_talking
function chats.hit_talking(ctx)
  local k = killer(ctx)
  return pick {
    "There's no hole deep enough for you to hide in, " .. k .. ".",
    k .. ", I'm gonna ram my gauntlet so far down your skinny throat, I'll untie your shoes.",
    "That was cheap, and you know it, " .. k .. ".",
    "You saying I talk too much, " .. k .. "?",
    "Wrong move.",
  }
end

-- hit_nodeath (bot is hit but not killed)
function chats.hit_nodeath(ctx)
  local k = killer(ctx)
  return pick {
    "You actually hit me, " .. k .. ". I'm impressed, kid.",
    "So kid, you going to make a habit of dinking ol' Visor?",
    "Another scar to remember you by, " .. k .. ".",
    "I've stopped counting the scars.",
    "That hurt, kid. But not enough to matter.",
  }
end

-- hit_nokill (bot hits but doesn't kill)
function chats.hit_nokill(ctx)
  local v = victim(ctx)
  return pick {
    "You survived? It's time to show you my latest move, " .. v .. ".",
    "I win the simple way, by out-fragging the competition.",
    "I hope you heal fast kid, because there's more where that came from.",
    "That'll sting in the morning.",
    "That's it. Run for the health. I know where to find you.",
  }
end

-- random
function chats.random(_ctx)
  if math.random() < 0.5 then
    return pick {
      "You will learn to fear me.",
      "I will be your personal nightmare, kid.",
      "Watch your back.",
      "I can smell fear.",
    }
  else
    return pick {
      "Victory will be mine!",
      "This is all I know. No one can take it from me.",
      "They come by the millions, but I turn them all back.",
      "Nothing fancy here, just solid skill.",
      "This is where it gets good.",
    }
  end
end

-- message: reply to incoming chat
function chats.message(ctx)
  local text = string.lower(ctx.text or "")
  if text:match("hi") or text:match("hello") or text:match("hey") then
    return pick { "Hey.", nil }
  end
  if text:match("gg") then
    return pick { "GG.", nil }
  end
  if text:match("good") or text:match("nice") then
    return pick { "Thanks, kid.", nil }
  end
  return nil
end

return chats
