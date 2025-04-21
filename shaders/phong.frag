#version 450

#define MAX_PROBE_COUNT 64

vec3 lerp(in vec3 a, in vec3 b, in float t)
{
	return mix(a, b, t);
}

vec3 bilerp(in vec3 a, in vec3 b, in vec3 c, in vec3 d, in vec2 t)
{
	const vec3 ab = lerp(a, b, t[0]);
	const vec3 cd = lerp(c, d, t[0]);
	return lerp(ab, cd, t[1]);
}

vec3 trilerp(in vec3 a, in vec3 b, in vec3 c, in vec3 d, in vec3 e, in vec3 f, in vec3 g, in vec3 h, in vec3 t)
{
	const vec3 abcd = bilerp(a, b, c, d, vec2(t[0], t[1]));
	const vec3 efgh = bilerp(e, f, g, h, vec2(t[0], t[1]));
	return lerp(abcd, efgh, t[2]);
}

vec3 lerpClamped(in vec3 a, in vec3 b, in float t)
{
	return mix(a, b, clamp(t, 0.0, 1.0));
}

vec3 bilerpClamped(in vec3 a, in vec3 b, in vec3 c, in vec3 d, in vec2 t)
{
	const vec3 ab = lerpClamped(a, b, t[0]);
	const vec3 cd = lerpClamped(c, d, t[0]);
	return lerpClamped(ab, cd, t[1]);
}

vec3 trilerpClamped(in vec3 a, in vec3 b, in vec3 c, in vec3 d, in vec3 e, in vec3 f, in vec3 g, in vec3 h, in vec3 t)
{
	const vec3 abcd = bilerpClamped(a, b, c, d, vec2(t[0], t[1]));
	const vec3 efgh = bilerpClamped(e, f, g, h, vec2(t[0], t[1]));
	return lerpClamped(abcd, efgh, t[2]);
}


layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 oColor;

layout(set = 1, binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 4) uniform samplerCube[MAX_PROBE_COUNT] irradianceMaps;

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

struct PointLight
{
	vec3 diffuseColor;
	float diffusePower;
	vec3 specularColor;
	float specularPower;
	vec3 position;
	float pad0[1];
};

layout(std430, set = 0, binding = 2) readonly buffer PointLightsData
{
	int pointLightCount;
	PointLight pointLights[];
};

struct DirectionalLight
{
	vec3 diffuseColor;
	float diffusePower;
	vec3 specularColor;
	float specularPower;
	vec3 direction;
	float pad0[1];
};

layout(std430, set = 0, binding = 3) readonly buffer DirectionalLightsData
{
	int directionalLightCount;
	DirectionalLight directionalLights[];
};

layout(push_constant, std430) uniform pc
{
    vec3 viewPos;
};

struct LightingResult
{
	vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

void applySinglePointLight(inout LightingResult fragLighting, in PointLight pointLight, in vec3 normal)
{
	fragLighting.ambient = vec3(0.1);
	vec3 lightDir = normalize(pointLight.position - fragPos);
	float diff = max(dot(normal, lightDir), 0.0);
	fragLighting.diffuse += diff * pointLight.diffuseColor * pointLight.diffusePower;
	fragLighting.specular += vec3(0.0);
}

void applySingleDirectionalLight(inout LightingResult fragLighting, in DirectionalLight directionalLight, in vec3 normal)
{
	fragLighting.ambient = vec3(0.1);
	vec3 lightDir = normalize(directionalLight.direction);
	float diff = max(dot(normal, lightDir), 0.0);
	fragLighting.diffuse += diff * directionalLight.diffuseColor * directionalLight.diffusePower;
	fragLighting.specular += vec3(0.0);
}

void applyImageBasedIrradiance(inout LightingResult fragLighting, in vec3 normal)
{
	const ivec3 indexBorders = dimensions - ivec3(1u);
	const vec3 fragPosLocalToGrid = max(fragPos - cornerPosition, 0.0);
	const ivec3 probeCornerIndex = ivec3(fragPosLocalToGrid / extent);

	const ivec3 probe3DIndex000 = min(probeCornerIndex + ivec3(0, 0, 0), indexBorders);
	const ivec3 probe3DIndex010 = min(probeCornerIndex + ivec3(0, 1, 0), indexBorders);
	const ivec3 probe3DIndex100 = min(probeCornerIndex + ivec3(1, 0, 0), indexBorders);
	const ivec3 probe3DIndex001 = min(probeCornerIndex + ivec3(0, 0, 1), indexBorders);
	const ivec3 probe3DIndex110 = min(probeCornerIndex + ivec3(1, 1, 0), indexBorders);
	const ivec3 probe3DIndex011 = min(probeCornerIndex + ivec3(0, 1, 1), indexBorders);
	const ivec3 probe3DIndex101 = min(probeCornerIndex + ivec3(1, 0, 1), indexBorders);
	const ivec3 probe3DIndex111 = min(probeCornerIndex + ivec3(1, 1, 1), indexBorders);

	// 1DIndex = 3DIndex.x * dimensions.z + 3DIndex.y * dimensions.z * dimensions.x + 3DIndex.z
	const ivec3 weights = ivec3(dimensions.z, dimensions.z * dimensions.x, 1);
	const int probe1DIndex000 = int(dot(probe3DIndex000, weights));
	const int probe1DIndex010 = int(dot(probe3DIndex010, weights));
	const int probe1DIndex100 = int(dot(probe3DIndex100, weights));
	const int probe1DIndex110 = int(dot(probe3DIndex110, weights));
	const int probe1DIndex001 = int(dot(probe3DIndex001, weights));
	const int probe1DIndex011 = int(dot(probe3DIndex011, weights));
	const int probe1DIndex101 = int(dot(probe3DIndex101, weights));
	const int probe1DIndex111 = int(dot(probe3DIndex111, weights));

	const vec3 probePos000 = probes[probe1DIndex000].position;
	const vec3 probePos111 = probes[probe1DIndex111].position;

	const vec3 t = (fragPos - probePos000) / (probePos111 - probePos000);
	
	const vec3 irradiance000 = texture(irradianceMaps[probe1DIndex000], normal).rgb;
	const vec3 irradiance010 = texture(irradianceMaps[probe1DIndex010], normal).rgb;
	const vec3 irradiance100 = texture(irradianceMaps[probe1DIndex100], normal).rgb;
	const vec3 irradiance110 = texture(irradianceMaps[probe1DIndex110], normal).rgb;
	const vec3 irradiance001 = texture(irradianceMaps[probe1DIndex001], normal).rgb;
	const vec3 irradiance011 = texture(irradianceMaps[probe1DIndex011], normal).rgb;
	const vec3 irradiance101 = texture(irradianceMaps[probe1DIndex101], normal).rgb;
	const vec3 irradiance111 = texture(irradianceMaps[probe1DIndex111], normal).rgb;
	
	vec3 interpIrradiance = trilerpClamped(irradiance000, irradiance010, irradiance100, irradiance110,
									irradiance001, irradiance011, irradiance101, irradiance111, t);

	fragLighting.diffuse += interpIrradiance;
	//fragLighting.diffuse += clamp(interpIrradiance, 0.0, 1.0);
	fragLighting.specular += vec3(0.0);
}

void main()
{
	vec3 normal = normalize(fragNormal);

	vec3 viewDirection = normalize(fragPos - viewPos);

	LightingResult fragLighting = { vec3(0.0), vec3(0.0), vec3(0.0) };

	for (int i = 0; i < pointLightCount; i++)
	{
		applySinglePointLight(fragLighting, pointLights[i], normal);
	}

	for (int i = 0; i < directionalLightCount; i++)
	{
		applySingleDirectionalLight(fragLighting, directionalLights[i], normal);
	}

	applyImageBasedIrradiance(fragLighting, normal);

	oColor = texture(texSampler, fragUV);
	oColor *= vec4(fragLighting.ambient + fragLighting.diffuse + fragLighting.specular, 1.0);

#ifdef DEBUG_IRRADIANCE_MAP
	oColor = texture(irradianceMap, normal);
#endif
}