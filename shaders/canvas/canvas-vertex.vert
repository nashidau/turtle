#version 450 
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform object {
    vec2 screenSize;
    float time;
} data;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec2 screenSize;
layout(location = 3) out float time;

vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 3.0),
    vec2(3.0, -1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    //fragTexCoord = gl_Position.xy / 2.0;
    fragTexCoord = data.screenSize / 2.0;
    fragColor = vec3(sin(screenSize.x), sin(screenSize.y) , 0.0);

    time = data.time;
    screenSize = data.screenSize;
}

/* vim: set sw=4 sts=4 : */
