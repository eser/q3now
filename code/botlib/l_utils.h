// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/*****************************************************************************
 * name:		l_util.h
 *
 * desc:		utils
 *
 * $Archive: /source/code/botlib/l_util.h $
 *
 *****************************************************************************/

#define Vector2Angles(v,a)		vectoangles(v,a)
#ifndef MAX_PATH
#define MAX_PATH				MAX_QPATH
#endif
#define Maximum(x,y)			(x > y ? x : y)
#define Minimum(x,y)			(x < y ? x : y)
