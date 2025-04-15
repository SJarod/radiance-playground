#version 450

#define USE_NORMAL_AS_UV

layout(location = 0) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 oColor;

layout(binding = 4) uniform samplerCube[1] environmentMaps;

layout(push_constant, std430) uniform pc
{
    vec3 viewPos;
};

void main()
{
	vec3 normal = normalize(fragNormal);

#ifdef USE_NORMAL_AS_UV
	vec3 sampleColor = texture(environmentMaps[0], normal).rgb;
#else
	vec3 viewDirection = normalize(fragPos - viewPos);
	vec3 viewReflection = reflect(viewDirection, normal);
	vec3 sampleColor = texture(environmentMaps[0], viewReflection).rgb;
#endif

	oColor = vec4(sampleColor, 1.0);
}