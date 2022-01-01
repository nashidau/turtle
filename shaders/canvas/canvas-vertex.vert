#version 450 
#extension GL_ARB_separate_shader_objects : enable

layout(constant_id = 0) const int screenWidth = 800;
layout(constant_id = 1) const int screenHeight = 600;

#include "../trtl_system.include"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec2 screenSize;
layout(location = 2) out float time;

vec4 positions[3] = vec4[](
    vec4(-1.0, -1.0, 0.0, 1.0),
    vec4(-1.0, 3.0, 0.0, 1.0),
    vec4(3.0, -1.0, 0.0, 1.0)
);

void main() {
    screenSize = trtl_system.screensize;
    gl_Position = positions[gl_VertexIndex];
    fragTexCoord = gl_Position.xy / 4.0;
    time = trtl_system.time;
}

/* vim: set sw=4 sts=4 : */
