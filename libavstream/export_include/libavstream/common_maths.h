#pragma once

#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <libavstream/common_packing.h>

namespace avs
{
	/// A model for how 3D space is mapped to X, Y and Z axes. A server may use one standard internally, while clients may use others.
	/// A server must be capable of supporting clients in at least EngineeringStyle and GlStyle.
	enum class AxesStandard : uint8_t
	{
		NotInitialized	 = 0,
		RightHanded		 = 1,
		LeftHanded		 = 2,
		YVertical		 = 4,
		ZVertical		 = 8,
		EngineeringStyle = ZVertical | RightHanded,
		GlStyle			 = 16 | YVertical | RightHanded,
		UnrealStyle		 = 32 | ZVertical | LeftHanded,
		UnityStyle		 = 64 | YVertical | LeftHanded,
	};

	inline AxesStandard operator|(const AxesStandard &a, const AxesStandard &b)
	{
		return (AxesStandard)((uint8_t)a | (uint8_t)b);
	}

	inline AxesStandard operator&(const AxesStandard &a, const AxesStandard &b)
	{
		return (AxesStandard)((uint8_t)a & (uint8_t)b);
	}

	struct Mat4x4
	{
		//[Row, Column]
		float m00, m01, m02, m03;
		float m10, m11, m12, m13;
		float m20, m21, m22, m23;
		float m30, m31, m32, m33;

		Mat4x4()
			: m00(1.0f), m01(0.0f), m02(0.0f), m03(0.0f), m10(0.0f), m11(1.0f), m12(0.0f), m13(0.0f), m20(0.0f), m21(0.0f), m22(1.0f), m23(0.0f), m30(0.0f),
			  m31(0.0f), m32(0.0f), m33(1.0f)
		{
		}

		Mat4x4(float m00,
			   float m01,
			   float m02,
			   float m03,
			   float m10,
			   float m11,
			   float m12,
			   float m13,
			   float m20,
			   float m21,
			   float m22,
			   float m23,
			   float m30,
			   float m31,
			   float m32,
			   float m33)
			: m00(m00), m01(m01), m02(m02), m03(m03), m10(m10), m11(m11), m12(m12), m13(m13), m20(m20), m21(m21), m22(m22), m23(m23), m30(m30), m31(m31),
			  m32(m32), m33(m33)
		{
		}

		Mat4x4 operator*(const Mat4x4 &rhs) const
		{
			return Mat4x4(m00 * rhs.m00 + m01 * rhs.m10 + m02 * rhs.m20 + m03 * rhs.m30,
						  m00 * rhs.m01 + m01 * rhs.m11 + m02 * rhs.m21 + m03 * rhs.m31,
						  m00 * rhs.m02 + m01 * rhs.m12 + m02 * rhs.m22 + m03 * rhs.m32,
						  m00 * rhs.m03 + m01 * rhs.m13 + m02 * rhs.m23 + m03 * rhs.m33,
						  m10 * rhs.m00 + m11 * rhs.m10 + m12 * rhs.m20 + m13 * rhs.m30,
						  m10 * rhs.m01 + m11 * rhs.m11 + m12 * rhs.m21 + m13 * rhs.m31,
						  m10 * rhs.m02 + m11 * rhs.m12 + m12 * rhs.m22 + m13 * rhs.m32,
						  m10 * rhs.m03 + m11 * rhs.m13 + m12 * rhs.m23 + m13 * rhs.m33,
						  m20 * rhs.m00 + m21 * rhs.m10 + m22 * rhs.m20 + m23 * rhs.m30,
						  m20 * rhs.m01 + m21 * rhs.m11 + m22 * rhs.m21 + m23 * rhs.m31,
						  m20 * rhs.m02 + m21 * rhs.m12 + m22 * rhs.m22 + m23 * rhs.m32,
						  m20 * rhs.m03 + m21 * rhs.m13 + m22 * rhs.m23 + m23 * rhs.m33,
						  m30 * rhs.m00 + m31 * rhs.m10 + m32 * rhs.m20 + m33 * rhs.m30,
						  m30 * rhs.m01 + m31 * rhs.m11 + m32 * rhs.m21 + m33 * rhs.m31,
						  m30 * rhs.m02 + m31 * rhs.m12 + m32 * rhs.m22 + m33 * rhs.m32,
						  m30 * rhs.m03 + m31 * rhs.m13 + m32 * rhs.m23 + m33 * rhs.m33);
		}
	};

	inline mat4 convertToStandard(const mat4 &matrix, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
	{

		mat4 convertedMatrix = matrix;

		switch (sourceStandard)
		{
		case avs::AxesStandard::UnityStyle:
			switch (targetStandard)
			{
			case avs::AxesStandard::EngineeringStyle:
				/// POSITION:
				// convertedMatrix._m03 = matrix._m03;
				convertedMatrix._m13 = matrix._m23;
				convertedMatrix._m23 = matrix._m13;

				// ROTATION (Implicitly handles scale.):
				// convertedMatrix._m00 = matrix._m00;
				convertedMatrix._m01 = matrix._m02;
				convertedMatrix._m02 = matrix._m01;

				convertedMatrix._m10 = matrix._m20;
				convertedMatrix._m11 = matrix._m22;
				convertedMatrix._m12 = matrix._m21;

				convertedMatrix._m20 = matrix._m10;
				convertedMatrix._m21 = matrix._m12;
				convertedMatrix._m22 = matrix._m11;

				break;
			case ::avs::AxesStandard::GlStyle:
				convertedMatrix._m02 = -matrix._m02;
				convertedMatrix._m12 = -matrix._m12;
				convertedMatrix._m20 = -matrix._m20;
				convertedMatrix._m21 = -matrix._m21;
				convertedMatrix._m23 = -matrix._m23;

				break;
			default:
				// AVSLOG(Error) << "Unrecognised targetStandard in Mat4x4::convertToStandard!\n";
				break;
			}
			break;
		case avs::AxesStandard::UnrealStyle:
			switch (targetStandard)
			{
			case avs::AxesStandard::EngineeringStyle:
				/// POSITION:
				convertedMatrix._m03 = matrix._m13;
				convertedMatrix._m13 = matrix._m03;
				// convertedMatrix._m23 = matrix._m23;

				// ROTATION (Implicitly handles scale.):
				convertedMatrix._m00 = matrix._m11;
				convertedMatrix._m01 = matrix._m10;
				convertedMatrix._m02 = matrix._m12;

				convertedMatrix._m10 = matrix._m01;
				convertedMatrix._m11 = matrix._m00;
				convertedMatrix._m12 = matrix._m02;

				convertedMatrix._m20 = matrix._m21;
				convertedMatrix._m21 = matrix._m20;
				// convertedMatrix._m22 = matrix._m22;

				break;
			case ::avs::AxesStandard::GlStyle:
				//+position.y, +position.z, -position.x
				/// POSITION:
				convertedMatrix._m03 = matrix._m13;
				convertedMatrix._m13 = matrix._m23;
				convertedMatrix._m23 = -matrix._m03;

				// ROTATION (Implicitly handles scale.):
				convertedMatrix._m00 = matrix._m11;
				convertedMatrix._m01 = matrix._m12;
				convertedMatrix._m02 = -matrix._m10;

				convertedMatrix._m10 = matrix._m21;
				convertedMatrix._m11 = matrix._m22;
				convertedMatrix._m12 = -matrix._m20;

				convertedMatrix._m20 = -matrix._m01;
				convertedMatrix._m21 = -matrix._m02;
				convertedMatrix._m22 = matrix._m00;

				break;
			default:
				// AVSLOG(Error) << "Unrecognised targetStandard in Mat4x4::convertToStandard!\n";
				break;
			}
			break;
		default:
			// AVSLOG(Error) << "Unrecognised sourceStandard in Mat4x4::convertToStandard!\n";
			break;
		}

		return convertedMatrix;
	}
	struct Transform
	{
		vec3 position = {0.f, 0.f, 0.f};	  //! 12
		vec4 rotation = {0.f, 0.f, 0.f, 1.f}; //! 16
		vec3 scale	  = {1.f, 1.f, 1.f};	  //! 12
	} AVS_PACKED;
	static_assert(sizeof(Transform) == 40, "Incorrect size for Transform");

} // namespace avs