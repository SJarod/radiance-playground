#version 450

#extension GL_EXT_multiview : enable

layout(location = 0) in vec3 aPos;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 texCoords;

layout(binding = 0) uniform MVPUniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
} mvp;

mat4 views[6] = {
					{
						{ 0.0, 0.0,-1.0, 0.0},
						{ 0.0,-1.0,-0.0, 0.0},
						{-1.0, 0.0,-0.0, 0.0},
						{-0.0,-0.0, 0.0, 1.0}
					},
					{
						{ 0.0, 0.0, 1.0, 0.0},
						{ 0.0,-1.0,-0.0, 0.0},
						{ 1.0, 0.0,-0.0, 0.0},
						{-0.0,-0.0, 0.0, 1.0}
					},
					{
						{ 1.0, 0.0,-0.0, 0.0},
						{ 0.0, 0.0,-1.0, 0.0},
						{ 0.0, 1.0,-0.0, 0.0},
						{-0.0,-0.0, 0.0, 1.0}
					},
					{
						{ 1.0, 0.0,-0.0, 0.0},
						{ 0.0, 0.0, 1.0, 0.0},
						{ 0.0,-1.0,-0.0, 0.0},
						{-0.0,-0.0, 0.0, 1.0}
					},
					{
						{ 1.0, 0.0,-0.0, 0.0},
						{ 0.0,-1.0,-0.0, 0.0},
						{-0.0, 0.0,-1.0, 0.0}, 
						{-0.0,-0.0, 0.0, 1.0}
					},
					{
						{-1.0, 0.0,-0.0, 0.0},
						{-0.0,-1.0,-0.0, 0.0},
						{-0.0, 0.0, 1.0, 0.0},
						{ 0.0,-0.0, 0.0, 1.0}
					}
				};

void main()
{
    texCoords = aPos;
	fragPos = aPos;
	vec4 pos = mvp.proj * mat4(mat3(views[gl_ViewIndex])) * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}  
