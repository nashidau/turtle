#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
	vec4 outTmp = texture(texSampler, fragTexCoord);
	if (outTmp.w < 1.0 && outTmp.w > 0.0) {
		outTmp = vec4(1.0,0.0,0.0,1.0);
	}
	outColor = outTmp;
}
