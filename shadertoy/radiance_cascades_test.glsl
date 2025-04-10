// radiance cascades

// ref 0 : paper : https://drive.google.com/file/d/1L6v1_7HY2X-LV3Ofb6oyTIxgEaP4LOI6/
// ref 1 : https://tmpvar.com/poc/radiance-cascades/
// ref 2 : https://radiance-cascades.com/

// p = probe count is a square number
#define MAX_PROBE_COUNT 16
// q = discrete value count will be doubled every cascade
#define MIN_DISCRETE_VALUE_COUNT 4
// dw = radiance interval length will be doubled every cascade
#define MIN_RADIANCE_INTERVAL_LENGTH 1.0
#define MAX_CASCADE_COUNT 1

// iteration along the ray for the raycasting function
// it is for now uniformly distributed (it may not be precise)
#define MAX_RAY_ITERATION_COUNT 128

// multiply by two when adding a cascade
#define MAX_DISCRETE_VALUE_ARRAY 4

#define M_PI 3.1415926535897932384626433832795

/// DEBUG

float draw_sphere(in vec2 uv, in vec2 center, in float radius)
{
    float d = distance(uv, center);
    float c = smoothstep(d, d+0.005, radius);

    return c;
}

/// DIRECT LIGHTING

struct Light
{
	vec2 position;
    vec4 color;
};

#define LIGHT_COUNT 2
Light lights[LIGHT_COUNT] = Light[LIGHT_COUNT](
	Light(vec2(0.25, 0.2), vec4(1.0, 0.0, 0.0, 1.0)),
	Light(vec2(0.75, 1.0), vec4(0.0, 1.0, 0.0, 1.0))
	//Light(vec2(-0.6, -0.3), vec4(vec3(1.0), 1.0))
);

vec4 compute_direct_light(in vec2 fragPos)
{
	float lightRadius = 0.08;
	
    vec4 sceneColor = vec4(0.0);
    for (int i = 0; i < int(LIGHT_COUNT); ++i)
    {
		sceneColor += lights[i].color * draw_sphere(fragPos, lights[i].position, lightRadius);
    }
    return sceneColor;
}

/// RADIANCE CASCADES

struct probe
{
	vec2 position;
	// values of radiance intervals (incoming radiance from a direction)
	// the more the values, the more the directions to compute
    vec4 radianceIntervals[MAX_DISCRETE_VALUE_ARRAY];
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
struct cascade
{
	cascade_desc desc;

	probe probes[MAX_PROBE_COUNT];

	// total of radiance intervals
	// m = pq
	int m;
};

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

// raycasting to detect incoming radiance to a point p (and detect transparency)
// R(p, w)
vec4 raycasting_function(vec2 p, vec2 dir, float len)
{
	float stepSize = len / float(MAX_RAY_ITERATION_COUNT);
	vec2 sampledStep = p + dir * stepSize;
    
	vec4 incomingRadiance = vec4(0.0);
	for (int i = 0; i < int(MAX_RAY_ITERATION_COUNT); ++i)
	{
		// incoming radiance (direct lighting)
		// in flatland, the incoming radiance is the color of the render texture
		// in 3D it would be the lighting value of the hit surface
		// the raycasting function may use a quadtree or octree to
		// process through the ray in a scene independent manner
		vec4 directLighting = compute_direct_light(sampledStep);
		// directLighting.a == 1.0 : opaque (raycast hit)
		// directLighting.a == 0.0 : transparent (raycast miss)
		float hit = step(0.5, directLighting.w);
		incomingRadiance = directLighting * hit;
            
        sampledStep += dir * stepSize * (1.0 - hit);
	}

    return incomingRadiance;
}

// compute radiance interval for a given ray
// La,b(p, w)
vec4 radiance_interval(vec2 p, vec2 dir)
{
	return vec4(0.0);
}

// B(p, w)a,b
vec4 transparency_interval(vec2 p, vec2 dir)
{
	return vec4(0.0);
}

vec2 get_direction_from_angle(float w)
{
	return normalize(vec2(cos(w), sin(w)));
}

void probe_encode_radiance(inout probe p, in cascade_desc cd, in float intervalOffset)
{
	// 1D direction (angle)
	// in 3D it would be a 2D direction (longitude and latitude)
	float angle = 2.0 * float(M_PI) / float(cd.q);
	float w = angle / 2.0;
	for (int i = 0; i < cd.q; ++i)
	{
		vec2 dir = get_direction_from_angle(w);
		p.radianceIntervals[i] = raycasting_function(p.position + dir * intervalOffset, dir, cd.dw);
		w += angle;
	}
}

void radiance_gather(inout cascade cascades[MAX_CASCADE_COUNT])
{
    for (int i = 0; i < MAX_CASCADE_COUNT; ++i)
    {
		float intervalOffset = 0.0;
        for (int j = 0; j < cascades[i].desc.p; ++j)
        {
            probe_encode_radiance(cascades[i].probes[j], cascades[i].desc, intervalOffset);
        }
		intervalOffset = cascades[i].desc.dw;
    }
}

probe get_closest_probe_from_pos(in cascade c, in vec2 pos)
{
	float px = sqrt(float(c.desc.p));
	int index = int(px * pos.x + pos.y);
	return c.probes[index];
}

probe[4] get_surrounding_probes(in cascade c, in vec2 pos)
{
	probe result[4];
	for (int i = 0; i < 4; ++i)
	{
		
	}
	return result;
}

vec4 radiance_apply_forward(in cascade cascades[MAX_CASCADE_COUNT], in vec2 uv)
{
	vec4 mergedRadiance = vec4(vec3(0.0), 1.0);
    for (int i = 0; i < MAX_CASCADE_COUNT; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
			float d = distance(uv, cascades[i].probes[j].position);
			float weight = 1.0 - clamp(d, 0.0, 1.0);
			for (int k = 0; k < cascades[i].desc.q; ++k)
			{
				vec4 interval = cascades[i].probes[j].radianceIntervals[k];
				mergedRadiance += weight * interval;
			}
        }
    }
	return mergedRadiance;
}

// experimental
vec4 radiance_apply_backward(in cascade cascades[MAX_CASCADE_COUNT], in vec2 uv)
{
	vec4 mergedRadiance = vec4(vec3(0.0), 1.0);
    for (int i = MAX_CASCADE_COUNT - 1; i >= 0; --i)
    {
		for (int k = 0; k < cascades[i].desc.q; ++k)
        {
			float transparency = 1.0;
			for (int j = 0; j < cascades[i].desc.p; ++j)
			{
				float d = distance(uv, cascades[i].probes[j].position);
				float weight = 1.0 - clamp(d, 0.0, 1.0);
				vec4 interval = cascades[i].probes[j].radianceIntervals[k];

				transparency = interval.w;
				mergedRadiance += weight * interval * transparency;
			}
        }
    }
	return mergedRadiance;
}

vec4 radiance_apply(in cascade cascades[MAX_CASCADE_COUNT], in vec2 uv)
{
	return radiance_apply_forward(cascades, uv);
}

/// VISUALIZATION

void render_probes(inout vec4 col, in vec2 uv, in cascade_desc cd, in cascade c)
{
    vec4 probeColor = vec4(0.0, 0.0, 1.0, 1.0);
    float probeRadius = 0.01;
	
	//col += probeColor * draw_sphere(uv, c.probes[int(uv.x)/cd.p + int(uv.y) * cd.p].position, probeRadius);
    for (int i = 0; i < cd.p; ++i)
    {
        col += probeColor * draw_sphere(uv, c.probes[i].position, probeRadius);
    }
}

cascade_desc cd0 = cascade_desc(MAX_PROBE_COUNT, MIN_DISCRETE_VALUE_COUNT, MIN_RADIANCE_INTERVAL_LENGTH);

/// MAIN

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 uv = fragCoord/iResolution.xy;
    
    // create cascades
    cascade cascades[MAX_CASCADE_COUNT] = create_cascades(cd0, MAX_CASCADE_COUNT);
    
    // probes visualization
    vec4 probeSceneColor = vec4(0.0);
    render_probes(probeSceneColor, uv, cascades[0].desc, cascades[0]);
    
    // probes radiance gather
    radiance_gather(cascades);
    
    // apply radiance to pixel
    vec4 indirectLight = radiance_apply(cascades, uv);
    
    vec4 directLight = compute_direct_light(uv);
    vec4 combined = mix(indirectLight + probeSceneColor, directLight, step(0.5, directLight.w));
    
    // final color
    fragColor = vec4(combined.xyz, 1.0);
}