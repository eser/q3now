// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*****************************************************************************
 * name:		snd_dma.c
 *
 * desc:		main control for any streaming sound output device
 *
 * $Archive: /MissionPack/code/client/snd_dma.c $
 *
 *****************************************************************************/

#include "snd_local.h"
#include "snd_codec.h"
#include "client.h"
LOG_DECLARE_CHANNEL( ch_client, "client" );
LOG_DECLARE_CHANNEL( ch_sound, "sound" );

static void S_Update_( int msec );
static void S_UpdateBackgroundTrack( void );
static void S_Base_StopAllSounds( void );
static void S_Base_StopBackgroundTrack( void );
static void S_memoryLoad( sfx_t *sfx );

static snd_stream_t *s_backgroundStream = NULL;
static char s_backgroundLoop[MAX_QPATH];
//static char		s_backgroundMusic[MAX_QPATH]; //TTimo: unused

// =======================================================================
// Internal sound data & structures
// =======================================================================

// Base one-shot volume, also scaled for announcer-channel sounds. The 3D
// falloff / distance attenuation now lives in the engine (S_EngineConfigAttenuation).
#define		MASTER_VOL			127

channel_t   s_channels[MAX_CHANNELS];

static		qboolean	s_soundStarted;
static		qboolean	s_soundMuted;

dma_t		dma;

static int			listener_number;
static vec3_t		listener_origin;
static vec3_t		listener_axis[3];

int			s_soundtime;		// sample PAIRS
int   		s_paintedtime; 		// sample PAIRS

// Window-focus mute state, driven by the platform focus events (not polled).
// s_focusUnmuted is the mute decision the mixer reads: qtrue while the window
// has focus, qfalse while it is unfocused. It is set once per focus transition
// by S_FocusChanged, so the mixer reacts to the transition instead of re-deriving
// from a window flag every mix. s_focusMuteStart records the s_soundtime sample
// at which the most recent unfocus began, so the refocus flush can identify the
// one-shot channels that were started during the unfocused window.
//
// s_focusEventsSeen guards the fallback: a platform that delivers focus events
// (the SDL backend) drives s_focusUnmuted and gets the refocus flush, while a
// platform that does not (the native Win32 backend, whose wndproc still owns
// gw_active) leaves s_focusEventsSeen qfalse, and S_FocusUnmuted falls back to
// the polled gw_active so that backend's behaviour is unchanged.
static		qboolean	s_focusUnmuted = qtrue;	// qtrue = window focused
static		qboolean	s_focusEventsSeen = qfalse;	// a focus event ever delivered
static		int			s_focusMuteStart;		// s_soundtime when focus was lost

// MAX_SFX may be larger than MAX_SOUNDS because
// of custom player sounds
#define MAX_SFX			4096
static sfx_t s_knownSfx[MAX_SFX];
static int s_numSfx = 0;

#define LOOP_HASH		128
static sfx_t *sfxHash[LOOP_HASH];

cvar_t		*s_testsound;
cvar_t		*s_khz;
cvar_t		*s_show;
static cvar_t *s_mixahead;
static cvar_t *s_mixOffset;
static cvar_t *s_linearFalloff;
/* miniaudio backend cvars: registered for all platforms now that miniaudio
 * replaces the legacy ALSA/WASAPI/CoreAudio backends. The original Linux-only
 * ALSA s_device is superseded by the cross-platform one below. */
cvar_t		*s_device;
cvar_t		*s_latency;
cvar_t		*s_underruns;

static loopSound_t	loopSounds[MAX_GENTITIES];
static	channel_t	*freelist = NULL;


// ====================================================================
// User-setable variables
// ====================================================================


static void S_Base_SoundInfo( void ) {
	Com_Log( SEV_INFO, LOG_CH(ch_sound), "----- Sound Info -----\n" );
	if ( !s_soundStarted ) {
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "sound system not started\n" );
	} else {
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "%5d channels\n", dma.channels);
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "%5d samples\n", dma.samples);
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "%5d samplebits (%s)\n", dma.samplebits, dma.isfloat ? "float" : "int");
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "%5d submission_chunk\n", dma.submission_chunk);
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "%5d speed\n", dma.speed);
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "%p dma buffer\n", dma.buffer);
		if ( dma.driver ) {
			Com_Log( SEV_INFO, LOG_CH(ch_sound), "Using %s subsystem\n", dma.driver );
		}
		if ( s_backgroundStream ) {
			Com_Log( SEV_INFO, LOG_CH(ch_sound), "Background file: %s\n", s_backgroundLoop );
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_sound), "No background file.\n" );
		}

	}
	Com_Log( SEV_INFO, LOG_CH(ch_sound), "----------------------\n" );
}


/*
=================
S_Base_SoundList
=================
*/
static void S_Base_SoundList( void ) {
	/* Indexed by sfx->soundCompressionMethod; slots 2/3 are retired (were the
	 * removed wavelet/mu-law paths) but kept as placeholders to preserve the
	 * index alignment. Live methods: 0=16bit, 1=adpcm (FEAT-fenced). */
	static const char *type[4] = { "16bit", "adpcm", "unused", "unused" };
	static const char *mem[2] = { "paged out", "resident" };

	int		total = 0;
	const sfx_t *sfx = s_knownSfx;
	for (int i=0 ; i<s_numSfx ; i++, sfx++) {
		const int size = sfx->soundLength * sizeof(short);
		if ( sfx->inMemory ) {
			total += size;
		}
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "%7i[%s] : %s [%s]\n", size,
				type[sfx->soundCompressionMethod],
				sfx->soundName, mem[sfx->inMemory] );
	}
	Com_Log( SEV_INFO, LOG_CH(ch_client), "Total resident: %i\n", total);
	S_DisplayFreeMemory();
}


static void S_ChannelFree( channel_t *v ) {
	v->thesfx = NULL;
	*(channel_t **)v = freelist;
	freelist = (channel_t*)v;
}


static channel_t* S_ChannelMalloc( int allocTime ) {
	channel_t *v;
	if (freelist == NULL) {
		return NULL;
	}
	v = freelist;
	freelist = *(channel_t **)freelist;
	v->allocTime = allocTime;
	return v;
}


static void S_ChannelSetup( void ) {
	channel_t *p, *q;

	// clear all the sounds
	memset( s_channels, 0, sizeof( s_channels ) );

	p = s_channels;
	q = p + MAX_CHANNELS;
	while (--q > p) {
		*(channel_t **)q = q-1;
	}

	*(channel_t **)q = NULL;
	freelist = p + MAX_CHANNELS - 1;
}



// =======================================================================
// Load a sound
// =======================================================================

/*
================
return a hash value for the sfx name
================
*/
static unsigned int S_HashSFXName(const char *name) {
	unsigned int hash = 0;
	int		i = 0;
	while (name[i] != '\0') {
		char	letter = tolower(name[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(int)(letter)*(i+119);
		i++;
	}
	hash &= (LOOP_HASH-1);
	return hash;
}


/*
==================
S_FindName

Will allocate a new sfx if it isn't found
==================
*/
static sfx_t *S_FindName( const char *name ) {
	if ( !name ) {
		Com_Terminate( TERM_UNRECOVERABLE, "Sound name is NULL" );
	}

	if ( !name[0] ) {
		COM_WARN( LOG_CH(ch_client), "WARNING: Sound name is empty\n" );
		return NULL;
	}

	if ( strlen( name ) >= MAX_VFS_PATH ) {
		COM_WARN( LOG_CH(ch_client), "WARNING: Sound name is too long: %s\n", name );
		return NULL;
	}

	if ( name[0] == '*' ) {
		COM_WARN( LOG_CH(ch_client), "WARNING: Tried to load player sound directly: %s\n", name );
		return NULL;
	}

	int		hash = S_HashSFXName( name );

	sfx_t	*sfx = sfxHash[hash];
	// see if already loaded
	while (sfx) {
		if (!Q_stricmp(sfx->soundName, name) ) {
			return sfx;
		}
		sfx = sfx->next;
	}

	// find a free sfx
	int		i;
	for ( i=0 ; i < s_numSfx ; i++) {
		if (!s_knownSfx[i].soundName) {
			break;
		}
	}

	if (i == s_numSfx) {
		if (s_numSfx >= MAX_SFX) {
			Com_Terminate( TERM_UNRECOVERABLE, "S_FindName: out of sfx_t");
		}
		s_numSfx++;
	}

	sfx = &s_knownSfx[i];
	memset (sfx, 0, sizeof(*sfx));
	sfx->soundName = CopyString( name );

	sfx->next = sfxHash[hash];
	sfxHash[hash] = sfx;

	return sfx;
}


/*
===================
S_DisableSounds

Disables sounds until the next S_BeginRegistration.
This is called when the hunk is cleared and the sounds
are no longer valid.
===================
*/
static void S_Base_DisableSounds( void ) {
	S_Base_StopAllSounds();
	s_soundMuted = qtrue;
}


/*
==================
S_RegisterSound

Creates a default buzz sound if the file can't be loaded
==================
*/
static sfxHandle_t S_Base_RegisterSound( const char *name, qboolean compressed ) {
	sfx_t	*sfx;

	compressed = qfalse;
	if (!s_soundStarted) {
		return 0;
	}

	if ( strlen( name ) >= MAX_VFS_PATH ) {
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "Sound name exceeds MAX_VFS_PATH\n" );
		return 0;
	}

	sfx = S_FindName( name );
	if ( !sfx ) {
		return 0;
	}

	if ( sfx->soundData ) {
		if ( sfx->defaultSound ) {
			return 0;
		}
		return sfx - s_knownSfx;
	}

	sfx->inMemory = qfalse;
	sfx->soundCompressed = compressed;

	S_memoryLoad( sfx );

	if ( sfx->defaultSound ) {
		return 0;
	}

	return sfx - s_knownSfx;
}


#ifndef HEADLESS
/*
==================
S_EnginePlay_f

Dev console command:
  s_enginePlay <sfxname> [sfxname ...]      play unspatialized (listener-relative)
  s_enginePlay <sfxname> <x> <y> <z>        play positioned at a Q3 world point

Registers the named sound and plays it through ma_engine's graph as a real
ma_sound. The positional form spatializes the voice at the given world origin,
so a source placed to the listener's left (Q3 +Y) should pan left — the way to
check the ma_spatializer axis mapping (confirm with s_engineLevels: L > R). This
lives here — not in snd_main.c with the other command functions — because it
needs the private sfx_t behind a handle (s_knownSfx), which only this file can
reach.

Plays the named sound through ma_engine's graph; prints a hint if it does not
play (engine not up or the sound failed to load).
==================
*/
void S_EnginePlay_f( void ) {
	int c;
	int i;

	if ( !s_soundStarted ) {
		return;
	}

	c = Cmd_Argc();
	if ( c < 2 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_sound),
			"Usage: s_enginePlay <sfxname> [sfxname ...] | s_enginePlay <sfxname> <x> <y> <z>\n" );
		return;
	}

	// Positional form: exactly one name followed by three numeric coordinates.
	if ( c == 5 ) {
		vec3_t origin;
		sfxHandle_t h;
		origin[0] = atof( Cmd_Argv( 2 ) );
		origin[1] = atof( Cmd_Argv( 3 ) );
		origin[2] = atof( Cmd_Argv( 4 ) );
		h = S_Base_RegisterSound( Cmd_Argv( 1 ), qfalse );
		if ( h <= 0 || h >= s_numSfx ) {
			Com_Log( SEV_INFO, LOG_CH(ch_sound), "s_enginePlay: could not load \"%s\"\n",
				Cmd_Argv( 1 ) );
			return;
		}
		if ( S_EnginePlaySfxEx( &s_knownSfx[h], origin, qtrue ) ) {
			// Log the listener frame too so the pan can be reasoned about: the
			// source is to the listener's left when (origin-listener) . left > 0,
			// where left = listener_axis[1] (Q3 +Y is left).
			vec3_t rel;
			VectorSubtract( origin, listener_origin, rel );
			Com_Log( SEV_INFO, LOG_CH(ch_sound),
				"s_enginePlay: \"%s\" positioned at (%.0f %.0f %.0f); listener (%.0f %.0f %.0f) "
				"fwd (%.2f %.2f %.2f) left (%.2f %.2f %.2f); rel.left=%.1f rel.fwd=%.1f\n",
				Cmd_Argv( 1 ), origin[0], origin[1], origin[2],
				listener_origin[0], listener_origin[1], listener_origin[2],
				listener_axis[0][0], listener_axis[0][1], listener_axis[0][2],
				listener_axis[1][0], listener_axis[1][1], listener_axis[1][2],
				DotProduct( rel, listener_axis[1] ), DotProduct( rel, listener_axis[0] ) );
		} else {
			Com_Log( SEV_INFO, LOG_CH(ch_sound),
				"s_enginePlay: \"%s\" did not play\n", Cmd_Argv( 1 ) );
		}
		return;
	}

	for ( i = 1; i < c; i++ ) {
		sfxHandle_t h = S_Base_RegisterSound( Cmd_Argv( i ), qfalse );
		if ( h <= 0 || h >= s_numSfx ) {
			Com_Log( SEV_INFO, LOG_CH(ch_sound), "s_enginePlay: could not load \"%s\"\n",
				Cmd_Argv( i ) );
			continue;
		}
		if ( !S_EnginePlaySfx( &s_knownSfx[h] ) ) {
			Com_Log( SEV_INFO, LOG_CH(ch_sound),
				"s_enginePlay: \"%s\" did not play\n", Cmd_Argv( i ) );
		}
	}
}
#endif


/*
==================
S_Base_SoundDuration

Returns the cached duration of a sfx in milliseconds, or 0 if the
handle is out of range / not loaded. The duration field is populated in
S_LoadSound when the sound first hits memory.
==================
*/
static int S_Base_SoundDuration( sfxHandle_t handle ) {
	if ( handle < 0 || handle >= s_numSfx ) {
		return 0;
	}
	return s_knownSfx[ handle ].duration;
}


/*
=====================
S_BeginRegistration
=====================
*/
static void S_Base_BeginRegistration( void ) {
	s_soundMuted = qfalse;		// we can play again

	if ( s_numSfx )
		return;

	SND_setup();

	memset( s_knownSfx, 0, sizeof( s_knownSfx ) );
	memset( sfxHash, 0, sizeof( sfxHash ) );

	S_Base_RegisterSound( "sound/feedback/hit.opus", qfalse ); // changed to a sound in base
}


static void S_memoryLoad( sfx_t *sfx ) {

	// load the sound file
	if ( !S_LoadSound ( sfx ) ) {
		sfx->defaultSound = qtrue;
	}

	sfx->inMemory = qtrue;
}

//=============================================================================



// =======================================================================
// Start a sound effect
// =======================================================================

/*
====================
S_Base_StartSound

Validates the parms and ques the sound up
if origin is NULL, the sound will be dynamically sourced from the entity
Entchannel 0 will never override a playing sound
====================
*/
static void S_Base_StartSound( const vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle ) {
	channel_t	*ch;
	sfx_t		*sfx;
	int	inplay, allowed;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( !origin && ( entityNum < 0 || entityNum >= MAX_GENTITIES ) ) {
		Com_Terminate( TERM_CLIENT_DROP, "S_StartSound: bad entitynum %i", entityNum );
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		COM_WARN( LOG_CH(ch_client), "S_StartSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if ( sfx->inMemory == qfalse ) {
		S_memoryLoad(sfx);
	}

	if ( s_show->integer == 1 ) {
		Com_Log( SEV_INFO, LOG_CH(ch_sound), "%i : %s\n", s_paintedtime, sfx->soundName );
	}

	// a UNIQUE entity starting the same sound twice in a frame is either a bug,
	// a timedemo, or a shitmap (eg q3ctf4) giving multiple items on spawn.
	// even if you can create a case where it IS "valid", it's still pointless
	// because you implicitly can't DISTINGUISH between the sounds:
	// all that happens is the sound plays at double volume, which is just annoying

	int startTime = s_soundtime; // Com_Milliseconds();

	if ( entityNum != ENTITYNUM_WORLD ) {
		ch = s_channels;
		for ( int i = 0; i < MAX_CHANNELS; i++, ch++ ) {
			if ( ch->entnum != entityNum )
				continue;
			if ( ch->allocTime != startTime )
				continue;
			if ( ch->thesfx != sfx )
				continue;
			sfx->lastTimeUsed = startTime;
			//Com_Log( SEV_INFO, LOG_CH(ch_sound), S_COLOR_YELLOW "double sound start: %d %s\n", entityNum, sfx->soundName);
			return;
		}
	}

//	Com_Log( SEV_INFO, LOG_CH(ch_sound), "playing %s\n", sfx->soundName);
	// pick a channel to play on

	// try to limit sound duplication
	if ( entityNum == listener_number )
		allowed = 16;
	else
		allowed = 8;

	ch = s_channels;
	inplay = 0;
	for ( int i = 0; i < MAX_CHANNELS; i++, ch++ ) {
		if ( ch->entnum == entityNum && ch->thesfx == sfx ) {
			if ( startTime - ch->allocTime < 20 ) {
				Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "S_StartSound: Double start (%d ms < 20 ms) for %s\n", startTime - ch->allocTime, sfx->soundName);
				return;
			}
			inplay++;
		}
	}

	// too much duplicated sounds, ignore
	if ( inplay > allowed ) {
		Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "S_StartSound: %s hit the concurrent channels limit (%d)\n", sfx->soundName, allowed);
		return;
	}

	sfx->lastTimeUsed = startTime;

	ch = S_ChannelMalloc( startTime ); // entityNum, entchannel);
	if (!ch) {
		int i, oldest, chosen;
		ch = s_channels;

		oldest = sfx->lastTimeUsed;
		chosen = -1;
		for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
			if (ch->entnum != listener_number && ch->entnum == entityNum && ch->allocTime - oldest < 0 && ch->entchannel != CHAN_ANNOUNCER) {
				oldest = ch->allocTime;
				chosen = i;
			}
		}
		if (chosen == -1) {
			ch = s_channels;
			for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
				if (ch->entnum != listener_number && ch->allocTime - oldest < 0 && ch->entchannel != CHAN_ANNOUNCER) {
					oldest = ch->allocTime;
					chosen = i;
				}
			}
			if (chosen == -1) {
				ch = s_channels;
				if (ch->entnum == listener_number) {
					for ( i = 0 ; i < MAX_CHANNELS ; i++, ch++ ) {
						if ( ch->allocTime - oldest < 0 ) {
							oldest = ch->allocTime;
							chosen = i;
						}
					}
				}
				if (chosen == -1) {
					Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "S_StartSound: No more channels free for %s\n", sfx->soundName);
					return;
				}
			}
		}
		ch = &s_channels[chosen];
		ch->allocTime = sfx->lastTimeUsed;
		Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "S_StartSound: No more channels free for %s, dropping earliest sound: %s\n", sfx->soundName, ch->thesfx->soundName);
	}

	if ( origin ) {
		VectorCopy( origin, ch->origin );
		ch->fixed_origin = qtrue;
	} else {
		ch->fixed_origin = qfalse;
	}

	ch->master_vol = entchannel == CHAN_ANNOUNCER
		? (int)( MASTER_VOL * s_announcerVolume->value )
		: MASTER_VOL;
	ch->entnum = entityNum;
	ch->thesfx = sfx;
	ch->startSample = START_SAMPLE_IMMEDIATE;
	ch->entchannel = entchannel;
	ch->leftvol = ch->master_vol;		// these will get calced at next spatialize
	ch->rightvol = ch->master_vol;		// unless the game isn't running
	ch->doppler = qfalse;

#ifndef HEADLESS
	// ma_engine plays this one-shot with real 3D positioning. The channel bookkeeping
	// above still runs (it provides the per-entity dedup / concurrency limiting), but
	// the audible voice is the ma_sound created here. Sounds from the view entity
	// (own weapon/footsteps) and announcer voices stay listener-relative
	// (unspatialized, full volume); everything else is positioned at the entity's
	// origin (fixed origin if one was given, else the entity's tracked position).
	{
		qboolean spatialize = ( entityNum != listener_number ) && ( entchannel != CHAN_ANNOUNCER );
		if ( spatialize ) {
			vec3_t soundOrigin;
			if ( ch->fixed_origin ) {
				VectorCopy( ch->origin, soundOrigin );
			} else {
				VectorCopy( loopSounds[ entityNum ].origin, soundOrigin );
			}
			S_EnginePlaySfxEx( sfx, soundOrigin, qtrue );
		} else {
			S_EnginePlaySfxEx( sfx, NULL, qfalse );
		}
	}
#endif
}


/*
==================
S_StartLocalSound
==================
*/
static void S_Base_StartLocalSound( sfxHandle_t sfxHandle, int channelNum ) {
	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		COM_WARN( LOG_CH(ch_client), "S_StartLocalSound: handle %i out of range\n", sfxHandle );
		return;
	}

	S_Base_StartSound (NULL, listener_number, channelNum, sfxHandle );
}


/*
==================
S_ClearSoundBuffer

If we are about to perform file access, clear the buffer
so sound doesn't stutter.
==================
*/
static void S_Base_ClearSoundBuffer( void ) {
	if (!s_soundStarted)
		return;

	// stop looping sounds
	memset(loopSounds, 0, sizeof(loopSounds));

#ifndef HEADLESS
	// The per-frame loop reconcile (S_AddLoopSounds) is the only thing that stops
	// looping ma_sounds, and it is skipped while the sound system is muted/stopped
	// (S_Base_Respatialize early-returns). Clearing loopSounds[] here would
	// otherwise strand the live engine loop voices, which ma_engine keeps mixing
	// to the device — so drain the registry explicitly.
	S_EngineLoopStopAll();
#endif

	S_ChannelSetup();
}


/*
==================
S_StopAllSounds
==================
*/
static void S_Base_StopAllSounds( void ) {
	if ( !s_soundStarted ) {
		return;
	}

	// stop the background music
	S_Base_StopBackgroundTrack();

	S_Base_ClearSoundBuffer();
}


/*
==================
S_Base_FocusChanged

Drive the mute decision from a window-focus transition. On focus loss the mixer
mutes (it stops painting channels into the hardware buffer); on focus gain it
unmutes after flushing the one-shot channels that were started while unfocused,
so refocus does not burst a backlog of stale sounds. Looping/ambient sounds are
not touched here: loopSounds[] holds the per-entity loop state that the engine
loop registry reads each frame, so any still-active loop resumes on its own after unmute.

The flush targets only s_channels (fire-and-forget sounds) whose allocTime is at
or after the unfocus point (s_focusMuteStart). Sounds that were already playing
before the window lost focus keep their channel; only the during-unfocus backlog
is cleared. The subtraction-vs-zero comparison is wraparound-safe, matching the
allocTime ordering elsewhere in this file.
*/
void S_Base_FocusChanged( qboolean focused ) {
	s_focusEventsSeen = qtrue;

	if ( !s_soundStarted ) {
		s_focusUnmuted = focused ? qtrue : qfalse;
		return;
	}

	if ( !focused ) {
		// entering the unfocused window: remember the moment so the eventual
		// refocus can identify the sounds queued while we were away.
		if ( s_focusUnmuted ) {
			s_focusMuteStart = s_soundtime;
		}
		s_focusUnmuted = qfalse;
#ifndef HEADLESS
		S_UpdateAutoMute();		// apply the new focus state to the engine now
#endif
		return;
	}

	// regaining focus: flush the one-shots started during the unfocused window
	// before unmuting, so they don't suddenly play.
	if ( !s_focusUnmuted ) {
		channel_t *ch = s_channels;
		for ( int i = 0; i < MAX_CHANNELS; i++, ch++ ) {
			if ( ch->thesfx == NULL ) {
				continue;
			}
			// allocTime - s_focusMuteStart >= 0  ==  started at/after unfocus
			if ( ch->allocTime - s_focusMuteStart >= 0 ) {
				memset( ch, 0, sizeof( *ch ) );
			}
		}
	}
	s_focusUnmuted = qtrue;
#ifndef HEADLESS
	S_UpdateAutoMute();			// apply the new focus state to the engine now
#endif
}


/*
==================
S_FocusUnmuted

Read the focus-driven mute decision (qtrue while the window is focused). The
mixer consults this once per mix instead of re-deriving from a window flag.

When the platform never delivers focus events (the native Win32 backend keeps
gw_active in its wndproc and does not call S_FocusChanged), fall back to the
polled gw_active so that backend behaves exactly as before this change.
==================
*/
qboolean S_FocusUnmuted( void ) {
	if ( !s_focusEventsSeen ) {
		return gw_active;
	}
	return s_focusUnmuted;
}


/*
==================
S_ComputeAutoMute

Decide whether audio should be silenced because the window is unfocused or
minimized. Returns qtrue to mute. Reads the focus state through S_FocusUnmuted()
(the focus-event-driven decision, not a raw window poll) and the minimize flag.

s_autoMute is a bitmask that, when non-zero, overrides the legacy
s_muteWhenUnfocused / s_muteWhenMinimized toggles:
  bit 1 = mute while unfocused (but not while minimized)
  bit 2 = mute while minimized
With s_autoMute == 0 the legacy per-condition cvars apply instead. The match-alert
force-unmute (s_autoMute_OverrideMute) wins over everything.

Main-thread only; all inputs are event-driven flags + cvars.
==================
*/
qboolean S_ComputeAutoMute( void ) {
	qboolean unfocused = !S_FocusUnmuted();
	int am = s_autoMute ? s_autoMute->integer : 0;
	qboolean wantMute;

	if ( am != 0 ) {
		wantMute = qfalse;
		if ( (am & 1) && unfocused && !gw_minimized ) {
			wantMute = qtrue;
		}
		if ( (am & 2) && gw_minimized ) {
			wantMute = qtrue;
		}
	} else {
		wantMute = qfalse;
		if ( unfocused && !gw_minimized && s_muteWhenUnfocused->integer ) {
			wantMute = qtrue;
		}
		if ( gw_minimized && s_muteWhenMinimized->integer ) {
			wantMute = qtrue;
		}
	}

	if ( s_autoMute_OverrideMute ) {
		wantMute = qfalse;
	}

	return wantMute;
}


/*
==============================================================

continuous looping sounds are added each frame

==============================================================
*/

void S_Base_StopLoopingSound(int entityNum) {
	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Terminate( TERM_CLIENT_DROP, "S_StopLoopingSound: bad entitynum %i", entityNum );
	}
	loopSounds[entityNum].active = qfalse;
//	loopSounds[entityNum].sfx = 0;
	loopSounds[entityNum].kill = qfalse;
}


/*
==================
S_ClearLoopingSounds
==================
*/
void S_Base_ClearLoopingSounds( qboolean killall ) {
	for ( int i = 0 ; i < MAX_GENTITIES ; i++) {
		if (killall || loopSounds[i].kill == qtrue || (loopSounds[i].sfx && loopSounds[i].sfx->soundLength == 0)) {
			S_Base_StopLoopingSound(i);
		}
	}
}


/*
==================
S_AddLoopingSound

Called during entity generation for a frame
Include velocity in case I get around to doing doppler...
==================
*/
void S_Base_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle ) {
	sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Terminate( TERM_CLIENT_DROP, "S_AddLoopingSound: bad entitynum %i", entityNum );
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		COM_WARN( LOG_CH(ch_client), "S_AddLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if (sfx->inMemory == qfalse) {
		S_memoryLoad(sfx);
	}

	if ( !sfx->soundLength ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s has length 0", sfx->soundName );
	}

	VectorCopy( origin, loopSounds[entityNum].origin );
	VectorCopy( velocity, loopSounds[entityNum].velocity );
	loopSounds[entityNum].active = qtrue;
	loopSounds[entityNum].kill = qtrue;
	loopSounds[entityNum].doppler = qfalse;
	loopSounds[entityNum].oldDopplerScale = 1.0;
	loopSounds[entityNum].dopplerScale = 1.0;
	loopSounds[entityNum].sfx = sfx;

	if (s_doppler->integer && VectorLengthSquared(velocity)>0.0) {
		vec3_t	out;
		float	lena, lenb;

		loopSounds[entityNum].doppler = qtrue;
		lena = DistanceSquared(loopSounds[listener_number].origin, loopSounds[entityNum].origin);
		VectorAdd(loopSounds[entityNum].origin, loopSounds[entityNum].velocity, out);
		lenb = DistanceSquared(loopSounds[listener_number].origin, out);
		if ((loopSounds[entityNum].framenum+1) != cls.framecount) {
			loopSounds[entityNum].oldDopplerScale = 1.0;
		} else {
			loopSounds[entityNum].oldDopplerScale = loopSounds[entityNum].dopplerScale;
		}
		loopSounds[entityNum].dopplerScale = lenb/(lena*100);
		if (loopSounds[entityNum].dopplerScale<=1.0) {
			loopSounds[entityNum].doppler = qfalse;			// don't bother doing the math
		} else if (loopSounds[entityNum].dopplerScale>MAX_DOPPLER_SCALE) {
			loopSounds[entityNum].dopplerScale = MAX_DOPPLER_SCALE;
		}
	}

	loopSounds[entityNum].framenum = cls.framecount;
}


/*
==================
S_AddLoopingSound

Called during entity generation for a frame
Include velocity in case I get around to doing doppler...
==================
*/
void S_Base_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle ) {
	sfx_t *sfx;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Terminate( TERM_CLIENT_DROP, "S_AddRealLoopingSound: bad entitynum %i", entityNum );
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		COM_WARN( LOG_CH(ch_client), "S_AddRealLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx = &s_knownSfx[ sfxHandle ];

	if (sfx->inMemory == qfalse) {
		S_memoryLoad(sfx);
	}

	if ( !sfx->soundLength ) {
		Com_Terminate( TERM_CLIENT_DROP, "%s has length 0", sfx->soundName );
	}
	VectorCopy( origin, loopSounds[entityNum].origin );
	VectorCopy( velocity, loopSounds[entityNum].velocity );
	loopSounds[entityNum].sfx = sfx;
	loopSounds[entityNum].active = qtrue;
	loopSounds[entityNum].kill = qfalse;
	loopSounds[entityNum].doppler = qfalse;
}


/*
==================
S_AddLoopSounds

Spatialize all of the looping sounds.
All sounds are on the same cycle, so any duplicates can just
sum up the channel multipliers.
==================
*/
void S_AddLoopSounds( void ) {
	loopSound_t	*loop;
	int			startTime = s_soundtime; // Com_Milliseconds();

#ifndef HEADLESS
	// Reconcile the persistent per-(entityNum,sfx) looping ma_sound registry
	// against this frame's loopSounds[]. Each active loop is created once
	// (looping) and thereafter only repositioned — never restarted, which would
	// click. A voice not re-asserted this frame is uninited by the end-of-frame
	// sweep. ma_spatializer positions each source independently, so there is no
	// same-sfx merge: kill (S_AddLoopingSound, point source) uses the louder
	// MASTER_VOL curve; !kill (S_AddRealLoopingSound, sphere) uses the quieter
	// SPHERE_VOL base on the same falloff.

	// Reap finished one-shot voices once per frame (their only other reap is
	// lazily on the next one-shot play), so they do not linger in the mix graph.
	S_EngineReapVoices();
	S_EngineLoopBeginFrame();
	for ( int i = 0 ; i < MAX_GENTITIES ; i++ ) {
		loop = &loopSounds[i];
		if ( !loop->active || !loop->sfx || loop->sfx->soundLength == 0 ) {
			continue;
		}
		loop->sfx->lastTimeUsed = startTime;
		// loop->kill: point source (MASTER_VOL). !kill: sphere (SPHERE_VOL).
		// Pass the stored velocity so the engine can doppler-shift moving
		// point sources (sphere sources keep doppler off inside the upsert).
		S_EngineLoopUpsert( i, loop->sfx, loop->origin, loop->velocity, loop->kill ? qfalse : qtrue );
	}
	S_EngineLoopEndFrame();
#else
	(void)loop; (void)startTime;
#endif
}


/*
=====================
S_UpdateEntityPosition

let the sound system know where an entity currently is
======================
*/
void S_Base_UpdateEntityPosition( int entityNum, const vec3_t origin ) {
	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Terminate( TERM_CLIENT_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum );
	}
	VectorCopy( origin, loopSounds[entityNum].origin );
}


/*
============
S_Respatialize

Change the volumes of all the playing sounds for changes in their positions
============
*/
void S_Base_Respatialize( int entityNum, const vec3_t head, vec3_t axis[3], int inwater ) {
	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Terminate( TERM_CLIENT_DROP, "S_Respatialize: bad entitynum %i", entityNum );
	}

	listener_number = entityNum;
	VectorCopy(head, listener_origin);
	VectorCopy(axis[0], listener_axis[0]);
	VectorCopy(axis[1], listener_axis[1]);
	VectorCopy(axis[2], listener_axis[2]);

#ifndef HEADLESS
	// ma_engine's listener + per-voice ma_spatializer do the 3D positioning. Push
	// the listener frame (position from the view origin, forward from viewaxis[0],
	// up from viewaxis[2]); loop reconcile follows.
	S_EngineSetListener( listener_origin, listener_axis[0], listener_axis[2] );
	S_AddLoopSounds();
#endif
}


/*
========================
S_ScanChannelStarts

Returns qtrue if any new sounds were started since the last mix
========================
*/
static qboolean S_ScanChannelStarts( void ) {
	channel_t		*ch;
	qboolean		newSamples = qfalse;

	ch = s_channels;

	for ( int i = 0; i < MAX_CHANNELS; i++, ch++ ) {
		if ( !ch->thesfx ) {
			continue;
		}
		// if this channel was just started this frame,
		// set the sample count to it begins mixing
		// into the very first sample
		if ( ch->startSample == START_SAMPLE_IMMEDIATE ) {
			ch->startSample = s_paintedtime;
			newSamples = qtrue;
			continue;
		}

		// if it is completely finished by now, clear it
		if ( ch->startSample + (ch->thesfx->soundLength) - s_soundtime <= 0 ) {
			S_ChannelFree( ch );
		}
	}

	return newSamples;
}


/*
============
S_Update

Called once each time through the main loop
============
*/
static void S_Base_Update( int msec ) {
	if ( !s_soundStarted || s_soundMuted ) {
//		Com_Log( SEV_DEBUG, LOG_CH(ch_client), "not started or muted\n");
		return;
	}

	//
	// debugging output
	//
	if ( s_show->integer == 2 ) {
		int			total = 0;
		channel_t	*ch = s_channels;
		for (int i=0 ; i<MAX_CHANNELS; i++, ch++) {
			if (ch->thesfx && (ch->leftvol || ch->rightvol) ) {
				Com_Log( SEV_INFO, LOG_CH(ch_client), "%d %d %s\n", ch->leftvol, ch->rightvol, ch->thesfx->soundName);
				total++;
			}
		}

		Com_Log( SEV_INFO, LOG_CH(ch_client), "----(%i)---- painted: %i\n", total, s_paintedtime);
	}

	// mix some sound
	S_Update_( msec );
}


static void S_GetSoundtime( void )
{
	int		samplepos;
	static	int		buffers;
	static	int		oldsamplepos;

	if ( CL_VideoRecording() )
	{
		const float duration = MAX( (float)dma.speed / cl_aviFrameRate->value, 1.0f );
		const float frameDuration = duration + clientActiveApp->clc.aviSoundFrameRemainder;
		const int msec = (int)frameDuration;

		s_soundtime += msec;
		clientActiveApp->clc.aviSoundFrameRemainder = frameDuration - msec;

		// use same offset as in game
		s_paintedtime = s_soundtime + (int)(s_mixOffset->value * (float)dma.speed);

		// render exactly one frame of audio data
		clientActiveApp->clc.aviFrameEndTime = s_paintedtime + (int)(duration + clientActiveApp->clc.aviSoundFrameRemainder);
		return;
	}

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	samplepos = SNDDMA_GetDMAPos();
	if (samplepos < oldsamplepos)
	{
		buffers++;					// buffer wrapped

		if (s_paintedtime > 0x40000000)
		{	// time to chop things off to avoid 32 bit limits
			buffers = 0;
			s_paintedtime = dma.fullsamples;
			S_Base_StopAllSounds ();
		}
	}
	oldsamplepos = samplepos;

	s_soundtime = buffers * dma.fullsamples + samplepos/dma.channels;

	if ( dma.submission_chunk < 256 ) {
		s_paintedtime = s_soundtime + s_mixOffset->value * dma.speed;
	} else {
		s_paintedtime = s_soundtime + dma.submission_chunk;
	}
}


#ifndef HEADLESS
/*
======================
S_EngineFeedAviCapture

Source the AVI audio track from the engine while recording. Recording runs on a
synthetic per-video-frame clock (S_GetSoundtime's recording branch), so the
engine is in read-driven mode (S_EngineBeginCapture, entered at AVI open) and we
pull exactly the frame count that clock computed for this video frame:
aviFrameEndTime - s_paintedtime. That count varies +/-1 frame-to-frame by design
(the fractional-sample remainder carry) and must be read straight off the clock,
never recomputed. The pulled s16 stereo PCM goes to CL_WriteAVIAudioFrame.

Main-thread only; inert when not recording. Called from S_Update_ after the
clock is set.
======================
*/
static void S_EngineFeedAviCapture( void ) {
	int count;
	int got;
	// s16 stereo scratch for one video frame of audio. At 48 kHz / min 1 fps that
	// is at most 48000 frames; size for a generous ceiling and clamp.
	static short aviPcm[48000 * AVI_CAPTURE_CHANNELS];
	int maxFrames = (int)( sizeof( aviPcm ) / ( sizeof( short ) * AVI_CAPTURE_CHANNELS ) );

	if ( !CL_VideoRecording() ) {
		return;
	}

	count = clientActiveApp->clc.aviFrameEndTime - s_paintedtime;
	if ( count <= 0 ) {
		return;
	}
	if ( count > maxFrames ) {
		count = maxFrames;
	}

	got = S_EngineReadCaptureFrames( aviPcm, count );
	if ( got > 0 ) {
		// The AVI track is stereo regardless of the live device channel count;
		// S_EngineReadCaptureFrames wrote AVI_CAPTURE_CHANNELS-interleaved s16.
		CL_WriteAVIAudioFrame( (const byte *)aviPcm, got * AVI_CAPTURE_CHANNELS * dma.samplebits / 8 );
	}
}
#endif


static void S_Update_( int msec ) {
	int				thisTime;
	static int		ot = -1;
	static int		lastTime = 0;

	(void)msec;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	thisTime = Com_Milliseconds();

	// Updates s_soundtime
	S_GetSoundtime();

#ifndef HEADLESS
	// Apply the window focus/minimize auto-mute each frame (before the clock-stall
	// early-out below), so minimize/restore and cvar/override changes take effect
	// even though SDL raises no focus event on minimize. Sets the engine master
	// volume only on a state change.
	S_UpdateAutoMute();
#endif

	if ( s_soundtime == ot ) {
		return;
	}

	ot = s_soundtime;

	// clear any sound effects that end before the current time,
	// and start any new sounds
	S_ScanChannelStarts();

#ifndef HEADLESS
	// While recording, source the AVI audio track from the engine deterministically
	// on this synthetic per-video-frame clock: pull exactly the frame count
	// S_GetSoundtime computed for this video frame (aviFrameEndTime - s_paintedtime)
	// and hand it to the AVI writer. S_EngineFeedAviCapture is inert when not
	// recording or when the engine is not in offline-capture mode.
	S_EngineFeedAviCapture();
#endif

	// keep the background-music ring fed (ma_engine drains it at device rate)
	S_UpdateBackgroundTrack();

	lastTime = thisTime;
}


/*
===============================================================================

background music functions

===============================================================================
*/

/*
======================
S_StopBackgroundTrack
======================
*/
static void S_Base_StopBackgroundTrack( void ) {
#ifndef HEADLESS
	// Tear down the engine music voice (ma_sound + ring); a
	// no-op when it was never started.
	S_EngineMusicStop();
#endif
	if(!s_backgroundStream)
		return;
	S_CodecCloseStream(s_backgroundStream);
	s_backgroundStream = NULL;
}


/*
======================
S_OpenBackgroundStream
======================
*/
static void S_OpenBackgroundStream( const char *filename ) {
	char cleanPath[MAX_QPATH];
	char stemPath[MAX_QPATH];
	{
		char *p;
		Q_strncpyz( cleanPath, filename, sizeof( cleanPath ) );
		for ( p = cleanPath; *p; p++ ) { if ( *p == '\\' ) *p = '/'; }
	}
	COM_StripExtension( cleanPath, stemPath, sizeof( stemPath ) );

	// close the background track
	// if restarting the same background track
	if( s_backgroundStream )
	{
		S_CodecCloseStream( s_backgroundStream );
		s_backgroundStream = NULL;
	}

	// Open stream
	s_backgroundStream = S_CodecOpenStream( cleanPath );
	if( !s_backgroundStream ) {
		AssetLog_Event( "music", stemPath, "wav,opus,ogg", NULL, ASSET_LOG_WARN );
		return;
	}

	if( s_backgroundStream->info.channels != 2 || s_backgroundStream->info.rate != 48000 ) {
		Com_Log( SEV_WARN, LOG_CH(ch_sound), "WARNING: music file %s is not 48kHz stereo\n", filename );
	}
}


/*
======================
S_StartBackgroundTrack
======================
*/
static void S_Base_StartBackgroundTrack( const char *intro, const char *loop ){
	if ( !intro ) {
		intro = "";
	}
	if ( !loop || !loop[0] ) {
		loop = intro;
	}
	Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "S_StartBackgroundTrack( %s, %s )\n", intro, loop );

	if(!*intro)
	{
		S_Base_StopBackgroundTrack();
		return;
	}

#ifndef HEADLESS
	// Starting a new track: drop any engine music voice so its ring does not play
	// a stale tail of the previous track. The feeder re-creates it next update at
	// the new stream's format. (A loop reopen goes through S_OpenBackgroundStream
	// directly and keeps the voice for a seamless loop.)
	S_EngineMusicStop();
#endif

	Q_strncpyz( s_backgroundLoop, loop, sizeof( s_backgroundLoop ) );

	S_OpenBackgroundStream( intro );
}


#ifndef HEADLESS
/*
======================
S_EngineFeedBackgroundTrack

Music feeder: decode the background stream and push it into ma_engine's
streaming ring, which its audio thread drains at device rate.

The engine music voice is created lazily here at the stream's format, and
re-created if a loop reopen changes the channel count / rate. Fill is paced by
the ring's free space (S_EngineMusicWritableFrames).
Runs on the MAIN THREAD; only decode + ring writes happen here.
======================
*/
static void S_EngineFeedBackgroundTrack( void ) {
	byte	raw[30000];		// scratch for one decode chunk
	short	conv[15000];	// s16 expansion of 8-bit source (<= raw/2 samples * 1)

	// Ensure a music voice exists at the current stream's format; (re)open it on
	// first use and whenever a loop reopen changed channels/rate.
	{
		int haveCh = 0, haveRate = 0;
		qboolean live = S_EngineMusicActive( &haveCh, &haveRate );
		if ( !live || haveCh != s_backgroundStream->info.channels ||
		     haveRate != s_backgroundStream->info.rate ) {
			if ( !S_EngineMusicStart( s_backgroundStream->info.channels,
			                          s_backgroundStream->info.rate, s_musicVolume->value ) ) {
				return;	// engine music unavailable — nothing to feed
			}
		} else {
			// keep the live voice's volume in step with s_musicVolume
			S_EngineMusicSetVolume( s_musicVolume->value );
		}
	}

	// Fill the ring while it has room. ma_engine resamples the stream rate to the
	// device rate and applies the music volume, so we push samples at the SOURCE
	// rate with no per-sample scaling.
	while ( S_EngineMusicWritableFrames() > 0 ) {
		int width    = s_backgroundStream->info.width;
		int channels = s_backgroundStream->info.channels;
		int frameSize = width * channels;			// bytes per source frame
		int roomFrames = S_EngineMusicWritableFrames();
		int fileFrames = (int)( sizeof(raw) / frameSize );
		int r;

		if ( roomFrames < fileFrames )
			fileFrames = roomFrames;
		if ( fileFrames <= 0 )
			return;

		r = S_CodecReadStream( s_backgroundStream, fileFrames * frameSize, raw );

		if ( r > 0 ) {
			int gotFrames = r / frameSize;
			const short *pcm;

			if ( width == 2 ) {
				// already s16 interleaved — push directly
				pcm = (const short *)raw;
			} else {
				// 8-bit source: expand each byte to s16 (unsigned -> signed)
				int n = gotFrames * channels;
				for ( int i = 0; i < n && i < (int)( sizeof(conv)/sizeof(conv[0]) ); i++ ) {
					conv[i] = (short)( ( (int)( (byte)raw[i] ) - 128 ) << 8 );
				}
				pcm = conv;
			}

			S_EngineMusicWrite( pcm, gotFrames );
		} else {
			// end of stream — loop the loop file, else stop
			if ( s_backgroundLoop[0] != '\0' ) {
				S_OpenBackgroundStream( s_backgroundLoop );
				if ( !s_backgroundStream )
					return;
			} else {
				S_Base_StopBackgroundTrack();
				return;
			}
		}
	}
}
#endif

/*
======================
S_UpdateBackgroundTrack
======================
*/
static void S_UpdateBackgroundTrack( void ) {
	if ( !s_backgroundStream ) {
		return;
	}

	// don't bother playing anything if musicvolume is 0
	if ( s_musicVolume->value == 0.0f ) {
		return;
	}

#ifndef HEADLESS
	// Feed ma_engine's streaming ring: decode the stream and push it into the
	// engine's music voice, which its audio thread drains at device rate.
	S_EngineFeedBackgroundTrack();
#endif
}


/*
======================
S_FreeOldestSound
======================
*/
void S_FreeOldestSound( void ) {
	// all sounds may be loaded with (s_soundtime + 1) at this moment
	// so we need to trigger match condition at least once
	int	oldest = s_soundtime + 2; // Com_Milliseconds();
	int	used = 0;

	for ( int i = 1 ; i < s_numSfx ; i++ ) {
		sfx_t	*sfx = &s_knownSfx[i];
		if ( sfx->inMemory && sfx->lastTimeUsed - oldest < 0 ) {
			used = i;
			oldest = sfx->lastTimeUsed;
		}
	}

	sfx_t	*sfx = &s_knownSfx[used];

	Com_Log( SEV_DEBUG, LOG_CH(ch_sound), "S_FreeOldestSound: freeing sound %s\n", sfx->soundName);

	sndBuffer	*buffer = sfx->soundData;
	while(buffer != NULL) {
		sndBuffer	*nbuffer = buffer->next;
		SND_free(buffer);
		buffer = nbuffer;
	}
	sfx->inMemory = qfalse;
	sfx->soundData = NULL;

	// Deliberately keep sfx->enginePcm (the flat decode used by the engine path).
	// It is an independent copy of the samples, so freeing soundData does not
	// touch it, and a live engine voice may still be reading it — freeing it here
	// could use-after-free. It is content-identical to a fresh decode, so the
	// retained buffer stays valid and is released only at S_Base_Shutdown.
}


// =======================================================================
// Shutdown sound engine
// =======================================================================

static void S_Base_Shutdown( void ) {

	if ( !s_soundStarted ) {
		return;
	}

	SNDDMA_Shutdown();

	S_OpusDecoderShutdown();

	// release sound buffers only when a server is running (no local audio
	// playback) to avoid redundant reallocation at client restart
	if ( com_sv_running->integer )
		SND_shutdown();

	s_soundStarted = qfalse;

	// Free heap-owned soundName strings before resetting the pool. Also release
	// any cached flat engine-PCM buffer (materialized for the ma_engine feed):
	// SNDDMA_Shutdown above has already stopped every voice, so no ma_sound
	// still points into these buffers.
	for ( int i = 0; i < s_numSfx; i++ ) {
#ifndef HEADLESS
		S_EngineFreeSfxPcm( &s_knownSfx[i] );
#endif
		if ( s_knownSfx[i].soundName ) {
			Z_Free( (void *)s_knownSfx[i].soundName );
			s_knownSfx[i].soundName = NULL;
		}
	}
	s_numSfx = 0; // clean up sound cache -EC-

	Cmd_RemoveCommand( "s_info" );

	cls.soundRegistered = qfalse;
}


static const cvarDesc_t sndDmaDescs[] = {
	/* 0 */ CVAR_INT(    "s_khz",          "48",  CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,     "Specifies the sound sampling rate, (8, 11, 22, 44, 48) in kHz. Default value is 48.", 0, 48 ),
	/* 1 */ CVAR_FLOAT(  "s_mixAhead",     "0.2", CVAR_ARCHIVE | CVAR_NODEFAULT,                  "Amount of time to pre-mix sound data to avoid potential skips/stuttering in case of unstable framerate. Higher values add more CPU usage.", 0.001f, 0.5f ),
	/* 2 */ CVAR_FLOAT(  "s_mixOffset",    "0",   CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_CHEAT, NULL, 0, 0.5f ),
	/* 3 */ CVAR_BOOL(   "s_linearFalloff","1",   CVAR_ARCHIVE | CVAR_NODEFAULT,
		"Distance-based sound attenuation model.\n"
		" 0: classic Q3 falloff (full volume within SOUND_FULLVOLUME, then exponential decay)\n"
		" 1: linear falloff from listener to SOUND_MAX_DIST (default)" ),
	/* 4 */ CVAR_BOOL(   "s_show",         "0",   CVAR_CHEAT,                       "Debugging output (used sound files)." ),
	/* 5 */ CVAR_BOOL(   "s_testsound",    "0",   CVAR_CHEAT,                       "Debugging tool that plays a simple sine wave tone to test the sound system." ),
	/* 6 */ CVAR_STRING( "s_device",       "",    CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,
		"Audio output device name (miniaudio backend).\n"
		" Empty string = system default device.\n"
		" To pick a specific device, set this to its name as reported\n"
		" by your OS (use the OS sound panel to look up the exact name).\n"
		" Requires snd_restart to take effect." ),
	/* 7 */ CVAR_INT(    "s_latency",      "6",   CVAR_ARCHIVE | CVAR_NODEFAULT | CVAR_LATCH,
		"Audio output latency hint in milliseconds (clamped 2-20).\n"
		" Lower = less delay but higher CPU and risk of underruns.\n"
		" Higher = safer but more audible delay.\n"
		" Requires snd_restart to take effect.", 2, 20 ),
	/* 8 */ CVAR_INT(    "s_underruns",    "0",   CVAR_TEMP,
		"Read-only counter: number of audio underruns since startup.\n"
		" Updated by the miniaudio backend each frame.\n"
		" Non-zero values indicate the audio thread is starved.", 0, 0 ),
};

enum {
	SNDDMA_KHZ, SNDDMA_MIXAHEAD, SNDDMA_MIXOFFSET, SNDDMA_LINEARFALLOFF,
	SNDDMA_SHOW, SNDDMA_TESTSOUND, SNDDMA_DEVICE, SNDDMA_LATENCY, SNDDMA_UNDERRUNS,
	SNDDMA_CVAR_COUNT
};

_Static_assert( ARRAY_LEN( sndDmaDescs ) == SNDDMA_CVAR_COUNT, "sndDmaDescs/enum mismatch" );
static cvar_t *sndDmaHandles[SNDDMA_CVAR_COUNT];


/*
================
S_Init
================
*/
qboolean S_Base_Init( soundInterface_t *si ) {
	qboolean	r;

	if ( !si ) {
		return qfalse;
	}

	Cvar_RegisterTable( sndDmaDescs, ARRAY_LEN( sndDmaDescs ), sndDmaHandles );
	s_khz          = sndDmaHandles[SNDDMA_KHZ];
	s_mixahead     = sndDmaHandles[SNDDMA_MIXAHEAD];
	s_mixOffset    = sndDmaHandles[SNDDMA_MIXOFFSET];
	s_linearFalloff = sndDmaHandles[SNDDMA_LINEARFALLOFF];
	s_show         = sndDmaHandles[SNDDMA_SHOW];
	s_testsound    = sndDmaHandles[SNDDMA_TESTSOUND];
	s_device       = sndDmaHandles[SNDDMA_DEVICE];
	s_latency      = sndDmaHandles[SNDDMA_LATENCY];
	s_underruns    = sndDmaHandles[SNDDMA_UNDERRUNS];

	switch( s_khz->integer ) {
		case 48:
		case 44:
		case 22:
		case 11:
		case 8:
			// these are legal values
			break;
		default:
			// anything else is illegal
			Com_Log( SEV_INFO, LOG_CH(ch_sound), "WARNING: cvar 's_khz' must be one of (8, 11, 22, 44, 48), setting to '%s'\n", s_khz->resetString );
			Cvar_ForceReset( "s_khz" );
			break;
	}

	r = SNDDMA_Init();

	if ( r ) {
		s_soundStarted = qtrue;
		s_soundMuted = qtrue;
//		s_numSfx = 0;

		memset( sfxHash, 0, sizeof( sfxHash ) );

		s_soundtime = 0;
		s_paintedtime = 0;

		S_Base_StopAllSounds();

		S_OpusDecoderInit();
	} else {
		return qfalse;
	}

	si->Shutdown = S_Base_Shutdown;
	si->StartSound = S_Base_StartSound;
	si->StartLocalSound = S_Base_StartLocalSound;
	si->StartBackgroundTrack = S_Base_StartBackgroundTrack;
	si->StopBackgroundTrack = S_Base_StopBackgroundTrack;
	si->StopAllSounds = S_Base_StopAllSounds;
	si->ClearLoopingSounds = S_Base_ClearLoopingSounds;
	si->AddLoopingSound = S_Base_AddLoopingSound;
	si->AddRealLoopingSound = S_Base_AddRealLoopingSound;
	si->StopLoopingSound = S_Base_StopLoopingSound;
	si->Respatialize = S_Base_Respatialize;
	si->UpdateEntityPosition = S_Base_UpdateEntityPosition;
	si->Update = S_Base_Update;
	si->DisableSounds = S_Base_DisableSounds;
	si->BeginRegistration = S_Base_BeginRegistration;
	si->RegisterSound = S_Base_RegisterSound;
	si->SoundDuration = S_Base_SoundDuration;
	si->ClearSoundBuffer = S_Base_ClearSoundBuffer;
	si->SoundInfo = S_Base_SoundInfo;
	si->SoundList = S_Base_SoundList;

	return qtrue;
}
