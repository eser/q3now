// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
traps.h — primitive submission trap signatures for cgame

These traps are the only API cgame uses to submit visual content
to the renderer. Effect-specific composition (helices, beams,
trails, bursts) is built in cgame on top of these primitives.
The renderer never sees an effect name.

Each trap submits ONE primitive instance. Repeated calls per
frame are supported. Submission is one-way: cgame owns the
descriptor memory; the renderer copies what it needs before the
trap returns.
*/
#pragma once

#include "primitives.h"
#include "particle_class.h"

// Ribbon trap takes its fields as separate args (not as a packed
// ribbonDesc_t struct) because the descriptor contains a pointer
// (`points`), and a struct-with-pointer cannot cross the WASM-VM
// syscall boundary safely:
//   - 32-bit WASM pointer vs 64-bit host pointer changes the layout
//     of every field after the pointer slot, and
//   - VMA() translates only the outermost pointer arg, not pointers
//     embedded inside structs.
// The other four primitives are flat (no pointer fields) and pass
// their descriptors as structs — they don't need this treatment.
// `ribbonDesc_t` is still the host-side contract used at
// RE_AddRibbonToScene; the dispatcher rebuilds it from the four
// scalars + the VMA-translated `points` pointer.
void trap_R_AddRibbonToScene  ( const ribbonPoint_t *points, int numPoints,
                                qhandle_t shader, int flags );
void trap_R_AddBeamToScene    ( const beamDesc_t    *desc );
// Parametric helix ribbon: submit once at fire; the renderer's persistent
// pool regenerates the evolving spiral each frame. railRibbonDesc_t is flat
// POD (no pointers), so it passes as a struct like beamDesc_t.
void trap_R_AddRailRibbonToScene( const railRibbonDesc_t *desc );
void trap_R_AddSpriteToScene  ( const spriteDesc_t  *desc );
void trap_R_EmitParticles     ( const emitterDesc_t *desc );
void trap_R_AddDecalToScene   ( const decalDesc_t   *desc );
// Lens-source occlusion oracle: register a source, read back its visibility.
void trap_R_AddLensSourceToScene( const lensSourceDesc_t *desc );
qboolean trap_R_GetLensVisibility( int id, float *outVis );
// Direction-independent halo (occlusion gated by the lens oracle).
void trap_R_AddHaloToScene( const haloDesc_t *desc );

// Inform the renderer of a particle class. Called once per class at
// registration time (not per-frame). The renderer keeps a shadow
// copy for its compute shader. WASM-safe because particleClass_t is
// flat (no embedded pointers).
void trap_R_RegisterParticleClass( particleClassHandle_t handle, const particleClass_t *cls );

// Configure GPU-resident atmospheric weather (rain / snow). Called once
// per weather change (not per-frame); the renderer's dedicated
// atmospheric pool self-spawns / integrates / collides / draws. The
// collision heightgrid ships separately because a struct-nested pointer
// can't be VM-address-translated (only top-level syscall args are).
// `atmosphericDesc_t` is flat (no pointer field), so it crosses as a struct.
void trap_R_SetAtmosphere( const atmosphericDesc_t *desc );
void trap_R_SetAtmosphereHeightgrid( const float *grid, int count );
