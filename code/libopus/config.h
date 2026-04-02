#ifndef OPUS_CONFIG_H
#define OPUS_CONFIG_H

#define OPUS_BUILD 1
#define HAVE_LRINTF 1
#define FLOATING_POINT 1
#define USE_ALLOCA 1
#define PACKAGE_VERSION "1.5.2"

/* CPU features - detect at compile time */
#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define OPUS_X86_MAY_HAVE_SSE 1
#define OPUS_X86_MAY_HAVE_SSE2 1
#define OPUS_X86_MAY_HAVE_SSE4_1 1
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define OPUS_ARM_MAY_HAVE_NEON_INTR 1
#endif

#endif
