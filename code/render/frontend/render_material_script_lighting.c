// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "render_material_script.h"
#include "render_submission.h"

#include <string.h>

qboolean RenderMaterialScript_ApplyLighting(
		const renderMaterialScriptCatalog_t *catalog, const char *name,
		renderSubmissionState_t *submission, qhandle_t material ) {
	renderMaterialScriptEntry_t entry;
	renderMaterialLighting_t lighting;
	uint64_t hash = UINT64_C( 14695981039346656037 );
	const unsigned char *bytes;
	size_t index;
	if ( !catalog || !name || !submission || material <= 0 ||
		!RenderMaterialScript_Lookup( catalog, name, &entry ) || !entry.hasLighting ) return qfalse;
	for ( index = 0u; name[index]; ++index )
		hash = ( hash ^ (unsigned char)name[index] ) * UINT64_C( 1099511628211 );
	bytes = (const unsigned char *)&entry.lighting;
	for ( index = 0u; index < sizeof( entry.lighting ); ++index )
		hash = ( hash ^ bytes[index] ) * UINT64_C( 1099511628211 );
	if ( !hash ) hash = 1u;
	if ( !Render_EmissiveMaterialLightingBuild( &entry.lighting, hash, hash,
			&lighting ) ) return qfalse;
	return RenderSubmission_SetMaterialLighting( submission, material, &lighting );
}
