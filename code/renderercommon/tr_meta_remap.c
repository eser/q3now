/*
===========================================================================
Copyright (C) 2024 Wired engine contributors

This file is part of Quake III Arena source code.
Released under GPLv2 — see meta.h for the full notice.
===========================================================================
*/

// tr_meta_remap.c -- renderer-DLL side of the q3now typed meta-remap.
//
// The renderer DLL is loaded dynamically and does not link against
// code/qcommon/maps/meta_remap.c (the engine-side authoritative copy of
// the remap state). To keep the renderer's R_FindShader call sites
// agnostic, we define R_TryRemapShader / R_TryRemapTexture and the
// recursion-guard helpers here with the same signatures as meta.h
// declares; they delegate the actual lookup back across the DLL
// boundary via ri.MetaRemap_Lookup(kind, name).
//
// Recursion guard: handled renderer-locally with a depth counter. A
// non-zero depth means we're already resolving a remap target and must
// not consult the table again. R_PushNullRemap and R_PopRemap return
// void (the engine-side counterpart matches; old `remap_table_t *`
// signatures are gone).

#include "../qcommon/q_shared.h"
#include "../qcommon/maps/meta.h"
#include "tr_public.h"

extern refimport_t ri;

static int r_remapBypassDepth = 0;

static const char *DelegateLookup( remap_kind_t kind, const char *name ) {
	if ( r_remapBypassDepth > 0 ) return NULL;
	if ( !ri.MetaRemap_Lookup )   return NULL;   // early-init / no engine ABI
	if ( !name || !*name )        return NULL;
	return ri.MetaRemap_Lookup( (int)kind, name );
}

const char *R_TryRemapShader ( const char *name ) { return DelegateLookup( REMAP_KIND_SHADER,  name ); }
const char *R_TryRemapTexture( const char *name ) { return DelegateLookup( REMAP_KIND_TEXTURE, name ); }

// S_TryRemap{Sound,Music} are declared in meta.h for symmetry; not
// consumed by the renderer DLL. Sound system lives engine-side, and
// the engine-side meta_remap.c provides those implementations against
// its r_activeRemapSet directly. The renderer DLL has no use for them.

void R_PushNullRemap( void ) { r_remapBypassDepth++; }
void R_PopRemap     ( void ) { if ( r_remapBypassDepth > 0 ) r_remapBypassDepth--; }
