// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_pipeline.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ralWebGpuPipeline_s {
	ralWebGpuCore_t *core;
	ralWebGpuCoreReceipt_t coreReceipt;
	void *userData;
	ralWebGpuPipelineHostOps_t host;
	ralWebGpuPipelineReceipt_t receipt;
	qboolean live;
};

static qboolean Contains( const char *bytes, uint32_t byteCount,
		const char *needle ) {
	size_t needleSize;
	uint32_t i;
	if ( !bytes || !needle ) return qfalse;
	needleSize = strlen( needle );
	if ( !needleSize || needleSize > byteCount ) return qfalse;
	for ( i = 0u; i <= byteCount - needleSize; ++i )
		if ( !memcmp( bytes + i, needle, needleSize ) ) return qtrue;
	return qfalse;
}

static qboolean ModuleHasBinding( const ralWebGpuWgslModule_t *module,
		uint32_t set, uint32_t binding ) {
	char declaration[64];
	int length = snprintf( declaration, sizeof( declaration ),
		"@group(%u) @binding(%u)", set, binding );
	return length > 0 && (size_t)length < sizeof( declaration )
		&& Contains( module->code, module->byteCount, declaration );
}

static qboolean ModuleValid( const ralShaderModuleAbi_t *abi,
		const ralWebGpuWgslModule_t *module, ralShaderDigest_t *outDigest ) {
	char entry[96];
	int entryLength;
	const ralShaderArtifactAbi_t *artifact;
	ralShaderDigest_t digest;
	if ( !abi || !module || !module->code || !module->byteCount
			|| Contains( module->code, module->byteCount, "var<immediate>" )
			|| Contains( module->code, module->byteCount, "OpCapability" ) )
		return qfalse;
	entryLength = snprintf( entry, sizeof( entry ), "fn %s(", abi->entryPoint );
	if ( entryLength <= 0 || (size_t)entryLength >= sizeof( entry )
			|| !Contains( module->code, module->byteCount, entry ) ) return qfalse;
	artifact = &abi->artifacts[RAL_SHADER_ARTIFACT_WGSL];
	if ( artifact->target != RAL_SHADER_ARTIFACT_WGSL
			|| artifact->byteCount != module->byteCount
			|| Ral_ShaderArtifactDigest( module->code, module->byteCount,
				&digest ) != ralSuccess
			|| !Ral_ShaderDigestExact( &digest, &artifact->digest ) ) return qfalse;
	*outDigest = digest;
	return qtrue;
}

static qboolean BindingDeclarationsValid(
		const ralShaderAbiManifest_t *manifest,
		const ralWebGpuWgslModule_t *modules ) {
	uint32_t i, moduleIndex;
	for ( i = 0u; i < manifest->bindingCount; ++i ) {
		const ralShaderBindingAbi_t *binding = &manifest->bindings[i];
		qboolean found = qfalse;
		for ( moduleIndex = 0u; moduleIndex < manifest->moduleCount;
				++moduleIndex ) {
			if ( !( binding->stageFlags & manifest->modules[moduleIndex].stage ) )
				continue;
			found = qtrue;
			if ( !ModuleHasBinding( &modules[moduleIndex], binding->set,
					binding->binding ) ) return qfalse;
		}
		if ( !found ) return qfalse;
	}
	return qtrue;
}

static qboolean SpecValuesExact( const ralShaderAbiManifest_t *manifest,
		const ralShaderVariantAbi_t *variant,
		const ralSpecConstant_t *values, uint32_t valueCount ) {
	uint32_t i;
	if ( valueCount != variant->specValueCount
			|| ( valueCount && !values ) ) return qfalse;
	for ( i = 0u; i < valueCount; ++i )
		if ( values[i].constantId != variant->specValues[i].constantId
				|| values[i].value != variant->specValues[i].value
				|| manifest->specConstants[i].constantId
					!= variant->specValues[i].constantId ) return qfalse;
	return qtrue;
}

static uint32_t RequiredGroupCount( const ralShaderAbiManifest_t *manifest ) {
	return manifest->bindingCount
		? manifest->bindings[manifest->bindingCount - 1u].set + 1u : 0u;
}

static qboolean StateValid( const ralShaderAbiManifest_t *manifest,
		const ralShaderVariantAbi_t *variant,
		const ralWebGpuPipelineCreateInfo_t *createInfo ) {
	ralShaderDigest_t semantic;
	uint32_t i, groups = RequiredGroupCount( manifest );
	if ( manifest->pipelineKind == RAL_SHADER_PIPELINE_GRAPHICS ) {
		const ralGraphicsPipelineCreateInfo_t *state = createInfo->graphicsState;
		if ( !state || createInfo->computeState
				|| state->raster.polygonMode != RAL_POLYGON_FILL ) return qfalse;
		if ( Ral_GraphicsPipelineSemanticDigest( state, &semantic ) != ralSuccess
				|| !Ral_ShaderDigestExact( &semantic,
					&variant->semanticStateDigest )
				|| state->numBindGroupLayouts != groups
				|| state->pushConstantSize != manifest->inlineData.byteSize
				|| state->pushConstantStages != manifest->inlineData.stageFlags
				|| !SpecValuesExact( manifest, variant, state->specConstants,
					state->numSpecConstants )
				|| state->numVertexAttributes != manifest->vertexInputCount )
			return qfalse;
		for ( i = 0u; i < state->numVertexAttributes; ++i )
			if ( state->vertexAttributes[i].location
					!= manifest->vertexInputs[i].location
					|| state->vertexAttributes[i].format
						!= manifest->vertexInputs[i].format ) return qfalse;
		return qtrue;
	}
	if ( manifest->pipelineKind == RAL_SHADER_PIPELINE_COMPUTE ) {
		const ralComputePipelineCreateInfo_t *state = createInfo->computeState;
		if ( !state || createInfo->graphicsState ) return qfalse;
		return Ral_ComputePipelineSemanticDigest( state, &semantic ) == ralSuccess
			&& Ral_ShaderDigestExact( &semantic, &variant->semanticStateDigest )
			&& state->numBindGroupLayouts == groups
			&& state->pushConstantSize == manifest->inlineData.byteSize
			&& SpecValuesExact( manifest, variant, state->specConstants,
				state->numSpecConstants );
	}
	return qfalse;
}

static qboolean ReceiptValid( const ralWebGpuPipelineReceipt_t *receipt ) {
	uint32_t i;
	if ( !receipt || receipt->schemaVersion != RAL_WEBGPU_PIPELINE_SCHEMA_VERSION
			|| receipt->backendType != RAL_BACKEND_WEBGPU
			|| receipt->kind < RAL_SHADER_PIPELINE_GRAPHICS
			|| receipt->kind > RAL_SHADER_PIPELINE_COMPUTE
			|| !receipt->backendGeneration
			|| receipt->backendGeneration == UINT64_MAX
			|| !receipt->generation || receipt->generation == UINT64_MAX
			|| !receipt->pipelineIdentity || !receipt->pipelineLayoutIdentity
			|| receipt->shaderModuleCount == 0u
			|| receipt->shaderModuleCount > RAL_SHADER_ABI_MAX_MODULES
			|| receipt->bindGroupLayoutCount > RAL_SHADER_ABI_MAX_BIND_GROUPS
			|| receipt->bindingCount > RAL_SHADER_ABI_MAX_BINDINGS
			|| !Ral_ShaderPipelineKeyExact( &receipt->pipelineKey,
				&receipt->pipelineKey ) || receipt->pipelineKey.artifactTarget
				!= RAL_SHADER_ARTIFACT_WGSL || receipt->ready != qtrue ) return qfalse;
	for ( i = 0u; i < receipt->shaderModuleCount; ++i )
		if ( !receipt->shaderModuleIdentities[i]
				|| !Ral_ShaderDigestValid( &receipt->shaderModuleDigests[i] ) )
			return qfalse;
	for ( i = 0u; i < receipt->bindGroupLayoutCount; ++i )
		if ( !receipt->bindGroupLayoutIdentities[i] ) return qfalse;
	return qtrue;
}

static void DestroyCandidate( const ralWebGpuPipelineCreateInfo_t *createInfo,
		ralWebGpuPipelineReceipt_t *receipt ) {
	uint32_t i;
	if ( receipt->pipelineIdentity ) createInfo->host.destroyObject(
		createInfo->userData, RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE,
		receipt->pipelineIdentity );
	if ( receipt->pipelineLayoutIdentity ) createInfo->host.destroyObject(
		createInfo->userData, RAL_WEBGPU_PIPELINE_OBJECT_PIPELINE_LAYOUT,
		receipt->pipelineLayoutIdentity );
	for ( i = receipt->bindGroupLayoutCount; i > 0u; --i )
		if ( receipt->bindGroupLayoutIdentities[i - 1u] )
			createInfo->host.destroyObject( createInfo->userData,
				RAL_WEBGPU_PIPELINE_OBJECT_BIND_GROUP_LAYOUT,
				receipt->bindGroupLayoutIdentities[i - 1u] );
	for ( i = receipt->shaderModuleCount; i > 0u; --i )
		if ( receipt->shaderModuleIdentities[i - 1u] )
			createInfo->host.destroyObject( createInfo->userData,
				RAL_WEBGPU_PIPELINE_OBJECT_SHADER_MODULE,
				receipt->shaderModuleIdentities[i - 1u] );
}

qboolean RalWebGpu_PipelineCreate( ralWebGpuCore_t *core,
		const ralWebGpuCoreReceipt_t *coreReceipt,
		const ralWebGpuPipelineCreateInfo_t *createInfo,
		ralWebGpuPipeline_t **outPipeline,
		ralWebGpuPipelineReceipt_t *outReceipt ) {
	ralWebGpuPipelineReceipt_t receipt;
	ralWebGpuPipeline_t *pipeline = NULL;
	ralWebGpuBindGroupLayoutEntry_t entries[RAL_SHADER_ABI_MAX_BINDINGS_PER_GROUP];
	ralWebGpuNativePipelineDesc_t native;
	uint32_t i, group, groupCount, entryCount;
	if ( !core || !coreReceipt || !createInfo || !outPipeline || !outReceipt
			|| !RalWebGpu_CoreMatchesReceipt( core, coreReceipt )
			|| !createInfo->manifest || !createInfo->variant
			|| !createInfo->modules || !createInfo->generation
			|| createInfo->generation == UINT64_MAX
			|| !createInfo->host.createShaderModule
			|| !createInfo->host.createBindGroupLayout
			|| !createInfo->host.createPipelineLayout
			|| !createInfo->host.createPipeline
			|| !createInfo->host.destroyObject
			|| !Ral_ShaderAbiManifestValid( createInfo->manifest )
			|| !Ral_ShaderVariantAbiValid( createInfo->manifest,
				createInfo->variant )
			|| createInfo->variant->artifactTarget != RAL_SHADER_ARTIFACT_WGSL
			|| createInfo->moduleCount != createInfo->manifest->moduleCount
			|| !StateValid( createInfo->manifest, createInfo->variant, createInfo )
			|| !BindingDeclarationsValid( createInfo->manifest,
				createInfo->modules ) ) return qfalse;
	groupCount = RequiredGroupCount( createInfo->manifest );
	if ( groupCount > coreReceipt->caps.maxBindGroups ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_WEBGPU_PIPELINE_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_WEBGPU;
	receipt.kind = createInfo->manifest->pipelineKind;
	receipt.backendGeneration = coreReceipt->generation;
	receipt.generation = createInfo->generation;
	receipt.shaderModuleCount = createInfo->moduleCount;
	receipt.bindGroupLayoutCount = groupCount;
	receipt.bindingCount = createInfo->manifest->bindingCount;
	if ( Ral_ShaderAbiBuildPipelineKey( createInfo->manifest,
			createInfo->variant, &receipt.pipelineKey ) != ralSuccess ) return qfalse;
	for ( i = 0u; i < receipt.shaderModuleCount; ++i ) {
		ralWebGpuShaderModuleDesc_t desc;
		if ( !ModuleValid( &createInfo->manifest->modules[i],
				&createInfo->modules[i], &receipt.shaderModuleDigests[i] ) ) goto fail;
		memset( &desc, 0, sizeof( desc ) );
		desc.stage = createInfo->manifest->modules[i].stage;
		desc.entryPoint = createInfo->manifest->modules[i].entryPoint;
		desc.code = createInfo->modules[i].code;
		desc.byteCount = createInfo->modules[i].byteCount;
		desc.digest = receipt.shaderModuleDigests[i];
		if ( !createInfo->host.createShaderModule( createInfo->userData,
				coreReceipt->deviceIdentity, &desc,
				&receipt.shaderModuleIdentities[i] )
				|| !receipt.shaderModuleIdentities[i] ) goto fail;
	}
	for ( group = 0u; group < groupCount; ++group ) {
		entryCount = 0u;
		for ( i = 0u; i < createInfo->manifest->bindingCount; ++i ) {
			const ralShaderBindingAbi_t *source = &createInfo->manifest->bindings[i];
			if ( source->set != group ) continue;
			entries[entryCount].binding = source->binding;
			entries[entryCount].bindingClass = source->bindingClass;
			entries[entryCount].arrayCount = source->arrayCount;
			entries[entryCount].stageFlags = source->stageFlags;
			entries[entryCount].minBufferBindingSize = source->minBufferBindingSize;
			entries[entryCount].viewDimension = source->viewDimension;
			entries[entryCount].sampleType = source->sampleType;
			entries[entryCount].storageTextureFormat = source->storageTextureFormat;
			entries[entryCount].dynamicOffset = source->dynamicOffset;
			entryCount++;
		}
		if ( !createInfo->host.createBindGroupLayout( createInfo->userData,
				coreReceipt->deviceIdentity, group, entries, entryCount,
				&receipt.bindGroupLayoutIdentities[group] )
				|| !receipt.bindGroupLayoutIdentities[group] ) goto fail;
	}
	if ( !createInfo->host.createPipelineLayout( createInfo->userData,
			coreReceipt->deviceIdentity, receipt.bindGroupLayoutIdentities,
			groupCount, &receipt.pipelineLayoutIdentity )
			|| !receipt.pipelineLayoutIdentity ) goto fail;
	memset( &native, 0, sizeof( native ) );
	native.kind = createInfo->manifest->pipelineKind;
	memcpy( native.shaderModules, receipt.shaderModuleIdentities,
		sizeof( native.shaderModules ) );
	native.shaderModuleCount = receipt.shaderModuleCount;
	memcpy( native.bindGroupLayouts, receipt.bindGroupLayoutIdentities,
		sizeof( native.bindGroupLayouts ) );
	native.bindGroupLayoutCount = receipt.bindGroupLayoutCount;
	native.pipelineLayout = receipt.pipelineLayoutIdentity;
	native.graphicsState = createInfo->graphicsState;
	native.computeState = createInfo->computeState;
	native.specValues = createInfo->variant->specValues;
	native.specValueCount = createInfo->variant->specValueCount;
	if ( !createInfo->host.createPipeline( createInfo->userData,
			coreReceipt->deviceIdentity, &native, &receipt.pipelineIdentity )
			|| !receipt.pipelineIdentity ) goto fail;
	receipt.ready = qtrue;
	if ( !ReceiptValid( &receipt ) ) goto fail;
	pipeline = (ralWebGpuPipeline_t *)calloc( 1u, sizeof( *pipeline ) );
	if ( !pipeline ) goto fail;
	pipeline->core = core; pipeline->coreReceipt = *coreReceipt;
	pipeline->userData = createInfo->userData; pipeline->host = createInfo->host;
	pipeline->receipt = receipt; pipeline->live = qtrue;
	*outPipeline = pipeline; *outReceipt = receipt;
	return qtrue;
fail:
	DestroyCandidate( createInfo, &receipt );
	return qfalse;
}

qboolean RalWebGpu_PipelineReceiptExact(
		const ralWebGpuPipelineReceipt_t *a,
		const ralWebGpuPipelineReceipt_t *b ) {
	return ReceiptValid( a ) && ReceiptValid( b )
		&& !memcmp( a, b, sizeof( *a ) );
}

qboolean RalWebGpu_PipelineDestroy( ralWebGpuCore_t *core,
		ralWebGpuPipeline_t *pipeline,
		const ralWebGpuPipelineReceipt_t *authority ) {
	ralWebGpuPipelineCreateInfo_t destroyInfo;
	if ( !core || !pipeline || pipeline->core != core || !authority
			|| pipeline->live != qtrue
			|| !RalWebGpu_PipelineReceiptExact( &pipeline->receipt, authority ) )
		return qfalse;
	memset( &destroyInfo, 0, sizeof( destroyInfo ) );
	destroyInfo.userData = pipeline->userData;
	destroyInfo.host = pipeline->host;
	DestroyCandidate( &destroyInfo, &pipeline->receipt );
	pipeline->live = qfalse;
	memset( pipeline, 0, sizeof( *pipeline ) );
	free( pipeline );
	return qtrue;
}
