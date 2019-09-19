//#version 310 es
precision highp float;

//From Application VIA
layout(location = 0) in vec3 a_Position;
layout(location = 3) in vec2 a_UV0;
layout(location = 4) in vec2 a_UV1;

//From Application SR
layout(std140, binding = 0) uniform CameraUB
{
	mat4 u_ProjectionMatrix;
	mat4 u_ViewMatrix;
	vec4 u_Orientation; //Quaternion
	vec3 u_Position;
	float _pad;
}cam;

/*layout(std140, binding = 1) uniform TransformUB
{
	mat4 u_ModelMatrix;
}model;*/

//To Fragment Varying
layout(location = 7)  out vec2 v_UV0;
layout(location = 8)  out vec2 v_UV1;
layout(location = 9)  out vec3 v_CameraPosition;

void main()
{
    //gl_Position = cam.u_ProjectionMatrix * cam.u_ViewMatrix * model.u_ModelMatrix * vec4(a_Position, 1.0);
    gl_Position = sm.ProjectionMatrix[VIEW_ID] * sm.ViewMatrix[VIEW_ID] * ModelMatrix * vec4(a_Position, 1.0);

    v_UV0		= a_UV0;
    v_UV0.y		= 1.0 - a_UV0.y;
    v_UV1		= a_UV1;
    v_UV1.y		= 1.0 - a_UV1.y;
    v_CameraPosition = cam.u_Position;
}