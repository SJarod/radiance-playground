#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 oColor;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

void main()
{
	oColor = vec4(0.0, 0.0, 1.0, 1.0);
}