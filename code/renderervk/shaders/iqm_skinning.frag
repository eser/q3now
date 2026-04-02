#version 450

// IQM GPU skinning fragment shader
// Receives bone-transformed normal and tangent for future normal mapping.
// Currently does simple textured output; tangent-space basis is available
// for normal map sampling when a normal map texture is bound.

layout(set = 1, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec4 frag_tangent;  // tangent (xyz) + bitangent sign (w)

layout(location = 0) out vec4 out_color;

void main() {
	out_color = texture(texture0, frag_tex_coord);
}
