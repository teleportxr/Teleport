/************************************************************************************

Filename    :   SceneView.cpp
Content     :   Basic viewing and movement in a scene.
Created     :   December 19, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "SceneView.h"
#include "ModelRender.h"

#include "OVR_LogUtils.h"

#include "VrApi.h"
#include "VrApi_Helpers.h"

#include <algorithm>

namespace OVR
{

/*
	Vertex Color
*/

const char * VertexColorVertexShaderSrc =
	"attribute highp vec4 Position;\n"
	"attribute lowp vec4 VertexColor;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = TransformVertex( Position );\n"
	"   oColor = VertexColor;\n"
	"}\n";

const char * VertexColorSkinned1VertexShaderSrc =
	"uniform JointMatrices\n"
	"{\n"
	"	highp mat4 Joints[" MAX_JOINTS_STRING "];\n"
	"} jb;\n"
	"attribute highp vec4 Position;\n"
	"attribute lowp vec4 VertexColor;\n"
	"attribute highp vec4 JointWeights;\n"
	"attribute highp vec4 JointIndices;\n"
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"   highp vec4 localPos = jb.Joints[int(JointIndices.x)] * Position;\n"
	"   gl_Position = TransformVertex( localPos );\n"
	"   oColor = VertexColor;\n"
	"}\n";

const char * VertexColorFragmentShaderSrc =
	"varying lowp vec4 oColor;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = oColor;\n"
	"}\n";

/*
	Single Texture
*/

const char * SingleTextureVertexShaderSrc =
	"attribute highp vec4 Position;\n"
	"attribute highp vec2 TexCoord;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = TransformVertex( Position );\n"
	"   oTexCoord = TexCoord;\n"
	"}\n";

const char * SingleTextureSkinned1VertexShaderSrc =
	"uniform JointMatrices\n"
	"{\n"
	"	highp mat4 Joints[" MAX_JOINTS_STRING "];\n"
	"} jb;\n"
	"attribute highp vec4 Position;\n"
	"attribute highp vec2 TexCoord;\n"
	"attribute highp vec4 JointWeights;\n"
	"attribute highp vec4 JointIndices;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"   highp vec4 localPos = jb.Joints[int(JointIndices.x)] * Position;\n"
	"   gl_Position = TransformVertex( localPos );\n"
	"   oTexCoord = TexCoord;\n"
	"}\n";

const char * SingleTextureFragmentShaderSrc =
	"uniform sampler2D Texture0;\n"
	"varying highp vec2 oTexCoord;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = texture2D( Texture0, oTexCoord );\n"
	"}\n";

/*
	Light Mapped
*/

const char * LightMappedVertexShaderSrc =
	"attribute highp vec4 Position;\n"
	"attribute highp vec2 TexCoord;\n"
	"attribute highp vec2 TexCoord1;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = TransformVertex( Position );\n"
	"   oTexCoord = TexCoord;\n"
	"   oTexCoord1 = TexCoord1;\n"
	"}\n";

const char * LightMappedSkinned1VertexShaderSrc =
	"uniform JointMatrices\n"
	"{\n"
	"	highp mat4 Joints[" MAX_JOINTS_STRING "];\n"
	"} jb;\n"
	"attribute highp vec4 Position;\n"
	"attribute highp vec2 TexCoord;\n"
	"attribute highp vec2 TexCoord1;\n"
	"attribute highp vec4 JointWeights;\n"
	"attribute highp vec4 JointIndices;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"void main()\n"
	"{\n"
	"   highp vec4 localPos = jb.Joints[int(JointIndices.x)] * Position;\n"
	"   gl_Position = TransformVertex( localPos );\n"
	"   oTexCoord = TexCoord;\n"
	"   oTexCoord1 = TexCoord1;\n"
	"}\n";

const char * LightMappedFragmentShaderSrc =
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"void main()\n"
	"{\n"
	"   lowp vec4 diffuse = texture2D( Texture0, oTexCoord );\n"
	"   lowp vec4 emissive = texture2D( Texture1, oTexCoord1 );\n"
	"   gl_FragColor.xyz = diffuse.xyz * emissive.xyz * 1.5;\n"
	"   gl_FragColor.w = diffuse.w;\n"
	"}\n";

/*
	Reflection Mapped
*/

const char * ReflectionMappedVertexShaderSrc =
	"uniform highp mat4 Modelm;\n"
	"attribute highp vec4 Position;\n"
	"attribute highp vec3 Normal;\n"
	"attribute highp vec3 Tangent;\n"
	"attribute highp vec3 Binormal;\n"
	"attribute highp vec2 TexCoord;\n"
	"attribute highp vec2 TexCoord1;\n"
	"varying highp vec3 oEye;\n"
	"varying highp vec3 oNormal;\n"
	"varying highp vec3 oTangent;\n"
	"varying highp vec3 oBinormal;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"vec3 multiply( mat4 m, vec3 v )\n"
	"{\n"
	"   return vec3(\n"
	"      m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,\n"
	"      m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,\n"
	"      m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );\n"
	"}\n"
	"vec3 transposeMultiply( mat4 m, vec3 v )\n"
	"{\n"
	"   return vec3(\n"
	"      m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,\n"
	"      m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,\n"
	"      m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );\n"
	"}\n"
	"void main()\n"
	"{\n"
	"   gl_Position = TransformVertex( Position );\n"
	"   vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );\n"
	"   oEye = eye - vec3( Modelm * Position );\n"
	"   oNormal = multiply( Modelm, Normal );\n"
	"   oTangent = multiply( Modelm, Tangent );\n"
	"   oBinormal = multiply( Modelm, Binormal );\n"
	"   oTexCoord = TexCoord;\n"
	"   oTexCoord1 = TexCoord1;\n"
	"}\n";

const char * ReflectionMappedSkinned1VertexShaderSrc =
	"uniform highp mat4 Modelm;\n"
	"uniform JointMatrices\n"
	"{\n"
	"	highp mat4 Joints[" MAX_JOINTS_STRING "];\n"
	"} jb;\n"
	"attribute highp vec4 Position;\n"
	"attribute highp vec3 Normal;\n"
	"attribute highp vec3 Tangent;\n"
	"attribute highp vec3 Binormal;\n"
	"attribute highp vec2 TexCoord;\n"
	"attribute highp vec2 TexCoord1;\n"
	"attribute highp vec4 JointWeights;\n"
	"attribute highp vec4 JointIndices;\n"
	"varying highp vec3 oEye;\n"
	"varying highp vec3 oNormal;\n"
	"varying highp vec3 oTangent;\n"
	"varying highp vec3 oBinormal;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"vec3 multiply( mat4 m, vec3 v )\n"
	"{\n"
	"   return vec3(\n"
	"      m[0].x * v.x + m[1].x * v.y + m[2].x * v.z,\n"
	"      m[0].y * v.x + m[1].y * v.y + m[2].y * v.z,\n"
	"      m[0].z * v.x + m[1].z * v.y + m[2].z * v.z );\n"
	"}\n"
	"vec3 transposeMultiply( mat4 m, vec3 v )\n"
	"{\n"
	"   return vec3(\n"
	"      m[0].x * v.x + m[0].y * v.y + m[0].z * v.z,\n"
	"      m[1].x * v.x + m[1].y * v.y + m[1].z * v.z,\n"
	"      m[2].x * v.x + m[2].y * v.y + m[2].z * v.z );\n"
	"}\n"
	"void main()\n"
	"{\n"
	"   highp vec4 localPos = jb.Joints[int(JointIndices.x)] * Position;\n"
	"   gl_Position = TransformVertex( localPos );\n"
	"   vec3 eye = transposeMultiply( sm.ViewMatrix[VIEW_ID], -vec3( sm.ViewMatrix[VIEW_ID][3] ) );\n"
	"   oEye = eye - vec3( Modelm * ( jb.Joints[int(JointIndices.x)] * Position ) );\n"
	"   oNormal = multiply( Modelm, multiply( jb.Joints[int(JointIndices.x)], Normal ) );\n"
	"   oTangent = multiply( Modelm, multiply( jb.Joints[int(JointIndices.x)], Tangent ) );\n"
	"   oBinormal = multiply( Modelm, multiply( jb.Joints[int(JointIndices.x)], Binormal ) );\n"
	"   oTexCoord = TexCoord;\n"
	"   oTexCoord1 = TexCoord1;\n"
	"}\n";

const char * ReflectionMappedFragmentShaderSrc =
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"
	"uniform sampler2D Texture2;\n"
	"uniform sampler2D Texture3;\n"
	"uniform samplerCube Texture4;\n"
	"varying highp vec3 oEye;\n"
	"varying highp vec3 oNormal;\n"
	"varying highp vec3 oTangent;\n"
	"varying highp vec3 oBinormal;\n"
	"varying highp vec2 oTexCoord;\n"
	"varying highp vec2 oTexCoord1;\n"
	"void main()\n"
	"{\n"
	"   mediump vec3 normal = texture2D( Texture2, oTexCoord ).xyz * 2.0 - 1.0;\n"
	"   mediump vec3 surfaceNormal = normal.x * oTangent + normal.y * oBinormal + normal.z * oNormal;\n"
	"   mediump vec3 eyeDir = normalize( oEye.xyz );\n"
	"   mediump vec3 reflectionDir = dot( eyeDir, surfaceNormal ) * 2.0 * surfaceNormal - eyeDir;\n"
	"   lowp vec3 specular = texture2D( Texture3, oTexCoord ).xyz * textureCube( Texture4, reflectionDir ).xyz;\n"
	"   lowp vec4 diffuse = texture2D( Texture0, oTexCoord );\n"
	"   lowp vec4 emissive = texture2D( Texture1, oTexCoord1 );\n"
	"	gl_FragColor.xyz = diffuse.xyz * emissive.xyz * 1.5 + specular;\n"
	"   gl_FragColor.w = diffuse.w;\n"
	"}\n";

/*
PBR
Currently this is flat shaded with emmissive so not really PBR
*/

const char * SimplePBRVertexShaderSrc =
"attribute highp vec4 Position;\n"
"attribute highp vec2 TexCoord;\n"
"varying highp vec2 oTexCoord;\n"
"void main()\n"
"{\n"
"   gl_Position = TransformVertex( Position );\n"
"   oTexCoord = TexCoord;\n"
"}\n";

const char * SimplePBRSkinned1VertexShaderSrc =
"uniform JointMatrices\n"
"{\n"
"	highp mat4 Joints[" MAX_JOINTS_STRING "];\n"
"} jb;\n"
"attribute highp vec4 Position;\n"
"attribute highp vec2 TexCoord;\n"
"attribute highp vec4 JointWeights;\n"
"attribute highp vec4 JointIndices;\n"
"varying highp vec2 oTexCoord;\n"
"void main()\n"
"{\n"
"   highp vec4 localPos1 = jb.Joints[int(JointIndices.x)] * Position;\n"
"   highp vec4 localPos2 = jb.Joints[int(JointIndices.y)] * Position;\n"
"   highp vec4 localPos3 = jb.Joints[int(JointIndices.z)] * Position;\n"
"   highp vec4 localPos4 = jb.Joints[int(JointIndices.w)] * Position;\n"
"   highp vec4 localPos = localPos1 * JointWeights.x + localPos2 * JointWeights.y + localPos3 * JointWeights.z + localPos4 * JointWeights.w;\n"
"   gl_Position = TransformVertex( localPos );\n"
"   oTexCoord = TexCoord;\n"
"}\n";

const char * SimplePBRFragmentShaderSrc =
"uniform lowp vec4 BaseColorFactor;\n"
"void main()\n"
"{\n"
"   gl_FragColor = BaseColorFactor; \n"
"}\n";

const char * BaseColorPBRFragmentShaderSrc =
"uniform sampler2D BaseColorTexture;\n"
"uniform lowp vec4 BaseColorFactor;\n"
"varying highp vec2 oTexCoord;\n"
"void main()\n"
"{\n"
"   lowp vec4 BaseColor = texture2D( BaseColorTexture, oTexCoord );\n"
"   gl_FragColor.r = BaseColor.r * BaseColorFactor.r; \n"
"   gl_FragColor.g = BaseColor.g * BaseColorFactor.g; \n"
"   gl_FragColor.b = BaseColor.b * BaseColorFactor.b; \n"
"   gl_FragColor.w = BaseColor.w * BaseColorFactor.w; \n"
"}\n";

const char * BaseColorEmissivePBRFragmentShaderSrc =
"uniform sampler2D Texture0;\n"
"uniform sampler2D Texture1;\n"
"uniform lowp vec4 BaseColorFactor;\n"
"uniform lowp vec4 EmissiveFactor;\n"
"varying highp vec2 oTexCoord;\n"
"void main()\n"
"{\n"
"   lowp vec4 BaseColor = texture2D( Texture0, oTexCoord );\n"
"   BaseColor.r = BaseColor.r * BaseColorFactor.r; \n"
"   BaseColor.g = BaseColor.g * BaseColorFactor.g; \n"
"   BaseColor.b = BaseColor.b * BaseColorFactor.b; \n"
"   BaseColor.w = BaseColor.w * BaseColorFactor.w; \n"
"   lowp vec4 EmissiveColor = texture2D( Texture1, oTexCoord );\n"
"   gl_FragColor = BaseColor + EmissiveColor; \n"
"}\n";

void ModelInScene::SetModelFile( const ModelFile * mf ) 
{ 
	Definition = mf;
	if ( mf != NULL )
	{
		State.GenerateStateFromModelFile( mf );
	}
};

static Vector3f AnimationInterpolateVector3f( float * buffer, int frame, float fraction, ModelAnimationInterpolation interpolationType )
{
	Vector3f firstElement;
	firstElement.x = buffer[frame * 3 + 0];
	firstElement.y = buffer[frame * 3 + 1];
	firstElement.z = buffer[frame * 3 + 2];
	Vector3f secondElement;
	secondElement.x = buffer[frame * 3 + 3];
	secondElement.y = buffer[frame * 3 + 4];
	secondElement.z = buffer[frame * 3 + 5];

	if ( interpolationType == MODEL_ANIMATION_INTERPOLATION_LINEAR )
	{
		firstElement = firstElement.Lerp( secondElement, fraction );
		return firstElement;
	}
	else if ( interpolationType == MODEL_ANIMATION_INTERPOLATION_STEP )
	{
		if ( fraction >= 1.0f )
		{
			return secondElement;
		}
		else
		{
			return firstElement;
		}
	}
	else if ( interpolationType == MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE )
	{
		// #TODO implement MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE
		OVR_WARN( "MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE not implemented" );
		firstElement = firstElement.Lerp( secondElement, fraction );
		return firstElement;
	}
	else if ( interpolationType == MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE )
	{
		// #TODO implement MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE
		OVR_WARN( "MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE not implemented" );
		firstElement = firstElement.Lerp( secondElement, fraction );
		return firstElement;
	}
	else
	{
		OVR_WARN( "inavlid interpolation type on animation" );
		return firstElement;
	}
}

static Quatf AnimationInterpolateQuatf( float * buffer, int frame, float fraction, ModelAnimationInterpolation interpolationType )
{
	Quatf firstElement;
	firstElement.x = buffer[frame * 4 + 0];
	firstElement.y = buffer[frame * 4 + 1];
	firstElement.z = buffer[frame * 4 + 2];
	firstElement.w = buffer[frame * 4 + 3];
	Quatf secondElement;
	secondElement.x = buffer[frame * 4 + 4];
	secondElement.y = buffer[frame * 4 + 5];
	secondElement.z = buffer[frame * 4 + 6];
	secondElement.w = buffer[frame * 4 + 7];

	if ( interpolationType == MODEL_ANIMATION_INTERPOLATION_LINEAR )
	{
		firstElement = firstElement.Lerp( secondElement, fraction );
		return firstElement;
	}
	else if ( interpolationType == MODEL_ANIMATION_INTERPOLATION_STEP )
	{
		if ( fraction >= 1.0f )
		{
			return secondElement;
		}
		else
		{
			return firstElement;
		}
	}
	else if ( interpolationType == MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE )
	{
		OVR_WARN( "MODEL_ANIMATION_INTERPOLATION_CATMULLROMSPLINE does not make sense for quaternions." );
		firstElement = firstElement.Lerp( secondElement, fraction );
		return firstElement;
	}
	else if ( interpolationType == MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE )
	{
		OVR_WARN( "MODEL_ANIMATION_INTERPOLATION_CUBICSPLINE does not make sense for quaternions." );
		firstElement = firstElement.Lerp( secondElement, fraction );
		return firstElement;
	}
	else
	{
		OVR_WARN( "inavlid interpolation type on animation" );
		return firstElement;
	}
}

void ModelInScene::AnimateJoints( const double timeInSeconds )
{
	// old ovrscene animation method
	for ( int i = 0; i < static_cast< int >( State.subSceneStates.size() ); i++ )
	{
		for ( int j = 0; j < static_cast< int >( State.subSceneStates[i].nodeStates.size() ); j++ )
		{
			ModelNodeState * nodeState = &State.nodeStates[State.subSceneStates[i].nodeStates[j]];
			if ( nodeState->GetNode() != nullptr )
			{
				for ( int k = 0; k < static_cast< int >( nodeState->GetNode()->JointsOvrScene.size() ); k++ )
				{
					const ModelJoint * joint = &nodeState->GetNode()->JointsOvrScene[k];
					if ( joint->animation == MODEL_JOINT_ANIMATION_NONE )
					{
						continue;
					}

					double time = ( timeInSeconds + joint->timeOffset ) * joint->timeScale;

					switch ( joint->animation )
					{
					case MODEL_JOINT_ANIMATION_SWAY:
					{
						time = sin( time * MATH_DOUBLE_PI );
						// NOTE: fall through
					}
					case MODEL_JOINT_ANIMATION_ROTATE:
					{
						const Vector3d angles = Vector3d( joint->parameters ) * ( MATH_DOUBLE_DEGREETORADFACTOR * time );
						const Matrix4f matrix = joint->transform *
							Matrix4f::RotationY( static_cast< float >( fmod( angles.y, 2.0 * MATH_DOUBLE_PI ) ) ) *
							Matrix4f::RotationX( static_cast< float >( fmod( angles.x, 2.0 * MATH_DOUBLE_PI ) ) ) *
							Matrix4f::RotationZ( static_cast< float >( fmod( angles.z, 2.0 * MATH_DOUBLE_PI ) ) ) *
						joint->transform.Inverted();
						nodeState->JointMatricesOvrScene[k] = matrix;
						break;
					}
					case MODEL_JOINT_ANIMATION_BOB:
					{
						const float frac = static_cast< float >( sin( time * MATH_DOUBLE_PI ) );
						const Vector3f offset = joint->parameters * frac;
						const Matrix4f matrix = joint->transform *
							Matrix4f::Translation( offset ) *
						joint->transform.Inverted();
						nodeState->JointMatricesOvrScene[k] = matrix;
						break;
					}
					case MODEL_JOINT_ANIMATION_NONE:
						break;
					}
				}
			}
		}
	}

	
	// new animation method.
	{
		if ( State.animationTimelineStates.size() > 0 )
		{
			State.CalculateAnimationFrameAndFraction( MODEL_ANIMATION_TIME_TYPE_LOOP_FORWARD, (float)timeInSeconds );

			for ( int i = 0; i < static_cast< int >( State.mf->Animations.size() ); i++ )
			{
				const ModelAnimation & animation = State.mf->Animations[i];
				for ( int j = 0; j < static_cast< int >( animation.channels.size() ); j++ )
				{
					const ModelAnimationChannel & channel = animation.channels[j];
					ModelNodeState & nodeState = State.nodeStates[channel.nodeIndex];
					ModelAnimationTimeLineState & timeLineState = State.animationTimelineStates[channel.sampler->timeLineIndex];
					
					float * bufferData = (float *)(channel.sampler->output->BufferData() );
					if ( channel.path == MODEL_ANIMATION_PATH_TRANSLATION )
					{
						Vector3f translation = AnimationInterpolateVector3f( bufferData, timeLineState.frame, timeLineState.fraction, channel.sampler->interpolation );
						nodeState.translation = translation;
					}
					else if ( channel.path == MODEL_ANIMATION_PATH_SCALE )
					{
						Vector3f scale = AnimationInterpolateVector3f( bufferData, timeLineState.frame, timeLineState.fraction, channel.sampler->interpolation );
						nodeState.scale = scale;
					}
					else if ( channel.path == MODEL_ANIMATION_PATH_ROTATION )
					{
						
						Quatf rotation = AnimationInterpolateQuatf( bufferData, timeLineState.frame, timeLineState.fraction, channel.sampler->interpolation );
						nodeState.rotation = rotation;
					}
					else if ( channel.path == MODEL_ANIMATION_PATH_WEIGHTS )
					{
						OVR_WARN("Weights animation not currently supported on channel %d '%s'", j, animation.name.c_str() );
					}
					else
					{
						OVR_WARN( "Bad animation path on channel %d '%s'", j, animation.name.c_str() );
					}

					nodeState.CalculateLocalTransform();
				}
			}

			for ( int i = 0; i < static_cast< int >( State.nodeStates.size() ); i++ )
			{
				State.nodeStates[i].RecalculateMatrix();
			}

		}
	}
	
}

//-------------------------------------------------------------------------------------

OvrSceneView::OvrSceneView() :
	FreeWorldModelOnChange( false ),
	LoadedPrograms( false ),
	Paused( false ),
	SuppressModelsWithClientId( -1 ),
	// FIXME: ideally EyeHeight and IPD properties would default initialize to 0.0f, but there are a handful of
	// menus which cache these values before a frame has had a chance to run.
	EyeHeight( 1.6750f ),				// average eye height above the ground when standing
	InterPupillaryDistance( 0.0640f ),	// average interpupillary distance
	Znear( VRAPI_ZNEAR ),
	StickYaw( 0.0f ),
	StickPitch( 0.0f ),
	SceneYaw( 0.0f ),
	YawVelocity( 0.0f ),
	MoveSpeed( 3.0f ),
	FreeMove( false ),
	FootPos( 0.0f ),
	EyeYaw( 0.0f ),
	EyePitch( 0.0f ),
	EyeRoll( 0.0f ),
	YawMod( -1.0f )
{
	CenterEyeTransform = Matrix4f::Identity();
	CenterEyeViewMatrix = Matrix4f::Identity();
}

ModelGlPrograms OvrSceneView::GetDefaultGLPrograms()
{
	ModelGlPrograms programs;

	if ( !LoadedPrograms )
	{
		ProgVertexColor						= BuildProgram( VertexColorVertexShaderSrc, VertexColorFragmentShaderSrc );
		ProgSingleTexture					= BuildProgram( SingleTextureVertexShaderSrc, SingleTextureFragmentShaderSrc );
		ProgLightMapped						= BuildProgram( LightMappedVertexShaderSrc, LightMappedFragmentShaderSrc );
		ProgReflectionMapped				= BuildProgram( ReflectionMappedVertexShaderSrc, ReflectionMappedFragmentShaderSrc );
		ProgSimplePBR						= BuildProgram( SimplePBRVertexShaderSrc, SimplePBRFragmentShaderSrc );
		ProgBaseColorPBR					= BuildProgram( SimplePBRVertexShaderSrc, BaseColorPBRFragmentShaderSrc );
		ProgBaseColorEmissivePBR			= BuildProgram( SimplePBRVertexShaderSrc, BaseColorEmissivePBRFragmentShaderSrc );
		ProgSkinnedVertexColor				= BuildProgram( VertexColorSkinned1VertexShaderSrc, VertexColorFragmentShaderSrc );
		ProgSkinnedSingleTexture			= BuildProgram( SingleTextureSkinned1VertexShaderSrc, SingleTextureFragmentShaderSrc );
		ProgSkinnedLightMapped				= BuildProgram( LightMappedSkinned1VertexShaderSrc, LightMappedFragmentShaderSrc );
		ProgSkinnedReflectionMapped			= BuildProgram( ReflectionMappedSkinned1VertexShaderSrc, ReflectionMappedFragmentShaderSrc );
		ProgSkinnedSimplePBR				= BuildProgram( SimplePBRSkinned1VertexShaderSrc, SimplePBRFragmentShaderSrc );
		ProgSkinnedBaseColorPBR				= BuildProgram( SimplePBRSkinned1VertexShaderSrc, BaseColorPBRFragmentShaderSrc );
		ProgSkinnedBaseColorEmissivePBR		= BuildProgram( SimplePBRSkinned1VertexShaderSrc, BaseColorEmissivePBRFragmentShaderSrc );
		LoadedPrograms = true;
	}

	programs.ProgVertexColor				= & ProgVertexColor;
	programs.ProgSingleTexture				= & ProgSingleTexture;
	programs.ProgLightMapped				= & ProgLightMapped;
	programs.ProgReflectionMapped			= & ProgReflectionMapped;
	programs.ProgSimplePBR					= & ProgSimplePBR;
	programs.ProgBaseColorPBR				= & ProgBaseColorPBR;
	programs.ProgBaseColorEmissivePBR		= & ProgBaseColorEmissivePBR;
	programs.ProgSkinnedVertexColor			= & ProgSkinnedVertexColor;
	programs.ProgSkinnedSingleTexture		= & ProgSkinnedSingleTexture;
	programs.ProgSkinnedLightMapped			= & ProgSkinnedLightMapped;
	programs.ProgSkinnedReflectionMapped	= & ProgSkinnedReflectionMapped;
	programs.ProgSkinnedSimplePBR			= & ProgSkinnedSimplePBR;
	programs.ProgSkinnedBaseColorPBR		= & ProgSkinnedBaseColorPBR;
	programs.ProgSkinnedBaseColorEmissivePBR= & ProgSkinnedBaseColorEmissivePBR;

	return programs;
}

void OvrSceneView::LoadWorldModel( const char * sceneFileName, const MaterialParms & materialParms, const bool fromApk )
{
	OVR_LOG( "OvrSceneView::LoadScene( %s )", sceneFileName );

	if ( GlPrograms.ProgSingleTexture == NULL )
	{
		GlPrograms = GetDefaultGLPrograms();
	}

	ModelFile * model = NULL;
	// Load the scene we are going to draw
	if ( fromApk )
	{
		model = LoadModelFileFromApplicationPackage( sceneFileName, GlPrograms, materialParms );
	}
	else
	{
		model = LoadModelFile( sceneFileName, GlPrograms, materialParms );
	}

	if ( model == nullptr )
	{
		OVR_WARN( "OvrSceneView::LoadScene( %s ) failed", sceneFileName );
		return;
	}

	SetWorldModel( *model );

	FreeWorldModelOnChange = true;
}

void OvrSceneView::LoadWorldModelFromApplicationPackage( const char * sceneFileName, const MaterialParms & materialParms )
{
	LoadWorldModel( sceneFileName, materialParms, true );
}

void OvrSceneView::LoadWorldModel( const char * sceneFileName, const MaterialParms & materialParms )
{
	LoadWorldModel( sceneFileName, materialParms, false );
}

void OvrSceneView::SetWorldModel( ModelFile & world )
{
	OVR_LOG( "OvrSceneView::SetWorldModel( %s )", world.FileName.c_str() );

	if ( FreeWorldModelOnChange && static_cast< int >( Models.size() ) > 0 )
	{
		delete WorldModel.Definition;
		FreeWorldModelOnChange = false;
	}
	Models.clear();

	WorldModel.SetModelFile( &world );
	AddModel( &WorldModel );

	// Set the initial player position
	FootPos = Vector3f( 0.0f, 0.0f, 0.0f );
	StickYaw = 0.0f;
	StickPitch = 0.0f;
	SceneYaw = 0.0f;
}

void OvrSceneView::ClearStickAngles()
{
	StickYaw = 0.0f;
	StickPitch = 0.0f;
}

ovrSurfaceDef * OvrSceneView::FindNamedSurface( const char * name ) const
{
	return ( WorldModel.Definition == NULL ) ? NULL : WorldModel.Definition->FindNamedSurface( name );
}

const ModelTexture * OvrSceneView::FindNamedTexture( const char * name ) const
{
	return ( WorldModel.Definition == NULL ) ? NULL : WorldModel.Definition->FindNamedTexture( name );
}

const ModelTag * OvrSceneView::FindNamedTag( const char * name ) const
{
	return ( WorldModel.Definition == NULL ) ? NULL : WorldModel.Definition->FindNamedTag( name );
}

Bounds3f OvrSceneView::GetBounds() const
{
	return ( WorldModel.Definition == NULL ) ?
			Bounds3f( Vector3f( 0.0f, 0.0f, 0.0f ), Vector3f( 0.0f, 0.0f, 0.0f ) ) :
			WorldModel.Definition->GetBounds();
}

int OvrSceneView::AddModel( ModelInScene * model )
{
	const int modelsSize = static_cast< int >( Models.size() );

	// scan for a NULL entry
	for ( int i = 0; i < modelsSize; ++i )
	{
		if ( Models[i] == NULL )
		{
			Models[i] = model;
			return i;
		}
	}

	Models.push_back( model );

	return static_cast< int >( Models.size() ) - 1;
}

void OvrSceneView::RemoveModelIndex( int index )
{
	Models[index] = NULL;
}

void OvrSceneView::GetFrameMatrices( const float fovDegreesX, const float fovDegreesY, ovrFrameMatrices & frameMatrices ) const
{
	frameMatrices.CenterView = GetCenterEyeViewMatrix();
	for ( int i = 0; i < 2; i++ )
	{
		frameMatrices.EyeView[i] = GetEyeViewMatrix( i );
		frameMatrices.EyeProjection[i] = GetEyeProjectionMatrix( i, fovDegreesX, fovDegreesY );
	}
}

void OvrSceneView::GenerateFrameSurfaceList( const ovrFrameMatrices & frameMatrices, std::vector< ovrDrawSurface > & surfaceList ) const
{
	Matrix4f symmetricEyeProjectionMatrix = frameMatrices.EyeProjection[0];
	symmetricEyeProjectionMatrix.M[0][0] = frameMatrices.EyeProjection[0].M[0][0] / ( fabsf( frameMatrices.EyeProjection[0].M[0][2] ) + 1.0f );
	symmetricEyeProjectionMatrix.M[0][2] = 0.0f;
 
	const float moveBackDistance = 0.5f * InterPupillaryDistance * symmetricEyeProjectionMatrix.M[0][0];
	Matrix4f centerEyeCullViewMatrix = Matrix4f::Translation( 0, 0, -moveBackDistance ) * frameMatrices.CenterView;

	std::vector< ModelNodeState * > emitNodes;
	for ( int i = 0; i < static_cast< int >( Models.size() ); i++ )
	{
		if ( Models[i] != NULL )
		{
			ModelState & state = Models[i]->State;
			if ( state.DontRenderForClientUid == SuppressModelsWithClientId )
			{
				continue;
			}
			for ( int j = 0; j < static_cast< int >( state.subSceneStates.size() ); j++ )
			{
				ModelSubSceneState & subSceneState = state.subSceneStates[j];
				if ( subSceneState.visible )
				{
					for ( int k = 0; k < static_cast< int >( subSceneState.nodeStates.size() ); k++ )
					{
						state.nodeStates[subSceneState.nodeStates[k]].AddNodesToEmitList( emitNodes );
					}
				}
			}
		}
	}

	BuildModelSurfaceList( surfaceList, emitNodes, EmitSurfaces, centerEyeCullViewMatrix, symmetricEyeProjectionMatrix );
}

void OvrSceneView::SetFootPos( const Vector3f & pos, bool updateCenterEye /*= true*/ )
{ 
	FootPos = pos; 
	if ( updateCenterEye )
	{
		UpdateCenterEye();
	}
}

Vector3f OvrSceneView::GetNeutralHeadCenter() const
{
	return Vector3f( FootPos.x, FootPos.y, FootPos.z );
}

Vector3f OvrSceneView::GetCenterEyePosition() const
{
	return Vector3f( CenterEyeTransform.M[0][3], CenterEyeTransform.M[1][3], CenterEyeTransform.M[2][3] );
}

Vector3f OvrSceneView::GetCenterEyeForward() const
{
	return Vector3f( -CenterEyeViewMatrix.M[2][0], -CenterEyeViewMatrix.M[2][1], -CenterEyeViewMatrix.M[2][2] );
}

Matrix4f OvrSceneView::GetCenterEyeTransform() const
{
	return CenterEyeTransform;
}

Matrix4f OvrSceneView::GetCenterEyeViewMatrix() const
{
	return CenterEyeViewMatrix;
}

Matrix4f OvrSceneView::GetEyeViewMatrix( const int eye ) const
{
	// World space head rotation
	const Matrix4f head_rotation = Matrix4f( CurrentTracking.HeadPose.Pose.Orientation );

	// Convert the eye view to world-space and remove translation
	Matrix4f eye_view_rot = CurrentTracking.Eye[eye].ViewMatrix;
	eye_view_rot.M[0][3] = 0;
	eye_view_rot.M[1][3] = 0;
	eye_view_rot.M[2][3] = 0;
	const Matrix4f eye_rotation = eye_view_rot.Inverted();

	// Compute the rotation tranform from head to eye (in case of rotated screens)
	const Matrix4f head_rot_inv = head_rotation.Inverted();
	Matrix4f head_eye_rotation = head_rot_inv * eye_rotation;

	// Add the IPD translation from head to eye
	const float eye_shift = ( ( eye == 0 ) ? -0.5f : 0.5f ) * InterPupillaryDistance;
	const Matrix4f head_eye_translation = Matrix4f::Translation( eye_shift, 0.0f, 0.0f );

	// The full transform from head to eye in world
	const Matrix4f head_eye_transform = head_eye_translation * head_eye_rotation;

	// Compute the new eye-pose using the input center eye view
	const Matrix4f center_eye_pose_m = CenterEyeViewMatrix.Inverted();   // convert to world
	const Matrix4f eye_pose_m = center_eye_pose_m * head_eye_transform;

	// Convert to view matrix
	Matrix4f eye_view = eye_pose_m.Inverted();
	return eye_view;
}

Matrix4f OvrSceneView::GetEyeProjectionMatrix( const int eye, const float fovDegreesX, const float fovDegreesY ) const
{
	OVR_UNUSED( eye );

	// We may want to make per-eye projection matrices if we move away from nearly-centered lenses.
	// Use an infinite projection matrix because, except for things right up against the near plane,
	// it provides better precision:
	//		"Tightening the Precision of Perspective Rendering"
	//		Paul Upchurch, Mathieu Desbrun
	//		Journal of Graphics Tools, Volume 16, Issue 1, 2012
	return ovrMatrix4f_CreateProjectionFov( fovDegreesX, fovDegreesY, 0.0f, 0.0f, Znear, 0.0f );
}

Matrix4f OvrSceneView::GetEyeViewProjectionMatrix( const int eye, const float fovDegreesX, const float fovDegreesY ) const
{
	return GetEyeProjectionMatrix( eye, fovDegreesX, fovDegreesY ) * GetEyeViewMatrix( eye );
}

float OvrSceneView::GetEyeHeight() const
{
	return EyeHeight;
}

// This is called by Frame(), but it must be explicitly called when FootPos is
// updated, or calls to GetCenterEyePosition() won't reflect changes until the
// following frame.
void OvrSceneView::UpdateCenterEye()
{
	Matrix4f input;
	if ( YawMod > 0.0f )
	{
		input = Matrix4f::Translation( GetNeutralHeadCenter() ) *
			Matrix4f::RotationY( ( StickYaw - fmodf( StickYaw, YawMod ) ) + SceneYaw ) *
			Matrix4f::RotationX( StickPitch );
	}
	else
	{
		input = Matrix4f::Translation( GetNeutralHeadCenter() ) *
			Matrix4f::RotationY( StickYaw + SceneYaw ) *
			Matrix4f::RotationX( StickPitch );
	}

	const Matrix4f transform = vrapi_GetTransformFromPose( (ovrPosef *)&CurrentTracking.HeadPose );
	CenterEyeTransform = ovrMatrix4f_Multiply( (ovrMatrix4f *)&input, (ovrMatrix4f *)&transform );
	CenterEyeViewMatrix = ovrMatrix4f_Inverse( (ovrMatrix4f *)&CenterEyeTransform );
}

void OvrSceneView::Frame( const ovrFrameInput & vrFrame,
							const long long suppressModelsWithClientId_ )
{
	SuppressModelsWithClientId = suppressModelsWithClientId_;
	CurrentTracking = vrFrame.Tracking;

	InterPupillaryDistance = vrFrame.IPD;
	EyeHeight = vrFrame.EyeHeight;

	// Delta time in seconds since last frame.
	const float dt = vrFrame.DeltaSeconds;
	const float angleSpeed = 1.5f;

	//
	// Player view angles
	//
	Vector3f headPos_gameSpace0;
	headPos_gameSpace0.x= vrFrame.Tracking.HeadPose.Pose.Position.x*cos(StickYaw)+vrFrame.Tracking.HeadPose.Pose.Position.z*sin(StickYaw);
	headPos_gameSpace0.y= vrFrame.Tracking.HeadPose.Pose.Position.y;
	headPos_gameSpace0.z=-vrFrame.Tracking.HeadPose.Pose.Position.x*sin(StickYaw)+vrFrame.Tracking.HeadPose.Pose.Position.z*cos(StickYaw);

	// Turn based on the look stick
	// Because this can be predicted ahead by async TimeWarp, we apply
	// the yaw from the previous frame's controls, trading a frame of
	// latency on stick controls to avoid a bounce-back.
	StickYaw -= YawVelocity * dt;
	if ( StickYaw < 0.0f )
	{
		StickYaw += 2.0f * MATH_FLOAT_PI;
	}
	else if ( StickYaw > 2.0f * MATH_FLOAT_PI )
	{
		StickYaw -= 2.0f * MATH_FLOAT_PI;
	}
	YawVelocity = angleSpeed * vrFrame.Input.sticks[1][0];

	// with any change in StickYaw, we are rotating footSpace in gameSpace,
	// with the result that footPos+footSpace(vrFrame.Tracking.HeadPose.Pose.Position) must remain constant.

	// i.e. footPos0+footSpace0(vrFrame.Tracking.HeadPose.Pose.Position)=footPos1+footSpace1(vrFrame.Tracking.HeadPose.Position)
	// so footPos1=footPos0+footSpace0(headpos)-footSpace1(headPos);

	Vector3f headPos_gameSpace1;
	headPos_gameSpace1.x= vrFrame.Tracking.HeadPose.Pose.Position.x*cos(StickYaw)+vrFrame.Tracking.HeadPose.Pose.Position.z*sin(StickYaw);
	headPos_gameSpace1.y= vrFrame.Tracking.HeadPose.Pose.Position.y;
	headPos_gameSpace1.z=-vrFrame.Tracking.HeadPose.Pose.Position.x*sin(StickYaw)+vrFrame.Tracking.HeadPose.Pose.Position.z*cos(StickYaw);
	FootPos+=headPos_gameSpace0-headPos_gameSpace1;


	// Only if there is no head tracking, allow right stick up/down to adjust pitch,
	// which can be useful for debugging without having to dock the device.
	if ( ( vrFrame.Tracking.Status & VRAPI_TRACKING_STATUS_ORIENTATION_TRACKED ) == 0 ||
		 ( vrFrame.Tracking.Status & VRAPI_TRACKING_STATUS_HMD_CONNECTED ) == 0 )
	{
		StickPitch -= angleSpeed * vrFrame.Input.sticks[1][1] * dt;
	}
	else
	{
		StickPitch = 0.0f;
	}

	// We extract Yaw, Pitch, Roll instead of directly using the orientation
	// to allow "additional" yaw manipulation with mouse/controller and scene offsets.
	const Quatf quat = vrFrame.Tracking.HeadPose.Pose.Orientation;

	quat.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>( &EyeYaw, &EyePitch, &EyeRoll );
#ifndef ENABLE_OVR_STICK_MOTION
	// Yaw is modified by both joystick and application-set scene yaw.
	// Pitch is only modified by joystick when no head tracking sensor is active.
	if ( YawMod > 0.0f )
	{
		EyeYaw += ( StickYaw - fmodf( StickYaw, YawMod ) ) + SceneYaw;
	}
	else
	{
		EyeYaw += StickYaw  + SceneYaw;
	}
	EyePitch += StickPitch;
	//
	// Player movement
	//

	// Allow up / down movement if there is no floor collision model or in 'free move' mode.
	const bool upDown = ( WorldModel.Definition == NULL || FreeMove ) && ( ( vrFrame.Input.buttonState & BUTTON_RIGHT_TRIGGER ) != 0 );
	const Vector3f gamepadMove(
		vrFrame.Input.sticks[0][0],
			upDown ? -vrFrame.Input.sticks[0][1] : 0.0f,
			upDown ? 0.0f : vrFrame.Input.sticks[0][1] );

	// Perform player movement if there is input.
	if ( gamepadMove.LengthSq() > 0.0f )
	{
		const Matrix4f yawRotate = Matrix4f::RotationY( EyeYaw );
		const Vector3f orientationVector = yawRotate.Transform( gamepadMove );

		// Don't let move get too crazy fast
		const float moveDistance = std::min<float>( MoveSpeed * (float)dt, 1.0f );
		if ( WorldModel.Definition != NULL && !FreeMove )
		{
			FootPos = SlideMove( FootPos, GetEyeHeight(), orientationVector, moveDistance,
						WorldModel.Definition->Collisions, WorldModel.Definition->GroundCollisions );
		}
		else
		{	// no scene loaded, walk without any collisions
			ModelCollision collisionModel;
			ModelCollision groundCollisionModel;
			FootPos = SlideMove( FootPos, GetEyeHeight(), orientationVector, moveDistance,
						collisionModel, groundCollisionModel );
		}
	}
#endif

	//
	// Center eye transform
	//
	UpdateCenterEye();

	//
	// Model animations
	//

	if ( !Paused )
	{
		for ( int i = 0; i < static_cast< int >( Models.size() ); i++ )
		{
			if ( Models[i] != NULL )
			{
				Models[i]->AnimateJoints( vrFrame.PredictedDisplayTimeInSeconds );
			}
		}
	}

	// External systems can add surfaces to this list before drawing.
	EmitSurfaces.resize( 0 );
}

}	// namespace OVR
