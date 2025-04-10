
/// CASCADES
/// PROBES SCENE

cascade create_cascade(in cascade_desc cd)
{
	cascade result;
	result.desc = cd;

	// probe count per dimension
	float px = sqrt(float(cd.p));
	float py = px;
    float ipx = 1.0 / px;
    float ipy = 1.0 / py;
    
    // center the probes
    vec2 offset = vec2(ipx, ipy) * -0.5;
    
	// probes within  the cascade
	for (float i = 0.0; i < px; ++i)
	{
		for (float j = 0.0; j < py; ++j)
		{
			// initialize probe posititon
            int probeIndex = int(px * i + j);
			result.probes[probeIndex].position.x = ipx * float(i) - offset.x;
			result.probes[probeIndex].position.y = ipy * float(j) - offset.y;
		}
	}

	// radiance interval count
	result.m = cd.p * cd.q;
    
	return result;
}

cascade[MAX_CASCADE_COUNT] create_cascades(in cascade_desc desc0, in int cascadeCount)
{
	cascade result[MAX_CASCADE_COUNT];

	for (int i = 0; i < cascadeCount; ++i)
	{
		result[i] = create_cascade(desc0);
		// P1 = P0/4
		desc0.p /= 4;
		// Q1 = 2Q0
		desc0.q *= 2;
		// DW1 = 2DW0
		desc0.dw *= 2.0;
	}

	return result;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    fragColor = vec4(0.0);
    
    cascade_desc cd0 = cascade_desc(MAX_PROBE_COUNT, MIN_DISCRETE_VALUE_COUNT, MIN_RADIANCE_INTERVAL_LENGTH);
    
    // create cascades
    cascade cascades[MAX_CASCADE_COUNT] = create_cascades(cd0, MAX_CASCADE_COUNT);
    
    // even rows are for cascade desc
    if (int(fragCoord.y) % 2 == 0)
    {
        int cascadeDescIndex = int(fragCoord.y) / 2;
        if (cascadeDescIndex >= MAX_CASCADE_COUNT || int(fragCoord.x) > 0)
        {
            fragColor = vec4(0.0);
            return;
        }
        // probe count
        fragColor.x = float(cascades[cascadeDescIndex].desc.p);
        // interval count
        fragColor.y = float(cascades[cascadeDescIndex].desc.q);
        // interval length
        fragColor.z = cascades[cascadeDescIndex].desc.dw;
    }
    // odd rows are raw probe info (position and interval count)
    else
    {
        // jump a row to encode the cascade descriptor
        // (take every 2 rows)
        int cascadeIndex = int(fragCoord.y) / 2;
        if (cascadeIndex >= MAX_CASCADE_COUNT)
        {
            fragColor = vec4(0.0);
            return;
        }
            
        int probeIndex = int(fragCoord.x);
        if (probeIndex >= cascades[cascadeIndex].desc.p)
        {
            fragColor = vec4(0.0);
            return;
        }
        
        // encode the cascade position and radiance interval count
        // xyz = position
        // w = interval count (does not work due to Shadertoy force alpha channel to be 1)
        fragColor = vec4(vec3(cascades[cascadeIndex].probes[probeIndex].position, 0.0), 4.0);
    }
}