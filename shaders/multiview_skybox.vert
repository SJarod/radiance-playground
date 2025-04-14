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

#define LOOK_AT_RIGHT_HANDED

mat4 lookAt(vec3 eye, vec3 target, vec3 up)
{
    vec3 forward = normalize(target - eye);

    vec3 zaxis = normalize(forward);
#if defined(LOOK_AT_RIGHT_HANDED)
    vec3 xaxis = normalize(cross(zaxis, up));
    vec3 yaxis = cross(xaxis, zaxis);
#else
    vec3 xaxis = normalize(cross(up, zaxis));
    vec3 yaxis = cross(zaxis, xaxis);
#endif
    mat4 lookAt4 = mat4(1.0);
    lookAt4[0][0] = xaxis.x;
	lookAt4[1][0] = xaxis.y;
	lookAt4[2][0] = xaxis.z;
	lookAt4[0][1] = yaxis.x;
	lookAt4[1][1] = yaxis.y;
	lookAt4[2][1] = yaxis.z;
#if defined(LOOK_AT_RIGHT_HANDED)
	lookAt4[0][2] =-zaxis.x;
	lookAt4[1][2] =-zaxis.y;
	lookAt4[2][2] =-zaxis.z;
    lookAt4[3][0] =-dot(xaxis, eye);
    lookAt4[3][1] =-dot(yaxis, eye);
    lookAt4[3][2] = dot(zaxis, eye);
#else
	lookAt4[0][2] = zaxis.x;
	lookAt4[1][2] = zaxis.y;
	lookAt4[2][2] = zaxis.z;
    lookAt4[3][0] =-dot(xaxis, eye);
    lookAt4[3][1] =-dot(yaxis, eye);
    lookAt4[3][2] =-dot(zaxis, eye);
#endif


    return lookAt4;
}

mat4 leftHandedViews[6] = {
								{
									{ 0.0, 0.0, 1.0, 0.0 },
									{ 0.0,-1.0, 0.0, 0.0 },
									{ 1.0, 0.0, 0.0, 0.0 },
									{ 0.0, 0.0, 0.0, 1.0 }
								},				  		 
								{				  		 
									{ 0.0, 0.0,-1.0, 0.0 },
									{ 0.0,-1.0, 0.0, 0.0 },
									{-1.0, 0.0, 0.0, 0.0 },
									{ 0.0, 0.0, 0.0, 1.0 }
								},				  
								{				  
									{-1.0, 0.0, 0.0, 0.0 },
									{ 0.0, 0.0, 1.0, 0.0 },
									{ 0.0, 1.0, 0.0, 0.0 },
									{ 0.0, 0.0, 0.0, 1.0 }
								},				  
								{				  
									{-1.0, 0.0, 0.0, 0.0 },
									{ 0.0, 0.0,-1.0, 0.0 },
									{ 0.0,-1.0, 0.0, 0.0 },
									{ 0.0, 0.0, 0.0, 1.0 }
								},				  
								{				  
									{-1.0, 0.0, 0.0, 0.0 },
									{ 0.0,-1.0, 0.0, 0.0 },
									{ 0.0, 0.0, 1.0, 0.0 },
									{ 0.0, 0.0, 0.0, 1.0 }
								},			 	  
								{			 	  
									{ 1.0, 0.0, 0.0, 0.0 },
									{ 0.0,-1.0, 0.0, 0.0 },
									{ 0.0, 0.0,-1.0, 0.0 },
									{ 0.0, 0.0, 0.0, 1.0 }
								}
							};

mat4 rightHandedViews[6] = {
								{
									{ 0.0, 0.0,-1.0, 0.0 },
									{ 0.0,-1.0,-0.0, 0.0 },
									{-1.0, 0.0,-0.0, 0.0 },
									{-0.0,-0.0, 0.0, 1.0 }
								},						 
								{						 
									{ 0.0, 0.0, 1.0, 0.0 },
									{ 0.0,-1.0,-0.0, 0.0 },
									{ 1.0, 0.0,-0.0, 0.0 },
									{-0.0,-0.0, 0.0, 1.0 }
								},						 
								{						 
									{ 1.0, 0.0,-0.0, 0.0 },
									{ 0.0, 0.0,-1.0, 0.0 },
									{ 0.0, 1.0,-0.0, 0.0 },
									{-0.0,-0.0, 0.0, 1.0 }
								},						 
								{						 
									{ 1.0, 0.0,-0.0, 0.0 },
									{ 0.0, 0.0, 1.0, 0.0 },
									{ 0.0,-1.0,-0.0, 0.0 },
									{-0.0,-0.0, 0.0, 1.0 }
								},						 
								{						 
									{ 1.0, 0.0,-0.0, 0.0 },
									{ 0.0,-1.0,-0.0, 0.0 },
									{-0.0, 0.0,-1.0, 0.0 }, 
									{-0.0,-0.0, 0.0, 1.0 }
								},						 
								{						 
									{-1.0, 0.0,-0.0, 0.0 },
									{-0.0,-1.0,-0.0, 0.0 },
									{-0.0, 0.0, 1.0, 0.0 },
									{ 0.0,-0.0, 0.0, 1.0 }
								}
							};

mat4 computedViews[6] = {
							lookAt(vec3(0.0, 0.0, 0.0), vec3(-1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0)),
							lookAt(vec3(0.0, 0.0, 0.0), vec3( 1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0)),
							lookAt(vec3(0.0, 0.0, 0.0), vec3( 0.0, 1.0, 0.0), vec3(0.0, 0.0,-1.0)),
							lookAt(vec3(0.0, 0.0, 0.0), vec3( 0.0,-1.0, 0.0), vec3(0.0, 0.0, 1.0)),
							lookAt(vec3(0.0, 0.0, 0.0), vec3( 0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0)),
							lookAt(vec3(0.0, 0.0, 0.0), vec3( 0.0, 0.0,-1.0), vec3(0.0, 1.0, 0.0))
						};

mat4 views[6] = computedViews;

void main()
{
    texCoords = aPos;
	fragPos = aPos;
	vec4 pos = mvp.proj * mat4(mat3(views[gl_ViewIndex])) * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}  
