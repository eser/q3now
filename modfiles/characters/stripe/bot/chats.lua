-- Stripe chat lines — migrated from stripe_t.c
local chats = {}

local function pick(list)
  local valid = {}
  for _, v in ipairs(list) do
    if v ~= nil then valid[#valid + 1] = v end
  end
  if #valid == 0 then return nil end
  return valid[math.random(#valid)]
end

local function victim(ctx) return (ctx.victim ~= "" and ctx.victim) or "fighter" end
local function killer(ctx) return (ctx.killer ~= "" and ctx.killer) or "someone" end

function chats.game_enter(ctx)
  return pick {
    "Y'all got your boots on. Let's get busy.",
    "I lived in worse places than " .. ctx.map .. ".",
    "All the comforts of home and good company too.",
    "Y'all don't want to get on my bad side today.",
    "Well, well, well.",
    "My reputation precedes me.",
  }
end

function chats.game_exit(ctx)
  return pick {
    "All y'all just got lucky, fighters. Duty calls.",
    "No bugs to squash in " .. ctx.map .. ".",
    "Gotta run. See ya.",
  }
end

function chats.level_start(_ctx)
  return pick {
    "Show me what y'all got, bug-lover.",
    "Come on junior, let's see what y'all got.",
    "Here we go again.",
  }
end

function chats.level_end(ctx)
  if ctx.won == 1 then
    return pick {
      "What else would y'all expect from an elite killing machine?",
      "Move like a dragonfly, hit like a tank, I looked pretty, but y'all don't even rank.",
      "I expect you to salute your superiors.",
      "Too easy.",
      "Crushed it.",
    }
  elseif ctx.won == -1 then
    return pick {
      "I'm a disgrace to the Legion.",
      "Where were y'all when I was on Beta-3?",
      "I'll be back.",
    }
  else
    return pick {
      "I feel like I just kissed my sista'.",
      "I might as well start raising pigeons.",
      "You're excused. But I'm gonna pay you back good.",
      "Good match.",
    }
  end
end

function chats.death(ctx)
  local w = ctx.weapon or ""
  local k = killer(ctx)

  if w == "telefrag" then
    return pick {
      "Get out of my head!",
      "What is this? A friggin' turf war?",
      "That had to hurt.",
      "Personal space. Heard of it?",
    }
  elseif w == "fall" then
    return pick {
      "I wish I was wearing my game shoes.",
      "Double-pump, 360 slam!",
      "D'oh!",
    }
  elseif w == "lava" then
    return pick {
      "And I thought Louisiana Hot Sauce was hot.",
      "Man! Almost as hot as a Texas summer.",
      "Too hot to handle.",
    }
  elseif w == "slime" then
    return pick {
      "AAAAAAAAH! Bug dip.",
      "D'oh!",
    }
  elseif w == "drown" then
    return pick {
      "They didn't cover this in basic.",
      "Well that's just great. Who broke the hydrant?",
      "I can swim. Really.",
    }
  elseif w == "suicide" or w == "crush" then
    return pick {
      "That's not right.",
      "D'oh!",
      "I never was a good rebounder.",
      "Self-inflicted.",
    }
  elseif w == "g" then
    return pick {
      "I might as well put on a dress and go back to the pen.",
      "Lean on the car and spread 'em, right?",
      "Do that again and I'll cut you, man!",
      "That's just rude.",
      "Hands to yourself!",
    }
  elseif w == "rg" then
    return pick {
      "I didn't know I was within five feet of the fence.",
      "That gun don't mess around.",
      "Y'all put that thing down, " .. k .. ". Don't go messing with what you can't handle.",
      "Nice shot, sniper.",
    }
  elseif w == "bfg" then
    return pick {
      "Now I know what a piece of toast feels like.",
      "Y'all don't got the training to use that piece of heat.",
      "There goes the neighborhood.",
      "Big fragging gun. Nice.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "Oh, so you want to play rough?",
        "You piece of crap! Are you wearing a rabbit's foot?",
        "You got lucky.",
        "Amateur hour.",
        "Whatever.",
      }
    else
      return pick {
        "Consider yourself lucky, " .. k .. ". You just offed a bonafide war hero.",
        "Just get my name right on the tombstone, fighter.",
        "Okay, you got me fair and square.",
        "That was skilled.",
      }
    end
  end
end

function chats.kill(ctx)
  local w = ctx.weapon or ""
  local v = victim(ctx)

  if w == "rg" then
    return pick {
      "Y'all give a new meaning to reach out and touch someone, " .. v .. ".",
      "Rails rule.",
      "Sniper? No, artist.",
    }
  elseif w == "g" then
    return pick {
      "Gonna have me a weiner roast; no need to build a fire...",
      "Y'all are one messed up fighter now, " .. v .. ".",
      "I'm not touching you.",
      "Shocking.",
    }
  elseif w == "telefrag" then
    return pick {
      "Get off my turf!",
      "There's one monkey off my back.",
      "I figured y'all forgot to flush, " .. v .. ". So I done it for y'all.",
      "Express delivery.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "The bugs in the kitchen gave me more of a fight than y'all did, " .. v .. ".",
        "Let's hope you never go to prison.",
        "Yo, " .. v .. "! You take that target off when y'all sleep?",
        "Is that all?",
        "Easy.",
        "Better luck next time.",
      }
    else
      return pick {
        "Y'all consider yourself honored. I'm a trained professional.",
        "Y'all were a worthy opponent, for an amateur.",
        "Nice frag.",
        "Good game.",
      }
    end
  end
end

function chats.enemy_suicide(ctx)
  local v = victim(ctx)
  return pick {
    "Taking the easy way out, eh?",
    "Step TO the edge, brainchild. Not OVER it!",
    "First time with live ammo, fighter?",
  }
end

function chats.hit_talking(ctx)
  local k = killer(ctx)
  return pick {
    "Y'all wanna street fight punk? YOU GOT IT!",
    "All's fair in love and war, right? Y'all are so WRONG!",
    "Y'all don't go messing with Stripe!",
    "Wrong move.",
  }
end

function chats.hit_nodeath(ctx)
  local k = killer(ctx)
  return pick {
    "Either I'm getting slow or y'all're using a sloppy weapon.",
    k .. ". y'all are beginning to irritate me.",
    "I must have a tracer on me.",
    "Don't play games with me, " .. k .. ".",
    "Y'all hit the man, he supposed to stay down. Y'all messed up.",
  }
end

function chats.hit_nokill(ctx)
  local v = victim(ctx)
  return pick {
    "I wish I had my .44.",
    "Look at " .. v .. " go! I bet that fighter could outrun police dogs!",
    "Last time I shot something that ran as fast as y'all, I cooked it for dinner.",
    "Be nice if they give us some live ammo.",
  }
end

function chats.random(ctx)
  if math.random() < 0.5 then
    return pick {
      "Are y'all gonna fire that thing or just put us on a fashion show?",
      "Does your momma know where y'all are?",
      "Does " .. ctx.map .. " feel like home to y'all? Good, cuz they'll bury y'all here.",
      "Y'all needs to practice more with live ammo.",
      "Firing them video guns in the arcade don't count as field training.",
      "Watch your back.",
      "I can smell fear.",
    }
  else
    return pick {
      "I can't believe I still have my safety on.",
      "I could go for a big plate of something good, the way my momma made it.",
      "This is where it gets good.",
      "Interesting day.",
      "Getting interesting.",
    }
  end
end

function chats.message(ctx)
  local text = string.lower(ctx.text or "")
  if text:match("hi") or text:match("hello") then
    return pick { "How's it goin'.", nil }
  end
  if text:match("gg") then
    return pick { "Good fight, y'all.", nil }
  end
  return nil
end

return chats
