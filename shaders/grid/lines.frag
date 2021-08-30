#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

#define BORDER_WIDTH 0.01

void main(){
    //vec2 st = gl_FragCoord.xy/u_resolution.xy;
    vec2 st = fragTexCoord;//gl_FragCoord.xy/ vec2(800.0, 600.0);
    //vec3 color = vec3(0.0);
    vec3 color = fragColor;
/*
    // bottom-left
    vec2 bl = step(vec2(BORDER_WIDTH),st);
    float pct = bl.x * bl.y;

    // top-right
    vec2 tr = step(vec2(BORDER_WIDTH),1.0-st);
    pct *= tr.x * tr.y;

    color = vec3(pct);
*/
    outColor = vec4(color,1.0);
}
