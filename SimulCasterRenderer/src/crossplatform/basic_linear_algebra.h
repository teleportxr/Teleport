// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once
#include <cmath>

//TODO: Placeholder! Find maths library!
namespace scr
{
	const float PI = 3.1415926535f;
	const float TAU = 2.0f * PI;
	const float HALF_PI = 0.5f * PI;

	struct vec2
	{
		float x, y;

		vec2()
			:x(0), y(0) {};
		vec2(float x, float y)
			:x(x), y(y) {};

		inline float Length() 
		{ 
			return sqrtf((powf(x, 2) + powf(y, 2))); 
		}
		inline vec2 Normalise()
		{
			return (*this * (1.0f / Length()));
		}
		inline float Dot(const vec2& other)
		{
			return (this->x * other.x) + (this->y * other.y);
		}

		vec2 operator+ (const vec2& other) const
		{
			return vec2(x + other.x, y + other.y);
		}

		vec2 operator- (const vec2& other) const
		{
			return vec2(x - other.x, y - other.y);
		}

		//Scales the vec2 by the scaler a. The scaler go on the rhs of the object.
		vec2 operator*(float a) const
		{
			return vec2(a * x, a * y);
		}
	};
	struct vec3
	{
		float x, y, z;

		vec3()
			:x(0), y(0), z(0) {};
		vec3(float x, float y, float z)
			:x(x), y(y), z(z) {};

		inline float Length()
		{
			return sqrtf((powf(x, 2) + powf(y, 2) + powf(z, 2)));
		}
		inline vec3 Normalise()
		{
			return (*this * (1.0f / Length()));
		}
		inline float Dot(const vec3& other)
		{
			return (this->x * other.x) + (this->y * other.y) + (this->z * other.z);
		}
		inline vec3 Cross(const vec3& other)
		{
			return vec3(this->y * other.z - this->z * other.y, this->z * other.x - this->x * other.z, this->x * other.y - this->y * other.x);
		}

		vec3 operator+ (const vec3& other) const
		{
			return vec3(x + other.x, y + other.y, z + other.z);
		}

		vec3 operator- (const vec3& other) const
		{
			return vec3(x - other.x, y - other.y, z - other.z);
		}

		//Scales the vec3 by the scaler a. The scaler go on the rhs of the object.
		vec3 operator*(float a) const
		{
			return vec3(a * x, a * y, a * z);
		}
	};
	struct vec4
	{
		float x, y, z, w;

		vec4()
			:x(0), y(0), z(0), w(0) {};
		vec4(float x, float y, float z, float w)
			:x(x), y(y), z(z), w(w) {};

		inline float Length()
		{
			return sqrtf((powf(x, 2) + powf(y, 2) + powf(z, 2) + powf(w, 2)));
		}
		inline vec4 Normalise()
		{
			return (*this * (1.0f / Length()));
		}
		inline float Dot(const vec4& other)
		{
			return (this->x * other.x) + (this->y * other.y) + (this->z * other.z) + (this->w * other.w);
		}

		vec4 operator+ (const vec4& other) const
		{
			return vec4(x + other.x, y + other.y, z + other.z, w + other.w);
		}

		vec4 operator- (const vec4& other) const
		{
			return vec4(x - other.x, y - other.y, z - other.z, w - other.w);
		}

		//Scales the vec3 by the scaler a. The scaler go on the rhs of the object.
		vec4 operator*(float a) const
		{
			return vec4(a * x, a * y, a * z, a * w);
		}
	};
	struct quat
	{
		float s, i, j, k;

		quat()
			:s(0), i(0), j(0), k(0) {}

		quat(float s, float i, float j, float k)
			: s(s), i(i), j(j), k(k) {}
		
		quat(float angle, const vec3& axis)
		{
			vec3 scaledAxis = axis * sinf(angle / 2.0f);
			s = cosf(angle / 2.0f);
			i = scaledAxis.x;
			j = scaledAxis.y;
			k = scaledAxis.z;
			Normalise();
		}

		quat Conjugate()
		{
			return quat(this->s, -this->i, -this->j, -this->k);
		}
		quat Normalise()
		{
			float length = sqrtf(s * s + i * i + j * j + k * k);
			s /= length;
			i /= length;
			j /= length;
			k /= length;
			return *this;
		}
		vec3 ToVec3(const quat& other)
		{
			vec3 result = vec3(other.i, other.j, other.k);
			float theta = 2 * acosf(other.s);
			if (theta > 0)
			{
				result * (1.0f / sinf(theta / 2.0f));
			}
			return result;
		}
		quat operator*(const quat& other) const
		{
			return quat(
				((s * other.s) - (i * other.i) - (j * other.j) - (k * other.k)),
				((s * other.i) + (i * other.s) + (j * other.k) - (k * other.j)),
				((s * other.j) - (i * other.k) + (j * other.s) + (k * other.i)),
				((s * other.k) + (i * other.j) - (j * other.i) + (k * other.s))
			);
		}
		quat operator*(const vec3& other) const
		{
			return quat(
				(-(i * other.x) - (j * other.y) - (k * other.z)),
				(+(s * other.x) + (j * other.z) - (k * other.y)),
				(+(s * other.y) + (k * other.x) - (i * other.z)),
				(+(s * other.z) + (i * other.y) - (j * other.x))
			);
		}
	};
	struct mat4
	{
		float a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p;

		mat4()
			:a(0), b(0), c(0), d(0), e(0), f(0), g(0), h(0),
			i(0), j(0), k(0), l(0), m(0), n(0), o(0), p(0) {}

		mat4(float a, float b, float c, float d, float e, float f, float g, float h,
			float i, float j, float k, float l, float m, float n, float o, float p)
			: a(a), b(b), c(c), d(d), e(e), f(f), g(g), h(h),
			i(i), j(j), k(k), l(l), m(m), n(n), o(o), p(p) {}

		mat4(float diagonal)
			: a(diagonal), b(0), c(0), d(0), e(0), f(diagonal), g(0), h(0),
			i(0), j(0), k(diagonal), l(0), m(0), n(0), o(0), p(diagonal) {}

		mat4(const vec4& a, const vec4& b, const vec4& c, const vec4& d)
			: a(a.x), b(a.y), c(a.z), d(a.w), e(b.x), f(b.y), g(b.z), h(b.w),
			i(c.x), j(c.y), k(c.z), l(c.w), m(d.x), n(d.y), o(d.z), p(d.w) {}

		void Transpose()
		{
			/*a = a;
			f = f;
			k = k;
			p = p;*/

			float temp_b = b;
			float temp_c = c;
			float temp_d = d;
			float temp_g = g;
			float temp_h = h;
			float temp_l = l;

			b = e;
			c = i;
			d = m;
			g = j;
			h = n;
			l = o;

			e = temp_b;
			i = temp_c;
			m = temp_d;
			j = temp_g;
			n = temp_h;
			o = temp_l;
		}
		mat4 Identity()
		{
			return mat4(1);
		}

		static mat4 Perspective(float horizontalFOV, float aspectRatio, float zNear, float zFar) 
		{
			return mat4((1 / (aspectRatio * static_cast<float>(tanf(horizontalFOV / 2)))), (0), (0), (0),
				(0), (1 / static_cast<float>(tanf(horizontalFOV / 2))), (0), (0),
				(0), (0), -((zFar + zNear) / (zFar - zNear)), -((2 * zFar * zNear) / (zFar - zNear)),
				(0), (0), (-1), (0));
		};
		static mat4 Orthographic(float left, float right, float bottom, float top, float _near, float _far) 
		{
			return mat4((2 / (right - left)), (0), (0), (-(right + left) / (right - left)),
				(0), (2 / (top - bottom)), (0), (-(top + bottom) / (top - bottom)),
				(0), (0), (-2 / (_far - _near)), (-(_far + _near) / (_far - _near)),
				(0), (0), (0), (1));
		};
		
		static mat4 Translation(const vec3& translation)
		{
			mat4 result(1);
			result.d = translation.x;
			result.h = translation.y;
			result.l = translation.z;
			return result;
		};
		static mat4 Rotation(const quat& orientation) 
		{
			vec3 axis = vec3(orientation.i, orientation.j, orientation.k);
			float angle = 2 * acosf(orientation.s);
			if (angle > 0)
			{
				axis * (1.0f / sinf(angle / 2.0f));
			}
			
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
		};
		static mat4 Rotation(float angle, const vec3& axis)
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
		};
		static mat4 Scale(const vec3& scale) 
		{
			mat4 result(1);
			result.a = scale.x;
			result.f = scale.y;
			result.k = scale.z;
			return result;
		};
		
		inline vec4 Mat4Vec4Multi(const vec4& input, const mat4& transform)
		{
			float x = input.x;
			float y = input.y;
			float z = input.z;
			float w = input.w;
			vec4 transform_i(transform.a, transform.e, transform.i, transform.m);
			vec4 transform_j(transform.b, transform.f, transform.j, transform.n);
			vec4 transform_k(transform.c, transform.g, transform.k, transform.o);
			vec4 transform_l(transform.d, transform.h, transform.l, transform.p);
			vec4 output(transform_i * x + transform_j * y + transform_k * z + transform_l * w);
			return output;
		}
		inline vec4 operator* (const vec4& input)
		{
			return Mat4Vec4Multi(input, *this);
		}

		inline mat4 Mat4Mat4Multi(mat4& transform, const mat4& input)
		{
			vec4 input_i(input.a, input.e, input.i, input.m);
			vec4 input_j(input.b, input.f, input.j, input.n);
			vec4 input_k(input.c, input.g, input.k, input.o);
			vec4 input_l(input.d, input.h, input.l, input.p);
			vec4 output_i = transform * input_i;
			vec4 output_j = transform * input_j;
			vec4 output_k = transform * input_k;
			vec4 output_l = transform * input_l;
			mat4 output(output_i, output_j, output_k, output_l);
			output.Transpose();
			return output;
		}
		inline mat4 operator* (const mat4& input)
		{
			return Mat4Mat4Multi(*this, input);
		}


	};
}