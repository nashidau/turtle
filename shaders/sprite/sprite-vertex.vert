#version 450

layout(binding = 0) uniform grid_vertex  {
	vec3 pos;
	vec3 tile;
	vec2 tex;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
	gl_Position = vec4(ubo.pos, 1.0);
	fragColor = inColor;
	fragTexCoord = inTexCoord;
}
