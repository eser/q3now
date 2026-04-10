-- Grunt chat lines — migrated from grunt_t.c
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

function chats.game_enter(ctx)
  return pick {
    "Anyone standing in my way is gonna be dogmeat.",
    "A Marine laughs at danger. And I see nothing here to laugh about.",
    "I am jacked in and good to go.",
    "Outstanding!",
    ctx.map .. " is my kind of place!",
    "How's it hangin'?",
    "Nice place you got here. Could use a few gibs though.",
    "Well, well, well.",
    "Time to make an entrance.",
  }
end

function chats.game_exit(_ctx)
  return pick {
    "I am so outta here.",
    "Gotta run. Happy fraggin'.",
    "It's time for me to bug on out of here.",
    "Gotta run. See ya.",
  }
end

function chats.level_start(_ctx)
  return pick {
    "Let's get this party started.",
    "OK you rubber monkeys, make me look good.",
    "Let's rumble.",
    "I'm psyched up. Let's get it on.",
  }
end

function chats.level_end(ctx)
  if ctx.won == 1 then
    return pick {
      "Winning ain't everything ... but losing sure stinks, right, " .. ctx.map .. "?",
      "Wasn't my turn to win today ... so I took someone else's.",
      "My momma didn't raise me to be a loser.",
      "Crushed it.",
    }
  elseif ctx.won == -1 then
    return pick {
      "Losing make me mad. Real mad. Sort of homicidal.",
      "Losing sucks big time.",
      "I'll be back.",
    }
  else
    return pick {
      "At least I'm not in the basement on this one.",
      "Good match.",
    }
  end
end

function chats.death(ctx)
  local w = ctx.weapon or ""
  local k = killer(ctx)

  if w == "telefrag" then
    return pick {
      "You're in my space.",
      "That had to hurt.",
    }
  elseif w == "lava" then
    return pick {
      "I normally like the hot stuff.",
      "Flip me over when I'm about medium rare.",
    }
  elseif w == "slime" then
    return pick {
      "Bah! I drink coffee stronger than that.",
      "Reminds me of the battery acid we used to get at the mess hall.",
    }
  elseif w == "drown" then
    return pick {
      "Guess I was having too good a time down there.",
      "There's Pirate Treasure down here!",
      "D'oh!",
    }
  elseif w == "fall" or w == "suicide" or w == "crush" then
    return pick {
      "I ought to get hazard pay for this.",
      "I'm better than that. Honest.",
      "I swear somebody rearranged the arena.",
      "Heh. Had me a terminal case of hangnail there.",
    }
  elseif w == "g" then
    return pick {
      "You gotta have a lotta guts to come here and do that, " .. k .. ".",
      "I'm impressed. Now it's my turn.",
      k .. " You are in for a beating now.",
      "Thanks for sharing that with me, " .. k .. ".",
      "That's just rude.",
    }
  elseif w == "rg" then
    return pick {
      "OK, that was a good shot.",
      "That just ruins my day. Now I gotta ruin yours, " .. k .. ".",
      "Nice shot, sniper.",
    }
  elseif w == "bfg" then
    return pick {
      k .. ", I'm tempted to grab that thing and shove it ...",
      "Use that thing once more and you're gonna eat it " .. k .. ".",
      "Big fragging gun. Nice.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "I gave you an easy shot there " .. k .. ".",
        "OK, " .. k .. ", now try that on a moving target.",
        "Nobody does that to Grunt and just walks away from it.",
        "Lousy stinking .... Camper!",
        "Amateur.",
      }
    else
      return pick {
        "Not half bad there " .. k .. ".",
        "This could be more difficult than I thought.",
        "You been taking lessons, " .. k .. "?",
        "Okay, you got me fair and square.",
      }
    end
  end
end

function chats.kill(ctx)
  local w = ctx.weapon or ""
  local v = victim(ctx)

  if w == "rg" then
    return pick {
      "'Old Betsy' still works like a charm.",
      "I just love modern technology.",
      v .. ", consider yourself terminated with extreme prejudice.",
      "Rails rule.",
    }
  elseif w == "g" then
    return pick {
      "Feel the POWER!",
      "Ow. My hand hurts now.",
      "Beats a hand blaster any day.",
      "Shocking.",
    }
  elseif w == "telefrag" then
    return pick {
      "Blood and gore all over the floor and me without a spoon.",
      "Express delivery.",
      "Next time, look both ways.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "That's another score for the good guys.",
        "Of course I'm picking on you, " .. v .. ". You're an easy target.",
        "I'm trying to cure you of the uglies, but it ain't working.",
        "Easy.",
        "Is that all?",
        "Better luck next time.",
      }
    else
      return pick {
        "You're a hard foe to pin down, " .. v .. ".",
        "Dang, you're good, " .. v .. "! That frag should count double.",
        "For a moment there, I thought you were a Marine, " .. v .. ".",
        "Nice frag.",
      }
    end
  end
end

function chats.enemy_suicide(ctx)
  local v = victim(ctx)
  return pick {
    "Dang it, " .. v .. ". I don't need your help to win here.",
    "That can't be good for your complexion " .. v .. ".",
    "Dr. Death would be so proud of you " .. v .. ".",
  }
end

function chats.hit_talking(ctx)
  local k = killer(ctx)
  return pick {
    "Gahhhh. It figures that " .. k .. " would be the kind of jerk who'd do that.",
    "Yeah, shoot me while I'm an easy target why don't ya, " .. k .. ".",
    "I was on the phone!",
    "Wrong move.",
  }
end

function chats.hit_nodeath(ctx)
  local k = killer(ctx)
  return pick {
    k .. ", buddy, you are going to regret not fragging me in one shot.",
    "I've had worse cuts shaving, " .. k .. ".",
    "Looks like you just failed target practice today, " .. k .. ".",
  }
end

function chats.hit_nokill(ctx)
  local v = victim(ctx)
  return pick {
    "Yeah!! I could feel that one hit all the way over here.",
    "You survived that, " .. v .. "? I must be gettin' old.",
    "I think I found my range with that hit.",
  }
end

function chats.random(_ctx)
  if math.random() < 0.5 then
    return pick {
      "If only the good die young, I think you just might live forever.",
      "I don't think you can handle me.",
      "And here I thought this match was going to be challenging.",
      "You're going down.",
      "Bring it on.",
    }
  else
    return pick {
      "This is where it gets good.",
      "Time to focus.",
      "I'm too hot for my own good.",
      "I've got a grenade with your name on it.",
      "I'm ready to dance when you are.",
    }
  end
end

function chats.message(ctx)
  local text = string.lower(ctx.text or "")
  if text:match("hi") or text:match("hello") then
    return pick { "How's it hangin'.", nil }
  end
  if text:match("gg") then
    return pick { "Good fight.", nil }
  end
  return nil
end

return chats
