#version 450

layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D baseImage;

void main()
{
	vec3 col = texture(baseImage, fragUV).rgb;
	col.g = 0.0;

	oColor = vec4(col, 1.0);
}