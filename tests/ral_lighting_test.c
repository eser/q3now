// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
#define Q(x) ((x)*RAL_LIGHT_Q16_ONE)

static ralLightDescription_t Point(uint64_t id,ralLightMobility_t mobility,uint32_t flags){
	ralLightDescription_t l;memset(&l,0,sizeof(l));l.schemaVersion=RAL_LIGHT_SCHEMA_VERSION;
	l.lightId=id;l.sourceGeneration=2u;l.provenanceHash=0xabc0u+id;l.kind=RAL_LIGHT_KIND_POINT;l.mobility=mobility;
	l.boundsMin=(ralLightVec3Q16_t){Q(-8),Q(-8),Q(-8)};l.boundsMax=(ralLightVec3Q16_t){Q(8),Q(8),Q(8)};
	l.radianceQ16[0]=Q(12);l.radianceQ16[1]=Q(4);l.radianceQ16[2]=Q(1);l.rangeQ16=Q(8);
	l.shadowPriority=4u;l.flags=flags;return l;
}

static int CatalogAndDirtyKeys(void){
	ralLightDescription_t lights[3];ralLightCatalogReceipt_t catalog,changed;
	ralLightingCacheInputs_t inputs;ralLightingCacheKey_t base,next,before;
	ralLightUpdatePlan_t update,beforeUpdate;ralLightDescription_t oldLight;
	lights[0]=Point(10u,RAL_LIGHT_MOBILITY_STATIC,RAL_LIGHT_CONTRIBUTE_BAKE);
	lights[1]=Point(20u,RAL_LIGHT_MOBILITY_STATIONARY,RAL_LIGHT_CONTRIBUTE_DIRECT|RAL_LIGHT_CONTRIBUTE_BAKE|RAL_LIGHT_CAST_SHADOW|RAL_LIGHT_INJECT_ATMOSPHERE);
	lights[2]=Point(30u,RAL_LIGHT_MOBILITY_DYNAMIC,RAL_LIGHT_CONTRIBUTE_DIRECT|RAL_LIGHT_CAST_SHADOW);
	CHECK(Ral_LightCatalogBuild(lights,3u,7u,&catalog)&&Ral_LightCatalogReceiptValid(&catalog));
	CHECK(catalog.staticCount==1u&&catalog.stationaryCount==1u&&catalog.dynamicCount==1u);
	CHECK(catalog.shadowCandidateCount==2u&&catalog.atmosphereLightCount==1u);
	memset(&inputs,0,sizeof(inputs));inputs.schemaVersion=RAL_LIGHTING_CACHE_SCHEMA_VERSION;
	inputs.geometryHash=1u;inputs.materialHash=2u;inputs.probeLayoutHash=3u;inputs.producerVersion=4u;inputs.settingsHash=5u;inputs.lightCatalog=catalog;
	CHECK(Ral_LightingCacheKeyBuild(&inputs,&base)&&Ral_LightingCacheKeyValid(&base));
	CHECK(Ral_LightingCacheDirtyMask(&base,&base)==0u);
	lights[2].radianceQ16[0]++;CHECK(Ral_LightCatalogBuild(lights,3u,8u,&changed));inputs.lightCatalog=changed;
	CHECK(Ral_LightingCacheKeyBuild(&inputs,&next));
	CHECK(Ral_LightingCacheDirtyMask(&base,&next)==0u);
	oldLight=lights[2];CHECK(Ral_LightUpdatePlanBuild(&oldLight,&lights[2],&update)==qfalse);
	lights[2].radianceQ16[1]++;CHECK(Ral_LightUpdatePlanBuild(&oldLight,&lights[2],&update)
		&&Ral_LightUpdatePlanValid(&update));
	CHECK(update.derivedDirtyMask==0u&&update.runtimeUpdateMask==RAL_LIGHT_UPDATE_RUNTIME_DIRECT
		&&update.bounded==qtrue);
	beforeUpdate=update;lights[2].lightId=31u;CHECK(!Ral_LightUpdatePlanBuild(&oldLight,&lights[2],&update)
		&&!memcmp(&update,&beforeUpdate,sizeof(update)));lights[2].lightId=30u;lights[2].radianceQ16[1]--;
	lights[1].radianceQ16[0]++;CHECK(Ral_LightCatalogBuild(lights,3u,9u,&changed));inputs.lightCatalog=changed;
	CHECK(Ral_LightingCacheKeyBuild(&inputs,&next));
	CHECK(Ral_LightingCacheDirtyMask(&base,&next)==RAL_LIGHTING_DIRTY_ALL);
	oldLight=Point(20u,RAL_LIGHT_MOBILITY_STATIONARY,RAL_LIGHT_CONTRIBUTE_DIRECT|RAL_LIGHT_CONTRIBUTE_BAKE|RAL_LIGHT_CAST_SHADOW|RAL_LIGHT_INJECT_ATMOSPHERE);
	CHECK(Ral_LightUpdatePlanBuild(&oldLight,&lights[1],&update));
	CHECK(update.derivedDirtyMask==RAL_LIGHTING_DIRTY_ALL
		&&update.runtimeUpdateMask==(RAL_LIGHT_UPDATE_RUNTIME_DIRECT|RAL_LIGHT_UPDATE_RUNTIME_ATMOSPHERE));
	before=next;lights[1].lightId=lights[0].lightId;CHECK(!Ral_LightCatalogBuild(lights,3u,10u,&changed));
	inputs.geometryHash=0u;CHECK(!Ral_LightingCacheKeyBuild(&inputs,&next)&&!memcmp(&next,&before,sizeof(next)));
	return 0;
}

static int CasterUpdates(void){
	ralLightDescription_t lights[36];
	ralCasterUpdatePlan_t plan,beforePlan;
	ralLightVec3Q16_t beforeMin={0,0,0},beforeMax={Q(2),Q(2),Q(2)};
	ralLightVec3Q16_t afterMin={Q(1),Q(1),Q(1)},afterMax={Q(3),Q(3),Q(3)};
	uint32_t i;
	for(i=0u;i<36u;i++){
		lights[i]=Point(100u+i,i==0u?RAL_LIGHT_MOBILITY_STATIC:RAL_LIGHT_MOBILITY_DYNAMIC,
			RAL_LIGHT_CONTRIBUTE_DIRECT|RAL_LIGHT_CAST_SHADOW);
		lights[i].shadowPriority=i;
	}
	CHECK(Ral_CasterUpdatePlanBuild(13u,17u,&beforeMin,&beforeMax,&afterMin,&afterMax,
		lights,36u,&plan)&&Ral_CasterUpdatePlanValid(&plan));
	CHECK(plan.candidateCount==35u&&plan.affectedLightCount==32u&&plan.droppedLightCount==3u);
	CHECK(plan.affectedLights[0].lightId==135u&&plan.affectedLights[31].lightId==104u);
	CHECK(plan.affectedBoundsMin.x==0&&plan.affectedBoundsMax.x==Q(3)&&plan.bounded);
	beforePlan=plan;lights[10].lightId=lights[9].lightId;
	CHECK(!Ral_CasterUpdatePlanBuild(13u,17u,&beforeMin,&beforeMax,&afterMin,&afterMax,
		lights,36u,&plan)&&!memcmp(&plan,&beforePlan,sizeof(plan)));
	return 0;
}

static int Emissive(void){
	ralEmissiveSurface_t s;ralEmissiveProxyPolicy_t p;ralLightDescription_t lights[4],before[4];
	ralEmissiveProxyReceipt_t r,rejected,beforeRejected;ralEmissiveRouteReceipt_t route,beforeRoute;
	int32_t sum[3]={0,0,0};uint32_t i;
	memset(&s,0,sizeof(s));s.schemaVersion=RAL_EMISSIVE_PROXY_SCHEMA_VERSION;s.surfaceId=99u;
	s.sourceGeneration=8u;s.provenanceHash=77u;s.materialArtifactHash=66u;s.mobility=RAL_LIGHT_MOBILITY_STATIONARY;
	s.boundsMin=(ralLightVec3Q16_t){Q(-6),Q(-1),Q(-1)};s.boundsMax=(ralLightVec3Q16_t){Q(6),Q(1),Q(1)};
	s.normal.z=RAL_LIGHT_Q16_ONE;s.radianceQ16[0]=Q(10);s.radianceQ16[1]=Q(3);s.radianceQ16[2]=Q(1);
	s.areaQ16=Q(24);s.influenceRangeQ16=Q(12);s.shadowPriority=5u;s.requestedProxyCount=4u;
	s.contributesToBake=qtrue;s.injectsAtmosphere=qtrue;
	p=(ralEmissiveProxyPolicy_t){4u,3u,Q(1),Q(1)};memset(lights,0,sizeof(lights));
	CHECK(Ral_EmissiveProxyBuild(&s,&p,4u,lights,&r)&&Ral_EmissiveProxyReceiptValid(&r));
	CHECK(r.proxyCount==3u&&r.consumedGlobalBudget==3u&&lights[0].position.x<lights[1].position.x&&lights[1].position.x<lights[2].position.x);
	for(i=0u;i<r.proxyCount;i++){CHECK(Ral_LightDescriptionValid(&lights[i]));sum[0]+=lights[i].radianceQ16[0];sum[1]+=lights[i].radianceQ16[1];sum[2]+=lights[i].radianceQ16[2];}
	CHECK(sum[0]==s.radianceQ16[0]&&sum[1]==s.radianceQ16[1]&&sum[2]==s.radianceQ16[2]);
	CHECK(Ral_EmissiveRouteBuild(&s,&r,&route)&&Ral_EmissiveRouteReceiptValid(&route));
	CHECK(route.consumerMask==(RAL_EMISSIVE_CONSUMER_DIRECT_PROXY|RAL_EMISSIVE_CONSUMER_STATIC_BAKE
		|RAL_EMISSIVE_CONSUMER_BLOOM_SOURCE|RAL_EMISSIVE_CONSUMER_ATMOSPHERE));
	CHECK(route.proxyCount==3u&&route.proxyReason==RAL_EMISSIVE_PROXY_ACCEPTED
		&&!memcmp(route.radianceQ16,s.radianceQ16,sizeof(route.radianceQ16)));
	memcpy(before,lights,sizeof(before));s.explicitProxyAuthority=qtrue;
	CHECK(Ral_EmissiveProxyBuild(&s,&p,0u,NULL,&rejected)&&Ral_EmissiveProxyReceiptValid(&rejected));
	CHECK(rejected.reason==RAL_EMISSIVE_PROXY_REJECT_EXPLICIT_AUTHORITY&&!memcmp(before,lights,sizeof(before)));
	CHECK(Ral_EmissiveRouteBuild(&s,&rejected,&route)&&route.proxyCount==0u
		&&(route.consumerMask&RAL_EMISSIVE_CONSUMER_DIRECT_PROXY));
	s.explicitProxyAuthority=qfalse;p.remainingGlobalBudget=0u;
	CHECK(Ral_EmissiveProxyBuild(&s,&p,0u,NULL,&rejected)&&rejected.reason==RAL_EMISSIVE_PROXY_REJECT_GLOBAL_BUDGET);
	CHECK(Ral_EmissiveRouteBuild(&s,&rejected,&route)
		&&!(route.consumerMask&RAL_EMISSIVE_CONSUMER_DIRECT_PROXY)
		&&(route.consumerMask&RAL_EMISSIVE_CONSUMER_STATIC_BAKE));
	beforeRoute=route;rejected.surfaceId++;CHECK(!Ral_EmissiveRouteBuild(&s,&rejected,&route)
		&&!memcmp(&route,&beforeRoute,sizeof(route)));
	p.remainingGlobalBudget=4u;CHECK(!Ral_EmissiveProxyBuild(&s,&p,2u,lights,&rejected));
	beforeRejected=rejected;s.radianceQ16[0]=s.radianceQ16[1]=s.radianceQ16[2]=0;
	CHECK(!Ral_EmissiveProxyBuild(&s,&p,4u,lights,&rejected)
		&&!memcmp(&rejected,&beforeRejected,sizeof(rejected)));return 0;
}

int main(void){CHECK(CatalogAndDirtyKeys()==0);CHECK(CasterUpdates()==0);CHECK(Emissive()==0);puts("ral_lighting_test: ok");return 0;}
