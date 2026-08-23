// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_material.h"

#include <math.h>
#include <string.h>

#define FNV64_OFFSET UINT64_C(14695981039346656037)
#define FNV64_PRIME UINT64_C(1099511628211)

static uint64_t Byte( uint64_t h, unsigned char v ) { return ( h ^ v ) * FNV64_PRIME; }
static uint64_t U32( uint64_t h, uint32_t v ) { uint32_t i; for ( i=0u;i<4u;i++ ) h=Byte(h,(unsigned char)(v>>(i*8u))); return h; }
static uint64_t U64( uint64_t h, uint64_t v ) { uint32_t i; for ( i=0u;i<8u;i++ ) h=Byte(h,(unsigned char)(v>>(i*8u))); return h; }
static uint32_t FloatBits( float v ) { uint32_t bits=0u; if ( v != 0.0f ) memcpy(&bits,&v,sizeof(bits)); return bits; }
static qboolean Bool( qboolean v ) { return v == qfalse || v == qtrue; }

static qboolean RoleValid( const ralMaterialTexture_t *t ) {
	const ralTextureAssetReceipt_t *a;
	if ( !t || t->role < RAL_MATERIAL_TEXTURE_BASE_COLOR
			|| t->role > RAL_MATERIAL_TEXTURE_LEGACY_STAGE1 || t->uvSet > 1u
			|| !Ral_TextureCacheKeyValid( &t->texture ) ) return qfalse;
	a = &t->texture.asset;
	switch ( t->role ) {
	case RAL_MATERIAL_TEXTURE_BASE_COLOR:
		return t->channels == RAL_MATERIAL_CHANNEL_RGBA
			&& a->colorEncoding == RAL_TEXTURE_ENCODING_SRGB
			&& a->channelSemantic == RAL_TEXTURE_CHANNEL_COLOR;
	case RAL_MATERIAL_TEXTURE_NORMAL:
		return t->channels == RAL_MATERIAL_CHANNEL_XY_NORMAL
			&& a->colorEncoding == RAL_TEXTURE_ENCODING_LINEAR
			&& a->channelSemantic == RAL_TEXTURE_CHANNEL_NORMAL_XY;
	case RAL_MATERIAL_TEXTURE_ORM:
		return t->channels == RAL_MATERIAL_CHANNEL_RGB_ORM
			&& a->colorEncoding == RAL_TEXTURE_ENCODING_LINEAR
			&& a->channelSemantic == RAL_TEXTURE_CHANNEL_ORM;
	case RAL_MATERIAL_TEXTURE_OCCLUSION:
		return t->channels == RAL_MATERIAL_CHANNEL_R_OCCLUSION
			&& a->colorEncoding == RAL_TEXTURE_ENCODING_LINEAR
			&& a->channelSemantic == RAL_TEXTURE_CHANNEL_DATA;
	case RAL_MATERIAL_TEXTURE_METALLIC_ROUGHNESS:
		return t->channels == RAL_MATERIAL_CHANNEL_GB_ROUGHNESS_METALLIC
			&& a->colorEncoding == RAL_TEXTURE_ENCODING_LINEAR
			&& a->channelSemantic == RAL_TEXTURE_CHANNEL_ORM;
	case RAL_MATERIAL_TEXTURE_EMISSIVE:
		return t->channels == RAL_MATERIAL_CHANNEL_RGB_EMISSIVE
			&& a->colorEncoding == RAL_TEXTURE_ENCODING_SRGB
			&& a->channelSemantic == RAL_TEXTURE_CHANNEL_COLOR;
	case RAL_MATERIAL_TEXTURE_LIGHTMAP:
	case RAL_MATERIAL_TEXTURE_LEGACY_STAGE0:
	case RAL_MATERIAL_TEXTURE_LEGACY_STAGE1:
		return ( t->channels == RAL_MATERIAL_CHANNEL_RGBA
				|| t->channels == RAL_MATERIAL_CHANNEL_LEGACY_RGBA )
			&& a->channelSemantic == RAL_TEXTURE_CHANNEL_COLOR;
	case RAL_MATERIAL_TEXTURE_ENVIRONMENT:
		return t->channels == RAL_MATERIAL_CHANNEL_RGBA
			&& a->colorEncoding == RAL_TEXTURE_ENCODING_LINEAR
			&& a->channelSemantic == RAL_TEXTURE_CHANNEL_HDR_COLOR;
	default: return qfalse;
	}
}

static qboolean Normalize( const ralMaterialDescription_t *d,
		ralMaterialDescription_t *out ) {
	uint32_t i; qboolean orm=qfalse, split=qfalse;
	if ( !d || !out || d->schemaVersion != RAL_MATERIAL_SCHEMA_VERSION
			|| !d->materialGeneration || !d->provenanceHash
			|| d->source < RAL_MATERIAL_SOURCE_Q3 || d->source > RAL_MATERIAL_SOURCE_GLTF
			|| d->textureCount > RAL_MATERIAL_MAX_TEXTURES
			|| d->renderPolicy.alphaMode > RAL_MATERIAL_ALPHA_ADDITIVE
			|| !Bool(d->renderPolicy.doubleSided) || !Bool(d->renderPolicy.depthTest)
			|| !Bool(d->renderPolicy.depthWrite) ) return qfalse;
	for ( i=0u;i<4u;i++ ) if ( !isfinite(d->factors.baseColor[i])
			|| d->factors.baseColor[i] < 0.0f || d->factors.baseColor[i] > 1.0f ) return qfalse;
	for ( i=0u;i<3u;i++ ) if ( !isfinite(d->factors.emissive[i]) || d->factors.emissive[i] < 0.0f ) return qfalse;
	if ( !isfinite(d->factors.metallic) || d->factors.metallic < 0.0f || d->factors.metallic > 1.0f
			|| !isfinite(d->factors.roughness) || d->factors.roughness < 0.0f || d->factors.roughness > 1.0f
			|| !isfinite(d->factors.normalScale) || d->factors.normalScale < 0.0f
			|| !isfinite(d->factors.occlusionStrength) || d->factors.occlusionStrength < 0.0f
			|| d->factors.occlusionStrength > 1.0f || !isfinite(d->renderPolicy.alphaCutoff)
			|| ( d->renderPolicy.alphaMode == RAL_MATERIAL_ALPHA_MASK
				? ( d->renderPolicy.alphaCutoff <= 0.0f || d->renderPolicy.alphaCutoff > 1.0f )
				: d->renderPolicy.alphaCutoff != 0.0f ) ) return qfalse;
	memset( out, 0, sizeof( *out ) ); out->schemaVersion=d->schemaVersion;
	out->materialGeneration=d->materialGeneration; out->provenanceHash=d->provenanceHash;
	out->source=d->source; out->textureCount=d->textureCount;
	for ( i=0u;i<d->textureCount;i++ ) {
		if ( !RoleValid(&d->textures[i]) || (i && d->textures[i-1u].role >= d->textures[i].role) ) return qfalse;
		out->textures[i]=d->textures[i];
		if ( d->textures[i].role == RAL_MATERIAL_TEXTURE_ORM ) orm=qtrue;
		if ( d->textures[i].role == RAL_MATERIAL_TEXTURE_OCCLUSION
				|| d->textures[i].role == RAL_MATERIAL_TEXTURE_METALLIC_ROUGHNESS ) split=qtrue;
	}
	if ( orm && split ) return qfalse;
	out->factors=d->factors; out->renderPolicy=d->renderPolicy;
	for(i=0u;i<4u;i++) if(out->factors.baseColor[i]==0.0f) out->factors.baseColor[i]=0.0f;
	for(i=0u;i<3u;i++) if(out->factors.emissive[i]==0.0f) out->factors.emissive[i]=0.0f;
	if(out->factors.metallic==0.0f)out->factors.metallic=0.0f;
	if(out->factors.roughness==0.0f)out->factors.roughness=0.0f;
	if(out->factors.normalScale==0.0f)out->factors.normalScale=0.0f;
	if(out->factors.occlusionStrength==0.0f)out->factors.occlusionStrength=0.0f;
	return qtrue;
}

static uint64_t Hash( const ralMaterialDescription_t *d, qboolean artifact ) {
	uint64_t h=FNV64_OFFSET; uint32_t i,j;
	h=U32(h,RAL_MATERIAL_RECEIPT_SCHEMA_VERSION); h=U32(h,(uint32_t)d->source);
	if(artifact){h=U64(h,d->materialGeneration);h=U64(h,d->provenanceHash);}
	h=U32(h,d->textureCount);
	for(i=0u;i<d->textureCount;i++){
		h=U32(h,(uint32_t)d->textures[i].role);h=U32(h,(uint32_t)d->textures[i].channels);
		h=U32(h,d->textures[i].uvSet);h=U32(h,(uint32_t)d->textures[i].texture.asset.targetFormat);
		if(artifact)h=U64(h,d->textures[i].texture.keyHash);
	}
	for(i=0u;i<4u;i++)h=U32(h,FloatBits(d->factors.baseColor[i]));
	for(i=0u;i<3u;i++)h=U32(h,FloatBits(d->factors.emissive[i]));
	(void)j;
	h=U32(h,FloatBits(d->factors.metallic));h=U32(h,FloatBits(d->factors.roughness));
	h=U32(h,FloatBits(d->factors.normalScale));h=U32(h,FloatBits(d->factors.occlusionStrength));
	h=U32(h,(uint32_t)d->renderPolicy.alphaMode);h=U32(h,FloatBits(d->renderPolicy.alphaCutoff));
	h=U32(h,(uint32_t)d->renderPolicy.doubleSided);h=U32(h,(uint32_t)d->renderPolicy.depthTest);
	h=U32(h,(uint32_t)d->renderPolicy.depthWrite);return h?h:1u;
}

qboolean Ral_MaterialCompile( const ralMaterialDescription_t *d,
		ralMaterialReceipt_t *out ) {
	ralMaterialReceipt_t v;
	if(!out)return qfalse;
	memset(&v,0,sizeof(v));
	if(!Normalize(d,&v.material))return qfalse;
	v.schemaVersion=RAL_MATERIAL_RECEIPT_SCHEMA_VERSION;
	v.variantKey=Hash(&v.material,qfalse);v.artifactHash=Hash(&v.material,qtrue);v.ready=qtrue;
	*out=v;return qtrue;
}

qboolean Ral_MaterialReceiptValid( const ralMaterialReceipt_t *r ) {
	ralMaterialReceipt_t v;
	return r&&r->schemaVersion==RAL_MATERIAL_RECEIPT_SCHEMA_VERSION&&r->ready==qtrue
		&&Ral_MaterialCompile(&r->material,&v)&&r->variantKey==v.variantKey
		&&r->artifactHash==v.artifactHash;
}

qboolean Ral_MaterialReceiptExact( const ralMaterialReceipt_t *a,
		const ralMaterialReceipt_t *b ) {
	return Ral_MaterialReceiptValid(a)&&Ral_MaterialReceiptValid(b)
		&&!memcmp(a,b,sizeof(*a));
}
