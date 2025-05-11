#version 450

layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D baseImage;

void main()
{
	vec4 col = texture(baseImage, fragUV);
	//col.g = 0.0;

	oColor = col;
}