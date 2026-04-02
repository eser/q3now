/*
 * cg_modern_private.h -- Modern text rendering types and draw style flags.
 *
 * Shared by cg_moderntext.c, cg_scoreboard.c, cg_drawtools.c, cg_utils.c
 * and any other cgame code that uses the CG_ModernDraw* API.
 */
#ifndef CG_MODERN_PRIVATE_H
#define CG_MODERN_PRIVATE_H

#include "cg_local.h"

// Modern text rendering types
#define OSP_TEXT_CMD_MAX 2048

typedef enum {
	OSP_TEXT_CMD_CHAR = 0,
	OSP_TEXT_CMD_STOP,
	OSP_TEXT_CMD_FADE,
	OSP_TEXT_CMD_TEXT_COLOR,
	OSP_TEXT_CMD_SHADOW_COLOR,
} text_command_type_t;

typedef struct {
	text_command_type_t type;
	union {
		char character;
		float fade;
		vec4_t color;
	} value;
} text_command_t;

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

// OSP2 draw style flags
#ifndef DS_HLEFT
#define DS_HLEFT        0x0000
#define DS_HCENTER      0x0001
#define DS_HRIGHT       0x0002
#define DS_VTOP         0x0000
#define DS_VCENTER      0x0004
#define DS_VBOTTOM      0x0008
#define DS_PROPORTIONAL 0x0010
#define DS_SHADOW       0x0020
#define DS_EMOJI        0x0040
#define DS_FORCE_COLOR  0x0080
#define DS_MAX_WIDTH_IS_CHARS 0x0100
#endif

// Modern text/font system (implemented in cg_moderntext.c)
int CG_FontIndexFromName( const char *name );
void CG_FontSelect( int fontIndex );
qboolean CG_Hex16GetColor( const char *str, float *color );
text_command_t *CG_CompileText( const char *text );
void CG_CompiledTextDestroy( text_command_t *root );
void CG_ModernDrawString( float x, float y, const char *str, const vec4_t color, float charW, float charH, int maxWidth, int flags, vec4_t bgColor );
int CG_ModernDrawStringLenPix( const char *str, float charW, int flags, int toWidth );
void CG_ModernDrawStringNew( float x, float y, const char *str, const vec4_t color, vec4_t shadowColor, float charW, float charH, int maxWidth, int flags, vec4_t bgColor, vec4_t border, vec4_t borderColor );

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
