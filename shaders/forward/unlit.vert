#version 450

layout(location = 0) in vec3 aPos;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

layout(binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} mvp;

void main()
{
	gl_Position = mvp.proj * mvp.view * mvp.model * vec4(aPos, 1.0);
	fragColor = aColor;
	fragUV = aUV;
}