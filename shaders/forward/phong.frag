#version 450

#define DEFAULT_AMBIENT vec3(0.0)

#ifndef DEFAULT_AMBIENT
	#define DEFAULT_AMBIENT vec3(0.0)
#endif

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

struct Probe
{
	vec3 position;
	float pad0[1];
};


struct PointLight
{
	vec3 diffuseColor;
	float diffusePower;
	vec3 specularColor;
	float specularPower;
	vec3 position;
	float pad0[1];
	vec3 attenuation;
	float pad1[1];
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
	const vec3 fragPosToLightPos = pointLight.position - fragPos;
	const float lightDist = length(fragPosToLightPos);
	const vec3 lightDir = fragPosToLightPos / lightDist;

	const vec3 lightAttenuationWeights = vec3(1.0, lightDist, lightDist * lightDist);

	// Get attenuation (c + l * d + q * d^2)
	const float diffuseAttenuation = dot(pointLight.attenuation, lightAttenuationWeights);

	float diffuseIntensity = max(dot(normal, lightDir), 0.0);
	fragLighting.diffuse += diffuseIntensity * pointLight.diffuseColor * pointLight.diffusePower / diffuseAttenuation;
	fragLighting.specular += vec3(0.0);
}

void applySingleDirectionalLight(inout LightingResult fragLighting, in DirectionalLight directionalLight, in vec3 normal)
{
	vec3 lightDir = normalize(directionalLight.direction);
	float diff = max(dot(normal, lightDir), 0.0);
	fragLighting.diffuse += diff * directionalLight.diffuseColor * directionalLight.diffusePower;
	fragLighting.specular += vec3(0.0);
}

void main()
{
	vec3 normal = normalize(fragNormal);

	vec3 viewDirection = normalize(fragPos - viewPos);

	LightingResult fragLighting = { DEFAULT_AMBIENT, vec3(0.0), vec3(0.0) };

	for (int i = 0; i < pointLightCount; i++)
	{
		applySinglePointLight(fragLighting, pointLights[i], normal);
	}

	for (int i = 0; i < directionalLightCount; i++)
	{
		applySingleDirectionalLight(fragLighting, directionalLights[i], normal);
	}

	vec3 color = vec3(1.0);texture(texSampler, fragUV).rgb;
	fragLighting.ambient = vec3(0.1);
	color *= fragLighting.ambient + fragLighting.diffuse + fragLighting.specular;

#ifdef DEBUG_IRRADIANCE_MAP
	color = texture(irradianceMap, normal);
#endif

	oColor = vec4(pow(color, vec3(1.0/2.2)), 1.0);
}