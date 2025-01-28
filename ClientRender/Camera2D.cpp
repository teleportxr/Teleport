#include "Platform/Core/RuntimeError.h"
#include "Platform/Math/Quaternion.h"
#include "Platform/Math/Matrix4x4.h"
#include "Platform/Math/MatrixVector3.h"
#include "Platform/CrossPlatform/BaseRenderer.h"
#include "Platform/Math/Pi.h"
#include "Camera2D.h"
#include <memory.h>
#include <algorithm>
using namespace platform;
using namespace math;
using namespace crossplatform;
using namespace teleport;
using namespace clientrender;

static const float DEG_TO_RAD=SIMUL_PI_F/180.f;
static const float RAD_TO_DEG=180.f/SIMUL_PI_F;

static const float x_sgn = +1.f; //Was -1.f
static const float y_sgn = +1.f; //Was -1.f


static math::Matrix4x4 MakeOrthoProjectionMatrix(float left,
 	float right,
 	float bottom,
 	float top,
 	float nearVal,
 	float farVal)
{
	math::Matrix4x4 m;
	m.ResetToUnitMatrix();
	float rl=right-left;
	float tb=top-bottom;
	float fn=farVal-nearVal;
	m.m00=2.0f/(rl);
	m.m11=2.0f/(tb);
	m.m22=-2.0f/(fn);
	m.m03=(right+left)/(rl);
	m.m13=(top+bottom)/(tb);
	m.m23=(farVal+nearVal)/(fn);
	m.m33=1.f;
	return m;
}

Camera2D::Camera2D():Orientation()
{
	VerticalFieldOfViewInRadians=60.f*SIMUL_PI_F/180.f;
    HorizontalFieldOfViewInRadians = 90.f * SIMUL_PI_F / 180.f;
	Orientation.Rotate(3.14f/2.f,platform::math::Vector3(1,0,0));
}

Camera2D::~Camera2D()
{
}

const float *Camera2D::MakeViewMatrix() const
{
	return Orientation.GetInverseMatrix().RowPointer(0);
}

// Really making projT here.
const float *Camera2D::MakeDepthReversedProjectionMatrix(float h,float v,float zNear,float zFar)
{
	float xScale= 1.f/tan(h/2.f);
	float yScale = 1.f/tan(v/2.f);
	/// TODO: Not thread-safe
	static math::Matrix4x4 m;
	memset(&m,0,16*sizeof(float));
	m._11	=xScale;
	m._22	=yScale;
	if(zFar>0)
	{
		m._33	=zNear/(zFar-zNear);	m._34	=-1.f;
		m._43	=zFar*zNear/(zFar-zNear);
		// i.e. z=((n/(f-n)*d+nf/(f-n))/(-d)
		//		 =(n+nf/d)/(f-n)
		//		 =n(1+f/d)/(f-n)
		// so at d=-f, z=0
		// and at d=-n, z=(n-f)/(f-n)=-1

		// suppose we say:
		//			-Z=(n-nf/d)/(f-n)
		// i.e.      Z=n(f/d-1)/(f-n)

		// then we would have:
		//			at d=n, Z	=n(f/n-1)/(f-n)
		//						=(f-n)/(f-n)=1.0
		// and at      d=f, Z	=n(f/f-1)/(f-n)
		//						=0.
	}
	else // infinite far plane.
	{
		m._33	=0.f;					m._34	=-1.f;
		m._43	=zNear;
		// z = (33 Z + 43) / (34 Z +44)
		//   = near/-Z
		// z = -n/Z
		// But if z = (33 Z + 34) / (43 Z +44)
		// z = -1/nZ
	}
	// testing:
	//GetFrustumFromProjectionMatrix(m);
	return m;
}

const float *Camera2D::MakeDepthReversedProjectionMatrix(const FovPort &fovPort, float zNear, float zFar)
{
	float xScale	= 2.0f / (fovPort.leftTan + fovPort.rightTan);
	float xOffset	 = (fovPort.leftTan - fovPort.rightTan) * xScale * 0.5f;
	float yScale	= 2.0f / (fovPort.upTan + fovPort.downTan);
	float yOffset	= (fovPort.upTan - fovPort.downTan) * yScale * 0.5f;

	static float handedness = 1.0f;// right-handed obviously.
	/// TODO: Not thread-safe
	static math::Matrix4x4 m;
	memset(&m, 0, 16 * sizeof(float));
	m._11 = xScale;
	m._22 = yScale;
	m._31 = -handedness*xOffset;
	m._32 = handedness *yOffset;
	if (zFar>0)
	{
		m._33 = zNear / (zFar - zNear);	m._34 = -1.f;
		m._43 = zFar*zNear / (zFar - zNear);
		// z = (33 Z + 43) / (34 Z +44)
		//  z = (N/(F-N)*Z+F*N/(F-N))/(-Z)
		// if Z=N then z = (N/(F-N)*N+F*N/(F-N))/(-N)
		//               = (N*N+F*N)/(-N(F-N))
		//				= (F+N)/(N-F)
		// What if z = (33 Z + 34) /  (43 Z + 44)
		//  then   z= (n/(f-n)*Z-1)/(n*f/(f-n) Z)	= (1/f-(1/n-1/f)/Z)		=  1/f-(1/n-1/f)/Z
		// so if Z=n,	z = 1/f - (1/n - 1/f)/n
	}
	else // infinite far plane.
	{
		m._33 = 0.f;					m._34 = -1.f;
		m._43 = zNear;
	}
	// testing:
	GetFrustumFromProjectionMatrix(m);
	return m;
}

const float *Camera2D::MakeDepthReversedProjectionMatrix(float aspect) const
{
	float h=HorizontalFieldOfViewInRadians;
	float v=VerticalFieldOfViewInRadians;
	if(aspect&&v&&!h)
		h=2.f*atan(tan(v/2.0f)*aspect);
	if(aspect&&h&&!v)
		v=2.f*atan(tan(h/2.0f)/aspect);
	static float max_fov=160.0f*SIMUL_PI_F/180.0f;
	if(h>max_fov)
		h=max_fov;
	if(v>max_fov)
		v=max_fov;
	if(!h)
		h=v;
	if(!v)
		v=h;
	return MakeDepthReversedProjectionMatrix(h,v,cameraViewStruct.nearZ,cameraViewStruct.InfiniteFarPlane?0.f:cameraViewStruct.farZ);
}

const float *Camera2D::MakeProjectionMatrix(float h,float v,float zNear,float zFar)
{
	float xScale = 1.f / tan(h / 2.f);
	float yScale = 1.f / tan(v / 2.f);
	static math::Matrix4x4 m;
	memset(&m, 0, 16 * sizeof(float));
	m._11 = xScale;
	m._22 = yScale;

	m._33 = zFar / (zNear - zFar);		m._34 = -1.f;
	m._43 = zNear*zFar / (zNear - zFar);
	//	Zp		=m33 Zv + m43
	// Wp		=-Zv.
	// so     z = (33 Z + 43)/-Z = (F/(N-F)Z+FN/(N-F))/-Z     =     (FZ+FN)/(-Z(N-F))
	//
	return m;
}
const float *Camera2D::MakeProjectionMatrix(const FovPort &fovPort, float zNear, float zFar)
{
	float xScale = 2.0f / (fovPort.leftTan + fovPort.rightTan);
	float xOffset = (fovPort.leftTan - fovPort.rightTan) * xScale * 0.5f;
	float yScale = 2.0f / (fovPort.upTan + fovPort.downTan);
	float yOffset = (fovPort.upTan - fovPort.downTan) * yScale * 0.5f;

	static float handedness = 1.0f;// right-handed obviously.
	static math::Matrix4x4 m;
	memset(&m, 0, 16 * sizeof(float));
	m._11 = xScale;
	m._22 = yScale;
	m._12 = -handedness*xOffset;
	m._23 = handedness *yOffset;

	m._33 = zFar / (zNear - zFar);		m._34 = -1.f;
	m._43 = zNear*zFar / (zNear - zFar);

	return m;
}


const float *Camera2D::MakeProjectionMatrix(float aspect) const
{
// According to the documentation for D3DXMatrixPerspectiveFovLH:
//	xScale     0          0               0
//	0        yScale       0               0
//	0          0       zf/(zf-zn)         1
//	0          0       -zn*zf/(zf-zn)     0
//	where:
//	yScale = cot(fovY/2)
//
//	xScale = yScale / aspect ratio
// But for RH:
//	xScale     0          0               0
//	0        yScale       0               0
//	0          0       zf/(zn-zf)         -1
//	0          0       zn*zf/(zn-zf)     0
	float h=HorizontalFieldOfViewInRadians;
	float v=VerticalFieldOfViewInRadians;
	if(aspect&&v&&!h)
		h=2.0f*atan(tan(v/2.0f)*aspect);
	if(aspect&&h&&!v)
		v=2.0f*atan(tan(h/2.0f)/aspect);
	static float max_fov=160.0f*SIMUL_PI_F/180.0f;
	if(h>max_fov)
		h=max_fov;
	if(v>max_fov)
		v=max_fov;
	if(!h)
		h=v;
	if(!v)
		v=h;
	return MakeProjectionMatrix(h,v,cameraViewStruct.nearZ,cameraViewStruct.farZ);
}
			
const float *Camera2D::MakeStereoProjectionMatrix(WhichEye ,float aspect,bool ReverseDepth) const
{
	if(ReverseDepth)
		return MakeDepthReversedProjectionMatrix(aspect);
	else
		return MakeProjectionMatrix(aspect);
}

const float *Camera2D::MakeStereoViewMatrix(WhichEye ) const
{
	return MakeViewMatrix();
}

vec3 Camera2D::ScreenPositionToDirection(float x,float y,float aspect)
{
	Matrix4x4 proj			=MakeDepthReversedProjectionMatrix(aspect);
	Matrix4x4 view			=MakeViewMatrix();
	Matrix4x4 ivp;
	platform::crossplatform::MakeInvViewProjMatrix((float*)&ivp,view,proj);
	Vector3 clip(x*2.0f-1.0f,1.0f-y*2.0f,1.0f);
	vec3 res;
	ivp.Transpose();
	Multiply4(*((Vector3*)&res),ivp,clip);
	res=res/length(res);
	return res;
}

void Camera2D::SetCameraViewStruct(const CameraViewStruct &c)
{
	cameraViewStruct=c;
}

const CameraViewStruct &Camera2D::GetCameraViewStruct() const
{
	return cameraViewStruct;
}

	// virtual from OrientationInterface
const float *Camera2D::GetOrientationAsPermanentMatrix() const
{
	return (const float *)(&Orientation.GetMatrix());
}

const float *Camera2D::GetRotationAsQuaternion() const
{
	static platform::math::Quaternion q;
	platform::math::MatrixToQuaternion(q,Orientation.GetMatrix());
	return (const float *)(&Orientation.GetQuaternion());
}

const float *Camera2D::GetPosition() const
{ 
	return (const float *)(&Orientation.GetMatrix().GetRowVector(3));
}

void Camera2D::SetOrientationAsMatrix(const float *m)
{
	Orientation.SetFromMatrix(m);
}

void Camera2D::SetOrientationAsQuaternion(const float *q)
{
	Orientation.Define(platform::math::Quaternion(q));
}

void Camera2D::SetPosition(const float *x)
{
	Orientation.SetPosition(platform::math::Vector3(x));
}

void Camera2D::SetPositionAsXYZ(float x,float y,float z)
{
	Orientation.SetPosition(platform::math::Vector3(x,y,z));
}

void Camera2D::Move(const float *x)
{
	Orientation.Translate(x);
}

void Camera2D::LocalMove(const float *x)
{
	Orientation.LocalTranslate(x);
}
				
void Camera2D::Rotate(float x,const float *a)
{
	Orientation.Rotate(x,a);
}		
void Camera2D::LocalRotate(float x,const float *a)
{
	Orientation.LocalRotate(x,a);
}
void Camera2D::LocalRotate(const float *a)
{
	Orientation.LocalRotate(platform::math::Vector3(a));
}

bool Camera2D::TimeStep(float )
{
	return true;
}

float Camera2D::GetHorizontalFieldOfViewDegrees() const
{
	return HorizontalFieldOfViewInRadians*RAD_TO_DEG;
}

void Camera2D::SetHorizontalFieldOfViewDegrees(float f)
{
	HorizontalFieldOfViewInRadians=f*DEG_TO_RAD;
}

float Camera2D::GetVerticalFieldOfViewDegrees() const
{
	return VerticalFieldOfViewInRadians*RAD_TO_DEG;
}

void Camera2D::SetVerticalFieldOfViewDegrees(float f)
{
	VerticalFieldOfViewInRadians=f*DEG_TO_RAD;
}

void Camera2D::CreateViewMatrix(float *mat,const float *view_dir, const float *view_up,const float *pos)
{
	Matrix4x4 &M=*((Matrix4x4*)mat);
	Vector3 d(view_dir);
	d.Normalize();
	Vector3 u(view_up);
	Vector3 x = d^u;
	x.Normalize();
	u = x^d;
	platform::math::SimulOrientation ori;
	ori.DefineFromYZ(u,-d);
	if(pos)
		ori.SetPosition(pos);
	ori.GetMatrix().Inverse(M);
}

void Camera2D::LookInDirection(const float *view_dir,const float *view_up)
{
	Vector3 d(view_dir);
	d.Normalize();
	Vector3 u(view_up);
	Vector3 x=d^u;
	x.Normalize();
	u=x^d;
	Orientation.DefineFromYZ(u,-d);
}

void Camera2D::LookInDirection(const float *view_dir)
{
	Vector3 d(view_dir);
	d.Normalize();
	Vector3 x=d^Vector3(0,0,1.f);
	x.Normalize();
	Vector3 u=x^d;
	Orientation.DefineFromYZ(u,-d);
}

void teleport::clientrender::UpdateMouseCamera(	Camera2D *cam
					,float time_step
					,float cam_spd
					,MouseCameraState &state
					,MouseCameraInput &input
					,float max_height
					,bool lock_height
					,int rotateButton)
{
	platform::math::Vector3 pos=cam->GetPosition();

	static float CameraDamping=1e4f;
	float retain			=1.f/(1.f+CameraDamping*time_step);
	float introduce			=1.f-retain;

	state.forward_back_spd	*=retain;
	state.right_left_spd	*=retain;
	state.up_down_spd		*=retain;

	state.forward_back_spd	+=input.forward_back_input*cam_spd*introduce;
	state.right_left_spd	+=input.right_left_input*cam_spd*introduce;
	state.up_down_spd		+=input.up_down_input*cam_spd*introduce;

	math::Vector3 forward = cam->Orientation.Tz();
	if (lock_height)
	{
		forward.z = 0;
		forward.Normalize();
	}
	pos						-=state.forward_back_spd*time_step*forward;
	pos						+=state.right_left_spd*time_step*cam->Orientation.Tx();
	if (!lock_height)
	{
		pos.z += state.up_down_spd * time_step;
	}
	
	if(pos.z>max_height)
		pos.z=max_height;

	cam->SetPosition(pos.Values);

	int dx=input.MouseX-input.LastMouseX;
	int dy=input.MouseY-input.LastMouseY;
	float mouseDeltaX=0.f,mouseDeltaY=0.f;
	static float rr=750.0f;
	if(input.MouseButtons&(rotateButton))
	{
		mouseDeltaX =dx/rr;
		mouseDeltaY =dy/rr;
	}
	input.LastMouseX=input.MouseX;
	input.LastMouseY=input.MouseY;

	static float x_rotate=0.f;
	static float y_rotate=0.f;
	x_rotate		*=retain;
	x_rotate		+=mouseDeltaX*introduce;
	mouseDeltaX		=0.f;
	y_rotate		*=retain;
	y_rotate		+=mouseDeltaY*introduce;
	mouseDeltaY		=0.f;

	vec3 down(0,0,-1.0f);
	vec3 y(0,1.0f,0);
	cam->LookInDirection(down,y);
	/*
	platform::math::Vector3 vertical(0,0,-1.f);
	platform::math::Vector3 del	=vertical*x_rotate;
	platform::math::Vector3 dir	=del;
	dir.Normalize();
	cam->Rotate(del.Magnitude(),dir.Values);

	del	=cam->Orientation.Tx()*y_rotate*(-1.f);
	dir	=del;
	dir.Normalize();
	cam->Rotate(del.Magnitude(),dir.Values);

	float tilt	=0;
	tilt		=asin(cam->Orientation.Tx().z);
	dir			=cam->Orientation.Tz();
	dir.Normalize();
	cam->Rotate(-0.5f*tilt,dir.Values);*/
}