// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors
// snd_local.h -- private sound definitions


#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "snd_public.h"
#include "../qcommon/q_feats.h"

#define	PAINTBUFFER_SIZE		4096					// this is in samples

// The AVI audio track is always recorded as s16 stereo. The playback device may
// open with a surround channel count (5.1/7.1) and the mix is rendered into it,
// but the AVI WAV header is plain PCM, which only cleanly describes mono/stereo —
// a multi-channel plain-PCM track would need WAVE_FORMAT_EXTENSIBLE + a channel
// mask. So the capture format is pinned to this constant (independent of the live
// device channel count) and the surround mix is downmixed to stereo for the file.
#define AVI_CAPTURE_CHANNELS	2

#define SND_CHUNK_SIZE			1024					// samples
#define SND_CHUNK_SIZE_FLOAT	(SND_CHUNK_SIZE/2)		// floats
#define SND_CHUNK_SIZE_BYTE		(SND_CHUNK_SIZE*2)		// floats

typedef struct adpcm_state {
	short	sample;		/* Previous output value */
	byte	index;		/* Index into stepsize table (always [0, 88]) */
} adpcm_state_t;

typedef	struct sndBuffer_s {
	short					sndChunk[SND_CHUNK_SIZE];
	struct sndBuffer_s		*next;
	int						size;
	adpcm_state_t			adpcm;
} sndBuffer;

typedef struct sfx_s {
	sndBuffer		*soundData;
	qboolean		defaultSound;			// couldn't be loaded, so use buzz
	qboolean		inMemory;				// not in Memory
	qboolean		soundCompressed;		// not in Memory
	int				soundCompressionMethod;
	int 			soundLength;
	int				soundChannels;
	int				duration;				// cached length in milliseconds
	// Heap-owned path string. Set once via CopyString() in S_FindName,
	// freed in S_Base_Shutdown. NULL only for unused slots.
	const char		*soundName;
	int				lastTimeUsed;
	struct sfx_s	*next;

	// Flat interleaved s16 copy of the whole sound, decoded once on the main
	// thread the first time this sfx is played, for the engine to play.
	// ma_audio_buffer_ref wraps this contiguous block so the audio thread reads a
	// plain flat buffer — never decoding or walking the sndBuffer chain on the
	// audio thread. NULL until materialized; freed in S_Base_Shutdown.
	// Independent of the soundData chain, which is the decode source
	// (S_LoadSound) that enginePcm is built from.
	short			*enginePcm;			// malloc'd flat s16, or NULL
	int				enginePcmFrames;	// frame count in enginePcm (samples / channels)
	int				enginePcmRate;		// native sample rate of enginePcm (Hz)
} sfx_t;

typedef struct {
	unsigned int channels;
	unsigned int samples;				// mono samples in buffer
	int			fullsamples;			// samples with all channels in buffer (samples divided by channels)
	int			submission_chunk;		// don't mix less than this #
	int			samplebits;
	int			isfloat;
	int			speed;
	byte		*buffer;
	const char	*driver;
} dma_t;


#define START_SAMPLE_IMMEDIATE	0x7fffffff

#define MAX_DOPPLER_SCALE 50.0f //arbitrary

typedef struct loopSound_s {
	vec3_t		origin;
	vec3_t		velocity;
	sfx_t		*sfx;
	int			mergeFrame;
	qboolean	active;
	qboolean	kill;
	qboolean	doppler;
	float		dopplerScale;
	float		oldDopplerScale;
	int			framenum;
} loopSound_t;

typedef struct
{
	int			allocTime;
	int			startSample;	// START_SAMPLE_IMMEDIATE = set immediately on next mix
	int			entnum;			// to allow overriding a specific sound
	int			entchannel;		// to allow overriding a specific sound
	int			leftvol;		// 0-255 volume after spatialization
	int			rightvol;		// 0-255 volume after spatialization
	int			master_vol;		// 0-255 volume before spatialization
	float		dopplerScale;
	float		oldDopplerScale;
	vec3_t		origin;			// only use if fixed_origin is set
	qboolean	fixed_origin;	// use origin instead of fetching entnum's origin
	sfx_t		*thesfx;		// sfx structure
	qboolean	doppler;
} channel_t;


#define WAV_FORMAT_PCM			0x0001
#define WAVE_FORMAT_IEEE_FLOAT	0x0003

typedef struct {
	int			format;
	int			rate;
	int			width;
	int			channels;
	int			samples;
	int			dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

// Interface between Q3 sound "api" and the sound backend
typedef struct
{
	void (*Shutdown)(void);
	void (*StartSound)( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
	void (*StartLocalSound)( sfxHandle_t sfx, int channelNum );
	void (*StartBackgroundTrack)( const char *intro, const char *loop );
	void (*StopBackgroundTrack)( void );
	void (*StopAllSounds)( void );
	void (*ClearLoopingSounds)( qboolean killall );
	void (*AddLoopingSound)( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
	void (*AddRealLoopingSound)( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
	void (*StopLoopingSound)(int entityNum );
	void (*Respatialize)( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
	void (*UpdateEntityPosition)( int entityNum, const vec3_t origin );
	void (*Update)( int msec );
	void (*DisableSounds)( void );
	void (*BeginRegistration)( void );
	sfxHandle_t (*RegisterSound)( const char *sample, qboolean compressed );
	int (*SoundDuration)( sfxHandle_t handle );
	void (*ClearSoundBuffer)( void );
	void (*SoundInfo)( void );
	void (*SoundList)( void );
} soundInterface_t;


/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init(void);

// gets the current DMA position
int		SNDDMA_GetDMAPos(void);

// shutdown the DMA xfer.
void	SNDDMA_Shutdown(void);

void	SNDDMA_BeginPainting (void);

void	SNDDMA_Submit(void);

//====================================================================

#define	MAX_CHANNELS			96

extern	channel_t   s_channels[MAX_CHANNELS];

extern	int		s_soundtime;
extern	int		s_paintedtime;
extern	vec3_t	listener_forward;
extern	vec3_t	listener_right;
extern	vec3_t	listener_up;
extern	dma_t	dma;

extern cvar_t *s_volume;
extern cvar_t *s_musicVolume;
extern cvar_t *s_announcerVolume;
extern cvar_t *s_doppler;
extern cvar_t *s_muteWhenUnfocused;
extern cvar_t *s_muteWhenMinimized;

/* miniaudio backend cvars */
extern cvar_t *s_device;
extern cvar_t *s_latency;
extern cvar_t *s_underruns;

/* s_autoMute bitmask
 *   0 = never auto-mute
 *   1 = mute when the window loses input focus
 *   2 = mute when the window is minimized
 *   3 = mute in either case
 *
 * When non-zero it overrides the legacy s_muteWhenUnfocused/
 * s_muteWhenMinimized toggles.
 *
 * cl_matchAlerts bit 8 (match-alert unmute) can temporarily force audio
 * on via `s_autoMute_OverrideMute` — see cl_main.c.
 */
extern cvar_t *s_autoMute;
extern qboolean s_autoMute_OverrideMute; /* qtrue = force unmute */

/* Window-focus mute state (snd_dma.c). S_Base_FocusChanged is called once per
 * focus transition by the platform layer (via S_FocusChanged); the focus state
 * is read through S_FocusUnmuted instead of polling a window flag. */
void		S_Base_FocusChanged( qboolean focused );
qboolean	S_FocusUnmuted( void );

/* Auto-mute on window unfocus/minimize. S_ComputeAutoMute (snd_dma.c, next to
 * the focus state) decides whether audio should be silenced; S_UpdateAutoMute
 * (snd_miniaudio.c, next to the engine) applies it via the ma_engine master
 * volume, only on a state change. Both main-thread only, client-only (the
 * headless server has no window and no engine). */
#ifndef HEADLESS
qboolean	S_ComputeAutoMute( void );
void		S_UpdateAutoMute( void );
#endif

extern cvar_t *s_testsound;

qboolean S_LoadSound( sfx_t *sfx );

/* ma_engine source feed (snd_miniaudio.c). Main-thread only. S_EnginePlaySfx
 * plays a decoded sfx through ma_engine's graph as a real ma_sound (inert
 * unless the engine is up); S_EngineFreeSfxPcm releases the sfx's cached flat PCM at
 * shutdown. Both take a private sfx_t, so they live here rather than in the
 * engine-facing snd_public.h. */
#ifndef HEADLESS
qboolean	S_EnginePlaySfx( sfx_t *sfx );
void		S_EngineFreeSfxPcm( sfx_t *sfx );

/* ma_engine 3D positioning (snd_miniaudio.c). Main-thread only, inert unless
 * the engine is up. ma_engine's listener + per-voice ma_spatializer do the 3D
 * positioning.
 *   - S_EngineSetListener: update the listener each frame from the Q3 view
 *     (origin, forward = viewaxis[0], up = viewaxis[2]).
 *   - S_EnginePlaySfxEx: fire-and-forget one-shot; positioned in 3D when
 *     spatialized && origin != NULL, else listener-relative (self/local/UI).
 *   - S_EngineLoopBeginFrame / S_EngineLoopUpsert / S_EngineLoopEndFrame:
 *     per-frame reconcile of persistent looping voices keyed by (entityNum,sfx)
 *     — create once, reposition, uninit stale. */
void		S_EngineSetListener( const vec3_t origin, const vec3_t forward, const vec3_t up );
qboolean	S_EnginePlaySfxEx( sfx_t *sfx, const vec3_t origin, qboolean spatialized );
void		S_EngineReapVoices( void );
void		S_EngineLoopBeginFrame( void );
void		S_EngineLoopUpsert( int entityNum, sfx_t *sfx, const vec3_t origin, const vec3_t velocity, qboolean sphere );
void		S_EngineLoopEndFrame( void );
/* Drain the whole looping-voice registry immediately (mute/stop/map-load), not
 * just the per-frame stale sweep — called from S_Base_ClearSoundBuffer so engine
 * loops go silent with the rest of the mixer. */
void		S_EngineLoopStopAll( void );

/* Background-music streaming voice (snd_miniaudio.c). Main-thread only, inert
 * unless the engine is up. The decoder (S_UpdateBackgroundTrack) pushes s16 PCM into a
 * lock-free ring the audio thread drains at device rate.
 *   - S_EngineMusicStart: open a ring + non-spatialized ma_sound at the track's
 *     channels/rate; returns qfalse when the engine is not up.
 *   - S_EngineMusicActive: is a voice live (+ its channels/rate for format-change
 *     detection on loop).
 *   - S_EngineMusicWritableFrames / S_EngineMusicWrite: ring free space and push.
 *   - S_EngineMusicSetVolume: live s_musicVolume update.
 *   - S_EngineMusicStop: teardown (sound -> ring). */
qboolean	S_EngineMusicStart( int channels, int rate, float volume );
qboolean	S_EngineMusicActive( int *outChannels, int *outRate );
int			S_EngineMusicWritableFrames( void );
int			S_EngineMusicWrite( const short *interleaved, int frames );
void		S_EngineMusicSetVolume( float volume );
void		S_EngineMusicStop( void );

/* Offline AVI-audio capture (snd_miniaudio.c). Recording runs on a synthetic
 * per-video-frame clock, not real time, so the engine is switched to read-driven
 * (no-device) mode while capturing and pulled deterministically on the main
 * thread. S_EngineBeginCapture / S_EngineEndCapture re-init the engine into /
 * out of that mode (called from the AVI open/close lifecycle);
 * S_EngineReadCaptureFrames pulls `frames` frames of mixed output as interleaved
 * s16 (the AVI audio format). Main-thread only. */
void		S_EngineBeginCapture( void );
void		S_EngineEndCapture( void );
int			S_EngineReadCaptureFrames( short *out, int frames );

/* Cinematic (ROQ movie) audio streaming (snd_miniaudio.c). The movie decoder
 * pushes raw s16 PCM per frame; it is streamed through a lock-free ring feeding a
 * non-spatialized ma_sound (same primitive as music). Movie video is paced by the
 * wall clock, so no audio-clock coupling is needed. Main-thread only.
 *   - S_EngineCinematicFeed: (re)opens the voice at channels/rate on first use /
 *     format change and pushes `frames` of interleaved s16 (dropping overflow).
 *   - S_EngineCinematicStop: teardown (called when a movie ends / at shutdown). */
qboolean	S_EngineCinematicStart( int channels, int rate, float volume );
void		S_EngineCinematicFeed( const short *interleaved, int frames, int channels, int rate, float volume );
void		S_EngineCinematicStop( void );
/* Dev console command (snd_dma.c): s_enginePlay <sfxname> — plays a sound
 * through the ma_engine graph. Registered/removed in snd_main.c (S_Init/
 * S_Shutdown). Reaches the private sfx_t via s_knownSfx, so it lives in
 * snd_dma.c, not with the other command functions in snd_main.c. */
void		S_EnginePlay_f( void );
/* Dev console command (snd_miniaudio.c): s_engineDecodeTest — asset-independent
 * round-trip unit test of the method-0 flat decode. */
void		S_EngineDecodeTest_f( void );
/* Dev console command (snd_miniaudio.c): s_engineLevels — logs the peak/last
 * RMS from the output-tap capture ring, to confirm the tap sees signal. */
void		S_EngineLevels_f( void );
#endif

void		SND_free(sndBuffer *v);
sndBuffer*	SND_malloc( void );
void		SND_setup( void );
void		SND_shutdown( void );

// adpcm functions
#if FEAT_LEGACY_FORMATS_AUDIO
int  S_AdpcmMemoryNeeded( const wavinfo_t *info );
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples );
void S_AdpcmGetSamples(sndBuffer *chunk, short *to);
#endif

// Opus in-memory compression (soundCompressionMethod == 4)
#define OPUS_INMEM_FRAME_SAMPLES	960		// 20ms at 48kHz
#define OPUS_INMEM_RATE				48000

void  S_OpusEncodeSound( sfx_t *sfx, short *samples );
void  S_OpusDecoderInit( void );
void  S_OpusDecoderShutdown( void );
int   S_OpusGetSamples( const sfx_t *sc, int sampleOffset, short *out, int count );

// wavelet function

#define SENTINEL_MULAW_ZERO_RUN 127
#define SENTINEL_MULAW_FOUR_BIT_RUN 126

void S_FreeOldestSound( void );

// dead wavelet/muLaw CPU audio-compression removed (superseded by Opus)

extern short *sfxScratchBuffer;
extern sfx_t *sfxScratchPointer;
extern int	   sfxScratchIndex;

qboolean S_Base_Init( soundInterface_t *si );
