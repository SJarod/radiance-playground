#version 450

#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragPos;

layout(binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 views[6];
	mat4 proj;
} mvp;

void main()
{
	gl_Position = mvp.proj * mvp.views[gl_ViewIndex] * mvp.model * vec4(aPos, 1.0);

	fragPos = vec3(mvp.model * vec4(aPos, 1.0));
	fragNormal = normalize(mat3(mvp.model) * aNormal);
	fragColor = aColor;
	fragUV = aUV;
}