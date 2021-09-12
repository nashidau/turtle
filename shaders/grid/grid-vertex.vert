#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable

layout(constant_id = 0) const int screenWidth = 800;
layout(constant_id = 1) const int screenHeight = 600;
layout(constant_id = 2) const int tileSize = 128;

layout(binding = 0) uniform  pos2d {
	float x;
	float y;
} centerPos;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in u8vec4 tileData;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec2 tilePos;
layout(location = 3) out u8vec4 tileDataOut;

const float pi = 3.141592653589793;
const float degree45 = pi / 4.0;

mat2 rotate2d(float angle) {
	return mat2(cos(angle), -sin(angle),
		    sin(angle),  cos(angle));
}

void main() {
	// FIXME: This 2 is a hack.
    vec2 screenSize = vec2(screenWidth, screenHeight) / 2.0;
    vec2 pos = inPosition.xy;

    // Center on the top left tile
    pos -= vec2(0.5, 0.5);

    // Move based on the current view position
    pos.x -= centerPos.x;
    pos.y -= centerPos.y;

    // rotate
    pos = rotate2d(degree45) * pos;

    // Flattern to isometric
    pos.y = pos.y / 2.0;

    // Expand out by the tile size
    pos *= tileSize;
    pos /= screenSize;

    gl_Position = vec4(pos, 0.0, 1.0);
    if (inPosition.x == centerPos.x && inPosition.y == centerPos.y) {
	    fragColor = vec3(1.0, 0.0, 0.0);
    } else {
	    fragColor = vec3(1.0, 1.0, 1.0);
    }
    fragTexCoord = inTexCoord.xy;

    tileDataOut = tileData;
    tilePos = vec2(tileData.x, tileData.y);
}

