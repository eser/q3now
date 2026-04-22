/*
===========================================================================
event.c — Event primitive stubs (V1)

DO NOT implement real dispatch here. Stop per the spec: no async dispatch,
no attributes, no per-subsystem categories in V1. Each function emits a
once-only debug log entry and returns a no-op value.
===========================================================================
*/
#include "q_shared.h"
#include "qcommon.h"
#include "event.h"

void Event_RegisterType( event_type_t type )
{
    static qboolean warned = qfalse;
    if ( !warned ) {
        Com_Printf( "Event_RegisterType: TODO V2\n" );
        warned = qtrue;
    }
    (void)type;
}

event_subscription_t *Event_Subscribe( event_type_t type,
                                        event_handler_fn handler,
                                        void *ctx )
{
    static qboolean warned = qfalse;
    if ( !warned ) {
        Com_Printf( "Event_Subscribe: TODO V2\n" );
        warned = qtrue;
    }
    (void)type; (void)handler; (void)ctx;
    return NULL;
}

void Event_Unsubscribe( event_subscription_t *sub )
{
    static qboolean warned = qfalse;
    if ( !warned ) {
        Com_Printf( "Event_Unsubscribe: TODO V2\n" );
        warned = qtrue;
    }
    (void)sub;
}

void Event_Emit( event_type_t type, const void *data, int data_size )
{
    static qboolean warned = qfalse;
    if ( !warned ) {
        Com_Printf( "Event_Emit: TODO V2\n" );
        warned = qtrue;
    }
    (void)type; (void)data; (void)data_size;
}

// =========================================================================
// Q3 event queue — extracted from common.c Phase 1 Group 2
// =========================================================================

#define MAX_PUSHED_EVENTS   256
static int          com_pushedEventsHead = 0;
static int          com_pushedEventsTail = 0;
static sysEvent_t   com_pushedEvents[ MAX_PUSHED_EVENTS ];

#define MAX_QUED_EVENTS     128
#define MASK_QUED_EVENTS    ( MAX_QUED_EVENTS - 1 )

static sysEvent_t           eventQue[ MAX_QUED_EVENTS ];
static sysEvent_t          *lastEvent = eventQue + MAX_QUED_EVENTS - 1;
static unsigned int         eventHead = 0;
static unsigned int         eventTail = 0;

static const char *Sys_EventName( sysEventType_t evType ) {
	static const char *evNames[ SE_MAX ] = {
		"SE_NONE",
		"SE_KEY",
		"SE_CHAR",
		"SE_MOUSE",
		"SE_JOYSTICK_AXIS",
		"SE_CONSOLE"
	};

	if ( (unsigned)evType >= ARRAY_LEN( evNames ) ) {
		return "SE_UNKNOWN";
	} else {
		return evNames[ evType ];
	}
}

void Sys_QueEvent( int evTime, sysEventType_t evType, int value, int value2, int ptrLength, void *ptr ) {
	sysEvent_t	*ev;

	if ( evTime == 0 ) {
		evTime = Sys_Milliseconds();
	}

	// try to combine all sequential mouse moves in one event
	if ( evType == SE_MOUSE && lastEvent->evType == SE_MOUSE && eventHead != eventTail ) {
		lastEvent->evValue += value;
		lastEvent->evValue2 += value2;
		lastEvent->evTime = evTime;
		return;
	}

	ev = &eventQue[ eventHead & MASK_QUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUED_EVENTS ) {
		Com_Printf( "%s(type=%s,keys=(%i,%i),time=%i): overflow\n", __func__, Sys_EventName( evType ), value, value2, evTime );
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	ev->evTime = evTime;
	ev->evType = evType;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;

	lastEvent = ev;
}

static sysEvent_t Com_GetSystemEvent( void ) {
	sysEvent_t  ev;
	const char	*s;
	int			evTime;

	// return if we have data
	if ( eventHead - eventTail > 0 )
		return eventQue[ ( eventTail++ ) & MASK_QUED_EVENTS ];

	Sys_SendKeyEvents();

	evTime = Sys_Milliseconds();

	// check for console commands
	s = Sys_ConsoleInput();
	if ( s )
	{
		char  *b;
		int   len;

		len = strlen( s ) + 1;
		b = Z_Malloc( len );
		strcpy( b, s );
		Sys_QueEvent( evTime, SE_CONSOLE, 0, 0, len, b );
	}

	// return if we have data
	if ( eventHead - eventTail > 0 )
		return eventQue[ ( eventTail++ ) & MASK_QUED_EVENTS ];

	// create an empty event to return
	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = evTime;

	return ev;
}

static sysEvent_t Com_GetRealEvent( void ) {

	// get or save an event from/to the journal file
	if ( com_journalFile != FS_INVALID_HANDLE ) {
		int			r;
		sysEvent_t	ev;

		if ( com_journal->integer == 2 ) {
			Sys_SendKeyEvents();
			r = FS_Read( &ev, sizeof(ev), com_journalFile );
			if ( r != sizeof(ev) ) {
				Com_Error( ERR_FATAL, "Error reading from journal file" );
			}
			if ( ev.evPtrLength ) {
				ev.evPtr = Z_Malloc( ev.evPtrLength );
				r = FS_Read( ev.evPtr, ev.evPtrLength, com_journalFile );
				if ( r != ev.evPtrLength ) {
					Com_Error( ERR_FATAL, "Error reading from journal file" );
				}
			}
		} else {
			ev = Com_GetSystemEvent();

			// write the journal value out if needed
			if ( com_journal->integer == 1 ) {
				r = FS_Write( &ev, sizeof(ev), com_journalFile );
				if ( r != sizeof(ev) ) {
					Com_Error( ERR_FATAL, "Error writing to journal file" );
				}
				if ( ev.evPtrLength ) {
					r = FS_Write( ev.evPtr, ev.evPtrLength, com_journalFile );
					if ( r != ev.evPtrLength ) {
						Com_Error( ERR_FATAL, "Error writing to journal file" );
					}
				}
			}
		}

		return ev;
	}

	return Com_GetSystemEvent();
}

void Com_InitPushEvent( void ) {
	memset( com_pushedEvents, 0, sizeof(com_pushedEvents) );
	com_pushedEventsHead = 0;
	com_pushedEventsTail = 0;
}

static void Com_PushEvent( const sysEvent_t *event ) {
	sysEvent_t		*ev;
	static int printedWarning = 0;

	ev = &com_pushedEvents[ com_pushedEventsHead & (MAX_PUSHED_EVENTS-1) ];

	if ( com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS ) {

		// don't print the warning constantly, or it can give time for more...
		if ( !printedWarning ) {
			printedWarning = qtrue;
			Com_Printf( "WARNING: Com_PushEvent overflow\n" );
		}

		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		com_pushedEventsTail++;
	} else {
		printedWarning = qfalse;
	}

	*ev = *event;
	com_pushedEventsHead++;
}

static sysEvent_t Com_GetEvent( void ) {
	if ( com_pushedEventsHead - com_pushedEventsTail > 0 ) {
		return com_pushedEvents[ (com_pushedEventsTail++) & (MAX_PUSHED_EVENTS-1) ];
	}
	return Com_GetRealEvent();
}

void Com_RunAndTimeServerPacket( const netadr_t *evFrom, msg_t *buf ) {
	int		t1, t2, msec;

	t1 = 0;

	if ( com_speeds->integer ) {
		t1 = Sys_Milliseconds ();
	}

	SV_PacketEvent( evFrom, buf );

	if ( com_speeds->integer ) {
		t2 = Sys_Milliseconds ();
		msec = t2 - t1;
		(void)msec;
	}
}

int Com_EventLoop( void ) {
	sysEvent_t	ev;

#ifndef DEDICATED
	byte		bufData[ MAX_MSGLEN_BUF ];
	msg_t		buf;

	MSG_Init( &buf, bufData, MAX_MSGLEN );
#endif // !DEDICATED

	while ( 1 ) {
		ev = Com_GetEvent();

		// if no more events are available
		if ( ev.evType == SE_NONE ) {
			// manually send packet events for the loopback channel
#ifndef DEDICATED
			netadr_t evFrom;
			while ( NET_GetLoopPacket( NS_CLIENT, &evFrom, &buf ) ) {
				CL_PacketEvent( &evFrom, &buf );
			}
			while ( NET_GetLoopPacket( NS_SERVER, &evFrom, &buf ) ) {
				// if the server just shut down, flush the events
				if ( com_sv_running->integer ) {
					Com_RunAndTimeServerPacket( &evFrom, &buf );
				}
			}
#endif // !DEDICATED
			return ev.evTime;
		}

		switch ( ev.evType ) {
#ifndef DEDICATED
		case SE_KEY:
			CL_KeyEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
		case SE_CHAR:
			CL_CharEvent( ev.evValue );
			break;
		case SE_MOUSE:
			CL_MouseEvent( ev.evValue, ev.evValue2 );
			break;
		case SE_JOYSTICK_AXIS:
			CL_JoystickEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
#endif // !DEDICATED
		case SE_CONSOLE:
			Cbuf_AddText( (char *)ev.evPtr );
			Cbuf_AddText( "\n" );
			break;
		default:
				Com_Error( ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evType );
			break;
		}

		// free any block data
		if ( ev.evPtr ) {
			Z_Free( ev.evPtr );
			ev.evPtr = NULL;
		}
	}

	return 0;	// never reached
}

int Com_Milliseconds( void ) {

	sysEvent_t	ev;

	// get events and push them until we get a null event with the current time
	do {
		ev = Com_GetRealEvent();
		if ( ev.evType != SE_NONE ) {
			Com_PushEvent( &ev );
		}
	} while ( ev.evType != SE_NONE );

	return ev.evTime;
}
