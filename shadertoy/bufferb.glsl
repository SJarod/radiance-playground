/// RADIANCE APPLY
/// INDIRECT LIGHTING

vec4 radiance_apply(in vec2 uv)
{
	vec4 totalRadiance = vec4(vec3(0.0), 1.0);
    for (int i = 0; i < MAX_CASCADE_COUNT; ++i)
    {
        vec4 desc = texelFetch(iChannel1, ivec2(0, i * 2), 0);
        int probeCount = int(desc.x);
        int probeRowCount = int(sqrt(float(probeCount)));
        int intervalCount = int(desc.y);
        
        // closest probe at position "bottom left"
        float probeIndexOffset = 1.0/(float(probeRowCount)*2.0);
        int p0Index = int((uv.y - probeIndexOffset) * float(probeRowCount)) + int((uv.x - probeIndexOffset) * float(probeRowCount)) * probeRowCount;
        int p1Index = p0Index + 1;
        int p2Index = p0Index + probeRowCount;
        int p3Index = p0Index + probeRowCount + 1;
        
        // 4 probes in 2D
        // 8 probes in 3D
        probe p0, p1, p2, p3;
        // bottom left
        p0.position = texelFetch(iChannel1, ivec2(p0Index, (i * 2) + 1), 0).xy;
        // top left
        p1.position = texelFetch(iChannel1, ivec2(p1Index, (i * 2) + 1), 0).xy;
        // bottom right
        p2.position = texelFetch(iChannel1, ivec2(p2Index, (i * 2) + 1), 0).xy;
        // top right
        p3.position = texelFetch(iChannel1, ivec2(p3Index, (i * 2) + 1), 0).xy;
        
        // range of position between probes
        float xrange = p2.position.x - p0.position.x;
        float yrange = p1.position.y - p0.position.y;
        // linear interpolation between the probes (two values in 2D)
        // (three values in 3D)
        float lerpx = (uv.x - p0.position.x) / xrange;
        float lerpy = (uv.y - p0.position.y) / yrange;
        
        vec4 mergedRadiance = vec4(vec3(0.0), 1.0);
        float transparency = 1.0;
        for (int j = 0; j < intervalCount; ++j)
        {
            int intervalIndex = p0Index*intervalCount + j;
            
            vec4 interval0 = texelFetch(iChannel2, ivec2(intervalIndex, i), 0);
            vec4 interval1 = texelFetch(iChannel2, ivec2(intervalIndex + intervalCount, i), 0);
            vec4 interval2 = texelFetch(iChannel2, ivec2(intervalIndex + probeRowCount * intervalCount, i), 0);
            vec4 interval3 = texelFetch(iChannel2, ivec2(intervalIndex + probeRowCount * intervalCount + intervalCount, i), 0);
            
            mergedRadiance += bilerp(interval0, interval1, interval2, interval3, vec2(lerpy, lerpx)) * transparency;
        }
        totalRadiance += mergedRadiance / float(intervalCount) * INTENSITY;
    }
	return totalRadiance * INV_MAX_CASCADE_COUNT;
}

/// PROBE VISUALIZATION

void render_probes(inout vec4 col, in vec2 uv)
{
    for (int i = 0; i < MAX_CASCADE_COUNT; ++i)
    {
        float probeRadius = float(i + 1) / 100.0;

        vec4 desc = texelFetch(iChannel1, ivec2(0, i * 2), 0);
        int probeCount = int(desc.x);
        int intervalCount = int(desc.y);
        
        for (int j = 0; j < probeCount; ++j)
        {
            vec2 probePos = texelFetch(iChannel1, ivec2(j, i * 2 + 1), 0).xy;
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