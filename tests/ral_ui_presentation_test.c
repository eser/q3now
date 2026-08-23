// SPDX-License-Identifier: GPL-3.0-or-later
#include "ral_ui_presentation.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static ralUiPresentationRequest_t Request(ralUiOutput_t output){ralUiPresentationRequest_t q;memset(&q,0,sizeof(q));
	q.schemaVersion=RAL_UI_PRESENTATION_SCHEMA_VERSION;q.policyGeneration=7u;q.profile=RAL_UI_PROFILE_NORMAL;q.output=output;
	q.viewportWidth=1280u;q.viewportHeight=720u;q.viewportScaleQ16=RAL_UI_PRESENTATION_Q16_ONE;q.springQ16=RAL_UI_PRESENTATION_Q16_ONE*2u;
	q.dampingQ16=RAL_UI_PRESENTATION_Q16_ONE;q.inertiaEnabled=qtrue;return q;}
static int Fixture(ralUiOutput_t output){ralUiPresentationRequest_t q=Request(output),bad;ralUiPresentationReceipt_t p,before;
	ralUiInertiaState_t a,b,out,beforeState;CHECK(Ral_UiPresentationResolve(&q,6u,&p)&&p.activeEffectMask==0u&&p.effectWorkPixels==0u);
	CHECK(p.stages[0]==RAL_UI_STAGE_SCENE_TONEMAP&&p.stages[1]==RAL_UI_STAGE_HUD&&p.stages[2]==RAL_UI_STAGE_EFFECTS
		&&p.stages[3]==RAL_UI_STAGE_CONSOLE&&p.stages[4]==RAL_UI_STAGE_CURSOR&&p.stages[5]==RAL_UI_STAGE_CAPTURE);
	q.effectMask=RAL_UI_EFFECT_CHROMATIC|RAL_UI_EFFECT_HELMET;q.effectStrengthQ16[0]=100u;q.effectStrengthQ16[3]=200u;
	CHECK(Ral_UiPresentationResolve(&q,6u,&p)&&p.effectWorkPixels==1280u*720u);
	CHECK(Ral_UiInertiaInit(&p,1u,&a)&&Ral_UiInertiaInit(&p,1u,&b));
	CHECK(Ral_UiInertiaAdvance(&a,&p,RAL_UI_PRESENTATION_Q16_ONE,0,16u,2u,&out));
	CHECK(Ral_UiInertiaAdvance(&a,&p,RAL_UI_PRESENTATION_Q16_ONE,0,16u,3u,&out));
	CHECK(Ral_UiInertiaAdvance(&b,&p,RAL_UI_PRESENTATION_Q16_ONE,0,32u,3u,&out));
	CHECK(Ral_UiInertiaStateExact(&a,&b));beforeState=a;
	CHECK(!Ral_UiInertiaAdvance(&a,&p,RAL_UI_PRESENTATION_Q16_ONE,0,101u,4u,&out)&&!memcmp(&a,&beforeState,sizeof(a)));
	q.profile=RAL_UI_PROFILE_COMPETITIVE;CHECK(Ral_UiPresentationResolve(&q,6u,&p)&&p.activeEffectMask==0u&&!p.inertiaEnabled);
	CHECK(Ral_UiInertiaInit(&p,1u,&a)&&Ral_UiInertiaAdvance(&a,&p,1000,1000,16u,2u,&out)
		&&out.positionXQ16==0&&out.velocityXQ16==0);
	bad=q;bad.effectMask=1u<<30;before=p;CHECK(!Ral_UiPresentationResolve(&bad,6u,&p)&&!memcmp(&p,&before,sizeof(p)));
	CHECK(!Ral_UiPresentationResolve(&q,5u,&p));return 0;}
int main(void){CHECK(Fixture(RAL_UI_OUTPUT_SDR)==0&&Fixture(RAL_UI_OUTPUT_HDR)==0);puts("ral_ui_presentation_test: ok");return 0;}
