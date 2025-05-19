#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec3 gPosition;
layout(location = 2) out vec3 gNormal;
layout(location = 3) out vec3 gSpecular;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

void main()
{
	gAlbedo = texture(texSampler, fragUV);
	gPosition = fragPos;
	gNormal = fragNormal;
	gSpecular = vec3(0.5);
}