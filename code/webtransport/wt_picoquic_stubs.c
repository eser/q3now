/*
===========================================================================
wt_picoquic_stubs.c — Stub implementations for optional picoquic features

picoquic's tls_api.c references optional TLS backends (minicrypto, fusion)
and optional features (qlog) that we don't compile. The linker needs
these symbols even though they're never called at runtime when using
the OpenSSL backend.
===========================================================================
*/
#include "../game/q_feats.h"

#if FEAT_QUIC_TRANSPORT

#include <stddef.h>
#include <stdint.h>

/* picoquic forward declarations — avoid including full header just for stubs */
typedef struct st_picoquic_quic_t picoquic_quic_t;
typedef struct st_picoquic_tls_api_t picoquic_tls_api_t;

/* Stub: minicrypto TLS backend (we use OpenSSL instead) */
int picoquic_ptls_minicrypto_load(picoquic_tls_api_t* tls_api, const char* cert_file,
    const char* key_file)
{
    (void)tls_api; (void)cert_file; (void)key_file;
    return -1; /* not available */
}

/* Stub: AES-NI fusion backend (x86 only, we're on ARM or use OpenSSL) */
int picoquic_ptls_fusion_load(picoquic_tls_api_t* tls_api)
{
    (void)tls_api;
    return -1; /* not available */
}

/* Stub: qlog (optional structured logging we don't compile) */
void picoquic_set_qlog(picoquic_quic_t* quic, const char* qlog_dir)
{
    (void)quic; (void)qlog_dir;
}

#endif /* FEAT_QUIC_TRANSPORT */
