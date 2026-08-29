// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors
#include "ral_lighting_product.h"
#include <string.h>
static void Put16(unsigned char*b,uint16_t v){b[0]=(unsigned char)v;b[1]=(unsigned char)(v>>8u);}
static void Put32(unsigned char*b,uint32_t v){uint32_t i;for(i=0;i<4u;i++)b[i]=(unsigned char)(v>>(i*8u));}
static void Put64(unsigned char*b,uint64_t v){uint32_t i;for(i=0;i<8u;i++)b[i]=(unsigned char)(v>>(i*8u));}
qboolean Ral_LightingProductWrite(const ralLightingProductRequest_t*q,void*radianceMemory,uint64_t radianceCapacity,
		void*directionMemory,uint64_t directionCapacity,void*artifact,uint64_t artifactCapacity,ralLightingArtifactReceipt_t*out){
	ralLightingArtifactDefinition_t d;ralLightingPayloadView_t payloads[3];unsigned char*radiance=(unsigned char*)radianceMemory;
	unsigned char*direction=(unsigned char*)directionMemory;uint64_t texels,radianceBytes,directionBytes;uint32_t i,payloadCount=2u;
	if(!q||!out||q->schemaVersion!=RAL_LIGHTING_PRODUCT_SCHEMA_VERSION||!q->artifactGeneration
		||!Ral_LightingBakeReceiptValid(&q->bake)||!q->pageWidth||!q->pageHeight||!q->pageCount
		||!q->indirectRadiance||!q->dominantDirection||!q->texelCount||!radiance||!direction||!artifact
		||(q->encoding!=RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8&&q->encoding!=RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16))return qfalse;
	texels=(uint64_t)q->pageWidth*q->pageHeight*q->pageCount;if(texels!=q->texelCount)return qfalse;
	radianceBytes=texels*(q->encoding==RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8?4u:8u);
	directionBytes=texels*(q->encoding==RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8?2u:4u);
	if(radianceCapacity<radianceBytes||directionCapacity<directionBytes
		||((q->stationaryVisibility!=NULL)!=(q->stationaryVisibilityCount!=0u))
		||(q->stationaryVisibility&&q->stationaryVisibilityCount!=q->texelCount))return qfalse;
	for(i=0;i<q->texelCount;i++){int32_t rgbQ16[3]={q->indirectRadiance[i].x,q->indirectRadiance[i].y,q->indirectRadiance[i].z};
		if(q->encoding==RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8){uint32_t rgb;uint16_t oct;
			if(!Ral_StaticLightingEncodeRgb9e5(rgbQ16,&rgb)||!Ral_StaticLightingEncodeOct8(q->dominantDirection[i],&oct))return qfalse;
			Put32(radiance+(uint64_t)i*4u,rgb);Put16(direction+(uint64_t)i*2u,oct);
		}else{uint64_t rgba;uint32_t oct;
			if(!Ral_StaticLightingEncodeRgba16f(rgbQ16,&rgba)||!Ral_StaticLightingEncodeOct16(q->dominantDirection[i],&oct))return qfalse;
			Put64(radiance+(uint64_t)i*8u,rgba);Put32(direction+(uint64_t)i*4u,oct);}}
	memset(&d,0,sizeof(d));d.schemaVersion=RAL_LIGHTING_ARTIFACT_SCHEMA_VERSION;d.kind=RAL_LIGHTING_ARTIFACT_DIRECTIONAL_LIGHTMAP;
	d.artifactGeneration=q->artifactGeneration;d.cacheKey=q->bake.staticIndirectKey;d.producerVersion=q->bake.producerVersion;
	d.encoding=q->encoding;d.flags=RAL_LIGHTING_ARTIFACT_STATIC_DIFFUSE_INDIRECT_ONLY;d.dimensions[0]=q->pageWidth;
	d.dimensions[1]=q->pageHeight;d.dimensions[2]=q->pageCount;d.payloadCount=payloadCount+(q->stationaryVisibility?1u:0u);
	payloads[0]=(ralLightingPayloadView_t){RAL_LIGHTING_PAYLOAD_RADIANCE,radiance,radianceBytes};
	payloads[1]=(ralLightingPayloadView_t){RAL_LIGHTING_PAYLOAD_DIRECTION,direction,directionBytes};
	if(q->stationaryVisibility){d.flags|=RAL_LIGHTING_ARTIFACT_HAS_STATIONARY_VISIBILITY;
		payloads[2]=(ralLightingPayloadView_t){RAL_LIGHTING_PAYLOAD_STATIONARY_VISIBILITY,q->stationaryVisibility,q->stationaryVisibilityCount};}
	return Ral_LightingArtifactWrite(&d,payloads,artifact,artifactCapacity,out);}
