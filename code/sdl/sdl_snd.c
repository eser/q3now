/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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

#include <SDL3/SDL.h>

#include "../qcommon/q_shared.h"
#include "../client/snd_local.h"
#include "../client/client.h"

qboolean snd_inited = qfalse;

extern cvar_t *s_khz;
cvar_t *s_sdlBits;
cvar_t *s_sdlChannels;
cvar_t *s_sdlDevSamps;
cvar_t *s_sdlMixSamps;
static cvar_t *s_sdlDevice;

/*
 * Audio data flow (SDL3 stream model):
 *
 *  Engine mixer thread         SDL3 audio thread
 *  ──────────────────          ─────────────────────
 *  fills dma.buffer ──────────> SNDDMA_AudioCallback()
 *  (ring buffer)               calls SDL_PutAudioStreamData()
 *                                        │
 *                              sdlPlaybackStream (internal SDL buffer)
 *                                        │
 *                              audio device output
 *
 * No lock/unlock needed — SDL3 audio streams are thread-safe.
 */

static int dmapos = 0;
static int dmasize = 0;

static SDL_AudioStream *sdlPlaybackStream = NULL;

#ifdef USE_VOIP
static SDL_AudioStream *sdlCaptureStream = NULL;
static cvar_t *s_sdlCapture;
static float sdlMasterGain = 1.0f;
#endif


/*
===============
SNDDMA_AudioCallback

SDL3 stream callback: called when the stream needs more audio data.
additional_amount = bytes needed; push exactly that many from our ring buffer.
===============
*/
static void SNDDMA_AudioCallback( void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount )
{
	int pos = (dmapos * (dma.samplebits/8));
	if (pos >= dmasize)
		dmapos = pos = 0;

	if ( !snd_inited )
	{
		/* Zero-fill: silence until engine audio is ready */
		void *silence = SDL_calloc( 1, additional_amount );
		if ( silence )
		{
			SDL_PutAudioStreamData( stream, silence, additional_amount );
			SDL_free( silence );
		}
		return;
	}

	{
		int len = additional_amount;
		int tobufend = dmasize - pos;
		int len1 = len;
		int len2 = 0;

		if ( len1 > tobufend )
		{
			len1 = tobufend;
			len2 = len - len1;
		}

		/* Apply master gain if needed (VoIP) */
#ifdef USE_VOIP
		if ( sdlMasterGain != 1.0f )
		{
			int i;
			if ( dma.isfloat && (dma.samplebits == 32) )
			{
				float *ptr = (float *) (dma.buffer + pos);
				int count = len1 / sizeof(*ptr);
				float *tmp = SDL_malloc( len1 );
				if ( tmp )
				{
					for ( i = 0; i < count; i++ )
						tmp[i] = ptr[i] * sdlMasterGain;
					SDL_PutAudioStreamData( stream, tmp, len1 );
					SDL_free( tmp );
				}
				else
				{
					SDL_PutAudioStreamData( stream, dma.buffer + pos, len1 );
				}
			}
			else if ( dma.samplebits == 16 )
			{
				Sint16 *ptr = (Sint16 *) (dma.buffer + pos);
				int count = len1 / sizeof(*ptr);
				Sint16 *tmp = SDL_malloc( len1 );
				if ( tmp )
				{
					for ( i = 0; i < count; i++ )
						tmp[i] = (Sint16)(((float)ptr[i]) * sdlMasterGain);
					SDL_PutAudioStreamData( stream, tmp, len1 );
					SDL_free( tmp );
				}
				else
				{
					SDL_PutAudioStreamData( stream, dma.buffer + pos, len1 );
				}
			}
			else if ( dma.samplebits == 8 )
			{
				Uint8 *ptr = (Uint8 *) (dma.buffer + pos);
				int count = len1 / sizeof(*ptr);
				Uint8 *tmp = SDL_malloc( len1 );
				if ( tmp )
				{
					for ( i = 0; i < count; i++ )
						tmp[i] = (Uint8)(((float)ptr[i]) * sdlMasterGain);
					SDL_PutAudioStreamData( stream, tmp, len1 );
					SDL_free( tmp );
				}
				else
				{
					SDL_PutAudioStreamData( stream, dma.buffer + pos, len1 );
				}
			}
			else
			{
				SDL_PutAudioStreamData( stream, dma.buffer + pos, len1 );
			}
		}
		else
#endif
		{
			SDL_PutAudioStreamData( stream, dma.buffer + pos, len1 );
		}

		if ( len2 <= 0 )
		{
			dmapos += (len1 / (dma.samplebits/8));
		}
		else
		{
			SDL_PutAudioStreamData( stream, dma.buffer, len2 );
			dmapos = (len2 / (dma.samplebits/8));
		}
	}

	if ( dmapos >= dmasize )
		dmapos = 0;
}


static const struct
{
	SDL_AudioFormat	enumFormat;
	const char	*stringFormat;
} formatToStringTable[ ] =
{
	{ SDL_AUDIO_U8,     "SDL_AUDIO_U8" },
	{ SDL_AUDIO_S8,     "SDL_AUDIO_S8" },
	{ SDL_AUDIO_S16LE,  "SDL_AUDIO_S16LE" },
	{ SDL_AUDIO_S16BE,  "SDL_AUDIO_S16BE" },
	{ SDL_AUDIO_S32LE,  "SDL_AUDIO_S32LE" },
	{ SDL_AUDIO_S32BE,  "SDL_AUDIO_S32BE" },
	{ SDL_AUDIO_F32LE,  "SDL_AUDIO_F32LE" },
	{ SDL_AUDIO_F32BE,  "SDL_AUDIO_F32BE" }
};

static int formatToStringTableSize = ARRAY_LEN( formatToStringTable );

/*
===============
SNDDMA_PrintAudiospec
===============
*/
static void SNDDMA_PrintAudiospec( const char *str, const SDL_AudioSpec *spec )
{
	const char *fmt = NULL;
	int i;

	Com_Printf( "%s:\n", str );

	for ( i = 0; i < formatToStringTableSize; i++ ) {
		if ( spec->format == formatToStringTable[ i ].enumFormat ) {
			fmt = formatToStringTable[ i ].stringFormat;
		}
	}

	if ( fmt ) {
		Com_Printf( "  Format:   %s\n", fmt );
	} else {
		Com_Printf( "  Format:   " S_COLOR_RED "UNKNOWN\n" );
	}

	Com_Printf( "  Freq:     %d\n", (int) spec->freq );
	Com_Printf( "  Channels: %d\n", (int) spec->channels );
}


static int SNDDMA_KHzToHz( int khz )
{
	switch ( khz )
	{
		default:
		case 48: return 48000;
		case 22: return 22050;
		case 44: return 44100;
		case 11: return 11025;
		case  8: return  8000;
	}
}


/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init( void )
{
	SDL_AudioSpec desired;
	SDL_AudioDeviceID devid = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
	int tmp;

	if ( snd_inited )
		return qtrue;

	s_sdlBits = Cvar_Get( "s_sdlBits", "16", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_sdlBits, "8", "16", CV_INTEGER );
	Cvar_SetDescription( s_sdlBits, "Bits per-sample to request for SDL audio output (8 or 16)." );

	s_sdlChannels = Cvar_Get( "s_sdlChannels", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_sdlChannels, "1", "2", CV_INTEGER );
	Cvar_SetDescription( s_sdlChannels, "Number of audio channels to request for SDL audio output." );

	s_sdlDevSamps = Cvar_Get( "s_sdlDevSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_sdlDevSamps, "Approximate number of audio samples for the device buffer. 0 = SDL3 default." );

	s_sdlMixSamps = Cvar_Get( "s_sdlMixSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_sdlMixSamps, "Number of audio samples for Quake 3's mixer. 0 = auto." );

	s_sdlDevice = Cvar_Get( "s_sdlDevice", "", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( s_sdlDevice, "Name of SDL audio output device. Empty = system default." );

	Com_Printf( "SDL_Init( SDL_INIT_AUDIO )... " );

	if ( !SDL_Init( SDL_INIT_AUDIO ) )
	{
		Com_Printf( "FAILED (%s)\n", SDL_GetError() );
		return qfalse;
	}

	Com_Printf( "OK\n" );
	Com_Printf( "SDL audio driver is \"%s\".\n", SDL_GetCurrentAudioDriver() );

	/* Enumerate available playback devices */
	{
		int count = 0;
		SDL_AudioDeviceID *devices = SDL_GetAudioPlaybackDevices( &count );
		if ( devices && count > 0 )
		{
			Com_Printf( "Available audio playback devices:\n" );
			for ( int i = 0; i < count; i++ )
			{
				const char *name = SDL_GetAudioDeviceName( devices[i] );
				Com_Printf( "  [%d] %s\n", i, name ? name : "(unknown)" );

				if ( s_sdlDevice->string[0] && name &&
				     Q_stricmp( s_sdlDevice->string, name ) == 0 )
				{
					devid = devices[i];
					Com_Printf( "  --> selected: %s\n", name );
				}
			}
		}
		SDL_free( devices );
	}

	SDL_zero( desired );
	desired.freq = SNDDMA_KHzToHz( s_khz->integer );
	if ( desired.freq == 0 )
		desired.freq = 48000;

	tmp = s_sdlBits->integer;
	if ( tmp < 16 )
		tmp = 8;

	desired.format = ( tmp == 16 ) ? SDL_AUDIO_S16 : SDL_AUDIO_U8;
	desired.channels = s_sdlChannels->integer;

	sdlPlaybackStream = SDL_OpenAudioDeviceStream( devid, &desired, SNDDMA_AudioCallback, NULL );
	if ( !sdlPlaybackStream )
	{
		Com_Printf( "SDL_OpenAudioDeviceStream() failed: %s\n", SDL_GetError() );
		SDL_QuitSubSystem( SDL_INIT_AUDIO );
		return qfalse;
	}

	SNDDMA_PrintAudiospec( "SDL_AudioSpec (requested)", &desired );

	/* Compute mixer buffer size.
	 * SDL3 removed AudioSpec.samples; use our own estimate based on freq. */
	{
		int approxSamples;
		if ( s_sdlDevSamps->integer )
			approxSamples = s_sdlDevSamps->integer;
		else if ( desired.freq <= 11025 )
			approxSamples = 256;
		else if ( desired.freq <= 22050 )
			approxSamples = 512;
		else if ( desired.freq <= 44100 )
			approxSamples = 1024;
		else
			approxSamples = 2048;

		tmp = s_sdlMixSamps->integer;
		if ( !tmp )
			tmp = (approxSamples * desired.channels) * 10;
	}

	/* samples must be divisible by number of channels */
	tmp -= tmp % desired.channels;
	/* round up to next power of 2 */
	tmp = log2pad( tmp, 1 );

	dmapos = 0;
	dma.samplebits = SDL_AUDIO_BITSIZE( desired.format );
	dma.isfloat = SDL_AUDIO_ISFLOAT( desired.format );
	dma.channels = desired.channels;
	dma.samples = tmp;
	dma.fullsamples = dma.samples / dma.channels;
	dma.submission_chunk = 1;
	dma.speed = desired.freq;
	dmasize = (dma.samples * (dma.samplebits/8));
	dma.buffer = calloc( 1, dmasize );

#ifdef USE_VOIP
	s_sdlCapture = Cvar_Get( "s_sdlCapture", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( s_sdlCapture, "Set to 1 to enable SDL audio capture for VoIP." );

	/* SDL3 always supports audio capture. Pulseaudio workaround still applies. */
	if ( Q_stricmp( SDL_GetCurrentAudioDriver(), "pulseaudio" ) == 0 )
	{
		Com_Printf( "SDL audio capture support disabled for pulseaudio (https://bugzilla.libsdl.org/show_bug.cgi?id=4087)\n" );
	}
	else if ( !s_sdlCapture->integer )
	{
		Com_Printf( "SDL audio capture support disabled by user ('+set s_sdlCapture 1' to enable)\n" );
	}
#if USE_MUMBLE
	else if ( cl_useMumble->integer )
	{
		Com_Printf( "SDL audio capture support disabled for Mumble support\n" );
	}
#endif
	else
	{
		SDL_AudioSpec captureSpec;
		SDL_zero( captureSpec );
		captureSpec.freq = 48000;
		captureSpec.format = SDL_AUDIO_S16;
		captureSpec.channels = 1;

		sdlCaptureStream = SDL_OpenAudioDeviceStream(
			SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &captureSpec, NULL, NULL );
		Com_Printf( "SDL capture device %s.\n",
			sdlCaptureStream ? "opened" : "failed to open" );
	}

	sdlMasterGain = 1.0f;
#endif /* USE_VOIP */

	Com_Printf( "Starting SDL audio...\n" );
	SDL_ResumeAudioStreamDevice( sdlPlaybackStream );
	/* capture stream is unpaused in SNDDMA_StartCapture */

	Com_Printf( "SDL audio initialized.\n" );
	snd_inited = qtrue;
	return qtrue;
}


/*
===============
SNDDMA_GetDMAPos
===============
*/
int SNDDMA_GetDMAPos( void )
{
	return dmapos;
}


/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown( void )
{
	if ( sdlPlaybackStream )
	{
		Com_Printf( "Closing SDL audio playback stream...\n" );
		SDL_DestroyAudioStream( sdlPlaybackStream );
		Com_Printf( "SDL audio playback stream closed.\n" );
		sdlPlaybackStream = NULL;
	}

#ifdef USE_VOIP
	if ( sdlCaptureStream )
	{
		Com_Printf( "Closing SDL audio capture stream...\n" );
		SDL_DestroyAudioStream( sdlCaptureStream );
		Com_Printf( "SDL audio capture stream closed.\n" );
		sdlCaptureStream = NULL;
	}
#endif

	SDL_QuitSubSystem( SDL_INIT_AUDIO );
	free( dma.buffer );
	dma.buffer = NULL;
	dmapos = dmasize = 0;
	snd_inited = qfalse;
	Com_Printf( "SDL audio shut down.\n" );
}


/*
===============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer.
SDL3 streams are thread-safe — no lock/unlock needed.
===============
*/
void SNDDMA_Submit( void )
{
}


/*
===============
SNDDMA_BeginPainting
===============
*/
void SNDDMA_BeginPainting( void )
{
}


#ifdef USE_VOIP
void SNDDMA_StartCapture( void )
{
	if ( sdlCaptureStream )
	{
		SDL_ClearAudioStream( sdlCaptureStream );
		SDL_ResumeAudioStreamDevice( sdlCaptureStream );
	}
}


int SNDDMA_AvailableCaptureSamples( void )
{
	/* divided by 2 to convert from bytes to (mono16) samples */
	return sdlCaptureStream ? (SDL_GetAudioStreamAvailable( sdlCaptureStream ) / 2) : 0;
}


void SNDDMA_Capture( int samples, byte *data )
{
	if ( sdlCaptureStream )
	{
		/* multiplied by 2 to convert from (mono16) samples to bytes */
		SDL_GetAudioStreamData( sdlCaptureStream, data, samples * 2 );
	}
	else
	{
		SDL_memset( data, '\0', samples * 2 );
	}
}


void SNDDMA_StopCapture( void )
{
	if ( sdlCaptureStream )
	{
		SDL_PauseAudioStreamDevice( sdlCaptureStream );
	}
}


void SNDDMA_MasterGain( float val )
{
	sdlMasterGain = val;
}
#endif /* USE_VOIP */
