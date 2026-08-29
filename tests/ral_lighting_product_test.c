// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "ral_lighting_product.h"
#include <stdio.h>
#include <string.h>
#define C(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define Q(x) ((x)*RAL_LIGHT_Q16_ONE)
int main(void){ralLightingProductRequest_t q;ralLightingArtifactReceipt_t primary,fallback,read;
	ralLightingPayloadView_t views[5];ralLightVec3Q16_t radiance[2]={{Q(4),Q(1),0},{Q(1),Q(2),Q(3)}};
	ralLightVec3Q16_t direction[2]={{0,0,Q(1)},{Q(1),0,Q(1)}};uint8_t visibility[2]={255,128};
	unsigned char radianceScratch[16],directionScratch[8],artifact[512];memset(&q,0,sizeof(q));
	q.schemaVersion=RAL_LIGHTING_PRODUCT_SCHEMA_VERSION;q.artifactGeneration=7;q.bake.schemaVersion=RAL_LIGHTING_BAKE_RECEIPT_SCHEMA_VERSION;
	q.bake.bakeGeneration=1;q.bake.staticIndirectKey=2;q.bake.producerVersion=3;q.bake.radianceHash=4;q.bake.directionHash=5;
	q.bake.patchCount=2;q.bake.linkCount=1;q.bake.completedBounces=4;q.bake.ready=qtrue;q.pageWidth=2;q.pageHeight=q.pageCount=1;
	q.indirectRadiance=radiance;q.dominantDirection=direction;q.texelCount=2;q.stationaryVisibility=visibility;q.stationaryVisibilityCount=2;
	q.encoding=RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8;C(Ral_LightingProductWrite(&q,radianceScratch,sizeof(radianceScratch),directionScratch,sizeof(directionScratch),artifact,sizeof(artifact),&primary));
	C(Ral_LightingArtifactRead(artifact,primary.byteLength,&read,views)&&Ral_LightingArtifactReceiptExact(&primary,&read));
	C(views[0].byteLength==8&&views[1].byteLength==4&&views[2].byteLength==2);
	q.encoding=RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16;q.artifactGeneration++;
	C(Ral_LightingProductWrite(&q,radianceScratch,sizeof(radianceScratch),directionScratch,sizeof(directionScratch),artifact,sizeof(artifact),&fallback));
	C(fallback.encoding==RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16&&fallback.payloads[0].byteLength==16&&fallback.payloads[1].byteLength==8);
	q.texelCount=1;C(!Ral_LightingProductWrite(&q,radianceScratch,sizeof(radianceScratch),directionScratch,sizeof(directionScratch),artifact,sizeof(artifact),&fallback));
	puts("ral_lighting_product_test: ok");return 0;}
