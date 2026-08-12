// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef WIRED_CORE_CVARS_CVAR_VALUE_H
#define WIRED_CORE_CVARS_CVAR_VALUE_H

#include <stddef.h>

typedef enum {
	WIRED_CVAR_VALUE_OK = 0,
	WIRED_CVAR_VALUE_INVALID,
	WIRED_CVAR_VALUE_OUT_OF_RANGE
} wiredCvarValueStatus_t;

typedef struct {
	int integer;
	float number;
	/* Borrowed: points to a static bool literal or the matching enum
	 * descriptor string. The caller must keep enum descriptor storage alive. */
	const char *normalized;
} wiredCvarValue_t;

wiredCvarValueStatus_t wired_cvar_value_bool( const char *text, wiredCvarValue_t *out );
wiredCvarValueStatus_t wired_cvar_value_int( const char *text, int minimum, int maximum,
	wiredCvarValue_t *out );
wiredCvarValueStatus_t wired_cvar_value_float( const char *text, float minimum, float maximum,
	wiredCvarValue_t *out );
wiredCvarValueStatus_t wired_cvar_value_enum( const char *text,
	const char *const *values, size_t valueCount, wiredCvarValue_t *out );

#endif
