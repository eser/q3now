// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

/* The depth ranges a draw may use. Kept in its own header, with no Vulkan
 * dependency, because the renderer's shared state (backEndState_t in tr_local.h)
 * stores one, and the neutral renderer files (which cannot see Vulkan) must still see
 * the same struct layout as the Vulkan-aware ones. */

#ifndef VK_DEPTH_RANGE_H
#define VK_DEPTH_RANGE_H

typedef enum {
	DEPTH_RANGE_NORMAL,		// [0..1]
	DEPTH_RANGE_ZERO,		// [0..0]
	DEPTH_RANGE_ONE,		// [1..1]
	DEPTH_RANGE_WEAPON,		// [0..0.3]
	DEPTH_RANGE_COUNT
}  Vk_Depth_Range;

#endif
