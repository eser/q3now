// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
===========================================================================
r_log.h — channel-aware renderer logging (rilog-channel-mechanism Turn A).

Wraps the new ri.GetLogChannel / ri.LogCh refimport entries with the same
"per-TU file-scope cached int" pattern the engine uses for LOG_DECLARE_CHANNEL
/ LOG_CH (qcommon/wired/core/logging/log_channels.h). Each call site looks
like a printf and resolves the channel id at most ONCE per TU.

Usage:
    #include "r_log.h"

    R_LOG_DECLARE_CHANNEL( rch_init, "renderer.init" );   // file scope

    void R_Foo( void ) {
        R_LOG( rch_init, SEV_INFO, "foo initialized: %d slots", n );
    }

Channel taxonomy registered at engine boot (cl_main.c CL_InitRef):
    renderer            — root, floored at WARN by default
    renderer.init       — startup banners and one-shot init logs
    renderer.shaders    — shader compile / pipeline cache (incl. GLSL)
    renderer.assets     — image / model / shader loaders
    renderer.vk         — Vulkan-backend internals (qvk* dispatch, validation)
    renderer.gl         — GL1/GL2 backend internals (qgl* dispatch, ARB, VBO, ext probe)
    renderer.ral        — RAL bringup / adoption / submit
    renderer.hdr        — HDR / colour-space pipeline
    renderer.fbo        — framebuffer / render-pass / attachment lifecycle
    renderer.timing     — fence / acquire / submit / present timing
    renderer.cmd        — per-frame command-buffer recording
    renderer.temporal   — default-off temporal sequencing/projection diagnostics

Severity inheritance: a sub-channel without its own explicit override
inherits its effective threshold from the nearest registered ancestor —
so `log renderer info` opens the whole tree; `log renderer.init info`
opens only that slice.

R_LOG is purely additive — the legacy ri.Log path stays for any code that
does not select a channel and shared-code Com_Log. Both route through the
same underlying log buffer; only the channel attribution differs.
===========================================================================
*/
#ifndef WIRED_R_LOG_H
#define WIRED_R_LOG_H

// tr_local.h must be included before this header — it brings in tr_public.h
// (the refimport_t definition with ri.GetLogChannel + ri.LogCh) and log.h
// (the log_severity_t enum).

/*
 * R_LOG_DECLARE_CHANNEL — file-scope cached channel handle.
 *
 * Place at TU file scope, near the other static-storage declarations. The
 * `var` token is the identifier callers reference in R_LOG; `name_str` is
 * the dot-hierarchical channel name registered with the engine. Multiple
 * TUs may declare the same channel name — they share the id since
 * ri.GetLogChannel is name-keyed.
 */
#define R_LOG_DECLARE_CHANNEL(var, name_str)                                \
	static int        _rlog_##var      = -1;                                \
	static const char *_rlog_name_##var = (name_str)

/*
 * R_LOG_CH_ID — internal helper that lazily resolves the channel id on
 * first use, then returns the cached int on every subsequent call. The
 * branch is well-predicted (after the first call the cached >=0 branch
 * dominates); the engine-side Log_GetChannel takes the registry mutex
 * once per TU per channel. ri.GetLogChannel is null-checked defensively
 * for the rare case of a renderer linked against an older engine — the
 * fallback id 0 routes to "general" rather than crashing.
 */
#define R_LOG_CH_ID(var)                                                    \
	( _rlog_##var >= 0                                                      \
	    ? _rlog_##var                                                       \
	    : ( _rlog_##var =                                                   \
	          ( ri.GetLogChannel                                            \
	              ? ri.GetLogChannel( _rlog_name_##var )                    \
	              : 0 ) ) )

/*
 * R_LOG — channel-aware log call. Resolves the channel id (cached on
 * subsequent calls), then dispatches to ri.LogCh which wraps Com_Logv
 * engine-side. `sev` is a log_severity_t value (SEV_INFO / SEV_DEBUG / ...).
 * Same printf semantics as ri.Log.
 */
#define R_LOG(var, sev, ...)                                                \
	do {                                                                    \
		if ( ri.LogCh )                                                     \
			ri.LogCh( R_LOG_CH_ID(var), (sev), __VA_ARGS__ );               \
	} while ( 0 )

#endif /* WIRED_R_LOG_H */
