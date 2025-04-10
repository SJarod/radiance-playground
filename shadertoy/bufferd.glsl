
/// RADIANCE GATHER
/// PROBE ENCODE RADIANCE

// raycasting to detect incoming radiance to a point p (and detect transparency)
// R(p, w)
vec4 raycasting_function(vec2 p, vec2 dir, float len)
{
	float stepSize = len / float(MAX_RAY_ITERATION_COUNT);
	vec2 sampledStep = p;
    
	vec4 incomingRadiance = vec4(0.0);
	for (int i = 0; i < MAX_RAY_ITERATION_COUNT + 1; ++i)
	{
		// incoming radiance (direct lighting)
		// in flatland, the incoming radiance is the color of the render texture
		// in 3D it would be the lighting value of the hit surface
		// the raycasting function may use a quadtree or octree to
		// process through the ray in a scene independent manner
		vec4 directLighting = texture(iChannel0, sampledStep);
		// directLighting.a == 1.0 : opaque (raycast hit)
		// directLighting.a == 0.0 : transparent (raycast miss)
        float hit = step(0.5, directLighting.w);
        
		incomingRadiance = directLighting * hit;
        // radiance.a == 1.0 : transparent (transparency value)
        // radiance.a == 0.0 : opaque
        incomingRadiance.a = 1.0 - hit;
        
        sampledStep += dir * stepSize * (1.0 - hit); // only move step if hit failed
	}
    return incomingRadiance;
}

vec4 probe_encode_radiance(inout probe p, in int intervalIndex, in int intervalCount, in float intervalLength, in float intervalOffset)
{
	// 1D direction (angle)
	// in 3D it would be a 2D direction (longitude and latitude)
	float angle = 2.0 * float(M_PI) / float(intervalCount);
	float w = angle / 2.0 + angle * float(intervalIndex);

	vec2 dir = get_direction_from_angle(w);
    
    // radiance interval
    vec4 interval = raycasting_function(p.position + dir * intervalOffset, dir, intervalLength);
    // debug : show blue value for interval index
    //interval.b = float(intervalIndex) / float(intervalCount);
    return interval;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // radiance gather
    int cascadeIndex = int(fragCoord.y);

    // every rows are probes from a cascade
	if (cascadeIndex >= MAX_CASCADE_COUNT)
	{
		fragColor = vec4(0.0);
		return;
	}
    int cascadeDescIndex = cascadeIndex * 2;
    vec4 desc = texelFetch(iChannel1, ivec2(0, cascadeDescIndex), 0);

	int probeCount = int(desc.x);
    int intervalCount = int(desc.y);
    // each column is a probe's radiance interval
    // i00 i01 i02 i03 i10 i11 i12 i13
    int probeIndex = int(fragCoord.x) / intervalCount;
	if (probeIndex >= probeCount)
	{
		fragColor = vec4(0.0);
		return;
	}

	float intervalLength = desc.z;
	int intervalIndex = int(fragCoord.x) % intervalCount;
    
    float intervalOffset = 0.0;
    if (cascadeIndex > 0)
        intervalOffset = pow(2.0, float(cascadeIndex - 1));
    intervalOffset *= float(MIN_RADIANCE_INTERVAL_LENGTH);
    
    int cascadeDataIndex = cascadeDescIndex + 1;
	probe p;
	p.position = texelFetch(iChannel1, ivec2(probeIndex, cascadeDataIndex), 0).xy;
	vec4 radianceInterval = probe_encode_radiance(p, intervalIndex, intervalCount, intervalLength, intervalOffset);

    fragColor = radianceInterval;
}