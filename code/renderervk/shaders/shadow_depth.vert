#version 450
// Shadow depth vertex shader — renders scene from light's perspective.
// Only outputs position (depth is written automatically by the rasterizer).

layout(push_constant) uniform Transform {
	mat4 lightMVP;  // light view-projection * model matrix
};

layout(location = 0) in vec3 in_position;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	gl_Position = lightMVP * vec4( in_position, 1.0 );
}
