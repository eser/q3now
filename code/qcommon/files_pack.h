/*
 * files_pack.h -- pack_t definition shared between files.c and sw3z.c
 *
 * pack_t is the unified archive structure for both PK3 (ZIP) and SW3Z
 * archive formats.  The packType_t field discriminates at runtime.
 */

#ifndef FILES_PACK_H
#define FILES_PACK_H

#include "q_shared.h"
#include "qcommon.h"		/* fileInPack_t, fileOffset_t, fileTime_t */
#include "q_feats.h"
#include "unzip.h"			/* unzFile */

#if FEAT_SW3Z
#include "sw3z.h"			/* sw3zEntry_t */
#endif

/* ── compile-time feature toggles (must match files.c) ──────────────── */

#define USE_PK3_CACHE
#define USE_PK3_CACHE_FILE

#define USE_HANDLE_CACHE
#define MAX_CACHED_HANDLES 384

/* ── pack type discriminator ────────────────────────────────────────── */

typedef enum {
	PACK_PK3
#if FEAT_SW3Z
	, PACK_SW3Z
#endif
} packType_t;

/* ── unified pack structure ─────────────────────────────────────────── */

typedef struct pack_s {
	packType_t		type;
	char			*pakFilename;				// c:\quake3\baseq3\pak0.pk3
	char			*pakBasename;				// pak0
	const char		*pakGamename;				// baseq3
#if FEAT_SW3Z
	union {
		unzFile		zip;
		FILE		*file;
	} handle;
#else
	unzFile			handle;						// handle to zip file
#endif
	int				checksum;					// regular checksum
	int				pure_checksum;				// checksum for pure
	int				numfiles;					// number of files in pk3
	int				referenced;					// referenced file flags
	qboolean		exclude;					// found in \fs_excludeReference list
	int				hashSize;					// hash table size (power of 2)
	fileInPack_t*	*hashTable;					// hash table
	fileInPack_t*	buildBuffer;				// buffer with the filenames etc.
	int				index;

	int				handleUsed;

#if FEAT_SW3Z
	unsigned int	entryCount;
	sw3zEntry_t		*entries;
	char			*stringTable;
	unsigned int	stringTableSize;
	unsigned long	dataOffset;
#endif

#ifdef USE_HANDLE_CACHE
	struct pack_s	*next_h;					// double-linked list of unreferenced paks with open file handles
	struct pack_s	*prev_h;
#endif

	// caching subsystem
#ifdef USE_PK3_CACHE
	unsigned int	namehash;
	fileOffset_t	size;
	fileTime_t		mtime;
	fileTime_t		ctime;
	qboolean		touched;
	struct pack_s	*next;
	struct pack_s	*prev;
	int				checksumFeed;
	int				*headerLongs;
	int				numHeaderLongs;
#endif
} pack_t;

/* Convenience macros to access the handle union.
 * When FEAT_SW3Z=1 the handle is a union; pk3 code uses .zip.
 * When FEAT_SW3Z=0 the handle is a plain unzFile.
 */
#if FEAT_SW3Z
#define PACK_ZIP_HANDLE(p)	((p)->handle.zip)
#define PACK_FILE_HANDLE(p)	((p)->handle.file)
#else
#define PACK_ZIP_HANDLE(p)	((p)->handle)
#endif

#endif /* FILES_PACK_H */
