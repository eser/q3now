// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_TR_FIXTURES_H
#define WIRED_TR_FIXTURES_H

// Include after tr_local.h: the seam below uses its renderer and atmosphere types.

/*
Renderer test fixtures.

The smoke, lighting-reference and emissive-authority checks need content the
game would normally supply: a ribbon, a beam, an atmosphere, a lava material.
They inject it through the same public scene and frontend contracts as real
content, behind default-off CVAR_CHEAT cvars. That code lives only in
tr_fixtures.c; the product renderer sees this header and nothing else.

Configure with -DWIRED_RENDERER_FIXTURES=OFF to compile the fixtures out of a
shipping renderer: the cvars are then not registered, the predicate is a
constant 0 and the seams below vanish.
*/

#ifndef WIRED_RENDERER_FIXTURES
#define WIRED_RENDERER_FIXTURES 1
#endif

#if WIRED_RENDERER_FIXTURES

extern cvar_t *r_ralEffectsSmoke;         // default-off procedural-effects RAL native smoke
extern cvar_t *r_lightingReferenceFixture; // stable canonical lighting-reference atmosphere
extern cvar_t *r_emissiveAuthorityFixture; // typed stationary emissive authority (lava-sanctum gate)

// The effects smoke also arms backend receipts inside vk.c; they ask here.
#define WIRED_FIXTURE_EFFECTS_SMOKE() ( r_ralEffectsSmoke && r_ralEffectsSmoke->integer )

void wired_renderer_fixtures_register_cvars( void );
// Public-API fixtures: injected into the scene being built (RE_RenderScene).
void wired_renderer_fixtures_inject_scene( const refdef_t *fd );
// Frontend-contract fixtures: injected through the frontend submission seam below.
void wired_renderer_fixtures_inject_frontend( const refdef_t *view );

#else

#define WIRED_FIXTURE_EFFECTS_SMOKE() 0
#define wired_renderer_fixtures_register_cvars() ( (void)0 )
#define wired_renderer_fixtures_inject_scene( fd ) ( (void)0 )
#define wired_renderer_fixtures_inject_frontend( view ) ( (void)0 )

#endif

// Frontend submission seam, defined in tr_init.c. It is the same path game
// content takes, so a fixture exercises the real contract.
qhandle_t FrontendRegisterPrimitive( const char *name );
qboolean FrontendTrySetAtmosphere( const atmosphericDesc_t *atmosphere );
qboolean FrontendTryAddAtmosphereEmitter( const atmosphereEmitter_t *emitter );
void FrontendAddAtmosphereEmitter( const atmosphereEmitter_t *emitter );
qboolean FrontendTryRegisterAtmosphereEffectProfile( uint32_t handle, const atmosphereEffectProfile_t *profile );
qboolean FrontendTryAddAtmosphereMediaVolume( const atmosphereMediaVolume_t *volume );
void FrontendAddAtmosphereMediaVolume( const atmosphereMediaVolume_t *volume );
renderSubmissionState_t *wired_renderer_frontend_submission( void );

#endif
