#version 450
#extension GL_ARB_separate_shader_objects : enable

const mat4 sepia = mat4( 0.393f, 0.769f, 0.189f, 0.0f,
                         0.349f, 0.686f, 0.168f, 0.0f,
	           	 0.272f, 0.534f, 0.131f, 0.0f,
	           	 0.0f,   0.0f,   0.0f,   1.0f);

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 inColor;
/*
layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec2 screenSize_in;
layout(location = 2) in float time;
*/


void main() {
	outColor = sepia * inColor;
}

