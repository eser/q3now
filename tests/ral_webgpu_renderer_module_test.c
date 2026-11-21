// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "ral_webgpu_renderer_module.h"
#include "render_pbr_control.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if ( !(x) ) { fprintf( stderr, "FAIL %s:%d: %s\n", \
	__FILE__, __LINE__, #x ); return 1; } } while ( 0 )

static cvar_t pbrControl;
static unsigned pbrGets, pbrRanges;
static int pbrInvalid;
static qboolean rejectPbr;
static cvar_t *PbrGet( const char *name, const char *value, int flags ) {
	++pbrGets;
	pbrInvalid |= strcmp(name, "r_pbr") || strcmp(value, "1")
		|| flags != (CVAR_ARCHIVE | CVAR_NODEFAULT);
	return rejectPbr ? NULL : &pbrControl;
}
static void PbrRange( cvar_t *control, const char *low, const char *high, cvarValidator_t type ) {
	++pbrRanges;
	pbrInvalid |= control != &pbrControl || strcmp(low,"0") || strcmp(high,"1") || type != CV_INTEGER;
}
static int PbrRegistration( void ) {
	refimport_t imports = {0}; cvar_t defaults = {0};
	CHECK(wired_render_pbr_register(&imports, &defaults) == &defaults && defaults.integer == 0);
	imports.Cvar_Get = PbrGet; imports.Cvar_CheckRange = PbrRange;
	pbrControl.integer = 1; // registering must retain an already selected user value
	CHECK(wired_render_pbr_register(&imports, &defaults) == &pbrControl);
	CHECK(pbrControl.integer == 1 && pbrGets == 1 && pbrRanges == 1 && !pbrInvalid);
	rejectPbr = qtrue;
	CHECK(!wired_render_pbr_register(&imports, &defaults) && pbrRanges == 1);
	rejectPbr = qfalse;
	CHECK(wired_render_pbr_register(&imports, &defaults) == &pbrControl && pbrRanges == 2);
	return 0;
}

int main( void ) {
	CHECK(PbrRegistration() == 0);
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
		&& renderer->WorldContentIdentity
		&& renderer->AddSpriteToScene && renderer->AddRibbonToScene
		&& renderer->AddBeamToScene && !renderer->EmitParticles
		&& renderer->AddDecalToScene && renderer->AddLensSourceToScene
		&& renderer->GetLensVisibility
		&& renderer->CookLightingProject
		&& !renderer->CookLightingProject( "/tmp" ) );
	CHECK( WiredWebGpu_RendererPoll() == RAL_WEBGPU_ASYNC_READY );
    { float value=123.0f;
      CHECK(!renderer->GetLensVisibility(7, &value)); CHECK(value==123.0f); }
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
