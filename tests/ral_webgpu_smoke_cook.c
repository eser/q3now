// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#include "ral_webgpu_smoke_fixture.h"
#include "render_material_project_cook.h"
#include "maps/map_format_registry.h"

#include <stdio.h>
#include <string.h>

refimport_t ri;

/* Build-time only. Produce actual cooked artifacts using the production
 * decoder and project cooker; the runtime probe never invokes a cooker. */
int main( int argc, char **argv ) {
	static renderMaterialScriptCatalog_t catalog;
	mapFile_t map = {0};
	dshader_t shader = {0};
	dsurface_t surface = {0};
	renderMaterialProjectCookReceipt_t receipt;
	renderMaterialProjectCookDiagnostic_t diagnostic;
	char path[4096];
	FILE *file;
	/* Small original 2x2 uncompressed 32-bit TGA, BGRA byte order. */
	const byte texture[] = {
		0,0,2,0,0,0,0,0,0,0,0,0,2,0,2,0,32,8,
		96,160,224,255, 160,224,96,255,
		224,96,160,255, 192,192,192,255
	};
	if ( argc != 2 || strlen( argv[1] ) + 64u >= sizeof( path ) ) return 1;
	(void)snprintf( path, sizeof( path ), "%s/%s", argv[1], WIRED_WEBGPU_SMOKE_TEXTURE );
	if ( !( file = fopen( path, "wb" ) ) ) return 2;
	qboolean written = fwrite( texture, 1, sizeof( texture ), file ) == sizeof( texture );
	if ( fclose( file ) != 0 ) written = qfalse;
	if ( !written ) return 3;
	WiredWebGpuSmokeFixture_Init( &ri, argv[1] );
	catalog.ready = qtrue;
	(void)snprintf( map.name, sizeof( map.name ), "%s", WIRED_WEBGPU_SMOKE_MAP );
	map.checksum = WIRED_WEBGPU_SMOKE_CHECKSUM;
	map.numShaders = 1;
	map.shaders = &shader;
	map.numSurfaces = 1;
	map.surfaces = &surface;
	surface.surfaceType = MST_PLANAR;
	(void)snprintf( shader.shader, sizeof( shader.shader ), "%s", WIRED_WEBGPU_SMOKE_TEXTURE );
	(void)snprintf( path, sizeof( path ), "%s/materials/cooked", argv[1] );
	if ( !RenderMaterialProjectCook_Execute( &catalog, &map, &ri, path,
			1u, &receipt, &diagnostic ) ) {
		fprintf( stderr, "WebGPU fixture cook failed: reason=%u shader=%u dependency=%u\n",
			diagnostic.reason, diagnostic.shaderIndex, diagnostic.dependencyIndex );
		return 4;
	}
	if ( !RenderMaterialProjectCook_ReceiptValid( &receipt )
			|| receipt.cookedMaterialCount != 1u || receipt.fallbackStageCount ) return 5;
	return RenderMaterialProjectCook_LogTelemetry( &receipt, &ri ) ? 0 : 6;
}
