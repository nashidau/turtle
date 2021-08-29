#version 450 
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 3.0),
    vec2(3.0, -1.0)
);

void main() {

	//gl_Position = vec4(inPosition, 1.0);
     gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragTexCoord = gl_Position.xy / 2.0;
}

#if 0
void main() 
{
	fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(fragTexCoord * 2.0f + -1.0f, 0.0f, 1.0f);

//	fragColor = vec3(1.0,1.0,0);
/*
   if (gl_VertexIndex == 0) {
	   gl_Position = vec4(-1, -1, 0, 1.0);
   } else if (gl_VertexIndex == 1) {
	gl_Position = vec4(3, -1, 0, 1.0);
	} else {
	gl_Position = vec4(-1, 3, 0, 1.0);
}

    float x = -1.0 + float((gl_VertexIndex & 1) << 2);
    float y = -1.0 + float((gl_VertexIndex & 2) << 1);
    fragTexCoord.x = (x+1.0)*0.5;
    fragTexCoord.y = (y+1.0)*0.5;
    gl_Position = vec4(x, y, 0, 1); */
}
#endif
