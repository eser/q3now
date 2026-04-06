/*
 * lz4hc.c — STUB implementations for decompression-only builds
 *
 * lz4frame.c references HC compression functions.  We only use the
 * decompression side, so these stubs satisfy the linker without
 * pulling in the real HC compressor (~1600 lines).
 *
 * All functions return failure / no-op.  If anyone accidentally calls
 * them at runtime, the caller gets an error return, not a crash.
 */

#define LZ4_HC_STATIC_LINKING_ONLY
#include "lz4hc.h"

#include <stddef.h>  /* NULL, size_t */

LZ4LIB_API LZ4_streamHC_t* LZ4_createStreamHC(void) {
    return NULL;
}

LZ4LIB_API int LZ4_freeStreamHC(LZ4_streamHC_t* streamHCPtr) {
    (void)streamHCPtr;
    return 0;
}

LZ4LIB_API int LZ4_sizeofStateHC(void) {
    return (int)sizeof(LZ4_streamHC_t);
}

LZ4LIB_API int LZ4_compress_HC_extStateHC(void* stateHC,
    const char* src, char* dst,
    int srcSize, int maxDstSize,
    int compressionLevel) {
    (void)stateHC; (void)src; (void)dst;
    (void)srcSize; (void)maxDstSize; (void)compressionLevel;
    return 0;  /* 0 = failure in LZ4 compress API */
}

LZ4LIB_API int LZ4_compress_HC_continue(LZ4_streamHC_t* streamHCPtr,
    const char* src, char* dst,
    int srcSize, int maxDstSize) {
    (void)streamHCPtr; (void)src; (void)dst;
    (void)srcSize; (void)maxDstSize;
    return 0;
}

LZ4LIB_API int LZ4_loadDictHC(LZ4_streamHC_t* streamHCPtr,
    const char* dictionary, int dictSize) {
    (void)streamHCPtr; (void)dictionary; (void)dictSize;
    return 0;
}

LZ4LIB_API int LZ4_saveDictHC(LZ4_streamHC_t* streamHCPtr,
    char* safeBuffer, int maxDictSize) {
    (void)streamHCPtr; (void)safeBuffer; (void)maxDictSize;
    return 0;
}

LZ4LIB_API void LZ4_resetStreamHC(LZ4_streamHC_t* streamHCPtr,
    int compressionLevel) {
    (void)streamHCPtr; (void)compressionLevel;
}

LZ4LIB_API LZ4_streamHC_t* LZ4_initStreamHC(void* buffer, size_t size) {
    (void)buffer; (void)size;
    return NULL;
}

LZ4LIB_API void LZ4_attach_HC_dictionary(LZ4_streamHC_t* working_stream,
    const LZ4_streamHC_t* dictionary_stream) {
    (void)working_stream; (void)dictionary_stream;
}

/* Static-linking-only functions */
LZ4LIB_API void LZ4_setCompressionLevel(
    LZ4_streamHC_t* LZ4_streamHCPtr, int compressionLevel) {
    (void)LZ4_streamHCPtr; (void)compressionLevel;
}

LZ4LIB_API void LZ4_favorDecompressionSpeed(
    LZ4_streamHC_t* LZ4_streamHCPtr, int favor) {
    (void)LZ4_streamHCPtr; (void)favor;
}

LZ4LIB_API void LZ4_resetStreamHC_fast(
    LZ4_streamHC_t* LZ4_streamHCPtr, int compressionLevel) {
    (void)LZ4_streamHCPtr; (void)compressionLevel;
}

LZ4LIB_API int LZ4_compress_HC_extStateHC_fastReset(
    void* state,
    const char* src, char* dst,
    int srcSize, int dstCapacity,
    int compressionLevel) {
    (void)state; (void)src; (void)dst;
    (void)srcSize; (void)dstCapacity; (void)compressionLevel;
    return 0;
}
