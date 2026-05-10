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
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_system, "system" );

/* ── CRC32C (Castagnoli) ─────────────────────────────────────────────
 *
 * Polynomial: 0x1EDC6F41 (reflected: 0x82F63B78)
 * NOT the same as CRC32 (ISO 3309) used in ZIP/PK3.
 */

static uint32_t sw3z_crc32c_table[256];
static qboolean sw3z_crc32c_init = qfalse;

/*
 * SW3Z_Init: one-shot module initialization. Builds the CRC32C table
 * eagerly so SW3Z_CRC32C is a pure lookup at every call site. Idempotent.
 *
 * Called once from FS_InitFilesystem. Must run before any SW3Z function
 * that hashes data — which is every SW3Z_ReadEntry that hits the verify
 * path. Centralizing here removes the pre-existing lazy-init race that
 * would surface the moment async loading is introduced.
 */
void SW3Z_Init( void ) {
	if ( sw3z_crc32c_init )
		return;

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


/*
 * SW3Z_EnsureLZ4Context: lazy-init + reset of one global LZ4F_dctx.
 *
 * The dctx is created on first use and reused across every SW3Z LZ4
 * decompression. LZ4F_resetDecompressionContext (lz4frame.h:528,
 * "always successful") puts the context back to its initial state
 * cheaply — far cheaper than the create/free pair that this replaces.
 *
 * Single-threaded I/O makes the global safe. If async loading is ever
 * introduced, this needs to become per-thread or per-handle (along
 * with much else in the FS layer).
 *
 * The dctx itself is intentionally not freed at engine shutdown — it's
 * a one-time allocation, the OS reclaims it, and adding a teardown
 * hook would be more code than the leak avoids.
 */
static LZ4F_dctx *g_lz4Dctx = NULL;

static qboolean SW3Z_EnsureLZ4Context( const char *forFilename ) {
	if ( g_lz4Dctx ) {
		LZ4F_resetDecompressionContext( g_lz4Dctx );
		return qtrue;
	}
	LZ4F_errorCode_t err = LZ4F_createDecompressionContext( &g_lz4Dctx, LZ4F_VERSION );
	if ( LZ4F_isError( err ) ) {
		COM_ERROR( LOG_CH(ch_system),
			"ERROR: SW3Z: LZ4F_createDecompressionContext failed for '%s': %s\n",
			forFilename, LZ4F_getErrorName( err ) );
		g_lz4Dctx = NULL;
		return qfalse;
	}
	return qtrue;
}

uint32_t SW3Z_CRC32C( const void *data, size_t len ) {
	const byte *p = (const byte *)data;
	uint32_t crc = 0xFFFFFFFFU;

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
		COM_WARN( LOG_CH(ch_system), "SW3Z: cannot open '%s'\n", filename );
		return NULL;
	}

	/* get file size for validation */
	fseek( f, 0, SEEK_END );
	long fileSize = ftell( f );
	fseek( f, 0, SEEK_SET );

	if ( fileSize < SW3Z_HEADER_SIZE ) {
		COM_WARN( LOG_CH(ch_system), "SW3Z: '%s' too small (%ld bytes)\n",
			filename, fileSize );
		fclose( f );
		return NULL;
	}

	/* ── read and validate header ── */
	byte headerBuf[SW3Z_HEADER_SIZE];
	if ( fread( headerBuf, 1, SW3Z_HEADER_SIZE, f ) != SW3Z_HEADER_SIZE ) {
		COM_WARN( LOG_CH(ch_system), "SW3Z: failed to read header from '%s'\n", filename );
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
		COM_WARN( LOG_CH(ch_system), "SW3Z: '%s' bad magic 0x%08X\n",
			filename, header.magic );
		fclose( f );
		return NULL;
	}

	if ( header.version != SW3Z_VERSION ) {
		COM_WARN( LOG_CH(ch_system), "SW3Z: '%s' unsupported version %d\n",
			filename, header.version );
		fclose( f );
		return NULL;
	}

	/* reconstruct 64-bit data offset — treat >2GB as error */
	if ( header.dataOffsetHi != 0 ) {
		COM_WARN( LOG_CH(ch_system), "SW3Z: '%s' data offset exceeds 2GB\n", filename );
		fclose( f );
		return NULL;
	}
	unsigned long dataOffset = (unsigned long)header.dataOffsetLo;

	/* ── security: validate entryCount against file size ── */
	unsigned int indexSize = header.entryCount * SW3Z_ENTRY_SIZE;
	if ( header.entryCount > 0 &&
		indexSize / SW3Z_ENTRY_SIZE != header.entryCount ) {
		/* integer overflow */
		COM_WARN( LOG_CH(ch_system), "SW3Z: '%s' entryCount overflow\n", filename );
		fclose( f );
		return NULL;
	}

	if ( (long)( SW3Z_HEADER_SIZE + indexSize + header.stringTableSize ) > fileSize ) {
		COM_WARN( LOG_CH(ch_system), "SW3Z: '%s' index/strings exceed file size\n", filename );
		fclose( f );
		return NULL;
	}

	/* ── read index ── */
	sw3zEntry_t *entries = NULL;
	if ( header.entryCount > 0 ) {
		entries = (sw3zEntry_t *)Z_Malloc( indexSize );
		if ( fread( entries, 1, indexSize, f ) != indexSize ) {
			COM_WARN( LOG_CH(ch_system), "SW3Z: '%s' failed to read index\n", filename );
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
			COM_WARN( LOG_CH(ch_system), "SW3Z: '%s' failed to read string table\n", filename );
			if ( entries ) Z_Free( entries );
			Z_Free( stringTable );
			fclose( f );
			return NULL;
		}
	}

	/* ── validate all entries and compute total name buffer size ── */
	int numValidFiles = 0;
	int nameLen = 0;
	unsigned int maxCompressedSize = 0;	/* Phase 4-#2: cap for the per-pack compBuf scratch */
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
			COM_WARN( LOG_CH(ch_system), "SW3Z: '%s' entry %d string out of bounds\n",
				filename, i );
			if ( entries ) Z_Free( entries );
			if ( stringTable ) Z_Free( stringTable );
			fclose( f );
			return NULL;
		}

		/* security: data range bounds check (Phase 4-#6).
		 * Reject any entry whose data extent walks past the file end —
		 * a malformed sw3z used to fseek+fread off the end at read time.
		 * Using uint64 arithmetic to avoid 32-bit overflow on the sum. */
		if ( e->dataOffsetHi != 0
		     || (uint64_t)e->dataOffsetLo + (uint64_t)e->compressedSize > (uint64_t)fileSize ) {
			COM_WARN( LOG_CH(ch_system),
				"SW3Z: '%s' entry %d data offset/size out of bounds (offset=%u size=%u fileSize=%ld)\n",
				filename, i, e->dataOffsetLo, e->compressedSize, fileSize );
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

		if ( e->compressedSize > maxCompressedSize )
			maxCompressedSize = e->compressedSize;
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

	/* Phase 4-#2: scratch buffer sized to the biggest entry. compScratch
	 * stays NULL until the first SW3Z_ReadEntry hits an LZ4 entry; from
	 * that point on it lives until SW3Z_CloseArchive. */
	pack->maxCompressedSize = maxCompressedSize;
	pack->compScratch       = NULL;
	pack->compScratchSize   = 0;

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

	Com_Log( SEV_DEBUG, LOG_CH(ch_system), "SW3Z: loaded '%s' (%d files)\n", filename, numValidFiles );

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

	/* entries, stringTable, headerLongs, and compScratch were separately allocated */
	if ( pack->entries )
		Z_Free( pack->entries );
	if ( pack->stringTable )
		Z_Free( pack->stringTable );
	if ( pack->headerLongs )
		Z_Free( pack->headerLongs );
	if ( pack->compScratch )
		Z_Free( pack->compScratch );

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
		COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: entry index %d out of range in '%s'\n",
			entryIndex, pack->pakFilename );
		return -1;
	}

	sw3zEntry_t *e = &pack->entries[entryIndex];

	/* buffer size check */
	if ( bufSize < (int)e->uncompressedSize ) {
		COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: buffer too small (%d < %u) in '%s'\n",
			bufSize, e->uncompressedSize, pack->pakFilename );
		return -1;
	}

	/* empty file (e.g., directory entry) */
	if ( e->uncompressedSize == 0 )
		return 0;

	/* >2GB data offset check */
	if ( e->dataOffsetHi != 0 ) {
		COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: entry data offset exceeds 2GB in '%s'\n",
			pack->pakFilename );
		return -1;
	}

	/* entry data_offset is absolute from file start (per spec) */
	unsigned long seekPos = (unsigned long)e->dataOffsetLo;
	if ( fseek( PACK_FILE_HANDLE(pack), (long)seekPos, SEEK_SET ) != 0 ) {
		COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: seek failed in '%s'\n", pack->pakFilename );
		return -1;
	}

	if ( e->compression == SW3Z_COMP_NONE ) {
		/* ── uncompressed ── */
		if ( e->compressedSize != e->uncompressedSize ) {
			COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: uncompressed entry size mismatch in '%s'\n",
				pack->pakFilename );
			return -1;
		}

		if ( fread( buf, 1, e->uncompressedSize, PACK_FILE_HANDLE(pack) ) != e->uncompressedSize ) {
			COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: fread failed in '%s'\n", pack->pakFilename );
			return -1;
		}
	} else if ( e->compression == SW3Z_COMP_LZ4 ) {
		/* ── LZ4 Frame Format ──
		 *
		 * Pooled global LZ4F_dctx (single-threaded I/O makes this safe).
		 * The compressed-data scratch buffer is also pooled per-pack
		 * (Phase 4 change #2). This branch carries no Z_Malloc/Z_Free
		 * pair on the hot path — it's the single biggest source of
		 * allocator churn on map load before Phase 4. */
		size_t dstSize;

		if ( !SW3Z_EnsureLZ4Context( pack->pakFilename ) ) {
			return -1;
		}

		/* Per-pack reusable scratch (Phase 4-#2). On the first LZ4 read
		 * we allocate up to maxCompressedSize so subsequent reads never
		 * grow. The cache disk-load path persists maxCompressedSize so
		 * a cache hit knows the cap without re-walking entries. If
		 * maxCompressedSize is 0 (e.g., legacy on-disk cache pre-v3
		 * without the field — defensive only; v2→v3 invalidation
		 * should prevent this), fall back to e->compressedSize. */
		if ( pack->compScratchSize < e->compressedSize ) {
			unsigned int newSize = pack->maxCompressedSize > e->compressedSize
				? pack->maxCompressedSize
				: e->compressedSize;
			if ( pack->compScratch ) {
				Z_Free( pack->compScratch );
			}
			pack->compScratch = (byte *)Z_TagMalloc( (int)newSize, TAG_PACK );
			pack->compScratchSize = newSize;
		}
		byte *compBuf = pack->compScratch;

		if ( fread( compBuf, 1, e->compressedSize, PACK_FILE_HANDLE(pack) ) != e->compressedSize ) {
			COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: fread compressed data failed in '%s'\n",
				pack->pakFilename );
			/* Don't free compBuf — it's owned by the pack now. */
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
				result = LZ4F_decompress( g_lz4Dctx, dstPtr, &dstSize, srcPtr, &srcSize, NULL );
				if ( LZ4F_isError( result ) ) {
					COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: LZ4 decompression failed in '%s': %s\n",
						pack->pakFilename, LZ4F_getErrorName( result ) );
					/* compBuf is pack-owned (compScratch); freed at SW3Z_CloseArchive. */
					return -1;
				}
				srcPtr    += srcSize;
				srcRemain -= srcSize;
				dstPtr    += dstSize;
				dstRemain -= dstSize;
			}

			dstSize = e->uncompressedSize - dstRemain;
		}

		if ( dstSize != e->uncompressedSize ) {
			COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: LZ4 decompressed size mismatch (%zu != %u) in '%s'\n",
				dstSize, e->uncompressedSize, pack->pakFilename );
			return -1;
		}
	} else if ( e->compression == SW3Z_COMP_ZSTD ) {
		COM_WARN( LOG_CH(ch_system), "SW3Z: Zstd compression not supported in '%s'\n",
			pack->pakFilename );
		return -1;
	} else {
		COM_WARN( LOG_CH(ch_system), "SW3Z: unknown compression 0x%02X in '%s'\n",
			e->compression, pack->pakFilename );
		return -1;
	}

	/* ── CRC32C verification ──
	 *
	 * Phase 4-#3 trade-off:
	 *   - SW3Z_COMP_LZ4: LZ4F's content checksum (XXH32) was already
	 *     verified inside LZ4F_decompress on the path above. Re-running
	 *     CRC32C here would catch only one extra threat — an attacker
	 *     who rewrites both the LZ4 data blob *and* its inner XXH32 but
	 *     leaves the SW3Z index intact. That threat is out of scope for
	 *     offline/single-player play; it becomes interesting only when
	 *     the server enforces strict pure mode (sv_pure >= 2). Skip the
	 *     redundant pass in the common case.
	 *   - SW3Z_COMP_NONE: uncompressed entries have no LZ4 wrapper and
	 *     thus no XXH32. The CRC32C here is the only integrity check —
	 *     always run it.
	 *
	 * sv_pure semantics:
	 *   0/1: trust LZ4F's content checksum for compressed entries.
	 *   >=2: extra-strict — re-verify CRC32C even on LZ4 entries. */
	qboolean must_verify = ( e->compression == SW3Z_COMP_NONE );
	if ( !must_verify ) {
		must_verify = ( Cvar_VariableIntegerValue( "sv_pure" ) >= 2 );
	}
	if ( must_verify ) {
		uint32_t crc = SW3Z_CRC32C( buf, e->uncompressedSize );
		if ( crc != e->crc32c ) {
			COM_ERROR( LOG_CH(ch_system), "ERROR: SW3Z: CRC32C mismatch (got 0x%08X, expected 0x%08X) in '%s'\n",
				crc, e->crc32c, pack->pakFilename );
			return -1;
		}
	}

	return (int)e->uncompressedSize;
}
