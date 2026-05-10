/*
===========================================================================
Copyright (C) 2024-2026 Wired engine contributors. GPLv2.
===========================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "q_shared.h"
#include "qcommon.h"
#include "snd_codec.h"      /* S_CodecResolves   — engine-side probe */
#include "r_image_probe.h"  /* R_ImageResolves   — engine-side probe */
#include "asset_resolve.h"
#include "category_map.h"

// Sound/music availability is delegated to S_CodecResolves; texture
// availability to R_ImageResolves. Both probes live in the engine
// source tree (code/client + code/renderercommon) and walk the same
// extension priority lists the engine uses at runtime, so the tool's
// answer matches what the engine would render / play.

qboolean Asset_IsAvailable(const needed_asset_t *a,
                           const shader_index_t *idx)
{
	if ( !a ) return qfalse;
	switch ( a->kind ) {
	case ASSET_KIND_SHADER:
		// Shader hit in the index counts. Otherwise fall through to
		// the bare-shader case (engine looks for a same-named image).
		if ( ShaderIndex_Has( idx, a->path ) ) return qtrue;
		return R_ImageResolves( a->path );
	case ASSET_KIND_TEXTURE:
		// A "texture" path is also satisfied by a shader entry of the
		// same name (textures/common/caulk, textures/common/clip,
		// model/mapobjects/spotlamp/*, etc. exist only as shaders, no
		// matching .tga). Inventory stores extension-less paths so
		// the lookup can be done literally — same convention the
		// engine's R_FindShader uses after COM_StripExtension.
		if ( ShaderIndex_Has( idx, a->path ) ) return qtrue;
		return R_ImageResolves( a->path );
	case ASSET_KIND_SOUND:
	case ASSET_KIND_MUSIC:
		// Same path normalization + codec extension fallback the
		// engine uses at runtime. Tool-side answer is identical
		// to engine-side because both walk the s_codec_extensions
		// list inside snd_codec.c.
		return S_CodecResolves( a->path );
	}
	return qfalse;
}

// Helper: same as Asset_IsAvailable but takes an explicit kind +
// path (for testing rewritten / default candidates).
static qboolean PathIsAvailableAs(asset_kind_t kind, const char *path,
                                  const shader_index_t *idx)
{
	needed_asset_t probe;
	probe.kind = kind;
	Q_strncpyz( probe.path, path, sizeof( probe.path ) );
	return Asset_IsAvailable( &probe, idx );
}

void Asset_Resolve(const needed_asset_t *missing,
                   const shader_index_t *idx,
                   resolution_t *out)
{
	if ( !missing || !out ) return;
	memset( out, 0, sizeof( *out ) );
	out->source            = *missing;
	out->resolved          = qfalse;
	out->resolution_method = "unresolved";

	// A. Category swap.
	char rewritten[ MAX_QPATH ];
	if ( CategoryMap_TryRewrite( missing, rewritten, sizeof( rewritten ) ) ) {
		if ( PathIsAvailableAs( missing->kind, rewritten, idx ) ) {
			Q_strncpyz( out->replacement, rewritten, sizeof( out->replacement ) );
			out->resolved          = qtrue;
			out->resolution_method = "swap_category";
			return;
		}
	}

	// B. First-in-target-dir is approximate. We don't have a directory
	// enumerator that returns assets independent of extension cheaply,
	// so we try the rewritten path's directory with the bare basename
	// "default" as a soft probe — if a category default lives at the
	// rewritten directory, treat that as a directory hit. Not exhaustive,
	// but matches the spec's "approximate" disclaimer.
	{
		const char *src_dir_seed = ( rewritten[0] ) ? rewritten : missing->path;
		char        candidate[ MAX_QPATH ];
		Q_strncpyz( candidate, src_dir_seed, sizeof( candidate ) );
		// Trim back to last '/' and append "default" as the probe stem.
		char *last_slash = strrchr( candidate, '/' );
		if ( last_slash ) {
			// Append "default" after the trailing slash, bounded to MAX_QPATH.
			const size_t prefix_len = (size_t)( last_slash - candidate ) + 1;
			Q_strncpyz( candidate + prefix_len, "default",
			            (int)( sizeof( candidate ) - prefix_len ) );
			if ( PathIsAvailableAs( missing->kind, candidate, idx ) ) {
				Q_strncpyz( out->replacement, candidate,
				            sizeof( out->replacement ) );
				out->resolved          = qtrue;
				out->resolution_method = "first_in_dir";
				return;
			}
		}
	}

	// C. Category default.
	char fallback[ MAX_QPATH ];
	if ( CategoryMap_TryDefault( missing->kind, fallback, sizeof( fallback ) ) ) {
		if ( PathIsAvailableAs( missing->kind, fallback, idx ) ) {
			Q_strncpyz( out->replacement, fallback, sizeof( out->replacement ) );
			out->resolved          = qtrue;
			out->resolution_method = "category_default";
			return;
		}
	}

	// D. Unresolved.
	out->replacement[0]    = '\0';
	out->resolved          = qfalse;
	out->resolution_method = "unresolved";
}
