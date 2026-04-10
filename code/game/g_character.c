#include "g_character.h"
#include "g_local.h"

#define MAX_CHARACTERS 256

static g_characterInfo_t s_characters[MAX_CHARACTERS];
static int s_numCharacters;

static qboolean G_Character_HasName( const char *name ) {
	int i;
	for ( i = 0; i < s_numCharacters; i++ ) {
		if ( s_characters[i].inuse && !Q_stricmp( s_characters[i].name, name ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean G_Character_Add(const char *name) {
	char profilePath[MAX_QPATH];
	char botPath[MAX_QPATH];
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

	Com_sprintf( botPath, sizeof( botPath ), "characters/%s/bot/main.lua", name );
	len = trap_FS_FOpenFile( botPath, &f, FS_READ );
	if ( len <= 0 || !f ) {
		if ( f ) {
			trap_FS_FCloseFile( f );
		}
		Com_sprintf( botPath, sizeof( botPath ), "characters/_base/bot/main.lua" );
		len = trap_FS_FOpenFile( botPath, &f, FS_READ );
		if ( len <= 0 || !f ) {
			return qfalse;
		}
	}
	trap_FS_FCloseFile( f );

	s_characters[s_numCharacters].inuse = qtrue;
	Q_strncpyz( s_characters[s_numCharacters].name, name, sizeof( s_characters[s_numCharacters].name ) );
	Q_strncpyz( s_characters[s_numCharacters].display_name, name, sizeof( s_characters[s_numCharacters].display_name ) );
	// capitalise first letter of display_name
	if ( s_characters[s_numCharacters].display_name[0] >= 'a' && s_characters[s_numCharacters].display_name[0] <= 'z' ) {
		s_characters[s_numCharacters].display_name[0] -= 32;
	}
	// model in "name/default" format as used by Q3 userinfo
	Com_sprintf( s_characters[s_numCharacters].model, sizeof( s_characters[s_numCharacters].model ), "%s/default", name );
	Q_strncpyz( s_characters[s_numCharacters].profilePath, profilePath, sizeof( s_characters[s_numCharacters].profilePath ) );
	Q_strncpyz( s_characters[s_numCharacters].botPath, botPath, sizeof( s_characters[s_numCharacters].botPath ) );
	s_numCharacters++;

	return qtrue;
}

void G_Characters_Init( void ) {
	char dirlist[4096];
	char *dirptr;
	int numdirs;
	int i;
	int dirlen;

	Com_Memset( s_characters, 0, sizeof( s_characters ) );
	s_numCharacters = 0;

	numdirs = trap_FS_GetFileList( "characters", ".lua", dirlist, sizeof( dirlist ) );
	dirptr = dirlist;

	for ( i = 0; i < numdirs; i++ ) {
		char name[MAX_QPATH];
		char *slash;
		dirlen = strlen( dirptr );
		if ( dirlen <= 0 ) {
			dirptr++;
			continue;
		}

		if ( dirlen <= 9 || Q_stricmp( dirptr + dirlen - 9, "/main.lua" ) ) {
			dirptr += dirlen + 1;
			continue;
		}

		Q_strncpyz( name, dirptr, sizeof( name ) );
		slash = strchr( name, '/' );
		if ( !slash ) {
			dirptr += dirlen + 1;
			continue;
		}
		*slash = '\0';

		if ( !name[0] || !Q_stricmp( name, "_base" ) || G_Character_HasName( name ) ) {
			dirptr += dirlen + 1;
			continue;
		}

		G_Character_Add( name );

		dirptr += dirlen + 1;
	}

	G_Printf( "%i characters parsed\n", s_numCharacters );
}

int G_Characters_Count( void ) {
	return s_numCharacters;
}

const g_characterInfo_t *G_Character_GetByName( const char *name ) {
	int i;

	if ( !name || !name[0] ) {
		return NULL;
	}

	for ( i = 0; i < s_numCharacters; i++ ) {
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
