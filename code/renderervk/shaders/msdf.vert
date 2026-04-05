#version 450

// 64 bytes
layout(push_constant) uniform Transform {
    mat4 mvp;
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color0;
layout(location = 2) in vec2 in_tex_coord0;

layout(location = 0) out vec4 frag_color0;
layout(location = 1) centroid out vec2 frag_tex_coord0;

void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    frag_color0 = in_color0;
    frag_tex_coord0 = in_tex_coord0;
}
