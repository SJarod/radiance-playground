
/// RADIANCE APPLY
/// INDIRECT LIGHTING

cascade_desc retrieve_cascade_desc(int cascadeIndex)
{
    vec4 d = texelFetch(iChannel0, ivec2(0, cascadeIndex * 2), 0);
    return cascade_desc(int(d.x), int(d.y), d.z);
}

vec2 retrieve_probe_position(int cascadeIndex, int probeIndex)
{
    // from buffer C
    return texelFetch(iChannel0, ivec2(probeIndex, (cascadeIndex * 2) + 1), 0).xy;
}

vec4 retrieve_radiance_interval(int cascadeIndex, int probeIndex, int intervalIndex, int intervalCount)
{
    // from buffer D
    return texelFetch(iChannel1, ivec2(probeIndex * intervalCount + intervalIndex, cascadeIndex), 0);
}

int[4] get_surrounding_probe_indices_from_uv(vec2 uv, int cascadeIndex)
{
    cascade_desc desc = retrieve_cascade_desc(cascadeIndex);

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
    
    int lastCascadeIndex = MAX_CASCADE_COUNT - 1;
    cascade_desc lastDesc = retrieve_cascade_desc(lastCascadeIndex);
    
    // max number of interval
    // iterating on the intervals from probes of the last cascade
    for (int i = 0; i < lastDesc.q; ++i)
    {
        float transparency = 1.0;
        vec4 mergedRadiance = vec4(0.0);

        // number of cascades
        // merging intervals from different cascade
        for (int j = 0; j < MAX_CASCADE_COUNT; ++j)
        {
            // computing the right interval index in function of
            // the wanted final interval and the cascade index
            int intervalIndex = i / (1 << (MAX_CASCADE_COUNT - 1 - j));

            int[4] probeIndices = get_surrounding_probe_indices_from_uv(uv, j);
            cascade_desc desc = retrieve_cascade_desc(j);
            
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

            vec4 interval0 = retrieve_radiance_interval(j, probeIndices[0], intervalIndex, desc.q);
            vec4 interval1 = retrieve_radiance_interval(j, probeIndices[1], intervalIndex, desc.q);
            vec4 interval2 = retrieve_radiance_interval(j, probeIndices[2], intervalIndex, desc.q);
            vec4 interval3 = retrieve_radiance_interval(j, probeIndices[3], intervalIndex, desc.q);
            
            vec4 bilerped = bilerp(interval0, interval1, interval2, interval3, vec2(lerpy, lerpx));
            mergedRadiance += bilerped * transparency;
            
            // save current transparency for next cascade
            transparency *= bilerped.a;
        }
        totalRadiance += mergedRadiance;
    }
	return totalRadiance / float(lastDesc.q) * INTENSITY;
}

/// PROBE VISUALIZATION

void render_probes(inout vec4 col, in vec2 uv)
{
    for (int i = 0; i < MAX_CASCADE_COUNT; ++i)
    {
        float probeRadius = float(i + 1) / 100.0;

        vec4 desc = texelFetch(iChannel0, ivec2(0, i * 2), 0);
        int probeCount = int(desc.x);
        int intervalCount = int(desc.y);
        
        for (int j = 0; j < probeCount; ++j)
        {
            vec2 probePos = texelFetch(iChannel0, ivec2(j, i * 2 + 1), 0).xy;
            vec4 probeColor = vec4(0.0);
            probeColor.r = length(probePos);
            probeColor.g = float(j + 1) / float(MAX_PROBE_COUNT + 1);
            probeColor.b = float(i + 1) / float(MAX_CASCADE_COUNT + 1);
            probeColor.a = 1.0;
            
            col += probeColor * draw_sphere(uv, probePos, probeRadius, iResolution.y);
        }
    }
}

/// INDIRECT LIGHTING

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.xy;
    
#ifdef DEBUG_DISPLAY_PROBES
    // probes visualization
    vec4 probeSceneColor = vec4(0.0);
    render_probes(probeSceneColor, uv);
    fragColor = vec4(probeSceneColor.xyz, 1.0);
#else
    fragColor = vec4(0.0);
#endif
    
    // apply radiance to pixel
    vec4 indirectLight = radiance_apply(uv);
    fragColor += vec4(indirectLight.rgb, 1.0);
}