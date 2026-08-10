// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#version 450

// Post-gamma HUD overlay fragment stage. Modulates the bound texture by the
// per-vertex color. For a solid panel fill the shader handle is cls.whiteShader,
// whose texture is a 1x1 white texel, so the output is simply the (display-space,
// already-perceptual) vertex color — its alpha blends against the gamma-encoded
// swapchain image, giving light-independent translucency. For a textured overlay
// (icon) the sampler supplies the image and the color tints it.

layout(set = 0, binding = 0) uniform sampler2D u_tex;

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_tex_coord0;

layout(location = 0) out vec4 out_color;

void main() {
	out_color = texture( u_tex, frag_tex_coord0 ) * frag_color;
}
