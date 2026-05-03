#include "g_character.h"
#include "g_local.h"

#define MAX_CHARACTERS 256

static g_characterInfo_t s_characters[MAX_CHARACTERS];
static int s_numCharacters;

static qboolean G_Character_HasName( const char *name ) {
	for ( int i = 0; i < s_numCharacters; i++ ) {
		if ( s_characters[i].inuse && !Q_stricmp( s_characters[i].name, name ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean G_Character_Add(const char *name) {
	char profilePath[MAX_QPATH];
	fileHandle_t f;
	int len;

	if ( !name || !name[0] ) {
		return qfalse;
	}

	if ( s_numCharacters >= MAX_CHARACTERS ) {
		return qfalse;
	}

	Com_sprintf( profilePath, sizeof( profilePath ), "characters/%s/main.lua", name );
	len = trap_FS_FOpenFile( profilePath, &f, FS_READ );
	if ( len <= 0 || !f ) {
		return qfalse;
	}
	trap_FS_FCloseFile( f );

	// Fetch display_name via engine Lua runtime.
	{
		char key[MAX_QPATH];
		Com_sprintf( key, sizeof( key ), "char:%s:display_name", name );
		if ( !trap_GetValue( s_characters[s_numCharacters].display_name,
		                     sizeof( s_characters[s_numCharacters].display_name ),
		                     key ) || !s_characters[s_numCharacters].display_name[0] ) {
			// Fallback: capitalize the directory slug.
			Q_strncpyz( s_characters[s_numCharacters].display_name, name,
				sizeof( s_characters[s_numCharacters].display_name ) );
			if ( s_characters[s_numCharacters].display_name[0] >= 'a' &&
			     s_characters[s_numCharacters].display_name[0] <= 'z' ) {
				s_characters[s_numCharacters].display_name[0] -= 32;
			}
		}
	}

	s_characters[s_numCharacters].inuse = qtrue;
	Q_strncpyz( s_characters[s_numCharacters].name, name, sizeof( s_characters[s_numCharacters].name ) );
	Q_strncpyz( s_characters[s_numCharacters].skinName, "default", sizeof( s_characters[s_numCharacters].skinName ) );
	Q_strncpyz( s_characters[s_numCharacters].profilePath, profilePath, sizeof( s_characters[s_numCharacters].profilePath ) );
	s_numCharacters++;

	return qtrue;
}

void G_Characters_Init( void ) {
	char buf[MAX_QPATH];
	char key[64];
	int count, i;

	memset( s_characters, 0, sizeof( s_characters ) );
	s_numCharacters = 0;

	buf[0] = '\0';
	trap_GetValue( buf, sizeof( buf ), "char_count" );
	count = atoi( buf );

	for ( i = 0; i < count; i++ ) {
		Com_sprintf( key, sizeof( key ), "char_at:%d", i );
		buf[0] = '\0';
		trap_GetValue( buf, sizeof( buf ), key );
		if ( buf[0] && buf[0] != '_' ) {
			G_Character_Add( buf );
		}
	}

	Com_Log( SEV_INFO, LOG_CAT_GAME, "%i characters parsed\n", s_numCharacters );
}

int G_Characters_Count( void ) {
	return s_numCharacters;
}

const g_characterInfo_t *G_Character_GetByName( const char *name ) {
	if ( !name || !name[0] ) {
		return NULL;
	}

	for ( int i = 0; i < s_numCharacters; i++ ) {
		if ( s_characters[i].inuse && !Q_stricmp( s_characters[i].name, name ) ) {
			return &s_characters[i];
		}
	}

	return NULL;
}

const g_characterInfo_t *G_Character_GetByIndex( int index ) {
	if ( index < 0 || index >= s_numCharacters ) {
		return NULL;
	}
	if ( !s_characters[index].inuse ) {
		return NULL;
	}
	return &s_characters[index];
}

int G_Character_GetRandomIndex( void ) {
	if ( s_numCharacters <= 0 ) {
		return -1;
	}
	if ( s_numCharacters == 1 ) {
		return 0;
	}
	return (int)( random() * s_numCharacters ) % s_numCharacters;
}

const g_characterInfo_t *G_Characters_GetAll( void ) {
	return s_characters;
}
