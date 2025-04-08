#version 450

layout(location = 0) in vec3 fragPos;

layout(location = 0) out vec4 oColor;

layout(binding = 1) uniform samplerCube environmentMap;

const float PI = 3.14159265359;
const float TWO_PI = 2.0 * PI;
const float HALF_PI = 0.5 * PI;

void main()
{		
    vec3 normal = normalize(fragPos);

    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(worldUp, normal));
    vec3 up = normalize(cross(normal, right));
       
    const float sampleDelta = 0.025;
    float nrSamples = 0.0;

    vec3 irradiance = vec3(0.0);
    for(float phi = 0.0; phi < TWO_PI; phi += sampleDelta)
    {
        const float sinPhi = sin(phi);
        const float cosPhi = cos(phi);
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            const float sinTheta = sin(theta);
            const float cosTheta = cos(theta);
            vec3 tangentSample = vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);

            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 

            irradiance += texture(environmentMap, sampleVec).rgb * cosTheta * sinTheta;
            nrSamples++;
        }
    }

    irradiance = PI * irradiance / float(nrSamples);
    
    oColor = vec4(irradiance, 1.0);
}