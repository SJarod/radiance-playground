#version 450

layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D baseImage;

// radiance cascades

// ref 0 : paper : https://drive.google.com/file/d/1L6v1_7HY2X-LV3Ofb6oyTIxgEaP4LOI6/
// ref 1 : https://tmpvar.com/poc/radiance-cascades/
// ref 2 : https://radiance-cascades.com/

// p = probe count is a square number
#define MAX_PROBE_COUNT 64
// q = discrete value count will be doubled every cascade
#define MIN_DISCRETE_VALUE_COUNT 4
// dw = radiance interval length will be doubled every cascade
#define MIN_RADIANCE_INTERVAL_LENGTH 0.2
#define MAX_CASCADE_COUNT 4

// iteration along the ray for the raycasting function
#define MAX_RAY_ITERATION_COUNT 4

// multiply by two when adding a cascade
#define MAX_DISCRETE_VALUE_ARRAY 4 * 2 * 2 * 2

#define M_PI 3.1415926535897932384626433832795

/// DEBUG

float draw_sphere(in vec2 uv, in vec2 center, in float radius)
{
    float d = distance(uv, center);
    float c = step(d, radius);

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
	Light(vec2(0.4, 0.7), vec4(1.0, 0.0, 0.0, 1.0)),
	Light(vec2(0.7, 0.1), vec4(0.0, 1.0, 0.0, 1.0))
	//Light(vec2(0.6, 0.5), vec4(0.0))
);

vec4 compute_direct_light(in vec2 fragPos)
{
	return texture(baseImage, fragPos);
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
    
	// probes within  the cascade
	for (float i = 0.0; i < px; ++i)
	{
		for (float j = 0.0; j < py; ++j)
		{
			// initialize probe posititon
            int probeIndex = int(px * i + j);
			result.probes[probeIndex].position.x = ipx * float(i) + ipx * 0.5;
			result.probes[probeIndex].position.y = ipy * float(j) + ipy * 0.5;
			
			// initialize probe radiance intervals to 0
			for (int k = 0; k < cd.q; ++k)
			{
				result.probes[probeIndex].radianceIntervals[k] = vec4(0.0);
			}
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
    
	// incomingRadiance.a == 0.0 : opaque
	// incomingRadiance.a == 1.0 : transparent
	vec4 incomingRadiance = vec4(vec3(0.0), 1.0);
	for (int i = 0; i < int(MAX_RAY_ITERATION_COUNT); ++i)
	{
		// incoming radiance (direct lighting)
		// in flatland, the incoming radiance is the color of the render texture
		// in 3D it would be the lighting value of the hit surface
		// the raycasting function may use a quadtree or octree to
		// process through the ray in a scene independent manner
		incomingRadiance += compute_direct_light(sampledStep);
            
        sampledStep += dir * stepSize;
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

void probe_encode_radiance(inout probe p, in cascade_desc cd)
{
	// 1D direction (angle)
	// in 3D it would be a 2D direction (longitude and latitude)
	float angle = 2.0 * float(M_PI) / float(cd.q);
	float w = angle / 2.0;
	for (int i = 0; i < cd.q; ++i)
	{
		p.radianceIntervals[i] = raycasting_function(p.position, get_direction_from_angle(w), cd.dw);
		w += angle;
	}
}

void radiance_gather(inout cascade cascades[MAX_CASCADE_COUNT])
{
    for (int i = 0; i < MAX_CASCADE_COUNT; ++i)
    {
        for (int j = 0; j < cascades[i].desc.p; ++j)
        {
            probe_encode_radiance(cascades[i].probes[j], cascades[i].desc);
        }
    }
}

vec4 radiance_apply(in cascade cascades[MAX_CASCADE_COUNT], in vec2 uv)
{
	vec4 mergedRadiance = vec4(vec3(0.0), 1.0);
    for (int i = 0; i < MAX_CASCADE_COUNT; ++i)
    {
        for (int j = 0; j < cascades[i].desc.p; ++j)
        {
			for (int k = 0; k < cascades[i].desc.q; ++k)
			{
				float d = distance(uv, cascades[i].probes[j].position);
				float weight = 1.0 - clamp(d, 0.0, 1.0);
				vec4 interval = cascades[i].probes[j].radianceIntervals[k];
				mergedRadiance += weight * interval;
			}
        }
    }
	return mergedRadiance;
}

/// VISUALIZATION

void render_probes(inout vec4 col, in vec2 uv, in cascade_desc cd, in cascade c)
{
    vec4 probeColor = vec4(0.0, 0.0, 1.0, 1.0);
    float probeRadius = 0.01;

	for (int i = 0; i < cd.p; ++i)
	{		
		col += probeColor * draw_sphere(uv, c.probes[i].position, probeRadius);
	}
}

cascade_desc cd0 = cascade_desc(MAX_PROBE_COUNT, MIN_DISCRETE_VALUE_COUNT, MIN_RADIANCE_INTERVAL_LENGTH);

/// MAIN

void main()
{
return;
	vec3 col = texture(baseImage, fragUV).rgb;

	// create cascades
	cascade cascades[MAX_CASCADE_COUNT] = create_cascades(cd0, MAX_CASCADE_COUNT);

	// probes visualization
	vec4 probeSceneColor = vec4(0.0);
	render_probes(probeSceneColor, fragUV, cascades[0].desc, cascades[0]);
    
    // probes radiance gather
    radiance_gather(cascades);
    
    // apply radiance to pixel
    vec4 indirectLight = radiance_apply(cascades, fragUV);
    
    // final color
    oColor = vec4(vec3(compute_direct_light(fragUV).xyz + indirectLight.xyz + probeSceneColor.xyz), 1.0);
}