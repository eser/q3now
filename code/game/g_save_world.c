// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
//
// g_save_world.c — the .svg world orchestration (Phase-4): the entity / client /
// level walks over the real game globals, the AI-pool serialization, and the
// dev/cheat-gated savegame / loadgame command. This is the game-only glue that
// drives the game-type-free field-walker (g_save_serialize.c) with the real
// g_entities / level.clients / bg_itemlist and the landed atomic I/O
// (g_save_file.c). RealRTCW's WriteGame/ReadGame, built from q3now's own pieces.
//
// Sub-landing (Phase-4): WRITE is complete; READ parses + validates the buffer
// into the serialized (index-relocated) form. The load FIXUP — resolving indices
// to live pointers, trap_LinkEntity, re-acquiring the pools, re-interning strings,
// re-parsing scripts — is PHASE-6. loadgame here reads + validates + reports
// "Phase-6 pending"; it does NOT resume the live world.
//
// Byte-identical for normal gameplay: the only entry point is the savegame /
// loadgame command, gated on sv_cheats. No cheat -> the serializer is never
// reached -> gameplay unchanged.

#include "g_local.h"
#include "g_behavior.h"    // behaviorState_t (AI-pool slot type)
#include "g_save.h"
#include "g_save_serialize.h"
#include "g_save_file.h"

LOG_DECLARE_CHANNEL( ch_game, "game" );

// The .svg world payload. The largest snapshot (all entities + clients + level +
// pools) must fit; oversize is rejected (overflow), never truncated. Sized to fit
// the WASM module's 16 MiB initial memory: a full arena (≈1 KB/entity × ~1 K
// entities + clients + pools) is ≈1-2 MiB, so 2 MiB is ample — and it stays within
// the RLE worst-case (2×) headroom of the 4 MiB I/O scratch in g_save_file.c, so a
// non-compressible payload can never overflow the encode buffer. This is the
// serializer's working buffer, handed to SG_WriteFile (header + optional RLE).
#define SG_WORLD_PAYLOAD_BYTES ( 2 * 1024 * 1024 )
static byte sg_worldPayload[SG_WORLD_PAYLOAD_BYTES];

// Section tags — a light structural frame so the read can sanity-check ordering.
// (The header/magic is the file-level guard from Phase-3; these bound the walks.)
#define SG_SEC_ENTITIES  0x454E5453u  // 'ENTS'
#define SG_SEC_CLIENTS   0x434C4E54u  // 'CLNT'
#define SG_SEC_LEVEL     0x4C564C20u  // 'LVL '
#define SG_SEC_BEHAVIOR  0x42484156u  // 'BHAV'
#define SG_SEC_END       0x454E4420u  // 'END '

// Sentinel for the per-entity index marker that terminates the entity list.
#define SG_ENT_LIST_END  ( -1 )

// Fill the relocation bases from the live game globals. The walker never names
// these; SG_WriteWorld/SG_ReadWorld inject them.
static void SG_FillBases( sgRelocBases_t *b ) {
	b->entityBase   = g_entities;
	b->entityStride = sizeof( gentity_t );
	b->entityCount  = MAX_GENTITIES;
	b->clientBase   = level.clients;
	b->clientStride = sizeof( gclient_t );
	b->clientCount  = level.maxclients;
	b->itemBase     = bg_itemlist;
	b->itemStride   = sizeof( gitem_t );
	b->itemCount    = bg_numItems;
}

static void SG_PutTag( sgStream_t *s, uint32_t tag ) {
	SG_StreamWriteRaw( s, &tag, sizeof( tag ) );
}
static int SG_ExpectTag( sgStream_t *s, uint32_t want ) {
	uint32_t got = 0;
	SG_StreamReadRaw( s, &got, sizeof( got ) );
	return ( !s->overflow && got == want );
}

// ── WRITE: serialize the live world into the payload buffer ──────────────────
// Returns the payload byte count, or (size_t)-1 on overflow.
static size_t SG_WriteWorld( byte *payload, size_t cap ) {
	sgStream_t     s;
	sgRelocBases_t bases;
	int            i;

	SG_StreamInitWrite( &s, payload, cap );
	SG_FillBases( &bases );

	// ── entities: each inuse gentity as { index, fields }; list ends at -1 ──
	SG_PutTag( &s, SG_SEC_ENTITIES );
	for ( i = 0; i < level.num_entities; i++ ) {
		gentity_t *ent = &g_entities[i];
		if ( !ent->inuse ) {
			continue;
		}
		SG_StreamWriteI32( &s, i );                       // the entity's own index
		SG_WriteFields( gentityFields, ent, &bases, &s );
	}
	SG_StreamWriteI32( &s, SG_ENT_LIST_END );

	// ── clients: fixed 0..maxclients (a client slot is dense) ──
	SG_PutTag( &s, SG_SEC_CLIENTS );
	for ( i = 0; i < level.maxclients; i++ ) {
		SG_WriteFields( gclientFields, &level.clients[i], &bases, &s );
	}

	// ── level globals ──
	SG_PutTag( &s, SG_SEC_LEVEL );
	SG_WriteFields( levelFields, &level, &bases, &s );

	// ── AI pools: each inuse monster's behaviorState slot, keyed by entity idx.
	// behaviorState is pure data (serialized verbatim); the slot<->entity link is
	// rebuilt on load by re-acquiring. navState is NOT saved (rebuilt from navGoal
	// in Phase-6). List ends at -1.
	SG_PutTag( &s, SG_SEC_BEHAVIOR );
#if FEAT_MONSTER_AI
	{
		byte   slot[sizeof( behaviorState_t )];
		size_t slotSz = Behavior_SlotSize();
		for ( i = 0; i < level.num_entities; i++ ) {
			gentity_t *ent = &g_entities[i];
			if ( !ent->inuse || !ent->behaviorState ) {
				continue;
			}
			if ( Behavior_SaveSlot( ent, slot, slotSz ) ) {
				SG_StreamWriteI32( &s, i );
				SG_StreamWriteRaw( &s, slot, slotSz );
			}
		}
	}
#endif
	SG_StreamWriteI32( &s, SG_ENT_LIST_END );

	SG_PutTag( &s, SG_SEC_END );

	if ( s.overflow ) {
		return (size_t)-1;
	}
	return s.len;
}

// ── READ: parse + validate the payload into the serialized form ──────────────
// Phase-4 reads into scratch structs and validates the framing / indices / names;
// it does NOT patch the live world (that is Phase-6). Returns 1 if the buffer is a
// well-formed, self-consistent .svg world, 0 otherwise. `entOut`/`clientOut` are
// caller scratch of MAX_GENTITIES / maxclients elements the parse fills (Phase-6
// consumes them); pass NULL to validate-only.
static int SG_ReadWorld( const byte *payload, size_t len ) {
	sgStream_t     s;
	sgRelocBases_t bases;
	int            i;
	gentity_t      scratchEnt;   // one-at-a-time parse target (Phase-6 keeps them)
	gclient_t      scratchClient;
	level_locals_t scratchLevel;

	SG_StreamInitRead( &s, payload, len );
	SG_FillBases( &bases );

	// ── entities ──
	if ( !SG_ExpectTag( &s, SG_SEC_ENTITIES ) ) {
		return 0;
	}
	for ( ;; ) {
		int idx = SG_StreamReadI32( &s );
		if ( s.overflow ) {
			return 0;
		}
		if ( idx == SG_ENT_LIST_END ) {
			break;
		}
		if ( idx < 0 || idx >= MAX_GENTITIES ) {
			return 0;   // out-of-range entity index = corrupt
		}
		memset( &scratchEnt, 0, sizeof( scratchEnt ) );
		if ( !SG_ReadFields( gentityFields, &scratchEnt, &bases, &s, NULL ) ) {
			return 0;
		}
	}

	// ── clients ──
	if ( !SG_ExpectTag( &s, SG_SEC_CLIENTS ) ) {
		return 0;
	}
	for ( i = 0; i < level.maxclients; i++ ) {
		memset( &scratchClient, 0, sizeof( scratchClient ) );
		if ( !SG_ReadFields( gclientFields, &scratchClient, &bases, &s, NULL ) ) {
			return 0;
		}
	}

	// ── level ──
	if ( !SG_ExpectTag( &s, SG_SEC_LEVEL ) ) {
		return 0;
	}
	memset( &scratchLevel, 0, sizeof( scratchLevel ) );
	if ( !SG_ReadFields( levelFields, &scratchLevel, &bases, &s, NULL ) ) {
		return 0;
	}

	// ── behavior pools ──
	if ( !SG_ExpectTag( &s, SG_SEC_BEHAVIOR ) ) {
		return 0;
	}
#if FEAT_MONSTER_AI
	{
		byte   slot[sizeof( behaviorState_t )];
		size_t slotSz = Behavior_SlotSize();
		for ( ;; ) {
			int idx = SG_StreamReadI32( &s );
			if ( s.overflow ) {
				return 0;
			}
			if ( idx == SG_ENT_LIST_END ) {
				break;
			}
			if ( idx < 0 || idx >= MAX_GENTITIES ) {
				return 0;
			}
			SG_StreamReadRaw( &s, slot, slotSz );   // parse the slot (Phase-6 loads it)
			if ( s.overflow ) {
				return 0;
			}
		}
	}
#else
	// If monster-AI is compiled out, the behavior list must still be a lone -1.
	{
		int idx = SG_StreamReadI32( &s );
		if ( s.overflow || idx != SG_ENT_LIST_END ) {
			return 0;
		}
	}
#endif

	if ( !SG_ExpectTag( &s, SG_SEC_END ) ) {
		return 0;
	}
	return 1;
}

// ═════════════════════════════════════════════════════════════════════════════
// The LIVE .svg load (Phase-6) — turn a validated buffer into a resumed world.
//
// Single-pass relocation discipline: Phase-A loads EVERY entity/client into its
// fixed-base slot (g_entities[idx] / level.clients[idx]) with the pointer fields
// still holding INDICES; Phase-B then resolves every pointer field to &base[idx]
// once all slots are populated, so forward-references (A -> B where B loads after
// A) resolve correctly. Then the engine re-link: trap_LinkEntity re-establishes
// the transient r. (world/areaportal links that were never saved), the AI pools
// are re-acquired from the verbatim behaviorState blobs, navState is rebuilt.
//
// The internal-index-reloc fields (groundEntityNum/clientNum/otherEntityNum in the
// s/ps RAW blobs, the biased logicEntities[]) are ENTITYNUMs referencing the fixed
// g_entities base BY NUMBER — since Phase-A repopulates every numbered slot, they
// survive VERBATIM (the entity at g_entities[num] is the one they meant). No
// per-blob fixup is needed beyond the pointer-field Phase-B pass.
//
// Returns 1 on a resumed world, 0 on a malformed buffer (world left cleared).
static int SG_LoadWorld( const byte *payload, size_t len ) {
	sgStream_t     s;
	sgRelocBases_t bases;
	int            i;
	int            loadedEnts[MAX_GENTITIES];
	int            numLoadedEnts = 0;
#if FEAT_MONSTER_AI
	// Deferred behavior slots: entity index + its verbatim behaviorState blob,
	// applied after Phase-B (the entity must be fully loaded first).
	struct { int idx; byte slot[sizeof( behaviorState_t )]; } behav[MAX_MONSTERS];
	int    numBehav = 0;
	size_t slotSz = Behavior_SlotSize();
#endif

	SG_StreamInitRead( &s, payload, len );
	SG_FillBases( &bases );

	// Clear the live world first (mirrors G_InitGame's entity reset), so any
	// entity not present in the save starts free.
	memset( g_entities, 0, MAX_GENTITIES * sizeof( g_entities[0] ) );
#if FEAT_MONSTER_AI
	// Free every behaviorState pool slot before the re-acquire below — the entity
	// memset dropped the ent->behaviorState back-pointers, so without this the
	// prior world's slots would stay marked used and a repeated load would exhaust
	// the pool. ResetPool clears behaviorPoolUsed[], matching the g_entities clear.
	Behavior_ResetPool();
#endif

	// ── Phase-A: entities into their fixed-base slots ──
	if ( !SG_ExpectTag( &s, SG_SEC_ENTITIES ) ) {
		return 0;
	}
	for ( ;; ) {
		int idx = SG_StreamReadI32( &s );
		if ( s.overflow ) {
			return 0;
		}
		if ( idx == SG_ENT_LIST_END ) {
			break;
		}
		if ( idx < 0 || idx >= MAX_GENTITIES ) {
			return 0;
		}
		// Load directly into the live slot; strings re-interned via G_NewString.
		if ( !SG_ReadFields( gentityFields, &g_entities[idx], &bases, &s, G_NewString ) ) {
			return 0;
		}
		loadedEnts[numLoadedEnts++] = idx;
	}

	// ── Phase-A: clients into their fixed-base slots ──
	if ( !SG_ExpectTag( &s, SG_SEC_CLIENTS ) ) {
		return 0;
	}
	for ( i = 0; i < level.maxclients; i++ ) {
		if ( !SG_ReadFields( gclientFields, &level.clients[i], &bases, &s, G_NewString ) ) {
			return 0;
		}
	}

	// ── Phase-A: level globals (levelFields excludes the base pointers, so the
	// live level.clients / level.gentities survive; only saved fields load) ──
	if ( !SG_ExpectTag( &s, SG_SEC_LEVEL ) ) {
		return 0;
	}
	if ( !SG_ReadFields( levelFields, &level, &bases, &s, G_NewString ) ) {
		return 0;
	}

	// ── behavior pools: collect the slots (apply after Phase-B) ──
	if ( !SG_ExpectTag( &s, SG_SEC_BEHAVIOR ) ) {
		return 0;
	}
#if FEAT_MONSTER_AI
	for ( ;; ) {
		int idx = SG_StreamReadI32( &s );
		if ( s.overflow ) {
			return 0;
		}
		if ( idx == SG_ENT_LIST_END ) {
			break;
		}
		if ( idx < 0 || idx >= MAX_GENTITIES || numBehav >= MAX_MONSTERS ) {
			return 0;
		}
		behav[numBehav].idx = idx;
		SG_StreamReadRaw( &s, behav[numBehav].slot, slotSz );
		if ( s.overflow ) {
			return 0;
		}
		numBehav++;
	}
#else
	{
		int idx = SG_StreamReadI32( &s );
		if ( s.overflow || idx != SG_ENT_LIST_END ) {
			return 0;
		}
	}
#endif

	if ( !SG_ExpectTag( &s, SG_SEC_END ) ) {
		return 0;
	}

	// ── Phase-B: resolve every pointer field now that all slots are populated ──
	for ( i = 0; i < numLoadedEnts; i++ ) {
		SG_ResolveIndices( gentityFields, &g_entities[loadedEnts[i]], &bases );
	}
	for ( i = 0; i < level.maxclients; i++ ) {
		SG_ResolveIndices( gclientFields, &level.clients[i], &bases );
	}
	SG_ResolveIndices( levelFields, &level, &bases );

	// ── Engine re-link + AI-pool re-acquire ──
	// Wire each loaded client's ps back to its gentity (the client<->entity link is
	// re-derived, not saved), then trap_LinkEntity to rebuild the transient r.
	for ( i = 0; i < numLoadedEnts; i++ ) {
		gentity_t *ent = &g_entities[loadedEnts[i]];
		if ( ent->inuse ) {
			trap_LinkEntity( ent );
		}
	}
#if FEAT_MONSTER_AI
	// Re-acquire a behaviorState slot for each saved monster and refill it from the
	// verbatim blob (navState is NOT loaded — Behavior_LoadSlot leaves the monster
	// to rebuild nav from navGoal on its next think).
	for ( i = 0; i < numBehav; i++ ) {
		gentity_t *ent = &g_entities[behav[i].idx];
		if ( ent->inuse ) {
			Behavior_LoadSlot( ent, behav[i].slot, slotSz );
		}
	}
#endif

	return 1;
}

// ═════════════════════════════════════════════════════════════════════════════
// The .psw PERSISTENT cross-map subset (Phase-5).
//
// The lightweight carry that survives a map transition — deliberately NOT the
// full world. It reuses the Phase-4 walker (the range-walker for the client
// subset, the field-walker/RAW for objectives) and the Phase-3 atomic write. The
// subset is exactly:
//   1. player-progression: each client's gclientPersFields[] (ps / sess / pers —
//      persistant/stats/ammo/powerups/weapon + identity + netname), copied
//      verbatim as byte ranges. ps carries internal index-reloc fields resolved by
//      the Phase-6 fixup, identical to the .svg ps RAW blob.
//   2. objectives: level.objectives[] + numObjectives + objectivesComplete (pure
//      POD mission-objective state — the objectives-binding persistence).
//
// W-87 boundary: this is the HEAVY reflection persistence. The LIGHT single-bit
// progress (sigil / rune collection via CVAR_ARCHIVE / Q1 serverflags) does NOT go
// through the .psw — that is the q1-sigil-serverflags-hub's separate cvar-archive
// path. Nothing sigil-related is serialized here.
//
// Campaign/episode progress: there is NO campaign-progress struct in the game yet
// (g_objectives.c: objectives are "NOT wired to cross-map campaign progression —
// the separate objectives-binding work"). So the .psw carries objectives +
// player-progression today; the campaign-progress SEAM is a Phase-6 /
// objectives-binding follow-up. No struct is invented here.
//
// Sub-landing (Phase-5): WRITE complete; READ parses + validates. The live
// re-apply (patching the incoming client's ps.persistant + level.objectives on the
// next map's G_InitGame) is PHASE-6 — loadpersistent reports "Phase-6 pending".
// ═════════════════════════════════════════════════════════════════════════════

#define SG_SEC_PROGRESSION 0x50524F47u  // 'PROG'
#define SG_SEC_OBJECTIVES  0x4F424A53u  // 'OBJS'

// Build the .psw subset payload from the live world. Returns byte count, or -1.
static size_t SG_WritePersistent( byte *payload, size_t cap ) {
	sgStream_t s;
	int        i;

	SG_StreamInitWrite( &s, payload, cap );

	// ── player progression: each client's persistent subset (ranges, verbatim) ──
	SG_PutTag( &s, SG_SEC_PROGRESSION );
	SG_StreamWriteI32( &s, level.maxclients );
	for ( i = 0; i < level.maxclients; i++ ) {
		SG_WriteRanges( gclientPersFields, &level.clients[i], &s );
	}

	// ── objectives: the mission-objective completion state (POD, verbatim) ──
	SG_PutTag( &s, SG_SEC_OBJECTIVES );
	SG_StreamWriteRaw( &s, level.objectives, sizeof( level.objectives ) );
	SG_StreamWriteI32( &s, level.numObjectives );
	SG_StreamWriteI32( &s, (int32_t)level.objectivesComplete );

	// ── campaign-progress: no struct yet — the SEAM is Phase-6/objectives-binding.
	// A zero-length section marker documents the seam without inventing state.
	SG_PutTag( &s, SG_SEC_END );

	if ( s.overflow ) {
		return (size_t)-1;
	}
	return s.len;
}

// Parse + validate the .psw subset. Phase-4-style: reads into scratch, validates
// framing/sizes; does NOT re-apply to the live world (Phase-6). Returns 1 if the
// buffer is a well-formed .psw, 0 otherwise.
static int SG_ReadPersistent( const byte *payload, size_t len ) {
	sgStream_t s;
	int        i, savedMaxclients;
	gclient_t  scratchClient;

	SG_StreamInitRead( &s, payload, len );

	// ── player progression ──
	if ( !SG_ExpectTag( &s, SG_SEC_PROGRESSION ) ) {
		return 0;
	}
	savedMaxclients = SG_StreamReadI32( &s );
	if ( s.overflow || savedMaxclients < 0 || savedMaxclients > MAX_CLIENTS ) {
		return 0;   // implausible client count = corrupt
	}
	for ( i = 0; i < savedMaxclients; i++ ) {
		memset( &scratchClient, 0, sizeof( scratchClient ) );
		if ( !SG_ReadRanges( gclientPersFields, &scratchClient, &s ) ) {
			return 0;
		}
	}

	// ── objectives ──
	if ( !SG_ExpectTag( &s, SG_SEC_OBJECTIVES ) ) {
		return 0;
	}
	{
		missionObjective_t scratchObj[MAX_MISSION_OBJECTIVES];
		int                numObj;
		int                complete;
		SG_StreamReadRaw( &s, scratchObj, sizeof( scratchObj ) );
		numObj   = SG_StreamReadI32( &s );
		complete = SG_StreamReadI32( &s );
		if ( s.overflow || numObj < 0 || numObj > MAX_MISSION_OBJECTIVES ) {
			return 0;
		}
		( void )complete;  // validated; Phase-6 applies it
	}

	if ( !SG_ExpectTag( &s, SG_SEC_END ) ) {
		return 0;
	}
	return 1;
}

// The LIVE .psw apply (Phase-6). The narrow cross-map carry: patch each present
// client's persistent progression subset (gclientPersFields ranges) into the LIVE
// level.clients[i], and apply the mission objectives. No entity relocation (the
// .psw carries no entities), so this is a straight range-copy + objectives write —
// the simpler of the two load paths. Returns 1 on success, 0 on a malformed .psw.
//
// This is where objectives persistence goes LIVE (the objectives-binding go-live):
// level.objectives / numObjectives / objectivesComplete are restored from the .psw.
static int SG_ApplyPersistent( const byte *payload, size_t len ) {
	sgStream_t s;
	int        i, savedMaxclients;

	SG_StreamInitRead( &s, payload, len );

	// ── player progression: apply each saved client's subset into the live client.
	if ( !SG_ExpectTag( &s, SG_SEC_PROGRESSION ) ) {
		return 0;
	}
	savedMaxclients = SG_StreamReadI32( &s );
	if ( s.overflow || savedMaxclients < 0 || savedMaxclients > MAX_CLIENTS ) {
		return 0;
	}
	for ( i = 0; i < savedMaxclients; i++ ) {
		if ( i < level.maxclients ) {
			// Apply into the live client. gclientPersFields is ps/sess/pers — the
			// progression + identity that carries across the map. (ps internal
			// ENTITYNUMs survive verbatim; the incoming map re-derives transient
			// per-frame client state on its own ClientSpawn.)
			if ( !SG_ReadRanges( gclientPersFields, &level.clients[i], &s ) ) {
				return 0;
			}
		} else {
			// More saved clients than this map has slots — skip the extras by
			// reading into a scratch (keeps the stream aligned).
			gclient_t scratch;
			if ( !SG_ReadRanges( gclientPersFields, &scratch, &s ) ) {
				return 0;
			}
		}
	}

	// ── objectives: restore the mission-objective state into the live level ──
	if ( !SG_ExpectTag( &s, SG_SEC_OBJECTIVES ) ) {
		return 0;
	}
	{
		int numObj, complete;
		SG_StreamReadRaw( &s, level.objectives, sizeof( level.objectives ) );
		numObj   = SG_StreamReadI32( &s );
		complete = SG_StreamReadI32( &s );
		if ( s.overflow || numObj < 0 || numObj > MAX_MISSION_OBJECTIVES ) {
			return 0;
		}
		level.numObjectives      = numObj;
		level.objectivesComplete = ( complete != 0 );
	}

	if ( !SG_ExpectTag( &s, SG_SEC_END ) ) {
		return 0;
	}
	return 1;
}

// ── the dev/cheat-gated command ──────────────────────────────────────────────

static void SG_SaveGame_f( void ) {
	char   name[MAX_QPATH];
	char   qpath[MAX_QPATH];
	size_t bytes;

	if ( trap_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: savegame <name>\n" );
		return;
	}
	trap_Argv( 1, name, sizeof( name ) );

	bytes = SG_WriteWorld( sg_worldPayload, sizeof( sg_worldPayload ) );
	if ( bytes == (size_t)-1 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
			"savegame: world too large for the %d-byte payload buffer\n",
			(int)sizeof( sg_worldPayload ) );
		return;
	}

	Com_sprintf( qpath, sizeof( qpath ), "save/%s.svg", name );
	if ( !SG_WriteFile( qpath, sg_worldPayload, bytes, 1 /* RLE */ ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game), "savegame: write failed for %s\n", qpath );
		return;
	}
	Com_Log( SEV_INFO, LOG_CH(ch_game),
		"savegame: wrote %s (%d payload bytes)\n", qpath, (int)bytes );
}

static void SG_LoadGame_f( void ) {
	char   name[MAX_QPATH];
	char   qpath[MAX_QPATH];
	size_t payloadLen;

	if ( trap_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: loadgame <name>\n" );
		return;
	}
	trap_Argv( 1, name, sizeof( name ) );
	Com_sprintf( qpath, sizeof( qpath ), "save/%s.svg", name );

	payloadLen = SG_ReadFile( qpath, sg_worldPayload, sizeof( sg_worldPayload ) );
	if ( payloadLen == (size_t)-1 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
			"loadgame: %s missing / bad header / too large\n", qpath );
		return;
	}

	// Validate first (a malformed buffer must not clear the world), then load live.
	if ( !SG_ReadWorld( sg_worldPayload, payloadLen ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
			"loadgame: %s payload is malformed (framing/index/callback validation failed)\n",
			qpath );
		return;
	}

	// Phase-6: the LIVE resume. Two-phase relocation (load-all -> resolve) +
	// trap_LinkEntity + AI-pool re-acquire + string re-intern.
	if ( !SG_LoadWorld( sg_worldPayload, payloadLen ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
			"loadgame: %s failed during live load (world may be partially cleared)\n", qpath );
		return;
	}
	Com_Log( SEV_INFO, LOG_CH(ch_game),
		"loadgame: %s RESUMED (%d payload bytes) — entities relinked, pointers resolved, "
		"AI pools re-acquired.\n", qpath, (int)payloadLen );
}

// ── the .psw persistent-subset command (Phase-5) ─────────────────────────────

static void SG_SavePersistent_f( void ) {
	char   name[MAX_QPATH];
	char   qpath[MAX_QPATH];
	size_t bytes;

	if ( trap_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: savepersistent <slot>\n" );
		return;
	}
	trap_Argv( 1, name, sizeof( name ) );

	// The .psw subset is small — the world payload buffer more than covers it.
	bytes = SG_WritePersistent( sg_worldPayload, sizeof( sg_worldPayload ) );
	if ( bytes == (size_t)-1 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
			"savepersistent: subset overflow (should not happen for the .psw subset)\n" );
		return;
	}

	Com_sprintf( qpath, sizeof( qpath ), "save/%s.psw", name );
	if ( !SG_WriteFile( qpath, sg_worldPayload, bytes, 1 /* RLE */ ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game), "savepersistent: write failed for %s\n", qpath );
		return;
	}
	Com_Log( SEV_INFO, LOG_CH(ch_game),
		"savepersistent: wrote %s (%d payload bytes: progression + objectives)\n",
		qpath, (int)bytes );
}

static void SG_LoadPersistent_f( void ) {
	char   name[MAX_QPATH];
	char   qpath[MAX_QPATH];
	size_t payloadLen;

	if ( trap_Argc() < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game), "Usage: loadpersistent <slot>\n" );
		return;
	}
	trap_Argv( 1, name, sizeof( name ) );
	Com_sprintf( qpath, sizeof( qpath ), "save/%s.psw", name );

	payloadLen = SG_ReadFile( qpath, sg_worldPayload, sizeof( sg_worldPayload ) );
	if ( payloadLen == (size_t)-1 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
			"loadpersistent: %s missing / bad header / too large\n", qpath );
		return;
	}

	if ( !SG_ReadPersistent( sg_worldPayload, payloadLen ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
			"loadpersistent: %s payload is malformed (framing/size validation failed)\n",
			qpath );
		return;
	}

	// Phase-6: the LIVE apply — patch the persistent progression + objectives into
	// the live world (the objectives-binding go-live).
	if ( !SG_ApplyPersistent( sg_worldPayload, payloadLen ) ) {
		Com_Log( SEV_WARN, LOG_CH(ch_game),
			"loadpersistent: %s failed during live apply\n", qpath );
		return;
	}
	Com_Log( SEV_INFO, LOG_CH(ch_game),
		"loadpersistent: %s APPLIED (%d payload bytes) — progression + objectives "
		"carried into the live world.\n", qpath, (int)payloadLen );
}

// Dispatched from ConsoleCommand. Returns qtrue if it handled `cmd`. Gated on
// sv_cheats so normal gameplay never reaches the serializer (byte-identical).
// Handles both the .svg full quicksave (savegame/loadgame) and the .psw persistent
// cross-map subset (savepersistent/loadpersistent).
qboolean G_Save_Command( const char *cmd ) {
	int isSvg = ( Q_stricmp( cmd, "savegame" ) == 0 || Q_stricmp( cmd, "loadgame" ) == 0 );
	int isPsw = ( Q_stricmp( cmd, "savepersistent" ) == 0 || Q_stricmp( cmd, "loadpersistent" ) == 0 );

	if ( !isSvg && !isPsw ) {
		return qfalse;
	}
	if ( !g_cheats.integer ) {
		Com_Log( SEV_INFO, LOG_CH(ch_game),
			"%s is cheat-protected (sv_cheats 1)\n", cmd );
		return qtrue;
	}
	if ( Q_stricmp( cmd, "savegame" ) == 0 ) {
		SG_SaveGame_f();
	} else if ( Q_stricmp( cmd, "loadgame" ) == 0 ) {
		SG_LoadGame_f();
	} else if ( Q_stricmp( cmd, "savepersistent" ) == 0 ) {
		SG_SavePersistent_f();
	} else {
		SG_LoadPersistent_f();
	}
	return qtrue;
}
