// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_shader_abi.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK failed %s:%d\n",__FILE__,__LINE__); return 1; } } while(0)

int main( void ) {
	static const char source[] = "@compute @workgroup_size(1) fn main() {}";
	ralShaderModuleAbi_t module;
	ralShaderBindingAbi_t binding;
	ralShaderAbiManifest_t manifest;
	ralShaderVariantAbi_t variant;
	ralShaderPipelineKey_t key;
	memset( &module, 0, sizeof( module ) ); memset( &binding, 0, sizeof( binding ) );
	memset( &manifest, 0, sizeof( manifest ) ); memset( &variant, 0, sizeof( variant ) );
	module.stage = RAL_STAGE_COMPUTE; module.entryPoint = "main";
	module.sourceDigest.lane0 = 1; module.sourceDigest.lane1 = 2;
	module.artifacts[0].target = RAL_SHADER_ARTIFACT_SPIRV;
	module.artifacts[0].byteCount = 4; module.artifacts[0].digest.lane0 = 3; module.artifacts[0].digest.lane1 = 4;
	module.artifacts[1].target = RAL_SHADER_ARTIFACT_MSL;
	module.artifacts[2].target = RAL_SHADER_ARTIFACT_WGSL;
	module.artifacts[2].byteCount = sizeof( source ) - 1u;
	CHECK( Ral_ShaderArtifactDigest( source, sizeof( source ) - 1u, &module.artifacts[2].digest ) == ralSuccess );
	binding.set = 0; binding.binding = 0; binding.bindingClass = RAL_SHADER_BIND_UNIFORM_BUFFER;
	binding.arrayCount = 1; binding.stageFlags = RAL_STAGE_COMPUTE; binding.minBufferBindingSize = 32;
	manifest.schemaVersion = RAL_SHADER_ABI_SCHEMA_VERSION; manifest.generation = 1;
	manifest.pipelineKind = RAL_SHADER_PIPELINE_COMPUTE; manifest.modules = &module; manifest.moduleCount = 1;
	manifest.bindings = &binding; manifest.bindingCount = 1;
	manifest.inlineData.byteSize = 16; manifest.inlineData.stageFlags = RAL_STAGE_COMPUTE;
	manifest.inlineData.webgpuUniformSet = 0; manifest.inlineData.webgpuUniformBinding = 0;
	variant.artifactTarget = RAL_SHADER_ARTIFACT_WGSL; variant.generation = 1;
	variant.semanticStateDigest.lane0 = 5; variant.semanticStateDigest.lane1 = 6;
	CHECK( Ral_ShaderAbiManifestValid( &manifest ) );
	CHECK( Ral_ShaderVariantAbiValid( &manifest, &variant ) );
	CHECK( Ral_ShaderAbiBuildPipelineKey( &manifest, &variant, &key ) == ralSuccess );
	CHECK( key.artifactTarget == RAL_SHADER_ARTIFACT_WGSL && key.ready == qtrue );
	binding.minBufferBindingSize = 8;
	CHECK( !Ral_ShaderAbiManifestValid( &manifest ) );
	puts( "ral WebGPU shader ABI contract: PASS" );
	return 0;
}
