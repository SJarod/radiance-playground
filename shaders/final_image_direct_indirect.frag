#version 450

#define DEBUG_DISPLAY_PROBES

layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D baseImage;

struct probe
{
	vec2 position;
};

struct cascade_desc
{
    // number of probes p
    int p;
    // number of discrete values per probes q
    int q;

    // interval length
    float dw;
};

float lerp(float a, float b, float x)
{
    //return mix(a, b, x);
    // https://registry.khronos.org/OpenGL-Refpages/gl4/html/mix.xhtml
    return a * (1.0-x) + b * x;
}

vec4 bilerp(vec4 a, vec4 b, vec4 c, vec4 d, vec2 x)
{
    vec4 xy1 = (1.0 - x.x) * a + x.x * b;
    vec4 xy2 = (1.0 - x.x) * c + x.x * d;
    return (1.0 - x.y) * xy1 + x.y * xy2;
}

/// DEBUG

float draw_sphere(in vec2 uv, in vec2 center, in float radius, float resy)
{
    float d = distance(uv, center);
    float c = smoothstep(d, d + 1.5 / resy, radius);

    return c;
}

layout (std140, binding = 1) uniform parameters {
    int maxCascadeCount;
    int maxProbeCount;
    int minDiscreteValueCount;
    float minRadianceintervalLength;
    float lightIntensity;
    int maxRayIterationCount;
} paramsubo;

// cascade desc buffer
layout (std430, binding = 2) readonly buffer CascadeDescUBO {
    cascade_desc[] descs;
} cdubo;

// cascade probes position buffer
layout (std140, binding = 3) readonly buffer CascadeUBO {
    probe[] positions;
} cubo;

// radiance interval storage buffer
layout (std140, binding = 4) readonly buffer RadianceIntervalUBO {
    vec4[] intervals;
} riubo;

vec2 retrieve_probe_position(int cascadeIndex, int probeIndex)
{
    int offset = 0;
    for (int i = 0; i < cascadeIndex; ++i)
    {
        offset += cdubo.descs[i].p;
    }

    return cubo.positions[offset + probeIndex].position;
}

vec4 retrieve_radiance_interval(int cascadeIndex, int probeIndex, int intervalIndex)
{
    cascade_desc desc = cdubo.descs[cascadeIndex];
    int intervalCount = desc.q;

    int intervalIndexOffset = 0;
    for (int i = 0; i < cascadeIndex; ++i)
    {
        // get previous cascade (min index is 0)
        int previousCascadeIndex = max(0, i);
        cascade_desc previous_desc = cdubo.descs[previousCascadeIndex];

        intervalIndexOffset += previous_desc.p * previous_desc.q;
    }

    float intervalOffset = 0.0;
    if (cascadeIndex > 0)
        intervalOffset = pow(2.0, float(cascadeIndex - 1));
    intervalOffset *= float(paramsubo.minRadianceintervalLength);

    // index of interval is offsetted by the number of intervals before hand
    // the number of intervals per probes in the previous cascades
    // the strides are intervalCount length
    int intervalProbeIndex = intervalIndexOffset + probeIndex * intervalCount;
    int computedIntervalIndex = intervalProbeIndex + intervalIndex;

    return riubo.intervals[computedIntervalIndex];
}

int[4] get_surrounding_probe_indices_from_uv(vec2 uv, int cascadeIndex)
{
    cascade_desc desc = cdubo.descs[cascadeIndex];

    int probeCount = int(desc.p);
    int probeRowCount = int(sqrt(float(probeCount)));
    int intervalCount = int(desc.q);
    
    // closest probe at position "bottom left"
    float probeIndexOffset = 1.0/(float(probeRowCount)*2.0);

    // 4 probes in 2D
    // 8 probes in 3D

    // bottom left
    int p0Index = int((uv.y - probeIndexOffset) * float(probeRowCount)) + int((uv.x - probeIndexOffset) * float(probeRowCount)) * probeRowCount;
    // top left
    int p1Index = p0Index + 1;
    // bottom right
    int p2Index = p0Index + probeRowCount;
    // top right
    int p3Index = p0Index + probeRowCount + 1;

    return int[4](p0Index, p1Index, p2Index, p3Index);
}

vec4 radiance_apply(in vec2 uv)
{
    // all intervals squashed together
	vec4 totalRadiance = vec4(0.0);
    
    int lastCascadeIndex = paramsubo.maxCascadeCount - 1;
    cascade_desc lastDesc = cdubo.descs[lastCascadeIndex];
    
    // max number of interval
    // iterating on the intervals from probes of the last cascade
    for (int i = 0; i < lastDesc.q; ++i)
    {
        float transparency = 1.0;
        vec4 mergedRadiance = vec4(0.0);

        // number of cascades
        // merging intervals from different cascade
        for (int j = 0; j < paramsubo.maxCascadeCount; ++j)
        {
            // computing the right interval index in function of
            // the wanted final interval and the cascade index
            int intervalIndex = i / int(pow(2.0, float(paramsubo.maxCascadeCount- 1 - j)));

            int[4] probeIndices = get_surrounding_probe_indices_from_uv(uv, j);
            cascade_desc desc = cdubo.descs[j];
            
            probe p0, p1, p2, p3;
            p0.position = retrieve_probe_position(j, probeIndices[0]);
            p1.position = retrieve_probe_position(j, probeIndices[1]);
            p2.position = retrieve_probe_position(j, probeIndices[2]);
            p3.position = retrieve_probe_position(j, probeIndices[3]);

            // range of position between probes
            float xrange = p2.position.x - p0.position.x;
            float yrange = p1.position.y - p0.position.y;
            // linear interpolation between the probes (two values in 2D)
            // (three values in 3D)
            float lerpx = (uv.x - p0.position.x) / xrange;
            float lerpy = (uv.y - p0.position.y) / yrange;

            vec4 interval0 = retrieve_radiance_interval(j, probeIndices[0], intervalIndex);
            vec4 interval1 = retrieve_radiance_interval(j, probeIndices[1], intervalIndex);
            vec4 interval2 = retrieve_radiance_interval(j, probeIndices[2], intervalIndex);
            vec4 interval3 = retrieve_radiance_interval(j, probeIndices[3], intervalIndex);
            
            vec4 bilerped = bilerp(interval0, interval1, interval2, interval3, vec2(lerpy, lerpx));
            mergedRadiance += bilerped * transparency;
            
            // save current transparency for next cascade
            transparency *= bilerped.a;
        }
        totalRadiance += mergedRadiance;
    }
	return totalRadiance / float(lastDesc.q) * paramsubo.lightIntensity;
}

/// PROBE VISUALIZATION

void render_probes(inout vec4 col, in vec2 uv)
{
    for (int i = 0; i < paramsubo.maxCascadeCount; ++i)
    {
        float probeRadius = float(i + 1) / 100.0;

        cascade_desc desc = cdubo.descs[i];
        int probeCount = desc.p;
        int intervalCount = desc.q;
        
        int probeIndexOffset = 0;
        int intervalIndexOffset = 0;
        for (int j = 0; j < i; ++j)
        {
            // get previous cascade (min index is 0)
            int previousCascadeIndex = max(0, j);
            cascade_desc previous_desc = cdubo.descs[previousCascadeIndex];

            probeIndexOffset += previous_desc.p;
            intervalIndexOffset += previous_desc.p * previous_desc.q;
        }

        for (int j = 0; j < probeCount; ++j)
        {
            // index of probe is offsetted by the number of probes in the previous cascade
            int probeIndex = probeIndexOffset + j;

            vec2 probePos = cubo.positions[probeIndex].position;

            vec4 probeColor = vec4(0.0);
            for (int k = 0; k < intervalCount; ++k)
            {
                int intervalProbeIndex = intervalIndexOffset + j * intervalCount;
                int intervalIndex = intervalProbeIndex + k;
                probeColor += riubo.intervals[intervalIndex];
            }
            probeColor /= intervalCount;
            
            col += probeColor * draw_sphere(uv, probePos, probeRadius, textureSize(baseImage, 0).y);
        }
    }
}

void main()
{
    vec4 direct = texture(baseImage, fragUV);

    vec4 indirect = vec4(0.0);
    vec2 uv = fragUV;
    
#ifdef DEBUG_DISPLAY_PROBES
    // probes visualization
    vec4 probeSceneColor = vec4(0.0);
    render_probes(probeSceneColor, uv);
    indirect += vec4(probeSceneColor.xyz, 1.0);
#else
    indirect += vec4(0.0);
#endif
    
    // apply radiance to pixel
    vec4 indirectLight = radiance_apply(uv);
    indirect += vec4(indirectLight.rgb, 1.0);

    oColor = vec4(direct.rgb + mix(indirect.rgb, direct.rgb, step(0.5, direct.w)), 1.0);
}