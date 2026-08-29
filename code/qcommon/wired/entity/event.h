// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_ENTITY_EVENT_H
#define WIRED_ENTITY_EVENT_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRED_ENTITY_EVENT_SCHEMA_VERSION 1u
#define WIRED_ENTITY_EVENT_QUEUE_SCHEMA_VERSION 1u
#define WIRED_ENTITY_EVENT_NAME_CAPACITY 64u
#define WIRED_ENTITY_EVENT_TEXT_CAPACITY 192u
#define WIRED_ENTITY_EVENT_QUEUE_CAPACITY 256u

typedef enum {
	WIRED_ENTITY_EVENT_VALUE_NONE = 0,
	WIRED_ENTITY_EVENT_VALUE_INT,
	WIRED_ENTITY_EVENT_VALUE_FLOAT,
	WIRED_ENTITY_EVENT_VALUE_STRING,
	WIRED_ENTITY_EVENT_VALUE_VEC3
} wiredEntityEventValueType_t;

typedef struct {
	uint32_t schemaVersion;
	uint32_t stableEventId;
	uint32_t stableFieldId;
	uint64_t sequence;
	int32_t gameTime;
	int32_t entityNum;
	int32_t sourceEntityNum;
	wiredEntityEventValueType_t valueType;
	int32_t intValue;
	float floatValue;
	vec3_t vectorValue;
	char name[WIRED_ENTITY_EVENT_NAME_CAPACITY];
	char textValue[WIRED_ENTITY_EVENT_TEXT_CAPACITY];
	qboolean ready;
} wiredEntityEvent_t;

typedef struct {
	uint32_t schemaVersion;
	wiredEntityEvent_t events[WIRED_ENTITY_EVENT_QUEUE_CAPACITY];
	uint32_t head;
	uint32_t count;
	uint64_t nextSequence;
	uint64_t acceptedCount;
	uint64_t droppedCount;
} wiredEntityEventQueue_t;

uint32_t WiredEntityEvent_NameId( const char *name );
qboolean WiredEntityEvent_Valid( const wiredEntityEvent_t *event );
void WiredEntityEventQueue_Init( wiredEntityEventQueue_t *queue );
qboolean WiredEntityEventQueue_Enqueue( wiredEntityEventQueue_t *queue,
	const wiredEntityEvent_t *event );
qboolean WiredEntityEventQueue_Pop( wiredEntityEventQueue_t *queue,
	wiredEntityEvent_t *outEvent );

#ifdef __cplusplus
}
#endif

#endif
