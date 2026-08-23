// SPDX-License-Identifier: GPL-3.0-or-later
#include "ral_ui_presentation.h"
#include <limits.h>
#include <string.h>
#define UI_TARGET_LIMIT (8*RAL_UI_PRESENTATION_Q16_ONE)
#define UI_DT_Q16 66
static qboolean PolicyValid(const ralUiPresentationReceipt_t*p){static const ralUiStage_t order[6]={RAL_UI_STAGE_SCENE_TONEMAP,
	RAL_UI_STAGE_HUD,RAL_UI_STAGE_EFFECTS,RAL_UI_STAGE_CONSOLE,RAL_UI_STAGE_CURSOR,RAL_UI_STAGE_CAPTURE};
	return p&&p->schemaVersion==RAL_UI_PRESENTATION_RECEIPT_SCHEMA_VERSION&&p->policyGeneration
	&&p->profile>=RAL_UI_PROFILE_NORMAL&&p->profile<=RAL_UI_PROFILE_COMPETITIVE
	&&p->output>=RAL_UI_OUTPUT_SDR&&p->output<=RAL_UI_OUTPUT_HDR&&p->stageCount==6u
	&&!memcmp(p->stages,order,sizeof(order))&&!(p->activeEffectMask&~RAL_UI_EFFECT_ALL)
	&&p->ready==qtrue;}
qboolean Ral_UiPresentationResolve(const ralUiPresentationRequest_t*q,uint32_t capacity,ralUiPresentationReceipt_t*out){
	ralUiPresentationReceipt_t v;uint64_t pixels;uint32_t i;static const ralUiStage_t order[6]={RAL_UI_STAGE_SCENE_TONEMAP,
	RAL_UI_STAGE_HUD,RAL_UI_STAGE_EFFECTS,RAL_UI_STAGE_CONSOLE,RAL_UI_STAGE_CURSOR,RAL_UI_STAGE_CAPTURE};
	if(!out||!q||q->schemaVersion!=RAL_UI_PRESENTATION_SCHEMA_VERSION||!q->policyGeneration
		||q->profile<RAL_UI_PROFILE_NORMAL||q->profile>RAL_UI_PROFILE_COMPETITIVE
		||q->output<RAL_UI_OUTPUT_SDR||q->output>RAL_UI_OUTPUT_HDR||!q->viewportWidth||!q->viewportHeight
		||q->viewportScaleQ16<RAL_UI_PRESENTATION_Q16_ONE/4u||q->viewportScaleQ16>RAL_UI_PRESENTATION_Q16_ONE*4u
		||(q->effectMask&~RAL_UI_EFFECT_ALL)||q->safeAreaQ16>RAL_UI_PRESENTATION_Q16_ONE/4u
		||q->readabilityMask&~RAL_UI_EFFECT_ALL||capacity<RAL_UI_PRESENTATION_STAGE_COUNT
		||(q->inertiaEnabled!=qfalse&&q->inertiaEnabled!=qtrue)||q->springQ16>RAL_UI_PRESENTATION_Q16_ONE*4u
		||q->dampingQ16>RAL_UI_PRESENTATION_Q16_ONE*4u)return qfalse;
	for(i=0u;i<4u;i++)if(q->effectStrengthQ16[i]>RAL_UI_PRESENTATION_Q16_ONE)return qfalse;
	pixels=(uint64_t)q->viewportWidth*q->viewportHeight;if(pixels>UINT32_MAX)return qfalse;
	memset(&v,0,sizeof(v));v.schemaVersion=RAL_UI_PRESENTATION_RECEIPT_SCHEMA_VERSION;v.policyGeneration=q->policyGeneration;
	v.profile=q->profile;v.output=q->output;v.stageCount=6u;memcpy(v.stages,order,sizeof(order));
	v.safeAreaQ16=q->safeAreaQ16;v.readabilityMask=q->readabilityMask;
	if(q->profile==RAL_UI_PROFILE_NORMAL){v.activeEffectMask=q->effectMask;memcpy(v.effectStrengthQ16,q->effectStrengthQ16,sizeof(v.effectStrengthQ16));
		v.inertiaEnabled=q->inertiaEnabled;v.springQ16=q->springQ16;v.dampingQ16=q->dampingQ16;}
	if(v.activeEffectMask)v.effectWorkPixels=(uint32_t)pixels;v.ready=qtrue;if(!PolicyValid(&v))return qfalse;*out=v;return qtrue;
}
qboolean Ral_UiInertiaInit(const ralUiPresentationReceipt_t*p,uint64_t generation,ralUiInertiaState_t*out){ralUiInertiaState_t v;
	if(!out||!generation||!PolicyValid(p))return qfalse;memset(&v,0,sizeof(v));v.schemaVersion=RAL_UI_INERTIA_SCHEMA_VERSION;
	v.policyGeneration=p->policyGeneration;v.transactionGeneration=generation;v.ready=qtrue;*out=v;return qtrue;}
static qboolean Step(int32_t target,int32_t spring,int32_t damping,int32_t*position,int32_t*velocity){
	int64_t error=(int64_t)target-*position;int64_t acceleration=((error*spring)-((int64_t)*velocity*damping))/RAL_UI_PRESENTATION_Q16_ONE;
	int64_t nextVelocity=(int64_t)*velocity+(acceleration*UI_DT_Q16)/RAL_UI_PRESENTATION_Q16_ONE;
	int64_t nextPosition=(int64_t)*position+(nextVelocity*UI_DT_Q16)/RAL_UI_PRESENTATION_Q16_ONE;
	if(nextVelocity<=INT32_MIN||nextVelocity>=INT32_MAX||nextPosition<=INT32_MIN||nextPosition>=INT32_MAX)return qfalse;
	*velocity=(int32_t)nextVelocity;*position=(int32_t)nextPosition;return qtrue;}
qboolean Ral_UiInertiaAdvance(ralUiInertiaState_t*s,const ralUiPresentationReceipt_t*p,int32_t tx,int32_t ty,
		uint32_t delta,uint64_t generation,ralUiInertiaState_t*out){ralUiInertiaState_t v;uint32_t i;
	if(!out||!s||s->schemaVersion!=RAL_UI_INERTIA_SCHEMA_VERSION||s->ready!=qtrue||!PolicyValid(p)
		||s->policyGeneration!=p->policyGeneration||generation<=s->transactionGeneration||!delta||delta>100u
		||tx<=-UI_TARGET_LIMIT||tx>=UI_TARGET_LIMIT||ty<=-UI_TARGET_LIMIT||ty>=UI_TARGET_LIMIT)return qfalse;
	v=*s;v.transactionGeneration=generation;if(!p->inertiaEnabled){v.positionXQ16=v.positionYQ16=v.velocityXQ16=v.velocityYQ16=0;}
	else for(i=0u;i<delta;i++)if(!Step(tx,(int32_t)p->springQ16,(int32_t)p->dampingQ16,&v.positionXQ16,&v.velocityXQ16)
		||!Step(ty,(int32_t)p->springQ16,(int32_t)p->dampingQ16,&v.positionYQ16,&v.velocityYQ16))return qfalse;
	*s=v;*out=v;return qtrue;}
qboolean Ral_UiInertiaStateExact(const ralUiInertiaState_t*a,const ralUiInertiaState_t*b){return a&&b
	&&a->schemaVersion==RAL_UI_INERTIA_SCHEMA_VERSION&&a->policyGeneration&&a->transactionGeneration&&a->ready==qtrue
	&&b->schemaVersion==RAL_UI_INERTIA_SCHEMA_VERSION&&b->ready==qtrue&&!memcmp(a,b,sizeof(*a));}
