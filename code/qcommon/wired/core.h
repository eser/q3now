// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
wired/core.h — foundational types and limits used by every wired module.

This header has ZERO subsystem APIs. No Cvar, no Cmd, no FS, no logger, no
parser, no hash, no math helpers, no protocol structs. It only defines the
universal vocabulary every translation unit needs to start from:

  - Boolean / primitive aliases:  byte, qboolean
  - Resource handles:             qhandle_t, sfxHandle_t, fileHandle_t,
                                  clipHandle_t
  - Vectors / fixed-point:        vec_t, vec[2-5]_t, quat_t, fixed*_t
  - Path / string size limits:    MAX_QPATH, MAX_VFS_PATH, MAX_OSPATH,
                                  MAX_STRING_CHARS, MAX_TOKEN_CHARS, …
  - Color packing:                color4ub_t, floatint_u
  - Numeric / array macros:       ARRAY_LEN, PAD, MAX_QINT, NULL, M_PI

Two headers build on top of this one:
  - wired/protocol.h    — engine ↔ game wire contract
  - qcommon/q_shared.h  — engine-internal API surface (cvar/cmd/fs/…)

Game modules see this header transitively through bg_public.h → wired/protocol.h.
===========================================================================
*/

#ifndef WIRED_CORE_H
#define WIRED_CORE_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>     /* for PATH_MAX */

/* ── Boolean and primitive aliases ────────────────────────────────────── */
typedef unsigned char byte;
typedef enum { qfalse = 0, qtrue } qboolean;

/* ── Resource handles ─────────────────────────────────────────────────── */
typedef int		qhandle_t;
typedef int		sfxHandle_t;
typedef int		fileHandle_t;
typedef int		clipHandle_t;

/* ── Color packing ────────────────────────────────────────────────────── */
typedef union {
	byte rgba[4];
	uint32_t u32;
} color4ub_t;

/* Float/int bit-cast — needed by the endian-swap helpers in q_shared.h. */
typedef union floatint_u { int32_t i; uint32_t u; float f; unsigned char b[4]; } floatint_t;

/* ── Vectors and fixed-point ──────────────────────────────────────────── */
typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];
typedef vec_t quat_t[4];

typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;

/* ── Padding / alignment ──────────────────────────────────────────────── */
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

/* ── Numeric / array helpers ──────────────────────────────────────────── */
#define	MAX_QINT			0x7fffffff
#define	MIN_QINT			(-MAX_QINT-1)
#define	MAX_UINT			((unsigned)(~0))

#define ARRAY_LEN(x)		(sizeof(x) / sizeof(*(x)))
#define STRARRAY_LEN(x)		(ARRAY_LEN(x) - 1)

/* ── Angle indexes for vec3_t-as-euler-angles ─────────────────────────── */
#define	PITCH				0		// up / down
#define	YAW					1		// left / right
#define	ROLL				2		// fall over

/* ── String length limits — used by string utilities and the parser ───── */
#define	MAX_STRING_CHARS	1024	// max length of a string passed to Cmd_TokenizeString
#define	MAX_STRING_TOKENS	1024	// max tokens resulting from Cmd_TokenizeString
#define	MAX_TOKEN_CHARS		1024	// max length of an individual token

#define	MAX_INFO_STRING		1024
#define	MAX_INFO_KEY		1024
#define	MAX_INFO_VALUE		1024
#define MAX_INFO_TOKENS		((MAX_INFO_STRING/3)+2)
#define MAX_USERINFO_LENGTH	(MAX_INFO_STRING-13)	// incl. 'connect ""' or 'userinfo ""' wrap

#define	BIG_INFO_STRING		8192	// used for system info key only
#define	BIG_INFO_KEY		8192
#define	BIG_INFO_VALUE		8192

typedef struct {
	char        buffer[MAX_INFO_STRING];
	const char  *keys[MAX_INFO_TOKENS];
	const char  *values[MAX_INFO_TOKENS];
	int         count;
} InfoTokens;

/* ── Path limits ──────────────────────────────────────────────────────── */
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

/* ── Math constants ───────────────────────────────────────────────────── */
#ifndef M_PI
#define M_PI		3.14159265358979323846f	// matches value in gcc v2 math.h
#endif

#ifndef M_LN2
#define M_LN2		0.693147180559945309417f
#endif

#endif /* WIRED_CORE_H */
