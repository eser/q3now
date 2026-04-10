#ifndef G_CHARACTER_H
#define G_CHARACTER_H

#include "../qcommon/q_shared.h"

typedef struct {
	qboolean inuse;
	char name[MAX_QPATH];
	char display_name[64];
	char model[MAX_QPATH];   // "visor/default" format for userinfo
	char profilePath[MAX_QPATH];
	char botPath[MAX_QPATH];
} g_characterInfo_t;

void                    G_Characters_Init( void );
int                     G_Characters_Count( void );
const g_characterInfo_t *G_Character_GetByName( const char *name );
const g_characterInfo_t *G_Character_GetByIndex( int index );
const g_characterInfo_t *G_Characters_GetAll( void );
int                     G_Character_GetRandomIndex( void );

#endif
