// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#ifndef WIRED_RAL_UI_PRESENTATION_H
#define WIRED_RAL_UI_PRESENTATION_H
#include "q_shared.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RAL_UI_PRESENTATION_SCHEMA_VERSION 1u
#define RAL_UI_PRESENTATION_RECEIPT_SCHEMA_VERSION 1u
#define RAL_UI_INERTIA_SCHEMA_VERSION 1u
#define RAL_UI_PRESENTATION_Q16_ONE 65536
#define RAL_UI_PRESENTATION_STAGE_COUNT 6u
typedef enum { RAL_UI_PROFILE_NORMAL=1,RAL_UI_PROFILE_STATIC,RAL_UI_PROFILE_ACCESSIBILITY,RAL_UI_PROFILE_COMPETITIVE } ralUiProfile_t;
typedef enum { RAL_UI_OUTPUT_SDR=1,RAL_UI_OUTPUT_HDR } ralUiOutput_t;
typedef enum { RAL_UI_STAGE_SCENE_TONEMAP=1,RAL_UI_STAGE_HUD,RAL_UI_STAGE_EFFECTS,
	RAL_UI_STAGE_CONSOLE,RAL_UI_STAGE_CURSOR,RAL_UI_STAGE_CAPTURE } ralUiStage_t;
typedef enum { RAL_UI_EFFECT_CHROMATIC=1u<<0,RAL_UI_EFFECT_BARREL=1u<<1,
	RAL_UI_EFFECT_CRT=1u<<2,RAL_UI_EFFECT_HELMET=1u<<3 } ralUiEffectBit_t;
#define RAL_UI_EFFECT_ALL ((uint32_t)(RAL_UI_EFFECT_CHROMATIC|RAL_UI_EFFECT_BARREL|RAL_UI_EFFECT_CRT|RAL_UI_EFFECT_HELMET))
typedef struct { uint32_t schemaVersion;uint64_t policyGeneration;ralUiProfile_t profile;ralUiOutput_t output;
	uint32_t viewportWidth,viewportHeight,viewportScaleQ16,effectMask,effectStrengthQ16[4];
	uint32_t safeAreaQ16,readabilityMask;qboolean inertiaEnabled;uint32_t springQ16,dampingQ16; } ralUiPresentationRequest_t;
typedef struct { uint32_t schemaVersion;uint64_t policyGeneration;ralUiProfile_t profile;ralUiOutput_t output;
	uint32_t stageCount;ralUiStage_t stages[RAL_UI_PRESENTATION_STAGE_COUNT];uint32_t activeEffectMask;
	uint32_t effectStrengthQ16[4],effectWorkPixels,safeAreaQ16,readabilityMask;
	qboolean inertiaEnabled;uint32_t springQ16,dampingQ16;qboolean ready; } ralUiPresentationReceipt_t;
typedef struct { uint32_t schemaVersion;uint64_t policyGeneration,transactionGeneration;
	int32_t positionXQ16,positionYQ16,velocityXQ16,velocityYQ16;qboolean ready; } ralUiInertiaState_t;
qboolean Ral_UiPresentationResolve(const ralUiPresentationRequest_t*request,uint32_t stageCapacity,
	ralUiPresentationReceipt_t*outReceipt);
qboolean Ral_UiInertiaInit(const ralUiPresentationReceipt_t*policy,uint64_t transactionGeneration,
	ralUiInertiaState_t*outState);
qboolean Ral_UiInertiaAdvance(ralUiInertiaState_t*state,const ralUiPresentationReceipt_t*policy,
	int32_t targetXQ16,int32_t targetYQ16,uint32_t deltaMilliseconds,uint64_t transactionGeneration,
	ralUiInertiaState_t*outState);
qboolean Ral_UiInertiaStateExact(const ralUiInertiaState_t*a,const ralUiInertiaState_t*b);
#ifdef __cplusplus
}
#endif
#endif
