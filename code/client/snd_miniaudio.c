/*
===========================================================================
snd_miniaudio.c -- audio output via miniaudio (single-header library)

Replaces the platform-specific backends (sdl_snd.c, win_snd.c, linux_snd.c)
with a single cross-platform output path. miniaudio routes to:
  - WASAPI on Windows
  - CoreAudio on macOS
  - PulseAudio / ALSA on Linux

Architecture:
  Game/cgame  ->  trap_S_*  ->  snd_dma.c (S_*)  ->  snd_mix.c (S_PaintChannels)
                                                       |
                                                       v
                                                 dma.buffer (ring buffer)
                                                       |
                                                       v
                                          snd_miniaudio.c (this file)
                                                       |
                                                       v
                                          ma_device callback (audio thread)
                                                       |
                                                       v
                              WASAPI / CoreAudio / PulseAudio / ALSA

Audio thread rules (HARD):
  - The miniaudio callback runs on a dedicated audio thread.
  - NO mutexes, semaphores, or any blocking primitive in the callback path.
  - Position tracking uses ma_atomic_uint32 load/store only (lock-free).
  - The callback must be wait-free: copy from dma.buffer to output and return.

Future HRTF hook:
  A processing stage could be inserted between the dma.buffer read and the
  output write in S_MiniaudioCallback. That is where binaural convolution
  would go. NOT IMPLEMENTED in this spec -- out of scope (see spec section 10).

Vendored miniaudio version: see code/client/miniaudio.h header (v0.11.25).
This file is the SOLE place that defines MINIAUDIO_IMPLEMENTATION.
===========================================================================
*/

#ifndef DEDICATED  /* entire file is no-op for dedicated server builds */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "snd_local.h"

extern cvar_t *s_khz;

/* ---- module state ----------------------------------------------------- */

static qboolean         s_maInitialized = qfalse;
static ma_device        s_maDevice;

/*
 * Underrun counter (task-3). Incremented by the audio thread each time the
 * callback fires before SNDDMA_Init has finished or while the engine has no
 * dma.buffer available. Surfaced to the s_underruns cvar by the main thread
 * inside SNDDMA_GetDMAPos -- we never touch the cvar system from the audio
 * thread (Cvar_Set is not documented as thread-safe).
 *
 * Atomic: written by the audio thread via fetch_add (lock-free), read by the
 * main thread via _get. ma_atomic_uint32 wraps stdatomic / compiler builtins.
 */
static ma_atomic_uint32 s_underrunsLocal;

/* ---- audio level capture for waveform visualization -----------------
 * Lock-free ring buffer of recent RMS levels. Audio callback writes one
 * RMS value per period into the ring; the Wired UI audio_waveform element
 * reads the most recent N levels from the main thread via S_GetRecentLevels().
 *
 * Race tolerance: the writer races with a reader that may snapshot a
 * partial update. That's acceptable — the visual jitter is invisible at
 * 60 Hz, and the only failure mode is a single bar briefly showing a
 * stale value. No sync primitive is needed in the hot path.
 */
#define S_LEVELS_RING_SIZE 128
static float            s_levelsRing[S_LEVELS_RING_SIZE];
static ma_atomic_uint32 s_levelsRingPos;   /* write index, monotonic wrap */

/* ---- audio test (snd_test command) ---------------------------------
 * When s_testFramesRemaining > 0 the callback synthesizes a 1 kHz sine
 * into both output channels (0.25 amplitude) and decrements the counter
 * by the number of frames written. Phase accumulates so the waveform
 * stays continuous across callback boundaries.
 */
static ma_atomic_uint32 s_testFramesRemaining;  /* 0 = no test active */
static ma_atomic_uint32 s_testPhase;             /* frame-counter accumulator */

/*
 * dmapos is the engine's "DMA position" -- a monotonically increasing
 * count of MONO samples consumed by the audio device since the last buffer
 * wrap. The engine reads it via SNDDMA_GetDMAPos() and detects wraparound
 * by comparing successive values; on wrap it bumps an internal "buffers"
 * counter and computes s_soundtime. See snd_dma.c S_GetSoundtime().
 *
 * Unit: mono samples. With 2 channels and 16-bit samples, advancing by N
 * mono samples means advancing by N * sizeof(int16_t) bytes in dma.buffer
 * (and N / 2 frames in the audio device).
 *
 * Atomic: written by the audio thread (callback), read by the main thread
 * (S_GetSoundtime). ma_atomic_uint32 wraps the platform's atomic intrinsic
 * (C11 stdatomic on modern compilers, or compiler-specific builtins).
 */
static ma_atomic_uint32 s_dmapos;
static ma_uint32        s_dmasize_bytes;   /* total bytes in dma.buffer */
static ma_uint32        s_dmasize_samples; /* total mono samples in dma.buffer */
static ma_uint32        s_bytesPerSample;  /* dma.samplebits / 8, e.g. 2 for s16 */

/* ---- helpers ---------------------------------------------------------- */

static int SNDDMA_KHzToHz( int khz )
{
	switch ( khz )
	{
		default:
		case 48: return 48000;
		case 44: return 44100;
		case 22: return 22050;
		case 11: return 11025;
		case  8: return  8000;
	}
}

/* ---- audio callback (RUNS ON AUDIO THREAD -- NO LOCKS, NO MALLOC) ----- */

static void S_MiniaudioCallback( ma_device *pDevice, void *pOutput,
                                  const void *pInput, ma_uint32 frameCount )
{
	/*
	 * AUDIO THREAD -- NO LOCKS, NO MALLOC, NO LOG, NO BLOCKING.
	 *
	 * frameCount  = number of frames the device wants this call.
	 * dma.channels= channel count (2 for stereo).
	 * One frame    = dma.channels mono samples.
	 *
	 * We copy frameCount * dma.channels mono samples (= frameCount frames)
	 * out of the engine's ring buffer (dma.buffer) into pOutput, handling
	 * wraparound. Then we atomically advance the read position so the
	 * main thread's S_GetSoundtime() can observe progress.
	 *
	 * HRTF hook point -- see spec section 10. A future binaural processor
	 * would read from dma.buffer here, transform per source, and write to
	 * pOutput. Not implemented in this spec.
	 */
	(void)pInput;
	(void)pDevice;

	if ( !s_maInitialized || dma.buffer == NULL )
	{
		/* Engine audio not ready -- output silence (memset is wait-free).
		 * Count this as an underrun. fetch_add is lock-free; the cvar update
		 * happens later on the main thread inside SNDDMA_GetDMAPos. */
		ma_atomic_uint32_fetch_add( &s_underrunsLocal, 1 );
		memset( pOutput, 0, (size_t)frameCount * dma.channels * s_bytesPerSample );
		return;
	}

	ma_uint32 bytesNeeded = frameCount * dma.channels * s_bytesPerSample;

	/* Read current position atomically. The mixer thread never writes to
	 * dmapos, only reads it, so a single load is sufficient. */
	ma_uint32 pos = ma_atomic_uint32_get( &s_dmapos ) * s_bytesPerSample;
	if ( pos >= s_dmasize_bytes )
		pos = 0;

	ma_uint32 bytesToEnd = s_dmasize_bytes - pos;
	ma_uint32 chunk1, chunk2;
	if ( bytesNeeded <= bytesToEnd )
	{
		chunk1 = bytesNeeded;
		chunk2 = 0;
	}
	else
	{
		chunk1 = bytesToEnd;
		chunk2 = bytesNeeded - bytesToEnd;
	}

	memcpy( pOutput, dma.buffer + pos, chunk1 );
	if ( chunk2 > 0 )
	{
		memcpy( (byte *)pOutput + chunk1, dma.buffer, chunk2 );
		ma_atomic_uint32_set( &s_dmapos, chunk2 / s_bytesPerSample );
	}
	else
	{
		ma_uint32 newPos = ( pos + chunk1 ) / s_bytesPerSample;
		if ( newPos >= s_dmasize_samples )
			newPos = 0;
		ma_atomic_uint32_set( &s_dmapos, newPos );
	}

	/* ---- snd_test sine sweep injection --------------------------
	 * When the user runs `snd_test`, override the freshly-copied
	 * engine output with a 1 kHz stereo sine for ~2 seconds. This
	 * runs after the normal memcpy so it fully overwrites the mixer
	 * output for the test period. Lock-free: two atomic loads +
	 * sinf() per sample. No allocation, no logging.
	 */
	{
		ma_uint32 testRemaining = ma_atomic_uint32_get( &s_testFramesRemaining );
		if ( testRemaining > 0 && dma.channels == 2 && s_bytesPerSample == 2 )
		{
			int16_t *out = (int16_t *)pOutput;
			ma_uint32 framesToWrite = ( frameCount < testRemaining ) ? frameCount : testRemaining;
			ma_uint32 phase = ma_atomic_uint32_get( &s_testPhase );
			const float twopi_over_sr = 6.2831853071795864f / (float)s_maDevice.sampleRate;
			const float freq = 1000.0f; /* 1 kHz */
			for ( ma_uint32 i = 0; i < framesToWrite; i++ )
			{
				float t = (float)( phase + i );
				int16_t sample = (int16_t)( 0.25f * 32767.0f * sinf( t * freq * twopi_over_sr ) );
				out[i * 2 + 0] = sample; /* L */
				out[i * 2 + 1] = sample; /* R */
			}
			ma_atomic_uint32_set( &s_testPhase, phase + framesToWrite );
			ma_atomic_uint32_set( &s_testFramesRemaining, testRemaining - framesToWrite );
		}
	}

	/* ---- RMS level capture for waveform widget ------------------
	 * Compute one RMS value over the full period we just wrote, and
	 * push it into the lock-free ring buffer. The main-thread HUD
	 * reader (S_GetRecentLevels) consumes this ring to draw ~120
	 * bars. Single pass over pOutput, no allocation, no logging.
	 */
	if ( s_bytesPerSample == 2 )
	{
		int16_t *out = (int16_t *)pOutput;
		ma_uint32 totalSamples = frameCount * (ma_uint32)dma.channels;
		double sumSquares = 0.0;
		for ( ma_uint32 i = 0; i < totalSamples; i++ )
		{
			float s = (float)out[i] * ( 1.0f / 32768.0f );
			sumSquares += (double)( s * s );
		}
		float rms = ( totalSamples > 0 )
		    ? (float)sqrt( sumSquares / (double)totalSamples )
		    : 0.0f;
		ma_uint32 ringPos = ma_atomic_uint32_get( &s_levelsRingPos );
		s_levelsRing[ ringPos % S_LEVELS_RING_SIZE ] = rms;
		ma_atomic_uint32_set( &s_levelsRingPos, ringPos + 1 );
	}
}

/* ---- snd_test console command ---------------------------------------- */

/*
===============
S_Test_f

Console command: snd_test
Plays a 1 kHz sine wave through both stereo channels for ~2 seconds.
Feeds the audio_waveform Wired UI element so users can verify audio
output and visualize the RMS envelope.

Runs on the main thread. Writes to two lock-free atomics that the
audio callback observes on its next wake-up — no locking, no race.
===============
*/
static void S_Test_f( void )
{
	if ( !s_maInitialized )
	{
		Com_Printf( "snd_test: audio device not initialized\n" );
		return;
	}

	/* 2 seconds at the device sample rate. */
	ma_uint32 frames = 2 * (ma_uint32)s_maDevice.sampleRate;
	ma_atomic_uint32_set( &s_testPhase, 0 );
	ma_atomic_uint32_set( &s_testFramesRemaining, frames );

	Com_Printf( "snd_test: playing 2 second 1 kHz sine sweep\n" );
}


/*
===============
S_GetRecentLevels

Main-thread reader for the audio callback's lock-free RMS ring buffer.
Copies the most recent `outCount` levels (newest last) into `outLevels`.

Race behaviour: a single atomic load gets the writer's current position
snapshot. The reader then scans backwards from pos-1 (newest) and
dereferences ring slots directly. The writer may overwrite a slot while
the reader is mid-scan — that is acceptable: the resulting visual jitter
is imperceptible at 60 Hz, and there is no sync primitive in the hot
audio path.

Returns the number of levels actually written to `outLevels` (≤ outCount).
===============
*/
int S_GetRecentLevels( float *outLevels, int outCount )
{
	if ( outLevels == NULL || outCount <= 0 )
		return 0;
	if ( outCount > S_LEVELS_RING_SIZE )
		outCount = S_LEVELS_RING_SIZE;

	ma_uint32 pos = ma_atomic_uint32_get( &s_levelsRingPos );

	/* Walk backwards from the most-recent write: index (pos-1) is newest,
	 * (pos-2) is one period older, etc. Store them in outLevels so that
	 * outLevels[outCount-1] is the newest sample (caller can draw left-
	 * to-right with oldest first). */
	for ( int i = 0; i < outCount; i++ )
	{
		ma_uint32 idx = ( pos - 1 - (ma_uint32)i ) % S_LEVELS_RING_SIZE;
		outLevels[ outCount - 1 - i ] = s_levelsRing[ idx ];
	}

	return outCount;
}


/* ---- device enumeration ---------------------------------------------- */

/*
===============
S_GetAudioDeviceList

Enumerates available playback devices via miniaudio.
Fills outNames[] with up to outCapacity device-name pointers. The strings
are owned by a static buffer inside this function and remain valid until
the next call. Callers MUST NOT free or modify the returned strings.

Returns:
   > 0: number of device names written into outNames
   0:   no devices found (empty)
  -1:   enumeration failed (error)

Threading: runs on the MAIN THREAD only. Spins up a fresh ma_context, calls
ma_context_get_devices, copies names into the static buffer, then uninits
the context. NEVER call from the audio callback (S_MiniaudioCallback) — it
allocates internally and is not lock-free.
===============
*/

#define S_DEVLIST_MAX_DEVICES   64
#define S_DEVLIST_MAX_NAME_LEN  256

static char s_devListNames[S_DEVLIST_MAX_DEVICES][S_DEVLIST_MAX_NAME_LEN];
static int  s_devListCount = 0;

int S_GetAudioDeviceList( const char **outNames, int outCapacity )
{
	ma_context        context;
	ma_device_info   *pPlaybackInfos = NULL;
	ma_uint32         playbackCount  = 0;

	if ( outNames == NULL || outCapacity <= 0 )
		return 0;

	if ( ma_context_init( NULL, 0, NULL, &context ) != MA_SUCCESS )
	{
		s_devListCount = 0;
		return -1;
	}

	if ( ma_context_get_devices( &context, &pPlaybackInfos, &playbackCount,
	                              NULL, NULL ) != MA_SUCCESS )
	{
		ma_context_uninit( &context );
		s_devListCount = 0;
		return -1;
	}

	/* Copy device names into our static buffer so the pointers stay valid
	 * after we uninit the context. */
	s_devListCount = 0;
	for ( ma_uint32 i = 0; i < playbackCount && s_devListCount < S_DEVLIST_MAX_DEVICES; i++ )
	{
		Q_strncpyz( s_devListNames[s_devListCount], pPlaybackInfos[i].name,
		            sizeof( s_devListNames[0] ) );
		s_devListCount++;
	}

	ma_context_uninit( &context );

	int written = ( s_devListCount < outCapacity ) ? s_devListCount : outCapacity;
	for ( ma_uint32 i = 0; i < (ma_uint32)written; i++ )
	{
		outNames[i] = s_devListNames[i];
	}

	return written;
}


/* ---- SNDDMA_* interface ----------------------------------------------- */

/*
===============
SNDDMA_Init

Open the default playback device via miniaudio, populate the dma_t struct
the engine mixer expects, and start the device. Returns qtrue on success.
===============
*/
qboolean SNDDMA_Init( void )
{
	if ( s_maInitialized )
		return qtrue;

	/*
	 * Sample rate from the s_khz cvar (8/11/22/44/48). The engine sets this
	 * via Cvar_Get long before SNDDMA_Init is called from S_Base_Init.
	 */
	ma_uint32 sampleRate = (ma_uint32)SNDDMA_KHzToHz( s_khz ? s_khz->integer : 48 );
	if ( sampleRate == 0 )
		sampleRate = 48000;

	/* Stereo s16 -- the engine mixer always paints stereo s16, regardless
	 * of source sample count, so we hardwire it here. Future HRTF work
	 * would change this. */
	ma_uint32       channels = 2;
	ma_uint32       mixerSamples;
	ma_uint32       periodSizeInFrames;
	ma_device_config config;
	ma_result       result;

	/*
	 * periodSizeInFrames is derived from the s_latency cvar (clamped to
	 * [2, 20] ms). At 48 kHz, 6 ms -> 288 frames per period. Two periods
	 * (double buffering) yields ~12 ms of total output latency.
	 */
	{
		ma_uint32 latencyMs = (ma_uint32)( s_latency ? s_latency->integer : 6 );
		if ( latencyMs < 2 )  latencyMs = 2;
		if ( latencyMs > 20 ) latencyMs = 20;
		periodSizeInFrames = ( latencyMs * sampleRate ) / 1000;
		if ( periodSizeInFrames < 32 )
			periodSizeInFrames = 32;
	}

	config = ma_device_config_init( ma_device_type_playback );
	config.playback.format   = ma_format_s16;
	config.playback.channels = channels;
	config.sampleRate        = sampleRate;
	config.periodSizeInFrames = periodSizeInFrames;
	config.periods           = 2;
	config.dataCallback      = S_MiniaudioCallback;
	config.pUserData         = NULL;

	/*
	 * Optional device selection from the s_device cvar. Empty string =
	 * system default. Otherwise we enumerate playback devices via a
	 * temporary context, search for a name match (case-insensitive), and
	 * point config.playback.pDeviceID at the matching device's ID. If the
	 * named device is not found we silently fall back to the default.
	 *
	 * The enumeration is handled inside this scope rather than calling
	 * S_GetAudioDeviceList because we also need the ma_device_id (not just
	 * the name) to forward to ma_device_init. ma_device_init copies the
	 * device ID, so the local `chosen` variable does not need to outlive
	 * this scope.
	 */
	{
		ma_context        context;
		ma_device_id      chosen;
		qboolean          haveChosen = qfalse;

		if ( s_device && s_device->string[0] != '\0' )
		{
			if ( ma_context_init( NULL, 0, NULL, &context ) == MA_SUCCESS )
			{
				ma_device_info *pPlaybackInfos = NULL;
				ma_uint32       playbackCount  = 0;
				if ( ma_context_get_devices( &context, &pPlaybackInfos, &playbackCount,
				                              NULL, NULL ) == MA_SUCCESS )
				{
					ma_uint32 i;
					/* Refresh the static name cache for S_GetAudioDeviceList
					 * callers (e.g. the Wired UI dropdown) so opening the
					 * audio settings menu after snd_restart shows current
					 * devices without having to call S_GetAudioDeviceList
					 * separately. */
					s_devListCount = 0;
					for ( i = 0; i < playbackCount && s_devListCount < S_DEVLIST_MAX_DEVICES; i++ )
					{
						Q_strncpyz( s_devListNames[s_devListCount], pPlaybackInfos[i].name,
						            sizeof( s_devListNames[0] ) );
						s_devListCount++;
					}
					for ( i = 0; i < playbackCount; i++ )
					{
						if ( !Q_stricmp( pPlaybackInfos[i].name, s_device->string ) )
						{
							chosen = pPlaybackInfos[i].id;
							haveChosen = qtrue;
							break;
						}
					}
				}
				ma_context_uninit( &context );
			}

			if ( haveChosen )
			{
				config.playback.pDeviceID = &chosen;
				Com_Printf( "miniaudio: using requested device \"%s\"\n",
				            s_device->string );
			}
			else
			{
				Com_Printf( "miniaudio: requested device \"%s\" not found, "
				            "falling back to default\n", s_device->string );
			}
		}

		Com_Printf( "Opening miniaudio device...\n" );
		result = ma_device_init( NULL, &config, &s_maDevice );
		if ( result != MA_SUCCESS )
		{
			Com_Printf( "miniaudio: ma_device_init failed (code %d)\n", (int)result );
			return qfalse;
		}
	}

	/*
	 * After init, the device's actual format/channels/sampleRate may differ
	 * from what we requested if the OS forces a mix format (e.g. WASAPI
	 * shared mode). miniaudio handles the conversion internally, so the
	 * dma.* fields below reflect what the ENGINE feeds the device, not the
	 * device's native format.
	 */

	/*
	 * Mixer ring buffer size, in mono samples. Match the SDL backend's
	 * formula: approxSamples * channels * 10. This gives us a healthy
	 * margin (~10 periods) so the painter is never racing the audio thread.
	 * Round up to a power of two -- the engine assumes this for its
	 * `dma.submission_chunk-1` mask logic.
	 */
	{
		int approxSamples;
		if ( sampleRate <= 11025 )
			approxSamples = 256;
		else if ( sampleRate <= 22050 )
			approxSamples = 512;
		else if ( sampleRate <= 44100 )
			approxSamples = 1024;
		else
			approxSamples = 2048;
		mixerSamples = (ma_uint32)( approxSamples * (int)channels * 10 );
	}
	mixerSamples -= mixerSamples % channels;
	mixerSamples = (ma_uint32)log2pad( mixerSamples, 1 );

	memset( &dma, 0, sizeof( dma ) );
	dma.speed            = (int)sampleRate;
	dma.channels         = channels;
	dma.samplebits       = 16;
	dma.isfloat          = 0;
	dma.samples          = (int)mixerSamples;
	dma.fullsamples      = dma.samples / dma.channels;
	dma.submission_chunk = 1;
	dma.driver           = "miniaudio";

	s_bytesPerSample  = (ma_uint32)( dma.samplebits / 8 );
	s_dmasize_samples = mixerSamples;
	s_dmasize_bytes   = mixerSamples * s_bytesPerSample;

	dma.buffer = (byte *)calloc( 1, s_dmasize_bytes );
	if ( dma.buffer == NULL )
	{
		Com_Printf( "miniaudio: failed to allocate %u byte mixer buffer\n",
		            (unsigned)s_dmasize_bytes );
		ma_device_uninit( &s_maDevice );
		return qfalse;
	}

	ma_atomic_uint32_set( &s_dmapos, 0 );
	ma_atomic_uint32_set( &s_underrunsLocal, 0 );
	if ( s_underruns )
		Cvar_Set( "s_underruns", "0" );

	/* waveform ring + snd_test atomics — reset on every init/restart */
	ma_atomic_uint32_set( &s_levelsRingPos, 0 );
	ma_atomic_uint32_set( &s_testFramesRemaining, 0 );
	ma_atomic_uint32_set( &s_testPhase, 0 );
	memset( s_levelsRing, 0, sizeof( s_levelsRing ) );

	result = ma_device_start( &s_maDevice );
	if ( result != MA_SUCCESS )
	{
		Com_Printf( "miniaudio: ma_device_start failed (code %d)\n", (int)result );
		free( dma.buffer );
		dma.buffer = NULL;
		ma_device_uninit( &s_maDevice );
		return qfalse;
	}

	s_maInitialized = qtrue;

	/* snd_test console command — feeds the Wired UI audio_waveform
	 * element with a known-good 1 kHz stereo sine. Registered after
	 * the device starts so the command only exists while the audio
	 * path is alive. */
	Cmd_AddCommand( "snd_test", S_Test_f );

	Com_Printf( "miniaudio: backend=%s, %u Hz, %u ch, s16, %u-frame periods x %u\n",
	            ma_get_backend_name( s_maDevice.pContext->backend ),
	            (unsigned)sampleRate,
	            (unsigned)channels,
	            (unsigned)periodSizeInFrames,
	            (unsigned)config.periods );
	Com_Printf( "miniaudio: device name = \"%s\"\n",
	            s_maDevice.playback.name[0] ? s_maDevice.playback.name : "(default)" );
	Com_Printf( "miniaudio: mixer ring = %u mono samples (%u bytes), fullsamples=%d\n",
	            (unsigned)s_dmasize_samples, (unsigned)s_dmasize_bytes, dma.fullsamples );

	return qtrue;
}


/*
===============
SNDDMA_GetDMAPos

Returns the current sample position (in mono samples) inside the ring
buffer. Read by the main thread; written by the audio callback. The
ma_atomic_uint32_get() helper is wait-free.

The engine uses successive returns of this value to detect buffer wrap
and compute s_soundtime. It expects the value to be in the range
[0, dma.samples) and to wrap monotonically.
===============
*/
int SNDDMA_GetDMAPos( void )
{
	if ( !s_maInitialized )
		return 0;

	/*
	 * Surface the audio-thread underrun counter to the s_underruns cvar.
	 * This runs on the main thread (the engine calls SNDDMA_GetDMAPos
	 * from S_GetSoundtime once per frame), so Cvar_SetValue is safe here.
	 * We never touch the cvar system from the audio callback itself --
	 * see s_underrunsLocal commentary at the top of this file.
	 */
	if ( s_underruns )
	{
		ma_uint32 cur = ma_atomic_uint32_get( &s_underrunsLocal );
		if ( (ma_uint32)s_underruns->integer != cur )
			Cvar_SetValue( "s_underruns", (float)cur );
	}

	return (int)ma_atomic_uint32_get( &s_dmapos );
}


/*
===============
SNDDMA_Shutdown

Stop and tear down the device, free the mixer ring buffer.
===============
*/
void SNDDMA_Shutdown( void )
{
	if ( !s_maInitialized )
		return;

	Cmd_RemoveCommand( "snd_test" );

	Com_Printf( "Closing miniaudio device...\n" );
	ma_device_uninit( &s_maDevice );

	if ( dma.buffer )
	{
		free( dma.buffer );
		dma.buffer = NULL;
	}

	s_dmasize_bytes   = 0;
	s_dmasize_samples = 0;
	ma_atomic_uint32_set( &s_dmapos, 0 );
	ma_atomic_uint32_set( &s_levelsRingPos, 0 );
	ma_atomic_uint32_set( &s_testFramesRemaining, 0 );
	ma_atomic_uint32_set( &s_testPhase, 0 );
	memset( s_levelsRing, 0, sizeof( s_levelsRing ) );
	s_maInitialized = qfalse;

	Com_Printf( "miniaudio: shut down.\n" );
}


/*
===============
SNDDMA_BeginPainting

No-op for the miniaudio callback model: the audio thread pulls from the
ring buffer whenever the device needs more data, and the painter writes
to the ring buffer asynchronously. There is no buffer to lock or remap.
Mirrors the SDL3 backend's empty implementation.
===============
*/
void SNDDMA_BeginPainting( void )
{
}


/*
===============
SNDDMA_Submit

No-op for the miniaudio callback model -- see SNDDMA_BeginPainting above.
The painter has already written into dma.buffer; the audio callback will
consume it on its next invocation. Mirrors the SDL3 backend.
===============
*/
void SNDDMA_Submit( void )
{
}

/*
===============
SNDDMA_Activate

No-op for miniaudio. Historically used by DirectSound to focus/unfocus the
audio device on window activation. miniaudio handles device state internally
and does not need per-window focus management -- but the symbol must exist
because code/win32/win_wndproc.c calls it.
===============
*/
void SNDDMA_Activate( void )
{
}

#endif  /* !DEDICATED */
