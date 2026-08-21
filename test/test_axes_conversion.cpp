// Axes-standard conversion invariants for libavstream/common_maths.h.
//
// Mirrors teleport-nodejs/test/test_axes_conversion.js so the two implementations of the same
// table keep identical semantics — they are the two halves of the same wire contract.
//
// These are property tests rather than tables of expected numbers, deliberately: the tables are
// what is under test, and a hand-transcribed table can be wrong in ways that still look
// plausible. This file exists because two such defects were live here at once:
//
//   * ConvertPosition/ConvertRotation/ConvertScale had no EngineeringStyle <-> GlStyle entries
//     at all. They mutate in place with no else and no diagnostic, so the pair the protocol
//     requires every server to support (docs/protocol/conventions.rst) was a silent identity.
//   * platform::crossplatform::ConvertRotation(Engineering -> OpenGL) negated x, which is not
//     the inverse of its own OpenGL -> Engineering and disagrees with ConvertPosition.
//
// Note that a round-trip test alone catches neither cleanly: a missing entry is a no-op, and a
// no-op round-trips perfectly. The load-bearing checks here are "basis vectors map to basis
// vectors" and "a conversion between differing standards is never the identity".

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <string>

#include "libavstream/common_maths.h"

namespace
{
	//! What each standard means, stated independently of the conversion tables: which way is
	//! up, and which way a node with identity orientation faces. Mirrors
	//! teleport-nodejs/client/motion/axes_basis.js.
	//!
	//!   EngineeringStyle  right-handed, Z up:  (x,y,z) = (right, forward, up)
	//!   GlStyle           right-handed, Y up:  (x,y,z) = (right, up, back)
	//!   UnrealStyle       left-handed,  Z up:  (x,y,z) = (forward, right, up)
	//!   UnityStyle        left-handed,  Y up:  (x,y,z) = (right, up, forward)
	struct Basis
	{
		vec3 up;
		vec3 forward;
	};

	Basis BasisFor(avs::AxesStandard standard)
	{
		switch (standard)
		{
		case avs::AxesStandard::EngineeringStyle:
			return {{0, 0, 1}, {0, 1, 0}};
		case avs::AxesStandard::GlStyle:
			return {{0, 1, 0}, {0, 0, -1}};
		case avs::AxesStandard::UnrealStyle:
			return {{0, 0, 1}, {1, 0, 0}};
		case avs::AxesStandard::UnityStyle:
			return {{0, 1, 0}, {0, 0, 1}};
		default:
			FAIL("no basis for axes standard " << static_cast<int>(standard));
			return {{0, 0, 1}, {0, 1, 0}};
		}
	}

	constexpr std::array<avs::AxesStandard, 4> ALL_STANDARDS = {avs::AxesStandard::EngineeringStyle,
															   avs::AxesStandard::GlStyle,
															   avs::AxesStandard::UnrealStyle,
															   avs::AxesStandard::UnityStyle};

	std::string Name(avs::AxesStandard standard)
	{
		switch (standard)
		{
		case avs::AxesStandard::EngineeringStyle: return "EngineeringStyle";
		case avs::AxesStandard::GlStyle:		  return "GlStyle";
		case avs::AxesStandard::UnrealStyle:	  return "UnrealStyle";
		case avs::AxesStandard::UnityStyle:		  return "UnityStyle";
		default:								  return "unknown";
		}
	}

	//! Unreal <-> Unity is the one pair with no entry in any implementation of this table; both
	//! directions leave the value untouched. Named here so the tests exclude it deliberately
	//! rather than quietly, and so adding the pair later fails a test that says so.
	bool IsUnsupportedPair(avs::AxesStandard from, avs::AxesStandard to)
	{
		return (from == avs::AxesStandard::UnrealStyle && to == avs::AxesStandard::UnityStyle) ||
			   (from == avs::AxesStandard::UnityStyle && to == avs::AxesStandard::UnrealStyle);
	}

	bool IsSupportedPair(avs::AxesStandard from, avs::AxesStandard to)
	{
		return from != to && !IsUnsupportedPair(from, to);
	}

	constexpr float EPSILON = 1e-5f;

	bool Close(const vec3 &a, const vec3 &b)
	{
		return std::fabs(a.x - b.x) < EPSILON && std::fabs(a.y - b.y) < EPSILON && std::fabs(a.z - b.z) < EPSILON;
	}

	bool Close(const vec4 &a, const vec4 &b)
	{
		return std::fabs(a.x - b.x) < EPSILON && std::fabs(a.y - b.y) < EPSILON && std::fabs(a.z - b.z) < EPSILON &&
			   std::fabs(a.w - b.w) < EPSILON;
	}

	std::string Show(const vec3 &v)
	{
		return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
	}

	std::string Show(const vec4 &v)
	{
		return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ", " +
			   std::to_string(v.w) + ")";
	}

	std::string Label(avs::AxesStandard from, avs::AxesStandard to)
	{
		return Name(from) + " -> " + Name(to);
	}

	vec3 Convert(avs::AxesStandard from, avs::AxesStandard to, vec3 v)
	{
		avs::ConvertPosition(from, to, v);
		return v;
	}

	vec4 ConvertRot(avs::AxesStandard from, avs::AxesStandard to, vec4 q)
	{
		avs::ConvertRotation(from, to, q);
		return q;
	}

	vec3 ConvertScl(avs::AxesStandard from, avs::AxesStandard to, vec3 v)
	{
		avs::ConvertScale(from, to, v);
		return v;
	}

	vec3 Normalise(vec3 v)
	{
		const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		return {v.x / length, v.y / length, v.z / length};
	}

	//! Quaternion for a rotation of angle radians about a unit axis.
	vec4 QuaternionAbout(const vec3 &axis, float angle)
	{
		const float s = std::sin(angle * 0.5f);
		return {axis.x * s, axis.y * s, axis.z * s, std::cos(angle * 0.5f)};
	}
}

TEST_CASE("the four complete standards have the documented wire values", "[axes]")
{
	// These bytes are the wire format (docs/protocol/conventions.rst) and are mirrored in
	// teleport-nodejs/core/core.js, teleport-web-client/src/wire/types.ts and the Unity C#
	// SDK. Changing one without the others is a real defect: the C# enum was missing
	// ZVertical, making its GlStyle 17 and its UnrealStyle 34.
	CHECK(static_cast<uint8_t>(avs::AxesStandard::EngineeringStyle) == 9);
	CHECK(static_cast<uint8_t>(avs::AxesStandard::GlStyle) == 21);
	CHECK(static_cast<uint8_t>(avs::AxesStandard::UnrealStyle) == 42);
	CHECK(static_cast<uint8_t>(avs::AxesStandard::UnityStyle) == 70);
}

TEST_CASE("basis vectors map to basis vectors", "[axes]")
{
	// The real test. Each standard is defined by which way is up and which way a node with
	// identity orientation faces; a conversion between two of them must carry one definition
	// onto the other. Nothing here is derived from the conversion tables, so a wrong or
	// missing entry cannot satisfy it by construction.
	for (avs::AxesStandard from : ALL_STANDARDS)
	{
		for (avs::AxesStandard to : ALL_STANDARDS)
		{
			if (!IsSupportedPair(from, to))
			{
				continue;
			}
			INFO(Label(from, to));
			const Basis source = BasisFor(from);
			const Basis target = BasisFor(to);
			CHECK(Close(Convert(from, to, source.up), target.up));
			CHECK(Close(Convert(from, to, source.forward), target.forward));
		}
	}
}

TEST_CASE("conversion between differing standards is never the identity", "[axes]")
{
	// The failure mode an absent table entry produces: the value passes through untouched,
	// which round-trips perfectly and looks like success. A vector with three distinct
	// non-zero components cannot survive any real change of basis unchanged.
	const vec3 v = {1.0f, 2.0f, 3.0f};
	for (avs::AxesStandard from : ALL_STANDARDS)
	{
		for (avs::AxesStandard to : ALL_STANDARDS)
		{
			if (!IsSupportedPair(from, to))
			{
				continue;
			}
			INFO(Label(from, to) << " left the position unchanged, so the table entry is missing");
			CHECK_FALSE(Close(Convert(from, to, v), v));
		}
	}
}

TEST_CASE("every supported conversion round-trips", "[axes]")
{
	const vec3 p = {1.0f, 2.0f, 3.0f};
	const vec4 q = {0.1f, 0.2f, 0.3f, 0.9f};
	const vec3 s = {1.0f, 2.0f, 4.0f};
	for (avs::AxesStandard from : ALL_STANDARDS)
	{
		for (avs::AxesStandard to : ALL_STANDARDS)
		{
			if (!IsSupportedPair(from, to))
			{
				continue;
			}
			INFO(Label(from, to));
			CHECK(Close(Convert(to, from, Convert(from, to, p)), p));
			CHECK(Close(ConvertRot(to, from, ConvertRot(from, to, q)), q));
			CHECK(Close(ConvertScl(to, from, ConvertScl(from, to, s)), s));
		}
	}
}

TEST_CASE("a rotation about an axis becomes a rotation about the converted axis", "[axes]")
{
	// Ties ConvertRotation to ConvertPosition: whatever permutation the table applies to a
	// direction, it must apply the same one to the axis a rotation turns about. A change of
	// basis between two standards of the same handedness is a rotation, so the angle survives
	// it; between differing handedness it is a mirror, so the angle reverses.
	//
	// The axis is deliberately general rather than one of the basis vectors. A basis vector has
	// two zero components and so cannot see a sign error in either of them — which is exactly
	// how the Engineering -> OpenGL x-negation survived: Engineering's up is (0,0,1).
	const vec3 axis = Normalise({0.3f, -0.5f, 0.8f});
	const float theta = 0.7f;
	for (avs::AxesStandard from : ALL_STANDARDS)
	{
		for (avs::AxesStandard to : ALL_STANDARDS)
		{
			if (!IsSupportedPair(from, to))
			{
				continue;
			}
			const bool sameHandedness =
				(from & avs::AxesStandard::LeftHanded) == (to & avs::AxesStandard::LeftHanded);
			const vec4 got	= ConvertRot(from, to, QuaternionAbout(axis, theta));
			const vec4 want = QuaternionAbout(Convert(from, to, axis), sameHandedness ? theta : -theta);
			INFO(Label(from, to) << (sameHandedness ? " (same handedness)" : " (differing handedness)") << ": got "
								 << Show(got) << ", want " << Show(want));
			CHECK(Close(got, want));
		}
	}
}

TEST_CASE("scale is the unsigned form of the position permutation", "[axes]")
{
	// Scale permutes exactly as position does but never changes sign — a negative scale would
	// mirror the mesh. Deriving one from the other keeps the two tables honest with each other.
	const vec3 s = {2.0f, 3.0f, 5.0f};
	for (avs::AxesStandard from : ALL_STANDARDS)
	{
		for (avs::AxesStandard to : ALL_STANDARDS)
		{
			if (!IsSupportedPair(from, to))
			{
				continue;
			}
			const vec3 converted = Convert(from, to, s);
			const vec3 want		 = {std::fabs(converted.x), std::fabs(converted.y), std::fabs(converted.z)};
			INFO(Label(from, to) << ": got " << Show(ConvertScl(from, to, s)) << ", want " << Show(want));
			CHECK(Close(ConvertScl(from, to, s), want));
		}
	}
}

TEST_CASE("Engineering <-> Gl uses the documented mapping", "[axes]")
{
	// Spelled out rather than derived, because this is the pair the protocol requires every
	// server to support ("A server must be capable of supporting clients in at least
	// EngineeringStyle and GlStyle") and the one this table omitted entirely.
	// Engineering is (right, forward, up); Gl is (right, up, back).
	const auto eng = avs::AxesStandard::EngineeringStyle;
	const auto gl  = avs::AxesStandard::GlStyle;

	CHECK(Close(Convert(eng, gl, {1, 2, 3}), vec3{1, 3, -2}));
	CHECK(Close(Convert(gl, eng, {1, 2, 3}), vec3{1, -3, 2}));

	// Both standards are right-handed, so the change of basis has determinant +1 and the
	// quaternion's vector part permutes exactly as a position does.
	CHECK(Close(ConvertRot(eng, gl, {1, 2, 3, 4}), vec4{1, 3, -2, 4}));
	CHECK(Close(ConvertRot(gl, eng, {1, 2, 3, 4}), vec4{1, -3, 2, 4}));

	CHECK(Close(ConvertScl(eng, gl, {1, 2, 3}), vec3{1, 3, 2}));
	CHECK(Close(ConvertScl(gl, eng, {1, 2, 3}), vec3{1, 3, 2}));
}

TEST_CASE("matrix conversion agrees with converting the transform's parts", "[axes]")
{
	// convertToStandard(mat4) and ConvertTransform must be two routes to the same answer.
	// This is what ClientRender/Tests.cpp meant to check, but nothing ever called it — and it
	// only covered Unity and Unreal sources, which are the only two the matrix switch handled.
	// An Engineering or Gl source used to fall through and return the matrix untouched.
	for (avs::AxesStandard from : ALL_STANDARDS)
	{
		for (avs::AxesStandard to : ALL_STANDARDS)
		{
			if (!IsSupportedPair(from, to))
			{
				continue;
			}
			INFO(Label(from, to));

			// A real rigid transform, so the matrix and quaternion routes are comparable: an
			// unnormalised quaternion would not give an orthogonal 3x3 part and the equality
			// would be meaningless.
			const vec4 rotation = QuaternionAbout(Normalise({0.2f, 0.4f, 0.9f}), 0.6f);
			const vec3 position = {1.0f, 2.0f, 3.0f};

			// Build the source matrix from the quaternion, convert it, and compare against the
			// matrix built from the separately-converted quaternion and position.
			mat4 sourceMatrix;
			sourceMatrix.setRotationTranslation(rotation, position);
			const mat4 convertedMatrix = avs::convertToStandard(sourceMatrix, from, to);

			mat4 expectedMatrix;
			expectedMatrix.setRotationTranslation(ConvertRot(from, to, rotation), Convert(from, to, position));

			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					INFO("element [" << i << "][" << j << "]");
					CHECK(std::fabs(convertedMatrix.M[i][j] - expectedMatrix.M[i][j]) < EPSILON);
				}
			}
		}
	}
}

TEST_CASE("converting to the same standard changes nothing", "[axes]")
{
	const vec3 p = {1.0f, 2.0f, 3.0f};
	const vec4 q = {0.1f, 0.2f, 0.3f, 0.9f};
	for (avs::AxesStandard standard : ALL_STANDARDS)
	{
		INFO(Name(standard));
		CHECK(Close(Convert(standard, standard, p), p));
		CHECK(Close(ConvertRot(standard, standard, q), q));
		CHECK(Close(ConvertScl(standard, standard, p), p));
	}
}
