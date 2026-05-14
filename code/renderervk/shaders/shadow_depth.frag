// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450
// Shadow depth fragment shader — depth-only pass, no color output needed.
// The depth buffer write comes from the rasterizer automatically.
// This shader exists only because Vulkan requires a fragment stage.

void main() {
	// intentionally empty — depth is written by fixed-function
}
