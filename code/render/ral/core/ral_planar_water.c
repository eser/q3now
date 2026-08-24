// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_planar_water.h"
#include <limits.h>
#include <string.h>

static qboolean TargetValid(const ralTextureResourceReceipt_t*r){
	return r&&r->schemaVersion==RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION
		&&r->backendType>=RAL_BACKEND_VULKAN&&r->backendType<RAL_BACKEND_COUNT
		&&r->textureIdentity&&r->resourceGeneration&&r->type==RAL_TEXTURE_2D
		&&r->width&&r->height&&r->mipLevels==1u&&r->arrayLayers==1u
		&&(r->usage&RAL_TEXTURE_USAGE_SAMPLED)&&(r->usage&RAL_TEXTURE_USAGE_COLOR_ATTACHMENT)
		&&r->ready==qtrue;
}
static qboolean PlaneValid(ralPlanarPlaneQ16_t p){
	int64_t length=(int64_t)p.x*p.x+(int64_t)p.y*p.y+(int64_t)p.z*p.z;
	int64_t one=(int64_t)RAL_PLANAR_WATER_Q16_ONE*RAL_PLANAR_WATER_Q16_ONE;
	return length>=one-(one/128)&&length<=one+(one/128)&&p.w>INT32_MIN/2&&p.w<INT32_MAX/2;
}
static qboolean CandidateValid(const ralPlanarWaterRequest_t*q,const ralPlanarWaterCandidate_t*c){
	uint64_t pixels;
	if(!c||!c->surfaceId||!c->surfaceGeneration||c->targetGraphGeneration!=q->frameGraphGeneration
		||!c->visiblePixels||c->lastCaptureFrame>=q->frameIndex||!PlaneValid(c->plane)
		||c->fresnelF0Q16>RAL_PLANAR_WATER_Q16_ONE||c->dudvStrengthQ16>RAL_PLANAR_WATER_Q16_ONE/4u
		||!TargetValid(&c->reflectionTarget)||!TargetValid(&c->refractionTarget)
		||c->reflectionTarget.backendType!=c->refractionTarget.backendType
		||c->reflectionTarget.textureIdentity==c->refractionTarget.textureIdentity
		||c->reflectionTarget.width!=c->refractionTarget.width
		||c->reflectionTarget.height!=c->refractionTarget.height)return qfalse;
	pixels=(uint64_t)c->reflectionTarget.width*c->reflectionTarget.height*2u;
	return pixels<=UINT32_MAX;
}
static qboolean Better(const ralPlanarWaterCandidate_t*a,const ralPlanarWaterCandidate_t*b,uint64_t frame){
	uint64_t ageA=frame-a->lastCaptureFrame,ageB=frame-b->lastCaptureFrame;
	return !b||a->priority>b->priority||(a->priority==b->priority&&(ageA>ageB
		||(ageA==ageB&&(a->visiblePixels>b->visiblePixels
		||(a->visiblePixels==b->visiblePixels&&a->surfaceId<b->surfaceId)))));
}
qboolean Ral_PlanarWaterPlan(const ralPlanarWaterRequest_t*q,const ralPlanarWaterCandidate_t*c,
		uint32_t capacity,ralPlanarWaterReceipt_t*out){
	const ralPlanarWaterCandidate_t*order[RAL_PLANAR_WATER_MAX_SURFACES];ralPlanarWaterReceipt_t v;
	uint32_t i,j,count=0u;
	if(!out||!q||q->schemaVersion!=RAL_PLANAR_WATER_SCHEMA_VERSION||!q->frameGraphGeneration
		||!q->frameIndex||!q->maxSurfaces||q->maxSurfaces>RAL_PLANAR_WATER_MAX_SURFACES
		||!q->maxPixelWork||q->candidateCount>RAL_PLANAR_WATER_MAX_SURFACES
		||(q->candidateCount&&!c))return qfalse;
	memset(order,0,sizeof(order));
	for(i=0u;i<q->candidateCount;i++){
		if(!CandidateValid(q,&c[i]))return qfalse;
		for(j=0u;j<i;j++)if(c[j].surfaceId==c[i].surfaceId
			||c[j].reflectionTarget.textureIdentity==c[i].reflectionTarget.textureIdentity
			||c[j].reflectionTarget.textureIdentity==c[i].refractionTarget.textureIdentity
			||c[j].refractionTarget.textureIdentity==c[i].reflectionTarget.textureIdentity
			||c[j].refractionTarget.textureIdentity==c[i].refractionTarget.textureIdentity)return qfalse;
		for(j=0u;j<i;j++)if(Better(&c[i],order[j],q->frameIndex))break;
		{uint32_t k;for(k=i;k>j;k--)order[k]=order[k-1u];order[j]=&c[i];}
	}
	memset(&v,0,sizeof(v));v.schemaVersion=RAL_PLANAR_WATER_RECEIPT_SCHEMA_VERSION;
	v.frameGraphGeneration=q->frameGraphGeneration;v.frameIndex=q->frameIndex;
	for(i=0u;i<q->candidateCount&&count<q->maxSurfaces;i++){
		const ralPlanarWaterCandidate_t*x=order[i];uint32_t pixels=x->reflectionTarget.width*x->reflectionTarget.height*2u;
		ralPlanarWaterUpdate_t*u;if(pixels>q->maxPixelWork-v.totalPixelWork)continue;
		if(count>=capacity)return qfalse;u=&v.updates[count++];u->surfaceId=x->surfaceId;u->surfaceGeneration=x->surfaceGeneration;
		u->pixelWork=pixels;u->fresnelF0Q16=x->fresnelF0Q16;u->dudvStrengthQ16=x->dudvStrengthQ16;
		u->reflection.clipPolicy=RAL_PLANAR_CLIP_KEEP_POSITIVE;u->reflection.plane=x->plane;u->reflection.target=x->reflectionTarget;
		u->refraction.clipPolicy=RAL_PLANAR_CLIP_KEEP_NEGATIVE;u->refraction.plane=x->plane;u->refraction.target=x->refractionTarget;
		v.totalPixelWork+=pixels;
	}
	v.updateCount=count;v.ready=qtrue;*out=v;return qtrue;
}
qboolean Ral_PlanarWaterReceiptExact(const ralPlanarWaterReceipt_t*a,const ralPlanarWaterReceipt_t*b){
	uint32_t i,total=0u;if(!a||!b||a->schemaVersion!=RAL_PLANAR_WATER_RECEIPT_SCHEMA_VERSION
		||!a->frameGraphGeneration||!a->frameIndex||a->ready!=qtrue||a->updateCount>RAL_PLANAR_WATER_MAX_SURFACES)return qfalse;
	for(i=0u;i<a->updateCount;i++){
		if(a->updates[i].reflection.clipPolicy!=RAL_PLANAR_CLIP_KEEP_POSITIVE
			||a->updates[i].refraction.clipPolicy!=RAL_PLANAR_CLIP_KEEP_NEGATIVE)return qfalse;
		total+=a->updates[i].pixelWork;
	}
	return total==a->totalPixelWork&&!memcmp(a,b,sizeof(*a));
}
