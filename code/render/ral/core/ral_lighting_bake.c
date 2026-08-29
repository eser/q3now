// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "ral_lighting_bake.h"
#include <limits.h>
#include <string.h>
#define FO UINT64_C(14695981039346656037)
#define FP UINT64_C(1099511628211)
static uint64_t B(uint64_t h,unsigned char v){return(h^v)*FP;}
static uint64_t U32(uint64_t h,uint32_t v){uint32_t i;for(i=0;i<4;i++)h=B(h,(unsigned char)(v>>(i*8)));return h;}
static uint64_t U64(uint64_t h,uint64_t v){uint32_t i;for(i=0;i<8;i++)h=B(h,(unsigned char)(v>>(i*8)));return h;}
static uint64_t V3(uint64_t h,ralLightVec3Q16_t v){h=U32(h,(uint32_t)v.x);h=U32(h,(uint32_t)v.y);return U32(h,(uint32_t)v.z);}
static uint64_t Sqrt64(uint64_t value);
static uint64_t Abs64(int64_t value);
static int32_t Channel(const ralLightVec3Q16_t*v,uint32_t c){return c==0?v->x:c==1?v->y:v->z;}
static void SetChannel(ralLightVec3Q16_t*v,uint32_t c,int32_t x){if(c==0)v->x=x;else if(c==1)v->y=x;else v->z=x;}
static qboolean RequestValid(const ralLightingBakeRequest_t*q){uint32_t i;
	if(!q||q->schemaVersion!=RAL_LIGHTING_BAKE_SCHEMA_VERSION||!q->bakeGeneration
		||!q->staticIndirectKey||!q->staticBakeHash||!q->producerVersion||!q->settingsHash
		||!q->patches||!q->patchCount||q->patchCount>RAL_LIGHTING_BAKE_MAX_PATCHES
		||q->linkCount>RAL_LIGHTING_BAKE_MAX_LINKS||(q->linkCount&&!q->links)
		||!q->bounceCount||q->bounceCount>RAL_LIGHTING_BAKE_MAX_BOUNCES
		||q->energyClampQ16<=0||q->energyClampQ16>RAL_LIGHTING_BAKE_MAX_ENERGY_Q16)return qfalse;
	for(i=0;i<q->patchCount;i++){uint32_t c;if(!q->patches[i].patchId||!q->patches[i].sourceGeneration
		||!q->patches[i].provenanceHash||!q->patches[i].regionId||q->patches[i].areaQ16<=0
		||(i&&q->patches[i-1].patchId>=q->patches[i].patchId))return qfalse;
		for(c=0;c<3;c++)if(q->patches[i].diffuseReflectanceQ16[c]<0
			||q->patches[i].diffuseReflectanceQ16[c]>RAL_LIGHT_Q16_ONE
			||q->patches[i].emissionRadianceQ16[c]<0
			||q->patches[i].emissionRadianceQ16[c]>q->energyClampQ16)return qfalse;}
	for(i=0;i<q->linkCount;i++){const ralLightingBakeLink_t*l=&q->links[i];
		if(l->receiverPatch>=q->patchCount||l->emitterPatch>=q->patchCount
			||l->receiverPatch==l->emitterPatch||!l->formFactorQ16
			||l->formFactorQ16>RAL_LIGHT_Q16_ONE)return qfalse;
		if(i&&(q->links[i-1].receiverPatch>l->receiverPatch
			||(q->links[i-1].receiverPatch==l->receiverPatch
			&&q->links[i-1].emitterPatch>=l->emitterPatch)))return qfalse;}
	return qtrue;}
qboolean Ral_LightingBakeReceiptValid(const ralLightingBakeReceipt_t*r){return r
	&&r->schemaVersion==RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION&&r->bakeGeneration
	&&r->staticIndirectKey&&r->producerVersion&&r->radianceHash&&r->directionHash&&r->patchCount
	&&r->patchCount<=RAL_LIGHTING_BAKE_MAX_PATCHES&&r->linkCount<=RAL_LIGHTING_BAKE_MAX_LINKS
	&&r->completedBounces&&r->completedBounces<=RAL_LIGHTING_BAKE_MAX_BOUNCES&&r->ready==qtrue;}
qboolean Ral_LightingBakeRun(const ralLightingBakeRequest_t*q,
		ralLightVec3Q16_t*incoming,uint32_t incomingCapacity,
		ralLightVec3Q16_t*outgoing,uint32_t outgoingCapacity,
		ralLightingBakeDirectionAccum_t*directionAccum,uint32_t directionScratchCapacity,
		ralLightVec3Q16_t*indirect,uint32_t outputCapacity,
		ralLightVec3Q16_t*dominantDirection,uint32_t directionCapacity,ralLightingBakeReceipt_t*out){
	ralLightingBakeReceipt_t r;uint32_t i,bounce,c;uint64_t hash,directionHash;
	if(!out||!RequestValid(q)||!incoming||incomingCapacity<q->patchCount
		||!outgoing||outgoingCapacity<q->patchCount||!directionAccum||directionScratchCapacity<q->patchCount
		||!indirect||outputCapacity<q->patchCount||!dominantDirection||directionCapacity<q->patchCount)return qfalse;
	memset(incoming,0,q->patchCount*sizeof(*incoming));memset(outgoing,0,q->patchCount*sizeof(*outgoing));
	memset(directionAccum,0,q->patchCount*sizeof(*directionAccum));memset(indirect,0,q->patchCount*sizeof(*indirect));
	memset(dominantDirection,0,q->patchCount*sizeof(*dominantDirection));
	for(i=0;i<q->patchCount;i++){outgoing[i].x=q->patches[i].emissionRadianceQ16[0];
		outgoing[i].y=q->patches[i].emissionRadianceQ16[1];outgoing[i].z=q->patches[i].emissionRadianceQ16[2];}
	memset(&r,0,sizeof(r));r.schemaVersion=RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
	r.bakeGeneration=q->bakeGeneration;r.staticIndirectKey=q->staticIndirectKey;r.producerVersion=q->producerVersion;
	r.patchCount=q->patchCount;r.linkCount=q->linkCount;
	for(bounce=0;bounce<q->bounceCount;bounce++){
		memset(incoming,0,q->patchCount*sizeof(*incoming));
		for(i=0;i<q->linkCount;i++){const ralLightingBakeLink_t*l=&q->links[i];
			int64_t dx=(int64_t)q->patches[l->emitterPatch].centroid.x-q->patches[l->receiverPatch].centroid.x;
			int64_t dy=(int64_t)q->patches[l->emitterPatch].centroid.y-q->patches[l->receiverPatch].centroid.y;
			int64_t dz=(int64_t)q->patches[l->emitterPatch].centroid.z-q->patches[l->receiverPatch].centroid.z;
			uint64_t distance=Sqrt64((uint64_t)(dx*dx)+(uint64_t)(dy*dy)+(uint64_t)(dz*dz));
			int64_t luminance=((int64_t)Channel(&outgoing[l->emitterPatch],0)*54
				+(int64_t)Channel(&outgoing[l->emitterPatch],1)*183
				+(int64_t)Channel(&outgoing[l->emitterPatch],2)*19)/256;
			for(c=0;c<3;c++){int64_t sum=(int64_t)Channel(&incoming[l->receiverPatch],c)
				+((int64_t)Channel(&outgoing[l->emitterPatch],c)*l->formFactorQ16)/RAL_LIGHT_Q16_ONE;
				if(sum>q->energyClampQ16){sum=q->energyClampQ16;r.clampedChannelCount++;}
				SetChannel(&incoming[l->receiverPatch],c,(int32_t)sum);}
			if(distance&&luminance>0){int64_t weight=luminance*l->formFactorQ16/RAL_LIGHT_Q16_ONE;
				directionAccum[l->receiverPatch].x+=(dx*RAL_LIGHT_Q16_ONE/(int64_t)distance)*weight/RAL_LIGHT_Q16_ONE;
				directionAccum[l->receiverPatch].y+=(dy*RAL_LIGHT_Q16_ONE/(int64_t)distance)*weight/RAL_LIGHT_Q16_ONE;
				directionAccum[l->receiverPatch].z+=(dz*RAL_LIGHT_Q16_ONE/(int64_t)distance)*weight/RAL_LIGHT_Q16_ONE;}}
		for(i=0;i<q->patchCount;i++)for(c=0;c<3;c++){
			int64_t reflected=((int64_t)Channel(&incoming[i],c)*q->patches[i].diffuseReflectanceQ16[c])/RAL_LIGHT_Q16_ONE;
			int64_t total=(int64_t)Channel(&indirect[i],c)+reflected;
			if(reflected>q->energyClampQ16){reflected=q->energyClampQ16;r.clampedChannelCount++;}
			if(total>q->energyClampQ16){total=q->energyClampQ16;r.clampedChannelCount++;}
			SetChannel(&outgoing[i],c,(int32_t)reflected);SetChannel(&indirect[i],c,(int32_t)total);}
		r.completedBounces++;
	}
	for(i=0;i<q->patchCount;i++){uint64_t sum=Abs64(directionAccum[i].x)+Abs64(directionAccum[i].y)+Abs64(directionAccum[i].z);
		if(sum){dominantDirection[i].x=(int32_t)(directionAccum[i].x*RAL_LIGHT_Q16_ONE/(int64_t)sum);
			dominantDirection[i].y=(int32_t)(directionAccum[i].y*RAL_LIGHT_Q16_ONE/(int64_t)sum);
			dominantDirection[i].z=(int32_t)(directionAccum[i].z*RAL_LIGHT_Q16_ONE/(int64_t)sum);}}
	hash=U32(FO,RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION);hash=U64(hash,q->staticIndirectKey);
	hash=U64(hash,q->staticBakeHash);hash=U64(hash,q->producerVersion);hash=U64(hash,q->settingsHash);
	hash=U32(hash,q->patchCount);hash=U32(hash,q->linkCount);hash=U32(hash,q->bounceCount);
	for(i=0;i<q->patchCount;i++){hash=U64(hash,q->patches[i].patchId);hash=U64(hash,q->patches[i].provenanceHash);
		hash=U32(hash,(uint32_t)indirect[i].x);
		hash=U32(hash,(uint32_t)indirect[i].y);hash=U32(hash,(uint32_t)indirect[i].z);}
	directionHash=U64(U32(FO,RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION),q->staticIndirectKey);
	for(i=0;i<q->patchCount;i++){directionHash=U64(directionHash,q->patches[i].patchId);directionHash=V3(directionHash,dominantDirection[i]);}
	r.radianceHash=hash?hash:1u;r.directionHash=directionHash?directionHash:1u;r.ready=qtrue;
	if(!Ral_LightingBakeReceiptValid(&r))return qfalse;*out=r;return qtrue;}

static uint64_t Sqrt64(uint64_t value){uint64_t result=0u,bit=UINT64_C(1)<<62;
	while(bit>value)bit>>=2u;while(bit){if(value>=result+bit){value-=result+bit;result=(result>>1u)+bit;}else result>>=1u;bit>>=2u;}return result;}
static uint64_t Abs64(int64_t value){return value<0?(uint64_t)(-value):(uint64_t)value;}
static int64_t Cross(int32_t a0,int32_t a1,int32_t b0,int32_t b1){return(int64_t)a0*b1-(int64_t)a1*b0;}
static qboolean Delta(int32_t a,int32_t b,int32_t*out){int64_t d=(int64_t)a-b;if(d<INT32_MIN||d>INT32_MAX)return qfalse;*out=(int32_t)d;return qtrue;}
static qboolean PatchCoord(int32_t value){return value>-(1<<29)&&value<(1<<29);}
static qboolean TrianglePatch(const ralLightingPatchTriangle_t*t,ralLightingBakePatch_t*out){
	int32_t ax,ay,az,bx,by,bz;int64_t cross[3],scaled[3];uint64_t maximum,length,originalLength;uint32_t shift=0,c;uint64_t h;
	if(!t||!out||t->schemaVersion!=RAL_LIGHTING_PATCH_SCHEMA_VERSION||!t->triangleId||!t->surfaceId
		||!t->sourceGeneration||!t->provenanceHash||!t->regionId)return qfalse;
	for(c=0;c<3;c++)if(!PatchCoord(t->vertices[c].x)||!PatchCoord(t->vertices[c].y)||!PatchCoord(t->vertices[c].z))return qfalse;
	for(c=0;c<3;c++)if(t->diffuseReflectanceQ16[c]<0||t->diffuseReflectanceQ16[c]>RAL_LIGHT_Q16_ONE
		||t->emissionRadianceQ16[c]<0||t->emissionRadianceQ16[c]>RAL_LIGHTING_BAKE_MAX_ENERGY_Q16)return qfalse;
	if(!Delta(t->vertices[1].x,t->vertices[0].x,&ax)||!Delta(t->vertices[1].y,t->vertices[0].y,&ay)
		||!Delta(t->vertices[1].z,t->vertices[0].z,&az)||!Delta(t->vertices[2].x,t->vertices[0].x,&bx)
		||!Delta(t->vertices[2].y,t->vertices[0].y,&by)||!Delta(t->vertices[2].z,t->vertices[0].z,&bz))return qfalse;
	cross[0]=Cross(ay,az,by,bz);cross[1]=Cross(az,ax,bz,bx);cross[2]=Cross(ax,ay,bx,by);
	maximum=Abs64(cross[0]);if(Abs64(cross[1])>maximum)maximum=Abs64(cross[1]);if(Abs64(cross[2])>maximum)maximum=Abs64(cross[2]);
	if(!maximum)return qfalse;while((maximum>>shift)>UINT32_C(1073741823))shift++;
	for(c=0;c<3;c++)scaled[c]=cross[c]>>shift;
	length=Sqrt64((uint64_t)(scaled[0]*scaled[0])+(uint64_t)(scaled[1]*scaled[1])+(uint64_t)(scaled[2]*scaled[2]));
	if(!length||length>(UINT64_MAX>>shift))return qfalse;originalLength=length<<shift;
	if(originalLength/(2u*RAL_LIGHT_Q16_ONE)>INT32_MAX)return qfalse;
	memset(out,0,sizeof(*out));h=U64(U64(U64(U64(U32(FO,RAL_LIGHTING_PATCH_SCHEMA_VERSION),
		t->surfaceId),t->triangleId),t->sourceGeneration),t->provenanceHash);
	out->patchId=t->triangleId;out->sourceGeneration=t->sourceGeneration;out->provenanceHash=h?h:1u;out->regionId=t->regionId;
	out->centroid.x=(int32_t)(((int64_t)t->vertices[0].x+t->vertices[1].x+t->vertices[2].x)/3);
	out->centroid.y=(int32_t)(((int64_t)t->vertices[0].y+t->vertices[1].y+t->vertices[2].y)/3);
	out->centroid.z=(int32_t)(((int64_t)t->vertices[0].z+t->vertices[1].z+t->vertices[2].z)/3);
	out->normal.x=(int32_t)(scaled[0]*RAL_LIGHT_Q16_ONE/(int64_t)length);
	out->normal.y=(int32_t)(scaled[1]*RAL_LIGHT_Q16_ONE/(int64_t)length);
	out->normal.z=(int32_t)(scaled[2]*RAL_LIGHT_Q16_ONE/(int64_t)length);
	out->areaQ16=(int32_t)(originalLength/(2u*RAL_LIGHT_Q16_ONE));
	memcpy(out->diffuseReflectanceQ16,t->diffuseReflectanceQ16,sizeof(out->diffuseReflectanceQ16));
	memcpy(out->emissionRadianceQ16,t->emissionRadianceQ16,sizeof(out->emissionRadianceQ16));return out->areaQ16>0;}
static qboolean DirtyRegion(const ralLightingPatchGraphRequest_t*q,uint64_t region){uint32_t i;if(!q->dirtyRegionCount)return qtrue;
	for(i=0;i<q->dirtyRegionCount;i++)if(q->dirtyRegionIds[i]==region)return qtrue;return qfalse;}
static uint32_t FormFactor(const ralLightingBakePatch_t*receiver,const ralLightingBakePatch_t*emitter,uint32_t visibility){
	int64_t dx=(int64_t)emitter->centroid.x-receiver->centroid.x,dy=(int64_t)emitter->centroid.y-receiver->centroid.y,dz=(int64_t)emitter->centroid.z-receiver->centroid.z;
	uint64_t distanceSquared=(uint64_t)(dx*dx)+(uint64_t)(dy*dy)+(uint64_t)(dz*dz),distance,denominator;int64_t dotR,dotE,value;
	if(!distanceSquared)return 0u;distance=Sqrt64(distanceSquared);if(!distance)return 0u;
	dotR=((int64_t)receiver->normal.x*dx+(int64_t)receiver->normal.y*dy+(int64_t)receiver->normal.z*dz)/(int64_t)distance;
	dotE=-((int64_t)emitter->normal.x*dx+(int64_t)emitter->normal.y*dy+(int64_t)emitter->normal.z*dz)/(int64_t)distance;
	if(dotR<=0||dotE<=0)return 0u;value=dotR*dotE/RAL_LIGHT_Q16_ONE;value=value*visibility/RAL_LIGHT_Q16_ONE;
	value=value*emitter->areaQ16/RAL_LIGHT_Q16_ONE;
	denominator=(distanceSquared/((uint64_t)RAL_LIGHT_Q16_ONE*RAL_LIGHT_Q16_ONE))*UINT64_C(205887);
	if(!denominator||value<=0)return 0u;value=(value*RAL_LIGHT_Q16_ONE)/(int64_t)denominator;
	if(value>RAL_LIGHT_Q16_ONE)value=RAL_LIGHT_Q16_ONE;return(uint32_t)value;}
qboolean Ral_LightingPatchGraphReceiptValid(const ralLightingPatchGraphReceipt_t*r){return r
	&&r->schemaVersion==RAL_LIGHTING_PATCH_GRAPH_RECEIPT_SCHEMA_VERSION&&r->graphGeneration&&r->geometryHash
	&&r->materialHash&&r->emissiveHash&&r->graphHash&&r->patchCount&&r->patchCount<=RAL_LIGHTING_BAKE_MAX_PATCHES
	&&r->linkCount<=RAL_LIGHTING_BAKE_MAX_LINKS&&r->dirtyPatchCount<=r->patchCount&&r->ready==qtrue;}
qboolean Ral_LightingPatchGraphBuild(const ralLightingPatchGraphRequest_t*q,ralLightingBakePatch_t*patches,uint32_t patchCapacity,
		ralLightingBakeLink_t*links,uint32_t linkCapacity,uint8_t*dirty,uint32_t dirtyCapacity,ralLightingPatchGraphReceipt_t*out){
	ralLightingPatchGraphReceipt_t r;uint32_t i,count=0;uint64_t gh=FO,mh=FO,eh=FO,graph=FO;
	if(!out||!q||q->schemaVersion!=RAL_LIGHTING_PATCH_GRAPH_SCHEMA_VERSION||!q->graphGeneration||!q->triangles
		||!q->triangleCount||q->triangleCount>RAL_LIGHTING_BAKE_MAX_PATCHES||patchCapacity<q->triangleCount||!patches
		||q->visibilityCount>RAL_LIGHTING_BAKE_MAX_LINKS||(q->visibilityCount&&!q->visibility)||linkCapacity<q->visibilityCount
		||(q->visibilityCount&&!links)||!dirty||dirtyCapacity<q->triangleCount||q->minimumFormFactorQ16>RAL_LIGHT_Q16_ONE
		||q->dirtyRegionCount>RAL_LIGHTING_PATCH_MAX_DIRTY_REGIONS)return qfalse;
	for(i=0;i<q->dirtyRegionCount;i++)if(!q->dirtyRegionIds[i]||(i&&q->dirtyRegionIds[i-1]>=q->dirtyRegionIds[i]))return qfalse;
	memset(&r,0,sizeof(r));
	for(i=0;i<q->triangleCount;i++){const ralLightingPatchTriangle_t*t=&q->triangles[i];uint32_t c;
		if((i&&q->triangles[i-1].triangleId>=t->triangleId)||!TrianglePatch(t,&patches[i]))return qfalse;
		dirty[i]=DirtyRegion(q,t->regionId)?1u:0u;r.dirtyPatchCount+=dirty[i];gh=U64(gh,t->triangleId);gh=V3(gh,t->vertices[0]);gh=V3(gh,t->vertices[1]);gh=V3(gh,t->vertices[2]);
		mh=U64(mh,t->provenanceHash);for(c=0;c<3;c++){mh=U32(mh,(uint32_t)t->diffuseReflectanceQ16[c]);eh=U32(eh,(uint32_t)t->emissionRadianceQ16[c]);}}
	for(i=0;i<q->visibilityCount;i++){const ralLightingPatchVisibility_t*v=&q->visibility[i];uint32_t ff;
		if(v->receiverTriangle>=q->triangleCount||v->emitterTriangle>=q->triangleCount||v->receiverTriangle==v->emitterTriangle
			||!v->visibilityQ16||v->visibilityQ16>RAL_LIGHT_Q16_ONE||(i&&(q->visibility[i-1].receiverTriangle>v->receiverTriangle
			||(q->visibility[i-1].receiverTriangle==v->receiverTriangle&&q->visibility[i-1].emitterTriangle>=v->emitterTriangle))))return qfalse;
		ff=FormFactor(&patches[v->receiverTriangle],&patches[v->emitterTriangle],v->visibilityQ16);if(!ff||ff<q->minimumFormFactorQ16)continue;
		links[count++]=(ralLightingBakeLink_t){v->receiverTriangle,v->emitterTriangle,ff};}
	r.schemaVersion=RAL_LIGHTING_PATCH_GRAPH_RECEIPT_SCHEMA_VERSION;r.graphGeneration=q->graphGeneration;
	r.geometryHash=gh?gh:1u;r.materialHash=mh?mh:1u;r.emissiveHash=eh?eh:1u;r.patchCount=q->triangleCount;r.linkCount=count;
	graph=U64(U64(U64(graph,r.geometryHash),r.materialHash),r.emissiveHash);
	for(i=0;i<count;i++){graph=U32(graph,links[i].receiverPatch);graph=U32(graph,links[i].emitterPatch);graph=U32(graph,links[i].formFactorQ16);}r.graphHash=graph?graph:1u;r.ready=qtrue;
	if(!Ral_LightingPatchGraphReceiptValid(&r))return qfalse;*out=r;return qtrue;}
