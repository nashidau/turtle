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
	mat4 model;
	vec3 position;
} meshinfo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
	//vec3 pos = inPosition.xyz * rotateZaxis(toRad(45.0)) * rotateXaxis(toRad(-60.0));
	vec3 pos = inPosition.xyz * rotateYaxis(meshinfo.angle);
	/*
	pos.y = (pos.y + pos.z) / 2.0;
	*/
//	pos.z = +0.5;
//	pos.z /= 100.0;
	//pos.z -= 0.5;
//	pos.z *= 1.0;
	pos.z /= 128.0;
//fixme: why won't this adjust the z dimtneion of hte obect on teh screen
//	- because z clipping isn't on.  I shoudl set the z to the color and see what happens
	pos.y *= -1.0;

	float scale = (trtl_strata_grid.tile_size.x / trtl_strata_base.screensize_time_unused.x) 
		* 2.0; // should be:w
	pos *= scale;
	gl_Position = vec4(pos, 1.0);

#if 0
    vec2 screenSize = vec2(trtl_strata_base.screensize_time_unused.x,
		    trtl_strata_base.screensize_time_unused.y);

	vec3 pos = inPosition;
	
	// Move up half a tile
	pos -= vec3(0.5, 0.5, 0);

	// rotate
	//pos = rotate2dmat3(degree45) * pos;

	// Flattern to isometric
	pos.y = pos.y / 2.0 + pos.z;

	vec2 pos2d = vec2(pos.x, pos.y);

    // Expand out by the tile size
    pos2d *= trtl_strata_grid.tile_size.x;
    pos2d *= 2.0; // Vulkan is -1 to 1
    pos2d /= screenSize;

    gl_Position = vec4(pos2d, 1.0, 1.0);

  vec4 phack = vec4(inPosition / 3.0, 1.0);
  gl_Position = phack;

/*


	float scale = trtl_strata_grid.tile_size.x / trtl_strata_base.screensize_time_unused.x 
		* 2.0;

	// Move based on the current view position
	vec3 pos = vec3(inPosition.xy, 0.0);
	pos.x -= trtl_strata_grid.camera_center.x;
	pos.y -= trtl_strata_grid.camera_center.y;

	vec3 iTmp = pos * scale;
	//gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(iTmp, 1.0);
	//gl_Position = ubo.model * vec4(iTmp, 1.0);
*/	

#endif
	fragColor = inColor;
	//fragColor = pos.zzz;
	//fragColor = vec3(1.0, 0.0, 0);
	fragTexCoord = inTexCoord;
}
