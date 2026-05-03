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
  elseif w == "mg" then
    return pick {
      "How many bullets did that take?",
      "Suppressed by a machinegun. Filing a complaint.",
      "You just hosed me down. Real elegant.",
      "That's what a hundred rounds buys you.",
    }
  elseif w == "sg" then
    return pick {
      "Had to get up close for that, huh?",
      "Buckshot. Classy.",
      "Point blank. You fight dirty.",
      "Scatter me once, shame on me.",
    }
  elseif w == "gl" then
    return pick {
      "Lucky bounce.",
      "Right, the grenades. Of course.",
      "Bounced it right off the wall. I'll give you that one.",
      "Predictable weapon. Unpredictable timing.",
    }
  elseif w == "rl" then
    return pick {
      "Classic. Splash damage and zero finesse.",
      "Rocket. Fine. Good shot.",
      "You and your rockets.",
      "Caught it right in the blast radius. Rookie mistake.",
    }
  elseif w == "lg" then
    return pick {
      "Decent arc on that lightning.",
      "You held the beam. I respect that.",
      "Arc-welded by " .. killer(ctx) .. ".",
      "Kept the beam on me. Not bad.",
    }
  elseif w == "pr" then
    return pick {
      "Plasma. Efficient.",
      "You just pellet-spammed me to death.",
      "Plasma rifle does the job, I suppose.",
      "Bolts in the face. I've had worse mornings.",
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
  elseif w == "mg" then
    return pick {
      "Spent half a clip on you. Worth it.",
      "You just got hosed, " .. v .. ".",
      "Machinegun. Efficient enough.",
      "Strafed and deleted.",
    }
  elseif w == "sg" then
    return pick {
      "Up close and personal, " .. v .. ".",
      "You never saw me coming.",
      "Buckshot. The original close-range solution.",
      "Two feet away. Optimal range.",
    }
  elseif w == "gl" then
    return pick {
      "Corner pocket, " .. v .. ".",
      "Watch where you're standing next time.",
      "You walked right into that.",
      "Grenade says hi.",
    }
  elseif w == "rl" then
    return pick {
      "Splash damage: works every time.",
      "Rockets don't miss at this range, " .. v .. ".",
      "You ran right into the blast.",
      "Nothing personal. Just physics.",
    }
  elseif w == "lg" then
    return pick {
      "Held the beam. You didn't move fast enough.",
      "Lightning: point and hold.",
      "Conducted right through you, " .. v .. ".",
      "Arc maintained. Target acquired. Done.",
    }
  elseif w == "pr" then
    return pick {
      "Plasma on target. Consistent.",
      "You can't dodge all of them, " .. v .. ".",
      "Plasma rifle: boring but effective.",
      "Bolts add up fast.",
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

-- Team broadcasts (ctx.team = 1 so these go to team channel)
function chats.team_need_health(_ctx)
  return pick {
    "I'm low. Cover me.",
    "Need health. Watch my back.",
    "Someone drop me health.",
  }
end

function chats.team_need_weapon(_ctx)
  return pick {
    "Need ammo.",
    "Running dry. Anyone?",
    nil,
  }
end

function chats.team_cover_me(_ctx)
  return pick {
    "Cover me!",
    "I'm taking fire. Cover.",
    "Back me up.",
  }
end

function chats.team_follow_me(_ctx)
  return pick {
    "Follow me.",
    "Stay close, I know the path.",
    "With me.",
  }
end

function chats.team_enemy_base_attack(_ctx)
  return pick {
    "Moving on enemy base.",
    "Hitting their base. Support?",
    "In their base. Clear the path.",
  }
end

function chats.team_defending_base(_ctx)
  return pick {
    "Holding our base.",
    "Defense. I've got it covered.",
    "Guarding home.",
  }
end

function chats.team_got_flag_need_support(_ctx)
  return pick {
    "I have the flag! Clear a path!",
    "Flag carrier here. Cover me!",
    "Got it! Escort me home!",
    "Flag secured. Need support!",
  }
end

-- Powerup events
function chats.powerup_quad(_ctx)
  return pick {
    "Quad. Let the carnage begin.",
    "Quad damage acquired. Stand clear.",
    "Quad. This just got unfair.",
    "Armed and extremely dangerous.",
  }
end

function chats.powerup_haste(_ctx)
  return pick {
    "Haste. I was already fast.",
    "Speed boost. Now I'm unreasonable.",
    "Haste. You won't see me coming.",
    nil,
  }
end

function chats.powerup_invis(_ctx)
  return pick {
    "Invisibility. You can't see what you can't see.",
    "Off the grid.",
    "Ghost mode.",
    nil,
  }
end

function chats.powerup_regen(_ctx)
  return pick {
    "Regeneration. I'll be here all night.",
    "Regen. Harder to kill than before.",
    nil,
  }
end

function chats.powerup_battlesuit(_ctx)
  return pick {
    "Battlesuit. Splash won't help you now.",
    "Fully armored.",
    nil,
  }
end

function chats.powerup_enemy_quad(_ctx)
  return pick {
    "Enemy has quad. Watch yourself.",
    "Quad on the enemy. Tactical retreat optional.",
    "They picked up quad. This will hurt.",
    "Clear a path or get flattened.",
  }
end

function chats.powerup_enemy_any(_ctx)
  return pick {
    "Powerup picked up. Stay alert.",
    "They have a powerup. Adjust.",
    nil,
  }
end

-- Kill streaks
function chats.kill_double(_ctx)
  return pick {
    "Double.",
    "Two in a row.",
    "Back to back.",
    nil,
  }
end

function chats.kill_streak_5(ctx)
  local c = ctx.count or 5
  return pick {
    tostring(c) .. " straight.",
    "Five consecutive frags. I'm in the zone.",
    "Momentum.",
    "Five. Don't stop now.",
  }
end

function chats.kill_streak_10(ctx)
  local c = ctx.count or 10
  return pick {
    tostring(c) .. " frags straight.",
    "Ten in a row. This is my arena.",
    "Double digits. No one here can touch me.",
    "Running. Can't. Stop.",
  }
end

function chats.kill_rampage(ctx)
  local c = ctx.count or 15
  return pick {
    tostring(c) .. " and still counting.",
    "RAMPAGE. Visor owns this server.",
    "Is anyone going to stop me? No.",
    "Total domination.",
  }
end

-- Score milestones
function chats.score_first_place(ctx)
  return pick {
    "First place. As expected.",
    "Top of the board. Where else.",
    "Rank one. I live here.",
    "Look at the scoreboard, " .. (ctx.sender ~= "" and ctx.sender or "everyone") .. ".",
  }
end

function chats.score_falling_back(ctx)
  local s = ctx.score or 0
  return pick {
    "Fine. Enjoy it while it lasts.",
    "Slipped. Correcting now.",
    tostring(s) .. " kills and you still got ahead. Impressive.",
    "That won't last.",
    nil,
  }
end

function chats.score_last_place(_ctx)
  return pick {
    "Last. This is temporary.",
    "Bottom of the board. Noted.",
    "I've had worse starts.",
    nil,
  }
end

function chats.score_frag_milestone(ctx)
  local c = ctx.count or ctx.score or 0
  return pick {
    tostring(c) .. " frags.",
    "That's " .. tostring(c) .. ".",
    tostring(c) .. " and counting.",
    nil,
  }
end

-- CTF events
function chats.ctf_got_flag(_ctx)
  return pick {
    "Flag acquired. Moving out.",
    "Got it. Clear a path.",
    "Flag is mine. Heading home.",
    "Objective secured.",
  }
end

function chats.ctf_enemy_got_flag(_ctx)
  return pick {
    "They have our flag. Hunt them down.",
    "Flag's gone. Someone get it back.",
    "Our flag is out. Move.",
    "Intercept the carrier.",
  }
end

function chats.ctf_returning_flag(_ctx)
  return pick {
    "Flag returned. You're welcome.",
    "Recovered and reset.",
    "Flag's back. Let's move.",
    "Kept it from them.",
  }
end

function chats.ctf_capture(_ctx)
  return pick {
    "Capture. Point for us.",
    "That's how it's done.",
    "Flag delivered. Score updated.",
    "Clean run. Next.",
  }
end

function chats.ctf_flag_dropped(_ctx)
  return pick {
    "Flag's loose. Someone grab it.",
    "Dropped. Don't leave it there.",
    "Flag is down. Pick it up.",
    nil,
  }
end

function chats.ctf_attack(_ctx)
  return pick {
    "Moving on their flag.",
    "Going in.",
    "Offensive run. Cover me.",
    nil,
  }
end

function chats.ctf_defend(_ctx)
  return pick {
    "Holding position.",
    "Guarding base.",
    "Defense. I'll hold the line.",
    nil,
  }
end

-- message: reply to incoming chat
function chats.message(ctx)
  local text = string.lower(ctx.text or "")
  if text:match("hi") or text:match("hello") or text:match("hey") or text:match("sup") then
    return pick { ".", "Acknowledged.", nil }
  end
  if text:match("gg") or text:match("good game") then
    return pick { "GG.", "Processed.", nil }
  end
  if text:match("nice") or text:match("good shot") or text:match("well played") or text:match("wp") then
    return pick { "Expected.", "Data confirmed.", "That's how it's done.", nil }
  end
  if text:match("noob") or text:match("suck") or text:match("bad") or text:match("loser") or text:match("trash") then
    return pick { "Incorrect.", "Your data is wrong.", nil }
  end
  if text:match("rail") or text:match("railgun") or text:match("rg") then
    return pick { "Precision tool.", "One shot.", "Clean.", nil }
  end
  if text:match("%?") then
    return pick { "Affirmative.", "Negative.", "Irrelevant.", nil }
  end
  if ctx.team and ctx.team ~= 0 then
    return pick { "Copy.", "Roger.", nil }
  end
  return pick { "...", nil }
end

return chats
