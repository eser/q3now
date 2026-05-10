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
void trap_R_AddSpriteToScene  ( const spriteDesc_t  *desc );
void trap_R_EmitParticles     ( const emitterDesc_t *desc );
void trap_R_AddDecalToScene   ( const decalDesc_t   *desc );

// Inform the renderer of a particle class. Called once per class at
// registration time (not per-frame). The renderer keeps a shadow
// copy for its compute shader. WASM-safe because particleClass_t is
// flat (no embedded pointers).
void trap_R_RegisterParticleClass( particleClassHandle_t handle, const particleClass_t *cls );
