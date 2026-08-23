// SPDX-License-Identifier: GPL-3.0-or-later
#include "ral_ssr_policy.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static ralTextureResourceReceipt_t Tex(ralBackendType_t b,uintptr_t id,ralTextureType_t type){ralTextureResourceReceipt_t r;memset(&r,0,sizeof(r));
	r.schemaVersion=RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;r.backendType=b;r.textureIdentity=id;r.resourceGeneration=2u;r.type=type;
	r.format=RAL_FORMAT_R16G16B16A16_SFLOAT;r.usage=RAL_TEXTURE_USAGE_SAMPLED;r.width=r.height=64u;r.mipLevels=1u;r.arrayLayers=type==RAL_TEXTURE_CUBE?6u:1u;r.ready=qtrue;return r;}
static int Fallback(ralBackendType_t b,uint64_t query,ralReflectionProbeReceipt_t*out){ralReflectionProbeCatalog_t c;ralReflectionProbeQuery_t q;
	ralTextureResourceReceipt_t cube=Tex(b,0x9000u,RAL_TEXTURE_CUBE);memset(&q,0,sizeof(q));q.schemaVersion=RAL_REFLECTION_PROBE_QUERY_SCHEMA_VERSION;
	q.catalogGeneration=1u;q.queryGeneration=query;q.reflectionDirection.z=RAL_REFLECTION_PROBE_Q16_ONE;
	return Ral_ReflectionProbeCatalogBuild(&cube,NULL,0u,1u,&c)&&Ral_ReflectionProbeResolve(&c,&q,0u,out);}
static int Fixture(ralBackendType_t b,ralSsrReceipt_t*outResult){ralSsrRequest_t q;ralSsrSurface_t s[3];ralSsrReceipt_t r,before;uint32_t i;
	memset(&q,0,sizeof(q));q.schemaVersion=RAL_SSR_SCHEMA_VERSION;q.frameGraphGeneration=4u;q.frameIndex=10u;q.historyFrame=9u;
	q.tier=RAL_SSR_TIER_OFF;q.maxRoughnessQ16=RAL_SSR_Q16_ONE/2u;q.maxTraceSteps=48u;q.maxRayPixels=100u;q.maxHistoryAge=2u;q.surfaceCount=3u;
	q.sceneColor=Tex(b,0x100u,RAL_TEXTURE_2D);q.sceneDepth=Tex(b,0x200u,RAL_TEXTURE_2D);q.historyColor=Tex(b,0x300u,RAL_TEXTURE_2D);
	memset(s,0,sizeof(s));for(i=0u;i<3u;i++){s[i].surfaceId=i+1u;s[i].surfaceGeneration=1u;s[i].pixelCount=60u;s[i].depthValid=qtrue;CHECK(Fallback(b,i+1u,&s[i].fallback));}
	s[1].roughnessQ16=RAL_SSR_Q16_ONE;CHECK(Ral_SsrResolve(&q,s,3u,&r)&&r.totalRayPixels==0u
		&&r.decisions[0].result==RAL_SSR_RESULT_DISABLED&&r.decisions[0].rejectionBits==RAL_SSR_REJECT_DISABLED);
	q.tier=RAL_SSR_TIER_BALANCED;CHECK(Ral_SsrResolve(&q,s,3u,&r)&&r.decisions[0].result==RAL_SSR_RESULT_TRACE
		&&r.decisions[0].traceSteps==24u&&r.decisions[1].rejectionBits==RAL_SSR_REJECT_ROUGHNESS
		&&r.decisions[2].rejectionBits==RAL_SSR_REJECT_BUDGET&&r.totalRayPixels==60u);before=r;
	CHECK(!Ral_SsrResolve(&q,s,2u,&r)&&!memcmp(&r,&before,sizeof(r)));q.historyFrame=1u;
	CHECK(Ral_SsrResolve(&q,s,3u,&r)&&r.decisions[0].rejectionBits==RAL_SSR_REJECT_HISTORY);
	q.historyFrame=9u;q.sceneDepth.backendType=b==RAL_BACKEND_VULKAN?RAL_BACKEND_WEBGPU:RAL_BACKEND_VULKAN;
	CHECK(!Ral_SsrResolve(&q,s,3u,&r));*outResult=before;return 0;}
int main(void){ralSsrReceipt_t vk,web;CHECK(Fixture(RAL_BACKEND_VULKAN,&vk)==0&&Fixture(RAL_BACKEND_WEBGPU,&web)==0);
	CHECK(vk.totalRayPixels==web.totalRayPixels&&vk.decisions[0].result==web.decisions[0].result
		&&vk.decisions[2].rejectionBits==web.decisions[2].rejectionBits);puts("ral_ssr_policy_test: ok");return 0;}
