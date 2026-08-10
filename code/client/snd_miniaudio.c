// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
snd_miniaudio.c -- audio output via miniaudio (single-header library)

Replaces the platform-specific backends (sdl_snd.c, win_snd.c, linux_snd.c)
with a single cross-platform output path. miniaudio routes to:
  - WASAPI on Windows
  - CoreAudio on macOS
  - PulseAudio / ALSA on Linux

Architecture:
  Game/cgame  ->  trap_S_*  ->  snd_dma.c (S_*)  ->  ma_engine graph (this file)
                                                       |
                                                       v
                              WASAPI / CoreAudio / PulseAudio / ALSA

  ma_engine owns the playback device and mixes its own node graph (per-voice sfx
  + spatializer, loop registry, streaming music / cinematic rings) directly into
  the device output. It calls S_EngineProcess (the onProcess tap) at the end of
  every mix pass for the snd_test sine + RMS capture. For offline AVI capture the
  engine runs read-driven (no device) and is pulled on the main thread.

Audio thread rules (HARD):
  - S_EngineProcess runs on ma_engine's internal audio thread (device mode).
  - NO mutexes, semaphores, or any blocking primitive in that path.
  - It touches only lock-free ma_atomic operations and per-sample math.

Vendored miniaudio version: see code/client/miniaudio.h header (v0.11.25).
This file is the SOLE place that defines MINIAUDIO_IMPLEMENTATION.
===========================================================================
*/

#ifndef HEADLESS  /* entire file is no-op for headless server builds */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "snd_local.h"
#include "../qcommon/wired/stalltrace.h"
LOG_DECLARE_CHANNEL( ch_sound, "sound" );

extern cvar_t *s_khz;

/* ---- module state ----------------------------------------------------- */

static qboolean         s_maInitialized = qfalse;

/*
 * ma_engine owns the playback device and mixes its node graph directly into the
 * output; it wires its own internal audio-thread callback (device-backed mode)
 * or is pulled by the host via ma_engine_read_pcm_frames (read-driven / no-device
 * mode, used for offline AVI capture). s_engineOwned is qtrue while the engine
 * is up.
 */
static qboolean         s_engineOwned = qfalse;
static ma_engine        s_maEngine;
static ma_uint32        s_engineChannels;   /* ma_engine's mix channel count (f32) */
/* qtrue while the engine runs in read-driven no-device mode (offline AVI capture,
 * pulled by S_EngineReadCaptureFrames on the main thread instead of a device
 * audio thread). Normal playback runs device-backed (s_engineOffline == qfalse). */
static qboolean         s_engineOffline = qfalse;
/* snd_test is registered once for the engine's lifetime and only removed at real
 * shutdown, so a device<->no-device mode switch does not double-register it. */
static qboolean         s_engineTestCmdAdded = qfalse;

/* Auto-mute applier state (S_UpdateAutoMute). The master volume is engine-owned
 * and resets to 1.0 every time an engine is created, so this tracker is reset at
 * every engine bring-up (S_ResetAutoMuteTracker, called from S_EngineInit) to
 * force the fresh engine's volume to reflect the current mute decision before the
 * first mix. In steady state the applier still only sets the volume on a change:
 * s_autoMuteApplied guards against per-frame churn; s_autoMuteLast holds the last
 * applied decision. */
static qboolean         s_autoMuteApplied = qfalse;   /* a decision has been applied */
static qboolean         s_autoMuteLast    = qfalse;   /* last applied mute state */

/*
 * Underrun counter. Incremented by the audio thread each time the
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

/* Per-channel RMS of the most recent output block, one entry per device channel,
 * stored as fixed-point (rms * 100000) so a single lock-free atomic store per
 * channel carries it from the audio thread to the main thread. Read by
 * s_engineLevels to confirm the spatializer panned a source to the correct
 * channel — including the rear/side channels on a surround (5.1/7.1) device, not
 * just L/R. Sized to the miniaudio channel-count ceiling; only [0, channels) are
 * written each block. */
#define S_LEVELS_FIXED_SCALE 100000.0f
static ma_atomic_uint32 s_levelFixed[MA_MAX_CHANNELS];

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


/* ---- ma_engine output tap (RUNS ON AUDIO THREAD -- NO LOCKS, NO MALLOC) ---
 *
 * ma_engine mixes its node graph directly into the device output; there is no
 * user device callback to hook. Instead ma_engine calls this onProcess proc at
 * the end of every mix pass, handing us the just-produced output block as
 * interleaved f32. Two taps read it:
 *   - the snd_test sine (overwrites the block for the test window), and
 *   - the RMS level capture that feeds the waveform widget.
 *
 * Lock-free: two atomic loads + sinf per sample, one RMS pass, no allocation,
 * no logging, no blocking.
 */
static void S_EngineProcess( void *pUserData, float *pFramesOut, ma_uint64 frameCount )
{
	ma_uint32 channels = s_engineChannels;
	(void)pUserData;

	if ( pFramesOut == NULL || channels == 0 )
		return;
	if ( channels > MA_MAX_CHANNELS )   /* bound the fixed per-channel level buffer */
		channels = MA_MAX_CHANNELS;

	/* Advance the engine's sample clock so the main thread's S_GetSoundtime (via
	 * SNDDMA_GetDMAPos) keeps progressing during normal playback. Wrap within the
	 * ring size. */
	if ( s_dmasize_samples > 0 )
	{
		ma_uint32 advance = (ma_uint32)frameCount * channels; /* mono samples */
		ma_uint32 newPos  = ( ma_atomic_uint32_get( &s_dmapos ) + advance ) % s_dmasize_samples;
		ma_atomic_uint32_set( &s_dmapos, newPos );
	}

	/* ---- snd_test sine injection (f32) ----------------------------
	 * Overwrite the engine's output with a 1 kHz sine across every
	 * channel for the test window. Runs after ma_engine mixed, so it
	 * fully replaces the mix output for the test period. */
	{
		ma_uint32 testRemaining = ma_atomic_uint32_get( &s_testFramesRemaining );
		if ( testRemaining > 0 )
		{
			ma_uint32 framesToWrite = ( (ma_uint64)testRemaining < frameCount )
			                          ? testRemaining : (ma_uint32)frameCount;
			ma_uint32 phase = ma_atomic_uint32_get( &s_testPhase );
			ma_uint32 sr = ma_engine_get_sample_rate( &s_maEngine );
			const float twopi_over_sr = 6.2831853071795864f / (float)( sr ? sr : 48000 );
			const float freq = 1000.0f; /* 1 kHz */
			ma_uint32 i, c;
			for ( i = 0; i < framesToWrite; i++ )
			{
				float sample = 0.25f * sinf( (float)( phase + i ) * freq * twopi_over_sr );
				for ( c = 0; c < channels; c++ )
					pFramesOut[ i * channels + c ] = sample;
			}
			ma_atomic_uint32_set( &s_testPhase, phase + framesToWrite );
			ma_atomic_uint32_set( &s_testFramesRemaining, testRemaining - framesToWrite );
		}
	}

	/* ---- RMS level capture (f32) ----------------------------------
	 * One combined RMS over the whole block for the waveform ring, plus a
	 * per-channel RMS (every device channel, not just L/R) so the main thread can
	 * confirm the spatializer panned a source to the correct channel — including
	 * rear/side channels on a surround device. All lock-free: fixed stack sums +
	 * atomic stores, no allocation, no logging. */
	{
		double    perChanSq[MA_MAX_CHANNELS];
		ma_uint64 totalSamples = frameCount * (ma_uint64)channels;
		double    sumSquares = 0.0;
		ma_uint32 c;
		ma_uint64 i;

		for ( c = 0; c < channels; c++ )
			perChanSq[c] = 0.0;

		for ( i = 0; i < frameCount; i++ )
		{
			const float *frame = &pFramesOut[ i * channels ];
			for ( c = 0; c < channels; c++ )
			{
				float s = frame[c];
				double sq = (double)( s * s );
				perChanSq[c] += sq;
				sumSquares   += sq;
			}
		}

		float rms = ( totalSamples > 0 )
		    ? (float)sqrt( sumSquares / (double)totalSamples )
		    : 0.0f;
		ma_uint32 ringPos = ma_atomic_uint32_get( &s_levelsRingPos );
		s_levelsRing[ ringPos % S_LEVELS_RING_SIZE ] = rms;
		ma_atomic_uint32_set( &s_levelsRingPos, ringPos + 1 );

		for ( c = 0; c < channels; c++ )
		{
			float rmsC = ( frameCount > 0 )
			    ? (float)sqrt( perChanSq[c] / (double)frameCount ) : 0.0f;
			ma_atomic_uint32_set( &s_levelFixed[c], (ma_uint32)( rmsC * S_LEVELS_FIXED_SCALE ) );
		}
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
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "snd_test: audio device not initialized\n" );
		return;
	}

	/* 2 seconds at the engine's sample rate. */
	ma_uint32 sampleRate = ma_engine_get_sample_rate( &s_maEngine );
	if ( sampleRate == 0 )
		sampleRate = 48000;
	ma_uint32 frames = 2 * sampleRate;
	ma_atomic_uint32_set( &s_testPhase, 0 );
	ma_atomic_uint32_set( &s_testFramesRemaining, frames );

	Com_Log( SEV_INFO, LOG_CH(ch_sound), "snd_test: playing 2 second 1 kHz sine sweep\n" );
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

/*
===============
S_EngineLevels_f

Dev console command: s_engineLevels — logs the peak and most-recent RMS level
from the audio-thread capture ring, plus the most-recent per-channel (left/
right) RMS. A quick main-thread way to confirm the output tap is seeing signal
(peak > 0 means audio reached the device output) and that the spatializer panned
a source to the correct side (left > right for a sound on the player's left).
Works on either audio path.
===============
*/
void S_EngineLevels_f( void )
{
	float       levels[S_LEVELS_RING_SIZE];
	int         n;
	int         i;
	float       peak = 0.0f;
	float       last = 0.0f;
	ma_uint32   channels;
	ma_device  *device;
	ma_uint32   c;

	if ( !s_maInitialized )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "s_engineLevels: audio not initialized\n" );
		return;
	}

	n = S_GetRecentLevels( levels, S_LEVELS_RING_SIZE );
	for ( i = 0; i < n; i++ )
	{
		if ( levels[i] > peak )
			peak = levels[i];
	}
	if ( n > 0 )
		last = levels[n - 1];

	channels = s_engineChannels;
	if ( channels > MA_MAX_CHANNELS )
		channels = MA_MAX_CHANNELS;

	Com_Log( SEV_INFO, LOG_CH(ch_sound),
	         "s_engineLevels: peak=%.5f last=%.5f over %d periods, %u channels:\n",
	         peak, last, n, (unsigned)channels );

	/* Per-channel RMS, labeled by the device's standard speaker position so a
	 * rear/side-panned source can be identified without hardcoding an index (the
	 * rear channel index differs between 5.1 and 7.1). The device is NULL in
	 * offline (no-device) capture mode — fall back to a bare index then. */
	device = ma_engine_get_device( &s_maEngine );
	for ( c = 0; c < channels; c++ )
	{
		float lvl = (float)ma_atomic_uint32_get( &s_levelFixed[c] ) / S_LEVELS_FIXED_SCALE;
		const char *pos = ( device != NULL )
		    ? ma_channel_position_to_string( device->playback.channelMap[c] )
		    : "n/a";
		Com_Log( SEV_INFO, LOG_CH(ch_sound),
		         "  ch%-2u %-6s = %.5f\n", (unsigned)c, pos, lvl );
	}
}


/* ---- ma_engine source feed (sfx -> ma_sound) -------------------------
 *
 * The first step of routing real sounds through ma_engine instead of the Q3
 * CPU mixer. A decoded sfx's PCM lives in the engine's sndBuffer chain as a
 * LINKED LIST of 1024-sample chunks (see snd_local.h). ma_engine wants a
 * ma_data_source; the simplest data source is ma_audio_buffer_ref, which wraps
 * a CONTIGUOUS block of PCM the caller keeps alive.
 *
 * So we decode the whole sfx ONCE on the main thread into a flat s16 buffer
 * (sfx->enginePcm), wrap it in a ma_audio_buffer_ref, and spawn a ma_sound from
 * it. The audio thread then reads a plain flat buffer through ma_engine's graph
 * — it never walks the chunk chain or touches a decoder. All decoding (Opus /
 * ADPCM / chunk-walk) happens here on the main thread.
 *
 * Ownership contract (miniaudio does NOT copy the source data):
 *   flat buffer (sfx->enginePcm)  outlives  ma_audio_buffer_ref  outlives  ma_sound
 * A live ma_sound holds a pointer into its ma_audio_buffer_ref, which holds a
 * pointer into sfx->enginePcm. Teardown order is therefore the reverse:
 *   ma_sound_uninit -> ma_audio_buffer_ref_uninit -> (flat buffer freed later)
 * The flat buffer is cached on the sfx and freed only at sound-system shutdown
 * (S_EngineFreeSfxPcm, called from S_Base_Shutdown), so it always outlives any
 * voice that references it.
 *
 * ma_engine's listener + per-voice ma_spatializer do the 3D positioning; sound
 * origins and the listener are mapped from Q3 world axes to miniaudio axes by
 * Q3ToMiniaudio (below).
 *
 * Voices come in two shapes:
 *   - ONE-SHOT: fire-and-forget, positioned once at start, reaped on end. Used
 *     for S_StartSound (and the s_enginePlay dev command). Self / local / UI /
 *     announcer sounds play unspatialized (listener-relative, full volume).
 *   - LOOPING: a PERSISTENT ma_sound keyed by (entityNum, sfx), created once with
 *     looping enabled and REPOSITIONED every frame (never restarted — a restart
 *     is an audible click). A per-frame present/absent sweep uninits any loop
 *     voice whose (entityNum,sfx) was not re-asserted this frame.
 */

/* Q3 (forward=+X, left=+Y, up=+Z) -> miniaudio (right=+X, up=+Y, forward=-Z).
 * Applied identically to the listener position, listener forward, listener
 * world-up, and every sound position. Finalized in Q3ToMiniaudio below. */

#define S_ENGINE_MAX_ONESHOTS   16
#define S_ENGINE_MAX_LOOPS      64   /* persistent per-(entityNum,sfx) loop voices */

/* Distance parameters mirroring the Q3 mixer's two falloff modes (snd_dma.c),
 * selected by the s_linearFalloff cvar. Both modes are linear in distance, so
 * ma_attenuation_model_linear expresses both — only the min/max distances differ:
 *
 *   s_linearFalloff 1 (default): full volume at 0, linear to silence at 1250.
 *       -> min_distance 0, max_distance 1250 (= SOUND_MAX_DIST)
 *   s_linearFalloff 0 (Q3 classic power mode): full volume out to 80 units, then
 *       linear rolloff reaching silence at 80 + 1/0.0008 = 1330 units.
 *       -> min_distance 80 (= SOUND_FULLVOLUME), max_distance 1330
 *
 * rolloff stays 1.0 in both. Sphere loops (SPHERE_VOL) use the SAME curve at a
 * lower base volume, not a reshaped one — the ratios come from the Q3 mixer's
 * MASTER_VOL / SPHERE_VOL. */
#define S_ENGINE_SOUND_MAX_DIST     1250.0f    /* linear-mode cull distance */
#define S_ENGINE_FULLVOLUME_DIST    80.0f      /* classic-mode inner full-volume radius */
#define S_ENGINE_CLASSIC_MAX_DIST   ( S_ENGINE_FULLVOLUME_DIST + S_ENGINE_SOUND_MAX_DIST ) /* 1330 */
#define S_ENGINE_MASTER_VOL         127
#define S_ENGINE_SPHERE_VOL         90

/* One-shot voice: transient, reaped when its ma_sound reaches the end. */
typedef struct {
	ma_sound             sound;
	ma_audio_buffer_ref  bufRef;
	qboolean             active;
} engineVoice_t;

static engineVoice_t s_engineVoices[S_ENGINE_MAX_ONESHOTS];

/* Persistent looping voice, one per live (entityNum, sfx) pair. seenFrame marks
 * the mark-and-sweep: a voice not touched this frame is stale and gets uninited. */
typedef struct {
	ma_sound             sound;
	ma_audio_buffer_ref  bufRef;
	sfx_t               *sfx;        /* which sound (also identifies the flat PCM) */
	int                  entityNum;  /* owning entity */
	int                  seenFrame;  /* last frame this voice was re-asserted */
	qboolean             active;
} engineLoopVoice_t;

static engineLoopVoice_t s_engineLoops[S_ENGINE_MAX_LOOPS];
static int               s_engineLoopFrame;   /* bumped once per reconcile sweep */

/* Background-music streaming voice. The music decoder (S_UpdateBackgroundTrack,
 * main thread) pushes freshly decoded PCM into a lock-free ring; ma_engine's
 * audio thread drains the ring at device rate through the ma_sound built on it.
 * ma_pcm_rb's first member is a ma_data_source, so the ring feeds ma_sound
 * directly. Non-spatialized (music is 2D full-screen). Looping is handled by the
 * decoder refilling the ring (reopening the loop file on EOF), not by ma_sound.
 * Sized generously so the once-per-frame producer never starves the device. */
#define S_ENGINE_MUSIC_RING_FRAMES  32768   /* ~0.68 s at 48 kHz — ample slack */

typedef struct {
	ma_pcm_rb  rb;
	ma_sound   sound;
	int        channels;   /* ring channel count (matches the stream) */
	int        rate;       /* ring native sample rate (engine resamples) */
	qboolean   active;
} engineMusic_t;

static engineMusic_t s_engineMusic;

/*
===============
S_EngineMaterializeSfx

Decode the whole sfx into a flat, contiguous, interleaved s16 buffer suitable
for ma_audio_buffer_ref. Runs on the MAIN THREAD only (it may invoke the Opus /
ADPCM decoders, which are non-reentrant and main-thread-only).

Decodes once and caches the result on sfx->enginePcm; subsequent calls are a
cheap no-op that returns the cached buffer. The buffer holds
soundLength * soundChannels shorts (soundLength is the per-channel frame count).

Fills *outFrames with the frame count and *outRate with the buffer's native
sample rate (the engine resamples to the mix rate). Returns qtrue on success.
===============
*/
static qboolean S_EngineMaterializeSfx( sfx_t *sfx, int *outFrames, int *outRate )
{
	int channels;
	int frames;
	int rate;

	if ( sfx == NULL || sfx->defaultSound || sfx->soundData == NULL || sfx->soundLength <= 0 )
		return qfalse;

	channels = sfx->soundChannels;
	if ( channels != 1 && channels != 2 )
		return qfalse;

	frames = sfx->soundLength;   /* per-channel frame count */

	/* Native rate: raw (method 0) and ADPCM (method 1) were resampled to the
	 * mixer rate at load time (ResampleSfx uses dma.speed); Opus (method 4) is
	 * always the fixed 48 kHz in-memory rate. The engine resamples either way. */
	rate = ( sfx->soundCompressionMethod == 4 ) ? OPUS_INMEM_RATE : dma.speed;
	if ( rate <= 0 )
		rate = 48000;

	/* Already materialized — return the cache. */
	if ( sfx->enginePcm != NULL )
	{
		if ( outFrames ) *outFrames = sfx->enginePcmFrames;
		if ( outRate )   *outRate   = sfx->enginePcmRate;
		return qtrue;
	}

	{
		size_t totalShorts = (size_t)frames * (size_t)channels;
		short *flat = (short *)calloc( totalShorts, sizeof( short ) );
		if ( flat == NULL )
		{
			Com_Log( SEV_WARN, LOG_CH(ch_sound),
			         "S_EngineMaterializeSfx: out of memory for %s (%u shorts)\n",
			         sfx->soundName ? sfx->soundName : "?", (unsigned)totalShorts );
			return qfalse;
		}

		switch ( sfx->soundCompressionMethod )
		{
		case 0:
			{
				/* Raw s16: walk the sndBuffer chain, copying the interleaved
				 * samples exactly as S_PaintChannelFrom16 reads them. Each chunk
				 * holds SND_CHUNK_SIZE (1024) shorts; the last chunk is partial. */
				sndBuffer *chunk = sfx->soundData;
				size_t copied = 0;
				while ( copied < totalShorts && chunk != NULL )
				{
					size_t inThisChunk = totalShorts - copied;
					if ( inThisChunk > (size_t)SND_CHUNK_SIZE )
						inThisChunk = SND_CHUNK_SIZE;
					memcpy( flat + copied, chunk->sndChunk, inThisChunk * sizeof( short ) );
					copied += inThisChunk;
					chunk = chunk->next;
				}
				if ( copied != totalShorts )
				{
					Com_Log( SEV_WARN, LOG_CH(ch_sound),
					         "S_EngineMaterializeSfx: %s chunk chain short (%u of %u shorts)\n",
					         sfx->soundName ? sfx->soundName : "?",
					         (unsigned)copied, (unsigned)totalShorts );
					free( flat );
					return qfalse;
				}
			}
			break;

#if FEAT_LEGACY_FORMATS_AUDIO
		case 1:
			{
				/* ADPCM: always mono. Each sndBuffer chunk decodes to
				 * SND_CHUNK_SIZE_BYTE*2 (4096) mono samples via S_AdpcmGetSamples.
				 * Decode chunk-by-chunk into a scratch, copy what we still need. */
				short scratch[SND_CHUNK_SIZE_BYTE * 2];
				sndBuffer *chunk = sfx->soundData;
				size_t copied = 0;
				while ( copied < totalShorts && chunk != NULL )
				{
					size_t inThisChunk = totalShorts - copied;
					if ( inThisChunk > (size_t)( SND_CHUNK_SIZE_BYTE * 2 ) )
						inThisChunk = SND_CHUNK_SIZE_BYTE * 2;
					S_AdpcmGetSamples( chunk, scratch );
					memcpy( flat + copied, scratch, inThisChunk * sizeof( short ) );
					copied += inThisChunk;
					chunk = chunk->next;
				}
				if ( copied != totalShorts )
				{
					Com_Log( SEV_WARN, LOG_CH(ch_sound),
					         "S_EngineMaterializeSfx: %s ADPCM chain short (%u of %u samples)\n",
					         sfx->soundName ? sfx->soundName : "?",
					         (unsigned)copied, (unsigned)totalShorts );
					free( flat );
					return qfalse;
				}
			}
			break;
#endif

		case 4:
			{
				/* Opus in-memory: decode the whole mono stream at 48 kHz on the
				 * main thread. S_OpusGetSamples may return fewer than requested
				 * only at the true end of stream. */
				int got = S_OpusGetSamples( sfx, 0, flat, frames );
				if ( got != frames )
				{
					Com_Log( SEV_WARN, LOG_CH(ch_sound),
					         "S_EngineMaterializeSfx: %s Opus decoded %d of %d samples\n",
					         sfx->soundName ? sfx->soundName : "?", got, frames );
					/* Not fatal: zero-pad the tail (calloc already zeroed it). */
				}
			}
			break;

		default:
			Com_Log( SEV_WARN, LOG_CH(ch_sound),
			         "S_EngineMaterializeSfx: %s unsupported compression method %d\n",
			         sfx->soundName ? sfx->soundName : "?", sfx->soundCompressionMethod );
			free( flat );
			return qfalse;
		}

		sfx->enginePcm       = flat;
		sfx->enginePcmFrames = frames;
		sfx->enginePcmRate   = rate;
	}

	if ( outFrames ) *outFrames = sfx->enginePcmFrames;
	if ( outRate )   *outRate   = sfx->enginePcmRate;
	return qtrue;
}

/*
===============
Q3ToMiniaudio

Map a Q3 world vector (forward=+X, left=+Y, up=+Z) into miniaudio's coordinate
system (right=+X, up=+Y, forward=-Z). This single linear map is applied
identically to the listener position, the listener forward vector, the listener
world-up vector, and every sound position, so that L/R and front/back pan the
same way the Q3 stereo panner did.

  out[0] = -in[1]   Q3 +Y (LEFT)    -> miniaudio -X (left of +X-right)
  out[1] =  in[2]   Q3 +Z (UP)      -> miniaudio +Y (up)
  out[2] = -in[0]   Q3 +X (FORWARD) -> miniaudio -Z (forward)

Both systems are right-handed; this map is a proper rotation (determinant +1),
so it rotates rather than mirrors — no silent pan inversion.
===============
*/
static void Q3ToMiniaudio( const vec3_t in, float out[3] )
{
	out[0] = -in[1];
	out[1] =  in[2];
	out[2] = -in[0];
}

/*
===============
S_EngineConfigAttenuation

Configure one positioned ma_sound's distance attenuation to match the Q3 mixer's
falloff, honoring the s_linearFalloff cvar's two modes. Both Q3 modes are linear
in distance, so ma_attenuation_model_linear (OpenAL linear-clamped, gain =
1 - (dist - min) / (max - min), clamped) reproduces each exactly by choosing the
min/max distances:

  s_linearFalloff 1: min 0,  max 1250  -> gain = 1 - dist/1250
  s_linearFalloff 0: min 80, max 1330  -> full volume within 80 units, then linear
                                           to silence at 1330 (Q3's SOUND_FULLVOLUME
                                           / SOUND_ATTENUATE power mode is linear)

rolloff stays 1.0. `baseVolume` is the peak level (1.0 for one-shots and point
loops at MASTER_VOL; ~0.71 for the quieter sphere loops at SPHERE_VOL) — Q3's
SPHERE_VOL is just a lower master fed to the same curve, so we scale the base
volume and keep the curve identical.
===============
*/
static void S_EngineConfigAttenuation( ma_sound *snd, float baseVolume )
{
	float minDist, maxDist;

	if ( Cvar_VariableIntegerValue( "s_linearFalloff" ) != 0 )
	{
		minDist = 0.0f;
		maxDist = S_ENGINE_SOUND_MAX_DIST;
	}
	else
	{
		minDist = S_ENGINE_FULLVOLUME_DIST;
		maxDist = S_ENGINE_CLASSIC_MAX_DIST;
	}

	ma_sound_set_attenuation_model( snd, ma_attenuation_model_linear );
	ma_sound_set_min_distance( snd, minDist );
	ma_sound_set_max_distance( snd, maxDist );
	ma_sound_set_rolloff( snd, 1.0f );
	ma_sound_set_volume( snd, baseVolume );
}

/*
===============
S_EngineSetDopplerEnabled

Turn a positioned voice's doppler on or off. A positioned ma_sound has
dopplerFactor 1 by default (doppler already active), so to keep s_doppler 0
silent we must explicitly zero the factor. When enabled we leave the factor at
the default 1 (miniaudio's standard OpenAL model, speed of sound 343.3) and rely
on per-frame velocity feeding to drive the pitch shift.
===============
*/
static void S_EngineSetDopplerEnabled( ma_sound *snd, qboolean enabled )
{
	ma_sound_set_doppler_factor( snd, enabled ? 1.0f : 0.0f );
}

/*
===============
S_EngineSetVelocity

Feed a source velocity (Q3 world frame) to a positioned voice, mapped through
the same axis transform as its position — miniaudio projects velocity onto the
listener-to-source vector, so a raw Q3-frame velocity would give an inverted or
scrambled doppler sign. Must be called every frame the source moves so the
spatializer's doppler tracks the current motion.
===============
*/
static void S_EngineSetVelocity( ma_sound *snd, const vec3_t velocity )
{
	float v[3];
	Q3ToMiniaudio( velocity, v );
	ma_sound_set_velocity( snd, v[0], v[1], v[2] );
}

/*
===============
S_EngineInitSoundFromSfx

Shared voice construction: wrap an sfx's flat PCM in a ma_audio_buffer_ref and
init a ma_sound from it. On success `bufRef` and `sound` are live and must be
torn down in order (sound, then bufRef). Does NOT start the sound. Returns qtrue
on success; on failure nothing is left initialized.

flags: MA_SOUND_FLAG_NO_SPATIALIZATION for listener-relative (self/local/UI/
announcer) voices, 0 for positioned voices.
===============
*/
static qboolean S_EngineInitSoundFromSfx( sfx_t *sfx, ma_uint32 flags,
                                          ma_audio_buffer_ref *bufRef, ma_sound *sound )
{
	int       frames = 0;
	int       rate   = 0;
	ma_result result;

	if ( !S_EngineMaterializeSfx( sfx, &frames, &rate ) )
		return qfalse;

	/* The ref does NOT copy the data — it points into sfx->enginePcm, which
	 * outlives the voice. ma_audio_buffer_ref_init leaves sampleRate 0, so set
	 * the native rate ourselves; the engine resamples to the mix rate. */
	result = ma_audio_buffer_ref_init( ma_format_s16, (ma_uint32)sfx->soundChannels,
	                                   sfx->enginePcm, (ma_uint64)frames, bufRef );
	if ( result != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine sound: buffer-ref init failed (%d)\n",
		         (int)result );
		return qfalse;
	}
	bufRef->sampleRate = (ma_uint32)rate;

	result = ma_sound_init_from_data_source( &s_maEngine, bufRef, flags, NULL, sound );
	if ( result != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine sound: sound init failed (%d)\n",
		         (int)result );
		ma_audio_buffer_ref_uninit( bufRef );
		return qfalse;
	}

	return qtrue;
}

/*
===============
S_EngineSetListener

Update the ma_engine listener from the Q3 view each frame (called from
S_Base_Respatialize on the engine path). origin is the view origin, forward is
Q3 viewaxis[0], up is Q3 viewaxis[2]; all three are mapped through Q3ToMiniaudio.
Listener index 0 always exists (ma_engine forces at least one).
===============
*/
void S_EngineSetListener( const vec3_t origin, const vec3_t forward, const vec3_t up )
{
	float p[3], f[3], u[3];

	if ( !s_maInitialized || !s_engineOwned )
		return;

	Q3ToMiniaudio( origin,  p );
	Q3ToMiniaudio( forward, f );
	Q3ToMiniaudio( up,      u );

	ma_engine_listener_set_position ( &s_maEngine, 0, p[0], p[1], p[2] );
	ma_engine_listener_set_direction( &s_maEngine, 0, f[0], f[1], f[2] );
	ma_engine_listener_set_world_up ( &s_maEngine, 0, u[0], u[1], u[2] );
}

/*
===============
S_ResetAutoMuteTracker

Forget the last-applied mute state so the next S_UpdateAutoMute treats its
decision as a change and re-applies the master volume. Called at engine bring-up
(the fresh engine's master volume is 1.0 regardless of what was applied to the
previous engine) so the mute state survives snd_restart / vid_restart / the
AVI-capture device<->no-device switch.
===============
*/
static void S_ResetAutoMuteTracker( void )
{
	s_autoMuteApplied = qfalse;
}

/*
===============
S_UpdateAutoMute

Apply the window focus/minimize auto-mute decision to the engine by driving the
master output volume to 0 (muted) or 1 (audible). The decision itself lives in
S_ComputeAutoMute (main thread; reads the focus state + cvars). Sets the volume
only on a state transition, not every frame — ma_engine_set_volume is a lock-free
atomic exchange the audio thread reads, so a main-thread set is safe with no
per-frame work. No-op until the engine is up.

Called on a focus transition (S_Base_FocusChanged), once per sound frame
(S_Update_, covering minimize/restore and cvar/override changes without extra
event plumbing since SDL raises no focus event on minimize), and once at engine
bring-up (S_EngineInit, after S_ResetAutoMuteTracker) so a freshly created engine
starts at the correct volume.
===============
*/
void S_UpdateAutoMute( void )
{
	qboolean wantMute;

	if ( !s_maInitialized || !s_engineOwned )
		return;   /* engine not up — nothing to apply to yet */

	wantMute = S_ComputeAutoMute();
	if ( s_autoMuteApplied && wantMute == s_autoMuteLast )
		return;   /* no change — leave the master volume as is */

	ma_engine_set_volume( &s_maEngine, wantMute ? 0.0f : 1.0f );
	s_autoMuteLast    = wantMute;
	s_autoMuteApplied = qtrue;
}

/*
===============
S_EngineReapVoices

Free any one-shot voices whose sound has finished. Called on the main thread
before allocating a new one-shot, once per frame from the loop reconcile (so
finished one-shots are released promptly regardless of play cadence), and on
shutdown. ma_sound_at_end reports true once a non-looping sound has played
through; we uninit it and its buffer-ref in the correct order (sound first, then
the ref it reads from). Looping voices are never reaped here — they end via the
stale sweep in S_EngineLoopEndFrame or S_EngineLoopStopAll.
===============
*/
void S_EngineReapVoices( void )
{
	int i;
	if ( !s_maInitialized || !s_engineOwned )
		return;
	for ( i = 0; i < S_ENGINE_MAX_ONESHOTS; i++ )
	{
		if ( !s_engineVoices[i].active )
			continue;
		if ( ma_sound_at_end( &s_engineVoices[i].sound ) )
		{
			ma_sound_uninit( &s_engineVoices[i].sound );
			ma_audio_buffer_ref_uninit( &s_engineVoices[i].bufRef );
			s_engineVoices[i].active = qfalse;
		}
	}
}

/*
===============
S_EnginePlaySfxEx

Play a decoded sfx through ma_engine's graph as a fire-and-forget one-shot. If
`spatialized` and `origin` is non-NULL, the voice is positioned in 3D (mapped
through Q3ToMiniaudio) with Q3-matched linear attenuation; otherwise it plays
listener-relative at full volume (self / local / UI / announcer sounds). Reaped
on end. Runs on the MAIN THREAD. Returns qtrue if a voice started.

Inert unless the engine is up.
===============
*/
qboolean S_EnginePlaySfxEx( sfx_t *sfx, const vec3_t origin, qboolean spatialized )
{
	int       slot;
	ma_uint32 flags;

	if ( !s_maInitialized || !s_engineOwned || sfx == NULL )
		return qfalse;

	S_EngineReapVoices();

	for ( slot = 0; slot < S_ENGINE_MAX_ONESHOTS; slot++ )
	{
		if ( !s_engineVoices[slot].active )
			break;
	}
	if ( slot >= S_ENGINE_MAX_ONESHOTS )
	{
		Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "engine one-shots: all %d busy\n",
		         S_ENGINE_MAX_ONESHOTS );
		return qfalse;
	}

	flags = ( spatialized && origin != NULL ) ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;

	if ( !S_EngineInitSoundFromSfx( sfx, flags,
	                                &s_engineVoices[slot].bufRef,
	                                &s_engineVoices[slot].sound ) )
		return qfalse;

	if ( spatialized && origin != NULL )
	{
		float p[3];
		Q3ToMiniaudio( origin, p );
		ma_sound_set_position( &s_engineVoices[slot].sound, p[0], p[1], p[2] );
		S_EngineConfigAttenuation( &s_engineVoices[slot].sound, 1.0f );
		/* One-shots carry no velocity (transient — doppler on a sub-second sound
		 * is negligible), and a positioned voice defaults to doppler on, so zero
		 * the factor to keep it a pure static-position source. */
		S_EngineSetDopplerEnabled( &s_engineVoices[slot].sound, qfalse );
	}

	if ( ma_sound_start( &s_engineVoices[slot].sound ) != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine one-shot: start failed for %s\n",
		         sfx->soundName ? sfx->soundName : "?" );
		ma_sound_uninit( &s_engineVoices[slot].sound );
		ma_audio_buffer_ref_uninit( &s_engineVoices[slot].bufRef );
		return qfalse;
	}

	s_engineVoices[slot].active = qtrue;
	return qtrue;
}

/*
===============
S_EnginePlaySfx

Unspatialized fire-and-forget play (the s_enginePlay dev command). Kept as a thin
wrapper so the command logs the same one-line summary it always has.
===============
*/
qboolean S_EnginePlaySfx( sfx_t *sfx )
{
	if ( !S_EnginePlaySfxEx( sfx, NULL, qfalse ) )
		return qfalse;

	Com_Log( SEV_INFO, LOG_CH(ch_sound),
	         "S_EnginePlaySfx: %s (%d frames, %d ch) played (unspatialized)\n",
	         sfx->soundName ? sfx->soundName : "?", sfx->enginePcmFrames, sfx->soundChannels );
	return qtrue;
}

/* ---- looping voice registry (persistent per (entityNum, sfx)) ---------- */

/*
===============
S_EngineLoopBeginFrame

Start a per-frame loop-reconcile sweep. Bumps the frame stamp so voices touched
by S_EngineLoopUpsert this frame are marked current; untouched ones are pruned
by S_EngineLoopEndFrame. Call once before the loopSounds[] walk each frame.
===============
*/
void S_EngineLoopBeginFrame( void )
{
	if ( !s_maInitialized || !s_engineOwned )
		return;
	s_engineLoopFrame++;
}

/*
===============
S_EngineLoopUpsert

Reconcile one looping source for this frame. If a persistent voice already
exists for (entityNum, sfx), it is repositioned in place (NEVER restarted — a
restart would click/retrigger) and its velocity refreshed. Otherwise a new
looping ma_sound is created, positioned, and started once. `origin`/`velocity`
are the Q3 world position/velocity; `sphere` picks the quieter/wider SPHERE_VOL
base level (S_AddRealLoopingSound) vs the louder point-source MASTER_VOL
(S_AddLoopingSound).

Doppler: point sources honor it when s_doppler is non-zero (velocity fed each
frame, factor 1); sphere sources keep doppler off, matching the Q3 mixer which
never doppler-shifts S_AddRealLoopingSound. When s_doppler is 0 the factor is
zeroed so there is no pitch shift at all.

Runs on the MAIN THREAD. Inert unless the engine is up.
===============
*/
void S_EngineLoopUpsert( int entityNum, sfx_t *sfx, const vec3_t origin, const vec3_t velocity, qboolean sphere )
{
	int      slot = -1;   /* first free registry slot */
	int      i;
	float    p[3];
	float    baseVolume;
	qboolean doppler;

	if ( !s_maInitialized || !s_engineOwned || sfx == NULL )
		return;

	Q3ToMiniaudio( origin, p );

	/* Point sources doppler when s_doppler is on; sphere sources never do. */
	doppler = ( !sphere && s_doppler && s_doppler->integer != 0 ) ? qtrue : qfalse;

	/* Existing voice for this (entityNum, sfx): reposition + refresh velocity,
	 * mark current. Velocity must update every frame so the doppler tracks. */
	for ( i = 0; i < S_ENGINE_MAX_LOOPS; i++ )
	{
		if ( s_engineLoops[i].active &&
		     s_engineLoops[i].entityNum == entityNum &&
		     s_engineLoops[i].sfx == sfx )
		{
			ma_sound_set_position( &s_engineLoops[i].sound, p[0], p[1], p[2] );
			/* Keep the doppler state in sync so toggling s_doppler mid-session
			 * takes effect on already-playing loops. */
			S_EngineSetDopplerEnabled( &s_engineLoops[i].sound, doppler );
			S_EngineSetVelocity( &s_engineLoops[i].sound, doppler ? velocity : vec3_origin );
			s_engineLoops[i].seenFrame = s_engineLoopFrame;
			return;
		}
		if ( slot < 0 && !s_engineLoops[i].active )
			slot = i;
	}

	if ( slot < 0 )
	{
		Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "engine loops: registry full (%d)\n",
		         S_ENGINE_MAX_LOOPS );
		return;
	}

	/* New looping voice. */
	if ( !S_EngineInitSoundFromSfx( sfx, 0,
	                                &s_engineLoops[slot].bufRef,
	                                &s_engineLoops[slot].sound ) )
		return;

	baseVolume = sphere ? ( (float)S_ENGINE_SPHERE_VOL / (float)S_ENGINE_MASTER_VOL ) : 1.0f;
	ma_sound_set_position( &s_engineLoops[slot].sound, p[0], p[1], p[2] );
	S_EngineConfigAttenuation( &s_engineLoops[slot].sound, baseVolume );
	S_EngineSetDopplerEnabled( &s_engineLoops[slot].sound, doppler );
	if ( doppler )
		S_EngineSetVelocity( &s_engineLoops[slot].sound, velocity );
	ma_sound_set_looping( &s_engineLoops[slot].sound, MA_TRUE );

	if ( ma_sound_start( &s_engineLoops[slot].sound ) != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine loop: start failed for %s\n",
		         sfx->soundName ? sfx->soundName : "?" );
		ma_sound_uninit( &s_engineLoops[slot].sound );
		ma_audio_buffer_ref_uninit( &s_engineLoops[slot].bufRef );
		return;
	}

	s_engineLoops[slot].sfx       = sfx;
	s_engineLoops[slot].entityNum = entityNum;
	s_engineLoops[slot].seenFrame = s_engineLoopFrame;
	s_engineLoops[slot].active    = qtrue;

	/* One-time per voice (create only — not per frame): a re-asserted loop takes
	 * the reposition branch above and never reaches here, so this cannot spam. */
	Com_Log( SEV_DEBUG, LOG_CH(ch_sound),
	         "engine loop: start ent %d %s (%s, doppler %s) at (%.0f %.0f %.0f)\n",
	         entityNum, sfx->soundName ? sfx->soundName : "?",
	         sphere ? "sphere" : "point", doppler ? "on" : "off", p[0], p[1], p[2] );
}

/*
===============
S_EngineLoopEndFrame

Finish the loop-reconcile sweep: uninit any looping voice not re-asserted this
frame (its (entityNum, sfx) went silent). Call once after the loopSounds[] walk.
Teardown order sound -> bufRef; the flat PCM stays owned by sfx_t.
===============
*/
void S_EngineLoopEndFrame( void )
{
	int i;
	if ( !s_maInitialized || !s_engineOwned )
		return;

	for ( i = 0; i < S_ENGINE_MAX_LOOPS; i++ )
	{
		if ( !s_engineLoops[i].active )
			continue;
		if ( s_engineLoops[i].seenFrame != s_engineLoopFrame )
		{
			Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "engine loop: stop ent %d %s\n",
			         s_engineLoops[i].entityNum,
			         s_engineLoops[i].sfx && s_engineLoops[i].sfx->soundName
			             ? s_engineLoops[i].sfx->soundName : "?" );
			ma_sound_uninit( &s_engineLoops[i].sound );
			ma_audio_buffer_ref_uninit( &s_engineLoops[i].bufRef );
			s_engineLoops[i].sfx    = NULL;
			s_engineLoops[i].active = qfalse;
		}
	}
}

/*
===============
S_EngineLoopStopAll

Immediately uninit every live looping voice (sound -> bufRef). Unlike the
per-frame stale sweep, this drains the whole registry regardless of the frame
stamp. Needed because the loop reconcile only runs from S_AddLoopSounds, which is
skipped when the sound system is muted/stopped: S_Base_Respatialize early-returns
on s_soundMuted, and S_Base_ClearSoundBuffer (map load, StopAllSounds,
DisableSounds) zeroes loopSounds[] without ever re-entering the reconcile. Since
ma_engine keeps mixing every live ma_sound to the device, those loop voices would
otherwise stay audible while the rest of the mixer is silent. S_Base_ClearSoundBuffer
calls this on the engine path so the loops go silent with everything else. The
flat PCM buffers stay owned by sfx_t.
===============
*/
void S_EngineLoopStopAll( void )
{
	int i;
	if ( !s_maInitialized || !s_engineOwned )
		return;
	for ( i = 0; i < S_ENGINE_MAX_LOOPS; i++ )
	{
		if ( !s_engineLoops[i].active )
			continue;
		ma_sound_uninit( &s_engineLoops[i].sound );
		ma_audio_buffer_ref_uninit( &s_engineLoops[i].bufRef );
		s_engineLoops[i].sfx    = NULL;
		s_engineLoops[i].active = qfalse;
	}
}

/*
===============
S_EngineStopAllVoices

Uninit every live voice — one-shots and loops — sound first then its buffer-ref.
Called at shutdown before ma_engine_uninit so no voice outlives the engine. The
flat PCM buffers are freed separately (S_EngineFreeSfxPcm), since they are cached
on sfx_t and shared across plays.
===============
*/
static void S_EngineStopAllVoices( void )
{
	int i;
	for ( i = 0; i < S_ENGINE_MAX_ONESHOTS; i++ )
	{
		if ( !s_engineVoices[i].active )
			continue;
		ma_sound_uninit( &s_engineVoices[i].sound );
		ma_audio_buffer_ref_uninit( &s_engineVoices[i].bufRef );
		s_engineVoices[i].active = qfalse;
	}
	S_EngineLoopStopAll();
}

/*
===============
S_EngineFreeSfxPcm

Free a sfx's cached flat engine-PCM buffer, if any. Called from S_Base_Shutdown
for every known sfx, after the voices have been stopped, so no ma_sound still
references the buffer. Safe to call on an sfx that was never materialized.
===============
*/
void S_EngineFreeSfxPcm( sfx_t *sfx )
{
	if ( sfx == NULL || sfx->enginePcm == NULL )
		return;
	free( sfx->enginePcm );
	sfx->enginePcm       = NULL;
	sfx->enginePcmFrames = 0;
	sfx->enginePcmRate   = 0;
}

/* ---- background-music streaming voice (ma_pcm_rb -> ma_sound) ---------- */

/*
===============
S_EngineMusicStop

Tear down the streaming music voice: uninit the ma_sound first, then the ring it
reads from (mirrors the sfx voice teardown order). Called on track stop and at
shutdown before ma_engine_uninit. Safe to call when no music voice is active.
===============
*/
void S_EngineMusicStop( void )
{
	if ( !s_engineMusic.active )
		return;
	ma_sound_uninit( &s_engineMusic.sound );
	ma_pcm_rb_uninit( &s_engineMusic.rb );
	s_engineMusic.active   = qfalse;
	s_engineMusic.channels = 0;
	s_engineMusic.rate     = 0;
}

/*
===============
S_EngineMusicStart

Bring up a streaming music voice for a track of the given channel count and
sample rate (s16 interleaved). Creates a lock-free PCM ring, wraps it in a
non-spatialized ma_sound (music is 2D), applies the current music volume, and
starts it. The decoder then keeps the ring filled via S_EngineMusicWrite; the
audio thread drains it at device rate (the engine resamples from `rate`).

Any previous music voice is stopped first. Inert (returns qfalse) unless
ma_engine owns the device. Runs on the MAIN THREAD.
===============
*/
qboolean S_EngineMusicStart( int channels, int rate, float volume )
{
	ma_result result;

	if ( !s_maInitialized || !s_engineOwned )
		return qfalse;

	if ( channels != 1 && channels != 2 )
		return qfalse;
	if ( rate <= 0 )
		rate = 48000;

	S_EngineMusicStop();

	result = ma_pcm_rb_init( ma_format_s16, (ma_uint32)channels,
	                         S_ENGINE_MUSIC_RING_FRAMES, NULL, NULL, &s_engineMusic.rb );
	if ( result != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine music: ring init failed (%d)\n", (int)result );
		return qfalse;
	}
	/* The ring is a data source; tag its native rate so ma_engine resamples the
	 * stream to the device rate. */
	ma_pcm_rb_set_sample_rate( &s_engineMusic.rb, (ma_uint32)rate );

	result = ma_sound_init_from_data_source( &s_maEngine, &s_engineMusic.rb,
	                                         MA_SOUND_FLAG_NO_SPATIALIZATION, NULL,
	                                         &s_engineMusic.sound );
	if ( result != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine music: sound init failed (%d)\n", (int)result );
		ma_pcm_rb_uninit( &s_engineMusic.rb );
		return qfalse;
	}

	ma_sound_set_volume( &s_engineMusic.sound, volume );

	if ( ma_sound_start( &s_engineMusic.sound ) != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine music: start failed\n" );
		ma_sound_uninit( &s_engineMusic.sound );
		ma_pcm_rb_uninit( &s_engineMusic.rb );
		return qfalse;
	}

	s_engineMusic.channels = channels;
	s_engineMusic.rate     = rate;
	s_engineMusic.active   = qtrue;

	Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "engine music: started (%d ch, %d Hz)\n", channels, rate );
	return qtrue;
}

/*
===============
S_EngineMusicActive

True when a streaming music voice is live. Also reports its channel count and
rate (out params may be NULL) so the producer can detect a format change on loop
and re-open the voice.
===============
*/
qboolean S_EngineMusicActive( int *outChannels, int *outRate )
{
	if ( outChannels ) *outChannels = s_engineMusic.channels;
	if ( outRate )     *outRate     = s_engineMusic.rate;
	return s_engineMusic.active;
}

/*
===============
S_EngineMusicWritableFrames

Frames of free space in the music ring — how much the decoder may push this
call. 0 when no music voice is active. Runs on the MAIN THREAD.
===============
*/
int S_EngineMusicWritableFrames( void )
{
	if ( !s_engineMusic.active )
		return 0;
	return (int)ma_pcm_rb_available_write( &s_engineMusic.rb );
}

/*
===============
S_EngineMusicWrite

Push up to `frames` of interleaved s16 PCM (at the ring's channel count) into the
music ring. Uses acquire/commit and loops to handle the ring's internal wrap, so
a single call can straddle the buffer boundary. Returns the number of frames
actually written (may be < frames if the ring fills). Runs on the MAIN THREAD;
the audio thread only drains the ring, so this is lock-free.
===============
*/
int S_EngineMusicWrite( const short *interleaved, int frames )
{
	int written = 0;

	if ( !s_engineMusic.active || interleaved == NULL || frames <= 0 )
		return 0;

	while ( written < frames )
	{
		ma_uint32 want = (ma_uint32)( frames - written );
		void     *dst  = NULL;
		ma_result r    = ma_pcm_rb_acquire_write( &s_engineMusic.rb, &want, &dst );
		if ( r != MA_SUCCESS || want == 0 || dst == NULL )
			break;   /* ring full (or error) — drop the remainder this call */

		memcpy( dst, interleaved + (size_t)written * s_engineMusic.channels,
		        (size_t)want * s_engineMusic.channels * sizeof( short ) );

		ma_pcm_rb_commit_write( &s_engineMusic.rb, want );
		written += (int)want;
	}

	return written;
}

/*
===============
S_EngineMusicSetVolume

Update the music voice volume (from s_musicVolume) on the fly. No-op when no
music voice is active. Runs on the MAIN THREAD.
===============
*/
void S_EngineMusicSetVolume( float volume )
{
	if ( !s_engineMusic.active )
		return;
	ma_sound_set_volume( &s_engineMusic.sound, volume );
}

/* ---- cinematic (ROQ movie) audio streaming voice (ma_pcm_rb -> ma_sound) ----
 *
 * ROQ cinematic audio is a raw PCM stream (fixed 22050 Hz s16) the movie decoder
 * pushes each frame; it is fed to ma_engine through a lock-free ring exactly like
 * the music voice. Movie video is paced by the wall clock independently, so no
 * lip-sync clock coupling is needed — ma_engine drains the ring at device rate.
 * Non-spatialized (2D). */
typedef struct {
	ma_pcm_rb  rb;
	ma_sound   sound;
	int        channels;
	int        rate;
	qboolean   active;
} engineCinematic_t;

static engineCinematic_t s_engineCinematic;

/*
===============
S_EngineCinematicStop

Tear down the cinematic streaming voice (ma_sound before its ring). Called when a
movie ends and at shutdown. Safe when no cinematic voice is active.
===============
*/
void S_EngineCinematicStop( void )
{
	if ( !s_engineCinematic.active )
		return;
	ma_sound_uninit( &s_engineCinematic.sound );
	ma_pcm_rb_uninit( &s_engineCinematic.rb );
	s_engineCinematic.active   = qfalse;
	s_engineCinematic.channels = 0;
	s_engineCinematic.rate     = 0;
}

/*
===============
S_EngineCinematicStart

Bring up a cinematic streaming voice at the given channel count / sample rate
(s16 interleaved). Any previous cinematic voice is stopped first. Non-spatialized.
Inert unless the engine is up. Main-thread only. Returns qtrue on success.
===============
*/
qboolean S_EngineCinematicStart( int channels, int rate, float volume )
{
	ma_result result;

	if ( !s_maInitialized || !s_engineOwned )
		return qfalse;
	if ( channels != 1 && channels != 2 )
		return qfalse;
	if ( rate <= 0 )
		rate = 22050;

	S_EngineCinematicStop();

	result = ma_pcm_rb_init( ma_format_s16, (ma_uint32)channels,
	                         S_ENGINE_MUSIC_RING_FRAMES, NULL, NULL, &s_engineCinematic.rb );
	if ( result != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine cinematic: ring init failed (%d)\n", (int)result );
		return qfalse;
	}
	ma_pcm_rb_set_sample_rate( &s_engineCinematic.rb, (ma_uint32)rate );

	result = ma_sound_init_from_data_source( &s_maEngine, &s_engineCinematic.rb,
	                                         MA_SOUND_FLAG_NO_SPATIALIZATION, NULL,
	                                         &s_engineCinematic.sound );
	if ( result != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine cinematic: sound init failed (%d)\n", (int)result );
		ma_pcm_rb_uninit( &s_engineCinematic.rb );
		return qfalse;
	}

	ma_sound_set_volume( &s_engineCinematic.sound, volume );

	if ( ma_sound_start( &s_engineCinematic.sound ) != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "engine cinematic: start failed\n" );
		ma_sound_uninit( &s_engineCinematic.sound );
		ma_pcm_rb_uninit( &s_engineCinematic.rb );
		return qfalse;
	}

	s_engineCinematic.channels = channels;
	s_engineCinematic.rate     = rate;
	s_engineCinematic.active   = qtrue;
	return qtrue;
}

/*
===============
S_EngineCinematicFeed

Push interleaved s16 PCM into the cinematic ring, (re)opening the voice at
`channels`/`rate` on first use or a format change. Drops overflow frames when the
ring is full (the movie decoder can burst ahead during a video catch-up) — same
best-effort "video leads, audio follows" tolerance the retired raw-stream had.
`volume` updates the live voice each call. Main-thread only.
===============
*/
void S_EngineCinematicFeed( const short *interleaved, int frames, int channels, int rate, float volume )
{
	int written = 0;

	if ( !s_maInitialized || !s_engineOwned || interleaved == NULL || frames <= 0 )
		return;

	if ( !s_engineCinematic.active ||
	     s_engineCinematic.channels != channels ||
	     s_engineCinematic.rate != rate )
	{
		if ( !S_EngineCinematicStart( channels, rate, volume ) )
			return;
	}
	else
	{
		ma_sound_set_volume( &s_engineCinematic.sound, volume );
	}

	while ( written < frames )
	{
		ma_uint32 want = (ma_uint32)( frames - written );
		void     *dst  = NULL;
		ma_result r    = ma_pcm_rb_acquire_write( &s_engineCinematic.rb, &want, &dst );
		if ( r != MA_SUCCESS || want == 0 || dst == NULL )
			break;   /* ring full — drop the remainder (best-effort) */

		memcpy( dst, interleaved + (size_t)written * channels,
		        (size_t)want * channels * sizeof( short ) );
		ma_pcm_rb_commit_write( &s_engineCinematic.rb, want );
		written += (int)want;
	}
}

/*
===============
S_EngineDecodeSelfTest

Asset-independent, deterministic unit test for the method-0 flat decode in
S_EngineMaterializeSfx: builds an in-RAM sfx_t whose sndBuffer chain holds a
known interleaved-stereo ramp spanning several 1024-sample chunks (plus a
partial final chunk, and a chunk boundary that falls mid-frame), materializes
it, and asserts the flat buffer round-trips the source exactly — count, first
and last samples, and the samples straddling a chunk boundary.

This exercises the chunk-walk that is otherwise hard to cover without real sound
assets. It builds its own sndBuffer chain (SND_malloc), so it needs the sound
buffer pool set up (SND_setup); the s_engineDecodeTest command guards that.

Returns qtrue if every assertion holds. On failure it logs the specific
mismatch at SEV_WARN and returns qfalse (it never terminates — this is a dev
diagnostic, not a boot invariant).
===============
*/
static qboolean S_EngineDecodeSelfTest( void )
{
	sfx_t     sfx;
	sndBuffer *chunk;
	sndBuffer *head;
	int        channels     = 2;
	int        frames       = 2600;   /* > 2 chunks of interleaved stereo (2*1024) */
	int        totalShorts  = frames * channels;  /* 5200 shorts across 6 chunks */
	int        i;
	int        outFrames    = 0;
	int        outRate      = 0;
	qboolean   ok           = qtrue;

	/* Build the source sndBuffer chain, filling sndChunk[] with a known pattern:
	 * sample n = (short)( (n % 60000) - 30000 ), so consecutive shorts are
	 * distinct and span negative/positive. Chain of ceil(totalShorts/1024)
	 * chunks; the final chunk is partial. */
	memset( &sfx, 0, sizeof( sfx ) );
	sfx.soundChannels          = channels;
	sfx.soundLength            = frames;
	sfx.soundCompressionMethod = 0;
	sfx.soundName              = "<engine-decode-selftest>";
	head  = NULL;
	chunk = NULL;
	for ( i = 0; i < totalShorts; i++ )
	{
		int part = i & ( SND_CHUNK_SIZE - 1 );
		if ( part == 0 )
		{
			sndBuffer *nc = SND_malloc();
			if ( nc == NULL )
			{
				Com_Log( SEV_WARN, LOG_CH(ch_sound),
				         "S_EngineDecodeSelfTest: SND_malloc failed at short %d\n", i );
				/* Free what we built and bail without asserting. */
				while ( head ) { sndBuffer *nx = head->next; SND_free( head ); head = nx; }
				return qfalse;
			}
			nc->next = NULL;
			if ( chunk == NULL ) head = nc; else chunk->next = nc;
			chunk = nc;
		}
		chunk->sndChunk[part] = (short)( ( i % 60000 ) - 30000 );
	}
	sfx.soundData = head;

	if ( !S_EngineMaterializeSfx( &sfx, &outFrames, &outRate ) )
	{
		Com_Log( SEV_WARN, LOG_CH(ch_sound), "S_EngineDecodeSelfTest: materialize failed\n" );
		ok = qfalse;
	}
	else
	{
		if ( outFrames != frames )
		{
			Com_Log( SEV_WARN, LOG_CH(ch_sound),
			         "S_EngineDecodeSelfTest: frame count %d != %d\n", outFrames, frames );
			ok = qfalse;
		}
		/* Verify EVERY short round-trips (the chain is small; exhaustive is
		 * cheap and catches any chunk-boundary or stride error). */
		for ( i = 0; i < totalShorts && ok; i++ )
		{
			short expected = (short)( ( i % 60000 ) - 30000 );
			if ( sfx.enginePcm[i] != expected )
			{
				Com_Log( SEV_WARN, LOG_CH(ch_sound),
				         "S_EngineDecodeSelfTest: short %d = %d, expected %d "
				         "(chunk boundary near %d)\n",
				         i, (int)sfx.enginePcm[i], (int)expected, SND_CHUNK_SIZE );
				ok = qfalse;
			}
		}
		if ( ok )
		{
			Com_Log( SEV_INFO, LOG_CH(ch_sound),
			         "S_EngineDecodeSelfTest: PASS (method 0, %d frames x %d ch, "
			         "%d shorts across %d chunks, native %d Hz)\n",
			         frames, channels, totalShorts,
			         ( totalShorts + SND_CHUNK_SIZE - 1 ) / SND_CHUNK_SIZE, outRate );
		}
	}

	/* Tear down the fixture: free the cached flat buffer and the source chain. */
	S_EngineFreeSfxPcm( &sfx );
	while ( head ) { sndBuffer *nx = head->next; SND_free( head ); head = nx; }

	return ok;
}

/*
===============
S_EngineDecodeTest_f

Dev console command: s_engineDecodeTest — runs the asset-independent method-0
flat-decode self-test and reports PASS/FAIL. Tests the decode helper only (not
the device path), so it does not need an active device.
===============
*/
void S_EngineDecodeTest_f( void )
{
	if ( S_EngineDecodeSelfTest() )
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "s_engineDecodeTest: PASS\n" );
	else
		Com_Log( SEV_WARN, LOG_CH(ch_sound), "s_engineDecodeTest: FAIL\n" );
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
the context. NEVER call from the audio callback (S_EngineProcess) — it
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
S_EngineInit

Bring up ma_engine as the audio mixer with an ENGINE-OWNED device: ma_engine
creates and owns its playback device internally (native → WASAPI / CoreAudio /
PulseAudio / ALSA; web → the WebAudio backend), wires its own audio-thread
callback, and mixes directly. No sources are fed yet — feeding the sfx buffer
chain through a custom data source is a later step — so output is near-silent;
this establishes the device ownership + the output tap.

The engine's post-mix output is tapped via the onProcess callback
(S_EngineProcess), which carries the snd_test sine and the RMS waveform capture.

Also populates the dma_t struct so the rest of the sound system (the sample
clock) keeps running.

Returns qtrue on success.
===============
*/
static qboolean S_EngineInit( qboolean offline )
{
	ma_engine_config engineConfig;
	ma_result        result;
	ma_uint32        sampleRate = (ma_uint32)SNDDMA_KHzToHz( s_khz ? s_khz->integer : 48 );
	ma_uint32        mixerSamples;
	int              approxSamples;

	if ( sampleRate == 0 )
		sampleRate = 48000;

	engineConfig            = ma_engine_config_init();
	engineConfig.sampleRate = sampleRate;   /* 0 would let the device decide; pin to s_khz */
	engineConfig.onProcess  = S_EngineProcess;
	engineConfig.pProcessUserData = NULL;

	if ( offline )
	{
		/* No device: the engine creates no playback device and no audio thread.
		 * The host pulls the whole graph deterministically via
		 * S_EngineReadCaptureFrames (used for AVI capture, which runs on a
		 * synthetic per-video-frame clock, not real time). With no device to
		 * infer from, the channel count and rate MUST be pinned. The AVI track is
		 * stereo, so the offline engine mixes to stereo directly (miniaudio's
		 * spatializer downmixes each voice into the stereo map) — the live device
		 * channel count is deliberately NOT used here so recording never renders a
		 * surround track. Rate matches the AVI format snapshot. */
		engineConfig.noDevice   = MA_TRUE;
		engineConfig.channels   = AVI_CAPTURE_CHANNELS;
		engineConfig.sampleRate = (ma_uint32)dma.speed;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_sound), "Opening ma_engine (%s)...\n",
	         offline ? "read-driven, no device" : "engine-owned device" );
	STALLTRACE( "ma_engine_init", result = ma_engine_init( &engineConfig, &s_maEngine ) );
	if ( result != MA_SUCCESS )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "miniaudio: ma_engine_init failed (code %d)\n", (int)result );
		return qfalse;
	}

	/* The engine's actual channel count / sample rate come from the device it
	 * opened (or the pinned config in no-device mode). The output tap needs the
	 * channel count to stride the f32 block. */
	s_engineChannels = ma_engine_get_channels( &s_maEngine );
	if ( s_engineChannels == 0 )
		s_engineChannels = 2;
	sampleRate = ma_engine_get_sample_rate( &s_maEngine );
	if ( sampleRate == 0 )
		sampleRate = 48000;

	/*
	 * Populate dma_t: the speed / channels / samplebits fields describe the
	 * output format that the sample clock reads. channels tracks the real device
	 * channel count (s_engineChannels) so the sample clock strides correctly on a
	 * surround (5.1/7.1) device; the AVI capture path uses its own stereo constant
	 * instead of this field. Mirror the ring sizing so downstream size assumptions
	 * hold.
	 */
	if ( sampleRate <= 11025 )      approxSamples = 256;
	else if ( sampleRate <= 22050 ) approxSamples = 512;
	else if ( sampleRate <= 44100 ) approxSamples = 1024;
	else                            approxSamples = 2048;
	mixerSamples  = (ma_uint32)( approxSamples * 2 * 10 );
	mixerSamples -= mixerSamples % 2;
	mixerSamples  = (ma_uint32)log2pad( mixerSamples, 1 );
	/* The sample clock credits dma.fullsamples = mixerSamples/channels frames per
	 * ring wrap, so mixerSamples must be an exact multiple of the channel count or
	 * the truncation drifts the clock every wrap. The log2pad size is a power of
	 * two (divides 2/4/8 evenly — unchanged there), but a 5.1 device (6 channels)
	 * does not divide it; round the ring size down to a channel multiple so
	 * dma.fullsamples * channels == mixerSamples exactly for every channel count.
	 * The ring is wrapped with %, not a power-of-two mask, so this is safe. */
	mixerSamples -= mixerSamples % s_engineChannels;

	memset( &dma, 0, sizeof( dma ) );
	dma.speed            = (int)sampleRate;
	dma.channels         = (int)s_engineChannels;
	dma.samplebits       = 16;
	dma.isfloat          = 0;
	dma.samples          = (int)mixerSamples;
	dma.fullsamples      = dma.samples / dma.channels;
	dma.submission_chunk = 1;
	dma.driver           = "miniaudio-engine";

	s_bytesPerSample  = (ma_uint32)( dma.samplebits / 8 );
	s_dmasize_samples = mixerSamples;
	s_dmasize_bytes   = mixerSamples * s_bytesPerSample;

	dma.buffer = (byte *)calloc( 1, s_dmasize_bytes );
	if ( dma.buffer == NULL )
	{
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "miniaudio: failed to allocate %u byte mixer buffer\n",
		            (unsigned)s_dmasize_bytes );
		ma_engine_uninit( &s_maEngine );
		return qfalse;
	}

	ma_atomic_uint32_set( &s_dmapos, 0 );
	ma_atomic_uint32_set( &s_underrunsLocal, 0 );
	if ( s_underruns )
		Cvar_Set( "s_underruns", "0" );
	ma_atomic_uint32_set( &s_levelsRingPos, 0 );
	ma_atomic_uint32_set( &s_testFramesRemaining, 0 );
	ma_atomic_uint32_set( &s_testPhase, 0 );
	memset( s_levelsRing, 0, sizeof( s_levelsRing ) );
	{
		ma_uint32 lc;
		for ( lc = 0; lc < MA_MAX_CHANNELS; lc++ )
			ma_atomic_uint32_set( &s_levelFixed[lc], 0 );
	}

	/* A device-backed engine auto-starts on init (noAutoStart defaults false), so
	 * the device is already running here. A no-device engine has no thread and is
	 * pulled by the host instead. */
	s_engineOwned   = qtrue;
	s_engineOffline = offline;
	s_maInitialized = qtrue;

	/* The fresh engine's master volume is 1.0. Re-apply the current auto-mute
	 * decision so audio comes up silenced if the window is unfocused/minimized —
	 * across snd_restart / vid_restart / the capture mode switch, where the
	 * per-frame applier's state would otherwise be stale against this new engine. */
	S_ResetAutoMuteTracker();
	S_UpdateAutoMute();

	if ( !s_engineTestCmdAdded )
	{
		Cmd_AddCommand( "snd_test", S_Test_f );
		s_engineTestCmdAdded = qtrue;
	}

	Com_Log( SEV_INFO, LOG_CH(ch_sound),
	            "ma_engine: %u Hz, %u ch (f32 mix)%s, mix ring = %u mono samples\n",
	            (unsigned)sampleRate, (unsigned)s_engineChannels,
	            offline ? ", read-driven" : "", (unsigned)s_dmasize_samples );

	return qtrue;
}

/*
===============
S_EngineSwitchMode

Re-initialize the engine into device-backed or read-driven (no-device) mode. All
live voices, loops, and the music stream are bound to the current engine, so they
are stopped first (while it is alive), then the engine is uninited and rebuilt in
the target mode. The voice pools are left empty; playback repopulates them the
usual way (loops re-assert each frame, music re-opens, one-shots on demand). The
sfx flat-PCM caches on sfx_t survive — they are independent of the engine.

Used to enter/leave offline AVI capture: recording drives the engine read-driven
on a synthetic per-video-frame clock, so it must not own a real-time device while
capturing. Returns qtrue on success.
===============
*/
static qboolean S_EngineSwitchMode( qboolean offline )
{
	if ( !s_maInitialized || !s_engineOwned )
		return qfalse;
	if ( s_engineOffline == offline )
		return qtrue;   /* already in the requested mode */

	/* Stop everything bound to the current engine, in the established order
	 * (music + cinematic + one-shots + loops, each sound before its ring/ref) so
	 * no ma_sound outlives the engine it belongs to. */
	S_EngineMusicStop();
	S_EngineCinematicStop();
	S_EngineStopAllVoices();

	Com_Log( SEV_INFO, LOG_CH(ch_sound), "Switching ma_engine to %s mode...\n",
	         offline ? "read-driven (capture)" : "device" );
	ma_engine_uninit( &s_maEngine );

	/* S_EngineInit re-allocates dma.buffer; free the old one so it does not leak
	 * across the switch. dma.channels / dma.speed are preserved and pin the
	 * no-device engine's format to the AVI format. */
	if ( dma.buffer )
	{
		free( dma.buffer );
		dma.buffer = NULL;
	}
	s_maInitialized = qfalse;   /* S_EngineInit re-asserts it */

	if ( !S_EngineInit( offline ) )
	{
		Com_Log( SEV_WARN, LOG_CH(ch_sound), "ma_engine mode switch failed\n" );
		return qfalse;
	}
	return qtrue;
}

/*
===============
S_EngineBeginCapture / S_EngineEndCapture

Enter / leave offline AVI capture. On begin, the engine goes read-driven (no
device); on end, it returns to the normal device-backed mode. Main-thread only.
Inert unless the engine is up. Called from the AVI open/close lifecycle so the
engine's output format already matches the AVI header (dma.channels / dma.speed).
===============
*/
void S_EngineBeginCapture( void )
{
	S_EngineSwitchMode( qtrue );
}

void S_EngineEndCapture( void )
{
	S_EngineSwitchMode( qfalse );
}

/*
===============
S_EngineReadCaptureFrames

Deterministically pull `frames` frames of mix output from the read-driven engine
and write them to `out` as interleaved s16 STEREO (AVI_CAPTURE_CHANNELS) — the AVI
audio format — regardless of the engine's own channel count. Only valid while the
engine is in offline capture mode; returns 0 otherwise.

The offline engine is opened at AVI_CAPTURE_CHANNELS, so miniaudio's spatializer
already mixes each voice down into the stereo bus and s_engineChannels is normally
2 (a straight copy here). The mono and >2 cases are handled defensively so a
malformed capture engine still yields a valid non-silent stereo track rather than
truncating channels: mono is duplicated to both sides, and a wider mix is folded
by averaging every source channel equally into L and R (energy-preserving, no
channel silently dropped).

ma_engine's graph is f32; ma_engine_read_pcm_frames mixes the whole graph into an
f32 scratch, which we clamp and convert to s16.

Runs on the MAIN THREAD (a no-device engine has no audio thread — the read IS the
mix). Returns frames written.
===============
*/
int S_EngineReadCaptureFrames( short *out, int frames )
{
	float     scratch[4096];       /* interleaved f32, drained in bounded chunks */
	int       done = 0;
	ma_uint32 inCh;
	ma_uint64 maxChunk;

	if ( !s_maInitialized || !s_engineOffline || out == NULL || frames <= 0 )
		return 0;
	if ( s_engineChannels == 0 )
		return 0;

	inCh = s_engineChannels;

	/* Chunk so that want * inCh never overruns the scratch. */
	maxChunk = (ma_uint64)( sizeof( scratch ) / sizeof( scratch[0] ) ) / inCh;
	if ( maxChunk == 0 )
		return 0;

	while ( done < frames )
	{
		ma_uint64 want = (ma_uint64)( frames - done );
		ma_uint64 got  = 0;
		ma_uint64 i;
		if ( want > maxChunk )
			want = maxChunk;

		if ( ma_engine_read_pcm_frames( &s_maEngine, scratch, want, &got ) != MA_SUCCESS )
			break;
		if ( got == 0 )
			break;

		for ( i = 0; i < got; i++ )
		{
			const float *in = &scratch[ i * inCh ];
			short       *o  = &out[ (size_t)( done + i ) * AVI_CAPTURE_CHANNELS ];
			float        l, r;

			if ( inCh == 1 )
			{
				l = r = in[0];                 /* mono -> both sides */
			}
			else if ( inCh == 2 )
			{
				l = in[0];                     /* stereo -> straight copy */
				r = in[1];
			}
			else
			{
				/* Wider mix -> fold every channel equally into L and R so no
				 * channel is dropped (defensive; the offline engine is stereo). */
				float sum = 0.0f;
				ma_uint32 c;
				for ( c = 0; c < inCh; c++ )
					sum += in[c];
				l = r = sum / (float)inCh;
			}

			if ( l >  1.0f ) l =  1.0f;
			if ( l < -1.0f ) l = -1.0f;
			if ( r >  1.0f ) r =  1.0f;
			if ( r < -1.0f ) r = -1.0f;
			o[0] = (short)( l * 32767.0f );
			o[1] = (short)( r * 32767.0f );
		}
		done += (int)got;
	}

	return done;
}

/*
===============
SNDDMA_Init

Open the audio output. ma_engine owns the device and mixes its graph directly;
S_EngineInit populates the dma_t struct the rest of the sound system reads.
Returns qtrue on success.
===============
*/
qboolean SNDDMA_Init( void )
{
	if ( s_maInitialized )
		return qtrue;

	return S_EngineInit( qfalse );
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
	s_engineTestCmdAdded = qfalse;

	/* Tear down live engine voices before the engine that hosts them. Their flat
	 * PCM buffers are freed separately (S_EngineFreeSfxPcm) from S_Base_Shutdown,
	 * since they are cached on sfx_t. The streaming music and cinematic voices
	 * (ma_sound + their rings) are torn down here too, before the engine. */
	S_EngineMusicStop();
	S_EngineCinematicStop();
	S_EngineStopAllVoices();
	Com_Log( SEV_INFO, LOG_CH(ch_sound), "Closing ma_engine...\n" );
	STALLTRACE( "ma_engine_uninit", ma_engine_uninit( &s_maEngine ) );
	s_engineOwned    = qfalse;
	s_engineOffline  = qfalse;
	s_engineChannels = 0;

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

	Com_Log( SEV_INFO, LOG_CH(ch_sound), "miniaudio: shut down.\n" );
}


/*
===============
SNDDMA_BeginPainting / SNDDMA_Submit

No-ops retained as part of the SNDDMA_* backend interface. ma_engine mixes its
graph directly into the device, so there is no external buffer to lock, remap, or
submit. Mirror the SDL3 backend's empty implementations.
===============
*/
void SNDDMA_BeginPainting( void )
{
}

void SNDDMA_Submit( void )
{
}

/*
===============
SNDDMA_Activate

No-op for miniaudio. Historically used by DirectSound to focus/unfocus the
audio device on window activation. miniaudio handles device state internally
and does not need per-window focus management. The symbol is retained as part
of the sound public API (snd_public.h); focus-driven muting now lives in the
mixer (see S_FocusChanged / S_FocusUnmuted).
===============
*/
void SNDDMA_Activate( void )
{
}

#endif  /* !HEADLESS */
