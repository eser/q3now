// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_static_lighting.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
#define Q(x) ((x)*RAL_LIGHT_Q16_ONE)

static int Product(void){
	ralStaticLightingDescription_t d;ralStaticLightingReceipt_t p;ralStaticLightingPlanRequest_t q;
	ralStaticLightingPlanReceipt_t plan,before;memset(&d,0,sizeof(d));d.schemaVersion=RAL_STATIC_LIGHTING_SCHEMA_VERSION;
	d.productGeneration=1u;d.cacheKey=2u;d.producerVersion=3u;d.geometryHash=4u;d.materialHash=5u;d.staticBakeHash=6u;
	d.radiancePayloadHash=7u;d.directionPayloadHash=8u;d.stationaryVisibilityPayloadHash=9u;
	d.pageWidth=d.pageHeight=128u;d.pageCount=3u;d.bounceCount=4u;d.energyClampQ16=Q(16);
	d.primaryEncoding=RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;d.fallbackEncoding=RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16;
	d.hasStationaryVisibility=qtrue;CHECK(Ral_StaticLightingBuild(&d,&p)&&Ral_StaticLightingReceiptValid(&p));
	memset(&q,0,sizeof(q));q.schemaVersion=RAL_STATIC_LIGHTING_PLAN_SCHEMA_VERSION;q.frameGeneration=11u;q.product=p;
	q.capabilities.sampledRgb9e5=q.capabilities.textureArrays=qtrue;
	CHECK(Ral_StaticLightingPlanBuild(&q,&plan)&&plan.mode==RAL_STATIC_LIGHTING_PLAN_PRIMARY&&plan.textureSampleCount==3u);
	CHECK(Ral_StaticLightingPlanReceiptValid(&plan));q.capabilities.sampledRgb9e5=qfalse;
	q.capabilities.sampledRgba16Float=q.capabilities.sampledRg16Snorm=qtrue;
	CHECK(Ral_StaticLightingPlanBuild(&q,&plan)&&plan.mode==RAL_STATIC_LIGHTING_PLAN_FALLBACK);
	before=plan;q.capabilities.textureArrays=qfalse;CHECK(!Ral_StaticLightingPlanBuild(&q,&plan)&&!memcmp(&plan,&before,sizeof(plan)));
	d.stationaryVisibilityPayloadHash=0u;CHECK(!Ral_StaticLightingBuild(&d,&p));return 0;
}
static int Probes(void){
	ralIrradianceProbeVolume_t v;ralIrradianceProbeQuery_t q;ralIrradianceProbeReceipt_t r,before;
	uint8_t validity[8]={1,1,1,1,1,1,1,1};uint32_t i,sum;
	memset(&v,0,sizeof(v));v.schemaVersion=RAL_IRRADIANCE_VOLUME_SCHEMA_VERSION;v.productGeneration=10u;
	v.cacheKey=11u;v.coefficientPayloadHash=12u;v.validityPayloadHash=13u;v.spacing=(ralLightVec3Q16_t){Q(4),Q(4),Q(4)};
	v.dimensions[0]=v.dimensions[1]=v.dimensions[2]=2u;v.encoding=RAL_IRRADIANCE_SH_L1_RGB16F;
	v.fallback=RAL_IRRADIANCE_FALLBACK_LIGHTGRID;v.ready=qtrue;CHECK(Ral_IrradianceProbeVolumeValid(&v));
	memset(&q,0,sizeof(q));q.schemaVersion=RAL_IRRADIANCE_QUERY_SCHEMA_VERSION;q.queryGeneration=20u;q.productGeneration=10u;
	q.position=(ralLightVec3Q16_t){Q(2),Q(2),Q(2)};CHECK(Ral_IrradianceProbeResolve(&v,&q,validity,8u,&r));
	CHECK(Ral_IrradianceProbeReceiptValid(&r)&&r.sampleCount==8u&&!r.usedFallback);sum=0u;for(i=0u;i<8u;i++)sum+=r.weightsQ16[i];CHECK(sum==RAL_LIGHT_Q16_ONE);
	validity[7]=0u;CHECK(Ral_IrradianceProbeResolve(&v,&q,validity,8u,&r)&&r.sampleCount==7u&&Ral_IrradianceProbeReceiptValid(&r));
	memset(validity,0,sizeof(validity));CHECK(Ral_IrradianceProbeResolve(&v,&q,validity,8u,&r)&&r.usedFallback&&Ral_IrradianceProbeReceiptValid(&r));
	q.position.x=Q(-1);CHECK(Ral_IrradianceProbeResolve(&v,&q,NULL,0u,&r)&&r.usedFallback);
	before=r;q.productGeneration++;CHECK(!Ral_IrradianceProbeResolve(&v,&q,NULL,0u,&r)&&!memcmp(&r,&before,sizeof(r)));return 0;
}
static int Encoding(void){int32_t radiance[3]={Q(4),Q(1),Q(0)};uint32_t packed,before=77u,oct16;uint64_t half;uint16_t oct;
	CHECK(Ral_StaticLightingEncodeRgb9e5(radiance,&packed)&&packed!=0u);
	CHECK(Ral_StaticLightingEncodeRgb9e5(radiance,&packed));
	radiance[0]=-1;packed=before;CHECK(!Ral_StaticLightingEncodeRgb9e5(radiance,&packed)&&packed==before);
	CHECK(Ral_StaticLightingEncodeOct8((ralLightVec3Q16_t){0,0,Q(1)},&oct));
	CHECK((oct&255u)==128u&&(oct>>8u)==128u);
	CHECK(Ral_StaticLightingEncodeOct8((ralLightVec3Q16_t){Q(1),0,0},&oct));
	CHECK((oct&255u)==255u&&(oct>>8u)==128u);before=oct;
	CHECK(!Ral_StaticLightingEncodeOct8((ralLightVec3Q16_t){0,0,0},&oct)&&oct==(uint16_t)before);
	radiance[0]=Q(4);CHECK(Ral_StaticLightingEncodeRgba16f(radiance,&half));
	CHECK((half&65535u)==0x4400u&&((half>>16u)&65535u)==0x3c00u&&((half>>48u)&65535u)==0x3c00u);
	CHECK(Ral_StaticLightingEncodeOct16((ralLightVec3Q16_t){0,0,Q(1)},&oct16));
	CHECK((oct16&65535u)==32768u&&(oct16>>16u)==32768u);return 0;}
int main(void){CHECK(Product()==0);CHECK(Probes()==0);CHECK(Encoding()==0);puts("ral_static_lighting_test: ok");return 0;}
