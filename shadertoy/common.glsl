
/// DIRECT LIGHTING

struct Light
{
	vec2 position;
    float radius;
    vec4 color;
};

#define LIGHT_COUNT 4
Light lights[LIGHT_COUNT] = Light[LIGHT_COUNT](
	Light(vec2(1.0, 0.6), 0.1, vec4(1.0, 0.0, 0.0, 1.0)),
	Light(vec2(0.3, 0.4), 0.1, vec4(0.0, 1.0, 0.0, 1.0)),
	Light(vec2(0.8, 0.5), 0.05, vec4(0.0, 0.0, 1.0, 1.0)),
    Light(vec2(1.4, 0.2), 0.2, vec4(1.0, 0.0, 1.0, 1.0))
);

/// RADIANCE CASCADES

#define MAX_CASCADE_COUNT 3
// p = probe count is a square number
const int MAX_PROBE_COUNT = int(pow(2.0, 8.0));
// q = discrete value count will be doubled every cascade
#define MIN_DISCRETE_VALUE_COUNT 8
// dw = radiance interval length will be doubled every cascade
const float MIN_RADIANCE_INTERVAL_LENGTH = 20.0 / float(MAX_PROBE_COUNT);

// intensity of every lights (when applying irradiance)
#define INTENSITY 1.0

#define DEBUG_DISPLAY_PROBES

// iteration along the ray for the raycasting function
// it is for now uniformly distributed (it may not be precise)
#define MAX_RAY_ITERATION_COUNT 16

#define M_PI 3.1415926535897932384626433832795

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

struct cascade
{
	cascade_desc desc;

    probe probes[MAX_PROBE_COUNT];

	// total of radiance intervals in the cascade
	// m = pq
	int m;
};


vec2 get_direction_from_angle(float w)
{
	return normalize(vec2(cos(w), sin(w)));
}

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