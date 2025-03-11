#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragPos;
layout(location = 4) out vec3 viewPos;

layout(binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} mvp;

void main()
{
	gl_Position = mvp.proj * mvp.view * mvp.model * vec4(aPos, 1.0);

	fragPos = vec3(mvp.model * vec4(aPos, 1.0));
	fragNormal = normalize(mat3(mvp.model) * aNormal);
	fragColor = aColor;
	fragUV = aUV;
	viewPos = mvp.view[3].xyz;
}