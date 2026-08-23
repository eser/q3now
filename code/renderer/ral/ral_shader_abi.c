// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_shader_abi.h"
#include "ral_pipeline.h"

#include <limits.h>
#include <string.h>

typedef struct {
	uint64_t a;
	uint64_t b;
} ralShaderHashState_t;

static void ralShaderHashInit( ralShaderHashState_t *hash ) {
	hash->a = 0xcbf29ce484222325ull;
	hash->b = 0x84222325cbf29ce4ull;
}

static void ralShaderHashBytes( ralShaderHashState_t *hash, const void *data, size_t size ) {
	const unsigned char *bytes = (const unsigned char *)data;
	size_t i;
	for ( i = 0; i < size; ++i ) {
		hash->a ^= bytes[i]; hash->a *= 0x100000001b3ull;
		hash->b ^= (uint64_t)( bytes[i] + 0x9du ); hash->b *= 0x100000001b3ull;
		hash->b ^= hash->a >> 29;
	}
}

static void ralShaderHashU32( ralShaderHashState_t *hash, uint32_t value ) {
	unsigned char bytes[4];
	bytes[0] = (unsigned char)( value & 0xffu );
	bytes[1] = (unsigned char)( ( value >> 8u ) & 0xffu );
	bytes[2] = (unsigned char)( ( value >> 16u ) & 0xffu );
	bytes[3] = (unsigned char)( ( value >> 24u ) & 0xffu );
	ralShaderHashBytes( hash, bytes, sizeof( bytes ) );
}

static void ralShaderHashU64( ralShaderHashState_t *hash, uint64_t value ) {
	unsigned char bytes[8];
	uint32_t i;
	for ( i = 0; i < 8u; ++i ) bytes[i] = (unsigned char)( ( value >> ( i * 8u ) ) & 0xffu );
	ralShaderHashBytes( hash, bytes, sizeof( bytes ) );
}

static qboolean ralShaderStageSingle( uint32_t stage ) {
	return stage == RAL_STAGE_VERTEX || stage == RAL_STAGE_FRAGMENT
		|| stage == RAL_STAGE_COMPUTE;
}

static qboolean ralShaderStagesValid( uint32_t stages ) {
	return stages != 0 && ( stages & ~RAL_STAGE_ALL ) == 0;
}

static qboolean ralShaderEntryValid( const char *entry ) {
	size_t i;
	if ( !entry || !entry[0] ) return qfalse;
	for ( i = 0; i < 64u && entry[i]; ++i ) {
		const char c = entry[i];
		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' )
		     || ( c >= '0' && c <= '9' ) || c == '_' ) ) return qfalse;
	}
	return i > 0 && i < 64u;
}

qboolean Ral_ShaderDigestValid( const ralShaderDigest_t *digest ) {
	return digest && digest->lane0 != 0 && digest->lane1 != 0;
}

qboolean Ral_ShaderDigestExact( const ralShaderDigest_t *a,
	                           const ralShaderDigest_t *b ) {
	return Ral_ShaderDigestValid( a ) && Ral_ShaderDigestValid( b )
		&& a->lane0 == b->lane0 && a->lane1 == b->lane1;
}

ralResult_t Ral_ShaderArtifactDigest( const void *bytes, uint32_t byteCount,
	                                 ralShaderDigest_t *outDigest ) {
	ralShaderHashState_t hash;
	ralShaderDigest_t digest;
	if ( !bytes || byteCount == 0 || !outDigest ) return ralErrorInvalidArgument;
	ralShaderHashInit( &hash );
	ralShaderHashU32( &hash, byteCount );
	ralShaderHashBytes( &hash, bytes, byteCount );
	digest.lane0 = hash.a; digest.lane1 = hash.b;
	if ( !Ral_ShaderDigestValid( &digest ) ) return ralErrorInvalidArgument;
	*outDigest = digest;
	return ralSuccess;
}

static qboolean ralShaderArtifactValid( const ralShaderArtifactAbi_t *artifact,
	                                    ralShaderArtifactTarget_t target,
	                                    qboolean required ) {
	if ( !artifact || artifact->target != target ) return qfalse;
	if ( artifact->byteCount == 0 )
		return !required && artifact->digest.lane0 == 0 && artifact->digest.lane1 == 0;
	return Ral_ShaderDigestValid( &artifact->digest );
}

static qboolean ralShaderBindingClassValid( ralShaderBindingClass_t bindingClass ) {
	return bindingClass >= RAL_SHADER_BIND_UNIFORM_BUFFER
		&& bindingClass <= RAL_SHADER_BIND_COMPARISON_SAMPLER;
}

static qboolean ralShaderViewValid( ralShaderTextureViewDimension_t view ) {
	return view >= RAL_SHADER_VIEW_2D && view <= RAL_SHADER_VIEW_3D;
}

static qboolean ralShaderSampleValid( ralShaderTextureSampleType_t sample ) {
	return sample >= RAL_SHADER_SAMPLE_FLOAT && sample <= RAL_SHADER_SAMPLE_UINT;
}

static qboolean ralShaderBindingValid( const ralShaderBindingAbi_t *binding ) {
	const qboolean buffer = binding && binding->bindingClass >= RAL_SHADER_BIND_UNIFORM_BUFFER
		&& binding->bindingClass <= RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE;
	const qboolean sampled = binding && binding->bindingClass == RAL_SHADER_BIND_SAMPLED_TEXTURE;
	const qboolean storage = binding && ( binding->bindingClass == RAL_SHADER_BIND_STORAGE_TEXTURE_READ
		|| binding->bindingClass == RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE );
	const qboolean sampler = binding && ( binding->bindingClass == RAL_SHADER_BIND_FILTERING_SAMPLER
		|| binding->bindingClass == RAL_SHADER_BIND_COMPARISON_SAMPLER );
	if ( !binding || !ralShaderBindingClassValid( binding->bindingClass )
	  || binding->set >= RAL_SHADER_ABI_MAX_BIND_GROUPS
	  || binding->binding >= RAL_SHADER_ABI_MAX_BINDINGS_PER_GROUP
	  || binding->arrayCount == 0
	  || binding->arrayCount > RAL_SHADER_ABI_MAX_BINDING_ARRAY_COUNT
	  || !ralShaderStagesValid( binding->stageFlags )
	  || ( binding->dynamicOffset != qfalse && binding->dynamicOffset != qtrue ) ) return qfalse;
	if ( buffer ) return binding->minBufferBindingSize > 0
		&& binding->viewDimension == 0 && binding->sampleType == 0
		&& binding->storageTextureFormat == RAL_FORMAT_UNDEFINED;
	if ( sampled ) return binding->minBufferBindingSize == 0
		&& ralShaderViewValid( binding->viewDimension )
		&& ralShaderSampleValid( binding->sampleType )
		&& binding->storageTextureFormat == RAL_FORMAT_UNDEFINED
		&& binding->dynamicOffset == qfalse;
	if ( storage ) return binding->minBufferBindingSize == 0
		&& ralShaderViewValid( binding->viewDimension )
		&& binding->sampleType == 0
		&& binding->storageTextureFormat > RAL_FORMAT_UNDEFINED
		&& binding->storageTextureFormat < RAL_FORMAT_COUNT
		&& binding->dynamicOffset == qfalse;
	if ( sampler ) return binding->minBufferBindingSize == 0
		&& binding->viewDimension == 0 && binding->sampleType == 0
		&& binding->storageTextureFormat == RAL_FORMAT_UNDEFINED
		&& binding->dynamicOffset == qfalse;
	return qfalse;
}

static const ralShaderBindingAbi_t *ralShaderFindBinding(
	const ralShaderAbiManifest_t *manifest, uint32_t set, uint32_t binding ) {
	uint32_t i;
	for ( i = 0; i < manifest->bindingCount; ++i )
		if ( manifest->bindings[i].set == set && manifest->bindings[i].binding == binding )
			return &manifest->bindings[i];
	return NULL;
}

qboolean Ral_ShaderAbiManifestValid( const ralShaderAbiManifest_t *manifest ) {
	uint32_t i;
	if ( !manifest || manifest->schemaVersion != RAL_SHADER_ABI_SCHEMA_VERSION
	  || manifest->generation == 0 || manifest->generation == UINT64_MAX
	  || ( manifest->pipelineKind != RAL_SHADER_PIPELINE_GRAPHICS
	    && manifest->pipelineKind != RAL_SHADER_PIPELINE_COMPUTE )
	  || !manifest->modules || manifest->moduleCount == 0
	  || manifest->moduleCount > RAL_SHADER_ABI_MAX_MODULES
	  || manifest->bindingCount > RAL_SHADER_ABI_MAX_BINDINGS
	  || ( manifest->bindingCount && !manifest->bindings )
	  || manifest->vertexInputCount > RAL_SHADER_ABI_MAX_VERTEX_INPUTS
	  || ( manifest->vertexInputCount && !manifest->vertexInputs )
	  || manifest->specConstantCount > RAL_SHADER_ABI_MAX_SPEC_CONSTANTS
	  || ( manifest->specConstantCount && !manifest->specConstants ) ) return qfalse;
	if ( manifest->pipelineKind == RAL_SHADER_PIPELINE_GRAPHICS ) {
		if ( manifest->moduleCount != 2 || manifest->modules[0].stage != RAL_STAGE_VERTEX
		  || manifest->modules[1].stage != RAL_STAGE_FRAGMENT ) return qfalse;
	} else if ( manifest->moduleCount != 1 || manifest->modules[0].stage != RAL_STAGE_COMPUTE
	        || manifest->vertexInputCount != 0 ) return qfalse;
	for ( i = 0; i < manifest->moduleCount; ++i ) {
		const ralShaderModuleAbi_t *module = &manifest->modules[i];
		uint32_t target;
		if ( !ralShaderStageSingle( module->stage ) || !ralShaderEntryValid( module->entryPoint )
		  || !Ral_ShaderDigestValid( &module->sourceDigest ) ) return qfalse;
		for ( target = 0; target < RAL_SHADER_ARTIFACT_COUNT; ++target )
			if ( !ralShaderArtifactValid( &module->artifacts[target],
			                              (ralShaderArtifactTarget_t)target,
			                              target == RAL_SHADER_ARTIFACT_SPIRV ) ) return qfalse;
	}
	for ( i = 0; i < manifest->bindingCount; ++i ) {
		if ( !ralShaderBindingValid( &manifest->bindings[i] ) ) return qfalse;
		if ( i > 0 && ( manifest->bindings[i - 1].set > manifest->bindings[i].set
		  || ( manifest->bindings[i - 1].set == manifest->bindings[i].set
		    && manifest->bindings[i - 1].binding >= manifest->bindings[i].binding ) ) ) return qfalse;
	}
	for ( i = 0; i < manifest->vertexInputCount; ++i ) {
		if ( manifest->vertexInputs[i].format <= RAL_FORMAT_UNDEFINED
		  || manifest->vertexInputs[i].format >= RAL_FORMAT_COUNT
		  || ( i > 0 && manifest->vertexInputs[i - 1].location >= manifest->vertexInputs[i].location ) )
			return qfalse;
	}
	if ( manifest->inlineData.byteSize == 0 ) {
		if ( manifest->inlineData.stageFlags != 0 || manifest->inlineData.webgpuUniformSet != 0
		  || manifest->inlineData.webgpuUniformBinding != 0 ) return qfalse;
	} else {
		const ralShaderBindingAbi_t *fallback;
		if ( manifest->inlineData.byteSize > 256 || !ralShaderStagesValid( manifest->inlineData.stageFlags ) )
			return qfalse;
		fallback = ralShaderFindBinding( manifest, manifest->inlineData.webgpuUniformSet,
		                                 manifest->inlineData.webgpuUniformBinding );
		if ( !fallback || fallback->bindingClass != RAL_SHADER_BIND_UNIFORM_BUFFER
		  || fallback->minBufferBindingSize < manifest->inlineData.byteSize
		  || ( fallback->stageFlags & manifest->inlineData.stageFlags ) != manifest->inlineData.stageFlags )
			return qfalse;
	}
	for ( i = 0; i < manifest->specConstantCount; ++i ) {
		if ( !ralShaderStagesValid( manifest->specConstants[i].stageFlags )
		  || ( i > 0 && manifest->specConstants[i - 1].constantId >= manifest->specConstants[i].constantId ) )
			return qfalse;
	}
	return qtrue;
}

qboolean Ral_ShaderVariantAbiValid( const ralShaderAbiManifest_t *manifest,
	                                const ralShaderVariantAbi_t *variant ) {
	uint32_t i;
	if ( !Ral_ShaderAbiManifestValid( manifest ) || !variant
	  || variant->artifactTarget >= RAL_SHADER_ARTIFACT_COUNT
	  || variant->generation == 0 || variant->generation == UINT64_MAX
	  || !Ral_ShaderDigestValid( &variant->semanticStateDigest )
	  || variant->specValueCount != manifest->specConstantCount
	  || ( variant->specValueCount && !variant->specValues ) ) return qfalse;
	for ( i = 0; i < manifest->moduleCount; ++i )
		if ( !ralShaderArtifactValid( &manifest->modules[i].artifacts[variant->artifactTarget],
		                              variant->artifactTarget, qtrue ) ) return qfalse;
	for ( i = 0; i < variant->specValueCount; ++i )
		if ( variant->specValues[i].constantId != manifest->specConstants[i].constantId ) return qfalse;
	return qtrue;
}

static void ralShaderHashDigest( ralShaderHashState_t *hash, const ralShaderDigest_t *digest ) {
	ralShaderHashU64( hash, digest->lane0 ); ralShaderHashU64( hash, digest->lane1 );
}

static void ralShaderHashString( ralShaderHashState_t *hash, const char *text ) {
	const uint32_t length = (uint32_t)strlen( text );
	ralShaderHashU32( hash, length ); ralShaderHashBytes( hash, text, length );
}

ralResult_t Ral_ShaderAbiBuildPipelineKey( const ralShaderAbiManifest_t *manifest,
	                                      const ralShaderVariantAbi_t *variant,
	                                      ralShaderPipelineKey_t *outKey ) {
	ralShaderHashState_t hash;
	ralShaderPipelineKey_t key;
	uint32_t i, target;
	if ( !outKey || !Ral_ShaderVariantAbiValid( manifest, variant ) )
		return ralErrorInvalidArgument;
	ralShaderHashInit( &hash );
	ralShaderHashU32( &hash, manifest->schemaVersion );
	ralShaderHashU64( &hash, manifest->generation );
	ralShaderHashU32( &hash, (uint32_t)manifest->pipelineKind );
	ralShaderHashU32( &hash, manifest->moduleCount );
	for ( i = 0; i < manifest->moduleCount; ++i ) {
		const ralShaderModuleAbi_t *module = &manifest->modules[i];
		ralShaderHashU32( &hash, module->stage ); ralShaderHashString( &hash, module->entryPoint );
		ralShaderHashDigest( &hash, &module->sourceDigest );
		for ( target = 0; target < RAL_SHADER_ARTIFACT_COUNT; ++target ) {
			ralShaderHashU32( &hash, module->artifacts[target].target );
			ralShaderHashU32( &hash, module->artifacts[target].byteCount );
			if ( module->artifacts[target].byteCount ) ralShaderHashDigest( &hash, &module->artifacts[target].digest );
		}
	}
	ralShaderHashU32( &hash, manifest->bindingCount );
	for ( i = 0; i < manifest->bindingCount; ++i ) {
		const ralShaderBindingAbi_t *binding = &manifest->bindings[i];
		ralShaderHashU32( &hash, binding->set ); ralShaderHashU32( &hash, binding->binding );
		ralShaderHashU32( &hash, binding->bindingClass ); ralShaderHashU32( &hash, binding->arrayCount );
		ralShaderHashU32( &hash, binding->stageFlags ); ralShaderHashU64( &hash, binding->minBufferBindingSize );
		ralShaderHashU32( &hash, binding->viewDimension ); ralShaderHashU32( &hash, binding->sampleType );
		ralShaderHashU32( &hash, binding->storageTextureFormat ); ralShaderHashU32( &hash, binding->dynamicOffset );
	}
	ralShaderHashU32( &hash, manifest->vertexInputCount );
	for ( i = 0; i < manifest->vertexInputCount; ++i ) {
		ralShaderHashU32( &hash, manifest->vertexInputs[i].location );
		ralShaderHashU32( &hash, manifest->vertexInputs[i].format );
	}
	ralShaderHashU32( &hash, manifest->inlineData.byteSize );
	ralShaderHashU32( &hash, manifest->inlineData.stageFlags );
	ralShaderHashU32( &hash, manifest->inlineData.webgpuUniformSet );
	ralShaderHashU32( &hash, manifest->inlineData.webgpuUniformBinding );
	ralShaderHashU32( &hash, manifest->specConstantCount );
	for ( i = 0; i < manifest->specConstantCount; ++i ) {
		ralShaderHashU32( &hash, manifest->specConstants[i].constantId );
		ralShaderHashU32( &hash, manifest->specConstants[i].stageFlags );
		ralShaderHashU32( &hash, manifest->specConstants[i].defaultValue );
	}
	ralShaderHashU32( &hash, variant->artifactTarget );
	ralShaderHashU64( &hash, variant->generation );
	ralShaderHashDigest( &hash, &variant->semanticStateDigest );
	for ( i = 0; i < variant->specValueCount; ++i ) {
		ralShaderHashU32( &hash, variant->specValues[i].constantId );
		ralShaderHashU32( &hash, variant->specValues[i].value );
	}
	memset( &key, 0, sizeof( key ) );
	key.schemaVersion = manifest->schemaVersion;
	key.manifestGeneration = manifest->generation;
	key.variantGeneration = variant->generation;
	key.artifactTarget = variant->artifactTarget;
	key.digest.lane0 = hash.a; key.digest.lane1 = hash.b;
	key.ready = qtrue;
	if ( !Ral_ShaderDigestValid( &key.digest ) ) return ralErrorInvalidArgument;
	*outKey = key;
	return ralSuccess;
}

qboolean Ral_ShaderPipelineKeyExact( const ralShaderPipelineKey_t *a,
	                                const ralShaderPipelineKey_t *b ) {
	return a && b && a->schemaVersion == RAL_SHADER_ABI_SCHEMA_VERSION
		&& b->schemaVersion == RAL_SHADER_ABI_SCHEMA_VERSION
		&& a->manifestGeneration != 0 && a->manifestGeneration != UINT64_MAX
		&& b->manifestGeneration != 0 && b->manifestGeneration != UINT64_MAX
		&& a->variantGeneration != 0 && a->variantGeneration != UINT64_MAX
		&& b->variantGeneration != 0 && b->variantGeneration != UINT64_MAX
		&& a->artifactTarget < RAL_SHADER_ARTIFACT_COUNT
		&& b->artifactTarget < RAL_SHADER_ARTIFACT_COUNT
		&& a->ready == qtrue && b->ready == qtrue
		&& Ral_ShaderDigestExact( &a->digest, &b->digest )
		&& a->manifestGeneration == b->manifestGeneration
		&& a->variantGeneration == b->variantGeneration
		&& a->artifactTarget == b->artifactTarget;
}

static void ralShaderHashFloat( ralShaderHashState_t *hash, float value ) {
	uint32_t bits;
	memcpy( &bits, &value, sizeof( bits ) );
	ralShaderHashU32( hash, bits );
}

static void ralShaderHashStencil( ralShaderHashState_t *hash,
	                              const ralStencilOpState_t *state ) {
	ralShaderHashU32( hash, state->failOp ); ralShaderHashU32( hash, state->passOp );
	ralShaderHashU32( hash, state->depthFailOp ); ralShaderHashU32( hash, state->compareOp );
	ralShaderHashU32( hash, state->compareMask ); ralShaderHashU32( hash, state->writeMask );
	ralShaderHashU32( hash, state->reference );
}

ralResult_t Ral_GraphicsPipelineSemanticDigest(
	const ralGraphicsPipelineCreateInfo_t *ci, ralShaderDigest_t *outDigest ) {
	ralShaderHashState_t hash;
	ralShaderDigest_t digest;
	uint32_t i;
	if ( !ci || !outDigest
	  || ci->numVertexBindings > RAL_SHADER_ABI_MAX_VERTEX_INPUTS
	  || ci->numVertexAttributes > RAL_SHADER_ABI_MAX_VERTEX_INPUTS
	  || ci->numColorBlends > RAL_MAX_COLOR_ATTACHMENTS
	  || ci->numColorFormats > RAL_MAX_COLOR_ATTACHMENTS
	  || ci->numSpecConstants > RAL_SHADER_ABI_MAX_SPEC_CONSTANTS
	  || ( ci->numVertexBindings && !ci->vertexBindings )
	  || ( ci->numVertexAttributes && !ci->vertexAttributes )
	  || ( ci->numColorBlends && !ci->colorBlends )
	  || ( ci->numSpecConstants && !ci->specConstants ) ) return ralErrorInvalidArgument;
	ralShaderHashInit( &hash );
	ralShaderHashU32( &hash, RAL_SHADER_PIPELINE_GRAPHICS );
	ralShaderHashU32( &hash, ci->numVertexBindings );
	for ( i = 0; i < ci->numVertexBindings; ++i ) {
		ralShaderHashU32( &hash, ci->vertexBindings[i].binding );
		ralShaderHashU32( &hash, ci->vertexBindings[i].stride );
		ralShaderHashU32( &hash, ci->vertexBindings[i].inputRate );
	}
	ralShaderHashU32( &hash, ci->numVertexAttributes );
	for ( i = 0; i < ci->numVertexAttributes; ++i ) {
		ralShaderHashU32( &hash, ci->vertexAttributes[i].location );
		ralShaderHashU32( &hash, ci->vertexAttributes[i].binding );
		ralShaderHashU32( &hash, ci->vertexAttributes[i].format );
		ralShaderHashU32( &hash, ci->vertexAttributes[i].offset );
	}
	ralShaderHashU32( &hash, ci->topology );
	ralShaderHashU32( &hash, ci->raster.polygonMode ); ralShaderHashU32( &hash, ci->raster.cullMode );
	ralShaderHashU32( &hash, ci->raster.frontFace ); ralShaderHashU32( &hash, ci->raster.depthBiasEnable );
	ralShaderHashFloat( &hash, ci->raster.depthBiasConstant ); ralShaderHashFloat( &hash, ci->raster.depthBiasSlope );
	ralShaderHashFloat( &hash, ci->raster.depthBiasClamp ); ralShaderHashU32( &hash, ci->raster.depthClampEnable );
	ralShaderHashFloat( &hash, ci->raster.lineWidth );
	ralShaderHashU32( &hash, ci->depthStencil.depthTestEnable );
	ralShaderHashU32( &hash, ci->depthStencil.depthWriteEnable );
	ralShaderHashU32( &hash, ci->depthStencil.depthCompareOp );
	ralShaderHashU32( &hash, ci->depthStencil.stencilTestEnable );
	ralShaderHashStencil( &hash, &ci->depthStencil.stencilFront );
	ralShaderHashStencil( &hash, &ci->depthStencil.stencilBack );
	ralShaderHashU32( &hash, ci->numColorBlends );
	for ( i = 0; i < ci->numColorBlends; ++i ) {
		const ralColorBlendAttachment_t *blend = &ci->colorBlends[i];
		ralShaderHashU32( &hash, blend->blendEnable ); ralShaderHashU32( &hash, blend->srcColor );
		ralShaderHashU32( &hash, blend->dstColor ); ralShaderHashU32( &hash, blend->colorOp );
		ralShaderHashU32( &hash, blend->srcAlpha ); ralShaderHashU32( &hash, blend->dstAlpha );
		ralShaderHashU32( &hash, blend->alphaOp ); ralShaderHashU32( &hash, blend->writeMask );
		ralShaderHashU32( &hash, blend->writeMaskExplicit );
	}
	ralShaderHashU32( &hash, ci->numColorFormats );
	for ( i = 0; i < ci->numColorFormats; ++i ) ralShaderHashU32( &hash, ci->colorFormats[i] );
	ralShaderHashU32( &hash, ci->depthFormat ); ralShaderHashU32( &hash, ci->sampleCount );
	ralShaderHashU32( &hash, ci->numBindGroupLayouts );
	ralShaderHashU32( &hash, ci->optionalBindGroupMask );
	ralShaderHashU32( &hash, ci->pushConstantSize ); ralShaderHashU32( &hash, ci->pushConstantStages );
	ralShaderHashU32( &hash, ci->numSpecConstants );
	for ( i = 0; i < ci->numSpecConstants; ++i ) {
		ralShaderHashU32( &hash, ci->specConstants[i].constantId );
		ralShaderHashU32( &hash, ci->specConstants[i].value );
	}
	ralShaderHashU32( &hash, ci->shadingRate );
	digest.lane0 = hash.a; digest.lane1 = hash.b;
	if ( !Ral_ShaderDigestValid( &digest ) ) return ralErrorInvalidArgument;
	*outDigest = digest;
	return ralSuccess;
}

ralResult_t Ral_ComputePipelineSemanticDigest(
	const ralComputePipelineCreateInfo_t *ci, ralShaderDigest_t *outDigest ) {
	ralShaderHashState_t hash;
	ralShaderDigest_t digest;
	uint32_t i;
	if ( !ci || !outDigest || ci->numSpecConstants > RAL_SHADER_ABI_MAX_SPEC_CONSTANTS
	  || ( ci->numSpecConstants && !ci->specConstants ) ) return ralErrorInvalidArgument;
	ralShaderHashInit( &hash ); ralShaderHashU32( &hash, RAL_SHADER_PIPELINE_COMPUTE );
	ralShaderHashU32( &hash, ci->numBindGroupLayouts ); ralShaderHashU32( &hash, ci->pushConstantSize );
	ralShaderHashU32( &hash, ci->numSpecConstants );
	for ( i = 0; i < ci->numSpecConstants; ++i ) {
		ralShaderHashU32( &hash, ci->specConstants[i].constantId );
		ralShaderHashU32( &hash, ci->specConstants[i].value );
	}
	digest.lane0 = hash.a; digest.lane1 = hash.b;
	if ( !Ral_ShaderDigestValid( &digest ) ) return ralErrorInvalidArgument;
	*outDigest = digest;
	return ralSuccess;
}

static qboolean ralShaderBindingsMatchLayoutCount( const ralShaderAbiManifest_t *manifest,
	                                               uint32_t layoutCount ) {
	const uint32_t required = manifest->bindingCount
		? manifest->bindings[manifest->bindingCount - 1u].set + 1u : 0u;
	return layoutCount == required;
}

static qboolean ralShaderSpecValuesMatch( const ralShaderAbiManifest_t *manifest,
	                                     const ralShaderVariantAbi_t *variant,
	                                     const ralSpecConstant_t *values,
	                                     uint32_t valueCount ) {
	uint32_t i;
	if ( valueCount != manifest->specConstantCount || valueCount != variant->specValueCount
	  || ( valueCount && !values ) ) return qfalse;
	for ( i = 0; i < valueCount; ++i )
		if ( values[i].constantId != manifest->specConstants[i].constantId
		  || values[i].constantId != variant->specValues[i].constantId
		  || values[i].value != variant->specValues[i].value ) return qfalse;
	return qtrue;
}

static qboolean ralShaderArtifactMatchesBytes( const ralShaderArtifactAbi_t *artifact,
	                                          const uint32_t *words, uint32_t byteCount ) {
	ralShaderDigest_t digest;
	if ( !artifact || artifact->target != RAL_SHADER_ARTIFACT_SPIRV
	  || artifact->byteCount != byteCount || !words || byteCount < sizeof( uint32_t )
	  || ( byteCount & 3u ) != 0 || words[0] != 0x07230203u
	  || Ral_ShaderArtifactDigest( words, byteCount, &digest ) != ralSuccess ) return qfalse;
	return Ral_ShaderDigestExact( &artifact->digest, &digest );
}

qboolean Ral_ShaderAbiMatchesGraphicsPipeline(
	const ralShaderAbiManifest_t *manifest, const ralShaderVariantAbi_t *variant,
	const ralGraphicsPipelineCreateInfo_t *ci, ralShaderPipelineKey_t *outKey ) {
	ralShaderDigest_t state;
	uint32_t i;
	if ( !ci || !outKey || !Ral_ShaderVariantAbiValid( manifest, variant )
	  || manifest->pipelineKind != RAL_SHADER_PIPELINE_GRAPHICS
	  || variant->artifactTarget != RAL_SHADER_ARTIFACT_SPIRV
	  || strcmp( ci->vertexEntry && ci->vertexEntry[0] ? ci->vertexEntry : "main",
	             manifest->modules[0].entryPoint ) != 0
	  || strcmp( ci->fragmentEntry && ci->fragmentEntry[0] ? ci->fragmentEntry : "main",
	             manifest->modules[1].entryPoint ) != 0
	  || !ralShaderArtifactMatchesBytes( &manifest->modules[0].artifacts[RAL_SHADER_ARTIFACT_SPIRV],
	                                    ci->vertexSpirv, ci->vertexSpirvSize )
	  || !ralShaderArtifactMatchesBytes( &manifest->modules[1].artifacts[RAL_SHADER_ARTIFACT_SPIRV],
	                                    ci->fragmentSpirv, ci->fragmentSpirvSize )
	  || ci->numVertexAttributes != manifest->vertexInputCount
	  || !ralShaderBindingsMatchLayoutCount( manifest, ci->numBindGroupLayouts )
	  || ci->pushConstantSize != manifest->inlineData.byteSize
	  || ci->pushConstantStages != manifest->inlineData.stageFlags
	  || !ralShaderSpecValuesMatch( manifest, variant, ci->specConstants, ci->numSpecConstants ) )
		return qfalse;
	for ( i = 0; i < ci->numVertexAttributes; ++i )
		if ( !ci->vertexAttributes
		  || ci->vertexAttributes[i].location != manifest->vertexInputs[i].location
		  || ci->vertexAttributes[i].format != manifest->vertexInputs[i].format ) return qfalse;
	if ( Ral_GraphicsPipelineSemanticDigest( ci, &state ) != ralSuccess
	  || !Ral_ShaderDigestExact( &state, &variant->semanticStateDigest ) ) return qfalse;
	return Ral_ShaderAbiBuildPipelineKey( manifest, variant, outKey ) == ralSuccess;
}

qboolean Ral_ShaderAbiMatchesComputePipeline(
	const ralShaderAbiManifest_t *manifest, const ralShaderVariantAbi_t *variant,
	const ralComputePipelineCreateInfo_t *ci, ralShaderPipelineKey_t *outKey ) {
	ralShaderDigest_t state;
	if ( !ci || !outKey || !Ral_ShaderVariantAbiValid( manifest, variant )
	  || manifest->pipelineKind != RAL_SHADER_PIPELINE_COMPUTE
	  || variant->artifactTarget != RAL_SHADER_ARTIFACT_SPIRV
	  || strcmp( ci->computeEntry && ci->computeEntry[0] ? ci->computeEntry : "main",
	             manifest->modules[0].entryPoint ) != 0
	  || !ralShaderArtifactMatchesBytes( &manifest->modules[0].artifacts[RAL_SHADER_ARTIFACT_SPIRV],
	                                    ci->computeSpirv, ci->computeSpirvSize )
	  || !ralShaderBindingsMatchLayoutCount( manifest, ci->numBindGroupLayouts )
	  || ci->pushConstantSize != manifest->inlineData.byteSize
	  || ( ci->pushConstantSize && manifest->inlineData.stageFlags != RAL_STAGE_COMPUTE )
	  || !ralShaderSpecValuesMatch( manifest, variant, ci->specConstants, ci->numSpecConstants )
	  || Ral_ComputePipelineSemanticDigest( ci, &state ) != ralSuccess
	  || !Ral_ShaderDigestExact( &state, &variant->semanticStateDigest ) ) return qfalse;
	return Ral_ShaderAbiBuildPipelineKey( manifest, variant, outKey ) == ralSuccess;
}
