#version 450

layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D baseImage;

struct cascade_desc
{
    // number of probes p
    int p;
    // number of discrete values per probes q
    int q;

    // interval length
    float dw;
};

// cascade desc buffer
layout (binding = 1) readonly buffer CascadeDescUBO {
    cascade_desc[] descs;
} cdubo;

// cascade probes position buffer
layout (binding = 2) readonly buffer CascadeUBO {
    vec2[] positions;
} cubo;

// radiance interval storage buffer
layout (binding = 3) readonly buffer RadianceIntervalUBO {
    vec4[] intervals;
} riubo;

void main()
{
	vec3 col = texture(baseImage, fragUV).rgb;
	oColor = vec4(riubo.intervals[0].rgb, 1.0);
}