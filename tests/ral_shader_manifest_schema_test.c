// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include <stdio.h>
#include <string.h>

#include "ral_shader_abi.h"

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL:%d: %s\n", __LINE__, #x ); return 1; } } while ( 0 )

static void Module( ralShaderModuleAbi_t *module, uint32_t stage, uint32_t bytes,
	                uint64_t source0, uint64_t source1, uint64_t spirv0, uint64_t spirv1 ) {
	uint32_t i;
	memset( module, 0, sizeof( *module ) );
	module->stage = stage; module->entryPoint = "main";
	module->sourceDigest.lane0 = source0; module->sourceDigest.lane1 = source1;
	for ( i = 0; i < RAL_SHADER_ARTIFACT_COUNT; ++i ) module->artifacts[i].target = (ralShaderArtifactTarget_t)i;
	module->artifacts[RAL_SHADER_ARTIFACT_SPIRV].byteCount = bytes;
	module->artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest.lane0 = spirv0;
	module->artifacts[RAL_SHADER_ARTIFACT_SPIRV].digest.lane1 = spirv1;
}

static void BindingBase( ralShaderBindingAbi_t *binding, uint32_t slot, uint32_t stage ) {
	memset( binding, 0, sizeof( *binding ) );
	binding->binding = slot; binding->arrayCount = 1; binding->stageFlags = stage;
}

int main( void ) {
	ralShaderAbiManifest_t manifest;
	ralShaderModuleAbi_t modules[2];
	ralShaderBindingAbi_t bindings[4];
	ralShaderVertexInputAbi_t input;
	ralShaderSpecConstantAbi_t spec;

	memset( &manifest, 0, sizeof( manifest ) );
	Module( &modules[0], RAL_STAGE_VERTEX, 1024u, 1u, 2u,
	       0x1255590707a965bdull, 0x6a46950a85f3705full );
	Module( &modules[1], RAL_STAGE_FRAGMENT, 2308u, 3u, 4u,
	       0x7d06d608e32896a2ull, 0xe6232629bee4ad5dull );
	BindingBase( &bindings[0], 0u, RAL_STAGE_VERTEX );
	bindings[0].bindingClass = RAL_SHADER_BIND_UNIFORM_BUFFER;
	bindings[0].minBufferBindingSize = 544u;
	input.location = 0u; input.format = RAL_FORMAT_R32G32B32_SFLOAT;
	spec.constantId = 4u; spec.stageFlags = RAL_STAGE_FRAGMENT; spec.defaultValue = 0u;
	manifest.schemaVersion = 1u; manifest.generation = 1u;
	manifest.pipelineKind = RAL_SHADER_PIPELINE_GRAPHICS;
	manifest.modules = modules; manifest.moduleCount = 2u;
	manifest.bindings = bindings; manifest.bindingCount = 1u;
	manifest.vertexInputs = &input; manifest.vertexInputCount = 1u;
	manifest.specConstants = &spec; manifest.specConstantCount = 1u;
	CHECK( Ral_ShaderAbiManifestValid( &manifest ) );

	memset( &manifest, 0, sizeof( manifest ) );
	Module( &modules[0], RAL_STAGE_COMPUTE, 9004u, 5u, 6u,
	       0x50918049c8deeaebull, 0x2b4af8d5ead023f6ull );
	BindingBase( &bindings[0], 0u, RAL_STAGE_COMPUTE );
	bindings[0].bindingClass = RAL_SHADER_BIND_STORAGE_TEXTURE_WRITE;
	bindings[0].viewDimension = RAL_SHADER_VIEW_2D;
	bindings[0].storageTextureFormat = RAL_FORMAT_R16G16_SFLOAT;
	manifest.schemaVersion = 1u; manifest.generation = 2u;
	manifest.pipelineKind = RAL_SHADER_PIPELINE_COMPUTE;
	manifest.modules = modules; manifest.moduleCount = 1u;
	manifest.bindings = bindings; manifest.bindingCount = 1u;
	CHECK( Ral_ShaderAbiManifestValid( &manifest ) );

	Module( &modules[0], RAL_STAGE_COMPUTE, 3384u, 7u, 8u,
	       0xb3d978881ef424afull, 0x04af177ab9d517f8ull );
	BindingBase( &bindings[0], 0u, RAL_STAGE_COMPUTE );
	bindings[0].bindingClass = RAL_SHADER_BIND_SAMPLED_TEXTURE;
	bindings[0].viewDimension = RAL_SHADER_VIEW_2D; bindings[0].sampleType = RAL_SHADER_SAMPLE_FLOAT;
	BindingBase( &bindings[1], 1u, RAL_STAGE_COMPUTE );
	bindings[1].bindingClass = RAL_SHADER_BIND_FILTERING_SAMPLER;
	BindingBase( &bindings[2], 2u, RAL_STAGE_COMPUTE );
	bindings[2].bindingClass = RAL_SHADER_BIND_STORAGE_BUFFER_READ_WRITE;
	bindings[2].minBufferBindingSize = 1024u;
	BindingBase( &bindings[3], 3u, RAL_STAGE_COMPUTE );
	bindings[3].bindingClass = RAL_SHADER_BIND_UNIFORM_BUFFER;
	bindings[3].minBufferBindingSize = 8u;
	manifest.generation = 3u; manifest.bindings = bindings; manifest.bindingCount = 4u;
	manifest.inlineData.byteSize = 8u; manifest.inlineData.stageFlags = RAL_STAGE_COMPUTE;
	manifest.inlineData.webgpuUniformSet = 0u; manifest.inlineData.webgpuUniformBinding = 3u;
	CHECK( Ral_ShaderAbiManifestValid( &manifest ) );
	manifest.inlineData.webgpuUniformBinding = 2u;
	CHECK( !Ral_ShaderAbiManifestValid( &manifest ) );

	puts( "RAL shader composed manifest schema: PASS" );
	return 0;
}
