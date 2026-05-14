// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// cg_playlist.c -- background music playlist engine

#include "cg_local.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_cgame, "cgame" );

#if FEAT_MUSIC_PLAYLIST

#define MAX_PLAYLIST_ENTRIES    100
#define BUFFER_SIZE             4096

typedef struct {
	unsigned long id;
	unsigned long size;
} chunkHeader_t;

typedef struct {
	short          wFormatTag;
	unsigned short wChannels;
	unsigned long  dwSamplesPerSec;
	unsigned long  dwAvgBytesPerSec;
	unsigned short wBlockAlign;
	unsigned short wBitsPerSample;
} formatChunk_t;

typedef struct {
	char introPart[128];
	char mainPart[128];
	long duration;          // milliseconds; -1 = infinite, 0 = skip
} playListEntry_t;

static fileHandle_t    file;
static int             fileSize;
static int             bufPos;
static int             bufLen;
static unsigned char   buffer[BUFFER_SIZE];

static qboolean        running;
static playListEntry_t playList[MAX_PLAYLIST_ENTRIES];
static int             numEntries;
static int             currentEntry;
static int             stopEntryTime;
static int             startEntryTime;

static int ReadByte( void ) {
	if ( bufPos >= bufLen ) {
		if ( fileSize <= 0 ) return -1;
		bufLen = fileSize;
		if ( bufLen > BUFFER_SIZE ) bufLen = BUFFER_SIZE;
		trap_FS_Read( buffer, bufLen, file );
		fileSize -= bufLen;
		bufPos = 0;
	}
	return buffer[bufPos++];
}

static int BytesLeft( void ) {
	return fileSize + bufLen - bufPos;
}

static qboolean ReadDWORD( void *dataBuf ) {
	unsigned long dword;
	if ( BytesLeft() < 4 ) return qfalse;
	dword  = (unsigned long)ReadByte();
	dword |= (unsigned long)ReadByte() << 8;
	dword |= (unsigned long)ReadByte() << 16;
	dword |= (unsigned long)ReadByte() << 24;
	if ( dataBuf ) *((unsigned long *)dataBuf) = dword;
	return qtrue;
}

static qboolean OpenWaveFile( const char *name ) {
	unsigned long id;

	fileSize = trap_FS_FOpenFile( name, &file, FS_READ );
	if ( !file ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "^3Couldn't open '%s'\n", name );
		return qfalse;
	}
	if ( fileSize < 44 ) {
		BadFile:
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "^3Unknown file format: '%s'\n", name );
		return qfalse;
	}
	bufPos = 0;
	bufLen = 0;
	if ( !ReadDWORD( &id ) ) goto BadFile;
	if ( id != 0x46464952 ) goto BadFile;   // 'RIFF'
	if ( !ReadDWORD( NULL ) ) goto BadFile; // length
	if ( !ReadDWORD( &id ) ) goto BadFile;
	if ( id != 0x45564157 ) goto BadFile;   // 'WAVE'
	return qtrue;
}

static void SkipToEndOfFile( void ) {
	fileSize = 0;
	bufPos   = 0;
	bufLen   = 0;
}

static qboolean ReadChunkHeader( chunkHeader_t *header ) {
	if ( !ReadDWORD( &header->id ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "^3Unexpected end of file\n" );
		return qfalse;
	}
	if ( !ReadDWORD( &header->size ) ) {
		Com_Log( SEV_INFO, LOG_CH(ch_cgame), "^3Unexpected end of file\n" );
		return qfalse;
	}
	return qtrue;
}

static int ReadChunkData( int size, void *buf, int bufSize ) {
	int bytesRead = 0;
	while ( size > 0 ) {
		int b = ReadByte();
		if ( b < 0 ) break;
		if ( buf && bytesRead < bufSize )
			((unsigned char *)buf)[bytesRead] = (unsigned char)b;
		bytesRead++;
		size--;
	}
	return bytesRead;
}

// Returns WAV duration in milliseconds (0 on error).
static long GetWaveDuration( const char *name ) {
	chunkHeader_t header;
	formatChunk_t fmt;
	long          duration          = 0;
	long          numBytes          = 0;
	long          bytesPerMillisecond = 0;

	if ( !OpenWaveFile( name ) ) return 0;

	while ( ReadChunkHeader( &header ) ) {
		if ( header.id == 0x20746d66 ) {    // 'fmt '
			int read = ReadChunkData( (int)header.size, &fmt, sizeof( fmt ) );
			if ( read >= (int)sizeof( fmt ) && fmt.dwAvgBytesPerSec > 0 )
				bytesPerMillisecond = (long)fmt.dwAvgBytesPerSec / 1000;
			if ( (long)header.size > read )
				ReadChunkData( (int)header.size - read, NULL, 0 );
		} else if ( header.id == 0x61746164 ) { // 'data'
			numBytes = (long)header.size;
			SkipToEndOfFile();
			break;
		} else {
			ReadChunkData( (int)header.size, NULL, 0 );
		}
	}

	if ( numBytes > 0 && bytesPerMillisecond > 1 )
		duration = numBytes / bytesPerMillisecond;

	if ( file ) trap_FS_FCloseFile( file );
	return duration;
}

void CG_InitPlayList( void ) {
	memset( &playList, 0, sizeof( playList ) );
	numEntries    = 0;
	currentEntry  = 0;
	stopEntryTime = -1;
	startEntryTime = -1;
	running       = qfalse;
}

void CG_ParsePlayList( void ) {
	int i;

	CG_InitPlayList();

	for ( i = 0; i < MAX_PLAYLIST_ENTRIES; i++ ) {
		playListEntry_t *entry;
		char             info[MAX_INFO_STRING];
		int              repetition;
		long             introDuration;
		long             mainDuration;

		entry = &playList[i];
		trap_Cvar_VariableStringBuffer( va( "playlist%02d", i ), info, sizeof( info ) );
		if ( !info[0] ) break;

		Q_strncpyz( entry->introPart, Info_ValueForKey( info, "intro" ), sizeof( entry->introPart ) );
		Q_strncpyz( entry->mainPart,  Info_ValueForKey( info, "main"  ), sizeof( entry->mainPart  ) );
		repetition = atoi( Info_ValueForKey( info, "rep" ) );

		introDuration = entry->introPart[0] ? GetWaveDuration( entry->introPart ) : 0;
		mainDuration  = entry->mainPart[0]  ? GetWaveDuration( entry->mainPart  ) : 0;

		if ( repetition >= 0 || mainDuration == 0 )
			entry->duration = introDuration + mainDuration * repetition;
		else
			entry->duration = -1;
	}

	numEntries = i;
	Com_Log( SEV_INFO, LOG_CH(ch_cgame), "%d entries in playlist\n", numEntries );
}

void CG_StopPlayList( void ) {
	trap_S_StopBackgroundTrack();
	stopEntryTime  = -1;
	startEntryTime = -1;
	running        = qfalse;
}

void CG_ContinuePlayList( void ) {
	if ( running ) return;
	trap_S_StopBackgroundTrack();
	stopEntryTime  = -1;
	startEntryTime = numEntries > 0 ? 0 : -1;
	running        = qtrue;
}

void CG_ResetPlayList( void ) {
	running      = qfalse;
	currentEntry = 0;
	CG_ContinuePlayList();
}

void CG_RunPlayListFrame( void ) {
	static int oldMusicMode = -1;
	int        currentTime;

	if ( cg_music.integer != oldMusicMode ) {
		oldMusicMode = cg_music.integer;
		CG_StartMusic();
	}

	if ( !running ) return;

	currentTime = trap_Milliseconds();

	if ( stopEntryTime >= 0 && currentTime >= stopEntryTime ) {
		trap_S_StopBackgroundTrack();
		stopEntryTime = -1;
		currentEntry++;
		startEntryTime = currentTime + 2000;
	}

	if ( startEntryTime >= 0 && currentTime >= startEntryTime && numEntries > 0 ) {
		const playListEntry_t *entry;

		if ( currentEntry >= numEntries ) currentEntry = 0;
		entry = &playList[currentEntry];
		if ( entry->duration == 0 ) {
			currentEntry++;
			return;
		}

		trap_S_StartBackgroundTrack( entry->introPart, entry->mainPart );
		startEntryTime = -1;
		if ( entry->duration > 0 )
			stopEntryTime = currentTime + (int)entry->duration;
	}
}

#endif // FEAT_MUSIC_PLAYLIST
