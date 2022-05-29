#include "Tests.h"

#include "libavstream/common_maths.h"
#include "TeleportClient/Log.h"
#include "TeleportCore/ErrorHandling.h"

#include "Common.h"
#include "Transform.h"

namespace clientrender
{
	void Tests::RunAllTests()
	{
		RunConversionEquivalenceTests();
	}

	void Tests::RunConversionEquivalenceTests()
	{
		//Test conversions from Unity server.
		RunConversionEquivalenceTest(avs::AxesStandard::UnityStyle, avs::AxesStandard::EngineeringStyle);
		RunConversionEquivalenceTest(avs::AxesStandard::UnityStyle, avs::AxesStandard::GlStyle);
	
		//Test conversions from Unreal server.
		RunConversionEquivalenceTest(avs::AxesStandard::UnrealStyle, avs::AxesStandard::EngineeringStyle);
		RunConversionEquivalenceTest(avs::AxesStandard::UnrealStyle, avs::AxesStandard::GlStyle);
	}

	void Tests::RunConversionEquivalenceTest(avs::AxesStandard fromStandard, avs::AxesStandard toStandard)
	{
		avs::Transform transformAVS;
		transformAVS.position = avs::vec3(1.0f, 2.0f, 3.0f);
		transformAVS.rotation = avs::vec4(4.0f, 5.0f, 6.0f, 7.0f);
		transformAVS.scale = avs::vec3(8.0f, 9.0f, 10.0f);

		avs::Transform convertedTransformAVS(transformAVS);
		avs::ConvertTransform(fromStandard, toStandard, convertedTransformAVS);

		clientrender::Transform transformSCR(transformAVS);
		clientrender::Transform convertedTransformSCR(convertedTransformAVS);

		mat4 matrix = transformSCR.GetTransformMatrix();
		avs::Mat4x4 convertedMatrix = avs::Mat4x4::convertToStandard(*((avs::Mat4x4*)&matrix), fromStandard, toStandard);

		if(*((mat4*)&convertedMatrix)!= convertedTransformSCR.GetTransformMatrix())
		{
			TELEPORT_CERR_BREAK("Test failure! Failed equivalence check between transform conversion and matrix conversion!", EPROTO)
		}
	}
}
