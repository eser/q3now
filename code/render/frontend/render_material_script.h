// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RENDER_FRONTEND_MATERIAL_SCRIPT_H
#define WIRED_RENDER_FRONTEND_MATERIAL_SCRIPT_H

#include "render_submission_material.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RENDER_MATERIAL_SCRIPT_MAX_ENTRIES 4096u

typedef struct {
	char name[MAX_QPATH];
	char imageName[MAX_QPATH];
	renderAlphaMode_t alphaMode;
	float alphaCutoff;
	qboolean depthWrite;
	qboolean clampToEdge;
	qboolean sky;
} renderMaterialScriptEntry_t;

typedef struct {
	renderMaterialScriptEntry_t entries[RENDER_MATERIAL_SCRIPT_MAX_ENTRIES];
	uint32_t count;
	qboolean ready;
} renderMaterialScriptCatalog_t;

qboolean RenderMaterialScript_Load( renderMaterialScriptCatalog_t *catalog,
	const refimport_t *imports );
qboolean RenderMaterialScript_Lookup(
	const renderMaterialScriptCatalog_t *catalog, const char *name,
	renderMaterialScriptEntry_t *outEntry );

#ifdef __cplusplus
}
#endif

#endif
