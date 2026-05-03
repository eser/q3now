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
#include "snd_local.h"
#include "snd_public.h"
#include "../qcommon/arena.h"

/* ---- Audio_Arena (Step 3.4) -----------------------------------------------
   The audio subsystem uses static globals and backend-internal allocations,
   not hunk memory.  Audio_Arena is a lifecycle-tracking arena: it is created
   at S_Init time and destroyed at S_Shutdown (process exit only), making the
   lifetime boundary explicit and visible in /meminfo.
   --------------------------------------------------------------------------*/
static arena_t *s_audioArena = NULL;
#define AUDIO_ARENA_SIZE 4096  /* header only — no subsystem data in bump pool */

cvar_t *s_volume;
cvar_t *s_musicVolume;
cvar_t *s_announcerVolume;
cvar_t *s_doppler;
cvar_t *s_muteWhenMinimized;
cvar_t *s_muteWhenUnfocused;

/* CNQ3 backport: s_autoMute bitmask */
cvar_t *s_autoMute;
qboolean s_autoMute_OverrideMute = qfalse;

static soundInterface_t si;

/*
=================
S_ValidateInterface
=================
*/
static qboolean S_ValidSoundInterface( const soundInterface_t *s )
{
	if( !s->Shutdown ) return qfalse;
	if( !s->StartSound ) return qfalse;
	if( !s->StartLocalSound ) return qfalse;
	if( !s->StartBackgroundTrack ) return qfalse;
	if( !s->StopBackgroundTrack ) return qfalse;
	if( !s->RawSamples ) return qfalse;
	if( !s->StopAllSounds ) return qfalse;
	if( !s->ClearLoopingSounds ) return qfalse;
	if( !s->AddLoopingSound ) return qfalse;
	if( !s->AddRealLoopingSound ) return qfalse;
	if( !s->StopLoopingSound ) return qfalse;
	if( !s->Respatialize ) return qfalse;
	if( !s->UpdateEntityPosition ) return qfalse;
	if( !s->Update ) return qfalse;
	if( !s->DisableSounds ) return qfalse;
	if( !s->BeginRegistration ) return qfalse;
	if( !s->RegisterSound ) return qfalse;
	if( !s->SoundDuration ) return qfalse;
	if( !s->ClearSoundBuffer ) return qfalse;
	if( !s->SoundInfo ) return qfalse;
	if( !s->SoundList ) return qfalse;

	return qtrue;
}


/*
=================
S_StartSound
=================
*/
void S_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx )
{
	if( si.StartSound ) {
		si.StartSound( origin, entnum, entchannel, sfx );
	}
}


/*
=================
S_StartLocalSound
=================
*/
void S_StartLocalSound( sfxHandle_t sfx, int channelNum )
{
	if( si.StartLocalSound ) {
		si.StartLocalSound( sfx, channelNum );
	}
}


/*
=================
S_StartBackgroundTrack
=================
*/
void S_StartBackgroundTrack( const char *intro, const char *loop )
{
	if( si.StartBackgroundTrack ) {
		si.StartBackgroundTrack( intro, loop );
	}
}


/*
=================
S_StopBackgroundTrack
=================
*/
void S_StopBackgroundTrack( void )
{
	if( si.StopBackgroundTrack ) {
		si.StopBackgroundTrack( );
	}
}


/*
=================
S_RawSamples
=================
*/
void S_RawSamples (int samples, int rate, int width, int channels,
		   const byte *data, float volume)
{
	if( si.RawSamples ) {
		si.RawSamples( samples, rate, width, channels, data, volume );
	}
}


/*
=================
S_StopAllSounds
=================
*/
void S_StopAllSounds( void )
{
	if( si.StopAllSounds ) {
		si.StopAllSounds();
	}
}


/*
=================
S_ClearLoopingSounds
=================
*/
void S_ClearLoopingSounds( qboolean killall )
{
	if( si.ClearLoopingSounds ) {
		si.ClearLoopingSounds( killall );
	}
}


/*
=================
S_AddLoopingSound
=================
*/
void S_AddLoopingSound( int entityNum, const vec3_t origin,
		const vec3_t velocity, sfxHandle_t sfx )
{
	if( si.AddLoopingSound ) {
		si.AddLoopingSound( entityNum, origin, velocity, sfx );
	}
}


/*
=================
S_AddRealLoopingSound
=================
*/
void S_AddRealLoopingSound( int entityNum, const vec3_t origin,
		const vec3_t velocity, sfxHandle_t sfx )
{
	if( si.AddRealLoopingSound ) {
		si.AddRealLoopingSound( entityNum, origin, velocity, sfx );
	}
}


/*
=================
S_StopLoopingSound
=================
*/
void S_StopLoopingSound( int entityNum )
{
	if( si.StopLoopingSound ) {
		si.StopLoopingSound( entityNum );
	}
}


/*
=================
S_Respatialize
=================
*/
void S_Respatialize( int entityNum, const vec3_t origin,
		vec3_t axis[3], int inwater )
{
	if( si.Respatialize ) {
		si.Respatialize( entityNum, origin, axis, inwater );
	}
}


/*
=================
S_UpdateEntityPosition
=================
*/
void S_UpdateEntityPosition( int entityNum, const vec3_t origin )
{
	if( si.UpdateEntityPosition ) {
		si.UpdateEntityPosition( entityNum, origin );
	}
}


/*
=================
S_Update
=================
*/
void S_Update( int msec )
{
	if ( si.Update ) {
		si.Update( msec );
	}
}


/*
=================
S_SetMuteOverride

Temporarily override the auto-mute decision (used by the match-alert
bit 8 to make the alert audible while the window is unfocused).  When
called with qfalse the override is cleared and normal s_autoMute rules
apply again.
=================
*/
void S_SetMuteOverride( qboolean enabled )
{
	s_autoMute_OverrideMute = enabled ? qtrue : qfalse;
}


/*
=================
S_DisableSounds
=================
*/
void S_DisableSounds( void )
{
	if( si.DisableSounds ) {
		si.DisableSounds();
	}
}


/*
=================
S_BeginRegistration
=================
*/
void S_BeginRegistration( void )
{
	if ( si.BeginRegistration ) {
		si.BeginRegistration();
	}
}


/*
=================
S_RegisterSound
=================
*/
sfxHandle_t	S_RegisterSound( const char *sample, qboolean compressed )
{
	if ( !sample || !*sample ) {
		Com_Log( SEV_INFO, LOG_CAT_SOUND, "NULL sound\n" );
		return 0;
	}

	if( si.RegisterSound ) {
		return si.RegisterSound( sample, compressed );
	} else {
		return 0;
	}
}


/*
=================
S_SoundDuration

Phase 6.2: returns the duration of a registered sound in milliseconds.
Returns 0 if the handle is invalid or the sound system is not running.
=================
*/
int S_SoundDuration( sfxHandle_t handle )
{
	if ( si.SoundDuration ) {
		return si.SoundDuration( handle );
	}
	return 0;
}


/*
=================
S_ClearSoundBuffer
=================
*/
void S_ClearSoundBuffer( void )
{
	if( si.ClearSoundBuffer ) {
		si.ClearSoundBuffer();
	}
}


/*
=================
S_SoundInfo
=================
*/
static void S_SoundInfo( void )
{
	if( si.SoundInfo ) {
		si.SoundInfo();
	}
}


/*
=================
S_SoundList
=================
*/
static void S_SoundList( void )
{
	if( si.SoundList ) {
		si.SoundList();
	}
}

//=============================================================================

/*
=================
S_Play_f
=================
*/
static void S_Play_f( void ) {
	if( !si.RegisterSound || !si.StartLocalSound ) {
		return;
	}

	int c = Cmd_Argc();

	if( c < 2 ) {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Usage: play <sound filename> [sound filename] [sound filename] ...\n");
		return;
	}

	for( int i = 1; i < c; i++ ) {
		sfxHandle_t h = si.RegisterSound( Cmd_Argv(i), qfalse );

		if( h ) {
			si.StartLocalSound( h, CHAN_LOCAL_SOUND );
		}
	}
}


/*
=================
S_Music_f
=================
*/
static void S_Music_f( void ) {
	int		c;

	if( !si.StartBackgroundTrack ) {
		return;
	}

	c = Cmd_Argc();

	if ( c == 2 ) {
		si.StartBackgroundTrack( Cmd_Argv(1), NULL );
	} else if ( c == 3 ) {
		si.StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(2) );
	} else {
		Com_Log( SEV_INFO, LOG_CAT_CLIENT, "Usage: music <musicfile> [loopfile]\n");
		return;
	}

}


/*
=================
S_StopMusic_f
=================
*/
static void S_StopMusic_f( void )
{
	if ( !si.StopBackgroundTrack )
		return;

	si.StopBackgroundTrack();
}


//=============================================================================

static const cvarDesc_t sndMainDescs[] = {
	/* 0 */ CVAR_FLOAT( "s_volume",           "0.8",  CVAR_ARCHIVE,    "Sets master volume for all game audio.", 0, 1 ),
	/* 1 */ CVAR_FLOAT( "s_musicVolume",      "0.25", CVAR_ARCHIVE,    "Sets volume for in-game music only.", 0, 1 ),
	/* 2 */ CVAR_FLOAT( "s_announcerVolume",  "1.0",  CVAR_ARCHIVE,    "Volume multiplier for announcer sounds (0=silent, 1=normal, 2=double).", 0, 2 ),
	/* 3 */ CVAR_BOOL(  "s_doppler",          "1",    CVAR_ARCHIVE | CVAR_NODEFAULT, "Enables doppler effect on moving projectiles." ),
	/* 4 */ CVAR_BOOL(  "s_muteWhenUnfocused","1",    CVAR_ARCHIVE,    "Mutes all audio while game window is unfocused." ),
	/* 5 */ CVAR_BOOL(  "s_muteWhenMinimized","1",    CVAR_ARCHIVE,    "Mutes all audio while game is minimized." ),
	/* 6 */ CVAR_INT(   "s_autoMute",         "1",    CVAR_ARCHIVE,
		"Auto-mute bitmask (overrides s_muteWhenUnfocused/s_muteWhenMinimized):\n"
		"  0 = never mute\n"
		"  1 = mute when window is unfocused\n"
		"  2 = mute when window is minimized\n"
		"  3 = mute in both cases", 0, 3 ),
};

enum {
	SND_VOLUME, SND_MUSICVOLUME, SND_ANNOUNCERVOLUME,
	SND_DOPPLER, SND_MUTEWHENUNFOCUSED, SND_MUTEWHENMINIMIZED,
	SND_AUTOMUTE,
	SND_CVAR_COUNT
};

_Static_assert( ARRAY_LEN( sndMainDescs ) == SND_CVAR_COUNT, "sndMainDescs/enum mismatch" );
static cvar_t *sndMainHandles[SND_CVAR_COUNT];


/*
=================
S_Init
=================
*/
void S_Init( void )
{
	qboolean	started = qfalse;

	if ( !s_audioArena ) {
		s_audioArena = Arena_Create( "Audio", AUDIO_ARENA_SIZE );
	}

	Com_Log( SEV_INFO, LOG_CAT_SOUND, "------ Initializing Sound ------\n" );

	Cvar_RegisterTable( sndMainDescs, ARRAY_LEN( sndMainDescs ), sndMainHandles );
	s_volume            = sndMainHandles[SND_VOLUME];
	s_musicVolume       = sndMainHandles[SND_MUSICVOLUME];
	s_announcerVolume   = sndMainHandles[SND_ANNOUNCERVOLUME];
	s_doppler           = sndMainHandles[SND_DOPPLER];
	s_muteWhenUnfocused = sndMainHandles[SND_MUTEWHENUNFOCUSED];
	s_muteWhenMinimized = sndMainHandles[SND_MUTEWHENMINIMIZED];
	s_autoMute          = sndMainHandles[SND_AUTOMUTE];

	cvar_t *cv;
	{
		static const cvarDesc_t d = CVAR_BOOL( "s_initsound", "1", 0,
			"Whether or not to startup the sound system." );
		cv = Cvar_Register( &d );
	}
	if ( !cv->integer ) {
		Com_Log( SEV_INFO, LOG_CAT_SOUND, "Sound disabled.\n" );
	} else {

		S_CodecInit();

		Cmd_AddCommand( "play", S_Play_f );
		Cmd_AddCommand( "music", S_Music_f );
		Cmd_AddCommand( "stopmusic", S_StopMusic_f );
		Cmd_AddCommand( "s_list", S_SoundList );
		Cmd_AddCommand( "s_stop", S_StopAllSounds );
		Cmd_AddCommand( "s_info", S_SoundInfo );

		if ( !started ) {
			started = S_Base_Init( &si );
		}

		if ( started ) {
			if( !S_ValidSoundInterface( &si ) ) {
				Com_Terminate( TERM_UNRECOVERABLE, "Sound interface invalid" );
			}

			S_SoundInfo();
			Com_Log( SEV_INFO, LOG_CAT_SOUND, "Sound initialization successful.\n" );
		} else {
			Com_Log( SEV_INFO, LOG_CAT_SOUND, "Sound initialization failed.\n" );
		}
	}

	Com_Log( SEV_INFO, LOG_CAT_SOUND, "--------------------------------\n");
}


/*
=================
S_Shutdown
=================
*/
void S_Shutdown( void )
{
	if ( si.StopAllSounds ) {
		si.StopAllSounds();
	}

	if ( si.Shutdown ) {
		si.Shutdown();
	}

	memset( &si, 0, sizeof( soundInterface_t ) );

	Cmd_RemoveCommand( "play" );
	Cmd_RemoveCommand( "music");
	Cmd_RemoveCommand( "stopmusic");
	Cmd_RemoveCommand( "s_list" );
	Cmd_RemoveCommand( "s_stop" );
	Cmd_RemoveCommand( "s_info" );

	S_CodecShutdown();

	if ( s_audioArena ) {
		Arena_Destroy( s_audioArena );
		s_audioArena = NULL;
	}

	cls.soundStarted = qfalse;
}
