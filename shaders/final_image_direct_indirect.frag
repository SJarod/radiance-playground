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
layout (binding = 1) uniform CascadeDescUBO {
    cascade_desc[] descs;
} cdubo;

// cascade probes position buffer
layout (binding = 2) uniform CascadeUBO {
    vec2[] positions;
} cubo;

// radiance interval storage buffer
layout (binding = 3) uniform RadianceIntervalUBO {
    vec4[] intervals;
} riubo;

void main()
{
	vec3 col = texture(baseImage, fragUV).rgb;
	col.g = 0.0;

	//col = step(0.5, fragUV.y) * col;
	//col = step(-0.5, fragUV.x) * vec3(fragUV.xy, 1.0);

	oColor = vec4(col, 1.0);
	//oColor = vec4(vec3(1.0), 1.0);
}