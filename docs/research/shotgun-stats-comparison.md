# Shotgun (SG) Stats Comparison Across Quake Games

Research compiled from id Software source code (GitHub) and community wikis.

## Quick Comparison Table

| Parameter              | Quake 1 (QW)      | Quake 2            | Q3A (baseq3)       | CPMA               | Quake Live (final) | q3now (current)    |
|------------------------|--------------------|--------------------|---------------------|---------------------|--------------------|--------------------|
| Pellets per shot       | 6                  | 12                 | 11                  | 11 (unchanged)      | 20                 | 16                 |
| Damage per pellet      | 4                  | 4                  | 10                  | 10 (unchanged)      | 5                  | 8                  |
| Max damage (all hit)   | 24                 | 48                 | 110                 | 110 (unchanged)     | 100                | 128                |
| Fire interval          | 500ms              | ~1100ms (frames)   | 1000ms              | 1000ms              | 1000ms             | 1000ms (presumed)  |
| Shots per second       | 2.0                | ~0.9               | 1.0                 | 1.0                 | 1.0                | 1.0                |
| DPS (theoretical max)  | 48                 | ~43                | 110                 | 110                 | 100                | 128                |
| Spread type            | Random             | Random             | Seeded pseudo-random| Seeded pseudo-random| Seeded pseudo-random| Seeded pseudo-random|
| Ammo per shot          | 1 shell            | 1 shell            | 1 shell             | 1 shell             | 1 shell            | 1 shell            |
| Hitscan                | Yes                | Yes                | Yes                 | Yes                 | Yes                | Yes                |
| Damage falloff         | No                 | No (spread only)   | No                  | No                  | No                 | Yes (custom)       |
| Max range              | 2048 units         | Infinite (hitscan) | 8192*16 trace       | 8192*16 trace       | 8192*16 trace      | 8192*16 trace      |

## Detailed Breakdown

### Quake 1 / QuakeWorld

**Source**: `QW/progs/weapons.qc` (id-Software/Quake on GitHub), QWiki

```quakec
// W_FireShotgun:
self.attack_finished = time + 0.5;   // 500ms between shots
dir = aim (self, 100000);
FireBullets (6, dir, '0.04 0.04 0'); // 6 pellets, spread 0.04 H, 0.04 V
```

- **Pellets**: 6
- **Damage per pellet**: 4 (set inside FireBullets via TraceAttack)
- **Max damage**: 24
- **Fire rate**: 500ms (2 shots/sec)
- **Spread**: `'0.04 0.04 0'` -- 0.04 horizontal, 0.04 vertical (in directional multiplier units, ~2.3 degrees)
- **Spread type**: Random (`crandom()` per pellet per axis)
- **Knockback**: Proportional to damage (standard Q1 knockback formula)
- **Damage falloff**: None (hitscan with max range 2048 units)
- **Ammo**: 1 shell per shot
- **Notes**: Very weak weapon, mainly used as starter/fallback. Super Shotgun fires 14 pellets with spread `'0.14 0.08 0'` for comparison.

### Quake 2

**Source**: `game/p_weapon.c` and `game/g_local.h` (id-Software/Quake-2 on GitHub), Quake Wiki (Fandom)

```c
// g_local.h defines:
#define DEFAULT_SHOTGUN_HSPREAD     1000
#define DEFAULT_SHOTGUN_VSPREAD     500
#define DEFAULT_DEATHMATCH_SHOTGUN_COUNT  12
#define DEFAULT_SHOTGUN_COUNT       12
#define DEFAULT_SSHOTGUN_COUNT      20

// weapon_shotgun_fire in p_weapon.c:
int damage = 4;
int kick = 8;
// ...
if (deathmatch->value)
    fire_shotgun(ent, start, forward, damage, kick, 500, 500,
                 DEFAULT_DEATHMATCH_SHOTGUN_COUNT, MOD_SHOTGUN);
else
    fire_shotgun(ent, start, forward, damage, kick, 500, 500,
                 DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN);
```

**IMPORTANT NOTE**: The actual `fire_shotgun` call passes `hspread=500, vspread=500` for BOTH SP and DM, NOT the 1000/500 from the defines. The defines are only used by the Super Shotgun. The shotgun hardcodes 500/500 spread in the weapon fire function.

- **Pellets**: 12 (both SP and DM -- `DEFAULT_DEATHMATCH_SHOTGUN_COUNT` == `DEFAULT_SHOTGUN_COUNT` == 12)
- **Damage per pellet**: 4
- **Max damage**: 48
- **Fire rate**: ~1100ms (frame-based animation, gunframe 9 is the fire frame, ~10 frames at 10fps server tick)
- **Spread**: hspread=500, vspread=500 (in Q2 spread units -- applied as `crandom() * spread * 0.001` to direction)
- **Spread type**: Random (`crandom()` per pellet per axis)
- **Kick/Knockback**: kick=8 (viewkick to attacker: `kick_angles[0] = -2`, `kick_origin forward = -2`)
- **Damage falloff**: None (damage doesn't decrease with distance, but spread naturally reduces hits)
- **Ammo**: 1 shell per shot
- **Notes**: Significantly stronger than Q1 SG due to double the pellets. Wiki confirms 12 pellets. Frame-based fire rate means exact timing depends on server framerate.

### Quake III Arena (baseq3)

**Source**: `code/game/bg_public.h`, `code/game/g_weapon.c`, `code/game/bg_pmove.c` (id-Software/Quake-III-Arena on GitHub)

```c
// bg_public.h:
#define DEFAULT_SHOTGUN_SPREAD  700
#define DEFAULT_SHOTGUN_COUNT   11

// g_weapon.c:
#define DEFAULT_SHOTGUN_DAMAGE  10

// ShotgunPattern():
for (i = 0; i < DEFAULT_SHOTGUN_COUNT; i++) {
    r = Q_crandom(&seed) * DEFAULT_SHOTGUN_SPREAD * 16;
    u = Q_crandom(&seed) * DEFAULT_SHOTGUN_SPREAD * 16;
    VectorMA(origin, 8192 * 16, forward, end);
    VectorMA(end, r, right, end);
    VectorMA(end, u, up, end);
    // ...
}

// bg_pmove.c (PM_Weapon):
case WP_SHOTGUN:
    addTime = 1000;  // 1000ms between shots
    break;
```

- **Pellets**: 11
- **Damage per pellet**: 10
- **Max damage**: 110 (can one-shot an unarmored 100hp target)
- **Fire rate**: 1000ms (1 shot/sec)
- **Spread**: 700 (in Q3 spread units, applied as `Q_crandom(&seed) * 700 * 16` offset at distance `8192 * 16`)
  - Effective cone angle: ~4.9 degrees half-angle (`atan(700 / 8192)`)
  - At 500 units: ~42.7 unit radius spread
  - At 1000 units: ~85.4 unit radius spread
- **Spread type**: Seeded pseudo-random (`Q_crandom(&seed)`) -- the seed is synced between client and server so the pattern is deterministic per shot but appears random. Each pellet gets independent H and V offsets.
- **Knockback**: Standard Q3 knockback (damage-based, `damage * knockback_scale` applied as velocity impulse)
- **Damage falloff**: None
- **Team damage reduction**: Uses `DEFAULT_SHOTGUN_DAMAGE` directly (no team modifier on pellets, unlike MG which has `MACHINEGUN_TEAM_DAMAGE`)
- **Ammo**: 1 shell per shot, pickup gives 10 shells
- **Notes**: The 110 max damage makes it capable of one-shot kills on unarmored targets at point-blank. The seeded random spread means both client and server agree on pellet positions (important for client-side prediction of impact marks).

### CPMA (Challenge ProMode Arena)

**Source**: Community documentation, CPMA changelogs

CPMA does **NOT** modify the shotgun from baseq3. CPMA's weapon changes focus on:
- Rocket Launcher (reduced splash, knockback changes)
- Lightning Gun (damage changes in various versions)
- Railgun (fire rate changes in some configs)
- Plasma Gun (changes)
- Air control and movement physics

The shotgun retains all baseq3 values:
- **Pellets**: 11
- **Damage per pellet**: 10
- **Max damage**: 110
- **Fire rate**: 1000ms
- **Spread**: 700
- **All other parameters**: Identical to baseq3

**Notes**: In CPMA's VQ3 mode, the shotgun is identical to baseq3. In CPM mode, the shotgun is also unchanged -- CPMA considered the SG balanced as-is. The movement physics changes (air control, etc.) indirectly affect SG gameplay by making close-range fights more dynamic.

### Quake Live

**Source**: Quake Wiki (Fandom), community documentation, ESReality

Quake Live started as a direct port of Q3A but went through multiple weapon balance passes:

**Early QL (2009-2010)** -- Identical to Q3A:
- 11 pellets, 10 damage each, 110 max

**Post-balance patches (2010+)** -- Shotgun was nerfed:
- **Pellets**: Increased to 20
- **Damage per pellet**: Reduced to 5
- **Max damage**: 100 (down from 110)
- **Fire rate**: 1000ms (unchanged)
- **Spread**: Wider than Q3A (exact value varied by patch)
- **Spread type**: Seeded pseudo-random (same system as Q3A)

**Rationale**: The change from 11x10 to 20x5 was designed to:
1. Reduce the variance of shotgun damage (more pellets = more consistent hit counts)
2. Lower the one-shot-kill potential (100 < 125 starting health in QL)
3. Make the weapon more of a "chip damage" tool rather than a burst weapon
4. The wider spread with more pellets gives more forgiving aim but less burst

- **Knockback**: Standard (damage-proportional)
- **Damage falloff**: None (same as Q3A)
- **Ammo**: 1 shell per shot

**Notes**: The QL shotgun changes were controversial. The 20x5 configuration means you need ALL pellets to hit for 100 damage (vs Q3A where 11x10=110 could kill). In practice, QL shotgun deals ~40-60 damage at typical combat range vs Q3A's ~50-80, making it less threatening.

### q3now (Current Codebase)

**Source**: `code/game/bg_public.h` and `code/game/g_weapon.c` in this repository

```c
// bg_public.h:
#define DEFAULT_SHOTGUN_SPREAD  600    // (Q3A: 700)
#define DEFAULT_SHOTGUN_COUNT   16     // (Q3A: 11)

// g_weapon.c:
#define DEFAULT_SHOTGUN_DAMAGE  8      // (Q3A: 10)
```

- **Pellets**: 16 (Q3A: 11, QL: 20)
- **Damage per pellet**: 8 (Q3A: 10, QL: 5)
- **Max damage**: 128 (Q3A: 110, QL: 100)
- **Fire rate**: 1000ms (presumed, same as Q3A)
- **Spread**: 600 (Q3A: 700) -- tighter spread
  - Effective cone angle: ~4.2 degrees half-angle (vs Q3A's ~4.9)
- **Damage falloff**: Yes (custom `G_DamageFalloff` function -- full damage within 256 units, linear falloff to `g_damageFalloff` cvar distance, minimum 10% damage)
- **Notes**: q3now takes a middle ground between Q3A and QL: more pellets than Q3A but fewer than QL, damage per pellet between Q3A and QL, tighter spread than both, and uniquely adds damage falloff.

## Design Analysis

| Aspect                    | Q1       | Q2       | Q3A      | QL       | q3now    |
|---------------------------|----------|----------|----------|----------|----------|
| Pellet count trend        | 6        | 12       | 11       | 20       | 16       |
| Damage per pellet trend   | 4        | 4        | 10       | 5        | 8        |
| Max damage trend          | 24       | 48       | 110      | 100      | 128      |
| Fire rate trend           | 500ms    | ~1100ms  | 1000ms   | 1000ms   | 1000ms   |
| Spread trend (tighter=<)  | Tight    | Medium   | Medium   | Wide     | Tight    |
| One-shot potential        | No       | No       | Yes      | No       | Yes      |
| Consistency (more pellets=more consistent) | Low | Medium | Medium | High | High-Med |

### Key Observations:
1. **Q1 SG** was a weak starter weapon, never meant for serious combat
2. **Q2 SG** doubled pellet count but kept 4 dmg/pellet -- still a weak early weapon
3. **Q3A SG** made a dramatic shift: fewer pellets but 10 dmg each -- suddenly a lethal close-range weapon capable of one-shots
4. **QL** backed off from Q3A's burst potential, trading fewer/stronger pellets for more/weaker ones to reduce variance
5. **q3now** splits the difference: 16 pellets at 8 dmg gives high max damage (128) with moderate consistency, plus unique damage falloff mechanic
