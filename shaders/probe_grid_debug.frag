#version 450

#define MAX_PROBE_COUNT 64

layout(location = 0) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) flat in int instanceIndex;

layout(location = 0) out vec4 oColor;

layout(set = 0, binding = 4) uniform samplerCube[MAX_PROBE_COUNT] environmentMaps;

struct Probe
{
	vec3 position;
	float pad0[1];
};

layout(std430, set = 0, binding = 5) readonly buffer ProbesData
{
	ivec3 dimensions;
	float pad0[1];
	vec3 extent;
	float pad1[1];
	vec3 cornerPosition;
	float pad2[1];
	Probe probes[];
};

void main()
{
	const ivec3 lastIndex = dimensions - ivec3(1u);

	const vec3 spacing = extent / vec3(lastIndex);

	const vec3 debugCenter = probes[instanceIndex].position;

	const vec3 fragPosLocalToGrid = max(debugCenter - cornerPosition, 0.0);
	const ivec3 probe3DIndex = ivec3(clamp(fragPosLocalToGrid / spacing, vec3(0.0), dimensions));

	const ivec3 weights = ivec3(dimensions.y * dimensions.z, dimensions.z, 1);
	const int probe1DIndex = int(dot(probe3DIndex, weights));

	vec3 normal = normalize(fragNormal);

	vec3 sampleColor = texture(environmentMaps[probe1DIndex], normal).rgb;

	//oColor = vec4((debugCenter - cornerPosition) / extent, 1.0);
	oColor = vec4(sampleColor, 1.0);
}