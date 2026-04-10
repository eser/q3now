-- Daemia chat lines — migrated from daemia_t.c
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
    "Daemia enters the Arena, stage left. You fighters can all cower.",
    "I think I'm gonna like this place.",
    "Name's Daemia, and I'm hauling all of you in.",
    "I love " .. ctx.map .. "!",
    "Buenos Dias.",
    "My reputation precedes me.",
    "Well, well, well.",
  }
end

function chats.game_exit(_ctx)
  return pick {
    "Better worlds to conquer than this fighter heaven.",
    "I got contracts to catch up on.",
    "Hasta la vista.",
    "Te hablo luego.",
    "Adios.",
    "Later.",
  }
end

function chats.level_start(ctx)
  return pick {
    "Do I make you fighters nervous? If you are smart, you'd be nervous.",
    "It was a dark and rainy night in " .. ctx.map .. ". Suddenly, A shot rang out!",
    "I was born for this.",
    "Time to go to work.",
  }
end

function chats.level_end(ctx)
  if ctx.won == 1 then
    return pick {
      "Superior training, superior tactics, superior everything.",
      "I'm surprised... that it took me so long.",
      "La venganza es dulce.",
      "Te gane'.",
      "Victory is mine!",
      "Crushed it.",
    }
  elseif ctx.won == -1 then
    return pick {
      "Caramba! When did hell freeze over?",
      "Ay de mi'! This is a very bad thing. Que' malo.",
      "Me ganaste.",
      "Este el fin.",
      "I'll be back.",
    }
  else
    return pick {
      "Ay de mi'! It smells like purgatory. Huele malo.",
      "As far as I'm concerned, you're all a bunch of numbers and I'm about to win the lottery.",
      "Good match.",
    }
  end
end

function chats.death(ctx)
  local w = ctx.weapon or ""
  local k = killer(ctx)

  if w == "telefrag" then
    return pick {
      "You could've knocked first, you know.",
      "That had to hurt.",
      "Personal space. Heard of it?",
    }
  elseif w == "fall" then
    return pick {
      "Caramba! I've just enrolled in the WILE E. School of Stupidity.",
      "It's not how you fall. It's how you land that coun...",
      "No, jerk, I can't see my house from up there.",
    }
  elseif w == "lava" then
    return pick {
      "To me, Hell is like tropic resort. A vacation.",
      "No big deal. I had that stuff in my back yard.",
      "Hot stuff.",
    }
  elseif w == "slime" then
    return pick {
      "At least the goo that killed me was brighter than " .. k .. ".",
      "Revenge of the lime gelatin.",
      "The slime, he reminds me of my ex-husband, only cuter.",
    }
  elseif w == "drown" then
    return pick {
      "Well, this pool's going to need a good brushing down.",
      "Ay chihuahua! ... and my gear's dry-clean only. Figures.",
      "I can swim. Really.",
    }
  elseif w == "suicide" or w == "crush" then
    return pick {
      "You know you're getting too old for this when...",
      "Madre de dios!",
      "Ay chihuahua!",
      "D'oh!",
      "Self-inflicted.",
    }
  elseif w == "g" then
    return pick {
      "Scratch a little lower, " .. k .. ".",
      "I don't do 'patty-cake' on a first date, jerk.",
      "That's just rude.",
      "Did you just touch me?",
    }
  elseif w == "rg" then
    return pick {
      "Way to go, fighter. Let's get us a 'pic-a-nic' basket and go camping.",
      "How did you fire that thing? Chickens don't got fingers.",
      "Nice shot, sniper.",
    }
  elseif w == "bfg" then
    return pick {
      "It's not how big your gun is jerk... well, maybe for you it is.",
      "No skill needed there " .. k .. ".",
      "You liked that, si?",
      "Big fragging gun. Nice.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "You got me because I aimed for your brain, " .. k .. ". Impossible shot.",
        "Te veo en el infierno!",
        "Asi' se hace!",
        "You got lucky.",
        "Whatever.",
      }
    else
      return pick {
        "You're all right, " .. k .. ". You just might be around for a while.",
        "Nice job, " .. k .. ". Maybe next time, they let you carry a real gun.",
        "Ay caramba! You do that pretty good, " .. k .. ".",
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
      "Train leaving on track four! All aboard!",
      "Splat. Nothing to take back for a bounty.",
      "Orale!",
      "Me gusta asi!",
      "Rails rule.",
      "Sniper? No, artist.",
    }
  elseif w == "g" then
    return pick {
      "Give me that! You'll hurt someone with it.",
      "Coochie-coochie-coo!",
      "Es suficiente castigo?",
      "Feel the power of the gauntlet!",
    }
  elseif w == "telefrag" then
    return pick {
      "You know when those little hairs on the back of your neck stand up...",
      "Did you eat something that didn't agree with you, " .. v .. "?",
      "Con ganas!",
      "Express delivery.",
      "Next time, look both ways.",
    }
  else
    if math.random() < 0.5 then
      return pick {
        "Don't worry, that toe tag will let your mommy know who you are.",
        "Who's your Mommy?",
        "It's never too late for a change of career, " .. v .. ".",
        "Trust me when I say this: '" .. v .. ", you suck.'",
        "Te das por vencido " .. v .. ".",
        "Te gane'.",
        "Easy.",
        "Is that all?",
      }
    else
      return pick {
        "Well " .. v .. ", you move better than a mannequin. There's that.",
        "You did your best, " .. v .. ",...that's what should really bother you.",
        "Bueno! I didn't hear you whine once when I killed you, " .. v .. "!",
        "Gracias! You die pretty good for a fighter.",
        "Nice frag.",
        "Good game.",
      }
    end
  end
end

function chats.enemy_suicide(ctx)
  local v = victim(ctx)
  return pick {
    "Well, I can see you don't need me, " .. v .. ".",
    "Typical low-life. Always looking for the easy way out.",
    "You really shouldn't look down the barrel to see if it's loaded.",
  }
end

function chats.hit_talking(ctx)
  local k = killer(ctx)
  return pick {
    "If you're shooting, you're not listening.",
    "Jerk!",
    "I spit on you, " .. k .. "!",
    "Wrong move.",
    "Don't mess with me.",
  }
end

function chats.hit_nodeath(ctx)
  local k = killer(ctx)
  return pick {
    "Ow! Don't make me come over there, " .. k .. ".",
    "You are SO gonna wish you could take that back.",
    "I was gonna go easy on you... but no more, jerk.",
    "Score one for " .. k .. ". We'll put that on " .. k .. "'s tombstone.",
    "Gracias, " .. k .. ". I had an itch I couldn't reach there.",
  }
end

function chats.hit_nokill(ctx)
  local v = victim(ctx)
  return pick {
    "Cha-ching!",
    "That's like putting money in the bank.",
    "Keep running away. I can hit a moving target, no problem.",
    "I'm a pinball wizard, fighter.",
    "Es suficiente o quieres mas?",
    "No lo creo.",
  }
end

function chats.random(ctx)
  if math.random() < 0.5 then
    return pick {
      "Anyone ever mistake you for someone dangerous?",
      "Is that your face, or are you mooning me?",
      "Hey, I'm salt and you're a slug. Bad combination.",
      "Let me guess. You ate a lot of paint chips when you were a kid?",
      "So you got brain damage? Looks like an improvement to me.",
      "El mas fuerte no es siempre el mejor.",
      "This is embarrassing.",
      "I've seen better.",
      "You wish.",
    }
  else
    return pick {
      "El que rie de ultimo rie mejor.",
      "See the universe. Frag interesting people.",
      "I'm hauling all you fighters in.",
      "Medic! Quick! I'm laughing so hard I'm choking.",
      "Orale!",
      "This is where it gets good.",
      "Time to focus.",
    }
  end
end

function chats.message(ctx)
  local text = string.lower(ctx.text or "")
  if text:match("hi") or text:match("hello") or text:match("hey") then
    return pick { "Buenos Dias.", nil }
  end
  if text:match("gg") then
    return pick { "GG. Te gane'.", nil }
  end
  return nil
end

return chats
