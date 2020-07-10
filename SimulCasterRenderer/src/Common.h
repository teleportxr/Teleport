// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

//C Libraries
#include <iostream>
#include <string>
#include <assert.h>

//STL
#include <vector>
#include <deque>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>
#include <libavstream/geometry/mesh_interface.hpp>

//Debug
#define SCR_CERR_BREAK(msg, errCode) std::cerr << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);
#define SCR_CERR(msg)				 std::cerr << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl;

#define SCR_COUT_BREAK(msg, errCode) std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; throw(errCode);
#define SCR_COUT(msg)				 std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl;

extern void log_print(const char* source,const char *format, ...);

#if defined(__ANDROID__)
#include <android/log.h>
#define FILE_LINE (std::string(__FILE__) + std::string("(") +  std::to_string(__LINE__) + std::string("):")).c_str()
#define SCR_LOG(...) __android_log_print(ANDROID_LOG_INFO, "SCR", __VA_ARGS__);
#else
#define SCR_LOG(fmt,...) log_print( "SCR", fmt, __VA_ARGS__);
#endif

namespace scr
{
	class RenderPlatform;
	class APIObject
	{
	protected:
		const RenderPlatform*const renderPlatform;
		APIObject(const RenderPlatform*const r) 
			: renderPlatform(r) {}
	};
	enum BufferUsageBit : uint32_t
	{
		UNKNOWN_BIT = 0x00000000,
		STREAM_BIT	= 0x00000001,
		STATIC_BIT	= 0x00000002,
		DYNAMIC_BIT = 0x00000004,
		READ_BIT	= 0x00000008,
		WRITE_BIT	= 0x00000010,
		COPY_BIT	= 0x00000020,
		DRAW_BIT	= 0x00000040
	};

	struct SceneCapture2DTagData
	{
#ifndef __ANDROID__
#pragma pack(push, 1)
#endif
		uint32_t id;
		avs::Transform cameraTransform;
#ifndef __ANDROID__
#pragma pack(pop)
#endif
	}
#ifdef __ANDROID__
	__attribute__((packed))
#endif
		;

	// Taken from Unity
	enum class LightType
	{
		//
		// Summary:
		//     The light is a spot light.
		Spot = 0,
		//
		// Summary:
		//     The light is a directional light.
		Directional = 1,
		//
		// Summary:
		//     The light is a point light.
		Point = 2,
		Area = 3,
		//
		// Summary:
		//     The light is a rectangle shaped area light. It affects only baked lightmaps and
		//     lightprobes.
		Rectangle = 3,
		//
		// Summary:
		//     The light is a disc shaped area light. It affects only baked lightmaps and lightprobes.
		Disc = 4
	};

	struct SceneCaptureCubeCoreTagData
	{
#ifndef __ANDROID__
#pragma pack(push, 1)
#endif
		uint32_t id;
		avs::Transform cameraTransform;
		uint32_t lightCount;
#ifndef __ANDROID__
#pragma pack(pop)
#endif
	}
#ifdef __ANDROID__
	__attribute__((packed))
#endif
		;

	struct LightData
	{
#ifndef __ANDROID__
#pragma pack(push, 1)
#endif
		avs::Transform worldTransform;
		avs::vec4 color;
		float range;
		float spotAngle;
		LightType lightType;
		float shadowViewMatrix[4][4];
		float shadowProjectionMatrix[4][4];
#ifndef __ANDROID__
#pragma pack(pop)
#endif
	}
#ifdef __ANDROID__
	__attribute__((packed))
#endif
		;


	struct SceneCaptureCubeTagData
	{
		SceneCaptureCubeCoreTagData coreData;
		std::vector<LightData> lights;
	}
	;
}