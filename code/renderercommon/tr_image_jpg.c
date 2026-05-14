// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#include "../qcommon/q_shared.h"
#include "../renderercommon/tr_public.h"

void R_LoadJPG( const char *filename, unsigned char **pic, int *width, int *height )
{
	ri.CL_LoadJPG( filename, pic, width, height );
}
