
#include "Platform/Math/OrientationInterface.h"
#include "Platform/Math/Orientation.h"
#include "Platform/CrossPlatform/CameraInterface.h"
#include "Platform/Math/OrientationInterface.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/CrossPlatform/Frustum.h"
#include "client/Shaders/pbr_constants.sl"
#include "Platform/CrossPlatform/Camera.h"
namespace teleport
{
	namespace clientrender
	{
	
	void UpdateMouseCamera(	class Camera2D *cam
						,float time_step
						,float cam_spd
						,platform::crossplatform::MouseCameraState &state
						,platform::crossplatform::MouseCameraInput &input
						,float max_height
						,bool lock_height
						,int rotateButton);
	class  Camera2D :
			public platform::math::OrientationInterface,
			public platform::crossplatform::CameraInterface
		{
			platform::crossplatform::CameraViewStruct cameraViewStruct;
		public:
			Camera2D();
			virtual ~Camera2D();
			
			float HorizontalFieldOfViewInRadians;
			float VerticalFieldOfViewInRadians;
			platform::math::SimulOrientation	Orientation;
			/// Set the view struct for the camera
			void SetCameraViewStruct(const platform::crossplatform::CameraViewStruct &c);
			const platform::crossplatform::CameraViewStruct &GetCameraViewStruct() const;
			// virtual from OrientationInterface
			virtual const float *GetOrientationAsPermanentMatrix() const;		//! Permanent: this means that for as long as the interface exists, the address is valid.
			virtual const float *GetRotationAsQuaternion() const;
			virtual const float *GetPosition() const;
			/// Create and return the view matrix used by the camera.
			virtual const float *MakeViewMatrix() const;
			virtual const float *MakeDepthReversedProjectionMatrix(float aspect) const;
			virtual const float *MakeProjectionMatrix(float aspect) const;
			
			virtual const float *MakeStereoProjectionMatrix(platform::crossplatform::WhichEye whichEye,float aspect,bool ReverseDepth) const;
			virtual const float *MakeStereoViewMatrix(platform::crossplatform::WhichEye whichEye) const;

			vec3 ScreenPositionToDirection(float x,float y,float aspect);

			virtual void SetOrientationAsMatrix(const float *);
			virtual void SetOrientationAsQuaternion(const float *);
			virtual void SetPosition(const float *);
			/// Set the direction that the camera is pointing in.
			virtual void LookInDirection(const float *view_dir,const float *view_up);
			virtual void LookInDirection(const float *view_dir);
			virtual void SetPositionAsXYZ(float,float,float);
			virtual void Move(const float *);
			virtual void LocalMove(const float *);
			virtual void Rotate(float,const float *);
			virtual void LocalRotate(float,const float *);
			virtual void LocalRotate(const float *);

			virtual bool TimeStep(float delta_t);
			//
			float GetHorizontalFieldOfViewDegrees() const;
			/// Set the Horizontal FoV
			void SetHorizontalFieldOfViewDegrees(float f);
			float GetVerticalFieldOfViewDegrees() const;
			/// Set the Vertical FoV
			void SetVerticalFieldOfViewDegrees(float f);
			static void CreateViewMatrix(float *mat, const float *view_dir, const float *view_up,const float *pos=0);
			static const float *MakeDepthReversedProjectionMatrix(float h, float v, float zNear, float zFar);
			static const float *MakeDepthReversedProjectionMatrix(const platform::crossplatform::FovPort &fovPort, float zNear, float zFar);
			static const float *MakeProjectionMatrix(float h, float v, float zNear, float zFar);
			static const float *MakeProjectionMatrix(const platform::crossplatform::FovPort &fovPort, float zNear, float zFar);
		};
		}
		}