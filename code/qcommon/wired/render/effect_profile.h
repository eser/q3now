// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*
effect_profile.h — backend-neutral WiredFX authored recipe contract

This is an orchestration ABI, not a particle simulator and not an idDecl
parser.  It keeps the complete, source-verified id Tech 5 idDeclFX action
vocabulary while expressing every reference as a stable integer handle.
Pointers, backend-native objects and game-private types are forbidden so a
profile can cross the native/WASM seam once at registration time.
*/
#pragma once

#include "../../q_shared.h"

#define WIRED_FX_PROFILE_SCHEMA_VERSION 1u
#define WIRED_FX_EVENT_SCHEMA_VERSION 1u
#define WIRED_FX_MAX_ACTIONS 32u
#define WIRED_FX_MAX_PROFILES 64u
#define WIRED_FX_MAX_ENV_RENDER_PARMS 16u
#define WIRED_FX_MAX_FRAME_RENDER_PARMS 16u
#define WIRED_FX_NO_ACTION UINT32_MAX

/* Stable authored profile handles. Append only; recipes live in modfiles. */
#define WIRED_FX_PROFILE_ROCKET_EXPLOSION 1u
#define WIRED_FX_PROFILE_ROCKET_DETONATION 2u
#define WIRED_FX_PROFILE_ROCKET_UNDERWATER 3u

typedef enum {
	WIRED_FX_ACTION_LIGHT = 0,
	WIRED_FX_ACTION_PARTICLE,
	WIRED_FX_ACTION_DECAL,
	WIRED_FX_ACTION_DECAL2,
	WIRED_FX_ACTION_MODEL,
	WIRED_FX_ACTION_SOUND,
	WIRED_FX_ACTION_SCREEN_SHAKE,
	WIRED_FX_ACTION_CONTROLLER_SHAKE,
	WIRED_FX_ACTION_WIND,
	WIRED_FX_ACTION_RENDER_PARM,
	WIRED_FX_ACTION_ENV_OVERRIDE,
	WIRED_FX_ACTION_ENV_CHANGE,
	WIRED_FX_ACTION_FLARE,
	WIRED_FX_ACTION_RADIAL_BLUR,
	WIRED_FX_ACTION_RIBBON,
	WIRED_FX_ACTION_FADE_PARENT,
	WIRED_FX_ACTION_GODRAY,
	WIRED_FX_ACTION_COUNT
} wiredFxActionType_t;

typedef enum {
	WIRED_FX_ORIGIN_START = 0,
	WIRED_FX_ORIGIN_TRACK,
	WIRED_FX_ORIGIN_TRACK_LOCAL,
	WIRED_FX_ORIGIN_EXTERNAL
} wiredFxOriginType_t;

typedef enum {
	WIRED_FX_ROTATION_START_AXIS = 0,
	WIRED_FX_ROTATION_START_AXIS_PARENT,
	WIRED_FX_ROTATION_TRACK_AXIS,
	WIRED_FX_ROTATION_TRACK_AXIS_PARENT,
	WIRED_FX_ROTATION_TRACK_LOCAL_AXIS,
	WIRED_FX_ROTATION_EXPLICIT_ANGLES,
	WIRED_FX_ROTATION_EXPLICIT_CURVES,
	WIRED_FX_ROTATION_EXPLICIT_CURVES_LOCAL,
	WIRED_FX_ROTATION_EXTERNAL
} wiredFxRotationType_t;

#define WIRED_FX_ACTION_RESTART                 0x00000001u
#define WIRED_FX_ACTION_LOOP                    0x00000002u
#define WIRED_FX_ACTION_NO_SHADOWS              0x00000004u
#define WIRED_FX_ACTION_FILTER_HIGH_VIOLENCE    0x00000008u
#define WIRED_FX_ACTION_OPTIONAL_RESOURCE       0x00000010u
#define WIRED_FX_ACTION_TRACK_VELOCITY          0x00000020u
#define WIRED_FX_ACTION_USE_GPU_LIFECYCLE       0x00000040u

typedef struct {
	uint32_t handle;
	float value[4];
} wiredFxRenderParmValue_t;

typedef struct {
	uint32_t material;
	float radius[3];
	float intensity;
} wiredFxLightAction_t;

typedef struct {
	uint32_t particleClass;
	uint32_t maxParticles;
	float velocityScale;
	float minVelocity;
	float spawnRate;
	float trailSpacing;
	float screenExcludeAngle;
	uint32_t reserved;
} wiredFxParticleAction_t;

typedef struct {
	uint32_t material;
	float angle;
	float depth;
	float size;
} wiredFxDecalAction_t;

typedef struct {
	uint32_t model;
	uint32_t material;
} wiredFxModelAction_t;

typedef struct {
	uint32_t sound;
	int32_t channel;
} wiredFxSoundAction_t;

typedef struct {
	float magnitude;
	float controllerScale;
	float maxAngles[3];
	float maxOffset[3];
} wiredFxScreenShakeAction_t;

typedef struct {
	float highMagnitude;
	float lowMagnitude;
	float highDuration;
	float lowDuration;
	uint32_t amplitudeCurve;
} wiredFxControllerShakeAction_t;

typedef struct {
	float angle;
	float multiplier;
	float strength[2];
} wiredFxWindAction_t;

typedef struct {
	uint32_t handle;
	float value[4];
} wiredFxRenderParmAction_t;

typedef struct {
	uint32_t environment;
	uint32_t renderParmCount;
	wiredFxRenderParmValue_t renderParms[WIRED_FX_MAX_ENV_RENDER_PARMS];
} wiredFxEnvironmentAction_t;

typedef struct {
	uint32_t flare;
	float position[3];
	uint32_t autosprite;
} wiredFxFlareAction_t;

typedef struct {
	float maxScale;
} wiredFxRadialBlurAction_t;

typedef struct {
	uint32_t ribbon;
} wiredFxRibbonAction_t;

typedef struct {
	uint32_t material;
	float color[4];
	float colorScale;
	uint32_t size;
	uint32_t sourceSize;
} wiredFxGodrayAction_t;

typedef union {
	wiredFxLightAction_t light;
	wiredFxParticleAction_t particle;
	wiredFxDecalAction_t decal;
	wiredFxModelAction_t model;
	wiredFxSoundAction_t sound;
	wiredFxScreenShakeAction_t screenShake;
	wiredFxControllerShakeAction_t controllerShake;
	wiredFxWindAction_t wind;
	wiredFxRenderParmAction_t renderParm;
	wiredFxEnvironmentAction_t environment;
	wiredFxFlareAction_t flare;
	wiredFxRadialBlurAction_t radialBlur;
	wiredFxRibbonAction_t ribbon;
	wiredFxGodrayAction_t godray;
} wiredFxActionPayload_t;

typedef struct {
	uint32_t type;                 // wiredFxActionType_t
	uint32_t flags;                // WIRED_FX_ACTION_* bits
	uint32_t actionId;             // stable authoring identity; non-zero
	uint32_t groupId;              // optional stable group identity
	uint32_t parentAction;         // WIRED_FX_NO_ACTION or an earlier action
	uint32_t fireAction;           // WIRED_FX_NO_ACTION or any action index
	uint32_t originType;           // wiredFxOriginType_t
	uint32_t rotationType;         // wiredFxRotationType_t
	uint64_t startConditionMask;
	uint64_t stopConditionMask;
	uint64_t extraConditionMask;
	float delay[2];                // random range in seconds
	float duration;
	float fadeInTime;
	float fadeOutTime;
	float lodNear;
	float lodFar;
	float boundsRadius;
	float offset[3];
	float rotationDegrees[3];
	float color[4];
	uint32_t maxInstances;         // hard action-local admission budget
	uint32_t reserved[3];
	wiredFxActionPayload_t payload;
} wiredFxAction_t;

typedef struct {
	uint32_t schemaVersion;
	uint32_t actionCount;
	uint32_t maxActiveActions;
	uint32_t maxInstances;
	uint32_t parentProfile;        // zero or an earlier registered handle
	uint32_t actionOverrideMask;   // child action slots replacing parent slots
	uint32_t flags;                // reserved; zero in schema 1
	uint32_t seed;
	float duration;                // zero derives the maximum from actions
	float lodNear;
	float lodFar;
	float boundsRadius;
	wiredFxAction_t actions[WIRED_FX_MAX_ACTIONS];
	uint32_t reserved[8];
} wiredFxProfile_t;

#define WIRED_FX_EVENT_HAS_END_ORIGIN 0x00000001u
#define WIRED_FX_EVENT_HAS_VELOCITY   0x00000002u
#define WIRED_FX_EVENT_UNDERWATER     0x00000004u
#define WIRED_FX_EVENT_FREE_AIR       0x00000008u

#define WIRED_FX_CONDITION_MATERIAL_DEFAULT 0x0000000000000001ull
#define WIRED_FX_CONDITION_MATERIAL_METAL   0x0000000000000002ull
#define WIRED_FX_CONDITION_MATERIAL_FLESH   0x0000000000000004ull
#define WIRED_FX_CONDITION_UNDERWATER       0x0000000000000008ull
#define WIRED_FX_CONDITION_FREE_AIR         0x0000000000000010ull

/*
One semantic effect occurrence. The event carries only per-occurrence state;
all authored composition and lifecycle policy is referenced by `profile`.
The app may classify a hit surface with materialClass without exposing a
renderer material or native backend handle.
*/
typedef struct {
	uint32_t schemaVersion;
	uint32_t profile;
	uint32_t eventId;
	uint32_t flags;
	uint32_t seed;
	uint32_t materialClass;
	uint64_t conditionMask;
	uint64_t extraConditionMask;
	float startTimeSeconds;
	float origin[3];
	float endOrigin[3];
	float velocity[3];
	float axis[9];
	float color[4];
	float intensity;
	float sizeScale;
	uint32_t reserved[8];
} wiredFxEvent_t;

/*
Per-frame presentation state produced by typed actions that affect the whole
view rather than submitting scene primitives. It remains a pointer-free part
of refdef_t, so every renderer backend receives the same bounded contract.
Backends may degrade an unsupported presentation feature, but the client never
passes a native object or backend-specific command through this seam.
*/
#define WIRED_FX_FRAME_SCHEMA_VERSION 1u
#define WIRED_FX_FRAME_HAS_WIND          0x00000001u
#define WIRED_FX_FRAME_HAS_RENDER_PARMS  0x00000002u
#define WIRED_FX_FRAME_HAS_ENVIRONMENT   0x00000004u
#define WIRED_FX_FRAME_HAS_RADIAL_BLUR   0x00000008u

typedef struct {
	uint32_t schemaVersion;
	uint32_t flags;
	uint32_t environment;
	uint32_t renderParmCount;
	float windDirection[3];
	float windStrength;
	float radialBlurScale;
	float reservedFloat[3];
	wiredFxRenderParmValue_t renderParms[WIRED_FX_MAX_FRAME_RENDER_PARMS];
	uint32_t reserved[8];
} wiredFxFrameState_t;
