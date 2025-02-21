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
	float temp;
};

struct LightingResult
{
	vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

#define MAX_NUM_TOTAL_LIGHTS 2
layout(binding = 2) uniform LightArrayUniformBufferObject
{
	PointLight[MAX_NUM_TOTAL_LIGHTS] pointLights;
	int pointLightCount;
} lights;

void applySinglePointLight(inout LightingResult fragLighting, in PointLight pointLight, in vec3 normal)
{
	fragLighting.ambient = vec3(0.1);
	vec3 lightDir = normalize(pointLight.position - fragPos);
	float diff = max(dot(normal, lightDir), 0.0);
	fragLighting.diffuse = diff * pointLight.diffuseColor * pointLight.diffusePower;
	fragLighting.specular = vec3(0.0);
}

void main()
{
	vec3 normal = normalize(fragNormal);

	LightingResult fragLighting;

	for (int lightIdx = 0; lightIdx < lights.pointLightCount; lightIdx++)
	{
		applySinglePointLight(fragLighting, lights.pointLights[lightIdx], normal);
	}

	oColor = texture(texSampler, fragUV);
	oColor *= vec4(fragLighting.ambient + fragLighting.diffuse + fragLighting.specular, 1.0);
}