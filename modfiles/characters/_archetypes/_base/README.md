# _base archetype

Universal fallback for all characters. Every sound slot at every tier eventually resolves here if not overridden. If a slot is absent here, the engine plays the default beep and emits a console warning.

## Source

Non-voice neutral effects sourced from `pax01.sw3z` (this project's base asset pack, originally from ioquake3/Q3A).

Voice slots (pain25, pain50, pain75, pain100, death1–3, jump, taunt, falling, fall, gasp, drown, land, talk) are **intentionally absent**. They must be sourced from the original Q3 pak0.pk3 (`sound/player/male1/` voice pool) in a future task. Until then, characters with no archetype-tier or character-tier voice sounds will fall through to the engine default beep.

## Footstep provenance

All footstep files sourced from `sound/player/footsteps/` in pax01.sw3z (Q3 shared footstep pool).

| File | Source pak path |
|------|-----------------|
| footsteps/normal/step1.opus | sound/player/footsteps/step1.opus |
| footsteps/normal/step2.opus | sound/player/footsteps/step2.opus |
| footsteps/normal/step3.opus | sound/player/footsteps/step3.opus |
| footsteps/normal/step4.opus | sound/player/footsteps/step4.opus |
| footsteps/boot/step1.opus | sound/player/footsteps/boot1.opus |
| footsteps/boot/step2.opus | sound/player/footsteps/boot2.opus |
| footsteps/boot/step3.opus | sound/player/footsteps/boot3.opus |
| footsteps/boot/step4.opus | sound/player/footsteps/boot4.opus |
| footsteps/flesh/step1.opus | sound/player/footsteps/flesh1.opus |
| footsteps/flesh/step2.opus | sound/player/footsteps/flesh2.opus |
| footsteps/flesh/step3.opus | sound/player/footsteps/flesh3.opus |
| footsteps/flesh/step4.opus | sound/player/footsteps/flesh4.opus |
| footsteps/mech/step1.opus | sound/player/footsteps/mech1.opus |
| footsteps/mech/step2.opus | sound/player/footsteps/mech2.opus |
| footsteps/mech/step3.opus | sound/player/footsteps/mech3.opus |
| footsteps/mech/step4.opus | sound/player/footsteps/mech4.opus |
| footsteps/energy/step1.opus | sound/player/footsteps/energy1.opus |
| footsteps/energy/step2.opus | sound/player/footsteps/energy2.opus |
| footsteps/energy/step3.opus | sound/player/footsteps/energy3.opus |
| footsteps/energy/step4.opus | sound/player/footsteps/energy4.opus |
| footsteps/metal/step1.opus | sound/player/footsteps/clank1.opus |
| footsteps/metal/step2.opus | sound/player/footsteps/clank2.opus |
| footsteps/metal/step3.opus | sound/player/footsteps/clank3.opus |
| footsteps/metal/step4.opus | sound/player/footsteps/clank4.opus |
| footsteps/splash/step1.opus | sound/player/footsteps/splash1.opus |
| footsteps/splash/step2.opus | sound/player/footsteps/splash2.opus |
| footsteps/splash/step3.opus | sound/player/footsteps/splash3.opus |
| footsteps/splash/step4.opus | sound/player/footsteps/splash4.opus |

## Sound provenance

| File | Source pak path |
|------|-----------------|
| sounds/gib.opus | sound/player/gibimp1.opus |
| sounds/water_in.opus | sound/player/watr_in.opus |
| sounds/water_out.opus | sound/player/watr_out.opus |
| sounds/water_un.opus | sound/player/watr_un.opus |
| sounds/fry.opus | sound/player/fry.opus |

## Missing slots (to be populated from pak0.pk3)

pain25, pain50, pain75, pain100, death1, death2, death3, jump, taunt, falling, fall, gasp, drown, land, talk
