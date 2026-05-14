// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors


/*
The audio output backend was migrated from three platform-specific files
(`win_snd.c`, `linux_snd.c`, `sdl_snd.c`) to a single cross-platform `snd_miniaudio.c`
that uses the vendored [miniaudio](https://github.com/mackron/miniaudio) v0.11.25
single-header library via its low-level `ma_device` API only.

When working on audio:
- The audio callback in `S_MiniaudioCallback` MUST remain lock-free.
  Run `tools/check_audio_callback.sh` to verify.
- Do NOT use miniaudio's high-level `ma_engine` / `ma_sound` APIs — they would
  recreate the dual-mixer problem we deliberately avoided.
- Do NOT touch `snd_mix.c` (the engine mixer) or `S_SpatializeOrigin`. They are
  battle-tested and intentionally untouched by the migration.
- The dedicated server build skips the audio path via `#ifndef DEDICATED`.


*/

void S_Init( void );
void S_Shutdown( void );

/* temporarily override s_autoMute.  When enabled=qtrue, the sound
 * system will mix audio regardless of the s_autoMute state (used by
 * the match-alert bit 8 to make the alert audible even if the
 * window is unfocused). */
void S_SetMuteOverride( qboolean enabled );

// if origin is NULL, the sound will be dynamically sourced from the entity
void S_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
void S_StartLocalSound( sfxHandle_t sfx, int channelNum );

void S_StartBackgroundTrack( const char *intro, const char *loop );
void S_StopBackgroundTrack( void );

// cinematics and voice-over-network will send raw samples
// 1.0 volume will be direct output of source samples
void S_RawSamples (int samples, int rate, int width, int channels,
				   const byte *data, float volume);

// stop all sounds and the background track
void S_StopAllSounds( void );

// all continuous looping sounds must be added before calling S_Update
void S_ClearLoopingSounds( qboolean killall );
void S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void S_StopLoopingSound(int entityNum );

// recompute the relative volumes for all running sounds
// relative to the given entityNum / orientation
void S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );

// let the sound system know where an entity currently is
void S_UpdateEntityPosition( int entityNum, const vec3_t origin );

void S_Update( int msec );

void S_DisableSounds( void );

void S_BeginRegistration( void );

// RegisterSound will always return a valid sample, even if it
// has to create a placeholder.  This prevents continuous filesystem
// checks for missing files
sfxHandle_t	S_RegisterSound( const char *sample, qboolean compressed );

// Phase 6.2: returns the duration of a registered sound in milliseconds.
// Returns 0 if the handle is invalid or the sound system is not running.
int S_SoundDuration( sfxHandle_t handle );

void S_DisplayFreeMemory(void);

void S_ClearSoundBuffer( void );

void SNDDMA_Activate( void );

/* Wired UI audio waveform hook.
 * Copies the most recent `outCount` RMS levels from the lock-free ring
 * buffer written by the miniaudio audio callback into `outLevels` (oldest
 * first, newest last). Safe to call from the main/render thread. Returns
 * the number of levels written (≤ outCount, and ≤ S_LEVELS_RING_SIZE
 * internal to snd_miniaudio.c). When no audio device is running this
 * still writes zeros if the ring has been memset on shutdown. */
int S_GetRecentLevels( float *outLevels, int outCount );

/* Wired UI audio device dropdown hook.
 * Enumerates available playback devices via miniaudio and writes up to
 * `outCapacity` device-name pointers into `outNames`. The returned
 * pointers are owned by a static buffer inside snd_miniaudio.c and remain
 * valid until the next call to S_GetAudioDeviceList. Callers MUST NOT
 * free or modify the strings.
 *
 * Returns:
 *    > 0: number of device names written
 *    0:   no devices found (empty)
 *   -1:   enumeration failed (error)
 *
 * Threading: MAIN THREAD ONLY. Spins up a fresh ma_context, allocates
 * inside miniaudio, then uninits. Never call from the audio callback. */
int S_GetAudioDeviceList( const char **outNames, int outCapacity );
