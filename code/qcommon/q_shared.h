/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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
//
#ifndef __Q_SHARED_H
#define __Q_SHARED_H

// q_shared.h -- included first by ALL program modules.
// A user mod should never modify this file

#define Q3NOW_ENGINE_VERSION  "q3now 0.80"
#ifndef Q3NOW_ENGINE_RELEASE_VERSION
  #define Q3NOW_ENGINE_RELEASE_VERSION Q3NOW_ENGINE_VERSION
#endif
#ifdef CHANNEL_SUFFIX
#define CLIENT_WINDOW_TITLE   "q3now" CHANNEL_SUFFIX
#define CONSOLE_WINDOW_TITLE  "q3now" CHANNEL_SUFFIX " Console"
#else
#define CLIENT_WINDOW_TITLE   "q3now"
#define CONSOLE_WINDOW_TITLE  "q3now Console"
#endif
// 1.32 released 7-10-2002

//#define DEFAULT_GAME			"edawn"

#define BASEGAME				"baseq3"

#define MAX_NETNAME				36
#define MAX_TEAMNAME            32
#define MAX_MASTER_SERVERS      5	// number of supported master servers

#define GAMENAME_FOR_MASTER		"q3now"
#define HEARTBEAT_FOR_MASTER	"QuakeArena-1"

#define DEMOEXT	"dm_"			// standard demo extension

#ifdef _MSC_VER

#pragma warning(disable : 4018)     // signed/unsigned mismatch
//#pragma warning(disable : 4032)
//#pragma warning(disable : 4051)
#pragma warning(disable : 4057)		// slightly different base types
#pragma warning(disable : 4100)		// unreferenced formal parameter
//#pragma warning(disable : 4115)
#pragma warning(disable : 4125)		// decimal digit terminates octal escape sequence
#pragma warning(disable : 4127)		// conditional expression is constant
//#pragma warning(disable : 4136)
#pragma warning(disable : 4152)		// nonstandard extension, function/data pointer conversion in expression
#pragma warning(disable : 4200)		// nonstandard extension used: size-sided array in struct/union
//#pragma warning(disable : 4201)
#pragma warning(disable : 4206)		// nonstandard extension used: translation unit is empty
//#pragma warning(disable : 4214)
#pragma warning(disable : 4267)		// conversion from 'size_t' to 'int', possible loss of data
#pragma warning(disable : 4244)
#pragma warning(disable : 4142)		// benign redefinition
//#pragma warning(disable : 4305)		// truncation from const double to float
//#pragma warning(disable : 4310)		// cast truncates constant value
//#pragma warning(disable:  4505) 	// unreferenced local function has been removed
//#pragma warning(disable : 4514)
#pragma warning(disable : 4702)		// unreachable code
#pragma warning(disable : 4711)		// selected for automatic inline expansion
#pragma warning(disable : 4220)		// varargs matches remaining parameters
#pragma warning(disable : 4324)		// 'q_jpeg_error_mgr_s' : structure was padded due to alignment specifier
#pragma warning(disable : 4091)		// 'typedef': ignored on lef of <..> when no variable is declared
//#pragma intrinsic( memset, memcpy )
#endif

//Ignore __attribute__ on non-gcc/clang platforms
#if !defined(__GNUC__) && !defined(__clang__)
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#ifdef __GNUC__
#define UNUSED_VAR __attribute__((unused))
#else
#define UNUSED_VAR
#endif

#if (defined _MSC_VER)
#define Q_EXPORT __declspec(dllexport)
#elif (defined __SUNPRO_C)
#define Q_EXPORT __global
#elif ((__GNUC__ >= 3) && (!__EMX__) && (!sun))
#define Q_EXPORT __attribute__((visibility("default")))
#else
#define Q_EXPORT
#endif

#if defined(__GNUC__) || defined(__clang__)
#define NORETURN __attribute__((noreturn))
#define NORETURN_PTR __attribute__((noreturn))
#elif defined(_MSC_VER)
#define NORETURN __declspec(noreturn)
// __declspec doesn't work on function pointers
#define NORETURN_PTR /* nothing */
#else
#define NORETURN /* nothing */
#define NORETURN_PTR /* nothing */
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FORMAT_PRINTF(x, y) __attribute__((format (printf, x, y)))
#else
#define FORMAT_PRINTF(x, y) /* nothing */
#endif

/**********************************************************************
  VM Considerations

  The VM can not use the standard system headers because we aren't really
  using the compiler they were meant for.  We use bg_lib.h which contains
  prototypes for the functions we define for our own use in bg_lib.c.

  When writing mods, please add needed headers HERE, do not start including
  stuff like <stdio.h> in the various .c files that make up each of the VMs
  since you will be including system headers files can will have issues.

  Remember, if you use a C library function that is not defined in bg_lib.c,
  you will have to add your own version for support in the VM.

 **********************************************************************/

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>

#include "q_platform.h"

/*
============================================================================

                    BYTE ORDER FUNCTIONS

============================================================================
*/

/*
 * Compile-time endianness detection.
 * Q_BIG_ENDIAN evaluates to 1 on big-endian, 0 on little-endian.
 */
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
  #define Q_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#elif defined(_BIG_ENDIAN) || defined(__BIG_ENDIAN__)
  #define Q_BIG_ENDIAN 1
#elif defined(__LITTLE_ENDIAN__) || defined(_LITTLE_ENDIAN) \
   || defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64) \
   || defined(__x86_64__) || defined(__i386__) \
   || defined(__aarch64__) || defined(__arm__) \
   || defined(__EMSCRIPTEN__) || defined(__wasm__)
  #define Q_BIG_ENDIAN 0
#else
  #error "Cannot determine endianness - add your platform here"
#endif

/* Low-level byte swap via compiler intrinsics (single instruction) */
static ID_INLINE uint16_t Q_bswap16( uint16_t x )
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap16( x );
#elif defined(_MSC_VER)
    return _byteswap_ushort( x );
#else
    return (uint16_t)( (x >> 8) | (x << 8) );
#endif
}

static ID_INLINE uint32_t Q_bswap32( uint32_t x )
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap32( x );
#elif defined(_MSC_VER)
    return _byteswap_ulong( x );
#else
    return ( x >> 24 )
         | ( (x >> 8) & 0x0000FF00u )
         | ( (x << 8) & 0x00FF0000u )
         | ( x << 24 );
#endif
}

static ID_INLINE uint64_t Q_bswap64( uint64_t x )
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64( x );
#elif defined(_MSC_VER)
    return _byteswap_uint64( x );
#else
    x = ( (x & 0x00000000FFFFFFFFull) << 32 ) | ( (x & 0xFFFFFFFF00000000ull) >> 32 );
    x = ( (x & 0x0000FFFF0000FFFFull) << 16 ) | ( (x & 0xFFFF0000FFFF0000ull) >> 16 );
    x = ( (x & 0x00FF00FF00FF00FFull) << 8  ) | ( (x & 0xFF00FF00FF00FF00ull) >> 8  );
    return x;
#endif
}

/* float/int bit-cast union — needed by the endian swap helpers below */
typedef union floatint_u { int32_t i; uint32_t u; float f; unsigned char b[4]; } floatint_t;

/*
 * Host <-> Little/Big endian conversion.
 * Little-endian (x86, ARM, WASM): Little* is identity, Big* swaps.
 * Big-endian: reversed.
 */
#if Q_BIG_ENDIAN

static ID_INLINE short   LittleShort( short x )    { return (short)Q_bswap16( (uint16_t)x ); }
static ID_INLINE int     LittleLong( int x )       { return (int)Q_bswap32( (uint32_t)x ); }
static ID_INLINE int64_t LittleLong64( int64_t x ) { return (int64_t)Q_bswap64( (uint64_t)x ); }
static ID_INLINE float   LittleFloat( float x ) {
    floatint_t fi;
    fi.f = x;
    fi.i = (int)Q_bswap32( (uint32_t)fi.i );
    return fi.f;
}

static ID_INLINE short   BigShort( short x )       { return x; }
static ID_INLINE int     BigLong( int x )          { return x; }
static ID_INLINE int64_t BigLong64( int64_t x )    { return x; }
static ID_INLINE float   BigFloat( float x )       { return x; }

#else /* little-endian */

static ID_INLINE short   LittleShort( short x )    { return x; }
static ID_INLINE int     LittleLong( int x )       { return x; }
static ID_INLINE int64_t LittleLong64( int64_t x ) { return x; }
static ID_INLINE float   LittleFloat( float x )    { return x; }

static ID_INLINE short   BigShort( short x )       { return (short)Q_bswap16( (uint16_t)x ); }
static ID_INLINE int     BigLong( int x )          { return (int)Q_bswap32( (uint32_t)x ); }
static ID_INLINE int64_t BigLong64( int64_t x )    { return (int64_t)Q_bswap64( (uint64_t)x ); }
static ID_INLINE float   BigFloat( float x ) {
    floatint_t fi;
    fi.f = x;
    fi.i = (int)Q_bswap32( (uint32_t)fi.i );
    return fi.f;
}

#endif

//=============================================================

#if defined (_MSC_VER) && !defined(__clang__)
	typedef __int64 int64_t;
	typedef __int32 int32_t;
	typedef __int16 int16_t;
	typedef signed __int8 int8_t;
	typedef unsigned __int64 uint64_t;
	typedef unsigned __int32 uint32_t;
	typedef unsigned __int16 uint16_t;
	typedef unsigned __int8 uint8_t;
#else
	#include <stdint.h>
#endif

#if defined (_WIN32)
#if !defined(_MSC_VER)
// use GCC/Clang functions
#define Q_setjmp __builtin_setjmp
#define Q_longjmp __builtin_longjmp
#elif idx64 && (_MSC_VER >= 1910)
// use custom setjmp()/longjmp() implementations
#define Q_setjmp Q_setjmp_c
#define Q_longjmp Q_longjmp_c
int Q_setjmp_c(void *);
int Q_longjmp_c(void *, int);
#else // !idx64 || MSVC<2017
#define Q_setjmp setjmp
#define Q_longjmp longjmp
#endif
#else // !_WIN32
#define Q_setjmp setjmp
#define Q_longjmp longjmp
#endif

typedef unsigned char byte;

typedef enum { qfalse = 0, qtrue } qboolean;

typedef union {
	byte rgba[4];
	uint32_t u32;
} color4ub_t;


typedef int		qhandle_t;
typedef int		sfxHandle_t;
typedef int		fileHandle_t;
typedef int		clipHandle_t;

#define PAD(base, alignment)	(((base)+(alignment)-1) & ~((alignment)-1))
#define PADLEN(base, alignment)	(PAD((base), (alignment)) - (base))

#define PADP(base, alignment)	((void *) PAD((intptr_t) (base), (alignment)))

#ifdef __GNUC__
#define QALIGN(x) __attribute__((aligned(x)))
#else
#define QALIGN(x)
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#define	MAX_QINT			0x7fffffff
#define	MIN_QINT			(-MAX_QINT-1)

#define	MAX_UINT			((unsigned)(~0))

#define ARRAY_LEN(x)		(sizeof(x) / sizeof(*(x)))
#define STRARRAY_LEN(x)		(ARRAY_LEN(x) - 1)

// angle indexes
#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

// the game guarantees that no string from the network will ever
// exceed MAX_STRING_CHARS
#define	MAX_STRING_CHARS	1024	// max length of a string passed to Cmd_TokenizeString
#define	MAX_STRING_TOKENS	1024	// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		1024	// max length of an individual token

#define	MAX_INFO_STRING		1024
#define	MAX_INFO_KEY		1024
#define	MAX_INFO_VALUE		1024

#define MAX_INFO_TOKENS ((MAX_INFO_STRING/3)+2)

typedef struct {
	char        buffer[MAX_INFO_STRING];
	const char  *keys[MAX_INFO_TOKENS];
	const char  *values[MAX_INFO_TOKENS];
	int         count;
} InfoTokens;

#define MAX_USERINFO_LENGTH (MAX_INFO_STRING-13) // incl. length of 'connect ""' or 'userinfo ""' and reserving one byte to avoid q3msgboom
													
#define	BIG_INFO_STRING		8192  // used for system info key only
#define	BIG_INFO_KEY		  8192
#define	BIG_INFO_VALUE		8192


#define	MAX_QPATH		64		// max length of a quake game pathname — network-safe,
								// used in configstrings, userinfo, demo format, and any
								// string that crosses the client/server boundary
#define MAX_VFS_PATH	128		// max length of a virtual filesystem (pak-internal)
								// asset path — relative, forward-slash, local-only,
								// never sent over network. Use for stack buffers in
								// asset loaders and for inputs to registry functions.
#ifdef PATH_MAX
#define MAX_OSPATH		PATH_MAX
#else
#define	MAX_OSPATH		256		// max length of a filesystem pathname
#endif

#define	MAX_NAME_LENGTH			32		// max length of a client name
#define	MAX_HOSTNAME_LENGTH		80

#define	MAX_SAY_TEXT	150

// parameters for command buffer stuffing
typedef enum {
	EXEC_NOW,			// don't return until completed, a VM should NEVER use this,
						// because some commands might cause the VM to be unloaded...
	EXEC_INSERT,		// insert at current position, but don't run yet
	EXEC_APPEND			// add to end of the command buffer (normal case)
} cbufExec_t;


//
// these aren't needed by any of the VMs.  put in another header?
//
#define	MAX_MAP_AREA_BYTES		32		// bit vector of area visibility


// parameters to Com_Terminate
typedef enum {
	TERM_UNRECOVERABLE,     // process must exit (was ERR_FATAL)
	TERM_CLIENT_DROP,       // error disconnect, return to menu (was ERR_DROP)
	TERM_CLIENT_LEAVE,      // user-initiated disconnect (was ERR_DISCONNECT)
	TERM_SERVER_KICK        // server kicked client (was ERR_SERVERDISCONNECT)
} terminationReason_t;

// base model rendering values
#define	DEFAULT_MODEL			"visor"

// font rendering values used by ui and cgame

#define UI_LEFT			0x00000000	// default
#define UI_CENTER		0x00000001
#define UI_RIGHT		0x00000002
#define UI_FORMATMASK	0x00000007
#define UI_SMALLFONT	0x00000010
#define UI_BIGFONT		0x00000020	// default
#define UI_GIANTFONT	0x00000040
#define UI_DROPSHADOW	0x00000800
#define UI_BLINK		0x00001000
#define UI_INVERSE		0x00002000
#define UI_PULSE		0x00004000

#if defined(_DEBUG) && !defined(BSPC)
	#define HUNK_DEBUG
#endif

typedef enum {
	h_high,
	h_low,
	h_dontcare
} ha_pref;

#ifdef HUNK_DEBUG
#define Hunk_Alloc( size, preference )				Hunk_AllocDebug(size, preference, #size, __FILE__, __LINE__)
void *Hunk_AllocDebug( size_t size, ha_pref preference, const char *label, const char *file, int line );
#else
void *Hunk_Alloc( size_t size, ha_pref preference );
#endif

#define CIN_system	1
#define CIN_loop	2
#define	CIN_hold	4
#define CIN_silent	8
#define CIN_shader	16

/*
==============================================================

MATHLIB

==============================================================
*/


typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef vec_t quat_t[4];

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

#ifndef M_PI
#define M_PI		3.14159265358979323846f	// matches value in gcc v2 math.h
#endif

#ifndef M_LN2
#define M_LN2      0.693147180559945309417f
#endif

#ifdef __linux__
#ifdef __GLIBC__
#if idx64
// force version for better runtime compatibility
__asm__(".symver logf,logf@GLIBC_2.2.5");
__asm__(".symver powf,powf@GLIBC_2.2.5");
__asm__(".symver expf,expf@GLIBC_2.2.5");
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5");
#endif
#endif
#endif

#define NUMVERTEXNORMALS	162
extern	vec3_t	bytedirs[NUMVERTEXNORMALS];

#define TINYCHAR_WIDTH		(SMALLCHAR_WIDTH)
#define TINYCHAR_HEIGHT		(SMALLCHAR_HEIGHT/2)

#define SMALLCHAR_WIDTH		8
#define SMALLCHAR_HEIGHT	16

#define BIGCHAR_WIDTH		16
#define BIGCHAR_HEIGHT		16

extern	vec4_t		colorBlack;
extern	vec4_t		colorRed;
extern	vec4_t		colorGreen;
extern	vec4_t		colorBlue;
extern	vec4_t		colorYellow;
extern	vec4_t		colorMagenta;
extern	vec4_t		colorCyan;
extern	vec4_t		colorWhite;
extern	vec4_t		colorLtGrey;
extern	vec4_t		colorMdGrey;
extern	vec4_t		colorDkGrey;
extern	vec4_t		colorOrange;	// q3now
extern	vec4_t		colorIndigo;	// q3now
extern	vec4_t		colorSkyBlue;	// q3now

void Q_ParseColor( const char *str, float *col, float alpha );

#define Q_COLOR_ESCAPE	'^'
#define Q_IsColorString(p) ( *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE )

#define COLOR_BLACK		'0'
#define COLOR_RED		'1'
#define COLOR_GREEN		'2'
#define COLOR_YELLOW	'3'
#define COLOR_BLUE		'4'
#define COLOR_CYAN		'5'
#define COLOR_MAGENTA	'6'
#define COLOR_WHITE		'7'
#define ColorIndex(c)	( ( (c) - '0' ) & 7 )

#define S_COLOR_BLACK	"^0"
#define S_COLOR_RED		"^1"
#define S_COLOR_GREEN	"^2"
#define S_COLOR_YELLOW	"^3"
#define S_COLOR_BLUE	"^4"
#define S_COLOR_CYAN	"^5"
#define S_COLOR_MAGENTA	"^6"
#define S_COLOR_WHITE	"^7"

#define S_COLOR_DEVEL	S_COLOR_CYAN
#define S_COLOR_WARNING	S_COLOR_YELLOW
#define S_COLOR_ERROR	S_COLOR_RED

extern const vec4_t	g_color_table[ 64 ];
extern int ColorIndexFromChar( char ccode );

#define	MAKERGB( v, r, g, b ) v[0]=r;v[1]=g;v[2]=b
#define	MAKERGBA( v, r, g, b, a ) v[0]=r;v[1]=g;v[2]=b;v[3]=a

#define DEG2RAD( a ) ( ( (a) * M_PI ) / 180.0F )
#define RAD2DEG( a ) ( ( (a) * 180.0f ) / M_PI )

struct cplane_s;

extern	const vec3_t	vec3_origin;
extern	vec3_t	axisDefault[3];

#define	nanmask (255<<23)

#define	IS_NAN(x) (((*(int *)&x)&nanmask)==nanmask)

float Q_rsqrt( float f );		// reciprocal square root

float Q_log2f( float f );
float Q_exp2f( float f );

#define SQRTFAST( x ) ( (x) * Q_rsqrt( x ) )

signed char ClampChar( int i );
signed char ClampCharMove( int i );
signed short ClampShort( int i );

// this isn't a real cheap function to call!
int DirToByte( vec3_t dir );
void ByteToDir( int b, vec3_t dir );

#ifndef SGN
#define SGN(x) (((x) >= 0) ? !!(x) : -1)
#endif

#if	1

#define DotProduct(x,y)			((x)[0]*(y)[0]+(x)[1]*(y)[1]+(x)[2]*(y)[2])
#define VectorSubtract(a,b,c)	((c)[0]=(a)[0]-(b)[0],(c)[1]=(a)[1]-(b)[1],(c)[2]=(a)[2]-(b)[2])
#define VectorAdd(a,b,c)		((c)[0]=(a)[0]+(b)[0],(c)[1]=(a)[1]+(b)[1],(c)[2]=(a)[2]+(b)[2])
#define VectorCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2])
#define	VectorScale(v, s, o)	((o)[0]=(v)[0]*(s),(o)[1]=(v)[1]*(s),(o)[2]=(v)[2]*(s))
#define	VectorMA(v, s, b, o)	((o)[0]=(v)[0]+(b)[0]*(s),(o)[1]=(v)[1]+(b)[1]*(s),(o)[2]=(v)[2]+(b)[2]*(s))

#define DotProduct4(a,b)		((a)[0]*(b)[0] + (a)[1]*(b)[1] + (a)[2]*(b)[2] + (a)[3]*(b)[3])
#define VectorScale4(a,b,c)		((c)[0]=(a)[0]*(b),(c)[1]=(a)[1]*(b),(c)[2]=(a)[2]*(b),(c)[3]=(a)[3]*(b))

#else

#define DotProduct(x,y)			_DotProduct(x,y)
#define VectorSubtract(a,b,c)	_VectorSubtract(a,b,c)
#define VectorAdd(a,b,c)		_VectorAdd(a,b,c)
#define VectorCopy(a,b)			_VectorCopy(a,b)
#define	VectorScale(v, s, o)	_VectorScale(v,s,o)
#define	VectorMA(v, s, b, o)	_VectorMA(v,s,b,o)

#endif

#define VectorClear(a)			((a)[0]=(a)[1]=(a)[2]=0)
#define VectorNegate(a,b)		((b)[0]=-(a)[0],(b)[1]=-(a)[1],(b)[2]=-(a)[2])
#define VectorSet(v, x, y, z)	((v)[0]=(x), (v)[1]=(y), (v)[2]=(z))
#define Vector4Set(v,x,y,z,w)	((v)[0]=(x), (v)[1]=(y), (v)[2]=(z), v[3]=(w))
#define Vector4Copy(a,b)		((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define Byte4Copy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define QuatCopy(a,b)			((b)[0]=(a)[0],(b)[1]=(a)[1],(b)[2]=(a)[2],(b)[3]=(a)[3])

#define	SnapVector(v) {v[0]=((int)(v[0]));v[1]=((int)(v[1]));v[2]=((int)(v[2]));}
// just in case you don't want to use the macros
vec_t _DotProduct( const vec3_t v1, const vec3_t v2 );
void _VectorSubtract( const vec3_t veca, const vec3_t vecb, vec3_t out );
void _VectorAdd( const vec3_t veca, const vec3_t vecb, vec3_t out );
void _VectorCopy( const vec3_t in, vec3_t out );
void _VectorScale( const vec3_t in, float scale, vec3_t out );
void _VectorMA( const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc );

unsigned ColorBytes3 (float r, float g, float b);
unsigned ColorBytes4 (float r, float g, float b, float a);

float NormalizeColor( const vec3_t in, vec3_t out );

float RadiusFromBounds( const vec3_t mins, const vec3_t maxs );
void ClearBounds( vec3_t mins, vec3_t maxs );
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs );

static ID_INLINE int VectorCompare( const vec3_t v1, const vec3_t v2 ) {
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2]) {
		return 0;
	}			
	return 1;
}

static ID_INLINE vec_t VectorLength( const vec3_t v ) {
	return (vec_t)sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static ID_INLINE vec_t VectorLengthSquared( const vec3_t v ) {
	return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static ID_INLINE vec_t Distance( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return VectorLength( v );
}

static ID_INLINE vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
static ID_INLINE void VectorNormalizeFast( vec3_t v )
{
	float ilength;

	ilength = Q_rsqrt( DotProduct( v, v ) );

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

static ID_INLINE void VectorInverse( vec3_t v ){
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

static ID_INLINE void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}



vec_t VectorNormalize (vec3_t v);		// returns vector length
vec_t VectorNormalize2( const vec3_t v, vec3_t out );
void Vector4Scale( const vec4_t in, vec_t scale, vec4_t out );
void VectorRotate( const vec3_t in, const vec3_t matrix[3], vec3_t out );
int Q_log2(int val);

float Q_acos(float c);

int		Q_rand( int *seed );
float	Q_random( int *seed );
float	Q_crandom( int *seed );

#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#define crandom()	(2.0 * (random() - 0.5))

void vectoangles( const vec3_t value1, vec3_t angles);
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] );

void AxisClear( vec3_t axis[3] );
void AxisCopy( vec3_t in[3], vec3_t out[3] );

void SetPlaneSignbits( struct cplane_s *out );
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *plane);

qboolean BoundsIntersect(const vec3_t mins, const vec3_t maxs,
		const vec3_t mins2, const vec3_t maxs2);
qboolean BoundsIntersectSphere(const vec3_t mins, const vec3_t maxs,
		const vec3_t origin, vec_t radius);
qboolean BoundsIntersectPoint(const vec3_t mins, const vec3_t maxs,
		const vec3_t origin);

float	AngleMod(float a);
float	LerpAngle (float from, float to, float frac);
float	AngleSubtract( float a1, float a2 );
void	AnglesSubtract( vec3_t v1, vec3_t v2, vec3_t v3 );

float AngleNormalize360 ( float angle );
float AngleNormalize180 ( float angle );
float AngleDelta ( float angle1, float angle2 );

void SetupRotationMatrix( vec3_t matrix[3], const vec3_t dir, float degrees );

qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c );
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal );
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point, float degrees );
void RotateAroundDirection( vec3_t axis[3], float yaw );
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up );
// perpendicular vector could be replaced by this

//int	PlaneTypeForNormal (vec3_t normal);

void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]);
void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void PerpendicularVector( vec3_t dst, const vec3_t src );
int Q_isnan( float x );
float Q_atof( const char *str );

#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#endif

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif

//=============================================

float Com_Clamp( float min, float max, float value );

char	*COM_SkipPath( char *pathname );
const char	*COM_GetExtension( const char *name );
void	COM_StripExtension(const char *in, char *out, int destsize);
qboolean COM_CompareExtension(const char *in, const char *ext);
void	COM_DefaultExtension( char *path, int maxSize, const char *extension );

unsigned long Com_GenerateHashValue( const char *fname, const unsigned int size );
void Hash_SelfTest( void );

typedef enum {
	TK_GENEGIC = 0, // for single-char tokens
	TK_STRING,
	TK_QUOTED,
	TK_EQ,
	TK_NEQ,
	TK_GT,
	TK_GTE,
	TK_LT,
	TK_LTE,
	TK_MATCH,
	TK_OR,
	TK_AND,
	TK_SCOPE_OPEN,
	TK_SCOPE_CLOSE,
	TK_NEWLINE,
	TK_EOF,
} tokenType_t;

typedef struct {
	char        token[MAX_TOKEN_CHARS];
	char        parsename[MAX_TOKEN_CHARS];
	int         lines;
	int         tokenline;
	tokenType_t tokentype;
} ComParser;

void	COM_BeginParseSession( ComParser *parser, const char *name );
int		COM_GetCurrentParseLine( const ComParser *parser );
const char	*COM_Parse( ComParser *parser, const char **data_p );
const char	*COM_ParseExt( ComParser *parser, const char **data_p, qboolean allowLineBreak );
int		COM_Compress( char *data_p );
void	COM_ParseError( const ComParser *parser, const char *format, ... ) __attribute__ ((format (printf, 2, 3)));
void	COM_ParseWarning( const ComParser *parser, const char *format, ... ) __attribute__ ((format (printf, 2, 3)));
//int		COM_ParseInfos( const char *buf, int max, char infos[][MAX_INFO_STRING] );

char	*COM_ParseComplex( ComParser *parser, const char **data_p, qboolean allowLineBreak );

#define MAX_TOKENLENGTH		1024

#ifndef TT_STRING
//token types
#define TT_STRING					1			// string
#define TT_LITERAL					2			// literal
#define TT_NUMBER					3			// number
#define TT_NAME						4			// name
#define TT_PUNCTUATION				5			// punctuation
#endif

typedef struct pc_token_s
{
	int type;
	int subtype;
	int intvalue;
	float floatvalue;
	char string[MAX_TOKENLENGTH];
} pc_token_t;

// data is an in/out parm, returns a parsed out token

qboolean SkipBracedSection( ComParser *parser, const char **program, int depth );
void SkipRestOfLine( ComParser *parser, const char **data );

void Parse1DMatrix( ComParser *parser, const char **buf_p, int x, float *m);
void Parse2DMatrix( ComParser *parser, const char **buf_p, int y, int x, float *m);
void Parse3DMatrix( ComParser *parser, const char **buf_p, int z, int y, int x, float *m);

int QDECL Com_sprintf( char *dest, int size, const char *fmt, ... ) __attribute__ ((format (printf, 3, 4)));

const char *Com_SkipTokens( const char *s, int numTokens, const char *sep );
const char *Com_SkipCharset( const char *s, const char *sep );

void Com_RandomBytes( byte *string, int len );

void Com_SortList( char** list, int n );

// mode parm for FS_FOpenFile
typedef enum {
	FS_READ,
	FS_WRITE,
	FS_APPEND,
	FS_APPEND_SYNC
} fsMode_t;

typedef enum {
	FS_SEEK_CUR,
	FS_SEEK_END,
	FS_SEEK_SET
} fsOrigin_t;

//=============================================

extern const byte locase[ 256 ];

int Q_isprint( int c );
int Q_islower( int c );
int Q_isupper( int c );
int Q_isalpha( int c );

// portable case insensitive compare
int		Q_stricmp (const char *s1, const char *s2);
int		Q_stricmpn (const char *s1, const char *s2, int n);
char	*Q_strlwr( char *s1 );
char	*Q_strupr( char *s1 );
const char	*Q_stristr( const char *s, const char *find);

qboolean Q_isanumber( const char *s );
qboolean Q_isintegral( float f );

// buffer size safe library replacements
void	Q_strncpyz( char *dest, const char *src, int destsize );

int     Q_replace( const char *str1, const char *str2, char *src, int max_len );

char	*Q_stradd( char *dst, const char *src );
char	*Q_strncpy( char *dest, const char *src, int destsize );

// strlen that discounts Quake color sequences
int Q_PrintStrlen( const char *string );
// removes color sequences from string
char *Q_CleanStr( char *string );
// Count the number of char tocount encountered in string
int Q_CountChar(const char *string, char tocount);

// UTF-8 utilities
int         Q_UTF8_NextCodepoint( const char *str, int *bytesRead );
int         Q_UTF8_Strlen( const char *str );
int         Q_UTF8_Encode( int codepoint, char *out );
const char *Q_UTF8_Advance( const char *str, int n );

//=============================================
// qstring_t — bounded string with tracked length

typedef struct {
	char    *data;
	int     len;
	int     capacity;
} qstring_t;

// Declare a stack-local qstring_t. size must be a compile-time constant.
#define QS_LOCAL( name, size ) \
	char _##name##_buf[size]; \
	qstring_t name = QS_Wrap( _##name##_buf, size )

static ID_INLINE int         QS_Len( const qstring_t *qs )       { return qs->len; }
static ID_INLINE const char *QS_CStr( const qstring_t *qs )      { return qs->data; }
static ID_INLINE int         QS_Remaining( const qstring_t *qs ) { return qs->capacity - qs->len - 1; }
static ID_INLINE qboolean    QS_Empty( const qstring_t *qs )     { return (qboolean)(qs->len == 0); }

qstring_t QS_Wrap( char *buffer, int capacity );
qstring_t QS_WrapFrom( char *buffer, int capacity, const char *initial );
qstring_t QS_WrapExisting( char *buffer, int capacity );

void      QS_Set( qstring_t *qs, const char *src );
void      QS_Append( qstring_t *qs, const char *src );
void      QS_AppendChar( qstring_t *qs, char c );
void      QS_AppendN( qstring_t *qs, const char *src, int n );
void      QS_Clear( qstring_t *qs );
void      QS_Truncate( qstring_t *qs, int maxLen );

#ifdef __GNUC__
void      QS_Setf( qstring_t *qs, const char *fmt, ... ) __attribute__((format(printf, 2, 3)));
void      QS_Appendf( qstring_t *qs, const char *fmt, ... ) __attribute__((format(printf, 2, 3)));
#else
void      QS_Setf( qstring_t *qs, const char *fmt, ... );
void      QS_Appendf( qstring_t *qs, const char *fmt, ... );
#endif

qboolean  QS_Equal( const qstring_t *a, const qstring_t *b );
qboolean  QS_EqualI( const qstring_t *a, const qstring_t *b );
qboolean  QS_EqualStr( const qstring_t *qs, const char *str );
qboolean  QS_EqualStrI( const qstring_t *qs, const char *str );

void      QS_CopyTo( const qstring_t *qs, char *dest, int destsize );

//=============================================

// 64-bit integers for global rankings interface
// implemented as a struct for qvm compatibility
typedef struct
{
	byte	b0;
	byte	b1;
	byte	b2;
	byte	b3;
	byte	b4;
	byte	b5;
	byte	b6;
	byte	b7;
} qint64;

//=============================================
/*
short	BigShort(short l);
short	LittleShort(short l);
int		BigLong (int l);
int		LittleLong (int l);
qint64  BigLong64 (qint64 l);
qint64  LittleLong64 (qint64 l);
float	BigFloat (const float *l);
float	LittleFloat (const float *l);

void	Swap_Init (void);
*/
const char *QDECL va( const char *format, ... ) FORMAT_PRINTF(1, 2);

#define TRUNCATE_LENGTH	64
void Com_TruncateLongString( char *buffer, const char *s );

//=============================================

//
// key / value info strings
//
const char *Info_ValueForKey( const char *s, const char *key );
void Info_Tokenize( InfoTokens *tokens, const char *s );
const char *Info_ValueForKeyToken( const InfoTokens *tokens, const char *key );
#define Info_SetValueForKey( buf, key, value ) Info_SetValueForKey_s( (buf), MAX_INFO_STRING, (key), (value) )
qboolean Info_SetValueForKey_s( char *s, int slen, const char *key, const char *value );
qboolean Info_Validate( const char *s );
qboolean Info_ValidateKeyValue( const char *s );
const char *Info_NextPair( const char *s, char *key, char *value );
int Info_RemoveKey( char *s, const char *key );

// log.h needs cvar_t* (pointer only); forward-declare here so the include works
// before struct cvar_s is fully defined below.  The full typedef at line ~1091
// is a compatible redeclaration and is silently deduped by the C standard.
typedef struct cvar_s cvar_t;
#include "wired/core/shell/log.h"   // defines log_severity_t; declares Com_Log

// this is only here so the functions in q_shared.c and bg_*.c can link
void NORETURN FORMAT_PRINTF(2, 3) QDECL Com_Terminate( terminationReason_t level, const char *fmt, ... );
// Com_Log is declared by the log.h include above (correct FORMAT_PRINTF(2,3))

// Platform mutex abstraction. Fixed-size opaque struct; platform files define
// the actual member via _Static_assert-guarded cast into opaque[]. 128 bytes
// covers pthread_mutex_t on macOS arm64 (64 B, the largest target) and
// CRITICAL_SECTION on Win64 (40 B). Sys_MutexInit returns qfalse on resource
// exhaustion; Com_Init callers must Com_Terminate TERM_UNRECOVERABLE on failure.
#define SYS_MUTEX_OPAQUE_SIZE 128
typedef struct sys_mutex_s {
	unsigned char opaque[SYS_MUTEX_OPAQUE_SIZE];
} sys_mutex_t;
qboolean  Sys_MutexInit   ( sys_mutex_t *m );
void      Sys_MutexLock   ( sys_mutex_t *m );
void      Sys_MutexUnlock ( sys_mutex_t *m );
void      Sys_MutexDestroy( sys_mutex_t *m );


/*
==========================================================

CVARS (console variables)

Many variables can be used for cheating purposes, so when
cheats is zero, force all unspecified variables to their
default values.
==========================================================
*/

#define	CVAR_ARCHIVE			0x00000001	// set to cause it to be saved to vars.rc
					// used for system variables, not for player
					// specific configurations
#define	CVAR_USERINFO			0x00000002	// sent to server on connect or change
#define	CVAR_SERVERINFO			0x00000004	// sent in response to front end requests
#define	CVAR_SYSTEMINFO			0x00000008	// these cvars will be duplicated on all clients
#define	CVAR_INIT				0x00000010	// don't allow change from console at all,
					// but can be set from the command line
#define	CVAR_LATCH				0x00000020	// will only change when C code next does
					// a Cvar_Get(), so it can't be changed
					// without proper initialization.  modified
					// will be set, even though the value hasn't
					// changed yet
#define	CVAR_ROM				0x00000040	// display only, cannot be set by user at all
#define	CVAR_USER_CREATED		0x00000080	// created by a set command
#define	CVAR_TEMP				0x00000100	// can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT				0x00000200	// can not be changed if cheats are disabled
#define CVAR_NORESTART			0x00000400	// do not clear when a cvar_restart is issued

#define CVAR_SERVER_CREATED		0x00000800	// cvar was created by a server the client connected to.
#define CVAR_VM_CREATED			0x00001000	// cvar was created exclusively in one of the VMs.
#define CVAR_PROTECTED			0x00002000	// prevent modifying this var from VMs or the server

#define CVAR_NODEFAULT			0x00004000	// do not write to config if matching with default value

#define CVAR_PRIVATE			0x00008000	// can't be read from VM

#define CVAR_NOTABCOMPLETE		0x00010000 // no tab completion in console

#define CVAR_CMDLINE_CREATED	0x00020000 // cvar was created through the command-line (+set)

// These flags are only returned by the Cvar_Flags() function
#define CVAR_MODIFIED			0x40000000	// Cvar was modified
#define CVAR_NONEXISTENT		0x80000000	// Cvar doesn't exist.

typedef enum {
	CV_NONE = 0,
	CV_FLOAT,
	CV_INTEGER,
	CV_FSPATH,
	CV_MAX,
} cvarValidator_t;

typedef enum {
	CVG_NONE = 0,
	CVG_RENDERER,
	CVG_SERVER,
	CVG_MAX,
} cvarGroup_t;

// cvarType_t — explicit value type, used by the typed Cvar_Register() API.
// CVT_STRING = 0 so zero-initialized legacy cvars (from Cvar_Get) are CVT_STRING.
typedef enum {
	CVT_STRING = 0, // any string; ->integer = atoi, ->value = atof (best-effort)
	CVT_BOOL,       // 0/1; also accepts true/false, yes/no, on/off; normalises to "0"/"1"
	CVT_INT,        // integer; optional [min, max]; rejects non-integer and out-of-range
	CVT_FLOAT,      // float;   optional [min, max]; rejects non-numeric and out-of-range
	CVT_ENUM        // one of a fixed string set; ->integer = index in enumValues[]
} cvarType_t;

// Callback fired AFTER the value is set, validated, and modificationCount incremented.
// The cvar retains its new value; the callback cannot veto it.
// May set other cvars (inline dispatch); re-entrancy is guarded.
typedef void (*cvarCallback_t)( struct cvar_s *self );

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s cvar_t;

struct cvar_s {
	char		*name;
	char		*string;
	char		*resetString;		// cvar_restart will reset to this value
	char		*latchedString;		// for CVAR_LATCH vars
	int			flags;
	int			modificationCount;	// incremented each time the cvar is changed
	float		value;				// Q_atof( string )
	int			integer;			// atoi( string )
	cvarValidator_t validator;
	char		*mins;
	char		*maxs;
	char		*description;

	cvar_t		*next;
	cvar_t		*prev;
	cvar_t		*hashNext;
	cvar_t		*hashPrev;
	int			hashIndex;
	cvarGroup_t	group;				// to track changes

	// --- typed registration fields (set by Cvar_Register; zero for Cvar_Get cvars) ---
	cvarType_t      type;           // value type; CVT_STRING=0 for all legacy cvars
	float           typeMin;        // CVT_INT/CVT_FLOAT: inclusive lower bound (0==no check when typeMin==typeMax)
	float           typeMax;        // CVT_INT/CVT_FLOAT: inclusive upper bound
	const char    **enumValues;     // CVT_ENUM: NULL-terminated array of valid strings (static)
	int             enumCount;      // cached count of enumValues entries
	cvarCallback_t  onChange;       // called after value is set and validated (NULL = none)
};

#define	MAX_CVAR_VALUE_STRING	256

// ---------------------------------------------------------------------------
// cvarDesc_t — single-point-of-truth cvar specification for Cvar_Register().
// All properties (name, default, description, flags, type, range, enum list,
// callback) declared in one struct; no follow-up Cvar_SetDescription /
// Cvar_CheckRange calls needed.
// ---------------------------------------------------------------------------

typedef struct {
    const char      *name;
    const char      *defaultValue;
    const char      *description;
    int              flags;
    cvarType_t       type;
    float            min;           // CVT_INT/CVT_FLOAT: lower bound (min==max==0 → no range check)
    float            max;           // CVT_INT/CVT_FLOAT: upper bound
    const char     **enumValues;    // CVT_ENUM: NULL-terminated valid-value array (must be static)
    cvarCallback_t   onChange;      // called after value changes; NULL = none
} cvarDesc_t;

// Convenience initialiser macros.
// Usage: static const cvarDesc_t my_cvar = CVAR_INT( "my_cvar", "0", CVAR_ARCHIVE, "desc", 0, 100 );
#define CVAR_STRING( name_, def_, flags_, desc_ ) \
    { name_, def_, desc_, flags_, CVT_STRING, 0, 0, NULL, NULL }

#define CVAR_BOOL( name_, def_, flags_, desc_ ) \
    { name_, def_, desc_, flags_, CVT_BOOL, 0, 0, NULL, NULL }

#define CVAR_INT( name_, def_, flags_, desc_, lo_, hi_ ) \
    { name_, def_, desc_, flags_, CVT_INT, (float)(lo_), (float)(hi_), NULL, NULL }

#define CVAR_FLOAT( name_, def_, flags_, desc_, lo_, hi_ ) \
    { name_, def_, desc_, flags_, CVT_FLOAT, (lo_), (hi_), NULL, NULL }

#define CVAR_ENUM( name_, def_, flags_, desc_, vals_ ) \
    { name_, def_, desc_, flags_, CVT_ENUM, 0, 0, (vals_), NULL }

// _CB variants add a callback parameter.
#define CVAR_STRING_CB( name_, def_, flags_, desc_, cb_ ) \
    { name_, def_, desc_, flags_, CVT_STRING, 0, 0, NULL, cb_ }

#define CVAR_BOOL_CB( name_, def_, flags_, desc_, cb_ ) \
    { name_, def_, desc_, flags_, CVT_BOOL, 0, 0, NULL, cb_ }

#define CVAR_INT_CB( name_, def_, flags_, desc_, lo_, hi_, cb_ ) \
    { name_, def_, desc_, flags_, CVT_INT, (float)(lo_), (float)(hi_), NULL, cb_ }

#define CVAR_FLOAT_CB( name_, def_, flags_, desc_, lo_, hi_, cb_ ) \
    { name_, def_, desc_, flags_, CVT_FLOAT, (lo_), (hi_), NULL, cb_ }

#define CVAR_ENUM_CB( name_, def_, flags_, desc_, vals_, cb_ ) \
    { name_, def_, desc_, flags_, CVT_ENUM, 0, 0, (vals_), cb_ }

typedef int	cvarHandle_t;

// the modules that run in the virtual machine can't access the cvar_t directly,
// so they must ask for structured updates
typedef struct {
	cvarHandle_t	handle;
	int			modificationCount;
	float		value;
	int			integer;
	char		string[MAX_CVAR_VALUE_STRING];
} vmCvar_t;

/*
==============================================================

COLLISION DETECTION

==============================================================
*/

#include "surfaceflags.h"			// shared with the q3map utility

// plane types are used to speed some tests
// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2
#define	PLANE_NON_AXIAL	3


/*
=================
PlaneTypeForNormal
=================
*/

#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL) ) )

// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s {
	vec3_t	normal;
	float	dist;
	byte	type;			// for fast side tests: 0,1,2 = axial, 3 = nonaxial
	byte	signbits;		// signx + (signy<<1) + (signz<<2), used as lookup during collision
	byte	pad[2];
} cplane_t;


// a trace is returned when a box is swept through the world
typedef struct {
	qboolean	allsolid;	// if true, plane is not valid
	qboolean	startsolid;	// if true, the initial point was in a solid area
	float		fraction;	// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;		// final position
	cplane_t	plane;		// surface normal at impact, transformed to world space
	int			surfaceFlags;	// surface hit
	int			contents;	// contents on other side of surface hit
	int			entityNum;	// entity the contacted surface is a part of
} trace_t;

// trace type for collision detection
typedef enum {
	TT_NONE,
	TT_AABB,
	TT_CAPSULE,
	TT_BISPHERE
} traceType_t;

// trace->entityNum can also be 0 to (MAX_GENTITIES-1)
// or ENTITYNUM_NONE, ENTITYNUM_WORLD


// markfragments are returned by R_MarkFragments()
typedef struct {
	int		firstPoint;
	int		numPoints;
} markFragment_t;



typedef struct {
	vec3_t		origin;
	vec3_t		axis[3];
} orientation_t;

//=====================================================================


// in order from highest priority to lowest
// if none of the catchers are active, bound key strings will be executed
#define KEYCATCH_CONSOLE    0x0001
#define KEYCATCH_UI         0x0002
#define KEYCATCH_MESSAGE    0x0004
#define KEYCATCH_CGAME      0x0008


// sound channels
// channel 0 never willingly overrides
// other channels will always override a playing sound on that channel
typedef enum {
	CHAN_AUTO,
	CHAN_LOCAL,		// menu sounds, etc
	CHAN_WEAPON,
	CHAN_VOICE,
	CHAN_ITEM,
	CHAN_BODY,
	CHAN_LOCAL_SOUND,	// chat messages, etc
	CHAN_ANNOUNCER		// announcer voices, etc
} soundChannel_t;


/*
========================================================================

  ELEMENTS COMMUNICATED ACROSS THE NET

========================================================================
*/

#define	ANGLE2SHORT(x)	((int)((x)*65536/360) & 65535)
#define	SHORT2ANGLE(x)	((x)*(360.0/65536))

#define	SNAPFLAG_RATE_DELAYED	1
#define	SNAPFLAG_NOT_ACTIVE		2	// snapshot used during connection and for zombies
#define SNAPFLAG_SERVERCOUNT	4	// toggled every map_restart so transitions can be detected

//
// per-level limits
//
#define	MAX_CLIENTS			64		// absolute limit
#define MAX_LOCATIONS		64

#define	GENTITYNUM_BITS		10		// don't need to send any more
#define	MAX_GENTITIES		(1<<GENTITYNUM_BITS)

// entitynums are communicated with GENTITY_BITS, so any reserved
// values that are going to be communcated over the net need to
// also be in this range
#define	ENTITYNUM_NONE		(MAX_GENTITIES-1)
#define	ENTITYNUM_WORLD		(MAX_GENTITIES-2)
#define	ENTITYNUM_MAX_NORMAL	(MAX_GENTITIES-2)


#define	MAX_MODELS			256		// these are sent over the net as 8 bits
#define	MAX_SOUNDS			256		// so they cannot be blindly increased


#define	MAX_CONFIGSTRINGS	1024

// these are the only configstrings that the system reserves, all the
// other ones are strictly for servergame to clientgame communication
#define	CS_SERVERINFO		0		// an info string with all the serverinfo cvars
#define	CS_SYSTEMINFO		1		// an info string for server system to client system configuration (timescale, etc)

#define	RESERVED_CONFIGSTRINGS	2	// game can't modify below this, only the system can

#define	MAX_GAMESTATE_CHARS	16000
typedef struct {
	int			stringOffsets[MAX_CONFIGSTRINGS];
	char		stringData[MAX_GAMESTATE_CHARS];
	int			dataCount;
} gameState_t;

//=========================================================

// bit field limits
#define	MAX_STATS				16
#define	MAX_PERSISTANT			24		// increased from 20 for observer/web stat slots
#define	MAX_POWERUPS			16
#define	MAX_WEAPONS				16
#define	MAX_ATTACKS				32

#define	MAX_PS_EVENTS			2

#define PS_PMOVEFRAMECOUNTBITS	6

// playerState_t is the information needed by both the client and server
// to predict player motion and actions
// nothing outside of pmove should modify these, or some degree of prediction error
// will occur

// you can't add anything to this without modifying the code in msg.c

// playerState_t is a full superset of entityState_t as it is used by players,
// so if a playerState_t is transmitted, the entityState_t can be fully derived
// from it.
typedef struct playerState_s {
	int			commandTime;	// cmd->serverTime of last executed command
	int			pm_type;
	int			bobCycle;		// for view bobbing and footstep generation
	int			pm_flags;		// ducked, jump_held, etc
	int			pm_time;

	vec3_t		origin;
	vec3_t		velocity;
	int			weaponTime;
	int			gravity;
	int			speed;
	int			delta_angles[3];	// add to command angles to get view direction
									// changed by spawns, rotating objects, and teleporters

	int			groundEntityNum;// ENTITYNUM_NONE = in air

	int			legsTimer;		// don't change low priority animations until this runs out
	int			legsAnim;		// mask off ANIM_TOGGLEBIT

	int			torsoTimer;		// don't change low priority animations until this runs out
	int			torsoAnim;		// mask off ANIM_TOGGLEBIT

	int			movementDir;	// a number 0 to 7 that represents the relative angle
								// of movement to the view angle (axial and diagonals)
								// when at rest, the value will remain unchanged
								// used to twist the legs during strafing

	vec3_t		grapplePoint;	// location of grapple to pull towards if PMF_GRAPPLE_PULL

	int			eFlags;			// copied to entityState_t->eFlags

	int			eventSequence;	// pmove generated events
	int			events[MAX_PS_EVENTS];
	int			eventParms[MAX_PS_EVENTS];

	int			externalEvent;	// events set on player from another source
	int			externalEventParm;
	int			externalEventTime;

	int			clientNum;		// ranges from 0 to MAX_CLIENTS-1
	int			weapon;			// copied to entityState_t->weapon
	int			weaponstate;
	int			burstRoundsRemaining;	// rounds left in current burst fire
	int			chargeStartTime;		// time when alt-fire charge began (0 = not charging)
	int			cooldownEndTime;		// time when weapon cooldown ends (0 = no cooldown)
	int			doubleBlastState;		// state for shotgun double-blast (0 = idle, 1 = first blast fired, waiting for second)

	vec3_t		viewangles;		// for fixed views
	int			viewheight;

	// damage feedback
	int			damageEvent;	// when it changes, latch the other parms
	int			damageYaw;
	int			damagePitch;
	int			damageCount;

	int			stats[MAX_STATS];
	int			persistant[MAX_PERSISTANT];	// stats that aren't cleared on death
	int			powerups[MAX_POWERUPS];	// level.time that the powerup runs out
	int			ammo[MAX_WEAPONS];

	int			generic1;
	int			loopSound;
	int			jumppad_ent;	// jumppad entity hit this frame

	// not communicated over the net at all
	int			ping;			// server to game info for scoreboard
	int			pmove_framecount;	// FIXME: don't transmit over the network
	int			jumppad_frame;
	int			entityEventSequence;
} playerState_t;


//====================================================================


//
// usercmd_t->button bits, many of which are generated by the client system,
// so they aren't game/cgame only definitions
//
#define	BUTTON_ATTACK_PRI	1
#define	BUTTON_ATTACK_SEC	2			// secondary fire (bit 1, slot next to +attack)
#define	BUTTON_TALK			4096		// displays talk balloon and disables actions
#define	BUTTON_USE_HOLDABLE	4
#define	BUTTON_GESTURE		8
#define	BUTTON_WALKING		16			// walking can't just be inferred from MOVE_RUN
										// because a key pressed late in the frame will
										// only generate a small move value for that frame
										// walking will use different animations and
										// won't generate footsteps
#define BUTTON_AFFIRMATIVE	32
#define	BUTTON_NEGATIVE		64

#define BUTTON_GETFLAG		128
#define BUTTON_GUARDBASE	256
#define BUTTON_PATROL		512
#define BUTTON_FOLLOWME		1024

#define	BUTTON_ANY			2048			// any key whatsoever

#define	MOVE_RUN			120			// if forwardmove or rightmove are >= MOVE_RUN,
										// then BUTTON_WALKING should be set

// usercmd_t is sent to the server each client frame
typedef struct usercmd_s {
	int				serverTime;
	int				angles[3];
	int 			buttons;
	byte			weapon;           // weapon 
	signed char	forwardmove, rightmove, upmove;
} usercmd_t;

//===================================================================

// if entityState->solid == SOLID_BMODEL, modelindex is an inline model number
#define	SOLID_BMODEL	0xffffff

typedef enum {
	TR_STATIONARY,
	TR_INTERPOLATE,				// non-parametric, but interpolate between snapshots
	TR_LINEAR,
	TR_LINEAR_STOP,
	TR_SINE,					// value = base + sin( time / duration ) * delta
	TR_GRAVITY,
	// q3now: additional trajectory types used by bg_misc.c
	TR_GRAVITY_DOUBLE,			// double strength gravity
	TR_ACCELERATE,				// accelerating linear
	TR_SMALL_GRAVITY,			// half strength gravity
	TR_ORBITAL					// gravity + centripetal
} trType_t;

typedef struct {
	trType_t	trType;
	int		trTime;
	int		trDuration;			// if non 0, trTime + trDuration = stop time
	vec3_t	trBase;
	vec3_t	trDelta;			// velocity, etc
} trajectory_t;

// entityState_t is the information conveyed from the server
// in an update message about entities that the client will
// need to render in some way
// Different eTypes may use the information in different ways
// The messages are delta compressed, so it doesn't really matter if
// the structure size is fairly large

typedef struct entityState_s {
	int		number;			// entity index
	int		eType;			// entityType_t
	int		eFlags;

	trajectory_t	pos;	// for calculating position
	trajectory_t	apos;	// for calculating angles

	int		time;
	int		time2;

	vec3_t	origin;
	vec3_t	origin2;

	vec3_t	angles;
	vec3_t	angles2;

	int		otherEntityNum;	// shotgun sources, etc
	int		otherEntityNum2;

	int		groundEntityNum;	// ENTITYNUM_NONE = in air

	int		constantLight;	// r + (g<<8) + (b<<16) + (intensity<<24)
	int		loopSound;		// constantly loop this sound

	int		modelindex;
	int		modelindex2;
	int		clientNum;		// 0 to (MAX_CLIENTS - 1), for players and corpses
	int		frame;

	int		solid;			// for client side prediction, trap_linkentity sets this properly

	int		event;			// impulse events -- muzzle flashes, footsteps, etc
	int		eventParm;

	// for players
	int		powerups;		// bit flags
	int		weapon;			// determines weapon and flash model, etc
	int		pType;			// projectileType_t — ET_MISSILE visual discriminator
	int		legsAnim;		// mask off ANIM_TOGGLEBIT
	int		torsoAnim;		// mask off ANIM_TOGGLEBIT

	int		generic1;
} entityState_t;

typedef enum {
	CA_UNINITIALIZED,
	CA_DISCONNECTED, 	// not talking to a server
	CA_AUTHORIZING,		// not used any more, was checking cd key 
	CA_CONNECTING,		// sending request packets to the server
	CA_CHALLENGING,		// sending challenge packets to the server
	CA_CONNECTED,		// netchan_t established, getting gamestate
	CA_LOADING,			// only during cgame initialization, never during main loop
	CA_PRIMED,			// got gamestate, waiting for first frame
	CA_ACTIVE,			// game views should be displayed
	CA_CINEMATIC		// playing a cinematic or a static pic, not connected to a server
} connstate_t;

// font support 

#define GLYPH_START 0
#define GLYPH_END 255
#define GLYPH_CHARSTART 32
#define GLYPH_CHAREND 127
#define GLYPHS_PER_FONT GLYPH_END - GLYPH_START + 1
typedef struct {
  int height;       // number of scan lines
  int top;          // top of glyph in buffer
  int bottom;       // bottom of glyph in buffer
  int pitch;        // width for copying
  int xSkip;        // x adjustment
  int imageWidth;   // width of actual image
  int imageHeight;  // height of actual image
  float s;          // x offset in image where glyph starts
  float t;          // y offset in image where glyph starts
  float s2;
  float t2;
  qhandle_t glyph;  // handle to the shader with the glyph
  char shaderName[32];
} glyphInfo_t;

typedef struct {
  glyphInfo_t glyphs [GLYPHS_PER_FONT];
  float glyphScale;
  char name[MAX_QPATH];
} fontInfo_t;

#define Square(x) ((x)*(x))

// real time
//=============================================


typedef struct qtime_s {
	int tm_sec;     /* seconds after the minute - [0,59] */
	int tm_min;     /* minutes after the hour - [0,59] */
	int tm_hour;    /* hours since midnight - [0,23] */
	int tm_mday;    /* day of the month - [1,31] */
	int tm_mon;     /* months since January - [0,11] */
	int tm_year;    /* years since 1900 */
	int tm_wday;    /* days since Sunday - [0,6] */
	int tm_yday;    /* days since January 1 - [0,365] */
	int tm_isdst;   /* daylight savings time flag */
} qtime_t;


// server browser sources
#define AS_LOCAL		0
#define AS_GLOBAL		2
#define AS_FAVORITES	3


// cinematic states
typedef enum {
	FMV_IDLE,
	FMV_PLAY,		// play
	FMV_EOF,		// all other conditions, i.e. stop/EOF/abort
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;

typedef enum _flag_status {
	FLAG_ATBASE = 0,
	FLAG_TAKEN,			// CTF
	FLAG_TAKEN_RED,		// One Flag CTF
	FLAG_TAKEN_BLUE,	// One Flag CTF
	FLAG_DROPPED
} flagStatus_t;


#define	MAX_GLOBAL_SERVERS			4096
#define	MAX_OTHER_SERVERS			128
#define MAX_PINGREQUESTS			32
#define MAX_SERVERSTATUSREQUESTS	16

#define SAY_ALL		0
#define SAY_TEAM	1
#define SAY_TELL	2

#define LERP( a, b, w ) ( ( a ) * ( 1.0f - ( w ) ) + ( b ) * ( w ) )
#define LUMA( red, green, blue ) ( 0.2126f * ( red ) + 0.7152f * ( green ) + 0.0722f * ( blue ) )

#endif	// __Q_SHARED_H
