#version 450

layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 oColor;

void main()
{
	oColor = vec4(fragUV, 0.0, 1.0);
}