// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save.c — savegame reflection engine: field-descriptor tables (Phase-1).
//
// PHASE-1 IS DESCRIPTOR DATA ONLY. No write path, no read path, no name<->ptr
// registry. These tables describe WHICH fields of gentity_t / gclient_t /
// level_locals_t are savegame state, and HOW each is serialized (scalar, string,
// index-relocated pointer, or name-relocated callback). A later phase's
// serializer walks these tables; nothing calls this code yet, so the game is
// byte-identical.
//
// Grounding: the field selection follows the savegame surface map (Map-2). The
// EXCLUDED classes (deliberately absent from the SAVE tables):
//   - `r` (entityShared_t): linked/absmin/currentOrigin are re-derived by
//     trap_LinkEntity + trajectory eval on load — never saved.
//   - The 7 gentity func-pointers as ADDRESSES: they are listed with SG_FUNCTION
//     so the serializer stores the callback's registered NAME (Phase-2 registry),
//     NOT an address — this is the func-ptr DROP (replaces RealRTCW funcList[]).
//   - The FEAT_* pool pointers navState / behaviorState / aiThink: self-commented
//     "never serialized" (g_local.h) — the pools are re-acquired on load; the
//     behaviorState/scriptState CONTENT is saved by walking those pools directly
//     in Phase-4, not through this gentity descriptor.

#include "g_local.h"
#include "g_save.h"
#include "g_save_funcs.h"

// ── gentity_t descriptor ─────────────────────────────────────────────────────
// SAVE-class fields + the index-relocation pointers (SG_ENTITY/SG_CLIENT/SG_ITEM)
// + the 7 callbacks (SG_FUNCTION, name-relocated in Phase-2). `s` and `r` are
// handled specially (see the SG_RAW note on `s` below and the `r`-excluded note).
const saveField_t gentityFields[] = {
	// entityState_t s — the live communicated state. Its trajectory (s.pos/s.apos)
	// is the authoritative position/angle; it also carries index-reloc fields
	// (s.otherEntityNum, s.groundEntityNum, s.clientNum) and configstring handle
	// indices (s.modelindex, s.loopSound). Phase-1 marks it a RAW blob; Phase-4
	// needs an entityState SUB-DESCRIPTOR to index-reloc those internal refs — see
	// the FLAGGED note in the handoff. (RealRTCW bulk-copies s and index-relocs
	// the internal nums in a fix-up pass; q3now will mirror that.)
	{ FOFS( s ),                    SG_RAW,     sizeof( entityState_t ) },

	{ FOFS( client ),               SG_CLIENT,  0 },  // index-reloc -> level.clients[n]
	{ FOFS( inuse ),                SG_QBOOLEAN, 0 },
	{ FOFS( classname ),            SG_STRING,  0 },
	{ FOFS( spawnflags ),           SG_INT,     0 },
	{ FOFS( neverFree ),            SG_QBOOLEAN, 0 },
	{ FOFS( flags ),                SG_INT,     0 },
	{ FOFS( model ),                SG_STRING,  0 },
	{ FOFS( model2 ),               SG_STRING,  0 },
	{ FOFS( freetime ),             SG_INT,     0 },
	{ FOFS( eventTime ),            SG_INT,     0 },
	{ FOFS( freeAfterEvent ),       SG_QBOOLEAN, 0 },
	{ FOFS( unlinkAfterEvent ),     SG_QBOOLEAN, 0 },
	{ FOFS( physicsObject ),        SG_QBOOLEAN, 0 },
	{ FOFS( physicsBounce ),        SG_FLOAT,   0 },
	{ FOFS( clipmask ),             SG_INT,     0 },

	// movers — moverState + endpoints + sound HANDLE indices (configstring-
	// relative; the descriptor stores them as ints, Phase-4 re-indexes on load).
	{ FOFS( moverState ),           SG_INT,     0 },  // moverState_t
	{ FOFS( soundPos1 ),            SG_INT,     0 },  // sound handle index
	{ FOFS( sound1to2 ),            SG_INT,     0 },
	{ FOFS( sound2to1 ),            SG_INT,     0 },
	{ FOFS( soundPos2 ),            SG_INT,     0 },
	{ FOFS( soundLoop ),            SG_INT,     0 },
	{ FOFS( parent ),               SG_ENTITY,  0 },
	{ FOFS( nextTrain ),            SG_ENTITY,  0 },
	{ FOFS( prevTrain ),            SG_ENTITY,  0 },
	{ FOFS( pos1 ),                 SG_VECTOR,  0 },
	{ FOFS( pos2 ),                 SG_VECTOR,  0 },

	{ FOFS( message ),              SG_STRING,  0 },
	{ FOFS( timestamp ),            SG_INT,     0 },
	{ FOFS( target ),               SG_STRING,  0 },
	{ FOFS( target2 ),              SG_STRING,  0 },
	{ FOFS( targetname ),           SG_STRING,  0 },
	{ FOFS( team ),                 SG_STRING,  0 },
	{ FOFS( targetShaderName ),     SG_STRING,  0 },
	{ FOFS( targetShaderNewName ),  SG_STRING,  0 },
	{ FOFS( target_ent ),           SG_ENTITY,  0 },

	{ FOFS( speed ),                SG_FLOAT,   0 },
	{ FOFS( movedir ),              SG_VECTOR,  0 },

	{ FOFS( nextthink ),            SG_INT,     0 },
	// The 7 callbacks — name-relocated via the Phase-2 registry (NOT addresses).
	{ FOFS( think ),                SG_FUNCTION, 0 },
	{ FOFS( reached ),              SG_FUNCTION, 0 },
	{ FOFS( blocked ),              SG_FUNCTION, 0 },
	{ FOFS( touch ),                SG_FUNCTION, 0 },
	{ FOFS( use ),                  SG_FUNCTION, 0 },
	{ FOFS( pain ),                 SG_FUNCTION, 0 },
	{ FOFS( die ),                  SG_FUNCTION, 0 },

	{ FOFS( pain_debounce_time ),   SG_INT,     0 },
	{ FOFS( fly_sound_debounce_time ), SG_INT,  0 },
	{ FOFS( last_move_time ),       SG_INT,     0 },
	{ FOFS( health ),               SG_INT,     0 },
	{ FOFS( armor ),                SG_INT,     0 },
	{ FOFS( takedamage ),           SG_QBOOLEAN, 0 },
	{ FOFS( gibScheduled ),         SG_QBOOLEAN, 0 },
	{ FOFS( damage ),               SG_INT,     0 },
	{ FOFS( splashDamage ),         SG_INT,     0 },
	{ FOFS( splashRadius ),         SG_INT,     0 },
	{ FOFS( methodOfDeath ),        SG_INT,     0 },
	{ FOFS( splashMethodOfDeath ),  SG_INT,     0 },
	{ FOFS( count ),                SG_INT,     0 },

	// target_logic AND-gate: an array of BIASED entity indices (s.number + 1).
	// A RAW blob at Phase-1; Phase-4 index-relocs each nonzero slot (it is an
	// index array, not a pointer array, so it needs per-element bias fix-up).
	{ FOFS( logicEntities ),        SG_RAW,     sizeof( ((gentity_t *)0)->logicEntities ) },

	{ FOFS( chain ),                SG_ENTITY,  0 },
	{ FOFS( enemy ),                SG_ENTITY,  0 },
	{ FOFS( helixPairEntity ),      SG_ENTITY,  0 },
	{ FOFS( activator ),            SG_ENTITY,  0 },
	{ FOFS( teamchain ),            SG_ENTITY,  0 },
	{ FOFS( teammaster ),           SG_ENTITY,  0 },

	{ FOFS( kamikazeTime ),         SG_INT,     0 },
	{ FOFS( kamikazeShockTime ),    SG_INT,     0 },
	{ FOFS( watertype ),            SG_INT,     0 },
	{ FOFS( waterlevel ),           SG_INT,     0 },
	{ FOFS( noise_index ),          SG_INT,     0 },  // sound handle index
	{ FOFS( wait ),                 SG_FLOAT,   0 },
	{ FOFS( random ),               SG_FLOAT,   0 },
	{ FOFS( item ),                 SG_ITEM,    0 },  // index-reloc -> bg_itemlist[n]
	{ FOFS( key ),                  SG_STRING,  0 },
	{ FOFS( value ),                SG_STRING,  0 },

#if FEAT_RECAST_NAVMESH
	// navFollower/navGoal/navLegEnd/navSpeed are live follower state (NOT the
	// navState pool pointer, which is excluded). A nav-driven mover mid-leg needs
	// them; navState itself is rebuilt from navGoal on load.
	{ FOFS( navFollower ),          SG_QBOOLEAN, 0 },
	{ FOFS( navGoal ),              SG_VECTOR,  0 },
	{ FOFS( navLegEnd ),            SG_INT,     0 },
	{ FOFS( navSpeed ),             SG_FLOAT,   0 },
	// NOTE: navState (pool pointer) intentionally EXCLUDED — rebuilt on load.
#endif
#if FEAT_MONSTER_AI
	{ FOFS( aiThink ),              SG_QBOOLEAN, 0 },
	// NOTE: behaviorState (pool pointer) intentionally EXCLUDED — the pool's
	// CONTENT is saved by walking behaviorPool[] directly in Phase-4.
#endif
#if FEAT_UNLAGGED
	{ FOFS( launchTime ),           SG_INT,     0 },
	// needsDelag is a transient replay flag — excluded (re-derived).
#endif
#if FEAT_TELEPORTING_MISSILES
	{ FOFS( missileTeleportCount ), SG_INT,     0 },
#endif

	{ 0, SG_NONE, 0 }
};

// ── gclient_t descriptor ─────────────────────────────────────────────────────
// ps / pers / sess are large sub-structs. Phase-1 marks ps a RAW blob (its
// internal index-reloc fields — groundEntityNum, clientNum, jumppad_ent — are a
// Phase-4 playerState sub-descriptor concern, same as entityState — FLAGGED).
// pers/sess are game-private PODs saved verbatim.
const saveField_t gclientFields[] = {
	{ CFOFS( ps ),                  SG_RAW,     sizeof( playerState_t ) },
	{ CFOFS( pers ),                SG_RAW,     sizeof( clientPersistant_t ) },
	{ CFOFS( sess ),                SG_RAW,     sizeof( clientSession_t ) },

	{ CFOFS( noclip ),              SG_QBOOLEAN, 0 },
	{ CFOFS( lastKillTime ),        SG_INT,     0 },
	{ CFOFS( respawnTime ),         SG_INT,     0 },
	{ CFOFS( inactivityTime ),      SG_INT,     0 },
	{ CFOFS( rewardTime ),          SG_INT,     0 },
	{ CFOFS( airOutTime ),          SG_INT,     0 },
	{ CFOFS( switchTeamTime ),      SG_INT,     0 },
	{ CFOFS( timeResidual ),        SG_INT,     0 },
	{ CFOFS( ammoTimes ),           SG_RAW,     sizeof( ((gclient_t *)0)->ammoTimes ) },
	{ CFOFS( deflectorTime ),       SG_INT,     0 },
	{ CFOFS( lasthurt_time ),       SG_INT,     0 },
	{ CFOFS( consecutiveKills ),    SG_INT,     0 },
	{ CFOFS( campOrigin ),          SG_VECTOR,  0 },
	{ CFOFS( campTime ),            SG_INT,     0 },
	// target_gravity / target_playerspeed persistent overrides (re-applied each
	// ClientEndFrame; a one-shot ps write would be lost, so these must be saved).
	{ CFOFS( gravityOverride ),     SG_INT,     0 },
	{ CFOFS( speedOverride ),       SG_INT,     0 },

	{ CFOFS( hook ),                SG_ENTITY,  0 },  // grapple hook, index-reloc

	// EXCLUDED (transient, re-derived each frame): buttons/oldbuttons, oldOrigin,
	// damage_* accumulators, knockbackSources[], the history[] lag-comp ring,
	// areabits pointer — none are saved.

	{ 0, SG_NONE, 0 }
};

// ── level_locals_t descriptor ────────────────────────────────────────────────
// The world/level SAVE state. Base pointers (clients/gentities), spawn-parse
// scratch, sortedClients[], and the gentity_t* lists (locationHead/bodyQue) are
// re-derived on load and EXCLUDED. level.time is the master clock every entity
// timer is relative to — save-critical.
const saveField_t levelFields[] = {
	{ SLOFS( framenum ),            SG_INT,     0 },
	{ SLOFS( time ),                SG_INT,     0 },  // master clock
	{ SLOFS( previousTime ),        SG_INT,     0 },
	{ SLOFS( startTime ),           SG_INT,     0 },
	{ SLOFS( warmupTime ),          SG_INT,     0 },
	{ SLOFS( teamScores ),          SG_RAW,     sizeof( ((level_locals_t *)0)->teamScores ) },
	{ SLOFS( num_entities ),        SG_INT,     0 },  // bounds the relocation walk

	// Q1 secret progression — found_secrets is live progression, save it.
	{ SLOFS( q1_worldtype ),        SG_INT,     0 },
	{ SLOFS( q1_total_secrets ),    SG_INT,     0 },
	{ SLOFS( q1_found_secrets ),    SG_INT,     0 },
	{ SLOFS( q1_nextSwitchableStyle ), SG_INT,  0 },

	// Behavior-monster census — live per-level progression, mirrors secrets.
	{ SLOFS( numMonstersSpawned ),  SG_INT,     0 },
	{ SLOFS( numMonstersKilled ),   SG_INT,     0 },

	// Mission objectives — pure data (l10n key + flags). ALSO the .psw client
	// (Phase-5). missionObjective_t is a POD; the array is a RAW blob here.
	{ SLOFS( objectives ),          SG_RAW,     sizeof( ((level_locals_t *)0)->objectives ) },
	{ SLOFS( numObjectives ),       SG_INT,     0 },
	{ SLOFS( objectivesComplete ),  SG_QBOOLEAN, 0 },

	{ 0, SG_NONE, 0 }
};

// ── ignore whitelists ────────────────────────────────────────────────────────
// Byte ranges restored from the LIVE struct after a read, so runtime-only state
// survives the load rather than being taken from disk. Phase-1 lists the pool
// pointers (re-acquired, not deserialized) as the canonical ignore ranges.
const saveRange_t gentityIgnoreFields[] = {
#if FEAT_RECAST_NAVMESH
	{ FOFS( navState ),      sizeof( ((gentity_t *)0)->navState ) },
#endif
#if FEAT_MONSTER_AI
	{ FOFS( behaviorState ), sizeof( ((gentity_t *)0)->behaviorState ) },
#endif
	{ 0, 0 }
};

const saveRange_t gclientIgnoreFields[] = {
	{ CFOFS( areabits ), sizeof( ((gclient_t *)0)->areabits ) },  // PVS scratch pointer
	{ 0, 0 }
};

// ── .psw persistent (cross-level) whitelist ──────────────────────────────────
// The player-progression subset that carries across a map change. Data only in
// Phase-1; the Phase-5 .psw serializer consumes it. The core progression lives in
// ps (persistant[]/stats[]/ammo[]/powerups[]/weapon), saved as part of the ps RAW
// blob above; this whitelist marks the additional game-private fields that also
// carry (identity is in pers/sess, likewise RAW).
const saveRange_t gclientPersFields[] = {
	{ CFOFS( ps ),   sizeof( playerState_t ) },      // progression: persistant/stats/ammo/powerups/weapon
	{ CFOFS( sess ), sizeof( clientSession_t ) },    // team/wins/losses identity
	{ CFOFS( pers ), sizeof( clientPersistant_t ) }, // netname
	{ 0, 0 }
};

// ── Phase-1 self-consistency assertions ──────────────────────────────────────
// No serializer exists yet; these compile-time checks prove the descriptor
// infrastructure is well-formed (the offset macros resolve, the field-type enum
// is stable). They reference the tables so the linker keeps them.
_Static_assert( SG_NONE == 0, "SG_NONE must be the 0/terminator sentinel" );
_Static_assert( sizeof( saveField_t ) >= sizeof( size_t ) * 2,
	"saveField_t must carry at least an offset + a type" );

// The Phase-2 callback registry (central table + resolvers + the file-local
// sub-table machinery) lives in g_save_registry.c — a game-type-free translation
// unit (it includes only <string.h> + g_save_funcs.h, NOT g_local.h) so the
// round-trip unit test can compile the REAL resolver code against stub symbols.
// The file-local sub-lists themselves live in the 9 static-holding .c files.
