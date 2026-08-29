// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_static_lighting.h"

#include <limits.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static uint64_t HB(uint64_t h,unsigned char v){return(h^v)*FNV64_PRIME;}
static uint64_t H32(uint64_t h,uint32_t v){uint32_t i;for(i=0u;i<4u;i++)h=HB(h,(unsigned char)(v>>(i*8u)));return h;}
static uint64_t H64(uint64_t h,uint64_t v){uint32_t i;for(i=0u;i<8u;i++)h=HB(h,(unsigned char)(v>>(i*8u)));return h;}
static uint64_t NZ(uint64_t h){return h?h:1u;}
static uint64_t DescriptionHash(const ralStaticLightingDescription_t*d){
	uint64_t h=H32(FNV64_OFFSET,RAL_STATIC_LIGHTING_RECEIPT_SCHEMA_VERSION);
	h=H64(h,d->productGeneration);h=H64(h,d->cacheKey);h=H64(h,d->producerVersion);
	h=H64(h,d->geometryHash);h=H64(h,d->materialHash);h=H64(h,d->staticBakeHash);
	h=H64(h,d->radiancePayloadHash);h=H64(h,d->directionPayloadHash);
	h=H64(h,d->stationaryVisibilityPayloadHash);h=H32(h,d->pageWidth);h=H32(h,d->pageHeight);
	h=H32(h,d->pageCount);h=H32(h,d->bounceCount);h=H32(h,d->energyClampQ16);
	h=H32(h,(uint32_t)d->primaryEncoding);h=H32(h,(uint32_t)d->fallbackEncoding);
	return NZ(H32(h,(uint32_t)d->hasStationaryVisibility));
}
static qboolean DescriptionValid(const ralStaticLightingDescription_t*d){
	uint64_t texels;
	if(!d||d->schemaVersion!=RAL_STATIC_LIGHTING_SCHEMA_VERSION||!d->productGeneration
		||!d->cacheKey||!d->producerVersion||!d->geometryHash||!d->materialHash||!d->staticBakeHash
		||!d->radiancePayloadHash||!d->directionPayloadHash||!d->pageWidth||!d->pageHeight
		||!d->pageCount||!d->bounceCount||d->bounceCount>16u||!d->energyClampQ16
		||d->primaryEncoding<RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8
		||d->primaryEncoding>RAL_STATIC_LIGHTING_ENCODING_LEGACY_SRGB8
		||d->fallbackEncoding<RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8
		||d->fallbackEncoding>RAL_STATIC_LIGHTING_ENCODING_LEGACY_SRGB8
		||d->fallbackEncoding==d->primaryEncoding
		||(d->hasStationaryVisibility&&!d->stationaryVisibilityPayloadHash)
		||(!d->hasStationaryVisibility&&d->stationaryVisibilityPayloadHash))return qfalse;
	texels=(uint64_t)d->pageWidth*d->pageHeight*d->pageCount;
	return texels>0u&&texels<=UINT32_MAX;
}
qboolean Ral_StaticLightingBuild(const ralStaticLightingDescription_t*d,ralStaticLightingReceipt_t*out){
	ralStaticLightingReceipt_t v;if(!out||!DescriptionValid(d))return qfalse;memset(&v,0,sizeof(v));
	v.schemaVersion=RAL_STATIC_LIGHTING_RECEIPT_SCHEMA_VERSION;v.description=*d;
	v.artifactHash=DescriptionHash(d);v.ready=qtrue;*out=v;return qtrue;
}
qboolean Ral_StaticLightingReceiptValid(const ralStaticLightingReceipt_t*r){
	return r&&r->schemaVersion==RAL_STATIC_LIGHTING_RECEIPT_SCHEMA_VERSION&&r->ready==qtrue
		&&DescriptionValid(&r->description)&&r->artifactHash==DescriptionHash(&r->description);
}
static qboolean EncodingSupported(ralStaticLightingEncoding_t e,const ralStaticLightingCapabilities_t*c){
	if(e==RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8)return c->sampledRgb9e5;
	if(e==RAL_STATIC_LIGHTING_ENCODING_RGBA16F_OCT16)return c->sampledRgba16Float&&c->sampledRg16Snorm;
	return e==RAL_STATIC_LIGHTING_ENCODING_LEGACY_SRGB8;
}
qboolean Ral_StaticLightingPlanBuild(const ralStaticLightingPlanRequest_t*q,ralStaticLightingPlanReceipt_t*out){
	ralStaticLightingPlanReceipt_t v;const ralStaticLightingDescription_t*d;
	if(!out||!q||q->schemaVersion!=RAL_STATIC_LIGHTING_PLAN_SCHEMA_VERSION||!q->frameGeneration
		||!Ral_StaticLightingReceiptValid(&q->product))return qfalse;
	d=&q->product.description;
	if(d->pageCount>1u&&!q->capabilities.textureArrays)return qfalse;
	memset(&v,0,sizeof(v));v.schemaVersion=RAL_STATIC_LIGHTING_PLAN_RECEIPT_SCHEMA_VERSION;
	v.frameGeneration=q->frameGeneration;v.artifactHash=q->product.artifactHash;
	if(EncodingSupported(d->primaryEncoding,&q->capabilities)){v.mode=RAL_STATIC_LIGHTING_PLAN_PRIMARY;v.selectedEncoding=d->primaryEncoding;}
	else if(EncodingSupported(d->fallbackEncoding,&q->capabilities)){v.mode=d->fallbackEncoding==RAL_STATIC_LIGHTING_ENCODING_LEGACY_SRGB8?RAL_STATIC_LIGHTING_PLAN_LEGACY_COMPATIBILITY:RAL_STATIC_LIGHTING_PLAN_FALLBACK;v.selectedEncoding=d->fallbackEncoding;}
	else return qfalse;
	if(q->requireStationaryVisibility&&!d->hasStationaryVisibility)return qfalse;
	v.textureSampleCount=d->hasStationaryVisibility?3u:2u;v.staticDiffuseIndirectOnly=qtrue;
	v.stationaryVisibilityActive=d->hasStationaryVisibility;v.ready=qtrue;*out=v;return qtrue;
}
qboolean Ral_StaticLightingPlanReceiptValid(const ralStaticLightingPlanReceipt_t*r){
	return r&&r->schemaVersion==RAL_STATIC_LIGHTING_PLAN_RECEIPT_SCHEMA_VERSION&&r->frameGeneration
		&&r->artifactHash&&r->mode>=RAL_STATIC_LIGHTING_PLAN_PRIMARY&&r->mode<=RAL_STATIC_LIGHTING_PLAN_LEGACY_COMPATIBILITY
		&&r->selectedEncoding>=RAL_STATIC_LIGHTING_ENCODING_RGB9E5_OCT8
		&&r->selectedEncoding<=RAL_STATIC_LIGHTING_ENCODING_LEGACY_SRGB8
		&&r->textureSampleCount==(r->stationaryVisibilityActive?3u:2u)
		&&r->staticDiffuseIndirectOnly==qtrue&&r->ready==qtrue;
}

static uint32_t QuantizeSharedMantissa( uint32_t q16, uint32_t exponent ) {
	if ( exponent <= 8u ) return (uint32_t)( (uint64_t)q16 << ( 8u - exponent ) );
	{
		uint32_t shift = exponent - 8u;
		return (uint32_t)( ( (uint64_t)q16 + ( UINT64_C(1) << ( shift - 1u ) ) ) >> shift );
	}
}

qboolean Ral_StaticLightingEncodeRgb9e5( const int32_t radianceQ16[3],
		uint32_t *outPacked ) {
	uint32_t maximum, exponent, r, g, b;
	if ( !radianceQ16 || !outPacked || radianceQ16[0] < 0 || radianceQ16[1] < 0
			|| radianceQ16[2] < 0 ) return qfalse;
	maximum = (uint32_t)radianceQ16[0];
	if ( (uint32_t)radianceQ16[1] > maximum ) maximum = (uint32_t)radianceQ16[1];
	if ( (uint32_t)radianceQ16[2] > maximum ) maximum = (uint32_t)radianceQ16[2];
	for ( exponent = 0u; exponent < 32u; exponent++ )
		if ( QuantizeSharedMantissa( maximum, exponent ) <= 511u ) break;
	if ( exponent == 32u ) return qfalse;
	r = QuantizeSharedMantissa( (uint32_t)radianceQ16[0], exponent );
	g = QuantizeSharedMantissa( (uint32_t)radianceQ16[1], exponent );
	b = QuantizeSharedMantissa( (uint32_t)radianceQ16[2], exponent );
	if ( r > 511u || g > 511u || b > 511u ) return qfalse;
	*outPacked = r | ( g << 9u ) | ( b << 18u ) | ( exponent << 27u ); return qtrue;
}

static int32_t Abs32( int32_t value ) { return value < 0 ? -value : value; }
static int32_t Sign32( int32_t value ) { return value < 0 ? -1 : 1; }
static uint32_t OctUnorm( int32_t value, uint32_t maximum ) {
	int64_t shifted = (int64_t)value + RAL_LIGHT_Q16_ONE;
	return (uint32_t)( ( shifted * maximum + RAL_LIGHT_Q16_ONE )
		/ ( 2 * RAL_LIGHT_Q16_ONE ) );
}

qboolean Ral_StaticLightingEncodeOct8( ralLightVec3Q16_t direction,
		uint16_t *outPacked ) {
	int64_t sum64; int32_t x, y, z; uint32_t u, v;
	if ( !outPacked || direction.x == INT32_MIN || direction.y == INT32_MIN
			|| direction.z == INT32_MIN ) return qfalse;
	sum64 = (int64_t)Abs32( direction.x ) + Abs32( direction.y ) + Abs32( direction.z );
	if ( !sum64 || sum64 > INT32_MAX ) return qfalse;
	x = (int32_t)( (int64_t)direction.x * RAL_LIGHT_Q16_ONE / sum64 );
	y = (int32_t)( (int64_t)direction.y * RAL_LIGHT_Q16_ONE / sum64 );
	z = (int32_t)( (int64_t)direction.z * RAL_LIGHT_Q16_ONE / sum64 );
	if ( z < 0 ) {
		int32_t oldX = x;
		x = ( RAL_LIGHT_Q16_ONE - Abs32( y ) ) * Sign32( oldX );
		y = ( RAL_LIGHT_Q16_ONE - Abs32( oldX ) ) * Sign32( y );
	}
	u = OctUnorm( x, 255u ); v = OctUnorm( y, 255u );
	if ( u > 255u || v > 255u ) return qfalse;
	*outPacked = (uint16_t)( u | ( v << 8u ) ); return qtrue;
}

static uint16_t Q16ToHalf( uint32_t value ) {
	uint32_t highest = 0u, exponent, mantissa, base, shift;
	if ( !value ) return 0u;
	while ( ( value >> ( highest + 1u ) ) && highest < 30u ) highest++;
	if ( highest < 2u ) return (uint16_t)( value << 8u );
	exponent = highest - 1u; base = 1u << highest;
	if ( highest <= 10u ) mantissa = ( value - base ) << ( 10u - highest );
	else {
		shift = highest - 10u;
		mantissa = ( ( value - base ) + ( 1u << ( shift - 1u ) ) ) >> shift;
	}
	if ( mantissa == 1024u ) { mantissa = 0u; exponent++; }
	if ( exponent >= 31u ) return UINT16_C(0x7bff);
	return (uint16_t)( ( exponent << 10u ) | mantissa );
}

qboolean Ral_StaticLightingEncodeRgba16f( const int32_t radianceQ16[3],
		uint64_t *outPacked ) {
	uint16_t r, g, b;
	if ( !radianceQ16 || !outPacked || radianceQ16[0] < 0 || radianceQ16[1] < 0
			|| radianceQ16[2] < 0 ) return qfalse;
	r = Q16ToHalf( (uint32_t)radianceQ16[0] ); g = Q16ToHalf( (uint32_t)radianceQ16[1] );
	b = Q16ToHalf( (uint32_t)radianceQ16[2] );
	*outPacked = (uint64_t)r | ( (uint64_t)g << 16u ) | ( (uint64_t)b << 32u )
		| ( UINT64_C(0x3c00) << 48u ); return qtrue;
}

qboolean Ral_StaticLightingEncodeOct16( ralLightVec3Q16_t direction,
		uint32_t *outPacked ) {
	int64_t sum64; int32_t x, y, z; uint32_t u, v;
	if ( !outPacked || direction.x == INT32_MIN || direction.y == INT32_MIN
			|| direction.z == INT32_MIN ) return qfalse;
	sum64 = (int64_t)Abs32( direction.x ) + Abs32( direction.y ) + Abs32( direction.z );
	if ( !sum64 || sum64 > INT32_MAX ) return qfalse;
	x = (int32_t)( (int64_t)direction.x * RAL_LIGHT_Q16_ONE / sum64 );
	y = (int32_t)( (int64_t)direction.y * RAL_LIGHT_Q16_ONE / sum64 );
	z = (int32_t)( (int64_t)direction.z * RAL_LIGHT_Q16_ONE / sum64 );
	if ( z < 0 ) { int32_t oldX = x; x = ( RAL_LIGHT_Q16_ONE - Abs32( y ) ) * Sign32( oldX );
		y = ( RAL_LIGHT_Q16_ONE - Abs32( oldX ) ) * Sign32( y ); }
	u = OctUnorm( x, 65535u ); v = OctUnorm( y, 65535u );
	if ( u > 65535u || v > 65535u ) return qfalse;
	*outPacked = u | ( v << 16u ); return qtrue;
}

qboolean Ral_IrradianceProbeVolumeValid(const ralIrradianceProbeVolume_t*v){
	uint64_t count;if(!v||v->schemaVersion!=RAL_IRRADIANCE_VOLUME_SCHEMA_VERSION
		||!v->productGeneration||!v->cacheKey||!v->coefficientPayloadHash||!v->validityPayloadHash
		||v->spacing.x<=0||v->spacing.y<=0||v->spacing.z<=0
		||v->dimensions[0]<2u||v->dimensions[1]<2u||v->dimensions[2]<2u
		||v->encoding<RAL_IRRADIANCE_SH_L1_RGB16F||v->encoding>RAL_IRRADIANCE_SH_L1_RGB9E5
		||v->fallback<RAL_IRRADIANCE_FALLBACK_LIGHTGRID||v->fallback>RAL_IRRADIANCE_FALLBACK_GLOBAL_AMBIENT
		||v->ready!=qtrue)return qfalse;
	count=(uint64_t)v->dimensions[0]*v->dimensions[1]*v->dimensions[2];return count<=UINT32_MAX;
}
static qboolean AxisResolve(int32_t p,int32_t origin,int32_t spacing,uint32_t dim,uint32_t*cell,uint32_t*fraction){
	int64_t delta=(int64_t)p-origin,coord;if(delta<0)return qfalse;coord=(delta*RAL_LIGHT_Q16_ONE)/spacing;
	if(coord<0||coord>(int64_t)(dim-1u)*RAL_LIGHT_Q16_ONE)return qfalse;
	if(coord==(int64_t)(dim-1u)*RAL_LIGHT_Q16_ONE){*cell=dim-2u;*fraction=RAL_LIGHT_Q16_ONE;}
	else{*cell=(uint32_t)(coord/RAL_LIGHT_Q16_ONE);*fraction=(uint32_t)(coord%RAL_LIGHT_Q16_ONE);}return qtrue;
}
qboolean Ral_IrradianceProbeResolve(const ralIrradianceProbeVolume_t*v,const ralIrradianceProbeQuery_t*q,
		const uint8_t*validity,uint32_t validityCount,ralIrradianceProbeReceipt_t*out){
	ralIrradianceProbeReceipt_t r;uint32_t cell[3],f[3],corner,count=0u,total=0u,volumeCount;
	if(!out||!Ral_IrradianceProbeVolumeValid(v)||!q||q->schemaVersion!=RAL_IRRADIANCE_QUERY_SCHEMA_VERSION
		||!q->queryGeneration||q->productGeneration!=v->productGeneration)return qfalse;
	volumeCount=v->dimensions[0]*v->dimensions[1]*v->dimensions[2];
	if((validity&&validityCount<volumeCount)||(!validity&&validityCount))return qfalse;
	memset(&r,0,sizeof(r));r.schemaVersion=RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION;
	r.queryGeneration=q->queryGeneration;r.productGeneration=q->productGeneration;r.fallback=v->fallback;
	if(!AxisResolve(q->position.x,v->origin.x,v->spacing.x,v->dimensions[0],&cell[0],&f[0])
		||!AxisResolve(q->position.y,v->origin.y,v->spacing.y,v->dimensions[1],&cell[1],&f[1])
		||!AxisResolve(q->position.z,v->origin.z,v->spacing.z,v->dimensions[2],&cell[2],&f[2])){
		r.usedFallback=qtrue;r.ready=qtrue;*out=r;return qtrue;
	}
	for(corner=0u;corner<RAL_IRRADIANCE_MAX_CORNERS;corner++){
		uint32_t x=cell[0]+(corner&1u),y=cell[1]+((corner>>1u)&1u),z=cell[2]+((corner>>2u)&1u);
		uint32_t index=x+v->dimensions[0]*(y+v->dimensions[1]*z),w=RAL_LIGHT_Q16_ONE,axis;
		if(validity&&!validity[index])continue;
		for(axis=0u;axis<3u;axis++){uint32_t axisWeight=(corner&(1u<<axis))?f[axis]:RAL_LIGHT_Q16_ONE-f[axis];w=(uint32_t)(((uint64_t)w*axisWeight)/RAL_LIGHT_Q16_ONE);}
		if(!w)continue;r.probeIndices[count]=index;r.weightsQ16[count]=w;total+=w;count++;
	}
	if(!count||!total){r.usedFallback=qtrue;r.ready=qtrue;*out=r;return qtrue;}
	{
		uint32_t assigned=0u,i;for(i=0u;i<count;i++){uint32_t w=i+1u==count?RAL_LIGHT_Q16_ONE-assigned:(uint32_t)(((uint64_t)r.weightsQ16[i]*RAL_LIGHT_Q16_ONE)/total);r.weightsQ16[i]=w;assigned+=w;}
	}
	r.sampleCount=count;r.ready=qtrue;*out=r;return qtrue;
}
qboolean Ral_IrradianceProbeReceiptValid(const ralIrradianceProbeReceipt_t*r){
	uint32_t i,sum=0u;if(!r||r->schemaVersion!=RAL_IRRADIANCE_RECEIPT_SCHEMA_VERSION
		||!r->queryGeneration||!r->productGeneration||r->sampleCount>RAL_IRRADIANCE_MAX_CORNERS
		||r->fallback<RAL_IRRADIANCE_FALLBACK_LIGHTGRID||r->fallback>RAL_IRRADIANCE_FALLBACK_GLOBAL_AMBIENT
		||r->ready!=qtrue||((r->sampleCount==0u)!=r->usedFallback))return qfalse;
	for(i=0u;i<r->sampleCount;i++){uint32_t j;if(!r->weightsQ16[i])return qfalse;sum+=r->weightsQ16[i];for(j=0u;j<i;j++)if(r->probeIndices[i]==r->probeIndices[j])return qfalse;}
	for(i=r->sampleCount;i<RAL_IRRADIANCE_MAX_CORNERS;i++)if(r->probeIndices[i]||r->weightsQ16[i])return qfalse;
	return r->usedFallback?sum==0u:sum==RAL_LIGHT_Q16_ONE;
}
