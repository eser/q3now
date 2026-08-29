// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_SUBMISSION_EFFECTS_H
#define WIRED_RENDER_FRONTEND_SUBMISSION_EFFECTS_H

#include "tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_SUBMISSION_MAX_POLY_COMMANDS				  4096u
#define RENDER_SUBMISSION_MAX_POLY_VERTICES				  262144u
#define RENDER_SUBMISSION_MAX_LIGHTS					  4096u
#define RENDER_SUBMISSION_MAX_ATMOSPHERE_EMITTERS		  256u
#define RENDER_SUBMISSION_MAX_SURFACE_CLIMATE_TILES		  256u
#define RENDER_SUBMISSION_MAX_ATMOSPHERE_SURFACE_EVENTS	  256u
#define RENDER_SUBMISSION_MAX_ATMOSPHERE_MEDIA_VOLUMES	  64u
#define RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS 8u
#define RENDER_SUBMISSION_SURFACE_CLIMATE_TILE_SIZE		  128.0f
#define RENDER_SUBMISSION_MAX_EFFECT_SPRITES              2048u
#define RENDER_SUBMISSION_MAX_EFFECT_EMITTERS              512u
#define RENDER_SUBMISSION_MAX_EFFECT_DECALS               4096u
#define RENDER_SUBMISSION_MAX_EFFECT_RIBBONS               512u
#define RENDER_SUBMISSION_MAX_EFFECT_RIBBON_POINTS        8192u

typedef struct {
	qhandle_t material;
	uint32_t  firstVertex;
	uint32_t  verticesPerPolygon;
	uint32_t  polygonCount;
} renderPolyCommand_t;

typedef struct {
	float	 origin[3];
	float	 end[3];
	float	 intensity;
	float	 color[3];
	qboolean hasEnd;
	uint64_t emissiveAuthorityHash;
	uint32_t sourceFlags;
	uint32_t shadowPriority;
	qboolean authoredEmissive;
} renderLightCommand_t;

typedef struct {
	uint32_t firstPoint;
	uint32_t pointCount;
	qhandle_t material;
	uint32_t flags;
	float uvScroll[2];
} renderEffectRibbonCommand_t;

typedef struct {
	uint64_t digest;
	uint32_t spriteCount;
	uint32_t emitterCount;
	uint32_t decalCount;
	uint32_t ribbonCount;
	uint32_t ribbonPointCount;
	uint32_t droppedCount;
} renderEffectPrimitiveSnapshot_t;

typedef struct {
	atmosphereFrameState_t state;
	uint64_t			   generation;
	uint64_t			   digest;
	uint32_t			   emitterCount;
	qboolean			   active;
} renderAtmosphereSnapshot_t;

typedef struct {
	float wetness;
	float frost;
	float snow;
	float melt;
} renderSurfaceClimateTargets_t;

typedef struct {
	int32_t	 tileX;
	int32_t	 tileY;
	float	 wetness;
	float	 frost;
	float	 snow;
	float	 melt;
	float	 lastTimelineSeconds;
	uint32_t lastEventId;
	uint32_t eventMask;
	uint64_t generation;
} renderSurfaceClimateTile_t;

typedef struct {
	uint64_t generation;
	uint64_t digest;
	uint32_t tileCount;
	uint32_t frameEventCount;
	uint32_t evictionCount;
} renderSurfaceClimateSnapshot_t;

typedef struct {
	uint64_t digest;
	uint32_t count;
	uint32_t droppedCount;
} renderAtmosphereMediaSnapshot_t;

typedef struct {
	atmosphereEmitter_t		emitter;
	atmosphereEffectStage_t stage;
	particleClass_t			particleClass;
	uint32_t				firstParticle;
	uint32_t				particleCount;
	float					effectiveIntensity;
} renderAtmosphereEffectWorkload_t;

typedef struct {
	uint64_t						 digest;
	uint32_t						 sourceEmitterCount;
	uint32_t						 admittedEmitterCount;
	uint32_t						 droppedEmitterCount;
	uint32_t						 requestedParticleCount;
	uint32_t						 admittedParticleCount;
	uint32_t						 droppedParticleCount;
	renderAtmosphereEffectWorkload_t workloads[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS];
} renderAtmosphereEffectWorkloadSnapshot_t;

#define RENDER_SUBMISSION_ATMOSPHERE_EFFECT_GPU_VECTORS 6u
typedef struct {
	float vectors[RENDER_SUBMISSION_ATMOSPHERE_EFFECT_GPU_VECTORS][4];
} renderAtmosphereEffectGpuWorkload_t;

typedef struct renderSubmissionState_s renderSubmissionState_t;

qboolean RenderSubmission_EffectSnapshots( const renderSubmissionState_t *state,
										   const renderPolyCommand_t **outPolygons, uint32_t *outPolygonCommandCount,
										   const polyVert_t **outVertices, uint32_t *outVertexCount,
										   const renderLightCommand_t **outLights, uint32_t *outLightCount );
qboolean RenderSubmission_AddEffectSprite( renderSubmissionState_t *state,
	const spriteDesc_t *sprite );
qboolean RenderSubmission_AddEffectEmitter( renderSubmissionState_t *state,
	const emitterDesc_t *emitter );
qboolean RenderSubmission_AddEffectDecal( renderSubmissionState_t *state,
	const decalDesc_t *decal );
qboolean RenderSubmission_AddEffectRibbon( renderSubmissionState_t *state,
	const ribbonDesc_t *ribbon );
qboolean RenderSubmission_EffectPrimitiveSnapshots(
	const renderSubmissionState_t *state,
	renderEffectPrimitiveSnapshot_t *outSnapshot,
	const spriteDesc_t **outSprites,
	const emitterDesc_t **outEmitters,
	const decalDesc_t **outDecals,
	const renderEffectRibbonCommand_t **outRibbons,
	const ribbonPoint_t **outRibbonPoints );
uint32_t RenderSubmission_AtmosphereLightCount( const renderSubmissionState_t *state );
uint32_t RenderSubmission_AtmosphereShadowedLightCount( const renderSubmissionState_t *state );

qboolean RenderSubmission_InitAtmosphere( renderSubmissionState_t *state );
qboolean RenderSubmission_SetAtmosphere( renderSubmissionState_t *state, const atmosphereFrameState_t *atmosphere );
qboolean RenderSubmission_RegisterAtmosphereEffectProfile( renderSubmissionState_t *state, uint32_t handle,
														   const atmosphereEffectProfile_t *profile );
qboolean RenderSubmission_GetAtmosphereEffectProfile( const renderSubmissionState_t *state, uint32_t handle,
													  atmosphereEffectProfile_t *outProfile );
qboolean RenderSubmission_RegisterParticleClass( renderSubmissionState_t *state, particleClassHandle_t handle,
												 const particleClass_t *particleClass );
qboolean RenderSubmission_GetParticleClass( const renderSubmissionState_t *state, particleClassHandle_t handle,
											particleClass_t *outParticleClass );
qboolean RenderSubmission_AtmosphereEffectWorkloadSnapshot( const renderSubmissionState_t *state, uint32_t maxParticles,
															renderAtmosphereEffectWorkloadSnapshot_t *outSnapshot );
qboolean RenderSubmission_AtmosphereEffectGpuPayload(
	const renderSubmissionState_t *state, uint32_t maxParticles, uint32_t firstParticle,
	renderAtmosphereEffectGpuWorkload_t		  outWorkloads[RENDER_SUBMISSION_MAX_ATMOSPHERE_EFFECT_WORKLOADS],
	renderAtmosphereEffectWorkloadSnapshot_t *outSnapshot );
qboolean RenderSubmission_AddAtmosphereEmitter( renderSubmissionState_t *state, const atmosphereEmitter_t *emitter );
float	 RenderSubmission_AtmosphereEmitterIntensity( const atmosphereFrameState_t *atmosphere,
													  const atmosphereEmitter_t	   *emitter );
qboolean RenderSubmission_AtmosphereSurfaceTargets( const atmosphereFrameState_t  *atmosphere,
													renderSurfaceClimateTargets_t *outTargets );
qboolean RenderSubmission_AddAtmosphereSurfaceEvent( renderSubmissionState_t		*state,
													 const atmosphereSurfaceEvent_t *event );
qboolean RenderSubmission_AddAtmosphereMediaVolume( renderSubmissionState_t		  *state,
													const atmosphereMediaVolume_t *volume );
qboolean RenderSubmission_AtmosphereMediaSnapshot( const renderSubmissionState_t   *state,
												   renderAtmosphereMediaSnapshot_t *outSnapshot,
												   const atmosphereMediaVolume_t  **outVolumes );
qboolean RenderSubmission_AtmosphereSurfaceSnapshot( const renderSubmissionState_t	   *state,
													 renderSurfaceClimateSnapshot_t	   *outSnapshot,
													 const renderSurfaceClimateTile_t **outTiles );
void	 RenderSubmission_ClearAtmosphereSurfaceEvents( renderSubmissionState_t *state );
void	 RenderSubmission_ResetAtmosphereSurface( renderSubmissionState_t *state );
void	 RenderSubmission_ClearAtmosphereEmitters( renderSubmissionState_t *state );
void	 RenderSubmission_ClearAtmosphereMediaVolumes( renderSubmissionState_t *state );
uint64_t RenderSubmission_AtmosphereDigest( const renderSubmissionState_t *state );
qboolean RenderSubmission_AtmosphereSnapshot( const renderSubmissionState_t *state,
											  renderAtmosphereSnapshot_t	*outSnapshot,
											  const atmosphereEmitter_t	   **outEmitters );

#ifdef __cplusplus
}
#endif

#endif
