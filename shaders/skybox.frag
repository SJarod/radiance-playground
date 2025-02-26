#version 450

layout(location = 1) in vec3 texCoords;

layout(location = 0) out vec4 oColor;

layout(binding = 1) uniform samplerCube skybox;

void main()
{    
    oColor = texture(skybox, texCoords);
}
