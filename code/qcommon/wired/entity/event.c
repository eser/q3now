// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "event.h"

#include <math.h>
#include <string.h>

#define FNV32_OFFSET UINT32_C(2166136261)
#define FNV32_PRIME UINT32_C(16777619)

static qboolean Terminated( const char *text, uint32_t capacity )
{
	uint32_t index;
	for ( index = 0u; index < capacity; ++index )
		if ( text[index] == '\0' ) return qtrue;
	return qfalse;
}

uint32_t WiredEntityEvent_NameId( const char *name )
{
	uint32_t hash = FNV32_OFFSET;
	if ( !name || !name[0] ) return 0u;
	while ( *name ) {
		unsigned char value = (unsigned char)*name++;
		if ( value >= 'A' && value <= 'Z' ) value = (unsigned char)( value - 'A' + 'a' );
		hash = ( hash ^ value ) * FNV32_PRIME;
	}
	return hash ? hash : 1u;
}

qboolean WiredEntityEvent_Valid( const wiredEntityEvent_t *event )
{
	if ( !event || event->schemaVersion != WIRED_ENTITY_EVENT_SCHEMA_VERSION ||
		!Terminated( event->name, WIRED_ENTITY_EVENT_NAME_CAPACITY ) ||
		!Terminated( event->textValue, WIRED_ENTITY_EVENT_TEXT_CAPACITY ) ||
		!event->stableEventId || event->stableEventId != WiredEntityEvent_NameId( event->name ) ||
		event->entityNum < 0 || event->sourceEntityNum < -1 ||
		event->valueType < WIRED_ENTITY_EVENT_VALUE_NONE ||
		event->valueType > WIRED_ENTITY_EVENT_VALUE_VEC3 || event->ready != qtrue )
		return qfalse;
	if ( event->valueType == WIRED_ENTITY_EVENT_VALUE_NONE )
		return !event->stableFieldId && !event->intValue && event->floatValue == 0.0f &&
			event->vectorValue[0] == 0.0f && event->vectorValue[1] == 0.0f &&
			event->vectorValue[2] == 0.0f && !event->textValue[0];
	if ( event->valueType == WIRED_ENTITY_EVENT_VALUE_FLOAT )
		return isfinite( event->floatValue ) ? qtrue : qfalse;
	if ( event->valueType == WIRED_ENTITY_EVENT_VALUE_STRING )
		return event->textValue[0] ? qtrue : qfalse;
	if ( event->valueType == WIRED_ENTITY_EVENT_VALUE_VEC3 )
		return isfinite( event->vectorValue[0] ) && isfinite( event->vectorValue[1] ) &&
			isfinite( event->vectorValue[2] ) ? qtrue : qfalse;
	return qtrue;
}

void WiredEntityEventQueue_Init( wiredEntityEventQueue_t *queue )
{
	if ( !queue ) return;
	memset( queue, 0, sizeof( *queue ) );
	queue->schemaVersion = WIRED_ENTITY_EVENT_QUEUE_SCHEMA_VERSION;
	queue->nextSequence = 1u;
}

qboolean WiredEntityEventQueue_Enqueue( wiredEntityEventQueue_t *queue,
	const wiredEntityEvent_t *event )
{
	wiredEntityEvent_t stored;
	uint32_t tail;
	if ( !queue || queue->schemaVersion != WIRED_ENTITY_EVENT_QUEUE_SCHEMA_VERSION ||
		!queue->nextSequence || !WiredEntityEvent_Valid( event ) ) return qfalse;
	if ( queue->count >= WIRED_ENTITY_EVENT_QUEUE_CAPACITY ) {
		queue->droppedCount++;
		return qfalse;
	}
	stored = *event;
	stored.sequence = queue->nextSequence++;
	if ( !queue->nextSequence ) queue->nextSequence = 1u;
	tail = ( queue->head + queue->count ) % WIRED_ENTITY_EVENT_QUEUE_CAPACITY;
	queue->events[tail] = stored;
	queue->count++;
	queue->acceptedCount++;
	return qtrue;
}

qboolean WiredEntityEventQueue_Pop( wiredEntityEventQueue_t *queue,
	wiredEntityEvent_t *outEvent )
{
	wiredEntityEvent_t event;
	if ( !queue || !outEvent || queue->schemaVersion != WIRED_ENTITY_EVENT_QUEUE_SCHEMA_VERSION ||
		!queue->count ) return qfalse;
	event = queue->events[queue->head];
	if ( !WiredEntityEvent_Valid( &event ) || !event.sequence ) return qfalse;
	memset( &queue->events[queue->head], 0, sizeof( queue->events[queue->head] ) );
	queue->head = ( queue->head + 1u ) % WIRED_ENTITY_EVENT_QUEUE_CAPACITY;
	queue->count--;
	*outEvent = event;
	return qtrue;
}
