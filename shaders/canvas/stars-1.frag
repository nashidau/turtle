#version 450
#extension GL_ARB_separate_shader_objects : enable



//CBS
//Parallax scrolling fractal galaxy.
//Inspired by JoshP's Simplicity shader: https://www.shadertoy.com/view/lslGWr

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec2 screenSize_in;
layout(location = 3) in float time;



vec3 nrand3( vec2 co )
{
	vec3 a = fract( cos( co.x*8.3e-3 + co.y )*vec3(1.3e5, 4.7e5, 2.9e5) );
	vec3 b = fract( sin( co.x*0.3e-3 + co.y )*vec3(8.1e5, 1.0e5, 0.1e5) );
	vec3 c = mix(a, b, 0.5);
	return c;
}


void main() {
	vec2 screenSize = vec2(1600.0, 1200.0);

	vec2 iResolution = vec2(screenSize.x, screenSize.y);
	vec2 fragCoord = gl_FragCoord.xy / vec2(screenSize.x/screenSize.y);
	vec2 uv = 2. * fragCoord.xy / iResolution.xy - 1.;
	vec2 uvs = uv * iResolution.xy / max(iResolution.x, iResolution.y);
	vec3 p = vec3(uvs / 4., time) + vec3(1., -1.3, 0.);
	vec3 p2 = p;

	outColor = vec4(fragColor,0.2);
	
	//Let's add some stars
	//Thanks to http://glsl.heroku.com/e#6904.0
	vec2 seed = p.xy * 2.0;	
	seed = floor(seed * iResolution.x);
	vec3 rnd = nrand3( seed );
	outColor += vec4(pow(rnd.y,40.0));

	//Second Layer
	vec2 seed2 = p2.xy * 2.0;
	seed2 = floor(seed2 * iResolution.x);
	vec3 rnd2 = nrand3( seed2 );
	outColor += vec4(pow(rnd2.y,40.0));
	
	//fragColor = mix(freqs[3]-.3, 1., v) *
	//vec4(1.5*freqs[2] * t * t* t , 1.2*freqs[1] * t * t, freqs[3]*t, 1.0)+c2+starcolor;
	
}

