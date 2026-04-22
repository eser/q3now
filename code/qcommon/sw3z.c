/*
 * sw3z.c -- SW3Z archive format reader
 *
 * Read-only implementation of the SW3Z archive format.
 * See sw3z.h for format documentation and API.
 *
 * Data flow:
 *   SW3Z_LoadArchive:
 *     fopen → read header → validate → read index → read string table
 *     → validate bounds → build fileInPack_t hash table → return pack_t
 *
 *   SW3Z_ReadEntry:
 *     validate index → fseek to data → fread compressed blob
 *     → LZ4F_decompress (or raw copy) → CRC32C verify → return size
 */

#include "q_shared.h"
#include "qcommon.h"
#include "q_feats.h"
#include "wired/core/vfs/files_pack.h"

#include <lz4.h>
#include <lz4frame.h>

/* ── CRC32C (Castagnoli) ─────────────────────────────────────────────
 *
 * Polynomial: 0x1EDC6F41 (reflected: 0x82F63B78)
 * NOT the same as CRC32 (ISO 3309) used in ZIP/PK3.
 */

static uint32_t sw3z_crc32c_table[256];
static qboolean sw3z_crc32c_init = qfalse;

static void SW3Z_InitCRC32C( void ) {
	for ( int i = 0; i < 256; i++ ) {
		uint32_t crc = (uint32_t)i;
		for ( int j = 0; j < 8; j++ ) {
			if ( crc & 1 )
				crc = (crc >> 1) ^ 0x82F63B78U;
			else
				crc >>= 1;
		}
		sw3z_crc32c_table[i] = crc;
	}
	sw3z_crc32c_init = qtrue;
}

uint32_t SW3Z_CRC32C( const void *data, size_t len ) {
	const byte *p = (const byte *)data;
	uint32_t crc = 0xFFFFFFFFU;

	if ( !sw3z_crc32c_init )
		SW3Z_InitCRC32C();

	for ( size_t i = 0; i < len; i++ )
		crc = sw3z_crc32c_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);

	return crc ^ 0xFFFFFFFFU;
}


/* ── FNV-1a 64-bit hash ──────────────────────────────────────────────
 *
 * Path normalization per SW3Z spec:
 *   - Lowercase ASCII
 *   - Backslash → forward slash
 */
uint64_t SW3Z_FNV1a64( const char *path ) {
	uint64_t hash = SW3Z_FNV_OFFSET;
	while ( *path ) {
		byte c = (byte)(*path++);
		if ( c >= 'A' && c <= 'Z' )
			c += 32;
		if ( c == '\\' )
			c = '/';
		hash ^= (uint64_t)c;
		hash *= SW3Z_FNV_PRIME;
	}
	return hash;
}


/* ── SW3Z_LoadArchive ────────────────────────────────────────────────
 *
 * Opens an SW3Z file, reads and validates the header/index/string table,
 * then builds a fileInPack_t hash table for FS compatibility.
 *
 * Uses a single Z_TagMalloc allocation for the entire pack structure
 * (same pattern as FS_LoadZipFile in files.c).
 */
pack_t *SW3Z_LoadArchive( const char *filename ) {
	FILE *f = fopen( filename, "rb" );
	if ( !f ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: cannot open '%s'\n", filename );
		return NULL;
	}

	/* get file size for validation */
	fseek( f, 0, SEEK_END );
	long fileSize = ftell( f );
	fseek( f, 0, SEEK_SET );

	if ( fileSize < SW3Z_HEADER_SIZE ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: '%s' too small (%ld bytes)\n",
			filename, fileSize );
		fclose( f );
		return NULL;
	}

	/* ── read and validate header ── */
	byte headerBuf[SW3Z_HEADER_SIZE];
	if ( fread( headerBuf, 1, SW3Z_HEADER_SIZE, f ) != SW3Z_HEADER_SIZE ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: failed to read header from '%s'\n", filename );
		fclose( f );
		return NULL;
	}

	/* parse header fields with endian conversion */
	sw3zHeader_t header;
	header.magic          = LittleLong( *(unsigned int *)( headerBuf + 0 ) );
	header.version        = LittleShort( *(unsigned short *)( headerBuf + 4 ) );
	header.flags          = LittleShort( *(unsigned short *)( headerBuf + 6 ) );
	header.entryCount     = LittleLong( *(unsigned int *)( headerBuf + 8 ) );
	header.stringTableSize = LittleLong( *(unsigned int *)( headerBuf + 12 ) );
	header.dataOffsetLo   = LittleLong( *(unsigned int *)( headerBuf + 16 ) );
	header.dataOffsetHi   = LittleLong( *(unsigned int *)( headerBuf + 20 ) );

	if ( header.magic != SW3Z_MAGIC ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: '%s' bad magic 0x%08X\n",
			filename, header.magic );
		fclose( f );
		return NULL;
	}

	if ( header.version != SW3Z_VERSION ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: '%s' unsupported version %d\n",
			filename, header.version );
		fclose( f );
		return NULL;
	}

	/* reconstruct 64-bit data offset — treat >2GB as error */
	if ( header.dataOffsetHi != 0 ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: '%s' data offset exceeds 2GB\n", filename );
		fclose( f );
		return NULL;
	}
	unsigned long dataOffset = (unsigned long)header.dataOffsetLo;

	/* ── security: validate entryCount against file size ── */
	unsigned int indexSize = header.entryCount * SW3Z_ENTRY_SIZE;
	if ( header.entryCount > 0 &&
		indexSize / SW3Z_ENTRY_SIZE != header.entryCount ) {
		/* integer overflow */
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: '%s' entryCount overflow\n", filename );
		fclose( f );
		return NULL;
	}

	if ( (long)( SW3Z_HEADER_SIZE + indexSize + header.stringTableSize ) > fileSize ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: '%s' index/strings exceed file size\n", filename );
		fclose( f );
		return NULL;
	}

	/* ── read index ── */
	sw3zEntry_t *entries = NULL;
	if ( header.entryCount > 0 ) {
		entries = (sw3zEntry_t *)Z_Malloc( indexSize );
		if ( fread( entries, 1, indexSize, f ) != indexSize ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: '%s' failed to read index\n", filename );
			Z_Free( entries );
			fclose( f );
			return NULL;
		}
	}

	/* ── read string table ── */
	char *stringTable = NULL;
	if ( header.stringTableSize > 0 ) {
		stringTable = (char *)Z_Malloc( header.stringTableSize );
		if ( fread( stringTable, 1, header.stringTableSize, f ) != header.stringTableSize ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: '%s' failed to read string table\n", filename );
			if ( entries ) Z_Free( entries );
			Z_Free( stringTable );
			fclose( f );
			return NULL;
		}
	}

	/* ── validate all entries and compute total name buffer size ── */
	int numValidFiles = 0;
	int nameLen = 0;
	for ( int i = 0; i < (int)header.entryCount; i++ ) {
		sw3zEntry_t *e = &entries[i];

		/* endian convert entry fields */
		e->stringOffset     = LittleLong( e->stringOffset );
		e->stringLength     = LittleLong( e->stringLength );
		e->compressedSize   = LittleLong( e->compressedSize );
		e->uncompressedSize = LittleLong( e->uncompressedSize );
		e->crc32c           = LittleLong( e->crc32c );
		e->dataOffsetLo     = LittleLong( e->dataOffsetLo );
		e->dataOffsetHi     = LittleLong( e->dataOffsetHi );
		/* pathHash, compression, flags, alignment are byte-order independent or single bytes */

		/* security: string table bounds check */
		if ( e->stringOffset + e->stringLength > header.stringTableSize ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: '%s' entry %d string out of bounds\n",
				filename, i );
			if ( entries ) Z_Free( entries );
			if ( stringTable ) Z_Free( stringTable );
			fclose( f );
			return NULL;
		}

		/* skip directory entries (uncompressedSize == 0 and compressedSize == 0) */
		if ( e->uncompressedSize == 0 && e->compressedSize == 0 )
			continue;

		numValidFiles++;
		nameLen += e->stringLength + 1; /* +1 for NUL terminator */
	}

	/* ── compute hash table size (power of 2, same logic as FS_LoadZipFile) ── */
	int hashSize;
	for ( hashSize = 1; hashSize < numValidFiles; hashSize <<= 1 )
		;

	/* ── allocate pack structure ── */
	long allocSize = sizeof( pack_t )
		+ hashSize * (long)sizeof( fileInPack_t * )
		+ numValidFiles * (long)sizeof( fileInPack_t )
		+ nameLen
		+ (long)strlen( filename ) + 1;

	byte *mem = (byte *)Z_TagMalloc( (int)allocSize, TAG_SEARCH_PACK );
	memset( mem, 0, (int)allocSize );

	pack_t *pack = (pack_t *)mem;
	mem += sizeof( pack_t );

	fileInPack_t **hashTable = (fileInPack_t **)mem;
	mem += hashSize * sizeof( fileInPack_t * );

	fileInPack_t *buildBuffer = (fileInPack_t *)mem;
	mem += numValidFiles * sizeof( fileInPack_t );

	char *nameBuffer = (char *)mem;
	mem += nameLen;

	pack->pakFilename = (char *)mem;
	strcpy( pack->pakFilename, filename );

	/* extract base name */
	{
		const char *p = filename;
		const char *base = p;
		while ( *p ) {
			if ( *p == '/' || *p == '\\' )
				base = p + 1;
			p++;
		}
		pack->pakBasename = pack->pakFilename + (base - filename);
	}

	pack->type           = PACK_SW3Z;
	PACK_FILE_HANDLE(pack) = f;
	pack->entryCount     = header.entryCount;
	pack->entries        = entries;
	pack->stringTable    = stringTable;
	pack->stringTableSize = header.stringTableSize;
	pack->dataOffset     = dataOffset;
	pack->hashSize       = hashSize;
	pack->hashTable      = hashTable;
	pack->buildBuffer    = buildBuffer;
	pack->numfiles       = numValidFiles;
	pack->referenced     = 0;
	pack->exclude        = qfalse;

	/* ── build fileInPack_t hash table ── */
	{
		int fi = 0;
		for ( int i = 0; i < (int)header.entryCount; i++ ) {
			sw3zEntry_t *e = &entries[i];

			/* skip directory entries */
			if ( e->uncompressedSize == 0 && e->compressedSize == 0 )
				continue;

			/* copy filename from string table (NUL-terminate) */
			buildBuffer[fi].name = nameBuffer;
			memcpy( nameBuffer, stringTable + e->stringOffset, e->stringLength );
			nameBuffer[e->stringLength] = '\0';
			nameBuffer += e->stringLength + 1;

			/* store SW3Z entry index in pos, uncompressed size in size */
			buildBuffer[fi].pos  = (unsigned long)i;
			buildBuffer[fi].size = (unsigned long)e->uncompressedSize;

			/* insert into hash table (chaining, same as FS_LoadZipFile) */
			unsigned long hash = Com_GenerateHashValue( buildBuffer[fi].name, (unsigned int)hashSize );
			buildBuffer[fi].next = hashTable[hash];
			hashTable[hash] = &buildBuffer[fi];

			fi++;
		}
	}

	/* ── cache subsystem fields ── */
	Sys_GetFileStats( filename, &pack->size, &pack->mtime, &pack->ctime );

	/* Allocate headerLongs and fill slots 1..N from SW3Z CRC32C values.
	 * Slot 0 (checksumFeed) and checksum/pure_checksum computation are
	 * done by the caller in files.c, since fs_checksumFeed is static there.
	 * Until then, checksum/pure_checksum are set to provisional values. */
	pack->numHeaderLongs = numValidFiles + 1;
	pack->headerLongs = (int *)Z_Malloc( pack->numHeaderLongs * sizeof( int ) );
	pack->headerLongs[0] = 0; /* placeholder — set by files.c */
	{
		int hi = 1;
		for ( int i = 0; i < (int)header.entryCount; i++ ) {
			if ( entries[i].uncompressedSize == 0 && entries[i].compressedSize == 0 )
				continue;
			pack->headerLongs[hi++] = LittleLong( entries[i].crc32c );
		}
	}

	/* Provisional checksums — overwritten by files.c after checksumFeed is set */
	pack->checksum      = 0;
	pack->pure_checksum = 0;

	Com_DPrintf( "SW3Z: loaded '%s' (%d files)\n", filename, numValidFiles );

	return pack;
}


/* ── SW3Z_CloseArchive ───────────────────────────────────────────────── */

void SW3Z_CloseArchive( pack_t *pack ) {
	if ( !pack )
		return;

	if ( PACK_FILE_HANDLE(pack) ) {
		fclose( PACK_FILE_HANDLE(pack) );
		PACK_FILE_HANDLE(pack) = NULL;
	}

	/* entries, stringTable, and headerLongs were separately allocated */
	if ( pack->entries )
		Z_Free( pack->entries );
	if ( pack->stringTable )
		Z_Free( pack->stringTable );
	if ( pack->headerLongs )
		Z_Free( pack->headerLongs );

	/* pack itself (+ hashTable + buildBuffer + names) is one allocation */
	Z_Free( pack );
}


/* ── SW3Z_ReadEntry ──────────────────────────────────────────────────
 *
 * Decompression pipeline:
 *   fseek → fread compressed → [LZ4F_decompress | raw copy] → CRC32C verify
 */
int SW3Z_ReadEntry( pack_t *pack, int entryIndex, void *buf, int bufSize ) {
	if ( !pack || !PACK_FILE_HANDLE(pack) || !buf )
		return -1;

	/* bounds check */
	if ( entryIndex < 0 || entryIndex >= (int)pack->entryCount ) {
		Com_Printf( S_COLOR_RED "ERROR: SW3Z: entry index %d out of range in '%s'\n",
			entryIndex, pack->pakFilename );
		return -1;
	}

	sw3zEntry_t *e = &pack->entries[entryIndex];

	/* buffer size check */
	if ( bufSize < (int)e->uncompressedSize ) {
		Com_Printf( S_COLOR_RED "ERROR: SW3Z: buffer too small (%d < %u) in '%s'\n",
			bufSize, e->uncompressedSize, pack->pakFilename );
		return -1;
	}

	/* empty file (e.g., directory entry) */
	if ( e->uncompressedSize == 0 )
		return 0;

	/* >2GB data offset check */
	if ( e->dataOffsetHi != 0 ) {
		Com_Printf( S_COLOR_RED "ERROR: SW3Z: entry data offset exceeds 2GB in '%s'\n",
			pack->pakFilename );
		return -1;
	}

	/* entry data_offset is absolute from file start (per spec) */
	unsigned long seekPos = (unsigned long)e->dataOffsetLo;
	if ( fseek( PACK_FILE_HANDLE(pack), (long)seekPos, SEEK_SET ) != 0 ) {
		Com_Printf( S_COLOR_RED "ERROR: SW3Z: seek failed in '%s'\n", pack->pakFilename );
		return -1;
	}

	if ( e->compression == SW3Z_COMP_NONE ) {
		/* ── uncompressed ── */
		if ( e->compressedSize != e->uncompressedSize ) {
			Com_Printf( S_COLOR_RED "ERROR: SW3Z: uncompressed entry size mismatch in '%s'\n",
				pack->pakFilename );
			return -1;
		}

		if ( fread( buf, 1, e->uncompressedSize, PACK_FILE_HANDLE(pack) ) != e->uncompressedSize ) {
			Com_Printf( S_COLOR_RED "ERROR: SW3Z: fread failed in '%s'\n", pack->pakFilename );
			return -1;
		}
	} else if ( e->compression == SW3Z_COMP_LZ4 ) {
		/* ── LZ4 Frame Format ── */
		LZ4F_dctx *dctx = NULL;
		size_t dstSize;

		byte *compBuf = (byte *)Z_Malloc( e->compressedSize );
		if ( fread( compBuf, 1, e->compressedSize, PACK_FILE_HANDLE(pack) ) != e->compressedSize ) {
			Com_Printf( S_COLOR_RED "ERROR: SW3Z: fread compressed data failed in '%s'\n",
				pack->pakFilename );
			Z_Free( compBuf );
			return -1;
		}

		LZ4F_errorCode_t err = LZ4F_createDecompressionContext( &dctx, LZ4F_VERSION );
		if ( LZ4F_isError( err ) ) {
			Com_Printf( S_COLOR_RED "ERROR: SW3Z: LZ4F_createDecompressionContext failed: %s\n",
				LZ4F_getErrorName( err ) );
			Z_Free( compBuf );
			return -1;
		}

		/* LZ4 Frame Format may produce multiple blocks (e.g., default 4MB block size).
		 * Loop LZ4F_decompress until the entire frame is decoded (result == 0). */
		{
			const byte *srcPtr = compBuf;
			byte       *dstPtr = (byte *)buf;
			size_t      srcRemain = e->compressedSize;
			size_t      dstRemain = e->uncompressedSize;

			size_t result = 1; /* non-zero to enter loop */
			while ( result != 0 && dstRemain > 0 ) {
				size_t srcSize = srcRemain;
				dstSize = dstRemain;
				result = LZ4F_decompress( dctx, dstPtr, &dstSize, srcPtr, &srcSize, NULL );
				if ( LZ4F_isError( result ) ) {
					Com_Printf( S_COLOR_RED "ERROR: SW3Z: LZ4 decompression failed in '%s': %s\n",
						pack->pakFilename, LZ4F_getErrorName( result ) );
					LZ4F_freeDecompressionContext( dctx );
					Z_Free( compBuf );
					return -1;
				}
				srcPtr    += srcSize;
				srcRemain -= srcSize;
				dstPtr    += dstSize;
				dstRemain -= dstSize;
			}

			dstSize = e->uncompressedSize - dstRemain;
		}

		LZ4F_freeDecompressionContext( dctx );
		Z_Free( compBuf );

		if ( dstSize != e->uncompressedSize ) {
			Com_Printf( S_COLOR_RED "ERROR: SW3Z: LZ4 decompressed size mismatch (%zu != %u) in '%s'\n",
				dstSize, e->uncompressedSize, pack->pakFilename );
			return -1;
		}
	} else if ( e->compression == SW3Z_COMP_ZSTD ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: Zstd compression not supported in '%s'\n",
			pack->pakFilename );
		return -1;
	} else {
		Com_Printf( S_COLOR_YELLOW "WARNING: SW3Z: unknown compression 0x%02X in '%s'\n",
			e->compression, pack->pakFilename );
		return -1;
	}

	/* ── CRC32C verification ── */
	uint32_t crc = SW3Z_CRC32C( buf, e->uncompressedSize );
	if ( crc != e->crc32c ) {
		Com_Printf( S_COLOR_RED "ERROR: SW3Z: CRC32C mismatch (got 0x%08X, expected 0x%08X) in '%s'\n",
			crc, e->crc32c, pack->pakFilename );
		return -1;
	}

	return (int)e->uncompressedSize;
}
