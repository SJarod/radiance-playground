#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 oColor;

layout(binding = 1) uniform sampler2D texSampler;

struct PointLight
{
	vec3 diffuseColor;
	float diffusePower;
	vec3 specularColor;
	float specularPower;
	vec3 position;
	float pad0[1];
};

layout(std430, binding = 2) readonly buffer PointLightsData
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

layout(std430, binding = 3) readonly buffer DirectionalLightsData
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

	oColor = texture(texSampler, fragUV);
	oColor *= vec4(fragLighting.ambient + fragLighting.diffuse + fragLighting.specular, 1.0);
}