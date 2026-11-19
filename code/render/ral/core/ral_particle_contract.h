// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#pragma once

#include <stdint.h>

#include "../../../qcommon/wired/render/particle_class.h"

/* Backend-neutral GPU ABI for Wired particle execution.  Every production
 * RAL backend consumes this exact layout; a backend may not silently lower a
 * particleClass_t to a smaller private approximation. */
#define RAL_PARTICLE_POOL_CAPACITY 16384u
#define RAL_PARTICLE_GPU_BYTES 64u
#define RAL_PARTICLE_CLASS_GPU_BYTES 736u
#define RAL_PARTICLE_SPAWN_GPU_BYTES 96u
#define RAL_PARTICLE_SPAWN_REQUEST_MAX 1024u

typedef struct {
	int32_t calc;
	int32_t hasCurve;
	float val0;
	float val1;
	float variance;
	float pad[3];
	float samples[PARTICLE_CURVE_SAMPLES];
} ralParticleParmGpu_t;

typedef struct {
	uint32_t shader;
	uint32_t renderFlags;
	uint32_t emitMode;
	uint32_t scatterShape;
	float scatterMagnitude;
	uint32_t velocityShape;
	float axialSpeed;
	float cubeJitter;
	float coneHalfAngle;
	float lifetimeMean;
	float lifetimeJitter;
	int32_t paletteCount;
	float colorPalette[PARTICLE_CLASS_MAX_PALETTE][4];
	float colorEndMult[4];
	float sizeStart;
	float sizeEnd;
	float gravityScale;
	float drag;
	uint32_t shaderBlendIsAdditive;
	uint32_t pad1;
	uint32_t pad2;
	uint32_t pad3;
	float velocityBias[4];
	float velocityBiasJitter[4];
	float speedJitter;
	float sizeJitter;
	uint32_t colorDomain;
	uint32_t pad5;
	uint32_t frameSlots[PARTICLE_CLASS_MAX_FRAMES];
	uint32_t frameCount;
	uint32_t frameBlend;
	uint32_t framePad0;
	uint32_t framePad1;
	ralParticleParmGpu_t sizeParm;
	ralParticleParmGpu_t alphaParm;
	ralParticleParmGpu_t dragParm;
	ralParticleParmGpu_t gravityParm;
} ralParticleClassGpu_t;

typedef struct {
	float pos[3];
	float age;
	float vel[3];
	float lifetimeInv;
	uint32_t classHandle;
	uint32_t paletteIndex;
	float sizeJitterPick;
	uint32_t profileHandle;
	uint32_t stageIndex;
	uint32_t seed;
	uint32_t stageFlags;
	uint32_t packedColorTint;
} ralParticleGpu_t;

typedef struct {
	float origin[4];
	float axis[4];
	float end[4];
	float colorTint[4];
	uint32_t classHandle;
	uint32_t count;
	uint32_t firstSlot;
	uint32_t seed;
	uint32_t profileHandle;
	uint32_t stageIndex;
	uint32_t stageFlags;
	uint32_t reserved;
} ralParticleSpawnGpu_t;

typedef struct {
	float mvp[16];
	float viewLeft[4];
	float viewUp[4];
	float eyeWorld[4];
	float dt;
	uint32_t poolSize;
	uint32_t numClasses;
	uint32_t pingPongRead;
	float invResX;
	float invResY;
	/* 0: depth unavailable; +1: reversed-Z; -1: forward-Z.  The signed
	 * convention keeps soft-particle visibility backend-neutral without
	 * requiring either backend to change its native depth representation. */
	float depthValid;
	float exposureBias;
} ralParticleFrameGpu_t;

#if defined(__cplusplus)
static_assert( sizeof( ralParticleParmGpu_t ) == 64u, "RAL particle parm ABI drift" );
static_assert( sizeof( ralParticleClassGpu_t ) == RAL_PARTICLE_CLASS_GPU_BYTES,
	"RAL particle class ABI drift" );
static_assert( sizeof( ralParticleGpu_t ) == RAL_PARTICLE_GPU_BYTES,
	"RAL particle pool ABI drift" );
static_assert( sizeof( ralParticleSpawnGpu_t ) == RAL_PARTICLE_SPAWN_GPU_BYTES,
	"RAL particle spawn ABI drift" );
static_assert( sizeof( ralParticleFrameGpu_t ) == 144u, "RAL particle frame ABI drift" );
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert( sizeof( ralParticleParmGpu_t ) == 64u, "RAL particle parm ABI drift" );
_Static_assert( sizeof( ralParticleClassGpu_t ) == RAL_PARTICLE_CLASS_GPU_BYTES,
	"RAL particle class ABI drift" );
_Static_assert( sizeof( ralParticleGpu_t ) == RAL_PARTICLE_GPU_BYTES,
	"RAL particle pool ABI drift" );
_Static_assert( sizeof( ralParticleSpawnGpu_t ) == RAL_PARTICLE_SPAWN_GPU_BYTES,
	"RAL particle spawn ABI drift" );
_Static_assert( sizeof( ralParticleFrameGpu_t ) == 144u, "RAL particle frame ABI drift" );
#endif
