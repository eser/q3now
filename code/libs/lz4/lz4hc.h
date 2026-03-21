/*
 * lz4hc.h — STUB for decompression-only builds
 *
 * lz4frame.c unconditionally includes lz4hc.h because the frame API
 * contains both compression and decompression paths.  We only use the
 * decompression side, so this stub provides:
 *   - the constants lz4frame.c references (CLEVEL_MIN/MAX/DEFAULT, etc.)
 *   - the LZ4_streamHC_t type (needed for the cctx struct definition)
 *   - function declarations for every HC symbol lz4frame.c calls
 *
 * The compression functions are declared here but never defined.
 * Any attempt to actually *call* them will produce a link error,
 * which is exactly the safety net we want: if game code accidentally
 * tries to compress with HC, the build fails loudly.
 *
 * Based on lz4hc.h from LZ4 v1.10.0 (BSD 2-Clause).
 */

#ifndef LZ4_HC_H_19834876238432
#define LZ4_HC_H_19834876238432

#include "lz4.h"   /* LZ4LIB_API, LZ4_byte, LZ4_u32, etc. */

/* --- Useful constants --- */
#define LZ4HC_CLEVEL_MIN         2
#define LZ4HC_CLEVEL_DEFAULT     9
#define LZ4HC_CLEVEL_OPT_MIN   10
#define LZ4HC_CLEVEL_MAX        12

/* --- Private definitions (static allocation only) --- */
#define LZ4HC_DICTIONARY_LOGSIZE 16
#define LZ4HC_MAXD        (1 << LZ4HC_DICTIONARY_LOGSIZE)
#define LZ4HC_MAXD_MASK   (LZ4HC_MAXD - 1)

#define LZ4HC_HASH_LOG    15
#define LZ4HC_HASHTABLESIZE (1 << LZ4HC_HASH_LOG)
#define LZ4HC_HASH_MASK   (LZ4HC_HASHTABLESIZE - 1)

/* Internal context — never use directly */
typedef struct LZ4HC_CCtx_internal LZ4HC_CCtx_internal;
struct LZ4HC_CCtx_internal
{
    LZ4_u32        hashTable[LZ4HC_HASHTABLESIZE];
    LZ4_u16        chainTable[LZ4HC_MAXD];
    const LZ4_byte* end;
    const LZ4_byte* prefixStart;
    const LZ4_byte* dictStart;
    LZ4_u32        dictLimit;
    LZ4_u32        lowLimit;
    LZ4_u32        nextToUpdate;
    short          compressionLevel;
    LZ4_i8         favorDecSpeed;
    LZ4_i8         dirty;
    const LZ4HC_CCtx_internal* dictCtx;
};

#define LZ4_STREAMHC_MINSIZE  262200
union LZ4_streamHC_u {
    char minStateSize[LZ4_STREAMHC_MINSIZE];
    LZ4HC_CCtx_internal internal_donotuse;
};
typedef union LZ4_streamHC_u LZ4_streamHC_t;

/* --- Public API declarations (compression — link will fail if called) --- */
LZ4LIB_API LZ4_streamHC_t* LZ4_createStreamHC(void);
LZ4LIB_API int             LZ4_freeStreamHC(LZ4_streamHC_t* streamHCPtr);

LZ4LIB_API int LZ4_sizeofStateHC(void);
LZ4LIB_API int LZ4_compress_HC_extStateHC(void* stateHC,
                                           const char* src, char* dst,
                                           int srcSize, int maxDstSize,
                                           int compressionLevel);

LZ4LIB_API int LZ4_compress_HC_continue(LZ4_streamHC_t* streamHCPtr,
                                         const char* src, char* dst,
                                         int srcSize, int maxDstSize);

LZ4LIB_API int LZ4_loadDictHC(LZ4_streamHC_t* streamHCPtr,
                                const char* dictionary, int dictSize);

LZ4LIB_API int LZ4_saveDictHC(LZ4_streamHC_t* streamHCPtr,
                                char* safeBuffer, int maxDictSize);

LZ4LIB_API void LZ4_resetStreamHC(LZ4_streamHC_t* streamHCPtr,
                                    int compressionLevel);

LZ4LIB_API LZ4_streamHC_t* LZ4_initStreamHC(void* buffer, size_t size);

LZ4LIB_API void LZ4_attach_HC_dictionary(LZ4_streamHC_t* working_stream,
                                           const LZ4_streamHC_t* dictionary_stream);

#endif /* LZ4_HC_H_19834876238432 */


/* --- Static-linking-only section --- */
#ifdef LZ4_HC_STATIC_LINKING_ONLY
#ifndef LZ4_HC_SLO_098092834
#define LZ4_HC_SLO_098092834

#if defined (__cplusplus)
extern "C" {
#endif

#ifndef LZ4LIB_STATIC_API
#define LZ4LIB_STATIC_API LZ4LIB_API
#endif

LZ4LIB_STATIC_API void LZ4_setCompressionLevel(
    LZ4_streamHC_t* LZ4_streamHCPtr, int compressionLevel);

LZ4LIB_STATIC_API void LZ4_favorDecompressionSpeed(
    LZ4_streamHC_t* LZ4_streamHCPtr, int favor);

LZ4LIB_STATIC_API void LZ4_resetStreamHC_fast(
    LZ4_streamHC_t* LZ4_streamHCPtr, int compressionLevel);

LZ4LIB_STATIC_API int LZ4_compress_HC_extStateHC_fastReset(
    void* state,
    const char* src, char* dst,
    int srcSize, int dstCapacity,
    int compressionLevel);

#if defined (__cplusplus)
}
#endif

#endif /* LZ4_HC_SLO_098092834 */
#endif /* LZ4_HC_STATIC_LINKING_ONLY */
