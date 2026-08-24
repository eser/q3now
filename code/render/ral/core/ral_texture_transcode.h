// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_RAL_TEXTURE_TRANSCODE_H
#define WIRED_RAL_TEXTURE_TRANSCODE_H

#include "ral_texture_asset.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Produces the backend-neutral WRTXART cache artifact from an ETC1S/UASTC
 * KTX2 container. libktx remains an implementation detail of the .c file. */
qboolean Ral_TranscodeKtx2ToArtifact( const void *ktxBytes,
	uint64_t ktxByteLength, const ralKtx2Receipt_t *source,
	const ralTextureAssetReceipt_t *target, void *artifactBytes,
	uint64_t artifactCapacity, ralTextureArtifactReceipt_t *outReceipt );

#ifdef __cplusplus
}
#endif
#endif
