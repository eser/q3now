/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "client.h"
#include "snd_codec.h"

static snd_codec_t *codecs;

static void S_CodecRegister( snd_codec_t *codec );

static qboolean S_CodecPreferLegacy( void ) {
	char profile[16];
	int version;

	Cvar_VariableStringBuffer( "com_mapAssetProfile", profile, sizeof( profile ) );
	if ( !Q_stricmp( profile, "legacy" ) ) {
		return qtrue;
	}
	if ( !Q_stricmp( profile, "modern" ) ) {
		return qfalse;
	}

	version = Cvar_VariableIntegerValue( "com_mapBspVersion" );
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
	char		localName[ MAX_QPATH ];
	const char	*ext;
	char		altName[ MAX_QPATH ];
	void		*rtn = NULL;

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
				COM_StripExtension( filename, localName, MAX_QPATH );
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
				Com_DPrintf( S_COLOR_YELLOW "WARNING: %s not present, using %s instead\n",
					filename, altName );
			}

			return rtn;
		}
	}

	Com_DPrintf( S_COLOR_YELLOW "WARNING: Failed to %s sound %s!\n", info ? "load" : "open", filename );

	return NULL;
}


/*
=================
S_CodecInit
=================
*/
void S_CodecInit( void )
{
	codecs = NULL;

#if FEAT_LEGACY_FORMATS_AUDIO
#ifdef USE_OGG_VORBIS
	S_CodecRegister( &ogg_codec );
#endif
	S_CodecRegister( &wav_codec );
#endif // FEAT_LEGACY_FORMATS_AUDIO

	// Register opus codec last so it is head of list (tried first in fallback)
	S_CodecRegister( &opus_codec );
}


/*
=================
S_CodecShutdown
=================
*/
void S_CodecShutdown( void )
{
	codecs = NULL;
}


/*
=================
S_CodecRegister
=================
*/
static void S_CodecRegister( snd_codec_t *codec )
{
	codec->next = codecs;
	codecs = codec;
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
	snd_stream_t *stream;
	fileHandle_t hnd;
	int length;

	// Try to open the file
	length = FS_FOpenFileRead( filename, &hnd, qtrue );
	if ( hnd == FS_INVALID_HANDLE )
	{
		Com_DPrintf( "Can't read sound file %s\n", filename );
		return NULL;
	}

	// Allocate a stream
	stream = Z_Malloc( sizeof( snd_stream_t ) );
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
