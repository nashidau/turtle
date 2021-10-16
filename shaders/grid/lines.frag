#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec2 tilePos;
layout(location = 3) flat in uvec3 tileData;

layout(location = 0) out vec4 outColor;

#define BORDER_WIDTH  0.01
#define BORDER_ADJUST 0.01

float random (in vec2 _st) {
    return fract(sin(dot(_st.xy,
                         vec2(12.9898,78.233)))*
        43758.5453123);
}

float rand(in float x) {
	return fract(sin(x)*1.0);
}

// 2D Noise based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
float noise (in vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    // Smooth Interpolation

    // Cubic Hermine Curve.  Same as SmoothStep()
    vec2 u = f*f*(3.0-2.0*f);
    // u = smoothstep(0.,1.,f);

    // Mix 4 coorners percentages
    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}

void main(){
    vec3 brown2 = vec3(147.0/255.0, 127.0/255.0,111.0/255.0);
    vec3 darkbrown = vec3(0.19, 0.10, 0.05);
    vec3 black = vec3(0.0,0.0,0.0);
    vec3 red = vec3(1.0,0,0);

    vec2 st = fragTexCoord;

    uint seed = tileData.z;

    // Enable this to force tiling
    // So lets make some tiles sliced horizontaly or vertically
    float ntiles = float(seed & 0xff) / 255.0;
    float mul = floor(pow(ntiles, 4) * 3) + 1;

    st = fract(st * mul);

    vec3 color = fragColor;

    // This is our borders
    // bottom-left
    vec2 bl = step(vec2(BORDER_WIDTH + BORDER_ADJUST * sin(st.x+st.y*7)),st);
    float pct = bl.x * bl.y;
    // top-right
    vec2 tr = step(vec2(BORDER_WIDTH),1.0-st);
    pct *= tr.x * tr.y;

    // The brown
    float n = noise((fragTexCoord + 7 * tilePos) * 5);
 
    vec3 tileColor = mix(brown2, darkbrown, n);

    color = mix(color, tileColor, pct);

    outColor = vec4(color,1.0);
}
