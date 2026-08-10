// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// Post-gamma HUD overlay vertex stage. Unlike the world/2D shaders this reads
// NO uniform: the CPU (RB_StretchPicOverlay → vk_end_frame replay) has already
// converted each quad's screen-space rectangle into clip-space (NDC) positions,
// so the vertex passes straight through. Color and texcoord ride along to the
// fragment stage. Keeping it uniform-free means the overlay pipeline needs no
// descriptor set 0 UBO, so it composes cleanly with the gamma present pass's
// post-process layout (which has no per-draw vertex UBO).

layout(location = 0) in vec2 in_ndc;     // clip-space position (CPU-computed)
layout(location = 1) in vec4 in_color;   // rgba, display-space (perceptual)
layout(location = 2) in vec2 in_tex_coord0;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord0;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position     = vec4( in_ndc, 0.0, 1.0 );
	frag_color      = in_color;
	frag_tex_coord0 = in_tex_coord0;
}
