#pragma once

#include <cstdint>
#include <cmath>
#include <iostream>
//#include "Platform/Shaders/SL/CppSl.sl"
namespace avs
{
	enum class AxesStandard : uint8_t
	{
		NotInitialized = 0,
		RightHanded = 1,
		LeftHanded = 2,
		YVertical = 4,
		EngineeringStyle = 8 | RightHanded,
		GlStyle = 16 | RightHanded,
		UnrealStyle = 32 | LeftHanded,
		UnityStyle = 64 | LeftHanded | YVertical,
	};

	inline AxesStandard operator|(const AxesStandard& a, const AxesStandard& b)
	{
		return (AxesStandard)((uint8_t)a | (uint8_t)b);
	}

	inline AxesStandard operator&(const AxesStandard& a, const AxesStandard& b)
	{
		return (AxesStandard)((uint8_t)a & (uint8_t)b);
	}

	struct vec2
	{
		float x, y;

		vec2()
			:vec2(0.0f, 0.0f)
		{}

		constexpr vec2(float x, float y)
			:x(x), y(y)
		{}

		vec2 operator-() const
		{
			return vec2(-x, -y);
		}

		vec2 operator+(const vec2& rhs) const
		{
			return vec2(x + rhs.x, y + rhs.y);
		}

		vec2 operator-(const vec2& rhs) const
		{
			return vec2(x - rhs.x, y - rhs.y);
		}

		vec2 operator*(float rhs) const
		{
			return vec2(x * rhs, y * rhs);
		}

		vec2 operator/(float rhs) const
		{
			return vec2(x / rhs, y / rhs);
		}

		void operator+=(const vec2& rhs)
		{
			*this = *this + rhs;
		}

		void operator-=(const vec2& rhs)
		{
			*this = *this - rhs;
		}

		void operator*=(const float& rhs)
		{
			*this = *this * rhs;
		}

		void operator/=(const float& rhs)
		{
			*this = *this / rhs;
		}

		float Length() const
		{
			return sqrtf(x * x + y * y);
		}

		vec2 Normalised() const
		{
			return *this / Length();
		}

		float Dot(const vec2& other) const
		{
			return x * other.x + y * other.y;
		}

		vec2 GetAbsolute() const
		{
			return vec2(fabs(x), fabs(y));
		}

		template<typename OutStream>
		friend OutStream& operator<< (OutStream& out, const vec2& vec)
		{
			out << " "<<vec.x << " " << vec.y;
			return out;
		}

		template<typename InStream>
		friend InStream& operator>> (InStream& in, vec2& vec)
		{
			in >> vec.x >> vec.y;
			return in;
		}
	};

	struct vec3
	{
		float x, y, z;

		vec3()
			:vec3(0.0f, 0.0f, 0.0f)
		{}

		constexpr vec3(float x, float y, float z)
			:x(x), y(y), z(z)
		{}
		bool operator==(const vec3& v)
		{
			return x == v.x && y == v.y && z == v.z;
		}
		bool operator!=(const vec3& v)
		{
			return x != v.x || y != v.y || z != v.z;
		}

		vec3 operator-() const
		{
			return vec3(-x, -y, -z);
		}

		const float* operator()() const
		{
			return &x;
		}

		void operator=(const float* v)
		{
			x = v[0];
			y = v[1];
			z = v[2];
		}

		vec3 operator+(const vec3& rhs) const
		{
			return vec3(x + rhs.x, y + rhs.y, z + rhs.z);
		}

		vec3 operator-(const vec3& rhs) const
		{
			return vec3(x - rhs.x, y - rhs.y, z - rhs.z);
		}

		vec3 operator*(float rhs) const
		{
			return vec3(x * rhs, y * rhs, z * rhs);
		}

		vec3 operator/(float rhs) const
		{
			return vec3(x / rhs, y / rhs, z / rhs);
		}

		void operator+=(const vec3& rhs)
		{
			*this = *this + rhs;
		}

		void operator-=(const vec3& rhs)
		{
			*this = *this - rhs;
		}

		void operator*=(float rhs)
		{
			*this = *this * rhs;
		}

		void operator/=(float rhs)
		{
			*this = *this / rhs;
		}

		vec3 operator*(vec3 rhs) const
		{
			return vec3(x * rhs.x, y * rhs.y, z * rhs.z);
		}

		void operator*=(vec3 rhs)
		{
			*this = *this * rhs;
		}
	};
	
	inline float dot(const vec3& a,const vec3& b)
	{
		return a.x*b.x + a.y*b.y + a.z*b.z;
	}

	inline float length(const vec3& v)
	{
		return sqrt(dot(v,v));
	}
	

	inline vec3 normalize(const vec3& v)
	{
		return v / length(v);
	}
	inline vec3 cross(const vec3& a,const vec3& rhs)
	{
		return vec3(a.y * rhs.z - a.z * rhs.y, a.z * rhs.x - a.x * rhs.z, a.x * rhs.y - a.y * rhs.x);
	}
	inline vec3 abs(const vec3 &v)
	{
		vec3 result={::fabsf(v.x), ::fabsf(v.y), ::fabsf(v.z)};
		return result;
	}
	template<typename OutStream> OutStream& operator<< (OutStream& out, const vec3& vec)
	{
		std::basic_ostream<wchar_t,std::char_traits<wchar_t>> &o=out;
		o << vec.x << " " << vec.y << " " << vec.z;
		return out;
	}

	template<typename InStream> InStream& operator>> (InStream& in, vec3& vec)
	{
		in >> vec.x >> vec.y >> vec.z;
		return in;
	}

	struct vec4
	{
		float x, y, z, w;
	};

	inline vec2 operator*(float lhs, const vec2& rhs)
	{
		return vec2(rhs.x * lhs, rhs.y * lhs);
	}

	inline vec3 operator*(float lhs, const vec3& rhs)
	{
		return vec3(rhs.x * lhs, rhs.y * lhs, rhs.z * lhs);
	}

	struct int3
	{
		int v1, v2, v3;
	};

	struct Mat4x4
	{
		//[Row, Column]
		float m00, m01, m02, m03;
		float m10, m11, m12, m13;
		float m20, m21, m22, m23;
		float m30, m31, m32, m33;

		Mat4x4()
			:m00(1.0f), m01(0.0f), m02(0.0f), m03(0.0f),
			m10(0.0f), m11(1.0f), m12(0.0f), m13(0.0f),
			m20(0.0f), m21(0.0f), m22(1.0f), m23(0.0f),
			m30(0.0f), m31(0.0f), m32(0.0f), m33(1.0f)
		{}

		Mat4x4(float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33)
			:m00(m00), m01(m01), m02(m02), m03(m03),
			m10(m10), m11(m11), m12(m12), m13(m13),
			m20(m20), m21(m21), m22(m22), m23(m23),
			m30(m30), m31(m31), m32(m32), m33(m33)
		{}

		Mat4x4 operator*(const Mat4x4& rhs) const
		{
			return Mat4x4
			(
				m00 * rhs.m00 + m01 * rhs.m10 + m02 * rhs.m20 + m03 * rhs.m30, m00 * rhs.m01 + m01 * rhs.m11 + m02 * rhs.m21 + m03 * rhs.m31, m00 * rhs.m02 + m01 * rhs.m12 + m02 * rhs.m22 + m03 * rhs.m32, m00 * rhs.m03 + m01 * rhs.m13 + m02 * rhs.m23 + m03 * rhs.m33,
				m10 * rhs.m00 + m11 * rhs.m10 + m12 * rhs.m20 + m13 * rhs.m30, m10 * rhs.m01 + m11 * rhs.m11 + m12 * rhs.m21 + m13 * rhs.m31, m10 * rhs.m02 + m11 * rhs.m12 + m12 * rhs.m22 + m13 * rhs.m32, m10 * rhs.m03 + m11 * rhs.m13 + m12 * rhs.m23 + m13 * rhs.m33,
				m20 * rhs.m00 + m21 * rhs.m10 + m22 * rhs.m20 + m23 * rhs.m30, m20 * rhs.m01 + m21 * rhs.m11 + m22 * rhs.m21 + m23 * rhs.m31, m20 * rhs.m02 + m21 * rhs.m12 + m22 * rhs.m22 + m23 * rhs.m32, m20 * rhs.m03 + m21 * rhs.m13 + m22 * rhs.m23 + m23 * rhs.m33,
				m30 * rhs.m00 + m31 * rhs.m10 + m32 * rhs.m20 + m33 * rhs.m30, m30 * rhs.m01 + m31 * rhs.m11 + m32 * rhs.m21 + m33 * rhs.m31, m30 * rhs.m02 + m31 * rhs.m12 + m32 * rhs.m22 + m33 * rhs.m32, m30 * rhs.m03 + m31 * rhs.m13 + m32 * rhs.m23 + m33 * rhs.m33
			);
		}

		static Mat4x4 convertToStandard(const Mat4x4& matrix, avs::AxesStandard sourceStandard, avs::AxesStandard targetStandard)
		{

			Mat4x4 convertedMatrix = matrix;

			switch (sourceStandard)
			{
			case avs::AxesStandard::UnityStyle:
				switch (targetStandard)
				{
				case avs::AxesStandard::EngineeringStyle:
					///POSITION:
					//convertedMatrix.m03 = matrix.m03;
					convertedMatrix.m13 = matrix.m23;
					convertedMatrix.m23 = matrix.m13;

					//ROTATION (Implicitly handles scale.):
					//convertedMatrix.m00 = matrix.m00;
					convertedMatrix.m01 = matrix.m02;
					convertedMatrix.m02 = matrix.m01;

					convertedMatrix.m10 = matrix.m20;
					convertedMatrix.m11 = matrix.m22;
					convertedMatrix.m12 = matrix.m21;

					convertedMatrix.m20 = matrix.m10;
					convertedMatrix.m21 = matrix.m12;
					convertedMatrix.m22 = matrix.m11;

					break;
				case::avs::AxesStandard::GlStyle:
					convertedMatrix.m02 = -matrix.m02;
					convertedMatrix.m12 = -matrix.m12;
					convertedMatrix.m20 = -matrix.m20;
					convertedMatrix.m21 = -matrix.m21;
					convertedMatrix.m23 = -matrix.m23;

					break;
				default:
					//AVSLOG(Error) << "Unrecognised targetStandard in Mat4x4::convertToStandard!\n";
					break;
				}
				break;
			case avs::AxesStandard::UnrealStyle:
				switch (targetStandard)
				{
				case avs::AxesStandard::EngineeringStyle:
					///POSITION:
					convertedMatrix.m03 = matrix.m13;
					convertedMatrix.m13 = matrix.m03;
					//convertedMatrix.m23 = matrix.m23;

					//ROTATION (Implicitly handles scale.):
					convertedMatrix.m00 = matrix.m11;
					convertedMatrix.m01 = matrix.m10;
					convertedMatrix.m02 = matrix.m12;

					convertedMatrix.m10 = matrix.m01;
					convertedMatrix.m11 = matrix.m00;
					convertedMatrix.m12 = matrix.m02;

					convertedMatrix.m20 = matrix.m21;
					convertedMatrix.m21 = matrix.m20;
					//convertedMatrix.m22 = matrix.m22;

					break;
				case::avs::AxesStandard::GlStyle:
					//+position.y, +position.z, -position.x
					///POSITION:
					convertedMatrix.m03 = matrix.m13;
					convertedMatrix.m13 = matrix.m23;
					convertedMatrix.m23 = -matrix.m03;

					//ROTATION (Implicitly handles scale.):
					convertedMatrix.m00 = matrix.m11;
					convertedMatrix.m01 = matrix.m12;
					convertedMatrix.m02 = -matrix.m10;

					convertedMatrix.m10 = matrix.m21;
					convertedMatrix.m11 = matrix.m22;
					convertedMatrix.m12 = -matrix.m20;

					convertedMatrix.m20 = -matrix.m01;
					convertedMatrix.m21 = -matrix.m02;
					convertedMatrix.m22 = matrix.m00;

					break;
				default:
					//AVSLOG(Error) << "Unrecognised targetStandard in Mat4x4::convertToStandard!\n";
					break;
				}
				break;
			default:
				//AVSLOG(Error) << "Unrecognised sourceStandard in Mat4x4::convertToStandard!\n";
				break;
			}

			return convertedMatrix;
		}
	};

	struct Pose
	{
		avs::vec4 orientation = { 0, 0, 0, 1 };
		avs::vec3 position = { 0, 0, 0 };
	};
	struct PoseDynamic
	{
		Pose pose;
		avs::vec3 velocity;
		avs::vec3 angularVelocity;
	};
} //namespace avs