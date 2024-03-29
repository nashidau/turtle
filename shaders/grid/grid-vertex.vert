#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable

#include "../trtl_strata_base.include"
#include "../trtl_strata_grid.include"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uvec3 tileData;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec2 tilePos;
layout(location = 3) out uvec3 tileDataOut;
layout(location = 4) out float time;


mat2 rotate2d(float angle) {
	return mat2(cos(angle), -sin(angle),
		    sin(angle),  cos(angle));
}

void main() {
    vec2 screenSize = vec2(trtl_strata_base.screensize_time_unused.x,
		    trtl_strata_base.screensize_time_unused.y);

    vec2 pos = inPosition.xy;

    // Center on the top left tile
    pos -= vec2(0.5, 0.5);

    // Move based on the current view position
    pos.x -= trtl_strata_grid.camera_center.x;
    pos.y -= trtl_strata_grid.camera_center.y;

    // rotate
    pos = rotate2d(degree45) * pos;

    // Flattern to isometric
    pos.y = pos.y / 2.0;

    // Expand out by the tile size
    pos *= trtl_strata_grid.tile_size.x;
    pos *= 2.0; // Vulkan is -1 to 1
    pos /= screenSize;

    // That's our (2d) position
    gl_Position = vec4(pos, 0.0, 1.0);
    if (inPosition.x == trtl_strata_grid.camera_center.x && inPosition.y ==
		    trtl_strata_grid.camera_center.y) {
	    fragColor = vec3(1.0, 0.0, 0.0);
    } else {
	    fragColor = vec3(1.0, 1.0, 1.0);
    }
    fragTexCoord = inTexCoord.xy;

    tileDataOut = tileData;
    tilePos = vec2(tileData.x, tileData.y);
    time = trtl_strata_base.screensize_time_unused.z;
}

