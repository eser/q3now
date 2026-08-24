// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_reflection_probe.h"

#include <limits.h>
#include <string.h>

#define PROBE_COORD_LIMIT (1 << 29)

static qboolean CubeValid( const ralTextureResourceReceipt_t *r ) {
	return r && r->schemaVersion==RAL_TEXTURE_RESOURCE_RECEIPT_SCHEMA_VERSION
		&&r->backendType>=RAL_BACKEND_VULKAN&&r->backendType<RAL_BACKEND_COUNT
		&&r->textureIdentity&&r->resourceGeneration&&r->type==RAL_TEXTURE_CUBE
		&&r->width&&r->width==r->height&&r->mipLevels&&r->arrayLayers==6u
		&&(r->usage&RAL_TEXTURE_USAGE_SAMPLED)&&r->ready==qtrue;
}

static qboolean CoordValid( int32_t v ) { return v>-PROBE_COORD_LIMIT&&v<PROBE_COORD_LIMIT; }
static qboolean VecValid( ralProbeVec3Q16_t v ) { return CoordValid(v.x)&&CoordValid(v.y)&&CoordValid(v.z); }
static qboolean Inside( ralProbeVec3Q16_t p, ralProbeVec3Q16_t mn, ralProbeVec3Q16_t mx ) {
	return p.x>=mn.x&&p.x<=mx.x&&p.y>=mn.y&&p.y<=mx.y&&p.z>=mn.z&&p.z<=mx.z;
}

static qboolean LocalValid( const ralLocalReflectionProbe_t *p,
		ralBackendType_t backend ) {
	if(!p||!p->probeId||!p->probeGeneration||!VecValid(p->boundsMin)||!VecValid(p->boundsMax)
		||!VecValid(p->capturePosition)||p->priority>UINT16_MAX||p->blendDistanceQ16<0
		||p->boundsMin.x>=p->boundsMax.x||p->boundsMin.y>=p->boundsMax.y
		||p->boundsMin.z>=p->boundsMax.z||!Inside(p->capturePosition,p->boundsMin,p->boundsMax)
		||!CubeValid(&p->cubemap)||p->cubemap.backendType!=backend)return qfalse;
	return qtrue;
}

static qboolean Build( const ralTextureResourceReceipt_t *global,
		const ralLocalReflectionProbe_t *locals,uint32_t count,uint64_t generation,
		ralReflectionProbeCatalog_t *out ) {
	ralReflectionProbeCatalog_t v;uint32_t i,j;
	if(!out||!generation||!CubeValid(global)||count>RAL_REFLECTION_PROBE_MAX_LOCAL
		||(count&&!locals))return qfalse;
	memset(&v,0,sizeof(v));v.schemaVersion=RAL_REFLECTION_PROBE_SCHEMA_VERSION;
	v.catalogGeneration=generation;v.globalCubemap=*global;v.localCount=count;
	for(i=0u;i<count;i++){
		if(!LocalValid(&locals[i],global->backendType)
			||locals[i].cubemap.textureIdentity==global->textureIdentity)return qfalse;
		for(j=0u;j<i;j++)if(locals[j].probeId==locals[i].probeId
			||locals[j].cubemap.textureIdentity==locals[i].cubemap.textureIdentity)return qfalse;
		v.locals[i]=locals[i];
	}
	v.ready=qtrue;*out=v;return qtrue;
}

qboolean Ral_ReflectionProbeCatalogBuild( const ralTextureResourceReceipt_t *global,
		const ralLocalReflectionProbe_t *locals,uint32_t count,uint64_t generation,
		ralReflectionProbeCatalog_t *out ) {
	ralReflectionProbeCatalog_t v;if(!out||!Build(global,locals,count,generation,&v))return qfalse;
	*out=v;return qtrue;
}

qboolean Ral_ReflectionProbeCatalogValid( const ralReflectionProbeCatalog_t *c ) {
	ralReflectionProbeCatalog_t expected;
	return c&&c->schemaVersion==RAL_REFLECTION_PROBE_SCHEMA_VERSION&&c->ready==qtrue
		&&Build(&c->globalCubemap,c->locals,c->localCount,c->catalogGeneration,&expected)
		&&!memcmp(c,&expected,sizeof(*c));
}

static uint32_t Coverage( const ralLocalReflectionProbe_t *p,ralProbeVec3Q16_t pos ) {
	int32_t edge=pos.x-p->boundsMin.x,d=p->boundsMax.x-pos.x;
	if(d<edge)edge=d;d=pos.y-p->boundsMin.y;if(d<edge)edge=d;d=p->boundsMax.y-pos.y;if(d<edge)edge=d;
	d=pos.z-p->boundsMin.z;if(d<edge)edge=d;d=p->boundsMax.z-pos.z;if(d<edge)edge=d;
	if(edge<=0)return 0u;if(!p->blendDistanceQ16||edge>=p->blendDistanceQ16)return RAL_REFLECTION_PROBE_Q16_ONE;
	return (uint32_t)(((int64_t)edge*RAL_REFLECTION_PROBE_Q16_ONE)/p->blendDistanceQ16);
}

typedef struct { const ralLocalReflectionProbe_t *probe;uint32_t coverage; } Candidate;
static qboolean Better( const Candidate *a,const Candidate *b ) {
	return !b->probe||a->probe->priority>b->probe->priority
		||(a->probe->priority==b->probe->priority&&(a->coverage>b->coverage
		||(a->coverage==b->coverage&&a->probe->probeId<b->probe->probeId)));
}

static qboolean Parallax( const ralLocalReflectionProbe_t *p,
		ralProbeVec3Q16_t pos,ralProbeVec3Q16_t dir,ralProbeVec3Q16_t *out ) {
	int32_t pv[3]={pos.x,pos.y,pos.z},dv[3]={dir.x,dir.y,dir.z};
	int32_t mn[3]={p->boundsMin.x,p->boundsMin.y,p->boundsMin.z};
	int32_t mx[3]={p->boundsMax.x,p->boundsMax.y,p->boundsMax.z};
	int32_t cp[3]={p->capturePosition.x,p->capturePosition.y,p->capturePosition.z};
	int32_t result[3];int64_t t=INT64_MAX;uint32_t i;
	for(i=0u;i<3u;i++)if(dv[i]){
		int64_t delta=(dv[i]>0?mx[i]:mn[i])-pv[i];int64_t axis=(delta*RAL_REFLECTION_PROBE_Q16_ONE)/dv[i];
		if(axis>=0&&axis<t)t=axis;
	}
	if(t==INT64_MAX)return qfalse;
	for(i=0u;i<3u;i++){
		int64_t hit=pv[i]+((int64_t)dv[i]*t)/RAL_REFLECTION_PROBE_Q16_ONE;
		int64_t corrected=hit-cp[i];if(corrected<=INT32_MIN||corrected>=INT32_MAX)return qfalse;
		result[i]=(int32_t)corrected;
	}
	if(!result[0]&&!result[1]&&!result[2])return qfalse;
	out->x=result[0];out->y=result[1];out->z=result[2];return qtrue;
}

qboolean Ral_ReflectionProbeResolve( const ralReflectionProbeCatalog_t *c,
		const ralReflectionProbeQuery_t *q,uint32_t capacity,ralReflectionProbeReceipt_t *out ) {
	Candidate best[RAL_REFLECTION_PROBE_MAX_SELECTED];ralReflectionProbeReceipt_t v;
	uint32_t i,j,count=0u,localCoverage=0u,total=0u;
	if(!out||!Ral_ReflectionProbeCatalogValid(c)||!q
		||q->schemaVersion!=RAL_REFLECTION_PROBE_QUERY_SCHEMA_VERSION
		||q->catalogGeneration!=c->catalogGeneration||!q->queryGeneration
		||!VecValid(q->position)||!VecValid(q->reflectionDirection)
		||(!q->reflectionDirection.x&&!q->reflectionDirection.y&&!q->reflectionDirection.z))return qfalse;
	memset(best,0,sizeof(best));
	for(i=0u;i<c->localCount;i++)if(Inside(q->position,c->locals[i].boundsMin,c->locals[i].boundsMax)){
		Candidate x={&c->locals[i],Coverage(&c->locals[i],q->position)};if(!x.coverage)continue;
		for(j=0u;j<RAL_REFLECTION_PROBE_MAX_SELECTED;j++)if(Better(&x,&best[j])){
			Candidate displaced=best[j];best[j]=x;x=displaced;if(!x.probe)break;
		}
	}
	while(count<RAL_REFLECTION_PROBE_MAX_SELECTED&&best[count].probe)count++;
	if(capacity<count)return qfalse;
	memset(&v,0,sizeof(v));v.schemaVersion=RAL_REFLECTION_PROBE_RECEIPT_SCHEMA_VERSION;
	v.catalogGeneration=c->catalogGeneration;v.queryGeneration=q->queryGeneration;v.globalCubemap=c->globalCubemap;
	for(i=0u;i<count;i++){total+=best[i].coverage;if(best[i].coverage>localCoverage)localCoverage=best[i].coverage;}
	v.selectedCount=count;v.globalWeightQ16=RAL_REFLECTION_PROBE_Q16_ONE-localCoverage;
	for(i=0u;i<count;i++){
		ralReflectionProbeSelection_t *s=&v.selected[i];s->probeId=best[i].probe->probeId;
		s->probeGeneration=best[i].probe->probeGeneration;s->cubemap=best[i].probe->cubemap;
		s->weightQ16=i+1u==count?localCoverage:(uint32_t)(((uint64_t)localCoverage*best[i].coverage)/total);
		localCoverage-=s->weightQ16;total-=best[i].coverage;
		if(!Parallax(best[i].probe,q->position,q->reflectionDirection,&s->parallaxDirection))return qfalse;
	}
	v.ready=qtrue;*out=v;return qtrue;
}

qboolean Ral_ReflectionProbeReceiptValid( const ralReflectionProbeReceipt_t *r ) {
	uint32_t sum,i,j;if(!r||r->schemaVersion!=RAL_REFLECTION_PROBE_RECEIPT_SCHEMA_VERSION
		||!r->catalogGeneration||!r->queryGeneration||r->ready!=qtrue
		||r->selectedCount>RAL_REFLECTION_PROBE_MAX_SELECTED||!CubeValid(&r->globalCubemap))return qfalse;
	sum=r->globalWeightQ16;for(i=0u;i<r->selectedCount;i++){
		const ralReflectionProbeSelection_t*s=&r->selected[i];
		if(!s->probeId||!s->probeGeneration||!s->weightQ16||!CubeValid(&s->cubemap)
			||s->cubemap.backendType!=r->globalCubemap.backendType
			||(!s->parallaxDirection.x&&!s->parallaxDirection.y&&!s->parallaxDirection.z))return qfalse;
		for(j=0u;j<i;j++)if(r->selected[j].probeId==s->probeId
			||r->selected[j].cubemap.textureIdentity==s->cubemap.textureIdentity)return qfalse;
		sum+=s->weightQ16;
	}
	for(i=r->selectedCount;i<RAL_REFLECTION_PROBE_MAX_SELECTED;i++){
		ralReflectionProbeSelection_t zero;memset(&zero,0,sizeof(zero));
		if(memcmp(&r->selected[i],&zero,sizeof(zero)))return qfalse;
	}
	return sum==RAL_REFLECTION_PROBE_Q16_ONE;
}
qboolean Ral_ReflectionProbeReceiptExact( const ralReflectionProbeReceipt_t *a,
		const ralReflectionProbeReceipt_t *b ) {
	return Ral_ReflectionProbeReceiptValid(a)&&Ral_ReflectionProbeReceiptValid(b)
		&&!memcmp(a,b,sizeof(*a));
}
