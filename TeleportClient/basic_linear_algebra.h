// (C) Copyright 2018-2021 Simul Software Ltd
#pragma once
#include <cmath>

#include "libavstream/common_maths.h"

#include "Platform/Shaders/SL/CppSl.sl"

//TODO: Placeholder! Find maths library!
namespace scr
{
	//CONSTANTS

	constexpr float PI = 3.1415926535f;
	constexpr float TAU = 2.0f * PI;
	constexpr float HALF_PI = 0.5f * PI;
	constexpr float DEG_TO_RAD = PI / 180.f;
	constexpr float RAD_TO_DEG = 180.f / PI;
	
	//FORWARD DECLARATIONS

	struct quat;
	inline quat operator*(float lhs, const quat& rhs);

	struct mat4;
	inline mat4 operator*(float lhs, const mat4& rhs);

	//DEFINITIONS

	struct quat
	{
		float i, j, k, s;

		quat()
			:i(0), j(0), k(0), s(0)
		{}

		quat(float i, float j, float k, float s)
			:i(i), j(j), k(k), s(s)
		{}
		
		quat(float angle, const avs::vec3& axis)
		{
			avs::vec3 scaledAxis = axis * sinf(angle / 2.0f);
			s = cosf(angle / 2.0f);
			i = scaledAxis.x;
			j = scaledAxis.y;
			k = scaledAxis.z;

			Normalise();
		}

		quat(avs::vec4 vec)
			:i(vec.x), j(vec.y), k(vec.z), s(vec.w)
		{}

		quat Conjugate() const
		{
			return quat(-this->i, -this->j, -this->k, this->s);
		}

		quat& Normalise()
		{
			float length = sqrtf(s * s + i * i + j * j + k * k);
			s /= length;
			i /= length;
			j /= length;
			k /= length;

			return *this;
		}

		quat GetNormalised() const
		{
			float length = sqrtf(s * s + i * i + j * j + k * k);
			return quat(i / length, j / length, k / length, s / length);
		}

		void ToAxisAngle(avs::vec3& outAxis, float& outAngle) const
		{
			avs::vec3 result = avs::vec3(i, j, k);

			float theta = 2 * acosf(s);
			if(theta > 0)
			{
				result* (1.0f / sinf(theta / 2.0f));
			}

			outAxis = result;
			outAngle = theta;
		}

		avs::vec3 GetIJK() const
		{
			return avs::vec3(i, j, k).Normalised();
		}

		avs::vec3 RotateVector(const avs::vec3 rhs) const
		{
			avs::vec3 quatVec(i, j, k);

			return
				quatVec * 2.0f * quatVec.Dot(rhs) +
				rhs * (s * s - quatVec.Dot(quatVec)) +
				quatVec.Cross(rhs) * 2.0f * s;
		}

		static quat Slerp(const scr::quat& source, const scr::quat& target, float time)
		{
			avs::vec4 unitSource = source.GetNormalised();
			avs::vec4 unitTarget = target.GetNormalised();

			float dot = unitSource.Dot(unitTarget);
			if(dot < 0.0f)
			{
				unitSource = -unitSource;
				dot = -dot;
			}

			static const double DOT_THRESHOLD = 0.9995f;
			if(static_cast<double>(dot) > DOT_THRESHOLD)
			{
				quat result = (unitSource * time) + (unitTarget - unitSource);
				return result.Normalise();
			}

			float theta_0 = acos(dot);
			float theta = theta_0 * time;
			float sin_theta_0 = sin(theta_0);
			float sin_theta = sin(theta);

			float s0 = cos(theta) - dot * sin_theta / sin_theta_0;
			float s1 = sin_theta / sin_theta_0;

			return (s0 * unitSource) + (s1 * unitTarget);
		}

		quat Slerp(const scr::quat& rhs, float time) const
		{
			return Slerp(*this, rhs, time);
		}
		bool operator==(const quat& q)
		{
			return s==q.s&&i==q.i&&j==q.j&&k==q.k;
		}
		bool operator!=(const quat& q)
		{
			return s!=q.s||i!=q.i||j!=q.j||k!=q.k;
		}

		quat operator-() const
		{
			return quat(-i, -j, -k, -s);
		}

		quat operator*(const quat& other) const
		{
			return quat(
				((s * other.i) + (i * other.s) + (j * other.k) - (k * other.j)),	//I
				((s * other.j) - (i * other.k) + (j * other.s) + (k * other.i)),	//J
				((s * other.k) + (i * other.j) - (j * other.i) + (k * other.s)),	//K
				((s * other.s) - (i * other.i) - (j * other.j) - (k * other.k))		//S
			);
		}

		quat operator*(const avs::vec3& other) const
		{
			return quat(
				(+(s * other.x) + (j * other.z) - (k * other.y)),	//I
				(+(s * other.y) + (k * other.x) - (i * other.z)),	//J
				(+(s * other.z) + (i * other.y) - (j * other.x)),	//K
				(-(i * other.x) - (j * other.y) - (k * other.z))	//S
			);
		}

		quat operator*(float rhs) const
		{
			return quat(i * rhs, j * rhs, k * rhs, s * rhs);
		}

		void operator*=(const quat& other)
		{
			*this = *this * other;
		}

		void operator*=(const avs::vec3& other)
		{
			*this = *this * other;
		}

		const quat &operator=(const avs::vec4 &vec)
		{
			s = vec.w;
			i = vec.x;
			j = vec.y;
			k = vec.z;
			return *this;
		}

		operator avs::vec4() const
		{
			return avs::vec4(i, j, k, s);
		}
	};

	inline quat operator*(float lhs, const quat& rhs)
	{
		return quat(rhs.i * lhs, rhs.j * lhs, rhs.k * lhs, rhs.s * lhs);
	}

	struct mat2
	{
		float a, b;
		float c, d;

		mat2()
			:mat2(1.0f)
		{}

		mat2(float diagonal)
			:mat2
			(
				1.0f, 0.0f,
				0.0f, 1.0f
			)
		{}

		mat2(float a, float b, float c, float d)
			:a(a), b(b), c(c), d(d)
		{}

		static float Determinant(float a, float b, float c, float d)
		{
			return a * d - b * c;
		}

		float GetDeterminant() const
		{
			return Determinant(a, b, c, d);
		}
	};

	struct mat3
	{
		float a, b, c;
		float d, e, f;
		float g, h, i;

		mat3()
			:mat3(1.0f)
		{}

		mat3(float diagonal)
			:mat3
			(
				1.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 1.0f
			)
		{}

		mat3(float a, float b, float c, float d, float e, float f, float g, float h, float i)
			:a(a), b(b), c(c), d(d), e(e), f(f), g(g), h(h), i(i)
		{}

		static float Determinant(float a, float b, float c, float d, float e, float f, float g, float h, float i)
		{
			return a * mat2::Determinant(e, f, h, i) - b * mat2::Determinant(d, f, g, i) + c * mat2::Determinant(d, e, g, h);
		}

		float GetDeterminant() const
		{
			return Determinant(a, b, c, d, e, f, g, h, i);
		}
	};

	struct mat4
	{
		float a, b, c, d;
		float e, f, g, h;
		float i, j, k, l;
		float m, n, o, p;

		mat4()
			:mat4(1.0f)
		{}

		mat4(float a, float b, float c, float d, float e, float f, float g, float h,
			 float i, float j, float k, float l, float m, float n, float o, float p)
			:a(a), b(b), c(c), d(d),
			e(e), f(f), g(g), h(h),
			i(i), j(j), k(k), l(l),
			m(m), n(n), o(o), p(p)
		{}

		mat4(const float *m)
			:mat4(m[0], m[1], m[2], m[3],
				  m[4], m[5], m[6], m[7],
				  m[8], m[9], m[10], m[11],
				  m[12], m[13], m[14], m[15])
		{}

		mat4(float diagonal)
			:mat4
			(diagonal, 0.0f, 0.0f, 0.0f,
			 0.0f, diagonal, 0.0f, 0.0f,
			 0.0f, 0.0f, diagonal, 0.0f,
			 0.0f, 0.0f, 0.0f, diagonal
			)
		{}

		mat4(const avs::vec4& a, const avs::vec4& b, const avs::vec4& c, const avs::vec4& d)
			:mat4
			(a.x, a.y, a.z, a.w,
			 b.x, b.y, b.z, b.w,
			 c.x, c.y, c.z, c.w,
			 d.x, d.y, d.z, d.w
			)
		{}

		mat4(avs::Mat4x4 matrix)
			:mat4(matrix.m00, matrix.m01, matrix.m02, matrix.m03,
				  matrix.m10, matrix.m11, matrix.m12, matrix.m13,
				  matrix.m20, matrix.m21, matrix.m22, matrix.m23,
				  matrix.m30, matrix.m31, matrix.m32, matrix.m33)
		{}

		static mat4 Transpose(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float p)
		{
			return mat4
			(
				a, e, i, m,
				b, f, j, n,
				c, g, k, o,
				d, h, l, p
			);
		}

		static mat4 Transpose(mat4 source)
		{
			return Transpose(source.a, source.b, source.c, source.d, source.e, source.f, source.g, source.h, source.i, source.j, source.k, source.l, source.m, source.n, source.o, source.p);
		}

		mat4 GetTranspose() const
		{
			return Transpose(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);
		}

		mat4& Transposed()
		{
			/*a = a;
			f = f;
			k = k;
			p = p;*/

			std::swap(b, e);
			std::swap(c, i);
			std::swap(d, m);
			std::swap(g, j);
			std::swap(h, n);
			std::swap(l, o);

			return *this;
		}


		static float Determinant(float a, float b, float c, float d, float e, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float p)
		{
			return a * mat3::Determinant(f, g, h, j, k, l, n, o, p) - b * mat3::Determinant(e, g, h, i, k, l, m, o, p) + c * mat3::Determinant(e, f, h, i, j, l, m, n, p) - d * mat3::Determinant(e, f, g, i, j, k, m, n, o);
		}

		float GetDeterminant() const
		{
			return Determinant(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);
		}

		mat4 GetInverted() const
		{
			const mat4 cofactors
			(
				+mat3::Determinant(f, g, h, j, k, l, n, o, p), -mat3::Determinant(e, g, h, i, k, l, m, o, p), +mat3::Determinant(e, f, h, i, j, l, m, n, p), -mat3::Determinant(e, f, g, i, j, k, m, n, o),
				-mat3::Determinant(b, c, d, j, k, l, n, o, p), +mat3::Determinant(a, c, d, i, k, l, m, o, p), -mat3::Determinant(a, b, d, i, j, l, m, n, p), +mat3::Determinant(a, b, c, i, j, k, m, n, o),
				+mat3::Determinant(b, c, d, f, g, h, n, o, p), -mat3::Determinant(a, c, d, e, g, h, m, o, p), +mat3::Determinant(a, b, d, e, f, h, m, n, p), -mat3::Determinant(a, b, c, e, f, g, m, n, o),
				-mat3::Determinant(b, c, d, f, g, h, j, k, l), +mat3::Determinant(a, c, d, e, g, h, i, k, l), -mat3::Determinant(a, b, d, e, f, h, i, j, l), +mat3::Determinant(a, b, c, e, f, g, i, j, k)
			);

			mat4 adjugate = cofactors.GetTranspose();
			float determinate = a * cofactors.a + b * cofactors.b + c * cofactors.c + d * cofactors.d;

			return (1 / determinate) * adjugate;
		}

		avs::vec4 operator*(const avs::vec4& input) const
		{
			avs::vec4 transform_i(a, e, i, m);
			avs::vec4 transform_j(b, f, j, n);
			avs::vec4 transform_k(c, g, k, o);
			avs::vec4 transform_l(d, h, l, p);
			avs::vec4 output(transform_i * input.x + transform_j * input.y + transform_k * input.z + transform_l * input.w);
			return output;
		}
		
		mat4 operator* (const mat4& input) const
		{
			avs::vec4 input_i(input.a, input.e, input.i, input.m);
			avs::vec4 input_j(input.b, input.f, input.j, input.n);
			avs::vec4 input_k(input.c, input.g, input.k, input.o);
			avs::vec4 input_l(input.d, input.h, input.l, input.p);

			avs::vec4 output_i = *this * input_i;
			avs::vec4 output_j = *this * input_j;
			avs::vec4 output_k = *this * input_k;
			avs::vec4 output_l = *this * input_l;

			mat4 output(output_i, output_j, output_k, output_l);
			output.Transposed();
			return output;
		}

		mat4 operator*(float rhs) const
		{
			return mat4
			(
				a * rhs, b * rhs, c * rhs, d * rhs,
				e * rhs, f * rhs, g * rhs, h * rhs,
				i * rhs, j * rhs, k * rhs, l * rhs,
				m * rhs, n * rhs, o * rhs, p * rhs
			);
		}

		bool operator==(const mat4& rhs) const
		{
			return
				a == rhs.a &&
				b == rhs.b &&
				c == rhs.c &&
				d == rhs.d &&
				e == rhs.e &&
				f == rhs.f &&
				g == rhs.g &&
				h == rhs.h &&
				i == rhs.i &&
				j == rhs.j &&
				k == rhs.k &&
				l == rhs.l &&
				m == rhs.m &&
				n == rhs.n &&
				o == rhs.o &&
				p == rhs.p;
		}

		bool operator!=(const mat4& rhs) const
		{
			return !(*this == rhs);
		}

		avs::vec3 GetTranslation() const
		{
			return avs::vec3(d, h, l);
		}

		scr::quat GetRotation() const
		{
			//TODO: An actual implementation.
			return scr::quat(0,0,0,1.0f);
		}

		avs::vec3 GetScale() const
		{
			avs::vec3 x(a, e, i);
			avs::vec3 y(b, f, j);
			avs::vec3 z(c, g, k);

			return avs::vec3(x.Length(), y.Length(), z.Length());
		} 

		static mat4 Identity()
		{
			return mat4(1.0f);
		}

		static mat4 Perspective(float horizontalFOV, float aspectRatio, float zNear, float zFar)
		{
			return mat4
			(
				(1.0f / (aspectRatio * static_cast<float>(tanf(horizontalFOV / 2.0f)))), (0), (0), (0),
				(0), (1.0f / static_cast<float>(tanf(horizontalFOV / 2.0f))), (0), (0),
				(0), (0), -((zFar + zNear) / (zFar - zNear)), -((2.0f * zFar * zNear) / (zFar - zNear)),
				(0), (0), (-1.0f), (0)
			);
		}

		static mat4 Orthographic(float left, float right, float bottom, float top, float _near, float _far)
		{
			return mat4
			(
				(2.0f / (right - left)), (0), (0), (-(right + left) / (right - left)),
				(0), (2.0f / (top - bottom)), (0), (-(top + bottom) / (top - bottom)),
				(0), (0), (-2.0f / (_far - _near)), (-(_far + _near) / (_far - _near)),
				(0), (0), (0), (1.0f)
			);
		}

		static mat4 Translation(const avs::vec3& translation)
		{
			return mat4
			(
				1.0f, 0.0f, 0.0f, translation.x,
				0.0f, 1.0f, 0.0f, translation.y,
				0.0f, 0.0f, 1.0f, translation.z,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		}

		static mat4 Rotation(const quat& orientation)
		{
			return mat4
			(
				(powf(orientation.s, 2) + powf(orientation.i, 2) - powf(orientation.j, 2) - powf(orientation.k, 2)), 2 * (orientation.i * orientation.j - orientation.k * orientation.s), 2 * (orientation.i * orientation.k + orientation.j * orientation.s), 0,
				2 * (orientation.i * orientation.j + orientation.k * orientation.s), (powf(orientation.s, 2) - powf(orientation.i, 2) + powf(orientation.j, 2) - powf(orientation.k, 2)), 2 * (orientation.j * orientation.k - orientation.i * orientation.s), 0,
				2 * (orientation.i * orientation.k - orientation.j * orientation.s), 2 * (orientation.j * orientation.k + orientation.i * orientation.s), (powf(orientation.s, 2) - powf(orientation.i, 2) - powf(orientation.j, 2) + powf(orientation.k, 2)), 0,
				0, 0, 0, 1
			);
		}

		static mat4 Rotation(float angle, const avs::vec3& axis)
		{
			mat4 result(1);
			float c_angle = static_cast<float>(cos(angle));
			float s_angle = static_cast<float>(sin(angle));
			float omcos = static_cast<float>(1 - c_angle);

			float x = axis.x;
			float y = axis.y;
			float z = axis.z;

			result.a = x * x * omcos + c_angle;
			result.e = x * y * omcos + z * s_angle;
			result.i = x * z * omcos - y * s_angle;
			result.m = 0;

			result.b = y * x * omcos - z * s_angle;
			result.f = y * y * omcos + c_angle;
			result.j = y * z * omcos + x * s_angle;
			result.n = 0;

			result.c = z * x * omcos + y * s_angle;
			result.g = z * y * omcos - x * s_angle;
			result.k = z * z * omcos + c_angle;
			result.o = 0;

			result.d = 0;
			result.h = 0;
			result.l = 0;
			result.p = 1;

			return result;
		}

		static mat4 Scale(const avs::vec3& scale)
		{
			return mat4
			(
				scale.x, 0.0f, 0.0f, 0.0f,
				0.0f, scale.y, 0.0f, 0.0f,
				0.0f, 0.0f, scale.z, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);
		}

		operator avs::Mat4x4()
		{
			return avs::Mat4x4
			(
				a, b, c, d,
				e, f, g, h,
				i, j, k, l,
				m, n, o, p
			);
		}
	};

	inline mat4 operator*(float lhs, const mat4& rhs)
	{
		return rhs * lhs;
	}

	struct uvec2
	{
		uint32_t x, y;
	};

	struct uvec3
	{
		uint32_t x, y, z;
	};

	struct uvec4
	{
		uint32_t x, y, z, w;
	};

	struct ivec2
	{
		int32_t x, y;
	};

	struct ivec3
	{
		int32_t x, y, z;
	};

	struct ivec4
	{
		int32_t x, y, z, w;
	};

	/** Returns vertical FOV. FOV values in radians. */
	inline float GetVerticalFOVFromHorizontal(float horizontal, float aspect)
	{
		return 2 * std::atanf(tanf(horizontal * 0.5f) * aspect);
	}

	inline float GetVerticalFOVFromHorizontalInDegrees(float horizontal, float aspect)
	{
		return GetVerticalFOVFromHorizontal(horizontal * DEG_TO_RAD, aspect) * RAD_TO_DEG;
	}
}