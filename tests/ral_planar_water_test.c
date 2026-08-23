// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "ral_planar_water.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static ralTextureResourceReceipt_t Target(ralBackendType_t b,uintptr_t id){ralTextureResourceReceipt_t r;memset(&r,0,sizeof(r));
	r.schemaVersion=RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;r.backendType=b;r.textureIdentity=id;r.resourceGeneration=2u;
	r.type=RAL_TEXTURE_2D;r.format=RAL_FORMAT_R16G16B16A16_SFLOAT;r.usage=(ralTextureUsage_t)(RAL_TEXTURE_USAGE_SAMPLED|RAL_TEXTURE_USAGE_COLOR_ATTACHMENT);
	r.width=r.height=64u;r.mipLevels=r.arrayLayers=1u;r.ready=qtrue;return r;}
static ralPlanarWaterCandidate_t Water(ralBackendType_t b,uint64_t id,uint32_t priority,uint64_t last){ralPlanarWaterCandidate_t c;memset(&c,0,sizeof(c));
	c.surfaceId=id;c.surfaceGeneration=1u;c.targetGraphGeneration=4u;c.priority=priority;c.visiblePixels=100u+(uint32_t)id;c.lastCaptureFrame=last;
	c.plane.z=RAL_PLANAR_WATER_Q16_ONE;c.fresnelF0Q16=1311u;c.dudvStrengthQ16=2048u;
	c.reflectionTarget=Target(b,(uintptr_t)(0x1000u+id*2u));c.refractionTarget=Target(b,(uintptr_t)(0x1001u+id*2u));return c;}
static int Fixture(ralBackendType_t backend,ralPlanarWaterReceipt_t*out){ralPlanarWaterRequest_t q;ralPlanarWaterCandidate_t c[3],bad[3];
	ralPlanarWaterReceipt_t r,before;memset(&q,0,sizeof(q));q.schemaVersion=RAL_PLANAR_WATER_SCHEMA_VERSION;q.frameGraphGeneration=4u;
	q.frameIndex=10u;q.maxSurfaces=2u;q.maxPixelWork=64u*64u*4u;q.candidateCount=3u;
	c[0]=Water(backend,1u,1u,1u);c[1]=Water(backend,2u,2u,9u);c[2]=Water(backend,3u,2u,2u);
	CHECK(Ral_PlanarWaterPlan(&q,c,2u,&r)&&r.updateCount==2u&&r.updates[0].surfaceId==3u&&r.updates[1].surfaceId==2u);
	CHECK(r.totalPixelWork<=q.maxPixelWork&&r.updates[0].reflection.clipPolicy==RAL_PLANAR_CLIP_KEEP_POSITIVE
		&&r.updates[0].refraction.clipPolicy==RAL_PLANAR_CLIP_KEEP_NEGATIVE);before=r;
	CHECK(!Ral_PlanarWaterPlan(&q,c,1u,&r)&&!memcmp(&r,&before,sizeof(r)));
	memcpy(bad,c,sizeof(bad));bad[1].targetGraphGeneration++;CHECK(!Ral_PlanarWaterPlan(&q,bad,2u,&r));
	memcpy(bad,c,sizeof(bad));bad[1].refractionTarget.textureIdentity=bad[1].reflectionTarget.textureIdentity;
	CHECK(!Ral_PlanarWaterPlan(&q,bad,2u,&r));memcpy(bad,c,sizeof(bad));
	bad[1].reflectionTarget.backendType=backend==RAL_BACKEND_VULKAN?RAL_BACKEND_WEBGPU:RAL_BACKEND_VULKAN;
	CHECK(!Ral_PlanarWaterPlan(&q,bad,2u,&r));*out=before;return 0;}
int main(void){ralPlanarWaterReceipt_t vk,web;CHECK(Fixture(RAL_BACKEND_VULKAN,&vk)==0&&Fixture(RAL_BACKEND_WEBGPU,&web)==0);
	CHECK(vk.updateCount==web.updateCount&&vk.totalPixelWork==web.totalPixelWork&&vk.updates[0].surfaceId==web.updates[0].surfaceId);
	puts("ral_planar_water_test: ok");return 0;}
