-- Standard id Tech 3 animation frame table.
-- Frame numbers are ABSOLUTE (as stored in the MD3/animation.cfg).
-- Engine subtracts the LEGS_WALKCR offset when computing leg animation firstFrames.
--
-- Fields: { first, num, looping, fps }
--   first   = first frame (0-based, absolute)
--   num     = total frame count
--   looping = number of frames at the end that loop (0 = play-once)
--   fps     = frames per second
return {
  BOTH_DEATH1       = { first =   0, num = 30, looping =  0, fps = 25 },
  BOTH_DEAD1        = { first =  29, num =  1, looping =  0, fps = 25 },
  BOTH_DEATH2       = { first =  30, num = 30, looping =  0, fps = 25 },
  BOTH_DEAD2        = { first =  59, num =  1, looping =  0, fps = 25 },
  BOTH_DEATH3       = { first =  60, num = 30, looping =  0, fps = 25 },
  BOTH_DEAD3        = { first =  89, num =  1, looping =  0, fps = 25 },

  TORSO_GESTURE     = { first =  90, num = 40, looping =  0, fps = 20 },
  TORSO_ATTACK      = { first = 130, num =  6, looping =  0, fps = 15 },
  TORSO_ATTACK2     = { first = 136, num =  6, looping =  0, fps = 15 },
  TORSO_DROP        = { first = 142, num =  5, looping =  0, fps = 20 },
  TORSO_RAISE       = { first = 147, num =  4, looping =  0, fps = 20 },
  TORSO_STAND       = { first = 151, num =  1, looping =  1, fps = 15 },
  TORSO_STAND2      = { first = 152, num =  1, looping =  1, fps = 15 },

  LEGS_WALKCR       = { first = 153, num =  8, looping =  8, fps = 20 },
  LEGS_WALK         = { first = 161, num = 12, looping = 12, fps = 20 },
  LEGS_RUN          = { first = 173, num =  9, looping =  9, fps = 18 },
  LEGS_BACK         = { first = 182, num = 10, looping = 10, fps = 20 },
  LEGS_SWIM         = { first = 192, num = 10, looping = 10, fps = 15 },
  LEGS_JUMP         = { first = 202, num =  8, looping =  0, fps = 15 },
  LEGS_LAND         = { first = 210, num =  1, looping =  0, fps = 15 },
  LEGS_JUMPB        = { first = 211, num =  8, looping =  0, fps = 15 },
  LEGS_LANDB        = { first = 219, num =  1, looping =  0, fps = 15 },
  LEGS_IDLE         = { first = 220, num = 10, looping = 10, fps = 15 },
  LEGS_IDLECR       = { first = 230, num = 10, looping = 10, fps = 15 },
  LEGS_TURN         = { first = 240, num =  7, looping =  7, fps = 15 },

  -- CTF / team animations
  TORSO_GETFLAG     = { first = 247, num =  6, looping =  0, fps = 15 },
  TORSO_GUARDBASE   = { first = 253, num =  6, looping =  6, fps = 15 },
  TORSO_PATROL      = { first = 259, num = 11, looping = 11, fps = 15 },
  TORSO_FOLLOWME    = { first = 270, num =  5, looping =  0, fps = 15 },
  TORSO_AFFIRMATIVE = { first = 275, num =  6, looping =  0, fps = 15 },
  TORSO_NEGATIVE    = { first = 281, num =  8, looping =  0, fps = 15 },

  -- Extra leg animations (beyond MAX_ANIMATIONS, used internally)
  LEGS_BACKCR       = { first = 153, num =  8, looping =  8, fps = 20 },
  LEGS_BACKWALK     = { first = 161, num = 12, looping = 12, fps = 20 },
  FLAG_RUN          = { first = 289, num =  9, looping =  9, fps = 15 },
  FLAG_STAND        = { first = 298, num = 10, looping = 10, fps = 15 },
  FLAG_STAND2RUN    = { first = 308, num =  9, looping =  0, fps = 15 },
}
