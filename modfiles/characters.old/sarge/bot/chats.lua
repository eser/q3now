-- Sarge chat lines — migrated from sarge_t.c
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
    "Mmmmm... Smells like fresh meat.",
    "TEN-SHUN! Sarge is in the House.",
    "Alright, Hero. It's time for some Pain.",
    "*SNIFF* I smell victory in the air today.",
    "Time to make an entrance.",
  }
end

function chats.game_exit(_ctx)
  return pick {
    "Keep up the pace soldiers. I'm up for some R and R.",
    "Duty calls. Try to keep from fragging yourselves.",
    "Duty calls.",
    "Later.",
    "Gotta run. See ya.",
  }
end

function chats.level_start(_ctx)
  return pick {
    "Let's get this party started.",
    "Alright, Heroes. It's time for some Pain.",
    "Everyone drop and give me 20!",
    "Let's get this over with.",
    "It's time to separate the warriors from the kiddies.",
  }
end

function chats.level_end(ctx)
  if ctx.won == 1 then
    return pick {
      "You call that effort? You're not worthy of my ordinance, pissant.",
      "I'm feeling mighty fine right now. Mighty fine indeed.",
      "Yeah! The sweet smell of victory.",
      "I wish my momma could've been here to see this day.",
      "Did someone call for an exterminator?",
    }
  elseif ctx.won == -1 then
    return pick {
      "Frag off. I'll see you again.",
      "There will be some serious payback for this.",
      "Gloat while you can, fragbait. Gloat while you can.",
      "Looks like ol' Sarge's tactics could use some re-evaluation.",
      "I'll be back.",
    }
  else
    return pick {
      "Well fought troops. Well fought.",
      "I think we all earned a medal for that one.",
      "You call that a good match? Sloppy. Very sloppy.",
    }
  end
end

function chats.death(ctx)
  local w = ctx.weapon or ""
  local k = killer(ctx)

  if w == "telefrag" then
    return pick {
      "Rrrrrr.",
      "Well I'll be a son-of-a ... gun.",
      "That's gonna cost you a month of K-P duty, " .. k .. "!",
      "Ouch.",
    }
  elseif w == "lava" then
    return pick {
      "I didn't come here to be barbequed.",
      "Blast! Was it my turn to bring the marshmallows?",
      "Hot stuff.",
    }
  elseif w == "slime" then
    return pick {
      "Horrible stuff.",
      "Rrrrrr.",
      "I think I had that slime crap in an MRE once.",
      "D'oh!",
    }
  elseif w == "drown" then
    return pick {
      "I'm Army, not some waterlogged Marine.",
      "D'oh!",
    }
  elseif w == "fall" or w == "suicide" or w == "crush" then
    return pick {
      "Oooo. Don'tcha just hate it when that happens.",
      "Accidents happen.",
      "This battlefield is booby-trapped!",
      "I did that to make the odds more fair.",
      "That's no way for a warrior to die.",
      "No one said ANYTHING about land mines!",
    }
  elseif w == "g" then
    return pick {
      "Rrrrrr.",
      "You comin' on to me, fighter?",
      "You're allowed to do that just once, soldier.",
      "That's an ugly way to die.",
      "That's just rude.",
    }
  elseif w == "rg" then
    return pick {
      "Everyone gets lucky, once in awhile, " .. k .. ".",
      "Dirty, rotten, lousy sniper.",
      "A lucky shot, " .. k .. ".",
      "Nice shot, sniper.",
    }
  elseif w == "bfg" then
    return pick {
      "A real soldier can't depend on weapons of mass destruction.",
      "Next time, Private, face to face like real soldiers.",
      "Worthless fragbait.",
      "Your mommy say you could use that big gun, " .. k .. "?",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "Wipe that smile off your face, " .. k .. ". You ain't out of the woods yet.",
        "Anybody can be lucky once, fragbait. Do you understand me?",
        "My dog could have done that better and quicker.",
        "Sloppy, soldier. Very sloppy.",
        "That was my favorite cigar back there. Now I'm angry.",
        "You've had your lucky shot for the day, " .. k .. ".",
      }
    else
      return pick {
        "Nice shootin', Fragbait. Just don't be getting' cocky.",
        "Well, " .. k .. ". There's hope for you yet, soldier.",
        "Good shooting.",
        "We'll make a soldier out of you yet, " .. k .. ".",
      }
    end
  end
end

function chats.kill(ctx)
  local w = ctx.weapon or ""
  local v = victim(ctx)

  if w == "rg" then
    return pick {
      "Alright!",
      "Remind me to show you my sharpshooter ribbon.",
      "This is a truly fine weapon.",
      "Sniper? No, artist.",
    }
  elseif w == "g" then
    return pick {
      "Lesson Number One: Never let the enemy get that close.",
      "That's just me bein' friendly, " .. v .. ".",
      "I'm not touching you.",
    }
  elseif w == "telefrag" then
    return pick {
      "Alright!",
      "Let that be a lesson to you soldier... move your butt next time.",
      "Express delivery.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "It's just like a bad dream ain't it.",
        "Get used to it, fragbait.",
        "Heheh. Now you come in wallet size, " .. v .. ".",
        "This your first day with that weapon?",
        "You'd last about two seconds in a real shootin' war, " .. v .. ".",
        v .. ", you are making this far too easy.",
      }
    else
      return pick {
        "Fancy move, " .. v .. ". But it still got you killed.",
        "Finally, a challenge worthy of my skills.",
        "I see you've been practicing again, " .. v .. ".",
        "We'll make a soldier out of you yet, " .. v .. ".",
      }
    end
  end
end

function chats.enemy_suicide(ctx)
  local v = victim(ctx)
  return pick {
    "This your first day with that weapon, " .. v .. "?",
    "Oooo. Don'tcha just hate it when that happens.",
    "Barely enough to send home to Momma.",
  }
end

function chats.hit_talking(ctx)
  local k = killer(ctx)
  return pick {
    "What is it that you don't understand about a fair fight, fragbait?",
    "You've just moved to the top of my hit list, " .. k .. ".",
    "Shootin' a man while he's talkin' just rubs me raw, " .. k .. ".",
    "Wrong move.",
  }
end

function chats.hit_nodeath(ctx)
  local k = killer(ctx)
  return pick {
    "You comin' on to me, Private?",
    "Bring it on! Bring it on!",
    "Don't play games with me, " .. k .. ".",
  }
end

function chats.hit_nokill(ctx)
  local v = victim(ctx)
  return pick {
    "It'd be quicker if you just hold still.",
    "That's just me bein' friendly.",
    "Move around more, " .. v .. "! You're too easy a target.",
  }
end

function chats.random(_ctx)
  if math.random() < 0.5 then
    return pick {
      "Rrrrrr.",
      "It'd be quicker if you just hold still.",
      "You want a piece of me? You gotta take it the hard way!",
      "Come and get me fragbait.",
      "My old granny would be opening a can of whoopass here.",
      "You're going down.",
    }
  else
    return pick {
      "Alright!",
      "Rrrrrr.",
      "Yeah!",
      "Bring it on! Bring it on!",
      "This is where it gets good.",
      "It's time to wrap this up and move on.",
    }
  end
end

function chats.message(ctx)
  local text = string.lower(ctx.text or "")
  if text:match("hi") or text:match("hello") then
    return pick { "Soldier.", nil }
  end
  if text:match("gg") then
    return pick { "Good fight.", nil }
  end
  return nil
end

return chats
