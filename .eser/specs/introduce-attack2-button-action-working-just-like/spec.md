# Spec: introduce-attack2-button-action-working-just-like

## Status: approved

## Concerns: beautiful-product, long-lived, move-fast

## Discovery Answers

### status_quo

No secondary fire exists in q3now. Players have one fire button (+attack = BUTTON_ATTACK, button slot 0). There is no +attack2, no BUTTON_ATTACK2, no EV_FIRE_WEAPON2, no FireWeapon2(). The engine supports 16 generic button slots (in_buttons[0..15]) but only slot 0 is wired to attack.

### ambition

Alt-fire placeholder: Wire +attack2 through the full stack with its own button constant, event type, and dispatcher. 1-star: just the button binding. 5-star: button + event + empty dispatcher. 10-star: full stack with weapon state tracking, animation support, and ammo handling. We aim for 7-star: complete plumbing (button → event → dispatcher) with weapon state but no per-weapon alt-fire implementations yet — those come in a future spec.

### reversibility

Fully reversible. Adding a new button constant, event, and dispatcher is purely additive. No existing behavior changes.

### user_impact

No impact on existing users. +attack2 is unbound by default. Players can optionally bind it (e.g. bind mouse2 +attack2) but it does nothing until per-weapon alt-fire is implemented.

### verification

1) bind mouse2 +attack2 works without crash. 2) Pressing +attack2 triggers EV_FIRE_WEAPON2 event. 3) FireWeapon2() is called on the server. 4) No crash when +attack2 is pressed with any weapon. 5) +attack still works identically. 6) Weapon state tracks attack2 properly (weaponstate, weaponTime). 7) Bot AI is not broken.

### scope_boundary

Out of scope: per-weapon alt-fire implementations, HUD indicators for alt-fire, cgame alt-fire effects/sounds, UI settings for alt-fire binds. This spec ONLY wires the button through the engine stack and creates the empty dispatcher.

## v1 vs v2 Scope (move-fast)

_To be addressed during execution._

## Out of Scope

- Out of scope: per-weapon alt-fire implementations, HUD indicators for alt-fire, cgame alt-fire effects/sounds, UI settings for alt-fire binds
- This spec ONLY wires the button through the engine stack and creates the empty dispatcher.

## Tasks

- [ ] task-1: Add BUTTON_ATTACK2 constant in code/qcommon/q_shared.h — use bit 12 (4096) to avoid conflicts with existing BUTTON_* constants (bits 0-11 used). Also add EV_FIRE_WEAPON2 to the entity_event_t enum in code/game/bg_public.h, right after EV_FIRE_WEAPON.
- [ ] task-2: Register +attack2/-attack2 commands in code/client/cl_input.c — map to in_buttons[12] (matching bit 12). Add IN_Button12Down/IN_Button12Up handler pair if not already generated. Register with Cmd_AddCommand.
- [ ] task-3: Add BUTTON_ATTACK2 handling in code/game/bg_pmove.c PM_Weapon() — mirror the BUTTON_ATTACK check: if BUTTON_ATTACK2 is pressed, go through the same weapon state machine but fire EV_FIRE_WEAPON2 instead of EV_FIRE_WEAPON. Reuse same ammo, weaponTime, and animation logic.
- [ ] task-4: Handle EV_FIRE_WEAPON2 in server game code — In code/game/g_active.c ClientEvents(), add case EV_FIRE_WEAPON2 that calls FireWeapon2(ent). In code/game/g_weapon.c, add FireWeapon2() stub that currently just calls FireWeapon() (same behavior for now). Add prototype to code/game/g_local.h.
- [ ] task-5: Handle EV_FIRE_WEAPON2 in cgame — In code/cgame/cg_event.c, add case for EV_FIRE_WEAPON2 that does the same thing as EV_FIRE_WEAPON (plays fire sound, muzzle flash, etc). Mirror the existing EV_FIRE_WEAPON case.

## Verification

- 1) bind mouse2 +attack2 works without crash. 2) Pressing +attack2 triggers EV_FIRE_WEAPON2 event. 3) FireWeapon2() is called on the server. 4) No crash when +attack2 is pressed with any weapon. 5) +attack still works identically. 6) Weapon state tracks attack2 properly (weaponstate, weaponTime). 7) Bot AI is not broken.
