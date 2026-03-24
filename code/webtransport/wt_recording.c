/*
===========================================================================
wt_recording.c — Event stream recording to .q3events files

Records all QUIC events to disk as timestamped MessagePack entries.
Each entry is length-prefixed (uint32 big-endian) followed by the raw
msgpack payload from the event ring buffer.

File format:
  [4-byte magic: "Q3EV"]
  [4-byte version: 1]
  [8-byte start timestamp (microseconds)]
  [repeated: 4-byte length + msgpack payload]

Recording is controlled by the `sv_quicRecord` cvar:
  0 = disabled (default)
  1 = record all events to baseq3/recordings/

On disk-full or write error, recording stops silently and a warning
is printed to the server console. Events continue to stream over QUIC.
===========================================================================
*/
#include "wt_local.h"

#if FEAT_QUIC_OBSERVE

#define WT_RECORD_MAGIC     "Q3EV"
#define WT_RECORD_VERSION   1
#define WT_RECORD_DIR       "recordings"

// Recording state
static fileHandle_t wt_record_file = 0;
static qboolean     wt_recording = qfalse;
static cvar_t      *sv_quicRecord = NULL;
static uint64_t     wt_record_start_time = 0;
static int          wt_record_event_count = 0;


/*
====================
WT_RecordInit

Register the recording cvar. Called from QUIC_Init.
====================
*/
void WT_RecordInit( void )
{
	sv_quicRecord = Cvar_Get( "sv_quicRecord", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_quicRecord, "Enable QUIC event recording to .q3events files. 0=off, 1=on." );
}


/*
====================
WT_RecordStart

Open a new recording file. Called when sv_quicRecord transitions to 1.
====================
*/
static void WT_RecordStart( void )
{
	char filename[MAX_OSPATH];
	byte header[16];
	int  written;
	uint64_t start_time;

	if ( wt_recording )
		return;

	// Create recordings directory
	Cbuf_ExecuteText( EXEC_NOW, va("mkdir %s\n", WT_RECORD_DIR) );

	// Generate filename: recordings/YYYYMMDD-HHMMSS.q3events
	{
		qtime_t now;
		Com_RealTime( &now );
		Com_sprintf( filename, sizeof(filename), "%s/%04d%02d%02d-%02d%02d%02d.q3events",
			WT_RECORD_DIR,
			now.tm_year + 1900, now.tm_mon + 1, now.tm_mday,
			now.tm_hour, now.tm_min, now.tm_sec );
	}

	FS_FOpenFileByMode( filename, &wt_record_file, FS_WRITE );
	if ( !wt_record_file ) {
		Com_Printf( S_COLOR_RED "QUIC Recording: failed to open %s\n", filename );
		return;
	}

	// Write header
	start_time = (uint64_t)Sys_Microseconds();
	Com_Memcpy( header, WT_RECORD_MAGIC, 4 );
	header[4] = (WT_RECORD_VERSION >> 24) & 0xFF;
	header[5] = (WT_RECORD_VERSION >> 16) & 0xFF;
	header[6] = (WT_RECORD_VERSION >> 8) & 0xFF;
	header[7] = WT_RECORD_VERSION & 0xFF;
	header[8]  = (byte)((start_time >> 56) & 0xFF);
	header[9]  = (byte)((start_time >> 48) & 0xFF);
	header[10] = (byte)((start_time >> 40) & 0xFF);
	header[11] = (byte)((start_time >> 32) & 0xFF);
	header[12] = (byte)((start_time >> 24) & 0xFF);
	header[13] = (byte)((start_time >> 16) & 0xFF);
	header[14] = (byte)((start_time >> 8) & 0xFF);
	header[15] = (byte)(start_time & 0xFF);

	written = FS_Write( header, sizeof(header), wt_record_file );
	if ( written != sizeof(header) ) {
		Com_Printf( S_COLOR_RED "QUIC Recording: failed to write header\n" );
		FS_FCloseFile( wt_record_file );
		wt_record_file = 0;
		return;
	}

	wt_recording = qtrue;
	wt_record_start_time = start_time;
	wt_record_event_count = 0;

	Com_Printf( "QUIC Recording started: %s\n", filename );
}


/*
====================
WT_RecordStop

Close the recording file.
====================
*/
static void WT_RecordStop( void )
{
	if ( !wt_recording )
		return;

	FS_FCloseFile( wt_record_file );
	wt_record_file = 0;
	wt_recording = qfalse;

	Com_Printf( "QUIC Recording stopped. %d events recorded.\n", wt_record_event_count );
}


/*
====================
WT_RecordEvent

Write a single event to the recording file.
Format: [4-byte length (big-endian)] [msgpack payload]
On write error, stop recording.
====================
*/
void WT_RecordEvent( const byte *data, int len )
{
	byte len_header[4];
	int  written;

	// Check cvar transitions
	if ( sv_quicRecord && sv_quicRecord->integer && !wt_recording ) {
		WT_RecordStart();
	} else if ( sv_quicRecord && !sv_quicRecord->integer && wt_recording ) {
		WT_RecordStop();
		return;
	}

	if ( !wt_recording || !wt_record_file )
		return;

	if ( len <= 0 || len > 65535 )
		return;

	// Big-endian length prefix
	len_header[0] = (byte)((len >> 24) & 0xFF);
	len_header[1] = (byte)((len >> 16) & 0xFF);
	len_header[2] = (byte)((len >> 8) & 0xFF);
	len_header[3] = (byte)(len & 0xFF);

	written = FS_Write( len_header, 4, wt_record_file );
	if ( written != 4 ) {
		Com_Printf( S_COLOR_YELLOW "QUIC Recording: write error (disk full?). Stopping.\n" );
		WT_RecordStop();
		return;
	}

	written = FS_Write( data, len, wt_record_file );
	if ( written != len ) {
		Com_Printf( S_COLOR_YELLOW "QUIC Recording: write error (disk full?). Stopping.\n" );
		WT_RecordStop();
		return;
	}

	wt_record_event_count++;
}


/*
====================
WT_RecordShutdown

Clean up recording on server shutdown.
====================
*/
void WT_RecordShutdown( void )
{
	if ( wt_recording )
		WT_RecordStop();
}

#endif // FEAT_QUIC_OBSERVE
