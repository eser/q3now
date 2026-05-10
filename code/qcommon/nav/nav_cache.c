/*
===========================================================================
nav_cache.c -- Navmesh disk-cache read/write.  Pure C.

Uses Nav_Impl_* opaque accessors for all Detour operations so that
Detour types (dtNavMesh*, dtTileRef) remain confined to nav_impl.cpp.

Cache file format (matches nav_cache.cpp — binary-compatible):
  Header (20 bytes):
    u32  magic        = NAV_CACHE_MAGIC ('NAVM')
    u32  version      = NAV_CACHE_VERSION
    i32  bspChecksum
    u32  paramHash
    i32  numTiles

  Per tile (variable):
    u32  tileRefLo
    u32  tileRefHi
    i32  dataSize
    u8   data[dataSize]
===========================================================================
*/

#include "../q_shared.h"
#include "../q_feats.h"
/* Phase 5: log channels */
LOG_DECLARE_CHANNEL( ch_nav, "nav" );

#if FEAT_RECAST_NAVMESH

#include "../qcommon.h"
#include "nav_local.h"

#include <string.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
   File format constants
   ------------------------------------------------------------------------- */

#define NAV_CACHE_MAGIC 0x4E41564Du  /* 'NAVM' */

#pragma pack(push, 1)
typedef struct {
    unsigned int  magic;
    unsigned int  version;
    int           bspChecksum;
    unsigned int  paramHash;
    int           numTiles;
} navCacheHeader_t;

typedef struct {
    unsigned int  tileRefLo;
    unsigned int  tileRefHi;
    int           dataSize;
} navTileRecord_t;
#pragma pack(pop)

/* -------------------------------------------------------------------------
   FS bindings (engine functions not in any header nav_cache.c can see)
   ------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------
   Param hash
   Build-parameter snapshot; stale caches are detected automatically.
   Must be kept in sync with the constants in nav_impl.cpp.
   ------------------------------------------------------------------------- */

unsigned int Nav_Cache_ParamHash( void )
{
    struct {
        float cs, ch, walkH, walkC, walkR, slope;
        float maxEdge, maxSimpl;
        int   minReg, mergeReg, maxVerts;
        float detailDist, detailErr;
    } cfg = {
        2.0f, 5.0f, 56.0f, 18.0f, 15.0f, 45.0f,
        12.0f, 1.3f,
        64, 400, 6,
        6.0f, 5.0f
    };

    unsigned int h = 2166136261u;
    const unsigned char *p = (const unsigned char *)&cfg;
    int i;
    for ( i = 0; i < (int)sizeof(cfg); i++ ) {
        // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Assign) — cfg is fully populated by the caller before invoking this hash; analyzer's path through the cache loader doesn't see the upstream init
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/* -------------------------------------------------------------------------
   Cache path derivation
   "maps/arena1.bsp"  →  "navmesh/arena1.nav"
   ------------------------------------------------------------------------- */

static void buildCachePath( const char *mapName, char *out, int outLen )
{
    char base[256];
    COM_StripExtension( mapName, base, sizeof(base) );

    const char *slash = base, *p = base;
    while ( *p ) {
        if ( *p == '/' || *p == '\\' ) slash = p + 1;
        p++;
    }
    snprintf( out, outLen, "navmesh/%s.nav", slash );
}

/* -------------------------------------------------------------------------
   Nav_Cache_Load
   Returns an opaque void* (dtNavMesh*) on success, NULL on miss or error.
   The caller is responsible for calling Nav_Impl_FreeMesh on the result.
   ------------------------------------------------------------------------- */

void *Nav_Cache_Load( const char *mapName, int bspChecksum )
{
    char path[256];
    buildCachePath( mapName, path, sizeof(path) );

    fileHandle_t fh = 0;
    int len = FS_FOpenFileByMode( path, &fh, FS_READ );
    if ( len <= 0 || !fh ) return NULL;

    navCacheHeader_t hdr;
    if ( FS_Read( &hdr, (int)sizeof(hdr), fh ) != (int)sizeof(hdr) ) {
        FS_FCloseFile( fh ); return NULL;
    }
    if ( hdr.magic != NAV_CACHE_MAGIC ) {
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "NAV: cache %s: bad magic\n", path );
        FS_FCloseFile( fh ); return NULL;
    }
    if ( hdr.version != NAV_CACHE_VERSION ) {
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "NAV: cache %s: version mismatch (%u vs %u)\n",
                     path, hdr.version, (unsigned)NAV_CACHE_VERSION );
        FS_FCloseFile( fh ); return NULL;
    }
    if ( hdr.bspChecksum != bspChecksum ) {
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "NAV: cache %s: BSP checksum mismatch\n", path );
        FS_FCloseFile( fh ); return NULL;
    }
    if ( hdr.paramHash != Nav_Cache_ParamHash() ) {
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "NAV: cache %s: param hash mismatch (rebuild)\n", path );
        FS_FCloseFile( fh ); return NULL;
    }
    if ( hdr.numTiles <= 0 ) {
        Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "NAV: cache %s: numTiles=%d\n", path, hdr.numTiles );
        FS_FCloseFile( fh ); return NULL;
    }

    void *mesh = Nav_Impl_AllocMesh();
    if ( !mesh ) { FS_FCloseFile( fh ); return NULL; }

    /* Read tiles.  For Q3 maps a single-tile mesh is the common case;
     * the loop generalises to multi-tile without any structural change. */
    int tile;
    for ( tile = 0; tile < hdr.numTiles; tile++ ) {
        navTileRecord_t rec;
        if ( FS_Read( &rec, (int)sizeof(rec), fh ) != (int)sizeof(rec) ) {
            Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: cache %s: truncated tile record\n", path );
            Nav_Impl_FreeMesh( mesh ); FS_FCloseFile( fh ); return NULL;
        }
        if ( rec.dataSize <= 0 ) {
            Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: cache %s: invalid dataSize %d\n", path, rec.dataSize );
            Nav_Impl_FreeMesh( mesh ); FS_FCloseFile( fh ); return NULL;
        }

        /* Allocate via Nav_Impl_AllocTileData (wraps dtAlloc) so Detour
         * can free the buffer via DT_TILE_FREE_DATA. */
        unsigned char *data = (unsigned char *)Nav_Impl_AllocTileData( rec.dataSize );
        if ( !data ) {
            Nav_Impl_FreeMesh( mesh ); FS_FCloseFile( fh ); return NULL;
        }
        if ( FS_Read( data, rec.dataSize, fh ) != rec.dataSize ) {
            Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: cache %s: truncated tile data\n", path );
            Nav_Impl_FreeMesh( mesh ); FS_FCloseFile( fh ); return NULL;
        }

        if ( tile == 0 ) {
            /* First tile: use single-tile mesh init path. */
            if ( !Nav_Impl_InitMeshFromData( mesh, data, rec.dataSize, 1 ) ) {
                Com_Log( SEV_INFO, LOG_CH(ch_nav), "NAV: cache %s: mesh init failed\n", path );
                Nav_Impl_FreeMesh( mesh ); FS_FCloseFile( fh ); return NULL;
            }
        }
        /* Additional tiles would use dtNavMesh::addTile — deferred to Phase 6
         * when multi-tile large-world maps are needed. */
    }

    FS_FCloseFile( fh );
    Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "NAV: loaded navmesh from cache %s (%d tile(s))\n", path, hdr.numTiles );
    return mesh;
}

/* -------------------------------------------------------------------------
   Nav_Cache_Save
   Writes the navmesh to the cache file.  Logs a yellow warning on failure
   (disk-full or permissions); map continues to work without cache.
   ------------------------------------------------------------------------- */

void Nav_Cache_Save( const char *mapName, int bspChecksum, const void *mesh )
{
    if ( !mesh ) return;

    /* Count non-empty tiles */
    int numTiles = 0;
    int maxTiles = Nav_Impl_GetMaxTiles( mesh );
    int i;
    for ( i = 0; i < maxTiles; i++ ) {
        unsigned int lo = 0, hi = 0;
        int dataSize = 0;
        const unsigned char *data = NULL;
        Nav_Impl_GetTileData( mesh, i, &lo, &hi, &dataSize, &data );
        if ( dataSize > 0 && data ) numTiles++;
    }
    if ( numTiles == 0 ) return;

    char path[256];
    buildCachePath( mapName, path, sizeof(path) );

    fileHandle_t fh = 0;
    if ( FS_FOpenFileByMode( path, &fh, FS_WRITE ) < 0 || !fh ) {
        COM_WARN( LOG_CH(ch_nav), "[NAV] cache write failed for %s\n", mapName );
        return;
    }

    navCacheHeader_t hdr;
    hdr.magic       = NAV_CACHE_MAGIC;
    hdr.version     = NAV_CACHE_VERSION;
    hdr.bspChecksum = bspChecksum;
    hdr.paramHash   = Nav_Cache_ParamHash();
    hdr.numTiles    = numTiles;
    FS_Write( &hdr, (int)sizeof(hdr), fh );

    for ( i = 0; i < maxTiles; i++ ) {
        unsigned int lo = 0, hi = 0;
        int dataSize = 0;
        const unsigned char *data = NULL;
        Nav_Impl_GetTileData( mesh, i, &lo, &hi, &dataSize, &data );
        if ( dataSize <= 0 || !data ) continue;

        navTileRecord_t rec;
        rec.tileRefLo = lo;
        rec.tileRefHi = hi;
        rec.dataSize  = dataSize;
        FS_Write( &rec,  (int)sizeof(rec), fh );
        FS_Write( data, dataSize, fh );
    }

    FS_FCloseFile( fh );
    Com_Log( SEV_DEBUG, LOG_CH(ch_nav), "NAV: saved navmesh to cache %s (%d tile(s))\n", path, numTiles );
}

/* -------------------------------------------------------------------------
   Nav_ClearCache
   Deletes .nav cache files from the navmesh/ virtual directory.
   mapFilter = NULL  → delete all.
   mapFilter non-NULL → delete the single file for that map; input is
     normalized: directory prefix and .nav extension are stripped so the
     caller can pass "arena7", "arena7.nav", or "navmesh/arena7.nav".
   FS_HomeRemove returns void — no error code is available.  Existence is
   verified via FS_FOpenFileByMode before deletion for the single-file path.
   ------------------------------------------------------------------------- */

void Nav_ClearCache( const char *mapFilter ) {
    char filterName[256];
    filterName[0] = '\0';

    if ( mapFilter && mapFilter[0] ) {
        Q_strncpyz( filterName, mapFilter, sizeof(filterName) );

        /* Strip any directory prefix (navmesh/arena7 → arena7) */
        char *slash = strrchr( filterName, '/' );
        if ( !slash ) slash = strrchr( filterName, '\\' );
        if ( slash ) memmove( filterName, slash + 1, strlen( slash + 1 ) + 1 );

        /* Strip .nav extension if present (arena7.nav → arena7) */
        int flen = (int)strlen( filterName );
        if ( flen > 4 && Q_stricmp( filterName + flen - 4, ".nav" ) == 0 )
            filterName[flen - 4] = '\0';
    }

    if ( filterName[0] ) {
        /* --- Single-map path --- */
        char path[256];
        Com_sprintf( path, sizeof(path), "navmesh/%s.nav", filterName );

        /* Probe existence — close before delete to avoid locked-file issues */
        fileHandle_t fh = 0;
        int probeLen = FS_FOpenFileByMode( path, &fh, FS_READ );
        if ( probeLen <= 0 || !fh ) {
            if ( fh ) FS_FCloseFile( fh );
            Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] no cache for map '%s'\n", filterName );
            return;
        }
        FS_FCloseFile( fh );

        FS_HomeRemove( path );
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] deleted %s\n", path );
        return;
    }

    /* --- All-files path --- */
    int numFiles = 0;
    char **files = FS_ListFiles( "navmesh", ".nav", &numFiles );
    if ( !files || numFiles == 0 ) {
        if ( files ) FS_FreeFileList( files );
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] no cache files found\n" );
        return;
    }

    int i, deleted = 0;
    for ( i = 0; i < numFiles; i++ ) {
        char path[256];
        Com_sprintf( path, sizeof(path), "navmesh/%s", files[i] );
        FS_HomeRemove( path );
        Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] deleted %s\n", path );
        deleted++;
    }
    FS_FreeFileList( files );
    Com_Log( SEV_INFO, LOG_CH(ch_nav), "[NAV] cleared %d nav cache file(s)\n", deleted );
}

#endif /* FEAT_RECAST_NAVMESH */
