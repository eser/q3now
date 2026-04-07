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
#define FEAT_BOT_IMPROVEMENTS             1   // advanced bot AI: dodge, awareness, strafejump, item timing, weapon selection
#define FEAT_CRON_JOBS                    1   // 11  timed server-side tasks
#define FEAT_DAMAGE_PLUMS                 1   // 2A  floating damage numbers
#define FEAT_FAST_WEAPON_SWITCH           1   // 5A  fast weapon switch (0=normal, 1=skip drop, 2=instant)
#define FEAT_FS_PRECEDENCE                1   // archive dedup: basename desc, dir > ext precedence
#define FEAT_IPV6                         1   // IPv6 support
#define FEAT_JSON_STATS                   1   // 7B  post-match JSON export
#define FEAT_LENS_FLARES                  1   // 9A  map + missile lens flares (JUHOX)
#define FEAT_MATCH_SUMMARY                1   // 8B  intermission stats overlay
#define FEAT_SPAWN_PROTECTION             1   // 2B  attacker gets no points for spawnkills
#define FEAT_SPECTATOR_OUTLINES           1   // 8A  player outlines for spectators
#define FEAT_SW3Z                         1   // SW3Z archive format (.sw3z)
#define FEAT_THIRD_PERSON                 1   // third-person camera with proximity fade & shoulder cam
#define FEAT_UNLAGGED                     1   // 1B  server-side lag compensation (hitscan + projectile nudge)
#define FEAT_ZNUDGE                       1   // 1C  client-side forward extrapolation (player/missile prediction)

// ── gameplay (testing) ──────────────────────────────────────────────────
#define FEAT_DESTROYABLE_MISSILES         0   // 11B shoot down rockets/grenades/plasma
#define FEAT_DROP_ITEMS                   0   // 11D /drop command for items
#define FEAT_FREEZETAG                    0   // 7A  freeze on death, thaw by proximity
#define FEAT_GRAPPLE_DAMAGE               0   // 5D  hook deals damage while pulling
#define FEAT_PROJECTILE_BOUNCE            0   // 10H projectile reflection off shields
#define FEAT_SHOTGUN_PATTERN              1   // fixed pellet ring pattern (replaces random spread)
#define FEAT_SHOTGUN_PUMP                 1   // Doom-style pump animation after firing
#define FEAT_TELEPORTING_MISSILES         0   // 2F  rockets/plasma through teleporters

// ── bug fixes (CPMA-sourced id bug fixes) ────────────────────────────────
#define FEAT_ITEM_BOB_FIX                 0   // fix: item bobbing jitter after ~2 days of server uptime
#define FEAT_GRENADE_REST_FIX             0   // fix: grenades snap to axis-aligned angles when resting
#define FEAT_RAIL_BROADCAST               0   // fix: rail trails not visible to players near shooter
#define FEAT_CORPSE_MOVER_FIX             0   // fix: corpses block movers (doors, lifts, plats)
#define FEAT_TELEFRAG_FIX                 0   // fix: dead players trigger false telefrag on spawn points
#define FEAT_BOUNCE_SOUND_LIMIT           0   // fix: unlimited grenade bounce sounds on movers

// ── competitive (testing) ───────────────────────────────────────────────
#define FEAT_AUTO_DEMO                    1   // 10K auto-record demos in tournament
#define FEAT_CLAN_ARENA                   0   // 11  clan arena game mode
#define FEAT_ELIMINATION                  0   // 10B round-based elimination modifier
#define FEAT_ELO_TRACKING                 0   // 10J per-player skill rating
#define FEAT_RANKED_QUEUE                 0   // 10M matchmaking queue (needs ELO)
#define FEAT_READY_UP                     0   // 4E  /ready warmup system
#define FEAT_RTF                          0   // 11  return the flag mode
#define FEAT_TEAM_AUTOBALANCE             0   // 10A dynamic team balancing
#define FEAT_TOURNAMENT_PAUSE             0   // 10C mid-game pause/timeout

// ── visual / UI (testing) ───────────────────────────────────────────────
#define FEAT_ENV_LIGHTS                   0   // colored dlights from lava/slime/water surfaces (KEX-style)
#define FEAT_IMPACT_SPARKS                0   // 11A spark particles on player hit
#define FEAT_MAP_ROTATION                 1   // 6D  server-side map rotation list
#define FEAT_STATS_WINDOW                 1   // floating stats overlay + window system
#define FEAT_PING_LOCATION                0   // 4G  team coordination pings
#define FEAT_TEAM_LEADERSHIP              0   // 11  particle trail library
#define FEAT_FOLLOW_KILLER                0   // auto-follow killer on death
#define FEAT_CHAT_FILTER                  0   // /ignore and /unignore player commands
#define FEAT_RAIL_TRAIL                   0   // 0 = default, 1 = old, 2 = wicked
#define FEAT_MOVEMENT_KEYS                0   // show followed player's movement keys (spectator HUD)
#define FEAT_WIRED_UI                     1   // Wired UI: unified .menu/.hud/.gui system (replaces q3_ui + SuperHUD)
#define FEAT_LEGACY_UI                    0   // legacy TA menu/HUD code paths (compile-time hard cut)

// ── engine internals (testing) ────────────────────────────────────────
#define FEAT_BSP_ABSTRACTION              1   // Pluggable BSP format loaders
#define FEAT_LEGACY_FORMATS_AUDIO         1   // WAV, OGG Vorbis, ADPCM audio codecs (retire by setting to 0; Opus remains)
#define FEAT_LEGACY_FORMATS_IMAGE         1   // BMP, PCX, TGA image loaders (retire by setting to 0; PNG+JPG remain)
#define FEAT_WASM                         1   // WASM VM backend via WAMR (replaces QVM over time)
#define FEAT_LEGACY_QVM                   0   // QVM bytecode interpreter + JIT (retire by setting to 0)

// ── model formats ────────────────────────────────────────────────────
#define FEAT_IQM                          1   // IQM (Inter-Quake Model) skeletal mesh format

// ── renderer (from CNQ3) ───────────────────────────────────────────────
#define FEAT_FOG_SYSTEM                   0   // Enhanced fog types (linear, exp, exp2)
#define FEAT_CORONA                       0   // Corona/lens flare entities via flare pipeline
#define FEAT_HEADLESS_RENDERER            0   // Dedicated server renderer stub (sv_ref.c)
#define FEAT_DEPTH_CLAMP                  0   // disable near-plane vertex clipping at high FOV
#define FEAT_DEPTH_FADE                   0   // soft particle edges (explosions, smoke, blood)
#define FEAT_PARALLAX_MAPPING             0   // steep parallax mapping with normalmap (height in alpha)
#define FEAT_SSAO                         1   // screen-space ambient occlusion (embedded in gamma pass)
#define FEAT_TONEMAP                      1   // HDR tone mapping (Reinhard/ACES/Uncharted2)
#define FEAT_COLOR_GRADING                1   // color tint, saturation, contrast
#define FEAT_FXAA                         1   // fast approximate anti-aliasing (embedded in gamma pass)
#define FEAT_GODRAYS                      1   // screen-space crepuscular rays (depth-based sky detection)
#define FEAT_ADVANCED_WATER               1   // screen-space refraction + Fresnel + ripple noise for water
#define FEAT_SHADOW_MAPPING               1   // per-light shadow maps with PCF (1/5/9 samples)
#define FEAT_PBR                          1   // physically based rendering (GGX/Schlick/Smith BRDF)
#define FEAT_SMAA                         0   // sub-pixel morphological anti-aliasing (deferred)
#define FEAT_FORCE_ENTITY_VERTEX_ALPHA    1   // per-entity alpha override + dynamic pipeline swap
#define FEAT_FBO_DEBUG                    0   // verbose FBO pipeline diagnostics (format, layout, passes)

// ── networking / transport (testing) ─────────────────────────────
// Layered flags — each layer depends on the one above it.
// FEAT_QUIC_TRANSPORT is the foundation; higher layers require it.
//
//   TRANSPORT ──► OBSERVE ──► CONTROL
//       │
//       └──────► HTTP
//
#ifndef FEAT_QUIC_TRANSPORT
#define FEAT_QUIC_TRANSPORT               0   // picoquic integration, demux, handshake, QUIC datagrams
#endif
#ifndef FEAT_QUIC_OBSERVE
#define FEAT_QUIC_OBSERVE                 0   // structured game state datagrams + event stream (requires TRANSPORT)
#endif
#ifndef FEAT_QUIC_CONTROL
#define FEAT_QUIC_CONTROL                 0   // command channel + MCP server (requires OBSERVE)
#endif
#ifndef FEAT_QUIC_HTTP
#define FEAT_QUIC_HTTP                    0   // HTTP endpoints: /health, /metrics (requires TRANSPORT)
#endif

// dependency enforcement — higher layers silently enable their prerequisites
#if FEAT_QUIC_OBSERVE && !FEAT_QUIC_TRANSPORT
#undef  FEAT_QUIC_TRANSPORT
#define FEAT_QUIC_TRANSPORT               1
#endif
#if FEAT_QUIC_CONTROL && !FEAT_QUIC_OBSERVE
#undef  FEAT_QUIC_OBSERVE
#define FEAT_QUIC_OBSERVE                 1
#endif
#if FEAT_QUIC_CONTROL && !FEAT_QUIC_TRANSPORT
#undef  FEAT_QUIC_TRANSPORT
#define FEAT_QUIC_TRANSPORT               1
#endif
#if FEAT_QUIC_HTTP && !FEAT_QUIC_TRANSPORT
#undef  FEAT_QUIC_TRANSPORT
#define FEAT_QUIC_TRANSPORT               1
#endif

// ── missionpack (Team Arena features, individually toggleable) ────────
#ifdef MISSIONPACK
// backward compat: MISSIONPACK enables everything
#define FEAT_TA_UI                        1   // Team Arena UI framework, HUD, menus
#define FEAT_TA_VOICECHAT                 1   // vsay/vtell voice commands (bots use when enabled)
#define FEAT_TA_TEAM_OVERLAYS             1   // team status overlay HUD (health/armor/weapon of teammates)
#define FEAT_TA_TEAM_ORDERS               1   // team orders & squad commands (order teammates, accept/deny)
#define FEAT_HARVESTER                    1   // Harvester game mode (skull/cube collection)
#define FEAT_OVERLOAD                     1   // Overload game mode (obelisk control)
#define FEAT_PW_PORTAL                    1   // Invulnerability shield (sphere, railgun bounce)
#else
#define FEAT_TA_UI                        0
#define FEAT_TA_VOICECHAT                 0
#define FEAT_TA_TEAM_OVERLAYS             0
#define FEAT_TA_TEAM_ORDERS               0
#define FEAT_HARVESTER                    0
#define FEAT_OVERLOAD                     0
#define FEAT_PW_PORTAL                    0
#endif

#endif // _Q_FEATS_H
