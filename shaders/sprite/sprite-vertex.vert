#version 450
#extension GL_EXT_debug_printf : enable

layout(constant_id = 0) const int screenWidth = 800;
layout(constant_id = 1) const int screenHeight = 600;
layout(constant_id = 2) const int tileSize = 128;

layout(binding = 0) uniform pos2d {
    vec2[2] pos;
    //vec3 size;
} u_sprite;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

// FIXME: Shader lib too
const float pi = 3.141592653589793;
const float degree45 = pi / 4.0;

// FIXME: Should be in a shader-lib.  glslc allows includes
mat2 rotate2d(float angle) {
	return mat2(cos(angle), -sin(angle),
		    sin(angle),  cos(angle));
}

vec2 positions[4] = vec2[](
    vec2(-0.5, -1),
    vec2(-0.5, 0),
    vec2(0.5, -1),
    vec2(0.5, 0)
);

vec2 textureCoords[] = vec2[](
    vec2(0, 0),
    vec2(0, 1.0),
    vec2(1.0, 0),
    vec2(1.0, 1.0)
);

float width = 128;

void main() {
    vec2 screenSize = vec2(screenWidth, screenHeight) / 2.0;
    //vec2 pos = inPosition.xy;

  //  fragColor = inColor;
  //  fragTexCoord = inTexCoord;

    // Center on the top left tile
    //pos -= vec2(0.5, 0.5);

    // Move based on the current view position
    //pos.x -= centerPos.x;
    //pos.y -= centerPos.y;

    // rotate
    vec2 pos = rotate2d(degree45) * u_sprite.pos[gl_InstanceIndex];

    // Flattern to isometric
    pos.y = pos.y / 2.0;


    // So the zero point has been positioned, now move based on which corner we are. Offset by index
    pos += positions[gl_VertexIndex];

    // Expand out by the tile size
    pos *= tileSize;
    pos /= screenSize;


    gl_Position = vec4(pos, 0.0, 1.0);
    fragTexCoord = textureCoords[gl_VertexIndex];

//    vec2 screenSize = vec2(screenWidth, screenHeight);
//    gl_Position = positions[gl_VertexIndex];
 //   fragTexCoord = gl_Position.xy / 4.0;
}




// vim: set sts=4 sw=4 :
