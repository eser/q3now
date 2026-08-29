// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_renderer_module.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

int main( void ) {
	refimport_t imports;
	refexport_t *renderer;
	glconfig_t config;
	ralWebGpuRendererModuleFrameReceipt_t receipt;
	memset( &imports, 0, sizeof( imports ) );
	CHECK( GetRefAPI( REF_API_VERSION - 1, &imports ) == NULL );
	renderer = GetRefAPI( REF_API_VERSION, &imports );
	CHECK( renderer != NULL && renderer->Shutdown && renderer->BeginRegistration
		&& renderer->RegisterModel && renderer->RegisterShader
		&& renderer->LoadWorld && renderer->ClearScene
		&& renderer->AddRefEntityToScene && renderer->AddPolyToScene
		&& renderer->RenderScene && renderer->DrawStretchPic
		&& renderer->BeginFrame && renderer->EndFrame
		&& renderer->GetConfig && renderer->GetMemoryBudget
		&& renderer->AddRefEntityToSceneTemporal
		&& renderer->PresentationChanged
		&& renderer->CookLightingProject
		&& !renderer->CookLightingProject( "/tmp" ) );
	CHECK( WiredWebGpu_RendererPoll() == RAL_WEBGPU_ASYNC_READY );
	CHECK( !WiredWebGpu_GetFrameReceipt( &receipt ) );
	memset( &config, 0xa5, sizeof( config ) );
	renderer->BeginRegistration( &config );
	CHECK( renderer->initFailed == qtrue );
	CHECK( renderer->GetConfig() == NULL );
	renderer->Shutdown( REF_UNLOAD_DLL );
	CHECK( GetRefAPI( REF_API_VERSION, &imports ) != NULL );
	puts( "ral WebGPU renderer refexport fail-closed contract: PASS" );
	return 0;
}
