//#version 310 es
precision highp float;

//From Application VIA
layout(location = 0) in vec3 a_Position;

//From Application SR
/*layout(std140, binding = 0) uniform CameraUB
{
	mat4 u_ProjectionMatrix;
	mat4 u_ViewMatrix;
	vec4 u_Orientation; //Quaternion
	vec3 u_Position;
	float _pad;
}cam;

layout(std140, binding = 1) uniform TransformUB
{
	mat4 u_ModelMatrix;
}model;*/

void main()
{
    gl_Position = sm.ProjectionMatrix[VIEW_ID] * sm.ViewMatrix[VIEW_ID] * ModelMatrix * vec4(a_Position, 1.0) ;
}