// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_lighting.h"

#include <limits.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)
#define LIGHT_COORD_LIMIT (1 << 29)

static uint64_t HashByte( uint64_t h, unsigned char v ) { return ( h ^ v ) * FNV64_PRIME; }
static uint64_t HashU32( uint64_t h, uint32_t v ) { uint32_t i;for(i=0u;i<4u;i++)h=HashByte(h,(unsigned char)(v>>(i*8u)));return h; }
static uint64_t HashU64( uint64_t h, uint64_t v ) { uint32_t i;for(i=0u;i<8u;i++)h=HashByte(h,(unsigned char)(v>>(i*8u)));return h; }
static uint64_t HashNonZero( uint64_t h ) { return h?h:1u; }
static qboolean CoordValid( int32_t v ) { return v>-LIGHT_COORD_LIMIT&&v<LIGHT_COORD_LIMIT; }
static qboolean VecValid( ralLightVec3Q16_t v ) { return CoordValid(v.x)&&CoordValid(v.y)&&CoordValid(v.z); }
static qboolean VecZero( ralLightVec3Q16_t v ) { return !v.x&&!v.y&&!v.z; }
static qboolean BoundsValid( ralLightVec3Q16_t mn,ralLightVec3Q16_t mx ) {
	return VecValid(mn)&&VecValid(mx)&&mn.x<mx.x&&mn.y<mx.y&&mn.z<mx.z;
}
static qboolean Inside(ralLightVec3Q16_t p,ralLightVec3Q16_t mn,ralLightVec3Q16_t mx){
	return p.x>=mn.x&&p.x<=mx.x&&p.y>=mn.y&&p.y<=mx.y&&p.z>=mn.z&&p.z<=mx.z;
}

qboolean Ral_LightDescriptionValid( const ralLightDescription_t *l ) {
	uint32_t known=RAL_LIGHT_CONTRIBUTE_DIRECT|RAL_LIGHT_CONTRIBUTE_BAKE
		|RAL_LIGHT_INJECT_ATMOSPHERE|RAL_LIGHT_CAST_SHADOW|RAL_LIGHT_EMISSIVE_PROXY;
	if(!l||l->schemaVersion!=RAL_LIGHT_SCHEMA_VERSION||!l->lightId
		||!l->sourceGeneration||!l->provenanceHash
		||l->kind<RAL_LIGHT_KIND_DIRECTIONAL||l->kind>RAL_LIGHT_KIND_AREA_PROXY
		||l->mobility<RAL_LIGHT_MOBILITY_STATIC||l->mobility>RAL_LIGHT_MOBILITY_DYNAMIC
		||!VecValid(l->position)||!VecValid(l->direction)
		||l->radianceQ16[0]<0||l->radianceQ16[1]<0||l->radianceQ16[2]<0
		||(!l->radianceQ16[0]&&!l->radianceQ16[1]&&!l->radianceQ16[2])
		||l->rangeQ16<0||l->shadowPriority>UINT16_MAX||!l->flags||(l->flags&~known)
		||((l->flags&RAL_LIGHT_CAST_SHADOW)&&!(l->flags&RAL_LIGHT_CONTRIBUTE_DIRECT))
		||((l->flags&RAL_LIGHT_CONTRIBUTE_BAKE)&&l->mobility==RAL_LIGHT_MOBILITY_DYNAMIC)
		||((l->flags&RAL_LIGHT_EMISSIVE_PROXY)&&l->kind!=RAL_LIGHT_KIND_AREA_PROXY))return qfalse;
	if(l->kind==RAL_LIGHT_KIND_DIRECTIONAL){
		if(VecZero(l->direction)||l->rangeQ16||!VecZero(l->position)
			||!VecZero(l->boundsMin)||!VecZero(l->boundsMax))return qfalse;
	}else{
		if(l->rangeQ16<=0||!BoundsValid(l->boundsMin,l->boundsMax)
			||!Inside(l->position,l->boundsMin,l->boundsMax))return qfalse;
	}
	if(l->kind==RAL_LIGHT_KIND_SPOT){
		if(VecZero(l->direction)||l->outerConeCosQ16<0
			||l->innerConeCosQ16<l->outerConeCosQ16
			||l->innerConeCosQ16>RAL_LIGHT_Q16_ONE)return qfalse;
	}else if(l->innerConeCosQ16||l->outerConeCosQ16)return qfalse;
	return qtrue;
}

static uint64_t HashVec( uint64_t h,ralLightVec3Q16_t v ) {
	h=HashU32(h,(uint32_t)v.x);h=HashU32(h,(uint32_t)v.y);return HashU32(h,(uint32_t)v.z);
}
static uint64_t HashLight( uint64_t h,const ralLightDescription_t*l ) {
	uint32_t i;h=HashU64(h,l->lightId);h=HashU64(h,l->sourceGeneration);h=HashU64(h,l->provenanceHash);
	h=HashU32(h,(uint32_t)l->kind);h=HashU32(h,(uint32_t)l->mobility);h=HashVec(h,l->position);
	h=HashVec(h,l->direction);h=HashVec(h,l->boundsMin);h=HashVec(h,l->boundsMax);
	for(i=0u;i<3u;i++)h=HashU32(h,(uint32_t)l->radianceQ16[i]);h=HashU32(h,(uint32_t)l->rangeQ16);
	h=HashU32(h,(uint32_t)l->innerConeCosQ16);h=HashU32(h,(uint32_t)l->outerConeCosQ16);
	h=HashU32(h,l->shadowPriority);return HashU32(h,l->flags);
}

qboolean Ral_LightCatalogBuild( const ralLightDescription_t *lights,uint32_t count,
		uint64_t generation,ralLightCatalogReceipt_t *out ) {
	ralLightCatalogReceipt_t v;uint64_t all=FNV64_OFFSET,stat=FNV64_OFFSET,stationary=FNV64_OFFSET,dynamic=FNV64_OFFSET;
	uint32_t i;if(!out||!generation||count>RAL_LIGHT_MAX_CATALOG||(count&&!lights))return qfalse;
	memset(&v,0,sizeof(v));all=HashU32(all,RAL_LIGHT_CATALOG_RECEIPT_SCHEMA_VERSION);
	stat=HashU32(stat,RAL_LIGHT_MOBILITY_STATIC);stationary=HashU32(stationary,RAL_LIGHT_MOBILITY_STATIONARY);
	dynamic=HashU32(dynamic,RAL_LIGHT_MOBILITY_DYNAMIC);
	for(i=0u;i<count;i++){
		const ralLightDescription_t*l=&lights[i];if(!Ral_LightDescriptionValid(l)||(i&&lights[i-1].lightId>=l->lightId))return qfalse;
		all=HashLight(all,l);
		if(l->mobility==RAL_LIGHT_MOBILITY_STATIC){v.staticCount++;if(l->flags&RAL_LIGHT_CONTRIBUTE_BAKE)stat=HashLight(stat,l);}
		else if(l->mobility==RAL_LIGHT_MOBILITY_STATIONARY){v.stationaryCount++;stationary=HashLight(stationary,l);if(l->flags&RAL_LIGHT_CONTRIBUTE_BAKE)stat=HashLight(stat,l);}
		else{v.dynamicCount++;dynamic=HashLight(dynamic,l);}
		if(l->flags&RAL_LIGHT_CAST_SHADOW)v.shadowCandidateCount++;
		if(l->flags&RAL_LIGHT_INJECT_ATMOSPHERE)v.atmosphereLightCount++;
	}
	v.schemaVersion=RAL_LIGHT_CATALOG_RECEIPT_SCHEMA_VERSION;v.catalogGeneration=generation;
	v.catalogHash=HashNonZero(all);v.staticBakeHash=HashNonZero(stat);
	v.stationaryDirectHash=HashNonZero(stationary);v.dynamicRuntimeHash=HashNonZero(dynamic);
	v.lightCount=count;v.ready=qtrue;*out=v;return qtrue;
}

qboolean Ral_LightCatalogReceiptValid( const ralLightCatalogReceipt_t *r ) {
	return r&&r->schemaVersion==RAL_LIGHT_CATALOG_RECEIPT_SCHEMA_VERSION&&r->catalogGeneration
		&&r->catalogHash&&r->staticBakeHash&&r->stationaryDirectHash&&r->dynamicRuntimeHash
		&&r->lightCount<=RAL_LIGHT_MAX_CATALOG
		&&r->staticCount+r->stationaryCount+r->dynamicCount==r->lightCount
		&&r->shadowCandidateCount<=r->lightCount&&r->atmosphereLightCount<=r->lightCount&&r->ready==qtrue;
}

static int32_t AxisValue(ralLightVec3Q16_t v,uint32_t axis){return axis==0u?v.x:axis==1u?v.y:v.z;}
static void AxisSet(ralLightVec3Q16_t*v,uint32_t axis,int32_t x){if(!axis)v->x=x;else if(axis==1u)v->y=x;else v->z=x;}
static uint32_t LongestAxis(ralLightVec3Q16_t mn,ralLightVec3Q16_t mx){
	int32_t x=mx.x-mn.x,y=mx.y-mn.y,z=mx.z-mn.z;return y>x&&y>=z?1u:z>x&&z>y?2u:0u;
}
static int32_t SplitEnergy(int32_t total,uint32_t index,uint32_t count){
	int32_t base=total/(int32_t)count;return index+1u==count?total-base*(int32_t)(count-1u):base;
}
static uint64_t ProxyId(uint64_t surfaceId,uint32_t index){
	uint64_t h=HashU64(HashU32(FNV64_OFFSET,RAL_EMISSIVE_PROXY_SCHEMA_VERSION),surfaceId);
	return HashNonZero(HashU32(h,index));
}
static qboolean SurfaceValid(const ralEmissiveSurface_t*s){
	return s&&s->schemaVersion==RAL_EMISSIVE_PROXY_SCHEMA_VERSION&&s->surfaceId
		&&s->sourceGeneration&&s->provenanceHash&&s->materialArtifactHash
		&&s->mobility>=RAL_LIGHT_MOBILITY_STATIC&&s->mobility<=RAL_LIGHT_MOBILITY_DYNAMIC
		&&BoundsValid(s->boundsMin,s->boundsMax)&&VecValid(s->normal)&&!VecZero(s->normal)
		&&s->radianceQ16[0]>=0&&s->radianceQ16[1]>=0&&s->radianceQ16[2]>=0
		&&(s->radianceQ16[0]||s->radianceQ16[1]||s->radianceQ16[2])
		&&s->areaQ16>0&&s->influenceRangeQ16>0&&s->shadowPriority<=UINT16_MAX
		&&s->requestedProxyCount<=RAL_EMISSIVE_PROXY_MAX_PER_SURFACE
		&&!(s->mobility==RAL_LIGHT_MOBILITY_DYNAMIC&&s->contributesToBake);
}

qboolean Ral_EmissiveProxyBuild( const ralEmissiveSurface_t *s,
		const ralEmissiveProxyPolicy_t *p,uint32_t capacity,
		ralLightDescription_t *outLights,ralEmissiveProxyReceipt_t *out ) {
	ralLightDescription_t temp[RAL_EMISSIVE_PROXY_MAX_PER_SURFACE];ralEmissiveProxyReceipt_t r;
	uint32_t count,i,axis;int32_t peak,lo,hi,span;uint64_t hash=FNV64_OFFSET;
	if(!out||!SurfaceValid(s)||!p||!p->maxPerSurface||p->maxPerSurface>RAL_EMISSIVE_PROXY_MAX_PER_SURFACE
		||p->minimumAreaQ16<0||p->minimumPeakRadianceQ16<0)return qfalse;
	memset(&r,0,sizeof(r));r.schemaVersion=RAL_EMISSIVE_PROXY_RECEIPT_SCHEMA_VERSION;
	r.surfaceId=s->surfaceId;r.sourceGeneration=s->sourceGeneration;r.ready=qtrue;
	peak=s->radianceQ16[0];if(s->radianceQ16[1]>peak)peak=s->radianceQ16[1];if(s->radianceQ16[2]>peak)peak=s->radianceQ16[2];
	if(s->explicitProxyAuthority)r.reason=RAL_EMISSIVE_PROXY_REJECT_EXPLICIT_AUTHORITY;
	else if(s->areaQ16<p->minimumAreaQ16)r.reason=RAL_EMISSIVE_PROXY_REJECT_LOW_AREA;
	else if(peak<p->minimumPeakRadianceQ16)r.reason=RAL_EMISSIVE_PROXY_REJECT_LOW_RADIANCE;
	else if(!p->remainingGlobalBudget)r.reason=RAL_EMISSIVE_PROXY_REJECT_GLOBAL_BUDGET;
	if(r.reason){r.proxySetHash=HashNonZero(HashU32(HashU64(hash,s->surfaceId),(uint32_t)r.reason));*out=r;return qtrue;}
	count=s->requestedProxyCount?s->requestedProxyCount:1u;if(count>p->maxPerSurface)count=p->maxPerSurface;
	if(count>p->remainingGlobalBudget)count=p->remainingGlobalBudget;if(capacity<count||!outLights)return qfalse;
	axis=LongestAxis(s->boundsMin,s->boundsMax);lo=AxisValue(s->boundsMin,axis);hi=AxisValue(s->boundsMax,axis);span=hi-lo;
	memset(temp,0,sizeof(temp));
	for(i=0u;i<count;i++){
		ralLightDescription_t*l=&temp[i];int64_t numerator=(int64_t)(2u*i+1u)*span;
		l->schemaVersion=RAL_LIGHT_SCHEMA_VERSION;l->lightId=ProxyId(s->surfaceId,i);
		l->sourceGeneration=s->sourceGeneration;l->provenanceHash=s->provenanceHash;
		l->kind=RAL_LIGHT_KIND_AREA_PROXY;l->mobility=s->mobility;l->direction=s->normal;
		l->boundsMin=s->boundsMin;l->boundsMax=s->boundsMax;
		l->position.x=s->boundsMin.x+(s->boundsMax.x-s->boundsMin.x)/2;
		l->position.y=s->boundsMin.y+(s->boundsMax.y-s->boundsMin.y)/2;
		l->position.z=s->boundsMin.z+(s->boundsMax.z-s->boundsMin.z)/2;
		AxisSet(&l->position,axis,lo+(int32_t)(numerator/(2u*count)));
		l->radianceQ16[0]=SplitEnergy(s->radianceQ16[0],i,count);
		l->radianceQ16[1]=SplitEnergy(s->radianceQ16[1],i,count);
		l->radianceQ16[2]=SplitEnergy(s->radianceQ16[2],i,count);
		l->rangeQ16=s->influenceRangeQ16;l->shadowPriority=s->shadowPriority;
		l->flags=RAL_LIGHT_CONTRIBUTE_DIRECT|RAL_LIGHT_EMISSIVE_PROXY;
		if(s->contributesToBake)l->flags|=RAL_LIGHT_CONTRIBUTE_BAKE;
		if(s->injectsAtmosphere)l->flags|=RAL_LIGHT_INJECT_ATMOSPHERE;
		if(s->shadowPriority)l->flags|=RAL_LIGHT_CAST_SHADOW;
		if(!Ral_LightDescriptionValid(l))return qfalse;hash=HashLight(hash,l);
	}
	memcpy(outLights,temp,count*sizeof(temp[0]));r.reason=RAL_EMISSIVE_PROXY_ACCEPTED;
	r.proxyCount=count;r.consumedGlobalBudget=count;r.proxySetHash=HashNonZero(hash);*out=r;return qtrue;
}

qboolean Ral_EmissiveProxyReceiptValid(const ralEmissiveProxyReceipt_t*r){
	return r&&r->schemaVersion==RAL_EMISSIVE_PROXY_RECEIPT_SCHEMA_VERSION&&r->surfaceId
		&&r->sourceGeneration&&r->proxySetHash&&r->reason<=RAL_EMISSIVE_PROXY_REJECT_GLOBAL_BUDGET
		&&r->proxyCount<=RAL_EMISSIVE_PROXY_MAX_PER_SURFACE&&r->consumedGlobalBudget==r->proxyCount
		&&((r->reason==RAL_EMISSIVE_PROXY_ACCEPTED)==(r->proxyCount>0u))&&r->ready==qtrue;
}

static uint64_t EmissiveRadianceHash( const ralEmissiveSurface_t *s ) {
	uint64_t h=HashU32(FNV64_OFFSET,RAL_EMISSIVE_ROUTE_RECEIPT_SCHEMA_VERSION);uint32_t i;
	h=HashU64(h,s->surfaceId);h=HashU64(h,s->sourceGeneration);h=HashU64(h,s->provenanceHash);
	h=HashU64(h,s->materialArtifactHash);h=HashU32(h,(uint32_t)s->mobility);
	h=HashVec(h,s->boundsMin);h=HashVec(h,s->boundsMax);
	for(i=0u;i<3u;i++)h=HashU32(h,(uint32_t)s->radianceQ16[i]);
	return HashNonZero(h);
}

qboolean Ral_EmissiveRouteBuild( const ralEmissiveSurface_t *s,
		const ralEmissiveProxyReceipt_t *p,ralEmissiveRouteReceipt_t *out ) {
	ralEmissiveRouteReceipt_t r;
	if(!out||!SurfaceValid(s)||!Ral_EmissiveProxyReceiptValid(p)
		||p->surfaceId!=s->surfaceId||p->sourceGeneration!=s->sourceGeneration
		||(s->explicitProxyAuthority&&p->reason!=RAL_EMISSIVE_PROXY_REJECT_EXPLICIT_AUTHORITY)
		||(!s->explicitProxyAuthority&&p->reason==RAL_EMISSIVE_PROXY_REJECT_EXPLICIT_AUTHORITY))return qfalse;
	memset(&r,0,sizeof(r));r.schemaVersion=RAL_EMISSIVE_ROUTE_RECEIPT_SCHEMA_VERSION;
	r.surfaceId=s->surfaceId;r.sourceGeneration=s->sourceGeneration;r.provenanceHash=s->provenanceHash;
	r.materialArtifactHash=s->materialArtifactHash;r.radianceAuthorityHash=EmissiveRadianceHash(s);
	r.proxySetHash=p->proxySetHash;r.mobility=s->mobility;r.boundsMin=s->boundsMin;r.boundsMax=s->boundsMax;
	memcpy(r.radianceQ16,s->radianceQ16,sizeof(r.radianceQ16));r.shadowPriority=s->shadowPriority;
	r.proxyCount=p->proxyCount;r.proxyReason=p->reason;r.consumerMask=RAL_EMISSIVE_CONSUMER_BLOOM_SOURCE;
	if(s->explicitProxyAuthority||p->reason==RAL_EMISSIVE_PROXY_ACCEPTED)r.consumerMask|=RAL_EMISSIVE_CONSUMER_DIRECT_PROXY;
	if(s->contributesToBake)r.consumerMask|=RAL_EMISSIVE_CONSUMER_STATIC_BAKE;
	if(s->injectsAtmosphere)r.consumerMask|=RAL_EMISSIVE_CONSUMER_ATMOSPHERE;
	r.ready=qtrue;if(!Ral_EmissiveRouteReceiptValid(&r))return qfalse;*out=r;return qtrue;
}

qboolean Ral_EmissiveRouteReceiptValid(const ralEmissiveRouteReceipt_t*r){
	uint32_t known=RAL_EMISSIVE_CONSUMER_DIRECT_PROXY|RAL_EMISSIVE_CONSUMER_STATIC_BAKE
		|RAL_EMISSIVE_CONSUMER_BLOOM_SOURCE|RAL_EMISSIVE_CONSUMER_ATMOSPHERE;
	return r&&r->schemaVersion==RAL_EMISSIVE_ROUTE_RECEIPT_SCHEMA_VERSION&&r->surfaceId
		&&r->sourceGeneration&&r->provenanceHash&&r->materialArtifactHash&&r->radianceAuthorityHash
		&&r->proxySetHash&&r->mobility>=RAL_LIGHT_MOBILITY_STATIC&&r->mobility<=RAL_LIGHT_MOBILITY_DYNAMIC
		&&BoundsValid(r->boundsMin,r->boundsMax)&&r->radianceQ16[0]>=0&&r->radianceQ16[1]>=0
		&&r->radianceQ16[2]>=0&&r->consumerMask&&(r->consumerMask&~known)==0u
		&&(r->consumerMask&RAL_EMISSIVE_CONSUMER_BLOOM_SOURCE)
		&&r->shadowPriority<=UINT16_MAX&&r->proxyCount<=RAL_EMISSIVE_PROXY_MAX_PER_SURFACE
		&&r->proxyReason<=RAL_EMISSIVE_PROXY_REJECT_GLOBAL_BUDGET
		&&((r->proxyReason==RAL_EMISSIVE_PROXY_ACCEPTED)==(r->proxyCount>0u))&&r->ready==qtrue;
}

static uint64_t CacheHash(uint32_t domain,const ralLightingCacheInputs_t*i,uint64_t lightHash,uint64_t extra){
	uint64_t h=HashU32(FNV64_OFFSET,RAL_LIGHTING_CACHE_KEY_SCHEMA_VERSION);h=HashU32(h,domain);
	h=HashU64(h,i->geometryHash);h=HashU64(h,i->materialHash);h=HashU64(h,lightHash);
	h=HashU64(h,i->producerVersion);h=HashU64(h,i->settingsHash);return HashNonZero(HashU64(h,extra));
}
qboolean Ral_LightingCacheKeyBuild(const ralLightingCacheInputs_t*i,ralLightingCacheKey_t*out){
	ralLightingCacheKey_t v;if(!out||!i||i->schemaVersion!=RAL_LIGHTING_CACHE_SCHEMA_VERSION
		||!i->geometryHash||!i->materialHash||!i->producerVersion||!i->settingsHash
		||!i->probeLayoutHash||!Ral_LightCatalogReceiptValid(&i->lightCatalog))return qfalse;
	memset(&v,0,sizeof(v));v.schemaVersion=RAL_LIGHTING_CACHE_KEY_SCHEMA_VERSION;
	v.staticIndirectKey=CacheHash(1u,i,i->lightCatalog.staticBakeHash,0u);
	v.stationaryShadowKey=CacheHash(2u,i,i->lightCatalog.stationaryDirectHash,0u);
	v.probeVolumeKey=CacheHash(3u,i,i->lightCatalog.staticBakeHash,i->probeLayoutHash);
	v.ready=qtrue;*out=v;return qtrue;
}
qboolean Ral_LightingCacheKeyValid(const ralLightingCacheKey_t*k){
	return k&&k->schemaVersion==RAL_LIGHTING_CACHE_KEY_SCHEMA_VERSION&&k->staticIndirectKey
		&&k->stationaryShadowKey&&k->probeVolumeKey&&k->ready==qtrue;
}
uint32_t Ral_LightingCacheDirtyMask(const ralLightingCacheKey_t*a,const ralLightingCacheKey_t*b){
	uint32_t mask=0u;if(!Ral_LightingCacheKeyValid(a)||!Ral_LightingCacheKeyValid(b))return RAL_LIGHTING_DIRTY_ALL;
	if(a->staticIndirectKey!=b->staticIndirectKey)mask|=RAL_LIGHTING_DIRTY_STATIC_INDIRECT;
	if(a->stationaryShadowKey!=b->stationaryShadowKey)mask|=RAL_LIGHTING_DIRTY_STATIONARY_SHADOW;
	if(a->probeVolumeKey!=b->probeVolumeKey)mask|=RAL_LIGHTING_DIRTY_PROBE_VOLUME;return mask;
}

static qboolean LightExact(const ralLightDescription_t*a,const ralLightDescription_t*b){
	return a&&b&&HashLight(FNV64_OFFSET,a)==HashLight(FNV64_OFFSET,b);
}
static void UnionBounds(const ralLightDescription_t*a,const ralLightDescription_t*b,
		ralLightVec3Q16_t*mn,ralLightVec3Q16_t*mx){
	const ralLightDescription_t*first=a?a:b;*mn=first->boundsMin;*mx=first->boundsMax;
	if(!a||!b)return;
	if(b->boundsMin.x<mn->x)mn->x=b->boundsMin.x;if(b->boundsMin.y<mn->y)mn->y=b->boundsMin.y;
	if(b->boundsMin.z<mn->z)mn->z=b->boundsMin.z;if(b->boundsMax.x>mx->x)mx->x=b->boundsMax.x;
	if(b->boundsMax.y>mx->y)mx->y=b->boundsMax.y;if(b->boundsMax.z>mx->z)mx->z=b->boundsMax.z;
}
qboolean Ral_LightUpdatePlanBuild(const ralLightDescription_t*a,const ralLightDescription_t*b,
		ralLightUpdatePlan_t*out){
	ralLightUpdatePlan_t p;uint32_t bake=0u,stationary=0u,direct=0u,atmos=0u;
	if(!out||(!a&&!b)||(a&&!Ral_LightDescriptionValid(a))||(b&&!Ral_LightDescriptionValid(b))
		||(a&&b&&a->lightId!=b->lightId)||LightExact(a,b))return qfalse;
	memset(&p,0,sizeof(p));p.schemaVersion=RAL_LIGHT_UPDATE_PLAN_SCHEMA_VERSION;
	p.lightId=a?a->lightId:b->lightId;p.beforeProvenanceHash=a?a->provenanceHash:0u;
	p.afterProvenanceHash=b?b->provenanceHash:0u;
	if(a){bake|=(a->flags&RAL_LIGHT_CONTRIBUTE_BAKE);stationary|=(a->mobility==RAL_LIGHT_MOBILITY_STATIONARY);
		direct|=(a->flags&RAL_LIGHT_CONTRIBUTE_DIRECT);atmos|=(a->flags&RAL_LIGHT_INJECT_ATMOSPHERE);}
	if(b){bake|=(b->flags&RAL_LIGHT_CONTRIBUTE_BAKE);stationary|=(b->mobility==RAL_LIGHT_MOBILITY_STATIONARY);
		direct|=(b->flags&RAL_LIGHT_CONTRIBUTE_DIRECT);atmos|=(b->flags&RAL_LIGHT_INJECT_ATMOSPHERE);}
	if(bake)p.derivedDirtyMask|=RAL_LIGHTING_DIRTY_STATIC_INDIRECT|RAL_LIGHTING_DIRTY_PROBE_VOLUME;
	if(stationary)p.derivedDirtyMask|=RAL_LIGHTING_DIRTY_STATIONARY_SHADOW;
	if(direct)p.runtimeUpdateMask|=RAL_LIGHT_UPDATE_RUNTIME_DIRECT;
	if(atmos)p.runtimeUpdateMask|=RAL_LIGHT_UPDATE_RUNTIME_ATMOSPHERE;
	p.bounded=((!a||a->kind!=RAL_LIGHT_KIND_DIRECTIONAL)&&(!b||b->kind!=RAL_LIGHT_KIND_DIRECTIONAL));
	if(p.bounded)UnionBounds(a,b,&p.affectedBoundsMin,&p.affectedBoundsMax);
	p.ready=qtrue;if(!Ral_LightUpdatePlanValid(&p))return qfalse;*out=p;return qtrue;
}
qboolean Ral_LightUpdatePlanValid(const ralLightUpdatePlan_t*p){
	uint32_t runtime=RAL_LIGHT_UPDATE_RUNTIME_DIRECT|RAL_LIGHT_UPDATE_RUNTIME_ATMOSPHERE;
	return p&&p->schemaVersion==RAL_LIGHT_UPDATE_PLAN_SCHEMA_VERSION&&p->lightId
		&&(p->beforeProvenanceHash||p->afterProvenanceHash)
		&&(p->derivedDirtyMask||p->runtimeUpdateMask)
		&&(p->derivedDirtyMask&~RAL_LIGHTING_DIRTY_ALL)==0u&&(p->runtimeUpdateMask&~runtime)==0u
		&&((p->bounded&&BoundsValid(p->affectedBoundsMin,p->affectedBoundsMax))
			||(!p->bounded&&VecZero(p->affectedBoundsMin)&&VecZero(p->affectedBoundsMax)))
		&&p->ready==qtrue;
}

static qboolean BoundsOverlap( ralLightVec3Q16_t aMin, ralLightVec3Q16_t aMax,
		ralLightVec3Q16_t bMin, ralLightVec3Q16_t bMax ) {
	return aMin.x <= bMax.x && aMax.x >= bMin.x &&
		aMin.y <= bMax.y && aMax.y >= bMin.y &&
		aMin.z <= bMax.z && aMax.z >= bMin.z;
}

static void UnionRawBounds( ralLightVec3Q16_t aMin, ralLightVec3Q16_t aMax,
		ralLightVec3Q16_t bMin, ralLightVec3Q16_t bMax,
		ralLightVec3Q16_t *outMin, ralLightVec3Q16_t *outMax ) {
	*outMin = aMin; *outMax = aMax;
	if ( bMin.x < outMin->x ) outMin->x = bMin.x;
	if ( bMin.y < outMin->y ) outMin->y = bMin.y;
	if ( bMin.z < outMin->z ) outMin->z = bMin.z;
	if ( bMax.x > outMax->x ) outMax->x = bMax.x;
	if ( bMax.y > outMax->y ) outMax->y = bMax.y;
	if ( bMax.z > outMax->z ) outMax->z = bMax.z;
}

static qboolean AffectedBefore( const ralCasterAffectedLight_t *a,
		const ralCasterAffectedLight_t *b ) {
	return a->shadowPriority > b->shadowPriority ||
		( a->shadowPriority == b->shadowPriority && a->lightId < b->lightId );
}

qboolean Ral_CasterUpdatePlanBuild( uint64_t generation, uint64_t casterId,
		const ralLightVec3Q16_t *beforeMin, const ralLightVec3Q16_t *beforeMax,
		const ralLightVec3Q16_t *afterMin, const ralLightVec3Q16_t *afterMax,
		const ralLightDescription_t *lights, uint32_t lightCount,
		ralCasterUpdatePlan_t *out ) {
	ralCasterUpdatePlan_t candidate;
	ralLightVec3Q16_t dirtyMin, dirtyMax;
	uint32_t i;
	uint64_t hash;
	if ( !out || !generation || generation == UINT64_MAX || !casterId ||
		 !beforeMin || !beforeMax || !afterMin || !afterMax ||
		 !BoundsValid( *beforeMin, *beforeMax ) || !BoundsValid( *afterMin, *afterMax ) ||
		 lightCount > RAL_LIGHT_MAX_CATALOG || ( lightCount && !lights ) )
		return qfalse;
	UnionRawBounds( *beforeMin, *beforeMax, *afterMin, *afterMax, &dirtyMin, &dirtyMax );
	memset( &candidate, 0, sizeof( candidate ) );
	candidate.schemaVersion = RAL_CASTER_UPDATE_PLAN_SCHEMA_VERSION;
	candidate.planGeneration = generation;
	candidate.casterId = casterId;
	candidate.affectedBoundsMin = dirtyMin;
	candidate.affectedBoundsMax = dirtyMax;
	candidate.bounded = qtrue;
	for ( i = 0u; i < lightCount; ++i ) {
		ralCasterAffectedLight_t affected;
		uint32_t insertAt;
		if ( !Ral_LightDescriptionValid( &lights[i] ) ||
			 ( i && lights[i - 1u].lightId >= lights[i].lightId ) )
			return qfalse;
		if ( !( lights[i].flags & RAL_LIGHT_CAST_SHADOW ) )
			continue;
		if ( lights[i].kind == RAL_LIGHT_KIND_DIRECTIONAL ) {
			candidate.directionalShadowCount++;
			continue;
		}
		if ( lights[i].mobility == RAL_LIGHT_MOBILITY_STATIC ||
			 !BoundsOverlap( dirtyMin, dirtyMax, lights[i].boundsMin, lights[i].boundsMax ) )
			continue;
		candidate.candidateCount++;
		affected.lightId = lights[i].lightId;
		affected.provenanceHash = lights[i].provenanceHash;
		affected.shadowPriority = lights[i].shadowPriority;
		affected.mobility = lights[i].mobility;
		insertAt = candidate.affectedLightCount;
		if ( insertAt < RAL_CASTER_MAX_AFFECTED_LOCAL_LIGHTS )
			candidate.affectedLightCount++;
		else {
			insertAt = RAL_CASTER_MAX_AFFECTED_LOCAL_LIGHTS - 1u;
			if ( !AffectedBefore( &affected, &candidate.affectedLights[insertAt] ) )
				continue;
		}
		while ( insertAt && AffectedBefore( &affected, &candidate.affectedLights[insertAt - 1u] ) ) {
			if ( insertAt < RAL_CASTER_MAX_AFFECTED_LOCAL_LIGHTS )
				candidate.affectedLights[insertAt] = candidate.affectedLights[insertAt - 1u];
			insertAt--;
		}
		candidate.affectedLights[insertAt] = affected;
	}
	candidate.droppedLightCount = candidate.candidateCount - candidate.affectedLightCount;
	hash = HashU32( FNV64_OFFSET, RAL_CASTER_UPDATE_PLAN_SCHEMA_VERSION );
	hash = HashU64( hash, generation ); hash = HashU64( hash, casterId );
	hash = HashVec( hash, dirtyMin ); hash = HashVec( hash, dirtyMax );
	hash = HashU32( hash, candidate.candidateCount );
	hash = HashU32( hash, candidate.directionalShadowCount );
	for ( i = 0u; i < candidate.affectedLightCount; ++i ) {
		hash = HashU64( hash, candidate.affectedLights[i].lightId );
		hash = HashU64( hash, candidate.affectedLights[i].provenanceHash );
		hash = HashU32( hash, candidate.affectedLights[i].shadowPriority );
		hash = HashU32( hash, (uint32_t)candidate.affectedLights[i].mobility );
	}
	candidate.planHash = HashNonZero( hash );
	candidate.ready = qtrue;
	if ( !Ral_CasterUpdatePlanValid( &candidate ) ) return qfalse;
	*out = candidate;
	return qtrue;
}

qboolean Ral_CasterUpdatePlanValid( const ralCasterUpdatePlan_t *plan ) {
	uint32_t i;
	if ( !plan || plan->schemaVersion != RAL_CASTER_UPDATE_PLAN_SCHEMA_VERSION ||
		 !plan->planGeneration || plan->planGeneration == UINT64_MAX || !plan->casterId ||
		 !plan->planHash || !BoundsValid( plan->affectedBoundsMin, plan->affectedBoundsMax ) ||
		 plan->affectedLightCount > RAL_CASTER_MAX_AFFECTED_LOCAL_LIGHTS ||
		 plan->candidateCount != plan->affectedLightCount + plan->droppedLightCount ||
		 plan->bounded != qtrue || plan->ready != qtrue )
		return qfalse;
	for ( i = 0u; i < plan->affectedLightCount; ++i ) {
		const ralCasterAffectedLight_t *light = &plan->affectedLights[i];
		if ( !light->lightId || !light->provenanceHash ||
			 ( light->mobility != RAL_LIGHT_MOBILITY_STATIONARY &&
			   light->mobility != RAL_LIGHT_MOBILITY_DYNAMIC ) ||
			 light->shadowPriority > UINT16_MAX ||
			 ( i && !AffectedBefore( &plan->affectedLights[i - 1u], light ) ) )
			return qfalse;
	}
	return qtrue;
}
