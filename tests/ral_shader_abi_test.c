// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { \
	fprintf( stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #x ); \
	return 1; \
} } while ( 0 )

typedef struct {
	uint32_t vertexWords[4], fragmentWords[4];
	char wgslVertex[24];
	char wgslFragment[24];
	ralShaderModuleAbi_t modules[2];
	ralShaderBindingAbi_t bindings[2];
	ralShaderVertexInputAbi_t inputs[2];
	ralShaderSpecConstantAbi_t specs[2];
	ralShaderSpecValue_t values[2];
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralVertexBinding_t vertexBinding;
	ralVertexAttribute_t attributes[2];
	ralSpecConstant_t pipelineSpecs[2];
	const ralBindGroupLayout_t *layouts[1];
	ralGraphicsPipelineCreateInfo_t pipeline;
} Fixture;

static void Artifact( ralShaderArtifactAbi_t *artifact, ralShaderArtifactTarget_t target,
	                  const void *bytes, uint32_t byteCount ) {
	memset( artifact, 0, sizeof( *artifact ) ); artifact->target = target;
	if ( bytes ) {
		artifact->byteCount = byteCount;
		if ( Ral_ShaderArtifactDigest( bytes, byteCount, &artifact->digest ) != ralSuccess ) abort();
	}
}

static void Setup( Fixture *f ) {
	uint32_t i;
	memset( f, 0, sizeof( *f ) );
	f->vertexWords[0] = 0x07230203u; f->vertexWords[1] = 1u; f->vertexWords[2] = 2u; f->vertexWords[3] = 3u;
	f->fragmentWords[0] = 0x07230203u; f->fragmentWords[1] = 4u; f->fragmentWords[2] = 5u; f->fragmentWords[3] = 6u;
	memcpy( f->wgslVertex, "@vertex fn main() {}", 20u );
	memcpy( f->wgslFragment, "@fragment fn main() {}", 22u );
	for ( i = 0; i < 2; ++i ) {
		f->modules[i].stage = i == 0 ? RAL_STAGE_VERTEX : RAL_STAGE_FRAGMENT;
		f->modules[i].entryPoint = "main";
		f->modules[i].sourceDigest.lane0 = 10u + i;
		f->modules[i].sourceDigest.lane1 = 20u + i;
		Artifact( &f->modules[i].artifacts[RAL_SHADER_ARTIFACT_SPIRV], RAL_SHADER_ARTIFACT_SPIRV,
		          i == 0 ? (const void *)f->vertexWords : (const void *)f->fragmentWords,
		          sizeof( f->vertexWords ) );
		Artifact( &f->modules[i].artifacts[RAL_SHADER_ARTIFACT_MSL], RAL_SHADER_ARTIFACT_MSL, NULL, 0 );
		Artifact( &f->modules[i].artifacts[RAL_SHADER_ARTIFACT_WGSL], RAL_SHADER_ARTIFACT_WGSL,
		          i == 0 ? (const void *)f->wgslVertex : (const void *)f->wgslFragment,
		          i == 0 ? 20u : 22u );
	}
	f->bindings[0].set = 0; f->bindings[0].binding = 0;
	f->bindings[0].bindingClass = RAL_SHADER_BIND_UNIFORM_BUFFER;
	f->bindings[0].arrayCount = 1; f->bindings[0].stageFlags = RAL_STAGE_ALL_GRAPHICS;
	f->bindings[0].minBufferBindingSize = 64; f->bindings[0].dynamicOffset = qfalse;
	f->bindings[1].set = 0; f->bindings[1].binding = 1;
	f->bindings[1].bindingClass = RAL_SHADER_BIND_SAMPLED_TEXTURE;
	f->bindings[1].arrayCount = 1; f->bindings[1].stageFlags = RAL_STAGE_FRAGMENT;
	f->bindings[1].viewDimension = RAL_SHADER_VIEW_2D;
	f->bindings[1].sampleType = RAL_SHADER_SAMPLE_FLOAT; f->bindings[1].dynamicOffset = qfalse;
	f->inputs[0].location = 0; f->inputs[0].format = RAL_FORMAT_R32G32B32_SFLOAT;
	f->inputs[1].location = 1; f->inputs[1].format = RAL_FORMAT_R32G32_SFLOAT;
	f->specs[0].constantId = 7; f->specs[0].stageFlags = RAL_STAGE_VERTEX; f->specs[0].defaultValue = 1;
	f->specs[1].constantId = 9; f->specs[1].stageFlags = RAL_STAGE_FRAGMENT; f->specs[1].defaultValue = 0;
	f->values[0].constantId = 7; f->values[0].value = 4;
	f->values[1].constantId = 9; f->values[1].value = 8;
	f->manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	f->manifest.generation = 1; f->manifest.pipelineKind = RAL_SHADER_PIPELINE_GRAPHICS;
	f->manifest.modules = f->modules; f->manifest.moduleCount = 2;
	f->manifest.bindings = f->bindings; f->manifest.bindingCount = 2;
	f->manifest.vertexInputs = f->inputs; f->manifest.vertexInputCount = 2;
	f->manifest.inlineData.byteSize = 16; f->manifest.inlineData.stageFlags = RAL_STAGE_ALL_GRAPHICS;
	f->manifest.inlineData.webgpuUniformSet = 0; f->manifest.inlineData.webgpuUniformBinding = 0;
	f->manifest.specConstants = f->specs; f->manifest.specConstantCount = 2;
	f->variant.artifactTarget = RAL_SHADER_ARTIFACT_SPIRV; f->variant.generation = 1;
	f->variant.specValues = f->values; f->variant.specValueCount = 2;
	f->vertexBinding.binding = 0; f->vertexBinding.stride = 20; f->vertexBinding.inputRate = RAL_VERTEX_INPUT_PER_VERTEX;
	f->attributes[0].location = 0; f->attributes[0].binding = 0;
	f->attributes[0].format = RAL_FORMAT_R32G32B32_SFLOAT; f->attributes[0].offset = 0;
	f->attributes[1].location = 1; f->attributes[1].binding = 0;
	f->attributes[1].format = RAL_FORMAT_R32G32_SFLOAT; f->attributes[1].offset = 12;
	f->pipelineSpecs[0].constantId = 7; f->pipelineSpecs[0].value = 4;
	f->pipelineSpecs[1].constantId = 9; f->pipelineSpecs[1].value = 8;
	f->layouts[0] = (const ralBindGroupLayout_t *)(uintptr_t)1u;
	f->pipeline.vertexSpirv = f->vertexWords; f->pipeline.vertexSpirvSize = sizeof( f->vertexWords );
	f->pipeline.fragmentSpirv = f->fragmentWords; f->pipeline.fragmentSpirvSize = sizeof( f->fragmentWords );
	f->pipeline.vertexBindings = &f->vertexBinding; f->pipeline.numVertexBindings = 1;
	f->pipeline.vertexAttributes = f->attributes; f->pipeline.numVertexAttributes = 2;
	f->pipeline.topology = RAL_TOPOLOGY_TRIANGLE_LIST; f->pipeline.raster.lineWidth = 1.0f;
	f->pipeline.colorFormats[0] = RAL_FORMAT_R8G8B8A8_UNORM; f->pipeline.numColorFormats = 1;
	f->pipeline.sampleCount = 1; f->pipeline.bindGroupLayouts = f->layouts; f->pipeline.numBindGroupLayouts = 1;
	f->pipeline.pushConstantSize = 16; f->pipeline.pushConstantStages = RAL_STAGE_ALL_GRAPHICS;
	f->pipeline.specConstants = f->pipelineSpecs; f->pipeline.numSpecConstants = 2;
	if ( Ral_GraphicsPipelineSemanticDigest( &f->pipeline, &f->variant.semanticStateDigest ) != ralSuccess ) abort();
}

static int RunCompute( void ) {
	uint32_t words[4] = { 0x07230203u, 7u, 8u, 9u };
	ralShaderModuleAbi_t module;
	ralShaderBindingAbi_t binding;
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralComputePipelineCreateInfo_t pipeline;
	ralShaderPipelineKey_t key, sentinel, unchanged;
	const ralBindGroupLayout_t *layouts[1];
	memset( &module, 0, sizeof( module ) );
	memset( &binding, 0, sizeof( binding ) );
	memset( &manifest, 0, sizeof( manifest ) );
	memset( &variant, 0, sizeof( variant ) );
	memset( &pipeline, 0, sizeof( pipeline ) );
	module.stage = RAL_STAGE_COMPUTE; module.entryPoint = "main";
	module.sourceDigest.lane0 = 31u; module.sourceDigest.lane1 = 32u;
	Artifact( &module.artifacts[RAL_SHADER_ARTIFACT_SPIRV], RAL_SHADER_ARTIFACT_SPIRV,
	          words, sizeof( words ) );
	Artifact( &module.artifacts[RAL_SHADER_ARTIFACT_MSL], RAL_SHADER_ARTIFACT_MSL, NULL, 0u );
	Artifact( &module.artifacts[RAL_SHADER_ARTIFACT_WGSL], RAL_SHADER_ARTIFACT_WGSL, NULL, 0u );
	binding.set = 0u; binding.binding = 0u;
	binding.bindingClass = RAL_SHADER_BIND_UNIFORM_BUFFER;
	binding.arrayCount = 1u; binding.stageFlags = RAL_STAGE_COMPUTE;
	binding.minBufferBindingSize = 16u;
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION;
	manifest.generation = 2u; manifest.pipelineKind = RAL_SHADER_PIPELINE_COMPUTE;
	manifest.modules = &module; manifest.moduleCount = 1u;
	manifest.bindings = &binding; manifest.bindingCount = 1u;
	manifest.inlineData.byteSize = 16u; manifest.inlineData.stageFlags = RAL_STAGE_COMPUTE;
	manifest.inlineData.webgpuUniformSet = 0u;
	manifest.inlineData.webgpuUniformBinding = 0u;
	variant.artifactTarget = RAL_SHADER_ARTIFACT_SPIRV; variant.generation = 3u;
	layouts[0] = (const ralBindGroupLayout_t *)(uintptr_t)1u;
	pipeline.computeSpirv = words; pipeline.computeSpirvSize = sizeof( words );
	pipeline.bindGroupLayouts = layouts; pipeline.numBindGroupLayouts = 1u;
	pipeline.pushConstantSize = 16u;
	CHECK( Ral_ComputePipelineSemanticDigest( &pipeline,
	        &variant.semanticStateDigest ) == ralSuccess );
	CHECK( Ral_ShaderAbiMatchesComputePipeline( &manifest, &variant, &pipeline, &key ) );
	CHECK( key.ready == qtrue && key.artifactTarget == RAL_SHADER_ARTIFACT_SPIRV );
	memset( &sentinel, 0x6b, sizeof( sentinel ) ); unchanged = sentinel;
	pipeline.pushConstantSize = 12u;
	CHECK( !Ral_ShaderAbiMatchesComputePipeline( &manifest, &variant, &pipeline, &unchanged ) );
	CHECK( memcmp( &unchanged, &sentinel, sizeof( unchanged ) ) == 0 );
	pipeline.pushConstantSize = 16u; words[1]++;
	CHECK( !Ral_ShaderAbiMatchesComputePipeline( &manifest, &variant, &pipeline, &unchanged ) );
	CHECK( memcmp( &unchanged, &sentinel, sizeof( unchanged ) ) == 0 );
	return 0;
}

int main( void ) {
	Fixture f, mutated;
	ralShaderPipelineKey_t key, changed, sentinel;
	ralShaderDigest_t oldState, artifactVector;
	const uint8_t artifactBytes[8] = { 3u, 2u, 35u, 7u, 1u, 0u, 0u, 0u };
	CHECK( Ral_ShaderArtifactDigest( artifactBytes, sizeof( artifactBytes ),
	        &artifactVector ) == ralSuccess );
	CHECK( artifactVector.lane0 == 0x9ace6ca4ab9ab8d3ull );
	CHECK( artifactVector.lane1 == 0x33b99fd9410ba454ull );
	Setup( &f );
	CHECK( Ral_ShaderAbiManifestValid( &f.manifest ) );
	CHECK( Ral_ShaderVariantAbiValid( &f.manifest, &f.variant ) );
	CHECK( Ral_ShaderAbiBuildPipelineKey( &f.manifest, &f.variant, &key ) == ralSuccess );
	CHECK( Ral_ShaderAbiMatchesGraphicsPipeline( &f.manifest, &f.variant, &f.pipeline, &changed ) );
	CHECK( Ral_ShaderPipelineKeyExact( &key, &changed ) );

	mutated = f; mutated.manifest = f.manifest; mutated.manifest.bindings = mutated.bindings;
	memcpy( mutated.bindings, f.bindings, sizeof( f.bindings ) );
	mutated.bindings[1].sampleType = RAL_SHADER_SAMPLE_UNFILTERABLE_FLOAT;
	CHECK( Ral_ShaderAbiBuildPipelineKey( &mutated.manifest, &f.variant, &changed ) == ralSuccess );
	CHECK( !Ral_ShaderPipelineKeyExact( &key, &changed ) );
	mutated = f; mutated.manifest = f.manifest; mutated.manifest.modules = mutated.modules;
	memcpy( mutated.modules, f.modules, sizeof( f.modules ) );
	mutated.modules[0].sourceDigest.lane0++;
	CHECK( Ral_ShaderAbiBuildPipelineKey( &mutated.manifest, &f.variant, &changed ) == ralSuccess );
	CHECK( !Ral_ShaderPipelineKeyExact( &key, &changed ) );

	oldState = f.variant.semanticStateDigest;
	f.pipeline.raster.cullMode = RAL_CULL_BACK;
	CHECK( Ral_GraphicsPipelineSemanticDigest( &f.pipeline, &f.variant.semanticStateDigest ) == ralSuccess );
	CHECK( !Ral_ShaderDigestExact( &oldState, &f.variant.semanticStateDigest ) );
	CHECK( Ral_ShaderAbiBuildPipelineKey( &f.manifest, &f.variant, &changed ) == ralSuccess );
	CHECK( !Ral_ShaderPipelineKeyExact( &key, &changed ) );

	Setup( &f ); memset( &sentinel, 0x5a, sizeof( sentinel ) ); changed = sentinel;
	f.bindings[1].binding = 0;
	CHECK( !Ral_ShaderAbiManifestValid( &f.manifest ) );
	CHECK( Ral_ShaderAbiBuildPipelineKey( &f.manifest, &f.variant, &changed ) == ralErrorInvalidArgument );
	CHECK( memcmp( &changed, &sentinel, sizeof( changed ) ) == 0 );
	Setup( &f ); f.specs[1].constantId = 7;
	CHECK( !Ral_ShaderAbiManifestValid( &f.manifest ) );
	CHECK( Ral_ShaderAbiBuildPipelineKey( &f.manifest, &f.variant, &changed ) == ralErrorInvalidArgument );
	CHECK( memcmp( &changed, &sentinel, sizeof( changed ) ) == 0 );
	Setup( &f ); f.bindings[0].binding = 2u;
	CHECK( !Ral_ShaderAbiManifestValid( &f.manifest ) );
	CHECK( Ral_ShaderAbiBuildPipelineKey( &f.manifest, &f.variant, &changed ) == ralErrorInvalidArgument );
	CHECK( memcmp( &changed, &sentinel, sizeof( changed ) ) == 0 );
	Setup( &f ); f.manifest.bindingCount = RAL_SHADER_ABI_MAX_BINDINGS + 1u;
	CHECK( !Ral_ShaderAbiManifestValid( &f.manifest ) );
	CHECK( Ral_ShaderAbiBuildPipelineKey( &f.manifest, &f.variant, &changed ) == ralErrorInvalidArgument );
	CHECK( memcmp( &changed, &sentinel, sizeof( changed ) ) == 0 );
	Setup( &f ); f.bindings[1].set = RAL_SHADER_ABI_MAX_BIND_GROUPS;
	CHECK( !Ral_ShaderAbiManifestValid( &f.manifest ) );
	Setup( &f ); f.bindings[1].binding = RAL_SHADER_ABI_MAX_BINDINGS_PER_GROUP;
	CHECK( !Ral_ShaderAbiManifestValid( &f.manifest ) );
	Setup( &f ); f.bindings[1].arrayCount = RAL_SHADER_ABI_MAX_BINDING_ARRAY_COUNT + 1u;
	CHECK( !Ral_ShaderAbiManifestValid( &f.manifest ) );
	Setup( &f ); f.manifest.generation = UINT64_MAX;
	CHECK( !Ral_ShaderAbiManifestValid( &f.manifest ) );
	CHECK( Ral_ShaderAbiBuildPipelineKey( &f.manifest, &f.variant, &changed ) == ralErrorInvalidArgument );
	CHECK( memcmp( &changed, &sentinel, sizeof( changed ) ) == 0 );
	Setup( &f ); f.variant.generation = UINT64_MAX;
	CHECK( !Ral_ShaderVariantAbiValid( &f.manifest, &f.variant ) );
	CHECK( Ral_ShaderAbiBuildPipelineKey( &f.manifest, &f.variant, &changed ) == ralErrorInvalidArgument );
	CHECK( memcmp( &changed, &sentinel, sizeof( changed ) ) == 0 );
	Setup( &f ); f.values[1].constantId = 10;
	CHECK( !Ral_ShaderVariantAbiValid( &f.manifest, &f.variant ) );
	CHECK( RunCompute() == 0 );

	puts( "ral shader ABI contract: PASS" );
	return 0;
}
