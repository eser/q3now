// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "ral_ssr_policy.h"
#include <string.h>
static qboolean Sampled2D(const ralTextureResourceReceipt_t*r){return r
	&&r->schemaVersion==RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION
	&&r->backendType>=RAL_BACKEND_VULKAN&&r->backendType<=RAL_BACKEND_WEBGL2
	&&r->textureIdentity&&r->resourceGeneration&&r->type==RAL_TEXTURE_2D
	&&r->width&&r->height&&r->mipLevels==1u&&r->arrayLayers==1u
	&&(r->usage&RAL_TEXTURE_USAGE_SAMPLED)&&r->ready==qtrue;}
static qboolean RequestValid(const ralSsrRequest_t*q){return q&&q->schemaVersion==RAL_SSR_SCHEMA_VERSION
	&&q->frameGraphGeneration&&q->frameIndex&&q->tier>=RAL_SSR_TIER_OFF&&q->tier<=RAL_SSR_TIER_QUALITY
	&&q->maxRoughnessQ16<=RAL_SSR_Q16_ONE&&q->maxTraceSteps&&q->maxTraceSteps<=64u
	&&q->maxHistoryAge&&q->surfaceCount<=RAL_SSR_MAX_SURFACES&&Sampled2D(&q->sceneColor)
	&&Sampled2D(&q->sceneDepth)&&Sampled2D(&q->historyColor)
	&&q->sceneColor.backendType==q->sceneDepth.backendType&&q->sceneColor.backendType==q->historyColor.backendType
	&&q->sceneColor.width==q->sceneDepth.width&&q->sceneColor.width==q->historyColor.width
	&&q->sceneColor.height==q->sceneDepth.height&&q->sceneColor.height==q->historyColor.height
	&&q->sceneColor.textureIdentity!=q->sceneDepth.textureIdentity
	&&q->sceneColor.textureIdentity!=q->historyColor.textureIdentity
	&&q->sceneDepth.textureIdentity!=q->historyColor.textureIdentity;}
qboolean Ral_SsrResolve(const ralSsrRequest_t*q,const ralSsrSurface_t*s,uint32_t capacity,ralSsrReceipt_t*out){
	ralSsrReceipt_t v;uint32_t i;
	if(!out||!RequestValid(q)||(q->surfaceCount&&!s)||capacity<q->surfaceCount)return qfalse;
	for(i=0u;i<q->surfaceCount;i++)if(!s[i].surfaceId||!s[i].surfaceGeneration||!s[i].pixelCount
		||s[i].roughnessQ16>RAL_SSR_Q16_ONE||(s[i].depthValid!=qfalse&&s[i].depthValid!=qtrue)
		||!Ral_ReflectionProbeReceiptValid(&s[i].fallback)
		||s[i].fallback.globalCubemap.backendType!=q->sceneColor.backendType
		||(i&&s[i-1u].surfaceId>=s[i].surfaceId))return qfalse;
	memset(&v,0,sizeof(v));v.schemaVersion=RAL_SSR_RECEIPT_SCHEMA_VERSION;v.frameGraphGeneration=q->frameGraphGeneration;
	v.frameIndex=q->frameIndex;v.tier=q->tier;v.decisionCount=q->surfaceCount;
	for(i=0u;i<q->surfaceCount;i++){ralSsrDecision_t*d=&v.decisions[i];uint32_t reject=0u;
		d->surfaceId=s[i].surfaceId;d->surfaceGeneration=s[i].surfaceGeneration;d->fallback=s[i].fallback;
		if(q->tier==RAL_SSR_TIER_OFF)reject|=RAL_SSR_REJECT_DISABLED;
		else{if(s[i].roughnessQ16>q->maxRoughnessQ16)reject|=RAL_SSR_REJECT_ROUGHNESS;
			if(!s[i].depthValid)reject|=RAL_SSR_REJECT_DEPTH;
			if(!q->historyFrame||q->historyFrame>=q->frameIndex||q->frameIndex-q->historyFrame>q->maxHistoryAge)
				reject|=RAL_SSR_REJECT_HISTORY;}
		if(!reject&&s[i].pixelCount>q->maxRayPixels-v.totalRayPixels)reject|=RAL_SSR_REJECT_BUDGET;
		d->rejectionBits=reject;if(q->tier==RAL_SSR_TIER_OFF)d->result=RAL_SSR_RESULT_DISABLED;
		else if(reject)d->result=RAL_SSR_RESULT_PROBE_FALLBACK;
		else{d->result=RAL_SSR_RESULT_TRACE;d->traceSteps=q->tier==RAL_SSR_TIER_BALANCED&&q->maxTraceSteps>24u?24u:q->maxTraceSteps;
			d->rayPixels=s[i].pixelCount;v.totalRayPixels+=d->rayPixels;}
	}
	v.ready=qtrue;*out=v;return qtrue;
}
qboolean Ral_SsrReceiptExact(const ralSsrReceipt_t*a,const ralSsrReceipt_t*b){uint32_t i,total=0u;
	if(!a||!b||a->schemaVersion!=RAL_SSR_RECEIPT_SCHEMA_VERSION||!a->frameGraphGeneration||!a->frameIndex
		||a->tier>RAL_SSR_TIER_QUALITY||a->decisionCount>RAL_SSR_MAX_SURFACES||a->ready!=qtrue)return qfalse;
	for(i=0u;i<a->decisionCount;i++){if(!Ral_ReflectionProbeReceiptValid(&a->decisions[i].fallback))return qfalse;
		if(a->decisions[i].result==RAL_SSR_RESULT_TRACE)total+=a->decisions[i].rayPixels;}
	return total==a->totalRayPixels&&!memcmp(a,b,sizeof(*a));}
