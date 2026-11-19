// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Wired Engine contributors

#ifndef WIRED_RENDER_MATERIAL_VFS_H
#define WIRED_RENDER_MATERIAL_VFS_H

#include "render_material_lua.h"
#include "tr_public.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DEV/native authoring bridge. The renderer import table is used only to
 * obtain canonical TASK-237 identities and immutable source bytes; Lua still
 * lowers to the backend-neutral source receipt before any renderer consumes
 * the declaration. */
qboolean RenderMaterialVfs_LoadLuaSourceReceipt( lua_State *L,
	const refimport_t *imports, const char *requestedPath,
	ralMaterialSourceReceipt_t *outReceipt,
	renderMaterialLuaDiagnostic_t *outDiagnostic );

#ifdef __cplusplus
}
#endif
#endif
