/*
===========================================================================
nav_public.h -- public nav API exposed to server code (sv_game.c, sv_init.c)

This is the ONLY nav header that server code should include.
Never include nav_local.h or nav_impl headers directly from outside the nav module.
===========================================================================
*/
#ifndef NAV_PUBLIC_H
#define NAV_PUBLIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../q_shared.h"
#include "../q_feats.h"

/* Engine-side lifecycle */
void      Nav_Init( void );
void      Nav_Shutdown( void );
void      Nav_LoadMap( const char *mapname );
void      Nav_UnloadMap( void );
int       Nav_IsReady( void );

/* Trap dispatch — called from sv_game.c for all G_NAV_* syscalls */
intptr_t  Nav_HandleTrap( int trap, const intptr_t *args, byte *vmBase );

#ifdef __cplusplus
}
#endif

#endif /* NAV_PUBLIC_H */
