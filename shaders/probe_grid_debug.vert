#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

layout(location = 0) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;
layout(location = 4) flat out int instanceIndex;

layout(binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 views[6];
	mat4 proj;
} mvp;

struct Probe
{
	vec3 position;
};

layout(std430, set = 0, binding = 5) readonly buffer ProbesData
{
	ivec3 dimensions;
	float pad0[1];
	vec3 extent;
	vec3 cornerPosition;
	Probe probes[];
};

void main()
{
	instanceIndex = gl_InstanceIndex;
	vec3 pos = aPos + probes[instanceIndex].position;
	gl_Position = mvp.proj * mvp.views[0] * mvp.model * vec4(pos, 1.0);

	fragPos = vec3(mvp.model * vec4(pos, 1.0));
	fragNormal = normalize(mat3(mvp.model) * aNormal);
}
