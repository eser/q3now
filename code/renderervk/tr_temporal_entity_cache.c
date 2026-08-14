// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "tr_temporal_entity_cache.h"

#include <math.h>
#include <string.h>

#define TEMPORAL_ENTITY_HASH_CAPACITY 8192u
// One draw command cannot reference more renderer-owned refentities than the
// 12-bit draw-sort identity permits (the all-ones value is reserved for world).
#define TEMPORAL_ENTITY_BANK_CAPACITY 4095u

typedef struct {
	uint32_t key;
	uint32_t entityGeneration;
	uintptr_t source;
	temporalEntityPose_t pose;
} temporalEntityDenseEntry_t;

typedef struct {
	uint32_t topologyEpoch;
	uint32_t temporalGeneration;
	uint64_t lastCommittedFrame;
	uint32_t committedCount;
	uint16_t committedHash[TEMPORAL_ENTITY_HASH_CAPACITY];
	temporalEntityDenseEntry_t committed[TEMPORAL_ENTITY_BANK_CAPACITY];

	qboolean pending;
	qboolean poisoned;
	qboolean contiguous;
	uint32_t pendingTopologyEpoch;
	uint32_t pendingTemporalGeneration;
	uint64_t pendingFrame;
	uint32_t pendingCount;
	uint16_t pendingHash[TEMPORAL_ENTITY_HASH_CAPACITY];
	temporalEntityDenseEntry_t staging[TEMPORAL_ENTITY_BANK_CAPACITY];

	qboolean cameraValid;
	uint32_t cameraTopologyEpoch;
	uint32_t cameraTemporalGeneration;
	uint64_t cameraCommittedFrame;
	temporalCameraPose_t cameraPose;
	qboolean cameraPending;
	temporalCameraPose_t pendingCameraPose;
} temporalEntityWorldCache_t;

static temporalEntityWorldCache_t s_entityCache[MAX_RENDER_WORLDS];

static qboolean PoseValid( const temporalEntityPose_t *pose ) {
	if ( !pose || pose->hModel == 0 || !pose->modelToken || !pose->modelTopology
			|| !isfinite( pose->backlerp ) ) return qfalse;
	for ( int i = 0; i < 3; ++i ) if ( !isfinite( pose->origin[i] ) ) return qfalse;
	for ( int i = 0; i < 9; ++i ) if ( !isfinite( pose->axis[i] ) ) return qfalse;
	return pose->nonNormalizedAxes <= 1u;
}

static qboolean PoseEqual( const temporalEntityPose_t *a,
		const temporalEntityPose_t *b ) {
	return a->hModel == b->hModel && a->modelToken == b->modelToken
		&& a->modelDataToken == b->modelDataToken
		&& a->modelType == b->modelType && a->modelTopology == b->modelTopology
		&& a->frame == b->frame && a->oldframe == b->oldframe
		&& a->backlerp == b->backlerp
		&& a->nonNormalizedAxes == b->nonNormalizedAxes
		&& memcmp( a->origin, b->origin, sizeof( a->origin ) ) == 0
		&& memcmp( a->axis, b->axis, sizeof( a->axis ) ) == 0;
}

static qboolean ModelCompatible( const temporalEntityPose_t *a,
		const temporalEntityPose_t *b ) {
	return a->hModel == b->hModel && a->modelToken == b->modelToken
		&& a->modelDataToken == b->modelDataToken
		&& a->modelType == b->modelType && a->modelTopology == b->modelTopology;
}

static qboolean CameraValid( const temporalCameraPose_t *pose ) {
	const float *values = (const float *)pose;
	if ( !pose ) return qfalse;
	for ( size_t i = 0; i < sizeof( *pose ) / sizeof( *values ); ++i ) {
		if ( !isfinite( values[i] ) ) return qfalse;
	}
	return qtrue;
}

static qboolean WorldValid( int worldIndex ) {
	return worldIndex >= 0 && worldIndex < MAX_RENDER_WORLDS;
}

static uint32_t IdentityKey( uint32_t ownerId, uint32_t role ) {
	return ownerId * (uint32_t)REF_ENTITY_MOTION_ROLE_COUNT + role;
}

static uint32_t HashStart( uint32_t key ) {
	return ( key * 2654435761u ) & ( TEMPORAL_ENTITY_HASH_CAPACITY - 1u );
}

static int HashFind( const uint16_t *hash, const temporalEntityDenseEntry_t *bank,
		uint32_t key ) {
	uint32_t at = HashStart( key );
	for ( uint32_t probe = 0; probe < TEMPORAL_ENTITY_HASH_CAPACITY; ++probe ) {
		uint16_t encoded = hash[at];
		if ( !encoded ) return -1;
		if ( bank[encoded - 1u].key == key ) return (int)encoded - 1;
		at = ( at + 1u ) & ( TEMPORAL_ENTITY_HASH_CAPACITY - 1u );
	}
	return -1;
}

static qboolean HashInsert( uint16_t *hash,
		const temporalEntityDenseEntry_t *bank, uint32_t index ) {
	uint32_t at = HashStart( bank[index].key );
	for ( uint32_t probe = 0; probe < TEMPORAL_ENTITY_HASH_CAPACITY; ++probe ) {
		if ( !hash[at] ) {
			hash[at] = (uint16_t)( index + 1u );
			return qtrue;
		}
		at = ( at + 1u ) & ( TEMPORAL_ENTITY_HASH_CAPACITY - 1u );
	}
	return qfalse;
}

void R_TemporalEntityCacheResetAll( void ) {
	memset( s_entityCache, 0, sizeof( s_entityCache ) );
}

void R_TemporalEntityCacheResetWorld( int worldIndex ) {
	if ( WorldValid( worldIndex ) ) memset( &s_entityCache[worldIndex], 0,
		sizeof( s_entityCache[worldIndex] ) );
}

qboolean R_TemporalEntityCacheBegin( int worldIndex, uint32_t topologyEpoch,
		uint32_t temporalGeneration, uint64_t frameId ) {
	temporalEntityWorldCache_t *world;
	if ( !WorldValid( worldIndex ) || !topologyEpoch || !temporalGeneration
			|| !frameId ) return qfalse;
	world = &s_entityCache[worldIndex];
	if ( world->pending ) return world->pendingFrame == frameId
		&& world->pendingTopologyEpoch == topologyEpoch
		&& world->pendingTemporalGeneration == temporalGeneration
		&& !world->poisoned;
	if ( world->lastCommittedFrame && frameId <= world->lastCommittedFrame ) return qfalse;
	world->pending = qtrue;
	world->poisoned = qfalse;
	world->pendingFrame = frameId;
	world->pendingTopologyEpoch = topologyEpoch;
	world->pendingTemporalGeneration = temporalGeneration;
	world->contiguous = world->lastCommittedFrame
		&& frameId == world->lastCommittedFrame + 1u;
	world->pendingCount = 0;
	world->cameraPending = qfalse;
	memset( world->pendingHash, 0, sizeof( world->pendingHash ) );
	return qtrue;
}

qboolean R_TemporalEntityCacheStageCamera( int worldIndex, uint64_t frameId,
		const temporalCameraPose_t *current,
		temporalCameraPoseReceipt_t *outReceipt ) {
	temporalEntityWorldCache_t *world;
	temporalCameraPoseReceipt_t candidate;
	if ( !WorldValid( worldIndex ) || !outReceipt || !CameraValid( current ) ) return qfalse;
	world = &s_entityCache[worldIndex];
	if ( !world->pending || world->poisoned || world->pendingFrame != frameId ) return qfalse;
	if ( world->cameraPending && memcmp( &world->pendingCameraPose, current,
			sizeof( *current ) ) != 0 ) {
		world->poisoned = qtrue;
		return qfalse;
	}
	world->cameraPending = qtrue;
	world->pendingCameraPose = *current;
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.frameId = frameId;
	candidate.current = *current;
	candidate.valid = qtrue;
	candidate.previousValid = world->contiguous && world->cameraValid
		&& world->cameraTopologyEpoch == world->pendingTopologyEpoch
		&& world->cameraTemporalGeneration == world->pendingTemporalGeneration
		&& world->cameraCommittedFrame == world->lastCommittedFrame;
	if ( candidate.previousValid ) {
		candidate.previousFrameId = world->cameraCommittedFrame;
		candidate.previous = world->cameraPose;
	}
	*outReceipt = candidate;
	return qtrue;
}

qboolean R_TemporalEntityCacheRecord( int worldIndex, uint64_t frameId,
		uintptr_t sourceToken, const refEntityMotion_t *identity,
		const temporalEntityPose_t *current,
		temporalEntityPoseReceipt_t *outReceipt ) {
	temporalEntityWorldCache_t *world;
	temporalEntityDenseEntry_t *entry;
	temporalEntityPoseReceipt_t candidate;
	uint32_t key;
	int index, previousIndex;
	qboolean previousValid;
	if ( !WorldValid( worldIndex ) || !outReceipt || !sourceToken
			|| !RefEntityMotion_IsValid( identity ) || identity->generation == 0
			|| !PoseValid( current ) || identity->ownerId >= MAX_GENTITIES
			|| identity->role <= REF_ENTITY_MOTION_ROLE_NONE
			|| identity->role >= REF_ENTITY_MOTION_ROLE_COUNT ) return qfalse;
	world = &s_entityCache[worldIndex];
	if ( !world->pending || world->poisoned || world->pendingFrame != frameId ) return qfalse;
	key = IdentityKey( identity->ownerId, identity->role );
	index = HashFind( world->pendingHash, world->staging, key );
	if ( index >= 0 ) {
		entry = &world->staging[index];
		if ( entry->source != sourceToken
				|| entry->entityGeneration != identity->generation
				|| !PoseEqual( &entry->pose, current ) ) {
			world->poisoned = qtrue;
			return qfalse;
		}
	} else {
		if ( world->pendingCount >= TEMPORAL_ENTITY_BANK_CAPACITY ) {
			world->poisoned = qtrue;
			return qfalse;
		}
		index = (int)world->pendingCount++;
		entry = &world->staging[index];
		entry->key = key;
		entry->entityGeneration = identity->generation;
		entry->source = sourceToken;
		entry->pose = *current;
		if ( !HashInsert( world->pendingHash, world->staging, (uint32_t)index ) ) {
			world->poisoned = qtrue;
			return qfalse;
		}
	}
	previousIndex = HashFind( world->committedHash, world->committed, key );
	previousValid = world->contiguous && previousIndex >= 0
		&& world->topologyEpoch == world->pendingTopologyEpoch
		&& world->temporalGeneration == world->pendingTemporalGeneration
		&& world->committed[previousIndex].entityGeneration == identity->generation
		&& ModelCompatible( &world->committed[previousIndex].pose, current );
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.frameId = frameId;
	candidate.identity = *identity;
	candidate.current = *current;
	candidate.valid = qtrue;
	candidate.previousValid = previousValid;
	if ( previousValid ) {
		candidate.previousFrameId = world->lastCommittedFrame;
		candidate.previous = world->committed[previousIndex].pose;
	}
	*outReceipt = candidate;
	return qtrue;
}

qboolean R_TemporalEntityCacheFinish( int worldIndex, uint64_t frameId,
		qboolean submitted ) {
	temporalEntityWorldCache_t *world;
	qboolean publish;
	if ( !WorldValid( worldIndex ) ) return qfalse;
	world = &s_entityCache[worldIndex];
	if ( !world->pending || world->pendingFrame != frameId ) return qfalse;
	publish = submitted && !world->poisoned;
	if ( publish ) {
		world->committedCount = world->pendingCount;
		memcpy( world->committed, world->staging,
			world->committedCount * sizeof( world->committed[0] ) );
		memset( world->committedHash, 0, sizeof( world->committedHash ) );
		for ( uint32_t i = 0; i < world->committedCount; ++i ) {
			world->committed[i].source = 0;
			if ( !HashInsert( world->committedHash, world->committed, i ) ) {
				publish = qfalse;
				world->committedCount = 0;
				memset( world->committedHash, 0, sizeof( world->committedHash ) );
				break;
			}
		}
	}
	if ( publish ) {
		world->topologyEpoch = world->pendingTopologyEpoch;
		world->temporalGeneration = world->pendingTemporalGeneration;
		world->lastCommittedFrame = frameId;
		if ( world->cameraPending ) {
			world->cameraValid = qtrue;
			world->cameraTopologyEpoch = world->pendingTopologyEpoch;
			world->cameraTemporalGeneration = world->pendingTemporalGeneration;
			world->cameraCommittedFrame = frameId;
			world->cameraPose = world->pendingCameraPose;
		}
	}
	world->pending = qfalse;
	world->poisoned = qfalse;
	world->contiguous = qfalse;
	world->pendingTopologyEpoch = 0;
	world->pendingTemporalGeneration = 0;
	world->pendingFrame = 0;
	world->pendingCount = 0;
	world->cameraPending = qfalse;
	memset( world->pendingHash, 0, sizeof( world->pendingHash ) );
	memset( &world->pendingCameraPose, 0, sizeof( world->pendingCameraPose ) );
	return publish;
}
