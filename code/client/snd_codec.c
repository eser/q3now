// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2005 Stuart Dalton (badcdev@gmail.com)
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

// snd_codec.c — codec dispatcher + offline availability probe.
//
// This TU does not own engine init/shutdown anymore (those live in
// snd_codec_init.c). It is intentionally free of client-only
// includes so it can be linked into offline tools (extract-meta) for
// asset auditing without dragging the audio HW backend or the codec
// implementations.

#include "snd_codec.h"
#include "../qcommon/asset_load_log.h"

// Single source of truth for codec extension priority.
// Engine init walks this; offline probe walks this; runtime fallback
// in S_CodecGetSound walks this. Order matters: opus first (q3now
// native), legacy after.
static const char *s_codec_extensions[] = { "opus", "wav", "ogg", NULL };

// Codecs registered at runtime by S_CodecInit (in snd_codec_init.c).
// Stays NULL when snd_codec.c is linked standalone (e.g. extract-meta);
// S_CodecResolves walks the static array above instead.
static snd_codec_t *codecs;

static qboolean S_CodecPreferLegacy( void ) {
	char profile[16];

	Cvar_VariableStringBuffer( "com_mapAssetProfile", profile, sizeof( profile ) );
	if ( !Q_stricmp( profile, "legacy" ) ) {
		return qtrue;
	}
	if ( !Q_stricmp( profile, "modern" ) ) {
		return qfalse;
	}

	int version = Cvar_VariableIntegerValue( "com_mapBspVersion" );
	return ( version > 0 && ( version <= 46 || version == 68 ) ) ? qtrue : qfalse;
}

/*
=================
S_CodecGetSound

Opens/loads a sound, tries codec based on the sound's file extension
then tries all supported codecs.
=================
*/
static void *S_CodecGetSound( const char *filename, snd_info_t *info )
{
	snd_codec_t *codec;
	snd_codec_t *orgCodec = NULL;
	qboolean	orgNameFailed = qfalse;
	char		localName[ MAX_VFS_PATH ];
	const char	*ext;
	char		altName[ MAX_VFS_PATH ];
	void		*rtn = NULL;
	char		normName[ MAX_VFS_PATH ];

	// normalize backslash separators (BSP/map music paths often use backslashes)
	{
		char *p;
		Q_strncpyz( normName, filename, sizeof( normName ) );
		for ( p = normName; *p; p++ ) { if ( *p == '\\' ) *p = '/'; }
		filename = normName;
	}

	Q_strncpyz( localName, filename, sizeof( localName ) );

	ext = COM_GetExtension( localName );

	if ( *ext )
	{
		// Look for the correct loader and use it
		for ( codec = codecs; codec; codec = codec->next )
		{
			if ( !Q_stricmp( ext, codec->ext ) )
			{
				// Load
				if ( info )
					rtn = codec->load( localName, info );
				else
					rtn = codec->open( localName );
				break;
			}
		}

		// A loader was found
		if ( codec )
		{
			if ( !rtn )
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				orgNameFailed = qtrue;
				orgCodec = codec;
				COM_StripExtension( filename, localName, sizeof( localName ) );
			}
			else
			{
				// Something loaded
				return rtn;
			}
		}
	}

	// Try and find a suitable match using all
	// the sound codecs supported
	if ( !*ext ) {
		const char *preferredExt = S_CodecPreferLegacy() ? "wav" : "opus";
		Com_sprintf( altName, sizeof( altName ), "%s.%s", localName, preferredExt );

		for ( codec = codecs; codec; codec = codec->next ) {
			if ( Q_stricmp( codec->ext, preferredExt ) ) {
				continue;
			}

			if ( info ) {
				rtn = codec->load( altName, info );
			} else {
				rtn = codec->open( altName );
			}

			if ( rtn ) {
				return rtn;
			}

			break;
		}
	}

	// Try and find a suitable match using all
	// the sound codecs supported
	for ( codec = codecs; codec; codec = codec->next )
	{
		if ( codec == orgCodec )
			continue;

		Com_sprintf( altName, sizeof( altName ), "%s.%s", localName, codec->ext );

		// Load
		if ( info )
			rtn = codec->load( altName, info );
		else
			rtn = codec->open( altName );

		if ( rtn )
		{
			if ( orgNameFailed )
			{
				if ( Q_stricmpn( localName, "characters/", 11 ) != 0 &&
				     Q_stricmpn( localName, "music/", 6 ) != 0 )
				{
					char fbBuf[32];
					Com_sprintf( fbBuf, sizeof( fbBuf ), "%s>%s",
					             orgCodec ? orgCodec->ext : "?", codec->ext );
					AssetLog_Event( "sound", localName, fbBuf, NULL, ASSET_LOG_INFO );
				}
			}

			return rtn;
		}
	}

	if ( Q_stricmpn( localName, "characters/", 11 ) != 0 &&
	     Q_stricmpn( localName, "music/", 6 ) != 0 )
	{
		char extBuf[64];
		const snd_codec_t *c;
		qboolean first = qtrue;
		extBuf[0] = '\0';
		if ( orgCodec ) {
			Q_strncpyz( extBuf, orgCodec->ext, sizeof( extBuf ) );
			first = qfalse;
		}
		for ( c = codecs; c; c = c->next ) {
			if ( c == orgCodec ) continue;
			if ( !first )
				strncat( extBuf, ",", sizeof( extBuf ) - strlen( extBuf ) - 1 );
			strncat( extBuf, c->ext, sizeof( extBuf ) - strlen( extBuf ) - 1 );
			first = qfalse;
		}
		AssetLog_Event( "sound", localName, *extBuf ? extBuf : "wav,opus", NULL, ASSET_LOG_WARN );
	}

	return NULL;
}


/*
=================
S_CodecResolves

Offline availability probe: does `name` correspond to a sound file
the engine could load via any of the canonical codec extensions?
Walks s_codec_extensions[] (static array) so this works without
S_CodecInit having registered anything in the dynamic `codecs` list.
Used by extract-meta and any future asset-audit tool.
=================
*/
qboolean S_CodecResolves( const char *name )
{
	char normName[ MAX_VFS_PATH ];
	char localName[ MAX_VFS_PATH ];
	char altName[ MAX_VFS_PATH ];

	if ( !name || !*name ) return qfalse;

	// Backslash normalize (matches S_CodecGetSound).
	Q_strncpyz( normName, name, sizeof( normName ) );
	for ( char *p = normName; *p; p++ ) {
		if ( *p == '\\' ) *p = '/';
	}

	COM_StripExtension( normName, localName, sizeof( localName ) );

	for ( const char **ext = s_codec_extensions; *ext; ext++ ) {
		fileHandle_t f;
		Com_sprintf( altName, sizeof( altName ), "%s.%s", localName, *ext );
		if ( FS_FOpenFileRead( altName, &f, qtrue ) > 0 ) {
			FS_FCloseFile( f );
			return qtrue;
		}
	}
	return qfalse;
}


/*
=================
S_CodecRegister

Append a codec to the head of the dispatcher list. Called from
snd_codec_init.c::S_CodecInit at engine startup.
=================
*/
void S_CodecRegister( snd_codec_t *codec )
{
	codec->next = codecs;
	codecs = codec;
}


/*
=================
S_CodecResetList

Clear the dispatcher list. Called at the top of S_CodecInit (in
snd_codec_init.c) and at S_CodecShutdown so engine restart paths
can re-register codecs cleanly.
=================
*/
void S_CodecResetList( void )
{
	codecs = NULL;
}


/*
=================
S_CodecLoad
=================
*/
void *S_CodecLoad( const char *filename, snd_info_t *info )
{
	return S_CodecGetSound( filename, info );
}


/*
=================
S_CodecOpenStream
=================
*/
snd_stream_t *S_CodecOpenStream( const char *filename )
{
	return S_CodecGetSound( filename, NULL );
}


/*
=================
S_CodecCloseStream
=================
*/
void S_CodecCloseStream( snd_stream_t *stream )
{
	stream->codec->close( stream );
}


/*
=================
S_CodecReadStream
=================
*/
int S_CodecReadStream( snd_stream_t *stream, int bytes, void *buffer )
{
	return stream->codec->read( stream, bytes, buffer );
}


//=======================================================================
// Util functions (used by codecs)

/*
=================
S_CodecUtilOpen
=================
*/
snd_stream_t *S_CodecUtilOpen( const char *filename, snd_codec_t *codec )
{
	// Try to open the file
	fileHandle_t hnd;
	int length = FS_FOpenFileRead( filename, &hnd, qtrue );
	if ( hnd == FS_INVALID_HANDLE )
	{
		return NULL;
	}

	// Allocate a stream
	snd_stream_t *stream = Z_Malloc( sizeof( snd_stream_t ) );
	if ( !stream )
	{
		FS_FCloseFile( hnd );
		return NULL;
	}

	// Copy over, return
	stream->codec = codec;
	stream->file = hnd;
	stream->length = length;
	stream->info.loopStart = -1;
	stream->info.loopEnd = -1;
	return stream;
}

/*
=================
S_CodecUtilClose
=================
*/
void S_CodecUtilClose( snd_stream_t **stream )
{
	FS_FCloseFile( ( *stream )->file );
	Z_Free( *stream );
	*stream = NULL;
}
