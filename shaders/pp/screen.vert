#version 450

layout(location = 0) in vec3 aPos;
layout(location = 3) in vec2 aUV;

layout(location = 1) out vec2 fragUV;

void main()
{
	gl_Position	= vec4(aPos, 1.0);
	fragUV = aUV;
}