// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../client/client.h"
#include "../render/ral/backends/webgpu/ral_webgpu_browser_emscripten.h"

#include <string.h>

static ralPresentationHostReceipt_t s_receipt;

static qboolean WiredWeb_BuildPresentationReceipt(
		ralPresentationHostReceipt_t *outReceipt ) {
	ralWebGpuBrowserModuleBorrow_t browser;
	ralPresentationHostReceipt_t receipt;
	if ( !outReceipt || !RalWebGpu_BrowserModuleBorrow( &browser )
			|| !browser.runtime || !browser.presentation
			|| !browser.configuredReceipt.ready
			|| !browser.configuredReceipt.configured
			|| browser.configuredReceipt.suspended ) return qfalse;
	memset( &receipt, 0, sizeof( receipt ) );
	receipt.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION;
	receipt.backendType = RAL_BACKEND_WEBGPU;
	receipt.ownerGeneration = browser.runtimeReceipt.generation;
	receipt.surfaceGeneration =
		browser.configuredReceipt.presentationGeneration;
	receipt.ownerIdentity = (uintptr_t)browser.runtime;
	receipt.logicalWidth = browser.configuredReceipt.cssWidth;
	receipt.logicalHeight = browser.configuredReceipt.cssHeight;
	receipt.pixelWidth = browser.configuredReceipt.pixelWidth;
	receipt.pixelHeight = browser.configuredReceipt.pixelHeight;
	receipt.contentScaleX = receipt.logicalWidth
		? (float)receipt.pixelWidth / (float)receipt.logicalWidth : 0.0f;
	receipt.contentScaleY = receipt.logicalHeight
		? (float)receipt.pixelHeight / (float)receipt.logicalHeight : 0.0f;
	receipt.visible = qtrue;
	receipt.ready = qtrue;
	if ( !Ral_PresentationHostReceiptValid( &receipt ) ) return qfalse;
	*outReceipt = receipt;
	return qtrue;
}

qboolean RALimp_PresentationOpen( void *context,
		const ralPresentationHostOpenInfo_t *info,
		ralPresentationHostReceipt_t *outReceipt ) {
	(void)context;
	if ( !Ral_PresentationHostOpenInfoValid( info )
			|| info->backendType != RAL_BACKEND_WEBGPU
			|| !info->requestVisible
			|| !WiredWeb_BuildPresentationReceipt( &s_receipt ) ) return qfalse;
	*outReceipt = s_receipt;
	return qtrue;
}

qboolean RALimp_PresentationRefresh( void *context,
		const ralPresentationHostReceipt_t *currentReceipt,
		ralPresentationHostReceipt_t *outReceipt ) {
	ralPresentationHostReceipt_t refreshed;
	(void)context;
	if ( !outReceipt || !Ral_PresentationHostReceiptExact( currentReceipt,
			&s_receipt ) || !WiredWeb_BuildPresentationReceipt( &refreshed ) )
		return qfalse;
	s_receipt = refreshed;
	*outReceipt = refreshed;
	return qtrue;
}

qboolean RALimp_PresentationBorrow( void *context,
		const ralPresentationHostReceipt_t *currentReceipt,
		ralPresentationSurfaceBorrow_t *outBorrow ) {
	ralWebGpuBrowserModuleBorrow_t browser;
	ralPresentationSurfaceBorrow_t borrow;
	(void)context;
	if ( !outBorrow || !Ral_PresentationHostReceiptExact( currentReceipt,
			&s_receipt ) || !RalWebGpu_BrowserModuleBorrow( &browser ) ) return qfalse;
	memset( &borrow, 0, sizeof( borrow ) );
	borrow.schemaVersion = RAL_PRESENTATION_HOST_SCHEMA_VERSION;
	borrow.backendType = RAL_BACKEND_WEBGPU;
	borrow.ownerGeneration = currentReceipt->ownerGeneration;
	borrow.surfaceGeneration = currentReceipt->surfaceGeneration;
	borrow.ownerIdentity = currentReceipt->ownerIdentity;
	borrow.surfaceIdentity = browser.configuredReceipt.canvasIdentity;
	borrow.ready = qtrue;
	if ( !Ral_PresentationSurfaceBorrowValid( &borrow ) ) return qfalse;
	*outBorrow = borrow;
	return qtrue;
}

qboolean RALimp_PresentationClose( void *context,
		const ralPresentationHostReceipt_t *currentReceipt,
		ralPresentationHostCloseMode_t mode ) {
	(void)context;
	if ( !Ral_PresentationHostReceiptExact( currentReceipt, &s_receipt )
			|| ( mode != RAL_PRESENTATION_HOST_KEEP_OWNER
				&& mode != RAL_PRESENTATION_HOST_DESTROY_OWNER ) ) return qfalse;
	memset( &s_receipt, 0, sizeof( s_receipt ) );
	return qtrue;
}

void GLimp_InitGamma( glconfig_t *config ) {
	if ( config ) config->deviceSupportsGamma = qfalse;
}

void GLimp_SetGamma( unsigned char red[256], unsigned char green[256],
		unsigned char blue[256] ) {
	(void)red; (void)green; (void)blue;
}
