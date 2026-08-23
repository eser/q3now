// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_reflection_probe.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
#define Q(x) ((x)*RAL_REFLECTION_PROBE_Q16_ONE)

static ralTextureResourceReceipt_t Cube(ralBackendType_t b,uintptr_t id,uint64_t g){
	ralTextureResourceReceipt_t r;memset(&r,0,sizeof(r));r.schemaVersion=RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION;
	r.backendType=b;r.textureIdentity=id;r.resourceGeneration=g;r.type=RAL_TEXTURE_CUBE;r.format=RAL_FORMAT_R16G16B16A16_SFLOAT;
	r.usage=RAL_TEXTURE_USAGE_SAMPLED;r.width=r.height=64u;r.mipLevels=7u;r.arrayLayers=6u;r.ready=qtrue;return r;}
static ralLocalReflectionProbe_t Local(ralBackendType_t b,uint64_t id,uintptr_t texture,uint32_t priority,int x){
	ralLocalReflectionProbe_t p;memset(&p,0,sizeof(p));p.probeId=id;p.probeGeneration=3u;p.priority=priority;
	p.boundsMin.x=Q(x-4);p.boundsMin.y=p.boundsMin.z=Q(-4);p.boundsMax.x=Q(x+4);p.boundsMax.y=p.boundsMax.z=Q(4);
	p.capturePosition.x=Q(x);p.blendDistanceQ16=Q(2);p.cubemap=Cube(b,texture,5u);return p;}
static int Fixture(ralBackendType_t backend,ralReflectionProbeReceipt_t*outResult){
	ralTextureResourceReceipt_t global=Cube(backend,0x9000u,1u);ralLocalReflectionProbe_t p[2],bad[2];
	ralReflectionProbeCatalog_t c,beforeCatalog;ralReflectionProbeQuery_t q;ralReflectionProbeReceipt_t r,before;
	p[0]=Local(backend,10u,0x1000u,2u,0);p[1]=Local(backend,20u,0x2000u,1u,2);
	CHECK(Ral_ReflectionProbeCatalogBuild(&global,p,2u,7u,&c)&&Ral_ReflectionProbeCatalogValid(&c));
	memset(&q,0,sizeof(q));q.schemaVersion=RAL_REFLECTION_PROBE_QUERY_SCHEMA_VERSION;q.catalogGeneration=7u;
	q.queryGeneration=9u;q.position.x=Q(1);q.reflectionDirection.x=RAL_REFLECTION_PROBE_Q16_ONE;
	CHECK(Ral_ReflectionProbeResolve(&c,&q,2u,&r)&&r.selectedCount==2u&&r.selected[0].probeId==10u);
	CHECK(r.globalWeightQ16+r.selected[0].weightQ16+r.selected[1].weightQ16==RAL_REFLECTION_PROBE_Q16_ONE);
	CHECK(r.selected[0].parallaxDirection.x==Q(4));before=r;
	CHECK(!Ral_ReflectionProbeResolve(&c,&q,1u,&r)&&!memcmp(&r,&before,sizeof(r)));
	q.catalogGeneration++;CHECK(!Ral_ReflectionProbeResolve(&c,&q,2u,&r));q.catalogGeneration=7u;
	q.position.x=Q(20);CHECK(Ral_ReflectionProbeResolve(&c,&q,0u,&r)&&r.selectedCount==0u
		&&r.globalWeightQ16==RAL_REFLECTION_PROBE_Q16_ONE);
	memcpy(bad,p,sizeof(bad));bad[1].probeId=bad[0].probeId;beforeCatalog=c;
	CHECK(!Ral_ReflectionProbeCatalogBuild(&global,bad,2u,8u,&c)&&!memcmp(&c,&beforeCatalog,sizeof(c)));
	memcpy(bad,p,sizeof(bad));bad[1].cubemap.backendType=backend==RAL_BACKEND_VULKAN?RAL_BACKEND_WEBGPU:RAL_BACKEND_VULKAN;
	CHECK(!Ral_ReflectionProbeCatalogBuild(&global,bad,2u,8u,&c));*outResult=before;return 0;}
int main(void){ralReflectionProbeReceipt_t vk,web;CHECK(Fixture(RAL_BACKEND_VULKAN,&vk)==0);
	CHECK(Fixture(RAL_BACKEND_WEBGPU,&web)==0);CHECK(vk.selectedCount==web.selectedCount
		&&vk.globalWeightQ16==web.globalWeightQ16&&vk.selected[0].weightQ16==web.selected[0].weightQ16
		&&!memcmp(&vk.selected[0].parallaxDirection,&web.selected[0].parallaxDirection,sizeof(ralProbeVec3Q16_t)));
	puts("ral_reflection_probe_test: ok");return 0;}
