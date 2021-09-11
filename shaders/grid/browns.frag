
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

float random(vec2 st) {
   return fract(sin(dot(st.xy,vec2(12.874,33.333))) * 49993.24321);
}

/*
void main(){
    //vec2 st = gl_FragCoord.xy/u_resolution.xy;
    vec2 st = fragTexCoord;//gl_FragCoord.xy/ vec2(800.0, 600.0);
    vec3 color = vec3(0.0);

    // bottom-left
    vec2 bl = step(vec2(BORDER_WIDTH),st);
    float pct = bl.x * bl.y;

    // top-right
    vec2 tr = step(vec2(BORDER_WIDTH),1.0-st);
    pct *= tr.x * tr.y;

    color = vec3(pct);

    outColor = vec4(color,1.0);
}*/


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

void main()
{
   vec2  st = fragTexCoord.xy / 64.0;// / iResolution.xy;
   
   //st *= 10.0;
   
   vec2 pos = vec2(st*4.0);
   //pos.x += iTime;
   //pos.y += sin(iTime) * 2.0;

    // Use the noise function
    float n = noise(pos);
   
    outColor = mix(vec4(212.0/255.0,201.0/255.0,182.0/255.0,1.0),
		    vec4(147.0/255.0,127.0/255.0,111.0/255.0,1.0),n);
    outColor = vec4(fragTexCoord.xy, 0.0, 0.0);
}
