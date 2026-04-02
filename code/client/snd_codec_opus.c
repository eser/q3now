/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)
Copyright (C) 2005-2006 Joerg Dietrich <dietrich_joerg@gmx.de>

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

// includes for the Q3 sound system
#include "client.h"
#include "snd_codec.h"

// includes for the Opus codec
#include <errno.h>
#include <opusfile.h>

// Opus always decodes to 16-bit signed PCM
#define OPUS_SAMPLEWIDTH 2

// Q3 Opus codec
snd_codec_t opus_codec =
{
	"opus",
	S_OPUS_CodecLoad,
	S_OPUS_CodecOpenStream,
	S_OPUS_CodecReadStream,
	S_OPUS_CodecCloseStream,
	NULL
};

// callbacks for libopusfile

// op_read_func replacement
static int S_OPUS_Callback_read( void *datasource, unsigned char *ptr, int nbytes )
{
	snd_stream_t *stream;
	int bytesRead = 0;

	// check if input is valid
	if ( !ptr )
	{
		errno = EFAULT;
		return -1;
	}

	if ( nbytes <= 0 )
	{
		// It's not an error, caller just wants zero bytes!
		errno = 0;
		return 0;
	}

	if ( !datasource )
	{
		errno = EBADF;
		return -1;
	}

	// we use a snd_stream_t in the generic pointer to pass around
	stream = (snd_stream_t *) datasource;

	// read it with the Q3 function FS_Read()
	bytesRead = FS_Read( ptr, nbytes, stream->file );

	// update the file position
	stream->pos += bytesRead;

	return bytesRead;
}

// op_seek_func replacement
static int S_OPUS_Callback_seek( void *datasource, opus_int64 offset, int whence )
{
	snd_stream_t *stream;
	int retVal = 0;

	// check if input is valid
	if ( !datasource )
	{
		errno = EBADF;
		return -1;
	}

	// snd_stream_t in the generic pointer
	stream = (snd_stream_t *) datasource;

	// we must map the whence to its Q3 counterpart
	switch ( whence )
	{
		case SEEK_SET :
		{
			// set the file position in the actual file with the Q3 function
			retVal = FS_Seek( stream->file, (long) offset, FS_SEEK_SET );

			// something has gone wrong, so we return here
			if ( retVal < 0 )
			{
				return retVal;
			}

			// keep track of file position
			stream->pos = (int) offset;
			break;
		}

		case SEEK_CUR :
		{
			// set the file position in the actual file with the Q3 function
			retVal = FS_Seek( stream->file, (long) offset, FS_SEEK_CUR );

			// something has gone wrong, so we return here
			if ( retVal < 0 )
			{
				return retVal;
			}

			// keep track of file position
			stream->pos += (int) offset;
			break;
		}

		case SEEK_END :
		{
			// set the file position in the actual file with the Q3 function
			retVal = FS_Seek( stream->file, (long) offset, FS_SEEK_END );

			// something has gone wrong, so we return here
			if ( retVal < 0 )
			{
				return retVal;
			}

			// keep track of file position
			stream->pos = stream->length + (int) offset;
			break;
		}

		default :
		{
			// unknown whence, so we return an error
			errno = EINVAL;
			return -1;
		}
	}

	// stream->pos shouldn't be smaller than zero or bigger than the filesize
	stream->pos = (stream->pos < 0) ? 0 : stream->pos;
	stream->pos = (stream->pos > stream->length) ? stream->length : stream->pos;

	return 0;
}

// op_close_func replacement
static int S_OPUS_Callback_close( void *datasource )
{
	// we do nothing here and close all things manually in S_OPUS_CodecCloseStream()
	return 0;
}

// op_tell_func replacement
static opus_int64 S_OPUS_Callback_tell( void *datasource )
{
	snd_stream_t *stream;

	// check if input is valid
	if ( !datasource )
	{
		errno = EBADF;
		return -1;
	}

	// snd_stream_t in the generic pointer
	stream = (snd_stream_t *) datasource;

	return (opus_int64) FS_FTell( stream->file );
}

// the callback structure
static const OpusFileCallbacks S_OPUS_Callbacks =
{
	S_OPUS_Callback_read,
	S_OPUS_Callback_seek,
	S_OPUS_Callback_tell,
	S_OPUS_Callback_close
};

/*
=================
S_OPUS_CodecOpenStream
=================
*/
snd_stream_t *S_OPUS_CodecOpenStream( const char *filename )
{
	snd_stream_t *stream;

	// Opus codec control structure
	OggOpusFile *of;

	// some variables used to get informations about the Opus stream
	const OpusHead *head;
	ogg_int64_t numSamples;
	int error;

	// check if input is valid
	if ( !filename )
	{
		return NULL;
	}

	// Open the stream
	stream = S_CodecUtilOpen( filename, &opus_codec );
	if ( !stream )
	{
		return NULL;
	}

	// open the codec with our callbacks and stream as the generic pointer
	of = op_open_callbacks( stream, &S_OPUS_Callbacks, NULL, 0, &error );
	if ( !of )
	{
		Com_DPrintf( "S_OPUS: Failed to open %s (error %d)\n", filename, error );

		S_CodecUtilClose( &stream );

		return NULL;
	}

	// the stream must be seekable
	if ( !op_seekable( of ) )
	{
		op_free( of );

		S_CodecUtilClose( &stream );

		return NULL;
	}

	// get the info about channels
	head = op_head( of, -1 );
	if ( !head )
	{
		op_free( of );

		S_CodecUtilClose( &stream );

		return NULL;
	}

	// get the number of sample-frames in the Opus stream
	numSamples = op_pcm_total( of, -1 );

	// fill in the info-structure in the stream
	// Opus always decodes to 48 kHz
	stream->info.rate = 48000;
	stream->info.width = OPUS_SAMPLEWIDTH;
	stream->info.channels = head->channel_count;
	stream->info.samples = (int) numSamples;
	stream->info.size = stream->info.samples * stream->info.channels * stream->info.width;
	stream->info.dataofs = 0;

	// Read Opus tags for loop points
	{
		const OpusTags *tags = op_tags( of, -1 );
		if ( tags )
		{
			int i;
			for ( i = 0; i < tags->comments; i++ )
			{
				const char *comment = tags->user_comments[i];
				if ( !Q_stricmpn( comment, "LOOPSTART=", 10 ) )
				{
					stream->info.loopStart = atoi( comment + 10 );
				}
				else if ( !Q_stricmpn( comment, "LOOPEND=", 8 ) )
				{
					stream->info.loopEnd = atoi( comment + 8 );
				}
				else if ( !Q_stricmpn( comment, "LOOP_START=", 11 ) )
				{
					stream->info.loopStart = atoi( comment + 11 );
				}
				else if ( !Q_stricmpn( comment, "LOOP_END=", 9 ) )
				{
					stream->info.loopEnd = atoi( comment + 9 );
				}
			}
			if ( stream->info.loopStart >= 0 )
			{
				Com_DPrintf( "S_OPUS: %s has loop points: start=%d end=%d\n",
					filename, stream->info.loopStart, stream->info.loopEnd );
			}
		}
	}

	// We use stream->pos for the file pointer in the compressed opus file
	stream->pos = 0;

	// We use the generic pointer in stream for the Opus codec control structure
	stream->ptr = of;

	return stream;
}

/*
=================
S_OPUS_CodecCloseStream
=================
*/
void S_OPUS_CodecCloseStream( snd_stream_t *stream )
{
	// check if input is valid
	if ( !stream )
	{
		return;
	}

	// let the Opus codec cleanup its stuff
	// op_free does NOT close the underlying stream via our no-op close callback
	op_free( (OggOpusFile *) stream->ptr );

	// close the stream
	S_CodecUtilClose( &stream );
}

/*
=================
S_OPUS_CodecReadStream
=================
*/
int S_OPUS_CodecReadStream( snd_stream_t *stream, int bytes, void *buffer )
{
	// buffer handling
	int samplesNeeded, samplesRead, totalSamples;
	opus_int16 *bufPtr;

	// check if input is valid
	if ( !(stream && buffer) )
	{
		return 0;
	}

	if ( bytes <= 0 )
	{
		return 0;
	}

	bufPtr = (opus_int16 *) buffer;
	totalSamples = 0;

	// op_read takes the total number of values (samples * channels) in the buffer,
	// and returns the number of samples per channel read
	samplesNeeded = bytes / ( stream->info.channels * OPUS_SAMPLEWIDTH );

	// cycle until we have the requested or all available samples read
	while ( samplesNeeded > 0 )
	{
		// read some samples from the Opus codec
		samplesRead = op_read( (OggOpusFile *) stream->ptr, bufPtr,
			samplesNeeded * stream->info.channels, NULL );

		// no more samples are left
		if ( samplesRead <= 0 )
		{
			break;
		}

		totalSamples += samplesRead;
		samplesNeeded -= samplesRead;
		bufPtr += samplesRead * stream->info.channels;
	}

	// return the number of bytes read
	return totalSamples * stream->info.channels * OPUS_SAMPLEWIDTH;
}

/*
=====================================================================
S_OPUS_CodecLoad

We handle S_OPUS_CodecLoad as a special case of the streaming functions
where we read the whole stream at once.
======================================================================
*/
void *S_OPUS_CodecLoad( const char *filename, snd_info_t *info )
{
	snd_stream_t *stream;
	byte *buffer;
	int bytesRead;

	// check if input is valid
	if ( !(filename && info) )
	{
		return NULL;
	}

	// open the file as a stream
	stream = S_OPUS_CodecOpenStream( filename );
	if ( !stream )
	{
		return NULL;
	}

	// copy over the info
	info->rate = stream->info.rate;
	info->width = stream->info.width;
	info->channels = stream->info.channels;
	info->samples = stream->info.samples;
	info->size = stream->info.size;
	info->dataofs = stream->info.dataofs;
	info->loopStart = stream->info.loopStart;
	info->loopEnd = stream->info.loopEnd;

	// allocate a buffer
	// this buffer must be free-ed by the caller of this function
	buffer = Hunk_AllocateTempMemory( info->size );
	if ( !buffer )
	{
		S_OPUS_CodecCloseStream( stream );

		return NULL;
	}

	// fill the buffer
	bytesRead = S_OPUS_CodecReadStream( stream, info->size, buffer );

	// we don't even have read a single byte
	if ( bytesRead <= 0 )
	{
		Hunk_FreeTempMemory( buffer );
		S_OPUS_CodecCloseStream( stream );

		return NULL;
	}

	S_OPUS_CodecCloseStream( stream );

	return buffer;
}
