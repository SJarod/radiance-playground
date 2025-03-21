#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 oColor;

layout(binding = 4) uniform samplerCube environmentMap;

layout(push_constant, std430) uniform pc
{
    vec3 viewPos;
};

void main()
{
	vec3 normal = normalize(fragNormal);

	vec3 viewDirection = normalize(fragPos - viewPos);
	vec3 viewReflection = reflect(viewDirection, normal);
	vec3 reflectionSample = texture(environmentMap, viewReflection).rgb;

	oColor = vec4(reflectionSample, 1.0);
}