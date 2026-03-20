#ifndef _Q_FEATS_H
#define _Q_FEATS_H
//
// q_feats.h -- q3now feature flags
//
// All modules (game, cgame, ui) include this file.
// Set to 1 to enable, 0 to compile out with zero runtime cost.
//
// Lifecycle:  testing (0) → mature (1)
//             Flip a flag to 1, rebuild, test, commit.
//

// ── mature (stable, shipped) ────────────────────────────────────────────
#define FEAT_ATMOSPHERIC                  1   // 3B  rain & snow particles
#define FEAT_CALLVOTE_MENU                1   // 6C  GUI callvote from ESC menu
#define FEAT_DAMAGE_PLUMS                 1   // 2A  floating damage numbers
#define FEAT_FAST_WEAPON_SWITCH           1   // 5A  fast weapon switch (0=normal, 1=skip drop, 2=instant)
#define FEAT_LENS_FLARES                  1   // 9A  map + missile lens flares (JUHOX)
#define FEAT_MATCH_SUMMARY                1   // 8B  intermission stats overlay
#define FEAT_SPAWN_PROTECTION             1   // 2B  attacker gets no points for spawnkills
#define FEAT_SPECTATOR_OUTLINES           1   // 8A  player outlines for spectators
#define FEAT_TELEPORTING_MISSILES         1   // 2F  rockets/plasma through teleporters
#define FEAT_THIRD_PERSON                 1   // third-person camera with proximity fade & shoulder cam
#define FEAT_CRON_JOBS                    1   // 11  timed server-side tasks
#define FEAT_JSON_STATS                   1   // 7B  post-match JSON export

// ── gameplay (testing) ──────────────────────────────────────────────────
#define FEAT_DESTROYABLE_MISSILES         0   // 11B shoot down rockets/grenades/plasma
#define FEAT_DROP_ITEMS                   0   // 11D /drop command for items
#define FEAT_FREEZETAG                    0   // 7A  freeze on death, thaw by proximity
#define FEAT_GRAPPLE_DAMAGE               0   // 5D  hook deals damage while pulling
#define FEAT_PROJECTILE_BOUNCE            0   // 10H projectile reflection off shields
#define FEAT_UNLAGGED                     0   // 1B  hitscan lag compensation

// ── competitive (testing) ───────────────────────────────────────────────
#define FEAT_1FCTF                        0   // 10E one-flag CTF mode
#define FEAT_AUTO_DEMO                    0   // 10K auto-record demos in tournament
#define FEAT_CLAN_ARENA                   0   // 11  clan arena game mode
#define FEAT_CTF_SCORING                  0   // 10F enhanced CTF flag scoring
#define FEAT_ELIMINATION                  0   // 10B round-based elimination modifier
#define FEAT_ELO_TRACKING                 0   // 10J per-player skill rating
#define FEAT_OVERTIME                     0   // 10D auto-extend on tied score
#define FEAT_RANKED_QUEUE                 0   // 10M matchmaking queue (needs ELO)
#define FEAT_READY_UP                     0   // 4E  /ready warmup system
#define FEAT_RTF                          0   // 11  return the flag mode
#define FEAT_TEAM_AUTOBALANCE             0   // 10A dynamic team balancing
#define FEAT_TOURNAMENT_PAUSE             0   // 10C mid-game pause/timeout

// ── visual / UI (testing) ───────────────────────────────────────────────
#define FEAT_DYNAMIC_MENU                 0   // 6A  callback-driven submenu system
#define FEAT_IMPACT_SPARKS                0   // 11A spark particles on player hit
#define FEAT_MAP_ROTATION                 0   // 6D  server-side map rotation list
#define FEAT_PING_LOCATION                0   // 4G  team coordination pings
#define FEAT_TEAM_LEADERSHIP              0   // 11  particle trail library
#define FEAT_HIT_SOUNDS                   0   // damage-based hit sound pitch variation
#define FEAT_FOLLOW_KILLER                0   // auto-follow killer on death
#define FEAT_CHAT_FILTER                  0   // /ignore and /unignore player commands
#define FEAT_NEW_RAIL_STYLE               0   // alternate rail trail rendering

#endif // _Q_FEATS_H
