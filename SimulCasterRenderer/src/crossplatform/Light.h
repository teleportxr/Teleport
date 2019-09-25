// (C) Copyright 2018-2019 Simul Software Ltd
#pragma once

#include "Common.h"

#include "Actor.h"

#include "api/FrameBuffer.h"
#include "api/UniformBuffer.h"

namespace scr
{
	class Light
	{
	public:
		enum class Type : uint32_t
		{
			POINT,
			DIRECTIONAL,
			SPOT,
			AREA
		};
		struct LightCreateInfo
		{
			RenderPlatform* renderPlatform;
			Type type;
			vec3 position;
			quat orientation;
			std::shared_ptr<Texture> shadowMapTexture;
		};
	
	private:
		LightCreateInfo m_CI;

		struct LightData //Layout conformant to GLSL std140
		{
			vec4 colour;
			vec3 position;
			float power;		 //Strength or Power of the light in Watts equilavent to Radiant Flux in Radiometry.
			vec3 direction;
			float _pad;
			mat4 lightSpaceTransform;
		};
		static bool s_UninitialisedUB;
		std::shared_ptr<UniformBuffer> m_UB;

		static const uint32_t s_MaxLights;
		static std::vector<LightData> s_LightData;
		size_t m_LightID;
		bool m_IsValid = false;

		std::shared_ptr<Sampler> m_ShadowMapSampler;

		ShaderResourceLayout m_ShaderResourceLayout;
		ShaderResource m_ShaderResource;

	public:
		Light(LightCreateInfo* pLightCreateInfo);
		~Light() = default;

		void UpdatePosition(const vec3& position);
		void UpdateOrientation(const quat& orientation);

		inline const ShaderResource& GetDescriptorSet() const { return m_ShaderResource; }
		inline std::shared_ptr<Texture>& GetShadowMapTexture() { return m_CI.shadowMapTexture; }
		inline const std::vector<LightData>& GetLightData() const { return s_LightData; }
		inline bool IsValid() { return m_IsValid; }

	private:
		void UpdateLightSpaceTransform();
	};
}