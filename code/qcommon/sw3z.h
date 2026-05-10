/*
 * sw3z.h -- SW3Z archive format reader
 *
 * SW3Z is a read-optimized game asset archive format designed to replace
 * PK3/ZIP containers in id Tech 3 derived engines.
 * Spec: https://github.com/eser/sw3z/blob/main/SPEC.md
 *
 * File layout:
 *   [Header 24B] [Index N*40B] [String Table] [Data Blob] [Signature?]
 *
 * This implementation supports:
 *   - LZ4 Frame Format decompression (compression == 0x01)
 *   - CRC32C (Castagnoli) verification per entry
 *   - Uncompressed entries (compression == 0x00)
 *
 * Not implemented (deferred):
 *   - Zstd decompression (compression == 0x02)
 *   - Ed25519 signature verification
 */

#ifndef SW3Z_H
#define SW3Z_H

#include "q_shared.h"
#include "qcommon.h"	/* fileInPack_t */

/* ── format constants ────────────────────────────────────────────────── */

#define SW3Z_MAGIC			0x5A335753UL	/* "SW3Z" little-endian */
#define SW3Z_VERSION		1
#define SW3Z_HEADER_SIZE	24
#define SW3Z_ENTRY_SIZE		40

#define SW3Z_COMP_NONE		0x00
#define SW3Z_COMP_LZ4		0x01
#define SW3Z_COMP_ZSTD		0x02	/* not implemented */

#define SW3Z_FLAG_EXEC		0x01
#define SW3Z_FLAG_SYMLINK	0x02
#define SW3Z_FLAG_ALIGNED	0x04

#define SW3Z_SIGN_NONE		0x00
#define SW3Z_SIGN_ED25519	0x01	/* not implemented */

/* FNV-1a 64-bit constants */
#define SW3Z_FNV_OFFSET		0xcbf29ce484222325ULL
#define SW3Z_FNV_PRIME		0x00000100000001B3ULL

/* ── on-disk structures ──────────────────────────────────────────────── */

/*
 * Header (24 bytes):
 *   offset  size  field
 *   0       4     magic             "SW3Z"
 *   4       2     version           currently 1
 *   6       2     flags             bits 0-1: signing method
 *   8       4     entry_count
 *   12      4     string_table_size
 *   16      8     data_offset       byte offset where asset data begins
 */
typedef struct {
	unsigned int	magic;
	unsigned short	version;
	unsigned short	flags;
	unsigned int	entryCount;
	unsigned int	stringTableSize;
	/* data_offset is 8 bytes — read as two 32-bit halves for portability */
	unsigned int	dataOffsetLo;
	unsigned int	dataOffsetHi;
} sw3zHeader_t;

/*
 * Index entry (40 bytes):
 *   offset  size  field
 *   0       8     path_hash          FNV-1a 64-bit
 *   8       4     string_offset      into string table
 *   12      4     string_length      byte length (no NUL)
 *   16      8     data_offset        from file start
 *   24      4     compressed_size
 *   28      4     uncompressed_size
 *   32      4     crc32c             Castagnoli
 *   36      1     compression        0x00/0x01/0x02
 *   37      1     flags
 *   38      1     alignment          power-of-2 exponent
 *   39      1     reserved           must be 0
 */
typedef struct {
	unsigned int	pathHashLo;
	unsigned int	pathHashHi;
	unsigned int	stringOffset;
	unsigned int	stringLength;
	unsigned int	dataOffsetLo;
	unsigned int	dataOffsetHi;
	unsigned int	compressedSize;
	unsigned int	uncompressedSize;
	unsigned int	crc32c;
	byte			compression;
	byte			flags;
	byte			alignment;
	byte			reserved;
} sw3zEntry_t;

/*
 * sw3zPack_t has been merged into pack_t (files.c).
 * SW3Z-specific fields live inside pack_t behind #if FEAT_SW3Z.
 * The functions below operate on pack_t directly.
 */

/* Forward-declare pack_t so sw3z.h can be included before files.c defines it. */
struct pack_s;

/* ── reader API ──────────────────────────────────────────────────────── */

/*
 * One-shot module init. Builds the static CRC32C lookup table eagerly
 * so SW3Z_CRC32C is a pure function over the table at all call sites.
 * Must be called once before any other SW3Z_* function. Idempotent;
 * called from FS_InitFilesystem.
 *
 * (Replaces the prior lazy first-call init inside SW3Z_CRC32C, which
 * was a thread-safety footgun if async loading is ever added.)
 */
void		SW3Z_Init( void );

/*
 * Open an SW3Z archive.  Reads header, index, and string table.
 * Builds fileInPack_t hash table for FS compatibility.
 * Returns NULL on failure (logs a warning).
 */
struct pack_s	*SW3Z_LoadArchive( const char *filename );

/*
 * Close an SW3Z archive and free all associated memory.
 */
void		SW3Z_CloseArchive( struct pack_s *pack );

/*
 * Read and decompress a single entry from the archive.
 * Returns uncompressed size on success, -1 on failure.
 * buf must be at least entry->uncompressedSize bytes.
 */
int			SW3Z_ReadEntry( struct pack_s *pack, int entryIndex,
							void *buf, int bufSize );

/*
 * Compute FNV-1a 64-bit hash with SW3Z path normalization
 * (lowercase, forward slashes).
 */
uint64_t	SW3Z_FNV1a64( const char *path );

/*
 * Software CRC32C (Castagnoli polynomial 0x82F63B78).
 */
uint32_t	SW3Z_CRC32C( const void *data, size_t len );

#endif /* SW3Z_H */
