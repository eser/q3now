// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_FOG_RUNTIME_POLICY_H
#define WIRED_FOG_RUNTIME_POLICY_H

/* Keep these values aligned with renderer-local fogType_t. */
enum {
	WIRED_FOG_RUNTIME_NONE = 0,
	WIRED_FOG_RUNTIME_LINEAR = 1,
	WIRED_FOG_RUNTIME_EXP = 2,
	WIRED_FOG_RUNTIME_EXP2 = 3
};

static inline int wired_fog_runtime_resolve_volume_type(
	int enabled, int authoredType, int fallbackType )
{
	if ( !enabled ) {
		return WIRED_FOG_RUNTIME_NONE;
	}
	if ( authoredType >= WIRED_FOG_RUNTIME_LINEAR
		&& authoredType <= WIRED_FOG_RUNTIME_EXP2 ) {
		return authoredType;
	}
	if ( fallbackType < 0 || fallbackType > 2 ) {
		return WIRED_FOG_RUNTIME_NONE;
	}
	return WIRED_FOG_RUNTIME_LINEAR + fallbackType;
}

static inline int wired_fog_runtime_resolve_global_type(
	int enabled, int publishedType )
{
	if ( !enabled || publishedType < WIRED_FOG_RUNTIME_LINEAR
		|| publishedType > WIRED_FOG_RUNTIME_EXP2 ) {
		return WIRED_FOG_RUNTIME_NONE;
	}
	return publishedType;
}

#endif
