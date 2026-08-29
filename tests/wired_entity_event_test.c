// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "wired/entity/event.h"

#include <stdio.h>
#include <string.h>

#define CHECK( x ) do { if ( !(x) ) { fprintf( stderr, "FAIL %d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

static wiredEntityEvent_t Event( const char *name, int entityNum, int value )
{
	wiredEntityEvent_t event;
	memset( &event, 0, sizeof( event ) );
	event.schemaVersion = WIRED_ENTITY_EVENT_SCHEMA_VERSION;
	event.stableEventId = WiredEntityEvent_NameId( name );
	event.entityNum = entityNum;
	event.sourceEntityNum = -1;
	event.valueType = WIRED_ENTITY_EVENT_VALUE_INT;
	event.intValue = value;
	strcpy( event.name, name );
	event.ready = qtrue;
	return event;
}

int main( void )
{
	wiredEntityEventQueue_t queue;
	wiredEntityEvent_t event, output, before;
	uint32_t index;
	CHECK( WiredEntityEvent_NameId( "Pain" ) == WiredEntityEvent_NameId( "pain" ) );
	WiredEntityEventQueue_Init( &queue );
	event = Event( "pain", 7, 10 );
	CHECK( WiredEntityEventQueue_Enqueue( &queue, &event ) );
	event = Event( "death", 7, 20 );
	CHECK( WiredEntityEventQueue_Enqueue( &queue, &event ) );
	CHECK( WiredEntityEventQueue_Pop( &queue, &output ) && output.sequence == 1u &&
		!strcmp( output.name, "pain" ) && output.intValue == 10 );
	CHECK( WiredEntityEventQueue_Pop( &queue, &output ) && output.sequence == 2u &&
		!strcmp( output.name, "death" ) );
	before = output;
	CHECK( !WiredEntityEventQueue_Pop( &queue, &output ) && !memcmp( &before, &output, sizeof( output ) ) );
	event = Event( "bad", 1, 1 ); event.stableEventId++;
	CHECK( !WiredEntityEventQueue_Enqueue( &queue, &event ) );
	for ( index = 0u; index < WIRED_ENTITY_EVENT_QUEUE_CAPACITY; ++index ) {
		event = Event( "tick", (int)index, (int)index );
		CHECK( WiredEntityEventQueue_Enqueue( &queue, &event ) );
	}
	event = Event( "overflow", 1, 1 );
	CHECK( !WiredEntityEventQueue_Enqueue( &queue, &event ) && queue.droppedCount == 1u &&
		queue.count == WIRED_ENTITY_EVENT_QUEUE_CAPACITY );
	for ( index = 0u; index < WIRED_ENTITY_EVENT_QUEUE_CAPACITY; ++index )
		CHECK( WiredEntityEventQueue_Pop( &queue, &output ) && output.intValue == (int)index );
	puts( "wired_entity_event_test: ok" );
	return 0;
}
