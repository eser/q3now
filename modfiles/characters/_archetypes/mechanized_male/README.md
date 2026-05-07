# mechanized_male archetype

Archetype defaults for mechanized/cybernetic male characters. Currently used by: Visor, Keel.

Overrides `_base` for slots where a mechanical/robotic voice is appropriate.

## Source

All voice sounds sourced from `characters/visor/sounds/` in `pax01.sw3z` (Visor's in-game voice set from Q3A).

## Sound provenance

| File | Source pak path | Notes |
|------|-----------------|-------|
| sounds/death1.opus | characters/visor/sounds/death1.opus | |
| sounds/death2.opus | characters/visor/sounds/death2.opus | |
| sounds/death3.opus | characters/visor/sounds/death3.opus | |
| sounds/pain25.opus | characters/visor/sounds/pain25_1.opus | renamed: Q3 has _1 suffix |
| sounds/pain50.opus | characters/visor/sounds/pain50_1.opus | renamed |
| sounds/pain75.opus | characters/visor/sounds/pain75_1.opus | renamed |
| sounds/pain100.opus | characters/visor/sounds/pain100_1.opus | renamed |

## Footstep provenance

| File | Source pak path |
|------|-----------------|
| footsteps/mech/step1.opus | sound/player/footsteps/mech1.opus |
| footsteps/mech/step2.opus | sound/player/footsteps/mech2.opus |
| footsteps/mech/step3.opus | sound/player/footsteps/mech3.opus |
| footsteps/mech/step4.opus | sound/player/footsteps/mech4.opus |

## Missing slots

jump, taunt, falling, fall, gasp, drown — these fall through to `_base`. Since `_base` voice slots are also absent pending pak0.pk3 extraction, these will beep+warn until populated. Keel may override at character tier independently.
