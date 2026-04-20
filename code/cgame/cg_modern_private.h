/*
 * cg_modern_private.h -- Modern text rendering types and draw style flags.
 *
 * Shared by cg_moderntext.c, cg_drawtools.c, cg_utils.c
 * and any other cgame code that uses the CG_ModernDraw* API.
 */
#ifndef CG_MODERN_PRIVATE_H
#define CG_MODERN_PRIVATE_H

#include "cg_local.h"

/* text_command_t removed — bitmap text compiler dead after MSDF migration */

#ifndef FS_INVALID_HANDLE
#define FS_INVALID_HANDLE 0
#endif

// q3now compatibility macros
#ifndef Vector4Clear
#define Vector4Clear(a)  ((a)[0]=(a)[1]=(a)[2]=(a)[3]=0)
#endif
#ifndef Vector4Subtract
#define Vector4Subtract(a,b,c) ((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2],(c)[3]=(a)[3]-(b)[3])
#endif
#ifndef Vector4MA
#define Vector4MA(v,s,b,o) ((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s),(o)[3]=(v)[3]+(b)[3]*(s))
#endif

/* DS_* draw style flags removed — only used in client/wired (cl_wired_fonts.h) */
/* Old draw-string helpers removed -- use Text_Draw / Text_Measure */

// cg_utils.c -- A collection of utility functions
qboolean CG_ModernIsGameTypeFreeze( void );
qboolean CG_IsFollowing( void );
qboolean CG_IsSpectator( void );
void CG_ModernDrawFrame( float x, float y, float w, float h, const float *border, const float *borderColor, qboolean filled );

// OSP2 cvar stubs
extern vmCvar_t cg_MaxlocationWidth;

#ifndef DRAW_REWARDS_NOSOUND
#define DRAW_REWARDS_NOSOUND  0x02
#define DRAW_REWARDS_NOICON   0x04
#endif

#endif /* CG_MODERN_PRIVATE_H */
