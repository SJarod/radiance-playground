#version 450

layout(location = 0) in vec3 aPos;

layout(location = 1) out vec3 texCoords;

layout(binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} mvp;

void main()
{
    texCoords = aPos;
	vec4 pos = mvp.proj * mvp.view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}  
