#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../trtl_strata_base.include"
#include "../trtl_strata_grid.include"

layout(set=2, binding = 0) uniform UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
	float scale = trtl_strata_grid.tile_size.x / trtl_strata_base.screensize_time_unused.x * 2.0;
	vec3 iTmp = inPosition * scale;
	//gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(iTmp, 1.0);
	fragColor = inColor;
	fragTexCoord = inTexCoord;
}
