#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "../trtl_strata_base.include"
#include "../trtl_strata_grid.include"

/*
layout(set=2, binding = 0) uniform UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 proj;
} ubo;
*/
layout(set=2, binding = 0) uniform MeshShaderInfo {
	float angle; // In radian
	mat4 model; // FIXME: Unsued; shoudl rtoate the model to base position
	vec3 position; // FIXME: Unused: shoudl move the model to the base position.
} meshinfo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
	//vec3 pos = inPosition.xyz * rotateZaxis(toRad(45.0)) * rotateXaxis(toRad(-60.0));
	vec3 pos = inPosition.xyz * rotateYaxis(meshinfo.angle) * rotateXaxis(toRad(-30.0));
	pos.z = +0.5;
	pos.z /= 128.0;
	pos.y *= -1.0;

	float scale = (trtl_strata_grid.tile_size.x / trtl_strata_base.screensize_time_unused.x) 
		* 2.0;
	pos *= scale;
	gl_Position = vec4(pos, 1.0);

	fragColor = inColor;
	fragTexCoord = inTexCoord;
}
