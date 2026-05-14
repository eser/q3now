// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// common.c -- misc functions used in client and server

#include "q_shared.h"
#include "qcommon.h"
#include "wired/core/logging/log.h"
#include "maps/meta.h"
#include "crash.h"
#include <setjmp.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/stat.h> // umask
#include <sys/time.h>
#include <time.h>       // clock_gettime(CLOCK_MONOTONIC) for Sys_NanoTime
#else
#include <winsock.h>
#if defined(_DEBUG)
#include "../win32/win_local.h"
#endif
#endif

#include "../client/keys.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_system, "system" );

const int demo_protocols[] = { PROTOCOL_VERSION, 0 };

#define USE_MULTI_SEGMENT // allocate additional zone segments on demand

#define MIN_COMHUNKMEGS		128
#define DEF_COMHUNKMEGS		512
#define DEF_COMZONEMEGS		48

jmp_buf abortframe;	// an ERR_DROP occurred, exit the entire frame

int		CPU_Flags = 0;

fileHandle_t com_journalFile = FS_INVALID_HANDLE ; // events are written here
fileHandle_t com_journalDataFile = FS_INVALID_HANDLE; // config files are written here

cvar_t	*com_viewlog;
cvar_t	*com_speeds;
cvar_t	*com_dedicated;
cvar_t	*com_timescale;
static cvar_t *com_fixedtime;
cvar_t	*com_journal;
cvar_t	*com_protocol;
cvar_t	*com_busyWait;
#ifndef DEDICATED
cvar_t	*com_maxfps;
cvar_t	*com_maxfpsUnfocused;
cvar_t	*com_maxfpsMinimized;
cvar_t	*com_yieldCPU;
cvar_t	*com_timedemo;
#endif
#ifdef USE_AFFINITY_MASK
cvar_t	*com_affinityMask;
#endif
static cvar_t *com_showtrace;
cvar_t	*com_version;

#ifndef DEDICATED
static cvar_t	*com_introPlayed;
cvar_t	*com_skipIdLogo;

cvar_t	*cl_paused;
cvar_t	*cl_packetdelay;
cvar_t	*com_cl_running;
#endif

cvar_t	*sv_paused;
cvar_t  *sv_packetdelay;
cvar_t	*com_sv_running;

cvar_t	*com_cameraMode;
#if defined(_WIN32) && defined(_DEBUG)
cvar_t	*com_noErrorInterrupt;
#endif

// com_speeds times
int		time_game;
int		time_frontend;		// renderer frontend time
int		time_backend;		// renderer backend time
clProfile_t	cl_prof;

static int		lastTime;
static int64_t	lastTimeUsec;
int				com_frameTime;
int64_t			com_frameTimeUsec;
static int	com_frameNumber;

qboolean	com_errorEntered = qfalse;
qboolean	com_fullyInitialized = qfalse;

// renderer window states
qboolean	gw_minimized = qfalse; // this will be always true for dedicated servers
#ifndef DEDICATED
qboolean	gw_active = qtrue;
#endif

char com_errorMessage[ MAXPRINTMSG ];

void Com_Shutdown( void );
void CIN_CloseAllVideos( void );
static void Com_ViewlogChanged( cvar_t *self );
static void Com_DedicatedChanged( cvar_t *self );

//============================================================================


/*
=============
Com_Quit_f

Both client and server can use this, and it will
do the appropriate things.
=============
*/
void Com_Quit_f( void ) {
	const char *p = Cmd_ArgsFrom( 1 );
	// don't try to shutdown if we are in a recursive error
	if ( !com_errorEntered ) {
		// Some VMs might execute "quit" command directly,
		// which would trigger an unload of active VM error.
		// Sys_Quit will kill this process anyways, so
		// a corrupt call stack makes no difference
		VM_Forced_Unload_Start();
		SV_Shutdown( p[0] ? p : "Server quit" );
#ifndef DEDICATED
		CL_Shutdown( p[0] ? p : "Client quit", qtrue );
#endif
		VM_Forced_Unload_Done();
		Com_Shutdown();
		FS_Shutdown( qtrue );
	}
	Sys_Quit();
}


/*
============================================================================

COMMAND LINE FUNCTIONS

+ characters separate the commandLine string into multiple console
command lines.

All of these are valid:

quake3 +set test blah +map test
quake3 set test blah+map test
quake3 set test blah + map test

============================================================================
*/

#define	MAX_CONSOLE_LINES	32
static int	com_numConsoleLines;
static char	*com_consoleLines[MAX_CONSOLE_LINES];

/*
==================
Com_ParseCommandLine

Break it up into multiple console lines
==================
*/
static void Com_ParseCommandLine( char *commandLine ) {
	static int parsed = 0;

	if ( parsed )
		return;

	int inq = 0;
	com_consoleLines[0] = commandLine;

	while ( *commandLine ) {
		if (*commandLine == '"') {
			inq = !inq;
		}
		// look for a + separating character
		// if commandLine came from a file, we might have real line separators
		if ( (*commandLine == '+' && !inq) || *commandLine == '\n'  || *commandLine == '\r' ) {
			if ( com_numConsoleLines == MAX_CONSOLE_LINES ) {
				break;
			}
			com_consoleLines[com_numConsoleLines] = commandLine + 1;
			com_numConsoleLines++;
			*commandLine = '\0';
		}
		commandLine++;
	}
	parsed = 1;
}

char cl_title[ MAX_CVAR_VALUE_STRING ] = CLIENT_WINDOW_TITLE;

/*
===================
Com_EarlyParseCmdLine

returns qtrue if both vid_xpos and vid_ypos was set
===================
*/
qboolean Com_EarlyParseCmdLine( char *commandLine, char *con_title, int title_size, int *vid_xpos, int *vid_ypos )
{
	int		flags = 0;

	*con_title = '\0';
	Com_ParseCommandLine( commandLine );

	for ( int i = 0 ; i < com_numConsoleLines ; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( !Q_stricmpn( Cmd_Argv(0), "set", 3 ) && !Q_stricmp( Cmd_Argv(1), "cl_title" ) ) {
			com_consoleLines[i][0] = '\0';
			Q_strncpyz( cl_title, Cmd_ArgsFrom( 2 ), sizeof(cl_title) );
			continue;
		}
		if ( !Q_stricmp( Cmd_Argv(0), "cl_title" ) ) {
			com_consoleLines[i][0] = '\0';
			Q_strncpyz( cl_title, Cmd_ArgsFrom( 1 ), sizeof(cl_title) );
			continue;
		}
		if ( !Q_stricmpn( Cmd_Argv(0), "set", 3 ) && !Q_stricmp( Cmd_Argv(1), "con_title" ) ) {
			com_consoleLines[i][0] = '\0';
			Q_strncpyz( con_title, Cmd_ArgsFrom( 2 ), title_size );
			continue;
		}
		if ( !Q_stricmp( Cmd_Argv(0), "con_title" ) ) {
			com_consoleLines[i][0] = '\0';
			Q_strncpyz( con_title, Cmd_ArgsFrom( 1 ), title_size );
			continue;
		}
		if ( !Q_stricmpn( Cmd_Argv(0), "set", 3 ) && !Q_stricmp( Cmd_Argv(1), "vid_xpos" ) ) {
			*vid_xpos = atoi( Cmd_Argv( 2 ) );
			flags |= 1;
			continue;
		}
		if ( !Q_stricmp( Cmd_Argv(0), "vid_xpos" ) ) {
			*vid_xpos = atoi( Cmd_Argv( 1 ) );
			flags |= 1;
			continue;
		}
		if ( !Q_stricmpn( Cmd_Argv(0), "set", 3 ) && !Q_stricmp( Cmd_Argv(1), "vid_ypos" ) ) {
			*vid_ypos = atoi( Cmd_Argv( 2 ) );
			flags |= 2;
			continue;
		}
		if ( !Q_stricmp( Cmd_Argv(0), "vid_ypos" ) ) {
			*vid_ypos = atoi( Cmd_Argv( 1 ) );
			flags |= 2;
			continue;
		}
	}

	return (flags == 3) ? qtrue : qfalse ;
}


/*
===================
Com_SafeMode

Check for "safe" on the command line, which will
skip loading of config.cfg
===================
*/
qboolean Com_SafeMode( void ) {
	for ( int i = 0 ; i < com_numConsoleLines ; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( !Q_stricmp( Cmd_Argv(0), "safe" )
			|| !Q_stricmp( Cmd_Argv(0), "cvar_restart" ) ) {
			com_consoleLines[i][0] = '\0';
			return qtrue;
		}
	}
	return qfalse;
}


/*
===============
Com_StartupVariable

Searches for command line parameters that are set commands.
If match is not NULL, only that cvar will be looked for.
That is necessary because cddir and basedir need to be set
before the filesystem is started, but all other sets should
be after execing the config and default.
===============
*/
void Com_StartupVariable( const char *match ) {
	const char *name;

	for ( int i = 0; i < com_numConsoleLines; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( Q_stricmp( Cmd_Argv( 0 ), "set" ) ) {
			continue;
		}

		name = Cmd_Argv( 1 );
		if ( !match || Q_stricmp( name, match ) == 0 ) {
			if ( Cvar_Flags( name ) == CVAR_NONEXISTENT ) {
				Cvar_Get( name, Cmd_ArgsFrom( 2 ), CVAR_USER_CREATED | CVAR_CMDLINE_CREATED );
			} else {
				cvar_t *cv;
				Cvar_Set2( name, Cmd_ArgsFrom( 2 ), qfalse );
				cv = Cvar_Get( name, "", 0 );
				if ( cv != NULL ) {
					cv->flags |= CVAR_CMDLINE_CREATED;
				}
			}
		}
	}
}


/*
=================
Com_AddStartupCommands

Adds command line parameters as script statements
Commands are separated by + signs

Returns qtrue if any late commands were added, which
will keep the demoloop from immediately starting
=================
*/
static qboolean Com_AddStartupCommands( void ) {
	qboolean	added;

	added = qfalse;
	// quote every token, so args with semicolons can work
	for (int i=0 ; i < com_numConsoleLines ; i++) {
		if ( !com_consoleLines[i] || !com_consoleLines[i][0] ) {
			continue;
		}

		// set commands already added with Com_StartupVariable
		if ( !Q_stricmpn( com_consoleLines[i], "set ", 4 ) ) {
			continue;
		}

		added = qtrue;
		Cbuf_AddText( com_consoleLines[i] );
		Cbuf_AddText( "\n" );
	}

	return added;
}


//============================================================================

void Info_Print( const char *s ) {
	char	key[BIG_INFO_KEY];
	char	value[BIG_INFO_VALUE];

	do {
		s = Info_NextPair( s, key, value );
		if ( key[0] == '\0' )
			break;

		if ( value[0] == '\0' )
			strcpy( value, "MISSING VALUE" );

		Com_Log( SEV_INFO, LOG_CH(ch_system), "%-20s %s\n", key, value );

	} while ( *s != '\0' );
}




/* Sys_Microseconds, Sys_NanoTime, Sys_Milliseconds →
   wired/core/time/time.c */
/* Com_RealTime, Com_RealTimeMs, Com_FormatTimestamp →
   wired/core/time/time.c */


/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

#define	ZONE_ID		0x1d4a11
#define TRASH_ID	(ZONE_ID + 1)

#define MINFRAGMENT		64

#define BUCKET_COUNT	4
#define BUCKET_SIZE		64

#define USE_STATIC_TAGS
#ifdef _DEBUG
#define USE_TRASH_TEST
#define USE_ZONE_ID
#endif

#ifdef ZONE_DEBUG
typedef struct zonedebug_s {
	const char *label;
	const char *file;
	int line;
	int allocSize;
} zonedebug_t;
#endif

typedef struct memblock_s {
	struct memblock_s	*next, *prev;
	uint32_t	size;	// including the header and possibly tiny fragments, if 0 then it is a zone separator thus can't be released/merged
	memtag_t	tag;	// a tag of 0 is a free block
#ifdef USE_ZONE_ID
	int			id;		// should be ZONE_ID
#endif
#ifdef ZONE_DEBUG
	zonedebug_t d;
#endif
} memblock_t;

typedef struct freeblock_s {
	struct freeblock_s *prev;
	struct freeblock_s *next;
} freeblock_t;

typedef struct memzone_s {
	size_t		size;		// total bytes malloced, including header
	size_t		used;		// total bytes used
	memblock_t	blocklist;	// start / end cap for linked list
#ifdef USE_MULTI_SEGMENT
	struct {
		memblock_t	filler;	// just to allocate some space before freelist
		freeblock_t head;
	} bucket[BUCKET_COUNT];
#else
	memblock_t	*rover;
#endif
	const char *name;
} memzone_t;

static int minfragment = MINFRAGMENT; // may be adjusted at runtime

// main zone for all "dynamic" memory allocation
static memzone_t *mainzone;

// we also have a small zone for small allocations that would only
// fragment the main zone (think of cvar and cmd strings)
static memzone_t *smallzone;


#ifdef USE_MULTI_SEGMENT
static int GetBucketIndex( const memzone_t *zone, uint32_t size )
{
	const int index = size / BUCKET_SIZE;
	return index > (BUCKET_COUNT - 1) ? BUCKET_COUNT - 1 : index;
}


static void InsertFree( memzone_t *zone, memblock_t *block )
{
	freeblock_t *fb = (freeblock_t *)(block + 1);
	freeblock_t *prev, *next;
	const int index = GetBucketIndex( zone, block->size );
	prev = &zone->bucket[ index ].head;

	next = prev->next;

#ifdef ZONE_DEBUG
	if ( block->size < sizeof( *fb ) + sizeof( *block ) ) {
		Com_Terminate( TERM_UNRECOVERABLE, "InsertFree: bad block size: %i\n", block->size );
	}
#endif

	prev->next = fb;
	next->prev = fb;

	fb->prev = prev;
	fb->next = next;
}


static void RemoveFree( memblock_t *block )
{
	freeblock_t *fb = (freeblock_t *)(block + 1);
	freeblock_t *prev;
	freeblock_t *next;

#ifdef ZONE_DEBUG
	if ( fb->next == NULL || fb->prev == NULL || fb->next == fb || fb->prev == fb ) {
		Com_Terminate( TERM_UNRECOVERABLE, "RemoveFree: bad pointers fb->next: %p, fb->prev: %p\n", fb->next, fb->prev );
	}
#endif

	prev = fb->prev;
	next = fb->next;

	prev->next = next;
	next->prev = prev;
}


static memblock_t *SplitBlock( memblock_t *base, size_t base_size, size_t fragment_size )
{
	memblock_t *fragment = (memblock_t *)((unsigned char *)base + base_size);

	fragment->size = fragment_size;
	fragment->prev = base;
	fragment->next = base->next;
	fragment->next->prev = fragment;

	base->next = fragment;
	base->size = base_size;

	return fragment;
}


/*
================
NewBlock

Allocates new free block within specified memory zone

Separator is needed to avoid additional runtime checks in Z_Free()
to prevent merging it with previous free block
================
*/
static memblock_t *NewBlock( memzone_t *zone, uint32_t size )
{
	memblock_t *prev, *next;
	memblock_t *block, *sep;
	uint32_t alloc_size;

	// zone->prev is pointing on last block in the list
	prev = zone->blocklist.prev;
	next = prev->next;

	size = PAD( size, 1U << 21 ); // round up to 2M blocks
	// allocate separator block before new free block
	alloc_size = size + sizeof( *sep );

	//sep = (memblock_t *)calloc( alloc_size, 1 );
	sep = (memblock_t *)malloc( alloc_size );
	if ( sep == NULL ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Z_Malloc: failed on allocation of %u bytes from the %s zone",
			size, zone->name );
		return NULL;
	}
	memset( sep, 0x0, sizeof( *sep ) + sizeof( *block ) );
	block = sep + 1;

	// link separator with prev
	prev->next = sep;
	sep->prev = prev;

	// link separator with block
	sep->next = block;
	block->prev = sep;

	// link block with next
	block->next = next;
	next->prev = block;

	sep->tag = TAG_GENERAL; // in-use block
	sep->size = 0;			// 0 = segment separator

	block->tag = TAG_FREE;
	block->size = size;

#ifdef USE_ZONE_ID
	sep->id = -ZONE_ID;
	block->id = ZONE_ID;
#endif

	// update zone statistics
	zone->size += alloc_size;
	zone->used += sizeof( *sep );

	InsertFree( zone, block );

	return block;
}


static memblock_t *SearchFree( memzone_t *zone, uint32_t size )
{
	const int index = GetBucketIndex( zone, size );
	const freeblock_t *fb = zone->bucket[ index ].head.next;
	const freeblock_t *fh = &zone->bucket[ 0 ].head;

	for ( ;; ) {
		memblock_t *base;
		if ( fb == fh ) {
			return NewBlock( zone, size );
		}
		base = (memblock_t *)((byte *)fb - sizeof( *base ));
		fb = fb->next;
		if ( base->size >= size ) {
			return base;
		}
	}
	return NULL;
}
#endif // USE_MULTI_SEGMENT


/*
========================
Z_Init
========================
*/
static void Z_Init( memzone_t *zone, uint32_t size, const char *name )
{
	memblock_t *block;
	int i, n, min_fragment;

	memset( zone, 0x0, sizeof( *zone ) + sizeof( *block ) );

	zone->name = name;

#ifdef USE_MULTI_SEGMENT
	min_fragment = sizeof( memblock_t ) + sizeof( freeblock_t );
#else
	min_fragment = sizeof( memblock_t );
#endif

	if ( minfragment < min_fragment ) {
		// in debug mode size of memblock_t may exceed MINFRAGMENT
		minfragment = PAD( min_fragment, sizeof( intptr_t ) );
	}

	// set the entire zone to one free block
	zone->blocklist.next = zone->blocklist.prev = block = (memblock_t *)(zone + 1);
	zone->blocklist.tag = TAG_GENERAL; // in use block
	// zone->blocklist.size = 0;
	zone->size = size;
	// zone->used = 0;
#ifndef USE_MULTI_SEGMENT
	zone->rover = block;
#endif

	block->prev = block->next = &zone->blocklist;
	block->size = size - sizeof( *zone );
	block->tag = TAG_FREE;

#ifdef USE_ZONE_ID
	zone->blocklist.id = -ZONE_ID;
	block->id = ZONE_ID;
#endif

#ifdef USE_MULTI_SEGMENT
	n = ARRAY_LEN( zone->bucket );

	for ( i = 0; i < n; i++ ) {
		zone->bucket[i].head.next = &zone->bucket[(i + 1) % n].head;
		zone->bucket[i].head.prev = &zone->bucket[(i + n - 1) % n].head;
		// zone->bucket[i].filler.size = 0;
		zone->bucket[i].filler.tag = TAG_GENERAL;
#ifdef USE_ZONE_ID
		zone->bucket[i].filler.id = ZONE_ID;
#endif
	}

	InsertFree( zone, block );
#endif // USE_MULTI_SEGMENT
}


/*
========================
Z_AvailableZoneMemory
========================
*/
static int Z_AvailableZoneMemory( const memzone_t *zone )
{
#ifdef USE_MULTI_SEGMENT
	return (1024*1024*1024); // unlimited
#else
	return zone->size - zone->used;
#endif
}


/*
========================
Z_AvailableMemory
========================
*/
int Z_AvailableMemory( void )
{
	return Z_AvailableZoneMemory( mainzone );
}


static void MergeBlock( memblock_t *curr_free, const memblock_t *next )
{
	curr_free->size += next->size;
	curr_free->next = next->next;
	curr_free->next->prev = curr_free;
}


#if FEAT_MEMSTATS
static memTag_t ZoneTagToMemTag( memtag_t tag );
#endif

/*
========================
Z_Free
========================
*/
void Z_Free( void *ptr )
{
	if ( ptr == NULL ) {
#ifdef _DEBUG
		Com_Terminate( TERM_CLIENT_DROP, "Z_Free: NULL pointer" );
#else
		return;
#endif
	}

	memblock_t *block = (memblock_t *)((byte *)ptr - sizeof( memblock_t ));

#ifdef USE_ZONE_ID
	if ( block->id != ZONE_ID ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Z_Free: freed a pointer without ZONEID" );
	}
#endif

	if ( block->tag == TAG_FREE ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Z_Free: freed a freed pointer" );
	}

#ifdef USE_STATIC_TAGS
	if ( block->tag == TAG_STATIC ) {
		return;
	}
#endif

	// check the memory trash tester
#ifdef USE_TRASH_TEST
	if ( *(int *)((byte *)block + block->size - 4) != TRASH_ID ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Z_Free: memory block wrote past end" );
	}
#endif

	memzone_t *zone = ( block->tag == TAG_SMALL || block->tag == TAG_SOUND ) ? smallzone : mainzone;

	zone->used -= block->size;

#if FEAT_MEMSTATS
	MemStats_Free( ZoneTagToMemTag( block->tag ), (int)block->size );
#endif

	// set the block to something that should cause problems
	// if it is referenced...
#ifdef ZONE_DEBUG
	memset( ptr, 0xaa, block->size - sizeof( *block ) );
#endif

	block->tag = TAG_FREE; // mark as free
#ifdef USE_ZONE_ID
	block->id = ZONE_ID;
#endif

	memblock_t *other = block->prev;
	if ( other->tag == TAG_FREE ) {
#ifdef USE_MULTI_SEGMENT
		RemoveFree( other );
#endif
		// merge with previous free block
		MergeBlock( other, block );
#ifndef USE_MULTI_SEGMENT
		if ( block == zone->rover ) {
			zone->rover = other;
		}
#endif
		block = other;
	}

#ifndef USE_MULTI_SEGMENT
	zone->rover = block;
#endif

	other = block->next;
	if ( other->tag == TAG_FREE ) {
#ifdef USE_MULTI_SEGMENT
		RemoveFree( other );
#endif
		// merge the next free block onto the end
		MergeBlock( block, other );
	}

#ifdef USE_MULTI_SEGMENT
	InsertFree( zone, block );
#endif
}


/*
================
Z_FreeTags
================
*/
int Z_FreeTags( memtag_t tag )
{
	if ( tag == TAG_STATIC ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Z_FreeTags( TAG_STATIC )" );
		return 0;
	}
	memzone_t *zone = (tag == TAG_SMALL) ? smallzone : mainzone;
	int count = 0;
	for ( memblock_t *block = zone->blocklist.next; ; ) {
#ifdef USE_ZONE_ID
		if ( block->tag == tag && block->id == ZONE_ID ) {
#else
		if ( block->tag == tag && block->size != 0 ) {
#endif
			memblock_t *freed;
			if ( block->prev->tag == TAG_FREE )
				freed = block->prev;  // current block will be merged with previous
			else
				freed = block; // will leave in place
			Z_Free( (void *)(block + 1) );
			block = freed;
			count++;
		}
		if ( block->next == &zone->blocklist ) {
			break;	// all blocks have been hit
		}
		block = block->next;
	}

	return count;
}


/*
================
Z_TagMalloc
================
*/
#ifdef ZONE_DEBUG
void *Z_TagMallocDebug( size_t size, memtag_t tag, const char *label, const char *file, int line ) {
	size_t		allocSize;
#else
void *Z_TagMalloc( size_t size, memtag_t tag ) {
#endif
#ifndef USE_MULTI_SEGMENT
	memblock_t	*start, *rover;
#endif
	memblock_t *base;

	if ( tag == TAG_FREE ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Z_TagMalloc: tried to use with TAG_FREE" );
	}

	if ( size > INT_MAX ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Z_TagMalloc: %"PRIz"u > INT_MAX", size );
	}

	memzone_t *zone = ( tag == TAG_SMALL || tag == TAG_SOUND ) ? smallzone : mainzone;

#ifdef ZONE_DEBUG
	allocSize = size;
#endif

#ifdef USE_MULTI_SEGMENT
	if ( size < (sizeof( freeblock_t ) ) ) {
		size = (sizeof( freeblock_t ) );
	}
#endif

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof( *base );	// account for size of block header
#ifdef USE_TRASH_TEST
	size += 4;					// space for memory trash tester
#endif

	size = PAD( size, sizeof( intptr_t ) );		// align to 32/64 bit boundary

#ifdef USE_MULTI_SEGMENT
	base = SearchFree( zone, size );

	RemoveFree( base );
#else
	base = rover = zone->rover;
	start = base->prev;

	do {
		if ( rover == start ) {
			// scanned all the way around the list
#ifdef ZONE_DEBUG
			//Z_LogHeap();
			Com_Terminate( TERM_UNRECOVERABLE, "Z_Malloc: failed on allocation of %"PRIz"u bytes from the %s zone: %s, line: %d (%s)",
								size, zone->name, file, line, label );
#else
			Com_Terminate( TERM_UNRECOVERABLE, "Z_Malloc: failed on allocation of %"PRIz"u bytes from the %s zone",
								size, zone->name );
#endif
			return NULL;
		}
		if ( rover->tag != TAG_FREE ) {
			base = rover = rover->next;
		} else {
			rover = rover->next;
		}
	} while ( base->tag != TAG_FREE || base->size < size );
#endif

	//
	// found a block big enough
	//
	size_t extra = base->size - size;
	if ( extra >= minfragment ) {
		memblock_t *fragment = SplitBlock( base, size, extra );
#ifdef USE_MULTI_SEGMENT
		InsertFree( zone, fragment );
#endif
		fragment->tag = TAG_FREE;
#ifdef USE_ZONE_ID
		fragment->id = ZONE_ID;
#endif
	}

#ifndef USE_MULTI_SEGMENT
	zone->rover = base->next;	// next allocation will start looking here
#endif
	zone->used += base->size;

	base->tag = tag;			// no longer a free block
#ifdef USE_ZONE_ID
	base->id = ZONE_ID;
#endif

#ifdef ZONE_DEBUG
	base->d.label = label;
	base->d.file = file;
	base->d.line = line;
	base->d.allocSize = allocSize;
#endif

#ifdef USE_TRASH_TEST
	// marker for memory trash testing
	*(int *)((byte *)base + base->size - 4) = TRASH_ID;
#endif

#if FEAT_MEMSTATS
	MemStats_Alloc( ZoneTagToMemTag( tag ), (int)base->size );
#endif

	return (void *)(base + 1);
}


/*
========================
Z_Malloc
========================
*/
#ifdef ZONE_DEBUG
void *Z_MallocDebug( size_t size, const char *label, const char *file, int line ) {
#else
void *Z_Malloc( size_t size ) {
#endif
	void	*buf;

  //Z_CheckHeap ();	// DEBUG

#ifdef ZONE_DEBUG
	buf = Z_TagMallocDebug( size, TAG_GENERAL, label, file, line );
#else
	buf = Z_TagMalloc( size, TAG_GENERAL );
#endif
	memset( buf, 0, size );

	return buf;
}


/*
========================
S_Malloc
========================
*/
#ifdef ZONE_DEBUG
void *S_MallocDebug( size_t size, const char *label, const char *file, int line ) {
	return Z_TagMallocDebug( size, TAG_SOUND, label, file, line );
}
#else
void *S_Malloc( size_t size ) {
	return Z_TagMalloc( size, TAG_SOUND );
}
#endif


/*
========================
Z_CheckHeap
========================
*/
void Z_CheckHeap( void )
{
	const memblock_t *block;
	const memzone_t *zone;

	zone = mainzone;
	for ( block = zone->blocklist.next; ; ) {
		if ( block->next == &zone->blocklist ) {
			break;	// all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next ) {
#ifdef USE_MULTI_SEGMENT
			const memblock_t *next = block->next;
#ifdef USE_ZONE_ID
			if ( next->size == 0 && next->id == -ZONE_ID && next->tag == TAG_GENERAL ) {
#else
			if ( next->size == 0 && next->tag == TAG_GENERAL ) {
#endif
				block = next; // new zone segment
			} else
#endif
			Com_Terminate( TERM_UNRECOVERABLE, "Z_CheckHeap: block size does not touch the next block" );
		}
		if ( block->next->prev != block ) {
			Com_Terminate( TERM_UNRECOVERABLE, "Z_CheckHeap: next block doesn't have proper back link" );
		}
		if ( block->tag == TAG_FREE && block->next->tag == TAG_FREE ) {
			Com_Terminate( TERM_UNRECOVERABLE, "Z_CheckHeap: two consecutive free blocks" );
		}
		block = block->next;
	}
}


/*
========================
Z_LogZoneHeap
========================
*/
static void Z_LogZoneHeap( memzone_t *zone, const char *name )
{
#ifdef ZONE_DEBUG
	char dump[32], *ptr;
	int  i, j;
#endif
	size_t size = 0, numBlocks = 0;
	size_t allocSize = 0;
	char buf[4096];
	Com_sprintf( buf, sizeof(buf), "\r\n================\r\n%s log\r\n================\r\n", name );
	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
	for ( memblock_t *block = zone->blocklist.next ; ; ) {
		if ( block->tag != TAG_FREE ) {
#ifdef ZONE_DEBUG
			ptr = ((char *) block) + sizeof(memblock_t);
			j = 0;
			for (i = 0; i < 20 && i < block->d.allocSize; i++) {
				if (ptr[i] >= 32 && ptr[i] < 127) {
					dump[j++] = ptr[i];
				}
				else {
					dump[j++] = '_';
				}
			}
			dump[j] = '\0';
			Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s) [%s]\r\n", block->d.allocSize, block->d.file, block->d.line, block->d.label, dump);
			Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
			allocSize += block->d.allocSize;
#endif
			size += block->size;
			numBlocks++;
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
		block = block->next;
	}
#ifdef ZONE_DEBUG
	// subtract debug memory
	size -= numBlocks * sizeof(zonedebug_t);
#else
	allocSize = numBlocks * sizeof(memblock_t); // + 32 bit alignment
#endif
	Com_sprintf( buf, sizeof( buf ), "%"PRIz"u %s memory in %"PRIz"u blocks\r\n", size, name, numBlocks );
	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
	Com_sprintf( buf, sizeof( buf ), "%"PRIz"u %s memory overhead\r\n", size - allocSize, name );
	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
}


/*
========================
Z_LogHeap
========================
*/
void Z_LogHeap( void )
{
	Z_LogZoneHeap( mainzone, "MAIN" );
	Z_LogZoneHeap( smallzone, "SMALL" );
}

#ifdef USE_STATIC_TAGS

// static mem blocks to reduce a lot of small zone overhead
typedef struct memstatic_s {
	memblock_t b;
	byte mem[2];
} memstatic_t;

#ifdef USE_ZONE_ID
#define MEM_STATIC(chr) { { NULL, NULL, sizeof(memstatic_t), TAG_STATIC, ZONE_ID }, {chr,'\0'} }
#else
#define MEM_STATIC(chr) { { NULL, NULL, sizeof(memstatic_t), TAG_STATIC }, {chr,'\0'} }
#endif

static const memstatic_t emptystring =
	MEM_STATIC( '\0' );

static const memstatic_t numberstring[] = {
	MEM_STATIC( '0' ),
	MEM_STATIC( '1' ),
	MEM_STATIC( '2' ),
	MEM_STATIC( '3' ),
	MEM_STATIC( '4' ),
	MEM_STATIC( '5' ),
	MEM_STATIC( '6' ),
	MEM_STATIC( '7' ),
	MEM_STATIC( '8' ),
	MEM_STATIC( '9' )
};
#endif // USE_STATIC_TAGS

/*
========================
CopyString

 NOTE:	never write over the memory CopyString returns because
		memory from a memstatic_t might be returned
========================
*/
char *CopyString( const char *in )
{
	char *out;
#ifdef USE_STATIC_TAGS
	if ( in[0] == '\0' ) {
		return ((char *)&emptystring) + sizeof(memblock_t);
	}
	if ( in[0] >= '0' && in[0] <= '9' && in[1] == '\0' ) {
		return ( (char *)&numberstring[in[0] - '0'] ) + sizeof( memblock_t );
	}
#endif
	out = S_Malloc( strlen( in ) + 1 );
	strcpy( out, in );
	return out;
}


/*
==============================================================================

Goals:
	reproducible without history effects -- no out of memory errors on weird map to map changes
	allow restarting of the client without fragmentation
	minimize total pages in use at run time
	minimize total pages needed during load time

  Single block of memory with stack allocators coming from both ends towards the middle.

  One side is designated the temporary memory allocator.

  Temporary memory can be allocated and freed in any order.

  A highwater mark is kept of the most in use at any time.

  When there is no temporary memory allocated, the permanent and temp sides
  can be switched, allowing the already touched temp memory to be used for
  permanent storage.

  Temp memory must never be allocated on two ends at once, or fragmentation
  could occur.

  If we have any in-use temp memory, additional temp allocations must come from
  that side.

  If not, we can choose to make either side the new temp side and push future
  permanent allocations to the other side.  Permanent allocations should be
  kept on the side that has the current greatest wasted highwater mark.

==============================================================================
*/


#define	HUNK_MAGIC	0x89537892
#define	HUNK_FREE_MAGIC	0x89537893

typedef struct {
	unsigned int magic;
	unsigned int size;
} hunkHeader_t;

typedef struct {
	int		mark;
	int		permanent;
	int		temp;
	int		tempHighwater;
} hunkUsed_t;

typedef struct hunkblock_s {
	int size;
	byte printed;
	struct hunkblock_s *next;
	const char *label;
	const char *file;
	int line;
} hunkblock_t;

static	hunkblock_t *hunkblocks;

static	hunkUsed_t	hunk_low, hunk_high;
static	hunkUsed_t	*hunk_permanent, *hunk_temp;

static	byte	*s_hunkData = NULL;
static	int		s_hunkTotal;

static const char *tagName[ TAG_COUNT ] = {
	"FREE",
	"GENERAL",
	"PACK",
	"SEARCH-PATH",
	"SEARCH-PACK",
	"SEARCH-DIR",
	"BOTLIB",
	"RENDERER",
	"CLIENTS",
	"SMALL",
	"STATIC",
	"SOUND"
};

#if FEAT_MEMSTATS
memTagStats_t memStats[MEMTAG_COUNT];

const char *MemTag_Names[MEMTAG_COUNT] = {
	"general", "renderer", "sound", "network", "botai",
	"game", "cgame", "ui", "filesystem", "scripting",
	"collision", "temp"
};

static memTag_t ZoneTagToMemTag( memtag_t tag ) {
	switch ( tag ) {
		case TAG_BOTLIB:      return MEMTAG_BOTAI;
		case TAG_RENDERER:    return MEMTAG_RENDERER;
		case TAG_CLIENTS:     return MEMTAG_NETWORK;
		case TAG_PACK:
		case TAG_SEARCH_PATH:
		case TAG_SEARCH_PACK:
		case TAG_SEARCH_DIR:  return MEMTAG_FILESYSTEM;
		case TAG_SOUND:       return MEMTAG_SOUND;
		default:              return MEMTAG_GENERAL;
	}
}

typedef enum {
	MEMBUDGET_OK = 0,
	MEMBUDGET_WARN,
	MEMBUDGET_EXCEEDED
} memBudgetState_t;

static memBudgetState_t memBudgetState[MEMTAG_COUNT];
static cvar_t *memBudgetCvars[MEMTAG_COUNT];
static cvar_t *mem_budget_enforce;

static void MemStats_CheckBudget( memTag_t tag ) {
	float         budgetMB;
	int64_t       budgetBytes, used;
	memBudgetState_t newState;

	if ( !mem_budget_enforce || !mem_budget_enforce->integer ) return;
	if ( !memBudgetCvars[tag] ) return;

	budgetMB = memBudgetCvars[tag]->value;
	if ( budgetMB <= 0.0f ) return;

	budgetBytes = (int64_t)( budgetMB * 1024.0f * 1024.0f );
	used        = memStats[tag].currentBytes;

	if ( used >= budgetBytes ) {
		newState = MEMBUDGET_EXCEEDED;
	} else if ( used * 10 >= budgetBytes * 8 ) {
		newState = MEMBUDGET_WARN;
	} else {
		memBudgetState[tag] = MEMBUDGET_OK;
		return;
	}

	if ( newState <= memBudgetState[tag] ) return; // already at or past this level
	memBudgetState[tag] = newState;

	if ( newState == MEMBUDGET_EXCEEDED ) {
		if ( mem_budget_enforce->integer >= 2 ) {
			Com_Terminate( TERM_CLIENT_DROP, "mem budget exceeded: %s %.1f/%.0f MB",
				MemTag_Names[tag],
				used / (1024.0f * 1024.0f), budgetMB );
		}
		COM_WARN( LOG_CH(ch_system), "mem budget exceeded: %s %.1f/%.0f MB\n",
			MemTag_Names[tag], used / (1024.0f * 1024.0f), budgetMB );
	} else {
		COM_WARN( LOG_CH(ch_system), "mem budget 80%%: %s %.1f/%.0f MB\n",
			MemTag_Names[tag], used / (1024.0f * 1024.0f), budgetMB );
	}
}

void MemStats_Alloc( memTag_t tag, int size ) {
	memStats[tag].currentBytes += size;
	memStats[tag].currentCount++;
	memStats[tag].totalCount++;
	if ( memStats[tag].currentBytes > memStats[tag].peakBytes ) {
		memStats[tag].peakBytes = memStats[tag].currentBytes;
	}
	MemStats_CheckBudget( tag );
}

void MemStats_Free( memTag_t tag, int size ) {
	int64_t budgetBytes, used;
	float   budgetMB;

	memStats[tag].currentBytes -= size;
	memStats[tag].currentCount--;

	// Reset budget state when usage drops back below thresholds
	if ( memBudgetState[tag] != MEMBUDGET_OK && memBudgetCvars[tag] ) {
		budgetMB = memBudgetCvars[tag]->value;
		if ( budgetMB > 0.0f ) {
			budgetBytes = (int64_t)( budgetMB * 1024.0f * 1024.0f );
			used        = memStats[tag].currentBytes;
			if ( used * 10 < budgetBytes * 8 ) {
				memBudgetState[tag] = MEMBUDGET_OK;
			} else if ( memBudgetState[tag] == MEMBUDGET_EXCEEDED && used < budgetBytes ) {
				memBudgetState[tag] = MEMBUDGET_WARN;
			}
		}
	}
}

void MemStats_Reset( void ) {
	for ( int i = 0; i < MEMTAG_COUNT; i++ ) {
		memStats[i].peakBytes = memStats[i].currentBytes;
	}
}
#endif // FEAT_MEMSTATS

typedef struct zone_stats_s {
	size_t zoneSegments;
	size_t zoneBlocks;
	size_t zoneBytes;
	size_t botlibBytes;
	size_t rendererBytes;
	size_t freeBytes;
	size_t freeBlocks;
	size_t freeSmallest;
	size_t freeLargest;
} zone_stats_t;


static void Zone_Stats( const memzone_t *z, qboolean printDetails, zone_stats_t *stats )
{
	const memblock_t *block;
	const memzone_t *zone;
	zone_stats_t st;

	memset( &st, 0, sizeof( st ) );
	zone = z;
	st.zoneSegments = 1;
	st.freeSmallest = SIZE_MAX;

	//if ( printDetails ) {
	//	Com_Log( SEV_INFO, LOG_CH(ch_system), "---------- %s zone segment #%i ----------\n", name, zone->segnum );
	//}

	for ( block = zone->blocklist.next ; ; ) {
		if ( printDetails ) {
			int tag = block->tag;
			Com_Log( SEV_INFO, LOG_CH(ch_system), "block:%p  size:%8u  tag: %s\n", (void *)block, block->size,
				(unsigned)tag < TAG_COUNT ? tagName[ tag ] : va( "%i", tag ) );
		}
		if ( block->tag != TAG_FREE ) {
			st.zoneBytes += block->size;
			st.zoneBlocks++;
			if ( block->tag == TAG_BOTLIB ) {
				st.botlibBytes += block->size;
			} else if ( block->tag == TAG_RENDERER ) {
				st.rendererBytes += block->size;
			}
		} else {
			st.freeBytes += block->size;
			st.freeBlocks++;
			if ( block->size > st.freeLargest )
				st.freeLargest = block->size;
			if ( block->size < st.freeSmallest )
				st.freeSmallest = block->size;
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next) {
#ifdef USE_MULTI_SEGMENT
			const memblock_t *next = block->next;
#ifdef USE_ZONE_ID
			if ( next->size == 0 && next->id == -ZONE_ID && next->tag == TAG_GENERAL ) {
#else
			if ( next->size == 0 && next->tag == TAG_GENERAL ) {
#endif
				st.zoneSegments++;
				if ( printDetails ) {
					Com_Log( SEV_INFO, LOG_CH(ch_system), "---------- %s zone segment #%"PRIz"u ----------\n", zone->name, st.zoneSegments );
				}
				block = next->next;
				continue;
			}
#endif Com_Log( SEV_INFO, LOG_CH( ch_system ), "ERROR: block size does not touch the next block\n" );
		}
		if ( block->next->prev != block) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "ERROR: next block doesn't have proper back link\n" );
		}
		if ( block->tag == TAG_FREE && block->next->tag == TAG_FREE ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "ERROR: two consecutive free blocks\n" );
		}
		block = block->next;
	}

	// export stats
	if ( stats ) {
		memcpy( stats, &st, sizeof( *stats ) );
	}
}


#if FEAT_MEMSTATS
hunkStats_t Hunk_GetStats( void ) {
	hunkStats_t s;
	int low  = hunk_low.permanent  > hunk_low.temp  ? hunk_low.permanent  : hunk_low.temp;
	int high = hunk_high.permanent > hunk_high.temp ? hunk_high.permanent : hunk_high.temp;
	s.totalBytes         = s_hunkTotal;
	s.permanentLowBytes  = hunk_low.permanent;
	s.permanentHighBytes = hunk_high.permanent;
	s.tempBytes          = hunk_low.temp + hunk_high.temp - hunk_low.permanent - hunk_high.permanent;
	if ( s.tempBytes < 0 ) s.tempBytes = 0;
	s.freeBytes          = s_hunkTotal - ( low + high );
	s.peakUsedBytes      = hunk_low.tempHighwater + hunk_high.tempHighwater;
	return s;
}

zoneStats_t Zone_GetStats( void ) {
	zone_stats_t raw;
	zoneStats_t  s;
	memset( &s, 0, sizeof( s ) );
	Zone_Stats( mainzone, qfalse, &raw );
	s.totalBytes       = (int)mainzone->size;
	s.usedBytes        = (int)raw.zoneBytes;
	s.freeBytes        = (int)raw.freeBytes;
	s.freeBlockCount   = (int)raw.freeBlocks;
	s.largestFreeBlock = (int)raw.freeLargest;
	s.allocCount       = (int)raw.zoneBlocks;
	return s;
}
#endif // FEAT_MEMSTATS


/*
=================
Com_Meminfo_f
=================
*/
static void Com_Meminfo_f( void ) {
	zone_stats_t st;
	int		unused;

	Com_Log( SEV_INFO, LOG_CH(ch_system), "──────────────────────────────────────────\n" );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i bytes total hunk\n", s_hunkTotal );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "\n" );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i low mark\n", hunk_low.mark );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i low permanent\n", hunk_low.permanent );
	if ( hunk_low.temp != hunk_low.permanent ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i low temp\n", hunk_low.temp );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i low tempHighwater\n", hunk_low.tempHighwater );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "\n" );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i high mark\n", hunk_high.mark );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i high permanent\n", hunk_high.permanent );
	if ( hunk_high.temp != hunk_high.permanent ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i high temp\n", hunk_high.temp );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i high tempHighwater\n", hunk_high.tempHighwater );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "\n" );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i total hunk in use\n", hunk_low.permanent + hunk_high.permanent );
	unused = 0;
	if ( hunk_low.tempHighwater > hunk_low.permanent ) {
		unused += hunk_low.tempHighwater - hunk_low.permanent;
	}
	if ( hunk_high.tempHighwater > hunk_high.permanent ) {
		unused += hunk_high.tempHighwater - hunk_high.permanent;
	}
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8i unused highwater\n", unused );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "\n" );

	Zone_Stats( mainzone, !Q_stricmp( Cmd_Argv(1), "main" ) || !Q_stricmp( Cmd_Argv(1), "all" ), &st );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8"PRIz"u bytes total main zone\n\n", mainzone->size );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8"PRIz"u bytes in %"PRIz"u main zone blocks%s\n", st.zoneBytes, st.zoneBlocks,
		st.zoneSegments > 1 ? va( " and %"PRIz"u segments", st.zoneSegments ) : "" );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "        %8"PRIz"u bytes in botlib\n", st.botlibBytes );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "        %8"PRIz"u bytes in renderer\n", st.rendererBytes );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "        %8"PRIz"u bytes in other\n", st.zoneBytes - ( st.botlibBytes + st.rendererBytes ) );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "        %8"PRIz"u bytes in %"PRIz"u free blocks\n", st.freeBytes, st.freeBlocks );
	if ( st.freeBlocks > 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "        (largest: %"PRIz"u bytes, smallest: %"PRIz"u bytes)\n\n", st.freeLargest, st.freeSmallest );
	}

	Zone_Stats( smallzone, !Q_stricmp( Cmd_Argv(1), "small" ) || !Q_stricmp( Cmd_Argv(1), "all" ), &st );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8"PRIz"u bytes total small zone\n\n", smallzone->size );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%8"PRIz"u bytes in %"PRIz"u small zone blocks%s\n", st.zoneBytes, st.zoneBlocks,
		st.zoneSegments > 1 ? va( " and %"PRIz"u segments", st.zoneSegments ) : "" );
	Com_Log( SEV_INFO, LOG_CH(ch_system), "        %8"PRIz"u bytes in %"PRIz"u free blocks\n", st.freeBytes, st.freeBlocks );
	if ( st.freeBlocks > 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "        (largest: %"PRIz"u bytes, smallest: %"PRIz"u bytes)\n", st.freeLargest, st.freeSmallest );
	}

	/* Persistent arena allocators */
	Com_Log( SEV_INFO, LOG_CH(ch_system), "\n--- Persistent Arenas (survive map changes) ---\n" );
	Arena_PrintStats();

#if FEAT_MEMSTATS
	{
		Com_Log( SEV_INFO, LOG_CH(ch_system), "\nTAG BREAKDOWN (current / peak):\n" );
		for ( int i = 0; i < MEMTAG_COUNT; i++ ) {
			if ( memStats[i].currentBytes == 0 && memStats[i].peakBytes == 0 ) continue;
			Com_Log( SEV_INFO, LOG_CH(ch_system), "  %-12s %7.1f MB / %7.1f MB   %5i allocs\n",
				MemTag_Names[i],
				memStats[i].currentBytes / (1024.0f * 1024.0f),
				memStats[i].peakBytes    / (1024.0f * 1024.0f),
				memStats[i].currentCount );
		}
	}
#endif
	Com_Log( SEV_INFO, LOG_CH(ch_system), "──────────────────────────────────────────\n" );
}


#if FEAT_MEMSTATS
static void Com_MemInfoReset_f( void ) {
	MemStats_Reset();
	Com_Log( SEV_INFO, LOG_CH(ch_system), "Peak memory counters reset.\n" );
}

static void Com_MemInfoJson_f( void ) {
	hunkStats_t  hs = Hunk_GetStats();
	zoneStats_t  zs = Zone_GetStats();

	Com_Log( SEV_INFO, LOG_CH(ch_system), "{\"hunk\":{\"total\":%i,\"low\":%i,\"high\":%i,\"temp\":%i,\"free\":%i,\"peak\":%i}",
		hs.totalBytes, hs.permanentLowBytes, hs.permanentHighBytes,
		hs.tempBytes, hs.freeBytes, hs.peakUsedBytes );
	Com_Log( SEV_INFO, LOG_CH(ch_system), ",\"zone\":{\"total\":%i,\"used\":%i,\"free\":%i,\"freeBlocks\":%i,\"largestFree\":%i,\"allocs\":%i}",
		zs.totalBytes, zs.usedBytes, zs.freeBytes,
		zs.freeBlockCount, zs.largestFreeBlock, zs.allocCount );
	Com_Log( SEV_INFO, LOG_CH(ch_system), ",\"tags\":{" );
	for ( int i = 0; i < MEMTAG_COUNT; i++ ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "%s\"%s\":{\"current\":%lli,\"peak\":%lli,\"count\":%i,\"total\":%i}",
			i > 0 ? "," : "",
			MemTag_Names[i],
			(long long)memStats[i].currentBytes, (long long)memStats[i].peakBytes,
			memStats[i].currentCount, memStats[i].totalCount );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_system), "}}\n" );
}
#endif // FEAT_MEMSTATS


/*
===============
Com_TouchMemory

Touch all known used data to make sure it is paged in
===============
*/
unsigned int Com_TouchMemory( void ) {
	const memblock_t *block;
	const memzone_t *zone;
	int		start, end;
	int		i, j;
	unsigned int sum;

	Z_CheckHeap();

	start = Sys_Milliseconds();

	sum = 0;

	j = hunk_low.permanent >> 2;
	for ( i = 0 ; i < j ; i+= 1024 ) {			// only need to touch each page
		sum += ((unsigned int *)s_hunkData)[i];
	}

	i = ( s_hunkTotal - hunk_high.permanent ) >> 2;
	j = hunk_high.permanent >> 2;
	for (  ; i < j ; i += 1024 ) {			// only need to touch each page
		sum += ((unsigned int *)s_hunkData)[i];
	}

	zone = mainzone;
	for (block = zone->blocklist.next ; ; block = block->next) {
		if ( block->tag != TAG_FREE ) {
			j = block->size >> 2;
			for ( i = 0 ; i < j ; i += 1024 ) {				// only need to touch each page
				sum += ((unsigned int *)block)[i];
			}
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
	}

	end = Sys_Milliseconds();

	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "Com_TouchMemory: %i msec\n", end - start );

	return sum; // just to silent compiler warning
}


/*
=================
Com_InitSmallZoneMemory
=================
*/
void Com_InitSmallZoneMemory( void ) {
	static byte s_buf[ 512 * 1024 ];
	int smallZoneSize = sizeof( s_buf );
	memset( s_buf, 0, smallZoneSize );
	smallzone = (memzone_t *)s_buf;
	Z_Init( smallzone, smallZoneSize, "small" );
}


/*
=================
Com_InitZoneMemory
=================
*/
void Com_InitZoneMemory( void ) {
	int		mainZoneSize;
	cvar_t	*cv;

	// Please note: com_zoneMegs can only be set on the command line, and
	// not in config.cfg or Com_StartupVariable, as they haven't been
	// executed by this point. It's a chicken and egg problem. We need the
	// memory manager configured to handle those places where you would
	// configure the memory manager.

	// allocate the random block zone
	{
		static const cvarDesc_t d = CVAR_INT( "com_zoneMegs", XSTRING( DEF_COMZONEMEGS ),
			CVAR_LATCH | CVAR_ARCHIVE,
			"Initial amount of memory (RAM) allocated for the main block zone (in MB).",
			1, INT_MAX / (1024*1024) );
		cv = Cvar_Register( &d );
	}

#ifndef USE_MULTI_SEGMENT
	if ( cv->integer < DEF_COMZONEMEGS )
		mainZoneSize = 1024 * 1024 * DEF_COMZONEMEGS;
	else
#endif
		mainZoneSize = cv->integer * 1024 * 1024;

	mainzone = malloc( mainZoneSize );
	if ( !mainzone ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Zone data failed to allocate %i megs", mainZoneSize / (1024*1024) );
	}
	Z_Init( mainzone, mainZoneSize, "main");
}


/*
=================
Hunk_Log
=================
*/
void Hunk_Log( void ) {
	int size = 0, numBlocks = 0;
	char buf[4096];
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk log\r\n================\r\n");
	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
	for (hunkblock_t *block = hunkblocks ; block; block = block->next) {
#ifdef HUNK_DEBUG
		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", block->size, block->file, block->line, block->label);
		Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
#endif
		size += block->size;
		numBlocks++;
	}
	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
}


/*
=================
Hunk_SmallLog
=================
*/
#ifdef HUNK_DEBUG
void Hunk_SmallLog( void ) {
	for (hunkblock_t *block = hunkblocks ; block; block = block->next) {
		block->printed = qfalse;
	}
	int size = 0, numBlocks = 0;
	char buf[4096];
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk Small log\r\n================\r\n");
	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
	for (hunkblock_t *block = hunkblocks; block; block = block->next) {
		if (block->printed) {
			continue;
		}
		int locsize = block->size;
		for (hunkblock_t *block2 = block->next; block2; block2 = block2->next) {
			if (block->line != block2->line) {
				continue;
			}
			if (Q_stricmp(block->file, block2->file)) {
				continue;
			}
			size += block2->size;
			locsize += block2->size;
			block2->printed = qtrue;
		}
		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", locsize, block->file, block->line, block->label);
		Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
		size += block->size;
		numBlocks++;
	}
	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "%s", buf );
}
#endif


/*
=================
Com_InitHunkMemory
=================
*/
static void Com_InitHunkMemory( void ) {
	cvar_t	*cv;

	// make sure the file system has allocated and "not" freed any temp blocks
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the file system utilizing different
	// memory systems
	if ( FS_LoadStack() != 0 ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Hunk initialization failed. File system load stack not zero" );
	}

	// allocate the stack based hunk allocator
	{
		// NOLINTNEXTLINE(bugprone-integer-division) — both operands are exact integer multiples; max-megabyte count is naturally integer
		static const cvarDesc_t d = CVAR_INT( "com_hunkMegs", XSTRING( DEF_COMHUNKMEGS ),
			CVAR_LATCH | CVAR_ARCHIVE,
			"The size of the hunk memory segment.",
			MIN_COMHUNKMEGS, (INT_MAX-63) / (1024*1024) );
		cv = Cvar_Register( &d );
	}

	s_hunkTotal = cv->integer * 1024 * 1024;

	s_hunkData = calloc( s_hunkTotal + 63, 1 );
	if ( !s_hunkData ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Hunk data failed to allocate %i megs", s_hunkTotal / (1024*1024) );
	}

	// cacheline align
	s_hunkData = PADP( s_hunkData, 64 );
	Hunk_ClearLevel();

	Cmd_AddCommand( "meminfo", Com_Meminfo_f );
#if FEAT_MEMSTATS
	Cmd_AddCommand( "meminfo_reset", Com_MemInfoReset_f );
	Cmd_AddCommand( "meminfo_json",  Com_MemInfoJson_f );
	{
		char cvarName[64];
		{
			static const cvarDesc_t d = CVAR_BOOL( "mem_budget_enforce", "1", CVAR_ARCHIVE, NULL );
			mem_budget_enforce = Cvar_Register( &d );
		}
		for ( int i = 0; i < MEMTAG_COUNT; i++ ) {
			Com_sprintf( cvarName, sizeof( cvarName ), "mem_budget_%s", MemTag_Names[i] );
			memBudgetCvars[i] = Cvar_Get( cvarName, "0", CVAR_ARCHIVE );
		}
	}
#endif
#ifdef ZONE_DEBUG
	Cmd_AddCommand( "zonelog", Z_LogHeap );
#endif
#ifdef HUNK_DEBUG
	Cmd_AddCommand( "hunklog", Hunk_Log );
	Cmd_AddCommand( "hunksmalllog", Hunk_SmallLog );
#endif
}


/*
====================
Hunk_MemoryRemaining
====================
*/
int	Hunk_MemoryRemaining( void ) {
	int		low, high;

	low = hunk_low.permanent > hunk_low.temp ? hunk_low.permanent : hunk_low.temp;
	high = hunk_high.permanent > hunk_high.temp ? hunk_high.permanent : hunk_high.temp;

	return s_hunkTotal - ( low + high );
}


/*
===================
Hunk_SetMark

The server calls this after the level and game VM have been loaded
===================
*/
void Hunk_SetMark( void ) {
	hunk_low.mark = hunk_low.permanent;
	hunk_high.mark = hunk_high.permanent;
}


/*
=================
Hunk_ClearToMark

The client calls this before starting a vid_restart or snd_restart
=================
*/
void Hunk_ClearToMark( void ) {
	hunk_low.permanent = hunk_low.temp = hunk_low.mark;
	hunk_high.permanent = hunk_high.temp = hunk_high.mark;
}


/*
=================
Hunk_CheckMark
=================
*/
qboolean Hunk_CheckMark( void ) {
	if( hunk_low.mark || hunk_high.mark ) {
		return qtrue;
	}
	return qfalse;
}

void CL_ShutdownCGame( void );
void CL_ShutdownUI( void );
void SV_ShutdownGameProgs( void );

/*
=================
Hunk_ClearLevel

Called on every map transition to reset the level-scoped portion of the hunk.
Persistent subsystems (WiredScript_Arena, Console_Arena, Audio_Arena, renderer
backEndData) live outside the hunk and are NOT affected by this call.
=================
*/
void Hunk_ClearLevel( void ) {

#ifndef DEDICATED
	CL_ShutdownCGame();
	CL_ShutdownUI();
#endif
	SV_ShutdownGameProgs();
#ifndef DEDICATED
	CIN_CloseAllVideos();
#endif
	hunk_low.mark = 0;
	hunk_low.permanent = 0;
	hunk_low.temp = 0;
	hunk_low.tempHighwater = 0;

	hunk_high.mark = 0;
	hunk_high.permanent = 0;
	hunk_high.temp = 0;
	hunk_high.tempHighwater = 0;

	hunk_permanent = &hunk_low;
	hunk_temp = &hunk_high;

#if FEAT_MEMSTATS
	memStats[MEMTAG_GENERAL].currentBytes = 0;
	memStats[MEMTAG_GENERAL].currentCount = 0;
	memStats[MEMTAG_TEMP].currentBytes = 0;
	memStats[MEMTAG_TEMP].currentCount = 0;
#endif

	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "Hunk_ClearLevel: reset the hunk ok\n" );
	VM_Clear();
#ifdef HUNK_DEBUG
	hunkblocks = NULL;
#endif
}


static void Hunk_SwapBanks( void ) {
	hunkUsed_t	*swap;

	// can't swap banks if there is any temp already allocated
	if ( hunk_temp->temp != hunk_temp->permanent ) {
		return;
	}

	// if we have a larger highwater mark on this side, start making
	// our permanent allocations here and use the other side for temp
	if ( hunk_temp->tempHighwater - hunk_temp->permanent >
		hunk_permanent->tempHighwater - hunk_permanent->permanent ) {
		swap = hunk_temp;
		hunk_temp = hunk_permanent;
		hunk_permanent = swap;
	}
}


/*
=================
Hunk_Alloc

Allocate permanent (until the hunk is cleared) memory
=================
*/
#ifdef HUNK_DEBUG
void *Hunk_AllocDebug( size_t size, ha_pref preference, const char *label, const char *file, int line ) {
#else
void *Hunk_Alloc( size_t size, ha_pref preference ) {
#endif
	void	*buf;

	if ( s_hunkData == NULL)
	{
		Com_Terminate( TERM_UNRECOVERABLE, "Hunk_Alloc: Hunk memory system not initialized" );
	}

	if ( size > INT_MAX ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Hunk_Alloc: %"PRIz"u > INT_MAX", size );
	}

	if ( size > INT_MAX ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Hunk_Alloc: %"PRIz"u > INT_MAX", size );
	}

	// can't do preference if there is any temp allocated
	if (preference == h_dontcare || hunk_temp->temp != hunk_temp->permanent) {
		Hunk_SwapBanks();
	} else {
		if (preference == h_low && hunk_permanent != &hunk_low) {
			Hunk_SwapBanks();
		} else if (preference == h_high && hunk_permanent != &hunk_high) {
			Hunk_SwapBanks();
		}
	}

#ifdef HUNK_DEBUG
	size += sizeof(hunkblock_t);
#endif

	// round to cacheline
	size = PAD( size, 64 );

	if ( hunk_low.temp + hunk_high.temp + size > s_hunkTotal ) {
#ifdef HUNK_DEBUG
		Hunk_Log();
		Hunk_SmallLog();

		Com_Terminate( TERM_CLIENT_DROP, "Hunk_Alloc failed on %"PRIz"u: %s, line: %d (%s)", size, file, line, label);
#else
		Com_Terminate( TERM_CLIENT_DROP, "Hunk_Alloc failed on %"PRIz"u", size);
#endif
	}

	if ( hunk_permanent == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_permanent->permanent);
		hunk_permanent->permanent += size;
	} else {
		hunk_permanent->permanent += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_permanent->permanent );
	}

	hunk_permanent->temp = hunk_permanent->permanent;

	memset( buf, 0, size );

#ifdef HUNK_DEBUG
	{
		hunkblock_t *block;

		block = (hunkblock_t *) buf;
		block->size = size - sizeof(hunkblock_t);
		block->file = file;
		block->label = label;
		block->line = line;
		block->next = hunkblocks;
		hunkblocks = block;
		buf = ((byte *) buf) + sizeof(hunkblock_t);
	}
#endif
#if FEAT_MEMSTATS
	MemStats_Alloc( MEMTAG_GENERAL, (int)size );
#endif
	return buf;
}


/*
=================
Hunk_AllocateTempMemory

This is used by the file loading system.
Multiple files can be loaded in temporary memory.
When the files-in-use count reaches zero, all temp memory will be deleted
=================
*/
void *Hunk_AllocateTempMemory( size_t size ) {
	void		*buf;
	hunkHeader_t	*hdr;

	// return a Z_Malloc'd block if the hunk has not been initialized
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the file system utilizing different
	// memory systems
	if ( s_hunkData == NULL )
	{
		return Z_Malloc(size);
	}

	if ( size > INT_MAX ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Hunk_AllocateTempMemory: %"PRIz"u > INT_MAX", size );
	}

	Hunk_SwapBanks();

	size = PAD(size, sizeof(intptr_t)) + sizeof( hunkHeader_t );

	if ( hunk_temp->temp + hunk_permanent->permanent + size > s_hunkTotal ) {
		Com_Terminate( TERM_CLIENT_DROP, "Hunk_AllocateTempMemory: failed on %"PRIz"u", size );
	}

	if ( hunk_temp == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_temp->temp);
		hunk_temp->temp += size;
	} else {
		hunk_temp->temp += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp );
	}

	if ( hunk_temp->temp > hunk_temp->tempHighwater ) {
		hunk_temp->tempHighwater = hunk_temp->temp;
	}

	hdr = (hunkHeader_t *)buf;
	buf = (void *)(hdr+1);

	hdr->magic = HUNK_MAGIC;
	hdr->size = size;

#if FEAT_MEMSTATS
	MemStats_Alloc( MEMTAG_TEMP, (int)size );
#endif

	// don't bother clearing, because we are going to load a file over it
	return buf;
}


/*
==================
Hunk_FreeTempMemory
==================
*/
void Hunk_FreeTempMemory( void *buf ) {
	hunkHeader_t	*hdr;

	// free with Z_Free if the hunk has not been initialized
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the file system utilizing different
	// memory systems
	if ( s_hunkData == NULL )
	{
		Z_Free(buf);
		return;
	}

	hdr = ( (hunkHeader_t *)buf ) - 1;
	if ( hdr->magic != HUNK_MAGIC ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Hunk_FreeTempMemory: bad magic" );
	}

#if FEAT_MEMSTATS
	MemStats_Free( MEMTAG_TEMP, (int)hdr->size );
#endif

	hdr->magic = HUNK_FREE_MAGIC;

	// this only works if the files are freed in stack order,
	// otherwise the memory will stay around until Hunk_ClearTempMemory
	if ( hunk_temp == &hunk_low ) {
		if ( hdr == (void *)(s_hunkData + hunk_temp->temp - hdr->size ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Hunk_FreeTempMemory: not the final block\n" );
		}
	} else {
		if ( hdr == (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Hunk_FreeTempMemory: not the final block\n" );
		}
	}
}


/*
=================
Hunk_ClearTempMemory

The temp space is no longer needed.  If we have left more
touched but unused memory on this side, have future
permanent allocs use this side.
=================
*/
void Hunk_ClearTempMemory( void ) {
	if ( s_hunkData != NULL ) {
		hunk_temp->temp = hunk_temp->permanent;
	}
}

/*
===================================================================

EVENTS AND JOURNALING

In addition to these events, .cfg files are also copied to the
journaled file
===================================================================
*/

// MAX_PUSHED_EVENTS + com_pushedEvents* — moved to wired/event/event.c


/*
=================
Com_InitJournaling
=================
*/
static void Com_InitJournaling( void ) {
	if ( !com_journal->integer ) {
		return;
	}

	if ( com_journal->integer == 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Journaling events\n" );
		com_journalFile = FS_FOpenFileWrite( "journal.dat" );
		com_journalDataFile = FS_FOpenFileWrite( "journaldata.dat" );
	} else if ( com_journal->integer == 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Replaying journaled events\n" );
		FS_FOpenFileRead( "journal.dat", &com_journalFile, qtrue );
		FS_FOpenFileRead( "journaldata.dat", &com_journalDataFile, qtrue );
	}

	if ( com_journalFile == FS_INVALID_HANDLE || com_journalDataFile == FS_INVALID_HANDLE ) {
		Cvar_Set( "com_journal", "0" );
		if ( com_journalFile != FS_INVALID_HANDLE ) {
			FS_FCloseFile( com_journalFile );
			com_journalFile = FS_INVALID_HANDLE;
		}
		if ( com_journalDataFile != FS_INVALID_HANDLE ) {
			FS_FCloseFile( com_journalDataFile );
			com_journalDataFile = FS_INVALID_HANDLE;
		}
		Com_Log( SEV_INFO, LOG_CH(ch_system), "Couldn't open journal files\n" );
	}
}


// EVENT LOOP — moved to wired/event/event.c


//============================================================================

/*
=============
Com_Freeze_f

Just freeze in place for a given number of seconds to test
error recovery
=============
*/
static void Com_Freeze_f( void ) {
	int		s;
	int		start, now;

	if ( Cmd_Argc() != 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "freeze <seconds>\n" );
		return;
	}
	s = atoi( Cmd_Argv(1) ) * 1000;

	start = Com_Milliseconds();

	while ( 1 ) {
		now = Com_Milliseconds();
		if ( now - start > s ) {
			break;
		}
	}
}


/*
=================
Com_Crash_f

A way to force a bus error for development reasons
=================
*/
static void Com_Crash_f( void ) {
	// NOLINTNEXTLINE(clang-analyzer-core.NullDereference) — intentional crash to test signal handlers
	* ( volatile int * ) 0 = 0x12345678;
}


/*
==================
Com_ExecuteCfg

For controlling environment variables
==================
*/
static void Com_ExecuteCfg( void )
{
	Cbuf_ExecuteText(EXEC_NOW, "exec default.cfg\n");
	Cbuf_Execute(); // Always execute after exec to prevent text buffer overflowing

	if (!Com_SafeMode())
	{
		// skip the config.cfg and autoexec.cfg if "safe" is on the command line
		Cbuf_ExecuteText(EXEC_NOW, "exec " WIRED_CONFIG_CFG "\n");
		Cbuf_Execute();
		Cbuf_ExecuteText(EXEC_NOW, "exec autoexec.cfg\n");
		Cbuf_Execute();
	}
}


/*
==================
Com_GameRestart

Change to a new mod properly with cleaning up cvars before switching.
==================
*/
void Com_GameRestart( int checksumFeed, qboolean clientRestart )
{
	static qboolean com_gameRestarting = qfalse;

	// make sure no recursion can be triggered
	if ( !com_gameRestarting && com_fullyInitialized )
	{
		com_gameRestarting = qtrue;
#ifndef DEDICATED
		if ( clientRestart )
		{
			CL_Disconnect( qfalse );
			CL_ShutdownAll();
			CL_ClearMemory(); // Hunk_ClearLevel(); // -EC-
		}
#endif

		// Kill server if we have one
		if ( com_sv_running->integer )
			SV_Shutdown( "Game directory changed" );

		// Reset console command history
		Con_ResetHistory();

		// Shutdown FS early so Cvar_Restart will not reset old game cvars
		FS_Shutdown( qtrue );

		// Clean out any user and VM created cvars
		Cvar_Restart( qtrue );

#ifndef DEDICATED
		// Reparse pure paks and update cvars before FS startup
		if ( CL_GameSwitch() )
			CL_SystemInfoChanged( qfalse );
#endif

		FS_Restart( checksumFeed );

		// Load new configuration
		Com_ExecuteCfg();

#ifndef DEDICATED
		if ( clientRestart )
			CL_StartHunkUsers();
#endif

		com_gameRestarting = qfalse;
	}
}


/*
==================
Com_GameRestart_f

Expose possibility to change current running mod to the user
==================
*/
static void Com_GameRestart_f( void )
{
	Cvar_Set( "fs_game", Cmd_Argv( 1 ) );

	Com_GameRestart( 0, qtrue );
}


// TTimo: centralizing the cl_cdkey stuff after I discovered a buffer overflow problem with the dedicated server version
//   not sure it's necessary to have different defaults for regular and dedicated, but I don't want to risk it
//   https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=470
#ifndef DEDICATED
char	cl_cdkey[34] = "                                ";
#else
char	cl_cdkey[34] = "123456789";
#endif

/*
** --------------------------------------------------------------------------------
**
** PROCESSOR STUFF
**
** --------------------------------------------------------------------------------
*/

#ifdef USE_AFFINITY_MASK
static uint64_t eCoreMask;
static uint64_t pCoreMask;
static uint64_t affinityMask; // saved at startup
#endif

#if (idx64 || id386)

#if defined _MSC_VER
#include <intrin.h>
static void CPUID( int func, unsigned int *regs )
{
	__cpuid( (int*)regs, func );
}

#ifdef USE_AFFINITY_MASK
#if idx64
extern void CPUID_EX( int func, int param, unsigned int *regs );
#else
void CPUID_EX( int func, int param, unsigned int *regs )
{
	__asm {
		push edi
		mov eax, func
		mov ecx, param
		cpuid
		mov edi, regs
		mov [edi +0], eax
		mov [edi +4], ebx
		mov [edi +8], ecx
		mov [edi+12], edx
		pop edi
	}
}
#endif // !idx64
#endif // USE_AFFINITY_MASK

#else // clang/gcc/mingw

static void CPUID( int func, unsigned int *regs )
{
	__asm__ __volatile__( "cpuid" :
		"=a"(regs[0]),
		"=b"(regs[1]),
		"=c"(regs[2]),
		"=d"(regs[3]) :
		"a"(func) );
}

#ifdef USE_AFFINITY_MASK
static void CPUID_EX( int func, int param, unsigned int *regs )
{
	__asm__ __volatile__( "cpuid" :
		"=a"(regs[0]),
		"=b"(regs[1]),
		"=c"(regs[2]),
		"=d"(regs[3]) :
		"a"(func),
		"c"(param) );
}
#endif // USE_AFFINITY_MASK

#endif  // clang/gcc/mingw

static void Sys_GetProcessorId( char *vendor )
{
	uint32_t regs[4]; // EAX, EBX, ECX, EDX
	uint32_t cpuid_level_ex;
	char vendor_str[12 + 1]; // short CPU vendor string

	// setup initial features
#if idx64
	CPU_Flags |= CPU_SSE | CPU_SSE2 | CPU_FCOM;
#else
	CPU_Flags = 0;
#endif
	vendor[0] = '\0';

	CPUID( 0x80000000, regs );
	cpuid_level_ex = regs[0];

	// get CPUID level & short CPU vendor string
	CPUID( 0x0, regs );
	memcpy(vendor_str + 0, (char*)&regs[1], 4);
	memcpy(vendor_str + 4, (char*)&regs[3], 4);
	memcpy(vendor_str + 8, (char*)&regs[2], 4);
	vendor_str[12] = '\0';

	// get CPU feature bits
	CPUID( 0x1, regs );

	// bit 15 of EDX denotes CMOV/FCMOV/FCOMI existence
	if ( regs[3] & ( 1 << 15 ) )
		CPU_Flags |= CPU_FCOM;

	// bit 23 of EDX denotes MMX existence
	if ( regs[3] & ( 1 << 23 ) )
		CPU_Flags |= CPU_MMX;

	// bit 25 of EDX denotes SSE existence
	if ( regs[3] & ( 1 << 25 ) )
		CPU_Flags |= CPU_SSE;

	// bit 26 of EDX denotes SSE2 existence
	if ( regs[3] & ( 1 << 26 ) )
		CPU_Flags |= CPU_SSE2;

	// bit 0 of ECX denotes SSE3 existence
	//if ( regs[2] & ( 1 << 0 ) )
	//	CPU_Flags |= CPU_SSE3;

	// bit 19 of ECX denotes SSE41 existence
	if ( regs[ 2 ] & ( 1 << 19 ) )
		CPU_Flags |= CPU_SSE41;

	if ( vendor ) {
		if ( cpuid_level_ex >= 0x80000004 ) {
			// read CPU Brand string
			uint32_t i;
			for ( i = 0x80000002; i <= 0x80000004; i++) {
				CPUID( i, regs );
				memcpy( vendor+0, (char*)&regs[0], 4 );
				memcpy( vendor+4, (char*)&regs[1], 4 );
				memcpy( vendor+8, (char*)&regs[2], 4 );
				memcpy( vendor+12, (char*)&regs[3], 4 );
				vendor[16] = '\0';
				vendor += strlen( vendor );
			}
		} else {
			const int print_flags = CPU_Flags;
			vendor = Q_stradd( vendor, vendor_str );
			if (print_flags) {
				// print features
				strcat(vendor, " w/");
				if (print_flags & CPU_FCOM)
					strcat(vendor, " CMOV");
				if (print_flags & CPU_MMX)
					strcat(vendor, " MMX");
				if (print_flags & CPU_SSE)
					strcat(vendor, " SSE");
				if (print_flags & CPU_SSE2)
					strcat(vendor, " SSE2");
				//if ( CPU_Flags & CPU_SSE3 )
				//	strcat( vendor, " SSE3" );
				if (print_flags & CPU_SSE41)
					strcat(vendor, " SSE4.1");
			}
		}
	}
}


#ifdef USE_AFFINITY_MASK
static void DetectCPUCoresConfig( void )
{
	uint32_t regs[4];
	uint32_t i;

	// get highest function parameter and vendor id
	CPUID( 0x0, regs );
	if ( regs[1] != 0x756E6547 || regs[2] != 0x6C65746E || regs[3] != 0x49656E69 || regs[0] < 0x1A ) {
		// non-intel signature or too low cpuid level - unsupported
		eCoreMask = pCoreMask = affinityMask;
		return;
	}

	eCoreMask = 0;
	pCoreMask = 0;

	for ( i = 0; i < sizeof( affinityMask ) * 8; i++ ) {
		const uint64_t mask = 1ULL << i;
		if ( (mask & affinityMask) && Sys_SetAffinityMask( mask ) ) {
			CPUID_EX( 0x1A, 0x0, regs );
			switch ( (regs[0] >> 24) & 0xFF ) {
				case 0x20: eCoreMask |= mask; break;
				case 0x40: pCoreMask |= mask; break;
				default: // non-existing leaf
					eCoreMask = pCoreMask = 0;
					break;
			}
		}
	}

	// restore original affinity
	Sys_SetAffinityMask( affinityMask );

	if ( pCoreMask == 0 || eCoreMask == 0 ) {
		// if either mask is empty - assume non-hybrid configuration
		eCoreMask = pCoreMask = affinityMask;
	}
}
#endif // USE_AFFINITY_MASK

#else // non-x86

#ifndef __linux__

static void Sys_GetProcessorId( char *vendor )
{
	Com_sprintf( vendor, 100, "%s", ARCH_STRING );
#ifdef _WIN32
	CPU_Flags |= CPU_ARMv7 | CPU_IDIVA | CPU_VFPv3;
#endif
}

#else // __linux__

#include <sys/auxv.h>

#if arm32
#include <asm/hwcap.h>
#endif

static void Sys_GetProcessorId( char *vendor )
{
#if arm32
	const char *platform;
	long hwcaps;
	CPU_Flags = 0;

	platform = (const char*)getauxval( AT_PLATFORM );

	if ( !platform || *platform == '\0' ) {
		platform = "(unknown)";
	}

	if ( platform[0] == 'v' || platform[0] == 'V' ) {
		if ( atoi( platform + 1 ) >= 7 ) {
			CPU_Flags |= CPU_ARMv7;
		}
	}

	Com_sprintf( vendor, 100, "ARM %s", platform );
	hwcaps = getauxval( AT_HWCAP );
	if ( hwcaps & ( HWCAP_IDIVA | HWCAP_VFPv3 ) ) {
		strcat( vendor, " /w" );

		if ( hwcaps & HWCAP_IDIVA ) {
			CPU_Flags |= CPU_IDIVA;
			strcat( vendor, " IDIVA" );
		}

		if ( hwcaps & HWCAP_VFPv3 ) {
			CPU_Flags |= CPU_VFPv3;
			strcat( vendor, " VFPv3" );
		}

		if ( ( CPU_Flags & ( CPU_ARMv7 | CPU_VFPv3 ) ) == ( CPU_ARMv7 | CPU_VFPv3 ) ) {
			strcat( vendor, " QVM-bytecode" );
		}
	}
#else // !arm32
	CPU_Flags = 0;
#if arm64
	Com_sprintf( vendor, 100, "%s", ARCH_STRING );
#else
	Com_sprintf( vendor, 128, "%s %s", ARCH_STRING, (const char*)getauxval( AT_PLATFORM ) );
#endif
#endif // !arm32
}

#endif // __linux__

#endif // non-x86

/*
================
Sys_SnapVector
================
*/
#ifdef _MSC_VER
#if idx64
void Sys_SnapVector( float *vector )
{
	__m128 vf0, vf1, vf2;
	__m128i vi;
	DWORD mxcsr;

	mxcsr = _mm_getcsr();
	vf0 = _mm_setr_ps( vector[0], vector[1], vector[2], 0.0f );

	_mm_setcsr( mxcsr & ~0x6000 ); // enforce rounding mode to "round to nearest"

	vi = _mm_cvtps_epi32( vf0 );
	vf0 = _mm_cvtepi32_ps( vi );

	vf1 = _mm_shuffle_ps(vf0, vf0, _MM_SHUFFLE(1,1,1,1));
	vf2 = _mm_shuffle_ps(vf0, vf0, _MM_SHUFFLE(2,2,2,2));

	_mm_setcsr( mxcsr ); // restore rounding mode

	_mm_store_ss( &vector[0], vf0 );
	_mm_store_ss( &vector[1], vf1 );
	_mm_store_ss( &vector[2], vf2 );
}
#endif // idx64

#if id386
void Sys_SnapVector( float *vector )
{
	static const DWORD cw037F = 0x037F;
	DWORD cwCurr;
__asm {
	fnstcw word ptr [cwCurr]
	mov ecx, vector
	fldcw word ptr [cw037F]

	fld   dword ptr[ecx+8]
	fistp dword ptr[ecx+8]
	fild  dword ptr[ecx+8]
	fstp  dword ptr[ecx+8]

	fld   dword ptr[ecx+4]
	fistp dword ptr[ecx+4]
	fild  dword ptr[ecx+4]
	fstp  dword ptr[ecx+4]

	fld   dword ptr[ecx+0]
	fistp dword ptr[ecx+0]
	fild  dword ptr[ecx+0]
	fstp  dword ptr[ecx+0]

	fldcw word ptr cwCurr
	}; // __asm
}
#endif // id386

#if arm64 || arm32
void Sys_SnapVector( float *vector )
{
	vector[0] = rint( vector[0] );
	vector[1] = rint( vector[1] );
	vector[2] = rint( vector[2] );
}
#endif

#else // clang/gcc/mingw

#if id386

#define QROUNDX87(src) \
	"flds " src "\n" \
	"fistpl " src "\n" \
	"fildl " src "\n" \
	"fstps " src "\n"

void Sys_SnapVector( float *vector )
{
	static const unsigned short cw037F = 0x037F;
	unsigned short cwCurr;

	__asm__ volatile
	(
		"fnstcw %1\n" \
		"fldcw %2\n" \
		QROUNDX87("0(%0)")
		QROUNDX87("4(%0)")
		QROUNDX87("8(%0)")
		"fldcw %1\n" \
		:
		: "r" (vector), "m"(cwCurr), "m"(cw037F)
		: "memory", "st"
	);
}

#else // idx64, non-x86

void Sys_SnapVector( float *vector )
{
	vector[0] = rint( vector[0] );
	vector[1] = rint( vector[1] );
	vector[2] = rint( vector[2] );
}

#endif

#endif // clang/gcc/mingw

#ifdef USE_AFFINITY_MASK

static int hex_code( const int code ) {
	if ( code >= '0' && code <= '9' ) {
		return code - '0';
	}
	if ( code >= 'A' && code <= 'F' ) {
		return code - 'A' + 10;
	}
	if ( code >= 'a' && code <= 'f' ) {
		return code - 'a' + 10;
	}
	return -1;
}


static const char *parseAffinityMask( const char *str, uint64_t *outv, int level ) {
	uint64_t v, mask = 0;

	while ( *str != '\0' ) {
		if ( *str == 'A' || *str == 'a' ) {
			mask = affinityMask;
			++str;
			continue;
		}
		if ( *str == 'P' || *str == 'p' ) {
			mask = pCoreMask;
			++str;
			continue;
		}
		if ( *str == 'E' || *str == 'e' ) {
			mask = eCoreMask;
			++str;
			continue;
		}
		if ( *str == '0' && ( str[1] == 'x' || str[1] == 'X' ) && ( v = hex_code( str[2] ) ) >= 0 ) {
			int hex;
			str += 3; // 0xH
			while ( ( hex = hex_code( *str ) ) >= 0 ) {
				v = v * 16 + hex;
				str++;
			}
			mask = v;
			continue;
		}
		if ( *str >= '0' && *str <= '9' ) {
			mask = *str++ - '0';
			while ( *str >= '0' && *str <= '9' ) {
				mask = mask * 10 + *str - '0';
				++str;
			}
			continue;
		}

		if ( level == 0 ) {
			while ( *str == '+' || *str == '-' ) {
				str = parseAffinityMask( str + 1, &v, level + 1 );
				switch ( *str ) {
					case '+': mask |= v; break;
					case '-': mask &= ~v; break;
					default: str = ""; break;
				}
			}
			if ( *str != '\0' ) {
				++str; // skip unknown characters
			}
		} else {
			break;
		}
	}

	*outv = mask;
	return str;
}


// parse and set affinity mask
static void Com_SetAffinityMask( const char *str )
{
	uint64_t mask = 0;

	parseAffinityMask( str, &mask, 0 );

	if ( ( mask & affinityMask ) == 0 ) {
		mask = affinityMask; // reset to default
	}

	if ( mask != 0 ) {
		Sys_SetAffinityMask( mask );
	}
}

static void Com_AffinityMaskChanged( cvar_t *self )
{
	Com_SetAffinityMask( self->string );
}
#endif // USE_AFFINITY_MASK


/*
=================
Com_SetCvarsFromEnvironment

12-factor app support: read WIRED_* environment variables and set them as cvars.
Mapping: WIRED_SV_HOSTNAME → sv_hostname, WIRED_G_GAMETYPE → g_gametype, etc.

The WIRED_ prefix is stripped, and the remainder is lowercased to form the cvar name.
This allows server operators to configure the wired engine via environment variables in
Docker, systemd, k8s, or any 12-factor deployment:

  export WIRED_SV_HOSTNAME="My Server"
  export WIRED_SV_MAXCLIENTS=16
  export WIRED_G_GAMETYPE=4
  export WIRED_MAP=arena7
  export WIRED_SV_WIREDNETAUTHTOKEN="observer:member:user:abc123"
  ./wired-ded

Special vars:
  WIRED_MAP  → executed as "map <value>" after full init (not a cvar)
  WIRED_EXEC → executed as "exec <value>" after full init
=================
*/

// environment variables we know about — mapped to cvars
static const struct {
	const char *env;      // env var name (after WIRED_ prefix)
	const char *cvar;     // cvar name
} wn_env_map[] = {
	{ "SV_HOSTNAME",        	"sv_hostname" },
	{ "SV_MAXCLIENTS",      	"sv_maxclients" },
	{ "SV_PURE",            	"sv_pure" },
	{ "SV_FPS",             	"sv_fps" },
	{ "DEDICATED",          	"dedicated" },
	{ "G_GAMETYPE",         	"g_gametype" },
	{ "G_SCORELIMIT",       	"g_scorelimit" },
	{ "G_TIMELIMIT",        	"g_timelimit" },
	{ "NET_PORT",           	"net_port" },
	{ "COM_HUNKMEGS",       	"com_hunkmegs" },
	{ "SV_WIREDNETAUTHTOKEN",   "sv_wirednetAuthToken" },
	{ "SV_WIREDNETMAXCLIENTS",  "sv_wirednetMaxClients" },
	{ "SV_WIREDNETSTATERATE",   "sv_wirednetStateRate" },
	{ "SV_WIRDNETEVENTRATE",    "sv_wirednetEventRate" },
	{ "SV_WIREDNETRECORD",      "sv_wirednetRecord" },
	{ "RCONPASSWORD",       	"sv_wiredRconPassword" },
	{ "LOGFILE",            	"log_file_enabled" },
	{ NULL, NULL }
};

static void Com_SetCvarsFromEnvironment( void )
{
	const char *val;
	int count = 0;

	for ( int i = 0; wn_env_map[i].env != NULL; i++ ) {
		char envname[128];
		Com_sprintf( envname, sizeof(envname), "WIRED_%s", wn_env_map[i].env );
		val = getenv( envname );
		if ( val && *val ) {
			Cvar_Set( wn_env_map[i].cvar, val );
			Com_Log( SEV_INFO, LOG_CH(ch_system), "ENV: %s = \"%s\" → %s\n", envname, val, wn_env_map[i].cvar );
			count++;
		}
	}

	if ( count > 0 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "12-factor: %d cvars set from environment.\n", count );
	}
}


/*
=================
Com_Init
=================
*/
void Com_Init( char *commandLine ) {
	const char *s;
	int	qport;

	// get the initial time base
	Sys_Milliseconds();
	Sys_SetMainThreadPolicy();

	// Fallback stderr sink: plain write(2), no cvars, no filesystem.
	// Must be the very first act so banner and early errors reach stderr.
	if ( !Log_RegisterFallbackStderrSink() ) {
		Sys_Error( "Log pipeline: Sys_MutexInit failed" );
	}

	Com_Log( SEV_INFO, LOG_CH(ch_system), "%s %s %s\n", WIRED_ENGINE_RELEASE_VERSION, PLATFORM_STRING, __DATE__ );

	Hash_SelfTest();

	if ( Q_setjmp( (void **)abortframe ) ) {
		Sys_Error ("Error during initialization");
	}

	// bk001129 - do this before anything else decides to push events
	Com_InitPushEvent();

	Com_InitSmallZoneMemory();
	Cvar_Init();

	// Global log gate: registered immediately after Cvar_Init so its
	// onChange callback (wired in Log_InitChannels) can re-resolve every
	// channel's effectiveSev when the floor moves. Default INFO suppresses
	// TRACE/DEBUG at runtime.
	{
#ifdef _DEBUG
		static const cvarDesc_t d = CVAR_STRING( "log_severity", "DEBUG", 0, NULL );
#else
		static const cvarDesc_t d = CVAR_STRING( "log_severity", "INFO", 0, NULL );
#endif
		log_severity_cvar = Cvar_Register( &d );
	}
	Log_InitChannels();
	LogBuffer_Init();

	// TTY sink only needs cvars (con_severity / con_timestamp). Register it now
	// so FS_InitFilesystem, BSP_Init, and WiredScript_Init all emit with a
	// severity bracket and optional timestamp instead of raw fallback stderr.
	Log_RegisterTtySink();
	// Fallback served its purpose for the tiny pre-Cvar_Init window.
	Log_UnregisterFallbackStderrSink();

#if defined(_WIN32) && defined(_DEBUG)
	{
		static const cvarDesc_t d = CVAR_BOOL( "com_noErrorInterrupt", "0", 0, NULL );
		com_noErrorInterrupt = Cvar_Register( &d );
	}
#endif

#ifdef DEFAULT_GAME
	Cvar_Set( "fs_game", DEFAULT_GAME );
#endif

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	Com_ParseCommandLine( commandLine );

//	Swap_Init ();
	Cbuf_Init();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

	Com_InitZoneMemory();
	Cmd_Init();

	Com_StartupVariable( "journal" );
	{
		static const cvarDesc_t d = CVAR_INT( "journal", "0", CVAR_INIT | CVAR_PROTECTED,
			"When enabled, writes events and its data to 'journal.dat' and 'journaldata.dat'.", 0, 2 );
		com_journal = Cvar_Register( &d );
	}

	Com_StartupVariable( "sv_master1" );
	Cvar_Get( "sv_master1", MASTER_SERVER_NAME, CVAR_INIT );

	{
		static const cvarDesc_t d = CVAR_INT( "protocol", XSTRING( PROTOCOL_VERSION ), 0,
			"Specify network protocol version number.", 0, 0 );
		com_protocol = Cvar_Register( &d );
	}
	com_protocol->flags &= ~CVAR_USER_CREATED;
	com_protocol->flags |= CVAR_SERVERINFO | CVAR_ROM;

	// done early so bind command exists
	Com_InitKeyCommands();

	FS_InitFilesystem();

	// File sink needs VFS (fs_homepath set inside FS_InitFilesystem).
	// no-op if log_file_enabled == 0.
	Log_RegisterFileSink();

	// initialize BSP format registry (FEAT_BSP_ABSTRACTION)
	BSP_Init();
	Cvar_Get( "com_mapAssetProfile", "modern", CVAR_ROM );
	Cvar_Get( "com_mapBspVersion", "0", CVAR_ROM );

	// Per-map metadata arena. Must come after FS_InitFilesystem (the
	// scan reads .meta / .arena sidecars) but before any consumer queries
	// maps_list[]. Maps_ScanAll is invoked once here and again at the end
	// of FS_Restart.
	Maps_InitArena();
	Maps_ScanAll();

	WiredCore_Init();

	// Console sink registered late: it requires the ring buffer / Con_Init,
	// which depends on renderer init. TTY and file sinks are already live.
	// Compiled out in dedicated builds.
#ifndef DEDICATED
	Log_RegisterConsoleSink();
#endif

	Com_InitJournaling();

	Com_ExecuteCfg();

	// 12-factor: override config with WIRED_* environment variables.
	// Precedence: defaults < config files < env vars < command-line args.
	// Env var format: WIRED_CVAR_NAME=value (underscores map to underscores in cvar names).
	// Example: WIRED_SV_HOSTNAME="My Server" → +set sv_hostname "My Server"
	Com_SetCvarsFromEnvironment();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

	// get dedicated here for proper hunk megs initialization
	{
#ifdef DEDICATED
		static const cvarDesc_t d = CVAR_INT( "dedicated", "1", CVAR_INIT,
			"Enables dedicated server mode.\n 0: Listen server\n 1: Unlisted dedicated server \n 2: Listed dedicated server", 1, 2 );
#else
		static const cvarDesc_t d = CVAR_INT( "dedicated", "0", CVAR_LATCH,
			"Enables dedicated server mode.\n 0: Listen server\n 1: Unlisted dedicated server \n 2: Listed dedicated server", 0, 2 );
#endif
		com_dedicated = Cvar_Register( &d );
	}
	com_dedicated->onChange = Com_DedicatedChanged;
	// allocate the stack based hunk allocator
	Com_InitHunkMemory();

	// if any archived cvars are modified after this, we will trigger a writing
	// of the config file
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	//
	// init commands and vars
	//
#ifndef DEDICATED
	{
		static const cvarDesc_t d = CVAR_INT( "com_maxfps", "250", 0,
			"Sets maximum frames per second.", 0, 1000 );
		com_maxfps = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_INT( "com_maxfpsUnfocused", "60", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Sets maximum frames per second in unfocused game window.", 0, 1000 );
		com_maxfpsUnfocused = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_INT( "com_maxfpsMinimized", "5", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Sets maximum frames per second while the game window is minimized.\n"
			" Lower values reduce CPU/GPU use when the user is not looking at the game.\n"
			" Takes precedence over com_maxfpsUnfocused when minimized.", 1, 30 );
		com_maxfpsMinimized = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_INT( "com_yieldCPU", "1", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Attempt to sleep specified amount of time between rendered frames when game is active, this will greatly reduce CPU load. Use 0 only if you're experiencing some lag.", 0, 16 );
		com_yieldCPU = Cvar_Register( &d );
	}
#endif

	// com_busyWait: choose the frame limiter strategy.
	//   0: sleep most of the frame then busy-wait the last 2 ms (default)
	//   1: pure busy-wait (highest precision, pegs a CPU core)
	// When enabled, overrides com_yieldCPU because busy-waiting implies we
	// don't want to yield to the OS.
	{
		static const cvarDesc_t d = CVAR_BOOL( "com_busyWait", "0", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Frame rate limiter strategy:\n"
			" 0: hybrid sleep + 2 ms busy-wait (low CPU)\n"
			" 1: pure busy-wait (highest precision, pegs a core)\n"
			"Setting this to 1 overrides com_yieldCPU." );
		com_busyWait = Cvar_Register( &d );
	}

	// Crash reporter — must be initialised before Sys_Init so that the
	// platform-specific handler installer can find com_crashReport.
	Crash_Init();

#ifdef USE_AFFINITY_MASK
	{
		static const cvarDesc_t d = CVAR_STRING( "com_affinityMask", "", CVAR_ARCHIVE | CVAR_NODEFAULT,
			"Bind game process to bitmask-specified CPU core(s), special characters:\n A or a - all default cores\n P or p - performance cores\n E or e - efficiency cores\n 0x<value> - use hexadecimal notation\n + or - can be used to add or exclude particular cores" );
		com_affinityMask = Cvar_Register( &d );
	}
	com_affinityMask->onChange = Com_AffinityMaskChanged;
#endif

	// com_blood = Cvar_Get( "com_blood", "1", CVAR_ARCHIVE | CVAR_NODEFAULT );

	{
		static const cvarDesc_t d = CVAR_FLOAT( "timescale", "1", CVAR_CHEAT | CVAR_SYSTEMINFO,
			"System timing factor:\n < 1: Slows the game down\n = 1: Regular speed\n > 1: Speeds the game up", 0, 0 );
		com_timescale = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_BOOL( "fixedtime", "0", CVAR_CHEAT,
			"Toggle the rendering of every frame the game will wait until each frame is completely rendered before sending the next frame." );
		com_fixedtime = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_BOOL( "com_showtrace", "0", CVAR_CHEAT,
			"Debugging tool that prints out trace information." );
		com_showtrace = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_BOOL( "viewlog", "0", 0,
			"Toggle the display of the startup console window over the game screen." );
		com_viewlog = Cvar_Register( &d );
	}
	com_viewlog->onChange = Com_ViewlogChanged;
	{
		static const cvarDesc_t d = CVAR_BOOL( "com_speeds", "0", 0,
			"Prints speed information per frame to the console. Used for debugging." );
		com_speeds = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_BOOL( "com_cameraMode", "0", CVAR_CHEAT, NULL );
		com_cameraMode = Cvar_Register( &d );
	}

#ifndef DEDICATED
	{
		static const cvarDesc_t d = CVAR_BOOL( "timedemo", "0", 0,
			"When set to '1' times a demo and returns frames per second like a benchmark." );
		com_timedemo = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_BOOL( "cl_paused", "0", CVAR_ROM,
			"Read-only CVAR to toggle functionality of paused games (the variable holds the status of the paused flag on the client side)." );
		cl_paused = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_INT( "cl_packetdelay", "0", CVAR_CHEAT,
			"Artificially set the client's latency. Simulates packet delay, which can lead to packet loss.", 0, 0 );
		cl_packetdelay = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_BOOL( "cl_running", "0", CVAR_ROM | CVAR_NOTABCOMPLETE,
			"Can be used to check the status of the client game." );
		com_cl_running = Cvar_Register( &d );
	}
#endif

	{
		static const cvarDesc_t d = CVAR_BOOL( "sv_paused", "0", CVAR_ROM, NULL );
		sv_paused = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_INT( "sv_packetdelay", "0", CVAR_CHEAT,
			"Simulates packet delay, which can lead to packet loss. Server side.", 0, 0 );
		sv_packetdelay = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_BOOL( "sv_running", "0", CVAR_ROM | CVAR_NOTABCOMPLETE,
			"Communicates to game modules if there is a server currently running." );
		com_sv_running = Cvar_Register( &d );
	}

	Cvar_Get( "com_errorMessage", "", CVAR_ROM | CVAR_NORESTART );

#ifndef DEDICATED
	{
		static const cvarDesc_t d = CVAR_BOOL( "com_introplayed", "0", CVAR_ARCHIVE,
			"Skips the introduction cinematic." );
		com_introPlayed = Cvar_Register( &d );
	}
	{
		static const cvarDesc_t d = CVAR_BOOL( "com_skipIdLogo", "0", CVAR_ARCHIVE,
			"Skip playing Id Software logo cinematic at startup." );
		com_skipIdLogo = Cvar_Register( &d );
	}
#endif

	if ( com_dedicated->integer ) {
		if ( !com_viewlog->integer ) {
			Cvar_Set( "viewlog", "1" );
		}
		gw_minimized = qtrue;
	} else {
		gw_minimized = qfalse;
	}

#ifdef _DEBUG
	Cmd_AddCommand( "error", Com_Error_f );
	Cmd_AddCommand( "crash", Com_Crash_f );
	Cmd_AddCommand( "freeze", Com_Freeze_f );
#endif

	Cmd_AddCommand( "quit", Com_Quit_f );
	Cmd_AddCommand( "changeVectors", MSG_ReportChangeVectors_f );
	// writeconfig registered in Cvar_Init (wired/cvar/cvar.c)
	Cmd_AddCommand( "game_restart", Com_GameRestart_f );

	Help_Init();

	s = va( "%s %s %s", WIRED_ENGINE_VERSION, PLATFORM_STRING, __DATE__ );
	com_version = Cvar_Get( "version", s, CVAR_PROTECTED | CVAR_ROM | CVAR_SERVERINFO );
	Cvar_SetDescription( com_version, "Read-only CVAR to see the version of the game." );

	// this cvar is the single entry point of the entire extension system
	Cvar_Get( "//trap_GetValue", va( "%i", COM_TRAP_GETVALUE ), CVAR_PROTECTED | CVAR_ROM | CVAR_NOTABCOMPLETE );

	Sys_Init();

	// Install platform crash handlers once the system layer is ready.
	Crash_InstallHandlers();

	// CPU detection — observational: no consumer before this point reads sys_cpustring,
	// eCoreMask, pCoreMask, or affinityMask (renderer init is deferred until CL_Init).
	// Order is safe to leave here; only move if a pre-renderer SIMD consumer is added.
	Cvar_Get( "sys_cpustring", "detect", CVAR_PROTECTED | CVAR_ROM | CVAR_NORESTART );
	if ( !Q_stricmp( Cvar_VariableString( "sys_cpustring" ), "detect" ) ) {
		char vendor[128];
		Com_Log( SEV_INFO, LOG_CH(ch_system), "...detecting CPU, found " );
		Sys_GetProcessorId( vendor );
		Cvar_Set( "sys_cpustring", vendor );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_system), "%s\n", Cvar_VariableString( "sys_cpustring" ) );

#ifdef USE_AFFINITY_MASK
	// get initial process affinity - we will respect it when setting custom affinity masks
	eCoreMask = pCoreMask = affinityMask = Sys_GetAffinityMask();
#if (idx64 || id386)
	DetectCPUCoresConfig();
#endif
	if ( com_affinityMask->string[0] != '\0' ) {
		Com_SetAffinityMask( com_affinityMask->string );
	}
#endif

	// Pick a random port value
	Com_RandomBytes( (byte*)&qport, sizeof( qport ) );
	/* Netchan_Init removed — Phase D: netchan replaced by QUIC transport */

	VM_Init();

#ifndef DEDICATED
	// Load character archetypes before SV_Init so BotLua preload finds them at init time.
	CL_Characters_Init();
#endif

	SV_Init();

#ifndef DEDICATED
	if ( !com_dedicated->integer ) {
		CL_Init();
		// Sys_ShowConsole( com_viewlog->integer, qfalse ); // moved down
	}
#endif

	WiredScript_PostInit();

	// add + commands from command line
	if ( !Com_AddStartupCommands() ) {
		// if the user didn't give any commands, run default action
		if ( !com_dedicated->integer ) {
#ifndef DEDICATED
			if ( !com_skipIdLogo || !com_skipIdLogo->integer )
				Cbuf_AddText( "cinematic idlogo.RoQ\n" );
			if( !com_introPlayed->integer ) {
				Cvar_Set( com_introPlayed->name, "1" );
				Cvar_Set( "nextmap", "cinematic intro.RoQ" );
			}
#endif
		}
	}

#ifndef DEDICATED
	CL_StartHunkUsers();
#endif

	// set com_frameTime so that if a map is started on the
	// command line it will still be able to count on com_frameTime
	// being random enough for a serverid
	// lastTime = com_frameTime = Com_Milliseconds();
	Com_FrameInit();

	if ( !com_errorEntered )
		Sys_ShowConsole( com_viewlog->integer, qfalse );

#ifndef DEDICATED
	// make sure single player is off by default
	Cvar_Set( "ui_singlePlayerActive", "0" );
#endif

	// 12-factor: WIRED_MAP and WIRED_EXEC env vars execute commands after full init.
	// These run AFTER Com_AddStartupCommands, so command-line +map overrides WIRED_MAP.
	{
		const char *envMap = getenv( "WIRED_MAP" );
		const char *envExec = getenv( "WIRED_EXEC" );
		if ( envExec && *envExec ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "ENV: WIRED_EXEC = \"%s\"\n", envExec );
			Cbuf_AddText( va( "exec %s\n", envExec ) );
			Cbuf_Execute();
		}
		if ( envMap && *envMap ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "ENV: WIRED_MAP = \"%s\"\n", envMap );
			Cbuf_AddText( va( "map %s\n", envMap ) );
		}
	}

	com_fullyInitialized = qtrue;

	Com_Log( SEV_INFO, LOG_CH(ch_system), "--- Common Initialization Complete ---\n" );

	NET_Init();

	Com_Log( SEV_INFO, LOG_CH(ch_system), "Working directory: %s\n", Sys_Pwd() );
}


// Com_WriteConfigToFile, Com_WriteConfigToFileForced, Com_WriteConfiguration,
// Com_WriteConfig_f — moved to wired/cvar/cvar.c Phase 1 Group 3


/*
================
Com_ModifyMsec
================
*/
static int Com_ModifyMsec( int msec ) {
	int		clampTime;

	//
	// modify time for debugging values
	//
	if ( com_fixedtime->integer ) {
		msec = com_fixedtime->integer;
	} else if ( com_timescale->value ) {
		msec *= com_timescale->value;
	} else if (com_cameraMode->integer) {
		msec *= com_timescale->value;
	}

	// don't let it scale below 1 msec
	if ( msec < 1 && com_timescale->value) {
		msec = 1;
	}

	if ( com_dedicated->integer ) {
		// dedicated servers don't want to clamp for a much longer
		// period, because it would mess up all the client's views
		// of time.
		if (com_sv_running->integer && msec > 500)
			Com_Log( SEV_INFO, LOG_CH(ch_system), "Hitch warning: %i msec frame time\n", msec );

		clampTime = 5000;
	} else if ( !com_sv_running->integer ) {
		// clients of remote servers do not want to clamp time, because
		// it would skew their view of the server's time temporarily
		clampTime = 5000;
	} else {
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clampTime = 200;
	}

	if ( msec > clampTime ) {
		msec = clampTime;
	}

	return msec;
}


/*
=================
Com_TimeVal
=================
*/
static int Com_TimeVal( int minMsec )
{
	int timeVal = Com_Milliseconds() - com_frameTime;

	if ( timeVal >= minMsec )
		timeVal = 0;
	else
		timeVal = minMsec - timeVal;

	return timeVal;
}


static int Com_TimeValUsec( int minUsec )
{
	int timeVal = (int)( Sys_Microseconds() - com_frameTimeUsec );
	if ( timeVal >= minUsec )
		return 0;
	return minUsec - timeVal;
}

/*
=================
Com_FrameInit
=================
*/
void Com_FrameInit( void )
{
	lastTime = com_frameTime = Com_Milliseconds();
	lastTimeUsec = com_frameTimeUsec = Sys_Microseconds();
}

static void Com_ViewlogChanged( cvar_t *self ) {
	if ( !com_dedicated->integer ) {
		Sys_ShowConsole( self->integer, qfalse );
	}
}

static void Com_DedicatedChanged( cvar_t *self ) {
	Cvar_Get( "dedicated", "0", 0 );
	if ( !self->integer ) {
		SV_Shutdown( "dedicated set to 0" );
		SV_RemoveDedicatedCommands();
#ifndef DEDICATED
		CL_Init();
#endif
		Sys_ShowConsole( com_viewlog->integer, qfalse );
#ifndef DEDICATED
		gw_minimized = qfalse;
		CL_StartHunkUsers();
#endif
	} else {
#ifndef DEDICATED
		CL_Shutdown( "", qfalse );
		CL_ClearMemory();
#endif
		Sys_ShowConsole( 1, qtrue );
		SV_AddDedicatedCommands();
		gw_minimized = qtrue;
	}
}

/*
=================
Com_Frame
=================
*/
void Com_Frame( qboolean noDelay ) {

#ifndef DEDICATED
	static int biasUsec = 0;
#endif

	if ( Q_setjmp( (void **)abortframe ) ) {
#ifndef DEDICATED
		CL_AbortFrame();	// reset SCR_UpdateScreen guard on ERR_DROP recovery
#endif
		return;			// an ERR_DROP was thrown
	}

	int minUsec = 0; // silent compiler warning

	// bk001204 - init to zero.
	//  also:  might be clobbered by `longjmp' or `vfork'
	int timeBeforeFirstEvents = 0;
	int timeBeforeServer = 0;
	int timeBeforeEvents = 0;
	int timeBeforeClient = 0;
	int timeAfter = 0;
	memset( &cl_prof, 0, sizeof( cl_prof ) );

	// write config file if anything changed
#ifndef DELAY_WRITECONFIG
	Com_WriteConfiguration();
#endif

	//
	// main event loop
	//
	if ( com_speeds->integer ) {
		timeBeforeFirstEvents = Sys_Milliseconds();
	}

	// we may want to spin here if things are going too fast
	if ( com_dedicated->integer ) {
		minUsec = SV_FrameMsec() * 1000;
#ifndef DEDICATED
		biasUsec = 0;
#endif
	} else {
#ifndef DEDICATED
		if ( noDelay ) {
			minUsec = 0;
			biasUsec = 0;
		} else {
			int targetUsec;
			int elapsedUsec;
			// Frame pacing priority:
			//   1. Window minimized     -> com_maxfpsMinimized (default 5)
			//   2. Window unfocused     -> com_maxfpsUnfocused (default 60)
			//   3. Normal / focused     -> com_maxfps
			//   4. No cap               -> minUsec = 1000 (1ms)
			if ( gw_minimized && com_maxfpsMinimized->integer > 0 )
				targetUsec = 1000000 / com_maxfpsMinimized->integer;
			else if ( !gw_active && com_maxfpsUnfocused->integer > 0 )
				targetUsec = 1000000 / com_maxfpsUnfocused->integer;
			else if ( com_maxfps->integer > 0 )
				targetUsec = 1000000 / com_maxfps->integer;
			else
				targetUsec = 1000;

			elapsedUsec = (int)( com_frameTimeUsec - lastTimeUsec );
			biasUsec += elapsedUsec - targetUsec;

			if ( biasUsec > targetUsec )
				biasUsec = targetUsec;

			// Adjust minUsec if previous frame took too long to render so
			// that framerate is stable at the requested value.
			minUsec = targetUsec - biasUsec;
		}
#endif
	}

	// waiting for incoming packets
	//
	// Frame limiter strategy, controlled by com_busyWait:
	//   0: hybrid — sleep for (timeValUsec-2000) µs, busy-wait the final 2 ms
	//   1: pure busy-wait (highest precision, pegs a core)
	// Busy-wait mode overrides com_yieldCPU because the whole point is to
	// not yield to the OS.
	int timeValUsec;
	int timeValSV;
	int sleepUsec;
	if ( noDelay == qfalse ) {
		do {
		if ( com_sv_running->integer ) {
			timeValSV = SV_SendQueuedPackets();
			timeValUsec = Com_TimeValUsec( minUsec );
			if ( timeValSV * 1000 < timeValUsec )
				timeValUsec = timeValSV * 1000;
		} else {
			timeValUsec = Com_TimeValUsec( minUsec );
		}

		if ( com_busyWait->integer ) {
			// Pure busy-wait: never sleep, just poll events and spin.
#ifndef DEDICATED
			Com_EventLoop();
#endif
			// NET_Sleep with a tiny timeout lets us pull incoming packets
			// without actually yielding for a measurable period.
			NET_Sleep( 0 );
		} else {
			sleepUsec = timeValUsec;
#ifndef DEDICATED
			if ( !gw_minimized && timeValUsec > com_yieldCPU->integer * 1000 )
				sleepUsec = com_yieldCPU->integer * 1000;
			// Reserve the last 2 ms for a busy-wait to tighten the frame
			// boundary; sleep the rest via NET_Sleep as before.
			if ( sleepUsec > 2000 )
				sleepUsec -= 2000;
			else if ( sleepUsec > 0 )
				sleepUsec = 0;
			if ( timeValUsec > sleepUsec )
				Com_EventLoop();
#endif
			NET_Sleep( sleepUsec - 500 );
		}
		} while( Com_TimeValUsec( minUsec ) );
	}

	lastTime = com_frameTime;
	lastTimeUsec = com_frameTimeUsec;
	com_frameTime = Com_EventLoop();
	com_frameTimeUsec = Sys_Microseconds();
	int realMsec = com_frameTime - lastTime;

	Cbuf_Execute();

	// mess with msec if needed
	int msec = Com_ModifyMsec( realMsec );

	//
	// server side
	//
	if ( com_speeds->integer ) {
		timeBeforeServer = Sys_Milliseconds();
	}

	// Advance one phase of the async spawn state machine.
	// Must run before SV_Frame so SV_Frame sees a consistent spawn state.
	SV_SpawnServer_Tick();

	SV_Frame( msec );

#ifdef DEDICATED
	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds ();
		timeBeforeEvents = timeAfter;
		timeBeforeClient = timeAfter;
	}
#else
	//
	// client system
	//
	if ( !com_dedicated->integer ) {
		//
		// run event loop a second time to get server to client packets
		// without a frame of latency
		//
		if ( com_speeds->integer ) {
			timeBeforeEvents = Sys_Milliseconds();
		}
		Com_EventLoop();

		// drain QUIC socket so snapshots sent by SV_Frame above reach
		// cgame this frame — mirrors vanilla NET_GetLoopPacket timing
		NET_Sleep( 0 );

		Cbuf_Execute();

		//
		// client side
		//
		if ( com_speeds->integer ) {
			timeBeforeClient = Sys_Milliseconds();
		}

		CL_Frame( msec, realMsec );

		if ( com_speeds->integer ) {
			timeAfter = Sys_Milliseconds();
		}
	}
#endif

	NET_FlushPacketQueue( 0 );

	Cbuf_Wait();

	//
	// report timing information
	//
	if ( com_speeds->integer ) {
		int			all, sv, ev, cl;

		all = timeAfter - timeBeforeServer;
		sv = timeBeforeEvents - timeBeforeServer;
		ev = timeBeforeServer - timeBeforeFirstEvents + timeBeforeClient - timeBeforeEvents;
		cl = timeAfter - timeBeforeClient;
		sv -= time_game;
		cl -= time_frontend + time_backend;

		if ( com_speeds->integer < 2 || all >= com_speeds->integer ) {
			Com_Log( SEV_INFO, LOG_CH(ch_system), "frame:%i all:%3i sv:%3i ev:%3i cl:%3i gm:%3i rf:%3i bk:%3i\n",
						com_frameNumber, all, sv, ev, cl, time_game, time_frontend, time_backend );
			if ( com_speeds->integer >= 2 ) {
				Com_Log( SEV_INFO, LOG_CH(ch_system), "  cl: store:%d send:%d resend:%d cgtime:%d cgr:%d whud:%d wui:%d cons:%d snd:%d scr:%d end:%d ui:%d misc:%d (us)\n",
					cl_prof.store, cl_prof.send, cl_prof.resend, cl_prof.cgtime,
					cl_prof.cgr, cl_prof.whud, cl_prof.wui, cl_prof.cons,
					cl_prof.sound, cl_prof.scrextra, cl_prof.endframe,
					cl_prof.userinfo, cl_prof.misc);
				Com_Log( SEV_INFO, LOG_CH(ch_system), "  ev: wnframe:%d relstr:%d snapdg:%d chkpkt:%d (us)\n",
					cl_prof.wnframe, cl_prof.relstr, cl_prof.snapdg, cl_prof.chkpkt);
				Com_Log( SEV_INFO, LOG_CH(ch_system), "  whud: load:%d sync:%d render:%d score:%d (us)\n",
					cl_prof.whud_load, cl_prof.whud_sync, cl_prof.whud_render, cl_prof.whud_score);
			}
		}
	}

	//
	// trace optimization tracking
	//
	if ( com_showtrace->integer ) {

		extern	int c_traces, c_brush_traces, c_patch_traces;
		extern	int	c_pointcontents;

		Com_Log( SEV_INFO, LOG_CH(ch_system), "%4i traces  (%ib %ip) %4i points\n", c_traces,
			c_brush_traces, c_patch_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_patch_traces = 0;
		c_pointcontents = 0;
	}

	AssetLog_Tick();

	com_frameNumber++;
}


/*
=================
Com_Shutdown
=================
*/
void Com_Shutdown( void ) {
	AssetLog_Flush( NULL );

	LogBuffer_Shutdown();
	WiredCore_Shutdown();

	Maps_ShutdownArena();
	BSP_Shutdown();

	// Drain and close built-in sinks cleanly before filesystem shuts down.
	Log_UnregisterFileSink();
#ifndef DEDICATED
	Log_UnregisterConsoleSink();
#endif
	Log_UnregisterTtySink();

	if ( com_journalFile != FS_INVALID_HANDLE ) {
		FS_FCloseFile( com_journalFile );
		com_journalFile = FS_INVALID_HANDLE;
	}

	if ( com_journalDataFile != FS_INVALID_HANDLE ) {
		FS_FCloseFile( com_journalDataFile );
		com_journalDataFile = FS_INVALID_HANDLE;
	}
}

//------------------------------------------------------------------------


/*
===========================================
command line completion
===========================================
*/

/*
==================
Field_Clear
==================
*/
void Field_Clear( field_t *edit ) {
	memset( edit->buffer, 0, sizeof( edit->buffer ) );
	edit->cursor = 0;
	edit->scroll = 0;
	/* Clearing the buffer also invalidates any active cycling-completion
	 * state. */
	edit->acOffset = 0;
	edit->acLength = 0;
	edit->acMatchIndex = 0;
	edit->selActive = qfalse;
}

static const char *completionString;
static char shortestMatch[MAX_TOKEN_CHARS];
static int	matchCount;
// field we are working on, passed to Field_AutoComplete(&g_consoleCommand for instance)
static field_t *completionField;

/* Match list for cycling auto-completion (con_completionStyle 1).
 * Populated on the first Tab of a cycle, indexed through on each
 * subsequent Tab until the input line changes.
 * Each entry stores a cvar or command name so MAX_CYCLE_TOKEN is plenty. */
#define MAX_CYCLE_MATCHES 2048
#define MAX_CYCLE_TOKEN   128
static char cycleMatches[MAX_CYCLE_MATCHES][MAX_CYCLE_TOKEN];
static int  cycleMatchCount;
static qboolean cycleCollecting; /* true while FindMatches should store hits */

static cvar_t *con_completionStyle;

/*
===============
FindMatches
===============
*/
static void FindMatches( const char *s ) {
	int		i, n;

	if ( Q_stricmpn( s, completionString, strlen( completionString ) ) ) {
		return;
	}

	/* during a cycling-completion pass we also accumulate every
	 * matching string so the Tab handler can step through them on
	 * subsequent presses. */
	if ( cycleCollecting && cycleMatchCount < MAX_CYCLE_MATCHES ) {
		Q_strncpyz( cycleMatches[cycleMatchCount], s, sizeof( cycleMatches[0] ) );
		cycleMatchCount++;
	}

	matchCount++;
	if ( matchCount == 1 ) {
		Q_strncpyz( shortestMatch, s, sizeof( shortestMatch ) );
		return;
	}

	n = (int)strlen(s);
	// cut shortestMatch to the amount common with s
	for ( i = 0 ; shortestMatch[i] ; i++ ) {
		if ( i >= n ) {
			shortestMatch[i] = '\0';
			break;
		}

		if ( tolower(shortestMatch[i]) != tolower(s[i]) ) {
			shortestMatch[i] = '\0';
		}
	}
}


/*
===============
PrintMatches
===============
*/
static void PrintMatches( const char *s ) {
	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_system), "    %s\n", s );
	}
}


/*
===============
PrintCvarMatches
===============
*/
static void PrintCvarMatches( const char *s ) {
	char value[ TRUNCATE_LENGTH ];

	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_TruncateLongString( value, Cvar_VariableString( s ) );
		Com_Log( SEV_INFO, LOG_CH(ch_system), "    %s = \"%s\"\n", s, value );
	}
}


/*
===============
Field_FindFirstSeparator
===============
*/
static const char *Field_FindFirstSeparator( const char *s )
{
	char c;
	while ( (c = *s) != '\0' ) {
		if ( c == ';' )
			return s;
		s++;
	}
	return NULL;
}


/*
===============
Field_AddSpace
===============
*/
static void Field_AddSpace( void )
{
	size_t len = strlen( completionField->buffer );
	if ( len && len < sizeof( completionField->buffer ) - 1 && completionField->buffer[ len - 1 ] != ' ' )
	{
		memcpy( completionField->buffer + len, " ", 2 );
		completionField->cursor = (int)(len + 1);
	}
}


/*
===============
Field_Complete
===============
*/
static qboolean Field_Complete( void )
{
	int completionOffset;
	const int styleCycle = ( cycleCollecting && con_completionStyle && con_completionStyle->integer == 1 ) ? 1 : 0;

	if( matchCount == 0 )
		return qtrue;

	completionOffset = strlen( completionField->buffer ) - strlen( completionString );

	if ( styleCycle && matchCount >= 2 ) {
		/* Cycling tab: insert the first collected match in full (not the
		 * shortest common prefix), record the cycle offset so subsequent
		 * tabs can replace it in-place.  No match list is printed. */
		Q_strncpyz( &completionField->buffer[ completionOffset ],
			cycleMatches[0],
			sizeof( completionField->buffer ) - completionOffset );
		completionField->cursor = (int)strlen( completionField->buffer );
		completionField->acOffset = completionOffset;
		completionField->acLength = (int)strlen( cycleMatches[0] );
		completionField->acMatchIndex = 0;
		return qtrue;
	}

	Q_strncpyz( &completionField->buffer[ completionOffset ], shortestMatch,
		sizeof( completionField->buffer ) - completionOffset );

	completionField->cursor = strlen( completionField->buffer );

	if( matchCount == 1 )
	{
		Field_AddSpace();
		return qtrue;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_system), "]%s\n", completionField->buffer );

	return qfalse;
}


/*
===============
Field_AdvanceCycle

Replaces the currently cycling token with the next entry from the
match list.  Returns qtrue when a cycle step was performed.
===============
*/
static qboolean Field_AdvanceCycle( field_t *field )
{
	if ( field->acOffset <= 0 || cycleMatchCount < 2 ) {
		return qfalse;
	}

	/* clamp previous index in case the list size shifted (defensive) */
	if ( field->acMatchIndex < 0 || field->acMatchIndex >= cycleMatchCount ) {
		field->acMatchIndex = 0;
	}

	int index = ( field->acMatchIndex + 1 ) % cycleMatchCount;
	field->acMatchIndex = index;

	int offset = field->acOffset;
	if ( offset > (int)sizeof( field->buffer ) - 1 ) {
		return qfalse;
	}

	int newLen = (int)strlen( cycleMatches[index] );
	if ( offset + newLen >= (int)sizeof( field->buffer ) ) {
		return qfalse;
	}

	Q_strncpyz( field->buffer + offset, cycleMatches[index],
		sizeof( field->buffer ) - offset );
	field->cursor = offset + newLen;
	field->acLength = newLen;
	return qtrue;
}


/*
===============
Field_CompleteKeyname
===============
*/
void Field_CompleteKeyname( void )
{
	matchCount = 0;
	shortestMatch[ 0 ] = '\0';

	Key_KeynameCompletion( FindMatches );

	if ( !Field_Complete() )
		Key_KeynameCompletion( PrintMatches );
}


/*
===============
Field_CompleteList

Completes the current argument against an arbitrary list produced by
an enumeration callback.  The enumeration function must call the
supplied callback once per candidate string.
===============
*/
void Field_CompleteList( void (*enumerate)( void (*cb)( const char *s ) ) )
{
	if ( enumerate == NULL )
		return;

	matchCount = 0;
	shortestMatch[ 0 ] = '\0';

	enumerate( FindMatches );

	if ( !Field_Complete() )
		enumerate( PrintMatches );
}


/*
===============
Field_CompleteKeyBind
===============
*/
void Field_CompleteKeyBind( int key )
{
	const char *value = Key_GetBinding( key );
	if ( value == NULL || *value == '\0' )
		return;

	int blen = (int)strlen( completionField->buffer );
	int vlen = (int)strlen( value );

	if ( Field_FindFirstSeparator( (char*)value ) )
	{
		value = va( "\"%s\"", value );
		vlen += 2;
	}

	if ( vlen + blen > sizeof( completionField->buffer ) - 1 )
	{
		//vlen = sizeof( completionField->buffer ) - 1 - blen;
		return;
	}

	memcpy( completionField->buffer + blen, value, vlen + 1 );
	completionField->cursor = blen + vlen;

	Field_AddSpace();
}


static void Field_CompleteCvarValue( const char *value, const char *current )
{
	if ( *value == '\0' )
		return;

	int blen = (int)strlen( completionField->buffer );
	int vlen = (int)strlen( value );

	if ( *current != '\0' )
	{
#if 0
		int clen = (int) strlen( current );
		if ( strncmp( value, current, clen ) == 0 ) // current value is a substring of new value
		{
			value += clen;
			vlen -= clen;
		}
		else // modification, nothing to complete
#endif
		{
			return;
		}
	}

	if ( Field_FindFirstSeparator( (char*)value ) )
	{
		value = va( "\"%s\"", value );
		vlen += 2;
	}

	if ( vlen + blen > sizeof( completionField->buffer ) - 1 )
	{
		//vlen = sizeof( completionField->buffer ) - 1 - blen;
		return;
	}

	if ( blen > 1 )
	{
		if ( completionField->buffer[ blen-1 ] == '"' && completionField->buffer[ blen-2 ] == ' ' )
		{
			completionField->buffer[ blen-- ] = '\0'; // strip starting quote
		}
	}

	memcpy( completionField->buffer + blen, value, vlen + 1 );
	completionField->cursor = vlen + blen;

	Field_AddSpace();
}


/*
===============
Field_CompleteFilename
===============
*/
void Field_CompleteFilename( const char *dir, const char *ext, qboolean stripExt, int flags )
{
	matchCount = 0;
	shortestMatch[ 0 ] = '\0';

	FS_FilenameCompletion( dir, ext, stripExt, FindMatches, flags );

	if ( !Field_Complete() )
		FS_FilenameCompletion( dir, ext, stripExt, PrintMatches, flags );
}


/*
===============
Field_CompleteCommand
===============
*/
void Field_CompleteCommand( const char *cmd, qboolean doCommands, qboolean doCvars )
{
	int	completionArgument;

	// Skip leading whitespace and quotes
	cmd = Com_SkipCharset( cmd, " \"" );

	Cmd_TokenizeStringIgnoreQuotes( cmd );
	completionArgument = Cmd_Argc();

	// If there is trailing whitespace on the cmd
	if( *( cmd + strlen( cmd ) - 1 ) == ' ' )
	{
		completionString = "";
		completionArgument++;
	}
	else
		completionString = Cmd_Argv( completionArgument - 1 );

#ifndef DEDICATED
	// Unconditionally add a '\' to the start of the buffer
	if ( completionField->buffer[ 0 ] && completionField->buffer[ 0 ] != '\\' )
	{
		if( completionField->buffer[ 0 ] != '/' )
		{
			// Buffer is full, refuse to complete
			if ( strlen( completionField->buffer ) + 1 >= sizeof( completionField->buffer ) )
				return;

			memmove( &completionField->buffer[ 1 ],
				&completionField->buffer[ 0 ],
				strlen( completionField->buffer ) + 1 );
			completionField->cursor++;
		}

		completionField->buffer[ 0 ] = '\\';
	}
#endif

	if ( completionArgument > 1 )
	{
		const char *baseCmd = Cmd_Argv( 0 );
		const char *p;

#ifndef DEDICATED
			// This should always be true
			if ( baseCmd[ 0 ] == '\\' || baseCmd[ 0 ] == '/' )
				baseCmd++;
#endif

		if( ( p = Field_FindFirstSeparator( cmd ) ) != NULL )
		{
 			Field_CompleteCommand( p + 1, qtrue, qtrue ); // Compound command
		}
		else
		{
			qboolean argumentCompleted = Cmd_CompleteArgument( baseCmd, cmd, completionArgument );
			if ( ( matchCount == 1 || argumentCompleted ) && doCvars )
			{
				if ( cmd[0] == '/' || cmd[0] == '\\' )
					cmd++;
				Cmd_TokenizeString( cmd );
				Field_CompleteCvarValue( Cvar_VariableString( Cmd_Argv( 0 ) ), Cmd_Argv( 1 ) );
			}
		}
	}
	else
	{
		if ( completionString[0] == '\\' || completionString[0] == '/' )
			completionString++;

		matchCount = 0;
		shortestMatch[ 0 ] = '\0';

		if ( completionString[0] == '\0' ) {
			return;
		}

		if ( doCommands )
			Cmd_CommandCompletion( FindMatches );

		if ( doCvars )
			Cvar_CommandCompletion( FindMatches );

		if ( !Field_Complete() )
		{
			// run through again, printing matches
			if ( doCommands )
				Cmd_CommandCompletion( PrintMatches );

			if ( doCvars )
				Cvar_CommandCompletion( PrintCvarMatches );
		}
	}
}


/*
===============
Field_ResetCompletionCycle

Drop any cached cycling-completion state on the field so the next Tab
starts a new match list.  Called by the key event handler whenever a
non-Tab key is pressed.
===============
*/
void Field_ResetCompletionCycle( field_t *field )
{
	if ( field == NULL ) {
		return;
	}
	field->acOffset = 0;
	field->acLength = 0;
	field->acMatchIndex = 0;
}


/*
===============
Field_AutoComplete

Perform Tab expansion
===============
*/
void Field_AutoComplete( field_t *field )
{
	int style;
	qboolean cycleValid;

	completionField = field;

	/* lazily register con_completionStyle so the cvar is available
	 * the very first time the user hits Tab. */
	if ( con_completionStyle == NULL ) {
		static const cvarDesc_t d = CVAR_INT( "con_completionStyle", "0", CVAR_ARCHIVE,
			"Console auto-completion behaviour:\n"
			"  0 = print every match and complete to the common prefix (Q3 default)\n"
			"  1 = cycle through matches on repeated Tab presses (Enemy Territory style)", 0, 1 );
		con_completionStyle = Cvar_Register( &d );
	}

	style = con_completionStyle->integer;

	/* If a cycle is already in progress, verify the buffer still holds
	 * the last inserted match at acOffset.  When it doesn't (because the
	 * user edited the line, or the platform-specific input loop did not
	 * route through Field_ResetCompletionCycle), drop the stale cycle
	 * and start a fresh completion pass. */
	cycleValid = qfalse;
	if ( style == 1 && field->acOffset > 0 && cycleMatchCount >= 2
		&& field->acMatchIndex >= 0 && field->acMatchIndex < cycleMatchCount ) {
		const int bufLen = (int)strlen( field->buffer );
		const int expected = field->acOffset + field->acLength;
		if ( bufLen == expected
			&& field->acOffset < bufLen
			&& Q_stricmp( field->buffer + field->acOffset,
				cycleMatches[field->acMatchIndex] ) == 0 ) {
			cycleValid = qtrue;
		}
	}

	if ( cycleValid ) {
		if ( Field_AdvanceCycle( field ) ) {
			return;
		}
	}

	/* Starting a fresh completion pass.  Reset cycle state; when style 1
	 * is active, mark collection so FindMatches populates cycleMatches. */
	Field_ResetCompletionCycle( field );
	cycleMatchCount = 0;
	cycleCollecting = ( style == 1 ) ? qtrue : qfalse;

	Field_CompleteCommand( completionField->buffer, qtrue, qtrue );

	cycleCollecting = qfalse;
}


#if 0
static qboolean strgtr(const char *s0, const char *s1) {
	int l0, l1, i;

	l0 = strlen( s0 );
	l1 = strlen( s1 );

	if ( l1 < l0 ) {
		l0 = l1;
	}

	for( i = 0; i < l0; i++ ) {
		if ( s1[i] > s0[i] ) {
			return qtrue;
		}
		if ( s1[i] < s0[i] ) {
			return qfalse;
		}
	}
	return qfalse;
}
#endif




/*
==================
Com_SortFileList
==================
*/
#if 0
void Com_SortFileList( char **list, int nfiles, int fastSort )
{
	if ( nfiles > 1 && fastSort )
	{
		Com_SortList( list, nfiles-1 );
	}
	else // defrag mod demo UI can't handle _properly_ sorted directories
	{
		int i, flag;
		do {
			flag = 0;
			for( i = 1; i < nfiles; i++ ) {
				if ( strgtr( list[i-1], list[i] ) ) {
					char *temp = list[i];
					list[i] = list[i-1];
					list[i-1] = temp;
					flag = 1;
				}
			}
		} while( flag );
	}
}
#endif
