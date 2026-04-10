-- Major chat lines — migrated from major_t.c
local chats = {}

local function pick(list)
  local valid = {}
  for _, v in ipairs(list) do
    if v ~= nil then valid[#valid + 1] = v end
  end
  if #valid == 0 then return nil end
  return valid[math.random(#valid)]
end

local function victim(ctx) return (ctx.victim ~= "" and ctx.victim) or "soldier" end
local function killer(ctx) return (ctx.killer ~= "" and ctx.killer) or "someone" end

function chats.game_enter(_ctx)
  return pick {
    "I hope you're ready to lose today.",
    "You do know that I'm a VERY poor loser.",
    "Did you miss me?",
    "If you quit now, it will save a lot of bother.",
    "My reputation precedes me.",
    "So, this is where it happens.",
  }
end

function chats.game_exit(_ctx)
  return pick {
    "Just look at the time. I'm late already.",
    "Next time, don't bring the geeks... they make the place look all trashy.",
    "Later.",
  }
end

function chats.level_start(_ctx)
  return pick {
    "I know this place. You are so seriously dead now, warrior.",
    "I'm good to go. Let's do it.",
    "Time to go to work.",
  }
end

function chats.level_end(ctx)
  if ctx.won == 1 then
    return pick {
      "Uh-huh. A girl could really get used to this.",
      "Someone had to win. Today, I'm most definitely someone.",
      "Awwwww. Did mean ol' me pop a hole in someone's pretty little ego?",
      "Victory is mine!",
      "Crushed it.",
    }
  elseif ctx.won == -1 then
    return pick {
      "I can't believe I let you do that.",
      "Do you guys play harder when you think a girl might beat you.",
      "If this were just a game, I wouldn't be so pissed right now.",
      "I'll be back.",
    }
  else
    return pick {
      "So, I went easy on you this time. No biggie.",
      "It won't go the same way next time. Count on that.",
      "Good match.",
    }
  end
end

function chats.death(ctx)
  local w = ctx.weapon or ""
  local k = killer(ctx)

  if w == "telefrag" then
    return pick {
      "Thanks for ruining my day, " .. k .. ".",
      "Personal space. Heard of it?",
      "Ouch.",
    }
  elseif w == "lava" then
    return pick {
      "What is it with all this lava?",
      "Figures. I missed the lava dancing class.",
      "Too hot to handle.",
    }
  elseif w == "slime" then
    return pick {
      "So, which one of you slugs left this slime trail?",
      "Well, " .. k .. ". I can see you left your slime trail here.",
      "D'oh!",
    }
  elseif w == "drown" then
    return pick {
      "Hey, at least I didn't get sand in my swimsuit. I hate that.",
      "Forgot to pack my floaties.",
      "*[sigh]* I miss my rebreather.",
      "I can swim. Really.",
    }
  elseif w == "fall" or w == "suicide" or w == "crush" then
    return pick {
      "Well that was friggin' spectacular. Guess I win the prize.",
      "Oh ha ha. What am I... comic relief?",
      "D'oh!",
    }
  elseif w == "g" then
    return pick {
      k .. ", I suppose your mommy never said it was wrong to hit girls?",
      "I just love it when you do that " .. k .. ". I dare you to try it again.",
      "Did you just touch me?",
    }
  elseif w == "rg" then
    return pick {
      "Well, so much for having a quiet day at home.",
      "Camping AGAIN " .. k .. "? Didn't your therapist tell you to stop?",
      "Nice shot, sniper.",
    }
  elseif w == "bfg" then
    return pick {
      "You're developing a really sick relationship with that thing, " .. k .. ".",
      "C'mon " .. k .. ". Don't you feel cheap and trashy when you do that?",
      "There goes the neighborhood.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "Let's see, " .. k .. "; was that skill or just luck? I'm guessing... luck.",
        "Did that make you feel like a big Warrior " .. k .. ".",
        "That's right " .. k .. ", pick on the girl.",
        "Whatever.",
      }
    else
      return pick {
        "That was definitely... um... pretty good, " .. k .. ". Ok?",
        "I've seen better " .. k .. ", but not many.",
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
      "Pow!! Right in the kisser.",
      "Quick. Clean. Efficient.",
      "Point and squirt. There's something very primal about that.",
      "Hurry back, " .. v .. ". I can use the points.",
      "Sniper? No, artist.",
    }
  elseif w == "g" then
    return pick {
      "That left me breathless.",
      "Whoooooaaa. That felt great " .. v .. ". Mind if I do it again?",
      "Drat, I broke a nail. Heh. Looks like a fair trade.",
      "Feel the power of the gauntlet!",
    }
  elseif w == "telefrag" then
    return pick {
      "A frag's a frag, " .. v .. ", but I feel so... cheap.",
      "Heh. I think you look better this way " .. v .. ".",
      "Special delivery.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "Oh. Did I ruin your day, " .. v .. "?",
        "I came here expecting a challenge.",
        "C'mon, " .. v .. ". Try harder. I'm not breaking a sweat here yet.",
        "Is that all?",
        "Easy.",
      }
    else
      return pick {
        "You made that really hard, " .. v .. ". I like a challenge.",
        "That was so sweet of you, " .. v .. ", giving me a clear shot like that.",
        "Your gore color coordinates so nicely with this arena, " .. v .. ".",
        "Nice frag.",
        "Good game.",
      }
    end
  end
end

function chats.enemy_suicide(ctx)
  local v = victim(ctx)
  return pick {
    "Ugh. That's no way for a warrior to die, " .. v .. ".",
    "Oh, I get it now, " .. v .. ". You're trying to make it more challenging for yourself.",
    v .. ", my mother always said, 'Stupid is as stupid does.'",
    "Victors don't let anyone kill them... including themselves.",
  }
end

function chats.hit_talking(ctx)
  local k = killer(ctx)
  return pick {
    "Have you no honor, " .. k .. "?",
    "Just because you can shoot me while I'm talking doesn't make it right, " .. k .. ".",
    "Two can play that game, " .. k .. ".",
    "Wrong move.",
  }
end

function chats.hit_nodeath(ctx)
  local k = killer(ctx)
  return pick {
    "You should have taken me out when you had the chance, " .. k .. ".",
    "You know the rules, " .. k .. ". Anything less than a frag doesn't count.",
    "Only a flesh wound. I'll live. But YOU won't, " .. k .. ".",
    "Don't blame the gun, " .. k .. ". That was YOUR fault.",
  }
end

function chats.hit_nokill(ctx)
  local v = victim(ctx)
  return pick {
    "Shoot! Something jiggled my arm!",
    "I hate wasting ammo like that.",
    "You could have done me the courtesy of dying, " .. v .. ".",
  }
end

function chats.random(_ctx)
  if math.random() < 0.5 then
    return pick {
      "I've decided I like you better when you're dead.",
      "Oh, was that your ego I deflated back there?",
      "You know, bathing regularly isn't considered a crime.",
      "Watch it, boys.",
      "You wish.",
      "Tick, tick, tick...",
    }
  else
    return pick {
      "What am I doing here? I promised myself no more hanging with losers.",
      "Can we speed this up? I'm falling asleep out here.",
      "Time to focus.",
      "Just warming up.",
      "Getting interesting.",
    }
  end
end

function chats.message(ctx)
  local text = string.lower(ctx.text or "")
  if text:match("hi") or text:match("hello") then
    return pick { "Did you miss me?", nil }
  end
  if text:match("gg") then
    return pick { "Good game.", nil }
  end
  return nil
end

return chats
