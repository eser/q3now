// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "ral_lighting_bake.h"
#include <stdio.h>
#include <string.h>
#define C(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define Q(x) ((x)*RAL_LIGHT_Q16_ONE)
static int Graph(void){ralLightingPatchTriangle_t triangles[3];ralLightingPatchVisibility_t visibility[4];
	ralLightingPatchGraphRequest_t q;ralLightingBakePatch_t patches[3],before[3];ralLightingBakeLink_t links[4];
	ralLightingPatchGraphReceipt_t r,r2;ralLightingBakeRequest_t bake;ralLightingBakeReceipt_t baked;
	ralLightingBakeDirectionAccum_t directionScratch[3];ralLightVec3Q16_t incoming[3],outgoing[3],indirect[3],direction[3];uint8_t dirty[3];memset(triangles,0,sizeof(triangles));
	for(int i=0;i<3;i++){triangles[i].schemaVersion=RAL_LIGHTING_PATCH_SCHEMA_VERSION;triangles[i].triangleId=(uint64_t)(i+1);
		triangles[i].surfaceId=(uint64_t)(10+i);triangles[i].sourceGeneration=20;triangles[i].provenanceHash=(uint64_t)(30+i);
		triangles[i].regionId=i==2?200:100;for(int c=0;c<3;c++)triangles[i].diffuseReflectanceQ16[c]=Q(1)/2;}
	triangles[0].vertices[0]=(ralLightVec3Q16_t){0,0,0};triangles[0].vertices[1]=(ralLightVec3Q16_t){Q(2),0,0};triangles[0].vertices[2]=(ralLightVec3Q16_t){0,Q(2),0};
	triangles[1].vertices[0]=(ralLightVec3Q16_t){0,0,Q(4)};triangles[1].vertices[1]=(ralLightVec3Q16_t){0,Q(2),Q(4)};triangles[1].vertices[2]=(ralLightVec3Q16_t){Q(2),0,Q(4)};
	triangles[2].vertices[0]=(ralLightVec3Q16_t){Q(2),0,0};triangles[2].vertices[1]=(ralLightVec3Q16_t){Q(4),0,0};triangles[2].vertices[2]=(ralLightVec3Q16_t){Q(2),Q(2),0};
	triangles[1].emissionRadianceQ16[0]=Q(4);triangles[1].emissionRadianceQ16[1]=Q(1);
	visibility[0]=(ralLightingPatchVisibility_t){0,1,Q(1)};visibility[1]=(ralLightingPatchVisibility_t){1,0,Q(1)};
	visibility[2]=(ralLightingPatchVisibility_t){1,2,Q(1)};visibility[3]=(ralLightingPatchVisibility_t){2,1,Q(1)};
	memset(&q,0,sizeof(q));q.schemaVersion=RAL_LIGHTING_PATCH_GRAPH_SCHEMA_VERSION;q.graphGeneration=1;q.triangles=triangles;
	q.triangleCount=3;q.visibility=visibility;q.visibilityCount=4;q.minimumFormFactorQ16=1;q.dirtyRegionCount=1;q.dirtyRegionIds[0]=100;
	C(Ral_LightingPatchGraphBuild(&q,patches,3,links,4,dirty,3,&r)&&Ral_LightingPatchGraphReceiptValid(&r));
	C(r.patchCount==3&&r.linkCount==4&&r.dirtyPatchCount==2&&dirty[0]&&dirty[1]&&!dirty[2]);
	C(patches[0].normal.z>0&&patches[1].normal.z<0&&patches[0].areaQ16==Q(2));
	memcpy(before,patches,sizeof(patches));C(Ral_LightingPatchGraphBuild(&q,patches,3,links,4,dirty,3,&r2));
	C(!memcmp(before,patches,sizeof(patches))&&r.graphHash==r2.graphHash);
	memset(&bake,0,sizeof(bake));bake.schemaVersion=RAL_LIGHTING_BAKE_SCHEMA_VERSION;bake.bakeGeneration=2;
	bake.staticIndirectKey=r.graphHash;bake.staticBakeHash=r.emissiveHash;bake.producerVersion=3;bake.settingsHash=4;
	bake.patches=patches;bake.patchCount=3;bake.links=links;bake.linkCount=r.linkCount;bake.bounceCount=4;bake.energyClampQ16=Q(16);
	C(Ral_LightingBakeRun(&bake,incoming,3,outgoing,3,directionScratch,3,indirect,3,direction,3,&baked));
	C(indirect[0].x>0&&indirect[2].x>0&&indirect[0].x>indirect[0].y);
	C(direction[0].z>0&&direction[2].z>0);return 0;}

static int LavaSanctumBoundedBounce(void){
	ralLightingBakePatch_t patches[4];ralLightingBakeLink_t links[2];ralLightingBakeRequest_t q;
	ralLightingBakeDirectionAccum_t directionScratch[4];ralLightVec3Q16_t incoming[4],outgoing[4],indirect[4],direction[4];
	ralLightingBakeReceipt_t receipt;memset(patches,0,sizeof(patches));
	for(int i=0;i<4;i++){patches[i].patchId=(uint64_t)(100+i);patches[i].sourceGeneration=7;
		patches[i].provenanceHash=(uint64_t)(200+i);patches[i].regionId=(uint64_t)(300+i);
		patches[i].centroid.x=Q(i*4);patches[i].normal.z=Q(1);patches[i].areaQ16=Q(2);
		patches[i].diffuseReflectanceQ16[0]=Q(1)/2;patches[i].diffuseReflectanceQ16[1]=Q(1)/4;
		patches[i].diffuseReflectanceQ16[2]=Q(1)/8;}
	patches[0].emissionRadianceQ16[0]=Q(6);patches[0].emissionRadianceQ16[1]=Q(2);
	patches[0].emissionRadianceQ16[2]=Q(1)/2;
	links[0]=(ralLightingBakeLink_t){1,0,Q(1)/3};links[1]=(ralLightingBakeLink_t){2,1,Q(1)/4};
	memset(&q,0,sizeof(q));q.schemaVersion=RAL_LIGHTING_BAKE_SCHEMA_VERSION;q.bakeGeneration=9;
	q.staticIndirectKey=10;q.staticBakeHash=11;q.producerVersion=12;q.settingsHash=13;
	q.patches=patches;q.patchCount=4;q.links=links;q.linkCount=2;q.bounceCount=4;q.energyClampQ16=Q(16);
	C(Ral_LightingBakeRun(&q,incoming,4,outgoing,4,directionScratch,4,indirect,4,direction,4,&receipt));
	C(indirect[1].x>indirect[1].y&&indirect[1].y>indirect[1].z&&indirect[2].x>0);
	C(indirect[3].x==0&&indirect[3].y==0&&indirect[3].z==0&&receipt.clampedChannelCount==0);
	return 0;}
int main(void){ralLightingBakePatch_t patches[3];ralLightingBakeLink_t links[4];ralLightingBakeRequest_t q;
	ralLightingBakeDirectionAccum_t directionScratch[3];ralLightVec3Q16_t a[3],b[3],out1[3],out2[3],direction[3];ralLightingBakeReceipt_t r1,r2,before;
	memset(patches,0,sizeof(patches));patches[0].patchId=10;patches[1].patchId=20;patches[2].patchId=30;
	for(int i=0;i<3;i++){patches[i].sourceGeneration=1;patches[i].provenanceHash=(uint64_t)(2+i);patches[i].regionId=1;
		patches[i].centroid.x=Q(i+1);patches[i].normal.z=Q(1);patches[i].areaQ16=Q(1);}
	patches[0].emissionRadianceQ16[0]=Q(4);patches[0].emissionRadianceQ16[1]=Q(1);
	for(int i=0;i<3;i++)for(int c=0;c<3;c++)patches[i].diffuseReflectanceQ16[c]=Q(1)/2;
	links[0]=(ralLightingBakeLink_t){0,1,Q(1)/4};links[1]=(ralLightingBakeLink_t){1,0,Q(1)/2};
	links[2]=(ralLightingBakeLink_t){2,0,Q(1)/4};links[3]=(ralLightingBakeLink_t){2,1,Q(1)/2};
	memset(&q,0,sizeof(q));q.schemaVersion=RAL_LIGHTING_BAKE_SCHEMA_VERSION;q.bakeGeneration=1;
	q.staticIndirectKey=2;q.staticBakeHash=3;q.producerVersion=4;q.settingsHash=5;q.patches=patches;
	q.patchCount=3;q.links=links;q.linkCount=4;q.bounceCount=4;q.energyClampQ16=Q(16);
	C(Ral_LightingBakeRun(&q,a,3,b,3,directionScratch,3,out1,3,direction,3,&r1)&&Ral_LightingBakeReceiptValid(&r1));
	C(out1[1].x>0&&out1[2].x>0&&out1[0].x>0);C(out1[1].x>out1[1].y&&out1[1].y>out1[1].z);
	C(Ral_LightingBakeRun(&q,a,3,b,3,directionScratch,3,out2,3,direction,3,&r2)&&!memcmp(out1,out2,sizeof(out1))
		&&r1.radianceHash==r2.radianceHash&&r1.directionHash==r2.directionHash);
	before=r2;q.bounceCount=17;C(!Ral_LightingBakeRun(&q,a,3,b,3,directionScratch,3,out2,3,direction,3,&r2)&&!memcmp(&r2,&before,sizeof(r2)));
	C(Graph()==0);C(LavaSanctumBoundedBounce()==0);puts("ral_lighting_bake_test: ok");return 0;}
